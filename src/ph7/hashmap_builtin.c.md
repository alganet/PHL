# src/ph7/hashmap_builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2443/2843 lines (85.93%)

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
|    2256 |   64 | `PH7_PRIVATE int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |   65 | `{` |
|    2261 |   66 | `	int bRecursive = FALSE;` |
|    2261 |   67 | `	int bCycleDetected = FALSE;` |
|       - |   68 | `	sxi64 iCount;` |
|    2261 |   69 | `	if( nArg < 1 ){` |
|     ! 0 |   70 | `		return PH7_VmThrowException(pCtx,` |
|       - |   71 | `			"ArgumentCountError",` |
|       - |   72 | `			"count() expects at least 1 argument, 0 given"` |
|       - |   73 | `			);` |
|       - |   74 | `	}` |
|    2261 |   75 | `	if( nArg > 2 ){` |
|     ! 0 |   76 | `		return PH7_VmThrowException(pCtx,` |
|       - |   77 | `			"ArgumentCountError",` |
|       - |   78 | `			"count() expects at most 2 arguments, %d given",` |
|     ! 0 |   79 | `			nArg` |
|       - |   80 | `			);` |
|       - |   81 | `	}` |
|       - |   82 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|       - |   83 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|       - |   84 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|    2261 |   85 | `	if( nArg > 1 ){` |
|      47 |   86 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|      47 |   87 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|      11 |   88 | `			return PH7_VmThrowException(pCtx,` |
|       - |   89 | `				"ValueError",` |
|       - |   90 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|       - |   91 | `				);` |
|       - |   92 | `		}` |
|      36 |   93 | `		bRecursive = iMode == 1;` |
|      17 |   94 | `	}` |
|    2253 |   95 | `	if( !ph7_value_is_array(apArg[0]) ){` |
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
|    2183 |  120 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|    2183 |  121 | `	if( bCycleDetected ){` |
|       3 |  122 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|       1 |  123 | `	}` |
|    2183 |  124 | `	ph7_result_int64(pCtx,iCount);` |
|    2183 |  125 | `	return PH7_OK;` |
|    1133 |  126 | `}` |
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
|       5 |  139 | `{` |
|       - |  140 | `	sxi32 rc;` |
|     109 |  141 | `	if( nArg != 2 ){` |
|       - |  142 | `		/* PHP requires exactly two arguments */` |
|     ! 0 |  143 | `		return PH7_VmThrowException(pCtx,` |
|       - |  144 | `			"ArgumentCountError",` |
|       - |  145 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|     ! 0 |  146 | `			nArg` |
|       - |  147 | `			);` |
|       - |  148 | `	}` |
|       - |  149 | `	/* Make sure we are dealing with a valid hashmap */` |
|     109 |  150 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - |  151 | `		/* Type mismatch -> TypeError */` |
|       8 |  152 | `		return PH7_VmThrowException(pCtx,` |
|       - |  153 | `			"TypeError",` |
|       - |  154 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|       4 |  155 | `			ph7_type_name(apArg[1])` |
|       - |  156 | `			);` |
|       - |  157 | `	}` |
|       - |  158 | `	/* php only DEPRECATES a null / lossy-float key here; PHL rejects it. */` |
|     105 |  159 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|       3 |  160 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  161 | `			"array_key_exists(): Argument #1 ($key) must be of type string\|int, null given");` |
|     103 |  162 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|       3 |  163 | `		ph7_real rVal = apArg[0]->rVal;` |
|       3 |  164 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|       3 |  165 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  166 | `				"array_key_exists(): Argument #1 ($key) must be of type string\|int, float given");` |
|       - |  167 | `		}` |
|     ! 0 |  168 | `	}` |
|       - |  169 | `	/* Perform the lookup */` |
|     101 |  170 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|       - |  171 | `	/* lookup result */` |
|     101 |  172 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|     101 |  173 | `	return PH7_OK;` |
|      57 |  174 | `}` |
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
|       4 |  244 | `{` |
|       - |  245 | `	ph7_hashmap *pMap;` |
|       - |  246 | `	sxi32 rc;` |
|       - |  247 | `	int i;` |
|      26 |  248 | `	if( nArg < 1 ){` |
|     ! 0 |  249 | `		return PH7_VmThrowException(pCtx,` |
|       - |  250 | `			"ArgumentCountError",` |
|       - |  251 | `			"array_push() expects at least 1 argument, %d given",` |
|     ! 0 |  252 | `			nArg` |
|       - |  253 | `			);` |
|       - |  254 | `	}` |
|       - |  255 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - |  256 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      26 |  257 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       6 |  258 | `		return PH7_VmThrowException(pCtx,` |
|       - |  259 | `			"Error",` |
|       - |  260 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|       - |  261 | `			);` |
|       - |  262 | `	}` |
|       - |  263 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 |  264 | `	if( !ph7_value_is_array(apArg[0]) ){` |
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
|      15 |  288 | `}` |
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
|       2 | 1258 | `{` |
|       - | 1259 | `	ph7_hashmap_node *pNode;` |
|       - | 1260 | `	ph7_hashmap *pMap;` |
|       - | 1261 | `	ph7_value *pArray;` |
|       - | 1262 | `	ph7_value *pObj;` |
|       - | 1263 | `	sxu32 n;` |
|      48 | 1264 | `	if( nArg != 1 ){` |
|       - | 1265 | `		/* Wrong argument count, throw ArgumentCountError */` |
|     ! 0 | 1266 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1267 | `			"ArgumentCountError",` |
|       - | 1268 | `			"array_values() expects exactly 1 argument, %d given",` |
|     ! 0 | 1269 | `			nArg` |
|       - | 1270 | `			);` |
|       - | 1271 | `	}` |
|       - | 1272 | `	/* Make sure we are dealing with a valid hashmap */` |
|      48 | 1273 | `	if( !ph7_value_is_array(apArg[0]) ){` |
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
|      25 | 1303 | `}` |
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
|       4 | 1318 | `{` |
|       - | 1319 | `	ph7_hashmap_node *pNode;` |
|       - | 1320 | `	ph7_hashmap *pMap;` |
|       - | 1321 | `	ph7_value *pArray;` |
|       - | 1322 | `	ph7_value sObj;` |
|       - | 1323 | `	ph7_value sVal;` |
|       - | 1324 | `	SyString sKey;` |
|       - | 1325 | `	int bStrict;` |
|       - | 1326 | `	sxi32 rc;` |
|       - | 1327 | `	sxu32 n;` |
|     200 | 1328 | `	if( nArg < 1 ){` |
|       - | 1329 | `		/* Missing argument,throw ArgumentCountError */` |
|     ! 0 | 1330 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1331 | `			"ArgumentCountError",` |
|       - | 1332 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 1333 | `			);` |
|       - | 1334 | `	}` |
|       - | 1335 | `	/* Make sure we are dealing with a valid hashmap */` |
|     200 | 1336 | `	if( !ph7_value_is_array(apArg[0]) ){` |
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
|    1646 | 1367 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|    1452 | 1368 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     194 | 1369 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|      99 | 1370 | `		}else{` |
|    1261 | 1371 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|    1261 | 1372 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 1373 | `		}` |
|    1452 | 1374 | `		rc = 0;` |
|    1452 | 1375 | `		if( nArg > 1 ){` |
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
|    1452 | 1391 | `		if( rc == 0 ){` |
|       - | 1392 | `			/* Perform the insertion */` |
|    1420 | 1393 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|     708 | 1394 | `		}` |
|    1452 | 1395 | `		PH7_MemObjRelease(&sObj);` |
|       - | 1396 | `		/* Point to the next entry */` |
|    1452 | 1397 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     728 | 1398 | `	}` |
|       - | 1399 | `	/* return the new array */` |
|     198 | 1400 | `	ph7_result_value(pCtx,pArray);` |
|     198 | 1401 | `	return PH7_OK;` |
|     102 | 1402 | `}` |
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
|   32626 | 1939 | `PH7_PRIVATE int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1940 | `{` |
|       - | 1941 | `	ph7_value *pNeedle;` |
|       - | 1942 | `	int bStrict;` |
|       - | 1943 | `	int rc;` |
|   32631 | 1944 | `	if( nArg < 2 ){` |
|       - | 1945 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 1946 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1947 | `		return PH7_OK;` |
|       - | 1948 | `	}` |
|   32631 | 1949 | `	pNeedle = apArg[0];` |
|   32631 | 1950 | `	bStrict = 0;` |
|   32631 | 1951 | `	if( nArg > 2 ){` |
|      70 | 1952 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|      34 | 1953 | `	}` |
|   32631 | 1954 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 1955 | `		/* haystack must be an array,perform a standard comparison */` |
|     ! 0 | 1956 | `		rc = ph7_value_compare(pNeedle,apArg[1],bStrict);` |
|       - | 1957 | `		/* Set the comparison result */` |
|     ! 0 | 1958 | `		ph7_result_bool(pCtx,rc == 0);` |
|     ! 0 | 1959 | `		return PH7_OK;` |
|       - | 1960 | `	}` |
|       - | 1961 | `	/* Perform the lookup */` |
|   32631 | 1962 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 1963 | `	/* Lookup result */` |
|   32631 | 1964 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   32631 | 1965 | `	return PH7_OK;` |
|   16318 | 1966 | `}` |
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
|      36 | 1982 | `PH7_PRIVATE int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1983 | `{` |
|       - | 1984 | `	ph7_hashmap_node *pEntry;` |
|       - | 1985 | `	ph7_value *pVal,sNeedle;` |
|       - | 1986 | `	ph7_hashmap *pMap;` |
|       - | 1987 | `	ph7_value sVal;` |
|       - | 1988 | `	int bStrict;` |
|       - | 1989 | `	sxu32 n;` |
|       - | 1990 | `	int rc;` |
|      38 | 1991 | `	if( nArg < 2 ){` |
|       - | 1992 | `		/* Missing argument,throw ArgumentCountError */` |
|     ! 0 | 1993 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1994 | `			"ArgumentCountError",` |
|       - | 1995 | `			"array_search() expects at least 2 arguments, %d given",` |
|     ! 0 | 1996 | `			nArg` |
|       - | 1997 | `			);` |
|       - | 1998 | `	}` |
|      38 | 1999 | `	bStrict = FALSE;` |
|      38 | 2000 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2001 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 2002 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2003 | `			"TypeError",` |
|       - | 2004 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|       2 | 2005 | `			ph7_type_name(apArg[1])` |
|       - | 2006 | `			);` |
|       - | 2007 | `	}` |
|      35 | 2008 | `	if( nArg > 2 ){` |
|       - | 2009 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      21 | 2010 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|     ! 0 | 2011 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2012 | `				"TypeError",` |
|       - | 2013 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|     ! 0 | 2014 | `				ph7_type_name(apArg[2])` |
|       - | 2015 | `				);` |
|       - | 2016 | `		}` |
|      21 | 2017 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|      10 | 2018 | `	}` |
|       - | 2019 | `	/* Point to the internal representation of the internal hashmap */` |
|      35 | 2020 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 2021 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      35 | 2022 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      35 | 2023 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      35 | 2024 | `	pEntry = pMap->pFirst;` |
|      35 | 2025 | `	n = pMap->nEntry;` |
|      34 | 2026 | `	for(;;){` |
|      69 | 2027 | `		if( !n ){` |
|       9 | 2028 | `			break;` |
|       - | 2029 | `		}` |
|       - | 2030 | `		/* Extract node value */` |
|      61 | 2031 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      61 | 2032 | `		if( pVal ){` |
|       - | 2033 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 2034 | `			 * can change their type.` |
|       - | 2035 | `			 */` |
|      61 | 2036 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      61 | 2037 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      61 | 2038 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      61 | 2039 | `			PH7_MemObjRelease(&sVal);` |
|      61 | 2040 | `			PH7_MemObjRelease(&sNeedle);` |
|      61 | 2041 | `			if( rc == 0 ){` |
|       - | 2042 | `				/* Match found,return key */` |
|      27 | 2043 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 2044 | `					/* INT key */` |
|      21 | 2045 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|      11 | 2046 | `				}else{` |
|       7 | 2047 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 2048 | `					/* Blob key */` |
|       7 | 2049 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 2050 | `				}` |
|      27 | 2051 | `				return PH7_OK;` |
|       - | 2052 | `			}` |
|      17 | 2053 | `		}` |
|       - | 2054 | `		/* Point to the next entry */` |
|      35 | 2055 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      35 | 2056 | `		n--;` |
|       1 | 2057 | `	}` |
|       - | 2058 | `	/* No such value,return FALSE */` |
|       9 | 2059 | `	ph7_result_bool(pCtx,0);` |
|       9 | 2060 | `	return PH7_OK;` |
|      20 | 2061 | `}` |
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
|      24 | 2178 | `PH7_PRIVATE int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
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
|      29 | 2190 | `	if( nArg < 2 ){` |
|     ! 0 | 2191 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2192 | `			"ArgumentCountError",` |
|       - | 2193 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|     ! 0 | 2194 | `			nArg` |
|       - | 2195 | `			);` |
|       - | 2196 | `	}` |
|      29 | 2197 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2198 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2199 | `			"TypeError",` |
|       - | 2200 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2201 | `			ph7_type_name(apArg[0])` |
|       - | 2202 | `			);` |
|       - | 2203 | `	}` |
|       - | 2204 |  |
|       - | 2205 | `	/* php validates the CALLBACK (the last argument) before the intermediary` |
|       - | 2206 | ``	 * arrays: `array_udiff([1],"x",123)` reports Argument #3 (the bad callback),`` |
|       - | 2207 | `	 * not Argument #2 (the non-array). PHL had the middle-array loop first, so it` |
|       - | 2208 | `	 * named the wrong argument whenever both were invalid. */` |
|      27 | 2209 | `	pCallback = apArg[nArg - 1];` |
|      27 | 2210 | `	if( !ph7_value_is_callable(pCallback) ){` |
|      13 | 2211 | `		if( ph7_value_is_array(pCallback) ){` |
|       4 | 2212 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2213 | `				"TypeError",` |
|       - | 2214 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 2215 | `				nArg` |
|       - | 2216 | `				);` |
|       - | 2217 | `		}` |
|      11 | 2218 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 2219 | `			int len;` |
|       6 | 2220 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       8 | 2221 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2222 | `				"TypeError",` |
|       - | 2223 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       2 | 2224 | `				nArg,` |
|       2 | 2225 | `				zName` |
|       - | 2226 | `				);` |
|       - | 2227 | `		}` |
|       8 | 2228 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2229 | `			"TypeError",` |
|       - | 2230 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 2231 | `			nArg` |
|       - | 2232 | `			);` |
|       - | 2233 | `	}` |
|       - | 2234 |  |
|       - | 2235 | `	/* Now the intermediary arguments (arrays), left to right. */` |
|      21 | 2236 | `	for( i = 1 ; i < nArg - 1; i++ ){` |
|      13 | 2237 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       7 | 2238 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2239 | `				"TypeError",` |
|       - | 2240 | `				"array_udiff(): Argument #%d must be of type array, %s given",` |
|       2 | 2241 | `				i + 1,` |
|       4 | 2242 | `				ph7_type_name(apArg[i])` |
|       - | 2243 | `				);` |
|       - | 2244 | `		}` |
|       5 | 2245 | `	}` |
|       - | 2246 |  |
|      10 | 2247 | `	if( nArg == 2 ){` |
|       - | 2248 | `		/* Only the original array and the callback were provided. */` |
|       3 | 2249 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2250 | `		return PH7_OK;` |
|       - | 2251 | `	}` |
|       - | 2252 |  |
|       - | 2253 | `	/* Create a new array */` |
|       8 | 2254 | `	pArray = ph7_context_new_array(pCtx);` |
|       8 | 2255 | `	if( pArray == 0 ){` |
|     ! 0 | 2256 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2257 | `		return PH7_OK;` |
|       - | 2258 | `	}` |
|       - | 2259 | `	/* Point to the internal representation of the source hashmap */` |
|       8 | 2260 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2261 | `	/* Perform the diff */` |
|       8 | 2262 | `	pEntry = pSrc->pFirst;` |
|       8 | 2263 | `	n = pSrc->nEntry;` |
|       8 | 2264 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|       9 | 2265 | `	for(;;){` |
|      20 | 2266 | `		if( n < 1 ){` |
|       6 | 2267 | `			break;` |
|       - | 2268 | `		}` |
|       - | 2269 | `		/* Extract the node value */` |
|      16 | 2270 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      16 | 2271 | `		if( pVal ){` |
|      24 | 2272 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 2273 | `				/* Point to the internal representation of the hashmap */` |
|      16 | 2274 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2275 | `				/* Perform the lookup */` |
|      16 | 2276 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|      16 | 2277 | `				if( rc == SXRET_OK ){` |
|       - | 2278 | `					/* Value exist */` |
|       8 | 2279 | `					break;` |
|       - | 2280 | `				}` |
|       6 | 2281 | `			}` |
|      16 | 2282 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2283 | `				/* The comparison callback raised: propagate so the dispatcher` |
|       - | 2284 | `				 * unwinds, before any spurious insertion into the result. */` |
|       3 | 2285 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 2286 | `				return PH7_EXCEPTION;` |
|       - | 2287 | `			}` |
|      14 | 2288 | `			if( i >= (nArg - 1)){` |
|       - | 2289 | `				/* Perform the insertion */` |
|       8 | 2290 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       3 | 2291 | `			}` |
|       6 | 2292 | `		}` |
|       - | 2293 | `		/* Point to the next entry */` |
|      14 | 2294 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      14 | 2295 | `		n--;` |
|       2 | 2296 | `	}` |
|       - | 2297 | `	/* Return the freshly created array */` |
|       6 | 2298 | `	ph7_result_value(pCtx,pArray);` |
|       6 | 2299 | `	return PH7_OK;` |
|      17 | 2300 | `}` |
|       - | 2301 | `/*` |
|       - | 2302 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 2303 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 2304 | ` * Parameters` |
|       - | 2305 | ` *  $array1` |
|       - | 2306 | ` *    The array to compare from` |
|       - | 2307 | ` *  $array2` |
|       - | 2308 | ` *    An array to compare against` |
|       - | 2309 | ` *  $...` |
|       - | 2310 | ` *   More arrays to compare against` |
|       - | 2311 | ` * Return` |
|       - | 2312 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 2313 | ` *  are not present in any of the other arrays.` |
|       - | 2314 | ` */` |
|      20 | 2315 | `PH7_PRIVATE int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 2316 | `{` |
|       - | 2317 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 2318 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2319 | `	ph7_value *pArray;` |
|       - | 2320 | `	ph7_value *pVal;` |
|       - | 2321 | `	sxi32 rc;` |
|       - | 2322 | `	sxu32 n;` |
|       - | 2323 | `	int i;` |
|       - | 2324 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 2325 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 2326 | `	 * accompanying integration tests to pass. */` |
|      24 | 2327 | `	if( nArg < 1 ){` |
|     ! 0 | 2328 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2329 | `			"ArgumentCountError",` |
|       - | 2330 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|     ! 0 | 2331 | `			nArg` |
|       - | 2332 | `			);` |
|       - | 2333 | `	}` |
|      24 | 2334 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2335 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2336 | `			"TypeError",` |
|       - | 2337 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2338 | `			ph7_type_name(apArg[0])` |
|       - | 2339 | `			);` |
|       - | 2340 | `	}` |
|      37 | 2341 | `	for(i = 1 ; i < nArg ; i++){` |
|      23 | 2342 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       8 | 2343 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2344 | `				"TypeError",` |
|       - | 2345 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 2346 | `				i + 1,` |
|       4 | 2347 | `				ph7_type_name(apArg[i])` |
|       - | 2348 | `				);` |
|       - | 2349 | `		}` |
|      10 | 2350 | `	}` |
|      15 | 2351 | `	if( nArg == 1 ){` |
|       - | 2352 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 2353 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2354 | `		return PH7_OK;` |
|       - | 2355 | `	}` |
|       - | 2356 | `	/* Create a new array */` |
|      13 | 2357 | `	pArray = ph7_context_new_array(pCtx);` |
|      13 | 2358 | `	if( pArray == 0 ){` |
|     ! 0 | 2359 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2360 | `		return PH7_OK;` |
|       - | 2361 | `	}` |
|       - | 2362 | `	/* Point to the internal representation of the source hashmap */` |
|      13 | 2363 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2364 | `	/* Perform the diff */` |
|      13 | 2365 | `	pEntry = pSrc->pFirst;` |
|      13 | 2366 | `	n = pSrc->nEntry;` |
|      13 | 2367 | `	pN1 = pN2 = 0;` |
|      34 | 2368 | `	for(;;){` |
|       - | 2369 | `		int keep;` |
|      41 | 2370 | `		if( n < 1 ){` |
|      13 | 2371 | `			break;` |
|       - | 2372 | `		}` |
|       - | 2373 | `		/* assume the element should be kept until we find a match */` |
|      29 | 2374 | `		keep = 1;` |
|      47 | 2375 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 2376 | `			/* all arguments have been validated already, so cast directly */` |
|      33 | 2377 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2378 | `			/* Perform a key lookup first */` |
|      33 | 2379 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      13 | 2380 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       7 | 2381 | `			}else{` |
|      21 | 2382 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 2383 | `			}` |
|      33 | 2384 | `			if( rc != SXRET_OK ){` |
|       - | 2385 | `				/* this array does not contain the key, continue checking others */` |
|      17 | 2386 | `				continue;` |
|       - | 2387 | `			}` |
|       - | 2388 | `			/* key exists; check that value stored in the matching node is equal */` |
|      17 | 2389 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      17 | 2390 | `			if( pVal ){` |
|       - | 2391 | `				/* directly compare with value at pN1 rather than searching again */` |
|      17 | 2392 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      17 | 2393 | `				if( pVal2 ){` |
|       - | 2394 | `					ph7_value sV1,sV2;` |
|       - | 2395 | `					sxi32 cmp;` |
|       - | 2396 | `					/* Compare on duplicates: PH7_MemObjCmp converts its` |
|       - | 2397 | `					 * operands in place and these are LIVE array elements (a` |
|       - | 2398 | `					 * null element used to come back bool(false) in the` |
|       - | 2399 | `					 * caller's array). */` |
|      17 | 2400 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|      17 | 2401 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|      17 | 2402 | `					PH7_MemObjLoad(pVal,&sV1);` |
|      17 | 2403 | `					PH7_MemObjLoad(pVal2,&sV2);` |
|      17 | 2404 | `					cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|      17 | 2405 | `					PH7_MemObjRelease(&sV1);` |
|      17 | 2406 | `					PH7_MemObjRelease(&sV2);` |
|      17 | 2407 | `					if( cmp == 0 ){` |
|       - | 2408 | `						/* identical key+value found in one of the arrays => drop it */` |
|      15 | 2409 | `						keep = 0;` |
|      15 | 2410 | `						break;` |
|       - | 2411 | `					}` |
|       1 | 2412 | `				}` |
|       1 | 2413 | `			}` |
|       2 | 2414 | `		}` |
|      29 | 2415 | `		if( keep ){` |
|       - | 2416 | `			/* Perform the insertion */` |
|      15 | 2417 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       7 | 2418 | `		}` |
|       - | 2419 | `		/* Point to the next entry */` |
|      29 | 2420 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      29 | 2421 | `		n--;` |
|       1 | 2422 | `	}` |
|       - | 2423 | `	/* Return the freshly created array */` |
|      13 | 2424 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 2425 | `	return PH7_OK;` |
|      14 | 2426 | `}` |
|       - | 2427 | `/*` |
|       - | 2428 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 2429 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 2430 | ` *  by a user supplied callback function.` |
|       - | 2431 | ` * Parameters` |
|       - | 2432 | ` *  $array1` |
|       - | 2433 | ` *    The array to compare from` |
|       - | 2434 | ` *  $array2` |
|       - | 2435 | ` *    An array to compare against` |
|       - | 2436 | ` *  $...` |
|       - | 2437 | ` *   More arrays to compare against.` |
|       - | 2438 | ` *  $key_compare_func` |
|       - | 2439 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 2440 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 2441 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 2442 | ` * Return` |
|       - | 2443 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 2444 | ` *  are not present in any of the other arrays.` |
|       - | 2445 | ` */` |
|      24 | 2446 | `PH7_PRIVATE int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 2447 | `{` |
|       - | 2448 | `	ph7_hashmap_node *pEntry;` |
|       - | 2449 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2450 | `	ph7_value *pCallback;` |
|       - | 2451 | `	ph7_value *pArray;` |
|       - | 2452 | `	sxi32 rc;` |
|       - | 2453 | `	sxu32 n;` |
|       - | 2454 | `	int i;` |
|       - | 2455 |  |
|       - | 2456 | `	/* Argument validation mimicking PHP errors. */` |
|      28 | 2457 | `	if( nArg < 2 ){` |
|     ! 0 | 2458 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2459 | `			"ArgumentCountError",` |
|       - | 2460 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|     ! 0 | 2461 | `			nArg` |
|       - | 2462 | `			);` |
|       - | 2463 | `	}` |
|      28 | 2464 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2465 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2466 | `			"TypeError",` |
|       - | 2467 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2468 | `			ph7_type_name(apArg[0])` |
|       - | 2469 | `			);` |
|       - | 2470 | `	}` |
|       - | 2471 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 2472 | `	 * expected to be a callback. */` |
|       - | 2473 | `	/* php checks the CALLBACK before the intermediary arrays (see array_udiff). */` |
|      26 | 2474 | `	pCallback = apArg[nArg - 1];` |
|      26 | 2475 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       - | 2476 | `		/* Compose an error message that closely matches PHP output. When the` |
|       - | 2477 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|       - | 2478 | `		 * If the value is neither array nor string, PHP says "no array or` |
|       - | 2479 | `		 * string given" which we also reproduce. */` |
|      11 | 2480 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 2481 | `			/* ARRAY CALLBACK must have exactly two members */` |
|       4 | 2482 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2483 | `				"TypeError",` |
|       - | 2484 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 2485 | `				nArg` |
|       - | 2486 | `				);` |
|       - | 2487 | `		}` |
|       9 | 2488 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 2489 | `			/* A non-callable string names the function, like array_udiff. */` |
|       - | 2490 | `			int len;` |
|       3 | 2491 | `			const char *zName = ph7_value_to_string(pCallback,&len);` |
|       4 | 2492 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2493 | `				"TypeError",` |
|       - | 2494 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 2495 | `				nArg,` |
|       1 | 2496 | `				zName` |
|       - | 2497 | `				);` |
|       - | 2498 | `		}` |
|       - | 2499 | `		/* neither array nor string */` |
|       8 | 2500 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2501 | `			"TypeError",` |
|       - | 2502 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 2503 | `			nArg` |
|       - | 2504 | `			);` |
|       - | 2505 | `	}` |
|       - | 2506 | `	/* Now the intermediary arrays, left to right. */` |
|      28 | 2507 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      16 | 2508 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 2509 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2510 | `				"TypeError",` |
|       - | 2511 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 2512 | `				i + 1,` |
|       2 | 2513 | `				ph7_type_name(apArg[i])` |
|       - | 2514 | `				);` |
|       - | 2515 | `		}` |
|       7 | 2516 | `	}` |
|      13 | 2517 | `	if( nArg == 2 ){` |
|       - | 2518 | `		/* If we only have the first array and the callback, just return the` |
|       - | 2519 | `		 * input array. */` |
|       3 | 2520 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2521 | `		return PH7_OK;` |
|       - | 2522 | `	}` |
|       - | 2523 | `	/* Create a new array */` |
|      11 | 2524 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 2525 | `	if( pArray == 0 ){` |
|     ! 0 | 2526 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2527 | `		return PH7_OK;` |
|       - | 2528 | `	}` |
|       - | 2529 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 2530 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2531 | `	/* Perform the diff */` |
|      11 | 2532 | `	pEntry = pSrc->pFirst;` |
|      11 | 2533 | `	n = pSrc->nEntry;` |
|      21 | 2534 | `	for(;;){` |
|       - | 2535 | `		int keep;` |
|      27 | 2536 | `		if( n < 1 ){` |
|       9 | 2537 | `			break;` |
|       - | 2538 | `		}` |
|      19 | 2539 | `		keep = 1;` |
|      31 | 2540 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 2541 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      23 | 2542 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2543 | `			/* we must compare keys via callback, not by direct lookup */` |
|      23 | 2544 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      45 | 2545 | `			while( pIt ){` |
|       - | 2546 | `				/* build temporary key values for callback */` |
|       - | 2547 | `				ph7_value key1, key2, result;` |
|       - | 2548 | `				/* initialise only once using the appropriate helper */` |
|      33 | 2549 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 2550 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 2551 | `				}else{` |
|       - | 2552 | `					SyString sStr;` |
|      33 | 2553 | `					SyStringInitFromBuf(&sStr,` |
|       - | 2554 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 2555 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      33 | 2556 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 2557 | `				}` |
|      33 | 2558 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 2559 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 2560 | `				}else{` |
|       - | 2561 | `					SyString sStr;` |
|      33 | 2562 | `					SyStringInitFromBuf(&sStr,` |
|       - | 2563 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 2564 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      33 | 2565 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 2566 | `				}` |
|      33 | 2567 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 2568 | `				/* call user callback with (key1, key2) */` |
|       - | 2569 | `				{` |
|       - | 2570 | `					ph7_value *apK[2];` |
|      33 | 2571 | `					apK[0] = &key1;` |
|      33 | 2572 | `					apK[1] = &key2;` |
|      33 | 2573 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 2574 | `				}` |
|      33 | 2575 | `				if( rc == PH7_EXCEPTION ){` |
|       - | 2576 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|       - | 2577 | `					 * array_uintersect (which signal back from` |
|       - | 2578 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|       - | 2579 | `					 * function invokes the callback inline, so it cleans up its own` |
|       - | 2580 | `					 * temporaries and propagates the exception directly. */` |
|       3 | 2581 | `					PH7_MemObjRelease(&result);` |
|       3 | 2582 | `					PH7_MemObjRelease(&key1);` |
|       3 | 2583 | `					PH7_MemObjRelease(&key2);` |
|       3 | 2584 | `					return PH7_EXCEPTION;` |
|       - | 2585 | `				}` |
|      31 | 2586 | `				if( rc == SXRET_OK ){` |
|      31 | 2587 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 2588 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 2589 | `					}` |
|      31 | 2590 | `					if( result.x.iVal == 0 ){` |
|       - | 2591 | `						/* keys considered equal by callback; now compare values */` |
|      13 | 2592 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      13 | 2593 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      13 | 2594 | `						if( pVal1 && pVal2 ){` |
|       - | 2595 | `							ph7_value sV1,sV2;` |
|       - | 2596 | `							sxi32 cmp;` |
|       - | 2597 | `							/* Compare on duplicates: PH7_MemObjCmp converts in` |
|       - | 2598 | `							 * place and these are LIVE array elements. */` |
|      13 | 2599 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|      13 | 2600 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|      13 | 2601 | `							PH7_MemObjLoad(pVal1,&sV1);` |
|      13 | 2602 | `							PH7_MemObjLoad(pVal2,&sV2);` |
|      13 | 2603 | `							cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|      13 | 2604 | `							PH7_MemObjRelease(&sV1);` |
|      13 | 2605 | `							PH7_MemObjRelease(&sV2);` |
|      13 | 2606 | `							if( cmp == 0 ){` |
|       9 | 2607 | `								keep = 0;` |
|       9 | 2608 | `								PH7_MemObjRelease(&result);` |
|       - | 2609 | `								/* release keys too before breaking */` |
|       9 | 2610 | `								PH7_MemObjRelease(&key1);` |
|       9 | 2611 | `								PH7_MemObjRelease(&key2);` |
|       9 | 2612 | `								break;` |
|       - | 2613 | `							}` |
|       2 | 2614 | `						}` |
|       2 | 2615 | `					}` |
|      11 | 2616 | `				}` |
|      23 | 2617 | `				PH7_MemObjRelease(&result);` |
|      23 | 2618 | `				PH7_MemObjRelease(&key1);` |
|      23 | 2619 | `				PH7_MemObjRelease(&key2);` |
|       - | 2620 | `				/* move to next node */` |
|      23 | 2621 | `				pIt = pIt->pPrev;` |
|      23 | 2622 | `				if( keep == 0 ) break;` |
|       1 | 2623 | `			}` |
|      21 | 2624 | `			if( keep == 0 ) break;` |
|       7 | 2625 | `		}` |
|      17 | 2626 | `		if( keep ){` |
|       - | 2627 | `			/* Perform the insertion */` |
|       9 | 2628 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 2629 | `		}` |
|       - | 2630 | `		/* Point to the next entry */` |
|      17 | 2631 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      17 | 2632 | `		n--;` |
|       1 | 2633 | `	}` |
|       - | 2634 | `	/* Return the freshly created array */` |
|       9 | 2635 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 2636 | `	return PH7_OK;` |
|      16 | 2637 | `}` |
|       - | 2638 | `/*` |
|       - | 2639 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 2640 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 2641 | ` * Parameters` |
|       - | 2642 | ` *  $array1` |
|       - | 2643 | ` *    The array to compare from` |
|       - | 2644 | ` *  $array2` |
|       - | 2645 | ` *    An array to compare against` |
|       - | 2646 | ` *  $...` |
|       - | 2647 | ` *   More arrays to compare against` |
|       - | 2648 | ` * Return` |
|       - | 2649 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 2650 | ` *  in any of the other arrays.` |
|       - | 2651 | ` * Note that NULL is returned on failure.` |
|       - | 2652 | ` */` |
|      12 | 2653 | `PH7_PRIVATE int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2654 | `{` |
|       - | 2655 | `	ph7_hashmap_node *pEntry;` |
|       - | 2656 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2657 | `	ph7_value *pArray;` |
|       - | 2658 | `	sxi32 rc;` |
|       - | 2659 | `	sxu32 n;` |
|       - | 2660 | `	int i;` |
|       - | 2661 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 2662 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 2663 | `	 * helpers. */` |
|      15 | 2664 | `	if( nArg < 1 ){` |
|     ! 0 | 2665 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2666 | `			"ArgumentCountError",` |
|       - | 2667 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|     ! 0 | 2668 | `			nArg` |
|       - | 2669 | `			);` |
|       - | 2670 | `	}` |
|      15 | 2671 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2672 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2673 | `			"TypeError",` |
|       - | 2674 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2675 | `			ph7_type_name(apArg[0])` |
|       - | 2676 | `			);` |
|       - | 2677 | `	}` |
|      20 | 2678 | `	for(i = 1 ; i < nArg ; i++){` |
|      12 | 2679 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 2680 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2681 | `				"TypeError",` |
|       - | 2682 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 2683 | `				i + 1,` |
|       2 | 2684 | `				ph7_type_name(apArg[i])` |
|       - | 2685 | `				);` |
|       - | 2686 | `		}` |
|       5 | 2687 | `	}` |
|       9 | 2688 | `	if( nArg == 1 ){` |
|       - | 2689 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 2690 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2691 | `		return PH7_OK;` |
|       - | 2692 | `	}` |
|       - | 2693 | `	/* Create a new array */` |
|       7 | 2694 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 2695 | `	if( pArray == 0 ){` |
|     ! 0 | 2696 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2697 | `		return PH7_OK;` |
|       - | 2698 | `	}` |
|       - | 2699 | `	/* Point to the internal representation of the main hashmap */` |
|       7 | 2700 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2701 | `	/* Perfrom the diff */` |
|       7 | 2702 | `	pEntry = pSrc->pFirst;` |
|       7 | 2703 | `	n = pSrc->nEntry;` |
|      12 | 2704 | `	for(;;){` |
|      25 | 2705 | `		if( n < 1 ){` |
|       7 | 2706 | `			break;` |
|       - | 2707 | `		}` |
|      31 | 2708 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 2709 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 2710 | `				/* ignore */` |
|     ! 0 | 2711 | `				continue;` |
|       - | 2712 | `			}` |
|      23 | 2713 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      23 | 2714 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      17 | 2715 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 2716 | `				/* Blob lookup */` |
|      17 | 2717 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       9 | 2718 | `			}else{` |
|       - | 2719 | `				/* Int lookup */` |
|       7 | 2720 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 2721 | `			}` |
|      23 | 2722 | `			if( rc == SXRET_OK ){` |
|       - | 2723 | `				/* Key exists,break immediately */` |
|      11 | 2724 | `				break;` |
|       - | 2725 | `			}` |
|       7 | 2726 | `		}` |
|      19 | 2727 | `		if( i >= nArg ){` |
|       - | 2728 | `			/* Perform the insertion */` |
|       9 | 2729 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 2730 | `		}` |
|       - | 2731 | `		/* Point to the next entry */` |
|      19 | 2732 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 2733 | `		n--;` |
|       1 | 2734 | `	}` |
|       - | 2735 | `	/* Return the freshly created array */` |
|       7 | 2736 | `	ph7_result_value(pCtx,pArray);` |
|       7 | 2737 | `	return PH7_OK;` |
|       9 | 2738 | `}` |
|       - | 2739 | `/*` |
|       - | 2740 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 2741 | ` *  Computes the intersection of arrays.` |
|       - | 2742 | ` * Parameters` |
|       - | 2743 | ` *  $array1` |
|       - | 2744 | ` *    The array to compare from` |
|       - | 2745 | ` *  $array2` |
|       - | 2746 | ` *    An array to compare against` |
|       - | 2747 | ` *  $...` |
|       - | 2748 | ` *   More arrays to compare against` |
|       - | 2749 | ` * Return` |
|       - | 2750 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 2751 | ` *  in all of the parameters.` |
|       - | 2752 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 2753 | ` * Throws TypeError if any argument is not an array.` |
|       - | 2754 | ` */` |
|      20 | 2755 | `PH7_PRIVATE int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2756 | `{` |
|       - | 2757 | `	ph7_hashmap_node *pEntry;` |
|       - | 2758 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2759 | `	ph7_value *pArray;` |
|       - | 2760 | `	ph7_value *pVal;` |
|       - | 2761 | `	sxi32 rc;` |
|       - | 2762 | `	sxu32 n;` |
|       - | 2763 | `	int i;` |
|      23 | 2764 | `	if( nArg < 1 ){` |
|     ! 0 | 2765 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2766 | `			"ArgumentCountError",` |
|       - | 2767 | `			"array_intersect() expects at least 1 argument, %d given",` |
|     ! 0 | 2768 | `			nArg` |
|       - | 2769 | `			);` |
|       - | 2770 | `	}` |
|      23 | 2771 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2772 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2773 | `			"TypeError",` |
|       - | 2774 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2775 | `			ph7_type_name(apArg[0])` |
|       - | 2776 | `			);` |
|       - | 2777 | `	}` |
|      36 | 2778 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 2779 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 2780 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2781 | `				"TypeError",` |
|       - | 2782 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       1 | 2783 | `				i + 1,` |
|       2 | 2784 | `				ph7_type_name(apArg[i])` |
|       - | 2785 | `				);` |
|       - | 2786 | `		}` |
|       9 | 2787 | `	}` |
|      17 | 2788 | `	if( nArg == 1 ){` |
|       - | 2789 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 2790 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2791 | `		return PH7_OK;` |
|       - | 2792 | `	}` |
|       - | 2793 | `	/* Create a new array */` |
|      15 | 2794 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 2795 | `	if( pArray == 0 ){` |
|     ! 0 | 2796 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2797 | `		return PH7_OK;` |
|       - | 2798 | `	}` |
|       - | 2799 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 2800 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2801 | `	/* Perform the intersection */` |
|      15 | 2802 | `	pEntry = pSrc->pFirst;` |
|      15 | 2803 | `	n = pSrc->nEntry;` |
|      31 | 2804 | `	for(;;){` |
|      63 | 2805 | `		if( n < 1 ){` |
|      15 | 2806 | `			break;` |
|       - | 2807 | `		}` |
|       - | 2808 | `		/* Extract the node value */` |
|      49 | 2809 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      49 | 2810 | `		if( pVal ){` |
|      79 | 2811 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 2812 | `				/* Point to the internal representation of the hashmap */` |
|      55 | 2813 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2814 | `				/* Perform the lookup */` |
|      55 | 2815 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      55 | 2816 | `				if( rc != SXRET_OK ){` |
|       - | 2817 | `					/* Value does not exist */` |
|      25 | 2818 | `					break;` |
|       - | 2819 | `				}` |
|      16 | 2820 | `			}` |
|      49 | 2821 | `			if( i >= nArg ){` |
|       - | 2822 | `				/* Perform the insertion */` |
|      25 | 2823 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 2824 | `			}` |
|      24 | 2825 | `		}` |
|       - | 2826 | `		/* Point to the next entry */` |
|      49 | 2827 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      49 | 2828 | `		n--;` |
|       1 | 2829 | `	}` |
|       - | 2830 | `	/* Return the freshly created array */` |
|      15 | 2831 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 2832 | `	return PH7_OK;` |
|      13 | 2833 | `}` |
|       - | 2834 | `/*` |
|       - | 2835 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 2836 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 2837 | ` * Parameters` |
|       - | 2838 | ` *  $array1` |
|       - | 2839 | ` *    The array to compare from` |
|       - | 2840 | ` *  $array2` |
|       - | 2841 | ` *    An array to compare against` |
|       - | 2842 | ` *  $...` |
|       - | 2843 | ` *   More arrays to compare against` |
|       - | 2844 | ` * Return` |
|       - | 2845 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 2846 | ` *  in all the arguments, with matching keys.` |
|       - | 2847 | ` */` |
|      20 | 2848 | `PH7_PRIVATE int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2849 | `{` |
|       - | 2850 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 2851 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2852 | `	ph7_value *pArray;` |
|       - | 2853 | `	ph7_value *pVal;` |
|       - | 2854 | `	sxi32 rc;` |
|       - | 2855 | `	sxu32 n;` |
|       - | 2856 | `	int i;` |
|      23 | 2857 | `	if( nArg < 1 ){` |
|     ! 0 | 2858 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2859 | `			"ArgumentCountError",` |
|       - | 2860 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|     ! 0 | 2861 | `			nArg` |
|       - | 2862 | `			);` |
|       - | 2863 | `	}` |
|      23 | 2864 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2865 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2866 | `			"TypeError",` |
|       - | 2867 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2868 | `			ph7_type_name(apArg[0])` |
|       - | 2869 | `			);` |
|       - | 2870 | `	}` |
|      36 | 2871 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 2872 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 2873 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2874 | `				"TypeError",` |
|       - | 2875 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 2876 | `				i + 1,` |
|       2 | 2877 | `				ph7_type_name(apArg[i])` |
|       - | 2878 | `				);` |
|       - | 2879 | `		}` |
|       9 | 2880 | `	}` |
|      17 | 2881 | `	if( nArg == 1 ){` |
|       - | 2882 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 2883 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2884 | `		return PH7_OK;` |
|       - | 2885 | `	}` |
|       - | 2886 | `	/* Create a new array */` |
|      15 | 2887 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 2888 | `	if( pArray == 0 ){` |
|     ! 0 | 2889 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2890 | `		return PH7_OK;` |
|       - | 2891 | `	}` |
|       - | 2892 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 2893 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2894 | `	/* Perform the intersection */` |
|      15 | 2895 | `	pEntry = pSrc->pFirst;` |
|      15 | 2896 | `	n = pSrc->nEntry;` |
|      15 | 2897 | `	pN1 = pN2 = 0; /* cc warning */` |
|      23 | 2898 | `	for(;;){` |
|      47 | 2899 | `		if( n < 1 ){` |
|      15 | 2900 | `			break;` |
|       - | 2901 | `		}` |
|       - | 2902 | `		/* Extract the node value */` |
|      33 | 2903 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      33 | 2904 | `		if( pVal ){` |
|      53 | 2905 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 2906 | `				/* Point to the internal representation of the hashmap */` |
|      37 | 2907 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2908 | `				/* Perform a key lookup first */` |
|      37 | 2909 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      15 | 2910 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       8 | 2911 | `				}else{` |
|      23 | 2912 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 2913 | `				}` |
|      37 | 2914 | `				if( rc != SXRET_OK ){` |
|       - | 2915 | `					/* No such key,break immediately */` |
|       7 | 2916 | `					break;` |
|       - | 2917 | `				}` |
|       - | 2918 | `				/* Perform the lookup */` |
|      31 | 2919 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      31 | 2920 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|       - | 2921 | `					/* Value does not exist */` |
|       6 | 2922 | `					break;` |
|       - | 2923 | `				}` |
|      11 | 2924 | `			}` |
|      33 | 2925 | `			if( i >= nArg ){` |
|       - | 2926 | `				/* Perform the insertion */` |
|      17 | 2927 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 2928 | `			}` |
|      16 | 2929 | `		}` |
|       - | 2930 | `		/* Point to the next entry */` |
|      33 | 2931 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 2932 | `		n--;` |
|       1 | 2933 | `	}` |
|       - | 2934 | `	/* Return the freshly created array */` |
|      15 | 2935 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 2936 | `	return PH7_OK;` |
|      13 | 2937 | `}` |
|       - | 2938 | `/*` |
|       - | 2939 | ` * array array_intersect_key(array $array1 ,...)` |
|       - | 2940 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 2941 | ` * Parameters` |
|       - | 2942 | ` *  $array1` |
|       - | 2943 | ` *    The array to compare from` |
|       - | 2944 | ` *  $...` |
|       - | 2945 | ` *   More arrays to compare against` |
|       - | 2946 | ` * Return` |
|       - | 2947 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 2948 | ` *  have keys that are present in all arguments.` |
|       - | 2949 | ` * Note that NULL is returned on failure.` |
|       - | 2950 | ` */` |
|      20 | 2951 | `PH7_PRIVATE int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2952 | `{` |
|       - | 2953 | `	ph7_hashmap_node *pEntry;` |
|       - | 2954 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2955 | `	ph7_value *pArray;` |
|       - | 2956 | `	sxi32 rc;` |
|       - | 2957 | `	sxu32 n;` |
|       - | 2958 | `	int i;` |
|      23 | 2959 | `	if( nArg < 1 ){` |
|     ! 0 | 2960 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2961 | `			"ArgumentCountError",` |
|       - | 2962 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|     ! 0 | 2963 | `			nArg` |
|       - | 2964 | `			);` |
|       - | 2965 | `	}` |
|      23 | 2966 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2967 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2968 | `			"TypeError",` |
|       - | 2969 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2970 | `			ph7_type_name(apArg[0])` |
|       - | 2971 | `			);` |
|       - | 2972 | `	}` |
|      36 | 2973 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 2974 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 2975 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2976 | `				"TypeError",` |
|       - | 2977 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|       1 | 2978 | `				i + 1,` |
|       2 | 2979 | `				ph7_type_name(apArg[i])` |
|       - | 2980 | `				);` |
|       - | 2981 | `		}` |
|       9 | 2982 | `	}` |
|      17 | 2983 | `	if( nArg == 1 ){` |
|       - | 2984 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 2985 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2986 | `		return PH7_OK;` |
|       - | 2987 | `	}` |
|       - | 2988 | `	/* Create a new array */` |
|      15 | 2989 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 2990 | `	if( pArray == 0 ){` |
|     ! 0 | 2991 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2992 | `		return PH7_OK;` |
|       - | 2993 | `	}` |
|       - | 2994 | `	/* Point to the internal representation of the main hashmap */` |
|      15 | 2995 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2996 | `	/* Perform the intersection */` |
|      15 | 2997 | `	pEntry = pSrc->pFirst;` |
|      15 | 2998 | `	n = pSrc->nEntry;` |
|      24 | 2999 | `	for(;;){` |
|      49 | 3000 | `		if( n < 1 ){` |
|      15 | 3001 | `			break;` |
|       - | 3002 | `		}` |
|      57 | 3003 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      39 | 3004 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      39 | 3005 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      27 | 3006 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 3007 | `				/* Blob lookup */` |
|      27 | 3008 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      14 | 3009 | `			}else{` |
|       - | 3010 | `				/* Int key */` |
|      13 | 3011 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 3012 | `			}` |
|      39 | 3013 | `			if( rc != SXRET_OK ){` |
|       - | 3014 | `				/* Key does not exist, break immediately */` |
|      17 | 3015 | `				break;` |
|       - | 3016 | `			}` |
|      12 | 3017 | `		}` |
|      35 | 3018 | `		if( i >= nArg ){` |
|       - | 3019 | `			/* Perform the insertion */` |
|      19 | 3020 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       9 | 3021 | `		}` |
|       - | 3022 | `		/* Point to the next entry */` |
|      35 | 3023 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      35 | 3024 | `		n--;` |
|       1 | 3025 | `	}` |
|       - | 3026 | `	/* Return the freshly created array */` |
|      15 | 3027 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 3028 | `	return PH7_OK;` |
|      13 | 3029 | `}` |
|       - | 3030 | `/*` |
|       - | 3031 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 3032 | ` *  Computes the intersection of arrays.` |
|       - | 3033 | ` * Parameters` |
|       - | 3034 | ` *  $array1` |
|       - | 3035 | ` *    The array to compare from` |
|       - | 3036 | ` *  $array2` |
|       - | 3037 | ` *    An array to compare against` |
|       - | 3038 | ` *  $...` |
|       - | 3039 | ` *   More arrays to compare against` |
|       - | 3040 | ` * $callback` |
|       - | 3041 | ` *  The callback comparison function.` |
|       - | 3042 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 3043 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 3044 | ` *  than the second.` |
|       - | 3045 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 3046 | ` * Return` |
|       - | 3047 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 3048 | ` *  in all of the parameters. .` |
|       - | 3049 | ` * Note that NULL is returned on failure.` |
|       - | 3050 | ` */` |
|      26 | 3051 | `PH7_PRIVATE int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3052 | `{` |
|       - | 3053 | `	ph7_hashmap_node *pEntry;` |
|       - | 3054 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3055 | `	ph7_value *pCallback;` |
|       - | 3056 | `	ph7_value *pArray;` |
|       - | 3057 | `	ph7_value *pVal;` |
|       - | 3058 | `	sxi32 rc;` |
|       - | 3059 | `	sxu32 n;` |
|       - | 3060 | `	int i;` |
|       - | 3061 |  |
|       - | 3062 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      31 | 3063 | `	if( nArg < 2 ){` |
|     ! 0 | 3064 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3065 | `			"ArgumentCountError",` |
|       - | 3066 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|     ! 0 | 3067 | `			nArg` |
|       - | 3068 | `			);` |
|       - | 3069 | `	}` |
|      31 | 3070 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3071 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3072 | `			"TypeError",` |
|       - | 3073 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3074 | `			ph7_type_name(apArg[0])` |
|       - | 3075 | `			);` |
|       - | 3076 | `	}` |
|       - | 3077 |  |
|       - | 3078 | `	/* php checks the CALLBACK before the intermediary arrays (see array_udiff). */` |
|      29 | 3079 | `	pCallback = apArg[nArg - 1];` |
|      29 | 3080 | `	if( !ph7_value_is_callable(pCallback) ){` |
|      16 | 3081 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 3082 | `			/* PHP emits a special message when the array length is wrong.` |
|       - | 3083 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|       - | 3084 | `			 * method / missing class), we must emit a more general error instead.` |
|       - | 3085 | `			 */` |
|       9 | 3086 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|       9 | 3087 | `			if( pCb->nEntry != 2 ){` |
|       4 | 3088 | `				return PH7_VmThrowException(pCtx,` |
|       - | 3089 | `					"TypeError",` |
|       - | 3090 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 3091 | `					nArg` |
|       - | 3092 | `					);` |
|       - | 3093 | `			}` |
|       - | 3094 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|       - | 3095 | `			{` |
|       6 | 3096 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|       6 | 3097 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|       6 | 3098 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|       - | 3099 | `					int nMethodLen;` |
|       6 | 3100 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|       6 | 3101 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|       6 | 3102 | `					if( pClass ){` |
|       - | 3103 | `						/* Class exists but method is missing. */` |
|       4 | 3104 | `						return PH7_VmThrowException(pCtx,` |
|       - | 3105 | `							"TypeError",` |
|       - | 3106 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|       1 | 3107 | `							nArg,` |
|       1 | 3108 | `							(const char *)SyStringData(&pClass->sName),` |
|       1 | 3109 | `							zMethod` |
|       - | 3110 | `							);` |
|       - | 3111 | `					}` |
|       - | 3112 | `					/* Class not found */` |
|       - | 3113 | `					{` |
|       - | 3114 | `						int nName;` |
|       3 | 3115 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|       4 | 3116 | `						return PH7_VmThrowException(pCtx,` |
|       - | 3117 | `							"TypeError",` |
|       - | 3118 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|       1 | 3119 | `							nArg,` |
|       1 | 3120 | `							zName` |
|       - | 3121 | `							);` |
|       - | 3122 | `					}` |
|       - | 3123 | `				}` |
|       - | 3124 | `			}` |
|       - | 3125 | `			/* Fallback message */` |
|     ! 0 | 3126 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3127 | `				"TypeError",` |
|       - | 3128 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|     ! 0 | 3129 | `				nArg` |
|       - | 3130 | `				);` |
|       - | 3131 | `		}` |
|       8 | 3132 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 3133 | `			int len;` |
|       6 | 3134 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       8 | 3135 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3136 | `				"TypeError",` |
|       - | 3137 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       2 | 3138 | `				nArg,` |
|       2 | 3139 | `				zName` |
|       - | 3140 | `				);` |
|       - | 3141 | `		}` |
|       4 | 3142 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3143 | `			"TypeError",` |
|       - | 3144 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 3145 | `			nArg` |
|       - | 3146 | `			);` |
|       - | 3147 | `	}` |
|       - | 3148 |  |
|       - | 3149 | `	/* Now the intermediary arrays, left to right. */` |
|      20 | 3150 | `	for( i = 1 ; i < nArg - 1; i++ ){` |
|      10 | 3151 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3152 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3153 | `				"TypeError",` |
|       - | 3154 | `				"array_uintersect(): Argument #%d must be of type array, %s given",` |
|       1 | 3155 | `				i + 1,` |
|       2 | 3156 | `				ph7_type_name(apArg[i])` |
|       - | 3157 | `				);` |
|       - | 3158 | `		}` |
|       4 | 3159 | `	}` |
|       - | 3160 |  |
|      11 | 3161 | `	if( nArg == 2 ){` |
|       - | 3162 | `		/* Only the original array and the callback were provided. */` |
|       5 | 3163 | `		ph7_result_value(pCtx,apArg[0]);` |
|       5 | 3164 | `		return PH7_OK;` |
|       - | 3165 | `	}` |
|       - | 3166 |  |
|       - | 3167 | `	/* Create a new array */` |
|       7 | 3168 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 3169 | `	if( pArray == 0 ){` |
|     ! 0 | 3170 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3171 | `		return PH7_OK;` |
|       - | 3172 | `	}` |
|       - | 3173 | `	/* Point to the internal representation of the source hashmap */` |
|       7 | 3174 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3175 | `	/* Perform the intersection */` |
|       7 | 3176 | `	pEntry = pSrc->pFirst;` |
|       7 | 3177 | `	n = pSrc->nEntry;` |
|       7 | 3178 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|       9 | 3179 | `	for(;;){` |
|      19 | 3180 | `		if( n < 1 ){` |
|       5 | 3181 | `			break;` |
|       - | 3182 | `		}` |
|       - | 3183 | `		/* Extract the node value */` |
|      15 | 3184 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 3185 | `		if( pVal ){` |
|      23 | 3186 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|      15 | 3187 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 3188 | `					/* ignore */` |
|     ! 0 | 3189 | `					continue;` |
|       - | 3190 | `				}` |
|       - | 3191 | `				/* Point to the internal representation of the hashmap */` |
|      15 | 3192 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3193 | `				/* Perform the lookup */` |
|      15 | 3194 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|      15 | 3195 | `				if( rc != SXRET_OK ){` |
|       - | 3196 | `					/* Value does not exist */` |
|       7 | 3197 | `					break;` |
|       - | 3198 | `				}` |
|       5 | 3199 | `			}` |
|      15 | 3200 | `			if( i >= (nArg-1) ){` |
|       - | 3201 | `				/* Perform the insertion */` |
|       9 | 3202 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 3203 | `			}` |
|       7 | 3204 | `		}` |
|      15 | 3205 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 3206 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 3207 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 3208 | `			return PH7_EXCEPTION;` |
|       - | 3209 | `		}` |
|       - | 3210 | `		/* Point to the next entry */` |
|      13 | 3211 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      13 | 3212 | `		n--;` |
|       1 | 3213 | `	}` |
|       - | 3214 | `	/* Return the freshly created array */` |
|       5 | 3215 | `	ph7_result_value(pCtx,pArray);` |
|       5 | 3216 | `	return PH7_OK;` |
|      18 | 3217 | `}` |
|       - | 3218 | `/*` |
|       - | 3219 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 3220 | ` *  Fill an array with values.` |
|       - | 3221 | ` * Parameters` |
|       - | 3222 | ` *  $start_index` |
|       - | 3223 | ` *    The first index of the returned array.` |
|       - | 3224 | ` *  $num` |
|       - | 3225 | ` *   Number of elements to insert.` |
|       - | 3226 | ` *  $value` |
|       - | 3227 | ` *    Value to use for filling.` |
|       - | 3228 | ` * Return` |
|       - | 3229 | ` *  The filled array or null on failure.` |
|       - | 3230 | ` */` |
|     234 | 3231 | `PH7_PRIVATE int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 3232 | `{` |
|       - | 3233 | `	ph7_value *pArray;` |
|       - | 3234 | `	int i,nEntry;` |
|       - | 3235 |  |
|       - | 3236 | `	/* PHP enforces argument count and type checks. */` |
|     238 | 3237 | `	if( nArg != 3 ){` |
|       - | 3238 | `		/* wrong number of arguments -> ArgumentCountError */` |
|     ! 0 | 3239 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3240 | `			"ArgumentCountError",` |
|       - | 3241 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|     ! 0 | 3242 | `			nArg` |
|       - | 3243 | `			);` |
|       - | 3244 | `	}` |
|       - | 3245 |  |
|       - | 3246 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 3247 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 3248 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 3249 | `	 * and NULLs are rejected outright. */` |
|     351 | 3250 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     355 | 3251 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|     ! 0 | 3252 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3253 | `			"TypeError",` |
|       - | 3254 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|     ! 0 | 3255 | `			ph7_type_name(apArg[0])` |
|       - | 3256 | `			);` |
|       - | 3257 | `	}` |
|     238 | 3258 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 3259 | `		int len;` |
|       6 | 3260 | `		sxu8 bReal = FALSE;` |
|       6 | 3261 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       6 | 3262 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 3263 | `			/* Non‑numeric string is an error. */` |
|       3 | 3264 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3265 | `				"TypeError",` |
|       - | 3266 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 3267 | `				);` |
|       - | 3268 | `		}` |
|       3 | 3269 | `		if( bReal ){` |
|       - | 3270 | `			/* float-string -> deprecation warning */` |
|     ! 0 | 3271 | `			PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 3272 | `				"Implicit conversion from float-string to int loses precision");` |
|     ! 0 | 3273 | `		}` |
|       1 | 3274 | `	}` |
|       - | 3275 |  |
|       - | 3276 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 3277 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     348 | 3278 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     352 | 3279 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 3280 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3281 | `			"TypeError",` |
|       - | 3282 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 3283 | `			ph7_type_name(apArg[1])` |
|       - | 3284 | `			);` |
|       - | 3285 | `	}` |
|     236 | 3286 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 3287 | `		int len;` |
|       3 | 3288 | `		sxu8 bReal = FALSE;` |
|       3 | 3289 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 3290 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 3291 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3292 | `				"TypeError",` |
|       - | 3293 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 3294 | `				);` |
|       - | 3295 | `		}` |
|     ! 0 | 3296 | `	}` |
|       - | 3297 | `	/* Note: booleans and floats (including fractional) are now accepted; they` |
|       - | 3298 | `	 * will be converted by ph7_value_to_int below. */` |
|     233 | 3299 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 3300 | `		double d = ph7_value_to_double(apArg[1]);` |
|       - | 3301 | `		/* avoid hiding outer 'i' (loop index) */` |
|       3 | 3302 | `		sxi64 i64 = (sxi64)d;` |
|       3 | 3303 | `		if( d != (double)i64 ){` |
|       3 | 3304 | `			PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 3305 | `				"Implicit conversion from float to int loses precision");` |
|       1 | 3306 | `		}` |
|       1 | 3307 | `	}` |
|       - | 3308 |  |
|       - | 3309 | `	/* Total number of entries to insert */` |
|     233 | 3310 | `	nEntry = ph7_value_to_int(apArg[1]);` |
|       - | 3311 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     233 | 3312 | `	if( nEntry < 0 ){` |
|       3 | 3313 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3314 | `			"ValueError",` |
|       - | 3315 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 3316 | `			);` |
|       - | 3317 | `	}` |
|       - | 3318 |  |
|       - | 3319 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     230 | 3320 | `	if( nEntry == 0 ){` |
|       5 | 3321 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       5 | 3322 | `		return PH7_OK;` |
|       - | 3323 | `	}` |
|       - | 3324 |  |
|       - | 3325 | `	/* Create a new array */` |
|     226 | 3326 | `	pArray = ph7_context_new_array(pCtx);` |
|     226 | 3327 | `	if( pArray == 0 ){` |
|     ! 0 | 3328 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 3329 | `	}` |
|       - | 3330 |  |
|       - | 3331 | `	/* PHP 8 fills consecutive integer keys start_index, start_index+1, … even` |
|       - | 3332 | `	 * when start_index is negative (PHP 7 restarted the remaining keys from 0,` |
|       - | 3333 | `	 * so array_fill(-5,3) gave -5,0,1 instead of -5,-4,-3). Assign each key` |
|       - | 3334 | `	 * explicitly rather than relying on automatic (append) indexing. */` |
|     226 | 3335 | `	int iStart = ph7_value_to_int(apArg[0]);` |
| 2117826 | 3336 | `	for( i = 0 ; i < nEntry ; i++ ){` |
| 2117602 | 3337 | `		if( ph7_array_add_intkey_elem(pArray, iStart + i, apArg[2]) != SXRET_OK ){` |
|       - | 3338 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|     ! 0 | 3339 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 3340 | `		}` |
| 1058802 | 3341 | `	}` |
|       - | 3342 | `	/* Return the filled array */` |
|     226 | 3343 | `	ph7_result_value(pCtx, pArray);` |
|     226 | 3344 | `	return PH7_OK;` |
|     121 | 3345 | `}` |
|       - | 3346 | `/*` |
|       - | 3347 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 3348 | ` *  Fill an array with values, specifying keys.` |
|       - | 3349 | ` * Parameters` |
|       - | 3350 | ` *  $input` |
|       - | 3351 | ` *   Array of values that will be used as key.` |
|       - | 3352 | ` *  $value` |
|       - | 3353 | ` *    Value to use for filling.` |
|       - | 3354 | ` * Return` |
|       - | 3355 | ` *  The filled array.` |
|       - | 3356 | ` * Throws` |
|       - | 3357 | ` *  ValueError if $input is not an array.` |
|       - | 3358 | ` */` |
|      20 | 3359 | `PH7_PRIVATE int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3360 | `{` |
|       - | 3361 | `	ph7_hashmap_node *pEntry;` |
|       - | 3362 | `	ph7_hashmap *pSrc;` |
|       - | 3363 | `	ph7_value *pArray;` |
|       - | 3364 | `	sxu32 n;` |
|       - | 3365 | `	/* PHP enforces exactly 2 arguments. */` |
|      23 | 3366 | `	if( nArg != 2 ){` |
|     ! 0 | 3367 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3368 | `			"ArgumentCountError",` |
|       - | 3369 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|     ! 0 | 3370 | `			nArg` |
|       - | 3371 | `			);` |
|       - | 3372 | `	}` |
|       - | 3373 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 3374 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       8 | 3375 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3376 | `			"TypeError",` |
|       - | 3377 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|       2 | 3378 | `			ph7_type_name(apArg[0])` |
|       - | 3379 | `			);` |
|       - | 3380 | `	}` |
|       - | 3381 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 3382 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3383 | `	/* Create a new array */` |
|      17 | 3384 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 3385 | `	if( pArray == 0 ){` |
|     ! 0 | 3386 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3387 | `		return PH7_OK;` |
|       - | 3388 | `	}` |
|       - | 3389 | `	/* Perform the requested operation */` |
|      17 | 3390 | `	pEntry = pSrc->pFirst;` |
|      45 | 3391 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      29 | 3392 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|       - | 3393 | `		/* Point to the next entry */` |
|      29 | 3394 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 3395 | `	}` |
|       - | 3396 | `	/* Return the filled array */` |
|      17 | 3397 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 3398 | `	return PH7_OK;` |
|      13 | 3399 | `}` |
|       - | 3400 | `/*` |
|       - | 3401 | ` * array array_combine(array $keys,array $values)` |
|       - | 3402 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 3403 | ` * Parameters` |
|       - | 3404 | ` *  $keys` |
|       - | 3405 | ` *    Array of keys to be used.` |
|       - | 3406 | ` * $values` |
|       - | 3407 | ` *   Array of values to be used.` |
|       - | 3408 | ` * Return` |
|       - | 3409 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 3410 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 3411 | ` *  not an array.` |
|       - | 3412 | ` */` |
|      16 | 3413 | `PH7_PRIVATE int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 3414 | `{` |
|       - | 3415 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 3416 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 3417 | `	ph7_value *pArray;` |
|       - | 3418 | `	sxu32 n;` |
|       - | 3419 | `	/* PHP enforces argument count and type checks. */` |
|      20 | 3420 | `	if( nArg != 2 ){` |
|       - | 3421 | `		/* wrong number of arguments -> ArgumentCountError */` |
|     ! 0 | 3422 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3423 | `			"ArgumentCountError",` |
|       - | 3424 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|     ! 0 | 3425 | `			nArg` |
|       - | 3426 | `			);` |
|       - | 3427 | `	}` |
|       - | 3428 | `	/* Validate argument types individually so we can report the correct` |
|       - | 3429 | `	 * argument index in the error message. */` |
|      20 | 3430 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3431 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3432 | `			"TypeError",` |
|       - | 3433 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 3434 | `			ph7_type_name(apArg[0])` |
|       - | 3435 | `			);` |
|       - | 3436 | `	}` |
|      17 | 3437 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 3438 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3439 | `			"TypeError",` |
|       - | 3440 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 3441 | `			ph7_type_name(apArg[1])` |
|       - | 3442 | `			);` |
|       - | 3443 | `	}` |
|       - | 3444 | `	/* Point to the internal representation of the input hashmaps */` |
|      14 | 3445 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 3446 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      14 | 3447 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 3448 | `		/* Length mismatch -> ValueError */` |
|       3 | 3449 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3450 | `			"ValueError",` |
|       - | 3451 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 3452 | `			);` |
|       - | 3453 | `	}` |
|       - | 3454 | `	/* Create a new array */` |
|      11 | 3455 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 3456 | `	if( pArray == 0 ){` |
|     ! 0 | 3457 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3458 | `		return PH7_OK;` |
|       - | 3459 | `	}` |
|       - | 3460 | `	/* Perform the requested operation */` |
|      11 | 3461 | `	pKe = pKey->pFirst;` |
|      11 | 3462 | `	pVe = pValue->pFirst;` |
|      33 | 3463 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      23 | 3464 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      23 | 3465 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 3466 | `		/* PHP treats floats used as keys in array_combine differently than` |
|       - | 3467 | `		 * ordinary offset access: the float is stringified rather than` |
|       - | 3468 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|       - | 3469 | `		 * the value when it is a float and convert the copy to string.  The` |
|       - | 3470 | `		 * original array must not be mutated. */` |
|      23 | 3471 | `		ph7_value *pKeyCopy = pKeyVal;` |
|      23 | 3472 | `		if( ph7_value_is_float(pKeyVal) ){` |
|       5 | 3473 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|       5 | 3474 | `			if( pTmpKey ){` |
|       5 | 3475 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|       - | 3476 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|       5 | 3477 | `				PH7_MemObjToString(pTmpKey);` |
|       5 | 3478 | `				pKeyCopy = pTmpKey;` |
|       2 | 3479 | `			}` |
|       2 | 3480 | `		}` |
|      23 | 3481 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|       - | 3482 | `		/* Point to the next entry */` |
|      23 | 3483 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      23 | 3484 | `		pVe = pVe->pPrev;` |
|      12 | 3485 | `	}` |
|       - | 3486 | `	/* Return the filled array */` |
|      11 | 3487 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 3488 | `	return PH7_OK;` |
|      12 | 3489 | `}` |
|       - | 3490 | `/*` |
|       - | 3491 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 3492 | ` *  Return an array with elements in reverse order.` |
|       - | 3493 | ` * Parameters` |
|       - | 3494 | ` *  $array` |
|       - | 3495 | ` *   The input array.` |
|       - | 3496 | ` *  $preserve_keys (optional)` |
|       - | 3497 | ` *   If set to TRUE keys are preserved.` |
|       - | 3498 | ` * Return` |
|       - | 3499 | ` *  The reversed array.` |
|       - | 3500 | ` */` |
|      18 | 3501 | `PH7_PRIVATE int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3502 | `{` |
|       - | 3503 | `	ph7_hashmap_node *pEntry;` |
|       - | 3504 | `	ph7_hashmap *pSrc;` |
|       - | 3505 | `	ph7_value *pArray;` |
|       - | 3506 | `	int bPreserve;` |
|       - | 3507 | `	sxu32 n;` |
|      20 | 3508 | `	if( nArg < 1 ){` |
|     ! 0 | 3509 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3510 | `			"ArgumentCountError",` |
|       - | 3511 | `			"array_reverse() expects at least 1 argument, %d given",` |
|     ! 0 | 3512 | `			nArg` |
|       - | 3513 | `			);` |
|       - | 3514 | `	}` |
|       - | 3515 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 3516 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3517 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3518 | `			"TypeError",` |
|       - | 3519 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3520 | `			ph7_type_name(apArg[0])` |
|       - | 3521 | `			);` |
|       - | 3522 | `	}` |
|      17 | 3523 | `	bPreserve = FALSE;` |
|      17 | 3524 | `	if( nArg > 1 ){` |
|       7 | 3525 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       3 | 3526 | `	}` |
|       - | 3527 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 3528 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3529 | `	/* Create a new array */` |
|      17 | 3530 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 3531 | `	if( pArray == 0 ){` |
|     ! 0 | 3532 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3533 | `		return PH7_OK;` |
|       - | 3534 | `	}` |
|       - | 3535 | `	/* Perform the requested operation */` |
|      17 | 3536 | `	pEntry = pSrc->pLast;` |
|      55 | 3537 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 3538 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      39 | 3539 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      39 | 3540 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 3541 | `		/* Point to the previous entry */` |
|      39 | 3542 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      20 | 3543 | `	}` |
|      17 | 3544 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 3545 | `	return PH7_OK;` |
|      11 | 3546 | `}` |
|       - | 3547 | `/*` |
|       - | 3548 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|       - | 3549 | ` *  Removes duplicate values from an array.` |
|       - | 3550 | ` * Parameters` |
|       - | 3551 | ` *  $array` |
|       - | 3552 | ` *   The input array.` |
|       - | 3553 | ` *  $flags` |
|       - | 3554 | ` *   The optional second parameter may be used to modify the comparison` |
|       - | 3555 | ` *   behavior using these values:` |
|       - | 3556 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|       - | 3557 | ` *     SORT_NUMERIC - compare items numerically` |
|       - | 3558 | ` *     SORT_STRING  - compare items as strings` |
|       - | 3559 | ` * Return` |
|       - | 3560 | ` *  The filtered array.` |
|       - | 3561 | ` */` |
|      34 | 3562 | `PH7_PRIVATE int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3563 | `{` |
|       - | 3564 | `	ph7_hashmap_node *pEntry;` |
|       - | 3565 | `	ph7_value *pNeedle;` |
|       - | 3566 | `	ph7_hashmap *pSrc;` |
|       - | 3567 | `	ph7_value *pArray;` |
|       - | 3568 | `	int iFlags,base,bFold;` |
|       - | 3569 | `	sxu32 n;` |
|      36 | 3570 | `	if( nArg < 1 ){` |
|       - | 3571 | `		/* Missing arguments, throw ArgumentCountError */` |
|     ! 0 | 3572 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3573 | `			"ArgumentCountError",` |
|       - | 3574 | `			"array_unique() expects at least 1 argument, 0 given"` |
|       - | 3575 | `			);` |
|       - | 3576 | `	}` |
|      36 | 3577 | `	if( nArg > 2 ){` |
|       - | 3578 | `		/* Too many arguments, throw ArgumentCountError */` |
|     ! 0 | 3579 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3580 | `			"ArgumentCountError",` |
|       - | 3581 | `			"array_unique() expects at most 2 arguments, %d given",` |
|     ! 0 | 3582 | `			nArg` |
|       - | 3583 | `			);` |
|       - | 3584 | `	}` |
|       - | 3585 | `	/* Make sure we are dealing with a valid hashmap */` |
|      36 | 3586 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3587 | `		/* Type mismatch, throw TypeError */` |
|       4 | 3588 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3589 | `			"TypeError",` |
|       - | 3590 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3591 | `			ph7_type_name(apArg[0])` |
|       - | 3592 | `			);` |
|       - | 3593 | `	}` |
|       - | 3594 | `	/* php's default is SORT_STRING (2): elements compare as strings. Explicit` |
|       - | 3595 | `	 * flags select numeric / natural / case-insensitive comparison. */` |
|      33 | 3596 | `	iFlags = nArg > 1 ? ph7_value_to_int(apArg[1]) : 2 /* SORT_STRING */;` |
|      33 | 3597 | `	base = iFlags & ~8;` |
|      33 | 3598 | `	bFold = (iFlags & 8) != 0;` |
|       - | 3599 | `	/* Point to the internal representation of the input hashmap */` |
|      33 | 3600 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3601 | `	/* Create a new array */` |
|      33 | 3602 | `	pArray = ph7_context_new_array(pCtx);` |
|      33 | 3603 | `	if( pArray == 0 ){` |
|     ! 0 | 3604 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3605 | `		return PH7_OK;` |
|       - | 3606 | `	}` |
|       - | 3607 | `	/* Perform the requested operation */` |
|      33 | 3608 | `	pEntry = pSrc->pFirst;` |
|     145 | 3609 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|     113 | 3610 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|     113 | 3611 | `		if( pNeedle ){` |
|       - | 3612 | `			/* Keep this element unless a flag-equal one is already present. */` |
|     113 | 3613 | `			ph7_hashmap *pKept = (ph7_hashmap *)pArray->x.pOther;` |
|     113 | 3614 | `			ph7_hashmap_node *pK = pKept->pFirst;` |
|     113 | 3615 | `			int bDup = 0;` |
|       - | 3616 | `			sxu32 i;` |
|       - | 3617 | `			/* Forward iteration in this map uses the pPrev link (see the outer` |
|       - | 3618 | `			 * loop over pSrc). */` |
|     177 | 3619 | `			for( i = 0 ; i < pKept->nEntry && pK ; ++i ){` |
|     117 | 3620 | `				ph7_value *pV = HashmapExtractNodeValue(pK);` |
|     117 | 3621 | `				if( pV && HashmapValueFlagEqual(pCtx->pVm,pNeedle,pV,base,bFold) ){` |
|      53 | 3622 | `					bDup = 1;` |
|      53 | 3623 | `					break;` |
|       - | 3624 | `				}` |
|      65 | 3625 | `				pK = pK->pPrev;` |
|      33 | 3626 | `			}` |
|     113 | 3627 | `			if( !bDup ){` |
|      61 | 3628 | `				HashmapInsertNode(pKept,pEntry,TRUE);` |
|      30 | 3629 | `			}` |
|      56 | 3630 | `		}` |
|       - | 3631 | `		/* Point to the next entry */` |
|     113 | 3632 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      57 | 3633 | `	}` |
|       - | 3634 | `	/* Return the freshly created array */` |
|      33 | 3635 | `	ph7_result_value(pCtx,pArray);` |
|      33 | 3636 | `	return PH7_OK;` |
|      19 | 3637 | `}` |
|       - | 3638 | `/*` |
|       - | 3639 | ` * array array_flip(array $input)` |
|       - | 3640 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 3641 | ` * Parameter` |
|       - | 3642 | ` *  $input` |
|       - | 3643 | ` *   Input array.` |
|       - | 3644 | ` * Return` |
|       - | 3645 | ` *   The flipped array on success or NULL on failure.` |
|       - | 3646 | ` */` |
|      32 | 3647 | `PH7_PRIVATE int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3648 | `{` |
|       - | 3649 | `	ph7_hashmap_node *pEntry;` |
|       - | 3650 | `	ph7_hashmap *pSrc;` |
|       - | 3651 | `	ph7_value *pArray;` |
|       - | 3652 | `	ph7_value *pKey;` |
|       - | 3653 | `	ph7_value sVal;` |
|       - | 3654 | `	sxu32 n;` |
|       - | 3655 |  |
|       - | 3656 | `	/* PHP requires exactly one argument */` |
|      34 | 3657 | `	if( nArg != 1 ){` |
|       - | 3658 | `		/* Use ArgumentCountError like other array helpers */` |
|     ! 0 | 3659 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3660 | `			"ArgumentCountError",` |
|       - | 3661 | `			"array_flip() expects exactly 1 argument, %d given",` |
|     ! 0 | 3662 | `			nArg` |
|       - | 3663 | `			);` |
|       - | 3664 | `	}` |
|       - | 3665 | `	/* Make sure we are dealing with a valid hashmap */` |
|      34 | 3666 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3667 | `		/* Type mismatch -> TypeError */` |
|       4 | 3668 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3669 | `			"TypeError",` |
|       - | 3670 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3671 | `			ph7_type_name(apArg[0])` |
|       - | 3672 | `			);` |
|       - | 3673 | `	}` |
|       - | 3674 | `	/* Point to the internal representation of the input hashmap */` |
|      31 | 3675 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3676 | `	/* Create a new array */` |
|      31 | 3677 | `	pArray = ph7_context_new_array(pCtx);` |
|      31 | 3678 | `	if( pArray == 0 ){` |
|     ! 0 | 3679 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3680 | `		return PH7_OK;` |
|       - | 3681 | `	}` |
|       - | 3682 | `	/* Start processing */` |
|      31 | 3683 | `	pEntry = pSrc->pFirst;` |
|   22279 | 3684 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 3685 | `		/* Extract the node value (will become a key in the result) */` |
|   22249 | 3686 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22249 | 3687 | `		if( pKey ){` |
|       - | 3688 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22249 | 3689 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 3690 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3691 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 3692 | `					);` |
|   22248 | 3693 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 3694 | `				/* Prepare the value for insertion (original key) */` |
|   22235 | 3695 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20003 | 3696 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10002 | 3697 | `				}else{` |
|       - | 3698 | `					SyString sStr;` |
|    2233 | 3699 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2233 | 3700 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 3701 | `				}` |
|       - | 3702 | `				/* Perform the insertion */` |
|   22235 | 3703 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 3704 | `				/* Safely release the value because each inserted entry` |
|       - | 3705 | `				 * has its own private copy of the value.` |
|       - | 3706 | `				 */` |
|   22235 | 3707 | `				PH7_MemObjRelease(&sVal);` |
|   11118 | 3708 | `			}else{` |
|       - | 3709 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|      13 | 3710 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3711 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 3712 | `					);` |
|       - | 3713 | `			}` |
|   11124 | 3714 | `		}` |
|       - | 3715 | `		/* Point to the next entry */` |
|   22249 | 3716 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11125 | 3717 | `	}` |
|       - | 3718 | `	/* Return the freshly created array */` |
|      31 | 3719 | `	ph7_result_value(pCtx,pArray);` |
|      31 | 3720 | `	return PH7_OK;` |
|      18 | 3721 | `}` |
|       - | 3722 | `/*` |
|       - | 3723 | ` * number array_sum(array $array )` |
|       - | 3724 | ` *  Calculate the sum of values in an array.` |
|       - | 3725 | ` * Parameters` |
|       - | 3726 | ` *  $array: The input array.` |
|       - | 3727 | ` * Return` |
|       - | 3728 | ` *  Returns the sum of values as an integer or float.` |
|       - | 3729 | ` */` |
|      24 | 3730 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 3731 | `{` |
|       - | 3732 | `	ph7_hashmap_node *pEntry;` |
|       - | 3733 | `	ph7_value *pObj;` |
|      26 | 3734 | `	double dSum = 0;` |
|       - | 3735 | `	sxu32 n;` |
|      26 | 3736 | `	pEntry = pMap->pFirst;` |
|      92 | 3737 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      68 | 3738 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      68 | 3739 | `		if( pObj ){` |
|      68 | 3740 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      30 | 3741 | `				dSum += pObj->rVal;` |
|      54 | 3742 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      21 | 3743 | `				dSum += (double)pObj->x.iVal;` |
|      30 | 3744 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      16 | 3745 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|       - | 3746 | `					/* php warns and SKIPS a non-numeric string (the array/object/` |
|       - | 3747 | `					 * resource cases below already did; only this one was silent) */` |
|       3 | 3748 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - | 3749 | `						"Addition is not supported on type string");` |
|      14 | 3750 | `				}else if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      13 | 3751 | `					double dv = 0;` |
|      13 | 3752 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|      13 | 3753 | `					dSum += dv;` |
|       8 | 3754 | `				}` |
|      12 | 3755 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 3756 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3757 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 3758 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       - | 3759 | `				/* php names the CLASS here, not the literal word "object" */` |
|     ! 0 | 3760 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|     ! 0 | 3761 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - | 3762 | `					"Addition is not supported on type %s",` |
|     ! 0 | 3763 | `					pInst && pInst->pClass ? pInst->pClass->sName.zString : "object");` |
|       3 | 3764 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 3765 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3766 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 3767 | `			}` |
|       - | 3768 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      33 | 3769 | `		}` |
|       - | 3770 | `		/* Point to the next entry */` |
|      68 | 3771 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      35 | 3772 | `	}` |
|       - | 3773 | `	/* Return sum */` |
|      26 | 3774 | `	ph7_result_double(pCtx,dSum);` |
|      26 | 3775 | `}` |
|     690 | 3776 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 3777 | `{` |
|       - | 3778 | `	ph7_hashmap_node *pEntry;` |
|       - | 3779 | `	ph7_value *pObj;` |
|     692 | 3780 | `	sxi64 nSum = 0;` |
|       - | 3781 | `	sxu32 n;` |
|     692 | 3782 | `	pEntry = pMap->pFirst;` |
|    6704 | 3783 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|    6014 | 3784 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|    6014 | 3785 | `		if( pObj ){` |
|    6014 | 3786 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|    5994 | 3787 | `				nSum += pObj->x.iVal;` |
|    3018 | 3788 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      12 | 3789 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|       - | 3790 | `					/* php warns and SKIPS a non-numeric string */` |
|       5 | 3791 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - | 3792 | `						"Addition is not supported on type string");` |
|      10 | 3793 | `				}else if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       8 | 3794 | `					sxi64 nv = 0;` |
|       8 | 3795 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       8 | 3796 | `					nSum += nv;` |
|       5 | 3797 | `				}` |
|      17 | 3798 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       6 | 3799 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3800 | `					"array_sum(): Addition is not supported on type array");` |
|      10 | 3801 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       - | 3802 | `				/* php names the CLASS here, not the literal word "object" */` |
|       3 | 3803 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       5 | 3804 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - | 3805 | `					"Addition is not supported on type %s",` |
|       2 | 3806 | `					pInst && pInst->pClass ? pInst->pClass->sName.zString : "object");` |
|       7 | 3807 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 3808 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3809 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 3810 | `			}` |
|       - | 3811 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|    3006 | 3812 | `		}` |
|       - | 3813 | `		/* Point to the next entry */` |
|    6014 | 3814 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    3008 | 3815 | `	}` |
|       - | 3816 | `	/* Return sum */` |
|     692 | 3817 | `	ph7_result_int64(pCtx,nSum);` |
|     692 | 3818 | `}` |
|       - | 3819 | `/* number array_sum(array $array )` |
|       - | 3820 | ` * (See block-coment above)` |
|       - | 3821 | ` */` |
|     724 | 3822 | `PH7_PRIVATE int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3823 | `{` |
|       - | 3824 | `	ph7_hashmap_node *pEntry;` |
|       - | 3825 | `	ph7_hashmap *pMap;` |
|       - | 3826 | `	ph7_value *pObj;` |
|     727 | 3827 | `	int useDouble = 0;` |
|       - | 3828 | `	sxu32 n;` |
|       - | 3829 | `	/* PHP requires exactly one argument */` |
|     727 | 3830 | `	if( nArg != 1 ){` |
|     ! 0 | 3831 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3832 | `			"ArgumentCountError",` |
|       - | 3833 | `			"array_sum() expects exactly 1 argument, %d given",` |
|     ! 0 | 3834 | `			nArg` |
|       - | 3835 | `			);` |
|       - | 3836 | `	}` |
|       - | 3837 | `	/* Make sure we are dealing with a valid hashmap */` |
|     727 | 3838 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3839 | `		/* Type mismatch -> TypeError (php's true/false/class-name convention). */` |
|       - | 3840 | `		char zBuf[64];` |
|       8 | 3841 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3842 | `			"TypeError",` |
|       - | 3843 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 3844 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 3845 | `			);` |
|       - | 3846 | `	}` |
|     722 | 3847 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     722 | 3848 | `	if( pMap->nEntry < 1 ){` |
|       - | 3849 | `		/* Nothing to compute,return 0 */` |
|       7 | 3850 | `		ph7_result_int(pCtx,0);` |
|       7 | 3851 | `		return PH7_OK;` |
|       - | 3852 | `	}` |
|       - | 3853 | `	/* Scan all elements: if any value is a float, use floating-point` |
|       - | 3854 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|       - | 3855 | `	 */` |
|     716 | 3856 | `	pEntry = pMap->pFirst;` |
|    6736 | 3857 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|    6046 | 3858 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|    6046 | 3859 | `		if( pObj ){` |
|    6046 | 3860 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      20 | 3861 | `				useDouble = 1;` |
|      20 | 3862 | `				break;` |
|       - | 3863 | `			}` |
|    6028 | 3864 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|      18 | 3865 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|      18 | 3866 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|       - | 3867 | `				sxu32 i;` |
|      32 | 3868 | `				for( i = 0 ; i < nLen ; i++ ){` |
|      22 | 3869 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|       7 | 3870 | `						useDouble = 1;` |
|       7 | 3871 | `						break;` |
|       - | 3872 | `					}` |
|       9 | 3873 | `				}` |
|      18 | 3874 | `				if( useDouble ){` |
|       7 | 3875 | `					break;` |
|       - | 3876 | `				}` |
|       5 | 3877 | `			}` |
|    3010 | 3878 | `		}` |
|    6022 | 3879 | `		pEntry = pEntry->pPrev;` |
|    3012 | 3880 | `	}` |
|     716 | 3881 | `	if( useDouble ){` |
|      26 | 3882 | `		DoubleSum(pCtx,pMap);` |
|      14 | 3883 | `	}else{` |
|     692 | 3884 | `		Int64Sum(pCtx,pMap);` |
|       - | 3885 | `	}` |
|     716 | 3886 | `	return PH7_OK;` |
|     365 | 3887 | `}` |
|       - | 3888 | `/*` |
|       - | 3889 | ` * number array_product(array $array )` |
|       - | 3890 | ` *  Calculate the product of values in an array.` |
|       - | 3891 | ` * Parameters` |
|       - | 3892 | ` *  $array: The input array.` |
|       - | 3893 | ` * Return` |
|       - | 3894 | ` *  Returns the product of values as an integer or float.` |
|       - | 3895 | ` */` |
|       2 | 3896 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 3897 | `{` |
|       - | 3898 | `	ph7_hashmap_node *pEntry;` |
|       - | 3899 | `	ph7_value *pObj;` |
|       - | 3900 | `	double dProd;` |
|       - | 3901 | `	sxu32 n;` |
|       3 | 3902 | `	pEntry = pMap->pFirst;` |
|       3 | 3903 | `	dProd = 1;` |
|       7 | 3904 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       5 | 3905 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|       5 | 3906 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|       5 | 3907 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|       3 | 3908 | `				dProd *= pObj->rVal;` |
|       4 | 3909 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|       3 | 3910 | `				dProd *= (double)pObj->x.iVal;` |
|       1 | 3911 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 3912 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 3913 | `					double dv = 0;` |
|     ! 0 | 3914 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 3915 | `					dProd *= dv;` |
|     ! 0 | 3916 | `				}` |
|     ! 0 | 3917 | `			}` |
|       2 | 3918 | `		}` |
|       - | 3919 | `		/* Point to the next entry */` |
|       5 | 3920 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       3 | 3921 | `	}` |
|       - | 3922 | `	/* Return product */` |
|       3 | 3923 | `	ph7_result_double(pCtx,dProd);` |
|       3 | 3924 | `}` |
|       2 | 3925 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 3926 | `{` |
|       - | 3927 | `	ph7_hashmap_node *pEntry;` |
|       - | 3928 | `	ph7_value *pObj;` |
|       - | 3929 | `	sxi64 nProd;` |
|       - | 3930 | `	sxu32 n;` |
|       3 | 3931 | `	pEntry = pMap->pFirst;` |
|       3 | 3932 | `	nProd = 1;` |
|       9 | 3933 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       7 | 3934 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|       7 | 3935 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|       7 | 3936 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 3937 | `				nProd *= (sxi64)pObj->rVal;` |
|       7 | 3938 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|       7 | 3939 | `				nProd *= pObj->x.iVal;` |
|       3 | 3940 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 3941 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 3942 | `					sxi64 nv = 0;` |
|     ! 0 | 3943 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 3944 | `					nProd *= nv;` |
|     ! 0 | 3945 | `				}` |
|     ! 0 | 3946 | `			}` |
|       3 | 3947 | `		}` |
|       - | 3948 | `		/* Point to the next entry */` |
|       7 | 3949 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       4 | 3950 | `	}` |
|       - | 3951 | `	/* Return product */` |
|       3 | 3952 | `	ph7_result_int64(pCtx,nProd);` |
|       3 | 3953 | `}` |
|       - | 3954 | `/* number array_product(array $array )` |
|       - | 3955 | ` * (See block-block comment above)` |
|       - | 3956 | ` */` |
|      14 | 3957 | `PH7_PRIVATE int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3958 | `{` |
|       - | 3959 | `	ph7_hashmap *pMap;` |
|       - | 3960 | `	ph7_value *pObj;` |
|      15 | 3961 | `	if( nArg < 1 ){` |
|       - | 3962 | `		/* Missing arguments (arity is enforced upstream; defensive). */` |
|     ! 0 | 3963 | `		ph7_result_int(pCtx,1);` |
|     ! 0 | 3964 | `		return PH7_OK;` |
|       - | 3965 | `	}` |
|       - | 3966 | `	/* PHP 8: a non-array $array is a catchable TypeError, not a silent 0. */` |
|      15 | 3967 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3968 | `		char zBuf[64];` |
|      13 | 3969 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3970 | `			"TypeError",` |
|       - | 3971 | `			"array_product(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 3972 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 3973 | `			);` |
|       - | 3974 | `	}` |
|       7 | 3975 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 3976 | `	if( pMap->nEntry < 1 ){` |
|       - | 3977 | `		/* The product of an empty array is the multiplicative identity 1 (PHP). */` |
|       3 | 3978 | `		ph7_result_int(pCtx,1);` |
|       3 | 3979 | `		return PH7_OK;` |
|       - | 3980 | `	}` |
|       - | 3981 | `	/* If the first element is of type float,then perform floating` |
|       - | 3982 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 3983 | `	 */` |
|       5 | 3984 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|       5 | 3985 | `	if( pObj == 0 ){` |
|     ! 0 | 3986 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 3987 | `		return PH7_OK;` |
|       - | 3988 | `	}` |
|       5 | 3989 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|       3 | 3990 | `		DoubleProd(pCtx,pMap);` |
|       2 | 3991 | `	}else{` |
|       3 | 3992 | `		Int64Prod(pCtx,pMap);` |
|       - | 3993 | `	}` |
|       5 | 3994 | `	return PH7_OK;` |
|       8 | 3995 | `}` |
|       - | 3996 | `/*` |
|       - | 3997 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 3998 | ` *  Pick one or more random entries out of an array.` |
|       - | 3999 | ` * Parameters` |
|       - | 4000 | ` * $input` |
|       - | 4001 | ` *  The input array.` |
|       - | 4002 | ` * $num_req` |
|       - | 4003 | ` *  Specifies how many entries you want to pick.` |
|       - | 4004 | ` * Return` |
|       - | 4005 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 4006 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 4007 | ` *  NULL is returned on failure.` |
|       - | 4008 | ` */` |
|      36 | 4009 | `PH7_PRIVATE int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4010 | `{` |
|       - | 4011 | `	ph7_hashmap_node *pNode;` |
|       - | 4012 | `	ph7_hashmap *pMap;` |
|      37 | 4013 | `	int nItem = 1;` |
|      37 | 4014 | `	if( nArg < 1 ){` |
|       - | 4015 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|     ! 0 | 4016 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4017 | `		return PH7_OK;` |
|       - | 4018 | `	}` |
|       - | 4019 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|      37 | 4020 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4021 | `		char zBuf[64];` |
|      10 | 4022 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4023 | `			"TypeError",` |
|       - | 4024 | `			"array_rand(): Argument #1 ($array) must be of type array, %s given",` |
|       3 | 4025 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4026 | `			);` |
|       - | 4027 | `	}` |
|       - | 4028 | `	/* php validates $num (and weak-coerces it) BEFORE the empty-array body` |
|       - | 4029 | `	 * check, matching its ZPP-before-body ordering. */` |
|      31 | 4030 | `	if( nArg > 1 ){` |
|      23 | 4031 | `		ph7_value *pNum = apArg[1];` |
|      22 | 4032 | `		if( ph7_value_is_array(pNum) \|\| ph7_value_is_object(pNum)` |
|      23 | 4033 | `			\|\| ph7_value_is_resource(pNum) ){` |
|       - | 4034 | `			char zBuf[64];` |
|     ! 0 | 4035 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4036 | `				"TypeError",` |
|       - | 4037 | `				"array_rand(): Argument #2 ($num) must be of type int, %s given",` |
|     ! 0 | 4038 | `				VmValueGivenName(pNum,zBuf,sizeof(zBuf))` |
|       - | 4039 | `				);` |
|       - | 4040 | `		}` |
|      23 | 4041 | `		if( ph7_value_is_string(pNum) ){` |
|       - | 4042 | `			/* Weak int coercion of a string $num follows php's numeric-string` |
|       - | 4043 | `			 * grammar (whole string, int or float): a non-numeric string` |
|       - | 4044 | `			 * (incl. leading-numeric junk like "2abc" or "0x1A") is a TypeError,` |
|       - | 4045 | `			 * a well-formed float-string ("1e3") coerces like a float value.` |
|       - | 4046 | `			 * Reuses the range() ZPP number parser (§3.9 shared-helper note). */` |
|       - | 4047 | `			int len;` |
|       9 | 4048 | `			const char *zStr = ph7_value_to_string(pNum, &len);` |
|       - | 4049 | `			sxi64 iLong; double dReal;` |
|       9 | 4050 | `			sxu8 iKind = RangeStrToNumber(zStr, (sxu32)len, &iLong, &dReal);` |
|       9 | 4051 | `			if( iKind == RANGE_IN_ERROR ){` |
|       7 | 4052 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4053 | `					"TypeError",` |
|       - | 4054 | `					"array_rand(): Argument #2 ($num) must be of type int, string given"` |
|       - | 4055 | `					);` |
|       - | 4056 | `			}` |
|       - | 4057 | `			/* Clamp into a signed-int band so an absurd magnitude still yields` |
|       - | 4058 | `			 * the out-of-range ValueError below without an out-of-int cast. */` |
|       3 | 4059 | `			if( iKind == RANGE_IN_DOUBLE ){` |
|       3 | 4060 | `				iLong = dReal <= 0.0 ? 0 : (dReal >= 2147483647.0 ? 2147483647 : (sxi64)dReal);` |
|       1 | 4061 | `			}` |
|       3 | 4062 | `			if( iLong > 2147483647 ){ iLong = 2147483647; }` |
|       3 | 4063 | `			else if( iLong < -2147483647 ){ iLong = -2147483647; }` |
|       3 | 4064 | `			nItem = (int)iLong;` |
|       2 | 4065 | `		}else{` |
|      15 | 4066 | `			nItem = ph7_value_to_int(pNum);` |
|       - | 4067 | `		}` |
|       8 | 4068 | `	}` |
|       - | 4069 | `	/* Point to the internal representation of the input hashmap */` |
|      25 | 4070 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4071 | `	/* php 8: an empty array is a ValueError, not a NULL return */` |
|      25 | 4072 | `	if( pMap->nEntry < 1 ){` |
|       5 | 4073 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4074 | `			"ValueError",` |
|       - | 4075 | `			"array_rand(): Argument #1 ($array) must not be empty"` |
|       - | 4076 | `			);` |
|       - | 4077 | `	}` |
|       - | 4078 | `	/* php 8: $num outside [1, count] is a ValueError, not a clamp/wrong value */` |
|      21 | 4079 | `	if( nItem < 1 \|\| nItem > (int)pMap->nEntry ){` |
|       9 | 4080 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4081 | `			"ValueError",` |
|       - | 4082 | `			"array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)"` |
|       - | 4083 | `			);` |
|       - | 4084 | `	}` |
|      13 | 4085 | `	if( nItem < 2 ){` |
|       - | 4086 | `		sxu32 nEntry;` |
|       - | 4087 | `		/* Pick a random slot through the MT19937 generator so array_rand()` |
|       - | 4088 | `		 * responds to srand()/mt_srand() (reproducible), like php. The exact` |
|       - | 4089 | `		 * index php lands on differs (php samples its internal hashtable` |
|       - | 4090 | `		 * buckets), so this is deterministic-under-seed but not value-parity. */` |
|       9 | 4091 | `		nEntry = (sxu32)PH7_VmMtRandRange(pMap->pVm,0,(sxi64)pMap->nEntry - 1);` |
|       - | 4092 | `		/* Extract the desired entry.` |
|       - | 4093 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 4094 | `		 */` |
|       9 | 4095 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       3 | 4096 | `			pNode = pMap->pLast;` |
|       3 | 4097 | `			nEntry = pMap->nEntry - nEntry;` |
|       3 | 4098 | `			if( nEntry > 1 ){` |
|     ! 0 | 4099 | `				for(;;){` |
|     ! 0 | 4100 | `					if( nEntry == 0 ){` |
|     ! 0 | 4101 | `						break;` |
|       - | 4102 | `					}` |
|       - | 4103 | `					/* Point to the previous entry */` |
|     ! 0 | 4104 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 4105 | `					nEntry--;` |
|     ! 0 | 4106 | `				}` |
|     ! 0 | 4107 | `			}` |
|       2 | 4108 | `		}else{` |
|       7 | 4109 | `			pNode = pMap->pFirst;` |
|       3 | 4110 | `			for(;;){` |
|       9 | 4111 | `				if( nEntry == 0 ){` |
|       7 | 4112 | `					break;` |
|       - | 4113 | `				}` |
|       - | 4114 | `				/* Point to the next entry */` |
|       2 | 4115 | `				pNode = pNode->pPrev; /* Reverse link */` |
|       2 | 4116 | `				nEntry--;` |
|     ! 0 | 4117 | `			}` |
|       - | 4118 | `		}` |
|       9 | 4119 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 4120 | `			/* Int key */` |
|       7 | 4121 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       4 | 4122 | `		}else{` |
|       - | 4123 | `			/* Blob key */` |
|       3 | 4124 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 4125 | `		}` |
|       5 | 4126 | `	}else{` |
|       - | 4127 | `		ph7_value sKey,*pArray;` |
|       - | 4128 | `		ph7_hashmap *pDest;` |
|       - | 4129 | `		/* Create a new array */` |
|       5 | 4130 | `		pArray = ph7_context_new_array(pCtx);` |
|       5 | 4131 | `		if( pArray == 0 ){` |
|     ! 0 | 4132 | `			ph7_result_null(pCtx);` |
|     ! 0 | 4133 | `			return PH7_OK;` |
|       - | 4134 | `		}` |
|       - | 4135 | `		/* Point to the internal representation of the hashmap */` |
|       5 | 4136 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       5 | 4137 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 4138 | `		/* Copy the first n items */` |
|       5 | 4139 | `		pNode = pMap->pFirst;` |
|       5 | 4140 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 4141 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 4142 | `		}` |
|      15 | 4143 | `		while( nItem > 0){` |
|      11 | 4144 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|      11 | 4145 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|      11 | 4146 | `			PH7_MemObjRelease(&sKey);` |
|       - | 4147 | `			/* Point to the next entry */` |
|      11 | 4148 | `			pNode = pNode->pPrev; /* Reverse link */` |
|      11 | 4149 | `			nItem--;` |
|       1 | 4150 | `		}` |
|       - | 4151 | `		/* Shuffle the array */` |
|       5 | 4152 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 4153 | `		/* Rehash node */` |
|       5 | 4154 | `		HashmapSortRehash(pDest);` |
|       - | 4155 | `		/* Return the random array */` |
|       5 | 4156 | `		ph7_result_value(pCtx,pArray);` |
|       - | 4157 | `	}` |
|      13 | 4158 | `	return PH7_OK;` |
|      19 | 4159 | `}` |
|       - | 4160 | `/*` |
|       - | 4161 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 4162 | ` *  Split an array into chunks.` |
|       - | 4163 | ` * Parameters` |
|       - | 4164 | ` * $input` |
|       - | 4165 | ` *   The array to work on` |
|       - | 4166 | ` * $size` |
|       - | 4167 | ` *   The size of each chunk` |
|       - | 4168 | ` * $preserve_keys` |
|       - | 4169 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 4170 | ` *   the chunk numerically.` |
|       - | 4171 | ` * Return` |
|       - | 4172 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 4173 | ` *  zero, with each dimension containing size elements.` |
|       - | 4174 | ` */` |
|      36 | 4175 | `PH7_PRIVATE int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4176 | `{` |
|       - | 4177 | `	ph7_value *pArray,*pChunk;` |
|       - | 4178 | `	ph7_hashmap_node *pEntry;` |
|       - | 4179 | `	ph7_hashmap *pMap;` |
|       - | 4180 | `	int bPreserve;` |
|       - | 4181 | `	sxu32 nChunk;` |
|       - | 4182 | `	sxu32 nSize;` |
|       - | 4183 | `	sxu32 n;` |
|       - | 4184 | `	/* Argument count and types follow PHP semantics. */` |
|      41 | 4185 | `	if( nArg < 2 ){` |
|       - | 4186 | `		/* fewer than required arguments -> ArgumentCountError */` |
|     ! 0 | 4187 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4188 | `			"ArgumentCountError",` |
|       - | 4189 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|     ! 0 | 4190 | `			nArg` |
|       - | 4191 | `			);` |
|       - | 4192 | `	}` |
|      41 | 4193 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4194 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4195 | `			"TypeError",` |
|       - | 4196 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4197 | `			ph7_type_name(apArg[0])` |
|       - | 4198 | `			);` |
|       - | 4199 | `	}` |
|       - | 4200 | `	/* Create a new array */` |
|      38 | 4201 | `	pArray = ph7_context_new_array(pCtx);` |
|      38 | 4202 | `	if( pArray == 0 ){` |
|     ! 0 | 4203 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4204 | `		return PH7_OK;` |
|       - | 4205 | `	}` |
|       - | 4206 | `	/* Point to the internal representation of the input hashmap */` |
|      38 | 4207 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4208 | `	/* Extract and validate the chunk size argument. */` |
|       - | 4209 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      51 | 4210 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      72 | 4211 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      34 | 4212 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 4213 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4214 | `			"TypeError",` |
|       - | 4215 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 4216 | `			ph7_type_name(apArg[1])` |
|       - | 4217 | `			);` |
|       - | 4218 | `	}` |
|       - | 4219 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 4220 | `	 * strings are permitted; however those representing floats lose` |
|       - | 4221 | `	 * precision and PHP emits a deprecation warning. */` |
|      38 | 4222 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 4223 | `		int len;` |
|       3 | 4224 | `		sxu8 bReal = FALSE;` |
|       3 | 4225 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 4226 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 4227 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4228 | `				"TypeError",` |
|       - | 4229 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 4230 | `				);` |
|       - | 4231 | `		}` |
|     ! 0 | 4232 | `		if( bReal ){` |
|       - | 4233 | `			/* float-string -> warn but allow */` |
|     ! 0 | 4234 | `			PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 4235 | `				"Implicit conversion from float-string to int loses precision");` |
|     ! 0 | 4236 | `		}` |
|     ! 0 | 4237 | `	}` |
|       - | 4238 | `	/* If the value is a float with a fractional component, emit a` |
|       - | 4239 | `	 * deprecation warning but continue.  The following conversion occurs` |
|       - | 4240 | `	 * later via ph7_value_to_int. */` |
|      36 | 4241 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 4242 | `		double d = ph7_value_to_double(apArg[1]);` |
|       3 | 4243 | `		sxi64 i = (sxi64)d;` |
|       3 | 4244 | `		if( d != (double)i ){` |
|       3 | 4245 | `			PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 4246 | `				"Implicit conversion from float to int loses precision");` |
|       1 | 4247 | `		}` |
|       1 | 4248 | `	}` |
|       - | 4249 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 4250 | `	 * eliminated, this will not produce a warning. */` |
|       - | 4251 | `	{` |
|      36 | 4252 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      36 | 4253 | `		if( nSizeSigned < 1 ){` |
|       - | 4254 | `			/* size <= 0 -> ValueError */` |
|       6 | 4255 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4256 | `				"ValueError",` |
|       - | 4257 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 4258 | `				);` |
|       - | 4259 | `		}` |
|      30 | 4260 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 4261 | `	}` |
|      30 | 4262 | `	if( nSize >= pMap->nEntry ){` |
|       - | 4263 | `		/* Return the whole array */` |
|       3 | 4264 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 4265 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 4266 | `		return PH7_OK;` |
|       - | 4267 | `	}` |
|      28 | 4268 | `	bPreserve = 0;` |
|      28 | 4269 | `	if( nArg > 2 ){` |
|       - | 4270 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 4271 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 4272 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 4273 | `		 * normally, matching PHP behaviour. */` |
|      30 | 4274 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      31 | 4275 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 4276 | `			ph7_value_is_resource(apArg[2]) ){` |
|     ! 0 | 4277 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4278 | `				"TypeError",` |
|       - | 4279 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|     ! 0 | 4280 | `				ph7_type_name(apArg[2])` |
|       - | 4281 | `				);` |
|       - | 4282 | `		}` |
|      21 | 4283 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 4284 | `	}` |
|       - | 4285 | `	/* Start processing */` |
|      28 | 4286 | `	pEntry = pMap->pFirst;` |
|      28 | 4287 | `	nChunk = 0;` |
|      28 | 4288 | `	pChunk = 0;` |
|      28 | 4289 | `	n = pMap->nEntry;` |
|      54 | 4290 | `	for( ;; ){` |
|     110 | 4291 | `		if( n < 1 ){` |
|       - | 4292 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 4293 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 4294 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 4295 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 4296 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 4297 | `			 * exists. */` |
|      28 | 4298 | `			if( pChunk ){` |
|      28 | 4299 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      13 | 4300 | `			}` |
|      28 | 4301 | `			break;` |
|       - | 4302 | `		}` |
|      84 | 4303 | `		if( nChunk < 1 ){` |
|      72 | 4304 | `			if( pChunk ){` |
|       - | 4305 | `				/* Put the first chunk */` |
|      46 | 4306 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      22 | 4307 | `			}` |
|       - | 4308 | `			/* Create a new dimension */` |
|      72 | 4309 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 4310 | `												   * will be automatically released as soon we return` |
|       - | 4311 | `												   * from this function */` |
|      72 | 4312 | `			if( pChunk == 0 ){` |
|     ! 0 | 4313 | `				break;` |
|       - | 4314 | `			}` |
|      72 | 4315 | `			nChunk = nSize;` |
|      35 | 4316 | `		}` |
|       - | 4317 | `		/* Insert the entry */` |
|      84 | 4318 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 4319 | `		/* Point to the next entry */` |
|      84 | 4320 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      84 | 4321 | `		nChunk--;` |
|      84 | 4322 | `		n--;` |
|       2 | 4323 | `	}` |
|       - | 4324 | `	/* Return the multidimensional array */` |
|      28 | 4325 | `	ph7_result_value(pCtx,pArray);` |
|      28 | 4326 | `	return PH7_OK;` |
|      23 | 4327 | `}` |
|       - | 4328 | `/*` |
|       - | 4329 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 4330 | ` *  Pad array to the specified length with a value.` |
|       - | 4331 | ` * $input` |
|       - | 4332 | ` *   Initial array of values to pad.` |
|       - | 4333 | ` * $pad_size` |
|       - | 4334 | ` *   New size of the array.` |
|       - | 4335 | ` * $pad_value` |
|       - | 4336 | ` *   Value to pad if input is less than pad_size.` |
|       - | 4337 | ` */` |
|       - | 4338 | `/*` |
|       - | 4339 | ` * Shared "requested array size too large" guard (band A #8). php throws a` |
|       - | 4340 | ` * catchable ValueError when a builtin's caller-controlled target length` |
|       - | 4341 | ` * exceeds its hashtable capacity HT_MAX_SIZE (2^30 elements; probed against` |
|       - | 4342 | ` * php 8.5.7 — the boundary sits exactly between 1073741824 and 1073741825,` |
|       - | 4343 | ` * independent of the input array's size and symmetric for negative lengths).` |
|       - | 4344 | ` * Without this, a call like array_pad([1,2], 2000000000, 0) sits in the fill` |
|       - | 4345 | ` * loop for minutes and then OOMs. nRequested is the ABSOLUTE requested` |
|       - | 4346 | ` * length; pass a still-negative value (e.g. the unnegatable INT64_MIN,` |
|       - | 4347 | ` * mirroring php's ZEND_ABS overflow) to fail the guard unconditionally.` |
|       - | 4348 | ` * Returns SXRET_OK when the size is acceptable, else the throw status to` |
|       - | 4349 | ` * propagate. The cap constant is shared with range()'s guards` |
|       - | 4350 | ` * (PH7_RANGE_HT_MAX_SIZE above).` |
|       - | 4351 | ` */` |
|      50 | 4352 | `static sxi32 HashmapGuardArraySize(` |
|       - | 4353 | `	ph7_context *pCtx,` |
|       - | 4354 | `	const char *zFunc,     /* Function name for the message */` |
|       - | 4355 | `	int iArg,              /* 1-based argument position */` |
|       - | 4356 | `	const char *zParam     /* "$length"-style parameter name */,` |
|       - | 4357 | `	sxi64 nRequested       /* Absolute requested element count */` |
|       - | 4358 | `	)` |
|       1 | 4359 | `{` |
|      51 | 4360 | `	if( nRequested < 0 \|\| nRequested > PH7_RANGE_HT_MAX_SIZE ){` |
|      22 | 4361 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4362 | `			"ValueError",` |
|       - | 4363 | `			"%s(): Argument #%d (%s) must not exceed the maximum allowed array size",` |
|       7 | 4364 | `			zFunc,iArg,zParam` |
|       - | 4365 | `			);` |
|       - | 4366 | `	}` |
|      37 | 4367 | `	return SXRET_OK;` |
|      26 | 4368 | `}` |
|      60 | 4369 | `PH7_PRIVATE int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4370 | `{` |
|       - | 4371 | `	ph7_hashmap *pMap;` |
|       - | 4372 | `	ph7_value *pArray;` |
|       - | 4373 | `	sxi64 iLen,iAbs;` |
|       - | 4374 | `	int nEntry;` |
|       - | 4375 | `	sxi32 rc;` |
|      62 | 4376 | `	if( nArg != 3 ){` |
|     ! 0 | 4377 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4378 | `			"ArgumentCountError",` |
|       - | 4379 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|     ! 0 | 4380 | `			nArg` |
|       - | 4381 | `			);` |
|       - | 4382 | `	}` |
|      62 | 4383 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4384 | `		char zBuf[64];` |
|      11 | 4385 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4386 | `			"TypeError",` |
|       - | 4387 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|       3 | 4388 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4389 | `			);` |
|       - | 4390 | `	}` |
|       - | 4391 | `	/* php 8: $length must be int-coercible. An array/object/resource or a` |
|       - | 4392 | `	 * non-numeric string throws a TypeError instead of silently padding to 0;` |
|       - | 4393 | `	 * a numeric string is weak-coerced via php's is_numeric_string grammar` |
|       - | 4394 | `	 * (reusing the shared RangeStrToNumber, like array_rand's $num). */` |
|      54 | 4395 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|      55 | 4396 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|       - | 4397 | `		char zBuf[64];` |
|     ! 0 | 4398 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4399 | `			"TypeError",` |
|       - | 4400 | `			"array_pad(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 4401 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|       - | 4402 | `			);` |
|       - | 4403 | `	}` |
|      55 | 4404 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 4405 | `		int nStr;` |
|      11 | 4406 | `		const char *zStr = ph7_value_to_string(apArg[1],&nStr);` |
|       - | 4407 | `		sxi64 iLong; double dReal;` |
|      11 | 4408 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|      11 | 4409 | `		if( iKind == RANGE_IN_ERROR ){` |
|       5 | 4410 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4411 | `				"TypeError",` |
|       - | 4412 | `				"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|       - | 4413 | `				);` |
|       - | 4414 | `		}` |
|       7 | 4415 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|       - | 4416 | `			/* php ZPP: a float-string outside the int64 range (or NaN) fails` |
|       - | 4417 | `			 * outright — also keeps the (sxi64) cast below UB-free. */` |
|       3 | 4418 | `			if( dReal != dReal \|\| dReal >= 9223372036854775808.0 \|\| dReal < -9223372036854775808.0 ){` |
|     ! 0 | 4419 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4420 | `					"TypeError",` |
|       - | 4421 | `					"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|       - | 4422 | `					);` |
|       - | 4423 | `			}` |
|       3 | 4424 | `			iLen = (sxi64)dReal;` |
|       3 | 4425 | `			if( (double)iLen != dReal ){` |
|     ! 0 | 4426 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 4427 | `					"array_pad(): Argument #2 ($length) must be of type int, string given");` |
|       - | 4428 | `			}` |
|       2 | 4429 | `		}else{` |
|       5 | 4430 | `			iLen = iLong;` |
|       - | 4431 | `		}` |
|       4 | 4432 | `	}else{` |
|      45 | 4433 | `		iLen = ph7_value_to_int64(apArg[1]);` |
|       - | 4434 | `	}` |
|       - | 4435 | `	/* Point to the internal representation of the input hashmap */` |
|      51 | 4436 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4437 | `	/* php caps abs($length) at HT_MAX_SIZE either direction (INT64_MIN stays` |
|       - | 4438 | `	 * negative through the ABS, failing the guard like php's own ZEND_ABS` |
|       - | 4439 | `	 * overflow). */` |
|      51 | 4440 | `	iAbs = iLen;` |
|      51 | 4441 | `	if( iAbs < 0 && iAbs != (sxi64)-9223372036854775807LL - 1 ){` |
|      15 | 4442 | `		iAbs = -iAbs;` |
|       7 | 4443 | `	}` |
|      51 | 4444 | `	rc = HashmapGuardArraySize(pCtx,"array_pad",2,"$length",iAbs);` |
|      51 | 4445 | `	if( rc != SXRET_OK ){` |
|      15 | 4446 | `		return rc;` |
|       - | 4447 | `	}` |
|      37 | 4448 | `	nEntry = (int)iLen;` |
|       - | 4449 | `	/* Create a new array */` |
|      37 | 4450 | `	pArray = ph7_context_new_array(pCtx);` |
|      37 | 4451 | `	if( pArray == 0 ){` |
|     ! 0 | 4452 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 4453 | `	}` |
|      37 | 4454 | `	if( nEntry < 0 ){` |
|      11 | 4455 | `		nEntry = -nEntry;` |
|      11 | 4456 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 4457 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 4458 | `			/* Insert given items first */` |
|      25 | 4459 | `			while( nEntry > 0 ){` |
|      19 | 4460 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 4461 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 4462 | `				}` |
|      19 | 4463 | `				nEntry--;` |
|       1 | 4464 | `			}` |
|       - | 4465 | `			/* Merge the two arrays */` |
|       7 | 4466 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       4 | 4467 | `		}else{` |
|       5 | 4468 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 4469 | `		}` |
|      32 | 4470 | `	}else if( nEntry > 0 ){` |
|      25 | 4471 | `		if( nEntry > (int)pMap->nEntry ){` |
|      19 | 4472 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 4473 | `			/* Merge the two arrays first */` |
|      19 | 4474 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 4475 | `			/* Insert given items */` |
|     275 | 4476 | `			while( nEntry > 0 ){` |
|     257 | 4477 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 4478 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 4479 | `				}` |
|     257 | 4480 | `				nEntry--;` |
|       1 | 4481 | `			}` |
|      10 | 4482 | `		}else{` |
|       7 | 4483 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 4484 | `		}` |
|      13 | 4485 | `	}else{` |
|       - | 4486 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 4487 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 4488 | `	}` |
|       - | 4489 | `	/* Return the new array */` |
|      37 | 4490 | `	ph7_result_value(pCtx,pArray);` |
|      37 | 4491 | `	return PH7_OK;` |
|      32 | 4492 | `}` |
|       - | 4493 | `/*` |
|       - | 4494 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 4495 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 4496 | ` * Parameters` |
|       - | 4497 | ` * $array` |
|       - | 4498 | ` *   The array in which elements are replaced.` |
|       - | 4499 | ` * $array1` |
|       - | 4500 | ` *   The array from which elements will be extracted.` |
|       - | 4501 | ` * ....` |
|       - | 4502 | ` *  More arrays from which elements will be extracted.` |
|       - | 4503 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 4504 | ` * Return` |
|       - | 4505 | ` *  Returns an array.` |
|       - | 4506 | ` *  Throws ArgumentCountError if no arguments are given.` |
|       - | 4507 | ` *  Throws TypeError if any argument is not an array.` |
|       - | 4508 | ` */` |
|      20 | 4509 | `PH7_PRIVATE int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 4510 | `{` |
|       - | 4511 | `	ph7_hashmap *pMap;` |
|       - | 4512 | `	ph7_value *pArray;` |
|       - | 4513 | `	int i;` |
|      23 | 4514 | `	if( nArg < 1 ){` |
|     ! 0 | 4515 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4516 | `			"ArgumentCountError",` |
|       - | 4517 | `			"array_replace() expects at least 1 argument, 0 given"` |
|       - | 4518 | `			);` |
|       - | 4519 | `	}` |
|      23 | 4520 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4521 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4522 | `			"TypeError",` |
|       - | 4523 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4524 | `			ph7_type_name(apArg[0])` |
|       - | 4525 | `			);` |
|       - | 4526 | `	}` |
|       - | 4527 | `	/* Create a new array */` |
|      20 | 4528 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 4529 | `	if( pArray == 0 ){` |
|     ! 0 | 4530 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4531 | `		return PH7_OK;` |
|       - | 4532 | `	}` |
|       - | 4533 | `	/* Overwrite from the first array */` |
|      20 | 4534 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 4535 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 4536 | `	/* Perform the requested operation for remaining arrays */` |
|      36 | 4537 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4538 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 4539 | `			/* Type mismatch -> TypeError */` |
|       4 | 4540 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4541 | `				"TypeError",` |
|       - | 4542 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|       1 | 4543 | `				i + 1,` |
|       2 | 4544 | `				ph7_type_name(apArg[i])` |
|       - | 4545 | `				);` |
|       - | 4546 | `		}` |
|       - | 4547 | `		/* Point to the internal representation of the input hashmap */` |
|      17 | 4548 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      17 | 4549 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       9 | 4550 | `	}` |
|       - | 4551 | `	/* Return the new array */` |
|      17 | 4552 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 4553 | `	return PH7_OK;` |
|      13 | 4554 | `}` |
|       - | 4555 | `/*` |
|       - | 4556 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 4557 | ` *  Filters elements of an array using a callback function.` |
|       - | 4558 | ` * Parameters` |
|       - | 4559 | ` *  $input` |
|       - | 4560 | ` *    The array to iterate over` |
|       - | 4561 | ` * $callback` |
|       - | 4562 | ` *    The callback function to use` |
|       - | 4563 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 4564 | ` *    will be removed.` |
|       - | 4565 | ` * Return` |
|       - | 4566 | ` *  The filtered array.` |
|       - | 4567 | ` */` |
|      28 | 4568 | `PH7_PRIVATE int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4569 | `{` |
|       - | 4570 | `	ph7_hashmap_node *pEntry;` |
|       - | 4571 | `	ph7_hashmap *pMap;` |
|       - | 4572 | `	ph7_value *pArray;` |
|       - | 4573 | `	ph7_value sResult;   /* Callback result */` |
|       - | 4574 | `	ph7_value *pValue;` |
|       - | 4575 | `	sxi32 rc;` |
|       - | 4576 | `	int keep;` |
|       - | 4577 | `	sxu32 n;` |
|      30 | 4578 | `	if( nArg < 1 ){` |
|       - | 4579 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|     ! 0 | 4580 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4581 | `		return PH7_OK;` |
|       - | 4582 | `	}` |
|       - | 4583 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|      30 | 4584 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4585 | `		char zBuf[64];` |
|      16 | 4586 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4587 | `			"TypeError",` |
|       - | 4588 | `			"array_filter(): Argument #1 ($array) must be of type array, %s given",` |
|       5 | 4589 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4590 | `			);` |
|       - | 4591 | `	}` |
|       - | 4592 | `	/* Create a new array */` |
|      20 | 4593 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 4594 | `	if( pArray == 0 ){` |
|     ! 0 | 4595 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4596 | `		return PH7_OK;` |
|       - | 4597 | `	}` |
|       - | 4598 | `	/* Point to the internal representation of the input hashmap */` |
|      20 | 4599 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 4600 | `	pEntry = pMap->pFirst;` |
|      20 | 4601 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      20 | 4602 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 4603 | `	/* Perform the requested operation */` |
|      78 | 4604 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 4605 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      64 | 4606 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      64 | 4607 | `		if( pValue == 0 ){` |
|       - | 4608 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 4609 | `			keep = FALSE;` |
|      64 | 4610 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 4611 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|       - | 4612 | `				* TypeError when the value is not callable or null; prior PH7` |
|       - | 4613 | `				* silently dropped the element.  Emit similar message. */` |
|      36 | 4614 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|       3 | 4615 | `				if( ph7_value_is_string(apArg[1]) ){` |
|       - | 4616 | `					int len;` |
|       3 | 4617 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|       4 | 4618 | `					return PH7_VmThrowException(pCtx,` |
|       - | 4619 | `						"TypeError",` |
|       - | 4620 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|       1 | 4621 | `						zName` |
|       - | 4622 | `						);` |
|     ! 0 | 4623 | `				}else{` |
|     ! 0 | 4624 | `					return PH7_VmThrowException(pCtx,` |
|       - | 4625 | `						"TypeError",` |
|       - | 4626 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|     ! 0 | 4627 | `						ph7_type_name(apArg[1])` |
|       - | 4628 | `						);` |
|       - | 4629 | `				}` |
|       - | 4630 | `			}` |
|      33 | 4631 | `			keep = FALSE;` |
|      33 | 4632 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      33 | 4633 | `			if( rc == PH7_EXCEPTION ){` |
|       - | 4634 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 4635 | `				PH7_MemObjRelease(&sResult);` |
|       3 | 4636 | `				return PH7_EXCEPTION;` |
|       - | 4637 | `			}` |
|      31 | 4638 | `			if( rc == SXRET_OK ){` |
|       - | 4639 | `				/* Perform a boolean cast */` |
|      31 | 4640 | `				keep = ph7_value_to_bool(&sResult);` |
|      15 | 4641 | `			}` |
|      31 | 4642 | `			PH7_MemObjRelease(&sResult);` |
|      16 | 4643 | `		}else{` |
|       - | 4644 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 4645 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 4646 | `			 * the case where the callback argument is missing entirely.` |
|       - | 4647 | `			 */` |
|      29 | 4648 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 4649 | `		}` |
|      59 | 4650 | `		if( keep ){` |
|       - | 4651 | `			/* Perform the insertion,now the callback returned true */` |
|      21 | 4652 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      10 | 4653 | `		}` |
|       - | 4654 | `		/* Point to the next entry */` |
|      59 | 4655 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      30 | 4656 | `	}` |
|      15 | 4657 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 4658 | `	return PH7_OK;` |
|      16 | 4659 | `}` |
|       - | 4660 | `/*` |
|       - | 4661 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|       - | 4662 | ` *  Applies the callback to the elements of the given arrays.` |
|       - | 4663 | ` * Parameters` |
|       - | 4664 | ` *  $callback` |
|       - | 4665 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|       - | 4666 | ` *   array and a NULL callback this is the identity function (the array is` |
|       - | 4667 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|       - | 4668 | ` *   are zipped together.` |
|       - | 4669 | ` *  $array` |
|       - | 4670 | ` *   The first array to run through the callback function.` |
|       - | 4671 | ` *  $arrays` |
|       - | 4672 | ` *   Zero or more additional arrays to process in parallel.` |
|       - | 4673 | ` * Return` |
|       - | 4674 | ` *  Returns an array containing the results of applying the callback function.` |
|       - | 4675 | ` *  With a single array the keys are preserved; with several arrays the result` |
|       - | 4676 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|       - | 4677 | ` *  padding shorter arrays with NULL.` |
|       - | 4678 | ` */` |
|      88 | 4679 | `PH7_PRIVATE int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4680 | `{` |
|       - | 4681 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 4682 | `	ph7_hashmap_node *pEntry;` |
|       - | 4683 | `	ph7_hashmap *pMap;` |
|       - | 4684 | `	ph7_vm *pVm;` |
|       - | 4685 | `	int bNullCallback;` |
|       - | 4686 | `	sxi32 rc;` |
|       - | 4687 | `	int i;` |
|       - | 4688 | `	sxu32 n;` |
|      93 | 4689 | `	if( nArg < 2 ){` |
|     ! 0 | 4690 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4691 | `			"ArgumentCountError",` |
|       - | 4692 | `			"array_map() expects at least 2 arguments, %d given",` |
|     ! 0 | 4693 | `			nArg` |
|       - | 4694 | `			);` |
|       - | 4695 | `	}` |
|      93 | 4696 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|      93 | 4697 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|       8 | 4698 | `		if( ph7_value_is_string(apArg[0]) ){` |
|       6 | 4699 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|       8 | 4700 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4701 | `				"TypeError",` |
|       - | 4702 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 4703 | `				"function \"%s\" not found or invalid function name",` |
|       2 | 4704 | `				zFunc` |
|       - | 4705 | `				);` |
|       - | 4706 | `		}` |
|       3 | 4707 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4708 | `			"TypeError",` |
|       - | 4709 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 4710 | `			"no array or string given"` |
|       - | 4711 | `			);` |
|       - | 4712 | `	}` |
|       - | 4713 | `	/* Every remaining argument must be an array */` |
|     178 | 4714 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      98 | 4715 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       3 | 4716 | `			if( i == 1 ){` |
|       4 | 4717 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4718 | `					"TypeError",` |
|       - | 4719 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|       2 | 4720 | `					ph7_type_name(apArg[1])` |
|       - | 4721 | `					);` |
|       - | 4722 | `			}` |
|     ! 0 | 4723 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4724 | `				"TypeError",` |
|       - | 4725 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|     ! 0 | 4726 | `				i+1,ph7_type_name(apArg[i])` |
|       - | 4727 | `				);` |
|       - | 4728 | `		}` |
|      49 | 4729 | `	}` |
|      83 | 4730 | `	pVm = pCtx->pVm;` |
|       - | 4731 | `	/* Create a new array */` |
|      83 | 4732 | `	pArray = ph7_context_new_array(pCtx);` |
|      83 | 4733 | `	if( pArray == 0 ){` |
|     ! 0 | 4734 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4735 | `		return PH7_OK;` |
|       - | 4736 | `	}` |
|      83 | 4737 | `	PH7_MemObjInit(pVm,&sResult);` |
|      83 | 4738 | `	PH7_MemObjInit(pVm,&sKey);` |
|      83 | 4739 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      83 | 4740 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|      83 | 4741 | `	if( nArg == 2 ){` |
|       - | 4742 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|      73 | 4743 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      73 | 4744 | `		pEntry = pMap->pFirst;` |
|     257 | 4745 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 4746 | `			/* Extract the node value */` |
|     191 | 4747 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|     191 | 4748 | `			if( pValue ){` |
|       - | 4749 | `				/* Extract the node key */` |
|     191 | 4750 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|     191 | 4751 | `				if( bNullCallback ){` |
|       - | 4752 | `					/* NULL callback: identity function, keep original value */` |
|      11 | 4753 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|       6 | 4754 | `				}else{` |
|       - | 4755 | `					/* Invoke the supplied callback */` |
|     181 | 4756 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|     181 | 4757 | `					if( rc == PH7_EXCEPTION ){` |
|       - | 4758 | `						/* Callback raised: abort and let the foreign-function` |
|       - | 4759 | `						 * dispatcher unwind through the nearest try/catch. */` |
|       5 | 4760 | `						PH7_MemObjRelease(&sKey);` |
|       5 | 4761 | `						PH7_MemObjRelease(&sResult);` |
|       5 | 4762 | `						return PH7_EXCEPTION;` |
|       - | 4763 | `					}` |
|       - | 4764 | `					/* Insert the callback return value */` |
|     177 | 4765 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 4766 | `				}` |
|     187 | 4767 | `				PH7_MemObjRelease(&sKey);` |
|     187 | 4768 | `				PH7_MemObjRelease(&sResult);` |
|      92 | 4769 | `			}` |
|       - | 4770 | `			/* Point to the next entry */` |
|     187 | 4771 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      95 | 4772 | `		}` |
|      36 | 4773 | `	}else{` |
|       - | 4774 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|       - | 4775 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|      11 | 4776 | `		int nArrays = nArg - 1;` |
|       - | 4777 | `		ph7_hashmap_node **apCur;` |
|       - | 4778 | `		ph7_value **apCallArg;` |
|       - | 4779 | `		ph7_value sNull;` |
|      11 | 4780 | `		sxu32 nMax = 0;` |
|      11 | 4781 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|      11 | 4782 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|      11 | 4783 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|     ! 0 | 4784 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|     ! 0 | 4785 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|     ! 0 | 4786 | `			PH7_MemObjRelease(&sKey);` |
|     ! 0 | 4787 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 4788 | `			ph7_result_value(pCtx,pArray);` |
|     ! 0 | 4789 | `			return PH7_OK;` |
|       - | 4790 | `		}` |
|      11 | 4791 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|      11 | 4792 | `		sNull.nIdx = SXU32_HIGH;` |
|      33 | 4793 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|      23 | 4794 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|      23 | 4795 | `			apCur[i] = pMap->pFirst;` |
|      23 | 4796 | `			if( pMap->nEntry > nMax ){` |
|      13 | 4797 | `				nMax = pMap->nEntry;` |
|       6 | 4798 | `			}` |
|      12 | 4799 | `		}` |
|      35 | 4800 | `		for( n = 0 ; n < nMax ; n++ ){` |
|      25 | 4801 | `			ph7_value *pZip = 0;` |
|      25 | 4802 | `			if( bNullCallback ){` |
|       - | 4803 | `				/* zip: each result element is an array of the i-th values */` |
|       5 | 4804 | `				pZip = ph7_context_new_array(pCtx);` |
|       2 | 4805 | `			}` |
|      79 | 4806 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|      55 | 4807 | `				ph7_value *pv = &sNull;` |
|      55 | 4808 | `				if( apCur[i] ){` |
|      53 | 4809 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|      53 | 4810 | `					if( pNodeVal ){` |
|      53 | 4811 | `						pv = pNodeVal;` |
|      26 | 4812 | `					}` |
|      53 | 4813 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|      26 | 4814 | `				}` |
|      55 | 4815 | `				if( bNullCallback ){` |
|       9 | 4816 | `					if( pZip ){` |
|       9 | 4817 | `						ph7_array_add_elem(pZip,0,pv);` |
|       4 | 4818 | `					}` |
|       5 | 4819 | `				}else{` |
|      47 | 4820 | `					apCallArg[i] = pv;` |
|       - | 4821 | `				}` |
|      28 | 4822 | `			}` |
|      25 | 4823 | `			if( bNullCallback ){` |
|       5 | 4824 | `				if( pZip ){` |
|       5 | 4825 | `					ph7_array_add_elem(pArray,0,pZip);` |
|       2 | 4826 | `				}` |
|       3 | 4827 | `			}else{` |
|      21 | 4828 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|      21 | 4829 | `				if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 4830 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|     ! 0 | 4831 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|     ! 0 | 4832 | `					PH7_MemObjRelease(&sNull);` |
|     ! 0 | 4833 | `					PH7_MemObjRelease(&sKey);` |
|     ! 0 | 4834 | `					PH7_MemObjRelease(&sResult);` |
|     ! 0 | 4835 | `					return PH7_EXCEPTION;` |
|       - | 4836 | `				}` |
|      21 | 4837 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|      21 | 4838 | `				PH7_MemObjRelease(&sResult);` |
|       - | 4839 | `			}` |
|      13 | 4840 | `		}` |
|      11 | 4841 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|      11 | 4842 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|      11 | 4843 | `		PH7_MemObjRelease(&sNull);` |
|       - | 4844 | `	}` |
|      79 | 4845 | `	PH7_MemObjRelease(&sKey);` |
|      79 | 4846 | `	PH7_MemObjRelease(&sResult);` |
|      79 | 4847 | `	ph7_result_value(pCtx,pArray);` |
|      79 | 4848 | `	return PH7_OK;` |
|      49 | 4849 | `}` |
|       - | 4850 | `/*` |
|       - | 4851 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|       - | 4852 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 4853 | ` * Parameters` |
|       - | 4854 | ` *  $array` |
|       - | 4855 | ` *   The input array.` |
|       - | 4856 | ` *  $callback` |
|       - | 4857 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|       - | 4858 | ` *  $initial` |
|       - | 4859 | ` *   If the optional initial is available, it will be used at the beginning` |
|       - | 4860 | ` *   of the process, or as a final result in case the array is empty.` |
|       - | 4861 | ` * Return` |
|       - | 4862 | ` *  Returns the resulting value.` |
|       - | 4863 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 4864 | ` */` |
|      28 | 4865 | `PH7_PRIVATE int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4866 | `{` |
|       - | 4867 | `	ph7_hashmap_node *pEntry;` |
|       - | 4868 | `	ph7_hashmap *pMap;` |
|       - | 4869 | `	ph7_value *pValue;` |
|       - | 4870 | `	ph7_value sResult;` |
|       - | 4871 | `	sxi32 rc;` |
|       - | 4872 | `	sxu32 n;` |
|      33 | 4873 | `	if( nArg < 2 ){` |
|     ! 0 | 4874 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4875 | `			"ArgumentCountError",` |
|       - | 4876 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|     ! 0 | 4877 | `			nArg` |
|       - | 4878 | `			);` |
|       - | 4879 | `	}` |
|      33 | 4880 | `	if( nArg > 3 ){` |
|     ! 0 | 4881 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4882 | `			"ArgumentCountError",` |
|       - | 4883 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|     ! 0 | 4884 | `			nArg` |
|       - | 4885 | `			);` |
|       - | 4886 | `	}` |
|      33 | 4887 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4888 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4889 | `			"TypeError",` |
|       - | 4890 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4891 | `			ph7_type_name(apArg[0])` |
|       - | 4892 | `			);` |
|       - | 4893 | `	}` |
|      31 | 4894 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      12 | 4895 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 4896 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 4897 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4898 | `				"TypeError",` |
|       - | 4899 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 4900 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 4901 | `				zFunc` |
|       - | 4902 | `				);` |
|       - | 4903 | `		}` |
|       9 | 4904 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       3 | 4905 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4906 | `				"TypeError",` |
|       - | 4907 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 4908 | `				"array callback must have exactly two members"` |
|       - | 4909 | `				);` |
|       - | 4910 | `		}` |
|       6 | 4911 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4912 | `			"TypeError",` |
|       - | 4913 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 4914 | `			"no array or string given"` |
|       - | 4915 | `			);` |
|       - | 4916 | `	}` |
|       - | 4917 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 4918 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4919 | `	/* Assume a NULL initial value */` |
|      19 | 4920 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      19 | 4921 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      19 | 4922 | `	if( nArg > 2 ){` |
|       - | 4923 | `		/* Set the initial value */` |
|      13 | 4924 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       6 | 4925 | `	}` |
|       - | 4926 | `	/* Perform the requested operation */` |
|      19 | 4927 | `	pEntry = pMap->pFirst;` |
|      55 | 4928 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 4929 | `		/* Extract the node value */` |
|      39 | 4930 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 4931 | `		/* Invoke the supplied callback */` |
|      39 | 4932 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|      39 | 4933 | `		if( rc == PH7_EXCEPTION ){` |
|       - | 4934 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 4935 | `			PH7_MemObjRelease(&sResult);` |
|       3 | 4936 | `			return PH7_EXCEPTION;` |
|       - | 4937 | `		}` |
|       - | 4938 | `		/* Point to the next entry */` |
|      37 | 4939 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 4940 | `	}` |
|      17 | 4941 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      17 | 4942 | `	PH7_MemObjRelease(&sResult);` |
|      17 | 4943 | `	return PH7_OK;` |
|      19 | 4944 | `}` |
|       - | 4945 | `/*` |
|       - | 4946 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 4947 | ` *  Apply a user function to every member of an array.` |
|       - | 4948 | ` * Parameters` |
|       - | 4949 | ` *  $array` |
|       - | 4950 | ` *   The input array.` |
|       - | 4951 | ` *  $funcname` |
|       - | 4952 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 4953 | ` *   the first, and the key/index second.` |
|       - | 4954 | ` * Note:` |
|       - | 4955 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 4956 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 4957 | ` *  be made in the original array itself.` |
|       - | 4958 | ` *  $userdata` |
|       - | 4959 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 4960 | ` *   to the callback funcname.` |
|       - | 4961 | ` * Return` |
|       - | 4962 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 4963 | ` */` |
|      36 | 4964 | `PH7_PRIVATE int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4965 | `{` |
|       - | 4966 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 4967 | `	ph7_hashmap_node *pEntry;` |
|       - | 4968 | `	ph7_hashmap *pMap;` |
|       - | 4969 | `	sxu32 n;` |
|      41 | 4970 | `	if( nArg < 2 ){` |
|     ! 0 | 4971 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4972 | `			"ArgumentCountError",` |
|       - | 4973 | `			"array_walk() expects at least 2 arguments, %d given",` |
|     ! 0 | 4974 | `			nArg` |
|       - | 4975 | `			);` |
|       - | 4976 | `	}` |
|      41 | 4977 | `	if( nArg > 3 ){` |
|     ! 0 | 4978 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4979 | `			"ArgumentCountError",` |
|       - | 4980 | `			"array_walk() expects at most 3 arguments, %d given",` |
|     ! 0 | 4981 | `			nArg` |
|       - | 4982 | `			);` |
|       - | 4983 | `	}` |
|      41 | 4984 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4985 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4986 | `			"TypeError",` |
|       - | 4987 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4988 | `			ph7_type_name(apArg[0])` |
|       - | 4989 | `			);` |
|       - | 4990 | `	}` |
|      39 | 4991 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      17 | 4992 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       6 | 4993 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       8 | 4994 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4995 | `				"TypeError",` |
|       - | 4996 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 4997 | `				"function \"%s\" not found or invalid function name",` |
|       2 | 4998 | `				zFunc` |
|       - | 4999 | `				);` |
|       - | 5000 | `		}` |
|      12 | 5001 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       6 | 5002 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5003 | `				"TypeError",` |
|       - | 5004 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 5005 | `				"array callback must have exactly two members"` |
|       - | 5006 | `				);` |
|       - | 5007 | `		}` |
|       6 | 5008 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5009 | `			"TypeError",` |
|       - | 5010 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 5011 | `			"no array or string given"` |
|       - | 5012 | `			);` |
|       - | 5013 | `	}` |
|      23 | 5014 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 5015 | `	/* Point to the internal representation of the input hashmap */` |
|      23 | 5016 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      23 | 5017 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 5018 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      23 | 5019 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 5020 | `	/* Perform the desired operation */` |
|      23 | 5021 | `	pEntry = pMap->pFirst;` |
|      69 | 5022 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5023 | `		/* Extract the node value */` |
|      49 | 5024 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      49 | 5025 | `		if( pValue ){` |
|       - | 5026 | `			sxi32 rcW;` |
|       - | 5027 | `			/* Extract the entry key */` |
|      49 | 5028 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 5029 | `			/* Invoke the supplied callback */` |
|      49 | 5030 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      49 | 5031 | `			PH7_MemObjRelease(&sKey);` |
|      49 | 5032 | `			if( rcW == PH7_EXCEPTION ){` |
|       - | 5033 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 5034 | `				return PH7_EXCEPTION;` |
|       - | 5035 | `			}` |
|      23 | 5036 | `		}` |
|       - | 5037 | `		/* Point to the next entry */` |
|      47 | 5038 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      24 | 5039 | `	}` |
|       - | 5040 | `	/* All done, return TRUE */` |
|      21 | 5041 | `	ph7_result_bool(pCtx,1);` |
|      21 | 5042 | `	return PH7_OK;` |
|      23 | 5043 | `}` |
|       - | 5044 | `/*` |
|       - | 5045 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 5046 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 5047 | ` */` |
|      22 | 5048 | `static sxi32 HashmapWalkRecursive(` |
|       - | 5049 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 5050 | `	ph7_value *pCallback, /* User callback */` |
|       - | 5051 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 5052 | `	int iNest             /* Nesting level */` |
|       - | 5053 | `	)` |
|       1 | 5054 | `{` |
|       - | 5055 | `	ph7_hashmap_node *pEntry;` |
|       - | 5056 | `	ph7_value *pValue,sKey;` |
|       - | 5057 | `	sxi32 rc;` |
|       - | 5058 | `	sxu32 n;` |
|       - | 5059 | `	/* Iterate through hashmap entries */` |
|      23 | 5060 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      23 | 5061 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      23 | 5062 | `	pEntry = pMap->pFirst;` |
|      59 | 5063 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5064 | `		/* Extract the node value */` |
|      37 | 5065 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      37 | 5066 | `		if( pValue ){` |
|      37 | 5067 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      11 | 5068 | `				if( iNest < 32 ){` |
|       - | 5069 | `					/* Recurse */` |
|      11 | 5070 | `					iNest++;` |
|      11 | 5071 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      11 | 5072 | `					iNest--;` |
|      11 | 5073 | `					if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 5074 | `						return PH7_EXCEPTION;` |
|       - | 5075 | `					}` |
|       5 | 5076 | `				}` |
|       6 | 5077 | `			}else{` |
|       - | 5078 | `				/* Extract the node key */` |
|      27 | 5079 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 5080 | `				/* Invoke the supplied callback */` |
|      27 | 5081 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|      27 | 5082 | `				PH7_MemObjRelease(&sKey);` |
|      27 | 5083 | `				if( rc == PH7_EXCEPTION ){` |
|       - | 5084 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 5085 | `					return PH7_EXCEPTION;` |
|       - | 5086 | `				}` |
|       - | 5087 | `			}` |
|      18 | 5088 | `		}` |
|       - | 5089 | `		/* Point to the next entry */` |
|      37 | 5090 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 5091 | `	}` |
|      23 | 5092 | `	return PH7_OK;` |
|      12 | 5093 | `}` |
|       - | 5094 | `/*` |
|       - | 5095 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 5096 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 5097 | ` * Parameters` |
|       - | 5098 | ` *  $array` |
|       - | 5099 | ` *   The input array.` |
|       - | 5100 | ` *  $funcname` |
|       - | 5101 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 5102 | ` *   the first, and the key/index second.` |
|       - | 5103 | ` * Note:` |
|       - | 5104 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 5105 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 5106 | ` *  be made in the original array itself.` |
|       - | 5107 | ` *  $userdata` |
|       - | 5108 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 5109 | ` *   to the callback funcname.` |
|       - | 5110 | ` * Return` |
|       - | 5111 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 5112 | ` */` |
|      24 | 5113 | `PH7_PRIVATE int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5114 | `{` |
|       - | 5115 | `	ph7_hashmap *pMap;` |
|      29 | 5116 | `	if( nArg < 2 ){` |
|     ! 0 | 5117 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5118 | `			"ArgumentCountError",` |
|       - | 5119 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|     ! 0 | 5120 | `			nArg` |
|       - | 5121 | `			);` |
|       - | 5122 | `	}` |
|      29 | 5123 | `	if( nArg > 3 ){` |
|     ! 0 | 5124 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5125 | `			"ArgumentCountError",` |
|       - | 5126 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|     ! 0 | 5127 | `			nArg` |
|       - | 5128 | `			);` |
|       - | 5129 | `	}` |
|      29 | 5130 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5131 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5132 | `			"TypeError",` |
|       - | 5133 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5134 | `			ph7_type_name(apArg[0])` |
|       - | 5135 | `			);` |
|       - | 5136 | `	}` |
|      27 | 5137 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      14 | 5138 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 5139 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 5140 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5141 | `				"TypeError",` |
|       - | 5142 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 5143 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 5144 | `				zFunc` |
|       - | 5145 | `				);` |
|       - | 5146 | `		}` |
|      12 | 5147 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       6 | 5148 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5149 | `				"TypeError",` |
|       - | 5150 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 5151 | `				"array callback must have exactly two members"` |
|       - | 5152 | `				);` |
|       - | 5153 | `		}` |
|       6 | 5154 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5155 | `			"TypeError",` |
|       - | 5156 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 5157 | `			"no array or string given"` |
|       - | 5158 | `			);` |
|       - | 5159 | `	}` |
|       - | 5160 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 5161 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      13 | 5162 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5163 | `	/* Perform the desired operation */` |
|      13 | 5164 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|       - | 5165 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 5166 | `		return PH7_EXCEPTION;` |
|       - | 5167 | `	}` |
|       - | 5168 | `	/* All done, return TRUE */` |
|      13 | 5169 | `	ph7_result_bool(pCtx,1);` |
|      13 | 5170 | `	return PH7_OK;` |
|      17 | 5171 | `}` |
|       - | 5172 | `/*` |
|       - | 5173 | ` * bool array_is_list(array $array)` |
|       - | 5174 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|       - | 5175 | ` *  integers starting at 0. An empty array is a list.` |
|       - | 5176 | ` * Return` |
|       - | 5177 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|       - | 5178 | ` */` |
|       - | 5179 | `/*` |
|       - | 5180 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|       - | 5181 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|       - | 5182 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|       - | 5183 | ` */` |
|     360 | 5184 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|       3 | 5185 | `{` |
|     363 | 5186 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|     363 | 5187 | `	sxi64 iExpect = 0;` |
|       - | 5188 | `	sxu32 n;` |
|     841 | 5189 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     623 | 5190 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|       - | 5191 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|     145 | 5192 | `			return 0;` |
|       - | 5193 | `		}` |
|     481 | 5194 | `		++iExpect;` |
|     481 | 5195 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     242 | 5196 | `	}` |
|     221 | 5197 | `	return 1;` |
|     183 | 5198 | `}` |
|      12 | 5199 | `PH7_PRIVATE int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5200 | `{` |
|      13 | 5201 | `	if( nArg < 1 ){` |
|     ! 0 | 5202 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5203 | `			"ArgumentCountError",` |
|       - | 5204 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|       - | 5205 | `			);` |
|       - | 5206 | `	}` |
|      13 | 5207 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5208 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5209 | `			"TypeError",` |
|       - | 5210 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5211 | `			ph7_type_name(apArg[0])` |
|       - | 5212 | `			);` |
|       - | 5213 | `	}` |
|      13 | 5214 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|      13 | 5215 | `	return PH7_OK;` |
|       7 | 5216 | `}` |
|       - | 5217 | `/*` |
|       - | 5218 | ` * mixed array_first(array $array)` |
|       - | 5219 | ` * mixed array_last(array $array)` |
|       - | 5220 | ` *  Return the value of the first (respectively last) element of the array,` |
|       - | 5221 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|       - | 5222 | ` *  untouched (unlike reset()/end()).` |
|       - | 5223 | ` */` |
|      18 | 5224 | `static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|       1 | 5225 | `{` |
|       - | 5226 | `	ph7_hashmap *pMap;` |
|       - | 5227 | `	ph7_hashmap_node *pNode;` |
|       - | 5228 | `	ph7_value *pVal;` |
|      19 | 5229 | `	const char *zName = bLast ? "array_last" : "array_first";` |
|      19 | 5230 | `	if( nArg < 1 ){` |
|     ! 0 | 5231 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5232 | `			"ArgumentCountError",` |
|       - | 5233 | `			"%s() expects exactly 1 argument, 0 given",` |
|     ! 0 | 5234 | `			zName` |
|       - | 5235 | `			);` |
|       - | 5236 | `	}` |
|      19 | 5237 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5238 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5239 | `			"TypeError",` |
|       - | 5240 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5241 | `			zName,` |
|       1 | 5242 | `			ph7_type_name(apArg[0])` |
|       - | 5243 | `			);` |
|       - | 5244 | `	}` |
|      17 | 5245 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      17 | 5246 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|      17 | 5247 | `	if( pNode == 0 ){` |
|       - | 5248 | `		/* Empty array: PHP returns NULL */` |
|       5 | 5249 | `		ph7_result_null(pCtx);` |
|       5 | 5250 | `		return PH7_OK;` |
|       - | 5251 | `	}` |
|      13 | 5252 | `	pVal = HashmapExtractNodeValue(pNode);` |
|      13 | 5253 | `	if( pVal ){` |
|      13 | 5254 | `		ph7_result_value(pCtx,pVal);` |
|       7 | 5255 | `	}else{` |
|     ! 0 | 5256 | `		ph7_result_null(pCtx);` |
|       - | 5257 | `	}` |
|      13 | 5258 | `	return PH7_OK;` |
|      10 | 5259 | `}` |
|       8 | 5260 | `PH7_PRIVATE int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5261 | `{` |
|       9 | 5262 | `	return HashmapFirstLast(pCtx,nArg,apArg,0);` |
|       1 | 5263 | `}` |
|      10 | 5264 | `PH7_PRIVATE int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5265 | `{` |
|      11 | 5266 | `	return HashmapFirstLast(pCtx,nArg,apArg,1);` |
|       1 | 5267 | `}` |
|       - | 5268 | `/*` |
|       - | 5269 | ` * int\|string\|null array_key_first(array $array)` |
|       - | 5270 | ` * int\|string\|null array_key_last(array $array)` |
|       - | 5271 | ` *  Return the key of the first (respectively last) element of the array,` |
|       - | 5272 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|       - | 5273 | ` *  untouched.` |
|       - | 5274 | ` */` |
|      22 | 5275 | `static int HashmapKeyFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|       1 | 5276 | `{` |
|       - | 5277 | `	ph7_hashmap *pMap;` |
|       - | 5278 | `	ph7_hashmap_node *pNode;` |
|      23 | 5279 | `	const char *zName = bLast ? "array_key_last" : "array_key_first";` |
|      23 | 5280 | `	if( nArg < 1 ){` |
|     ! 0 | 5281 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5282 | `			"ArgumentCountError",` |
|       - | 5283 | `			"%s() expects exactly 1 argument, 0 given",` |
|     ! 0 | 5284 | `			zName` |
|       - | 5285 | `			);` |
|       - | 5286 | `	}` |
|      23 | 5287 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5288 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5289 | `			"TypeError",` |
|       - | 5290 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5291 | `			zName,` |
|       1 | 5292 | `			ph7_type_name(apArg[0])` |
|       - | 5293 | `			);` |
|       - | 5294 | `	}` |
|      21 | 5295 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      21 | 5296 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|      21 | 5297 | `	if( pNode == 0 ){` |
|       - | 5298 | `		/* Empty array: PHP returns NULL */` |
|       5 | 5299 | `		ph7_result_null(pCtx);` |
|       5 | 5300 | `		return PH7_OK;` |
|       - | 5301 | `	}` |
|      17 | 5302 | `	HashmapResultNodeKey(pCtx,pNode);` |
|      17 | 5303 | `	return PH7_OK;` |
|      12 | 5304 | `}` |
|      10 | 5305 | `PH7_PRIVATE int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5306 | `{` |
|      11 | 5307 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,0);` |
|       1 | 5308 | `}` |
|      12 | 5309 | `PH7_PRIVATE int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5310 | `{` |
|      13 | 5311 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,1);` |
|       1 | 5312 | `}` |
|       - | 5313 | `/*` |
|       - | 5314 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|       - | 5315 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|       - | 5316 | ` * array_column() for both the column value and the index key.` |
|       - | 5317 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|       - | 5318 | ` * container or the key is absent.` |
|       - | 5319 | ` */` |
|      32 | 5320 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|       1 | 5321 | `{` |
|      33 | 5322 | `	if( ph7_value_is_array(pRow) ){` |
|       - | 5323 | `		ph7_hashmap_node *pNode;` |
|      25 | 5324 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|      21 | 5325 | `			return HashmapExtractNodeValue(pNode);` |
|       1 | 5326 | `		}` |
|      11 | 5327 | `	}else if( ph7_value_is_object(pRow) ){` |
|       - | 5328 | `		ph7_value sName;` |
|       - | 5329 | `		const char *zName;` |
|       - | 5330 | `		ph7_value *pAttr;` |
|       - | 5331 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|       - | 5332 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|       9 | 5333 | `		PH7_MemObjInit(pVm,&sName);` |
|       9 | 5334 | `		PH7_MemObjStore(pKey,&sName);` |
|       9 | 5335 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|       9 | 5336 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|       9 | 5337 | `		PH7_MemObjRelease(&sName);` |
|       9 | 5338 | `		return pAttr;` |
|       - | 5339 | `	}` |
|       5 | 5340 | `	return 0;` |
|      17 | 5341 | `}` |
|       - | 5342 | `/*` |
|       - | 5343 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|       - | 5344 | ` *  Returns the values from a single column of the input, identified by` |
|       - | 5345 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|       - | 5346 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|       - | 5347 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|       - | 5348 | ` *  Each row may be an array or an object.` |
|       - | 5349 | ` */` |
|      12 | 5350 | `PH7_PRIVATE int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5351 | `{` |
|       - | 5352 | `	ph7_hashmap_node *pNode;` |
|       - | 5353 | `	ph7_hashmap *pMap;` |
|       - | 5354 | `	ph7_value *pArray;` |
|       - | 5355 | `	ph7_value *pRow;` |
|       - | 5356 | `	ph7_value *pCol;` |
|       - | 5357 | `	ph7_value *pIdx;` |
|       - | 5358 | `	int bWantCol;` |
|       - | 5359 | `	int bWantIdx;` |
|       - | 5360 | `	sxu32 n;` |
|      13 | 5361 | `	if( nArg < 2 ){` |
|     ! 0 | 5362 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5363 | `			"ArgumentCountError",` |
|       - | 5364 | `			"array_column() expects at least 2 arguments, %d given",` |
|     ! 0 | 5365 | `			nArg` |
|       - | 5366 | `			);` |
|       - | 5367 | `	}` |
|      13 | 5368 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5369 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5370 | `			"TypeError",` |
|       - | 5371 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5372 | `			ph7_type_name(apArg[0])` |
|       - | 5373 | `			);` |
|       - | 5374 | `	}` |
|      13 | 5375 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      13 | 5376 | `	pArray = ph7_context_new_array(pCtx);` |
|      13 | 5377 | `	if( pArray == 0 ){` |
|     ! 0 | 5378 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5379 | `		return PH7_OK;` |
|       - | 5380 | `	}` |
|       - | 5381 | `	/* A NULL column_key means "collect the entire row". */` |
|      13 | 5382 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|      13 | 5383 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|      13 | 5384 | `	pNode = pMap->pFirst;` |
|      33 | 5385 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      21 | 5386 | `		pRow = HashmapExtractNodeValue(pNode);` |
|      21 | 5387 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|      21 | 5388 | `		if( pRow == 0 ){` |
|     ! 0 | 5389 | `			continue;` |
|       - | 5390 | `		}` |
|      21 | 5391 | `		if( bWantCol ){` |
|      19 | 5392 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|      19 | 5393 | `			if( pCol == 0 ){` |
|       - | 5394 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|       3 | 5395 | `				continue;` |
|       - | 5396 | `			}` |
|       9 | 5397 | `		}else{` |
|       3 | 5398 | `			pCol = pRow;` |
|       - | 5399 | `		}` |
|      19 | 5400 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|      19 | 5401 | `		if( pIdx ){` |
|      13 | 5402 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|       7 | 5403 | `		}else{` |
|       7 | 5404 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|       - | 5405 | `		}` |
|      10 | 5406 | `	}` |
|      13 | 5407 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 5408 | `	return PH7_OK;` |
|       7 | 5409 | `}` |
|       - | 5410 | `/*` |
|       - | 5411 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|       - | 5412 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|       - | 5413 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|       - | 5414 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|       - | 5415 | ` */` |
|      28 | 5416 | `static sxi32 HashmapCallbackSearch(` |
|       - | 5417 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|       - | 5418 | `	const char *zName,            /* Function name for diagnostics */` |
|       - | 5419 | `	int bWant,                    /* Truthiness being hunted for */` |
|       - | 5420 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|       - | 5421 | `	)` |
|       1 | 5422 | `{` |
|       - | 5423 | `	ph7_hashmap_node *pEntry;` |
|       - | 5424 | `	ph7_hashmap *pMap;` |
|       - | 5425 | `	ph7_value *pValue;` |
|       - | 5426 | `	ph7_value *apCbArg[2];` |
|       - | 5427 | `	ph7_value sKey;` |
|       - | 5428 | `	ph7_value sResult;` |
|       - | 5429 | `	sxi32 rc;` |
|       - | 5430 | `	sxu32 n;` |
|      29 | 5431 | `	*ppMatch = 0;` |
|      29 | 5432 | `	if( nArg < 2 ){` |
|     ! 0 | 5433 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5434 | `			"ArgumentCountError",` |
|       - | 5435 | `			"%s() expects exactly 2 arguments, %d given",` |
|     ! 0 | 5436 | `			zName,nArg` |
|       - | 5437 | `			);` |
|       - | 5438 | `	}` |
|      29 | 5439 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5440 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5441 | `			"TypeError",` |
|       - | 5442 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5443 | `			zName,ph7_type_name(apArg[0])` |
|       - | 5444 | `			);` |
|       - | 5445 | `	}` |
|      29 | 5446 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|     ! 0 | 5447 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5448 | `			"TypeError",` |
|       - | 5449 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|     ! 0 | 5450 | `			zName,ph7_type_name(apArg[1])` |
|       - | 5451 | `			);` |
|       - | 5452 | `	}` |
|      29 | 5453 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      29 | 5454 | `	pEntry = pMap->pFirst;` |
|      29 | 5455 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      29 | 5456 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|      29 | 5457 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      29 | 5458 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      73 | 5459 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 5460 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      59 | 5461 | `		if( pValue ){` |
|       - | 5462 | `			/* The callback receives ($value, $key). */` |
|      59 | 5463 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      59 | 5464 | `			apCbArg[0] = pValue;` |
|      59 | 5465 | `			apCbArg[1] = &sKey;` |
|      59 | 5466 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|      59 | 5467 | `			if( rc == PH7_EXCEPTION ){` |
|       - | 5468 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 5469 | `				PH7_MemObjRelease(&sKey);` |
|     ! 0 | 5470 | `				PH7_MemObjRelease(&sResult);` |
|     ! 0 | 5471 | `				return PH7_EXCEPTION;` |
|       - | 5472 | `			}` |
|      59 | 5473 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|      15 | 5474 | `				*ppMatch = pEntry;` |
|      15 | 5475 | `				break;` |
|       - | 5476 | `			}` |
|      22 | 5477 | `		}` |
|      45 | 5478 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      23 | 5479 | `	}` |
|      29 | 5480 | `	PH7_MemObjRelease(&sKey);` |
|      29 | 5481 | `	PH7_MemObjRelease(&sResult);` |
|      29 | 5482 | `	return PH7_OK;` |
|      15 | 5483 | `}` |
|       - | 5484 | `/*` |
|       - | 5485 | ` * mixed array_find(array $array, callable $callback)` |
|       - | 5486 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|       - | 5487 | ` *  is truthy, or NULL if none match.` |
|       - | 5488 | ` */` |
|       6 | 5489 | `PH7_PRIVATE int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5490 | `{` |
|       - | 5491 | `	ph7_hashmap_node *pMatch;` |
|       - | 5492 | `	ph7_value *pVal;` |
|       - | 5493 | `	sxi32 rc;` |
|       7 | 5494 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|       7 | 5495 | `	if( rc != PH7_OK ){` |
|     ! 0 | 5496 | `		return rc;` |
|       - | 5497 | `	}` |
|       7 | 5498 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|       5 | 5499 | `		ph7_result_value(pCtx,pVal);` |
|       3 | 5500 | `	}else{` |
|       3 | 5501 | `		ph7_result_null(pCtx);` |
|       - | 5502 | `	}` |
|       7 | 5503 | `	return PH7_OK;` |
|       4 | 5504 | `}` |
|       - | 5505 | `/*` |
|       - | 5506 | ` * mixed array_find_key(array $array, callable $callback)` |
|       - | 5507 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|       - | 5508 | ` *  is truthy, or NULL if none match.` |
|       - | 5509 | ` */` |
|       6 | 5510 | `PH7_PRIVATE int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5511 | `{` |
|       - | 5512 | `	ph7_hashmap_node *pMatch;` |
|       - | 5513 | `	sxi32 rc;` |
|       7 | 5514 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|       7 | 5515 | `	if( rc != PH7_OK ){` |
|     ! 0 | 5516 | `		return rc;` |
|       - | 5517 | `	}` |
|       7 | 5518 | `	if( pMatch == 0 ){` |
|       3 | 5519 | `		ph7_result_null(pCtx);` |
|       6 | 5520 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|       3 | 5521 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|       2 | 5522 | `	}else{` |
|       4 | 5523 | `		ph7_result_string(pCtx,` |
|       2 | 5524 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|       2 | 5525 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|       - | 5526 | `	}` |
|       7 | 5527 | `	return PH7_OK;` |
|       4 | 5528 | `}` |
|       - | 5529 | `/*` |
|       - | 5530 | ` * bool array_any(array $array, callable $callback)` |
|       - | 5531 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|       - | 5532 | ` *  FALSE for an empty array.` |
|       - | 5533 | ` */` |
|       8 | 5534 | `PH7_PRIVATE int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5535 | `{` |
|       - | 5536 | `	ph7_hashmap_node *pMatch;` |
|       - | 5537 | `	sxi32 rc;` |
|       9 | 5538 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|       9 | 5539 | `	if( rc != PH7_OK ){` |
|     ! 0 | 5540 | `		return rc;` |
|       - | 5541 | `	}` |
|       9 | 5542 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|       9 | 5543 | `	return PH7_OK;` |
|       5 | 5544 | `}` |
|       - | 5545 | `/*` |
|       - | 5546 | ` * bool array_all(array $array, callable $callback)` |
|       - | 5547 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|       - | 5548 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|       - | 5549 | ` */` |
|       8 | 5550 | `PH7_PRIVATE int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5551 | `{` |
|       - | 5552 | `	ph7_hashmap_node *pMatch;` |
|       - | 5553 | `	sxi32 rc;` |
|       9 | 5554 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|       9 | 5555 | `	if( rc != PH7_OK ){` |
|     ! 0 | 5556 | `		return rc;` |
|       - | 5557 | `	}` |
|       9 | 5558 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|       9 | 5559 | `	return PH7_OK;` |
|       5 | 5560 | `}` |
|       - | 5561 | `/*` |
|       - | 5562 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|       - | 5563 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|       - | 5564 | ` */` |
|       - | 5565 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|       - | 5566 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|      80 | 5567 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       4 | 5568 | `{` |
|      84 | 5569 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|      40 | 5570 | `	(void)pVm;` |
|      84 | 5571 | `	p->nCount++;` |
|      84 | 5572 | `	if( p->pArray ){` |
|       - | 5573 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|       - | 5574 | `		 * otherwise append with an auto-assigned int index. */` |
|      70 | 5575 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|      33 | 5576 | `	}` |
|      84 | 5577 | `	return SXRET_OK;` |
|       4 | 5578 | `}` |
|       - | 5579 | `/*` |
|       - | 5580 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|       - | 5581 | ` */` |
|      30 | 5582 | `PH7_PRIVATE int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       4 | 5583 | `{` |
|       - | 5584 | `	struct IterCollect sCol;` |
|       - | 5585 | `	ph7_value *pArray;` |
|       - | 5586 | `	sxi32 rc;` |
|      34 | 5587 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      34 | 5588 | `	pArray = ph7_context_new_array(pCtx);` |
|      34 | 5589 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      34 | 5590 | `	sCol.pArray = pArray;` |
|      34 | 5591 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|      34 | 5592 | `	sCol.nCount = 0;` |
|      34 | 5593 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       - | 5594 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|       3 | 5595 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 5596 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 5597 | `		sxu32 n;` |
|       9 | 5598 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5599 | `			ph7_value sKey, *pVal;` |
|       7 | 5600 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|       7 | 5601 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       7 | 5602 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|       7 | 5603 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|       7 | 5604 | `			PH7_MemObjRelease(&sKey);` |
|       7 | 5605 | `			pEntry = pEntry->pPrev;` |
|       4 | 5606 | `		}` |
|       3 | 5607 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 5608 | `		return PH7_OK;` |
|       - | 5609 | `	}` |
|      32 | 5610 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|      32 | 5611 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|      30 | 5612 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 5613 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 5614 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|     ! 0 | 5615 | `			ph7_type_name(apArg[0]));` |
|       - | 5616 | `	}` |
|      30 | 5617 | `	ph7_result_value(pCtx,pArray);` |
|      30 | 5618 | `	return PH7_OK;` |
|      19 | 5619 | `}` |
|       - | 5620 | `/*` |
|       - | 5621 | ` * int iterator_count(Traversable\|array $iterator)` |
|       - | 5622 | ` */` |
|       8 | 5623 | `PH7_PRIVATE int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 5624 | `{` |
|       - | 5625 | `	struct IterCollect sCol;` |
|       - | 5626 | `	sxi32 rc;` |
|       9 | 5627 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|       9 | 5628 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       3 | 5629 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|       3 | 5630 | `		return PH7_OK;` |
|       - | 5631 | `	}` |
|       7 | 5632 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|       7 | 5633 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|       7 | 5634 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|       7 | 5635 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 5636 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 5637 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|     ! 0 | 5638 | `			ph7_type_name(apArg[0]));` |
|       - | 5639 | `	}` |
|       7 | 5640 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|       7 | 5641 | `	return PH7_OK;` |
|       5 | 5642 | `}` |
|       - | 5643 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|       - | 5644 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|       - | 5645 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|       - | 5646 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|      32 | 5647 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       1 | 5648 | `{` |
|      33 | 5649 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|       - | 5650 | `	ph7_value sResult;` |
|       - | 5651 | `	SySet aArg;` |
|       - | 5652 | `	sxi32 rc;` |
|       - | 5653 | `	int bContinue;` |
|      16 | 5654 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|      33 | 5655 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|      33 | 5656 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|       9 | 5657 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|       9 | 5658 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 5659 | `		sxu32 n;` |
|      17 | 5660 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       9 | 5661 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|       9 | 5662 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|       9 | 5663 | `			pEntry = pEntry->pPrev;` |
|       5 | 5664 | `		}` |
|       4 | 5665 | `	}` |
|      33 | 5666 | `	PH7_MemObjInit(pVm,&sResult);` |
|      49 | 5667 | `	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|      32 | 5668 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult);` |
|      33 | 5669 | `	SySetRelease(&aArg);` |
|      33 | 5670 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|      31 | 5671 | `	p->nCount++;` |
|      31 | 5672 | `	PH7_MemObjToBool(&sResult);` |
|      31 | 5673 | `	bContinue = (sResult.x.iVal != 0);` |
|      31 | 5674 | `	PH7_MemObjRelease(&sResult);` |
|      31 | 5675 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|      17 | 5676 | `}` |
|       - | 5677 | `/*` |
|       - | 5678 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|       - | 5679 | ` */` |
|      12 | 5680 | `PH7_PRIVATE int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 5681 | `{` |
|       - | 5682 | `	struct IterApply sApp;` |
|       - | 5683 | `	sxi32 rc;` |
|      13 | 5684 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|      13 | 5685 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|     ! 0 | 5686 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 5687 | `			"iterator_apply(): Argument #2 ($callback) must be a valid callback");` |
|       - | 5688 | `	}` |
|      13 | 5689 | `	sApp.pCallback = apArg[1];` |
|      13 | 5690 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|      13 | 5691 | `	sApp.nCount = 0;` |
|      13 | 5692 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|      13 | 5693 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|      11 | 5694 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 5695 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 5696 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|     ! 0 | 5697 | `			ph7_type_name(apArg[0]));` |
|       - | 5698 | `	}` |
|      11 | 5699 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|      11 | 5700 | `	return PH7_OK;` |
|       7 | 5701 | `}` |
|       - | 5702 |  |
