# src/ph7/hashmap_builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2449/2841 lines (86.20%)

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
|    2400 |   64 | `PH7_PRIVATE int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |   65 | `{` |
|    2405 |   66 | `	int bRecursive = FALSE;` |
|    2405 |   67 | `	int bCycleDetected = FALSE;` |
|       - |   68 | `	sxi64 iCount;` |
|    2405 |   69 | `	if( nArg < 1 ){` |
|     ! 0 |   70 | `		return PH7_VmThrowException(pCtx,` |
|       - |   71 | `			"ArgumentCountError",` |
|       - |   72 | `			"count() expects at least 1 argument, 0 given"` |
|       - |   73 | `			);` |
|       - |   74 | `	}` |
|    2405 |   75 | `	if( nArg > 2 ){` |
|     ! 0 |   76 | `		return PH7_VmThrowException(pCtx,` |
|       - |   77 | `			"ArgumentCountError",` |
|       - |   78 | `			"count() expects at most 2 arguments, %d given",` |
|     ! 0 |   79 | `			nArg` |
|       - |   80 | `			);` |
|       - |   81 | `	}` |
|       - |   82 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|       - |   83 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|       - |   84 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|    2405 |   85 | `	if( nArg > 1 ){` |
|      46 |   86 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|      46 |   87 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|      11 |   88 | `			return PH7_VmThrowException(pCtx,` |
|       - |   89 | `				"ValueError",` |
|       - |   90 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|       - |   91 | `				);` |
|       - |   92 | `		}` |
|      36 |   93 | `		bRecursive = iMode == 1;` |
|      17 |   94 | `	}` |
|    2397 |   95 | `	if( !ph7_value_is_array(apArg[0]) ){` |
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
|    2327 |  120 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|    2327 |  121 | `	if( bCycleDetected ){` |
|       3 |  122 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|       1 |  123 | `	}` |
|    2327 |  124 | `	ph7_result_int64(pCtx,iCount);` |
|    2327 |  125 | `	return PH7_OK;` |
|    1205 |  126 | `}` |
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
|     106 |  138 | `PH7_PRIVATE int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  139 | `{` |
|       - |  140 | `	sxi32 rc;` |
|     110 |  141 | `	if( nArg != 2 ){` |
|       - |  142 | `		/* PHP requires exactly two arguments */` |
|     ! 0 |  143 | `		return PH7_VmThrowException(pCtx,` |
|       - |  144 | `			"ArgumentCountError",` |
|       - |  145 | `			"array_key_exists() expects exactly 2 arguments, %d given",` |
|     ! 0 |  146 | `			nArg` |
|       - |  147 | `			);` |
|       - |  148 | `	}` |
|       - |  149 | `	/* Make sure we are dealing with a valid hashmap */` |
|     110 |  150 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - |  151 | `		/* Type mismatch -> TypeError */` |
|       8 |  152 | `		return PH7_VmThrowException(pCtx,` |
|       - |  153 | `			"TypeError",` |
|       - |  154 | `			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",` |
|       4 |  155 | `			ph7_type_name(apArg[1])` |
|       - |  156 | `			);` |
|       - |  157 | `	}` |
|       - |  158 | `	/* php only DEPRECATES a null / lossy-float key here; PHL rejects it. */` |
|     106 |  159 | `	if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|       3 |  160 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  161 | `			"array_key_exists(): Argument #1 ($key) must be of type string\|int, null given");` |
|     104 |  162 | `	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){` |
|       3 |  163 | `		ph7_real rVal = apArg[0]->rVal;` |
|       3 |  164 | `		if( rVal != (ph7_real)(sxi64)rVal ){` |
|       3 |  165 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  166 | `				"array_key_exists(): Argument #1 ($key) must be of type string\|int, float given");` |
|       - |  167 | `		}` |
|     ! 0 |  168 | `	}` |
|       - |  169 | `	/* Perform the lookup */` |
|     102 |  170 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);` |
|       - |  171 | `	/* lookup result */` |
|     102 |  172 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|     102 |  173 | `	return PH7_OK;` |
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
|     240 | 1317 | `PH7_PRIVATE int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
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
|     245 | 1328 | `	if( nArg < 1 ){` |
|       - | 1329 | `		/* Missing argument,throw ArgumentCountError */` |
|     ! 0 | 1330 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1331 | `			"ArgumentCountError",` |
|       - | 1332 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 1333 | `			);` |
|       - | 1334 | `	}` |
|       - | 1335 | `	/* Make sure we are dealing with a valid hashmap */` |
|     245 | 1336 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 1337 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 1338 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1339 | `			"TypeError",` |
|       - | 1340 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 1341 | `			ph7_type_name(apArg[0])` |
|       - | 1342 | `			);` |
|       - | 1343 | `	}` |
|       - | 1344 | `	/* Point to the internal representation of the input hashmap */` |
|     243 | 1345 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 1346 | `	/* Create a new array */` |
|     243 | 1347 | `	pArray = ph7_context_new_array(pCtx);` |
|     243 | 1348 | `	if( pArray == 0 ){` |
|     ! 0 | 1349 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1350 | `		return PH7_OK;` |
|       - | 1351 | `	}` |
|     243 | 1352 | `	bStrict = FALSE;` |
|     243 | 1353 | `	if( nArg > 2 ){` |
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
|     243 | 1365 | `	pNode = pMap->pFirst;` |
|     243 | 1366 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|    2297 | 1367 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|    2059 | 1368 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     698 | 1369 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|     351 | 1370 | `		}else{` |
|    1365 | 1371 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|    1365 | 1372 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 1373 | `		}` |
|    2059 | 1374 | `		rc = 0;` |
|    2059 | 1375 | `		if( nArg > 1 ){` |
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
|    2059 | 1391 | `		if( rc == 0 ){` |
|       - | 1392 | `			/* Perform the insertion */` |
|    2027 | 1393 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|    1011 | 1394 | `		}` |
|    2059 | 1395 | `		PH7_MemObjRelease(&sObj);` |
|       - | 1396 | `		/* Point to the next entry */` |
|    2059 | 1397 | `		pNode = pNode->pPrev; /* Reverse link */` |
|    1032 | 1398 | `	}` |
|       - | 1399 | `	/* return the new array */` |
|     243 | 1400 | `	ph7_result_value(pCtx,pArray);` |
|     243 | 1401 | `	return PH7_OK;` |
|     125 | 1402 | `}` |
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
|    1154 | 1446 | `PH7_PRIVATE int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1447 | `{` |
|       - | 1448 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 1449 | `	ph7_value *pArray;` |
|       - | 1450 | `	int i;` |
|       - | 1451 | `	/* Create a new array */` |
|    1159 | 1452 | `	pArray = ph7_context_new_array(pCtx);` |
|    1159 | 1453 | `	if( pArray == 0 ){` |
|     ! 0 | 1454 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1455 | `		return PH7_OK;` |
|       - | 1456 | `	}` |
|       - | 1457 | `	/* Point to the internal representation of the hashmap */` |
|    1159 | 1458 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 1459 | `	/* Start merging */` |
|    3443 | 1460 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 1461 | `		/* Make sure we are dealing with a valid hashmap */` |
|    2293 | 1462 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 1463 | `			/* Type mismatch -> TypeError */` |
|       8 | 1464 | `			return PH7_VmThrowException(pCtx,` |
|       - | 1465 | `				"TypeError",` |
|       - | 1466 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|       2 | 1467 | `				i + 1,` |
|       4 | 1468 | `				ph7_type_name(apArg[i])` |
|       - | 1469 | `				);` |
|     ! 0 | 1470 | `		}else{` |
|    2289 | 1471 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 1472 | `			/* Merge the two hashmaps */` |
|    2289 | 1473 | `			HashmapMerge(pSrc,pMap);` |
|       - | 1474 | `		}` |
|    1147 | 1475 | `	}` |
|       - | 1476 | `	/* Return the freshly created array */` |
|    1155 | 1477 | `	ph7_result_value(pCtx,pArray);` |
|    1155 | 1478 | `	return PH7_OK;` |
|     582 | 1479 | `}` |
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
|      66 | 1567 | `PH7_PRIVATE int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1568 | `{` |
|       - | 1569 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 1570 | `	ph7_hashmap_node *pCur;` |
|       - | 1571 | `	ph7_value *pArray;` |
|       - | 1572 | `	int iLength,iOfft;` |
|       - | 1573 | `	int bPreserve;` |
|       - | 1574 | `	sxi32 rc;` |
|      71 | 1575 | `	if( nArg < 2 ){` |
|     ! 0 | 1576 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1577 | `			"ArgumentCountError",` |
|       - | 1578 | `			"array_slice() expects at least 2 arguments, %d given",` |
|     ! 0 | 1579 | `			nArg` |
|       - | 1580 | `			);` |
|       - | 1581 | `	}` |
|      71 | 1582 | `	if( nArg > 4 ){` |
|     ! 0 | 1583 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1584 | `			"ArgumentCountError",` |
|       - | 1585 | `			"array_slice() expects at most 4 arguments, %d given",` |
|     ! 0 | 1586 | `			nArg` |
|       - | 1587 | `			);` |
|       - | 1588 | `	}` |
|      71 | 1589 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 1590 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1591 | `			"TypeError",` |
|       - | 1592 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 1593 | `			ph7_type_name(apArg[0])` |
|       - | 1594 | `			);` |
|       - | 1595 | `	}` |
|       - | 1596 | `	/* Validate $offset type: reject string, array, object, resource */` |
|      95 | 1597 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|      98 | 1598 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|       4 | 1599 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1600 | `			"TypeError",` |
|       - | 1601 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|       2 | 1602 | `			ph7_type_name(apArg[1])` |
|       - | 1603 | `			);` |
|       - | 1604 | `	}` |
|       - | 1605 | `	/* Validate $length type if provided: nullable int */` |
|      67 | 1606 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
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
|      65 | 1617 | `	if( nArg > 3 ){` |
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
|      65 | 1628 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      65 | 1629 | `	bPreserve = FALSE;` |
|       - | 1630 | `	/* Get the offset */` |
|       - | 1631 | `	{` |
|      65 | 1632 | `		sxi64 iTmp = 0;` |
|      65 | 1633 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"array_slice",2,"$offset","int",&iTmp);` |
|      65 | 1634 | `		if( rcArg != PH7_OK ){` |
|     ! 0 | 1635 | `			return rcArg;` |
|       - | 1636 | `		}` |
|      65 | 1637 | `		iOfft = (int)iTmp;` |
|       - | 1638 | `	}` |
|      65 | 1639 | `	if( iOfft < 0 ){` |
|       5 | 1640 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       5 | 1641 | `		if( iOfft < 0 ){` |
|       3 | 1642 | `			iOfft = 0;` |
|       1 | 1643 | `		}` |
|       2 | 1644 | `	}` |
|      65 | 1645 | `	if( iOfft >= (int)pSrc->nEntry ){` |
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
|      61 | 1656 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      61 | 1657 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
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
|      61 | 1669 | `	if( nArg > 3 ){` |
|       5 | 1670 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|       2 | 1671 | `	}` |
|       - | 1672 | `	/* Create a new array */` |
|      61 | 1673 | `	pArray = ph7_context_new_array(pCtx);` |
|      61 | 1674 | `	if( pArray == 0 ){` |
|     ! 0 | 1675 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1676 | `		return PH7_OK;` |
|       - | 1677 | `	}` |
|      61 | 1678 | `	if( iLength < 1 ){` |
|       - | 1679 | `		/* Don't bother processing,return the empty array */` |
|       5 | 1680 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 1681 | `		return PH7_OK;` |
|       - | 1682 | `	}` |
|       - | 1683 | `	/* Point to the desired entry */` |
|      57 | 1684 | `	pCur = pSrc->pFirst;` |
|      56 | 1685 | `	for(;;){` |
|     117 | 1686 | `		if( iOfft < 1 ){` |
|      57 | 1687 | `			break;` |
|       - | 1688 | `		}` |
|       - | 1689 | `		/* Point to the next entry */` |
|      65 | 1690 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      65 | 1691 | `		iOfft--;` |
|       5 | 1692 | `	}` |
|       - | 1693 | `	/* Point to the internal representation of the hashmap */` |
|      57 | 1694 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     111 | 1695 | `	for(;;){` |
|     227 | 1696 | `		if( iLength < 1 ){` |
|      57 | 1697 | `			break;` |
|       - | 1698 | `		}` |
|       - | 1699 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|       - | 1700 | `		{` |
|     175 | 1701 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|     175 | 1702 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|       - | 1703 | `		}` |
|     175 | 1704 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1705 | `			break;` |
|       - | 1706 | `		}` |
|       - | 1707 | `		/* Point to the next entry */` |
|     175 | 1708 | `		pCur = pCur->pPrev; /* Reverse link */` |
|     175 | 1709 | `		iLength--;` |
|       5 | 1710 | `	}` |
|       - | 1711 | `	/* Return the freshly created array */` |
|      57 | 1712 | `	ph7_result_value(pCtx,pArray);` |
|      57 | 1713 | `	return PH7_OK;` |
|      38 | 1714 | `}` |
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
|   34174 | 1939 | `PH7_PRIVATE int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1940 | `{` |
|       - | 1941 | `	ph7_value *pNeedle;` |
|       - | 1942 | `	int bStrict;` |
|       - | 1943 | `	int rc;` |
|   34179 | 1944 | `	if( nArg < 2 ){` |
|       - | 1945 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 1946 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1947 | `		return PH7_OK;` |
|       - | 1948 | `	}` |
|   34179 | 1949 | `	pNeedle = apArg[0];` |
|   34179 | 1950 | `	bStrict = 0;` |
|   34179 | 1951 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 1952 | `		/* haystack must be an array,throw TypeError (matches array_search) */` |
|       - | 1953 | `		char zBuf[64];` |
|      16 | 1954 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1955 | `			"TypeError",` |
|       - | 1956 | `			"in_array(): Argument #2 ($haystack) must be of type array, %s given",` |
|      10 | 1957 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|       - | 1958 | `			);` |
|       - | 1959 | `	}` |
|   34169 | 1960 | `	if( nArg > 2 ){` |
|      70 | 1961 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|      34 | 1962 | `	}` |
|       - | 1963 | `	/* Perform the lookup */` |
|   34169 | 1964 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 1965 | `	/* Lookup result */` |
|   34169 | 1966 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   34169 | 1967 | `	return PH7_OK;` |
|   17092 | 1968 | `}` |
|       - | 1969 | `/*` |
|       - | 1970 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 1971 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 1972 | ` * Parameters` |
|       - | 1973 | ` * $needle` |
|       - | 1974 | ` *   The searched value.` |
|       - | 1975 | ` * $haystack` |
|       - | 1976 | ` *   The array.` |
|       - | 1977 | ` * $strict` |
|       - | 1978 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 1979 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 1980 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 1981 | ` * Return` |
|       - | 1982 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 1983 | ` */` |
|      50 | 1984 | `PH7_PRIVATE int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1985 | `{` |
|       - | 1986 | `	ph7_hashmap_node *pEntry;` |
|       - | 1987 | `	ph7_value *pVal,sNeedle;` |
|       - | 1988 | `	ph7_hashmap *pMap;` |
|       - | 1989 | `	ph7_value sVal;` |
|       - | 1990 | `	int bStrict;` |
|       - | 1991 | `	sxu32 n;` |
|       - | 1992 | `	int rc;` |
|      53 | 1993 | `	if( nArg < 2 ){` |
|       - | 1994 | `		/* Missing argument,throw ArgumentCountError */` |
|     ! 0 | 1995 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1996 | `			"ArgumentCountError",` |
|       - | 1997 | `			"array_search() expects at least 2 arguments, %d given",` |
|     ! 0 | 1998 | `			nArg` |
|       - | 1999 | `			);` |
|       - | 2000 | `	}` |
|      53 | 2001 | `	bStrict = FALSE;` |
|      53 | 2002 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2003 | `		/* haystack must be an array,throw TypeError. VmValueGivenName gives php's` |
|       - | 2004 | `		 * ZPP value-name (true/false for bools, not ph7_type_name's "bool") */` |
|       - | 2005 | `		char zBuf[64];` |
|      20 | 2006 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2007 | `			"TypeError",` |
|       - | 2008 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|      12 | 2009 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|       - | 2010 | `			);` |
|       - | 2011 | `	}` |
|      40 | 2012 | `	if( nArg > 2 ){` |
|       - | 2013 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      21 | 2014 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|     ! 0 | 2015 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2016 | `				"TypeError",` |
|       - | 2017 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|     ! 0 | 2018 | `				ph7_type_name(apArg[2])` |
|       - | 2019 | `				);` |
|       - | 2020 | `		}` |
|      21 | 2021 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|      10 | 2022 | `	}` |
|       - | 2023 | `	/* Point to the internal representation of the internal hashmap */` |
|      40 | 2024 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 2025 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      40 | 2026 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      40 | 2027 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      40 | 2028 | `	pEntry = pMap->pFirst;` |
|      40 | 2029 | `	n = pMap->nEntry;` |
|      40 | 2030 | `	for(;;){` |
|      82 | 2031 | `		if( !n ){` |
|      12 | 2032 | `			break;` |
|       - | 2033 | `		}` |
|       - | 2034 | `		/* Extract node value */` |
|      72 | 2035 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      72 | 2036 | `		if( pVal ){` |
|       - | 2037 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 2038 | `			 * can change their type.` |
|       - | 2039 | `			 */` |
|      72 | 2040 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      72 | 2041 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      72 | 2042 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      72 | 2043 | `			PH7_MemObjRelease(&sVal);` |
|      72 | 2044 | `			PH7_MemObjRelease(&sNeedle);` |
|      72 | 2045 | `			if( rc == 0 ){` |
|       - | 2046 | `				/* Match found,return key */` |
|      30 | 2047 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 2048 | `					/* INT key */` |
|      24 | 2049 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|      13 | 2050 | `				}else{` |
|       7 | 2051 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 2052 | `					/* Blob key */` |
|       7 | 2053 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 2054 | `				}` |
|      30 | 2055 | `				return PH7_OK;` |
|       - | 2056 | `			}` |
|      21 | 2057 | `		}` |
|       - | 2058 | `		/* Point to the next entry */` |
|      44 | 2059 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      44 | 2060 | `		n--;` |
|       2 | 2061 | `	}` |
|       - | 2062 | `	/* No such value,return FALSE */` |
|      12 | 2063 | `	ph7_result_bool(pCtx,0);` |
|      12 | 2064 | `	return PH7_OK;` |
|      28 | 2065 | `}` |
|       - | 2066 | `/*` |
|       - | 2067 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 2068 | ` *  Computes the difference of arrays.` |
|       - | 2069 | ` * Parameters` |
|       - | 2070 | ` *  $array1` |
|       - | 2071 | ` *    The array to compare from` |
|       - | 2072 | ` *  $array2` |
|       - | 2073 | ` *    An array to compare against` |
|       - | 2074 | ` *  $...` |
|       - | 2075 | ` *   More arrays to compare against` |
|       - | 2076 | ` * Return` |
|       - | 2077 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 2078 | ` *  are not present in any of the other arrays.` |
|       - | 2079 | ` */` |
|      20 | 2080 | `PH7_PRIVATE int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2081 | `{` |
|       - | 2082 | `	ph7_hashmap_node *pEntry;` |
|       - | 2083 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2084 | `	ph7_value *pArray;` |
|       - | 2085 | `	ph7_value *pVal;` |
|       - | 2086 | `	sxi32 rc;` |
|       - | 2087 | `	sxu32 n;` |
|       - | 2088 | `	int i;` |
|       - | 2089 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 2090 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 2091 | `	 * debugging difficult. */` |
|      23 | 2092 | `	if( nArg < 1 ){` |
|     ! 0 | 2093 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2094 | `			"ArgumentCountError",` |
|       - | 2095 | `			"array_diff() expects at least 1 argument, %d given",` |
|     ! 0 | 2096 | `			nArg` |
|       - | 2097 | `			);` |
|       - | 2098 | `	}` |
|      23 | 2099 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2100 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2101 | `			"TypeError",` |
|       - | 2102 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2103 | `			ph7_type_name(apArg[0])` |
|       - | 2104 | `			);` |
|       - | 2105 | `	}` |
|      36 | 2106 | `	for(i = 1 ; i < nArg ; i++){` |
|      20 | 2107 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 2108 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2109 | `				"TypeError",` |
|       - | 2110 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 2111 | `				i + 1,` |
|       2 | 2112 | `				ph7_type_name(apArg[i])` |
|       - | 2113 | `				);` |
|       - | 2114 | `		}` |
|       9 | 2115 | `	}` |
|      17 | 2116 | `	if( nArg == 1 ){` |
|       - | 2117 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 2118 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2119 | `		return PH7_OK;` |
|       - | 2120 | `	}` |
|       - | 2121 | `	/* Create a new array */` |
|      15 | 2122 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 2123 | `	if( pArray == 0 ){` |
|     ! 0 | 2124 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2125 | `		return PH7_OK;` |
|       - | 2126 | `	}` |
|       - | 2127 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 2128 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2129 | `	/* Perform the diff */` |
|      15 | 2130 | `	pEntry = pSrc->pFirst;` |
|      15 | 2131 | `	n = pSrc->nEntry;` |
|      27 | 2132 | `	for(;;){` |
|      55 | 2133 | `		if( n < 1 ){` |
|      15 | 2134 | `			break;` |
|       - | 2135 | `		}` |
|       - | 2136 | `		/* Extract the node value */` |
|      41 | 2137 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      41 | 2138 | `		if( pVal ){` |
|      69 | 2139 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 2140 | `				/* Point to the internal representation of the hashmap */` |
|      45 | 2141 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2142 | `				/* Perform the lookup */` |
|      45 | 2143 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      45 | 2144 | `				if( rc == SXRET_OK ){` |
|       - | 2145 | `					/* Value exist */` |
|      17 | 2146 | `					break;` |
|       - | 2147 | `				}` |
|      15 | 2148 | `			}` |
|      41 | 2149 | `			if( i >= nArg ){` |
|       - | 2150 | `				/* Perform the insertion */` |
|      25 | 2151 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 2152 | `			}` |
|      20 | 2153 | `		}` |
|       - | 2154 | `		/* Point to the next entry */` |
|      41 | 2155 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      41 | 2156 | `		n--;` |
|       1 | 2157 | `	}` |
|       - | 2158 | `	/* Return the freshly created array */` |
|      15 | 2159 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 2160 | `	return PH7_OK;` |
|      13 | 2161 | `}` |
|       - | 2162 | `/*` |
|       - | 2163 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 2164 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 2165 | ` * Parameters` |
|       - | 2166 | ` *  $array1` |
|       - | 2167 | ` *    The array to compare from` |
|       - | 2168 | ` *  $array2` |
|       - | 2169 | ` *    An array to compare against` |
|       - | 2170 | ` *  $...` |
|       - | 2171 | ` *   More arrays to compare against.` |
|       - | 2172 | ` * $callback` |
|       - | 2173 | ` *  The callback comparison function.` |
|       - | 2174 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 2175 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 2176 | ` *  than the second.` |
|       - | 2177 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 2178 | ` * Return` |
|       - | 2179 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 2180 | ` *  are not present in any of the other arrays.` |
|       - | 2181 | ` */` |
|      24 | 2182 | `PH7_PRIVATE int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2183 | `{` |
|       - | 2184 | `	ph7_hashmap_node *pEntry;` |
|       - | 2185 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2186 | `	ph7_value *pCallback;` |
|       - | 2187 | `	ph7_value *pArray;` |
|       - | 2188 | `	ph7_value *pVal;` |
|       - | 2189 | `	sxi32 rc;` |
|       - | 2190 | `	sxu32 n;` |
|       - | 2191 | `	int i;` |
|       - | 2192 |  |
|       - | 2193 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      29 | 2194 | `	if( nArg < 2 ){` |
|     ! 0 | 2195 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2196 | `			"ArgumentCountError",` |
|       - | 2197 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|     ! 0 | 2198 | `			nArg` |
|       - | 2199 | `			);` |
|       - | 2200 | `	}` |
|      29 | 2201 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2202 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2203 | `			"TypeError",` |
|       - | 2204 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2205 | `			ph7_type_name(apArg[0])` |
|       - | 2206 | `			);` |
|       - | 2207 | `	}` |
|       - | 2208 |  |
|       - | 2209 | `	/* php validates the CALLBACK (the last argument) before the intermediary` |
|       - | 2210 | ``	 * arrays: `array_udiff([1],"x",123)` reports Argument #3 (the bad callback),`` |
|       - | 2211 | `	 * not Argument #2 (the non-array). PHL had the middle-array loop first, so it` |
|       - | 2212 | `	 * named the wrong argument whenever both were invalid. */` |
|      27 | 2213 | `	pCallback = apArg[nArg - 1];` |
|      27 | 2214 | `	if( !ph7_value_is_callable(pCallback) ){` |
|      13 | 2215 | `		if( ph7_value_is_array(pCallback) ){` |
|       4 | 2216 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2217 | `				"TypeError",` |
|       - | 2218 | `				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 2219 | `				nArg` |
|       - | 2220 | `				);` |
|       - | 2221 | `		}` |
|      11 | 2222 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 2223 | `			int len;` |
|       6 | 2224 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       8 | 2225 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2226 | `				"TypeError",` |
|       - | 2227 | `				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       2 | 2228 | `				nArg,` |
|       2 | 2229 | `				zName` |
|       - | 2230 | `				);` |
|       - | 2231 | `		}` |
|       8 | 2232 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2233 | `			"TypeError",` |
|       - | 2234 | `			"array_udiff(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 2235 | `			nArg` |
|       - | 2236 | `			);` |
|       - | 2237 | `	}` |
|       - | 2238 |  |
|       - | 2239 | `	/* Now the intermediary arguments (arrays), left to right. */` |
|      21 | 2240 | `	for( i = 1 ; i < nArg - 1; i++ ){` |
|      13 | 2241 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       7 | 2242 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2243 | `				"TypeError",` |
|       - | 2244 | `				"array_udiff(): Argument #%d must be of type array, %s given",` |
|       2 | 2245 | `				i + 1,` |
|       4 | 2246 | `				ph7_type_name(apArg[i])` |
|       - | 2247 | `				);` |
|       - | 2248 | `		}` |
|       5 | 2249 | `	}` |
|       - | 2250 |  |
|      10 | 2251 | `	if( nArg == 2 ){` |
|       - | 2252 | `		/* Only the original array and the callback were provided. */` |
|       3 | 2253 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2254 | `		return PH7_OK;` |
|       - | 2255 | `	}` |
|       - | 2256 |  |
|       - | 2257 | `	/* Create a new array */` |
|       8 | 2258 | `	pArray = ph7_context_new_array(pCtx);` |
|       8 | 2259 | `	if( pArray == 0 ){` |
|     ! 0 | 2260 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2261 | `		return PH7_OK;` |
|       - | 2262 | `	}` |
|       - | 2263 | `	/* Point to the internal representation of the source hashmap */` |
|       8 | 2264 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2265 | `	/* Perform the diff */` |
|       8 | 2266 | `	pEntry = pSrc->pFirst;` |
|       8 | 2267 | `	n = pSrc->nEntry;` |
|       8 | 2268 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|       9 | 2269 | `	for(;;){` |
|      20 | 2270 | `		if( n < 1 ){` |
|       6 | 2271 | `			break;` |
|       - | 2272 | `		}` |
|       - | 2273 | `		/* Extract the node value */` |
|      16 | 2274 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      16 | 2275 | `		if( pVal ){` |
|      24 | 2276 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 2277 | `				/* Point to the internal representation of the hashmap */` |
|      16 | 2278 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2279 | `				/* Perform the lookup */` |
|      16 | 2280 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|      16 | 2281 | `				if( rc == SXRET_OK ){` |
|       - | 2282 | `					/* Value exist */` |
|       8 | 2283 | `					break;` |
|       - | 2284 | `				}` |
|       6 | 2285 | `			}` |
|      16 | 2286 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2287 | `				/* The comparison callback raised: propagate so the dispatcher` |
|       - | 2288 | `				 * unwinds, before any spurious insertion into the result. */` |
|       3 | 2289 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 2290 | `				return PH7_EXCEPTION;` |
|       - | 2291 | `			}` |
|      14 | 2292 | `			if( i >= (nArg - 1)){` |
|       - | 2293 | `				/* Perform the insertion */` |
|       8 | 2294 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       3 | 2295 | `			}` |
|       6 | 2296 | `		}` |
|       - | 2297 | `		/* Point to the next entry */` |
|      14 | 2298 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      14 | 2299 | `		n--;` |
|       2 | 2300 | `	}` |
|       - | 2301 | `	/* Return the freshly created array */` |
|       6 | 2302 | `	ph7_result_value(pCtx,pArray);` |
|       6 | 2303 | `	return PH7_OK;` |
|      17 | 2304 | `}` |
|       - | 2305 | `/*` |
|       - | 2306 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 2307 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 2308 | ` * Parameters` |
|       - | 2309 | ` *  $array1` |
|       - | 2310 | ` *    The array to compare from` |
|       - | 2311 | ` *  $array2` |
|       - | 2312 | ` *    An array to compare against` |
|       - | 2313 | ` *  $...` |
|       - | 2314 | ` *   More arrays to compare against` |
|       - | 2315 | ` * Return` |
|       - | 2316 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 2317 | ` *  are not present in any of the other arrays.` |
|       - | 2318 | ` */` |
|      20 | 2319 | `PH7_PRIVATE int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 2320 | `{` |
|       - | 2321 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 2322 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2323 | `	ph7_value *pArray;` |
|       - | 2324 | `	ph7_value *pVal;` |
|       - | 2325 | `	sxi32 rc;` |
|       - | 2326 | `	sxu32 n;` |
|       - | 2327 | `	int i;` |
|       - | 2328 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 2329 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 2330 | `	 * accompanying integration tests to pass. */` |
|      24 | 2331 | `	if( nArg < 1 ){` |
|     ! 0 | 2332 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2333 | `			"ArgumentCountError",` |
|       - | 2334 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|     ! 0 | 2335 | `			nArg` |
|       - | 2336 | `			);` |
|       - | 2337 | `	}` |
|      24 | 2338 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2339 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2340 | `			"TypeError",` |
|       - | 2341 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2342 | `			ph7_type_name(apArg[0])` |
|       - | 2343 | `			);` |
|       - | 2344 | `	}` |
|      37 | 2345 | `	for(i = 1 ; i < nArg ; i++){` |
|      23 | 2346 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       8 | 2347 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2348 | `				"TypeError",` |
|       - | 2349 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 2350 | `				i + 1,` |
|       4 | 2351 | `				ph7_type_name(apArg[i])` |
|       - | 2352 | `				);` |
|       - | 2353 | `		}` |
|      10 | 2354 | `	}` |
|      15 | 2355 | `	if( nArg == 1 ){` |
|       - | 2356 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 2357 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2358 | `		return PH7_OK;` |
|       - | 2359 | `	}` |
|       - | 2360 | `	/* Create a new array */` |
|      13 | 2361 | `	pArray = ph7_context_new_array(pCtx);` |
|      13 | 2362 | `	if( pArray == 0 ){` |
|     ! 0 | 2363 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2364 | `		return PH7_OK;` |
|       - | 2365 | `	}` |
|       - | 2366 | `	/* Point to the internal representation of the source hashmap */` |
|      13 | 2367 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2368 | `	/* Perform the diff */` |
|      13 | 2369 | `	pEntry = pSrc->pFirst;` |
|      13 | 2370 | `	n = pSrc->nEntry;` |
|      13 | 2371 | `	pN1 = pN2 = 0;` |
|      34 | 2372 | `	for(;;){` |
|       - | 2373 | `		int keep;` |
|      41 | 2374 | `		if( n < 1 ){` |
|      13 | 2375 | `			break;` |
|       - | 2376 | `		}` |
|       - | 2377 | `		/* assume the element should be kept until we find a match */` |
|      29 | 2378 | `		keep = 1;` |
|      47 | 2379 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 2380 | `			/* all arguments have been validated already, so cast directly */` |
|      33 | 2381 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2382 | `			/* Perform a key lookup first */` |
|      33 | 2383 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      13 | 2384 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       7 | 2385 | `			}else{` |
|      21 | 2386 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 2387 | `			}` |
|      33 | 2388 | `			if( rc != SXRET_OK ){` |
|       - | 2389 | `				/* this array does not contain the key, continue checking others */` |
|      17 | 2390 | `				continue;` |
|       - | 2391 | `			}` |
|       - | 2392 | `			/* key exists; check that value stored in the matching node is equal */` |
|      17 | 2393 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      17 | 2394 | `			if( pVal ){` |
|       - | 2395 | `				/* directly compare with value at pN1 rather than searching again */` |
|      17 | 2396 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      17 | 2397 | `				if( pVal2 ){` |
|       - | 2398 | `					ph7_value sV1,sV2;` |
|       - | 2399 | `					sxi32 cmp;` |
|       - | 2400 | `					/* Compare on duplicates: PH7_MemObjCmp converts its` |
|       - | 2401 | `					 * operands in place and these are LIVE array elements (a` |
|       - | 2402 | `					 * null element used to come back bool(false) in the` |
|       - | 2403 | `					 * caller's array). */` |
|      17 | 2404 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|      17 | 2405 | `					PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|      17 | 2406 | `					PH7_MemObjLoad(pVal,&sV1);` |
|      17 | 2407 | `					PH7_MemObjLoad(pVal2,&sV2);` |
|      17 | 2408 | `					cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|      17 | 2409 | `					PH7_MemObjRelease(&sV1);` |
|      17 | 2410 | `					PH7_MemObjRelease(&sV2);` |
|      17 | 2411 | `					if( cmp == 0 ){` |
|       - | 2412 | `						/* identical key+value found in one of the arrays => drop it */` |
|      15 | 2413 | `						keep = 0;` |
|      15 | 2414 | `						break;` |
|       - | 2415 | `					}` |
|       1 | 2416 | `				}` |
|       1 | 2417 | `			}` |
|       2 | 2418 | `		}` |
|      29 | 2419 | `		if( keep ){` |
|       - | 2420 | `			/* Perform the insertion */` |
|      15 | 2421 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       7 | 2422 | `		}` |
|       - | 2423 | `		/* Point to the next entry */` |
|      29 | 2424 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      29 | 2425 | `		n--;` |
|       1 | 2426 | `	}` |
|       - | 2427 | `	/* Return the freshly created array */` |
|      13 | 2428 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 2429 | `	return PH7_OK;` |
|      14 | 2430 | `}` |
|       - | 2431 | `/*` |
|       - | 2432 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 2433 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 2434 | ` *  by a user supplied callback function.` |
|       - | 2435 | ` * Parameters` |
|       - | 2436 | ` *  $array1` |
|       - | 2437 | ` *    The array to compare from` |
|       - | 2438 | ` *  $array2` |
|       - | 2439 | ` *    An array to compare against` |
|       - | 2440 | ` *  $...` |
|       - | 2441 | ` *   More arrays to compare against.` |
|       - | 2442 | ` *  $key_compare_func` |
|       - | 2443 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 2444 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 2445 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 2446 | ` * Return` |
|       - | 2447 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 2448 | ` *  are not present in any of the other arrays.` |
|       - | 2449 | ` */` |
|      24 | 2450 | `PH7_PRIVATE int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 2451 | `{` |
|       - | 2452 | `	ph7_hashmap_node *pEntry;` |
|       - | 2453 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2454 | `	ph7_value *pCallback;` |
|       - | 2455 | `	ph7_value *pArray;` |
|       - | 2456 | `	sxi32 rc;` |
|       - | 2457 | `	sxu32 n;` |
|       - | 2458 | `	int i;` |
|       - | 2459 |  |
|       - | 2460 | `	/* Argument validation mimicking PHP errors. */` |
|      28 | 2461 | `	if( nArg < 2 ){` |
|     ! 0 | 2462 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2463 | `			"ArgumentCountError",` |
|       - | 2464 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|     ! 0 | 2465 | `			nArg` |
|       - | 2466 | `			);` |
|       - | 2467 | `	}` |
|      28 | 2468 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2469 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2470 | `			"TypeError",` |
|       - | 2471 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2472 | `			ph7_type_name(apArg[0])` |
|       - | 2473 | `			);` |
|       - | 2474 | `	}` |
|       - | 2475 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 2476 | `	 * expected to be a callback. */` |
|       - | 2477 | `	/* php checks the CALLBACK before the intermediary arrays (see array_udiff). */` |
|      26 | 2478 | `	pCallback = apArg[nArg - 1];` |
|      26 | 2479 | `	if( !ph7_value_is_callable(pCallback) ){` |
|       - | 2480 | `		/* Compose an error message that closely matches PHP output. When the` |
|       - | 2481 | `		 * argument is an array of the wrong shape we include an extra clause.` |
|       - | 2482 | `		 * If the value is neither array nor string, PHP says "no array or` |
|       - | 2483 | `		 * string given" which we also reproduce. */` |
|      11 | 2484 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 2485 | `			/* ARRAY CALLBACK must have exactly two members */` |
|       4 | 2486 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2487 | `				"TypeError",` |
|       - | 2488 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 2489 | `				nArg` |
|       - | 2490 | `				);` |
|       - | 2491 | `		}` |
|       9 | 2492 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 2493 | `			/* A non-callable string names the function, like array_udiff. */` |
|       - | 2494 | `			int len;` |
|       3 | 2495 | `			const char *zName = ph7_value_to_string(pCallback,&len);` |
|       4 | 2496 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2497 | `				"TypeError",` |
|       - | 2498 | `				"array_diff_uassoc(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       1 | 2499 | `				nArg,` |
|       1 | 2500 | `				zName` |
|       - | 2501 | `				);` |
|       - | 2502 | `		}` |
|       - | 2503 | `		/* neither array nor string */` |
|       8 | 2504 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2505 | `			"TypeError",` |
|       - | 2506 | `			"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",` |
|       2 | 2507 | `			nArg` |
|       - | 2508 | `			);` |
|       - | 2509 | `	}` |
|       - | 2510 | `	/* Now the intermediary arrays, left to right. */` |
|      28 | 2511 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      16 | 2512 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 2513 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2514 | `				"TypeError",` |
|       - | 2515 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 2516 | `				i + 1,` |
|       2 | 2517 | `				ph7_type_name(apArg[i])` |
|       - | 2518 | `				);` |
|       - | 2519 | `		}` |
|       7 | 2520 | `	}` |
|      13 | 2521 | `	if( nArg == 2 ){` |
|       - | 2522 | `		/* If we only have the first array and the callback, just return the` |
|       - | 2523 | `		 * input array. */` |
|       3 | 2524 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2525 | `		return PH7_OK;` |
|       - | 2526 | `	}` |
|       - | 2527 | `	/* Create a new array */` |
|      11 | 2528 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 2529 | `	if( pArray == 0 ){` |
|     ! 0 | 2530 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2531 | `		return PH7_OK;` |
|       - | 2532 | `	}` |
|       - | 2533 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 2534 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2535 | `	/* Perform the diff */` |
|      11 | 2536 | `	pEntry = pSrc->pFirst;` |
|      11 | 2537 | `	n = pSrc->nEntry;` |
|      21 | 2538 | `	for(;;){` |
|       - | 2539 | `		int keep;` |
|      27 | 2540 | `		if( n < 1 ){` |
|       9 | 2541 | `			break;` |
|       - | 2542 | `		}` |
|      19 | 2543 | `		keep = 1;` |
|      31 | 2544 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 2545 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      23 | 2546 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2547 | `			/* we must compare keys via callback, not by direct lookup */` |
|      23 | 2548 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      45 | 2549 | `			while( pIt ){` |
|       - | 2550 | `				/* build temporary key values for callback */` |
|       - | 2551 | `				ph7_value key1, key2, result;` |
|       - | 2552 | `				/* initialise only once using the appropriate helper */` |
|      33 | 2553 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 2554 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 2555 | `				}else{` |
|       - | 2556 | `					SyString sStr;` |
|      33 | 2557 | `					SyStringInitFromBuf(&sStr,` |
|       - | 2558 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 2559 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      33 | 2560 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 2561 | `				}` |
|      33 | 2562 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 2563 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 2564 | `				}else{` |
|       - | 2565 | `					SyString sStr;` |
|      33 | 2566 | `					SyStringInitFromBuf(&sStr,` |
|       - | 2567 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 2568 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      33 | 2569 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 2570 | `				}` |
|      33 | 2571 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 2572 | `				/* call user callback with (key1, key2) */` |
|       - | 2573 | `				{` |
|       - | 2574 | `					ph7_value *apK[2];` |
|      33 | 2575 | `					apK[0] = &key1;` |
|      33 | 2576 | `					apK[1] = &key2;` |
|      33 | 2577 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 2578 | `				}` |
|      33 | 2579 | `				if( rc == PH7_EXCEPTION ){` |
|       - | 2580 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|       - | 2581 | `					 * array_uintersect (which signal back from` |
|       - | 2582 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|       - | 2583 | `					 * function invokes the callback inline, so it cleans up its own` |
|       - | 2584 | `					 * temporaries and propagates the exception directly. */` |
|       3 | 2585 | `					PH7_MemObjRelease(&result);` |
|       3 | 2586 | `					PH7_MemObjRelease(&key1);` |
|       3 | 2587 | `					PH7_MemObjRelease(&key2);` |
|       3 | 2588 | `					return PH7_EXCEPTION;` |
|       - | 2589 | `				}` |
|      31 | 2590 | `				if( rc == SXRET_OK ){` |
|      31 | 2591 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 2592 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 2593 | `					}` |
|      31 | 2594 | `					if( result.x.iVal == 0 ){` |
|       - | 2595 | `						/* keys considered equal by callback; now compare values */` |
|      13 | 2596 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      13 | 2597 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      13 | 2598 | `						if( pVal1 && pVal2 ){` |
|       - | 2599 | `							ph7_value sV1,sV2;` |
|       - | 2600 | `							sxi32 cmp;` |
|       - | 2601 | `							/* Compare on duplicates: PH7_MemObjCmp converts in` |
|       - | 2602 | `							 * place and these are LIVE array elements. */` |
|      13 | 2603 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV1);` |
|      13 | 2604 | `							PH7_MemObjInit(pEntry->pMap->pVm,&sV2);` |
|      13 | 2605 | `							PH7_MemObjLoad(pVal1,&sV1);` |
|      13 | 2606 | `							PH7_MemObjLoad(pVal2,&sV2);` |
|      13 | 2607 | `							cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);` |
|      13 | 2608 | `							PH7_MemObjRelease(&sV1);` |
|      13 | 2609 | `							PH7_MemObjRelease(&sV2);` |
|      13 | 2610 | `							if( cmp == 0 ){` |
|       9 | 2611 | `								keep = 0;` |
|       9 | 2612 | `								PH7_MemObjRelease(&result);` |
|       - | 2613 | `								/* release keys too before breaking */` |
|       9 | 2614 | `								PH7_MemObjRelease(&key1);` |
|       9 | 2615 | `								PH7_MemObjRelease(&key2);` |
|       9 | 2616 | `								break;` |
|       - | 2617 | `							}` |
|       2 | 2618 | `						}` |
|       2 | 2619 | `					}` |
|      11 | 2620 | `				}` |
|      23 | 2621 | `				PH7_MemObjRelease(&result);` |
|      23 | 2622 | `				PH7_MemObjRelease(&key1);` |
|      23 | 2623 | `				PH7_MemObjRelease(&key2);` |
|       - | 2624 | `				/* move to next node */` |
|      23 | 2625 | `				pIt = pIt->pPrev;` |
|      23 | 2626 | `				if( keep == 0 ) break;` |
|       1 | 2627 | `			}` |
|      21 | 2628 | `			if( keep == 0 ) break;` |
|       7 | 2629 | `		}` |
|      17 | 2630 | `		if( keep ){` |
|       - | 2631 | `			/* Perform the insertion */` |
|       9 | 2632 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 2633 | `		}` |
|       - | 2634 | `		/* Point to the next entry */` |
|      17 | 2635 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      17 | 2636 | `		n--;` |
|       1 | 2637 | `	}` |
|       - | 2638 | `	/* Return the freshly created array */` |
|       9 | 2639 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 2640 | `	return PH7_OK;` |
|      16 | 2641 | `}` |
|       - | 2642 | `/*` |
|       - | 2643 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 2644 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 2645 | ` * Parameters` |
|       - | 2646 | ` *  $array1` |
|       - | 2647 | ` *    The array to compare from` |
|       - | 2648 | ` *  $array2` |
|       - | 2649 | ` *    An array to compare against` |
|       - | 2650 | ` *  $...` |
|       - | 2651 | ` *   More arrays to compare against` |
|       - | 2652 | ` * Return` |
|       - | 2653 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 2654 | ` *  in any of the other arrays.` |
|       - | 2655 | ` * Note that NULL is returned on failure.` |
|       - | 2656 | ` */` |
|      14 | 2657 | `PH7_PRIVATE int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2658 | `{` |
|       - | 2659 | `	ph7_hashmap_node *pEntry;` |
|       - | 2660 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2661 | `	ph7_value *pArray;` |
|       - | 2662 | `	sxi32 rc;` |
|       - | 2663 | `	sxu32 n;` |
|       - | 2664 | `	int i;` |
|       - | 2665 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 2666 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 2667 | `	 * helpers. */` |
|      17 | 2668 | `	if( nArg < 1 ){` |
|     ! 0 | 2669 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2670 | `			"ArgumentCountError",` |
|       - | 2671 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|     ! 0 | 2672 | `			nArg` |
|       - | 2673 | `			);` |
|       - | 2674 | `	}` |
|      17 | 2675 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2676 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2677 | `			"TypeError",` |
|       - | 2678 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2679 | `			ph7_type_name(apArg[0])` |
|       - | 2680 | `			);` |
|       - | 2681 | `	}` |
|      24 | 2682 | `	for(i = 1 ; i < nArg ; i++){` |
|      14 | 2683 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 2684 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2685 | `				"TypeError",` |
|       - | 2686 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 2687 | `				i + 1,` |
|       2 | 2688 | `				ph7_type_name(apArg[i])` |
|       - | 2689 | `				);` |
|       - | 2690 | `		}` |
|       6 | 2691 | `	}` |
|      11 | 2692 | `	if( nArg == 1 ){` |
|       - | 2693 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 2694 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2695 | `		return PH7_OK;` |
|       - | 2696 | `	}` |
|       - | 2697 | `	/* Create a new array */` |
|       9 | 2698 | `	pArray = ph7_context_new_array(pCtx);` |
|       9 | 2699 | `	if( pArray == 0 ){` |
|     ! 0 | 2700 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2701 | `		return PH7_OK;` |
|       - | 2702 | `	}` |
|       - | 2703 | `	/* Point to the internal representation of the main hashmap */` |
|       9 | 2704 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2705 | `	/* Perfrom the diff */` |
|       9 | 2706 | `	pEntry = pSrc->pFirst;` |
|       9 | 2707 | `	n = pSrc->nEntry;` |
|     269 | 2708 | `	for(;;){` |
|     539 | 2709 | `		if( n < 1 ){` |
|       9 | 2710 | `			break;` |
|       - | 2711 | `		}` |
|    1047 | 2712 | `		for( i = 1 ; i < nArg ; i++ ){` |
|     535 | 2713 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 2714 | `				/* ignore */` |
|     ! 0 | 2715 | `				continue;` |
|       - | 2716 | `			}` |
|     535 | 2717 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|     535 | 2718 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      17 | 2719 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 2720 | `				/* Blob lookup */` |
|      17 | 2721 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|       9 | 2722 | `			}else{` |
|       - | 2723 | `				/* Int lookup */` |
|     519 | 2724 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 2725 | `			}` |
|     535 | 2726 | `			if( rc == SXRET_OK ){` |
|       - | 2727 | `				/* Key exists,break immediately */` |
|      19 | 2728 | `				break;` |
|       - | 2729 | `			}` |
|     259 | 2730 | `		}` |
|     531 | 2731 | `		if( i >= nArg ){` |
|       - | 2732 | `			/* Perform the insertion */` |
|     513 | 2733 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|     256 | 2734 | `		}` |
|       - | 2735 | `		/* Point to the next entry */` |
|     531 | 2736 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     531 | 2737 | `		n--;` |
|       1 | 2738 | `	}` |
|       - | 2739 | `	/* Return the freshly created array */` |
|       9 | 2740 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 2741 | `	return PH7_OK;` |
|      10 | 2742 | `}` |
|       - | 2743 | `/*` |
|       - | 2744 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 2745 | ` *  Computes the intersection of arrays.` |
|       - | 2746 | ` * Parameters` |
|       - | 2747 | ` *  $array1` |
|       - | 2748 | ` *    The array to compare from` |
|       - | 2749 | ` *  $array2` |
|       - | 2750 | ` *    An array to compare against` |
|       - | 2751 | ` *  $...` |
|       - | 2752 | ` *   More arrays to compare against` |
|       - | 2753 | ` * Return` |
|       - | 2754 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 2755 | ` *  in all of the parameters.` |
|       - | 2756 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 2757 | ` * Throws TypeError if any argument is not an array.` |
|       - | 2758 | ` */` |
|      20 | 2759 | `PH7_PRIVATE int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2760 | `{` |
|       - | 2761 | `	ph7_hashmap_node *pEntry;` |
|       - | 2762 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2763 | `	ph7_value *pArray;` |
|       - | 2764 | `	ph7_value *pVal;` |
|       - | 2765 | `	sxi32 rc;` |
|       - | 2766 | `	sxu32 n;` |
|       - | 2767 | `	int i;` |
|      23 | 2768 | `	if( nArg < 1 ){` |
|     ! 0 | 2769 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2770 | `			"ArgumentCountError",` |
|       - | 2771 | `			"array_intersect() expects at least 1 argument, %d given",` |
|     ! 0 | 2772 | `			nArg` |
|       - | 2773 | `			);` |
|       - | 2774 | `	}` |
|      23 | 2775 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2776 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2777 | `			"TypeError",` |
|       - | 2778 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2779 | `			ph7_type_name(apArg[0])` |
|       - | 2780 | `			);` |
|       - | 2781 | `	}` |
|      36 | 2782 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 2783 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 2784 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2785 | `				"TypeError",` |
|       - | 2786 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       1 | 2787 | `				i + 1,` |
|       2 | 2788 | `				ph7_type_name(apArg[i])` |
|       - | 2789 | `				);` |
|       - | 2790 | `		}` |
|       9 | 2791 | `	}` |
|      17 | 2792 | `	if( nArg == 1 ){` |
|       - | 2793 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 2794 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2795 | `		return PH7_OK;` |
|       - | 2796 | `	}` |
|       - | 2797 | `	/* Create a new array */` |
|      15 | 2798 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 2799 | `	if( pArray == 0 ){` |
|     ! 0 | 2800 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2801 | `		return PH7_OK;` |
|       - | 2802 | `	}` |
|       - | 2803 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 2804 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2805 | `	/* Perform the intersection */` |
|      15 | 2806 | `	pEntry = pSrc->pFirst;` |
|      15 | 2807 | `	n = pSrc->nEntry;` |
|      31 | 2808 | `	for(;;){` |
|      63 | 2809 | `		if( n < 1 ){` |
|      15 | 2810 | `			break;` |
|       - | 2811 | `		}` |
|       - | 2812 | `		/* Extract the node value */` |
|      49 | 2813 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      49 | 2814 | `		if( pVal ){` |
|      79 | 2815 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 2816 | `				/* Point to the internal representation of the hashmap */` |
|      55 | 2817 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2818 | `				/* Perform the lookup */` |
|      55 | 2819 | `				rc = HashmapFindValue(pMap,pVal,0,TRUE);` |
|      55 | 2820 | `				if( rc != SXRET_OK ){` |
|       - | 2821 | `					/* Value does not exist */` |
|      25 | 2822 | `					break;` |
|       - | 2823 | `				}` |
|      16 | 2824 | `			}` |
|      49 | 2825 | `			if( i >= nArg ){` |
|       - | 2826 | `				/* Perform the insertion */` |
|      25 | 2827 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 2828 | `			}` |
|      24 | 2829 | `		}` |
|       - | 2830 | `		/* Point to the next entry */` |
|      49 | 2831 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      49 | 2832 | `		n--;` |
|       1 | 2833 | `	}` |
|       - | 2834 | `	/* Return the freshly created array */` |
|      15 | 2835 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 2836 | `	return PH7_OK;` |
|      13 | 2837 | `}` |
|       - | 2838 | `/*` |
|       - | 2839 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 2840 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 2841 | ` * Parameters` |
|       - | 2842 | ` *  $array1` |
|       - | 2843 | ` *    The array to compare from` |
|       - | 2844 | ` *  $array2` |
|       - | 2845 | ` *    An array to compare against` |
|       - | 2846 | ` *  $...` |
|       - | 2847 | ` *   More arrays to compare against` |
|       - | 2848 | ` * Return` |
|       - | 2849 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 2850 | ` *  in all the arguments, with matching keys.` |
|       - | 2851 | ` */` |
|      20 | 2852 | `PH7_PRIVATE int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2853 | `{` |
|       - | 2854 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 2855 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2856 | `	ph7_value *pArray;` |
|       - | 2857 | `	ph7_value *pVal;` |
|       - | 2858 | `	sxi32 rc;` |
|       - | 2859 | `	sxu32 n;` |
|       - | 2860 | `	int i;` |
|      23 | 2861 | `	if( nArg < 1 ){` |
|     ! 0 | 2862 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2863 | `			"ArgumentCountError",` |
|       - | 2864 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|     ! 0 | 2865 | `			nArg` |
|       - | 2866 | `			);` |
|       - | 2867 | `	}` |
|      23 | 2868 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2869 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2870 | `			"TypeError",` |
|       - | 2871 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2872 | `			ph7_type_name(apArg[0])` |
|       - | 2873 | `			);` |
|       - | 2874 | `	}` |
|      36 | 2875 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 2876 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 2877 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2878 | `				"TypeError",` |
|       - | 2879 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 2880 | `				i + 1,` |
|       2 | 2881 | `				ph7_type_name(apArg[i])` |
|       - | 2882 | `				);` |
|       - | 2883 | `		}` |
|       9 | 2884 | `	}` |
|      17 | 2885 | `	if( nArg == 1 ){` |
|       - | 2886 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 2887 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2888 | `		return PH7_OK;` |
|       - | 2889 | `	}` |
|       - | 2890 | `	/* Create a new array */` |
|      15 | 2891 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 2892 | `	if( pArray == 0 ){` |
|     ! 0 | 2893 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2894 | `		return PH7_OK;` |
|       - | 2895 | `	}` |
|       - | 2896 | `	/* Point to the internal representation of the source hashmap */` |
|      15 | 2897 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2898 | `	/* Perform the intersection */` |
|      15 | 2899 | `	pEntry = pSrc->pFirst;` |
|      15 | 2900 | `	n = pSrc->nEntry;` |
|      15 | 2901 | `	pN1 = pN2 = 0; /* cc warning */` |
|      23 | 2902 | `	for(;;){` |
|      47 | 2903 | `		if( n < 1 ){` |
|      15 | 2904 | `			break;` |
|       - | 2905 | `		}` |
|       - | 2906 | `		/* Extract the node value */` |
|      33 | 2907 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      33 | 2908 | `		if( pVal ){` |
|      53 | 2909 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 2910 | `				/* Point to the internal representation of the hashmap */` |
|      37 | 2911 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2912 | `				/* Perform a key lookup first */` |
|      37 | 2913 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      15 | 2914 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|       8 | 2915 | `				}else{` |
|      23 | 2916 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 2917 | `				}` |
|      37 | 2918 | `				if( rc != SXRET_OK ){` |
|       - | 2919 | `					/* No such key,break immediately */` |
|       7 | 2920 | `					break;` |
|       - | 2921 | `				}` |
|       - | 2922 | `				/* Perform the lookup */` |
|      31 | 2923 | `				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);` |
|      31 | 2924 | `				if( rc != SXRET_OK \|\| pN1 != pN2 ){` |
|       - | 2925 | `					/* Value does not exist */` |
|       6 | 2926 | `					break;` |
|       - | 2927 | `				}` |
|      11 | 2928 | `			}` |
|      33 | 2929 | `			if( i >= nArg ){` |
|       - | 2930 | `				/* Perform the insertion */` |
|      17 | 2931 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       8 | 2932 | `			}` |
|      16 | 2933 | `		}` |
|       - | 2934 | `		/* Point to the next entry */` |
|      33 | 2935 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 2936 | `		n--;` |
|       1 | 2937 | `	}` |
|       - | 2938 | `	/* Return the freshly created array */` |
|      15 | 2939 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 2940 | `	return PH7_OK;` |
|      13 | 2941 | `}` |
|       - | 2942 | `/*` |
|       - | 2943 | ` * array array_intersect_key(array $array1 ,...)` |
|       - | 2944 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 2945 | ` * Parameters` |
|       - | 2946 | ` *  $array1` |
|       - | 2947 | ` *    The array to compare from` |
|       - | 2948 | ` *  $...` |
|       - | 2949 | ` *   More arrays to compare against` |
|       - | 2950 | ` * Return` |
|       - | 2951 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 2952 | ` *  have keys that are present in all arguments.` |
|       - | 2953 | ` * Note that NULL is returned on failure.` |
|       - | 2954 | ` */` |
|      20 | 2955 | `PH7_PRIVATE int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2956 | `{` |
|       - | 2957 | `	ph7_hashmap_node *pEntry;` |
|       - | 2958 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2959 | `	ph7_value *pArray;` |
|       - | 2960 | `	sxi32 rc;` |
|       - | 2961 | `	sxu32 n;` |
|       - | 2962 | `	int i;` |
|      23 | 2963 | `	if( nArg < 1 ){` |
|     ! 0 | 2964 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2965 | `			"ArgumentCountError",` |
|       - | 2966 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|     ! 0 | 2967 | `			nArg` |
|       - | 2968 | `			);` |
|       - | 2969 | `	}` |
|      23 | 2970 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2971 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2972 | `			"TypeError",` |
|       - | 2973 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2974 | `			ph7_type_name(apArg[0])` |
|       - | 2975 | `			);` |
|       - | 2976 | `	}` |
|      36 | 2977 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 2978 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 2979 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2980 | `				"TypeError",` |
|       - | 2981 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|       1 | 2982 | `				i + 1,` |
|       2 | 2983 | `				ph7_type_name(apArg[i])` |
|       - | 2984 | `				);` |
|       - | 2985 | `		}` |
|       9 | 2986 | `	}` |
|      17 | 2987 | `	if( nArg == 1 ){` |
|       - | 2988 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 2989 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2990 | `		return PH7_OK;` |
|       - | 2991 | `	}` |
|       - | 2992 | `	/* Create a new array */` |
|      15 | 2993 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 | 2994 | `	if( pArray == 0 ){` |
|     ! 0 | 2995 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2996 | `		return PH7_OK;` |
|       - | 2997 | `	}` |
|       - | 2998 | `	/* Point to the internal representation of the main hashmap */` |
|      15 | 2999 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3000 | `	/* Perform the intersection */` |
|      15 | 3001 | `	pEntry = pSrc->pFirst;` |
|      15 | 3002 | `	n = pSrc->nEntry;` |
|      24 | 3003 | `	for(;;){` |
|      49 | 3004 | `		if( n < 1 ){` |
|      15 | 3005 | `			break;` |
|       - | 3006 | `		}` |
|      57 | 3007 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      39 | 3008 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      39 | 3009 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      27 | 3010 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 3011 | `				/* Blob lookup */` |
|      27 | 3012 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      14 | 3013 | `			}else{` |
|       - | 3014 | `				/* Int key */` |
|      13 | 3015 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 3016 | `			}` |
|      39 | 3017 | `			if( rc != SXRET_OK ){` |
|       - | 3018 | `				/* Key does not exist, break immediately */` |
|      17 | 3019 | `				break;` |
|       - | 3020 | `			}` |
|      12 | 3021 | `		}` |
|      35 | 3022 | `		if( i >= nArg ){` |
|       - | 3023 | `			/* Perform the insertion */` |
|      19 | 3024 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       9 | 3025 | `		}` |
|       - | 3026 | `		/* Point to the next entry */` |
|      35 | 3027 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      35 | 3028 | `		n--;` |
|       1 | 3029 | `	}` |
|       - | 3030 | `	/* Return the freshly created array */` |
|      15 | 3031 | `	ph7_result_value(pCtx,pArray);` |
|      15 | 3032 | `	return PH7_OK;` |
|      13 | 3033 | `}` |
|       - | 3034 | `/*` |
|       - | 3035 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 3036 | ` *  Computes the intersection of arrays.` |
|       - | 3037 | ` * Parameters` |
|       - | 3038 | ` *  $array1` |
|       - | 3039 | ` *    The array to compare from` |
|       - | 3040 | ` *  $array2` |
|       - | 3041 | ` *    An array to compare against` |
|       - | 3042 | ` *  $...` |
|       - | 3043 | ` *   More arrays to compare against` |
|       - | 3044 | ` * $callback` |
|       - | 3045 | ` *  The callback comparison function.` |
|       - | 3046 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 3047 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 3048 | ` *  than the second.` |
|       - | 3049 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 3050 | ` * Return` |
|       - | 3051 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 3052 | ` *  in all of the parameters. .` |
|       - | 3053 | ` * Note that NULL is returned on failure.` |
|       - | 3054 | ` */` |
|      26 | 3055 | `PH7_PRIVATE int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3056 | `{` |
|       - | 3057 | `	ph7_hashmap_node *pEntry;` |
|       - | 3058 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3059 | `	ph7_value *pCallback;` |
|       - | 3060 | `	ph7_value *pArray;` |
|       - | 3061 | `	ph7_value *pVal;` |
|       - | 3062 | `	sxi32 rc;` |
|       - | 3063 | `	sxu32 n;` |
|       - | 3064 | `	int i;` |
|       - | 3065 |  |
|       - | 3066 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      31 | 3067 | `	if( nArg < 2 ){` |
|     ! 0 | 3068 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3069 | `			"ArgumentCountError",` |
|       - | 3070 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|     ! 0 | 3071 | `			nArg` |
|       - | 3072 | `			);` |
|       - | 3073 | `	}` |
|      31 | 3074 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3075 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3076 | `			"TypeError",` |
|       - | 3077 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3078 | `			ph7_type_name(apArg[0])` |
|       - | 3079 | `			);` |
|       - | 3080 | `	}` |
|       - | 3081 |  |
|       - | 3082 | `	/* php checks the CALLBACK before the intermediary arrays (see array_udiff). */` |
|      29 | 3083 | `	pCallback = apArg[nArg - 1];` |
|      29 | 3084 | `	if( !ph7_value_is_callable(pCallback) ){` |
|      16 | 3085 | `		if( ph7_value_is_array(pCallback) ){` |
|       - | 3086 | `			/* PHP emits a special message when the array length is wrong.` |
|       - | 3087 | `			 * If the array has two elements but is still not callable (e.g. missing` |
|       - | 3088 | `			 * method / missing class), we must emit a more general error instead.` |
|       - | 3089 | `			 */` |
|       9 | 3090 | `			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;` |
|       9 | 3091 | `			if( pCb->nEntry != 2 ){` |
|       4 | 3092 | `				return PH7_VmThrowException(pCtx,` |
|       - | 3093 | `					"TypeError",` |
|       - | 3094 | `					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",` |
|       1 | 3095 | `					nArg` |
|       - | 3096 | `					);` |
|       - | 3097 | `			}` |
|       - | 3098 | `			/* Try to provide a more precise error like PHP does for missing classes/methods. */` |
|       - | 3099 | `			{` |
|       6 | 3100 | `				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);` |
|       6 | 3101 | `				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);` |
|       6 | 3102 | `				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){` |
|       - | 3103 | `					int nMethodLen;` |
|       6 | 3104 | `					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);` |
|       6 | 3105 | `					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);` |
|       6 | 3106 | `					if( pClass ){` |
|       - | 3107 | `						/* Class exists but method is missing. */` |
|       4 | 3108 | `						return PH7_VmThrowException(pCtx,` |
|       - | 3109 | `							"TypeError",` |
|       - | 3110 | `							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",` |
|       1 | 3111 | `							nArg,` |
|       1 | 3112 | `							(const char *)SyStringData(&pClass->sName),` |
|       1 | 3113 | `							zMethod` |
|       - | 3114 | `							);` |
|       - | 3115 | `					}` |
|       - | 3116 | `					/* Class not found */` |
|       - | 3117 | `					{` |
|       - | 3118 | `						int nName;` |
|       3 | 3119 | `						const char *zName = ph7_value_to_string(pKey,&nName);` |
|       4 | 3120 | `						return PH7_VmThrowException(pCtx,` |
|       - | 3121 | `							"TypeError",` |
|       - | 3122 | `							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",` |
|       1 | 3123 | `							nArg,` |
|       1 | 3124 | `							zName` |
|       - | 3125 | `							);` |
|       - | 3126 | `					}` |
|       - | 3127 | `				}` |
|       - | 3128 | `			}` |
|       - | 3129 | `			/* Fallback message */` |
|     ! 0 | 3130 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3131 | `				"TypeError",` |
|       - | 3132 | `				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|     ! 0 | 3133 | `				nArg` |
|       - | 3134 | `				);` |
|       - | 3135 | `		}` |
|       8 | 3136 | `		if( ph7_value_is_string(pCallback) ){` |
|       - | 3137 | `			int len;` |
|       6 | 3138 | `			const char *zName = ph7_value_to_string(pCallback, &len);` |
|       8 | 3139 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3140 | `				"TypeError",` |
|       - | 3141 | `				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",` |
|       2 | 3142 | `				nArg,` |
|       2 | 3143 | `				zName` |
|       - | 3144 | `				);` |
|       - | 3145 | `		}` |
|       4 | 3146 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3147 | `			"TypeError",` |
|       - | 3148 | `			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",` |
|       1 | 3149 | `			nArg` |
|       - | 3150 | `			);` |
|       - | 3151 | `	}` |
|       - | 3152 |  |
|       - | 3153 | `	/* Now the intermediary arrays, left to right. */` |
|      20 | 3154 | `	for( i = 1 ; i < nArg - 1; i++ ){` |
|      10 | 3155 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3156 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3157 | `				"TypeError",` |
|       - | 3158 | `				"array_uintersect(): Argument #%d must be of type array, %s given",` |
|       1 | 3159 | `				i + 1,` |
|       2 | 3160 | `				ph7_type_name(apArg[i])` |
|       - | 3161 | `				);` |
|       - | 3162 | `		}` |
|       4 | 3163 | `	}` |
|       - | 3164 |  |
|      11 | 3165 | `	if( nArg == 2 ){` |
|       - | 3166 | `		/* Only the original array and the callback were provided. */` |
|       5 | 3167 | `		ph7_result_value(pCtx,apArg[0]);` |
|       5 | 3168 | `		return PH7_OK;` |
|       - | 3169 | `	}` |
|       - | 3170 |  |
|       - | 3171 | `	/* Create a new array */` |
|       7 | 3172 | `	pArray = ph7_context_new_array(pCtx);` |
|       7 | 3173 | `	if( pArray == 0 ){` |
|     ! 0 | 3174 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3175 | `		return PH7_OK;` |
|       - | 3176 | `	}` |
|       - | 3177 | `	/* Point to the internal representation of the source hashmap */` |
|       7 | 3178 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3179 | `	/* Perform the intersection */` |
|       7 | 3180 | `	pEntry = pSrc->pFirst;` |
|       7 | 3181 | `	n = pSrc->nEntry;` |
|       7 | 3182 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|       9 | 3183 | `	for(;;){` |
|      19 | 3184 | `		if( n < 1 ){` |
|       5 | 3185 | `			break;` |
|       - | 3186 | `		}` |
|       - | 3187 | `		/* Extract the node value */` |
|      15 | 3188 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      15 | 3189 | `		if( pVal ){` |
|      23 | 3190 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|      15 | 3191 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 3192 | `					/* ignore */` |
|     ! 0 | 3193 | `					continue;` |
|       - | 3194 | `				}` |
|       - | 3195 | `				/* Point to the internal representation of the hashmap */` |
|      15 | 3196 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3197 | `				/* Perform the lookup */` |
|      15 | 3198 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|      15 | 3199 | `				if( rc != SXRET_OK ){` |
|       - | 3200 | `					/* Value does not exist */` |
|       7 | 3201 | `					break;` |
|       - | 3202 | `				}` |
|       5 | 3203 | `			}` |
|      15 | 3204 | `			if( i >= (nArg-1) ){` |
|       - | 3205 | `				/* Perform the insertion */` |
|       9 | 3206 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       4 | 3207 | `			}` |
|       7 | 3208 | `		}` |
|      15 | 3209 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 3210 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 3211 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 3212 | `			return PH7_EXCEPTION;` |
|       - | 3213 | `		}` |
|       - | 3214 | `		/* Point to the next entry */` |
|      13 | 3215 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      13 | 3216 | `		n--;` |
|       1 | 3217 | `	}` |
|       - | 3218 | `	/* Return the freshly created array */` |
|       5 | 3219 | `	ph7_result_value(pCtx,pArray);` |
|       5 | 3220 | `	return PH7_OK;` |
|      18 | 3221 | `}` |
|       - | 3222 | `/*` |
|       - | 3223 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 3224 | ` *  Fill an array with values.` |
|       - | 3225 | ` * Parameters` |
|       - | 3226 | ` *  $start_index` |
|       - | 3227 | ` *    The first index of the returned array.` |
|       - | 3228 | ` *  $num` |
|       - | 3229 | ` *   Number of elements to insert.` |
|       - | 3230 | ` *  $value` |
|       - | 3231 | ` *    Value to use for filling.` |
|       - | 3232 | ` * Return` |
|       - | 3233 | ` *  The filled array or null on failure.` |
|       - | 3234 | ` */` |
|     248 | 3235 | `PH7_PRIVATE int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3236 | `{` |
|       - | 3237 | `	ph7_value *pArray;` |
|       - | 3238 | `	int i,nEntry;` |
|       - | 3239 |  |
|       - | 3240 | `	/* PHP enforces argument count and type checks. */` |
|     253 | 3241 | `	if( nArg != 3 ){` |
|       - | 3242 | `		/* wrong number of arguments -> ArgumentCountError */` |
|     ! 0 | 3243 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3244 | `			"ArgumentCountError",` |
|       - | 3245 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|     ! 0 | 3246 | `			nArg` |
|       - | 3247 | `			);` |
|       - | 3248 | `	}` |
|       - | 3249 |  |
|       - | 3250 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 3251 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 3252 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 3253 | `	 * and NULLs are rejected outright. */` |
|     372 | 3254 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     377 | 3255 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|     ! 0 | 3256 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3257 | `			"TypeError",` |
|       - | 3258 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|     ! 0 | 3259 | `			ph7_type_name(apArg[0])` |
|       - | 3260 | `			);` |
|       - | 3261 | `	}` |
|     253 | 3262 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 3263 | `		int len;` |
|       8 | 3264 | `		sxu8 bReal = FALSE;` |
|       8 | 3265 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       8 | 3266 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 3267 | `			/* Non‑numeric string is an error. */` |
|       4 | 3268 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3269 | `				"TypeError",` |
|       - | 3270 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 3271 | `				);` |
|       - | 3272 | `		}` |
|       5 | 3273 | `		if( bReal ){` |
|       - | 3274 | `			/* php only DEPRECATES the lossy float-string -> int narrowing; §10 rejects` |
|       - | 3275 | `			 * it loudly, and the throw ABORTS the call (php would fill the array).` |
|       - | 3276 | `			 * Twin-pinned by array_lossy_int_arg_abort{,_zend}.phpt. */` |
|       3 | 3277 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 3278 | `				"Implicit conversion from float-string to int loses precision");` |
|       - | 3279 | `		}` |
|       1 | 3280 | `	}` |
|       - | 3281 |  |
|       - | 3282 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 3283 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     366 | 3284 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     370 | 3285 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 3286 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3287 | `			"TypeError",` |
|       - | 3288 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 3289 | `			ph7_type_name(apArg[1])` |
|       - | 3290 | `			);` |
|       - | 3291 | `	}` |
|     248 | 3292 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 3293 | `		int len;` |
|       3 | 3294 | `		sxu8 bReal = FALSE;` |
|       3 | 3295 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 3296 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 3297 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3298 | `				"TypeError",` |
|       - | 3299 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 3300 | `				);` |
|       - | 3301 | `		}` |
|     ! 0 | 3302 | `	}` |
|       - | 3303 | `	/* Note: booleans and WHOLE floats are accepted and converted by ph7_value_to_int` |
|       - | 3304 | `	 * below; a FRACTIONAL float is the §10 lossy narrowing and aborts the call. */` |
|     245 | 3305 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       8 | 3306 | `		double d = ph7_value_to_double(apArg[1]);` |
|       - | 3307 | `		/* avoid hiding outer 'i' (loop index) */` |
|       8 | 3308 | `		sxi64 i64 = (sxi64)d;` |
|       8 | 3309 | `		if( d != (double)i64 ){` |
|       6 | 3310 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 3311 | `				"Implicit conversion from float to int loses precision");` |
|       - | 3312 | `		}` |
|       1 | 3313 | `	}` |
|       - | 3314 |  |
|       - | 3315 | `	/* Total number of entries to insert. Read as 64-bit FIRST: the old 32-bit` |
|       - | 3316 | `	 * read truncated array_fill(0, PHP_INT_MAX, x) to -1 and reported the` |
|       - | 3317 | `	 * negative-count message where php says "is too large". */` |
|     240 | 3318 | `	sxi64 nEntry64 = ph7_value_to_int64(apArg[1]);` |
|       - | 3319 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     240 | 3320 | `	if( nEntry64 < 0 ){` |
|       6 | 3321 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3322 | `			"ValueError",` |
|       - | 3323 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 3324 | `			);` |
|       - | 3325 | `	}` |
|     235 | 3326 | `	if( nEntry64 > 0x7fffffff ){` |
|       - | 3327 | `		/* php's threshold (probed 8.5.8): count > INT32_MAX is the distinct` |
|       - | 3328 | `		 * "is too large" ValueError; INT32_MAX itself proceeds to allocation` |
|       - | 3329 | `		 * (php then dies on the overflowing allocation, PHL OOMs gracefully). */` |
|       5 | 3330 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3331 | `			"ValueError",` |
|       - | 3332 | `			"array_fill(): Argument #2 ($count) is too large"` |
|       - | 3333 | `			);` |
|       - | 3334 | `	}` |
|     231 | 3335 | `	nEntry = (int)nEntry64;` |
|       - | 3336 |  |
|       - | 3337 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     231 | 3338 | `	if( nEntry == 0 ){` |
|       5 | 3339 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       5 | 3340 | `		return PH7_OK;` |
|       - | 3341 | `	}` |
|       - | 3342 |  |
|       - | 3343 | `	/* Create a new array */` |
|     227 | 3344 | `	pArray = ph7_context_new_array(pCtx);` |
|     227 | 3345 | `	if( pArray == 0 ){` |
|     ! 0 | 3346 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 3347 | `	}` |
|       - | 3348 |  |
|       - | 3349 | `	/* PHP 8 fills consecutive integer keys start_index, start_index+1, … even` |
|       - | 3350 | `	 * when start_index is negative (PHP 7 restarted the remaining keys from 0,` |
|       - | 3351 | `	 * so array_fill(-5,3) gave -5,0,1 instead of -5,-4,-3). Assign each key` |
|       - | 3352 | `	 * explicitly rather than relying on automatic (append) indexing. */` |
|     227 | 3353 | `	int iStart = ph7_value_to_int(apArg[0]);` |
| 2117833 | 3354 | `	for( i = 0 ; i < nEntry ; i++ ){` |
| 2117607 | 3355 | `		if( ph7_array_add_intkey_elem(pArray, iStart + i, apArg[2]) != SXRET_OK ){` |
|       - | 3356 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|     ! 0 | 3357 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 3358 | `		}` |
| 1058804 | 3359 | `	}` |
|       - | 3360 | `	/* Return the filled array */` |
|     227 | 3361 | `	ph7_result_value(pCtx, pArray);` |
|     227 | 3362 | `	return PH7_OK;` |
|     129 | 3363 | `}` |
|       - | 3364 | `/*` |
|       - | 3365 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 3366 | ` *  Fill an array with values, specifying keys.` |
|       - | 3367 | ` * Parameters` |
|       - | 3368 | ` *  $input` |
|       - | 3369 | ` *   Array of values that will be used as key.` |
|       - | 3370 | ` *  $value` |
|       - | 3371 | ` *    Value to use for filling.` |
|       - | 3372 | ` * Return` |
|       - | 3373 | ` *  The filled array.` |
|       - | 3374 | ` * Throws` |
|       - | 3375 | ` *  ValueError if $input is not an array.` |
|       - | 3376 | ` */` |
|      20 | 3377 | `PH7_PRIVATE int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3378 | `{` |
|       - | 3379 | `	ph7_hashmap_node *pEntry;` |
|       - | 3380 | `	ph7_hashmap *pSrc;` |
|       - | 3381 | `	ph7_value *pArray;` |
|       - | 3382 | `	sxu32 n;` |
|       - | 3383 | `	/* PHP enforces exactly 2 arguments. */` |
|      23 | 3384 | `	if( nArg != 2 ){` |
|     ! 0 | 3385 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3386 | `			"ArgumentCountError",` |
|       - | 3387 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|     ! 0 | 3388 | `			nArg` |
|       - | 3389 | `			);` |
|       - | 3390 | `	}` |
|       - | 3391 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 | 3392 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       8 | 3393 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3394 | `			"TypeError",` |
|       - | 3395 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|       2 | 3396 | `			ph7_type_name(apArg[0])` |
|       - | 3397 | `			);` |
|       - | 3398 | `	}` |
|       - | 3399 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 3400 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3401 | `	/* Create a new array */` |
|      17 | 3402 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 3403 | `	if( pArray == 0 ){` |
|     ! 0 | 3404 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3405 | `		return PH7_OK;` |
|       - | 3406 | `	}` |
|       - | 3407 | `	/* Perform the requested operation */` |
|      17 | 3408 | `	pEntry = pSrc->pFirst;` |
|      45 | 3409 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      29 | 3410 | `		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);` |
|       - | 3411 | `		/* Point to the next entry */` |
|      29 | 3412 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      15 | 3413 | `	}` |
|       - | 3414 | `	/* Return the filled array */` |
|      17 | 3415 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 3416 | `	return PH7_OK;` |
|      13 | 3417 | `}` |
|       - | 3418 | `/*` |
|       - | 3419 | ` * array array_combine(array $keys,array $values)` |
|       - | 3420 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 3421 | ` * Parameters` |
|       - | 3422 | ` *  $keys` |
|       - | 3423 | ` *    Array of keys to be used.` |
|       - | 3424 | ` * $values` |
|       - | 3425 | ` *   Array of values to be used.` |
|       - | 3426 | ` * Return` |
|       - | 3427 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 3428 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 3429 | ` *  not an array.` |
|       - | 3430 | ` */` |
|      16 | 3431 | `PH7_PRIVATE int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 3432 | `{` |
|       - | 3433 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 3434 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 3435 | `	ph7_value *pArray;` |
|       - | 3436 | `	sxu32 n;` |
|       - | 3437 | `	/* PHP enforces argument count and type checks. */` |
|      20 | 3438 | `	if( nArg != 2 ){` |
|       - | 3439 | `		/* wrong number of arguments -> ArgumentCountError */` |
|     ! 0 | 3440 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3441 | `			"ArgumentCountError",` |
|       - | 3442 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|     ! 0 | 3443 | `			nArg` |
|       - | 3444 | `			);` |
|       - | 3445 | `	}` |
|       - | 3446 | `	/* Validate argument types individually so we can report the correct` |
|       - | 3447 | `	 * argument index in the error message. */` |
|      20 | 3448 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3449 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3450 | `			"TypeError",` |
|       - | 3451 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 3452 | `			ph7_type_name(apArg[0])` |
|       - | 3453 | `			);` |
|       - | 3454 | `	}` |
|      17 | 3455 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 3456 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3457 | `			"TypeError",` |
|       - | 3458 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 3459 | `			ph7_type_name(apArg[1])` |
|       - | 3460 | `			);` |
|       - | 3461 | `	}` |
|       - | 3462 | `	/* Point to the internal representation of the input hashmaps */` |
|      14 | 3463 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      14 | 3464 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      14 | 3465 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 3466 | `		/* Length mismatch -> ValueError */` |
|       3 | 3467 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3468 | `			"ValueError",` |
|       - | 3469 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 3470 | `			);` |
|       - | 3471 | `	}` |
|       - | 3472 | `	/* Create a new array */` |
|      11 | 3473 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 3474 | `	if( pArray == 0 ){` |
|     ! 0 | 3475 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3476 | `		return PH7_OK;` |
|       - | 3477 | `	}` |
|       - | 3478 | `	/* Perform the requested operation */` |
|      11 | 3479 | `	pKe = pKey->pFirst;` |
|      11 | 3480 | `	pVe = pValue->pFirst;` |
|      33 | 3481 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      23 | 3482 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      23 | 3483 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 3484 | `		/* PHP treats floats used as keys in array_combine differently than` |
|       - | 3485 | `		 * ordinary offset access: the float is stringified rather than` |
|       - | 3486 | `		 * truncated.  To emulate this behavior we create a temporary copy of` |
|       - | 3487 | `		 * the value when it is a float and convert the copy to string.  The` |
|       - | 3488 | `		 * original array must not be mutated. */` |
|      23 | 3489 | `		ph7_value *pKeyCopy = pKeyVal;` |
|      23 | 3490 | `		if( ph7_value_is_float(pKeyVal) ){` |
|       5 | 3491 | `			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);` |
|       5 | 3492 | `			if( pTmpKey ){` |
|       5 | 3493 | `				PH7_MemObjStore(pKeyVal,pTmpKey);` |
|       - | 3494 | `				/* Convert copy to string so it becomes "1.5" or "2" etc. */` |
|       5 | 3495 | `				PH7_MemObjToString(pTmpKey);` |
|       5 | 3496 | `				pKeyCopy = pTmpKey;` |
|       2 | 3497 | `			}` |
|       2 | 3498 | `		}` |
|      23 | 3499 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|       - | 3500 | `		/* Point to the next entry */` |
|      23 | 3501 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      23 | 3502 | `		pVe = pVe->pPrev;` |
|      12 | 3503 | `	}` |
|       - | 3504 | `	/* Return the filled array */` |
|      11 | 3505 | `	ph7_result_value(pCtx,pArray);` |
|      11 | 3506 | `	return PH7_OK;` |
|      12 | 3507 | `}` |
|       - | 3508 | `/*` |
|       - | 3509 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 3510 | ` *  Return an array with elements in reverse order.` |
|       - | 3511 | ` * Parameters` |
|       - | 3512 | ` *  $array` |
|       - | 3513 | ` *   The input array.` |
|       - | 3514 | ` *  $preserve_keys (optional)` |
|       - | 3515 | ` *   If set to TRUE keys are preserved.` |
|       - | 3516 | ` * Return` |
|       - | 3517 | ` *  The reversed array.` |
|       - | 3518 | ` */` |
|      18 | 3519 | `PH7_PRIVATE int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3520 | `{` |
|       - | 3521 | `	ph7_hashmap_node *pEntry;` |
|       - | 3522 | `	ph7_hashmap *pSrc;` |
|       - | 3523 | `	ph7_value *pArray;` |
|       - | 3524 | `	int bPreserve;` |
|       - | 3525 | `	sxu32 n;` |
|      20 | 3526 | `	if( nArg < 1 ){` |
|     ! 0 | 3527 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3528 | `			"ArgumentCountError",` |
|       - | 3529 | `			"array_reverse() expects at least 1 argument, %d given",` |
|     ! 0 | 3530 | `			nArg` |
|       - | 3531 | `			);` |
|       - | 3532 | `	}` |
|       - | 3533 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 3534 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3535 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3536 | `			"TypeError",` |
|       - | 3537 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3538 | `			ph7_type_name(apArg[0])` |
|       - | 3539 | `			);` |
|       - | 3540 | `	}` |
|      17 | 3541 | `	bPreserve = FALSE;` |
|      17 | 3542 | `	if( nArg > 1 ){` |
|       7 | 3543 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       3 | 3544 | `	}` |
|       - | 3545 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 3546 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3547 | `	/* Create a new array */` |
|      17 | 3548 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 3549 | `	if( pArray == 0 ){` |
|     ! 0 | 3550 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3551 | `		return PH7_OK;` |
|       - | 3552 | `	}` |
|       - | 3553 | `	/* Perform the requested operation */` |
|      17 | 3554 | `	pEntry = pSrc->pLast;` |
|      55 | 3555 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 3556 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      39 | 3557 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      39 | 3558 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 3559 | `		/* Point to the previous entry */` |
|      39 | 3560 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      20 | 3561 | `	}` |
|      17 | 3562 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 3563 | `	return PH7_OK;` |
|      11 | 3564 | `}` |
|       - | 3565 | `/*` |
|       - | 3566 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|       - | 3567 | ` *  Removes duplicate values from an array.` |
|       - | 3568 | ` * Parameters` |
|       - | 3569 | ` *  $array` |
|       - | 3570 | ` *   The input array.` |
|       - | 3571 | ` *  $flags` |
|       - | 3572 | ` *   The optional second parameter may be used to modify the comparison` |
|       - | 3573 | ` *   behavior using these values:` |
|       - | 3574 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|       - | 3575 | ` *     SORT_NUMERIC - compare items numerically` |
|       - | 3576 | ` *     SORT_STRING  - compare items as strings` |
|       - | 3577 | ` * Return` |
|       - | 3578 | ` *  The filtered array.` |
|       - | 3579 | ` */` |
|      36 | 3580 | `PH7_PRIVATE int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3581 | `{` |
|       - | 3582 | `	ph7_hashmap_node *pEntry;` |
|       - | 3583 | `	ph7_value *pNeedle;` |
|       - | 3584 | `	ph7_hashmap *pSrc;` |
|       - | 3585 | `	ph7_value *pArray;` |
|       - | 3586 | `	int iFlags,base,bFold;` |
|       - | 3587 | `	sxu32 n;` |
|      38 | 3588 | `	if( nArg < 1 ){` |
|       - | 3589 | `		/* Missing arguments, throw ArgumentCountError */` |
|     ! 0 | 3590 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3591 | `			"ArgumentCountError",` |
|       - | 3592 | `			"array_unique() expects at least 1 argument, 0 given"` |
|       - | 3593 | `			);` |
|       - | 3594 | `	}` |
|      38 | 3595 | `	if( nArg > 2 ){` |
|       - | 3596 | `		/* Too many arguments, throw ArgumentCountError */` |
|     ! 0 | 3597 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3598 | `			"ArgumentCountError",` |
|       - | 3599 | `			"array_unique() expects at most 2 arguments, %d given",` |
|     ! 0 | 3600 | `			nArg` |
|       - | 3601 | `			);` |
|       - | 3602 | `	}` |
|       - | 3603 | `	/* Make sure we are dealing with a valid hashmap */` |
|      38 | 3604 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3605 | `		/* Type mismatch, throw TypeError */` |
|       4 | 3606 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3607 | `			"TypeError",` |
|       - | 3608 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3609 | `			ph7_type_name(apArg[0])` |
|       - | 3610 | `			);` |
|       - | 3611 | `	}` |
|       - | 3612 | `	/* php's default is SORT_STRING (2): elements compare as strings. Explicit` |
|       - | 3613 | `	 * flags select numeric / natural / case-insensitive comparison. */` |
|      36 | 3614 | `	iFlags = nArg > 1 ? ph7_value_to_int(apArg[1]) : 2 /* SORT_STRING */;` |
|      36 | 3615 | `	base = iFlags & ~8;` |
|      36 | 3616 | `	bFold = (iFlags & 8) != 0;` |
|       - | 3617 | `	/* Point to the internal representation of the input hashmap */` |
|      36 | 3618 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3619 | `	/* Create a new array */` |
|      36 | 3620 | `	pArray = ph7_context_new_array(pCtx);` |
|      36 | 3621 | `	if( pArray == 0 ){` |
|     ! 0 | 3622 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3623 | `		return PH7_OK;` |
|       - | 3624 | `	}` |
|       - | 3625 | `	/* Perform the requested operation */` |
|      36 | 3626 | `	pEntry = pSrc->pFirst;` |
|     154 | 3627 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|     120 | 3628 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|     120 | 3629 | `		if( pNeedle ){` |
|       - | 3630 | `			/* Keep this element unless a flag-equal one is already present. */` |
|     120 | 3631 | `			ph7_hashmap *pKept = (ph7_hashmap *)pArray->x.pOther;` |
|     120 | 3632 | `			ph7_hashmap_node *pK = pKept->pFirst;` |
|     120 | 3633 | `			int bDup = 0;` |
|       - | 3634 | `			sxu32 i;` |
|       - | 3635 | `			/* Forward iteration in this map uses the pPrev link (see the outer` |
|       - | 3636 | `			 * loop over pSrc). */` |
|     184 | 3637 | `			for( i = 0 ; i < pKept->nEntry && pK ; ++i ){` |
|     122 | 3638 | `				ph7_value *pV = HashmapExtractNodeValue(pK);` |
|     122 | 3639 | `				if( pV && HashmapValueFlagEqual(pCtx->pVm,pNeedle,pV,base,bFold) ){` |
|      58 | 3640 | `					bDup = 1;` |
|      58 | 3641 | `					break;` |
|       - | 3642 | `				}` |
|      65 | 3643 | `				pK = pK->pPrev;` |
|      33 | 3644 | `			}` |
|     120 | 3645 | `			if( !bDup ){` |
|      64 | 3646 | `				HashmapInsertNode(pKept,pEntry,TRUE);` |
|      31 | 3647 | `			}` |
|      59 | 3648 | `		}` |
|       - | 3649 | `		/* Point to the next entry */` |
|     120 | 3650 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      61 | 3651 | `	}` |
|       - | 3652 | `	/* Return the freshly created array */` |
|      36 | 3653 | `	ph7_result_value(pCtx,pArray);` |
|      36 | 3654 | `	return PH7_OK;` |
|      20 | 3655 | `}` |
|       - | 3656 | `/*` |
|       - | 3657 | ` * array array_flip(array $input)` |
|       - | 3658 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 3659 | ` * Parameter` |
|       - | 3660 | ` *  $input` |
|       - | 3661 | ` *   Input array.` |
|       - | 3662 | ` * Return` |
|       - | 3663 | ` *   The flipped array on success or NULL on failure.` |
|       - | 3664 | ` */` |
|      32 | 3665 | `PH7_PRIVATE int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3666 | `{` |
|       - | 3667 | `	ph7_hashmap_node *pEntry;` |
|       - | 3668 | `	ph7_hashmap *pSrc;` |
|       - | 3669 | `	ph7_value *pArray;` |
|       - | 3670 | `	ph7_value *pKey;` |
|       - | 3671 | `	ph7_value sVal;` |
|       - | 3672 | `	sxu32 n;` |
|       - | 3673 |  |
|       - | 3674 | `	/* PHP requires exactly one argument */` |
|      34 | 3675 | `	if( nArg != 1 ){` |
|       - | 3676 | `		/* Use ArgumentCountError like other array helpers */` |
|     ! 0 | 3677 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3678 | `			"ArgumentCountError",` |
|       - | 3679 | `			"array_flip() expects exactly 1 argument, %d given",` |
|     ! 0 | 3680 | `			nArg` |
|       - | 3681 | `			);` |
|       - | 3682 | `	}` |
|       - | 3683 | `	/* Make sure we are dealing with a valid hashmap */` |
|      34 | 3684 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3685 | `		/* Type mismatch -> TypeError */` |
|       4 | 3686 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3687 | `			"TypeError",` |
|       - | 3688 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3689 | `			ph7_type_name(apArg[0])` |
|       - | 3690 | `			);` |
|       - | 3691 | `	}` |
|       - | 3692 | `	/* Point to the internal representation of the input hashmap */` |
|      31 | 3693 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3694 | `	/* Create a new array */` |
|      31 | 3695 | `	pArray = ph7_context_new_array(pCtx);` |
|      31 | 3696 | `	if( pArray == 0 ){` |
|     ! 0 | 3697 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3698 | `		return PH7_OK;` |
|       - | 3699 | `	}` |
|       - | 3700 | `	/* Start processing */` |
|      31 | 3701 | `	pEntry = pSrc->pFirst;` |
|   22279 | 3702 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 3703 | `		/* Extract the node value (will become a key in the result) */` |
|   22249 | 3704 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22249 | 3705 | `		if( pKey ){` |
|       - | 3706 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22249 | 3707 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 3708 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3709 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 3710 | `					);` |
|   22248 | 3711 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 3712 | `				/* Prepare the value for insertion (original key) */` |
|   22235 | 3713 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20003 | 3714 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10002 | 3715 | `				}else{` |
|       - | 3716 | `					SyString sStr;` |
|    2233 | 3717 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2233 | 3718 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 3719 | `				}` |
|       - | 3720 | `				/* Perform the insertion */` |
|   22235 | 3721 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 3722 | `				/* Safely release the value because each inserted entry` |
|       - | 3723 | `				 * has its own private copy of the value.` |
|       - | 3724 | `				 */` |
|   22235 | 3725 | `				PH7_MemObjRelease(&sVal);` |
|   11118 | 3726 | `			}else{` |
|       - | 3727 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|      13 | 3728 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3729 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 3730 | `					);` |
|       - | 3731 | `			}` |
|   11124 | 3732 | `		}` |
|       - | 3733 | `		/* Point to the next entry */` |
|   22249 | 3734 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11125 | 3735 | `	}` |
|       - | 3736 | `	/* Return the freshly created array */` |
|      31 | 3737 | `	ph7_result_value(pCtx,pArray);` |
|      31 | 3738 | `	return PH7_OK;` |
|      18 | 3739 | `}` |
|       - | 3740 | `/*` |
|       - | 3741 | ` * number array_sum(array $array )` |
|       - | 3742 | ` *  Calculate the sum of values in an array.` |
|       - | 3743 | ` * Parameters` |
|       - | 3744 | ` *  $array: The input array.` |
|       - | 3745 | ` * Return` |
|       - | 3746 | ` *  Returns the sum of values as an integer or float.` |
|       - | 3747 | ` */` |
|      24 | 3748 | `static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 3749 | `{` |
|       - | 3750 | `	ph7_hashmap_node *pEntry;` |
|       - | 3751 | `	ph7_value *pObj;` |
|      26 | 3752 | `	double dSum = 0;` |
|       - | 3753 | `	sxu32 n;` |
|      26 | 3754 | `	pEntry = pMap->pFirst;` |
|      92 | 3755 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|      68 | 3756 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      68 | 3757 | `		if( pObj ){` |
|      68 | 3758 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      30 | 3759 | `				dSum += pObj->rVal;` |
|      54 | 3760 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|      21 | 3761 | `				dSum += (double)pObj->x.iVal;` |
|      30 | 3762 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      16 | 3763 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|       - | 3764 | `					/* php warns and SKIPS a non-numeric string (the array/object/` |
|       - | 3765 | `					 * resource cases below already did; only this one was silent) */` |
|       3 | 3766 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - | 3767 | `						"Addition is not supported on type string");` |
|      14 | 3768 | `				}else if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|      13 | 3769 | `					double dv = 0;` |
|      13 | 3770 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|      13 | 3771 | `					dSum += dv;` |
|       8 | 3772 | `				}` |
|      12 | 3773 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       3 | 3774 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3775 | `					"array_sum(): Addition is not supported on type array");` |
|       4 | 3776 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       - | 3777 | `				/* php names the CLASS here, not the literal word "object" */` |
|     ! 0 | 3778 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|     ! 0 | 3779 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - | 3780 | `					"Addition is not supported on type %s",` |
|     ! 0 | 3781 | `					pInst && pInst->pClass ? pInst->pClass->sName.zString : "object");` |
|       3 | 3782 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 3783 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3784 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 3785 | `			}` |
|       - | 3786 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|      33 | 3787 | `		}` |
|       - | 3788 | `		/* Point to the next entry */` |
|      68 | 3789 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      35 | 3790 | `	}` |
|       - | 3791 | `	/* Return sum */` |
|      26 | 3792 | `	ph7_result_double(pCtx,dSum);` |
|      26 | 3793 | `}` |
|     692 | 3794 | `static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       2 | 3795 | `{` |
|       - | 3796 | `	ph7_hashmap_node *pEntry;` |
|       - | 3797 | `	ph7_value *pObj;` |
|     694 | 3798 | `	sxi64 nSum = 0;` |
|       - | 3799 | `	sxu32 n;` |
|     694 | 3800 | `	pEntry = pMap->pFirst;` |
|    6712 | 3801 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|    6020 | 3802 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|    6020 | 3803 | `		if( pObj ){` |
|    6020 | 3804 | `			if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|    6000 | 3805 | `				nSum += pObj->x.iVal;` |
|    3021 | 3806 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      12 | 3807 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|       - | 3808 | `					/* php warns and SKIPS a non-numeric string */` |
|       5 | 3809 | `					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - | 3810 | `						"Addition is not supported on type string");` |
|      10 | 3811 | `				}else if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|       8 | 3812 | `					sxi64 nv = 0;` |
|       8 | 3813 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|       8 | 3814 | `					nSum += nv;` |
|       5 | 3815 | `				}` |
|      17 | 3816 | `			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|       6 | 3817 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3818 | `					"array_sum(): Addition is not supported on type array");` |
|      10 | 3819 | `			}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       - | 3820 | `				/* php names the CLASS here, not the literal word "object" */` |
|       3 | 3821 | `				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       5 | 3822 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - | 3823 | `					"Addition is not supported on type %s",` |
|       2 | 3824 | `					pInst && pInst->pClass ? pInst->pClass->sName.zString : "object");` |
|       7 | 3825 | `			}else if( pObj->iFlags & MEMOBJ_RES ){` |
|     ! 0 | 3826 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3827 | `					"array_sum(): Addition is not supported on type resource");` |
|     ! 0 | 3828 | `			}` |
|       - | 3829 | `			/* NULL is silently treated as 0 (matches PHP) */` |
|    3009 | 3830 | `		}` |
|       - | 3831 | `		/* Point to the next entry */` |
|    6020 | 3832 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|    3011 | 3833 | `	}` |
|       - | 3834 | `	/* Return sum */` |
|     694 | 3835 | `	ph7_result_int64(pCtx,nSum);` |
|     694 | 3836 | `}` |
|       - | 3837 | `/* number array_sum(array $array )` |
|       - | 3838 | ` * (See block-coment above)` |
|       - | 3839 | ` */` |
|     726 | 3840 | `PH7_PRIVATE int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3841 | `{` |
|       - | 3842 | `	ph7_hashmap_node *pEntry;` |
|       - | 3843 | `	ph7_hashmap *pMap;` |
|       - | 3844 | `	ph7_value *pObj;` |
|     729 | 3845 | `	int useDouble = 0;` |
|       - | 3846 | `	sxu32 n;` |
|       - | 3847 | `	/* PHP requires exactly one argument */` |
|     729 | 3848 | `	if( nArg != 1 ){` |
|     ! 0 | 3849 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3850 | `			"ArgumentCountError",` |
|       - | 3851 | `			"array_sum() expects exactly 1 argument, %d given",` |
|     ! 0 | 3852 | `			nArg` |
|       - | 3853 | `			);` |
|       - | 3854 | `	}` |
|       - | 3855 | `	/* Make sure we are dealing with a valid hashmap */` |
|     729 | 3856 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3857 | `		/* Type mismatch -> TypeError (php's true/false/class-name convention). */` |
|       - | 3858 | `		char zBuf[64];` |
|       8 | 3859 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3860 | `			"TypeError",` |
|       - | 3861 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 3862 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 3863 | `			);` |
|       - | 3864 | `	}` |
|     724 | 3865 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     724 | 3866 | `	if( pMap->nEntry < 1 ){` |
|       - | 3867 | `		/* Nothing to compute,return 0 */` |
|       7 | 3868 | `		ph7_result_int(pCtx,0);` |
|       7 | 3869 | `		return PH7_OK;` |
|       - | 3870 | `	}` |
|       - | 3871 | `	/* Scan all elements: if any value is a float, use floating-point` |
|       - | 3872 | `	 * arithmetic for the entire sum (matches PHP behaviour).` |
|       - | 3873 | `	 */` |
|     718 | 3874 | `	pEntry = pMap->pFirst;` |
|    6744 | 3875 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|    6052 | 3876 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|    6052 | 3877 | `		if( pObj ){` |
|    6052 | 3878 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|      20 | 3879 | `				useDouble = 1;` |
|      20 | 3880 | `				break;` |
|       - | 3881 | `			}` |
|    6034 | 3882 | `			if( pObj->iFlags & MEMOBJ_STRING ){` |
|      18 | 3883 | `				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);` |
|      18 | 3884 | `				sxu32 nLen = SyBlobLength(&pObj->sBlob);` |
|       - | 3885 | `				sxu32 i;` |
|      32 | 3886 | `				for( i = 0 ; i < nLen ; i++ ){` |
|      22 | 3887 | `					if( zStr[i] == '.' \|\| zStr[i] == 'e' \|\| zStr[i] == 'E' ){` |
|       7 | 3888 | `						useDouble = 1;` |
|       7 | 3889 | `						break;` |
|       - | 3890 | `					}` |
|       9 | 3891 | `				}` |
|      18 | 3892 | `				if( useDouble ){` |
|       7 | 3893 | `					break;` |
|       - | 3894 | `				}` |
|       5 | 3895 | `			}` |
|    3013 | 3896 | `		}` |
|    6028 | 3897 | `		pEntry = pEntry->pPrev;` |
|    3015 | 3898 | `	}` |
|     718 | 3899 | `	if( useDouble ){` |
|      26 | 3900 | `		DoubleSum(pCtx,pMap);` |
|      14 | 3901 | `	}else{` |
|     694 | 3902 | `		Int64Sum(pCtx,pMap);` |
|       - | 3903 | `	}` |
|     718 | 3904 | `	return PH7_OK;` |
|     366 | 3905 | `}` |
|       - | 3906 | `/*` |
|       - | 3907 | ` * number array_product(array $array )` |
|       - | 3908 | ` *  Calculate the product of values in an array.` |
|       - | 3909 | ` * Parameters` |
|       - | 3910 | ` *  $array: The input array.` |
|       - | 3911 | ` * Return` |
|       - | 3912 | ` *  Returns the product of values as an integer or float.` |
|       - | 3913 | ` */` |
|       2 | 3914 | `static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 3915 | `{` |
|       - | 3916 | `	ph7_hashmap_node *pEntry;` |
|       - | 3917 | `	ph7_value *pObj;` |
|       - | 3918 | `	double dProd;` |
|       - | 3919 | `	sxu32 n;` |
|       3 | 3920 | `	pEntry = pMap->pFirst;` |
|       3 | 3921 | `	dProd = 1;` |
|       7 | 3922 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       5 | 3923 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|       5 | 3924 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|       5 | 3925 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|       3 | 3926 | `				dProd *= pObj->rVal;` |
|       4 | 3927 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|       3 | 3928 | `				dProd *= (double)pObj->x.iVal;` |
|       1 | 3929 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 3930 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 3931 | `					double dv = 0;` |
|     ! 0 | 3932 | `					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);` |
|     ! 0 | 3933 | `					dProd *= dv;` |
|     ! 0 | 3934 | `				}` |
|     ! 0 | 3935 | `			}` |
|       2 | 3936 | `		}` |
|       - | 3937 | `		/* Point to the next entry */` |
|       5 | 3938 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       3 | 3939 | `	}` |
|       - | 3940 | `	/* Return product */` |
|       3 | 3941 | `	ph7_result_double(pCtx,dProd);` |
|       3 | 3942 | `}` |
|       2 | 3943 | `static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)` |
|       1 | 3944 | `{` |
|       - | 3945 | `	ph7_hashmap_node *pEntry;` |
|       - | 3946 | `	ph7_value *pObj;` |
|       - | 3947 | `	sxi64 nProd;` |
|       - | 3948 | `	sxu32 n;` |
|       3 | 3949 | `	pEntry = pMap->pFirst;` |
|       3 | 3950 | `	nProd = 1;` |
|       9 | 3951 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       7 | 3952 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|       7 | 3953 | `		if( pObj && (pObj->iFlags & (MEMOBJ_NULL\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES)) == 0){` |
|       7 | 3954 | `			if( pObj->iFlags & MEMOBJ_REAL ){` |
|     ! 0 | 3955 | `				nProd *= (sxi64)pObj->rVal;` |
|       7 | 3956 | `			}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|       7 | 3957 | `				nProd *= pObj->x.iVal;` |
|       3 | 3958 | `			}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|     ! 0 | 3959 | `				if( SyBlobLength(&pObj->sBlob) > 0 ){` |
|     ! 0 | 3960 | `					sxi64 nv = 0;` |
|     ! 0 | 3961 | `					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);` |
|     ! 0 | 3962 | `					nProd *= nv;` |
|     ! 0 | 3963 | `				}` |
|     ! 0 | 3964 | `			}` |
|       3 | 3965 | `		}` |
|       - | 3966 | `		/* Point to the next entry */` |
|       7 | 3967 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|       4 | 3968 | `	}` |
|       - | 3969 | `	/* Return product */` |
|       3 | 3970 | `	ph7_result_int64(pCtx,nProd);` |
|       3 | 3971 | `}` |
|       - | 3972 | `/* number array_product(array $array )` |
|       - | 3973 | ` * (See block-block comment above)` |
|       - | 3974 | ` */` |
|      14 | 3975 | `PH7_PRIVATE int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3976 | `{` |
|       - | 3977 | `	ph7_hashmap *pMap;` |
|       - | 3978 | `	ph7_value *pObj;` |
|      15 | 3979 | `	if( nArg < 1 ){` |
|       - | 3980 | `		/* Missing arguments (arity is enforced upstream; defensive). */` |
|     ! 0 | 3981 | `		ph7_result_int(pCtx,1);` |
|     ! 0 | 3982 | `		return PH7_OK;` |
|       - | 3983 | `	}` |
|       - | 3984 | `	/* PHP 8: a non-array $array is a catchable TypeError, not a silent 0. */` |
|      15 | 3985 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3986 | `		char zBuf[64];` |
|      13 | 3987 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3988 | `			"TypeError",` |
|       - | 3989 | `			"array_product(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 3990 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 3991 | `			);` |
|       - | 3992 | `	}` |
|       7 | 3993 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       7 | 3994 | `	if( pMap->nEntry < 1 ){` |
|       - | 3995 | `		/* The product of an empty array is the multiplicative identity 1 (PHP). */` |
|       3 | 3996 | `		ph7_result_int(pCtx,1);` |
|       3 | 3997 | `		return PH7_OK;` |
|       - | 3998 | `	}` |
|       - | 3999 | `	/* If the first element is of type float,then perform floating` |
|       - | 4000 | `	 * point computaion.Otherwise switch to int64 computaion.` |
|       - | 4001 | `	 */` |
|       5 | 4002 | `	pObj = HashmapExtractNodeValue(pMap->pFirst);` |
|       5 | 4003 | `	if( pObj == 0 ){` |
|     ! 0 | 4004 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 4005 | `		return PH7_OK;` |
|       - | 4006 | `	}` |
|       5 | 4007 | `	if( pObj->iFlags & MEMOBJ_REAL ){` |
|       3 | 4008 | `		DoubleProd(pCtx,pMap);` |
|       2 | 4009 | `	}else{` |
|       3 | 4010 | `		Int64Prod(pCtx,pMap);` |
|       - | 4011 | `	}` |
|       5 | 4012 | `	return PH7_OK;` |
|       8 | 4013 | `}` |
|       - | 4014 | `/*` |
|       - | 4015 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 4016 | ` *  Pick one or more random entries out of an array.` |
|       - | 4017 | ` * Parameters` |
|       - | 4018 | ` * $input` |
|       - | 4019 | ` *  The input array.` |
|       - | 4020 | ` * $num_req` |
|       - | 4021 | ` *  Specifies how many entries you want to pick.` |
|       - | 4022 | ` * Return` |
|       - | 4023 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 4024 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 4025 | ` *  NULL is returned on failure.` |
|       - | 4026 | ` */` |
|      36 | 4027 | `PH7_PRIVATE int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4028 | `{` |
|       - | 4029 | `	ph7_hashmap_node *pNode;` |
|       - | 4030 | `	ph7_hashmap *pMap;` |
|      37 | 4031 | `	int nItem = 1;` |
|      37 | 4032 | `	if( nArg < 1 ){` |
|       - | 4033 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|     ! 0 | 4034 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4035 | `		return PH7_OK;` |
|       - | 4036 | `	}` |
|       - | 4037 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|      37 | 4038 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4039 | `		char zBuf[64];` |
|      10 | 4040 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4041 | `			"TypeError",` |
|       - | 4042 | `			"array_rand(): Argument #1 ($array) must be of type array, %s given",` |
|       3 | 4043 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4044 | `			);` |
|       - | 4045 | `	}` |
|       - | 4046 | `	/* php validates $num (and weak-coerces it) BEFORE the empty-array body` |
|       - | 4047 | `	 * check, matching its ZPP-before-body ordering. */` |
|      31 | 4048 | `	if( nArg > 1 ){` |
|      23 | 4049 | `		ph7_value *pNum = apArg[1];` |
|      22 | 4050 | `		if( ph7_value_is_array(pNum) \|\| ph7_value_is_object(pNum)` |
|      23 | 4051 | `			\|\| ph7_value_is_resource(pNum) ){` |
|       - | 4052 | `			char zBuf[64];` |
|     ! 0 | 4053 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4054 | `				"TypeError",` |
|       - | 4055 | `				"array_rand(): Argument #2 ($num) must be of type int, %s given",` |
|     ! 0 | 4056 | `				VmValueGivenName(pNum,zBuf,sizeof(zBuf))` |
|       - | 4057 | `				);` |
|       - | 4058 | `		}` |
|      23 | 4059 | `		if( ph7_value_is_string(pNum) ){` |
|       - | 4060 | `			/* Weak int coercion of a string $num follows php's numeric-string` |
|       - | 4061 | `			 * grammar (whole string, int or float): a non-numeric string` |
|       - | 4062 | `			 * (incl. leading-numeric junk like "2abc" or "0x1A") is a TypeError,` |
|       - | 4063 | `			 * a well-formed float-string ("1e3") coerces like a float value.` |
|       - | 4064 | `			 * Reuses the range() ZPP number parser (§3.9 shared-helper note). */` |
|       - | 4065 | `			int len;` |
|       9 | 4066 | `			const char *zStr = ph7_value_to_string(pNum, &len);` |
|       - | 4067 | `			sxi64 iLong; double dReal;` |
|       9 | 4068 | `			sxu8 iKind = RangeStrToNumber(zStr, (sxu32)len, &iLong, &dReal);` |
|       9 | 4069 | `			if( iKind == RANGE_IN_ERROR ){` |
|       7 | 4070 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4071 | `					"TypeError",` |
|       - | 4072 | `					"array_rand(): Argument #2 ($num) must be of type int, string given"` |
|       - | 4073 | `					);` |
|       - | 4074 | `			}` |
|       - | 4075 | `			/* Clamp into a signed-int band so an absurd magnitude still yields` |
|       - | 4076 | `			 * the out-of-range ValueError below without an out-of-int cast. */` |
|       3 | 4077 | `			if( iKind == RANGE_IN_DOUBLE ){` |
|       3 | 4078 | `				iLong = dReal <= 0.0 ? 0 : (dReal >= 2147483647.0 ? 2147483647 : (sxi64)dReal);` |
|       1 | 4079 | `			}` |
|       3 | 4080 | `			if( iLong > 2147483647 ){ iLong = 2147483647; }` |
|       3 | 4081 | `			else if( iLong < -2147483647 ){ iLong = -2147483647; }` |
|       3 | 4082 | `			nItem = (int)iLong;` |
|       2 | 4083 | `		}else{` |
|      15 | 4084 | `			nItem = ph7_value_to_int(pNum);` |
|       - | 4085 | `		}` |
|       8 | 4086 | `	}` |
|       - | 4087 | `	/* Point to the internal representation of the input hashmap */` |
|      25 | 4088 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4089 | `	/* php 8: an empty array is a ValueError, not a NULL return */` |
|      25 | 4090 | `	if( pMap->nEntry < 1 ){` |
|       5 | 4091 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4092 | `			"ValueError",` |
|       - | 4093 | `			"array_rand(): Argument #1 ($array) must not be empty"` |
|       - | 4094 | `			);` |
|       - | 4095 | `	}` |
|       - | 4096 | `	/* php 8: $num outside [1, count] is a ValueError, not a clamp/wrong value */` |
|      21 | 4097 | `	if( nItem < 1 \|\| nItem > (int)pMap->nEntry ){` |
|       9 | 4098 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4099 | `			"ValueError",` |
|       - | 4100 | `			"array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)"` |
|       - | 4101 | `			);` |
|       - | 4102 | `	}` |
|      13 | 4103 | `	if( nItem < 2 ){` |
|       - | 4104 | `		sxu32 nEntry;` |
|       - | 4105 | `		/* Pick a random slot through the MT19937 generator so array_rand()` |
|       - | 4106 | `		 * responds to srand()/mt_srand() (reproducible), like php. The exact` |
|       - | 4107 | `		 * index php lands on differs (php samples its internal hashtable` |
|       - | 4108 | `		 * buckets), so this is deterministic-under-seed but not value-parity. */` |
|       9 | 4109 | `		nEntry = (sxu32)PH7_VmMtRandRange(pMap->pVm,0,(sxi64)pMap->nEntry - 1);` |
|       - | 4110 | `		/* Extract the desired entry.` |
|       - | 4111 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 4112 | `		 */` |
|       9 | 4113 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       6 | 4114 | `			pNode = pMap->pLast;` |
|       6 | 4115 | `			nEntry = pMap->nEntry - nEntry;` |
|       6 | 4116 | `			if( nEntry > 1 ){` |
|     ! 0 | 4117 | `				for(;;){` |
|     ! 0 | 4118 | `					if( nEntry == 0 ){` |
|     ! 0 | 4119 | `						break;` |
|       - | 4120 | `					}` |
|       - | 4121 | `					/* Point to the previous entry */` |
|     ! 0 | 4122 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 4123 | `					nEntry--;` |
|     ! 0 | 4124 | `				}` |
|     ! 0 | 4125 | `			}` |
|       4 | 4126 | `		}else{` |
|       4 | 4127 | `			pNode = pMap->pFirst;` |
|       1 | 4128 | `			for(;;){` |
|       4 | 4129 | `				if( nEntry == 0 ){` |
|       4 | 4130 | `					break;` |
|       - | 4131 | `				}` |
|       - | 4132 | `				/* Point to the next entry */` |
|       1 | 4133 | `				pNode = pNode->pPrev; /* Reverse link */` |
|       1 | 4134 | `				nEntry--;` |
|       1 | 4135 | `			}` |
|       - | 4136 | `		}` |
|       9 | 4137 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 4138 | `			/* Int key */` |
|       7 | 4139 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       4 | 4140 | `		}else{` |
|       - | 4141 | `			/* Blob key */` |
|       3 | 4142 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 4143 | `		}` |
|       5 | 4144 | `	}else{` |
|       - | 4145 | `		ph7_value sKey,*pArray;` |
|       - | 4146 | `		ph7_hashmap *pDest;` |
|       - | 4147 | `		/* Create a new array */` |
|       5 | 4148 | `		pArray = ph7_context_new_array(pCtx);` |
|       5 | 4149 | `		if( pArray == 0 ){` |
|     ! 0 | 4150 | `			ph7_result_null(pCtx);` |
|     ! 0 | 4151 | `			return PH7_OK;` |
|       - | 4152 | `		}` |
|       - | 4153 | `		/* Point to the internal representation of the hashmap */` |
|       5 | 4154 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       5 | 4155 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 4156 | `		/* Copy the first n items */` |
|       5 | 4157 | `		pNode = pMap->pFirst;` |
|       5 | 4158 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 4159 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 4160 | `		}` |
|      15 | 4161 | `		while( nItem > 0){` |
|      11 | 4162 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|      11 | 4163 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|      11 | 4164 | `			PH7_MemObjRelease(&sKey);` |
|       - | 4165 | `			/* Point to the next entry */` |
|      11 | 4166 | `			pNode = pNode->pPrev; /* Reverse link */` |
|      11 | 4167 | `			nItem--;` |
|       1 | 4168 | `		}` |
|       - | 4169 | `		/* Shuffle the array */` |
|       5 | 4170 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 4171 | `		/* Rehash node */` |
|       5 | 4172 | `		HashmapSortRehash(pDest);` |
|       - | 4173 | `		/* Return the random array */` |
|       5 | 4174 | `		ph7_result_value(pCtx,pArray);` |
|       - | 4175 | `	}` |
|      13 | 4176 | `	return PH7_OK;` |
|      19 | 4177 | `}` |
|       - | 4178 | `/*` |
|       - | 4179 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 4180 | ` *  Split an array into chunks.` |
|       - | 4181 | ` * Parameters` |
|       - | 4182 | ` * $input` |
|       - | 4183 | ` *   The array to work on` |
|       - | 4184 | ` * $size` |
|       - | 4185 | ` *   The size of each chunk` |
|       - | 4186 | ` * $preserve_keys` |
|       - | 4187 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 4188 | ` *   the chunk numerically.` |
|       - | 4189 | ` * Return` |
|       - | 4190 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 4191 | ` *  zero, with each dimension containing size elements.` |
|       - | 4192 | ` */` |
|      40 | 4193 | `PH7_PRIVATE int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4194 | `{` |
|       - | 4195 | `	ph7_value *pArray,*pChunk;` |
|       - | 4196 | `	ph7_hashmap_node *pEntry;` |
|       - | 4197 | `	ph7_hashmap *pMap;` |
|       - | 4198 | `	int bPreserve;` |
|       - | 4199 | `	sxu32 nChunk;` |
|       - | 4200 | `	sxu32 nSize;` |
|       - | 4201 | `	sxu32 n;` |
|       - | 4202 | `	/* Argument count and types follow PHP semantics. */` |
|      45 | 4203 | `	if( nArg < 2 ){` |
|       - | 4204 | `		/* fewer than required arguments -> ArgumentCountError */` |
|     ! 0 | 4205 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4206 | `			"ArgumentCountError",` |
|       - | 4207 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|     ! 0 | 4208 | `			nArg` |
|       - | 4209 | `			);` |
|       - | 4210 | `	}` |
|      45 | 4211 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4212 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4213 | `			"TypeError",` |
|       - | 4214 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4215 | `			ph7_type_name(apArg[0])` |
|       - | 4216 | `			);` |
|       - | 4217 | `	}` |
|       - | 4218 | `	/* Create a new array */` |
|      42 | 4219 | `	pArray = ph7_context_new_array(pCtx);` |
|      42 | 4220 | `	if( pArray == 0 ){` |
|     ! 0 | 4221 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4222 | `		return PH7_OK;` |
|       - | 4223 | `	}` |
|       - | 4224 | `	/* Point to the internal representation of the input hashmap */` |
|      42 | 4225 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4226 | `	/* Extract and validate the chunk size argument. */` |
|       - | 4227 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      57 | 4228 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      80 | 4229 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      38 | 4230 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 4231 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4232 | `			"TypeError",` |
|       - | 4233 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 4234 | `			ph7_type_name(apArg[1])` |
|       - | 4235 | `			);` |
|       - | 4236 | `	}` |
|       - | 4237 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 4238 | `	 * strings are permitted; however those representing floats lose` |
|       - | 4239 | `	 * precision and PHP emits a deprecation warning. */` |
|      42 | 4240 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 4241 | `		int len;` |
|       6 | 4242 | `		sxu8 bReal = FALSE;` |
|       6 | 4243 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       6 | 4244 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       4 | 4245 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4246 | `				"TypeError",` |
|       - | 4247 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 4248 | `				);` |
|       - | 4249 | `		}` |
|       3 | 4250 | `		if( bReal ){` |
|       - | 4251 | `			/* php only DEPRECATES the lossy float-string -> int narrowing; §10 rejects` |
|       - | 4252 | `			 * it loudly, and the throw ABORTS the call (php would chunk the array).` |
|       - | 4253 | `			 * Twin-pinned by array_lossy_int_arg_abort{,_zend}.phpt. */` |
|       3 | 4254 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 4255 | `				"Implicit conversion from float-string to int loses precision");` |
|       - | 4256 | `		}` |
|     ! 0 | 4257 | `	}` |
|       - | 4258 | `	/* A FRACTIONAL float is the same §10 lossy narrowing and aborts the call; a whole` |
|       - | 4259 | `	 * float falls through to the ph7_value_to_int conversion below. */` |
|      37 | 4260 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       6 | 4261 | `		double d = ph7_value_to_double(apArg[1]);` |
|       6 | 4262 | `		sxi64 i = (sxi64)d;` |
|       6 | 4263 | `		if( d != (double)i ){` |
|       6 | 4264 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 4265 | `				"Implicit conversion from float to int loses precision");` |
|       - | 4266 | `		}` |
|     ! 0 | 4267 | `	}` |
|       - | 4268 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 4269 | `	 * eliminated, this will not produce a warning. */` |
|       - | 4270 | `	{` |
|      33 | 4271 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      33 | 4272 | `		if( nSizeSigned < 1 ){` |
|       - | 4273 | `			/* size <= 0 -> ValueError */` |
|       6 | 4274 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4275 | `				"ValueError",` |
|       - | 4276 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 4277 | `				);` |
|       - | 4278 | `		}` |
|      27 | 4279 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 4280 | `	}` |
|      27 | 4281 | `	if( nSize >= pMap->nEntry ){` |
|       - | 4282 | `		/* Return the whole array */` |
|       3 | 4283 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 4284 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 4285 | `		return PH7_OK;` |
|       - | 4286 | `	}` |
|      25 | 4287 | `	bPreserve = 0;` |
|      25 | 4288 | `	if( nArg > 2 ){` |
|       - | 4289 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 4290 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 4291 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 4292 | `		 * normally, matching PHP behaviour. */` |
|      30 | 4293 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      31 | 4294 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 4295 | `			ph7_value_is_resource(apArg[2]) ){` |
|     ! 0 | 4296 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4297 | `				"TypeError",` |
|       - | 4298 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|     ! 0 | 4299 | `				ph7_type_name(apArg[2])` |
|       - | 4300 | `				);` |
|       - | 4301 | `		}` |
|      21 | 4302 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 4303 | `	}` |
|       - | 4304 | `	/* Start processing */` |
|      25 | 4305 | `	pEntry = pMap->pFirst;` |
|      25 | 4306 | `	nChunk = 0;` |
|      25 | 4307 | `	pChunk = 0;` |
|      25 | 4308 | `	n = pMap->nEntry;` |
|      51 | 4309 | `	for( ;; ){` |
|     103 | 4310 | `		if( n < 1 ){` |
|       - | 4311 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 4312 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 4313 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 4314 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 4315 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 4316 | `			 * exists. */` |
|      25 | 4317 | `			if( pChunk ){` |
|      25 | 4318 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      12 | 4319 | `			}` |
|      25 | 4320 | `			break;` |
|       - | 4321 | `		}` |
|      79 | 4322 | `		if( nChunk < 1 ){` |
|      67 | 4323 | `			if( pChunk ){` |
|       - | 4324 | `				/* Put the first chunk */` |
|      43 | 4325 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      21 | 4326 | `			}` |
|       - | 4327 | `			/* Create a new dimension */` |
|      67 | 4328 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 4329 | `												   * will be automatically released as soon we return` |
|       - | 4330 | `												   * from this function */` |
|      67 | 4331 | `			if( pChunk == 0 ){` |
|     ! 0 | 4332 | `				break;` |
|       - | 4333 | `			}` |
|      67 | 4334 | `			nChunk = nSize;` |
|      33 | 4335 | `		}` |
|       - | 4336 | `		/* Insert the entry */` |
|      79 | 4337 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 4338 | `		/* Point to the next entry */` |
|      79 | 4339 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      79 | 4340 | `		nChunk--;` |
|      79 | 4341 | `		n--;` |
|       1 | 4342 | `	}` |
|       - | 4343 | `	/* Return the multidimensional array */` |
|      25 | 4344 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 4345 | `	return PH7_OK;` |
|      25 | 4346 | `}` |
|       - | 4347 | `/*` |
|       - | 4348 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 4349 | ` *  Pad array to the specified length with a value.` |
|       - | 4350 | ` * $input` |
|       - | 4351 | ` *   Initial array of values to pad.` |
|       - | 4352 | ` * $pad_size` |
|       - | 4353 | ` *   New size of the array.` |
|       - | 4354 | ` * $pad_value` |
|       - | 4355 | ` *   Value to pad if input is less than pad_size.` |
|       - | 4356 | ` */` |
|       - | 4357 | `/*` |
|       - | 4358 | ` * Shared "requested array size too large" guard (band A #8). php throws a` |
|       - | 4359 | ` * catchable ValueError when a builtin's caller-controlled target length` |
|       - | 4360 | ` * exceeds its hashtable capacity HT_MAX_SIZE (2^30 elements; probed against` |
|       - | 4361 | ` * php 8.5.7 — the boundary sits exactly between 1073741824 and 1073741825,` |
|       - | 4362 | ` * independent of the input array's size and symmetric for negative lengths).` |
|       - | 4363 | ` * Without this, a call like array_pad([1,2], 2000000000, 0) sits in the fill` |
|       - | 4364 | ` * loop for minutes and then OOMs. nRequested is the ABSOLUTE requested` |
|       - | 4365 | ` * length; pass a still-negative value (e.g. the unnegatable INT64_MIN,` |
|       - | 4366 | ` * mirroring php's ZEND_ABS overflow) to fail the guard unconditionally.` |
|       - | 4367 | ` * Returns SXRET_OK when the size is acceptable, else the throw status to` |
|       - | 4368 | ` * propagate. The cap constant is shared with range()'s guards` |
|       - | 4369 | ` * (PH7_RANGE_HT_MAX_SIZE above).` |
|       - | 4370 | ` */` |
|      50 | 4371 | `static sxi32 HashmapGuardArraySize(` |
|       - | 4372 | `	ph7_context *pCtx,` |
|       - | 4373 | `	const char *zFunc,     /* Function name for the message */` |
|       - | 4374 | `	int iArg,              /* 1-based argument position */` |
|       - | 4375 | `	const char *zParam     /* "$length"-style parameter name */,` |
|       - | 4376 | `	sxi64 nRequested       /* Absolute requested element count */` |
|       - | 4377 | `	)` |
|       1 | 4378 | `{` |
|      51 | 4379 | `	if( nRequested < 0 \|\| nRequested > PH7_RANGE_HT_MAX_SIZE ){` |
|      22 | 4380 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4381 | `			"ValueError",` |
|       - | 4382 | `			"%s(): Argument #%d (%s) must not exceed the maximum allowed array size",` |
|       7 | 4383 | `			zFunc,iArg,zParam` |
|       - | 4384 | `			);` |
|       - | 4385 | `	}` |
|      37 | 4386 | `	return SXRET_OK;` |
|      26 | 4387 | `}` |
|      60 | 4388 | `PH7_PRIVATE int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4389 | `{` |
|       - | 4390 | `	ph7_hashmap *pMap;` |
|       - | 4391 | `	ph7_value *pArray;` |
|       - | 4392 | `	sxi64 iLen,iAbs;` |
|       - | 4393 | `	int nEntry;` |
|       - | 4394 | `	sxi32 rc;` |
|      62 | 4395 | `	if( nArg != 3 ){` |
|     ! 0 | 4396 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4397 | `			"ArgumentCountError",` |
|       - | 4398 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|     ! 0 | 4399 | `			nArg` |
|       - | 4400 | `			);` |
|       - | 4401 | `	}` |
|      62 | 4402 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4403 | `		char zBuf[64];` |
|      11 | 4404 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4405 | `			"TypeError",` |
|       - | 4406 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|       3 | 4407 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4408 | `			);` |
|       - | 4409 | `	}` |
|       - | 4410 | `	/* php 8: $length must be int-coercible. An array/object/resource or a` |
|       - | 4411 | `	 * non-numeric string throws a TypeError instead of silently padding to 0;` |
|       - | 4412 | `	 * a numeric string is weak-coerced via php's is_numeric_string grammar` |
|       - | 4413 | `	 * (reusing the shared RangeStrToNumber, like array_rand's $num). */` |
|      54 | 4414 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|      55 | 4415 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|       - | 4416 | `		char zBuf[64];` |
|     ! 0 | 4417 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4418 | `			"TypeError",` |
|       - | 4419 | `			"array_pad(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 4420 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|       - | 4421 | `			);` |
|       - | 4422 | `	}` |
|      55 | 4423 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 4424 | `		int nStr;` |
|      11 | 4425 | `		const char *zStr = ph7_value_to_string(apArg[1],&nStr);` |
|       - | 4426 | `		sxi64 iLong; double dReal;` |
|      11 | 4427 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|      11 | 4428 | `		if( iKind == RANGE_IN_ERROR ){` |
|       5 | 4429 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4430 | `				"TypeError",` |
|       - | 4431 | `				"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|       - | 4432 | `				);` |
|       - | 4433 | `		}` |
|       7 | 4434 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|       - | 4435 | `			/* php ZPP: a float-string outside the int64 range (or NaN) fails` |
|       - | 4436 | `			 * outright — also keeps the (sxi64) cast below UB-free. */` |
|       3 | 4437 | `			if( dReal != dReal \|\| dReal >= 9223372036854775808.0 \|\| dReal < -9223372036854775808.0 ){` |
|     ! 0 | 4438 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4439 | `					"TypeError",` |
|       - | 4440 | `					"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|       - | 4441 | `					);` |
|       - | 4442 | `			}` |
|       3 | 4443 | `			iLen = (sxi64)dReal;` |
|       3 | 4444 | `			if( (double)iLen != dReal ){` |
|     ! 0 | 4445 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 4446 | `					"array_pad(): Argument #2 ($length) must be of type int, string given");` |
|       - | 4447 | `			}` |
|       2 | 4448 | `		}else{` |
|       5 | 4449 | `			iLen = iLong;` |
|       - | 4450 | `		}` |
|       4 | 4451 | `	}else{` |
|      45 | 4452 | `		iLen = ph7_value_to_int64(apArg[1]);` |
|       - | 4453 | `	}` |
|       - | 4454 | `	/* Point to the internal representation of the input hashmap */` |
|      51 | 4455 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4456 | `	/* php caps abs($length) at HT_MAX_SIZE either direction (INT64_MIN stays` |
|       - | 4457 | `	 * negative through the ABS, failing the guard like php's own ZEND_ABS` |
|       - | 4458 | `	 * overflow). */` |
|      51 | 4459 | `	iAbs = iLen;` |
|      51 | 4460 | `	if( iAbs < 0 && iAbs != (sxi64)-9223372036854775807LL - 1 ){` |
|      15 | 4461 | `		iAbs = -iAbs;` |
|       7 | 4462 | `	}` |
|      51 | 4463 | `	rc = HashmapGuardArraySize(pCtx,"array_pad",2,"$length",iAbs);` |
|      51 | 4464 | `	if( rc != SXRET_OK ){` |
|      15 | 4465 | `		return rc;` |
|       - | 4466 | `	}` |
|      37 | 4467 | `	nEntry = (int)iLen;` |
|       - | 4468 | `	/* Create a new array */` |
|      37 | 4469 | `	pArray = ph7_context_new_array(pCtx);` |
|      37 | 4470 | `	if( pArray == 0 ){` |
|     ! 0 | 4471 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 4472 | `	}` |
|      37 | 4473 | `	if( nEntry < 0 ){` |
|      11 | 4474 | `		nEntry = -nEntry;` |
|      11 | 4475 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 4476 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 4477 | `			/* Insert given items first */` |
|      25 | 4478 | `			while( nEntry > 0 ){` |
|      19 | 4479 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 4480 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 4481 | `				}` |
|      19 | 4482 | `				nEntry--;` |
|       1 | 4483 | `			}` |
|       - | 4484 | `			/* Merge the two arrays */` |
|       7 | 4485 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       4 | 4486 | `		}else{` |
|       5 | 4487 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 4488 | `		}` |
|      32 | 4489 | `	}else if( nEntry > 0 ){` |
|      25 | 4490 | `		if( nEntry > (int)pMap->nEntry ){` |
|      19 | 4491 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 4492 | `			/* Merge the two arrays first */` |
|      19 | 4493 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 4494 | `			/* Insert given items */` |
|     275 | 4495 | `			while( nEntry > 0 ){` |
|     257 | 4496 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 4497 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 4498 | `				}` |
|     257 | 4499 | `				nEntry--;` |
|       1 | 4500 | `			}` |
|      10 | 4501 | `		}else{` |
|       7 | 4502 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 4503 | `		}` |
|      13 | 4504 | `	}else{` |
|       - | 4505 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 4506 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 4507 | `	}` |
|       - | 4508 | `	/* Return the new array */` |
|      37 | 4509 | `	ph7_result_value(pCtx,pArray);` |
|      37 | 4510 | `	return PH7_OK;` |
|      32 | 4511 | `}` |
|       - | 4512 | `/*` |
|       - | 4513 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 4514 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 4515 | ` * Parameters` |
|       - | 4516 | ` * $array` |
|       - | 4517 | ` *   The array in which elements are replaced.` |
|       - | 4518 | ` * $array1` |
|       - | 4519 | ` *   The array from which elements will be extracted.` |
|       - | 4520 | ` * ....` |
|       - | 4521 | ` *  More arrays from which elements will be extracted.` |
|       - | 4522 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 4523 | ` * Return` |
|       - | 4524 | ` *  Returns an array.` |
|       - | 4525 | ` *  Throws ArgumentCountError if no arguments are given.` |
|       - | 4526 | ` *  Throws TypeError if any argument is not an array.` |
|       - | 4527 | ` */` |
|      20 | 4528 | `PH7_PRIVATE int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 4529 | `{` |
|       - | 4530 | `	ph7_hashmap *pMap;` |
|       - | 4531 | `	ph7_value *pArray;` |
|       - | 4532 | `	int i;` |
|      23 | 4533 | `	if( nArg < 1 ){` |
|     ! 0 | 4534 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4535 | `			"ArgumentCountError",` |
|       - | 4536 | `			"array_replace() expects at least 1 argument, 0 given"` |
|       - | 4537 | `			);` |
|       - | 4538 | `	}` |
|      23 | 4539 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4540 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4541 | `			"TypeError",` |
|       - | 4542 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4543 | `			ph7_type_name(apArg[0])` |
|       - | 4544 | `			);` |
|       - | 4545 | `	}` |
|       - | 4546 | `	/* Create a new array */` |
|      20 | 4547 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 4548 | `	if( pArray == 0 ){` |
|     ! 0 | 4549 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4550 | `		return PH7_OK;` |
|       - | 4551 | `	}` |
|       - | 4552 | `	/* Overwrite from the first array */` |
|      20 | 4553 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 4554 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 4555 | `	/* Perform the requested operation for remaining arrays */` |
|      36 | 4556 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4557 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 4558 | `			/* Type mismatch -> TypeError */` |
|       4 | 4559 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4560 | `				"TypeError",` |
|       - | 4561 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|       1 | 4562 | `				i + 1,` |
|       2 | 4563 | `				ph7_type_name(apArg[i])` |
|       - | 4564 | `				);` |
|       - | 4565 | `		}` |
|       - | 4566 | `		/* Point to the internal representation of the input hashmap */` |
|      17 | 4567 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      17 | 4568 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       9 | 4569 | `	}` |
|       - | 4570 | `	/* Return the new array */` |
|      17 | 4571 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 4572 | `	return PH7_OK;` |
|      13 | 4573 | `}` |
|       - | 4574 | `/*` |
|       - | 4575 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 4576 | ` *  Filters elements of an array using a callback function.` |
|       - | 4577 | ` * Parameters` |
|       - | 4578 | ` *  $input` |
|       - | 4579 | ` *    The array to iterate over` |
|       - | 4580 | ` * $callback` |
|       - | 4581 | ` *    The callback function to use` |
|       - | 4582 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 4583 | ` *    will be removed.` |
|       - | 4584 | ` * Return` |
|       - | 4585 | ` *  The filtered array.` |
|       - | 4586 | ` */` |
|      30 | 4587 | `PH7_PRIVATE int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4588 | `{` |
|       - | 4589 | `	ph7_hashmap_node *pEntry;` |
|       - | 4590 | `	ph7_hashmap *pMap;` |
|       - | 4591 | `	ph7_value *pArray;` |
|       - | 4592 | `	ph7_value sResult;   /* Callback result */` |
|       - | 4593 | `	ph7_value *pValue;` |
|       - | 4594 | `	sxi32 rc;` |
|       - | 4595 | `	int keep;` |
|       - | 4596 | `	sxu32 n;` |
|      32 | 4597 | `	if( nArg < 1 ){` |
|       - | 4598 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|     ! 0 | 4599 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4600 | `		return PH7_OK;` |
|       - | 4601 | `	}` |
|       - | 4602 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|      32 | 4603 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4604 | `		char zBuf[64];` |
|      16 | 4605 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4606 | `			"TypeError",` |
|       - | 4607 | `			"array_filter(): Argument #1 ($array) must be of type array, %s given",` |
|       5 | 4608 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4609 | `			);` |
|       - | 4610 | `	}` |
|       - | 4611 | `	/* Create a new array */` |
|      22 | 4612 | `	pArray = ph7_context_new_array(pCtx);` |
|      22 | 4613 | `	if( pArray == 0 ){` |
|     ! 0 | 4614 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4615 | `		return PH7_OK;` |
|       - | 4616 | `	}` |
|       - | 4617 | `	/* Point to the internal representation of the input hashmap */` |
|      22 | 4618 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      22 | 4619 | `	pEntry = pMap->pFirst;` |
|      22 | 4620 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      22 | 4621 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 4622 | `	/* Perform the requested operation */` |
|      86 | 4623 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 4624 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      70 | 4625 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      70 | 4626 | `		if( pValue == 0 ){` |
|       - | 4627 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 4628 | `			keep = FALSE;` |
|      70 | 4629 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 4630 | `			/* Callback was supplied (not NULL).  PHP 8 throws a` |
|       - | 4631 | `				* TypeError when the value is not callable or null; prior PH7` |
|       - | 4632 | `				* silently dropped the element.  Emit similar message. */` |
|      42 | 4633 | `			if( !ph7_value_is_callable(apArg[1]) ){` |
|       3 | 4634 | `				if( ph7_value_is_string(apArg[1]) ){` |
|       - | 4635 | `					int len;` |
|       3 | 4636 | `					const char *zName = ph7_value_to_string(apArg[1], &len);` |
|       4 | 4637 | `					return PH7_VmThrowException(pCtx,` |
|       - | 4638 | `						"TypeError",` |
|       - | 4639 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",` |
|       1 | 4640 | `						zName` |
|       - | 4641 | `						);` |
|     ! 0 | 4642 | `				}else{` |
|     ! 0 | 4643 | `					return PH7_VmThrowException(pCtx,` |
|       - | 4644 | `						"TypeError",` |
|       - | 4645 | `						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",` |
|     ! 0 | 4646 | `						ph7_type_name(apArg[1])` |
|       - | 4647 | `						);` |
|       - | 4648 | `				}` |
|       - | 4649 | `			}` |
|      39 | 4650 | `			keep = FALSE;` |
|      39 | 4651 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      39 | 4652 | `			if( rc == PH7_EXCEPTION ){` |
|       - | 4653 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 4654 | `				PH7_MemObjRelease(&sResult);` |
|       3 | 4655 | `				return PH7_EXCEPTION;` |
|       - | 4656 | `			}` |
|      37 | 4657 | `			if( rc == SXRET_OK ){` |
|       - | 4658 | `				/* Perform a boolean cast */` |
|      37 | 4659 | `				keep = ph7_value_to_bool(&sResult);` |
|      18 | 4660 | `			}` |
|      37 | 4661 | `			PH7_MemObjRelease(&sResult);` |
|      19 | 4662 | `		}else{` |
|       - | 4663 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 4664 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 4665 | `			 * the case where the callback argument is missing entirely.` |
|       - | 4666 | `			 */` |
|      29 | 4667 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 4668 | `		}` |
|      65 | 4669 | `		if( keep ){` |
|       - | 4670 | `			/* Perform the insertion,now the callback returned true */` |
|      25 | 4671 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4672 | `		}` |
|       - | 4673 | `		/* Point to the next entry */` |
|      65 | 4674 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 4675 | `	}` |
|      17 | 4676 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 4677 | `	return PH7_OK;` |
|      17 | 4678 | `}` |
|       - | 4679 | `/*` |
|       - | 4680 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|       - | 4681 | ` *  Applies the callback to the elements of the given arrays.` |
|       - | 4682 | ` * Parameters` |
|       - | 4683 | ` *  $callback` |
|       - | 4684 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|       - | 4685 | ` *   array and a NULL callback this is the identity function (the array is` |
|       - | 4686 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|       - | 4687 | ` *   are zipped together.` |
|       - | 4688 | ` *  $array` |
|       - | 4689 | ` *   The first array to run through the callback function.` |
|       - | 4690 | ` *  $arrays` |
|       - | 4691 | ` *   Zero or more additional arrays to process in parallel.` |
|       - | 4692 | ` * Return` |
|       - | 4693 | ` *  Returns an array containing the results of applying the callback function.` |
|       - | 4694 | ` *  With a single array the keys are preserved; with several arrays the result` |
|       - | 4695 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|       - | 4696 | ` *  padding shorter arrays with NULL.` |
|       - | 4697 | ` */` |
|      98 | 4698 | `PH7_PRIVATE int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4699 | `{` |
|       - | 4700 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 4701 | `	ph7_hashmap_node *pEntry;` |
|       - | 4702 | `	ph7_hashmap *pMap;` |
|       - | 4703 | `	ph7_vm *pVm;` |
|       - | 4704 | `	int bNullCallback;` |
|       - | 4705 | `	sxi32 rc;` |
|       - | 4706 | `	int i;` |
|       - | 4707 | `	sxu32 n;` |
|     103 | 4708 | `	if( nArg < 2 ){` |
|     ! 0 | 4709 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4710 | `			"ArgumentCountError",` |
|       - | 4711 | `			"array_map() expects at least 2 arguments, %d given",` |
|     ! 0 | 4712 | `			nArg` |
|       - | 4713 | `			);` |
|       - | 4714 | `	}` |
|     103 | 4715 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|     103 | 4716 | `	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){` |
|       8 | 4717 | `		if( ph7_value_is_string(apArg[0]) ){` |
|       6 | 4718 | `			const char *zFunc = ph7_value_to_string(apArg[0],0);` |
|       8 | 4719 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4720 | `				"TypeError",` |
|       - | 4721 | `				"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 4722 | `				"function \"%s\" not found or invalid function name",` |
|       2 | 4723 | `				zFunc` |
|       - | 4724 | `				);` |
|       - | 4725 | `		}` |
|       3 | 4726 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4727 | `			"TypeError",` |
|       - | 4728 | `			"array_map(): Argument #1 ($callback) must be a valid callback or null, "` |
|       - | 4729 | `			"no array or string given"` |
|       - | 4730 | `			);` |
|       - | 4731 | `	}` |
|       - | 4732 | `	/* Every remaining argument must be an array */` |
|     199 | 4733 | `	for( i = 1 ; i < nArg ; i++ ){` |
|     109 | 4734 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       3 | 4735 | `			if( i == 1 ){` |
|       4 | 4736 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4737 | `					"TypeError",` |
|       - | 4738 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|       2 | 4739 | `					ph7_type_name(apArg[1])` |
|       - | 4740 | `					);` |
|       - | 4741 | `			}` |
|     ! 0 | 4742 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4743 | `				"TypeError",` |
|       - | 4744 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|     ! 0 | 4745 | `				i+1,ph7_type_name(apArg[i])` |
|       - | 4746 | `				);` |
|       - | 4747 | `		}` |
|      55 | 4748 | `	}` |
|      94 | 4749 | `	pVm = pCtx->pVm;` |
|       - | 4750 | `	/* Create a new array */` |
|      94 | 4751 | `	pArray = ph7_context_new_array(pCtx);` |
|      94 | 4752 | `	if( pArray == 0 ){` |
|     ! 0 | 4753 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4754 | `		return PH7_OK;` |
|       - | 4755 | `	}` |
|      94 | 4756 | `	PH7_MemObjInit(pVm,&sResult);` |
|      94 | 4757 | `	PH7_MemObjInit(pVm,&sKey);` |
|      94 | 4758 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      94 | 4759 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|      94 | 4760 | `	if( nArg == 2 ){` |
|       - | 4761 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|      84 | 4762 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      84 | 4763 | `		pEntry = pMap->pFirst;` |
|     788 | 4764 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 4765 | `			/* Extract the node value */` |
|     712 | 4766 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|     712 | 4767 | `			if( pValue ){` |
|       - | 4768 | `				/* Extract the node key */` |
|     712 | 4769 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|     712 | 4770 | `				if( bNullCallback ){` |
|       - | 4771 | `					/* NULL callback: identity function, keep original value */` |
|      11 | 4772 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|       6 | 4773 | `				}else{` |
|       - | 4774 | `					/* Invoke the supplied callback */` |
|     702 | 4775 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|     702 | 4776 | `					if( rc == PH7_EXCEPTION ){` |
|       - | 4777 | `						/* Callback raised: abort and let the foreign-function` |
|       - | 4778 | `						 * dispatcher unwind through the nearest try/catch. */` |
|       5 | 4779 | `						PH7_MemObjRelease(&sKey);` |
|       5 | 4780 | `						PH7_MemObjRelease(&sResult);` |
|       5 | 4781 | `						return PH7_EXCEPTION;` |
|       - | 4782 | `					}` |
|       - | 4783 | `					/* Insert the callback return value */` |
|     698 | 4784 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 4785 | `				}` |
|     708 | 4786 | `				PH7_MemObjRelease(&sKey);` |
|     708 | 4787 | `				PH7_MemObjRelease(&sResult);` |
|     352 | 4788 | `			}` |
|       - | 4789 | `			/* Point to the next entry */` |
|     708 | 4790 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|     356 | 4791 | `		}` |
|      42 | 4792 | `	}else{` |
|       - | 4793 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|       - | 4794 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|      11 | 4795 | `		int nArrays = nArg - 1;` |
|       - | 4796 | `		ph7_hashmap_node **apCur;` |
|       - | 4797 | `		ph7_value **apCallArg;` |
|       - | 4798 | `		ph7_value sNull;` |
|      11 | 4799 | `		sxu32 nMax = 0;` |
|      11 | 4800 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|      11 | 4801 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|      11 | 4802 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|     ! 0 | 4803 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|     ! 0 | 4804 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|     ! 0 | 4805 | `			PH7_MemObjRelease(&sKey);` |
|     ! 0 | 4806 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 4807 | `			ph7_result_value(pCtx,pArray);` |
|     ! 0 | 4808 | `			return PH7_OK;` |
|       - | 4809 | `		}` |
|      11 | 4810 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|      11 | 4811 | `		sNull.nIdx = SXU32_HIGH;` |
|      33 | 4812 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|      23 | 4813 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|      23 | 4814 | `			apCur[i] = pMap->pFirst;` |
|      23 | 4815 | `			if( pMap->nEntry > nMax ){` |
|      13 | 4816 | `				nMax = pMap->nEntry;` |
|       6 | 4817 | `			}` |
|      12 | 4818 | `		}` |
|      35 | 4819 | `		for( n = 0 ; n < nMax ; n++ ){` |
|      25 | 4820 | `			ph7_value *pZip = 0;` |
|      25 | 4821 | `			if( bNullCallback ){` |
|       - | 4822 | `				/* zip: each result element is an array of the i-th values */` |
|       5 | 4823 | `				pZip = ph7_context_new_array(pCtx);` |
|       2 | 4824 | `			}` |
|      79 | 4825 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|      55 | 4826 | `				ph7_value *pv = &sNull;` |
|      55 | 4827 | `				if( apCur[i] ){` |
|      53 | 4828 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|      53 | 4829 | `					if( pNodeVal ){` |
|      53 | 4830 | `						pv = pNodeVal;` |
|      26 | 4831 | `					}` |
|      53 | 4832 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|      26 | 4833 | `				}` |
|      55 | 4834 | `				if( bNullCallback ){` |
|       9 | 4835 | `					if( pZip ){` |
|       9 | 4836 | `						ph7_array_add_elem(pZip,0,pv);` |
|       4 | 4837 | `					}` |
|       5 | 4838 | `				}else{` |
|      47 | 4839 | `					apCallArg[i] = pv;` |
|       - | 4840 | `				}` |
|      28 | 4841 | `			}` |
|      25 | 4842 | `			if( bNullCallback ){` |
|       5 | 4843 | `				if( pZip ){` |
|       5 | 4844 | `					ph7_array_add_elem(pArray,0,pZip);` |
|       2 | 4845 | `				}` |
|       3 | 4846 | `			}else{` |
|      21 | 4847 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|      21 | 4848 | `				if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 4849 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|     ! 0 | 4850 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|     ! 0 | 4851 | `					PH7_MemObjRelease(&sNull);` |
|     ! 0 | 4852 | `					PH7_MemObjRelease(&sKey);` |
|     ! 0 | 4853 | `					PH7_MemObjRelease(&sResult);` |
|     ! 0 | 4854 | `					return PH7_EXCEPTION;` |
|       - | 4855 | `				}` |
|      21 | 4856 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|      21 | 4857 | `				PH7_MemObjRelease(&sResult);` |
|       - | 4858 | `			}` |
|      13 | 4859 | `		}` |
|      11 | 4860 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|      11 | 4861 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|      11 | 4862 | `		PH7_MemObjRelease(&sNull);` |
|       - | 4863 | `	}` |
|      90 | 4864 | `	PH7_MemObjRelease(&sKey);` |
|      90 | 4865 | `	PH7_MemObjRelease(&sResult);` |
|      90 | 4866 | `	ph7_result_value(pCtx,pArray);` |
|      90 | 4867 | `	return PH7_OK;` |
|      54 | 4868 | `}` |
|       - | 4869 | `/*` |
|       - | 4870 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|       - | 4871 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 4872 | ` * Parameters` |
|       - | 4873 | ` *  $array` |
|       - | 4874 | ` *   The input array.` |
|       - | 4875 | ` *  $callback` |
|       - | 4876 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|       - | 4877 | ` *  $initial` |
|       - | 4878 | ` *   If the optional initial is available, it will be used at the beginning` |
|       - | 4879 | ` *   of the process, or as a final result in case the array is empty.` |
|       - | 4880 | ` * Return` |
|       - | 4881 | ` *  Returns the resulting value.` |
|       - | 4882 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 4883 | ` */` |
|      28 | 4884 | `PH7_PRIVATE int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4885 | `{` |
|       - | 4886 | `	ph7_hashmap_node *pEntry;` |
|       - | 4887 | `	ph7_hashmap *pMap;` |
|       - | 4888 | `	ph7_value *pValue;` |
|       - | 4889 | `	ph7_value sResult;` |
|       - | 4890 | `	sxi32 rc;` |
|       - | 4891 | `	sxu32 n;` |
|      33 | 4892 | `	if( nArg < 2 ){` |
|     ! 0 | 4893 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4894 | `			"ArgumentCountError",` |
|       - | 4895 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|     ! 0 | 4896 | `			nArg` |
|       - | 4897 | `			);` |
|       - | 4898 | `	}` |
|      33 | 4899 | `	if( nArg > 3 ){` |
|     ! 0 | 4900 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4901 | `			"ArgumentCountError",` |
|       - | 4902 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|     ! 0 | 4903 | `			nArg` |
|       - | 4904 | `			);` |
|       - | 4905 | `	}` |
|      33 | 4906 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4907 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4908 | `			"TypeError",` |
|       - | 4909 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4910 | `			ph7_type_name(apArg[0])` |
|       - | 4911 | `			);` |
|       - | 4912 | `	}` |
|      31 | 4913 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      12 | 4914 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 4915 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 4916 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4917 | `				"TypeError",` |
|       - | 4918 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 4919 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 4920 | `				zFunc` |
|       - | 4921 | `				);` |
|       - | 4922 | `		}` |
|       9 | 4923 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       3 | 4924 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4925 | `				"TypeError",` |
|       - | 4926 | `				"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 4927 | `				"array callback must have exactly two members"` |
|       - | 4928 | `				);` |
|       - | 4929 | `		}` |
|       6 | 4930 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4931 | `			"TypeError",` |
|       - | 4932 | `			"array_reduce(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 4933 | `			"no array or string given"` |
|       - | 4934 | `			);` |
|       - | 4935 | `	}` |
|       - | 4936 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 4937 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4938 | `	/* Assume a NULL initial value */` |
|      19 | 4939 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      19 | 4940 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      19 | 4941 | `	if( nArg > 2 ){` |
|       - | 4942 | `		/* Set the initial value */` |
|      13 | 4943 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       6 | 4944 | `	}` |
|       - | 4945 | `	/* Perform the requested operation */` |
|      19 | 4946 | `	pEntry = pMap->pFirst;` |
|      55 | 4947 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 4948 | `		/* Extract the node value */` |
|      39 | 4949 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 4950 | `		/* Invoke the supplied callback */` |
|      39 | 4951 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|      39 | 4952 | `		if( rc == PH7_EXCEPTION ){` |
|       - | 4953 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 4954 | `			PH7_MemObjRelease(&sResult);` |
|       3 | 4955 | `			return PH7_EXCEPTION;` |
|       - | 4956 | `		}` |
|       - | 4957 | `		/* Point to the next entry */` |
|      37 | 4958 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 4959 | `	}` |
|      17 | 4960 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      17 | 4961 | `	PH7_MemObjRelease(&sResult);` |
|      17 | 4962 | `	return PH7_OK;` |
|      19 | 4963 | `}` |
|       - | 4964 | `/*` |
|       - | 4965 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 4966 | ` *  Apply a user function to every member of an array.` |
|       - | 4967 | ` * Parameters` |
|       - | 4968 | ` *  $array` |
|       - | 4969 | ` *   The input array.` |
|       - | 4970 | ` *  $funcname` |
|       - | 4971 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 4972 | ` *   the first, and the key/index second.` |
|       - | 4973 | ` * Note:` |
|       - | 4974 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 4975 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 4976 | ` *  be made in the original array itself.` |
|       - | 4977 | ` *  $userdata` |
|       - | 4978 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 4979 | ` *   to the callback funcname.` |
|       - | 4980 | ` * Return` |
|       - | 4981 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 4982 | ` */` |
|      36 | 4983 | `PH7_PRIVATE int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4984 | `{` |
|       - | 4985 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 4986 | `	ph7_hashmap_node *pEntry;` |
|       - | 4987 | `	ph7_hashmap *pMap;` |
|       - | 4988 | `	sxu32 n;` |
|      41 | 4989 | `	if( nArg < 2 ){` |
|     ! 0 | 4990 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4991 | `			"ArgumentCountError",` |
|       - | 4992 | `			"array_walk() expects at least 2 arguments, %d given",` |
|     ! 0 | 4993 | `			nArg` |
|       - | 4994 | `			);` |
|       - | 4995 | `	}` |
|      41 | 4996 | `	if( nArg > 3 ){` |
|     ! 0 | 4997 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4998 | `			"ArgumentCountError",` |
|       - | 4999 | `			"array_walk() expects at most 3 arguments, %d given",` |
|     ! 0 | 5000 | `			nArg` |
|       - | 5001 | `			);` |
|       - | 5002 | `	}` |
|      41 | 5003 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5004 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5005 | `			"TypeError",` |
|       - | 5006 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5007 | `			ph7_type_name(apArg[0])` |
|       - | 5008 | `			);` |
|       - | 5009 | `	}` |
|      39 | 5010 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      17 | 5011 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       6 | 5012 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       8 | 5013 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5014 | `				"TypeError",` |
|       - | 5015 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 5016 | `				"function \"%s\" not found or invalid function name",` |
|       2 | 5017 | `				zFunc` |
|       - | 5018 | `				);` |
|       - | 5019 | `		}` |
|      12 | 5020 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       6 | 5021 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5022 | `				"TypeError",` |
|       - | 5023 | `				"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 5024 | `				"array callback must have exactly two members"` |
|       - | 5025 | `				);` |
|       - | 5026 | `		}` |
|       6 | 5027 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5028 | `			"TypeError",` |
|       - | 5029 | `			"array_walk(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 5030 | `			"no array or string given"` |
|       - | 5031 | `			);` |
|       - | 5032 | `	}` |
|      23 | 5033 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 5034 | `	/* Point to the internal representation of the input hashmap */` |
|      23 | 5035 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      23 | 5036 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 5037 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      23 | 5038 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 5039 | `	/* Perform the desired operation */` |
|      23 | 5040 | `	pEntry = pMap->pFirst;` |
|      69 | 5041 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5042 | `		/* Extract the node value */` |
|      49 | 5043 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      49 | 5044 | `		if( pValue ){` |
|       - | 5045 | `			sxi32 rcW;` |
|       - | 5046 | `			/* Extract the entry key */` |
|      49 | 5047 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 5048 | `			/* Invoke the supplied callback */` |
|      49 | 5049 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      49 | 5050 | `			PH7_MemObjRelease(&sKey);` |
|      49 | 5051 | `			if( rcW == PH7_EXCEPTION ){` |
|       - | 5052 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 5053 | `				return PH7_EXCEPTION;` |
|       - | 5054 | `			}` |
|      23 | 5055 | `		}` |
|       - | 5056 | `		/* Point to the next entry */` |
|      47 | 5057 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      24 | 5058 | `	}` |
|       - | 5059 | `	/* All done, return TRUE */` |
|      21 | 5060 | `	ph7_result_bool(pCtx,1);` |
|      21 | 5061 | `	return PH7_OK;` |
|      23 | 5062 | `}` |
|       - | 5063 | `/*` |
|       - | 5064 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 5065 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 5066 | ` */` |
|      22 | 5067 | `static sxi32 HashmapWalkRecursive(` |
|       - | 5068 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 5069 | `	ph7_value *pCallback, /* User callback */` |
|       - | 5070 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 5071 | `	int iNest             /* Nesting level */` |
|       - | 5072 | `	)` |
|       1 | 5073 | `{` |
|       - | 5074 | `	ph7_hashmap_node *pEntry;` |
|       - | 5075 | `	ph7_value *pValue,sKey;` |
|       - | 5076 | `	sxi32 rc;` |
|       - | 5077 | `	sxu32 n;` |
|       - | 5078 | `	/* Iterate through hashmap entries */` |
|      23 | 5079 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      23 | 5080 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      23 | 5081 | `	pEntry = pMap->pFirst;` |
|      59 | 5082 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5083 | `		/* Extract the node value */` |
|      37 | 5084 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      37 | 5085 | `		if( pValue ){` |
|      37 | 5086 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      11 | 5087 | `				if( iNest < 32 ){` |
|       - | 5088 | `					/* Recurse */` |
|      11 | 5089 | `					iNest++;` |
|      11 | 5090 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      11 | 5091 | `					iNest--;` |
|      11 | 5092 | `					if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 5093 | `						return PH7_EXCEPTION;` |
|       - | 5094 | `					}` |
|       5 | 5095 | `				}` |
|       6 | 5096 | `			}else{` |
|       - | 5097 | `				/* Extract the node key */` |
|      27 | 5098 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 5099 | `				/* Invoke the supplied callback */` |
|      27 | 5100 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|      27 | 5101 | `				PH7_MemObjRelease(&sKey);` |
|      27 | 5102 | `				if( rc == PH7_EXCEPTION ){` |
|       - | 5103 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 5104 | `					return PH7_EXCEPTION;` |
|       - | 5105 | `				}` |
|       - | 5106 | `			}` |
|      18 | 5107 | `		}` |
|       - | 5108 | `		/* Point to the next entry */` |
|      37 | 5109 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 5110 | `	}` |
|      23 | 5111 | `	return PH7_OK;` |
|      12 | 5112 | `}` |
|       - | 5113 | `/*` |
|       - | 5114 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 5115 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 5116 | ` * Parameters` |
|       - | 5117 | ` *  $array` |
|       - | 5118 | ` *   The input array.` |
|       - | 5119 | ` *  $funcname` |
|       - | 5120 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 5121 | ` *   the first, and the key/index second.` |
|       - | 5122 | ` * Note:` |
|       - | 5123 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 5124 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 5125 | ` *  be made in the original array itself.` |
|       - | 5126 | ` *  $userdata` |
|       - | 5127 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 5128 | ` *   to the callback funcname.` |
|       - | 5129 | ` * Return` |
|       - | 5130 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 5131 | ` */` |
|      24 | 5132 | `PH7_PRIVATE int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5133 | `{` |
|       - | 5134 | `	ph7_hashmap *pMap;` |
|      29 | 5135 | `	if( nArg < 2 ){` |
|     ! 0 | 5136 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5137 | `			"ArgumentCountError",` |
|       - | 5138 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|     ! 0 | 5139 | `			nArg` |
|       - | 5140 | `			);` |
|       - | 5141 | `	}` |
|      29 | 5142 | `	if( nArg > 3 ){` |
|     ! 0 | 5143 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5144 | `			"ArgumentCountError",` |
|       - | 5145 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|     ! 0 | 5146 | `			nArg` |
|       - | 5147 | `			);` |
|       - | 5148 | `	}` |
|      29 | 5149 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5150 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5151 | `			"TypeError",` |
|       - | 5152 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5153 | `			ph7_type_name(apArg[0])` |
|       - | 5154 | `			);` |
|       - | 5155 | `	}` |
|      27 | 5156 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|      14 | 5157 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       3 | 5158 | `			const char *zFunc = ph7_value_to_string(apArg[1],0);` |
|       4 | 5159 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5160 | `				"TypeError",` |
|       - | 5161 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 5162 | `				"function \"%s\" not found or invalid function name",` |
|       1 | 5163 | `				zFunc` |
|       - | 5164 | `				);` |
|       - | 5165 | `		}` |
|      12 | 5166 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       6 | 5167 | `			return PH7_VmThrowException(pCtx,` |
|       - | 5168 | `				"TypeError",` |
|       - | 5169 | `				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 5170 | `				"array callback must have exactly two members"` |
|       - | 5171 | `				);` |
|       - | 5172 | `		}` |
|       6 | 5173 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5174 | `			"TypeError",` |
|       - | 5175 | `			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "` |
|       - | 5176 | `			"no array or string given"` |
|       - | 5177 | `			);` |
|       - | 5178 | `	}` |
|       - | 5179 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 5180 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      13 | 5181 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5182 | `	/* Perform the desired operation */` |
|      13 | 5183 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|       - | 5184 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 5185 | `		return PH7_EXCEPTION;` |
|       - | 5186 | `	}` |
|       - | 5187 | `	/* All done, return TRUE */` |
|      13 | 5188 | `	ph7_result_bool(pCtx,1);` |
|      13 | 5189 | `	return PH7_OK;` |
|      17 | 5190 | `}` |
|       - | 5191 | `/*` |
|       - | 5192 | ` * bool array_is_list(array $array)` |
|       - | 5193 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|       - | 5194 | ` *  integers starting at 0. An empty array is a list.` |
|       - | 5195 | ` * Return` |
|       - | 5196 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|       - | 5197 | ` */` |
|       - | 5198 | `/*` |
|       - | 5199 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|       - | 5200 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|       - | 5201 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|       - | 5202 | ` */` |
|     414 | 5203 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|       4 | 5204 | `{` |
|     418 | 5205 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|     418 | 5206 | `	sxi64 iExpect = 0;` |
|       - | 5207 | `	sxu32 n;` |
|    1032 | 5208 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     774 | 5209 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|       - | 5210 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|     159 | 5211 | `			return 0;` |
|       - | 5212 | `		}` |
|     618 | 5213 | `		++iExpect;` |
|     618 | 5214 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     311 | 5215 | `	}` |
|     262 | 5216 | `	return 1;` |
|     211 | 5217 | `}` |
|      12 | 5218 | `PH7_PRIVATE int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5219 | `{` |
|      13 | 5220 | `	if( nArg < 1 ){` |
|     ! 0 | 5221 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5222 | `			"ArgumentCountError",` |
|       - | 5223 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|       - | 5224 | `			);` |
|       - | 5225 | `	}` |
|      13 | 5226 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5227 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5228 | `			"TypeError",` |
|       - | 5229 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5230 | `			ph7_type_name(apArg[0])` |
|       - | 5231 | `			);` |
|       - | 5232 | `	}` |
|      13 | 5233 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|      13 | 5234 | `	return PH7_OK;` |
|       7 | 5235 | `}` |
|       - | 5236 | `/*` |
|       - | 5237 | ` * mixed array_first(array $array)` |
|       - | 5238 | ` * mixed array_last(array $array)` |
|       - | 5239 | ` *  Return the value of the first (respectively last) element of the array,` |
|       - | 5240 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|       - | 5241 | ` *  untouched (unlike reset()/end()).` |
|       - | 5242 | ` */` |
|      18 | 5243 | `static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|       1 | 5244 | `{` |
|       - | 5245 | `	ph7_hashmap *pMap;` |
|       - | 5246 | `	ph7_hashmap_node *pNode;` |
|       - | 5247 | `	ph7_value *pVal;` |
|      19 | 5248 | `	const char *zName = bLast ? "array_last" : "array_first";` |
|      19 | 5249 | `	if( nArg < 1 ){` |
|     ! 0 | 5250 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5251 | `			"ArgumentCountError",` |
|       - | 5252 | `			"%s() expects exactly 1 argument, 0 given",` |
|     ! 0 | 5253 | `			zName` |
|       - | 5254 | `			);` |
|       - | 5255 | `	}` |
|      19 | 5256 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5257 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5258 | `			"TypeError",` |
|       - | 5259 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5260 | `			zName,` |
|       1 | 5261 | `			ph7_type_name(apArg[0])` |
|       - | 5262 | `			);` |
|       - | 5263 | `	}` |
|      17 | 5264 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      17 | 5265 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|      17 | 5266 | `	if( pNode == 0 ){` |
|       - | 5267 | `		/* Empty array: PHP returns NULL */` |
|       5 | 5268 | `		ph7_result_null(pCtx);` |
|       5 | 5269 | `		return PH7_OK;` |
|       - | 5270 | `	}` |
|      13 | 5271 | `	pVal = HashmapExtractNodeValue(pNode);` |
|      13 | 5272 | `	if( pVal ){` |
|      13 | 5273 | `		ph7_result_value(pCtx,pVal);` |
|       7 | 5274 | `	}else{` |
|     ! 0 | 5275 | `		ph7_result_null(pCtx);` |
|       - | 5276 | `	}` |
|      13 | 5277 | `	return PH7_OK;` |
|      10 | 5278 | `}` |
|       8 | 5279 | `PH7_PRIVATE int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5280 | `{` |
|       9 | 5281 | `	return HashmapFirstLast(pCtx,nArg,apArg,0);` |
|       1 | 5282 | `}` |
|      10 | 5283 | `PH7_PRIVATE int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5284 | `{` |
|      11 | 5285 | `	return HashmapFirstLast(pCtx,nArg,apArg,1);` |
|       1 | 5286 | `}` |
|       - | 5287 | `/*` |
|       - | 5288 | ` * int\|string\|null array_key_first(array $array)` |
|       - | 5289 | ` * int\|string\|null array_key_last(array $array)` |
|       - | 5290 | ` *  Return the key of the first (respectively last) element of the array,` |
|       - | 5291 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|       - | 5292 | ` *  untouched.` |
|       - | 5293 | ` */` |
|      22 | 5294 | `static int HashmapKeyFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|       1 | 5295 | `{` |
|       - | 5296 | `	ph7_hashmap *pMap;` |
|       - | 5297 | `	ph7_hashmap_node *pNode;` |
|      23 | 5298 | `	const char *zName = bLast ? "array_key_last" : "array_key_first";` |
|      23 | 5299 | `	if( nArg < 1 ){` |
|     ! 0 | 5300 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5301 | `			"ArgumentCountError",` |
|       - | 5302 | `			"%s() expects exactly 1 argument, 0 given",` |
|     ! 0 | 5303 | `			zName` |
|       - | 5304 | `			);` |
|       - | 5305 | `	}` |
|      23 | 5306 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5307 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5308 | `			"TypeError",` |
|       - | 5309 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5310 | `			zName,` |
|       1 | 5311 | `			ph7_type_name(apArg[0])` |
|       - | 5312 | `			);` |
|       - | 5313 | `	}` |
|      21 | 5314 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      21 | 5315 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|      21 | 5316 | `	if( pNode == 0 ){` |
|       - | 5317 | `		/* Empty array: PHP returns NULL */` |
|       5 | 5318 | `		ph7_result_null(pCtx);` |
|       5 | 5319 | `		return PH7_OK;` |
|       - | 5320 | `	}` |
|      17 | 5321 | `	HashmapResultNodeKey(pCtx,pNode);` |
|      17 | 5322 | `	return PH7_OK;` |
|      12 | 5323 | `}` |
|      10 | 5324 | `PH7_PRIVATE int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5325 | `{` |
|      11 | 5326 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,0);` |
|       1 | 5327 | `}` |
|      12 | 5328 | `PH7_PRIVATE int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5329 | `{` |
|      13 | 5330 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,1);` |
|       1 | 5331 | `}` |
|       - | 5332 | `/*` |
|       - | 5333 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|       - | 5334 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|       - | 5335 | ` * array_column() for both the column value and the index key.` |
|       - | 5336 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|       - | 5337 | ` * container or the key is absent.` |
|       - | 5338 | ` */` |
|      32 | 5339 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|       1 | 5340 | `{` |
|      33 | 5341 | `	if( ph7_value_is_array(pRow) ){` |
|       - | 5342 | `		ph7_hashmap_node *pNode;` |
|      25 | 5343 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|      21 | 5344 | `			return HashmapExtractNodeValue(pNode);` |
|       1 | 5345 | `		}` |
|      11 | 5346 | `	}else if( ph7_value_is_object(pRow) ){` |
|       - | 5347 | `		ph7_value sName;` |
|       - | 5348 | `		const char *zName;` |
|       - | 5349 | `		ph7_value *pAttr;` |
|       - | 5350 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|       - | 5351 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|       9 | 5352 | `		PH7_MemObjInit(pVm,&sName);` |
|       9 | 5353 | `		PH7_MemObjStore(pKey,&sName);` |
|       9 | 5354 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|       9 | 5355 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|       9 | 5356 | `		PH7_MemObjRelease(&sName);` |
|       9 | 5357 | `		return pAttr;` |
|       - | 5358 | `	}` |
|       5 | 5359 | `	return 0;` |
|      17 | 5360 | `}` |
|       - | 5361 | `/*` |
|       - | 5362 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|       - | 5363 | ` *  Returns the values from a single column of the input, identified by` |
|       - | 5364 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|       - | 5365 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|       - | 5366 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|       - | 5367 | ` *  Each row may be an array or an object.` |
|       - | 5368 | ` */` |
|      12 | 5369 | `PH7_PRIVATE int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5370 | `{` |
|       - | 5371 | `	ph7_hashmap_node *pNode;` |
|       - | 5372 | `	ph7_hashmap *pMap;` |
|       - | 5373 | `	ph7_value *pArray;` |
|       - | 5374 | `	ph7_value *pRow;` |
|       - | 5375 | `	ph7_value *pCol;` |
|       - | 5376 | `	ph7_value *pIdx;` |
|       - | 5377 | `	int bWantCol;` |
|       - | 5378 | `	int bWantIdx;` |
|       - | 5379 | `	sxu32 n;` |
|      13 | 5380 | `	if( nArg < 2 ){` |
|     ! 0 | 5381 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5382 | `			"ArgumentCountError",` |
|       - | 5383 | `			"array_column() expects at least 2 arguments, %d given",` |
|     ! 0 | 5384 | `			nArg` |
|       - | 5385 | `			);` |
|       - | 5386 | `	}` |
|      13 | 5387 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5388 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5389 | `			"TypeError",` |
|       - | 5390 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5391 | `			ph7_type_name(apArg[0])` |
|       - | 5392 | `			);` |
|       - | 5393 | `	}` |
|      13 | 5394 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      13 | 5395 | `	pArray = ph7_context_new_array(pCtx);` |
|      13 | 5396 | `	if( pArray == 0 ){` |
|     ! 0 | 5397 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5398 | `		return PH7_OK;` |
|       - | 5399 | `	}` |
|       - | 5400 | `	/* A NULL column_key means "collect the entire row". */` |
|      13 | 5401 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|      13 | 5402 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|      13 | 5403 | `	pNode = pMap->pFirst;` |
|      33 | 5404 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      21 | 5405 | `		pRow = HashmapExtractNodeValue(pNode);` |
|      21 | 5406 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|      21 | 5407 | `		if( pRow == 0 ){` |
|     ! 0 | 5408 | `			continue;` |
|       - | 5409 | `		}` |
|      21 | 5410 | `		if( bWantCol ){` |
|      19 | 5411 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|      19 | 5412 | `			if( pCol == 0 ){` |
|       - | 5413 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|       3 | 5414 | `				continue;` |
|       - | 5415 | `			}` |
|       9 | 5416 | `		}else{` |
|       3 | 5417 | `			pCol = pRow;` |
|       - | 5418 | `		}` |
|      19 | 5419 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|      19 | 5420 | `		if( pIdx ){` |
|      13 | 5421 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|       7 | 5422 | `		}else{` |
|       7 | 5423 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|       - | 5424 | `		}` |
|      10 | 5425 | `	}` |
|      13 | 5426 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 5427 | `	return PH7_OK;` |
|       7 | 5428 | `}` |
|       - | 5429 | `/*` |
|       - | 5430 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|       - | 5431 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|       - | 5432 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|       - | 5433 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|       - | 5434 | ` */` |
|      28 | 5435 | `static sxi32 HashmapCallbackSearch(` |
|       - | 5436 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|       - | 5437 | `	const char *zName,            /* Function name for diagnostics */` |
|       - | 5438 | `	int bWant,                    /* Truthiness being hunted for */` |
|       - | 5439 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|       - | 5440 | `	)` |
|       1 | 5441 | `{` |
|       - | 5442 | `	ph7_hashmap_node *pEntry;` |
|       - | 5443 | `	ph7_hashmap *pMap;` |
|       - | 5444 | `	ph7_value *pValue;` |
|       - | 5445 | `	ph7_value *apCbArg[2];` |
|       - | 5446 | `	ph7_value sKey;` |
|       - | 5447 | `	ph7_value sResult;` |
|       - | 5448 | `	sxi32 rc;` |
|       - | 5449 | `	sxu32 n;` |
|      29 | 5450 | `	*ppMatch = 0;` |
|      29 | 5451 | `	if( nArg < 2 ){` |
|     ! 0 | 5452 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5453 | `			"ArgumentCountError",` |
|       - | 5454 | `			"%s() expects exactly 2 arguments, %d given",` |
|     ! 0 | 5455 | `			zName,nArg` |
|       - | 5456 | `			);` |
|       - | 5457 | `	}` |
|      29 | 5458 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5459 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5460 | `			"TypeError",` |
|       - | 5461 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5462 | `			zName,ph7_type_name(apArg[0])` |
|       - | 5463 | `			);` |
|       - | 5464 | `	}` |
|      29 | 5465 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|     ! 0 | 5466 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5467 | `			"TypeError",` |
|       - | 5468 | `			"%s(): Argument #2 ($callback) must be a valid callback, %s given",` |
|     ! 0 | 5469 | `			zName,ph7_type_name(apArg[1])` |
|       - | 5470 | `			);` |
|       - | 5471 | `	}` |
|      29 | 5472 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      29 | 5473 | `	pEntry = pMap->pFirst;` |
|      29 | 5474 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      29 | 5475 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|      29 | 5476 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      29 | 5477 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      73 | 5478 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 5479 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      59 | 5480 | `		if( pValue ){` |
|       - | 5481 | `			/* The callback receives ($value, $key). */` |
|      59 | 5482 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      59 | 5483 | `			apCbArg[0] = pValue;` |
|      59 | 5484 | `			apCbArg[1] = &sKey;` |
|      59 | 5485 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|      59 | 5486 | `			if( rc == PH7_EXCEPTION ){` |
|       - | 5487 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 5488 | `				PH7_MemObjRelease(&sKey);` |
|     ! 0 | 5489 | `				PH7_MemObjRelease(&sResult);` |
|     ! 0 | 5490 | `				return PH7_EXCEPTION;` |
|       - | 5491 | `			}` |
|      59 | 5492 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|      15 | 5493 | `				*ppMatch = pEntry;` |
|      15 | 5494 | `				break;` |
|       - | 5495 | `			}` |
|      22 | 5496 | `		}` |
|      45 | 5497 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      23 | 5498 | `	}` |
|      29 | 5499 | `	PH7_MemObjRelease(&sKey);` |
|      29 | 5500 | `	PH7_MemObjRelease(&sResult);` |
|      29 | 5501 | `	return PH7_OK;` |
|      15 | 5502 | `}` |
|       - | 5503 | `/*` |
|       - | 5504 | ` * mixed array_find(array $array, callable $callback)` |
|       - | 5505 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|       - | 5506 | ` *  is truthy, or NULL if none match.` |
|       - | 5507 | ` */` |
|       6 | 5508 | `PH7_PRIVATE int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5509 | `{` |
|       - | 5510 | `	ph7_hashmap_node *pMatch;` |
|       - | 5511 | `	ph7_value *pVal;` |
|       - | 5512 | `	sxi32 rc;` |
|       7 | 5513 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|       7 | 5514 | `	if( rc != PH7_OK ){` |
|     ! 0 | 5515 | `		return rc;` |
|       - | 5516 | `	}` |
|       7 | 5517 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|       5 | 5518 | `		ph7_result_value(pCtx,pVal);` |
|       3 | 5519 | `	}else{` |
|       3 | 5520 | `		ph7_result_null(pCtx);` |
|       - | 5521 | `	}` |
|       7 | 5522 | `	return PH7_OK;` |
|       4 | 5523 | `}` |
|       - | 5524 | `/*` |
|       - | 5525 | ` * mixed array_find_key(array $array, callable $callback)` |
|       - | 5526 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|       - | 5527 | ` *  is truthy, or NULL if none match.` |
|       - | 5528 | ` */` |
|       6 | 5529 | `PH7_PRIVATE int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5530 | `{` |
|       - | 5531 | `	ph7_hashmap_node *pMatch;` |
|       - | 5532 | `	sxi32 rc;` |
|       7 | 5533 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|       7 | 5534 | `	if( rc != PH7_OK ){` |
|     ! 0 | 5535 | `		return rc;` |
|       - | 5536 | `	}` |
|       7 | 5537 | `	if( pMatch == 0 ){` |
|       3 | 5538 | `		ph7_result_null(pCtx);` |
|       6 | 5539 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|       3 | 5540 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|       2 | 5541 | `	}else{` |
|       4 | 5542 | `		ph7_result_string(pCtx,` |
|       2 | 5543 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|       2 | 5544 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|       - | 5545 | `	}` |
|       7 | 5546 | `	return PH7_OK;` |
|       4 | 5547 | `}` |
|       - | 5548 | `/*` |
|       - | 5549 | ` * bool array_any(array $array, callable $callback)` |
|       - | 5550 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|       - | 5551 | ` *  FALSE for an empty array.` |
|       - | 5552 | ` */` |
|       8 | 5553 | `PH7_PRIVATE int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5554 | `{` |
|       - | 5555 | `	ph7_hashmap_node *pMatch;` |
|       - | 5556 | `	sxi32 rc;` |
|       9 | 5557 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|       9 | 5558 | `	if( rc != PH7_OK ){` |
|     ! 0 | 5559 | `		return rc;` |
|       - | 5560 | `	}` |
|       9 | 5561 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|       9 | 5562 | `	return PH7_OK;` |
|       5 | 5563 | `}` |
|       - | 5564 | `/*` |
|       - | 5565 | ` * bool array_all(array $array, callable $callback)` |
|       - | 5566 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|       - | 5567 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|       - | 5568 | ` */` |
|       8 | 5569 | `PH7_PRIVATE int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5570 | `{` |
|       - | 5571 | `	ph7_hashmap_node *pMatch;` |
|       - | 5572 | `	sxi32 rc;` |
|       9 | 5573 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|       9 | 5574 | `	if( rc != PH7_OK ){` |
|     ! 0 | 5575 | `		return rc;` |
|       - | 5576 | `	}` |
|       9 | 5577 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|       9 | 5578 | `	return PH7_OK;` |
|       5 | 5579 | `}` |
|       - | 5580 | `/*` |
|       - | 5581 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|       - | 5582 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|       - | 5583 | ` */` |
|       - | 5584 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|       - | 5585 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|      80 | 5586 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       4 | 5587 | `{` |
|      84 | 5588 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|      40 | 5589 | `	(void)pVm;` |
|      84 | 5590 | `	p->nCount++;` |
|      84 | 5591 | `	if( p->pArray ){` |
|       - | 5592 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|       - | 5593 | `		 * otherwise append with an auto-assigned int index. */` |
|      70 | 5594 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|      33 | 5595 | `	}` |
|      84 | 5596 | `	return SXRET_OK;` |
|       4 | 5597 | `}` |
|       - | 5598 | `/*` |
|       - | 5599 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|       - | 5600 | ` */` |
|      30 | 5601 | `PH7_PRIVATE int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       4 | 5602 | `{` |
|       - | 5603 | `	struct IterCollect sCol;` |
|       - | 5604 | `	ph7_value *pArray;` |
|       - | 5605 | `	sxi32 rc;` |
|      34 | 5606 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      34 | 5607 | `	pArray = ph7_context_new_array(pCtx);` |
|      34 | 5608 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      34 | 5609 | `	sCol.pArray = pArray;` |
|      34 | 5610 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|      34 | 5611 | `	sCol.nCount = 0;` |
|      34 | 5612 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       - | 5613 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|       3 | 5614 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 5615 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 5616 | `		sxu32 n;` |
|       9 | 5617 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5618 | `			ph7_value sKey, *pVal;` |
|       7 | 5619 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|       7 | 5620 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       7 | 5621 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|       7 | 5622 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|       7 | 5623 | `			PH7_MemObjRelease(&sKey);` |
|       7 | 5624 | `			pEntry = pEntry->pPrev;` |
|       4 | 5625 | `		}` |
|       3 | 5626 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 5627 | `		return PH7_OK;` |
|       - | 5628 | `	}` |
|      32 | 5629 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|      32 | 5630 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|      30 | 5631 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 5632 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 5633 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|     ! 0 | 5634 | `			ph7_type_name(apArg[0]));` |
|       - | 5635 | `	}` |
|      30 | 5636 | `	ph7_result_value(pCtx,pArray);` |
|      30 | 5637 | `	return PH7_OK;` |
|      19 | 5638 | `}` |
|       - | 5639 | `/*` |
|       - | 5640 | ` * int iterator_count(Traversable\|array $iterator)` |
|       - | 5641 | ` */` |
|       8 | 5642 | `PH7_PRIVATE int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 5643 | `{` |
|       - | 5644 | `	struct IterCollect sCol;` |
|       - | 5645 | `	sxi32 rc;` |
|       9 | 5646 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|       9 | 5647 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       3 | 5648 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|       3 | 5649 | `		return PH7_OK;` |
|       - | 5650 | `	}` |
|       7 | 5651 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|       7 | 5652 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|       7 | 5653 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|       7 | 5654 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 5655 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 5656 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|     ! 0 | 5657 | `			ph7_type_name(apArg[0]));` |
|       - | 5658 | `	}` |
|       7 | 5659 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|       7 | 5660 | `	return PH7_OK;` |
|       5 | 5661 | `}` |
|       - | 5662 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|       - | 5663 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|       - | 5664 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|       - | 5665 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|      32 | 5666 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       1 | 5667 | `{` |
|      33 | 5668 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|       - | 5669 | `	ph7_value sResult;` |
|       - | 5670 | `	SySet aArg;` |
|       - | 5671 | `	sxi32 rc;` |
|       - | 5672 | `	int bContinue;` |
|      16 | 5673 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|      33 | 5674 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|      33 | 5675 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|       9 | 5676 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|       9 | 5677 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 5678 | `		sxu32 n;` |
|      17 | 5679 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       9 | 5680 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|       9 | 5681 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|       9 | 5682 | `			pEntry = pEntry->pPrev;` |
|       5 | 5683 | `		}` |
|       4 | 5684 | `	}` |
|      33 | 5685 | `	PH7_MemObjInit(pVm,&sResult);` |
|      49 | 5686 | `	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|      32 | 5687 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult);` |
|      33 | 5688 | `	SySetRelease(&aArg);` |
|      33 | 5689 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|      31 | 5690 | `	p->nCount++;` |
|      31 | 5691 | `	PH7_MemObjToBool(&sResult);` |
|      31 | 5692 | `	bContinue = (sResult.x.iVal != 0);` |
|      31 | 5693 | `	PH7_MemObjRelease(&sResult);` |
|      31 | 5694 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|      17 | 5695 | `}` |
|       - | 5696 | `/*` |
|       - | 5697 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|       - | 5698 | ` */` |
|      12 | 5699 | `PH7_PRIVATE int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 5700 | `{` |
|       - | 5701 | `	struct IterApply sApp;` |
|       - | 5702 | `	sxi32 rc;` |
|      13 | 5703 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|      13 | 5704 | `	if( !ph7_value_is_callable(apArg[1]) ){` |
|     ! 0 | 5705 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 5706 | `			"iterator_apply(): Argument #2 ($callback) must be a valid callback");` |
|       - | 5707 | `	}` |
|      13 | 5708 | `	sApp.pCallback = apArg[1];` |
|      13 | 5709 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|      13 | 5710 | `	sApp.nCount = 0;` |
|      13 | 5711 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|      13 | 5712 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|      11 | 5713 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 5714 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 5715 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|     ! 0 | 5716 | `			ph7_type_name(apArg[0]));` |
|       - | 5717 | `	}` |
|      11 | 5718 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|      11 | 5719 | `	return PH7_OK;` |
|       7 | 5720 | `}` |
|       - | 5721 |  |
