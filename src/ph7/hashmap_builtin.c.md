# src/ph7/hashmap_builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2444/2846 lines (85.87%)

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
|       - |   17 | `/*` |
|       - |   18 | ` * bool shuffle(array &$array)` |
|       - |   19 | ` *  shuffles (randomizes the order of the elements in) an array.` |
|       - |   20 | ` * Parameters` |
|       - |   21 | ` *  $array` |
|       - |   22 | ` *   The input array.` |
|       - |   23 | ` * Return` |
|       - |   24 | ` *  TRUE on success or FALSE on failure.` |
|       - |   25 | ` *` |
|       - |   26 | ` */` |
|       2 |   27 | `PH7_PRIVATE int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |   28 | `{` |
|       - |   29 | `	ph7_hashmap *pMap;` |
|       - |   30 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 |   31 | `	if( nArg < 1 \|\| !ph7_value_is_array(apArg[0]) ){` |
|       - |   32 | `		/* Missing/Invalid arguments,return FALSE */` |
|     ! 0 |   33 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |   34 | `		return PH7_OK;` |
|       - |   35 | `	}` |
|       - |   36 | `	/* Point to the internal representation of the input hashmap */` |
|       3 |   37 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|       3 |   38 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 |   39 | `	if( pMap->nEntry > 1 ){` |
|       - |   40 | `		/* Do the merge sort */` |
|       3 |   41 | `		HashmapMergeSort(pMap,HashmapCmpCallback7,0);` |
|       - |   42 | `		/* Fix the last link broken by the merge */` |
|      11 |   43 | `		while(pMap->pLast->pPrev){` |
|       9 |   44 | `			pMap->pLast = pMap->pLast->pPrev;` |
|       1 |   45 | `		}` |
|       1 |   46 | `	}` |
|       - |   47 | `	/* All done,return TRUE */` |
|       3 |   48 | `	ph7_result_bool(pCtx,1);` |
|       3 |   49 | `	return PH7_OK;` |
|       2 |   50 | `}` |
|       - |   51 | `/*` |
|       - |   52 | ` * int count(array $var [, int $mode = COUNT_NORMAL ])` |
|       - |   53 | ` *   Count all elements in an array, or something in an object.` |
|       - |   54 | ` * Parameters` |
|       - |   55 | ` *  $var` |
|       - |   56 | ` *   The array or the object.` |
|       - |   57 | ` * $mode` |
|       - |   58 | ` *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()` |
|       - |   59 | ` *  will recursively count the array. This is particularly useful for counting` |
|       - |   60 | ` *  all the elements of a multidimensional array.` |
|       - |   61 | ` * Return` |
|       - |   62 | ` *  Returns the number of elements in the array.` |
|       - |   63 | ` */` |
|    2250 |   64 | `PH7_PRIVATE int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |   65 | `{` |
|    2255 |   66 | `	int bRecursive = FALSE;` |
|    2255 |   67 | `	int bCycleDetected = FALSE;` |
|       - |   68 | `	sxi64 iCount;` |
|    2255 |   69 | `	if( nArg < 1 ){` |
|     ! 0 |   70 | `		return PH7_VmThrowException(pCtx,` |
|       - |   71 | `			"ArgumentCountError",` |
|       - |   72 | `			"count() expects at least 1 argument, 0 given"` |
|       - |   73 | `			);` |
|       - |   74 | `	}` |
|    2255 |   75 | `	if( nArg > 2 ){` |
|     ! 0 |   76 | `		return PH7_VmThrowException(pCtx,` |
|       - |   77 | `			"ArgumentCountError",` |
|       - |   78 | `			"count() expects at most 2 arguments, %d given",` |
|     ! 0 |   79 | `			nArg` |
|       - |   80 | `			);` |
|       - |   81 | `	}` |
|       - |   82 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|       - |   83 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|       - |   84 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|    2255 |   85 | `	if( nArg > 1 ){` |
|      43 |   86 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|      43 |   87 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|      12 |   88 | `			return PH7_VmThrowException(pCtx,` |
|       - |   89 | `				"ValueError",` |
|       - |   90 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|       - |   91 | `				);` |
|       - |   92 | `		}` |
|      32 |   93 | `		bRecursive = iMode == 1;` |
|      15 |   94 | `	}` |
|    2247 |   95 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |   96 | `		/* Countable object: dispatch to ->count() */` |
|      75 |   97 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      65 |   98 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      65 |   99 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|      65 |  100 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
|      63 |  101 | `				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,` |
|       - |  102 | `					"count",sizeof("count")-1);` |
|      63 |  103 | `				if( pMeth ){` |
|       - |  104 | `					ph7_value sResult;` |
|      63 |  105 | `					PH7_MemObjInit(pCtx->pVm,&sResult);` |
|      63 |  106 | `					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);` |
|      63 |  107 | `					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));` |
|      63 |  108 | `					PH7_MemObjRelease(&sResult);` |
|      63 |  109 | `					return PH7_OK;` |
|       - |  110 | `				}` |
|     ! 0 |  111 | `			}` |
|       1 |  112 | `		}` |
|      19 |  113 | `		return PH7_VmThrowException(pCtx,` |
|       - |  114 | `			"TypeError",` |
|       - |  115 | `			"count(): Argument #1 ($value) must be of type Countable\|array, %s given",` |
|       5 |  116 | `			ph7_type_name(apArg[0])` |
|       - |  117 | `			);` |
|       - |  118 | `	}` |
|       - |  119 | `	/* Count */` |
|    2177 |  120 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|    2177 |  121 | `	if( bCycleDetected ){` |
|       3 |  122 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|       1 |  123 | `	}` |
|    2177 |  124 | `	ph7_result_int64(pCtx,iCount);` |
|    2177 |  125 | `	return PH7_OK;` |
|    1130 |  126 | `}` |
|       - |  127 | `/*` |
|       - |  128 | ` * bool array_key_exists(value $key,array $search)` |
|       - |  129 | ` *  Checks if the given key or index exists in the array.` |
|       - |  130 | ` * Parameters` |
|       - |  131 | ` * $key` |
|       - |  132 | ` *   Value to check.` |
|       - |  133 | ` * $search` |
|       - |  134 | ` *  An array with keys to check.` |
|       - |  135 | ` * Return` |
|       - |  136 | ` *  TRUE on success or FALSE on failure.` |
|       - |  137 | ` */` |
|     104 |  138 | `PH7_PRIVATE int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  139 | `{` |
|       - |  140 | `	sxi32 rc;` |
|     108 |  141 | `	if( nArg != 2 ){` |
|       - |  142 | `		/* PHP requires exactly two arguments */` |
|     ! 0 |  143 | `		return PH7_VmThrowException(pCtx,` |
|       - |  144 | `			"ArgumentCountError",` |
|       - |  145 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|     ! 0 |  146 | `			nArg` |
|       - |  147 | `			);` |
|       - |  148 | `	}` |
|       - |  149 | `	/* Make sure we are dealing with a valid hashmap */` |
|     108 |  150 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - |  151 | `		/* Type mismatch -> TypeError */` |
|       8 |  152 | `		return PH7_VmThrowException(pCtx,` |
|       - |  153 | `			"TypeError",` |
|       - |  154 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|       4 |  155 | `			ph7_type_name(apArg[1])` |
|       - |  156 | `			);` |
|       - |  157 | `	}` |
|       - |  158 | `	/* php only DEPRECATES a null / lossy-float key here; PHL rejects it. */` |
|     104 |  159 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|       3 |  160 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  161 | `			"array_key_exists(): Argument #1 ($key) must be of type string\|int, null given");` |
|     102 |  162 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|       3 |  163 | `		ph7_real rVal = apArg[0]->rVal;` |
|       3 |  164 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|       3 |  165 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  166 | `				"array_key_exists(): Argument #1 ($key) must be of type string\|int, float given");` |
|       - |  167 | `		}` |
|     ! 0 |  168 | `	}` |
|       - |  169 | `	/* Perform the lookup */` |
|     100 |  170 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|       - |  171 | `	/* lookup result */` |
|     100 |  172 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|     100 |  173 | `	return PH7_OK;` |
|      56 |  174 | `}` |
|       - |  175 | `/*` |
|       - |  176 | ` * value array_pop(array $array)` |
|       - |  177 | ` *   POP the last inserted element from the array.` |
|       - |  178 | ` * Parameter` |
|       - |  179 | ` *  The array to get the value from.` |
|       - |  180 | ` * Return` |
|       - |  181 | ` *  Poped value or NULL on failure.` |
|       - |  182 | ` */` |
|     106 |  183 | `PH7_PRIVATE int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  184 | `{` |
|       - |  185 | `	ph7_hashmap *pMap;` |
|       - |  186 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|     110 |  187 | `	if( nArg != 1 ){` |
|     ! 0 |  188 | `		return PH7_VmThrowException(pCtx,` |
|       - |  189 | `			"ArgumentCountError",` |
|       - |  190 | `			"array_pop() expects exactly 1 argument, %d given",` |
|     ! 0 |  191 | `			nArg` |
|       - |  192 | `			);` |
|       - |  193 | `	}` |
|       - |  194 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - |  195 | `	 * error message as official PHP. Check the index to detect constants. */` |
|     110 |  196 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       6 |  197 | `		return PH7_VmThrowException(pCtx,` |
|       - |  198 | `			"Error",` |
|       - |  199 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|       - |  200 | `			);` |
|       - |  201 | `	}` |
|       - |  202 | `	/* Make sure we are dealing with a valid hashmap */` |
|     104 |  203 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 |  204 | `		return PH7_VmThrowException(pCtx,` |
|       - |  205 | `			"TypeError",` |
|       - |  206 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|       1 |  207 | `			ph7_type_name(apArg[0])` |
|       - |  208 | `			);` |
|       - |  209 | `	}` |
|     101 |  210 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     101 |  211 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     101 |  212 | `	if( pMap->nEntry < 1 ){` |
|       - |  213 | `		/* Nothing to pop,return NULL */` |
|       3 |  214 | `		ph7_result_null(pCtx);` |
|       2 |  215 | `	}else{` |
|      99 |  216 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|       - |  217 | `		ph7_value *pObj;` |
|      99 |  218 | `		pObj = HashmapExtractNodeValue(pLast);` |
|      99 |  219 | `		if( pObj ){` |
|       - |  220 | `			/* Node value */` |
|      99 |  221 | `			ph7_result_value(pCtx,pObj);` |
|       - |  222 | `			/* Unlink the node */` |
|      99 |  223 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|      50 |  224 | `		}else{` |
|     ! 0 |  225 | `			ph7_result_null(pCtx);` |
|       - |  226 | `		}` |
|       - |  227 | `		/* Reset the cursor */` |
|      99 |  228 | `		pMap->pCur = pMap->pFirst;` |
|       - |  229 | `	}` |
|     101 |  230 | `	return PH7_OK;` |
|      57 |  231 | `}` |
|       - |  232 | `/*` |
|       - |  233 | ` * int array_push($array,$var,...)` |
|       - |  234 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|       - |  235 | ` * Parameters` |
|       - |  236 | ` *  array` |
|       - |  237 | ` *    The input array.` |
|       - |  238 | ` *  var` |
|       - |  239 | ` *   On or more value to push.` |
|       - |  240 | ` * Return` |
|       - |  241 | ` *  New array count (including old items).` |
|       - |  242 | ` */` |
|      22 |  243 | `PH7_PRIVATE int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  244 | `{` |
|       - |  245 | `	ph7_hashmap *pMap;` |
|       - |  246 | `	sxi32 rc;` |
|       - |  247 | `	int i;` |
|      27 |  248 | `	if( nArg < 1 ){` |
|     ! 0 |  249 | `		return PH7_VmThrowException(pCtx,` |
|       - |  250 | `			"ArgumentCountError",` |
|       - |  251 | `			"array_push() expects at least 1 argument, %d given",` |
|     ! 0 |  252 | `			nArg` |
|       - |  253 | `			);` |
|       - |  254 | `	}` |
|       - |  255 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - |  256 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      27 |  257 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       6 |  258 | `		return PH7_VmThrowException(pCtx,` |
|       - |  259 | `			"Error",` |
|       - |  260 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|       - |  261 | `			);` |
|       - |  262 | `	}` |
|       - |  263 | `	/* Make sure we are dealing with a valid hashmap */` |
|      21 |  264 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 |  265 | `		return PH7_VmThrowException(pCtx,` |
|       - |  266 | `			"TypeError",` |
|       - |  267 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|       1 |  268 | `			ph7_type_name(apArg[0])` |
|       - |  269 | `			);` |
|       - |  270 | `	}` |
|       - |  271 | `	/* Point to the internal representation of the input hashmap */` |
|      18 |  272 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      18 |  273 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - |  274 | `	/* Start pushing given values */` |
|      34 |  275 | `	for( i = 1 ; i < nArg ; ++i ){` |
|      20 |  276 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|      20 |  277 | `		if( rc != SXRET_OK ){` |
|       3 |  278 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|       - |  279 | `				/* Saturated-append Error (php: array_push throws, no result) */` |
|       3 |  280 | `				return rc;` |
|       - |  281 | `			}` |
|     ! 0 |  282 | `			break;` |
|       - |  283 | `		}` |
|       9 |  284 | `	}` |
|       - |  285 | `	/* Return the new count */` |
|      15 |  286 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|      15 |  287 | `	return PH7_OK;` |
|      16 |  288 | `}` |
|       - |  289 | `/*` |
|       - |  290 | ` * value array_shift(array $array)` |
|       - |  291 | ` *   Shift an element off the beginning of array.` |
|       - |  292 | ` * Parameter` |
|       - |  293 | ` *  The array to get the value from.` |
|       - |  294 | ` * Return` |
|       - |  295 | ` *  Shifted value or NULL on failure.` |
|       - |  296 | ` */` |
|      42 |  297 | `PH7_PRIVATE int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  298 | `{` |
|       - |  299 | `	ph7_hashmap *pMap;` |
|       - |  300 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      47 |  301 | `	if( nArg != 1 ){` |
|     ! 0 |  302 | `		return PH7_VmThrowException(pCtx,` |
|       - |  303 | `			"ArgumentCountError",` |
|       - |  304 | `			"array_shift() expects exactly 1 argument, %d given",` |
|     ! 0 |  305 | `			nArg` |
|       - |  306 | `			);` |
|       - |  307 | `	}` |
|       - |  308 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|      47 |  309 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       6 |  310 | `		return PH7_VmThrowException(pCtx,` |
|       - |  311 | `			"Error",` |
|       - |  312 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|       - |  313 | `			);` |
|       - |  314 | `	}` |
|       - |  315 | `	/* Make sure we are dealing with a valid hashmap */` |
|      43 |  316 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 |  317 | `		return PH7_VmThrowException(pCtx,` |
|       - |  318 | `			"TypeError",` |
|       - |  319 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|       1 |  320 | `			ph7_type_name(apArg[0])` |
|       - |  321 | `			);` |
|       - |  322 | `	}` |
|       - |  323 | `	/* Point to the internal representation of the hashmap */` |
|      41 |  324 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      41 |  325 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      41 |  326 | `	if( pMap->nEntry < 1 ){` |
|       - |  327 | `		/* Empty hashmap,return NULL */` |
|       3 |  328 | `		ph7_result_null(pCtx);` |
|       2 |  329 | `	}else{` |
|      39 |  330 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - |  331 | `		ph7_value *pObj;` |
|       - |  332 | `		sxu32 n;` |
|      39 |  333 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      39 |  334 | `		if( pObj ){` |
|       - |  335 | `			/* Node value */` |
|      39 |  336 | `			ph7_result_value(pCtx,pObj);` |
|       - |  337 | `			/* Unlink the first node */` |
|      39 |  338 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|      22 |  339 | `		}else{` |
|     ! 0 |  340 | `			ph7_result_null(pCtx);` |
|       - |  341 | `		}` |
|       - |  342 | `		/* Rehash all int keys */` |
|      39 |  343 | `		n = pMap->nEntry;` |
|      39 |  344 | `		pEntry = pMap->pFirst;` |
|      39 |  345 | `		pMap->iNextIdx = 0;` |
|      39 |  346 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|      47 |  347 | `		for(;;){` |
|      99 |  348 | `			if( n < 1 ){` |
|      39 |  349 | `				break;` |
|       - |  350 | `			}` |
|      65 |  351 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      65 |  352 | `				HashmapRehashIntNode(pEntry);` |
|      30 |  353 | `			}` |
|       - |  354 | `			/* Point to the next entry */` |
|      65 |  355 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      65 |  356 | `			n--;` |
|       5 |  357 | `		}` |
|       - |  358 | `		/* Reset the cursor */` |
|      39 |  359 | `		pMap->pCur = pMap->pFirst;` |
|       - |  360 | `	}` |
|      41 |  361 | `	return PH7_OK;` |
|      26 |  362 | `}` |
|       - |  363 | `/*` |
|       - |  364 | ` * Extract the node cursor value.` |
|       - |  365 | ` */` |
|    1232 |  366 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|       1 |  367 | `{` |
|    1233 |  368 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|       - |  369 | `	ph7_value *pVal;` |
|    1233 |  370 | `	if( pCur == 0 ){` |
|       - |  371 | `		/* Cursor does not point to anything,return FALSE */` |
|      39 |  372 | `		ph7_result_bool(pCtx,0);` |
|      39 |  373 | `		return PH7_OK;` |
|       - |  374 | `	}` |
|    1195 |  375 | `	if( iDirection != 0 ){` |
|     227 |  376 | `		if( iDirection > 0 ){` |
|       - |  377 | `			/* Point to the next entry */` |
|     225 |  378 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|     225 |  379 | `			pCur = pMap->pCur;` |
|     113 |  380 | `		}else{` |
|       - |  381 | `			/* Point to the previous entry */` |
|       3 |  382 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|       3 |  383 | `			pCur = pMap->pCur;` |
|       - |  384 | `		}` |
|     227 |  385 | `		if( pCur == 0 ){` |
|       - |  386 | `			/* End of input reached,return FALSE */` |
|      91 |  387 | `			ph7_result_bool(pCtx,0);` |
|      91 |  388 | `			return PH7_OK;` |
|       - |  389 | `		}` |
|      68 |  390 | `	}` |
|       - |  391 | `	/* Point to the desired element */` |
|    1105 |  392 | `	pVal = HashmapExtractNodeValue(pCur);` |
|    1105 |  393 | `	if( pVal ){` |
|    1105 |  394 | `		ph7_result_value(pCtx,pVal);` |
|     553 |  395 | `	}else{` |
|     ! 0 |  396 | `		ph7_result_bool(pCtx,0);` |
|       - |  397 | `	}` |
|    1105 |  398 | `	return PH7_OK;` |
|     617 |  399 | `}` |
|       - |  400 | `/*` |
|       - |  401 | ` * value current(array $array)` |
|       - |  402 | ` *  Return the current element in an array.` |
|       - |  403 | ` * Parameter` |
|       - |  404 | ` *  $input: The input array.` |
|       - |  405 | ` * Return` |
|       - |  406 | ` *  The current() function simply returns the value of the array element that's currently` |
|       - |  407 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - |  408 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - |  409 | ` *  is empty, current() returns FALSE.` |
|       - |  410 | ` */` |
|     356 |  411 | `PH7_PRIVATE int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  412 | `{` |
|     357 |  413 | `	if( nArg < 1 ){` |
|       - |  414 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  415 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  416 | `		return PH7_OK;` |
|       - |  417 | `	}` |
|       - |  418 | `	/* Make sure we are dealing with a valid hashmap */` |
|     357 |  419 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  420 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  421 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  422 | `		return PH7_OK;` |
|       - |  423 | `	}` |
|     357 |  424 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|     357 |  425 | `	return PH7_OK;` |
|     179 |  426 | `}` |
|       - |  427 | `/*` |
|       - |  428 | ` * value next(array $input)` |
|       - |  429 | ` *  Advance the internal array pointer of an array.` |
|       - |  430 | ` * Parameter` |
|       - |  431 | ` *  $input: The input array.` |
|       - |  432 | ` * Return` |
|       - |  433 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|       - |  434 | ` *  pointer one place forward before returning the element value. That means it returns` |
|       - |  435 | ` *  the next array value and advances the internal array pointer by one.` |
|       - |  436 | ` */` |
|     224 |  437 | `PH7_PRIVATE int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  438 | `{` |
|     225 |  439 | `	if( nArg < 1 ){` |
|       - |  440 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  441 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  442 | `		return PH7_OK;` |
|       - |  443 | `	}` |
|       - |  444 | `	/* Make sure we are dealing with a valid hashmap */` |
|     225 |  445 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  446 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  447 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  448 | `		return PH7_OK;` |
|       - |  449 | `	}` |
|     225 |  450 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|     225 |  451 | `	return PH7_OK;` |
|     113 |  452 | `}` |
|       - |  453 | `/*` |
|       - |  454 | ` * value prev(array $input)` |
|       - |  455 | ` *  Rewind the internal array pointer.` |
|       - |  456 | ` * Parameter` |
|       - |  457 | ` *  $input: The input array.` |
|       - |  458 | ` * Return` |
|       - |  459 | ` *  Returns the array value in the previous place that's pointed` |
|       - |  460 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|       - |  461 | ` *  elements.` |
|       - |  462 | ` */` |
|       2 |  463 | `PH7_PRIVATE int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  464 | `{` |
|       3 |  465 | `	if( nArg < 1 ){` |
|       - |  466 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  467 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  468 | `		return PH7_OK;` |
|       - |  469 | `	}` |
|       - |  470 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 |  471 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  472 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  473 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  474 | `		return PH7_OK;` |
|       - |  475 | `	}` |
|       3 |  476 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|       3 |  477 | `	return PH7_OK;` |
|       2 |  478 | `}` |
|       - |  479 | `/*` |
|       - |  480 | ` * value end(array $input)` |
|       - |  481 | ` *  Set the internal pointer of an array to its last element.` |
|       - |  482 | ` * Parameter` |
|       - |  483 | ` *  $input: The input array.` |
|       - |  484 | ` * Return` |
|       - |  485 | ` *  Returns the value of the last element or FALSE for empty array.` |
|       - |  486 | ` */` |
|     390 |  487 | `PH7_PRIVATE int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  488 | `{` |
|       - |  489 | `	ph7_hashmap *pMap;` |
|     391 |  490 | `	if( nArg < 1 ){` |
|       - |  491 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  492 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  493 | `		return PH7_OK;` |
|       - |  494 | `	}` |
|       - |  495 | `	/* Make sure we are dealing with a valid hashmap */` |
|     391 |  496 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  497 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  498 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  499 | `		return PH7_OK;` |
|       - |  500 | `	}` |
|       - |  501 | `	/* Point to the internal representation of the input hashmap */` |
|     391 |  502 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - |  503 | `	/* Point to the last node */` |
|     391 |  504 | `	pMap->pCur = pMap->pLast;` |
|       - |  505 | `	/* Return the last node value */` |
|     391 |  506 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|     391 |  507 | `	return PH7_OK;` |
|     196 |  508 | `}` |
|       - |  509 | `/*` |
|       - |  510 | ` * value reset(array $array )` |
|       - |  511 | ` *  Set the internal pointer of an array to its first element.` |
|       - |  512 | ` * Parameter` |
|       - |  513 | ` *  $input: The input array.` |
|       - |  514 | ` * Return` |
|       - |  515 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|       - |  516 | ` */` |
|     260 |  517 | `PH7_PRIVATE int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  518 | `{` |
|       - |  519 | `	ph7_hashmap *pMap;` |
|     261 |  520 | `	if( nArg < 1 ){` |
|       - |  521 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  522 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  523 | `		return PH7_OK;` |
|       - |  524 | `	}` |
|       - |  525 | `	/* Make sure we are dealing with a valid hashmap */` |
|     261 |  526 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  527 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  528 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  529 | `		return PH7_OK;` |
|       - |  530 | `	}` |
|       - |  531 | `	/* Point to the internal representation of the input hashmap */` |
|     261 |  532 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - |  533 | `	/* Point to the first node */` |
|     261 |  534 | `	pMap->pCur = pMap->pFirst;` |
|       - |  535 | `	/* Return the last node value if available */` |
|     261 |  536 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|     261 |  537 | `	return PH7_OK;` |
|     131 |  538 | `}` |
|       - |  539 | `/*` |
|       - |  540 | ` * Emit a node's key (integer or blob) as the call result — shared by key(),` |
|       - |  541 | ` * array_key_first() and array_key_last().` |
|       - |  542 | ` */` |
|     772 |  543 | `static void HashmapResultNodeKey(ph7_context *pCtx,ph7_hashmap_node *pNode)` |
|       1 |  544 | `{` |
|     773 |  545 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - |  546 | `		/* Key is integer */` |
|     383 |  547 | `		ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|     192 |  548 | `	}else{` |
|       - |  549 | `		/* Key is blob */` |
|     586 |  550 | `		ph7_result_string(pCtx,` |
|     390 |  551 | `			(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - |  552 | `	}` |
|     773 |  553 | `}` |
|       - |  554 | `/*` |
|       - |  555 | ` * value key(array $array)` |
|       - |  556 | ` *   Fetch a key from an array` |
|       - |  557 | ` * Parameter` |
|       - |  558 | ` *  $input` |
|       - |  559 | ` *   The input array.` |
|       - |  560 | ` * Return` |
|       - |  561 | ` *  The key() function simply returns the key of the array element that's currently` |
|       - |  562 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - |  563 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - |  564 | ` *  is empty, key() returns NULL.` |
|       - |  565 | ` */` |
|     892 |  566 | `PH7_PRIVATE int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  567 | `{` |
|       - |  568 | `	ph7_hashmap_node *pCur;` |
|       - |  569 | `	ph7_hashmap *pMap;` |
|     893 |  570 | `	if( nArg < 1 ){` |
|       - |  571 | `		/* Missing arguments,return NULL */` |
|     ! 0 |  572 | `		ph7_result_null(pCtx);` |
|     ! 0 |  573 | `		return PH7_OK;` |
|       - |  574 | `	}` |
|       - |  575 | `	/* Make sure we are dealing with a valid hashmap */` |
|     893 |  576 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  577 | `		/* Invalid argument,return NULL */` |
|     ! 0 |  578 | `		ph7_result_null(pCtx);` |
|     ! 0 |  579 | `		return PH7_OK;` |
|       - |  580 | `	}` |
|     893 |  581 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     893 |  582 | `	pCur = pMap->pCur;` |
|     893 |  583 | `	if( pCur == 0 ){` |
|       - |  584 | `		/* Cursor does not point to anything,return NULL */` |
|     137 |  585 | `		ph7_result_null(pCtx);` |
|     137 |  586 | `		return PH7_OK;` |
|       - |  587 | `	}` |
|     757 |  588 | `	HashmapResultNodeKey(pCtx,pCur);` |
|     757 |  589 | `	return PH7_OK;` |
|     447 |  590 | `}` |
|       - |  591 | `/*` |
|       - |  592 | ` * array each(array $input)` |
|       - |  593 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|       - |  594 | ` * Parameter` |
|       - |  595 | ` *  $input` |
|       - |  596 | ` *    The input array.` |
|       - |  597 | ` * Return` |
|       - |  598 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|       - |  599 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|       - |  600 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|       - |  601 | ` *  If the internal pointer for the array points past the end of the array contents` |
|       - |  602 | ` *  each() returns FALSE.` |
|       - |  603 | ` */` |
|      22 |  604 | `PH7_PRIVATE int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  605 | `{` |
|       - |  606 | `	ph7_hashmap_node *pCur;` |
|       - |  607 | `	ph7_hashmap *pMap;` |
|       - |  608 | `	ph7_value *pArray;` |
|       - |  609 | `	ph7_value *pVal;` |
|       - |  610 | `	ph7_value sKey;` |
|      23 |  611 | `	if( nArg < 1 ){` |
|       - |  612 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  613 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  614 | `		return PH7_OK;` |
|       - |  615 | `	}` |
|       - |  616 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 |  617 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  618 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  619 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  620 | `		return PH7_OK;` |
|       - |  621 | `	}` |
|       - |  622 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 |  623 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 |  624 | `	if( pMap->pCur == 0 ){` |
|       - |  625 | `		/* Cursor does not point to anything,return FALSE */` |
|       9 |  626 | `		ph7_result_bool(pCtx,0);` |
|       9 |  627 | `		return PH7_OK;` |
|       - |  628 | `	}` |
|      15 |  629 | `	pCur = pMap->pCur;` |
|       - |  630 | `	/* Create a new array */` |
|      15 |  631 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 |  632 | `	if( pArray == 0 ){` |
|     ! 0 |  633 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  634 | `		return PH7_OK;` |
|       - |  635 | `	}` |
|      15 |  636 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       - |  637 | `	/* Insert the current value */` |
|      15 |  638 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|      15 |  639 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|       - |  640 | `	/* Make the key */` |
|      15 |  641 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|       7 |  642 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|       4 |  643 | `	}else{` |
|       9 |  644 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       9 |  645 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|       - |  646 | `	}` |
|       - |  647 | `	/* Insert the current key */` |
|      15 |  648 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|      15 |  649 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|      15 |  650 | `	PH7_MemObjRelease(&sKey);` |
|       - |  651 | `	/* Advance the cursor */` |
|      15 |  652 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       - |  653 | `	/* Return the current entry */` |
|      15 |  654 | `	ph7_result_value(pCtx,pArray);` |
|      15 |  655 | `	return PH7_OK;` |
|      12 |  656 | `}` |
|       - |  657 | `/*` |
|       - |  658 | ` * range() — a faithful port of php 8.5's ext/standard/array.c implementation` |
|       - |  659 | ` * (php_range_process_input + PHP_FUNCTION(range)), so the value semantics,` |
|       - |  660 | ` * diagnostics, and their ordering are byte-exact: decreasing ranges, float` |
|       - |  661 | ` * ranges, character ranges, the step/endpoint ValueErrors, the ZPP TypeErrors` |
|       - |  662 | ` * and null deprecations, and the string-endpoint warnings.` |
|       - |  663 | ` */` |
|       - |  664 | `#define PH7_RANGE_HT_MAX_SIZE 1073741824 /* php's HT_MAX_SIZE (2^30 entries) */` |
|       - |  665 | `/*` |
|       - |  666 | ` * Endpoint classification, mirroring php_range_process_input's return` |
|       - |  667 | ` * contract. php returns zval type tags whose ORDER encodes the logic` |
|       - |  668 | ` * (IS_LONG < IS_DOUBLE < IS_STRING < IS_ARRAY); the >=/< comparisons in` |
|       - |  669 | ` * ph7_hashmap_range depend on the same ordering here.` |
|       - |  670 | ` *   RANGE_IN_LONG/DOUBLE : only interpretable as int / float` |
|       - |  671 | ` *   RANGE_IN_STRING      : only interpretable as a (char-range) string` |
|       - |  672 | ` *   RANGE_IN_DIGIT       : single-byte numeric string — valid as both a char` |
|       - |  673 | ` *                          and a number (php returns IS_ARRAY for this)` |
|       - |  674 | ` * The RANGE_IN_* codes and RangeStrToNumber are declared in ph7int.h so the` |
|       - |  675 | ` * stage-2 ZPP domain-error sweep can reuse the classifier (PLAN §3.9(a)).` |
|       - |  676 | ` */` |
|       - |  677 | `/* IEEE special-value tests: the engine-wide bit-pattern macros from` |
|       - |  678 | ` * sxtypes.h (via ph7int.h) — same ones the printf/serialize paths use. */` |
|       - |  679 | `/*` |
|       - |  680 | ` * The type name php's ZPP prints after "must be of type ..., X given":` |
|       - |  681 | ` * the concrete class name for objects, the usual type name otherwise.` |
|       - |  682 | ` */` |
|     ! 0 |  683 | `static const char * RangeArgTypeName(ph7_value *pVal,char *zBuf,sxu32 nBufLen)` |
|     ! 0 |  684 | `{` |
|     ! 0 |  685 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 |  686 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|     ! 0 |  687 | `		sxu32 n = SXMIN(pThis->pClass->sName.nByte,nBufLen - 1);` |
|     ! 0 |  688 | `		SyMemcpy((const void *)pThis->pClass->sName.zString,zBuf,n);` |
|     ! 0 |  689 | `		zBuf[n] = 0;` |
|     ! 0 |  690 | `		return zBuf;` |
|       - |  691 | `	}` |
|     ! 0 |  692 | `	return ph7_type_name(pVal);` |
|     ! 0 |  693 | `}` |
|       - |  694 | `/*` |
|       - |  695 | ` * Classify a string with php's is_numeric_string() grammar:` |
|       - |  696 | ` *   [ws] [sign] ( D+ [ . D* ] \| . D+ ) [ (e\|E) [sign] D+ ] [ws]` |
|       - |  697 | ` * — the whole string must be consumed; hex/binary/"INF"/"NAN" are NOT` |
|       - |  698 | ` * numeric. Returns RANGE_IN_LONG with *pLong set, RANGE_IN_DOUBLE with` |
|       - |  699 | ` * *pDouble set (a fractional/exponent form, or an integer too wide for an` |
|       - |  700 | ` * sxi64 — php reclassifies those as float), or RANGE_IN_ERROR when the` |
|       - |  701 | ` * string is not numeric. The float value comes from libc strtod, like` |
|       - |  702 | ` * php's zend_strtod (byte-exact-floats rule). zIn must be NUL-terminated` |
|       - |  703 | ` * at zIn[nLen] — ph7_value_to_string guarantees this (SyBlobNullAppend) —` |
|       - |  704 | ` * so strtod can parse it in place once the grammar has validated it.` |
|       - |  705 | ` */` |
|     110 |  706 | `PH7_PRIVATE sxu8 RangeStrToNumber(const char *zIn,sxu32 nLen,sxi64 *pLong,double *pDouble)` |
|       1 |  707 | `{` |
|     111 |  708 | `	const char *z = zIn,*zEnd = &zIn[nLen];` |
|     111 |  709 | `	sxu64 uVal = 0;` |
|     111 |  710 | `	int bNeg = 0,bDigit = 0,bReal = 0,bOverflow = 0;` |
|     121 |  711 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|     111 |  712 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|       3 |  713 | `		bNeg = (z[0] == '-');` |
|       3 |  714 | `		z++;` |
|       1 |  715 | `	}` |
|     173 |  716 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|      63 |  717 | `		int d = z[0] - '0';` |
|       - |  718 | `		/* Track overflow past 2^63, the widest magnitude an sxi64 can carry` |
|       - |  719 | `		 * (as LONG_MIN); overflowing integers become floats like in php. */` |
|      63 |  720 | `		if( uVal > 922337203685477580ULL \|\| (uVal == 922337203685477580ULL && d > 8) ){` |
|     ! 0 |  721 | `			bOverflow = 1;` |
|     ! 0 |  722 | `		}else{` |
|      63 |  723 | `			uVal = uVal * 10 + (sxu64)d;` |
|       - |  724 | `		}` |
|      63 |  725 | `		bDigit = 1;` |
|      63 |  726 | `		z++;` |
|       1 |  727 | `	}` |
|     111 |  728 | `	if( z < zEnd && z[0] == '.' ){` |
|       3 |  729 | `		bReal = 1;` |
|       3 |  730 | `		z++;` |
|       5 |  731 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|       3 |  732 | `			bDigit = 1;` |
|       3 |  733 | `			z++;` |
|       1 |  734 | `		}` |
|       1 |  735 | `	}` |
|       - |  736 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|     111 |  737 | `	if( !bDigit ){` |
|      37 |  738 | `		return RANGE_IN_ERROR;` |
|       - |  739 | `	}` |
|       - |  740 | `	/* Optional exponent — needs at least one digit (rejects "1e", "1e+"). */` |
|      75 |  741 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|       9 |  742 | `		z++;` |
|       9 |  743 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){ z++; }` |
|       9 |  744 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|     ! 0 |  745 | `			return RANGE_IN_ERROR;` |
|       - |  746 | `		}` |
|       9 |  747 | `		bReal = 1;` |
|      17 |  748 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){ z++; }` |
|       4 |  749 | `	}` |
|       - |  750 | `	/* Trailing whitespace allowed; anything else means not numeric. */` |
|      79 |  751 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|      75 |  752 | `	if( z != zEnd ){` |
|      11 |  753 | `		return RANGE_IN_ERROR;` |
|       - |  754 | `	}` |
|      64 |  755 | `	if( bOverflow \|\| (!bNeg && uVal > (sxu64)LARGEST_INT64)` |
|      33 |  756 | `	 \|\| (bNeg && uVal > (sxu64)LARGEST_INT64 + 1) ){` |
|      64 |  757 | `		bReal = 1;` |
|      64 |  758 | `	}` |
|      33 |  759 | `	if( bReal ){` |
|      11 |  760 | `		*pDouble = strtod(zIn,0);` |
|      11 |  761 | `		return RANGE_IN_DOUBLE;` |
|       - |  762 | `	}` |
|       - |  763 | `	/* Negate in unsigned space so 2^63 lands on LONG_MIN without overflow. */` |
|      23 |  764 | `	*pLong = bNeg ? (sxi64)((sxu64)0 - uVal) : (sxi64)uVal;` |
|      23 |  765 | `	return RANGE_IN_LONG;` |
|      40 |  766 | `}` |
|       - |  767 | `/*` |
|       - |  768 | ` * ZPP emulation for $start/$end (php's Z_PARAM_NUMBER_OR_STR, weak mode):` |
|       - |  769 | ` * reject array/object/resource with php's TypeError, deprecate null (the` |
|       - |  770 | ` * value then reads as int 0 — *pbNullCoerced). php runs this for all` |
|       - |  771 | ` * arguments BEFORE any value/domain check, hence the split from` |
|       - |  772 | ` * RangeProcessInput below. Returns FALSE after throwing (*pRc set).` |
|       - |  773 | ` */` |
|     272 |  774 | `static int RangeEndpointZpp(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,int *pbNullCoerced,sxi32 *pRc)` |
|       1 |  775 | `{` |
|     136 |  776 | `	SXUNUSED(pbNullCoerced); /* php coerces null to 0 with a deprecation; PHL rejects it */` |
|     273 |  777 | `	*pRc = PH7_OK;` |
|     273 |  778 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|       - |  779 | `		char zType[80];` |
|     ! 0 |  780 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  781 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, %s given",` |
|     ! 0 |  782 | `			iArg,zName,RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|     ! 0 |  783 | `		return FALSE;` |
|       - |  784 | `	}` |
|     273 |  785 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|       - |  786 | `		/* php only DEPRECATES null here; PHL rejects it. */` |
|     ! 0 |  787 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  788 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, null given",` |
|     ! 0 |  789 | `			iArg,zName);` |
|     ! 0 |  790 | `		return FALSE;` |
|       - |  791 | `	}` |
|     273 |  792 | `	return TRUE;` |
|     137 |  793 | `}` |
|       - |  794 | `/*` |
|       - |  795 | ` * ZPP emulation for $step (php's Z_PARAM_NUMBER, weak mode): int/float pass` |
|       - |  796 | ` * through, bool coerces to int, null deprecates to int 0 (which then trips` |
|       - |  797 | ` * the "cannot be 0" ValueError like php), a numeric string coerces to its` |
|       - |  798 | ` * number, anything else is a TypeError. Returns RANGE_IN_LONG/DOUBLE, or` |
|       - |  799 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|       - |  800 | ` */` |
|      54 |  801 | `static sxu8 RangeStepInput(ph7_context *pCtx,ph7_value *pIn,sxi64 *pLong,double *pDouble,sxi32 *pRc)` |
|       1 |  802 | `{` |
|      55 |  803 | `	*pRc = PH7_OK;` |
|      55 |  804 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|       - |  805 | `		char zType[80];` |
|     ! 0 |  806 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  807 | `			"range(): Argument #3 ($step) must be of type int\|float, %s given",` |
|     ! 0 |  808 | `			RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|     ! 0 |  809 | `		return RANGE_IN_ERROR;` |
|       - |  810 | `	}` |
|      55 |  811 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|       - |  812 | `		/* php only DEPRECATES null here; PHL rejects it. */` |
|     ! 0 |  813 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  814 | `			"range(): Argument #3 ($step) must be of type int\|float, null given");` |
|     ! 0 |  815 | `		return RANGE_IN_ERROR;` |
|       - |  816 | `	}` |
|      55 |  817 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|      21 |  818 | `		*pDouble = ph7_value_to_double(pIn);` |
|      21 |  819 | `		return RANGE_IN_DOUBLE;` |
|       - |  820 | `	}` |
|      35 |  821 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|       - |  822 | `		const char *zStr;` |
|       - |  823 | `		int nLen;` |
|       - |  824 | `		sxu8 iKind;` |
|       3 |  825 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|       3 |  826 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|       3 |  827 | `		if( iKind == RANGE_IN_ERROR ){` |
|       3 |  828 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  829 | `				"range(): Argument #3 ($step) must be of type int\|float, string given");` |
|       1 |  830 | `		}` |
|       3 |  831 | `		return iKind;` |
|       - |  832 | `	}` |
|       - |  833 | `	/* int / bool */` |
|      33 |  834 | `	*pLong = ph7_value_to_int64(pIn);` |
|      33 |  835 | `	return RANGE_IN_LONG;` |
|      28 |  836 | `}` |
|       - |  837 | `/*` |
|       - |  838 | ` * php_range_process_input port: resolve $start/$end into a number and/or a` |
|       - |  839 | ` * char-range byte, emitting php's exact warnings (empty string, multi-byte` |
|       - |  840 | ` * string) and ValueErrors (INF/NAN). Returns a RANGE_IN_* code, or` |
|       - |  841 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|       - |  842 | ` */` |
|     244 |  843 | `static sxu8 RangeProcessInput(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,` |
|       - |  844 | `	int bNullCoerced,sxi64 *pLong,double *pDouble,unsigned char *pChar,sxi32 *pRc)` |
|       1 |  845 | `{` |
|       - |  846 | `	char zMsg[160];` |
|       - |  847 | `	double r;` |
|     245 |  848 | `	*pRc = PH7_OK;` |
|     245 |  849 | `	if( bNullCoerced ){` |
|       - |  850 | `		/* ZPP already deprecated the null; it reads as int 0. */` |
|     ! 0 |  851 | `		*pLong = 0;` |
|     ! 0 |  852 | `		*pDouble = 0.0;` |
|     ! 0 |  853 | `		return RANGE_IN_LONG;` |
|       - |  854 | `	}` |
|     245 |  855 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|      21 |  856 | `		r = ph7_value_to_double(pIn);` |
|      12 |  857 | `check_dval:` |
|      25 |  858 | `		if( PH7_IS_INF(r) ){` |
|       7 |  859 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|       2 |  860 | `				"range(): Argument #%d ($%s) must be a finite number, INF provided",iArg,zName);` |
|       5 |  861 | `			return RANGE_IN_ERROR;` |
|       - |  862 | `		}` |
|      21 |  863 | `		if( PH7_IS_NAN(r) ){` |
|       7 |  864 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|       2 |  865 | `				"range(): Argument #%d ($%s) must be a finite number, NAN provided",iArg,zName);` |
|       5 |  866 | `			return RANGE_IN_ERROR;` |
|       - |  867 | `		}` |
|      17 |  868 | `		*pDouble = r;` |
|      17 |  869 | `		return RANGE_IN_DOUBLE;` |
|       - |  870 | `	}` |
|     225 |  871 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|       - |  872 | `		const char *zStr;` |
|       - |  873 | `		int nLen;` |
|       - |  874 | `		sxu8 iKind;` |
|      41 |  875 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|      41 |  876 | `		if( nLen == 0 ){` |
|     ! 0 |  877 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|     ! 0 |  878 | `				"range(): Argument #%d ($%s) must not be empty, casted to 0",iArg,zName);` |
|     ! 0 |  879 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|     ! 0 |  880 | `			*pLong = 0;` |
|     ! 0 |  881 | `			*pDouble = 0.0;` |
|     ! 0 |  882 | `			return RANGE_IN_LONG;` |
|       - |  883 | `		}` |
|      41 |  884 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|      41 |  885 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|       5 |  886 | `			r = *pDouble;` |
|       5 |  887 | `			goto check_dval;` |
|       - |  888 | `		}` |
|      37 |  889 | `		if( iKind == RANGE_IN_LONG ){` |
|      13 |  890 | `			*pDouble = (double)*pLong;` |
|      13 |  891 | `			if( nLen == 1 ){` |
|       - |  892 | `				/* A single numeric digit works as both a char and a number. */` |
|       5 |  893 | `				*pChar = (unsigned char)zStr[0];` |
|       5 |  894 | `				return RANGE_IN_DIGIT;` |
|       - |  895 | `			}` |
|       9 |  896 | `			return RANGE_IN_LONG;` |
|       - |  897 | `		}` |
|      25 |  898 | `		if( nLen != 1 ){` |
|     ! 0 |  899 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|     ! 0 |  900 | `				"range(): Argument #%d ($%s) must be a single byte, subsequent bytes are ignored",iArg,zName);` |
|     ! 0 |  901 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|     ! 0 |  902 | `		}` |
|      25 |  903 | `		*pChar = (unsigned char)zStr[0];` |
|       - |  904 | `		/* Fall-back numeric value in case the other argument is not a string. */` |
|      25 |  905 | `		*pLong = 0;` |
|      25 |  906 | `		*pDouble = 0.0;` |
|      25 |  907 | `		return RANGE_IN_STRING;` |
|       - |  908 | `	}` |
|       - |  909 | `	/* int / bool */` |
|     185 |  910 | `	*pLong = ph7_value_to_int64(pIn);` |
|     185 |  911 | `	*pDouble = (double)*pLong;` |
|     185 |  912 | `	return RANGE_IN_LONG;` |
|     123 |  913 | `}` |
|       - |  914 | `/*` |
|       - |  915 | ` * The two "supplied range exceeds the maximum array size" ValueErrors.` |
|       - |  916 | ` * Both php messages print the macro's (start,end) parameters, which its` |
|       - |  917 | ` * callers pass SWAPPED for a decreasing range — a php quirk kept for` |
|       - |  918 | ` * byte-parity (callers below pass the values to *print*). The int and` |
|       - |  919 | ` * float variants differ in wording ("Maximum size: N." vs "Max size: N")` |
|       - |  920 | ` * exactly like php's two macros.` |
|       - |  921 | ` */` |
|       6 |  922 | `static sxi32 RangeLongSizeError(ph7_context *pCtx,sxu64 nCalc,sxi64 iStart,sxi64 iEnd,sxi64 iStep)` |
|       1 |  923 | `{` |
|      10 |  924 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  925 | `		"The supplied range exceeds the maximum array size by %qu elements: "` |
|       - |  926 | `		"start=%qd, end=%qd, step=%qd. Calculated size: %qu. Maximum size: %qu.",` |
|       3 |  927 | `		nCalc - (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1),iStart,iEnd,iStep,` |
|       3 |  928 | `		nCalc,(sxu64)PH7_RANGE_HT_MAX_SIZE);` |
|       1 |  929 | `}` |
|       6 |  930 | `static sxi32 RangeDoubleSizeError(ph7_context *pCtx,double rCalc,double rStart,double rEnd,double rStep)` |
|       1 |  931 | `{` |
|       - |  932 | `	/* Four %.1f doubles can reach ~313 bytes each near DBL_MAX, so format on` |
|       - |  933 | `	 * the VM heap (auto-released with the call context) rather than parking` |
|       - |  934 | `	 * ~1.5 KB on the native stack of a small-stack embedded port. */` |
|       7 |  935 | `	const unsigned int nBuf = 1500;` |
|       7 |  936 | `	char *zMsg = (char *)ph7_context_alloc_chunk(pCtx,nBuf,FALSE,TRUE/* Auto-release */);` |
|       7 |  937 | `	if( zMsg == 0 ){` |
|     ! 0 |  938 | `		return PH7_ContextMemoryError(pCtx);` |
|       - |  939 | `	}` |
|       7 |  940 | `	snprintf(zMsg,nBuf,` |
|       - |  941 | `		"The supplied range exceeds the maximum array size by %.1f elements: "` |
|       - |  942 | `		"start=%.1f, end=%.1f, step=%.1f. Max size: 1073741824",` |
|       - |  943 | `		rCalc - (double)PH7_RANGE_HT_MAX_SIZE,rStart,rEnd,rStep);` |
|       7 |  944 | `	return PH7_VmThrowException(pCtx,"ValueError","%s",zMsg);` |
|       4 |  945 | `}` |
|       - |  946 | `/*` |
|       - |  947 | ` * Set the element container to the next range element and append it to the` |
|       - |  948 | ` * result array, surfacing allocation failure as the OOM fatal (never a` |
|       - |  949 | ` * silently-truncated array). One helper per element type so the fill loops` |
|       - |  950 | ` * below stay one line per iteration.` |
|       - |  951 | ` */` |
|  401472 |  952 | `static sxi32 RangeAppendInt(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,sxi64 iVal)` |
|       1 |  953 | `{` |
|  401473 |  954 | `	ph7_value_int64(pValue,iVal);` |
|  401473 |  955 | `	if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|     ! 0 |  956 | `		return PH7_ContextMemoryError(pCtx);` |
|       - |  957 | `	}` |
|  401473 |  958 | `	return PH7_OK;` |
|  200737 |  959 | `}` |
|      50 |  960 | `static sxi32 RangeAppendDouble(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,double rVal)` |
|       1 |  961 | `{` |
|      51 |  962 | `	ph7_value_double(pValue,rVal);` |
|      51 |  963 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|     ! 0 |  964 | `		return PH7_ContextMemoryError(pCtx);` |
|       - |  965 | `	}` |
|      51 |  966 | `	return PH7_OK;` |
|      26 |  967 | `}` |
|     148 |  968 | `static sxi32 RangeAppendChar(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,char c)` |
|       1 |  969 | `{` |
|     149 |  970 | `	ph7_value_string(pValue,&c,1);` |
|     149 |  971 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|     ! 0 |  972 | `		return PH7_ContextMemoryError(pCtx);` |
|       - |  973 | `	}` |
|     149 |  974 | `	ph7_value_reset_string_cursor(pValue);` |
|     149 |  975 | `	return PH7_OK;` |
|      75 |  976 | `}` |
|       - |  977 | `/*` |
|       - |  978 | ` * array range(string\|int\|float $start,string\|int\|float $end,int\|float $step = 1)` |
|       - |  979 | ` *  Create an array containing a range of elements.` |
|       - |  980 | ` * Return` |
|       - |  981 | ` *  An array of elements from start to end, inclusive; int, float, or` |
|       - |  982 | ` *  single-character string elements depending on the inputs, like php 8.` |
|       - |  983 | ` */` |
|     136 |  984 | `PH7_PRIVATE int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  985 | `{` |
|       - |  986 | `	ph7_value *pValue,*pArray;` |
|     137 |  987 | `	sxi32 rc = PH7_OK;` |
|     137 |  988 | `	int is_step_double = 0,is_step_negative = 0;` |
|     137 |  989 | `	double step_double = 1.0;` |
|     137 |  990 | `	sxi64 step = 1;` |
|       - |  991 | `	sxu8 start_type,end_type;` |
|     137 |  992 | `	sxi64 start_long = 0,end_long = 0;` |
|     137 |  993 | `	double start_double = 0.0,end_double = 0.0;` |
|     137 |  994 | `	unsigned char cStart = 0,cEnd = 0;` |
|     137 |  995 | `	int bStartNull = FALSE,bEndNull = FALSE;` |
|       - |  996 | `	sxu32 i,size;` |
|       - |  997 |  |
|       - |  998 | `	/* php ZPP arity: at least 2 (enforced centrally, aBuiltinArity), at most 3. */` |
|     137 |  999 | `	if( nArg > 3 ){` |
|     ! 0 | 1000 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|     ! 0 | 1001 | `			"range() expects at most 3 arguments, %d given",nArg);` |
|       - | 1002 | `	}` |
|     137 | 1003 | `	if( nArg < 2 ){` |
|       - | 1004 | `		/* Defensive only: the central arity table throws before we run. */` |
|     ! 0 | 1005 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|     ! 0 | 1006 | `			"range() expects at least 2 arguments, %d given",nArg);` |
|       - | 1007 | `	}` |
|       - | 1008 | `	/* ZPP pass in argument order: type errors and null deprecations fire` |
|       - | 1009 | `	 * before any value/domain check, like php's zend_parse_parameters. */` |
|     137 | 1010 | `	if( !RangeEndpointZpp(pCtx,apArg[0],1,"start",&bStartNull,&rc) ){` |
|     ! 0 | 1011 | `		return rc;` |
|       - | 1012 | `	}` |
|     137 | 1013 | `	if( !RangeEndpointZpp(pCtx,apArg[1],2,"end",&bEndNull,&rc) ){` |
|     ! 0 | 1014 | `		return rc;` |
|       - | 1015 | `	}` |
|     137 | 1016 | `	if( nArg > 2 ){` |
|      55 | 1017 | `		sxu8 iStepKind = RangeStepInput(pCtx,apArg[2],&step,&step_double,&rc);` |
|      55 | 1018 | `		if( iStepKind == RANGE_IN_ERROR ){` |
|       3 | 1019 | `			return rc;` |
|       - | 1020 | `		}` |
|      53 | 1021 | `		if( iStepKind == RANGE_IN_DOUBLE ){` |
|      21 | 1022 | `			if( PH7_IS_INF(step_double) ){` |
|       3 | 1023 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1024 | `					"range(): Argument #3 ($step) must be a finite number, INF provided");` |
|       - | 1025 | `			}` |
|      19 | 1026 | `			if( PH7_IS_NAN(step_double) ){` |
|       3 | 1027 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1028 | `					"range(): Argument #3 ($step) must be a finite number, NAN provided");` |
|       - | 1029 | `			}` |
|       - | 1030 | `			/* We only want positive step values. */` |
|      17 | 1031 | `			if( step_double < 0.0 ){` |
|     ! 0 | 1032 | `				is_step_negative = 1;` |
|     ! 0 | 1033 | `				step_double *= -1;` |
|     ! 0 | 1034 | `			}` |
|       - | 1035 | `			/* zend_dval_to_lval_silent + zend_is_long_compatible: an integral` |
|       - | 1036 | `			 * in-sxi64-range float step behaves as an int (char ranges accept` |
|       - | 1037 | `			 * it, int endpoints stay int); anything else is a float step. */` |
|      17 | 1038 | `			if( step_double < 9223372036854775808.0 ){` |
|      15 | 1039 | `				step = (sxi64)step_double;` |
|      15 | 1040 | `				if( (double)step != step_double ){` |
|      13 | 1041 | `					is_step_double = 1;` |
|       6 | 1042 | `				}` |
|       8 | 1043 | `			}else{` |
|       - | 1044 | ``				/* Casting out-of-range would be UB; `step` stays unread —`` |
|       - | 1045 | `				 * every reader is gated behind !is_step_double. */` |
|       3 | 1046 | `				is_step_double = 1;` |
|       - | 1047 | `			}` |
|       9 | 1048 | `		}else{` |
|       - | 1049 | `			/* We only want positive step values. */` |
|      33 | 1050 | `			if( step < 0 ){` |
|      11 | 1051 | `				if( step == SMALLEST_INT64 ){` |
|       - | 1052 | `					/* -step would overflow */` |
|       4 | 1053 | `					return PH7_VmThrowException(pCtx,"ValueError",` |
|       1 | 1054 | `						"range(): Argument #3 ($step) must be greater than %qd",step);` |
|       - | 1055 | `				}` |
|       9 | 1056 | `				is_step_negative = 1;` |
|       9 | 1057 | `				step = -step;` |
|       4 | 1058 | `			}` |
|      31 | 1059 | `			step_double = (double)step;` |
|       - | 1060 | `		}` |
|      47 | 1061 | `		if( step_double == 0.0 ){` |
|       5 | 1062 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1063 | `				"range(): Argument #3 ($step) cannot be 0");` |
|       - | 1064 | `		}` |
|      21 | 1065 | `	}` |
|     125 | 1066 | `	start_type = RangeProcessInput(pCtx,apArg[0],1,"start",bStartNull,&start_long,&start_double,&cStart,&rc);` |
|     125 | 1067 | `	if( start_type == RANGE_IN_ERROR ){` |
|       5 | 1068 | `		return rc;` |
|       - | 1069 | `	}` |
|     121 | 1070 | `	end_type = RangeProcessInput(pCtx,apArg[1],2,"end",bEndNull,&end_long,&end_double,&cEnd,&rc);` |
|     121 | 1071 | `	if( end_type == RANGE_IN_ERROR ){` |
|       5 | 1072 | `		return rc;` |
|       - | 1073 | `	}` |
|       - | 1074 | `	/* Element container + result array */` |
|     117 | 1075 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     117 | 1076 | `	pArray = ph7_context_new_array(pCtx);` |
|     117 | 1077 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|     ! 0 | 1078 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1079 | `	}` |
|       - | 1080 | `	/* If the range is given as strings, generate an array of characters. */` |
|     117 | 1081 | `	if( start_type >= RANGE_IN_STRING \|\| end_type >= RANGE_IN_STRING ){` |
|      15 | 1082 | `		if( start_type < RANGE_IN_STRING \|\| end_type < RANGE_IN_STRING ){` |
|       - | 1083 | `			/* Only one side is a string: the char side converts to 0 (with a` |
|       - | 1084 | `			 * warning unless the numeric side is an ambiguous single digit)` |
|       - | 1085 | `			 * and the range is numeric. */` |
|     ! 0 | 1086 | `			if( start_type < RANGE_IN_STRING ){` |
|     ! 0 | 1087 | `				if( end_type != RANGE_IN_DIGIT ){` |
|     ! 0 | 1088 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 1089 | `						"range(): Argument #1 ($start) must be a single byte string if"` |
|       - | 1090 | `						" argument #2 ($end) is a single byte string, argument #2 ($end) converted to 0");` |
|     ! 0 | 1091 | `				}` |
|     ! 0 | 1092 | `				end_type = RANGE_IN_LONG;` |
|     ! 0 | 1093 | `			}else{` |
|     ! 0 | 1094 | `				if( start_type != RANGE_IN_DIGIT ){` |
|     ! 0 | 1095 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 1096 | `						"range(): Argument #2 ($end) must be a single byte string if"` |
|       - | 1097 | `						" argument #1 ($start) is a single byte string, argument #1 ($start) converted to 0");` |
|     ! 0 | 1098 | `				}` |
|     ! 0 | 1099 | `				start_type = RANGE_IN_LONG;` |
|       - | 1100 | `			}` |
|     ! 0 | 1101 | `			goto handle_numeric_inputs;` |
|       - | 1102 | `		}` |
|      15 | 1103 | `		if( is_step_double ){` |
|       - | 1104 | `			/* Only emit the warning if one of the inputs is not a numeric digit. */` |
|     ! 0 | 1105 | `			if( start_type == RANGE_IN_STRING \|\| end_type == RANGE_IN_STRING ){` |
|     ! 0 | 1106 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 1107 | `					"range(): Argument #3 ($step) must be of type int when generating an array"` |
|       - | 1108 | `					" of characters, inputs converted to 0");` |
|     ! 0 | 1109 | `			}` |
|     ! 0 | 1110 | `			start_type = RANGE_IN_LONG;` |
|     ! 0 | 1111 | `			end_type = RANGE_IN_LONG;` |
|     ! 0 | 1112 | `			goto handle_numeric_inputs;` |
|       - | 1113 | `		}` |
|       - | 1114 | `		/* Generate an array of characters */` |
|      15 | 1115 | `		if( cStart > cEnd ){` |
|       - | 1116 | `			/* Decreasing char range */` |
|       - | 1117 | `			int iCur;` |
|       3 | 1118 | `			if( (sxi64)(cStart - cEnd) < step ){` |
|     ! 0 | 1119 | `				goto boundary_error;` |
|       - | 1120 | `			}` |
|      17 | 1121 | `			for( iCur = (int)cStart ; iCur >= (int)cEnd ; iCur -= (int)step ){` |
|      15 | 1122 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|     ! 0 | 1123 | `					return rc;` |
|       - | 1124 | `				}` |
|       8 | 1125 | `			}` |
|      14 | 1126 | `		}else if( cEnd > cStart ){` |
|       - | 1127 | `			/* Increasing char range */` |
|       - | 1128 | `			int iCur;` |
|      11 | 1129 | `			if( is_step_negative ){` |
|       3 | 1130 | `				goto negative_step_error;` |
|       - | 1131 | `			}` |
|       9 | 1132 | `			if( (sxi64)(cEnd - cStart) < step ){` |
|       3 | 1133 | `				goto boundary_error;` |
|       - | 1134 | `			}` |
|     139 | 1135 | `			for( iCur = (int)cStart ; iCur <= (int)cEnd ; iCur += (int)step ){` |
|     133 | 1136 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|     ! 0 | 1137 | `					return rc;` |
|       - | 1138 | `				}` |
|      67 | 1139 | `			}` |
|       4 | 1140 | `		}else{` |
|       3 | 1141 | `			if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)cStart)) != PH7_OK ){` |
|     ! 0 | 1142 | `				return rc;` |
|       - | 1143 | `			}` |
|       - | 1144 | `		}` |
|      11 | 1145 | `		ph7_result_value(pCtx,pArray);` |
|      11 | 1146 | `		return PH7_OK;` |
|       - | 1147 | `	}` |
|      51 | 1148 | `handle_numeric_inputs:` |
|     109 | 1149 | `	if( start_type == RANGE_IN_DOUBLE \|\| end_type == RANGE_IN_DOUBLE \|\| is_step_double ){` |
|       - | 1150 | `		/* Float range */` |
|       - | 1151 | `		double elem,calc;` |
|      21 | 1152 | `		if( start_double > end_double ){` |
|       - | 1153 | `			/* Decreasing float range */` |
|       7 | 1154 | `			if( start_double - end_double < step_double ){` |
|     ! 0 | 1155 | `				goto boundary_error;` |
|       - | 1156 | `			}` |
|       7 | 1157 | `			calc = ((start_double - end_double) / step_double) + 1;` |
|       7 | 1158 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|       - | 1159 | `				/* php prints start/end swapped here (see RangeDoubleSizeError). */` |
|       3 | 1160 | `				return RangeDoubleSizeError(pCtx,calc,end_double,start_double,step_double);` |
|       - | 1161 | `			}` |
|       5 | 1162 | `			size = (sxu32)(calc + 0.5); /* _php_math_round(...,0,HALF_UP) */` |
|      19 | 1163 | `			for( i = 0,elem = start_double ; i < size && elem >= end_double ; ++i,elem = start_double - ((double)i * step_double) ){` |
|      15 | 1164 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|     ! 0 | 1165 | `					return rc;` |
|       - | 1166 | `				}` |
|       8 | 1167 | `			}` |
|      17 | 1168 | `		}else if( end_double > start_double ){` |
|       - | 1169 | `			/* Increasing float range */` |
|      15 | 1170 | `			if( is_step_negative ){` |
|     ! 0 | 1171 | `				goto negative_step_error;` |
|       - | 1172 | `			}` |
|      15 | 1173 | `			if( end_double - start_double < step_double ){` |
|       3 | 1174 | `				goto boundary_error;` |
|       - | 1175 | `			}` |
|      13 | 1176 | `			calc = ((end_double - start_double) / step_double) + 1;` |
|      13 | 1177 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|       5 | 1178 | `				return RangeDoubleSizeError(pCtx,calc,start_double,end_double,step_double);` |
|       - | 1179 | `			}` |
|       9 | 1180 | `			size = (sxu32)(calc + 0.5);` |
|      45 | 1181 | `			for( i = 0,elem = start_double ; i < size && elem <= end_double ; ++i,elem = start_double + ((double)i * step_double) ){` |
|      37 | 1182 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|     ! 0 | 1183 | `					return rc;` |
|       - | 1184 | `				}` |
|      19 | 1185 | `			}` |
|       5 | 1186 | `		}else{` |
|     ! 0 | 1187 | `			if( (rc = RangeAppendDouble(pCtx,pArray,pValue,start_double)) != PH7_OK ){` |
|     ! 0 | 1188 | `				return rc;` |
|       - | 1189 | `			}` |
|       - | 1190 | `		}` |
|       7 | 1191 | `	}else{` |
|       - | 1192 | `		/* Int range. All arithmetic in unsigned space so a span wider than` |
|       - | 1193 | `		 * LARGEST_INT64 (e.g. -PHP_INT_MAX..PHP_INT_MAX) wraps correctly` |
|       - | 1194 | `		 * instead of overflowing, exactly like php's zend_ulong math. */` |
|      83 | 1195 | `		sxu64 ustep = (sxu64)step;` |
|       - | 1196 | `		sxu64 calc;` |
|      83 | 1197 | `		if( start_long > end_long ){` |
|       - | 1198 | `			/* Decreasing int range */` |
|      13 | 1199 | `			if( (sxu64)start_long - (sxu64)end_long < ustep ){` |
|       3 | 1200 | `				goto boundary_error;` |
|       - | 1201 | `			}` |
|      11 | 1202 | `			calc = ((sxu64)start_long - (sxu64)end_long) / ustep;` |
|      11 | 1203 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|       - | 1204 | `				/* php prints start/end swapped here (see RangeLongSizeError). */` |
|       3 | 1205 | `				return RangeLongSizeError(pCtx,calc,end_long,start_long,step);` |
|       - | 1206 | `			}` |
|       9 | 1207 | `			size = (sxu32)(calc + 1);` |
|      55 | 1208 | `			for( i = 0 ; i < size ; ++i ){` |
|      47 | 1209 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long - (sxu64)i * ustep))) != PH7_OK ){` |
|     ! 0 | 1210 | `					return rc;` |
|       - | 1211 | `				}` |
|      24 | 1212 | `			}` |
|      75 | 1213 | `		}else if( end_long > start_long ){` |
|       - | 1214 | `			/* Increasing int range */` |
|      69 | 1215 | `			if( is_step_negative ){` |
|       3 | 1216 | `				goto negative_step_error;` |
|       - | 1217 | `			}` |
|      67 | 1218 | `			if( (sxu64)end_long - (sxu64)start_long < ustep ){` |
|       3 | 1219 | `				goto boundary_error;` |
|       - | 1220 | `			}` |
|      65 | 1221 | `			calc = ((sxu64)end_long - (sxu64)start_long) / ustep;` |
|      65 | 1222 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|       5 | 1223 | `				return RangeLongSizeError(pCtx,calc,start_long,end_long,step);` |
|       - | 1224 | `			}` |
|      61 | 1225 | `			size = (sxu32)(calc + 1);` |
|  401485 | 1226 | `			for( i = 0 ; i < size ; ++i ){` |
|  401425 | 1227 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long + (sxu64)i * ustep))) != PH7_OK ){` |
|     ! 0 | 1228 | `					return rc;` |
|       - | 1229 | `				}` |
|  200713 | 1230 | `			}` |
|      31 | 1231 | `		}else{` |
|       3 | 1232 | `			if( (rc = RangeAppendInt(pCtx,pArray,pValue,start_long)) != PH7_OK ){` |
|     ! 0 | 1233 | `				return rc;` |
|       - | 1234 | `			}` |
|       - | 1235 | `		}` |
|       - | 1236 | `	}` |
|       - | 1237 | `	/* Return the new array. 'pValue' is released automatically by the` |
|       - | 1238 | `	 * virtual machine as soon as we return from this foreign function. */` |
|      83 | 1239 | `	ph7_result_value(pCtx,pArray);` |
|      83 | 1240 | `	return PH7_OK;` |
|       2 | 1241 | `negative_step_error:` |
|       5 | 1242 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1243 | `		"range(): Argument #3 ($step) must be greater than 0 for increasing ranges");` |
|       4 | 1244 | `boundary_error:` |
|       9 | 1245 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1246 | `		"range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)");` |
|      69 | 1247 | `}` |
|       - | 1248 | `/*` |
|       - | 1249 | ` * array array_values(array $array)` |
|       - | 1250 | ` *  Return all the values of an array, indexed numerically.` |
|       - | 1251 | ` * Parameters` |
|       - | 1252 | ` *  $array` |
|       - | 1253 | ` *   The input array.` |
|       - | 1254 | ` * Return` |
|       - | 1255 | ` *  An indexed array of values or NULL on allocation failure.` |
|       - | 1256 | ` */` |
|      46 | 1257 | `PH7_PRIVATE int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1258 | `{` |
|       - | 1259 | `	ph7_hashmap_node *pNode;` |
|       - | 1260 | `	ph7_hashmap *pMap;` |
|       - | 1261 | `	ph7_value *pArray;` |
|       - | 1262 | `	ph7_value *pObj;` |
|       - | 1263 | `	sxu32 n;` |
|      49 | 1264 | `	if( nArg != 1 ){` |
|       - | 1265 | `		/* Wrong argument count, throw ArgumentCountError */` |
|     ! 0 | 1266 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1267 | `			"ArgumentCountError",` |
|       - | 1268 | `			"array_values() expects exactly 1 argument, %d given",` |
|     ! 0 | 1269 | `			nArg` |
|       - | 1270 | `			);` |
|       - | 1271 | `	}` |
|       - | 1272 | `	/* Make sure we are dealing with a valid hashmap */` |
|      49 | 1273 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 1274 | `		/* Type mismatch, throw TypeError */` |
|       4 | 1275 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1276 | `			"TypeError",` |
|       - | 1277 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 1278 | `			ph7_type_name(apArg[0])` |
|       - | 1279 | `			);` |
|       - | 1280 | `	}` |
|       - | 1281 | `	/* Point to the internal representation that describe the input hashmap */` |
|      46 | 1282 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 1283 | `	/* Create a new array */` |
|      46 | 1284 | `	pArray = ph7_context_new_array(pCtx);` |
|      46 | 1285 | `	if( pArray == 0 ){` |
|     ! 0 | 1286 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1287 | `		return PH7_OK;` |
|       - | 1288 | `	}` |
|       - | 1289 | `	/* Perform the requested operation */` |
|      46 | 1290 | `	pNode = pMap->pFirst;` |
|     144 | 1291 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     100 | 1292 | `		pObj = HashmapExtractNodeValue(pNode);` |
|     100 | 1293 | `		if( pObj ){` |
|       - | 1294 | `			/* perform the insertion */` |
|     100 | 1295 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|      49 | 1296 | `		}` |
|       - | 1297 | `		/* Point to the next entry */` |
|     100 | 1298 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      51 | 1299 | `	}` |
|       - | 1300 | `	/* return the new array */` |
|      46 | 1301 | `	ph7_result_value(pCtx,pArray);` |
|      46 | 1302 | `	return PH7_OK;` |
|      26 | 1303 | `}` |
|       - | 1304 | `/*` |
|       - | 1305 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|       - | 1306 | ` *  Return all the keys or a subset of the keys of an array.` |
|       - | 1307 | ` * Parameters` |
|       - | 1308 | ` *  $input` |
|       - | 1309 | ` *   An array containing keys to return.` |
|       - | 1310 | ` * $search_value` |
|       - | 1311 | ` *   If specified, then only keys containing these values are returned.` |
|       - | 1312 | ` * $strict` |
|       - | 1313 | ` *   Determines if strict comparison (===) should be used during the search.` |
|       - | 1314 | ` * Return` |
|       - | 1315 | ` *  An array of all the keys in input or NULL on failure.` |
|       - | 1316 | ` */` |
|     196 | 1317 | `PH7_PRIVATE int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1318 | `{` |
|       - | 1319 | `	ph7_hashmap_node *pNode;` |
|       - | 1320 | `	ph7_hashmap *pMap;` |
|       - | 1321 | `	ph7_value *pArray;` |
|       - | 1322 | `	ph7_value sObj;` |
|       - | 1323 | `	ph7_value sVal;` |
|       - | 1324 | `	SyString sKey;` |
|       - | 1325 | `	int bStrict;` |
|       - | 1326 | `	sxi32 rc;` |
|       - | 1327 | `	sxu32 n;` |
|     201 | 1328 | `	if( nArg < 1 ){` |
|       - | 1329 | `		/* Missing argument,throw ArgumentCountError */` |
|     ! 0 | 1330 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1331 | `			"ArgumentCountError",` |
|       - | 1332 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 1333 | `			);` |
|       - | 1334 | `	}` |
|       - | 1335 | `	/* Make sure we are dealing with a valid hashmap */` |
|     201 | 1336 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 1337 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 1338 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1339 | `			"TypeError",` |
|       - | 1340 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 1341 | `			ph7_type_name(apArg[0])` |
|       - | 1342 | `			);` |
|       - | 1343 | `	}` |
|       - | 1344 | `	/* Point to the internal representation of the input hashmap */` |
|     198 | 1345 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 1346 | `	/* Create a new array */` |
|     198 | 1347 | `	pArray = ph7_context_new_array(pCtx);` |
|     198 | 1348 | `	if( pArray == 0 ){` |
|     ! 0 | 1349 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1350 | `		return PH7_OK;` |
|       - | 1351 | `	}` |
|     198 | 1352 | `	bStrict = FALSE;` |
|     198 | 1353 | `	if( nArg > 2 ){` |
|       - | 1354 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|       9 | 1355 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|     ! 0 | 1356 | `			return PH7_VmThrowException(pCtx,` |
|       - | 1357 | `				"TypeError",` |
|       - | 1358 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|     ! 0 | 1359 | `				ph7_type_name(apArg[2])` |
|       - | 1360 | `				);` |
|       - | 1361 | `		}` |
|       9 | 1362 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       4 | 1363 | `	}` |
|       - | 1364 | `	/* Perform the requested operation */` |
|     198 | 1365 | `	pNode = pMap->pFirst;` |
|     198 | 1366 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|    1654 | 1367 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|    1460 | 1368 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     193 | 1369 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|      98 | 1370 | `		}else{` |
|    1269 | 1371 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|    1269 | 1372 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 1373 | `		}` |
|    1460 | 1374 | `		rc = 0;` |
|    1460 | 1375 | `		if( nArg > 1 ){` |
|      65 | 1376 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|      65 | 1377 | `			if( pValue ){` |
|       - | 1378 | `				ph7_value sNeedle;` |
|      65 | 1379 | `				PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      65 | 1380 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 1381 | `				/* Filter key — compare on duplicates of BOTH sides:` |
|       - | 1382 | `				 * PH7_MemObjCmp converts its operands in place, and a needle` |
|       - | 1383 | `				 * mutated on the first element (e.g. null coerced) would` |
|       - | 1384 | `				 * corrupt every later comparison. */` |
|      65 | 1385 | `				PH7_MemObjLoad(apArg[1],&sNeedle);` |
|      65 | 1386 | `				rc = ph7_value_compare(&sVal,&sNeedle,bStrict);` |
|      65 | 1387 | `				PH7_MemObjRelease(&sNeedle);` |
|      65 | 1388 | `				PH7_MemObjRelease(&sVal);` |
|      32 | 1389 | `			}` |
|      32 | 1390 | `		}` |
|    1460 | 1391 | `		if( rc == 0 ){` |
|       - | 1392 | `			/* Perform the insertion */` |
|    1428 | 1393 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|     712 | 1394 | `		}` |
|    1460 | 1395 | `		PH7_MemObjRelease(&sObj);` |
|       - | 1396 | `		/* Point to the next entry */` |
|    1460 | 1397 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     732 | 1398 | `	}` |
|       - | 1399 | `	/* return the new array */` |
|     198 | 1400 | `	ph7_result_value(pCtx,pArray);` |
|     198 | 1401 | `	return PH7_OK;` |
|     103 | 1402 | `}` |
|       - | 1403 | `/*` |
|       - | 1404 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 1405 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 1406 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 1407 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 1408 | ` * Parameters` |
|       - | 1409 | ` *  $arr1` |
|       - | 1410 | ` *   First array` |
|       - | 1411 | ` *  $arr2` |
|       - | 1412 | ` *   Second array` |
|       - | 1413 | ` * Return` |
|       - | 1414 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 1415 | ` * Note` |
|       - | 1416 | ` *  This function is a symisc eXtension.` |
|       - | 1417 | ` */` |
|       4 | 1418 | `PH7_PRIVATE int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1419 | `{` |
|       - | 1420 | `	ph7_hashmap *p1,*p2;` |
|       - | 1421 | `	int rc;` |
|       5 | 1422 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 1423 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 1424 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1425 | `		return PH7_OK;` |
|       - | 1426 | `	}` |
|       - | 1427 | `	/* Point to the hashmaps */` |
|       5 | 1428 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 1429 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 1430 | `	rc = (p1 == p2);` |
|       - | 1431 | `	/* Same instance? */` |
|       5 | 1432 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 1433 | `	return PH7_OK;` |
|       3 | 1434 | `}` |
|       - | 1435 | `/*` |
|       - | 1436 | ` * array array_merge(array ...$arrays)` |
|       - | 1437 | ` *  Merge one or more arrays.` |
|       - | 1438 | ` * Parameters` |
|       - | 1439 | ` *  ...$arrays` |
|       - | 1440 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|       - | 1441 | ` *   passing a non-array argument throws a TypeError.` |
|       - | 1442 | ` * Return` |
|       - | 1443 | ` *  The resulting merged array. Returns an empty array when called` |
|       - | 1444 | ` *  with no arguments.` |
|       - | 1445 | ` */` |
|    1122 | 1446 | `PH7_PRIVATE int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1447 | `{` |
|       - | 1448 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 1449 | `	ph7_value *pArray;` |
|       - | 1450 | `	int i;` |
|       - | 1451 | `	/* Create a new array */` |
|    1127 | 1452 | `	pArray = ph7_context_new_array(pCtx);` |
|    1127 | 1453 | `	if( pArray == 0 ){` |
|     ! 0 | 1454 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1455 | `		return PH7_OK;` |
|       - | 1456 | `	}` |
|       - | 1457 | `	/* Point to the internal representation of the hashmap */` |
|    1127 | 1458 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 1459 | `	/* Start merging */` |
|    3347 | 1460 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 1461 | `		/* Make sure we are dealing with a valid hashmap */` |
|    2229 | 1462 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 1463 | `			/* Type mismatch -> TypeError */` |
|       8 | 1464 | `			return PH7_VmThrowException(pCtx,` |
|       - | 1465 | `				"TypeError",` |
|       - | 1466 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|       2 | 1467 | `				i + 1,` |
|       4 | 1468 | `				ph7_type_name(apArg[i])` |
|       - | 1469 | `				);` |
|     ! 0 | 1470 | `		}else{` |
|    2225 | 1471 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 1472 | `			/* Merge the two hashmaps */` |
|    2225 | 1473 | `			HashmapMerge(pSrc,pMap);` |
|       - | 1474 | `		}` |
|    1115 | 1475 | `	}` |
|       - | 1476 | `	/* Return the freshly created array */` |
|    1123 | 1477 | `	ph7_result_value(pCtx,pArray);` |
|    1123 | 1478 | `	return PH7_OK;` |
|     566 | 1479 | `}` |
|       - | 1480 | `/*` |
|       - | 1481 | ` * array array_copy(array $source)` |
|       - | 1482 | ` *  Make a blind copy of the target array.` |
|       - | 1483 | ` * Parameters` |
|       - | 1484 | ` *  $source` |
|       - | 1485 | ` *   Target array` |
|       - | 1486 | ` * Return` |
|       - | 1487 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 1488 | ` * Note` |
|       - | 1489 | ` *  This function is a symisc eXtension.` |
|       - | 1490 | ` */` |
|      18 | 1491 | `PH7_PRIVATE int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1492 | `{` |
|       - | 1493 | `	ph7_hashmap *pMap;` |
|       - | 1494 | `	ph7_value *pArray;` |
|      19 | 1495 | `	if( nArg < 1 ){` |
|       - | 1496 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 1497 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1498 | `		return PH7_OK;` |
|       - | 1499 | `	}` |
|       - | 1500 | `	/* Create a new array */` |
|      19 | 1501 | `	pArray = ph7_context_new_array(pCtx);` |
|      19 | 1502 | `	if( pArray == 0 ){` |
|     ! 0 | 1503 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1504 | `		return PH7_OK;` |
|       - | 1505 | `	}` |
|       - | 1506 | `	/* Point to the internal representation of the hashmap */` |
|      19 | 1507 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      19 | 1508 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 1509 | `		/* Point to the internal representation of the source */` |
|      19 | 1510 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 1511 | `		/* Perform the copy */` |
|      19 | 1512 | `		PH7_HashmapDup(pSrc,pMap);` |
|      10 | 1513 | `	}else{` |
|       - | 1514 | `		/* Simple insertion */` |
|     ! 0 | 1515 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 1516 | `	}` |
|       - | 1517 | `	/* Return the duplicated array */` |
|      19 | 1518 | `	ph7_result_value(pCtx,pArray);` |
|      19 | 1519 | `	return PH7_OK;` |
|      10 | 1520 | `}` |
|       - | 1521 | `/*` |
|       - | 1522 | ` * bool array_erase(array $source)` |
|       - | 1523 | ` *  Remove all elements from a given array.` |
|       - | 1524 | ` * Parameters` |
|       - | 1525 | ` *  $source` |
|       - | 1526 | ` *   Target array` |
|       - | 1527 | ` * Return` |
|       - | 1528 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 1529 | ` * Note` |
|       - | 1530 | ` *  This function is a symisc eXtension.` |
|       - | 1531 | ` */` |
|      26 | 1532 | `PH7_PRIVATE int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1533 | `{` |
|       - | 1534 | `	ph7_hashmap *pMap;` |
|      28 | 1535 | `	if( nArg < 1 ){` |
|       - | 1536 | `		/* Missing arguments */` |
|     ! 0 | 1537 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1538 | `		return PH7_OK;` |
|       - | 1539 | `	}` |
|       - | 1540 | `	/* Point to the target hashmap */` |
|      28 | 1541 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      28 | 1542 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 1543 | `	/* Erase */` |
|      28 | 1544 | `	PH7_HashmapRelease(pMap,FALSE);` |
|      28 | 1545 | `	return PH7_OK;` |
|      15 | 1546 | `}` |
|       - | 1547 | `/*` |
|       - | 1548 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|       - | 1549 | ` *  Extract a slice of the array.` |
|       - | 1550 | ` * Parameters` |
|       - | 1551 | ` *  $array` |
|       - | 1552 | ` *    The input array.` |
|       - | 1553 | ` * $offset` |
|       - | 1554 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 1555 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 1556 | ` * $length (optional, nullable)` |
|       - | 1557 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 1558 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 1559 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|       - | 1560 | ` *    will have everything from offset up until the end of the array.` |
|       - | 1561 | ` * $preserve_keys (optional)` |
|       - | 1562 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 1563 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 1564 | ` * Return` |
|       - | 1565 | ` *   The new slice.` |
|       - | 1566 | ` */` |
|      64 | 1567 | `PH7_PRIVATE int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1568 | `{` |
|       - | 1569 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 1570 | `	ph7_hashmap_node *pCur;` |
|       - | 1571 | `	ph7_value *pArray;` |
|       - | 1572 | `	int iLength,iOfft;` |
|       - | 1573 | `	int bPreserve;` |
|       - | 1574 | `	sxi32 rc;` |
|      69 | 1575 | `	if( nArg < 2 ){` |
|     ! 0 | 1576 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1577 | `			"ArgumentCountError",` |
|       - | 1578 | `			"array_slice() expects at least 2 arguments, %d given",` |
|     ! 0 | 1579 | `			nArg` |
|       - | 1580 | `			);` |
|       - | 1581 | `	}` |
|      69 | 1582 | `	if( nArg > 4 ){` |
|     ! 0 | 1583 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1584 | `			"ArgumentCountError",` |
|       - | 1585 | `			"array_slice() expects at most 4 arguments, %d given",` |
|     ! 0 | 1586 | `			nArg` |
|       - | 1587 | `			);` |
|       - | 1588 | `	}` |
|      69 | 1589 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 1590 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1591 | `			"TypeError",` |
|       - | 1592 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 1593 | `			ph7_type_name(apArg[0])` |
|       - | 1594 | `			);` |
|       - | 1595 | `	}` |
|       - | 1596 | `	/* Validate $offset type: reject string, array, object, resource */` |
|      92 | 1597 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|      95 | 1598 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|       4 | 1599 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1600 | `			"TypeError",` |
|       - | 1601 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|       2 | 1602 | `			ph7_type_name(apArg[1])` |
|       - | 1603 | `			);` |
|       - | 1604 | `	}` |
|       - | 1605 | `	/* Validate $length type if provided: nullable int */` |
|      65 | 1606 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      56 | 1607 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|      56 | 1608 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 1609 | `			return PH7_VmThrowException(pCtx,` |
|       - | 1610 | `				"TypeError",` |
|       - | 1611 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|       2 | 1612 | `				ph7_type_name(apArg[2])` |
|       - | 1613 | `				);` |
|       - | 1614 | `		}` |
|      18 | 1615 | `	}` |
|       - | 1616 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|      63 | 1617 | `	if( nArg > 3 ){` |
|       7 | 1618 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|       4 | 1619 | `			ph7_value_is_resource(apArg[3]) ){` |
|     ! 0 | 1620 | `			return PH7_VmThrowException(pCtx,` |
|       - | 1621 | `				"TypeError",` |
|       - | 1622 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|     ! 0 | 1623 | `				ph7_type_name(apArg[3])` |
|       - | 1624 | `				);` |
|       - | 1625 | `		}` |
|       2 | 1626 | `	}` |
|       - | 1627 | `	/* Point the internal representation of the target array */` |
|      63 | 1628 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      63 | 1629 | `	bPreserve = FALSE;` |
|       - | 1630 | `	/* Get the offset */` |
|       - | 1631 | `	{` |
|      63 | 1632 | `		sxi64 iTmp = 0;` |
|      63 | 1633 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"array_slice",2,"$offset","int",&iTmp);` |
|      63 | 1634 | `		if( rcArg != PH7_OK ){` |
|     ! 0 | 1635 | `			return rcArg;` |
|       - | 1636 | `		}` |
|      63 | 1637 | `		iOfft = (int)iTmp;` |
|       - | 1638 | `	}` |
|      63 | 1639 | `	if( iOfft < 0 ){` |
|       5 | 1640 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       5 | 1641 | `		if( iOfft < 0 ){` |
|       3 | 1642 | `			iOfft = 0;` |
|       1 | 1643 | `		}` |
|       2 | 1644 | `	}` |
|      63 | 1645 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|       - | 1646 | `		/* Offset past end of array, return empty array */` |
|       5 | 1647 | `		pArray = ph7_context_new_array(pCtx);` |
|       5 | 1648 | `		if( pArray == 0 ){` |
|     ! 0 | 1649 | `			ph7_result_null(pCtx);` |
|     ! 0 | 1650 | `			return PH7_OK;` |
|       - | 1651 | `		}` |
|       5 | 1652 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 1653 | `		return PH7_OK;` |
|       - | 1654 | `	}` |
|       - | 1655 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|      59 | 1656 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      59 | 1657 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      37 | 1658 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      37 | 1659 | `		if( iLength < 0 ){` |
|       5 | 1660 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       2 | 1661 | `		}` |
|      37 | 1662 | `		if( iLength < 0 ){` |
|       3 | 1663 | `			iLength = 0;` |
|       1 | 1664 | `		}` |
|      37 | 1665 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 1666 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 1667 | `		}` |
|      18 | 1668 | `	}` |
|      59 | 1669 | `	if( nArg > 3 ){` |
|       5 | 1670 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|       2 | 1671 | `	}` |
|       - | 1672 | `	/* Create a new array */` |
|      59 | 1673 | `	pArray = ph7_context_new_array(pCtx);` |
|      59 | 1674 | `	if( pArray == 0 ){` |
|     ! 0 | 1675 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1676 | `		return PH7_OK;` |
|       - | 1677 | `	}` |
|      59 | 1678 | `	if( iLength < 1 ){` |
|       - | 1679 | `		/* Don't bother processing,return the empty array */` |
|       5 | 1680 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 1681 | `		return PH7_OK;` |
|       - | 1682 | `	}` |
|       - | 1683 | `	/* Point to the desired entry */` |
|      55 | 1684 | `	pCur = pSrc->pFirst;` |
|      54 | 1685 | `	for(;;){` |
|     113 | 1686 | `		if( iOfft < 1 ){` |
|      55 | 1687 | `			break;` |
|       - | 1688 | `		}` |
|       - | 1689 | `		/* Point to the next entry */` |
|      63 | 1690 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      63 | 1691 | `		iOfft--;` |
|       5 | 1692 | `	}` |
|       - | 1693 | `	/* Point to the internal representation of the hashmap */` |
|      55 | 1694 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     106 | 1695 | `	for(;;){` |
|     217 | 1696 | `		if( iLength < 1 ){` |
|      55 | 1697 | `			break;` |
|       - | 1698 | `		}` |
|       - | 1699 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|       - | 1700 | `		{` |
|     167 | 1701 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|     167 | 1702 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|       - | 1703 | `		}` |
|     167 | 1704 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1705 | `			break;` |
|       - | 1706 | `		}` |
|       - | 1707 | `		/* Point to the next entry */` |
|     167 | 1708 | `		pCur = pCur->pPrev; /* Reverse link */` |
|     167 | 1709 | `		iLength--;` |
|       5 | 1710 | `	}` |
|       - | 1711 | `	/* Return the freshly created array */` |
|      55 | 1712 | `	ph7_result_value(pCtx,pArray);` |
|      55 | 1713 | `	return PH7_OK;` |
|      37 | 1714 | `}` |
|       - | 1715 | `/*` |
|       - | 1716 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|       - | 1717 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|       - | 1718 | ` * beginning (becomes the new pFirst).` |
|       - | 1719 | ` */` |
|      38 | 1720 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|       1 | 1721 | `{` |
|       - | 1722 | `	ph7_hashmap_node *pNode;` |
|       - | 1723 | `	ph7_hashmap_node *pOldNext;` |
|      39 | 1724 | `	pNode = pMap->pLast;` |
|      39 | 1725 | `	if( pNode == 0 ){` |
|     ! 0 | 1726 | `		return;` |
|       - | 1727 | `	}` |
|      39 | 1728 | `	if( pNode->pNext == 0 ){` |
|       - | 1729 | `		/* Only node in the list, nothing to move */` |
|       5 | 1730 | `		return;` |
|       - | 1731 | `	}` |
|      35 | 1732 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|       - | 1733 | `		/* Already in the correct position */` |
|       9 | 1734 | `		return;` |
|       - | 1735 | `	}` |
|       - | 1736 | `	/* Unlink pNode from the end of the list */` |
|      27 | 1737 | `	pMap->pLast = pNode->pNext;` |
|      27 | 1738 | `	pMap->pLast->pPrev = 0;` |
|       - | 1739 | `	/* Insert pNode after pAfter in iteration order */` |
|      27 | 1740 | `	if( pAfter == 0 ){` |
|       - | 1741 | `		/* Insert at the very beginning, before pFirst */` |
|       3 | 1742 | `		pNode->pNext = 0;` |
|       3 | 1743 | `		pNode->pPrev = pMap->pFirst;` |
|       3 | 1744 | `		if( pMap->pFirst ){` |
|       3 | 1745 | `			pMap->pFirst->pNext = pNode;` |
|       1 | 1746 | `		}` |
|       3 | 1747 | `		pMap->pFirst = pNode;` |
|       2 | 1748 | `	}else{` |
|      25 | 1749 | `		pOldNext = pAfter->pPrev;` |
|      25 | 1750 | `		pNode->pPrev = pOldNext;` |
|      25 | 1751 | `		pNode->pNext = pAfter;` |
|      25 | 1752 | `		pAfter->pPrev = pNode;` |
|      25 | 1753 | `		if( pOldNext ){` |
|      25 | 1754 | `			pOldNext->pNext = pNode;` |
|      13 | 1755 | `		}else{` |
|     ! 0 | 1756 | `			pMap->pLast = pNode;` |
|       - | 1757 | `		}` |
|       - | 1758 | `	}` |
|      20 | 1759 | `}` |
|       - | 1760 | `/*` |
|       - | 1761 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|       - | 1762 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 1763 | ` * Parameters` |
|       - | 1764 | ` *  $array` |
|       - | 1765 | ` *    The input array.` |
|       - | 1766 | ` *  $offset` |
|       - | 1767 | ` *    If offset is positive then the start of removed portion is at that offset` |
|       - | 1768 | ` *    from the beginning of the input array.  If offset is negative then it` |
|       - | 1769 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|       - | 1770 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|       - | 1771 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|       - | 1772 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|       - | 1773 | ` *  $length (optional)` |
|       - | 1774 | ` *    If length is omitted, removes everything from offset to the end of the` |
|       - | 1775 | ` *    array.  If length is specified and is positive, then that many elements` |
|       - | 1776 | ` *    will be removed.  If length is specified and is negative then the end of` |
|       - | 1777 | ` *    the removed portion will be that many elements from the end of the array.` |
|       - | 1778 | ` *    If the resulting length is negative it is clamped to 0.` |
|       - | 1779 | ` *  $replacement (optional)` |
|       - | 1780 | ` *    If replacement array is specified, then the removed elements are replaced` |
|       - | 1781 | ` *    with elements from this array.` |
|       - | 1782 | ` *    If offset and length are such that nothing is removed, then the elements` |
|       - | 1783 | ` *    from the replacement array are inserted in the place specified by the` |
|       - | 1784 | ` *    offset.` |
|       - | 1785 | ` *    Note that keys in replacement array are not preserved.` |
|       - | 1786 | ` *    If replacement is just one element it is not necessary to put array()` |
|       - | 1787 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|       - | 1788 | ` * Return` |
|       - | 1789 | ` *   A new array consisting of the extracted elements.` |
|       - | 1790 | ` */` |
|      64 | 1791 | `PH7_PRIVATE int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1792 | `{` |
|       - | 1793 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|       - | 1794 | `	ph7_value *pArray,*pRvalue;` |
|       - | 1795 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 1796 | `	int iLength,iOfft,i;` |
|       - | 1797 | `	sxi32 rc;` |
|      66 | 1798 | `	if( nArg < 2 ){` |
|     ! 0 | 1799 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1800 | `			"ArgumentCountError",` |
|       - | 1801 | `			"array_splice() expects at least 2 arguments, %d given",` |
|     ! 0 | 1802 | `			nArg` |
|       - | 1803 | `			);` |
|       - | 1804 | `	}` |
|      66 | 1805 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 1806 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1807 | `			"TypeError",` |
|       - | 1808 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 1809 | `			ph7_type_name(apArg[0])` |
|       - | 1810 | `			);` |
|       - | 1811 | `	}` |
|       - | 1812 | `	/* Point to the internal representation of the target array */` |
|      63 | 1813 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      63 | 1814 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 1815 | `	/* Get the offset and clamp to valid range */` |
|      63 | 1816 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      63 | 1817 | `	if( iOfft < 0 ){` |
|       9 | 1818 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       9 | 1819 | `		if( iOfft < 0 ){` |
|       3 | 1820 | `			iOfft = 0;` |
|       2 | 1821 | `		}` |
|      59 | 1822 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|       3 | 1823 | `		iOfft = (int)pSrc->nEntry;` |
|       1 | 1824 | `	}` |
|       - | 1825 | `	/* Get the length and clamp to valid range.` |
|       - | 1826 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|      63 | 1827 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      63 | 1828 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      45 | 1829 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      45 | 1830 | `		if( iLength < 0 ){` |
|       7 | 1831 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       7 | 1832 | `			if( iLength < 0 ){` |
|       3 | 1833 | `				iLength = 0;` |
|       1 | 1834 | `			}` |
|       3 | 1835 | `		}` |
|      45 | 1836 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 1837 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 1838 | `		}` |
|      22 | 1839 | `	}` |
|       - | 1840 | `	/* Create the result array for removed elements */` |
|      63 | 1841 | `	pArray = ph7_context_new_array(pCtx);` |
|      63 | 1842 | `	if( pArray == 0 ){` |
|     ! 0 | 1843 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1844 | `		return PH7_OK;` |
|       - | 1845 | `	}` |
|       - | 1846 | `	/* Get replacement array if provided */` |
|      63 | 1847 | `	pRep = 0;` |
|      63 | 1848 | `	if( nArg > 3 ){` |
|      27 | 1849 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 1850 | `			/* Perform an array cast */` |
|       3 | 1851 | `			PH7_MemObjToHashmap(apArg[3]);` |
|       3 | 1852 | `			if( ph7_value_is_array(apArg[3]) ){` |
|       3 | 1853 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       1 | 1854 | `			}` |
|       2 | 1855 | `		}else{` |
|      25 | 1856 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 1857 | `		}` |
|      27 | 1858 | `		if( pRep ){` |
|       - | 1859 | `			/* Reset the loop cursor */` |
|      27 | 1860 | `			pRep->pCur = pRep->pFirst;` |
|      13 | 1861 | `		}` |
|      13 | 1862 | `	}` |
|       - | 1863 | `	/* No early return for the nothing-to-do case: php reindexes the input` |
|       - | 1864 | `	 * array's integer keys on EVERY splice, even a no-op one. */` |
|       - | 1865 | `	/* Navigate to the offset position */` |
|      63 | 1866 | `	pCur = pSrc->pFirst;` |
|     131 | 1867 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|      69 | 1868 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      35 | 1869 | `	}` |
|       - | 1870 | `	/* Save the node just before the splice range as the insertion anchor.` |
|       - | 1871 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|       - | 1872 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|      63 | 1873 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|       - | 1874 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|      63 | 1875 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     141 | 1876 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|      79 | 1877 | `		pPrev = pCur->pPrev;` |
|      79 | 1878 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|      79 | 1879 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|      79 | 1880 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1881 | `			break;` |
|       - | 1882 | `		}` |
|      79 | 1883 | `		pCur = pPrev; /* Reverse link */` |
|      40 | 1884 | `	}` |
|       - | 1885 | `	/* Insert replacement elements at the correct position */` |
|      63 | 1886 | `	if( pRep ){` |
|       - | 1887 | `		ph7_value sSafeVal;` |
|      78 | 1888 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|      39 | 1889 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|      39 | 1890 | `			if( pRvalue ){` |
|       - | 1891 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|       - | 1892 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|       - | 1893 | `				 * since it points into that same pool. */` |
|      39 | 1894 | `				sSafeVal = *pRvalue;` |
|      39 | 1895 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|      39 | 1896 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|      39 | 1897 | `					pNewNode = pSrc->pLast;` |
|      39 | 1898 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|      39 | 1899 | `					pInsertAfter = pNewNode;` |
|      19 | 1900 | `				}` |
|      19 | 1901 | `			}` |
|       1 | 1902 | `		}` |
|      13 | 1903 | `	}` |
|       - | 1904 | `	/* php renumbers ALL integer keys of the input array in iteration order` |
|       - | 1905 | `	 * (string keys preserved) — same pass as array_shift. Pre-fix the spliced` |
|       - | 1906 | `	 * array kept its old keys, so inserts landed with out-of-sequence keys` |
|       - | 1907 | `	 * and removals left gaps. */` |
|       - | 1908 | `	{` |
|      63 | 1909 | `		ph7_hashmap_node *pEntry = pSrc->pFirst;` |
|      63 | 1910 | `		sxu32 n = pSrc->nEntry;` |
|      63 | 1911 | `		pSrc->iNextIdx = 0;` |
|     233 | 1912 | `		while( n > 0 ){` |
|     171 | 1913 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     165 | 1914 | `				HashmapRehashIntNode(pEntry);` |
|      82 | 1915 | `			}` |
|     171 | 1916 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|     171 | 1917 | `			n--;` |
|       1 | 1918 | `		}` |
|      63 | 1919 | `		pSrc->pCur = pSrc->pFirst;` |
|       - | 1920 | `	}` |
|       - | 1921 | `	/* Return the freshly created array */` |
|      63 | 1922 | `	ph7_result_value(pCtx,pArray);` |
|      63 | 1923 | `	return PH7_OK;` |
|      34 | 1924 | `}` |
|       - | 1925 | `/*` |
|       - | 1926 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 1927 | ` *  Checks if a value exists in an array.` |
|       - | 1928 | ` * Parameters` |
|       - | 1929 | ` *  $needle` |
|       - | 1930 | ` *   The searched value.` |
|       - | 1931 | ` *   Note:` |
|       - | 1932 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 1933 | ` * $haystack` |
|       - | 1934 | ` *  The target array.` |
|       - | 1935 | ` * $strict` |
|       - | 1936 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 1937 | ` *  will also check the types of the needle in the haystack.` |
|       - | 1938 | ` */` |
|   32702 | 1939 | `PH7_PRIVATE int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1940 | `{` |
|       - | 1941 | `	ph7_value *pNeedle;` |
|       - | 1942 | `	int bStrict;` |
|       - | 1943 | `	int rc;` |
|   32707 | 1944 | `	if( nArg < 2 ){` |
|       - | 1945 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 1946 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1947 | `		return PH7_OK;` |
|       - | 1948 | `	}` |
|   32707 | 1949 | `	pNeedle = apArg[0];` |
|   32707 | 1950 | `	bStrict = 0;` |
|   32707 | 1951 | `	if( nArg > 2 ){` |
|      62 | 1952 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|      30 | 1953 | `	}` |
|   32707 | 1954 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 1955 | `		/* haystack must be an array,perform a standard comparison */` |
|     ! 0 | 1956 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|       - | 1957 | `		/* Set the comparison result */` |
|     ! 0 | 1958 | `		ph7_result_bool(pCtx,rc == 0);` |
|     ! 0 | 1959 | `		return PH7_OK;` |
|       - | 1960 | `	}` |
|       - | 1961 | `	/* Perform the lookup */` |
|   32707 | 1962 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 1963 | `	/* Lookup result */` |
|   32707 | 1964 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   32707 | 1965 | `	return PH7_OK;` |
|   16356 | 1966 | `}` |
|       - | 1967 | `/*` |
|       - | 1968 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 1969 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 1970 | ` * Parameters` |
|       - | 1971 | ` * $needle` |
|       - | 1972 | ` *   The searched value.` |
|       - | 1973 | ` * $haystack` |
|       - | 1974 | ` *   The array.` |
|       - | 1975 | ` * $strict` |
|       - | 1976 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 1977 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 1978 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 1979 | ` * Return` |
|       - | 1980 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 1981 | ` */` |
|      28 | 1982 | `PH7_PRIVATE int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1983 | `{` |
|       - | 1984 | `	ph7_hashmap_node *pEntry;` |
|       - | 1985 | `	ph7_value *pVal,sNeedle;` |
|       - | 1986 | `	ph7_hashmap *pMap;` |
|       - | 1987 | `	ph7_value sVal;` |
|       - | 1988 | `	int bStrict;` |
|       - | 1989 | `	sxu32 n;` |
|       - | 1990 | `	int rc;` |
|      30 | 1991 | `	if( nArg < 2 ){` |
|       - | 1992 | `		/* Missing argument,throw ArgumentCountError */` |
|     ! 0 | 1993 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1994 | `			"ArgumentCountError",` |
|       - | 1995 | `			"array_search() expects at least 2 arguments, %d given",` |
|     ! 0 | 1996 | `			nArg` |
|       - | 1997 | `			);` |
|       - | 1998 | `	}` |
|      30 | 1999 | `	bStrict = FALSE;` |
|      30 | 2000 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2001 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 2002 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2003 | `			"TypeError",` |
|       - | 2004 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|       2 | 2005 | `			ph7_type_name(apArg[1])` |
|       - | 2006 | `			);` |
|       - | 2007 | `	}` |
|      27 | 2008 | `	if( nArg > 2 ){` |
|       - | 2009 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      13 | 2010 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|     ! 0 | 2011 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2012 | `				"TypeError",` |
|       - | 2013 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|     ! 0 | 2014 | `				ph7_type_name(apArg[2])` |
|       - | 2015 | `				);` |
|       - | 2016 | `		}` |
|      13 | 2017 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       6 | 2018 | `	}` |
|       - | 2019 | `	/* Point to the internal representation of the internal hashmap */` |
|      27 | 2020 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 2021 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      27 | 2022 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      27 | 2023 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      27 | 2024 | `	pEntry = pMap->pFirst;` |
|      27 | 2025 | `	n = pMap->nEntry;` |
|      29 | 2026 | `	for(;;){` |
|      59 | 2027 | `		if( !n ){` |
|       9 | 2028 | `			break;` |
|       - | 2029 | `		}` |
|       - | 2030 | `		/* Extract node value */` |
|      51 | 2031 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      51 | 2032 | `		if( pVal ){` |
|       - | 2033 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 2034 | `			 * can change their type.` |
|       - | 2035 | `			 */` |
|      51 | 2036 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      51 | 2037 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      51 | 2038 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      51 | 2039 | `			PH7_MemObjRelease(&sVal);` |
|      51 | 2040 | `			PH7_MemObjRelease(&sNeedle);` |
|      51 | 2041 | `			if( rc == 0 ){` |
|       - | 2042 | `				/* Match found,return key */` |
|      19 | 2043 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 2044 | `					/* INT key */` |
|      13 | 2045 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|       7 | 2046 | `				}else{` |
|       7 | 2047 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 2048 | `					/* Blob key */` |
|       7 | 2049 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 2050 | `				}` |
|      19 | 2051 | `				return PH7_OK;` |
|       - | 2052 | `			}` |
|      16 | 2053 | `		}` |
|       - | 2054 | `		/* Point to the next entry */` |
|      33 | 2055 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 2056 | `		n--;` |
|       1 | 2057 | `	}` |
|       - | 2058 | `	/* No such value,return FALSE */` |
|       9 | 2059 | `	ph7_result_bool(pCtx,0);` |
|       9 | 2060 | `	return PH7_OK;` |
|      16 | 2061 | `}` |
|       - | 2062 | `/*` |
|       - | 2063 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 2064 | ` *  Computes the difference of arrays.` |
|       - | 2065 | ` * Parameters` |
|       - | 2066 | ` *  $array1` |
|       - | 2067 | ` *    The array to compare from` |
|       - | 2068 | ` *  $array2` |
|       - | 2069 | ` *    An array to compare against` |
|       - | 2070 | ` *  $...` |
|       - | 2071 | ` *   More arrays to compare against` |
|       - | 2072 | ` * Return` |
|       - | 2073 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 2074 | ` *  are not present in any of the other arrays.` |
|       - | 2075 | ` */` |
|      20 | 2076 | `PH7_PRIVATE int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2077 | `{` |
|       - | 2078 | `	ph7_hashmap_node *pEntry;` |
|       - | 2079 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2080 | `	ph7_value *pArray;` |
|       - | 2081 | `	ph7_value *pVal;` |
|       - | 2082 | `	sxi32 rc;` |
|       - | 2083 | `	sxu32 n;` |
|       - | 2084 | `	int i;` |
|       - | 2085 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 2086 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 2087 | `	 * debugging difficult. */` |
|      23 | 2088 | `	if( nArg < 1 ){` |
|     ! 0 | 2089 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2090 | `			"ArgumentCountError",` |
|       - | 2091 | `			"array_diff() expects at least 1 argument, %d given",` |
|     ! 0 | 2092 | `			nArg` |
|       - | 2093 | `			);` |
|       - | 2094 | `	}` |
|      23 | 2095 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2096 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2097 | `			"TypeError",` |
|       - | 2098 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2099 | `			ph7_type_name(apArg[0])` |
|       - | 2100 | `			);` |
|       - | 2101 | `	}` |
|      36 | 2102 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 2103 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 2104 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2105 | `				"TypeError",` |
|       - | 2106 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 2107 | `				i + 1,` |
|       2 | 2108 | `				ph7_type_name(apArg[i])` |
|       - | 2109 | `				);` |
|       - | 2110 | `		}` |
|       9 | 2111 | `	}` |
|      17 | 2112 | `	if( nArg == 1 ){` |
|       - | 2113 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 2114 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2115 | `		return PH7_OK;` |
|       - | 2116 | `	}` |
|       - | 2117 | `	/* Create a new array */` |
|      15 | 2118 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 2119 | `	if( pArray == 0 ){` |
|     ! 0 | 2120 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2121 | `		return PH7_OK;` |
|       - | 2122 | `	}` |
|       - | 2123 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 2124 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2125 | `	/* Perform the diff */` |
|      15 | 2126 | `	pEntry = pSrc->pFirst;` |
|      15 | 2127 | `	n = pSrc->nEntry;` |
|      27 | 2128 | `	for(;;){` |
|      55 | 2129 | `		if( n < 1 ){` |
|      15 | 2130 | `			break;` |
|       - | 2131 | `		}` |
|       - | 2132 | `		/* Extract the node value */` |
|      41 | 2133 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      41 | 2134 | `		if( pVal ){` |
|      69 | 2135 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 2136 | `				/* Point to the internal representation of the hashmap */` |
|      45 | 2137 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2138 | `				/* Perform the lookup */` |
|      45 | 2139 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      45 | 2140 | `				if( rc == SXRET_OK ){` |
|       - | 2141 | `					/* Value exist */` |
|      17 | 2142 | `					break;` |
|       - | 2143 | `				}` |
|      15 | 2144 | `			}` |
|      41 | 2145 | `			if( i >= nArg ){` |
|       - | 2146 | `				/* Perform the insertion */` |
|      25 | 2147 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 2148 | `			}` |
|      20 | 2149 | `		}` |
|       - | 2150 | `		/* Point to the next entry */` |
|      41 | 2151 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      41 | 2152 | `		n--;` |
|       1 | 2153 | `	}` |
|       - | 2154 | `	/* Return the freshly created array */` |
|      15 | 2155 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 2156 | `	return PH7_OK;` |
|      13 | 2157 | `}` |
|       - | 2158 | `/*` |
|       - | 2159 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 2160 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 2161 | ` * Parameters` |
|       - | 2162 | ` *  $array1` |
|       - | 2163 | ` *    The array to compare from` |
|       - | 2164 | ` *  $array2` |
|       - | 2165 | ` *    An array to compare against` |
|       - | 2166 | ` *  $...` |
|       - | 2167 | ` *   More arrays to compare against.` |
|       - | 2168 | ` * $callback` |
|       - | 2169 | ` *  The callback comparison function.` |
|       - | 2170 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 2171 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 2172 | ` *  than the second.` |
|       - | 2173 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 2174 | ` * Return` |
|       - | 2175 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 2176 | ` *  are not present in any of the other arrays.` |
|       - | 2177 | ` */` |
|      20 | 2178 | `PH7_PRIVATE int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2179 | `{` |
|       - | 2180 | `	ph7_hashmap_node *pEntry;` |
|       - | 2181 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2182 | `	ph7_value *pCallback;` |
|       - | 2183 | `	ph7_value *pArray;` |
|       - | 2184 | `	ph7_value *pVal;` |
|       - | 2185 | `	sxi32 rc;` |
|       - | 2186 | `	sxu32 n;` |
|       - | 2187 | `	int i;` |
|       - | 2188 |  |
|       - | 2189 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      25 | 2190 | `	if( nArg < 2 ){` |
|     ! 0 | 2191 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2192 | `			"ArgumentCountError",` |
|       - | 2193 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|     ! 0 | 2194 | `			nArg` |
|       - | 2195 | `			);` |
|       - | 2196 | `	}` |
|      25 | 2197 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2198 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2199 | `			"TypeError",` |
|       - | 2200 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2201 | `			ph7_type_name(apArg[0])` |
|       - | 2202 | `			);` |
|       - | 2203 | `	}` |
|       - | 2204 |  |
|      23 | 2205 | `	if( nArg == 2 ){` |
|       - | 2206 | `		/* Only the original array and the callback were provided. */` |
|       - | 2207 | `		/* Nevertheless, we still validate the callback after verifying any` |
|       - | 2208 | `		 * intermediate array arguments to match PHP's left-to-right parameter` |
|       - | 2209 | `		 * validation order.` |
|       - | 2210 | `		 */` |
|       4 | 2211 | `	} else {` |
|       - | 2212 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      27 | 2213 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      19 | 2214 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|      11 | 2215 | `				return PH7_VmThrowException(pCtx,` |
|       - | 2216 | `					"TypeError",` |
|       - | 2217 | `					"array_udiff(): Argument #%d must be of type array, %s given",` |
|       3 | 2218 | `					i + 1,` |
|       6 | 2219 | `					ph7_type_name(apArg[i])` |
|       - | 2220 | `					);` |
|       - | 2221 | `			}` |
|       7 | 2222 | `		}` |
|       - | 2223 | `	}` |
|       - | 2224 |  |
|       - | 2225 | `	/* Identify the callback (always expected as the last argument). */` |
|      16 | 2226 | `	pCallback = apArg[nArg - 1];` |
|       - | 2227 | `	/* Validate the callback to match PHP's error messages. */` |
|      16 | 2228 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       9 | 2229 | `		if( ph7_value_is_array(pCallback) ){` |
|       4 | 2230 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2231 | `				"TypeError",` |
|       - | 2232 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 2233 | `				nArg` |
|       - | 2234 | `				);` |
|       - | 2235 | `		}` |
|       6 | 2236 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 2237 | `			int len;` |
|       3 | 2238 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 2239 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2240 | `				"TypeError",` |
|       - | 2241 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 2242 | `				nArg,` |
|       1 | 2243 | `				zName` |
|       - | 2244 | `				);` |
|       - | 2245 | `		}` |
|       4 | 2246 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2247 | `			"TypeError",` |
|       - | 2248 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 2249 | `			nArg` |
|       - | 2250 | `			);` |
|       - | 2251 | `	}` |
|       - | 2252 |  |
|       7 | 2253 | `	if( nArg == 2 ){` |
|       - | 2254 | `		/* Only the original array and the callback were provided. */` |
|       3 | 2255 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2256 | `		return PH7_OK;` |
|       - | 2257 | `	}` |
|       - | 2258 |  |
|       - | 2259 | `	/* Create a new array */` |
|       5 | 2260 | `	pArray = ph7_context_new_array(pCtx);` |
|       5 | 2261 | `	if( pArray == 0 ){` |
|     ! 0 | 2262 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2263 | `		return PH7_OK;` |
|       - | 2264 | `	}` |
|       - | 2265 | `	/* Point to the internal representation of the source hashmap */` |
|       5 | 2266 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2267 | `	/* Perform the diff */` |
|       5 | 2268 | `	pEntry = pSrc->pFirst;` |
|       5 | 2269 | `	n = pSrc->nEntry;` |
|       5 | 2270 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|       5 | 2271 | `	for(;;){` |
|      11 | 2272 | `		if( n < 1 ){` |
|       3 | 2273 | `			break;` |
|       - | 2274 | `		}` |
|       - | 2275 | `		/* Extract the node value */` |
|       9 | 2276 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|       9 | 2277 | `		if( pVal ){` |
|      15 | 2278 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 2279 | `				/* Point to the internal representation of the hashmap */` |
|       9 | 2280 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2281 | `				/* Perform the lookup */` |
|       9 | 2282 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|       9 | 2283 | `				if( rc == SXRET_OK ){` |
|       - | 2284 | `					/* Value exist */` |
|       3 | 2285 | `					break;` |
|       - | 2286 | `				}` |
|       4 | 2287 | `			}` |
|       9 | 2288 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2289 | `				/* The comparison callback raised: propagate so the dispatcher` |
|       - | 2290 | `				 * unwinds, before any spurious insertion into the result. */` |
|       3 | 2291 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 2292 | `				return PH7_EXCEPTION;` |
|       - | 2293 | `			}` |
|       7 | 2294 | `			if( i >= (nArg - 1)){` |
|       - | 2295 | `				/* Perform the insertion */` |
|       5 | 2296 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       2 | 2297 | `			}` |
|       3 | 2298 | `		}` |
|       - | 2299 | `		/* Point to the next entry */` |
|       7 | 2300 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       7 | 2301 | `		n--;` |
|       1 | 2302 | `	}` |
|       - | 2303 | `	/* Return the freshly created array */` |
|       3 | 2304 | `	ph7_result_value(pCtx,pArray);` |
|       3 | 2305 | `	return PH7_OK;` |
|      15 | 2306 | `}` |
|       - | 2307 | `/*` |
|       - | 2308 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 2309 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 2310 | ` * Parameters` |
|       - | 2311 | ` *  $array1` |
|       - | 2312 | ` *    The array to compare from` |
|       - | 2313 | ` *  $array2` |
|       - | 2314 | ` *    An array to compare against` |
|       - | 2315 | ` *  $...` |
|       - | 2316 | ` *   More arrays to compare against` |
|       - | 2317 | ` * Return` |
|       - | 2318 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 2319 | ` *  are not present in any of the other arrays.` |
|       - | 2320 | ` */` |
|      20 | 2321 | `PH7_PRIVATE int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 2322 | `{` |
|       - | 2323 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 2324 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2325 | `	ph7_value *pArray;` |
|       - | 2326 | `	ph7_value *pVal;` |
|       - | 2327 | `	sxi32 rc;` |
|       - | 2328 | `	sxu32 n;` |
|       - | 2329 | `	int i;` |
|       - | 2330 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 2331 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 2332 | `	 * accompanying integration tests to pass. */` |
|      24 | 2333 | `	if( nArg < 1 ){` |
|     ! 0 | 2334 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2335 | `			"ArgumentCountError",` |
|       - | 2336 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|     ! 0 | 2337 | `			nArg` |
|       - | 2338 | `			);` |
|       - | 2339 | `	}` |
|      24 | 2340 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2341 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2342 | `			"TypeError",` |
|       - | 2343 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2344 | `			ph7_type_name(apArg[0])` |
|       - | 2345 | `			);` |
|       - | 2346 | `	}` |
|      37 | 2347 | `	for(i = 1 ; i < nArg ; i++){` |
|      23 | 2348 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       8 | 2349 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2350 | `				"TypeError",` |
|       - | 2351 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 2352 | `				i + 1,` |
|       4 | 2353 | `				ph7_type_name(apArg[i])` |
|       - | 2354 | `				);` |
|       - | 2355 | `		}` |
|      10 | 2356 | `	}` |
|      15 | 2357 | `	if( nArg == 1 ){` |
|       - | 2358 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 2359 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2360 | `		return PH7_OK;` |
|       - | 2361 | `	}` |
|       - | 2362 | `	/* Create a new array */` |
|      13 | 2363 | `	pArray = ph7_context_new_array(pCtx);` |
|      13 | 2364 | `	if( pArray == 0 ){` |
|     ! 0 | 2365 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2366 | `		return PH7_OK;` |
|       - | 2367 | `	}` |
|       - | 2368 | `	/* Point to the internal representation of the source hashmap */` |
|      13 | 2369 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2370 | `	/* Perform the diff */` |
|      13 | 2371 | `	pEntry = pSrc->pFirst;` |
|      13 | 2372 | `	n = pSrc->nEntry;` |
|      13 | 2373 | `	pN1 = pN2 = 0;` |
|      34 | 2374 | `	for(;;){` |
|       - | 2375 | `		int keep;` |
|      41 | 2376 | `		if( n < 1 ){` |
|      13 | 2377 | `			break;` |
|       - | 2378 | `		}` |
|       - | 2379 | `		/* assume the element should be kept until we find a match */` |
|      29 | 2380 | `		keep = 1;` |
|      47 | 2381 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 2382 | `			/* all arguments have been validated already, so cast directly */` |
|      33 | 2383 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2384 | `			/* Perform a key lookup first */` |
|      33 | 2385 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      13 | 2386 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       7 | 2387 | `			}else{` |
|      21 | 2388 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 2389 | `			}` |
|      33 | 2390 | `			if( rc != SXRET_OK ){` |
|       - | 2391 | `				/* this array does not contain the key, continue checking others */` |
|      17 | 2392 | `				continue;` |
|       - | 2393 | `			}` |
|       - | 2394 | `			/* key exists; check that value stored in the matching node is equal */` |
|      17 | 2395 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      17 | 2396 | `			if( pVal ){` |
|       - | 2397 | `				/* directly compare with value at pN1 rather than searching again */` |
|      17 | 2398 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      17 | 2399 | `				if( pVal2 ){` |
|       - | 2400 | `					ph7_value sV1,sV2;` |
|       - | 2401 | `					sxi32 cmp;` |
|       - | 2402 | `					/* Compare on duplicates: PH7_MemObjCmp converts its` |
|       - | 2403 | `					 * operands in place and these are LIVE array elements (a` |
|       - | 2404 | `					 * null element used to come back bool(false) in the` |
|       - | 2405 | `					 * caller's array). */` |
|      17 | 2406 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|      17 | 2407 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|      17 | 2408 | `					PH7_MemObjLoad(pVal,&sV1);` |
|      17 | 2409 | `					PH7_MemObjLoad(pVal2,&sV2);` |
|      17 | 2410 | `					cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|      17 | 2411 | `					PH7_MemObjRelease(&sV1);` |
|      17 | 2412 | `					PH7_MemObjRelease(&sV2);` |
|      17 | 2413 | `					if( cmp == 0 ){` |
|       - | 2414 | `						/* identical key+value found in one of the arrays => drop it */` |
|      15 | 2415 | `						keep = 0;` |
|      15 | 2416 | `						break;` |
|       - | 2417 | `					}` |
|       1 | 2418 | `				}` |
|       1 | 2419 | `			}` |
|       2 | 2420 | `		}` |
|      29 | 2421 | `		if( keep ){` |
|       - | 2422 | `			/* Perform the insertion */` |
|      15 | 2423 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       7 | 2424 | `		}` |
|       - | 2425 | `		/* Point to the next entry */` |
|      29 | 2426 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      29 | 2427 | `		n--;` |
|       1 | 2428 | `	}` |
|       - | 2429 | `	/* Return the freshly created array */` |
|      13 | 2430 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 2431 | `	return PH7_OK;` |
|      14 | 2432 | `}` |
|       - | 2433 | `/*` |
|       - | 2434 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 2435 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 2436 | ` *  by a user supplied callback function.` |
|       - | 2437 | ` * Parameters` |
|       - | 2438 | ` *  $array1` |
|       - | 2439 | ` *    The array to compare from` |
|       - | 2440 | ` *  $array2` |
|       - | 2441 | ` *    An array to compare against` |
|       - | 2442 | ` *  $...` |
|       - | 2443 | ` *   More arrays to compare against.` |
|       - | 2444 | ` *  $key_compare_func` |
|       - | 2445 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 2446 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 2447 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 2448 | ` * Return` |
|       - | 2449 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 2450 | ` *  are not present in any of the other arrays.` |
|       - | 2451 | ` */` |
|      22 | 2452 | `PH7_PRIVATE int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 2453 | `{` |
|       - | 2454 | `	ph7_hashmap_node *pEntry;` |
|       - | 2455 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2456 | `	ph7_value *pCallback;` |
|       - | 2457 | `	ph7_value *pArray;` |
|       - | 2458 | `	sxi32 rc;` |
|       - | 2459 | `	sxu32 n;` |
|       - | 2460 | `	int i;` |
|       - | 2461 |  |
|       - | 2462 | `	/* Argument validation mimicking PHP errors. */` |
|      26 | 2463 | `	if( nArg < 2 ){` |
|     ! 0 | 2464 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2465 | `			"ArgumentCountError",` |
|       - | 2466 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|     ! 0 | 2467 | `			nArg` |
|       - | 2468 | `			);` |
|       - | 2469 | `	}` |
|      26 | 2470 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2471 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2472 | `			"TypeError",` |
|       - | 2473 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2474 | `			ph7_type_name(apArg[0])` |
|       - | 2475 | `			);` |
|       - | 2476 | `	}` |
|       - | 2477 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 2478 | `	 * expected to be a callback. */` |
|      38 | 2479 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      19 | 2480 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 2481 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2482 | `				"TypeError",` |
|       - | 2483 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 2484 | `				i + 1,` |
|       2 | 2485 | `				ph7_type_name(apArg[i])` |
|       - | 2486 | `				);` |
|       - | 2487 | `		}` |
|       9 | 2488 | `	}` |
|       - | 2489 | `	/* Point to the callback value */` |
|      22 | 2490 | `	pCallback = apArg[nArg - 1];` |
|      22 | 2491 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       - | 2492 | `		/* Compose an error message that closely matches PHP output. When the` |
|       - | 2493 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|       - | 2494 | `		 * If the value is neither array nor string, PHP says "no array or` |
|       - | 2495 | `		 * string given" which we also reproduce. */` |
|       9 | 2496 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 2497 | `			/* ARRAY CALLBACK must have exactly two members */` |
|       4 | 2498 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2499 | `				"TypeError",` |
|       - | 2500 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 2501 | `				nArg` |
|       - | 2502 | `				);` |
|       - | 2503 | `		}` |
|       6 | 2504 | `		if( !ph7_value_is_string(pCallback) ){` |
|       - | 2505 | `			/* neither array nor string */` |
|       8 | 2506 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2507 | `				"TypeError",` |
|       - | 2508 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 2509 | `				nArg` |
|       - | 2510 | `				);` |
|       - | 2511 | `		}` |
|       - | 2512 | `		/* Fallback for string (non-callable) or other leftover cases */` |
|     ! 0 | 2513 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2514 | `			"TypeError",` |
|       - | 2515 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, %s given",` |
|     ! 0 | 2516 | `			nArg,` |
|     ! 0 | 2517 | `			ph7_type_name(pCallback)` |
|       - | 2518 | `			);` |
|       - | 2519 | `	}` |
|      13 | 2520 | `	if( nArg == 2 ){` |
|       - | 2521 | `		/* If we only have the first array and the callback, just return the` |
|       - | 2522 | `		 * input array. */` |
|       3 | 2523 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2524 | `		return PH7_OK;` |
|       - | 2525 | `	}` |
|       - | 2526 | `	/* Create a new array */` |
|      11 | 2527 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 2528 | `	if( pArray == 0 ){` |
|     ! 0 | 2529 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2530 | `		return PH7_OK;` |
|       - | 2531 | `	}` |
|       - | 2532 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 2533 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2534 | `	/* Perform the diff */` |
|      11 | 2535 | `	pEntry = pSrc->pFirst;` |
|      11 | 2536 | `	n = pSrc->nEntry;` |
|      21 | 2537 | `	for(;;){` |
|       - | 2538 | `		int keep;` |
|      27 | 2539 | `		if( n < 1 ){` |
|       9 | 2540 | `			break;` |
|       - | 2541 | `		}` |
|      19 | 2542 | `		keep = 1;` |
|      31 | 2543 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 2544 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      23 | 2545 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2546 | `			/* we must compare keys via callback, not by direct lookup */` |
|      23 | 2547 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      45 | 2548 | `			while( pIt ){` |
|       - | 2549 | `				/* build temporary key values for callback */` |
|       - | 2550 | `				ph7_value key1, key2, result;` |
|       - | 2551 | `				/* initialise only once using the appropriate helper */` |
|      33 | 2552 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 2553 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 2554 | `				}else{` |
|       - | 2555 | `					SyString sStr;` |
|      33 | 2556 | `					SyStringInitFromBuf(&sStr,` |
|       - | 2557 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 2558 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      33 | 2559 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 2560 | `				}` |
|      33 | 2561 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 2562 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 2563 | `				}else{` |
|       - | 2564 | `					SyString sStr;` |
|      33 | 2565 | `					SyStringInitFromBuf(&sStr,` |
|       - | 2566 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 2567 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      33 | 2568 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 2569 | `				}` |
|      33 | 2570 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 2571 | `				/* call user callback with (key1, key2) */` |
|       - | 2572 | `				{` |
|       - | 2573 | `					ph7_value *apK[2];` |
|      33 | 2574 | `					apK[0] = &key1;` |
|      33 | 2575 | `					apK[1] = &key2;` |
|      33 | 2576 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 2577 | `				}` |
|      33 | 2578 | `				if( rc == PH7_EXCEPTION ){` |
|       - | 2579 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|       - | 2580 | `					 * array_uintersect (which signal back from` |
|       - | 2581 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|       - | 2582 | `					 * function invokes the callback inline, so it cleans up its own` |
|       - | 2583 | `					 * temporaries and propagates the exception directly. */` |
|       3 | 2584 | `					PH7_MemObjRelease(&result);` |
|       3 | 2585 | `					PH7_MemObjRelease(&key1);` |
|       3 | 2586 | `					PH7_MemObjRelease(&key2);` |
|       3 | 2587 | `					return PH7_EXCEPTION;` |
|       - | 2588 | `				}` |
|      31 | 2589 | `				if( rc == SXRET_OK ){` |
|      31 | 2590 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 2591 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 2592 | `					}` |
|      31 | 2593 | `					if( result.x.iVal == 0 ){` |
|       - | 2594 | `						/* keys considered equal by callback; now compare values */` |
|      13 | 2595 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      13 | 2596 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      13 | 2597 | `						if( pVal1 && pVal2 ){` |
|       - | 2598 | `							ph7_value sV1,sV2;` |
|       - | 2599 | `							sxi32 cmp;` |
|       - | 2600 | `							/* Compare on duplicates: PH7_MemObjCmp converts in` |
|       - | 2601 | `							 * place and these are LIVE array elements. */` |
|      13 | 2602 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|      13 | 2603 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|      13 | 2604 | `							PH7_MemObjLoad(pVal1,&sV1);` |
|      13 | 2605 | `							PH7_MemObjLoad(pVal2,&sV2);` |
|      13 | 2606 | `							cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|      13 | 2607 | `							PH7_MemObjRelease(&sV1);` |
|      13 | 2608 | `							PH7_MemObjRelease(&sV2);` |
|      13 | 2609 | `							if( cmp == 0 ){` |
|       9 | 2610 | `								keep = 0;` |
|       9 | 2611 | `								PH7_MemObjRelease(&result);` |
|       - | 2612 | `								/* release keys too before breaking */` |
|       9 | 2613 | `								PH7_MemObjRelease(&key1);` |
|       9 | 2614 | `								PH7_MemObjRelease(&key2);` |
|       9 | 2615 | `								break;` |
|       - | 2616 | `							}` |
|       2 | 2617 | `						}` |
|       2 | 2618 | `					}` |
|      11 | 2619 | `				}` |
|      23 | 2620 | `				PH7_MemObjRelease(&result);` |
|      23 | 2621 | `				PH7_MemObjRelease(&key1);` |
|      23 | 2622 | `				PH7_MemObjRelease(&key2);` |
|       - | 2623 | `				/* move to next node */` |
|      23 | 2624 | `				pIt = pIt->pPrev;` |
|      23 | 2625 | `				if( keep == 0 ) break;` |
|       1 | 2626 | `			}` |
|      21 | 2627 | `			if( keep == 0 ) break;` |
|       7 | 2628 | `		}` |
|      17 | 2629 | `		if( keep ){` |
|       - | 2630 | `			/* Perform the insertion */` |
|       9 | 2631 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 2632 | `		}` |
|       - | 2633 | `		/* Point to the next entry */` |
|      17 | 2634 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      17 | 2635 | `		n--;` |
|       1 | 2636 | `	}` |
|       - | 2637 | `	/* Return the freshly created array */` |
|       9 | 2638 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 2639 | `	return PH7_OK;` |
|      15 | 2640 | `}` |
|       - | 2641 | `/*` |
|       - | 2642 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 2643 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 2644 | ` * Parameters` |
|       - | 2645 | ` *  $array1` |
|       - | 2646 | ` *    The array to compare from` |
|       - | 2647 | ` *  $array2` |
|       - | 2648 | ` *    An array to compare against` |
|       - | 2649 | ` *  $...` |
|       - | 2650 | ` *   More arrays to compare against` |
|       - | 2651 | ` * Return` |
|       - | 2652 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 2653 | ` *  in any of the other arrays.` |
|       - | 2654 | ` * Note that NULL is returned on failure.` |
|       - | 2655 | ` */` |
|      12 | 2656 | `PH7_PRIVATE int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2657 | `{` |
|       - | 2658 | `	ph7_hashmap_node *pEntry;` |
|       - | 2659 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2660 | `	ph7_value *pArray;` |
|       - | 2661 | `	sxi32 rc;` |
|       - | 2662 | `	sxu32 n;` |
|       - | 2663 | `	int i;` |
|       - | 2664 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 2665 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 2666 | `	 * helpers. */` |
|      15 | 2667 | `	if( nArg < 1 ){` |
|     ! 0 | 2668 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2669 | `			"ArgumentCountError",` |
|       - | 2670 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|     ! 0 | 2671 | `			nArg` |
|       - | 2672 | `			);` |
|       - | 2673 | `	}` |
|      15 | 2674 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2675 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2676 | `			"TypeError",` |
|       - | 2677 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2678 | `			ph7_type_name(apArg[0])` |
|       - | 2679 | `			);` |
|       - | 2680 | `	}` |
|      20 | 2681 | `	for(i = 1 ; i < nArg ; i++){` |
|      12 | 2682 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 2683 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2684 | `				"TypeError",` |
|       - | 2685 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 2686 | `				i + 1,` |
|       2 | 2687 | `				ph7_type_name(apArg[i])` |
|       - | 2688 | `				);` |
|       - | 2689 | `		}` |
|       5 | 2690 | `	}` |
|       9 | 2691 | `	if( nArg == 1 ){` |
|       - | 2692 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 2693 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2694 | `		return PH7_OK;` |
|       - | 2695 | `	}` |
|       - | 2696 | `	/* Create a new array */` |
|       7 | 2697 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 2698 | `	if( pArray == 0 ){` |
|     ! 0 | 2699 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2700 | `		return PH7_OK;` |
|       - | 2701 | `	}` |
|       - | 2702 | `	/* Point to the internal representation of the main hashmap */` |
|       7 | 2703 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2704 | `	/* Perfrom the diff */` |
|       7 | 2705 | `	pEntry = pSrc->pFirst;` |
|       7 | 2706 | `	n = pSrc->nEntry;` |
|      12 | 2707 | `	for(;;){` |
|      25 | 2708 | `		if( n < 1 ){` |
|       7 | 2709 | `			break;` |
|       - | 2710 | `		}` |
|      31 | 2711 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 2712 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 2713 | `				/* ignore */` |
|     ! 0 | 2714 | `				continue;` |
|       - | 2715 | `			}` |
|      23 | 2716 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      23 | 2717 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      17 | 2718 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 2719 | `				/* Blob lookup */` |
|      17 | 2720 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       9 | 2721 | `			}else{` |
|       - | 2722 | `				/* Int lookup */` |
|       7 | 2723 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 2724 | `			}` |
|      23 | 2725 | `			if( rc == SXRET_OK ){` |
|       - | 2726 | `				/* Key exists,break immediately */` |
|      11 | 2727 | `				break;` |
|       - | 2728 | `			}` |
|       7 | 2729 | `		}` |
|      19 | 2730 | `		if( i >= nArg ){` |
|       - | 2731 | `			/* Perform the insertion */` |
|       9 | 2732 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 2733 | `		}` |
|       - | 2734 | `		/* Point to the next entry */` |
|      19 | 2735 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 2736 | `		n--;` |
|       1 | 2737 | `	}` |
|       - | 2738 | `	/* Return the freshly created array */` |
|       7 | 2739 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 2740 | `	return PH7_OK;` |
|       9 | 2741 | `}` |
|       - | 2742 | `/*` |
|       - | 2743 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 2744 | ` *  Computes the intersection of arrays.` |
|       - | 2745 | ` * Parameters` |
|       - | 2746 | ` *  $array1` |
|       - | 2747 | ` *    The array to compare from` |
|       - | 2748 | ` *  $array2` |
|       - | 2749 | ` *    An array to compare against` |
|       - | 2750 | ` *  $...` |
|       - | 2751 | ` *   More arrays to compare against` |
|       - | 2752 | ` * Return` |
|       - | 2753 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 2754 | ` *  in all of the parameters.` |
|       - | 2755 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 2756 | ` * Throws TypeError if any argument is not an array.` |
|       - | 2757 | ` */` |
|      20 | 2758 | `PH7_PRIVATE int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2759 | `{` |
|       - | 2760 | `	ph7_hashmap_node *pEntry;` |
|       - | 2761 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2762 | `	ph7_value *pArray;` |
|       - | 2763 | `	ph7_value *pVal;` |
|       - | 2764 | `	sxi32 rc;` |
|       - | 2765 | `	sxu32 n;` |
|       - | 2766 | `	int i;` |
|      23 | 2767 | `	if( nArg < 1 ){` |
|     ! 0 | 2768 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2769 | `			"ArgumentCountError",` |
|       - | 2770 | `			"array_intersect() expects at least 1 argument, %d given",` |
|     ! 0 | 2771 | `			nArg` |
|       - | 2772 | `			);` |
|       - | 2773 | `	}` |
|      23 | 2774 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2775 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2776 | `			"TypeError",` |
|       - | 2777 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2778 | `			ph7_type_name(apArg[0])` |
|       - | 2779 | `			);` |
|       - | 2780 | `	}` |
|      36 | 2781 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 2782 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 2783 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2784 | `				"TypeError",` |
|       - | 2785 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       1 | 2786 | `				i + 1,` |
|       2 | 2787 | `				ph7_type_name(apArg[i])` |
|       - | 2788 | `				);` |
|       - | 2789 | `		}` |
|       9 | 2790 | `	}` |
|      17 | 2791 | `	if( nArg == 1 ){` |
|       - | 2792 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 2793 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2794 | `		return PH7_OK;` |
|       - | 2795 | `	}` |
|       - | 2796 | `	/* Create a new array */` |
|      15 | 2797 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 2798 | `	if( pArray == 0 ){` |
|     ! 0 | 2799 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2800 | `		return PH7_OK;` |
|       - | 2801 | `	}` |
|       - | 2802 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 2803 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2804 | `	/* Perform the intersection */` |
|      15 | 2805 | `	pEntry = pSrc->pFirst;` |
|      15 | 2806 | `	n = pSrc->nEntry;` |
|      31 | 2807 | `	for(;;){` |
|      63 | 2808 | `		if( n < 1 ){` |
|      15 | 2809 | `			break;` |
|       - | 2810 | `		}` |
|       - | 2811 | `		/* Extract the node value */` |
|      49 | 2812 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      49 | 2813 | `		if( pVal ){` |
|      79 | 2814 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 2815 | `				/* Point to the internal representation of the hashmap */` |
|      55 | 2816 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2817 | `				/* Perform the lookup */` |
|      55 | 2818 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      55 | 2819 | `				if( rc != SXRET_OK ){` |
|       - | 2820 | `					/* Value does not exist */` |
|      25 | 2821 | `					break;` |
|       - | 2822 | `				}` |
|      16 | 2823 | `			}` |
|      49 | 2824 | `			if( i >= nArg ){` |
|       - | 2825 | `				/* Perform the insertion */` |
|      25 | 2826 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 2827 | `			}` |
|      24 | 2828 | `		}` |
|       - | 2829 | `		/* Point to the next entry */` |
|      49 | 2830 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      49 | 2831 | `		n--;` |
|       1 | 2832 | `	}` |
|       - | 2833 | `	/* Return the freshly created array */` |
|      15 | 2834 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 2835 | `	return PH7_OK;` |
|      13 | 2836 | `}` |
|       - | 2837 | `/*` |
|       - | 2838 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 2839 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 2840 | ` * Parameters` |
|       - | 2841 | ` *  $array1` |
|       - | 2842 | ` *    The array to compare from` |
|       - | 2843 | ` *  $array2` |
|       - | 2844 | ` *    An array to compare against` |
|       - | 2845 | ` *  $...` |
|       - | 2846 | ` *   More arrays to compare against` |
|       - | 2847 | ` * Return` |
|       - | 2848 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 2849 | ` *  in all the arguments, with matching keys.` |
|       - | 2850 | ` */` |
|      20 | 2851 | `PH7_PRIVATE int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2852 | `{` |
|       - | 2853 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 2854 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2855 | `	ph7_value *pArray;` |
|       - | 2856 | `	ph7_value *pVal;` |
|       - | 2857 | `	sxi32 rc;` |
|       - | 2858 | `	sxu32 n;` |
|       - | 2859 | `	int i;` |
|      23 | 2860 | `	if( nArg < 1 ){` |
|     ! 0 | 2861 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2862 | `			"ArgumentCountError",` |
|       - | 2863 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|     ! 0 | 2864 | `			nArg` |
|       - | 2865 | `			);` |
|       - | 2866 | `	}` |
|      23 | 2867 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2868 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2869 | `			"TypeError",` |
|       - | 2870 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2871 | `			ph7_type_name(apArg[0])` |
|       - | 2872 | `			);` |
|       - | 2873 | `	}` |
|      36 | 2874 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 2875 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 2876 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2877 | `				"TypeError",` |
|       - | 2878 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 2879 | `				i + 1,` |
|       2 | 2880 | `				ph7_type_name(apArg[i])` |
|       - | 2881 | `				);` |
|       - | 2882 | `		}` |
|       9 | 2883 | `	}` |
|      17 | 2884 | `	if( nArg == 1 ){` |
|       - | 2885 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 2886 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2887 | `		return PH7_OK;` |
|       - | 2888 | `	}` |
|       - | 2889 | `	/* Create a new array */` |
|      15 | 2890 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 2891 | `	if( pArray == 0 ){` |
|     ! 0 | 2892 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2893 | `		return PH7_OK;` |
|       - | 2894 | `	}` |
|       - | 2895 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 2896 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2897 | `	/* Perform the intersection */` |
|      15 | 2898 | `	pEntry = pSrc->pFirst;` |
|      15 | 2899 | `	n = pSrc->nEntry;` |
|      15 | 2900 | `	pN1 = pN2 = 0; /* cc warning */` |
|      23 | 2901 | `	for(;;){` |
|      47 | 2902 | `		if( n < 1 ){` |
|      15 | 2903 | `			break;` |
|       - | 2904 | `		}` |
|       - | 2905 | `		/* Extract the node value */` |
|      33 | 2906 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      33 | 2907 | `		if( pVal ){` |
|      53 | 2908 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 2909 | `				/* Point to the internal representation of the hashmap */` |
|      37 | 2910 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2911 | `				/* Perform a key lookup first */` |
|      37 | 2912 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      15 | 2913 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       8 | 2914 | `				}else{` |
|      23 | 2915 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 2916 | `				}` |
|      37 | 2917 | `				if( rc != SXRET_OK ){` |
|       - | 2918 | `					/* No such key,break immediately */` |
|       7 | 2919 | `					break;` |
|       - | 2920 | `				}` |
|       - | 2921 | `				/* Perform the lookup */` |
|      31 | 2922 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      31 | 2923 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|       - | 2924 | `					/* Value does not exist */` |
|       6 | 2925 | `					break;` |
|       - | 2926 | `				}` |
|      11 | 2927 | `			}` |
|      33 | 2928 | `			if( i >= nArg ){` |
|       - | 2929 | `				/* Perform the insertion */` |
|      17 | 2930 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 2931 | `			}` |
|      16 | 2932 | `		}` |
|       - | 2933 | `		/* Point to the next entry */` |
|      33 | 2934 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 2935 | `		n--;` |
|       1 | 2936 | `	}` |
|       - | 2937 | `	/* Return the freshly created array */` |
|      15 | 2938 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 2939 | `	return PH7_OK;` |
|      13 | 2940 | `}` |
|       - | 2941 | `/*` |
|       - | 2942 | ` * array array_intersect_key(array $array1 ,...)` |
|       - | 2943 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 2944 | ` * Parameters` |
|       - | 2945 | ` *  $array1` |
|       - | 2946 | ` *    The array to compare from` |
|       - | 2947 | ` *  $...` |
|       - | 2948 | ` *   More arrays to compare against` |
|       - | 2949 | ` * Return` |
|       - | 2950 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 2951 | ` *  have keys that are present in all arguments.` |
|       - | 2952 | ` * Note that NULL is returned on failure.` |
|       - | 2953 | ` */` |
|      20 | 2954 | `PH7_PRIVATE int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2955 | `{` |
|       - | 2956 | `	ph7_hashmap_node *pEntry;` |
|       - | 2957 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2958 | `	ph7_value *pArray;` |
|       - | 2959 | `	sxi32 rc;` |
|       - | 2960 | `	sxu32 n;` |
|       - | 2961 | `	int i;` |
|      23 | 2962 | `	if( nArg < 1 ){` |
|     ! 0 | 2963 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2964 | `			"ArgumentCountError",` |
|       - | 2965 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|     ! 0 | 2966 | `			nArg` |
|       - | 2967 | `			);` |
|       - | 2968 | `	}` |
|      23 | 2969 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2970 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2971 | `			"TypeError",` |
|       - | 2972 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2973 | `			ph7_type_name(apArg[0])` |
|       - | 2974 | `			);` |
|       - | 2975 | `	}` |
|      36 | 2976 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 2977 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 2978 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2979 | `				"TypeError",` |
|       - | 2980 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|       1 | 2981 | `				i + 1,` |
|       2 | 2982 | `				ph7_type_name(apArg[i])` |
|       - | 2983 | `				);` |
|       - | 2984 | `		}` |
|       9 | 2985 | `	}` |
|      17 | 2986 | `	if( nArg == 1 ){` |
|       - | 2987 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 2988 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2989 | `		return PH7_OK;` |
|       - | 2990 | `	}` |
|       - | 2991 | `	/* Create a new array */` |
|      15 | 2992 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 2993 | `	if( pArray == 0 ){` |
|     ! 0 | 2994 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2995 | `		return PH7_OK;` |
|       - | 2996 | `	}` |
|       - | 2997 | `	/* Point to the internal representation of the main hashmap */` |
|      15 | 2998 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2999 | `	/* Perform the intersection */` |
|      15 | 3000 | `	pEntry = pSrc->pFirst;` |
|      15 | 3001 | `	n = pSrc->nEntry;` |
|      24 | 3002 | `	for(;;){` |
|      49 | 3003 | `		if( n < 1 ){` |
|      15 | 3004 | `			break;` |
|       - | 3005 | `		}` |
|      57 | 3006 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      39 | 3007 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      39 | 3008 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      27 | 3009 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 3010 | `				/* Blob lookup */` |
|      27 | 3011 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      14 | 3012 | `			}else{` |
|       - | 3013 | `				/* Int key */` |
|      13 | 3014 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 3015 | `			}` |
|      39 | 3016 | `			if( rc != SXRET_OK ){` |
|       - | 3017 | `				/* Key does not exist, break immediately */` |
|      17 | 3018 | `				break;` |
|       - | 3019 | `			}` |
|      12 | 3020 | `		}` |
|      35 | 3021 | `		if( i >= nArg ){` |
|       - | 3022 | `			/* Perform the insertion */` |
|      19 | 3023 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       9 | 3024 | `		}` |
|       - | 3025 | `		/* Point to the next entry */` |
|      35 | 3026 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      35 | 3027 | `		n--;` |
|       1 | 3028 | `	}` |
|       - | 3029 | `	/* Return the freshly created array */` |
|      15 | 3030 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 3031 | `	return PH7_OK;` |
|      13 | 3032 | `}` |
|       - | 3033 | `/*` |
|       - | 3034 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 3035 | ` *  Computes the intersection of arrays.` |
|       - | 3036 | ` * Parameters` |
|       - | 3037 | ` *  $array1` |
|       - | 3038 | ` *    The array to compare from` |
|       - | 3039 | ` *  $array2` |
|       - | 3040 | ` *    An array to compare against` |
|       - | 3041 | ` *  $...` |
|       - | 3042 | ` *   More arrays to compare against` |
|       - | 3043 | ` * $callback` |
|       - | 3044 | ` *  The callback comparison function.` |
|       - | 3045 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 3046 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 3047 | ` *  than the second.` |
|       - | 3048 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 3049 | ` * Return` |
|       - | 3050 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 3051 | ` *  in all of the parameters. .` |
|       - | 3052 | ` * Note that NULL is returned on failure.` |
|       - | 3053 | ` */` |
|      24 | 3054 | `PH7_PRIVATE int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3055 | `{` |
|       - | 3056 | `	ph7_hashmap_node *pEntry;` |
|       - | 3057 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3058 | `	ph7_value *pCallback;` |
|       - | 3059 | `	ph7_value *pArray;` |
|       - | 3060 | `	ph7_value *pVal;` |
|       - | 3061 | `	sxi32 rc;` |
|       - | 3062 | `	sxu32 n;` |
|       - | 3063 | `	int i;` |
|       - | 3064 |  |
|       - | 3065 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      29 | 3066 | `	if( nArg < 2 ){` |
|     ! 0 | 3067 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3068 | `			"ArgumentCountError",` |
|       - | 3069 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|     ! 0 | 3070 | `			nArg` |
|       - | 3071 | `			);` |
|       - | 3072 | `	}` |
|      29 | 3073 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3074 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3075 | `			"TypeError",` |
|       - | 3076 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3077 | `			ph7_type_name(apArg[0])` |
|       - | 3078 | `			);` |
|       - | 3079 | `	}` |
|       - | 3080 |  |
|      27 | 3081 | `	if( nArg == 2 ){` |
|       - | 3082 | `		/* Only the original array and the callback were provided. */` |
|       - | 3083 | `		/* Validate the callback below in order to match PHP's parameter` |
|       - | 3084 | `		 * validation ordering. */` |
|       3 | 3085 | `	} else {` |
|       - | 3086 | `		/* Ensure intermediary arguments are arrays (matches PHP strict typing). */` |
|      39 | 3087 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|      23 | 3088 | `			if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3089 | `				return PH7_VmThrowException(pCtx,` |
|       - | 3090 | `					"TypeError",` |
|       - | 3091 | `					"array_uintersect(): Argument #%d must be of type array, %s given",` |
|       1 | 3092 | `					i + 1,` |
|       2 | 3093 | `					ph7_type_name(apArg[i])` |
|       - | 3094 | `					);` |
|       - | 3095 | `			}` |
|      13 | 3096 | `		}` |
|       - | 3097 | `	}` |
|       - | 3098 |  |
|       - | 3099 | `	/* Identify the callback (always expected as the last argument). */` |
|      25 | 3100 | `	pCallback = apArg[nArg - 1];` |
|       - | 3101 | `	/* Validate the callback to match PHP's error messages. */` |
|      25 | 3102 | `	if( !ph7_value_is_callable(pCallback) ){` |
|      14 | 3103 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 3104 | `			/* PHP emits a special message when the array length is wrong.` |
|       - | 3105 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|       - | 3106 | `			 * method / missing class), we must emit a more general error instead.` |
|       - | 3107 | `			 */` |
|       9 | 3108 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|       9 | 3109 | `			if( pCb->nEntry != 2 ){` |
|       4 | 3110 | `				return PH7_VmThrowException(pCtx,` |
|       - | 3111 | `					"TypeError",` |
|       - | 3112 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 3113 | `					nArg` |
|       - | 3114 | `					);` |
|       - | 3115 | `			}` |
|       - | 3116 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|       - | 3117 | `			{` |
|       6 | 3118 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|       6 | 3119 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|       6 | 3120 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|       - | 3121 | `					int nMethodLen;` |
|       6 | 3122 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|       6 | 3123 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|       6 | 3124 | `					if( pClass ){` |
|       - | 3125 | `						/* Class exists but method is missing. */` |
|       4 | 3126 | `						return PH7_VmThrowException(pCtx,` |
|       - | 3127 | `							"TypeError",` |
|       - | 3128 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|       1 | 3129 | `							nArg,` |
|       1 | 3130 | `							(const char *)SyStringData(&pClass->sName),` |
|       1 | 3131 | `							zMethod` |
|       - | 3132 | `							);` |
|       - | 3133 | `					}` |
|       - | 3134 | `					/* Class not found */` |
|       - | 3135 | `					{` |
|       - | 3136 | `						int nName;` |
|       3 | 3137 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|       4 | 3138 | `						return PH7_VmThrowException(pCtx,` |
|       - | 3139 | `							"TypeError",` |
|       - | 3140 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|       1 | 3141 | `							nArg,` |
|       1 | 3142 | `							zName` |
|       - | 3143 | `							);` |
|       - | 3144 | `					}` |
|       - | 3145 | `				}` |
|       - | 3146 | `			}` |
|       - | 3147 | `			/* Fallback message */` |
|     ! 0 | 3148 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3149 | `				"TypeError",` |
|       - | 3150 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|     ! 0 | 3151 | `				nArg` |
|       - | 3152 | `				);` |
|       - | 3153 | `		}` |
|       6 | 3154 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 3155 | `			int len;` |
|       3 | 3156 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       4 | 3157 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3158 | `				"TypeError",` |
|       - | 3159 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 3160 | `				nArg,` |
|       1 | 3161 | `				zName` |
|       - | 3162 | `				);` |
|       - | 3163 | `		}` |
|       4 | 3164 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3165 | `			"TypeError",` |
|       - | 3166 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 3167 | `			nArg` |
|       - | 3168 | `			);` |
|       - | 3169 | `	}` |
|       - | 3170 |  |
|      11 | 3171 | `	if( nArg == 2 ){` |
|       - | 3172 | `		/* Only the original array and the callback were provided. */` |
|       5 | 3173 | `		ph7_result_value(pCtx,apArg[0]);` |
|       5 | 3174 | `		return PH7_OK;` |
|       - | 3175 | `	}` |
|       - | 3176 |  |
|       - | 3177 | `	/* Create a new array */` |
|       7 | 3178 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 3179 | `	if( pArray == 0 ){` |
|     ! 0 | 3180 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3181 | `		return PH7_OK;` |
|       - | 3182 | `	}` |
|       - | 3183 | `	/* Point to the internal representation of the source hashmap */` |
|       7 | 3184 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3185 | `	/* Perform the intersection */` |
|       7 | 3186 | `	pEntry = pSrc->pFirst;` |
|       7 | 3187 | `	n = pSrc->nEntry;` |
|       7 | 3188 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|       9 | 3189 | `	for(;;){` |
|      19 | 3190 | `		if( n < 1 ){` |
|       5 | 3191 | `			break;` |
|       - | 3192 | `		}` |
|       - | 3193 | `		/* Extract the node value */` |
|      15 | 3194 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 3195 | `		if( pVal ){` |
|      23 | 3196 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|      15 | 3197 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 3198 | `					/* ignore */` |
|     ! 0 | 3199 | `					continue;` |
|       - | 3200 | `				}` |
|       - | 3201 | `				/* Point to the internal representation of the hashmap */` |
|      15 | 3202 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3203 | `				/* Perform the lookup */` |
|      15 | 3204 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|      15 | 3205 | `				if( rc != SXRET_OK ){` |
|       - | 3206 | `					/* Value does not exist */` |
|       7 | 3207 | `					break;` |
|       - | 3208 | `				}` |
|       5 | 3209 | `			}` |
|      15 | 3210 | `			if( i >= (nArg-1) ){` |
|       - | 3211 | `				/* Perform the insertion */` |
|       9 | 3212 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 3213 | `			}` |
|       7 | 3214 | `		}` |
|      15 | 3215 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 3216 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 3217 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 3218 | `			return PH7_EXCEPTION;` |
|       - | 3219 | `		}` |
|       - | 3220 | `		/* Point to the next entry */` |
|      13 | 3221 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      13 | 3222 | `		n--;` |
|       1 | 3223 | `	}` |
|       - | 3224 | `	/* Return the freshly created array */` |
|       5 | 3225 | `	ph7_result_value(pCtx,pArray);` |
|       5 | 3226 | `	return PH7_OK;` |
|      17 | 3227 | `}` |
|       - | 3228 | `/*` |
|       - | 3229 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 3230 | ` *  Fill an array with values.` |
|       - | 3231 | ` * Parameters` |
|       - | 3232 | ` *  $start_index` |
|       - | 3233 | ` *    The first index of the returned array.` |
|       - | 3234 | ` *  $num` |
|       - | 3235 | ` *   Number of elements to insert.` |
|       - | 3236 | ` *  $value` |
|       - | 3237 | ` *    Value to use for filling.` |
|       - | 3238 | ` * Return` |
|       - | 3239 | ` *  The filled array or null on failure.` |
|       - | 3240 | ` */` |
|     234 | 3241 | `PH7_PRIVATE int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3242 | `{` |
|       - | 3243 | `	ph7_value *pArray;` |
|       - | 3244 | `	int i,nEntry;` |
|       - | 3245 |  |
|       - | 3246 | `	/* PHP enforces argument count and type checks. */` |
|     239 | 3247 | `	if( nArg != 3 ){` |
|       - | 3248 | `		/* wrong number of arguments -> ArgumentCountError */` |
|     ! 0 | 3249 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3250 | `			"ArgumentCountError",` |
|       - | 3251 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|     ! 0 | 3252 | `			nArg` |
|       - | 3253 | `			);` |
|       - | 3254 | `	}` |
|       - | 3255 |  |
|       - | 3256 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 3257 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 3258 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 3259 | `	 * and NULLs are rejected outright. */` |
|     351 | 3260 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     356 | 3261 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|     ! 0 | 3262 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3263 | `			"TypeError",` |
|       - | 3264 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|     ! 0 | 3265 | `			ph7_type_name(apArg[0])` |
|       - | 3266 | `			);` |
|       - | 3267 | `	}` |
|     239 | 3268 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 3269 | `		int len;` |
|       6 | 3270 | `		sxu8 bReal = FALSE;` |
|       6 | 3271 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       6 | 3272 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 3273 | `			/* Non‑numeric string is an error. */` |
|       3 | 3274 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3275 | `				"TypeError",` |
|       - | 3276 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 3277 | `				);` |
|       - | 3278 | `		}` |
|       3 | 3279 | `		if( bReal ){` |
|       - | 3280 | `			/* float-string -> deprecation warning */` |
|     ! 0 | 3281 | `			PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 3282 | `				"Implicit conversion from float-string to int loses precision");` |
|     ! 0 | 3283 | `		}` |
|       1 | 3284 | `	}` |
|       - | 3285 |  |
|       - | 3286 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 3287 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     348 | 3288 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     352 | 3289 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 3290 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3291 | `			"TypeError",` |
|       - | 3292 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 3293 | `			ph7_type_name(apArg[1])` |
|       - | 3294 | `			);` |
|       - | 3295 | `	}` |
|     236 | 3296 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 3297 | `		int len;` |
|       3 | 3298 | `		sxu8 bReal = FALSE;` |
|       3 | 3299 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 3300 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 3301 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3302 | `				"TypeError",` |
|       - | 3303 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 3304 | `				);` |
|       - | 3305 | `		}` |
|     ! 0 | 3306 | `	}` |
|       - | 3307 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|       - | 3308 | `	 * will be converted by ph7_value_to_int below. */` |
|     233 | 3309 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 3310 | `		double d = ph7_value_to_double(apArg[1]);` |
|       - | 3311 | `		/* avoid hiding outer 'i' (loop index) */` |
|       3 | 3312 | `		sxi64 i64 = (sxi64)d;` |
|       3 | 3313 | `		if( d != (double)i64 ){` |
|       3 | 3314 | `			PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 3315 | `				"Implicit conversion from float to int loses precision");` |
|       1 | 3316 | `		}` |
|       1 | 3317 | `	}` |
|       - | 3318 |  |
|       - | 3319 | `	/* Total number of entries to insert */` |
|     233 | 3320 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       - | 3321 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     233 | 3322 | `	if( nEntry < 0 ){` |
|       3 | 3323 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3324 | `			"ValueError",` |
|       - | 3325 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 3326 | `			);` |
|       - | 3327 | `	}` |
|       - | 3328 |  |
|       - | 3329 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     230 | 3330 | `	if( nEntry == 0 ){` |
|       5 | 3331 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       5 | 3332 | `		return PH7_OK;` |
|       - | 3333 | `	}` |
|       - | 3334 |  |
|       - | 3335 | `	/* Create a new array */` |
|     226 | 3336 | `	pArray = ph7_context_new_array(pCtx);` |
|     226 | 3337 | `	if( pArray == 0 ){` |
|     ! 0 | 3338 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 3339 | `	}` |
|       - | 3340 |  |
|       - | 3341 | `	/* PHP 8 fills consecutive integer keys start_index, start_index+1, … even` |
|       - | 3342 | `	 * when start_index is negative (PHP 7 restarted the remaining keys from 0,` |
|       - | 3343 | `	 * so array_fill(-5,3) gave -5,0,1 instead of -5,-4,-3). Assign each key` |
|       - | 3344 | `	 * explicitly rather than relying on automatic (append) indexing. */` |
|     226 | 3345 | `	int iStart = ph7_value_to_int(apArg[0]);` |
| 2117826 | 3346 | `	for( i = 0 ; i < nEntry ; i++ ){` |
| 2117602 | 3347 | `		if( ph7_array_add_intkey_elem(pArray, iStart + i, apArg[2]) != SXRET_OK ){` |
|       - | 3348 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|     ! 0 | 3349 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 3350 | `		}` |
| 1058802 | 3351 | `	}` |
|       - | 3352 | `	/* Return the filled array */` |
|     226 | 3353 | `	ph7_result_value(pCtx, pArray);` |
|     226 | 3354 | `	return PH7_OK;` |
|     122 | 3355 | `}` |
|       - | 3356 | `/*` |
|       - | 3357 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 3358 | ` *  Fill an array with values, specifying keys.` |
|       - | 3359 | ` * Parameters` |
|       - | 3360 | ` *  $input` |
|       - | 3361 | ` *   Array of values that will be used as key.` |
|       - | 3362 | ` *  $value` |
|       - | 3363 | ` *    Value to use for filling.` |
|       - | 3364 | ` * Return` |
|       - | 3365 | ` *  The filled array.` |
|       - | 3366 | ` * Throws` |
|       - | 3367 | ` *  ValueError if $input is not an array.` |
|       - | 3368 | ` */` |
|      20 | 3369 | `PH7_PRIVATE int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3370 | `{` |
|       - | 3371 | `	ph7_hashmap_node *pEntry;` |
|       - | 3372 | `	ph7_hashmap *pSrc;` |
|       - | 3373 | `	ph7_value *pArray;` |
|       - | 3374 | `	sxu32 n;` |
|       - | 3375 | `	/* PHP enforces exactly 2 arguments. */` |
|      23 | 3376 | `	if( nArg != 2 ){` |
|     ! 0 | 3377 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3378 | `			"ArgumentCountError",` |
|       - | 3379 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|     ! 0 | 3380 | `			nArg` |
|       - | 3381 | `			);` |
|       - | 3382 | `	}` |
|       - | 3383 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 3384 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       8 | 3385 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3386 | `			"TypeError",` |
|       - | 3387 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|       2 | 3388 | `			ph7_type_name(apArg[0])` |
|       - | 3389 | `			);` |
|       - | 3390 | `	}` |
|       - | 3391 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 3392 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3393 | `	/* Create a new array */` |
|      17 | 3394 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 3395 | `	if( pArray == 0 ){` |
|     ! 0 | 3396 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3397 | `		return PH7_OK;` |
|       - | 3398 | `	}` |
|       - | 3399 | `	/* Perform the requested operation */` |
|      17 | 3400 | `	pEntry = pSrc->pFirst;` |
|      45 | 3401 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      29 | 3402 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|       - | 3403 | `		/* Point to the next entry */` |
|      29 | 3404 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 3405 | `	}` |
|       - | 3406 | `	/* Return the filled array */` |
|      17 | 3407 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 3408 | `	return PH7_OK;` |
|      13 | 3409 | `}` |
|       - | 3410 | `/*` |
|       - | 3411 | ` * array array_combine(array $keys,array $values)` |
|       - | 3412 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 3413 | ` * Parameters` |
|       - | 3414 | ` *  $keys` |
|       - | 3415 | ` *    Array of keys to be used.` |
|       - | 3416 | ` * $values` |
|       - | 3417 | ` *   Array of values to be used.` |
|       - | 3418 | ` * Return` |
|       - | 3419 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 3420 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 3421 | ` *  not an array.` |
|       - | 3422 | ` */` |
|      16 | 3423 | `PH7_PRIVATE int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 3424 | `{` |
|       - | 3425 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 3426 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 3427 | `	ph7_value *pArray;` |
|       - | 3428 | `	sxu32 n;` |
|       - | 3429 | `	/* PHP enforces argument count and type checks. */` |
|      20 | 3430 | `	if( nArg != 2 ){` |
|       - | 3431 | `		/* wrong number of arguments -> ArgumentCountError */` |
|     ! 0 | 3432 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3433 | `			"ArgumentCountError",` |
|       - | 3434 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|     ! 0 | 3435 | `			nArg` |
|       - | 3436 | `			);` |
|       - | 3437 | `	}` |
|       - | 3438 | `	/* Validate argument types individually so we can report the correct` |
|       - | 3439 | `	 * argument index in the error message. */` |
|      20 | 3440 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3441 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3442 | `			"TypeError",` |
|       - | 3443 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 3444 | `			ph7_type_name(apArg[0])` |
|       - | 3445 | `			);` |
|       - | 3446 | `	}` |
|      17 | 3447 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 3448 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3449 | `			"TypeError",` |
|       - | 3450 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 3451 | `			ph7_type_name(apArg[1])` |
|       - | 3452 | `			);` |
|       - | 3453 | `	}` |
|       - | 3454 | `	/* Point to the internal representation of the input hashmaps */` |
|      14 | 3455 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 3456 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      14 | 3457 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 3458 | `		/* Length mismatch -> ValueError */` |
|       3 | 3459 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3460 | `			"ValueError",` |
|       - | 3461 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 3462 | `			);` |
|       - | 3463 | `	}` |
|       - | 3464 | `	/* Create a new array */` |
|      11 | 3465 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 3466 | `	if( pArray == 0 ){` |
|     ! 0 | 3467 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3468 | `		return PH7_OK;` |
|       - | 3469 | `	}` |
|       - | 3470 | `	/* Perform the requested operation */` |
|      11 | 3471 | `	pKe = pKey->pFirst;` |
|      11 | 3472 | `	pVe = pValue->pFirst;` |
|      33 | 3473 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      23 | 3474 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      23 | 3475 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 3476 | `		/* PHP treats floats used as keys in array_combine differently than` |
|       - | 3477 | `		 * ordinary offset access: the float is stringified rather than` |
|       - | 3478 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|       - | 3479 | `		 * the value when it is a float and convert the copy to string.  The` |
|       - | 3480 | `		 * original array must not be mutated. */` |
|      23 | 3481 | `		ph7_value *pKeyCopy = pKeyVal;` |
|      23 | 3482 | `		if( ph7_value_is_float(pKeyVal) ){` |
|       5 | 3483 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|       5 | 3484 | `			if( pTmpKey ){` |
|       5 | 3485 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|       - | 3486 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|       5 | 3487 | `				PH7_MemObjToString(pTmpKey);` |
|       5 | 3488 | `				pKeyCopy = pTmpKey;` |
|       2 | 3489 | `			}` |
|       2 | 3490 | `		}` |
|      23 | 3491 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|       - | 3492 | `		/* Point to the next entry */` |
|      23 | 3493 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      23 | 3494 | `		pVe = pVe->pPrev;` |
|      12 | 3495 | `	}` |
|       - | 3496 | `	/* Return the filled array */` |
|      11 | 3497 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 3498 | `	return PH7_OK;` |
|      12 | 3499 | `}` |
|       - | 3500 | `/*` |
|       - | 3501 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 3502 | ` *  Return an array with elements in reverse order.` |
|       - | 3503 | ` * Parameters` |
|       - | 3504 | ` *  $array` |
|       - | 3505 | ` *   The input array.` |
|       - | 3506 | ` *  $preserve_keys (optional)` |
|       - | 3507 | ` *   If set to TRUE keys are preserved.` |
|       - | 3508 | ` * Return` |
|       - | 3509 | ` *  The reversed array.` |
|       - | 3510 | ` */` |
|      18 | 3511 | `PH7_PRIVATE int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3512 | `{` |
|       - | 3513 | `	ph7_hashmap_node *pEntry;` |
|       - | 3514 | `	ph7_hashmap *pSrc;` |
|       - | 3515 | `	ph7_value *pArray;` |
|       - | 3516 | `	int bPreserve;` |
|       - | 3517 | `	sxu32 n;` |
|      20 | 3518 | `	if( nArg < 1 ){` |
|     ! 0 | 3519 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3520 | `			"ArgumentCountError",` |
|       - | 3521 | `			"array_reverse() expects at least 1 argument, %d given",` |
|     ! 0 | 3522 | `			nArg` |
|       - | 3523 | `			);` |
|       - | 3524 | `	}` |
|       - | 3525 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 3526 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3527 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3528 | `			"TypeError",` |
|       - | 3529 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3530 | `			ph7_type_name(apArg[0])` |
|       - | 3531 | `			);` |
|       - | 3532 | `	}` |
|      17 | 3533 | `	bPreserve = FALSE;` |
|      17 | 3534 | `	if( nArg > 1 ){` |
|       7 | 3535 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       3 | 3536 | `	}` |
|       - | 3537 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 3538 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3539 | `	/* Create a new array */` |
|      17 | 3540 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 3541 | `	if( pArray == 0 ){` |
|     ! 0 | 3542 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3543 | `		return PH7_OK;` |
|       - | 3544 | `	}` |
|       - | 3545 | `	/* Perform the requested operation */` |
|      17 | 3546 | `	pEntry = pSrc->pLast;` |
|      55 | 3547 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 3548 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      39 | 3549 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      39 | 3550 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 3551 | `		/* Point to the previous entry */` |
|      39 | 3552 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      20 | 3553 | `	}` |
|      17 | 3554 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 3555 | `	return PH7_OK;` |
|      11 | 3556 | `}` |
|       - | 3557 | `/*` |
|       - | 3558 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|       - | 3559 | ` *  Removes duplicate values from an array.` |
|       - | 3560 | ` * Parameters` |
|       - | 3561 | ` *  $array` |
|       - | 3562 | ` *   The input array.` |
|       - | 3563 | ` *  $flags` |
|       - | 3564 | ` *   The optional second parameter may be used to modify the comparison` |
|       - | 3565 | ` *   behavior using these values:` |
|       - | 3566 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|       - | 3567 | ` *     SORT_NUMERIC - compare items numerically` |
|       - | 3568 | ` *     SORT_STRING  - compare items as strings` |
|       - | 3569 | ` * Return` |
|       - | 3570 | ` *  The filtered array.` |
|       - | 3571 | ` */` |
|      34 | 3572 | `PH7_PRIVATE int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3573 | `{` |
|       - | 3574 | `	ph7_hashmap_node *pEntry;` |
|       - | 3575 | `	ph7_value *pNeedle;` |
|       - | 3576 | `	ph7_hashmap *pSrc;` |
|       - | 3577 | `	ph7_value *pArray;` |
|       - | 3578 | `	int iFlags,base,bFold;` |
|       - | 3579 | `	sxu32 n;` |
|      36 | 3580 | `	if( nArg < 1 ){` |
|       - | 3581 | `		/* Missing arguments, throw ArgumentCountError */` |
|     ! 0 | 3582 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3583 | `			"ArgumentCountError",` |
|       - | 3584 | `			"array_unique() expects at least 1 argument, 0 given"` |
|       - | 3585 | `			);` |
|       - | 3586 | `	}` |
|      36 | 3587 | `	if( nArg > 2 ){` |
|       - | 3588 | `		/* Too many arguments, throw ArgumentCountError */` |
|     ! 0 | 3589 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3590 | `			"ArgumentCountError",` |
|       - | 3591 | `			"array_unique() expects at most 2 arguments, %d given",` |
|     ! 0 | 3592 | `			nArg` |
|       - | 3593 | `			);` |
|       - | 3594 | `	}` |
|       - | 3595 | `	/* Make sure we are dealing with a valid hashmap */` |
|      36 | 3596 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3597 | `		/* Type mismatch, throw TypeError */` |
|       4 | 3598 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3599 | `			"TypeError",` |
|       - | 3600 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3601 | `			ph7_type_name(apArg[0])` |
|       - | 3602 | `			);` |
|       - | 3603 | `	}` |
|       - | 3604 | `	/* php's default is SORT_STRING (2): elements compare as strings. Explicit` |
|       - | 3605 | `	 * flags select numeric / natural / case-insensitive comparison. */` |
|      33 | 3606 | `	iFlags = nArg > 1 ? ph7_value_to_int(apArg[1]) : 2 /* SORT_STRING */;` |
|      33 | 3607 | `	base = iFlags & ~8;` |
|      33 | 3608 | `	bFold = (iFlags & 8) != 0;` |
|       - | 3609 | `	/* Point to the internal representation of the input hashmap */` |
|      33 | 3610 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3611 | `	/* Create a new array */` |
|      33 | 3612 | `	pArray = ph7_context_new_array(pCtx);` |
|      33 | 3613 | `	if( pArray == 0 ){` |
|     ! 0 | 3614 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3615 | `		return PH7_OK;` |
|       - | 3616 | `	}` |
|       - | 3617 | `	/* Perform the requested operation */` |
|      33 | 3618 | `	pEntry = pSrc->pFirst;` |
|     145 | 3619 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|     113 | 3620 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|     113 | 3621 | `		if( pNeedle ){` |
|       - | 3622 | `			/* Keep this element unless a flag-equal one is already present. */` |
|     113 | 3623 | `			ph7_hashmap *pKept = (ph7_hashmap *)pArray->x.pOther;` |
|     113 | 3624 | `			ph7_hashmap_node *pK = pKept->pFirst;` |
|     113 | 3625 | `			int bDup = 0;` |
|       - | 3626 | `			sxu32 i;` |
|       - | 3627 | `			/* Forward iteration in this map uses the pPrev link (see the outer` |
|       - | 3628 | `			 * loop over pSrc). */` |
|     177 | 3629 | `			for( i = 0 ; i < pKept->nEntry && pK ; ++i ){` |
|     117 | 3630 | `				ph7_value *pV = HashmapExtractNodeValue(pK);` |
|     117 | 3631 | `				if( pV && HashmapValueFlagEqual(pCtx->pVm,pNeedle,pV,base,bFold) ){` |
|      53 | 3632 | `					bDup = 1;` |
|      53 | 3633 | `					break;` |
|       - | 3634 | `				}` |
|      65 | 3635 | `				pK = pK->pPrev;` |
|      33 | 3636 | `			}` |
|     113 | 3637 | `			if( !bDup ){` |
|      61 | 3638 | `				HashmapInsertNode(pKept,pEntry,TRUE);` |
|      30 | 3639 | `			}` |
|      56 | 3640 | `		}` |
|       - | 3641 | `		/* Point to the next entry */` |
|     113 | 3642 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      57 | 3643 | `	}` |
|       - | 3644 | `	/* Return the freshly created array */` |
|      33 | 3645 | `	ph7_result_value(pCtx,pArray);` |
|      33 | 3646 | `	return PH7_OK;` |
|      19 | 3647 | `}` |
|       - | 3648 | `/*` |
|       - | 3649 | ` * array array_flip(array $input)` |
|       - | 3650 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 3651 | ` * Parameter` |
|       - | 3652 | ` *  $input` |
|       - | 3653 | ` *   Input array.` |
|       - | 3654 | ` * Return` |
|       - | 3655 | ` *   The flipped array on success or NULL on failure.` |
|       - | 3656 | ` */` |
|      28 | 3657 | `PH7_PRIVATE int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3658 | `{` |
|       - | 3659 | `	ph7_hashmap_node *pEntry;` |
|       - | 3660 | `	ph7_hashmap *pSrc;` |
|       - | 3661 | `	ph7_value *pArray;` |
|       - | 3662 | `	ph7_value *pKey;` |
|       - | 3663 | `	ph7_value sVal;` |
|       - | 3664 | `	sxu32 n;` |
|       - | 3665 |  |
|       - | 3666 | `	/* PHP requires exactly one argument */` |
|      30 | 3667 | `	if( nArg != 1 ){` |
|       - | 3668 | `		/* Use ArgumentCountError like other array helpers */` |
|     ! 0 | 3669 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3670 | `			"ArgumentCountError",` |
|       - | 3671 | `			"array_flip() expects exactly 1 argument, %d given",` |
|     ! 0 | 3672 | `			nArg` |
|       - | 3673 | `			);` |
|       - | 3674 | `	}` |
|       - | 3675 | `	/* Make sure we are dealing with a valid hashmap */` |
|      30 | 3676 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3677 | `		/* Type mismatch -> TypeError */` |
|       4 | 3678 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3679 | `			"TypeError",` |
|       - | 3680 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3681 | `			ph7_type_name(apArg[0])` |
|       - | 3682 | `			);` |
|       - | 3683 | `	}` |
|       - | 3684 | `	/* Point to the internal representation of the input hashmap */` |
|      27 | 3685 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3686 | `	/* Create a new array */` |
|      27 | 3687 | `	pArray = ph7_context_new_array(pCtx);` |
|      27 | 3688 | `	if( pArray == 0 ){` |
|     ! 0 | 3689 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3690 | `		return PH7_OK;` |
|       - | 3691 | `	}` |
|       - | 3692 | `	/* Start processing */` |
|      27 | 3693 | `	pEntry = pSrc->pFirst;` |
|   22263 | 3694 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 3695 | `		/* Extract the node value (will become a key in the result) */` |
|   22237 | 3696 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22237 | 3697 | `		if( pKey ){` |
|       - | 3698 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22237 | 3699 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 3700 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3701 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 3702 | `					);` |
|   22236 | 3703 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 3704 | `				/* Prepare the value for insertion (original key) */` |
|   22227 | 3705 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20001 | 3706 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10001 | 3707 | `				}else{` |
|       - | 3708 | `					SyString sStr;` |
|    2227 | 3709 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2227 | 3710 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 3711 | `				}` |
|       - | 3712 | `				/* Perform the insertion */` |
|   22227 | 3713 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 3714 | `				/* Safely release the value because each inserted entry` |
|       - | 3715 | `				 * has its own private copy of the value.` |
|       - | 3716 | `				 */` |
|   22227 | 3717 | `				PH7_MemObjRelease(&sVal);` |
|   11114 | 3718 | `			}else{` |
|       - | 3719 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|       9 | 3720 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3721 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 3722 | `					);` |
|       - | 3723 | `			}` |
|   11118 | 3724 | `		}` |
|       - | 3725 | `		/* Point to the next entry */` |
|   22237 | 3726 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11119 | 3727 | `	}` |
|       - | 3728 | `	/* Return the freshly created array */` |
|      27 | 3729 | `	ph7_result_value(pCtx,pArray);` |
|      27 | 3730 | `	return PH7_OK;` |
|      16 | 3731 | `}` |
|       - | 3732 | `/*` |
|       - | 3733 | ` * number array_sum(array $array )` |
|       - | 3734 | ` *  Calculate the sum of values in an array.` |
|       - | 3735 | ` * Parameters` |
|       - | 3736 | ` *  $array: The input array.` |
|       - | 3737 | ` * Return` |
|       - | 3738 | ` *  Returns the sum of values as an integer or float.` |
|       - | 3739 | ` */` |
|      24 | 3740 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 3741 | `{` |
|       - | 3742 | `	ph7_hashmap_node *pEntry;` |
|       - | 3743 | `	ph7_value *pObj;` |
|      26 | 3744 | `	double dSum = 0;` |
|       - | 3745 | `	sxu32 n;` |
|      26 | 3746 | `	pEntry = pMap->pFirst;` |
|      92 | 3747 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      68 | 3748 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      68 | 3749 | `		if( pObj ){` |
|      68 | 3750 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      30 | 3751 | `				dSum += pObj->rVal;` |
|      54 | 3752 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      21 | 3753 | `				dSum += (double)pObj->x.iVal;` |
|      30 | 3754 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      16 | 3755 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|       - | 3756 | `					/* php warns and SKIPS a non-numeric string (the array/object/` |
|       - | 3757 | `					 * resource cases below already did; only this one was silent) */` |
|       3 | 3758 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - | 3759 | `						"Addition is not supported on type string");` |
|      14 | 3760 | `				}else if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      13 | 3761 | `					double dv = 0;` |
|      13 | 3762 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|      13 | 3763 | `					dSum += dv;` |
|       8 | 3764 | `				}` |
|      12 | 3765 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 3766 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3767 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 3768 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       - | 3769 | `				/* php names the CLASS here, not the literal word "object" */` |
|     ! 0 | 3770 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|     ! 0 | 3771 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - | 3772 | `					"Addition is not supported on type %s",` |
|     ! 0 | 3773 | `					pInst && pInst->pClass ? pInst->pClass->sName.zString : "object");` |
|       3 | 3774 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 3775 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3776 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 3777 | `			}` |
|       - | 3778 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      33 | 3779 | `		}` |
|       - | 3780 | `		/* Point to the next entry */` |
|      68 | 3781 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      35 | 3782 | `	}` |
|       - | 3783 | `	/* Return sum */` |
|      26 | 3784 | `	ph7_result_double(pCtx,dSum);` |
|      26 | 3785 | `}` |
|     690 | 3786 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       3 | 3787 | `{` |
|       - | 3788 | `	ph7_hashmap_node *pEntry;` |
|       - | 3789 | `	ph7_value *pObj;` |
|     693 | 3790 | `	sxi64 nSum = 0;` |
|       - | 3791 | `	sxu32 n;` |
|     693 | 3792 | `	pEntry = pMap->pFirst;` |
|    6705 | 3793 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|    6015 | 3794 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|    6015 | 3795 | `		if( pObj ){` |
|    6015 | 3796 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|    5995 | 3797 | `				nSum += pObj->x.iVal;` |
|    3018 | 3798 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      12 | 3799 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|       - | 3800 | `					/* php warns and SKIPS a non-numeric string */` |
|       5 | 3801 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - | 3802 | `						"Addition is not supported on type string");` |
|      10 | 3803 | `				}else if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       8 | 3804 | `					sxi64 nv = 0;` |
|       8 | 3805 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       8 | 3806 | `					nSum += nv;` |
|       5 | 3807 | `				}` |
|      17 | 3808 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       6 | 3809 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3810 | `					"array_sum(): Addition is not supported on type array");` |
|      10 | 3811 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       - | 3812 | `				/* php names the CLASS here, not the literal word "object" */` |
|       3 | 3813 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       5 | 3814 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - | 3815 | `					"Addition is not supported on type %s",` |
|       2 | 3816 | `					pInst && pInst->pClass ? pInst->pClass->sName.zString : "object");` |
|       7 | 3817 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 3818 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3819 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 3820 | `			}` |
|       - | 3821 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|    3006 | 3822 | `		}` |
|       - | 3823 | `		/* Point to the next entry */` |
|    6015 | 3824 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    3009 | 3825 | `	}` |
|       - | 3826 | `	/* Return sum */` |
|     693 | 3827 | `	ph7_result_int64(pCtx,nSum);` |
|     693 | 3828 | `}` |
|       - | 3829 | `/* number array_sum(array $array )` |
|       - | 3830 | ` * (See block-coment above)` |
|       - | 3831 | ` */` |
|     724 | 3832 | `PH7_PRIVATE int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 3833 | `{` |
|       - | 3834 | `	ph7_hashmap_node *pEntry;` |
|       - | 3835 | `	ph7_hashmap *pMap;` |
|       - | 3836 | `	ph7_value *pObj;` |
|     728 | 3837 | `	int useDouble = 0;` |
|       - | 3838 | `	sxu32 n;` |
|       - | 3839 | `	/* PHP requires exactly one argument */` |
|     728 | 3840 | `	if( nArg != 1 ){` |
|     ! 0 | 3841 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3842 | `			"ArgumentCountError",` |
|       - | 3843 | `			"array_sum() expects exactly 1 argument, %d given",` |
|     ! 0 | 3844 | `			nArg` |
|       - | 3845 | `			);` |
|       - | 3846 | `	}` |
|       - | 3847 | `	/* Make sure we are dealing with a valid hashmap */` |
|     728 | 3848 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3849 | `		/* Type mismatch -> TypeError (php's true/false/class-name convention). */` |
|       - | 3850 | `		char zBuf[64];` |
|       8 | 3851 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3852 | `			"TypeError",` |
|       - | 3853 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 3854 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 3855 | `			);` |
|       - | 3856 | `	}` |
|     723 | 3857 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     723 | 3858 | `	if( pMap->nEntry < 1 ){` |
|       - | 3859 | `		/* Nothing to compute,return 0 */` |
|       7 | 3860 | `		ph7_result_int(pCtx,0);` |
|       7 | 3861 | `		return PH7_OK;` |
|       - | 3862 | `	}` |
|       - | 3863 | `	/* Scan all elements: if any value is a float, use floating-point` |
|       - | 3864 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|       - | 3865 | `	 */` |
|     717 | 3866 | `	pEntry = pMap->pFirst;` |
|    6737 | 3867 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|    6047 | 3868 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|    6047 | 3869 | `		if( pObj ){` |
|    6047 | 3870 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      20 | 3871 | `				useDouble = 1;` |
|      20 | 3872 | `				break;` |
|       - | 3873 | `			}` |
|    6029 | 3874 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|      18 | 3875 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|      18 | 3876 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|       - | 3877 | `				sxu32 i;` |
|      32 | 3878 | `				for( i = 0 ; i < nLen ; i++ ){` |
|      22 | 3879 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|       7 | 3880 | `						useDouble = 1;` |
|       7 | 3881 | `						break;` |
|       - | 3882 | `					}` |
|       9 | 3883 | `				}` |
|      18 | 3884 | `				if( useDouble ){` |
|       7 | 3885 | `					break;` |
|       - | 3886 | `				}` |
|       5 | 3887 | `			}` |
|    3010 | 3888 | `		}` |
|    6023 | 3889 | `		pEntry = pEntry->pPrev;` |
|    3013 | 3890 | `	}` |
|     717 | 3891 | `	if( useDouble ){` |
|      26 | 3892 | `		DoubleSum(pCtx,pMap);` |
|      14 | 3893 | `	}else{` |
|     693 | 3894 | `		Int64Sum(pCtx,pMap);` |
|       - | 3895 | `	}` |
|     717 | 3896 | `	return PH7_OK;` |
|     366 | 3897 | `}` |
|       - | 3898 | `/*` |
|       - | 3899 | ` * number array_product(array $array )` |
|       - | 3900 | ` *  Calculate the product of values in an array.` |
|       - | 3901 | ` * Parameters` |
|       - | 3902 | ` *  $array: The input array.` |
|       - | 3903 | ` * Return` |
|       - | 3904 | ` *  Returns the product of values as an integer or float.` |
|       - | 3905 | ` */` |
|       2 | 3906 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 3907 | `{` |
|       - | 3908 | `	ph7_hashmap_node *pEntry;` |
|       - | 3909 | `	ph7_value *pObj;` |
|       - | 3910 | `	double dProd;` |
|       - | 3911 | `	sxu32 n;` |
|       3 | 3912 | `	pEntry = pMap->pFirst;` |
|       3 | 3913 | `	dProd = 1;` |
|       7 | 3914 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       5 | 3915 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|       5 | 3916 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|       5 | 3917 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|       3 | 3918 | `				dProd *= pObj->rVal;` |
|       4 | 3919 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|       3 | 3920 | `				dProd *= (double)pObj->x.iVal;` |
|       1 | 3921 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 3922 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 3923 | `					double dv = 0;` |
|     ! 0 | 3924 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 3925 | `					dProd *= dv;` |
|     ! 0 | 3926 | `				}` |
|     ! 0 | 3927 | `			}` |
|       2 | 3928 | `		}` |
|       - | 3929 | `		/* Point to the next entry */` |
|       5 | 3930 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       3 | 3931 | `	}` |
|       - | 3932 | `	/* Return product */` |
|       3 | 3933 | `	ph7_result_double(pCtx,dProd);` |
|       3 | 3934 | `}` |
|       2 | 3935 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 3936 | `{` |
|       - | 3937 | `	ph7_hashmap_node *pEntry;` |
|       - | 3938 | `	ph7_value *pObj;` |
|       - | 3939 | `	sxi64 nProd;` |
|       - | 3940 | `	sxu32 n;` |
|       3 | 3941 | `	pEntry = pMap->pFirst;` |
|       3 | 3942 | `	nProd = 1;` |
|       9 | 3943 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       7 | 3944 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|       7 | 3945 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|       7 | 3946 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 3947 | `				nProd *= (sxi64)pObj->rVal;` |
|       7 | 3948 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|       7 | 3949 | `				nProd *= pObj->x.iVal;` |
|       3 | 3950 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 3951 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 3952 | `					sxi64 nv = 0;` |
|     ! 0 | 3953 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 3954 | `					nProd *= nv;` |
|     ! 0 | 3955 | `				}` |
|     ! 0 | 3956 | `			}` |
|       3 | 3957 | `		}` |
|       - | 3958 | `		/* Point to the next entry */` |
|       7 | 3959 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       4 | 3960 | `	}` |
|       - | 3961 | `	/* Return product */` |
|       3 | 3962 | `	ph7_result_int64(pCtx,nProd);` |
|       3 | 3963 | `}` |
|       - | 3964 | `/* number array_product(array $array )` |
|       - | 3965 | ` * (See block-block comment above)` |
|       - | 3966 | ` */` |
|      14 | 3967 | `PH7_PRIVATE int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3968 | `{` |
|       - | 3969 | `	ph7_hashmap *pMap;` |
|       - | 3970 | `	ph7_value *pObj;` |
|      15 | 3971 | `	if( nArg < 1 ){` |
|       - | 3972 | `		/* Missing arguments (arity is enforced upstream; defensive). */` |
|     ! 0 | 3973 | `		ph7_result_int(pCtx,1);` |
|     ! 0 | 3974 | `		return PH7_OK;` |
|       - | 3975 | `	}` |
|       - | 3976 | `	/* PHP 8: a non-array $array is a catchable TypeError, not a silent 0. */` |
|      15 | 3977 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3978 | `		char zBuf[64];` |
|      13 | 3979 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3980 | `			"TypeError",` |
|       - | 3981 | `			"array_product(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 3982 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 3983 | `			);` |
|       - | 3984 | `	}` |
|       7 | 3985 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 3986 | `	if( pMap->nEntry < 1 ){` |
|       - | 3987 | `		/* The product of an empty array is the multiplicative identity 1 (PHP). */` |
|       3 | 3988 | `		ph7_result_int(pCtx,1);` |
|       3 | 3989 | `		return PH7_OK;` |
|       - | 3990 | `	}` |
|       - | 3991 | `	/* If the first element is of type float,then perform floating` |
|       - | 3992 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 3993 | `	 */` |
|       5 | 3994 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|       5 | 3995 | `	if( pObj == 0 ){` |
|     ! 0 | 3996 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 3997 | `		return PH7_OK;` |
|       - | 3998 | `	}` |
|       5 | 3999 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|       3 | 4000 | `		DoubleProd(pCtx,pMap);` |
|       2 | 4001 | `	}else{` |
|       3 | 4002 | `		Int64Prod(pCtx,pMap);` |
|       - | 4003 | `	}` |
|       5 | 4004 | `	return PH7_OK;` |
|       8 | 4005 | `}` |
|       - | 4006 | `/*` |
|       - | 4007 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 4008 | ` *  Pick one or more random entries out of an array.` |
|       - | 4009 | ` * Parameters` |
|       - | 4010 | ` * $input` |
|       - | 4011 | ` *  The input array.` |
|       - | 4012 | ` * $num_req` |
|       - | 4013 | ` *  Specifies how many entries you want to pick.` |
|       - | 4014 | ` * Return` |
|       - | 4015 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 4016 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 4017 | ` *  NULL is returned on failure.` |
|       - | 4018 | ` */` |
|      36 | 4019 | `PH7_PRIVATE int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4020 | `{` |
|       - | 4021 | `	ph7_hashmap_node *pNode;` |
|       - | 4022 | `	ph7_hashmap *pMap;` |
|      37 | 4023 | `	int nItem = 1;` |
|      37 | 4024 | `	if( nArg < 1 ){` |
|       - | 4025 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|     ! 0 | 4026 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4027 | `		return PH7_OK;` |
|       - | 4028 | `	}` |
|       - | 4029 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|      37 | 4030 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4031 | `		char zBuf[64];` |
|      10 | 4032 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4033 | `			"TypeError",` |
|       - | 4034 | `			"array_rand(): Argument #1 ($array) must be of type array, %s given",` |
|       3 | 4035 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4036 | `			);` |
|       - | 4037 | `	}` |
|       - | 4038 | `	/* php validates $num (and weak-coerces it) BEFORE the empty-array body` |
|       - | 4039 | `	 * check, matching its ZPP-before-body ordering. */` |
|      31 | 4040 | `	if( nArg > 1 ){` |
|      23 | 4041 | `		ph7_value *pNum = apArg[1];` |
|      22 | 4042 | `		if( ph7_value_is_array(pNum) \|\| ph7_value_is_object(pNum)` |
|      23 | 4043 | `			\|\| ph7_value_is_resource(pNum) ){` |
|       - | 4044 | `			char zBuf[64];` |
|     ! 0 | 4045 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4046 | `				"TypeError",` |
|       - | 4047 | `				"array_rand(): Argument #2 ($num) must be of type int, %s given",` |
|     ! 0 | 4048 | `				VmValueGivenName(pNum,zBuf,sizeof(zBuf))` |
|       - | 4049 | `				);` |
|       - | 4050 | `		}` |
|      23 | 4051 | `		if( ph7_value_is_string(pNum) ){` |
|       - | 4052 | `			/* Weak int coercion of a string $num follows php's numeric-string` |
|       - | 4053 | `			 * grammar (whole string, int or float): a non-numeric string` |
|       - | 4054 | `			 * (incl. leading-numeric junk like "2abc" or "0x1A") is a TypeError,` |
|       - | 4055 | `			 * a well-formed float-string ("1e3") coerces like a float value.` |
|       - | 4056 | `			 * Reuses the range() ZPP number parser (§3.9 shared-helper note). */` |
|       - | 4057 | `			int len;` |
|       9 | 4058 | `			const char *zStr = ph7_value_to_string(pNum, &len);` |
|       - | 4059 | `			sxi64 iLong; double dReal;` |
|       9 | 4060 | `			sxu8 iKind = RangeStrToNumber(zStr, (sxu32)len, &iLong, &dReal);` |
|       9 | 4061 | `			if( iKind == RANGE_IN_ERROR ){` |
|       7 | 4062 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4063 | `					"TypeError",` |
|       - | 4064 | `					"array_rand(): Argument #2 ($num) must be of type int, string given"` |
|       - | 4065 | `					);` |
|       - | 4066 | `			}` |
|       - | 4067 | `			/* Clamp into a signed-int band so an absurd magnitude still yields` |
|       - | 4068 | `			 * the out-of-range ValueError below without an out-of-int cast. */` |
|       3 | 4069 | `			if( iKind == RANGE_IN_DOUBLE ){` |
|       3 | 4070 | `				iLong = dReal <= 0.0 ? 0 : (dReal >= 2147483647.0 ? 2147483647 : (sxi64)dReal);` |
|       1 | 4071 | `			}` |
|       3 | 4072 | `			if( iLong > 2147483647 ){ iLong = 2147483647; }` |
|       3 | 4073 | `			else if( iLong < -2147483647 ){ iLong = -2147483647; }` |
|       3 | 4074 | `			nItem = (int)iLong;` |
|       2 | 4075 | `		}else{` |
|      15 | 4076 | `			nItem = ph7_value_to_int(pNum);` |
|       - | 4077 | `		}` |
|       8 | 4078 | `	}` |
|       - | 4079 | `	/* Point to the internal representation of the input hashmap */` |
|      25 | 4080 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4081 | `	/* php 8: an empty array is a ValueError, not a NULL return */` |
|      25 | 4082 | `	if( pMap->nEntry < 1 ){` |
|       5 | 4083 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4084 | `			"ValueError",` |
|       - | 4085 | `			"array_rand(): Argument #1 ($array) must not be empty"` |
|       - | 4086 | `			);` |
|       - | 4087 | `	}` |
|       - | 4088 | `	/* php 8: $num outside [1, count] is a ValueError, not a clamp/wrong value */` |
|      21 | 4089 | `	if( nItem < 1 \|\| nItem > (int)pMap->nEntry ){` |
|       9 | 4090 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4091 | `			"ValueError",` |
|       - | 4092 | `			"array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)"` |
|       - | 4093 | `			);` |
|       - | 4094 | `	}` |
|      13 | 4095 | `	if( nItem < 2 ){` |
|       - | 4096 | `		sxu32 nEntry;` |
|       - | 4097 | `		/* Select a random number */` |
|       9 | 4098 | `		nEntry = PH7_VmRandomNum(pMap->pVm) % pMap->nEntry;` |
|       - | 4099 | `		/* Extract the desired entry.` |
|       - | 4100 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 4101 | `		 */` |
|       9 | 4102 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       2 | 4103 | `			pNode = pMap->pLast;` |
|       2 | 4104 | `			nEntry = pMap->nEntry - nEntry;` |
|       2 | 4105 | `			if( nEntry > 1 ){` |
|     ! 0 | 4106 | `				for(;;){` |
|     ! 0 | 4107 | `					if( nEntry == 0 ){` |
|     ! 0 | 4108 | `						break;` |
|       - | 4109 | `					}` |
|       - | 4110 | `					/* Point to the previous entry */` |
|     ! 0 | 4111 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 4112 | `					nEntry--;` |
|     ! 0 | 4113 | `				}` |
|     ! 0 | 4114 | `			}` |
|       1 | 4115 | `		}else{` |
|       8 | 4116 | `			pNode = pMap->pFirst;` |
|       7 | 4117 | `			for(;;){` |
|      11 | 4118 | `				if( nEntry == 0 ){` |
|       8 | 4119 | `					break;` |
|       - | 4120 | `				}` |
|       - | 4121 | `				/* Point to the next entry */` |
|       4 | 4122 | `				pNode = pNode->pPrev; /* Reverse link */` |
|       4 | 4123 | `				nEntry--;` |
|       1 | 4124 | `			}` |
|       - | 4125 | `		}` |
|       9 | 4126 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 4127 | `			/* Int key */` |
|       7 | 4128 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       4 | 4129 | `		}else{` |
|       - | 4130 | `			/* Blob key */` |
|       3 | 4131 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 4132 | `		}` |
|       5 | 4133 | `	}else{` |
|       - | 4134 | `		ph7_value sKey,*pArray;` |
|       - | 4135 | `		ph7_hashmap *pDest;` |
|       - | 4136 | `		/* Create a new array */` |
|       5 | 4137 | `		pArray = ph7_context_new_array(pCtx);` |
|       5 | 4138 | `		if( pArray == 0 ){` |
|     ! 0 | 4139 | `			ph7_result_null(pCtx);` |
|     ! 0 | 4140 | `			return PH7_OK;` |
|       - | 4141 | `		}` |
|       - | 4142 | `		/* Point to the internal representation of the hashmap */` |
|       5 | 4143 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       5 | 4144 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 4145 | `		/* Copy the first n items */` |
|       5 | 4146 | `		pNode = pMap->pFirst;` |
|       5 | 4147 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 4148 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 4149 | `		}` |
|      15 | 4150 | `		while( nItem > 0){` |
|      11 | 4151 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|      11 | 4152 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|      11 | 4153 | `			PH7_MemObjRelease(&sKey);` |
|       - | 4154 | `			/* Point to the next entry */` |
|      11 | 4155 | `			pNode = pNode->pPrev; /* Reverse link */` |
|      11 | 4156 | `			nItem--;` |
|       1 | 4157 | `		}` |
|       - | 4158 | `		/* Shuffle the array */` |
|       5 | 4159 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 4160 | `		/* Rehash node */` |
|       5 | 4161 | `		HashmapSortRehash(pDest);` |
|       - | 4162 | `		/* Return the random array */` |
|       5 | 4163 | `		ph7_result_value(pCtx,pArray);` |
|       - | 4164 | `	}` |
|      13 | 4165 | `	return PH7_OK;` |
|      19 | 4166 | `}` |
|       - | 4167 | `/*` |
|       - | 4168 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 4169 | ` *  Split an array into chunks.` |
|       - | 4170 | ` * Parameters` |
|       - | 4171 | ` * $input` |
|       - | 4172 | ` *   The array to work on` |
|       - | 4173 | ` * $size` |
|       - | 4174 | ` *   The size of each chunk` |
|       - | 4175 | ` * $preserve_keys` |
|       - | 4176 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 4177 | ` *   the chunk numerically.` |
|       - | 4178 | ` * Return` |
|       - | 4179 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 4180 | ` *  zero, with each dimension containing size elements.` |
|       - | 4181 | ` */` |
|      36 | 4182 | `PH7_PRIVATE int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4183 | `{` |
|       - | 4184 | `	ph7_value *pArray,*pChunk;` |
|       - | 4185 | `	ph7_hashmap_node *pEntry;` |
|       - | 4186 | `	ph7_hashmap *pMap;` |
|       - | 4187 | `	int bPreserve;` |
|       - | 4188 | `	sxu32 nChunk;` |
|       - | 4189 | `	sxu32 nSize;` |
|       - | 4190 | `	sxu32 n;` |
|       - | 4191 | `	/* Argument count and types follow PHP semantics. */` |
|      41 | 4192 | `	if( nArg < 2 ){` |
|       - | 4193 | `		/* fewer than required arguments -> ArgumentCountError */` |
|     ! 0 | 4194 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4195 | `			"ArgumentCountError",` |
|       - | 4196 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|     ! 0 | 4197 | `			nArg` |
|       - | 4198 | `			);` |
|       - | 4199 | `	}` |
|      41 | 4200 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4201 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4202 | `			"TypeError",` |
|       - | 4203 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4204 | `			ph7_type_name(apArg[0])` |
|       - | 4205 | `			);` |
|       - | 4206 | `	}` |
|       - | 4207 | `	/* Create a new array */` |
|      38 | 4208 | `	pArray = ph7_context_new_array(pCtx);` |
|      38 | 4209 | `	if( pArray == 0 ){` |
|     ! 0 | 4210 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4211 | `		return PH7_OK;` |
|       - | 4212 | `	}` |
|       - | 4213 | `	/* Point to the internal representation of the input hashmap */` |
|      38 | 4214 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4215 | `	/* Extract and validate the chunk size argument. */` |
|       - | 4216 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      51 | 4217 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      72 | 4218 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      34 | 4219 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 4220 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4221 | `			"TypeError",` |
|       - | 4222 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 4223 | `			ph7_type_name(apArg[1])` |
|       - | 4224 | `			);` |
|       - | 4225 | `	}` |
|       - | 4226 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 4227 | `	 * strings are permitted; however those representing floats lose` |
|       - | 4228 | `	 * precision and PHP emits a deprecation warning. */` |
|      38 | 4229 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 4230 | `		int len;` |
|       3 | 4231 | `		sxu8 bReal = FALSE;` |
|       3 | 4232 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 4233 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 4234 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4235 | `				"TypeError",` |
|       - | 4236 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 4237 | `				);` |
|       - | 4238 | `		}` |
|     ! 0 | 4239 | `		if( bReal ){` |
|       - | 4240 | `			/* float-string -> warn but allow */` |
|     ! 0 | 4241 | `			PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 4242 | `				"Implicit conversion from float-string to int loses precision");` |
|     ! 0 | 4243 | `		}` |
|     ! 0 | 4244 | `	}` |
|       - | 4245 | `	/* If the value is a float with a fractional component, emit a` |
|       - | 4246 | `	 * deprecation warning but continue.  The following conversion occurs` |
|       - | 4247 | `	 * later via ph7_value_to_int. */` |
|      35 | 4248 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 4249 | `		double d = ph7_value_to_double(apArg[1]);` |
|       3 | 4250 | `		sxi64 i = (sxi64)d;` |
|       3 | 4251 | `		if( d != (double)i ){` |
|       3 | 4252 | `			PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 4253 | `				"Implicit conversion from float to int loses precision");` |
|       1 | 4254 | `		}` |
|       1 | 4255 | `	}` |
|       - | 4256 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 4257 | `	 * eliminated, this will not produce a warning. */` |
|       - | 4258 | `	{` |
|      35 | 4259 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      35 | 4260 | `		if( nSizeSigned < 1 ){` |
|       - | 4261 | `			/* size <= 0 -> ValueError */` |
|       6 | 4262 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4263 | `				"ValueError",` |
|       - | 4264 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 4265 | `				);` |
|       - | 4266 | `		}` |
|      30 | 4267 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 4268 | `	}` |
|      30 | 4269 | `	if( nSize >= pMap->nEntry ){` |
|       - | 4270 | `		/* Return the whole array */` |
|       3 | 4271 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 4272 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 4273 | `		return PH7_OK;` |
|       - | 4274 | `	}` |
|      28 | 4275 | `	bPreserve = 0;` |
|      28 | 4276 | `	if( nArg > 2 ){` |
|       - | 4277 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 4278 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 4279 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 4280 | `		 * normally, matching PHP behaviour. */` |
|      30 | 4281 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      31 | 4282 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 4283 | `			ph7_value_is_resource(apArg[2]) ){` |
|     ! 0 | 4284 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4285 | `				"TypeError",` |
|       - | 4286 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|     ! 0 | 4287 | `				ph7_type_name(apArg[2])` |
|       - | 4288 | `				);` |
|       - | 4289 | `		}` |
|      21 | 4290 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 4291 | `	}` |
|       - | 4292 | `	/* Start processing */` |
|      28 | 4293 | `	pEntry = pMap->pFirst;` |
|      28 | 4294 | `	nChunk = 0;` |
|      28 | 4295 | `	pChunk = 0;` |
|      28 | 4296 | `	n = pMap->nEntry;` |
|      54 | 4297 | `	for( ;; ){` |
|     110 | 4298 | `		if( n < 1 ){` |
|       - | 4299 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 4300 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 4301 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 4302 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 4303 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 4304 | `			 * exists. */` |
|      28 | 4305 | `			if( pChunk ){` |
|      28 | 4306 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      13 | 4307 | `			}` |
|      28 | 4308 | `			break;` |
|       - | 4309 | `		}` |
|      84 | 4310 | `		if( nChunk < 1 ){` |
|      72 | 4311 | `			if( pChunk ){` |
|       - | 4312 | `				/* Put the first chunk */` |
|      46 | 4313 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      22 | 4314 | `			}` |
|       - | 4315 | `			/* Create a new dimension */` |
|      72 | 4316 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 4317 | `												   * will be automatically released as soon we return` |
|       - | 4318 | `												   * from this function */` |
|      72 | 4319 | `			if( pChunk == 0 ){` |
|     ! 0 | 4320 | `				break;` |
|       - | 4321 | `			}` |
|      72 | 4322 | `			nChunk = nSize;` |
|      35 | 4323 | `		}` |
|       - | 4324 | `		/* Insert the entry */` |
|      84 | 4325 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 4326 | `		/* Point to the next entry */` |
|      84 | 4327 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      84 | 4328 | `		nChunk--;` |
|      84 | 4329 | `		n--;` |
|       2 | 4330 | `	}` |
|       - | 4331 | `	/* Return the multidimensional array */` |
|      28 | 4332 | `	ph7_result_value(pCtx,pArray);` |
|      28 | 4333 | `	return PH7_OK;` |
|      23 | 4334 | `}` |
|       - | 4335 | `/*` |
|       - | 4336 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 4337 | ` *  Pad array to the specified length with a value.` |
|       - | 4338 | ` * $input` |
|       - | 4339 | ` *   Initial array of values to pad.` |
|       - | 4340 | ` * $pad_size` |
|       - | 4341 | ` *   New size of the array.` |
|       - | 4342 | ` * $pad_value` |
|       - | 4343 | ` *   Value to pad if input is less than pad_size.` |
|       - | 4344 | ` */` |
|       - | 4345 | `/*` |
|       - | 4346 | ` * Shared "requested array size too large" guard (band A #8). php throws a` |
|       - | 4347 | ` * catchable ValueError when a builtin's caller-controlled target length` |
|       - | 4348 | ` * exceeds its hashtable capacity HT_MAX_SIZE (2^30 elements; probed against` |
|       - | 4349 | ` * php 8.5.7 — the boundary sits exactly between 1073741824 and 1073741825,` |
|       - | 4350 | ` * independent of the input array's size and symmetric for negative lengths).` |
|       - | 4351 | ` * Without this, a call like array_pad([1,2], 2000000000, 0) sits in the fill` |
|       - | 4352 | ` * loop for minutes and then OOMs. nRequested is the ABSOLUTE requested` |
|       - | 4353 | ` * length; pass a still-negative value (e.g. the unnegatable INT64_MIN,` |
|       - | 4354 | ` * mirroring php's ZEND_ABS overflow) to fail the guard unconditionally.` |
|       - | 4355 | ` * Returns SXRET_OK when the size is acceptable, else the throw status to` |
|       - | 4356 | ` * propagate. The cap constant is shared with range()'s guards` |
|       - | 4357 | ` * (PH7_RANGE_HT_MAX_SIZE above).` |
|       - | 4358 | ` */` |
|      50 | 4359 | `static sxi32 HashmapGuardArraySize(` |
|       - | 4360 | `	ph7_context *pCtx,` |
|       - | 4361 | `	const char *zFunc,     /* Function name for the message */` |
|       - | 4362 | `	int iArg,              /* 1-based argument position */` |
|       - | 4363 | `	const char *zParam     /* "$length"-style parameter name */,` |
|       - | 4364 | `	sxi64 nRequested       /* Absolute requested element count */` |
|       - | 4365 | `	)` |
|       1 | 4366 | `{` |
|      51 | 4367 | `	if( nRequested < 0 \|\| nRequested > PH7_RANGE_HT_MAX_SIZE ){` |
|      22 | 4368 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4369 | `			"ValueError",` |
|       - | 4370 | `			"%s(): Argument #%d (%s) must not exceed the maximum allowed array size",` |
|       7 | 4371 | `			zFunc,iArg,zParam` |
|       - | 4372 | `			);` |
|       - | 4373 | `	}` |
|      37 | 4374 | `	return SXRET_OK;` |
|      26 | 4375 | `}` |
|      60 | 4376 | `PH7_PRIVATE int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4377 | `{` |
|       - | 4378 | `	ph7_hashmap *pMap;` |
|       - | 4379 | `	ph7_value *pArray;` |
|       - | 4380 | `	sxi64 iLen,iAbs;` |
|       - | 4381 | `	int nEntry;` |
|       - | 4382 | `	sxi32 rc;` |
|      62 | 4383 | `	if( nArg != 3 ){` |
|     ! 0 | 4384 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4385 | `			"ArgumentCountError",` |
|       - | 4386 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|     ! 0 | 4387 | `			nArg` |
|       - | 4388 | `			);` |
|       - | 4389 | `	}` |
|      62 | 4390 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4391 | `		char zBuf[64];` |
|      11 | 4392 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4393 | `			"TypeError",` |
|       - | 4394 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|       3 | 4395 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4396 | `			);` |
|       - | 4397 | `	}` |
|       - | 4398 | `	/* php 8: $length must be int-coercible. An array/object/resource or a` |
|       - | 4399 | `	 * non-numeric string throws a TypeError instead of silently padding to 0;` |
|       - | 4400 | `	 * a numeric string is weak-coerced via php's is_numeric_string grammar` |
|       - | 4401 | `	 * (reusing the shared RangeStrToNumber, like array_rand's $num). */` |
|      54 | 4402 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|      55 | 4403 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|       - | 4404 | `		char zBuf[64];` |
|     ! 0 | 4405 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4406 | `			"TypeError",` |
|       - | 4407 | `			"array_pad(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 4408 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|       - | 4409 | `			);` |
|       - | 4410 | `	}` |
|      55 | 4411 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 4412 | `		int nStr;` |
|      11 | 4413 | `		const char *zStr = ph7_value_to_string(apArg[1],&nStr);` |
|       - | 4414 | `		sxi64 iLong; double dReal;` |
|      11 | 4415 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|      11 | 4416 | `		if( iKind == RANGE_IN_ERROR ){` |
|       5 | 4417 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4418 | `				"TypeError",` |
|       - | 4419 | `				"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|       - | 4420 | `				);` |
|       - | 4421 | `		}` |
|       7 | 4422 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|       - | 4423 | `			/* php ZPP: a float-string outside the int64 range (or NaN) fails` |
|       - | 4424 | `			 * outright — also keeps the (sxi64) cast below UB-free. */` |
|       3 | 4425 | `			if( dReal != dReal \|\| dReal >= 9223372036854775808.0 \|\| dReal < -9223372036854775808.0 ){` |
|     ! 0 | 4426 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4427 | `					"TypeError",` |
|       - | 4428 | `					"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|       - | 4429 | `					);` |
|       - | 4430 | `			}` |
|       3 | 4431 | `			iLen = (sxi64)dReal;` |
|       3 | 4432 | `			if( (double)iLen != dReal ){` |
|     ! 0 | 4433 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 4434 | `					"array_pad(): Argument #2 ($length) must be of type int, string given");` |
|       - | 4435 | `			}` |
|       2 | 4436 | `		}else{` |
|       5 | 4437 | `			iLen = iLong;` |
|       - | 4438 | `		}` |
|       4 | 4439 | `	}else{` |
|      45 | 4440 | `		iLen = ph7_value_to_int64(apArg[1]);` |
|       - | 4441 | `	}` |
|       - | 4442 | `	/* Point to the internal representation of the input hashmap */` |
|      51 | 4443 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4444 | `	/* php caps abs($length) at HT_MAX_SIZE either direction (INT64_MIN stays` |
|       - | 4445 | `	 * negative through the ABS, failing the guard like php's own ZEND_ABS` |
|       - | 4446 | `	 * overflow). */` |
|      51 | 4447 | `	iAbs = iLen;` |
|      51 | 4448 | `	if( iAbs < 0 && iAbs != (sxi64)-9223372036854775807LL - 1 ){` |
|      15 | 4449 | `		iAbs = -iAbs;` |
|       7 | 4450 | `	}` |
|      51 | 4451 | `	rc = HashmapGuardArraySize(pCtx,"array_pad",2,"$length",iAbs);` |
|      51 | 4452 | `	if( rc != SXRET_OK ){` |
|      15 | 4453 | `		return rc;` |
|       - | 4454 | `	}` |
|      37 | 4455 | `	nEntry = (int)iLen;` |
|       - | 4456 | `	/* Create a new array */` |
|      37 | 4457 | `	pArray = ph7_context_new_array(pCtx);` |
|      37 | 4458 | `	if( pArray == 0 ){` |
|     ! 0 | 4459 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 4460 | `	}` |
|      37 | 4461 | `	if( nEntry < 0 ){` |
|      11 | 4462 | `		nEntry = -nEntry;` |
|      11 | 4463 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 4464 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 4465 | `			/* Insert given items first */` |
|      25 | 4466 | `			while( nEntry > 0 ){` |
|      19 | 4467 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 4468 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 4469 | `				}` |
|      19 | 4470 | `				nEntry--;` |
|       1 | 4471 | `			}` |
|       - | 4472 | `			/* Merge the two arrays */` |
|       7 | 4473 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       4 | 4474 | `		}else{` |
|       5 | 4475 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 4476 | `		}` |
|      32 | 4477 | `	}else if( nEntry > 0 ){` |
|      25 | 4478 | `		if( nEntry > (int)pMap->nEntry ){` |
|      19 | 4479 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 4480 | `			/* Merge the two arrays first */` |
|      19 | 4481 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 4482 | `			/* Insert given items */` |
|     275 | 4483 | `			while( nEntry > 0 ){` |
|     257 | 4484 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 4485 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 4486 | `				}` |
|     257 | 4487 | `				nEntry--;` |
|       1 | 4488 | `			}` |
|      10 | 4489 | `		}else{` |
|       7 | 4490 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 4491 | `		}` |
|      13 | 4492 | `	}else{` |
|       - | 4493 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 4494 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 4495 | `	}` |
|       - | 4496 | `	/* Return the new array */` |
|      37 | 4497 | `	ph7_result_value(pCtx,pArray);` |
|      37 | 4498 | `	return PH7_OK;` |
|      32 | 4499 | `}` |
|       - | 4500 | `/*` |
|       - | 4501 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 4502 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 4503 | ` * Parameters` |
|       - | 4504 | ` * $array` |
|       - | 4505 | ` *   The array in which elements are replaced.` |
|       - | 4506 | ` * $array1` |
|       - | 4507 | ` *   The array from which elements will be extracted.` |
|       - | 4508 | ` * ....` |
|       - | 4509 | ` *  More arrays from which elements will be extracted.` |
|       - | 4510 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 4511 | ` * Return` |
|       - | 4512 | ` *  Returns an array.` |
|       - | 4513 | ` *  Throws ArgumentCountError if no arguments are given.` |
|       - | 4514 | ` *  Throws TypeError if any argument is not an array.` |
|       - | 4515 | ` */` |
|      20 | 4516 | `PH7_PRIVATE int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 4517 | `{` |
|       - | 4518 | `	ph7_hashmap *pMap;` |
|       - | 4519 | `	ph7_value *pArray;` |
|       - | 4520 | `	int i;` |
|      23 | 4521 | `	if( nArg < 1 ){` |
|     ! 0 | 4522 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4523 | `			"ArgumentCountError",` |
|       - | 4524 | `			"array_replace() expects at least 1 argument, 0 given"` |
|       - | 4525 | `			);` |
|       - | 4526 | `	}` |
|      23 | 4527 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4528 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4529 | `			"TypeError",` |
|       - | 4530 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4531 | `			ph7_type_name(apArg[0])` |
|       - | 4532 | `			);` |
|       - | 4533 | `	}` |
|       - | 4534 | `	/* Create a new array */` |
|      20 | 4535 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 4536 | `	if( pArray == 0 ){` |
|     ! 0 | 4537 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4538 | `		return PH7_OK;` |
|       - | 4539 | `	}` |
|       - | 4540 | `	/* Overwrite from the first array */` |
|      20 | 4541 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 4542 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 4543 | `	/* Perform the requested operation for remaining arrays */` |
|      36 | 4544 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4545 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 4546 | `			/* Type mismatch -> TypeError */` |
|       4 | 4547 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4548 | `				"TypeError",` |
|       - | 4549 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|       1 | 4550 | `				i + 1,` |
|       2 | 4551 | `				ph7_type_name(apArg[i])` |
|       - | 4552 | `				);` |
|       - | 4553 | `		}` |
|       - | 4554 | `		/* Point to the internal representation of the input hashmap */` |
|      17 | 4555 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      17 | 4556 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       9 | 4557 | `	}` |
|       - | 4558 | `	/* Return the new array */` |
|      17 | 4559 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 4560 | `	return PH7_OK;` |
|      13 | 4561 | `}` |
|       - | 4562 | `/*` |
|       - | 4563 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 4564 | ` *  Filters elements of an array using a callback function.` |
|       - | 4565 | ` * Parameters` |
|       - | 4566 | ` *  $input` |
|       - | 4567 | ` *    The array to iterate over` |
|       - | 4568 | ` * $callback` |
|       - | 4569 | ` *    The callback function to use` |
|       - | 4570 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 4571 | ` *    will be removed.` |
|       - | 4572 | ` * Return` |
|       - | 4573 | ` *  The filtered array.` |
|       - | 4574 | ` */` |
|      28 | 4575 | `PH7_PRIVATE int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4576 | `{` |
|       - | 4577 | `	ph7_hashmap_node *pEntry;` |
|       - | 4578 | `	ph7_hashmap *pMap;` |
|       - | 4579 | `	ph7_value *pArray;` |
|       - | 4580 | `	ph7_value sResult;   /* Callback result */` |
|       - | 4581 | `	ph7_value *pValue;` |
|       - | 4582 | `	sxi32 rc;` |
|       - | 4583 | `	int keep;` |
|       - | 4584 | `	sxu32 n;` |
|      30 | 4585 | `	if( nArg < 1 ){` |
|       - | 4586 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|     ! 0 | 4587 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4588 | `		return PH7_OK;` |
|       - | 4589 | `	}` |
|       - | 4590 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|      30 | 4591 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4592 | `		char zBuf[64];` |
|      16 | 4593 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4594 | `			"TypeError",` |
|       - | 4595 | `			"array_filter(): Argument #1 ($array) must be of type array, %s given",` |
|       5 | 4596 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4597 | `			);` |
|       - | 4598 | `	}` |
|       - | 4599 | `	/* Create a new array */` |
|      20 | 4600 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 4601 | `	if( pArray == 0 ){` |
|     ! 0 | 4602 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4603 | `		return PH7_OK;` |
|       - | 4604 | `	}` |
|       - | 4605 | `	/* Point to the internal representation of the input hashmap */` |
|      20 | 4606 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 4607 | `	pEntry = pMap->pFirst;` |
|      20 | 4608 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      20 | 4609 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 4610 | `	/* Perform the requested operation */` |
|      78 | 4611 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 4612 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      64 | 4613 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      64 | 4614 | `		if( pValue == 0 ){` |
|       - | 4615 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 4616 | `			keep = FALSE;` |
|      64 | 4617 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 4618 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|       - | 4619 | `				* TypeError when the value is not callable or null; prior PH7` |
|       - | 4620 | `				* silently dropped the element.  Emit similar message. */` |
|      36 | 4621 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|       3 | 4622 | `				if( ph7_value_is_string(apArg[1]) ){` |
|       - | 4623 | `					int len;` |
|       3 | 4624 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|       4 | 4625 | `					return PH7_VmThrowException(pCtx,` |
|       - | 4626 | `						"TypeError",` |
|       - | 4627 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|       1 | 4628 | `						zName` |
|       - | 4629 | `						);` |
|     ! 0 | 4630 | `				}else{` |
|     ! 0 | 4631 | `					return PH7_VmThrowException(pCtx,` |
|       - | 4632 | `						"TypeError",` |
|       - | 4633 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|     ! 0 | 4634 | `						ph7_type_name(apArg[1])` |
|       - | 4635 | `						);` |
|       - | 4636 | `				}` |
|       - | 4637 | `			}` |
|      33 | 4638 | `			keep = FALSE;` |
|      33 | 4639 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      33 | 4640 | `			if( rc == PH7_EXCEPTION ){` |
|       - | 4641 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 4642 | `				PH7_MemObjRelease(&sResult);` |
|       3 | 4643 | `				return PH7_EXCEPTION;` |
|       - | 4644 | `			}` |
|      31 | 4645 | `			if( rc == SXRET_OK ){` |
|       - | 4646 | `				/* Perform a boolean cast */` |
|      31 | 4647 | `				keep = ph7_value_to_bool(&sResult);` |
|      15 | 4648 | `			}` |
|      31 | 4649 | `			PH7_MemObjRelease(&sResult);` |
|      16 | 4650 | `		}else{` |
|       - | 4651 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 4652 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 4653 | `			 * the case where the callback argument is missing entirely.` |
|       - | 4654 | `			 */` |
|      29 | 4655 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 4656 | `		}` |
|      59 | 4657 | `		if( keep ){` |
|       - | 4658 | `			/* Perform the insertion,now the callback returned true */` |
|      21 | 4659 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      10 | 4660 | `		}` |
|       - | 4661 | `		/* Point to the next entry */` |
|      59 | 4662 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      30 | 4663 | `	}` |
|      15 | 4664 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4665 | `	return PH7_OK;` |
|      16 | 4666 | `}` |
|       - | 4667 | `/*` |
|       - | 4668 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|       - | 4669 | ` *  Applies the callback to the elements of the given arrays.` |
|       - | 4670 | ` * Parameters` |
|       - | 4671 | ` *  $callback` |
|       - | 4672 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|       - | 4673 | ` *   array and a NULL callback this is the identity function (the array is` |
|       - | 4674 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|       - | 4675 | ` *   are zipped together.` |
|       - | 4676 | ` *  $array` |
|       - | 4677 | ` *   The first array to run through the callback function.` |
|       - | 4678 | ` *  $arrays` |
|       - | 4679 | ` *   Zero or more additional arrays to process in parallel.` |
|       - | 4680 | ` * Return` |
|       - | 4681 | ` *  Returns an array containing the results of applying the callback function.` |
|       - | 4682 | ` *  With a single array the keys are preserved; with several arrays the result` |
|       - | 4683 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|       - | 4684 | ` *  padding shorter arrays with NULL.` |
|       - | 4685 | ` */` |
|      88 | 4686 | `PH7_PRIVATE int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 4687 | `{` |
|       - | 4688 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 4689 | `	ph7_hashmap_node *pEntry;` |
|       - | 4690 | `	ph7_hashmap *pMap;` |
|       - | 4691 | `	ph7_vm *pVm;` |
|       - | 4692 | `	int bNullCallback;` |
|       - | 4693 | `	sxi32 rc;` |
|       - | 4694 | `	int i;` |
|       - | 4695 | `	sxu32 n;` |
|      92 | 4696 | `	if( nArg < 2 ){` |
|     ! 0 | 4697 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4698 | `			"ArgumentCountError",` |
|       - | 4699 | `			"array_map() expects at least 2 arguments, %d given",` |
|     ! 0 | 4700 | `			nArg` |
|       - | 4701 | `			);` |
|       - | 4702 | `	}` |
|      92 | 4703 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|      92 | 4704 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|       8 | 4705 | `		if( ph7_value_is_string(apArg[0]) ){` |
|       6 | 4706 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|       8 | 4707 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4708 | `				"TypeError",` |
|       - | 4709 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 4710 | `				"function \"%s\" not found or invalid function name",` |
|       2 | 4711 | `				zFunc` |
|       - | 4712 | `				);` |
|       - | 4713 | `		}` |
|       3 | 4714 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4715 | `			"TypeError",` |
|       - | 4716 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 4717 | `			"no array or string given"` |
|       - | 4718 | `			);` |
|       - | 4719 | `	}` |
|       - | 4720 | `	/* Every remaining argument must be an array */` |
|     178 | 4721 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      98 | 4722 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       3 | 4723 | `			if( i == 1 ){` |
|       4 | 4724 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4725 | `					"TypeError",` |
|       - | 4726 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|       2 | 4727 | `					ph7_type_name(apArg[1])` |
|       - | 4728 | `					);` |
|       - | 4729 | `			}` |
|     ! 0 | 4730 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4731 | `				"TypeError",` |
|       - | 4732 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|     ! 0 | 4733 | `				i+1,ph7_type_name(apArg[i])` |
|       - | 4734 | `				);` |
|       - | 4735 | `		}` |
|      50 | 4736 | `	}` |
|      84 | 4737 | `	pVm = pCtx->pVm;` |
|       - | 4738 | `	/* Create a new array */` |
|      84 | 4739 | `	pArray = ph7_context_new_array(pCtx);` |
|      84 | 4740 | `	if( pArray == 0 ){` |
|     ! 0 | 4741 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4742 | `		return PH7_OK;` |
|       - | 4743 | `	}` |
|      84 | 4744 | `	PH7_MemObjInit(pVm,&sResult);` |
|      84 | 4745 | `	PH7_MemObjInit(pVm,&sKey);` |
|      84 | 4746 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      84 | 4747 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|      84 | 4748 | `	if( nArg == 2 ){` |
|       - | 4749 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|      74 | 4750 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      74 | 4751 | `		pEntry = pMap->pFirst;` |
|     258 | 4752 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 4753 | `			/* Extract the node value */` |
|     192 | 4754 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|     192 | 4755 | `			if( pValue ){` |
|       - | 4756 | `				/* Extract the node key */` |
|     192 | 4757 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|     192 | 4758 | `				if( bNullCallback ){` |
|       - | 4759 | `					/* NULL callback: identity function, keep original value */` |
|      11 | 4760 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|       6 | 4761 | `				}else{` |
|       - | 4762 | `					/* Invoke the supplied callback */` |
|     182 | 4763 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|     182 | 4764 | `					if( rc == PH7_EXCEPTION ){` |
|       - | 4765 | `						/* Callback raised: abort and let the foreign-function` |
|       - | 4766 | `						 * dispatcher unwind through the nearest try/catch. */` |
|       5 | 4767 | `						PH7_MemObjRelease(&sKey);` |
|       5 | 4768 | `						PH7_MemObjRelease(&sResult);` |
|       5 | 4769 | `						return PH7_EXCEPTION;` |
|       - | 4770 | `					}` |
|       - | 4771 | `					/* Insert the callback return value */` |
|     178 | 4772 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 4773 | `				}` |
|     188 | 4774 | `				PH7_MemObjRelease(&sKey);` |
|     188 | 4775 | `				PH7_MemObjRelease(&sResult);` |
|      92 | 4776 | `			}` |
|       - | 4777 | `			/* Point to the next entry */` |
|     188 | 4778 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      96 | 4779 | `		}` |
|      37 | 4780 | `	}else{` |
|       - | 4781 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|       - | 4782 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|      11 | 4783 | `		int nArrays = nArg - 1;` |
|       - | 4784 | `		ph7_hashmap_node **apCur;` |
|       - | 4785 | `		ph7_value **apCallArg;` |
|       - | 4786 | `		ph7_value sNull;` |
|      11 | 4787 | `		sxu32 nMax = 0;` |
|      11 | 4788 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|      11 | 4789 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|      11 | 4790 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|     ! 0 | 4791 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|     ! 0 | 4792 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|     ! 0 | 4793 | `			PH7_MemObjRelease(&sKey);` |
|     ! 0 | 4794 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 4795 | `			ph7_result_value(pCtx,pArray);` |
|     ! 0 | 4796 | `			return PH7_OK;` |
|       - | 4797 | `		}` |
|      11 | 4798 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|      11 | 4799 | `		sNull.nIdx = SXU32_HIGH;` |
|      33 | 4800 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|      23 | 4801 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|      23 | 4802 | `			apCur[i] = pMap->pFirst;` |
|      23 | 4803 | `			if( pMap->nEntry > nMax ){` |
|      13 | 4804 | `				nMax = pMap->nEntry;` |
|       6 | 4805 | `			}` |
|      12 | 4806 | `		}` |
|      35 | 4807 | `		for( n = 0 ; n < nMax ; n++ ){` |
|      25 | 4808 | `			ph7_value *pZip = 0;` |
|      25 | 4809 | `			if( bNullCallback ){` |
|       - | 4810 | `				/* zip: each result element is an array of the i-th values */` |
|       5 | 4811 | `				pZip = ph7_context_new_array(pCtx);` |
|       2 | 4812 | `			}` |
|      79 | 4813 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|      55 | 4814 | `				ph7_value *pv = &sNull;` |
|      55 | 4815 | `				if( apCur[i] ){` |
|      53 | 4816 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|      53 | 4817 | `					if( pNodeVal ){` |
|      53 | 4818 | `						pv = pNodeVal;` |
|      26 | 4819 | `					}` |
|      53 | 4820 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|      26 | 4821 | `				}` |
|      55 | 4822 | `				if( bNullCallback ){` |
|       9 | 4823 | `					if( pZip ){` |
|       9 | 4824 | `						ph7_array_add_elem(pZip,0,pv);` |
|       4 | 4825 | `					}` |
|       5 | 4826 | `				}else{` |
|      47 | 4827 | `					apCallArg[i] = pv;` |
|       - | 4828 | `				}` |
|      28 | 4829 | `			}` |
|      25 | 4830 | `			if( bNullCallback ){` |
|       5 | 4831 | `				if( pZip ){` |
|       5 | 4832 | `					ph7_array_add_elem(pArray,0,pZip);` |
|       2 | 4833 | `				}` |
|       3 | 4834 | `			}else{` |
|      21 | 4835 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|      21 | 4836 | `				if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 4837 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|     ! 0 | 4838 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|     ! 0 | 4839 | `					PH7_MemObjRelease(&sNull);` |
|     ! 0 | 4840 | `					PH7_MemObjRelease(&sKey);` |
|     ! 0 | 4841 | `					PH7_MemObjRelease(&sResult);` |
|     ! 0 | 4842 | `					return PH7_EXCEPTION;` |
|       - | 4843 | `				}` |
|      21 | 4844 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|      21 | 4845 | `				PH7_MemObjRelease(&sResult);` |
|       - | 4846 | `			}` |
|      13 | 4847 | `		}` |
|      11 | 4848 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|      11 | 4849 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|      11 | 4850 | `		PH7_MemObjRelease(&sNull);` |
|       - | 4851 | `	}` |
|      80 | 4852 | `	PH7_MemObjRelease(&sKey);` |
|      80 | 4853 | `	PH7_MemObjRelease(&sResult);` |
|      80 | 4854 | `	ph7_result_value(pCtx,pArray);` |
|      80 | 4855 | `	return PH7_OK;` |
|      48 | 4856 | `}` |
|       - | 4857 | `/*` |
|       - | 4858 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|       - | 4859 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 4860 | ` * Parameters` |
|       - | 4861 | ` *  $array` |
|       - | 4862 | ` *   The input array.` |
|       - | 4863 | ` *  $callback` |
|       - | 4864 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|       - | 4865 | ` *  $initial` |
|       - | 4866 | ` *   If the optional initial is available, it will be used at the beginning` |
|       - | 4867 | ` *   of the process, or as a final result in case the array is empty.` |
|       - | 4868 | ` * Return` |
|       - | 4869 | ` *  Returns the resulting value.` |
|       - | 4870 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 4871 | ` */` |
|      28 | 4872 | `PH7_PRIVATE int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4873 | `{` |
|       - | 4874 | `	ph7_hashmap_node *pEntry;` |
|       - | 4875 | `	ph7_hashmap *pMap;` |
|       - | 4876 | `	ph7_value *pValue;` |
|       - | 4877 | `	ph7_value sResult;` |
|       - | 4878 | `	sxi32 rc;` |
|       - | 4879 | `	sxu32 n;` |
|      33 | 4880 | `	if( nArg < 2 ){` |
|     ! 0 | 4881 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4882 | `			"ArgumentCountError",` |
|       - | 4883 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|     ! 0 | 4884 | `			nArg` |
|       - | 4885 | `			);` |
|       - | 4886 | `	}` |
|      33 | 4887 | `	if( nArg > 3 ){` |
|     ! 0 | 4888 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4889 | `			"ArgumentCountError",` |
|       - | 4890 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|     ! 0 | 4891 | `			nArg` |
|       - | 4892 | `			);` |
|       - | 4893 | `	}` |
|      33 | 4894 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4895 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4896 | `			"TypeError",` |
|       - | 4897 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4898 | `			ph7_type_name(apArg[0])` |
|       - | 4899 | `			);` |
|       - | 4900 | `	}` |
|      31 | 4901 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      12 | 4902 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 4903 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 4904 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4905 | `				"TypeError",` |
|       - | 4906 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 4907 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 4908 | `				zFunc` |
|       - | 4909 | `				);` |
|       - | 4910 | `		}` |
|       9 | 4911 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       3 | 4912 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4913 | `				"TypeError",` |
|       - | 4914 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 4915 | `				"array callback must have exactly two members"` |
|       - | 4916 | `				);` |
|       - | 4917 | `		}` |
|       6 | 4918 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4919 | `			"TypeError",` |
|       - | 4920 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 4921 | `			"no array or string given"` |
|       - | 4922 | `			);` |
|       - | 4923 | `	}` |
|       - | 4924 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 4925 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4926 | `	/* Assume a NULL initial value */` |
|      19 | 4927 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      19 | 4928 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      19 | 4929 | `	if( nArg > 2 ){` |
|       - | 4930 | `		/* Set the initial value */` |
|      13 | 4931 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       6 | 4932 | `	}` |
|       - | 4933 | `	/* Perform the requested operation */` |
|      19 | 4934 | `	pEntry = pMap->pFirst;` |
|      55 | 4935 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 4936 | `		/* Extract the node value */` |
|      39 | 4937 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 4938 | `		/* Invoke the supplied callback */` |
|      39 | 4939 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|      39 | 4940 | `		if( rc == PH7_EXCEPTION ){` |
|       - | 4941 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 4942 | `			PH7_MemObjRelease(&sResult);` |
|       3 | 4943 | `			return PH7_EXCEPTION;` |
|       - | 4944 | `		}` |
|       - | 4945 | `		/* Point to the next entry */` |
|      37 | 4946 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 4947 | `	}` |
|      17 | 4948 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      17 | 4949 | `	PH7_MemObjRelease(&sResult);` |
|      17 | 4950 | `	return PH7_OK;` |
|      19 | 4951 | `}` |
|       - | 4952 | `/*` |
|       - | 4953 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 4954 | ` *  Apply a user function to every member of an array.` |
|       - | 4955 | ` * Parameters` |
|       - | 4956 | ` *  $array` |
|       - | 4957 | ` *   The input array.` |
|       - | 4958 | ` *  $funcname` |
|       - | 4959 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 4960 | ` *   the first, and the key/index second.` |
|       - | 4961 | ` * Note:` |
|       - | 4962 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 4963 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 4964 | ` *  be made in the original array itself.` |
|       - | 4965 | ` *  $userdata` |
|       - | 4966 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 4967 | ` *   to the callback funcname.` |
|       - | 4968 | ` * Return` |
|       - | 4969 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 4970 | ` */` |
|      34 | 4971 | `PH7_PRIVATE int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4972 | `{` |
|       - | 4973 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 4974 | `	ph7_hashmap_node *pEntry;` |
|       - | 4975 | `	ph7_hashmap *pMap;` |
|       - | 4976 | `	sxu32 n;` |
|      39 | 4977 | `	if( nArg < 2 ){` |
|     ! 0 | 4978 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4979 | `			"ArgumentCountError",` |
|       - | 4980 | `			"array_walk() expects at least 2 arguments, %d given",` |
|     ! 0 | 4981 | `			nArg` |
|       - | 4982 | `			);` |
|       - | 4983 | `	}` |
|      39 | 4984 | `	if( nArg > 3 ){` |
|     ! 0 | 4985 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4986 | `			"ArgumentCountError",` |
|       - | 4987 | `			"array_walk() expects at most 3 arguments, %d given",` |
|     ! 0 | 4988 | `			nArg` |
|       - | 4989 | `			);` |
|       - | 4990 | `	}` |
|      39 | 4991 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4992 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4993 | `			"TypeError",` |
|       - | 4994 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4995 | `			ph7_type_name(apArg[0])` |
|       - | 4996 | `			);` |
|       - | 4997 | `	}` |
|      37 | 4998 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      17 | 4999 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       6 | 5000 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       8 | 5001 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5002 | `				"TypeError",` |
|       - | 5003 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 5004 | `				"function \"%s\" not found or invalid function name",` |
|       2 | 5005 | `				zFunc` |
|       - | 5006 | `				);` |
|       - | 5007 | `		}` |
|      12 | 5008 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       6 | 5009 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5010 | `				"TypeError",` |
|       - | 5011 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 5012 | `				"array callback must have exactly two members"` |
|       - | 5013 | `				);` |
|       - | 5014 | `		}` |
|       6 | 5015 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5016 | `			"TypeError",` |
|       - | 5017 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 5018 | `			"no array or string given"` |
|       - | 5019 | `			);` |
|       - | 5020 | `	}` |
|      21 | 5021 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 5022 | `	/* Point to the internal representation of the input hashmap */` |
|      21 | 5023 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      21 | 5024 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      21 | 5025 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      21 | 5026 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 5027 | `	/* Perform the desired operation */` |
|      21 | 5028 | `	pEntry = pMap->pFirst;` |
|      61 | 5029 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5030 | `		/* Extract the node value */` |
|      43 | 5031 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      43 | 5032 | `		if( pValue ){` |
|       - | 5033 | `			sxi32 rcW;` |
|       - | 5034 | `			/* Extract the entry key */` |
|      43 | 5035 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 5036 | `			/* Invoke the supplied callback */` |
|      43 | 5037 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      43 | 5038 | `			PH7_MemObjRelease(&sKey);` |
|      43 | 5039 | `			if( rcW == PH7_EXCEPTION ){` |
|       - | 5040 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 5041 | `				return PH7_EXCEPTION;` |
|       - | 5042 | `			}` |
|      20 | 5043 | `		}` |
|       - | 5044 | `		/* Point to the next entry */` |
|      41 | 5045 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      21 | 5046 | `	}` |
|       - | 5047 | `	/* All done, return TRUE */` |
|      19 | 5048 | `	ph7_result_bool(pCtx,1);` |
|      19 | 5049 | `	return PH7_OK;` |
|      22 | 5050 | `}` |
|       - | 5051 | `/*` |
|       - | 5052 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 5053 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 5054 | ` */` |
|      22 | 5055 | `static sxi32 HashmapWalkRecursive(` |
|       - | 5056 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 5057 | `	ph7_value *pCallback, /* User callback */` |
|       - | 5058 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 5059 | `	int iNest             /* Nesting level */` |
|       - | 5060 | `	)` |
|       1 | 5061 | `{` |
|       - | 5062 | `	ph7_hashmap_node *pEntry;` |
|       - | 5063 | `	ph7_value *pValue,sKey;` |
|       - | 5064 | `	sxi32 rc;` |
|       - | 5065 | `	sxu32 n;` |
|       - | 5066 | `	/* Iterate through hashmap entries */` |
|      23 | 5067 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      23 | 5068 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      23 | 5069 | `	pEntry = pMap->pFirst;` |
|      59 | 5070 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5071 | `		/* Extract the node value */` |
|      37 | 5072 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      37 | 5073 | `		if( pValue ){` |
|      37 | 5074 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      11 | 5075 | `				if( iNest < 32 ){` |
|       - | 5076 | `					/* Recurse */` |
|      11 | 5077 | `					iNest++;` |
|      11 | 5078 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      11 | 5079 | `					iNest--;` |
|      11 | 5080 | `					if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 5081 | `						return PH7_EXCEPTION;` |
|       - | 5082 | `					}` |
|       5 | 5083 | `				}` |
|       6 | 5084 | `			}else{` |
|       - | 5085 | `				/* Extract the node key */` |
|      27 | 5086 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 5087 | `				/* Invoke the supplied callback */` |
|      27 | 5088 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|      27 | 5089 | `				PH7_MemObjRelease(&sKey);` |
|      27 | 5090 | `				if( rc == PH7_EXCEPTION ){` |
|       - | 5091 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 5092 | `					return PH7_EXCEPTION;` |
|       - | 5093 | `				}` |
|       - | 5094 | `			}` |
|      18 | 5095 | `		}` |
|       - | 5096 | `		/* Point to the next entry */` |
|      37 | 5097 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 5098 | `	}` |
|      23 | 5099 | `	return PH7_OK;` |
|      12 | 5100 | `}` |
|       - | 5101 | `/*` |
|       - | 5102 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 5103 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 5104 | ` * Parameters` |
|       - | 5105 | ` *  $array` |
|       - | 5106 | ` *   The input array.` |
|       - | 5107 | ` *  $funcname` |
|       - | 5108 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 5109 | ` *   the first, and the key/index second.` |
|       - | 5110 | ` * Note:` |
|       - | 5111 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 5112 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 5113 | ` *  be made in the original array itself.` |
|       - | 5114 | ` *  $userdata` |
|       - | 5115 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 5116 | ` *   to the callback funcname.` |
|       - | 5117 | ` * Return` |
|       - | 5118 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 5119 | ` */` |
|      24 | 5120 | `PH7_PRIVATE int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5121 | `{` |
|       - | 5122 | `	ph7_hashmap *pMap;` |
|      29 | 5123 | `	if( nArg < 2 ){` |
|     ! 0 | 5124 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5125 | `			"ArgumentCountError",` |
|       - | 5126 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|     ! 0 | 5127 | `			nArg` |
|       - | 5128 | `			);` |
|       - | 5129 | `	}` |
|      29 | 5130 | `	if( nArg > 3 ){` |
|     ! 0 | 5131 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5132 | `			"ArgumentCountError",` |
|       - | 5133 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|     ! 0 | 5134 | `			nArg` |
|       - | 5135 | `			);` |
|       - | 5136 | `	}` |
|      29 | 5137 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5138 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5139 | `			"TypeError",` |
|       - | 5140 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5141 | `			ph7_type_name(apArg[0])` |
|       - | 5142 | `			);` |
|       - | 5143 | `	}` |
|      27 | 5144 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      14 | 5145 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 5146 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 5147 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5148 | `				"TypeError",` |
|       - | 5149 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 5150 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 5151 | `				zFunc` |
|       - | 5152 | `				);` |
|       - | 5153 | `		}` |
|      12 | 5154 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       6 | 5155 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5156 | `				"TypeError",` |
|       - | 5157 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 5158 | `				"array callback must have exactly two members"` |
|       - | 5159 | `				);` |
|       - | 5160 | `		}` |
|       6 | 5161 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5162 | `			"TypeError",` |
|       - | 5163 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 5164 | `			"no array or string given"` |
|       - | 5165 | `			);` |
|       - | 5166 | `	}` |
|       - | 5167 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 5168 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      13 | 5169 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5170 | `	/* Perform the desired operation */` |
|      13 | 5171 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|       - | 5172 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 5173 | `		return PH7_EXCEPTION;` |
|       - | 5174 | `	}` |
|       - | 5175 | `	/* All done, return TRUE */` |
|      13 | 5176 | `	ph7_result_bool(pCtx,1);` |
|      13 | 5177 | `	return PH7_OK;` |
|      17 | 5178 | `}` |
|       - | 5179 | `/*` |
|       - | 5180 | ` * bool array_is_list(array $array)` |
|       - | 5181 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|       - | 5182 | ` *  integers starting at 0. An empty array is a list.` |
|       - | 5183 | ` * Return` |
|       - | 5184 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|       - | 5185 | ` */` |
|       - | 5186 | `/*` |
|       - | 5187 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|       - | 5188 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|       - | 5189 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|       - | 5190 | ` */` |
|     360 | 5191 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|       3 | 5192 | `{` |
|     363 | 5193 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|     363 | 5194 | `	sxi64 iExpect = 0;` |
|       - | 5195 | `	sxu32 n;` |
|     841 | 5196 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     623 | 5197 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|       - | 5198 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|     145 | 5199 | `			return 0;` |
|       - | 5200 | `		}` |
|     481 | 5201 | `		++iExpect;` |
|     481 | 5202 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     242 | 5203 | `	}` |
|     221 | 5204 | `	return 1;` |
|     183 | 5205 | `}` |
|      12 | 5206 | `PH7_PRIVATE int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5207 | `{` |
|      13 | 5208 | `	if( nArg < 1 ){` |
|     ! 0 | 5209 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5210 | `			"ArgumentCountError",` |
|       - | 5211 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|       - | 5212 | `			);` |
|       - | 5213 | `	}` |
|      13 | 5214 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5215 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5216 | `			"TypeError",` |
|       - | 5217 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5218 | `			ph7_type_name(apArg[0])` |
|       - | 5219 | `			);` |
|       - | 5220 | `	}` |
|      13 | 5221 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|      13 | 5222 | `	return PH7_OK;` |
|       7 | 5223 | `}` |
|       - | 5224 | `/*` |
|       - | 5225 | ` * mixed array_first(array $array)` |
|       - | 5226 | ` * mixed array_last(array $array)` |
|       - | 5227 | ` *  Return the value of the first (respectively last) element of the array,` |
|       - | 5228 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|       - | 5229 | ` *  untouched (unlike reset()/end()).` |
|       - | 5230 | ` */` |
|      18 | 5231 | `static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|       1 | 5232 | `{` |
|       - | 5233 | `	ph7_hashmap *pMap;` |
|       - | 5234 | `	ph7_hashmap_node *pNode;` |
|       - | 5235 | `	ph7_value *pVal;` |
|      19 | 5236 | `	const char *zName = bLast ? "array_last" : "array_first";` |
|      19 | 5237 | `	if( nArg < 1 ){` |
|     ! 0 | 5238 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5239 | `			"ArgumentCountError",` |
|       - | 5240 | `			"%s() expects exactly 1 argument, 0 given",` |
|     ! 0 | 5241 | `			zName` |
|       - | 5242 | `			);` |
|       - | 5243 | `	}` |
|      19 | 5244 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5245 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5246 | `			"TypeError",` |
|       - | 5247 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5248 | `			zName,` |
|       1 | 5249 | `			ph7_type_name(apArg[0])` |
|       - | 5250 | `			);` |
|       - | 5251 | `	}` |
|      17 | 5252 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      17 | 5253 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|      17 | 5254 | `	if( pNode == 0 ){` |
|       - | 5255 | `		/* Empty array: PHP returns NULL */` |
|       5 | 5256 | `		ph7_result_null(pCtx);` |
|       5 | 5257 | `		return PH7_OK;` |
|       - | 5258 | `	}` |
|      13 | 5259 | `	pVal = HashmapExtractNodeValue(pNode);` |
|      13 | 5260 | `	if( pVal ){` |
|      13 | 5261 | `		ph7_result_value(pCtx,pVal);` |
|       7 | 5262 | `	}else{` |
|     ! 0 | 5263 | `		ph7_result_null(pCtx);` |
|       - | 5264 | `	}` |
|      13 | 5265 | `	return PH7_OK;` |
|      10 | 5266 | `}` |
|       8 | 5267 | `PH7_PRIVATE int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5268 | `{` |
|       9 | 5269 | `	return HashmapFirstLast(pCtx,nArg,apArg,0);` |
|       1 | 5270 | `}` |
|      10 | 5271 | `PH7_PRIVATE int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5272 | `{` |
|      11 | 5273 | `	return HashmapFirstLast(pCtx,nArg,apArg,1);` |
|       1 | 5274 | `}` |
|       - | 5275 | `/*` |
|       - | 5276 | ` * int\|string\|null array_key_first(array $array)` |
|       - | 5277 | ` * int\|string\|null array_key_last(array $array)` |
|       - | 5278 | ` *  Return the key of the first (respectively last) element of the array,` |
|       - | 5279 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|       - | 5280 | ` *  untouched.` |
|       - | 5281 | ` */` |
|      22 | 5282 | `static int HashmapKeyFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|       1 | 5283 | `{` |
|       - | 5284 | `	ph7_hashmap *pMap;` |
|       - | 5285 | `	ph7_hashmap_node *pNode;` |
|      23 | 5286 | `	const char *zName = bLast ? "array_key_last" : "array_key_first";` |
|      23 | 5287 | `	if( nArg < 1 ){` |
|     ! 0 | 5288 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5289 | `			"ArgumentCountError",` |
|       - | 5290 | `			"%s() expects exactly 1 argument, 0 given",` |
|     ! 0 | 5291 | `			zName` |
|       - | 5292 | `			);` |
|       - | 5293 | `	}` |
|      23 | 5294 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5295 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5296 | `			"TypeError",` |
|       - | 5297 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5298 | `			zName,` |
|       1 | 5299 | `			ph7_type_name(apArg[0])` |
|       - | 5300 | `			);` |
|       - | 5301 | `	}` |
|      21 | 5302 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      21 | 5303 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|      21 | 5304 | `	if( pNode == 0 ){` |
|       - | 5305 | `		/* Empty array: PHP returns NULL */` |
|       5 | 5306 | `		ph7_result_null(pCtx);` |
|       5 | 5307 | `		return PH7_OK;` |
|       - | 5308 | `	}` |
|      17 | 5309 | `	HashmapResultNodeKey(pCtx,pNode);` |
|      17 | 5310 | `	return PH7_OK;` |
|      12 | 5311 | `}` |
|      10 | 5312 | `PH7_PRIVATE int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5313 | `{` |
|      11 | 5314 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,0);` |
|       1 | 5315 | `}` |
|      12 | 5316 | `PH7_PRIVATE int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5317 | `{` |
|      13 | 5318 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,1);` |
|       1 | 5319 | `}` |
|       - | 5320 | `/*` |
|       - | 5321 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|       - | 5322 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|       - | 5323 | ` * array_column() for both the column value and the index key.` |
|       - | 5324 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|       - | 5325 | ` * container or the key is absent.` |
|       - | 5326 | ` */` |
|      32 | 5327 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|       1 | 5328 | `{` |
|      33 | 5329 | `	if( ph7_value_is_array(pRow) ){` |
|       - | 5330 | `		ph7_hashmap_node *pNode;` |
|      25 | 5331 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|      21 | 5332 | `			return HashmapExtractNodeValue(pNode);` |
|       1 | 5333 | `		}` |
|      11 | 5334 | `	}else if( ph7_value_is_object(pRow) ){` |
|       - | 5335 | `		ph7_value sName;` |
|       - | 5336 | `		const char *zName;` |
|       - | 5337 | `		ph7_value *pAttr;` |
|       - | 5338 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|       - | 5339 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|       9 | 5340 | `		PH7_MemObjInit(pVm,&sName);` |
|       9 | 5341 | `		PH7_MemObjStore(pKey,&sName);` |
|       9 | 5342 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|       9 | 5343 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|       9 | 5344 | `		PH7_MemObjRelease(&sName);` |
|       9 | 5345 | `		return pAttr;` |
|       - | 5346 | `	}` |
|       5 | 5347 | `	return 0;` |
|      17 | 5348 | `}` |
|       - | 5349 | `/*` |
|       - | 5350 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|       - | 5351 | ` *  Returns the values from a single column of the input, identified by` |
|       - | 5352 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|       - | 5353 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|       - | 5354 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|       - | 5355 | ` *  Each row may be an array or an object.` |
|       - | 5356 | ` */` |
|      12 | 5357 | `PH7_PRIVATE int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5358 | `{` |
|       - | 5359 | `	ph7_hashmap_node *pNode;` |
|       - | 5360 | `	ph7_hashmap *pMap;` |
|       - | 5361 | `	ph7_value *pArray;` |
|       - | 5362 | `	ph7_value *pRow;` |
|       - | 5363 | `	ph7_value *pCol;` |
|       - | 5364 | `	ph7_value *pIdx;` |
|       - | 5365 | `	int bWantCol;` |
|       - | 5366 | `	int bWantIdx;` |
|       - | 5367 | `	sxu32 n;` |
|      13 | 5368 | `	if( nArg < 2 ){` |
|     ! 0 | 5369 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5370 | `			"ArgumentCountError",` |
|       - | 5371 | `			"array_column() expects at least 2 arguments, %d given",` |
|     ! 0 | 5372 | `			nArg` |
|       - | 5373 | `			);` |
|       - | 5374 | `	}` |
|      13 | 5375 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5376 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5377 | `			"TypeError",` |
|       - | 5378 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5379 | `			ph7_type_name(apArg[0])` |
|       - | 5380 | `			);` |
|       - | 5381 | `	}` |
|      13 | 5382 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      13 | 5383 | `	pArray = ph7_context_new_array(pCtx);` |
|      13 | 5384 | `	if( pArray == 0 ){` |
|     ! 0 | 5385 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5386 | `		return PH7_OK;` |
|       - | 5387 | `	}` |
|       - | 5388 | `	/* A NULL column_key means "collect the entire row". */` |
|      13 | 5389 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|      13 | 5390 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|      13 | 5391 | `	pNode = pMap->pFirst;` |
|      33 | 5392 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      21 | 5393 | `		pRow = HashmapExtractNodeValue(pNode);` |
|      21 | 5394 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|      21 | 5395 | `		if( pRow == 0 ){` |
|     ! 0 | 5396 | `			continue;` |
|       - | 5397 | `		}` |
|      21 | 5398 | `		if( bWantCol ){` |
|      19 | 5399 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|      19 | 5400 | `			if( pCol == 0 ){` |
|       - | 5401 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|       3 | 5402 | `				continue;` |
|       - | 5403 | `			}` |
|       9 | 5404 | `		}else{` |
|       3 | 5405 | `			pCol = pRow;` |
|       - | 5406 | `		}` |
|      19 | 5407 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|      19 | 5408 | `		if( pIdx ){` |
|      13 | 5409 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|       7 | 5410 | `		}else{` |
|       7 | 5411 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|       - | 5412 | `		}` |
|      10 | 5413 | `	}` |
|      13 | 5414 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 5415 | `	return PH7_OK;` |
|       7 | 5416 | `}` |
|       - | 5417 | `/*` |
|       - | 5418 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|       - | 5419 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|       - | 5420 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|       - | 5421 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|       - | 5422 | ` */` |
|      28 | 5423 | `static sxi32 HashmapCallbackSearch(` |
|       - | 5424 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|       - | 5425 | `	const char *zName,            /* Function name for diagnostics */` |
|       - | 5426 | `	int bWant,                    /* Truthiness being hunted for */` |
|       - | 5427 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|       - | 5428 | `	)` |
|       1 | 5429 | `{` |
|       - | 5430 | `	ph7_hashmap_node *pEntry;` |
|       - | 5431 | `	ph7_hashmap *pMap;` |
|       - | 5432 | `	ph7_value *pValue;` |
|       - | 5433 | `	ph7_value *apCbArg[2];` |
|       - | 5434 | `	ph7_value sKey;` |
|       - | 5435 | `	ph7_value sResult;` |
|       - | 5436 | `	sxi32 rc;` |
|       - | 5437 | `	sxu32 n;` |
|      29 | 5438 | `	*ppMatch = 0;` |
|      29 | 5439 | `	if( nArg < 2 ){` |
|     ! 0 | 5440 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5441 | `			"ArgumentCountError",` |
|       - | 5442 | `			"%s() expects exactly 2 arguments, %d given",` |
|     ! 0 | 5443 | `			zName,nArg` |
|       - | 5444 | `			);` |
|       - | 5445 | `	}` |
|      29 | 5446 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5447 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5448 | `			"TypeError",` |
|       - | 5449 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5450 | `			zName,ph7_type_name(apArg[0])` |
|       - | 5451 | `			);` |
|       - | 5452 | `	}` |
|      29 | 5453 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|     ! 0 | 5454 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5455 | `			"TypeError",` |
|       - | 5456 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|     ! 0 | 5457 | `			zName,ph7_type_name(apArg[1])` |
|       - | 5458 | `			);` |
|       - | 5459 | `	}` |
|      29 | 5460 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      29 | 5461 | `	pEntry = pMap->pFirst;` |
|      29 | 5462 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      29 | 5463 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|      29 | 5464 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      29 | 5465 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      73 | 5466 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 5467 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      59 | 5468 | `		if( pValue ){` |
|       - | 5469 | `			/* The callback receives ($value, $key). */` |
|      59 | 5470 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      59 | 5471 | `			apCbArg[0] = pValue;` |
|      59 | 5472 | `			apCbArg[1] = &sKey;` |
|      59 | 5473 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|      59 | 5474 | `			if( rc == PH7_EXCEPTION ){` |
|       - | 5475 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 5476 | `				PH7_MemObjRelease(&sKey);` |
|     ! 0 | 5477 | `				PH7_MemObjRelease(&sResult);` |
|     ! 0 | 5478 | `				return PH7_EXCEPTION;` |
|       - | 5479 | `			}` |
|      59 | 5480 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|      15 | 5481 | `				*ppMatch = pEntry;` |
|      15 | 5482 | `				break;` |
|       - | 5483 | `			}` |
|      22 | 5484 | `		}` |
|      45 | 5485 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      23 | 5486 | `	}` |
|      29 | 5487 | `	PH7_MemObjRelease(&sKey);` |
|      29 | 5488 | `	PH7_MemObjRelease(&sResult);` |
|      29 | 5489 | `	return PH7_OK;` |
|      15 | 5490 | `}` |
|       - | 5491 | `/*` |
|       - | 5492 | ` * mixed array_find(array $array, callable $callback)` |
|       - | 5493 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|       - | 5494 | ` *  is truthy, or NULL if none match.` |
|       - | 5495 | ` */` |
|       6 | 5496 | `PH7_PRIVATE int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5497 | `{` |
|       - | 5498 | `	ph7_hashmap_node *pMatch;` |
|       - | 5499 | `	ph7_value *pVal;` |
|       - | 5500 | `	sxi32 rc;` |
|       7 | 5501 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|       7 | 5502 | `	if( rc != PH7_OK ){` |
|     ! 0 | 5503 | `		return rc;` |
|       - | 5504 | `	}` |
|       7 | 5505 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|       5 | 5506 | `		ph7_result_value(pCtx,pVal);` |
|       3 | 5507 | `	}else{` |
|       3 | 5508 | `		ph7_result_null(pCtx);` |
|       - | 5509 | `	}` |
|       7 | 5510 | `	return PH7_OK;` |
|       4 | 5511 | `}` |
|       - | 5512 | `/*` |
|       - | 5513 | ` * mixed array_find_key(array $array, callable $callback)` |
|       - | 5514 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|       - | 5515 | ` *  is truthy, or NULL if none match.` |
|       - | 5516 | ` */` |
|       6 | 5517 | `PH7_PRIVATE int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5518 | `{` |
|       - | 5519 | `	ph7_hashmap_node *pMatch;` |
|       - | 5520 | `	sxi32 rc;` |
|       7 | 5521 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|       7 | 5522 | `	if( rc != PH7_OK ){` |
|     ! 0 | 5523 | `		return rc;` |
|       - | 5524 | `	}` |
|       7 | 5525 | `	if( pMatch == 0 ){` |
|       3 | 5526 | `		ph7_result_null(pCtx);` |
|       6 | 5527 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|       3 | 5528 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|       2 | 5529 | `	}else{` |
|       4 | 5530 | `		ph7_result_string(pCtx,` |
|       2 | 5531 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|       2 | 5532 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|       - | 5533 | `	}` |
|       7 | 5534 | `	return PH7_OK;` |
|       4 | 5535 | `}` |
|       - | 5536 | `/*` |
|       - | 5537 | ` * bool array_any(array $array, callable $callback)` |
|       - | 5538 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|       - | 5539 | ` *  FALSE for an empty array.` |
|       - | 5540 | ` */` |
|       8 | 5541 | `PH7_PRIVATE int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5542 | `{` |
|       - | 5543 | `	ph7_hashmap_node *pMatch;` |
|       - | 5544 | `	sxi32 rc;` |
|       9 | 5545 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|       9 | 5546 | `	if( rc != PH7_OK ){` |
|     ! 0 | 5547 | `		return rc;` |
|       - | 5548 | `	}` |
|       9 | 5549 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|       9 | 5550 | `	return PH7_OK;` |
|       5 | 5551 | `}` |
|       - | 5552 | `/*` |
|       - | 5553 | ` * bool array_all(array $array, callable $callback)` |
|       - | 5554 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|       - | 5555 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|       - | 5556 | ` */` |
|       8 | 5557 | `PH7_PRIVATE int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5558 | `{` |
|       - | 5559 | `	ph7_hashmap_node *pMatch;` |
|       - | 5560 | `	sxi32 rc;` |
|       9 | 5561 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|       9 | 5562 | `	if( rc != PH7_OK ){` |
|     ! 0 | 5563 | `		return rc;` |
|       - | 5564 | `	}` |
|       9 | 5565 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|       9 | 5566 | `	return PH7_OK;` |
|       5 | 5567 | `}` |
|       - | 5568 | `/*` |
|       - | 5569 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|       - | 5570 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|       - | 5571 | ` */` |
|       - | 5572 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|       - | 5573 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|      80 | 5574 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       4 | 5575 | `{` |
|      84 | 5576 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|      40 | 5577 | `	(void)pVm;` |
|      84 | 5578 | `	p->nCount++;` |
|      84 | 5579 | `	if( p->pArray ){` |
|       - | 5580 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|       - | 5581 | `		 * otherwise append with an auto-assigned int index. */` |
|      70 | 5582 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|      33 | 5583 | `	}` |
|      84 | 5584 | `	return SXRET_OK;` |
|       4 | 5585 | `}` |
|       - | 5586 | `/*` |
|       - | 5587 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|       - | 5588 | ` */` |
|      30 | 5589 | `PH7_PRIVATE int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       4 | 5590 | `{` |
|       - | 5591 | `	struct IterCollect sCol;` |
|       - | 5592 | `	ph7_value *pArray;` |
|       - | 5593 | `	sxi32 rc;` |
|      34 | 5594 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      34 | 5595 | `	pArray = ph7_context_new_array(pCtx);` |
|      34 | 5596 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      34 | 5597 | `	sCol.pArray = pArray;` |
|      34 | 5598 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|      34 | 5599 | `	sCol.nCount = 0;` |
|      34 | 5600 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       - | 5601 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|       3 | 5602 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 5603 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 5604 | `		sxu32 n;` |
|       9 | 5605 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5606 | `			ph7_value sKey, *pVal;` |
|       7 | 5607 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|       7 | 5608 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       7 | 5609 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|       7 | 5610 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|       7 | 5611 | `			PH7_MemObjRelease(&sKey);` |
|       7 | 5612 | `			pEntry = pEntry->pPrev;` |
|       4 | 5613 | `		}` |
|       3 | 5614 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 5615 | `		return PH7_OK;` |
|       - | 5616 | `	}` |
|      32 | 5617 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|      32 | 5618 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|      30 | 5619 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 5620 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 5621 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|     ! 0 | 5622 | `			ph7_type_name(apArg[0]));` |
|       - | 5623 | `	}` |
|      30 | 5624 | `	ph7_result_value(pCtx,pArray);` |
|      30 | 5625 | `	return PH7_OK;` |
|      19 | 5626 | `}` |
|       - | 5627 | `/*` |
|       - | 5628 | ` * int iterator_count(Traversable\|array $iterator)` |
|       - | 5629 | ` */` |
|       8 | 5630 | `PH7_PRIVATE int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 5631 | `{` |
|       - | 5632 | `	struct IterCollect sCol;` |
|       - | 5633 | `	sxi32 rc;` |
|       9 | 5634 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|       9 | 5635 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       3 | 5636 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|       3 | 5637 | `		return PH7_OK;` |
|       - | 5638 | `	}` |
|       7 | 5639 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|       7 | 5640 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|       7 | 5641 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|       7 | 5642 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 5643 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 5644 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|     ! 0 | 5645 | `			ph7_type_name(apArg[0]));` |
|       - | 5646 | `	}` |
|       7 | 5647 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|       7 | 5648 | `	return PH7_OK;` |
|       5 | 5649 | `}` |
|       - | 5650 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|       - | 5651 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|       - | 5652 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|       - | 5653 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|      32 | 5654 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       1 | 5655 | `{` |
|      33 | 5656 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|       - | 5657 | `	ph7_value sResult;` |
|       - | 5658 | `	SySet aArg;` |
|       - | 5659 | `	sxi32 rc;` |
|       - | 5660 | `	int bContinue;` |
|      16 | 5661 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|      33 | 5662 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|      33 | 5663 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|       9 | 5664 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|       9 | 5665 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 5666 | `		sxu32 n;` |
|      17 | 5667 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       9 | 5668 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|       9 | 5669 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|       9 | 5670 | `			pEntry = pEntry->pPrev;` |
|       5 | 5671 | `		}` |
|       4 | 5672 | `	}` |
|      33 | 5673 | `	PH7_MemObjInit(pVm,&sResult);` |
|      49 | 5674 | `	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|      32 | 5675 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult);` |
|      33 | 5676 | `	SySetRelease(&aArg);` |
|      33 | 5677 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|      31 | 5678 | `	p->nCount++;` |
|      31 | 5679 | `	PH7_MemObjToBool(&sResult);` |
|      31 | 5680 | `	bContinue = (sResult.x.iVal != 0);` |
|      31 | 5681 | `	PH7_MemObjRelease(&sResult);` |
|      31 | 5682 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|      17 | 5683 | `}` |
|       - | 5684 | `/*` |
|       - | 5685 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|       - | 5686 | ` */` |
|      12 | 5687 | `PH7_PRIVATE int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 5688 | `{` |
|       - | 5689 | `	struct IterApply sApp;` |
|       - | 5690 | `	sxi32 rc;` |
|      13 | 5691 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|      13 | 5692 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|     ! 0 | 5693 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 5694 | `			"iterator_apply(): Argument #2 ($callback) must be a valid callback");` |
|       - | 5695 | `	}` |
|      13 | 5696 | `	sApp.pCallback = apArg[1];` |
|      13 | 5697 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|      13 | 5698 | `	sApp.nCount = 0;` |
|      13 | 5699 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|      13 | 5700 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|      11 | 5701 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 5702 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 5703 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|     ! 0 | 5704 | `			ph7_type_name(apArg[0]));` |
|       - | 5705 | `	}` |
|      11 | 5706 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|      11 | 5707 | `	return PH7_OK;` |
|       7 | 5708 | `}` |
|       - | 5709 |  |
