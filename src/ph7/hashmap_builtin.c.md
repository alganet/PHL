# src/ph7/hashmap_builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2383/2759 lines (86.37%)

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
|    2512 |   64 | `PH7_PRIVATE int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |   65 | `{` |
|    2517 |   66 | `	int bRecursive = FALSE;` |
|    2517 |   67 | `	int bCycleDetected = FALSE;` |
|       - |   68 | `	sxi64 iCount;` |
|    2517 |   69 | `	if( nArg < 1 ){` |
|     ! 0 |   70 | `		return PH7_VmThrowException(pCtx,` |
|       - |   71 | `			"ArgumentCountError",` |
|       - |   72 | `			"count() expects at least 1 argument, 0 given"` |
|       - |   73 | `			);` |
|       - |   74 | `	}` |
|    2517 |   75 | `	if( nArg > 2 ){` |
|     ! 0 |   76 | `		return PH7_VmThrowException(pCtx,` |
|       - |   77 | `			"ArgumentCountError",` |
|       - |   78 | `			"count() expects at most 2 arguments, %d given",` |
|     ! 0 |   79 | `			nArg` |
|       - |   80 | `			);` |
|       - |   81 | `	}` |
|       - |   82 | `	/* PHP validates $mode right after parsing, before the type dispatch, so` |
|       - |   83 | `	 * an invalid mode raises ValueError whether $value is an array or a` |
|       - |   84 | `	 * Countable object (the mode is then ignored for the Countable path). */` |
|    2517 |   85 | `	if( nArg > 1 ){` |
|      47 |   86 | `		sxi32 iMode = ph7_value_to_int(apArg[1]);` |
|      47 |   87 | `		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){` |
|      11 |   88 | `			return PH7_VmThrowException(pCtx,` |
|       - |   89 | `				"ValueError",` |
|       - |   90 | `				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"` |
|       - |   91 | `				);` |
|       - |   92 | `		}` |
|      36 |   93 | `		bRecursive = iMode == 1;` |
|      17 |   94 | `	}` |
|    2509 |   95 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |   96 | `		/* Countable object: dispatch to ->count() */` |
|      75 |   97 | `		if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      66 |   98 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      66 |   99 | `			ph7_class *pCountable = pCtx->pVm->pCountableClass;` |
|      66 |  100 | `			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){` |
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
|    2439 |  120 | `	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);` |
|    2439 |  121 | `	if( bCycleDetected ){` |
|       3 |  122 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");` |
|       1 |  123 | `	}` |
|    2439 |  124 | `	ph7_result_int64(pCtx,iCount);` |
|    2439 |  125 | `	return PH7_OK;` |
|    1261 |  126 | `}` |
|       - |  127 | `/*` |
|       - |  128 | ` * bool array_key_exists(value $key,array $search)` |
|       - |  129 | ` * bool key_exists(value $key,array $search)` |
|       - |  130 | ` *  Checks if the given key or index exists in the array.` |
|       - |  131 | ` * Parameters` |
|       - |  132 | ` * $key` |
|       - |  133 | `` *   Value to check. Follows php's ARRAY-OFFSET rules, not a `string\|int` ZPP row`` |
|       - |  134 | ``  *   (PH7_VmArrayKeyArg): the key this builtin looks up is the key `$search[$key]` `` |
|       - |  135 | ` *   would look up, down to the diagnostics.` |
|       - |  136 | ` * $search` |
|       - |  137 | ` *  An array with keys to check.` |
|       - |  138 | ` * Return` |
|       - |  139 | ` *  TRUE on success or FALSE on failure.` |
|       - |  140 | ` */` |
|     144 |  141 | `PH7_PRIVATE int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  142 | `{` |
|     149 |  143 | `	const char *zName = ph7_function_name(pCtx);` |
|       - |  144 | `	/* php words the illegal-key rejection differently in the ALIAS than in` |
|       - |  145 | `	 * array_key_exists() itself; the two names share this routine, so match the` |
|       - |  146 | `	 * whole name rather than a leading byte. */` |
|     156 |  147 | `	int bAlias = zName && SyStrlen(zName) == sizeof("key_exists")-1` |
|     216 |  148 | `		&& SyMemcmp(zName,"key_exists",sizeof("key_exists")-1) == 0;` |
|       - |  149 | `	ph7_value sKey;` |
|       - |  150 | `	sxi32 rc;` |
|     149 |  151 | `	if( nArg != 2 ){` |
|       - |  152 | `		/* PHP requires exactly two arguments */` |
|     ! 0 |  153 | `		return PH7_VmThrowException(pCtx,` |
|       - |  154 | `			"ArgumentCountError",` |
|       - |  155 | `			"%s() expects exactly 2 arguments, %d given",` |
|     ! 0 |  156 | `			zName,nArg` |
|       - |  157 | `			);` |
|       - |  158 | `	}` |
|       - |  159 | `	/* Make sure we are dealing with a valid hashmap */` |
|     149 |  160 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - |  161 | `		/* Type mismatch -> TypeError */` |
|      15 |  162 | `		return PH7_VmThrowException(pCtx,` |
|       - |  163 | `			"TypeError",` |
|       - |  164 | `			"%s(): Argument #2 ($array) must be of type array, %s given",` |
|       8 |  165 | `			zName,ph7_type_name(apArg[1])` |
|       - |  166 | `			);` |
|       - |  167 | `	}` |
|       - |  168 | `	/* Normalize the key on a PRIVATE copy — a resource key is rewritten to its id` |
|       - |  169 | `	 * and the caller's own variable must not change. */` |
|     141 |  170 | `	PH7_MemObjInit(pCtx->pVm,&sKey);` |
|     141 |  171 | `	PH7_MemObjStore(apArg[0],&sKey);` |
|     141 |  172 | `	rc = PH7_VmArrayKeyArg(pCtx,&sKey,bAlias);` |
|     141 |  173 | `	if( rc != SXRET_OK ){` |
|      19 |  174 | `		PH7_MemObjRelease(&sKey);` |
|      19 |  175 | `		return rc;` |
|       - |  176 | `	}` |
|       - |  177 | `	/* Perform the lookup */` |
|     125 |  178 | `	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,&sKey,0);` |
|     125 |  179 | `	PH7_MemObjRelease(&sKey);` |
|       - |  180 | `	/* lookup result */` |
|     125 |  181 | `	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);` |
|     125 |  182 | `	return PH7_OK;` |
|      77 |  183 | `}` |
|       - |  184 | `/*` |
|       - |  185 | ` * value array_pop(array $array)` |
|       - |  186 | ` *   POP the last inserted element from the array.` |
|       - |  187 | ` * Parameter` |
|       - |  188 | ` *  The array to get the value from.` |
|       - |  189 | ` * Return` |
|       - |  190 | ` *  Poped value or NULL on failure.` |
|       - |  191 | ` */` |
|     106 |  192 | `PH7_PRIVATE int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  193 | `{` |
|       - |  194 | `	ph7_hashmap *pMap;` |
|       - |  195 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|     110 |  196 | `	if( nArg != 1 ){` |
|     ! 0 |  197 | `		return PH7_VmThrowException(pCtx,` |
|       - |  198 | `			"ArgumentCountError",` |
|       - |  199 | `			"array_pop() expects exactly 1 argument, %d given",` |
|     ! 0 |  200 | `			nArg` |
|       - |  201 | `			);` |
|       - |  202 | `	}` |
|       - |  203 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - |  204 | `	 * error message as official PHP. Check the index to detect constants. */` |
|     110 |  205 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       6 |  206 | `		return PH7_VmThrowException(pCtx,` |
|       - |  207 | `			"Error",` |
|       - |  208 | `			"array_pop(): Argument #1 ($array) could not be passed by reference"` |
|       - |  209 | `			);` |
|       - |  210 | `	}` |
|       - |  211 | `	/* Make sure we are dealing with a valid hashmap */` |
|     104 |  212 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 |  213 | `		return PH7_VmThrowException(pCtx,` |
|       - |  214 | `			"TypeError",` |
|       - |  215 | `			"array_pop(): Argument #1 ($array) must be of type array, %s given",` |
|       1 |  216 | `			ph7_type_name(apArg[0])` |
|       - |  217 | `			);` |
|       - |  218 | `	}` |
|     101 |  219 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|     101 |  220 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     101 |  221 | `	if( pMap->nEntry < 1 ){` |
|       - |  222 | `		/* Nothing to pop,return NULL */` |
|       3 |  223 | `		ph7_result_null(pCtx);` |
|       2 |  224 | `	}else{` |
|      99 |  225 | `		ph7_hashmap_node *pLast = pMap->pLast;` |
|       - |  226 | `		ph7_value *pObj;` |
|      99 |  227 | `		pObj = HashmapExtractNodeValue(pLast);` |
|      99 |  228 | `		if( pObj ){` |
|       - |  229 | `			/* Node value */` |
|      99 |  230 | `			ph7_result_value(pCtx,pObj);` |
|       - |  231 | `			/* Unlink the node */` |
|      99 |  232 | `			PH7_HashmapUnlinkNode(pLast,TRUE);` |
|      50 |  233 | `		}else{` |
|     ! 0 |  234 | `			ph7_result_null(pCtx);` |
|       - |  235 | `		}` |
|       - |  236 | `		/* Reset the cursor */` |
|      99 |  237 | `		pMap->pCur = pMap->pFirst;` |
|       - |  238 | `	}` |
|     101 |  239 | `	return PH7_OK;` |
|      57 |  240 | `}` |
|       - |  241 | `/*` |
|       - |  242 | ` * int array_push($array,$var,...)` |
|       - |  243 | ` *   Push one or more elements onto the end of array. (Stack insertion)` |
|       - |  244 | ` * Parameters` |
|       - |  245 | ` *  array` |
|       - |  246 | ` *    The input array.` |
|       - |  247 | ` *  var` |
|       - |  248 | ` *   On or more value to push.` |
|       - |  249 | ` * Return` |
|       - |  250 | ` *  New array count (including old items).` |
|       - |  251 | ` */` |
|      22 |  252 | `PH7_PRIVATE int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 |  253 | `{` |
|       - |  254 | `	ph7_hashmap *pMap;` |
|       - |  255 | `	sxi32 rc;` |
|       - |  256 | `	int i;` |
|      26 |  257 | `	if( nArg < 1 ){` |
|     ! 0 |  258 | `		return PH7_VmThrowException(pCtx,` |
|       - |  259 | `			"ArgumentCountError",` |
|       - |  260 | `			"array_push() expects at least 1 argument, %d given",` |
|     ! 0 |  261 | `			nArg` |
|       - |  262 | `			);` |
|       - |  263 | `	}` |
|       - |  264 | `	/* Passing a constant (including literals) or non-variable triggers the same` |
|       - |  265 | `	 * error message as official PHP. Check the index to detect constants. */` |
|      26 |  266 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       6 |  267 | `		return PH7_VmThrowException(pCtx,` |
|       - |  268 | `			"Error",` |
|       - |  269 | `			"array_push(): Argument #1 ($array) could not be passed by reference"` |
|       - |  270 | `			);` |
|       - |  271 | `	}` |
|       - |  272 | `	/* Make sure we are dealing with a valid hashmap */` |
|      21 |  273 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 |  274 | `		return PH7_VmThrowException(pCtx,` |
|       - |  275 | `			"TypeError",` |
|       - |  276 | `			"array_push(): Argument #1 ($array) must be of type array, %s given",` |
|       1 |  277 | `			ph7_type_name(apArg[0])` |
|       - |  278 | `			);` |
|       - |  279 | `	}` |
|       - |  280 | `	/* Point to the internal representation of the input hashmap */` |
|      18 |  281 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      18 |  282 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - |  283 | `	/* Start pushing given values */` |
|      34 |  284 | `	for( i = 1 ; i < nArg ; ++i ){` |
|      20 |  285 | `		rc = PH7_HashmapInsert(pMap,0,apArg[i]);` |
|      20 |  286 | `		if( rc != SXRET_OK ){` |
|       3 |  287 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|       - |  288 | `				/* Saturated-append Error (php: array_push throws, no result) */` |
|       3 |  289 | `				return rc;` |
|       - |  290 | `			}` |
|     ! 0 |  291 | `			break;` |
|       - |  292 | `		}` |
|       9 |  293 | `	}` |
|       - |  294 | `	/* Return the new count */` |
|      15 |  295 | `	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);` |
|      15 |  296 | `	return PH7_OK;` |
|      15 |  297 | `}` |
|       - |  298 | `/*` |
|       - |  299 | ` * value array_shift(array $array)` |
|       - |  300 | ` *   Shift an element off the beginning of array.` |
|       - |  301 | ` * Parameter` |
|       - |  302 | ` *  The array to get the value from.` |
|       - |  303 | ` * Return` |
|       - |  304 | ` *  Shifted value or NULL on failure.` |
|       - |  305 | ` */` |
|      42 |  306 | `PH7_PRIVATE int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |  307 | `{` |
|       - |  308 | `	ph7_hashmap *pMap;` |
|       - |  309 | `	/* PHP requires exactly one argument and it must be passed by reference */` |
|      47 |  310 | `	if( nArg != 1 ){` |
|     ! 0 |  311 | `		return PH7_VmThrowException(pCtx,` |
|       - |  312 | `			"ArgumentCountError",` |
|       - |  313 | `			"array_shift() expects exactly 1 argument, %d given",` |
|     ! 0 |  314 | `			nArg` |
|       - |  315 | `			);` |
|       - |  316 | `	}` |
|       - |  317 | `	/* Detect constants or literals, which cannot be passed by reference. */` |
|      47 |  318 | `	if( apArg[0]->nIdx == SXU32_HIGH ){` |
|       6 |  319 | `		return PH7_VmThrowException(pCtx,` |
|       - |  320 | `			"Error",` |
|       - |  321 | `			"array_shift(): Argument #1 ($array) could not be passed by reference"` |
|       - |  322 | `			);` |
|       - |  323 | `	}` |
|       - |  324 | `	/* Make sure we are dealing with a valid hashmap */` |
|      43 |  325 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 |  326 | `		return PH7_VmThrowException(pCtx,` |
|       - |  327 | `			"TypeError",` |
|       - |  328 | `			"array_shift(): Argument #1 ($array) must be of type array, %s given",` |
|       1 |  329 | `			ph7_type_name(apArg[0])` |
|       - |  330 | `			);` |
|       - |  331 | `	}` |
|       - |  332 | `	/* Point to the internal representation of the hashmap */` |
|      41 |  333 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      41 |  334 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      41 |  335 | `	if( pMap->nEntry < 1 ){` |
|       - |  336 | `		/* Empty hashmap,return NULL */` |
|       3 |  337 | `		ph7_result_null(pCtx);` |
|       2 |  338 | `	}else{` |
|      39 |  339 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - |  340 | `		ph7_value *pObj;` |
|       - |  341 | `		sxu32 n;` |
|      39 |  342 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|      39 |  343 | `		if( pObj ){` |
|       - |  344 | `			/* Node value */` |
|      39 |  345 | `			ph7_result_value(pCtx,pObj);` |
|       - |  346 | `			/* Unlink the first node */` |
|      39 |  347 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|      22 |  348 | `		}else{` |
|     ! 0 |  349 | `			ph7_result_null(pCtx);` |
|       - |  350 | `		}` |
|       - |  351 | `		/* Rehash all int keys */` |
|      39 |  352 | `		n = pMap->nEntry;` |
|      39 |  353 | `		pEntry = pMap->pFirst;` |
|      39 |  354 | `		pMap->iNextIdx = 0;` |
|      39 |  355 | `	pMap->bIntKeySeen = 0; /* Reset the automatic index */` |
|      47 |  356 | `		for(;;){` |
|      99 |  357 | `			if( n < 1 ){` |
|      39 |  358 | `				break;` |
|       - |  359 | `			}` |
|      65 |  360 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      65 |  361 | `				HashmapRehashIntNode(pEntry);` |
|      30 |  362 | `			}` |
|       - |  363 | `			/* Point to the next entry */` |
|      65 |  364 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|      65 |  365 | `			n--;` |
|       5 |  366 | `		}` |
|       - |  367 | `		/* Reset the cursor */` |
|      39 |  368 | `		pMap->pCur = pMap->pFirst;` |
|       - |  369 | `	}` |
|      41 |  370 | `	return PH7_OK;` |
|      26 |  371 | `}` |
|       - |  372 | `/*` |
|       - |  373 | ` * Extract the node cursor value.` |
|       - |  374 | ` */` |
|    1242 |  375 | `static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)` |
|       3 |  376 | `{` |
|    1245 |  377 | `	ph7_hashmap_node *pCur = pMap->pCur;` |
|       - |  378 | `	ph7_value *pVal;` |
|    1245 |  379 | `	if( pCur == 0 ){` |
|       - |  380 | `		/* Cursor does not point to anything,return FALSE */` |
|      42 |  381 | `		ph7_result_bool(pCtx,0);` |
|      42 |  382 | `		return PH7_OK;` |
|       - |  383 | `	}` |
|    1205 |  384 | `	if( iDirection != 0 ){` |
|     227 |  385 | `		if( iDirection > 0 ){` |
|       - |  386 | `			/* Point to the next entry */` |
|     225 |  387 | `			pMap->pCur = pCur->pPrev; /* Reverse link */` |
|     225 |  388 | `			pCur = pMap->pCur;` |
|     113 |  389 | `		}else{` |
|       - |  390 | `			/* Point to the previous entry */` |
|       3 |  391 | `			pMap->pCur = pCur->pNext; /* Reverse link */` |
|       3 |  392 | `			pCur = pMap->pCur;` |
|       - |  393 | `		}` |
|     227 |  394 | `		if( pCur == 0 ){` |
|       - |  395 | `			/* End of input reached,return FALSE */` |
|      91 |  396 | `			ph7_result_bool(pCtx,0);` |
|      91 |  397 | `			return PH7_OK;` |
|       - |  398 | `		}` |
|      68 |  399 | `	}` |
|       - |  400 | `	/* Point to the desired element */` |
|    1115 |  401 | `	pVal = HashmapExtractNodeValue(pCur);` |
|    1115 |  402 | `	if( pVal ){` |
|    1115 |  403 | `		ph7_result_value(pCtx,pVal);` |
|     559 |  404 | `	}else{` |
|     ! 0 |  405 | `		ph7_result_bool(pCtx,0);` |
|       - |  406 | `	}` |
|    1115 |  407 | `	return PH7_OK;` |
|     624 |  408 | `}` |
|       - |  409 | `/*` |
|       - |  410 | ` * value current(array $array)` |
|       - |  411 | ` *  Return the current element in an array.` |
|       - |  412 | ` * Parameter` |
|       - |  413 | ` *  $input: The input array.` |
|       - |  414 | ` * Return` |
|       - |  415 | ` *  The current() function simply returns the value of the array element that's currently` |
|       - |  416 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - |  417 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - |  418 | ` *  is empty, current() returns FALSE.` |
|       - |  419 | ` */` |
|     358 |  420 | `PH7_PRIVATE int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  421 | `{` |
|     360 |  422 | `	if( nArg < 1 ){` |
|       - |  423 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  424 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  425 | `		return PH7_OK;` |
|       - |  426 | `	}` |
|       - |  427 | `	/* Make sure we are dealing with a valid hashmap */` |
|     360 |  428 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  429 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  430 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  431 | `		return PH7_OK;` |
|       - |  432 | `	}` |
|     360 |  433 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);` |
|     360 |  434 | `	return PH7_OK;` |
|     181 |  435 | `}` |
|       - |  436 | `/*` |
|       - |  437 | ` * value next(array $input)` |
|       - |  438 | ` *  Advance the internal array pointer of an array.` |
|       - |  439 | ` * Parameter` |
|       - |  440 | ` *  $input: The input array.` |
|       - |  441 | ` * Return` |
|       - |  442 | ` *  next() behaves like current(), with one difference. It advances the internal array` |
|       - |  443 | ` *  pointer one place forward before returning the element value. That means it returns` |
|       - |  444 | ` *  the next array value and advances the internal array pointer by one.` |
|       - |  445 | ` */` |
|     224 |  446 | `PH7_PRIVATE int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  447 | `{` |
|     225 |  448 | `	if( nArg < 1 ){` |
|       - |  449 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  450 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  451 | `		return PH7_OK;` |
|       - |  452 | `	}` |
|       - |  453 | `	/* Make sure we are dealing with a valid hashmap */` |
|     225 |  454 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  455 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  456 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  457 | `		return PH7_OK;` |
|       - |  458 | `	}` |
|     225 |  459 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);` |
|     225 |  460 | `	return PH7_OK;` |
|     113 |  461 | `}` |
|       - |  462 | `/*` |
|       - |  463 | ` * value prev(array $input)` |
|       - |  464 | ` *  Rewind the internal array pointer.` |
|       - |  465 | ` * Parameter` |
|       - |  466 | ` *  $input: The input array.` |
|       - |  467 | ` * Return` |
|       - |  468 | ` *  Returns the array value in the previous place that's pointed` |
|       - |  469 | ` *  to by the internal array pointer, or FALSE if there are no more` |
|       - |  470 | ` *  elements.` |
|       - |  471 | ` */` |
|       2 |  472 | `PH7_PRIVATE int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  473 | `{` |
|       3 |  474 | `	if( nArg < 1 ){` |
|       - |  475 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  476 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  477 | `		return PH7_OK;` |
|       - |  478 | `	}` |
|       - |  479 | `	/* Make sure we are dealing with a valid hashmap */` |
|       3 |  480 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  481 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  482 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  483 | `		return PH7_OK;` |
|       - |  484 | `	}` |
|       3 |  485 | `	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);` |
|       3 |  486 | `	return PH7_OK;` |
|       2 |  487 | `}` |
|       - |  488 | `/*` |
|       - |  489 | ` * value end(array $input)` |
|       - |  490 | ` *  Set the internal pointer of an array to its last element.` |
|       - |  491 | ` * Parameter` |
|       - |  492 | ` *  $input: The input array.` |
|       - |  493 | ` * Return` |
|       - |  494 | ` *  Returns the value of the last element or FALSE for empty array.` |
|       - |  495 | ` */` |
|     390 |  496 | `PH7_PRIVATE int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  497 | `{` |
|       - |  498 | `	ph7_hashmap *pMap;` |
|     391 |  499 | `	if( nArg < 1 ){` |
|       - |  500 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  501 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  502 | `		return PH7_OK;` |
|       - |  503 | `	}` |
|       - |  504 | `	/* Make sure we are dealing with a valid hashmap */` |
|     391 |  505 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  506 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  507 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  508 | `		return PH7_OK;` |
|       - |  509 | `	}` |
|       - |  510 | `	/* Point to the internal representation of the input hashmap */` |
|     391 |  511 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - |  512 | `	/* Point to the last node */` |
|     391 |  513 | `	pMap->pCur = pMap->pLast;` |
|       - |  514 | `	/* Return the last node value */` |
|     391 |  515 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|     391 |  516 | `	return PH7_OK;` |
|     196 |  517 | `}` |
|       - |  518 | `/*` |
|       - |  519 | ` * value reset(array $array )` |
|       - |  520 | ` *  Set the internal pointer of an array to its first element.` |
|       - |  521 | ` * Parameter` |
|       - |  522 | ` *  $input: The input array.` |
|       - |  523 | ` * Return` |
|       - |  524 | ` *  Returns the value of the first array element,or FALSE if the array is empty.` |
|       - |  525 | ` */` |
|     268 |  526 | `PH7_PRIVATE int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  527 | `{` |
|       - |  528 | `	ph7_hashmap *pMap;` |
|     271 |  529 | `	if( nArg < 1 ){` |
|       - |  530 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  531 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  532 | `		return PH7_OK;` |
|       - |  533 | `	}` |
|       - |  534 | `	/* Make sure we are dealing with a valid hashmap */` |
|     271 |  535 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  536 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  537 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  538 | `		return PH7_OK;` |
|       - |  539 | `	}` |
|       - |  540 | `	/* Point to the internal representation of the input hashmap */` |
|     271 |  541 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - |  542 | `	/* Point to the first node */` |
|     271 |  543 | `	pMap->pCur = pMap->pFirst;` |
|       - |  544 | `	/* Return the last node value if available */` |
|     271 |  545 | `	HashmapCurrentValue(&(*pCtx),pMap,0);` |
|     271 |  546 | `	return PH7_OK;` |
|     137 |  547 | `}` |
|       - |  548 | `/*` |
|       - |  549 | ` * Emit a node's key (integer or blob) as the call result — shared by key(),` |
|       - |  550 | ` * array_key_first() and array_key_last().` |
|       - |  551 | ` */` |
|     776 |  552 | `static void HashmapResultNodeKey(ph7_context *pCtx,ph7_hashmap_node *pNode)` |
|       2 |  553 | `{` |
|     778 |  554 | `	if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - |  555 | `		/* Key is integer */` |
|     388 |  556 | `		ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|     195 |  557 | `	}else{` |
|       - |  558 | `		/* Key is blob */` |
|     586 |  559 | `		ph7_result_string(pCtx,` |
|     390 |  560 | `			(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - |  561 | `	}` |
|     778 |  562 | `}` |
|       - |  563 | `/*` |
|       - |  564 | ` * value key(array $array)` |
|       - |  565 | ` *   Fetch a key from an array` |
|       - |  566 | ` * Parameter` |
|       - |  567 | ` *  $input` |
|       - |  568 | ` *   The input array.` |
|       - |  569 | ` * Return` |
|       - |  570 | ` *  The key() function simply returns the key of the array element that's currently` |
|       - |  571 | ` *  being pointed to by the internal pointer. It does not move the pointer in any way.` |
|       - |  572 | ` *  If the internal pointer points beyond the end of the elements list or the array` |
|       - |  573 | ` *  is empty, key() returns NULL.` |
|       - |  574 | ` */` |
|     896 |  575 | `PH7_PRIVATE int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  576 | `{` |
|       - |  577 | `	ph7_hashmap_node *pCur;` |
|       - |  578 | `	ph7_hashmap *pMap;` |
|     898 |  579 | `	if( nArg < 1 ){` |
|       - |  580 | `		/* Missing arguments,return NULL */` |
|     ! 0 |  581 | `		ph7_result_null(pCtx);` |
|     ! 0 |  582 | `		return PH7_OK;` |
|       - |  583 | `	}` |
|       - |  584 | `	/* Make sure we are dealing with a valid hashmap */` |
|     898 |  585 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  586 | `		/* Invalid argument,return NULL */` |
|     ! 0 |  587 | `		ph7_result_null(pCtx);` |
|     ! 0 |  588 | `		return PH7_OK;` |
|       - |  589 | `	}` |
|     898 |  590 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     898 |  591 | `	pCur = pMap->pCur;` |
|     898 |  592 | `	if( pCur == 0 ){` |
|       - |  593 | `		/* Cursor does not point to anything,return NULL */` |
|     137 |  594 | `		ph7_result_null(pCtx);` |
|     137 |  595 | `		return PH7_OK;` |
|       - |  596 | `	}` |
|     762 |  597 | `	HashmapResultNodeKey(pCtx,pCur);` |
|     762 |  598 | `	return PH7_OK;` |
|     450 |  599 | `}` |
|       - |  600 | `/*` |
|       - |  601 | ` * array each(array $input)` |
|       - |  602 | ` *  Return the current key and value pair from an array and advance the array cursor.` |
|       - |  603 | ` * Parameter` |
|       - |  604 | ` *  $input` |
|       - |  605 | ` *    The input array.` |
|       - |  606 | ` * Return` |
|       - |  607 | ` *  Returns the current key and value pair from the array array. This pair is returned` |
|       - |  608 | ` *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key` |
|       - |  609 | ` *  contain the key name of the array element, and 1 and value contain the data.` |
|       - |  610 | ` *  If the internal pointer for the array points past the end of the array contents` |
|       - |  611 | ` *  each() returns FALSE.` |
|       - |  612 | ` */` |
|      22 |  613 | `PH7_PRIVATE int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  614 | `{` |
|       - |  615 | `	ph7_hashmap_node *pCur;` |
|       - |  616 | `	ph7_hashmap *pMap;` |
|       - |  617 | `	ph7_value *pArray;` |
|       - |  618 | `	ph7_value *pVal;` |
|       - |  619 | `	ph7_value sKey;` |
|      23 |  620 | `	if( nArg < 1 ){` |
|       - |  621 | `		/* Missing arguments,return FALSE */` |
|     ! 0 |  622 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  623 | `		return PH7_OK;` |
|       - |  624 | `	}` |
|       - |  625 | `	/* Make sure we are dealing with a valid hashmap */` |
|      23 |  626 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - |  627 | `		/* Invalid argument,return FALSE */` |
|     ! 0 |  628 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  629 | `		return PH7_OK;` |
|       - |  630 | `	}` |
|       - |  631 | `	/* Point to the internal representation that describe the input hashmap */` |
|      23 |  632 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 |  633 | `	if( pMap->pCur == 0 ){` |
|       - |  634 | `		/* Cursor does not point to anything,return FALSE */` |
|       9 |  635 | `		ph7_result_bool(pCtx,0);` |
|       9 |  636 | `		return PH7_OK;` |
|       - |  637 | `	}` |
|      15 |  638 | `	pCur = pMap->pCur;` |
|       - |  639 | `	/* Create a new array */` |
|      15 |  640 | `	pArray = ph7_context_new_array(pCtx);` |
|      15 |  641 | `	if( pArray == 0 ){` |
|     ! 0 |  642 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 |  643 | `		return PH7_OK;` |
|       - |  644 | `	}` |
|      15 |  645 | `	pVal = HashmapExtractNodeValue(pCur);` |
|       - |  646 | `	/* Insert the current value */` |
|      15 |  647 | `	ph7_array_add_intkey_elem(pArray,1,pVal);` |
|      15 |  648 | `	ph7_array_add_strkey_elem(pArray,"value",pVal);` |
|       - |  649 | `	/* Make the key */` |
|      15 |  650 | `	if( pCur->iType == HASHMAP_INT_NODE ){` |
|       7 |  651 | `		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);` |
|       4 |  652 | `	}else{` |
|       9 |  653 | `		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);` |
|       9 |  654 | `		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));` |
|       - |  655 | `	}` |
|       - |  656 | `	/* Insert the current key */` |
|      15 |  657 | `	ph7_array_add_intkey_elem(pArray,0,&sKey);` |
|      15 |  658 | `	ph7_array_add_strkey_elem(pArray,"key",&sKey);` |
|      15 |  659 | `	PH7_MemObjRelease(&sKey);` |
|       - |  660 | `	/* Advance the cursor */` |
|      15 |  661 | `	pMap->pCur = pCur->pPrev; /* Reverse link */` |
|       - |  662 | `	/* Return the current entry */` |
|      15 |  663 | `	ph7_result_value(pCtx,pArray);` |
|      15 |  664 | `	return PH7_OK;` |
|      12 |  665 | `}` |
|       - |  666 | `/*` |
|       - |  667 | ` * range() — a faithful port of php 8.5's ext/standard/array.c implementation` |
|       - |  668 | ` * (php_range_process_input + PHP_FUNCTION(range)), so the value semantics,` |
|       - |  669 | ` * diagnostics, and their ordering are byte-exact: decreasing ranges, float` |
|       - |  670 | ` * ranges, character ranges, the step/endpoint ValueErrors, the ZPP TypeErrors` |
|       - |  671 | ` * and null deprecations, and the string-endpoint warnings.` |
|       - |  672 | ` */` |
|       - |  673 | `#define PH7_RANGE_HT_MAX_SIZE 1073741824 /* php's HT_MAX_SIZE (2^30 entries) */` |
|       - |  674 | `/*` |
|       - |  675 | ` * Endpoint classification, mirroring php_range_process_input's return` |
|       - |  676 | ` * contract. php returns zval type tags whose ORDER encodes the logic` |
|       - |  677 | ` * (IS_LONG < IS_DOUBLE < IS_STRING < IS_ARRAY); the >=/< comparisons in` |
|       - |  678 | ` * ph7_hashmap_range depend on the same ordering here.` |
|       - |  679 | ` *   RANGE_IN_LONG/DOUBLE : only interpretable as int / float` |
|       - |  680 | ` *   RANGE_IN_STRING      : only interpretable as a (char-range) string` |
|       - |  681 | ` *   RANGE_IN_DIGIT       : single-byte numeric string — valid as both a char` |
|       - |  682 | ` *                          and a number (php returns IS_ARRAY for this)` |
|       - |  683 | ` * The RANGE_IN_* codes and RangeStrToNumber are declared in ph7int.h so the` |
|       - |  684 | ` * stage-2 ZPP domain-error sweep can reuse the classifier (PLAN §3.9(a)).` |
|       - |  685 | ` */` |
|       - |  686 | `/* IEEE special-value tests: the engine-wide bit-pattern macros from` |
|       - |  687 | ` * sxtypes.h (via ph7int.h) — same ones the printf/serialize paths use. */` |
|       - |  688 | `/*` |
|       - |  689 | ` * The type name php's ZPP prints after "must be of type ..., X given":` |
|       - |  690 | ` * the concrete class name for objects, the usual type name otherwise.` |
|       - |  691 | ` */` |
|     ! 0 |  692 | `static const char * RangeArgTypeName(ph7_value *pVal,char *zBuf,sxu32 nBufLen)` |
|     ! 0 |  693 | `{` |
|     ! 0 |  694 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 |  695 | `		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;` |
|     ! 0 |  696 | `		sxu32 n = SXMIN(pThis->pClass->sName.nByte,nBufLen - 1);` |
|     ! 0 |  697 | `		SyMemcpy((const void *)pThis->pClass->sName.zString,zBuf,n);` |
|     ! 0 |  698 | `		zBuf[n] = 0;` |
|     ! 0 |  699 | `		return zBuf;` |
|       - |  700 | `	}` |
|     ! 0 |  701 | `	return ph7_type_name(pVal);` |
|     ! 0 |  702 | `}` |
|       - |  703 | `/*` |
|       - |  704 | ` * Classify a string with php's is_numeric_string() grammar:` |
|       - |  705 | ` *   [ws] [sign] ( D+ [ . D* ] \| . D+ ) [ (e\|E) [sign] D+ ] [ws]` |
|       - |  706 | ` * — the whole string must be consumed; hex/binary/"INF"/"NAN" are NOT` |
|       - |  707 | ` * numeric. Returns RANGE_IN_LONG with *pLong set, RANGE_IN_DOUBLE with` |
|       - |  708 | ` * *pDouble set (a fractional/exponent form, or an integer too wide for an` |
|       - |  709 | ` * sxi64 — php reclassifies those as float), or RANGE_IN_ERROR when the` |
|       - |  710 | ` * string is not numeric. The float value comes from libc strtod, like` |
|       - |  711 | ` * php's zend_strtod (byte-exact-floats rule). zIn must be NUL-terminated` |
|       - |  712 | ` * at zIn[nLen] — ph7_value_to_string guarantees this (SyBlobNullAppend) —` |
|       - |  713 | ` * so strtod can parse it in place once the grammar has validated it.` |
|       - |  714 | ` */` |
|     110 |  715 | `PH7_PRIVATE sxu8 RangeStrToNumber(const char *zIn,sxu32 nLen,sxi64 *pLong,double *pDouble)` |
|       1 |  716 | `{` |
|     111 |  717 | `	const char *z = zIn,*zEnd = &zIn[nLen];` |
|     111 |  718 | `	sxu64 uVal = 0;` |
|     111 |  719 | `	int bNeg = 0,bDigit = 0,bReal = 0,bOverflow = 0;` |
|     121 |  720 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|     111 |  721 | `	if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){` |
|       3 |  722 | `		bNeg = (z[0] == '-');` |
|       3 |  723 | `		z++;` |
|       1 |  724 | `	}` |
|     173 |  725 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|      63 |  726 | `		int d = z[0] - '0';` |
|       - |  727 | `		/* Track overflow past 2^63, the widest magnitude an sxi64 can carry` |
|       - |  728 | `		 * (as LONG_MIN); overflowing integers become floats like in php. */` |
|      63 |  729 | `		if( uVal > 922337203685477580ULL \|\| (uVal == 922337203685477580ULL && d > 8) ){` |
|     ! 0 |  730 | `			bOverflow = 1;` |
|     ! 0 |  731 | `		}else{` |
|      63 |  732 | `			uVal = uVal * 10 + (sxu64)d;` |
|       - |  733 | `		}` |
|      63 |  734 | `		bDigit = 1;` |
|      63 |  735 | `		z++;` |
|       1 |  736 | `	}` |
|     111 |  737 | `	if( z < zEnd && z[0] == '.' ){` |
|       3 |  738 | `		bReal = 1;` |
|       3 |  739 | `		z++;` |
|       5 |  740 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){` |
|       3 |  741 | `			bDigit = 1;` |
|       3 |  742 | `			z++;` |
|       1 |  743 | `		}` |
|       1 |  744 | `	}` |
|       - |  745 | `	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */` |
|     111 |  746 | `	if( !bDigit ){` |
|      37 |  747 | `		return RANGE_IN_ERROR;` |
|       - |  748 | `	}` |
|       - |  749 | `	/* Optional exponent — needs at least one digit (rejects "1e", "1e+"). */` |
|      75 |  750 | `	if( z < zEnd && (z[0] == 'e' \|\| z[0] == 'E') ){` |
|       9 |  751 | `		z++;` |
|       9 |  752 | `		if( z < zEnd && (z[0] == '+' \|\| z[0] == '-') ){ z++; }` |
|       9 |  753 | `		if( z >= zEnd \|\| (unsigned char)z[0] >= 0xc0 \|\| !SyisDigit(z[0]) ){` |
|     ! 0 |  754 | `			return RANGE_IN_ERROR;` |
|       - |  755 | `		}` |
|       9 |  756 | `		bReal = 1;` |
|      17 |  757 | `		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){ z++; }` |
|       4 |  758 | `	}` |
|       - |  759 | `	/* Trailing whitespace allowed; anything else means not numeric. */` |
|      79 |  760 | `	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }` |
|      75 |  761 | `	if( z != zEnd ){` |
|      11 |  762 | `		return RANGE_IN_ERROR;` |
|       - |  763 | `	}` |
|      64 |  764 | `	if( bOverflow \|\| (!bNeg && uVal > (sxu64)LARGEST_INT64)` |
|      33 |  765 | `	 \|\| (bNeg && uVal > (sxu64)LARGEST_INT64 + 1) ){` |
|      64 |  766 | `		bReal = 1;` |
|      64 |  767 | `	}` |
|      33 |  768 | `	if( bReal ){` |
|      11 |  769 | `		*pDouble = strtod(zIn,0);` |
|      11 |  770 | `		return RANGE_IN_DOUBLE;` |
|       - |  771 | `	}` |
|       - |  772 | `	/* Negate in unsigned space so 2^63 lands on LONG_MIN without overflow. */` |
|      23 |  773 | `	*pLong = bNeg ? (sxi64)((sxu64)0 - uVal) : (sxi64)uVal;` |
|      23 |  774 | `	return RANGE_IN_LONG;` |
|      40 |  775 | `}` |
|       - |  776 | `/*` |
|       - |  777 | ` * ZPP emulation for $start/$end (php's Z_PARAM_NUMBER_OR_STR, weak mode):` |
|       - |  778 | ` * reject array/object/resource with php's TypeError, deprecate null (the` |
|       - |  779 | ` * value then reads as int 0 — *pbNullCoerced). php runs this for all` |
|       - |  780 | ` * arguments BEFORE any value/domain check, hence the split from` |
|       - |  781 | ` * RangeProcessInput below. Returns FALSE after throwing (*pRc set).` |
|       - |  782 | ` */` |
|     276 |  783 | `static int RangeEndpointZpp(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,int *pbNullCoerced,sxi32 *pRc)` |
|       2 |  784 | `{` |
|     138 |  785 | `	SXUNUSED(pbNullCoerced); /* php coerces null to 0 with a deprecation; PHL rejects it */` |
|     278 |  786 | `	*pRc = PH7_OK;` |
|     278 |  787 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|       - |  788 | `		char zType[80];` |
|     ! 0 |  789 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  790 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, %s given",` |
|     ! 0 |  791 | `			iArg,zName,RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|     ! 0 |  792 | `		return FALSE;` |
|       - |  793 | `	}` |
|     278 |  794 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|       - |  795 | `		/* php only DEPRECATES null here; PHL rejects it. */` |
|     ! 0 |  796 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  797 | `			"range(): Argument #%d ($%s) must be of type string\|int\|float, null given",` |
|     ! 0 |  798 | `			iArg,zName);` |
|     ! 0 |  799 | `		return FALSE;` |
|       - |  800 | `	}` |
|     278 |  801 | `	return TRUE;` |
|     140 |  802 | `}` |
|       - |  803 | `/*` |
|       - |  804 | ` * ZPP emulation for $step (php's Z_PARAM_NUMBER, weak mode): int/float pass` |
|       - |  805 | ` * through, bool coerces to int, null deprecates to int 0 (which then trips` |
|       - |  806 | ` * the "cannot be 0" ValueError like php), a numeric string coerces to its` |
|       - |  807 | ` * number, anything else is a TypeError. Returns RANGE_IN_LONG/DOUBLE, or` |
|       - |  808 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|       - |  809 | ` */` |
|      54 |  810 | `static sxu8 RangeStepInput(ph7_context *pCtx,ph7_value *pIn,sxi64 *pLong,double *pDouble,sxi32 *pRc)` |
|       1 |  811 | `{` |
|      55 |  812 | `	*pRc = PH7_OK;` |
|      55 |  813 | `	if( pIn->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_RES) ){` |
|       - |  814 | `		char zType[80];` |
|     ! 0 |  815 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  816 | `			"range(): Argument #3 ($step) must be of type int\|float, %s given",` |
|     ! 0 |  817 | `			RangeArgTypeName(pIn,zType,sizeof(zType)));` |
|     ! 0 |  818 | `		return RANGE_IN_ERROR;` |
|       - |  819 | `	}` |
|      55 |  820 | `	if( pIn->iFlags & MEMOBJ_NULL ){` |
|       - |  821 | `		/* php only DEPRECATES null here; PHL rejects it. */` |
|     ! 0 |  822 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  823 | `			"range(): Argument #3 ($step) must be of type int\|float, null given");` |
|     ! 0 |  824 | `		return RANGE_IN_ERROR;` |
|       - |  825 | `	}` |
|      55 |  826 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|      21 |  827 | `		*pDouble = ph7_value_to_double(pIn);` |
|      21 |  828 | `		return RANGE_IN_DOUBLE;` |
|       - |  829 | `	}` |
|      35 |  830 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|       - |  831 | `		const char *zStr;` |
|       - |  832 | `		int nLen;` |
|       - |  833 | `		sxu8 iKind;` |
|       3 |  834 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|       3 |  835 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|       3 |  836 | `		if( iKind == RANGE_IN_ERROR ){` |
|       3 |  837 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  838 | `				"range(): Argument #3 ($step) must be of type int\|float, string given");` |
|       1 |  839 | `		}` |
|       3 |  840 | `		return iKind;` |
|       - |  841 | `	}` |
|       - |  842 | `	/* int / bool */` |
|      33 |  843 | `	*pLong = ph7_value_to_int64(pIn);` |
|      33 |  844 | `	return RANGE_IN_LONG;` |
|      28 |  845 | `}` |
|       - |  846 | `/*` |
|       - |  847 | ` * php_range_process_input port: resolve $start/$end into a number and/or a` |
|       - |  848 | ` * char-range byte, emitting php's exact warnings (empty string, multi-byte` |
|       - |  849 | ` * string) and ValueErrors (INF/NAN). Returns a RANGE_IN_* code, or` |
|       - |  850 | ` * RANGE_IN_ERROR after throwing (*pRc set).` |
|       - |  851 | ` */` |
|     248 |  852 | `static sxu8 RangeProcessInput(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,` |
|       - |  853 | `	int bNullCoerced,sxi64 *pLong,double *pDouble,unsigned char *pChar,sxi32 *pRc)` |
|       2 |  854 | `{` |
|       - |  855 | `	char zMsg[160];` |
|       - |  856 | `	double r;` |
|     250 |  857 | `	*pRc = PH7_OK;` |
|     250 |  858 | `	if( bNullCoerced ){` |
|       - |  859 | `		/* ZPP already deprecated the null; it reads as int 0. */` |
|     ! 0 |  860 | `		*pLong = 0;` |
|     ! 0 |  861 | `		*pDouble = 0.0;` |
|     ! 0 |  862 | `		return RANGE_IN_LONG;` |
|       - |  863 | `	}` |
|     250 |  864 | `	if( pIn->iFlags & MEMOBJ_REAL ){` |
|      21 |  865 | `		r = ph7_value_to_double(pIn);` |
|      12 |  866 | `check_dval:` |
|      25 |  867 | `		if( PH7_IS_INF(r) ){` |
|       7 |  868 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|       2 |  869 | `				"range(): Argument #%d ($%s) must be a finite number, INF provided",iArg,zName);` |
|       5 |  870 | `			return RANGE_IN_ERROR;` |
|       - |  871 | `		}` |
|      21 |  872 | `		if( PH7_IS_NAN(r) ){` |
|       7 |  873 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|       2 |  874 | `				"range(): Argument #%d ($%s) must be a finite number, NAN provided",iArg,zName);` |
|       5 |  875 | `			return RANGE_IN_ERROR;` |
|       - |  876 | `		}` |
|      17 |  877 | `		*pDouble = r;` |
|      17 |  878 | `		return RANGE_IN_DOUBLE;` |
|       - |  879 | `	}` |
|     230 |  880 | `	if( pIn->iFlags & MEMOBJ_STRING ){` |
|       - |  881 | `		const char *zStr;` |
|       - |  882 | `		int nLen;` |
|       - |  883 | `		sxu8 iKind;` |
|      41 |  884 | `		zStr = ph7_value_to_string(pIn,&nLen);` |
|      41 |  885 | `		if( nLen == 0 ){` |
|     ! 0 |  886 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|     ! 0 |  887 | `				"range(): Argument #%d ($%s) must not be empty, casted to 0",iArg,zName);` |
|     ! 0 |  888 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|     ! 0 |  889 | `			*pLong = 0;` |
|     ! 0 |  890 | `			*pDouble = 0.0;` |
|     ! 0 |  891 | `			return RANGE_IN_LONG;` |
|       - |  892 | `		}` |
|      41 |  893 | `		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);` |
|      41 |  894 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|       5 |  895 | `			r = *pDouble;` |
|       5 |  896 | `			goto check_dval;` |
|       - |  897 | `		}` |
|      37 |  898 | `		if( iKind == RANGE_IN_LONG ){` |
|      13 |  899 | `			*pDouble = (double)*pLong;` |
|      13 |  900 | `			if( nLen == 1 ){` |
|       - |  901 | `				/* A single numeric digit works as both a char and a number. */` |
|       5 |  902 | `				*pChar = (unsigned char)zStr[0];` |
|       5 |  903 | `				return RANGE_IN_DIGIT;` |
|       - |  904 | `			}` |
|       9 |  905 | `			return RANGE_IN_LONG;` |
|       - |  906 | `		}` |
|      25 |  907 | `		if( nLen != 1 ){` |
|     ! 0 |  908 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|     ! 0 |  909 | `				"range(): Argument #%d ($%s) must be a single byte, subsequent bytes are ignored",iArg,zName);` |
|     ! 0 |  910 | `			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);` |
|     ! 0 |  911 | `		}` |
|      25 |  912 | `		*pChar = (unsigned char)zStr[0];` |
|       - |  913 | `		/* Fall-back numeric value in case the other argument is not a string. */` |
|      25 |  914 | `		*pLong = 0;` |
|      25 |  915 | `		*pDouble = 0.0;` |
|      25 |  916 | `		return RANGE_IN_STRING;` |
|       - |  917 | `	}` |
|       - |  918 | `	/* int / bool */` |
|     190 |  919 | `	*pLong = ph7_value_to_int64(pIn);` |
|     190 |  920 | `	*pDouble = (double)*pLong;` |
|     190 |  921 | `	return RANGE_IN_LONG;` |
|     126 |  922 | `}` |
|       - |  923 | `/*` |
|       - |  924 | ` * The two "supplied range exceeds the maximum array size" ValueErrors.` |
|       - |  925 | ` * Both php messages print the macro's (start,end) parameters, which its` |
|       - |  926 | ` * callers pass SWAPPED for a decreasing range — a php quirk kept for` |
|       - |  927 | ` * byte-parity (callers below pass the values to *print*). The int and` |
|       - |  928 | ` * float variants differ in wording ("Maximum size: N." vs "Max size: N")` |
|       - |  929 | ` * exactly like php's two macros.` |
|       - |  930 | ` */` |
|       6 |  931 | `static sxi32 RangeLongSizeError(ph7_context *pCtx,sxu64 nCalc,sxi64 iStart,sxi64 iEnd,sxi64 iStep)` |
|       1 |  932 | `{` |
|      10 |  933 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  934 | `		"The supplied range exceeds the maximum array size by %qu elements: "` |
|       - |  935 | `		"start=%qd, end=%qd, step=%qd. Calculated size: %qu. Maximum size: %qu.",` |
|       3 |  936 | `		nCalc - (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1),iStart,iEnd,iStep,` |
|       3 |  937 | `		nCalc,(sxu64)PH7_RANGE_HT_MAX_SIZE);` |
|       1 |  938 | `}` |
|       6 |  939 | `static sxi32 RangeDoubleSizeError(ph7_context *pCtx,double rCalc,double rStart,double rEnd,double rStep)` |
|       1 |  940 | `{` |
|       - |  941 | `	/* Four %.1f doubles can reach ~313 bytes each near DBL_MAX, so format on` |
|       - |  942 | `	 * the VM heap (auto-released with the call context) rather than parking` |
|       - |  943 | `	 * ~1.5 KB on the native stack of a small-stack embedded port. */` |
|       7 |  944 | `	const unsigned int nBuf = 1500;` |
|       7 |  945 | `	char *zMsg = (char *)ph7_context_alloc_chunk(pCtx,nBuf,FALSE,TRUE/* Auto-release */);` |
|       7 |  946 | `	if( zMsg == 0 ){` |
|     ! 0 |  947 | `		return PH7_ContextMemoryError(pCtx);` |
|       - |  948 | `	}` |
|       7 |  949 | `	snprintf(zMsg,nBuf,` |
|       - |  950 | `		"The supplied range exceeds the maximum array size by %.1f elements: "` |
|       - |  951 | `		"start=%.1f, end=%.1f, step=%.1f. Max size: 1073741824",` |
|       - |  952 | `		rCalc - (double)PH7_RANGE_HT_MAX_SIZE,rStart,rEnd,rStep);` |
|       7 |  953 | `	return PH7_VmThrowException(pCtx,"ValueError","%s",zMsg);` |
|       4 |  954 | `}` |
|       - |  955 | `/*` |
|       - |  956 | ` * Set the element container to the next range element and append it to the` |
|       - |  957 | ` * result array, surfacing allocation failure as the OOM fatal (never a` |
|       - |  958 | ` * silently-truncated array). One helper per element type so the fill loops` |
|       - |  959 | ` * below stay one line per iteration.` |
|       - |  960 | ` */` |
|  401492 |  961 | `static sxi32 RangeAppendInt(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,sxi64 iVal)` |
|       2 |  962 | `{` |
|  401494 |  963 | `	ph7_value_int64(pValue,iVal);` |
|  401494 |  964 | `	if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){` |
|     ! 0 |  965 | `		return PH7_ContextMemoryError(pCtx);` |
|       - |  966 | `	}` |
|  401494 |  967 | `	return PH7_OK;` |
|  200748 |  968 | `}` |
|      50 |  969 | `static sxi32 RangeAppendDouble(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,double rVal)` |
|       1 |  970 | `{` |
|      51 |  971 | `	ph7_value_double(pValue,rVal);` |
|      51 |  972 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|     ! 0 |  973 | `		return PH7_ContextMemoryError(pCtx);` |
|       - |  974 | `	}` |
|      51 |  975 | `	return PH7_OK;` |
|      26 |  976 | `}` |
|     148 |  977 | `static sxi32 RangeAppendChar(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,char c)` |
|       1 |  978 | `{` |
|     149 |  979 | `	ph7_value_string(pValue,&c,1);` |
|     149 |  980 | `	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){` |
|     ! 0 |  981 | `		return PH7_ContextMemoryError(pCtx);` |
|       - |  982 | `	}` |
|     149 |  983 | `	ph7_value_reset_string_cursor(pValue);` |
|     149 |  984 | `	return PH7_OK;` |
|      75 |  985 | `}` |
|       - |  986 | `/*` |
|       - |  987 | ` * array range(string\|int\|float $start,string\|int\|float $end,int\|float $step = 1)` |
|       - |  988 | ` *  Create an array containing a range of elements.` |
|       - |  989 | ` * Return` |
|       - |  990 | ` *  An array of elements from start to end, inclusive; int, float, or` |
|       - |  991 | ` *  single-character string elements depending on the inputs, like php 8.` |
|       - |  992 | ` */` |
|     138 |  993 | `PH7_PRIVATE int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  994 | `{` |
|       - |  995 | `	ph7_value *pValue,*pArray;` |
|     140 |  996 | `	sxi32 rc = PH7_OK;` |
|     140 |  997 | `	int is_step_double = 0,is_step_negative = 0;` |
|     140 |  998 | `	double step_double = 1.0;` |
|     140 |  999 | `	sxi64 step = 1;` |
|       - | 1000 | `	sxu8 start_type,end_type;` |
|     140 | 1001 | `	sxi64 start_long = 0,end_long = 0;` |
|     140 | 1002 | `	double start_double = 0.0,end_double = 0.0;` |
|     140 | 1003 | `	unsigned char cStart = 0,cEnd = 0;` |
|     140 | 1004 | `	int bStartNull = FALSE,bEndNull = FALSE;` |
|       - | 1005 | `	sxu32 i,size;` |
|       - | 1006 |  |
|       - | 1007 | `	/* php ZPP arity: at least 2 (enforced centrally, aBuiltinArity), at most 3. */` |
|     140 | 1008 | `	if( nArg > 3 ){` |
|     ! 0 | 1009 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|     ! 0 | 1010 | `			"range() expects at most 3 arguments, %d given",nArg);` |
|       - | 1011 | `	}` |
|     140 | 1012 | `	if( nArg < 2 ){` |
|       - | 1013 | `		/* Defensive only: the central arity table throws before we run. */` |
|     ! 0 | 1014 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|     ! 0 | 1015 | `			"range() expects at least 2 arguments, %d given",nArg);` |
|       - | 1016 | `	}` |
|       - | 1017 | `	/* ZPP pass in argument order: type errors and null deprecations fire` |
|       - | 1018 | `	 * before any value/domain check, like php's zend_parse_parameters. */` |
|     140 | 1019 | `	if( !RangeEndpointZpp(pCtx,apArg[0],1,"start",&bStartNull,&rc) ){` |
|     ! 0 | 1020 | `		return rc;` |
|       - | 1021 | `	}` |
|     140 | 1022 | `	if( !RangeEndpointZpp(pCtx,apArg[1],2,"end",&bEndNull,&rc) ){` |
|     ! 0 | 1023 | `		return rc;` |
|       - | 1024 | `	}` |
|     140 | 1025 | `	if( nArg > 2 ){` |
|      55 | 1026 | `		sxu8 iStepKind = RangeStepInput(pCtx,apArg[2],&step,&step_double,&rc);` |
|      55 | 1027 | `		if( iStepKind == RANGE_IN_ERROR ){` |
|       3 | 1028 | `			return rc;` |
|       - | 1029 | `		}` |
|      53 | 1030 | `		if( iStepKind == RANGE_IN_DOUBLE ){` |
|      21 | 1031 | `			if( PH7_IS_INF(step_double) ){` |
|       3 | 1032 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1033 | `					"range(): Argument #3 ($step) must be a finite number, INF provided");` |
|       - | 1034 | `			}` |
|      19 | 1035 | `			if( PH7_IS_NAN(step_double) ){` |
|       3 | 1036 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1037 | `					"range(): Argument #3 ($step) must be a finite number, NAN provided");` |
|       - | 1038 | `			}` |
|       - | 1039 | `			/* We only want positive step values. */` |
|      17 | 1040 | `			if( step_double < 0.0 ){` |
|     ! 0 | 1041 | `				is_step_negative = 1;` |
|     ! 0 | 1042 | `				step_double *= -1;` |
|     ! 0 | 1043 | `			}` |
|       - | 1044 | `			/* zend_dval_to_lval_silent + zend_is_long_compatible: an integral` |
|       - | 1045 | `			 * in-sxi64-range float step behaves as an int (char ranges accept` |
|       - | 1046 | `			 * it, int endpoints stay int); anything else is a float step. */` |
|      17 | 1047 | `			if( step_double < 9223372036854775808.0 ){` |
|      15 | 1048 | `				step = (sxi64)step_double;` |
|      15 | 1049 | `				if( (double)step != step_double ){` |
|      13 | 1050 | `					is_step_double = 1;` |
|       6 | 1051 | `				}` |
|       8 | 1052 | `			}else{` |
|       - | 1053 | ``				/* Casting out-of-range would be UB; `step` stays unread —`` |
|       - | 1054 | `				 * every reader is gated behind !is_step_double. */` |
|       3 | 1055 | `				is_step_double = 1;` |
|       - | 1056 | `			}` |
|       9 | 1057 | `		}else{` |
|       - | 1058 | `			/* We only want positive step values. */` |
|      33 | 1059 | `			if( step < 0 ){` |
|      11 | 1060 | `				if( step == SMALLEST_INT64 ){` |
|       - | 1061 | `					/* -step would overflow */` |
|       4 | 1062 | `					return PH7_VmThrowException(pCtx,"ValueError",` |
|       1 | 1063 | `						"range(): Argument #3 ($step) must be greater than %qd",step);` |
|       - | 1064 | `				}` |
|       9 | 1065 | `				is_step_negative = 1;` |
|       9 | 1066 | `				step = -step;` |
|       4 | 1067 | `			}` |
|      31 | 1068 | `			step_double = (double)step;` |
|       - | 1069 | `		}` |
|      47 | 1070 | `		if( step_double == 0.0 ){` |
|       5 | 1071 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1072 | `				"range(): Argument #3 ($step) cannot be 0");` |
|       - | 1073 | `		}` |
|      21 | 1074 | `	}` |
|     128 | 1075 | `	start_type = RangeProcessInput(pCtx,apArg[0],1,"start",bStartNull,&start_long,&start_double,&cStart,&rc);` |
|     128 | 1076 | `	if( start_type == RANGE_IN_ERROR ){` |
|       5 | 1077 | `		return rc;` |
|       - | 1078 | `	}` |
|     124 | 1079 | `	end_type = RangeProcessInput(pCtx,apArg[1],2,"end",bEndNull,&end_long,&end_double,&cEnd,&rc);` |
|     124 | 1080 | `	if( end_type == RANGE_IN_ERROR ){` |
|       5 | 1081 | `		return rc;` |
|       - | 1082 | `	}` |
|       - | 1083 | `	/* Element container + result array */` |
|     120 | 1084 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     120 | 1085 | `	pArray = ph7_context_new_array(pCtx);` |
|     120 | 1086 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|     ! 0 | 1087 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 1088 | `	}` |
|       - | 1089 | `	/* If the range is given as strings, generate an array of characters. */` |
|     120 | 1090 | `	if( start_type >= RANGE_IN_STRING \|\| end_type >= RANGE_IN_STRING ){` |
|      15 | 1091 | `		if( start_type < RANGE_IN_STRING \|\| end_type < RANGE_IN_STRING ){` |
|       - | 1092 | `			/* Only one side is a string: the char side converts to 0 (with a` |
|       - | 1093 | `			 * warning unless the numeric side is an ambiguous single digit)` |
|       - | 1094 | `			 * and the range is numeric. */` |
|     ! 0 | 1095 | `			if( start_type < RANGE_IN_STRING ){` |
|     ! 0 | 1096 | `				if( end_type != RANGE_IN_DIGIT ){` |
|     ! 0 | 1097 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 1098 | `						"range(): Argument #1 ($start) must be a single byte string if"` |
|       - | 1099 | `						" argument #2 ($end) is a single byte string, argument #2 ($end) converted to 0");` |
|     ! 0 | 1100 | `				}` |
|     ! 0 | 1101 | `				end_type = RANGE_IN_LONG;` |
|     ! 0 | 1102 | `			}else{` |
|     ! 0 | 1103 | `				if( start_type != RANGE_IN_DIGIT ){` |
|     ! 0 | 1104 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 1105 | `						"range(): Argument #2 ($end) must be a single byte string if"` |
|       - | 1106 | `						" argument #1 ($start) is a single byte string, argument #1 ($start) converted to 0");` |
|     ! 0 | 1107 | `				}` |
|     ! 0 | 1108 | `				start_type = RANGE_IN_LONG;` |
|       - | 1109 | `			}` |
|     ! 0 | 1110 | `			goto handle_numeric_inputs;` |
|       - | 1111 | `		}` |
|      15 | 1112 | `		if( is_step_double ){` |
|       - | 1113 | `			/* Only emit the warning if one of the inputs is not a numeric digit. */` |
|     ! 0 | 1114 | `			if( start_type == RANGE_IN_STRING \|\| end_type == RANGE_IN_STRING ){` |
|     ! 0 | 1115 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 1116 | `					"range(): Argument #3 ($step) must be of type int when generating an array"` |
|       - | 1117 | `					" of characters, inputs converted to 0");` |
|     ! 0 | 1118 | `			}` |
|     ! 0 | 1119 | `			start_type = RANGE_IN_LONG;` |
|     ! 0 | 1120 | `			end_type = RANGE_IN_LONG;` |
|     ! 0 | 1121 | `			goto handle_numeric_inputs;` |
|       - | 1122 | `		}` |
|       - | 1123 | `		/* Generate an array of characters */` |
|      15 | 1124 | `		if( cStart > cEnd ){` |
|       - | 1125 | `			/* Decreasing char range */` |
|       - | 1126 | `			int iCur;` |
|       3 | 1127 | `			if( (sxi64)(cStart - cEnd) < step ){` |
|     ! 0 | 1128 | `				goto boundary_error;` |
|       - | 1129 | `			}` |
|      17 | 1130 | `			for( iCur = (int)cStart ; iCur >= (int)cEnd ; iCur -= (int)step ){` |
|      15 | 1131 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|     ! 0 | 1132 | `					return rc;` |
|       - | 1133 | `				}` |
|       8 | 1134 | `			}` |
|      14 | 1135 | `		}else if( cEnd > cStart ){` |
|       - | 1136 | `			/* Increasing char range */` |
|       - | 1137 | `			int iCur;` |
|      11 | 1138 | `			if( is_step_negative ){` |
|       3 | 1139 | `				goto negative_step_error;` |
|       - | 1140 | `			}` |
|       9 | 1141 | `			if( (sxi64)(cEnd - cStart) < step ){` |
|       3 | 1142 | `				goto boundary_error;` |
|       - | 1143 | `			}` |
|     139 | 1144 | `			for( iCur = (int)cStart ; iCur <= (int)cEnd ; iCur += (int)step ){` |
|     133 | 1145 | `				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){` |
|     ! 0 | 1146 | `					return rc;` |
|       - | 1147 | `				}` |
|      67 | 1148 | `			}` |
|       4 | 1149 | `		}else{` |
|       3 | 1150 | `			if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)cStart)) != PH7_OK ){` |
|     ! 0 | 1151 | `				return rc;` |
|       - | 1152 | `			}` |
|       - | 1153 | `		}` |
|      11 | 1154 | `		ph7_result_value(pCtx,pArray);` |
|      11 | 1155 | `		return PH7_OK;` |
|       - | 1156 | `	}` |
|      52 | 1157 | `handle_numeric_inputs:` |
|     112 | 1158 | `	if( start_type == RANGE_IN_DOUBLE \|\| end_type == RANGE_IN_DOUBLE \|\| is_step_double ){` |
|       - | 1159 | `		/* Float range */` |
|       - | 1160 | `		double elem,calc;` |
|      21 | 1161 | `		if( start_double > end_double ){` |
|       - | 1162 | `			/* Decreasing float range */` |
|       7 | 1163 | `			if( start_double - end_double < step_double ){` |
|     ! 0 | 1164 | `				goto boundary_error;` |
|       - | 1165 | `			}` |
|       7 | 1166 | `			calc = ((start_double - end_double) / step_double) + 1;` |
|       7 | 1167 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|       - | 1168 | `				/* php prints start/end swapped here (see RangeDoubleSizeError). */` |
|       3 | 1169 | `				return RangeDoubleSizeError(pCtx,calc,end_double,start_double,step_double);` |
|       - | 1170 | `			}` |
|       5 | 1171 | `			size = (sxu32)(calc + 0.5); /* _php_math_round(...,0,HALF_UP) */` |
|      19 | 1172 | `			for( i = 0,elem = start_double ; i < size && elem >= end_double ; ++i,elem = start_double - ((double)i * step_double) ){` |
|      15 | 1173 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|     ! 0 | 1174 | `					return rc;` |
|       - | 1175 | `				}` |
|       8 | 1176 | `			}` |
|      17 | 1177 | `		}else if( end_double > start_double ){` |
|       - | 1178 | `			/* Increasing float range */` |
|      15 | 1179 | `			if( is_step_negative ){` |
|     ! 0 | 1180 | `				goto negative_step_error;` |
|       - | 1181 | `			}` |
|      15 | 1182 | `			if( end_double - start_double < step_double ){` |
|       3 | 1183 | `				goto boundary_error;` |
|       - | 1184 | `			}` |
|      13 | 1185 | `			calc = ((end_double - start_double) / step_double) + 1;` |
|      13 | 1186 | `			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){` |
|       5 | 1187 | `				return RangeDoubleSizeError(pCtx,calc,start_double,end_double,step_double);` |
|       - | 1188 | `			}` |
|       9 | 1189 | `			size = (sxu32)(calc + 0.5);` |
|      45 | 1190 | `			for( i = 0,elem = start_double ; i < size && elem <= end_double ; ++i,elem = start_double + ((double)i * step_double) ){` |
|      37 | 1191 | `				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){` |
|     ! 0 | 1192 | `					return rc;` |
|       - | 1193 | `				}` |
|      19 | 1194 | `			}` |
|       5 | 1195 | `		}else{` |
|     ! 0 | 1196 | `			if( (rc = RangeAppendDouble(pCtx,pArray,pValue,start_double)) != PH7_OK ){` |
|     ! 0 | 1197 | `				return rc;` |
|       - | 1198 | `			}` |
|       - | 1199 | `		}` |
|       7 | 1200 | `	}else{` |
|       - | 1201 | `		/* Int range. All arithmetic in unsigned space so a span wider than` |
|       - | 1202 | `		 * LARGEST_INT64 (e.g. -PHP_INT_MAX..PHP_INT_MAX) wraps correctly` |
|       - | 1203 | `		 * instead of overflowing, exactly like php's zend_ulong math. */` |
|      86 | 1204 | `		sxu64 ustep = (sxu64)step;` |
|       - | 1205 | `		sxu64 calc;` |
|      86 | 1206 | `		if( start_long > end_long ){` |
|       - | 1207 | `			/* Decreasing int range */` |
|      13 | 1208 | `			if( (sxu64)start_long - (sxu64)end_long < ustep ){` |
|       3 | 1209 | `				goto boundary_error;` |
|       - | 1210 | `			}` |
|      11 | 1211 | `			calc = ((sxu64)start_long - (sxu64)end_long) / ustep;` |
|      11 | 1212 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|       - | 1213 | `				/* php prints start/end swapped here (see RangeLongSizeError). */` |
|       3 | 1214 | `				return RangeLongSizeError(pCtx,calc,end_long,start_long,step);` |
|       - | 1215 | `			}` |
|       9 | 1216 | `			size = (sxu32)(calc + 1);` |
|      55 | 1217 | `			for( i = 0 ; i < size ; ++i ){` |
|      47 | 1218 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long - (sxu64)i * ustep))) != PH7_OK ){` |
|     ! 0 | 1219 | `					return rc;` |
|       - | 1220 | `				}` |
|      24 | 1221 | `			}` |
|      77 | 1222 | `		}else if( end_long > start_long ){` |
|       - | 1223 | `			/* Increasing int range */` |
|      72 | 1224 | `			if( is_step_negative ){` |
|       3 | 1225 | `				goto negative_step_error;` |
|       - | 1226 | `			}` |
|      70 | 1227 | `			if( (sxu64)end_long - (sxu64)start_long < ustep ){` |
|       3 | 1228 | `				goto boundary_error;` |
|       - | 1229 | `			}` |
|      68 | 1230 | `			calc = ((sxu64)end_long - (sxu64)start_long) / ustep;` |
|      68 | 1231 | `			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){` |
|       5 | 1232 | `				return RangeLongSizeError(pCtx,calc,start_long,end_long,step);` |
|       - | 1233 | `			}` |
|      64 | 1234 | `			size = (sxu32)(calc + 1);` |
|  401508 | 1235 | `			for( i = 0 ; i < size ; ++i ){` |
|  401446 | 1236 | `				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long + (sxu64)i * ustep))) != PH7_OK ){` |
|     ! 0 | 1237 | `					return rc;` |
|       - | 1238 | `				}` |
|  200724 | 1239 | `			}` |
|      33 | 1240 | `		}else{` |
|       3 | 1241 | `			if( (rc = RangeAppendInt(pCtx,pArray,pValue,start_long)) != PH7_OK ){` |
|     ! 0 | 1242 | `				return rc;` |
|       - | 1243 | `			}` |
|       - | 1244 | `		}` |
|       - | 1245 | `	}` |
|       - | 1246 | `	/* Return the new array. 'pValue' is released automatically by the` |
|       - | 1247 | `	 * virtual machine as soon as we return from this foreign function. */` |
|      86 | 1248 | `	ph7_result_value(pCtx,pArray);` |
|      86 | 1249 | `	return PH7_OK;` |
|       2 | 1250 | `negative_step_error:` |
|       5 | 1251 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1252 | `		"range(): Argument #3 ($step) must be greater than 0 for increasing ranges");` |
|       4 | 1253 | `boundary_error:` |
|       9 | 1254 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1255 | `		"range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)");` |
|      71 | 1256 | `}` |
|       - | 1257 | `/*` |
|       - | 1258 | ` * array array_values(array $array)` |
|       - | 1259 | ` *  Return all the values of an array, indexed numerically.` |
|       - | 1260 | ` * Parameters` |
|       - | 1261 | ` *  $array` |
|       - | 1262 | ` *   The input array.` |
|       - | 1263 | ` * Return` |
|       - | 1264 | ` *  An indexed array of values or NULL on allocation failure.` |
|       - | 1265 | ` */` |
|      52 | 1266 | `PH7_PRIVATE int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1267 | `{` |
|       - | 1268 | `	ph7_hashmap_node *pNode;` |
|       - | 1269 | `	ph7_hashmap *pMap;` |
|       - | 1270 | `	ph7_value *pArray;` |
|       - | 1271 | `	ph7_value *pObj;` |
|       - | 1272 | `	sxu32 n;` |
|      55 | 1273 | `	if( nArg != 1 ){` |
|       - | 1274 | `		/* Wrong argument count, throw ArgumentCountError */` |
|     ! 0 | 1275 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1276 | `			"ArgumentCountError",` |
|       - | 1277 | `			"array_values() expects exactly 1 argument, %d given",` |
|     ! 0 | 1278 | `			nArg` |
|       - | 1279 | `			);` |
|       - | 1280 | `	}` |
|       - | 1281 | `	/* Make sure we are dealing with a valid hashmap */` |
|      55 | 1282 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 1283 | `		/* Type mismatch, throw TypeError */` |
|       4 | 1284 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1285 | `			"TypeError",` |
|       - | 1286 | `			"array_values(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 1287 | `			ph7_type_name(apArg[0])` |
|       - | 1288 | `			);` |
|       - | 1289 | `	}` |
|       - | 1290 | `	/* Point to the internal representation that describe the input hashmap */` |
|      53 | 1291 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 1292 | `	/* Create a new array */` |
|      53 | 1293 | `	pArray = ph7_context_new_array(pCtx);` |
|      53 | 1294 | `	if( pArray == 0 ){` |
|     ! 0 | 1295 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1296 | `		return PH7_OK;` |
|       - | 1297 | `	}` |
|       - | 1298 | `	/* Perform the requested operation */` |
|      53 | 1299 | `	pNode = pMap->pFirst;` |
|     181 | 1300 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|     131 | 1301 | `		pObj = HashmapExtractNodeValue(pNode);` |
|     131 | 1302 | `		if( pObj ){` |
|       - | 1303 | `			/* perform the insertion */` |
|     131 | 1304 | `			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);` |
|      64 | 1305 | `		}` |
|       - | 1306 | `		/* Point to the next entry */` |
|     131 | 1307 | `		pNode = pNode->pPrev; /* Reverse link */` |
|      67 | 1308 | `	}` |
|       - | 1309 | `	/* return the new array */` |
|      53 | 1310 | `	ph7_result_value(pCtx,pArray);` |
|      53 | 1311 | `	return PH7_OK;` |
|      29 | 1312 | `}` |
|       - | 1313 | `/*` |
|       - | 1314 | ` * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )` |
|       - | 1315 | ` *  Return all the keys or a subset of the keys of an array.` |
|       - | 1316 | ` * Parameters` |
|       - | 1317 | ` *  $input` |
|       - | 1318 | ` *   An array containing keys to return.` |
|       - | 1319 | ` * $search_value` |
|       - | 1320 | ` *   If specified, then only keys containing these values are returned.` |
|       - | 1321 | ` * $strict` |
|       - | 1322 | ` *   Determines if strict comparison (===) should be used during the search.` |
|       - | 1323 | ` * Return` |
|       - | 1324 | ` *  An array of all the keys in input or NULL on failure.` |
|       - | 1325 | ` */` |
|     354 | 1326 | `PH7_PRIVATE int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1327 | `{` |
|       - | 1328 | `	ph7_hashmap_node *pNode;` |
|       - | 1329 | `	ph7_hashmap *pMap;` |
|       - | 1330 | `	ph7_value *pArray;` |
|       - | 1331 | `	ph7_value sObj;` |
|       - | 1332 | `	ph7_value sVal;` |
|       - | 1333 | `	SyString sKey;` |
|       - | 1334 | `	int bStrict;` |
|       - | 1335 | `	sxi32 rc;` |
|       - | 1336 | `	sxu32 n;` |
|     359 | 1337 | `	if( nArg < 1 ){` |
|       - | 1338 | `		/* Missing argument,throw ArgumentCountError */` |
|     ! 0 | 1339 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1340 | `			"ArgumentCountError",` |
|       - | 1341 | `			"array_keys() expects at least 1 argument, 0 given"` |
|       - | 1342 | `			);` |
|       - | 1343 | `	}` |
|       - | 1344 | `	/* Make sure we are dealing with a valid hashmap */` |
|     359 | 1345 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 1346 | `		/* haystack must be an array,throw TypeError */` |
|       4 | 1347 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1348 | `			"TypeError",` |
|       - | 1349 | `			"array_keys(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 1350 | `			ph7_type_name(apArg[0])` |
|       - | 1351 | `			);` |
|       - | 1352 | `	}` |
|       - | 1353 | `	/* Point to the internal representation of the input hashmap */` |
|     357 | 1354 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 1355 | `	/* Create a new array */` |
|     357 | 1356 | `	pArray = ph7_context_new_array(pCtx);` |
|     357 | 1357 | `	if( pArray == 0 ){` |
|     ! 0 | 1358 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1359 | `		return PH7_OK;` |
|       - | 1360 | `	}` |
|     357 | 1361 | `	bStrict = FALSE;` |
|     357 | 1362 | `	if( nArg > 2 ){` |
|       - | 1363 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|       9 | 1364 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|     ! 0 | 1365 | `			return PH7_VmThrowException(pCtx,` |
|       - | 1366 | `				"TypeError",` |
|       - | 1367 | `				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",` |
|     ! 0 | 1368 | `				ph7_type_name(apArg[2])` |
|       - | 1369 | `				);` |
|       - | 1370 | `		}` |
|       9 | 1371 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|       4 | 1372 | `	}` |
|       - | 1373 | `	/* Perform the requested operation */` |
|     357 | 1374 | `	pNode = pMap->pFirst;` |
|     357 | 1375 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|    2721 | 1376 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|    2369 | 1377 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     779 | 1378 | `			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);` |
|     392 | 1379 | `		}else{` |
|    1595 | 1380 | `			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));` |
|    1595 | 1381 | `			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);` |
|       - | 1382 | `		}` |
|    2369 | 1383 | `		rc = 0;` |
|    2369 | 1384 | `		if( nArg > 1 ){` |
|      73 | 1385 | `			ph7_value *pValue = HashmapExtractNodeValue(pNode);` |
|      73 | 1386 | `			if( pValue ){` |
|       - | 1387 | `				ph7_value sNeedle;` |
|      73 | 1388 | `				PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      73 | 1389 | `				PH7_MemObjLoad(pValue,&sVal);` |
|       - | 1390 | `				/* Filter key — compare on duplicates of BOTH sides:` |
|       - | 1391 | `				 * PH7_MemObjCmp converts its operands in place, and a needle` |
|       - | 1392 | `				 * mutated on the first element (e.g. null coerced) would` |
|       - | 1393 | `				 * corrupt every later comparison. */` |
|      73 | 1394 | `				PH7_MemObjLoad(apArg[1],&sNeedle);` |
|      73 | 1395 | `				rc = ph7_value_compare(&sVal,&sNeedle,bStrict);` |
|      73 | 1396 | `				PH7_MemObjRelease(&sNeedle);` |
|      73 | 1397 | `				PH7_MemObjRelease(&sVal);` |
|      35 | 1398 | `			}` |
|      35 | 1399 | `		}` |
|    2369 | 1400 | `		if( rc == 0 ){` |
|       - | 1401 | `			/* Perform the insertion */` |
|    2337 | 1402 | `			ph7_array_add_elem(pArray,0,&sObj);` |
|    1166 | 1403 | `		}` |
|    2369 | 1404 | `		PH7_MemObjRelease(&sObj);` |
|       - | 1405 | `		/* Point to the next entry */` |
|    2369 | 1406 | `		pNode = pNode->pPrev; /* Reverse link */` |
|    1187 | 1407 | `	}` |
|       - | 1408 | `	/* return the new array */` |
|     357 | 1409 | `	ph7_result_value(pCtx,pArray);` |
|     357 | 1410 | `	return PH7_OK;` |
|     182 | 1411 | `}` |
|       - | 1412 | `/*` |
|       - | 1413 | ` * bool array_same(array $arr1,array $arr2)` |
|       - | 1414 | ` *  Return TRUE if the given arrays are the same instance.` |
|       - | 1415 | ` *  This function is useful under PH7 since arrays are passed` |
|       - | 1416 | ` *  by reference unlike the zend engine which use pass by values.` |
|       - | 1417 | ` * Parameters` |
|       - | 1418 | ` *  $arr1` |
|       - | 1419 | ` *   First array` |
|       - | 1420 | ` *  $arr2` |
|       - | 1421 | ` *   Second array` |
|       - | 1422 | ` * Return` |
|       - | 1423 | ` *  TRUE if the arrays are the same instance.FALSE otherwise.` |
|       - | 1424 | ` * Note` |
|       - | 1425 | ` *  This function is a symisc eXtension.` |
|       - | 1426 | ` */` |
|       4 | 1427 | `PH7_PRIVATE int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1428 | `{` |
|       - | 1429 | `	ph7_hashmap *p1,*p2;` |
|       - | 1430 | `	int rc;` |
|       5 | 1431 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|       - | 1432 | `		/* Missing or invalid arguments,return FALSE*/` |
|     ! 0 | 1433 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1434 | `		return PH7_OK;` |
|       - | 1435 | `	}` |
|       - | 1436 | `	/* Point to the hashmaps */` |
|       5 | 1437 | `	p1 = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       5 | 1438 | `	p2 = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 | 1439 | `	rc = (p1 == p2);` |
|       - | 1440 | `	/* Same instance? */` |
|       5 | 1441 | `	ph7_result_bool(pCtx,rc);` |
|       5 | 1442 | `	return PH7_OK;` |
|       3 | 1443 | `}` |
|       - | 1444 | `/*` |
|       - | 1445 | ` * array array_merge(array ...$arrays)` |
|       - | 1446 | ` *  Merge one or more arrays.` |
|       - | 1447 | ` * Parameters` |
|       - | 1448 | ` *  ...$arrays` |
|       - | 1449 | ` *   Variable list of arrays to merge. Each argument must be an array;` |
|       - | 1450 | ` *   passing a non-array argument throws a TypeError.` |
|       - | 1451 | ` * Return` |
|       - | 1452 | ` *  The resulting merged array. Returns an empty array when called` |
|       - | 1453 | ` *  with no arguments.` |
|       - | 1454 | ` */` |
|    1178 | 1455 | `PH7_PRIVATE int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1456 | `{` |
|       - | 1457 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 1458 | `	ph7_value *pArray;` |
|       - | 1459 | `	int i;` |
|       - | 1460 | `	/* Create a new array */` |
|    1183 | 1461 | `	pArray = ph7_context_new_array(pCtx);` |
|    1183 | 1462 | `	if( pArray == 0 ){` |
|     ! 0 | 1463 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1464 | `		return PH7_OK;` |
|       - | 1465 | `	}` |
|       - | 1466 | `	/* Point to the internal representation of the hashmap */` |
|    1183 | 1467 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|       - | 1468 | `	/* Start merging */` |
|    3515 | 1469 | `	for( i = 0 ; i < nArg ; i++ ){` |
|       - | 1470 | `		/* Make sure we are dealing with a valid hashmap */` |
|    2341 | 1471 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 1472 | `			/* Type mismatch -> TypeError */` |
|       8 | 1473 | `			return PH7_VmThrowException(pCtx,` |
|       - | 1474 | `				"TypeError",` |
|       - | 1475 | `				"array_merge(): Argument #%d must be of type array, %s given",` |
|       2 | 1476 | `				i + 1,` |
|       4 | 1477 | `				ph7_type_name(apArg[i])` |
|       - | 1478 | `				);` |
|     ! 0 | 1479 | `		}else{` |
|    2337 | 1480 | `			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 1481 | `			/* Merge the two hashmaps */` |
|    2337 | 1482 | `			HashmapMerge(pSrc,pMap);` |
|       - | 1483 | `		}` |
|    1171 | 1484 | `	}` |
|       - | 1485 | `	/* Return the freshly created array */` |
|    1179 | 1486 | `	ph7_result_value(pCtx,pArray);` |
|    1179 | 1487 | `	return PH7_OK;` |
|     594 | 1488 | `}` |
|       - | 1489 | `/*` |
|       - | 1490 | ` * array array_copy(array $source)` |
|       - | 1491 | ` *  Make a blind copy of the target array.` |
|       - | 1492 | ` * Parameters` |
|       - | 1493 | ` *  $source` |
|       - | 1494 | ` *   Target array` |
|       - | 1495 | ` * Return` |
|       - | 1496 | ` *  Copy of the target array on success.NULL otherwise.` |
|       - | 1497 | ` * Note` |
|       - | 1498 | ` *  This function is a symisc eXtension.` |
|       - | 1499 | ` */` |
|      20 | 1500 | `PH7_PRIVATE int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1501 | `{` |
|       - | 1502 | `	ph7_hashmap *pMap;` |
|       - | 1503 | `	ph7_value *pArray;` |
|      21 | 1504 | `	if( nArg < 1 ){` |
|       - | 1505 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 1506 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1507 | `		return PH7_OK;` |
|       - | 1508 | `	}` |
|       - | 1509 | `	/* Create a new array */` |
|      21 | 1510 | `	pArray = ph7_context_new_array(pCtx);` |
|      21 | 1511 | `	if( pArray == 0 ){` |
|     ! 0 | 1512 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1513 | `		return PH7_OK;` |
|       - | 1514 | `	}` |
|       - | 1515 | `	/* Point to the internal representation of the hashmap */` |
|      21 | 1516 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|      21 | 1517 | `	if( ph7_value_is_array(apArg[0])){` |
|       - | 1518 | `		/* Point to the internal representation of the source */` |
|      21 | 1519 | `		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 1520 | `		/* Perform the copy */` |
|      21 | 1521 | `		PH7_HashmapDup(pSrc,pMap);` |
|      11 | 1522 | `	}else{` |
|       - | 1523 | `		/* Simple insertion */` |
|     ! 0 | 1524 | `		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);` |
|       - | 1525 | `	}` |
|       - | 1526 | `	/* Return the duplicated array */` |
|      21 | 1527 | `	ph7_result_value(pCtx,pArray);` |
|      21 | 1528 | `	return PH7_OK;` |
|      11 | 1529 | `}` |
|       - | 1530 | `/*` |
|       - | 1531 | ` * bool array_erase(array $source)` |
|       - | 1532 | ` *  Remove all elements from a given array.` |
|       - | 1533 | ` * Parameters` |
|       - | 1534 | ` *  $source` |
|       - | 1535 | ` *   Target array` |
|       - | 1536 | ` * Return` |
|       - | 1537 | ` *  TRUE on success.FALSE otherwise.` |
|       - | 1538 | ` * Note` |
|       - | 1539 | ` *  This function is a symisc eXtension.` |
|       - | 1540 | ` */` |
|      28 | 1541 | `PH7_PRIVATE int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1542 | `{` |
|       - | 1543 | `	ph7_hashmap *pMap;` |
|      30 | 1544 | `	if( nArg < 1 ){` |
|       - | 1545 | `		/* Missing arguments */` |
|     ! 0 | 1546 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1547 | `		return PH7_OK;` |
|       - | 1548 | `	}` |
|       - | 1549 | `	/* Point to the target hashmap */` |
|      30 | 1550 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      30 | 1551 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 1552 | `	/* Erase */` |
|      30 | 1553 | `	PH7_HashmapRelease(pMap,FALSE);` |
|      30 | 1554 | `	return PH7_OK;` |
|      16 | 1555 | `}` |
|       - | 1556 | `/*` |
|       - | 1557 | ` * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])` |
|       - | 1558 | ` *  Extract a slice of the array.` |
|       - | 1559 | ` * Parameters` |
|       - | 1560 | ` *  $array` |
|       - | 1561 | ` *    The input array.` |
|       - | 1562 | ` * $offset` |
|       - | 1563 | ` *    If offset is non-negative, the sequence will start at that offset in the array.` |
|       - | 1564 | ` *    If offset is negative, the sequence will start that far from the end of the array.` |
|       - | 1565 | ` * $length (optional, nullable)` |
|       - | 1566 | ` *    If length is given and is positive, then the sequence will have that many elements` |
|       - | 1567 | ` *    in it. If length is given and is negative then the sequence will stop that many` |
|       - | 1568 | ` *    elements from the end of the array. If it is omitted or NULL, then the sequence` |
|       - | 1569 | ` *    will have everything from offset up until the end of the array.` |
|       - | 1570 | ` * $preserve_keys (optional)` |
|       - | 1571 | ` *    Note that array_slice() will reorder and reset the array indices by default.` |
|       - | 1572 | ` *    You can change this behaviour by setting preserve_keys to TRUE.` |
|       - | 1573 | ` * Return` |
|       - | 1574 | ` *   The new slice.` |
|       - | 1575 | ` */` |
|     120 | 1576 | `PH7_PRIVATE int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1577 | `{` |
|       - | 1578 | `	ph7_hashmap *pMap,*pSrc;` |
|       - | 1579 | `	ph7_hashmap_node *pCur;` |
|       - | 1580 | `	ph7_value *pArray;` |
|       - | 1581 | `	int iLength,iOfft;` |
|       - | 1582 | `	int bPreserve;` |
|       - | 1583 | `	sxi32 rc;` |
|     125 | 1584 | `	if( nArg < 2 ){` |
|     ! 0 | 1585 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1586 | `			"ArgumentCountError",` |
|       - | 1587 | `			"array_slice() expects at least 2 arguments, %d given",` |
|     ! 0 | 1588 | `			nArg` |
|       - | 1589 | `			);` |
|       - | 1590 | `	}` |
|     125 | 1591 | `	if( nArg > 4 ){` |
|     ! 0 | 1592 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1593 | `			"ArgumentCountError",` |
|       - | 1594 | `			"array_slice() expects at most 4 arguments, %d given",` |
|     ! 0 | 1595 | `			nArg` |
|       - | 1596 | `			);` |
|       - | 1597 | `	}` |
|     125 | 1598 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 1599 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1600 | `			"TypeError",` |
|       - | 1601 | `			"array_slice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 1602 | `			ph7_type_name(apArg[0])` |
|       - | 1603 | `			);` |
|       - | 1604 | `	}` |
|       - | 1605 | `	/* Validate $offset type: reject string, array, object, resource */` |
|     176 | 1606 | `	if( ph7_value_is_string(apArg[1]) \|\| ph7_value_is_array(apArg[1]) \|\|` |
|     179 | 1607 | `		ph7_value_is_object(apArg[1]) \|\| ph7_value_is_resource(apArg[1]) ){` |
|       4 | 1608 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1609 | `			"TypeError",` |
|       - | 1610 | `			"array_slice(): Argument #2 ($offset) must be of type int, %s given",` |
|       2 | 1611 | `			ph7_type_name(apArg[1])` |
|       - | 1612 | `			);` |
|       - | 1613 | `	}` |
|       - | 1614 | `	/* Validate $length type if provided: nullable int */` |
|     121 | 1615 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|     137 | 1616 | `		if( ph7_value_is_string(apArg[2]) \|\| ph7_value_is_array(apArg[2]) \|\|` |
|     137 | 1617 | `			ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|       4 | 1618 | `			return PH7_VmThrowException(pCtx,` |
|       - | 1619 | `				"TypeError",` |
|       - | 1620 | `				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",` |
|       2 | 1621 | `				ph7_type_name(apArg[2])` |
|       - | 1622 | `				);` |
|       - | 1623 | `		}` |
|      45 | 1624 | `	}` |
|       - | 1625 | `	/* Validate $preserve_keys type if provided: reject array, object, resource */` |
|     119 | 1626 | `	if( nArg > 3 ){` |
|       7 | 1627 | `		if( ph7_value_is_array(apArg[3]) \|\| ph7_value_is_object(apArg[3]) \|\|` |
|       4 | 1628 | `			ph7_value_is_resource(apArg[3]) ){` |
|     ! 0 | 1629 | `			return PH7_VmThrowException(pCtx,` |
|       - | 1630 | `				"TypeError",` |
|       - | 1631 | `				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",` |
|     ! 0 | 1632 | `				ph7_type_name(apArg[3])` |
|       - | 1633 | `				);` |
|       - | 1634 | `		}` |
|       2 | 1635 | `	}` |
|       - | 1636 | `	/* Point the internal representation of the target array */` |
|     119 | 1637 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     119 | 1638 | `	bPreserve = FALSE;` |
|       - | 1639 | `	/* Get the offset */` |
|       - | 1640 | `	{` |
|     119 | 1641 | `		sxi64 iTmp = 0;` |
|     119 | 1642 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"array_slice",2,"$offset","int",&iTmp);` |
|     119 | 1643 | `		if( rcArg != PH7_OK ){` |
|     ! 0 | 1644 | `			return rcArg;` |
|       - | 1645 | `		}` |
|     119 | 1646 | `		iOfft = (int)iTmp;` |
|       - | 1647 | `	}` |
|     119 | 1648 | `	if( iOfft < 0 ){` |
|       5 | 1649 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       5 | 1650 | `		if( iOfft < 0 ){` |
|       3 | 1651 | `			iOfft = 0;` |
|       1 | 1652 | `		}` |
|       2 | 1653 | `	}` |
|     119 | 1654 | `	if( iOfft >= (int)pSrc->nEntry ){` |
|       - | 1655 | `		/* Offset past end of array, return empty array */` |
|       5 | 1656 | `		pArray = ph7_context_new_array(pCtx);` |
|       5 | 1657 | `		if( pArray == 0 ){` |
|     ! 0 | 1658 | `			ph7_result_null(pCtx);` |
|     ! 0 | 1659 | `			return PH7_OK;` |
|       - | 1660 | `		}` |
|       5 | 1661 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 1662 | `		return PH7_OK;` |
|       - | 1663 | `	}` |
|       - | 1664 | `	/* Get the length: NULL means "all remaining" (same as omitting) */` |
|     115 | 1665 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|     115 | 1666 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      91 | 1667 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      91 | 1668 | `		if( iLength < 0 ){` |
|       5 | 1669 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       2 | 1670 | `		}` |
|      91 | 1671 | `		if( iLength < 0 ){` |
|       3 | 1672 | `			iLength = 0;` |
|       1 | 1673 | `		}` |
|      91 | 1674 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 1675 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 1676 | `		}` |
|      45 | 1677 | `	}` |
|     115 | 1678 | `	if( nArg > 3 ){` |
|       5 | 1679 | `		bPreserve = ph7_value_to_bool(apArg[3]);` |
|       2 | 1680 | `	}` |
|       - | 1681 | `	/* Create a new array */` |
|     115 | 1682 | `	pArray = ph7_context_new_array(pCtx);` |
|     115 | 1683 | `	if( pArray == 0 ){` |
|     ! 0 | 1684 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1685 | `		return PH7_OK;` |
|       - | 1686 | `	}` |
|     115 | 1687 | `	if( iLength < 1 ){` |
|       - | 1688 | `		/* Don't bother processing,return the empty array */` |
|       5 | 1689 | `		ph7_result_value(pCtx,pArray);` |
|       5 | 1690 | `		return PH7_OK;` |
|       - | 1691 | `	}` |
|       - | 1692 | `	/* Point to the desired entry */` |
|     111 | 1693 | `	pCur = pSrc->pFirst;` |
|      83 | 1694 | `	for(;;){` |
|     171 | 1695 | `		if( iOfft < 1 ){` |
|     111 | 1696 | `			break;` |
|       - | 1697 | `		}` |
|       - | 1698 | `		/* Point to the next entry */` |
|      65 | 1699 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      65 | 1700 | `		iOfft--;` |
|       5 | 1701 | `	}` |
|       - | 1702 | `	/* Point to the internal representation of the hashmap */` |
|     111 | 1703 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     179 | 1704 | `	for(;;){` |
|     363 | 1705 | `		if( iLength < 1 ){` |
|     111 | 1706 | `			break;` |
|       - | 1707 | `		}` |
|       - | 1708 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|       - | 1709 | `		{` |
|     257 | 1710 | `			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|     257 | 1711 | `			rc = HashmapInsertNode(pMap,pCur,bKeep);` |
|       - | 1712 | `		}` |
|     257 | 1713 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1714 | `			break;` |
|       - | 1715 | `		}` |
|       - | 1716 | `		/* Point to the next entry */` |
|     257 | 1717 | `		pCur = pCur->pPrev; /* Reverse link */` |
|     257 | 1718 | `		iLength--;` |
|       5 | 1719 | `	}` |
|       - | 1720 | `	/* Return the freshly created array */` |
|     111 | 1721 | `	ph7_result_value(pCtx,pArray);` |
|     111 | 1722 | `	return PH7_OK;` |
|      65 | 1723 | `}` |
|       - | 1724 | `/*` |
|       - | 1725 | ` * Move the last node in the hashmap linked list to immediately after pAfter` |
|       - | 1726 | ` * in iteration order.  If pAfter is NULL the node is moved to the very` |
|       - | 1727 | ` * beginning (becomes the new pFirst).` |
|       - | 1728 | ` */` |
|      38 | 1729 | `static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)` |
|       1 | 1730 | `{` |
|       - | 1731 | `	ph7_hashmap_node *pNode;` |
|       - | 1732 | `	ph7_hashmap_node *pOldNext;` |
|      39 | 1733 | `	pNode = pMap->pLast;` |
|      39 | 1734 | `	if( pNode == 0 ){` |
|     ! 0 | 1735 | `		return;` |
|       - | 1736 | `	}` |
|      39 | 1737 | `	if( pNode->pNext == 0 ){` |
|       - | 1738 | `		/* Only node in the list, nothing to move */` |
|       5 | 1739 | `		return;` |
|       - | 1740 | `	}` |
|      35 | 1741 | `	if( pAfter != 0 && pAfter->pPrev == pNode ){` |
|       - | 1742 | `		/* Already in the correct position */` |
|       9 | 1743 | `		return;` |
|       - | 1744 | `	}` |
|       - | 1745 | `	/* Unlink pNode from the end of the list */` |
|      27 | 1746 | `	pMap->pLast = pNode->pNext;` |
|      27 | 1747 | `	pMap->pLast->pPrev = 0;` |
|       - | 1748 | `	/* Insert pNode after pAfter in iteration order */` |
|      27 | 1749 | `	if( pAfter == 0 ){` |
|       - | 1750 | `		/* Insert at the very beginning, before pFirst */` |
|       3 | 1751 | `		pNode->pNext = 0;` |
|       3 | 1752 | `		pNode->pPrev = pMap->pFirst;` |
|       3 | 1753 | `		if( pMap->pFirst ){` |
|       3 | 1754 | `			pMap->pFirst->pNext = pNode;` |
|       1 | 1755 | `		}` |
|       3 | 1756 | `		pMap->pFirst = pNode;` |
|       2 | 1757 | `	}else{` |
|      25 | 1758 | `		pOldNext = pAfter->pPrev;` |
|      25 | 1759 | `		pNode->pPrev = pOldNext;` |
|      25 | 1760 | `		pNode->pNext = pAfter;` |
|      25 | 1761 | `		pAfter->pPrev = pNode;` |
|      25 | 1762 | `		if( pOldNext ){` |
|      25 | 1763 | `			pOldNext->pNext = pNode;` |
|      13 | 1764 | `		}else{` |
|     ! 0 | 1765 | `			pMap->pLast = pNode;` |
|       - | 1766 | `		}` |
|       - | 1767 | `	}` |
|      20 | 1768 | `}` |
|       - | 1769 | `/*` |
|       - | 1770 | ` * array array_splice(array $array, int $offset [, int $length [, value $replacement]])` |
|       - | 1771 | ` *  Remove a portion of the array and replace it with something else.` |
|       - | 1772 | ` * Parameters` |
|       - | 1773 | ` *  $array` |
|       - | 1774 | ` *    The input array.` |
|       - | 1775 | ` *  $offset` |
|       - | 1776 | ` *    If offset is positive then the start of removed portion is at that offset` |
|       - | 1777 | ` *    from the beginning of the input array.  If offset is negative then it` |
|       - | 1778 | ` *    starts that far from the end of the input array.  If the absolute value of` |
|       - | 1779 | ` *    a negative offset exceeds the array length, offset is clamped to 0.  If a` |
|       - | 1780 | ` *    positive offset exceeds the array length, offset is clamped to the array` |
|       - | 1781 | ` *    length (i.e. nothing is removed, but replacement is appended).` |
|       - | 1782 | ` *  $length (optional)` |
|       - | 1783 | ` *    If length is omitted, removes everything from offset to the end of the` |
|       - | 1784 | ` *    array.  If length is specified and is positive, then that many elements` |
|       - | 1785 | ` *    will be removed.  If length is specified and is negative then the end of` |
|       - | 1786 | ` *    the removed portion will be that many elements from the end of the array.` |
|       - | 1787 | ` *    If the resulting length is negative it is clamped to 0.` |
|       - | 1788 | ` *  $replacement (optional)` |
|       - | 1789 | ` *    If replacement array is specified, then the removed elements are replaced` |
|       - | 1790 | ` *    with elements from this array.` |
|       - | 1791 | ` *    If offset and length are such that nothing is removed, then the elements` |
|       - | 1792 | ` *    from the replacement array are inserted in the place specified by the` |
|       - | 1793 | ` *    offset.` |
|       - | 1794 | ` *    Note that keys in replacement array are not preserved.` |
|       - | 1795 | ` *    If replacement is just one element it is not necessary to put array()` |
|       - | 1796 | ` *    around it, unless the element is an array itself, an object or NULL.` |
|       - | 1797 | ` * Return` |
|       - | 1798 | ` *   A new array consisting of the extracted elements.` |
|       - | 1799 | ` */` |
|      64 | 1800 | `PH7_PRIVATE int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1801 | `{` |
|       - | 1802 | `	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;` |
|       - | 1803 | `	ph7_value *pArray,*pRvalue;` |
|       - | 1804 | `	ph7_hashmap *pMap,*pSrc,*pRep;` |
|       - | 1805 | `	int iLength,iOfft,i;` |
|       - | 1806 | `	sxi32 rc;` |
|      66 | 1807 | `	if( nArg < 2 ){` |
|     ! 0 | 1808 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1809 | `			"ArgumentCountError",` |
|       - | 1810 | `			"array_splice() expects at least 2 arguments, %d given",` |
|     ! 0 | 1811 | `			nArg` |
|       - | 1812 | `			);` |
|       - | 1813 | `	}` |
|      66 | 1814 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 1815 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1816 | `			"TypeError",` |
|       - | 1817 | `			"array_splice(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 1818 | `			ph7_type_name(apArg[0])` |
|       - | 1819 | `			);` |
|       - | 1820 | `	}` |
|       - | 1821 | `	/* Point to the internal representation of the target array */` |
|      63 | 1822 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      63 | 1823 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 1824 | `	/* Get the offset and clamp to valid range */` |
|      63 | 1825 | `	iOfft = ph7_value_to_int(apArg[1]);` |
|      63 | 1826 | `	if( iOfft < 0 ){` |
|       9 | 1827 | `		iOfft = (int)pSrc->nEntry + iOfft;` |
|       9 | 1828 | `		if( iOfft < 0 ){` |
|       3 | 1829 | `			iOfft = 0;` |
|       2 | 1830 | `		}` |
|      59 | 1831 | `	}else if( iOfft > (int)pSrc->nEntry ){` |
|       3 | 1832 | `		iOfft = (int)pSrc->nEntry;` |
|       1 | 1833 | `	}` |
|       - | 1834 | `	/* Get the length and clamp to valid range.` |
|       - | 1835 | `	 * NULL means "all remaining" (same as omitting the argument). */` |
|      63 | 1836 | `	iLength = (int)pSrc->nEntry - iOfft;` |
|      63 | 1837 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      45 | 1838 | `		iLength = ph7_value_to_int(apArg[2]);` |
|      45 | 1839 | `		if( iLength < 0 ){` |
|       7 | 1840 | `			iLength = ((int)pSrc->nEntry + iLength) - iOfft;` |
|       7 | 1841 | `			if( iLength < 0 ){` |
|       3 | 1842 | `				iLength = 0;` |
|       1 | 1843 | `			}` |
|       3 | 1844 | `		}` |
|      45 | 1845 | `		if( iOfft + iLength > (int)pSrc->nEntry ){` |
|       3 | 1846 | `			iLength = (int)pSrc->nEntry - iOfft;` |
|       1 | 1847 | `		}` |
|      22 | 1848 | `	}` |
|       - | 1849 | `	/* Create the result array for removed elements */` |
|      63 | 1850 | `	pArray = ph7_context_new_array(pCtx);` |
|      63 | 1851 | `	if( pArray == 0 ){` |
|     ! 0 | 1852 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1853 | `		return PH7_OK;` |
|       - | 1854 | `	}` |
|       - | 1855 | `	/* Get replacement array if provided */` |
|      63 | 1856 | `	pRep = 0;` |
|      63 | 1857 | `	if( nArg > 3 ){` |
|      27 | 1858 | `		if( !ph7_value_is_array(apArg[3]) ){` |
|       - | 1859 | `			/* Perform an array cast */` |
|       3 | 1860 | `			PH7_MemObjToHashmap(apArg[3]);` |
|       3 | 1861 | `			if( ph7_value_is_array(apArg[3]) ){` |
|       3 | 1862 | `				pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       1 | 1863 | `			}` |
|       2 | 1864 | `		}else{` |
|      25 | 1865 | `			pRep = (ph7_hashmap *)apArg[3]->x.pOther;` |
|       - | 1866 | `		}` |
|      27 | 1867 | `		if( pRep ){` |
|       - | 1868 | `			/* Reset the loop cursor */` |
|      27 | 1869 | `			pRep->pCur = pRep->pFirst;` |
|      13 | 1870 | `		}` |
|      13 | 1871 | `	}` |
|       - | 1872 | `	/* No early return for the nothing-to-do case: php reindexes the input` |
|       - | 1873 | `	 * array's integer keys on EVERY splice, even a no-op one. */` |
|       - | 1874 | `	/* Navigate to the offset position */` |
|      63 | 1875 | `	pCur = pSrc->pFirst;` |
|     131 | 1876 | `	for( i = 0 ; i < iOfft && pCur ; i++ ){` |
|      69 | 1877 | `		pCur = pCur->pPrev; /* Reverse link */` |
|      35 | 1878 | `	}` |
|       - | 1879 | `	/* Save the node just before the splice range as the insertion anchor.` |
|       - | 1880 | `	 * pCur->pNext is the backward link (previous node in iteration order).` |
|       - | 1881 | `	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */` |
|      63 | 1882 | `	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;` |
|       - | 1883 | `	/* Remove nodes in the splice range and copy them to the result array */` |
|      63 | 1884 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     141 | 1885 | `	for( i = 0 ; i < iLength && pCur ; i++ ){` |
|      79 | 1886 | `		pPrev = pCur->pPrev;` |
|      79 | 1887 | `		rc = HashmapInsertNode(pMap,pCur,FALSE);` |
|      79 | 1888 | `		PH7_HashmapUnlinkNode(pCur,TRUE);` |
|      79 | 1889 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 1890 | `			break;` |
|       - | 1891 | `		}` |
|      79 | 1892 | `		pCur = pPrev; /* Reverse link */` |
|      40 | 1893 | `	}` |
|       - | 1894 | `	/* Insert replacement elements at the correct position */` |
|      63 | 1895 | `	if( pRep ){` |
|       - | 1896 | `		ph7_value sSafeVal;` |
|      78 | 1897 | `		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){` |
|      39 | 1898 | `			pRvalue = HashmapExtractNodeValue(pRnode);` |
|      39 | 1899 | `			if( pRvalue ){` |
|       - | 1900 | `				/* Make a stack copy before inserting.  HashmapInsert() may` |
|       - | 1901 | `				 * grow the VM memobj pool, which would invalidate pRvalue` |
|       - | 1902 | `				 * since it points into that same pool. */` |
|      39 | 1903 | `				sSafeVal = *pRvalue;` |
|      39 | 1904 | `				rc = HashmapInsert(pSrc,0,&sSafeVal);` |
|      39 | 1905 | `				if( rc == SXRET_OK && pSrc->pLast != 0 ){` |
|      39 | 1906 | `					pNewNode = pSrc->pLast;` |
|      39 | 1907 | `					HashmapMoveLastAfter(pSrc,pInsertAfter);` |
|      39 | 1908 | `					pInsertAfter = pNewNode;` |
|      19 | 1909 | `				}` |
|      19 | 1910 | `			}` |
|       1 | 1911 | `		}` |
|      13 | 1912 | `	}` |
|       - | 1913 | `	/* php renumbers ALL integer keys of the input array in iteration order` |
|       - | 1914 | `	 * (string keys preserved) — same pass as array_shift. Pre-fix the spliced` |
|       - | 1915 | `	 * array kept its old keys, so inserts landed with out-of-sequence keys` |
|       - | 1916 | `	 * and removals left gaps. */` |
|       - | 1917 | `	{` |
|      63 | 1918 | `		ph7_hashmap_node *pEntry = pSrc->pFirst;` |
|      63 | 1919 | `		sxu32 n = pSrc->nEntry;` |
|      63 | 1920 | `		pSrc->iNextIdx = 0;` |
|     233 | 1921 | `		while( n > 0 ){` |
|     171 | 1922 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     165 | 1923 | `				HashmapRehashIntNode(pEntry);` |
|      82 | 1924 | `			}` |
|     171 | 1925 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|     171 | 1926 | `			n--;` |
|       1 | 1927 | `		}` |
|      63 | 1928 | `		pSrc->pCur = pSrc->pFirst;` |
|       - | 1929 | `	}` |
|       - | 1930 | `	/* Return the freshly created array */` |
|      63 | 1931 | `	ph7_result_value(pCtx,pArray);` |
|      63 | 1932 | `	return PH7_OK;` |
|      34 | 1933 | `}` |
|       - | 1934 | `/*` |
|       - | 1935 | ` * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])` |
|       - | 1936 | ` *  Checks if a value exists in an array.` |
|       - | 1937 | ` * Parameters` |
|       - | 1938 | ` *  $needle` |
|       - | 1939 | ` *   The searched value.` |
|       - | 1940 | ` *   Note:` |
|       - | 1941 | ` *    If needle is a string, the comparison is done in a case-sensitive manner.` |
|       - | 1942 | ` * $haystack` |
|       - | 1943 | ` *  The target array.` |
|       - | 1944 | ` * $strict` |
|       - | 1945 | ` *  If the third parameter strict is set to TRUE then the in_array() function` |
|       - | 1946 | ` *  will also check the types of the needle in the haystack.` |
|       - | 1947 | ` */` |
|   35876 | 1948 | `PH7_PRIVATE int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1949 | `{` |
|       - | 1950 | `	ph7_value *pNeedle;` |
|       - | 1951 | `	int bStrict;` |
|       - | 1952 | `	int rc;` |
|   35881 | 1953 | `	if( nArg < 2 ){` |
|       - | 1954 | `		/* Missing argument,return FALSE */` |
|     ! 0 | 1955 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 1956 | `		return PH7_OK;` |
|       - | 1957 | `	}` |
|   35881 | 1958 | `	pNeedle = apArg[0];` |
|   35881 | 1959 | `	bStrict = 0;` |
|   35881 | 1960 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 1961 | `		/* haystack must be an array,throw TypeError (matches array_search) */` |
|       - | 1962 | `		char zBuf[64];` |
|      16 | 1963 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1964 | `			"TypeError",` |
|       - | 1965 | `			"in_array(): Argument #2 ($haystack) must be of type array, %s given",` |
|      10 | 1966 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|       - | 1967 | `			);` |
|       - | 1968 | `	}` |
|   35871 | 1969 | `	if( nArg > 2 ){` |
|      76 | 1970 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|      36 | 1971 | `	}` |
|       - | 1972 | `	/* Perform the lookup */` |
|   35871 | 1973 | `	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);` |
|       - | 1974 | `	/* Lookup result */` |
|   35871 | 1975 | `	ph7_result_bool(pCtx,rc == SXRET_OK);` |
|   35871 | 1976 | `	return PH7_OK;` |
|   17943 | 1977 | `}` |
|       - | 1978 | `/*` |
|       - | 1979 | ` * value array_search(value $needle,array $haystack[,bool $strict = false ])` |
|       - | 1980 | ` *  Searches the array for a given value and returns the corresponding key if successful.` |
|       - | 1981 | ` * Parameters` |
|       - | 1982 | ` * $needle` |
|       - | 1983 | ` *   The searched value.` |
|       - | 1984 | ` * $haystack` |
|       - | 1985 | ` *   The array.` |
|       - | 1986 | ` * $strict` |
|       - | 1987 | ` *  If the third parameter strict is set to TRUE then the array_search() function` |
|       - | 1988 | ` *  will search for identical elements in the haystack. This means it will also check` |
|       - | 1989 | ` *  the types of the needle in the haystack, and objects must be the same instance.` |
|       - | 1990 | ` * Return` |
|       - | 1991 | ` *  Returns the key for needle if it is found in the array, FALSE otherwise.` |
|       - | 1992 | ` */` |
|      56 | 1993 | `PH7_PRIVATE int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 1994 | `{` |
|       - | 1995 | `	ph7_hashmap_node *pEntry;` |
|       - | 1996 | `	ph7_value *pVal,sNeedle;` |
|       - | 1997 | `	ph7_hashmap *pMap;` |
|       - | 1998 | `	ph7_value sVal;` |
|       - | 1999 | `	int bStrict;` |
|       - | 2000 | `	sxu32 n;` |
|       - | 2001 | `	int rc;` |
|      60 | 2002 | `	if( nArg < 2 ){` |
|       - | 2003 | `		/* Missing argument,throw ArgumentCountError */` |
|     ! 0 | 2004 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2005 | `			"ArgumentCountError",` |
|       - | 2006 | `			"array_search() expects at least 2 arguments, %d given",` |
|     ! 0 | 2007 | `			nArg` |
|       - | 2008 | `			);` |
|       - | 2009 | `	}` |
|      60 | 2010 | `	bStrict = FALSE;` |
|      60 | 2011 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2012 | `		/* haystack must be an array,throw TypeError. VmValueGivenName gives php's` |
|       - | 2013 | `		 * ZPP value-name (true/false for bools, not ph7_type_name's "bool") */` |
|       - | 2014 | `		char zBuf[64];` |
|      20 | 2015 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2016 | `			"TypeError",` |
|       - | 2017 | `			"array_search(): Argument #2 ($haystack) must be of type array, %s given",` |
|      12 | 2018 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|       - | 2019 | `			);` |
|       - | 2020 | `	}` |
|      47 | 2021 | `	if( nArg > 2 ){` |
|       - | 2022 | `		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */` |
|      21 | 2023 | `		if( ph7_value_is_array(apArg[2]) \|\| ph7_value_is_object(apArg[2]) \|\| ph7_value_is_resource(apArg[2]) ){` |
|     ! 0 | 2024 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2025 | `				"TypeError",` |
|       - | 2026 | `				"array_search(): Argument #3 ($strict) must be of type bool, %s given",` |
|     ! 0 | 2027 | `				ph7_type_name(apArg[2])` |
|       - | 2028 | `				);` |
|       - | 2029 | `		}` |
|      21 | 2030 | `		bStrict = ph7_value_to_bool(apArg[2]);` |
|      10 | 2031 | `	}` |
|       - | 2032 | `	/* Point to the internal representation of the internal hashmap */` |
|      47 | 2033 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       - | 2034 | `	/* Perform a linear search since we cannot sort the hashmap based on values */` |
|      47 | 2035 | `	PH7_MemObjInit(pMap->pVm,&sVal);` |
|      47 | 2036 | `	PH7_MemObjInit(pMap->pVm,&sNeedle);` |
|      47 | 2037 | `	pEntry = pMap->pFirst;` |
|      47 | 2038 | `	n = pMap->nEntry;` |
|      44 | 2039 | `	for(;;){` |
|      91 | 2040 | `		if( !n ){` |
|      14 | 2041 | `			break;` |
|       - | 2042 | `		}` |
|       - | 2043 | `		/* Extract node value */` |
|      79 | 2044 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      79 | 2045 | `		if( pVal ){` |
|       - | 2046 | `			/* Make a copy of the vuurent values since the comparison routine` |
|       - | 2047 | `			 * can change their type.` |
|       - | 2048 | `			 */` |
|      79 | 2049 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      79 | 2050 | `			PH7_MemObjLoad(apArg[0],&sNeedle);` |
|      79 | 2051 | `			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);` |
|      79 | 2052 | `			PH7_MemObjRelease(&sVal);` |
|      79 | 2053 | `			PH7_MemObjRelease(&sNeedle);` |
|      79 | 2054 | `			if( rc == 0 ){` |
|       - | 2055 | `				/* Match found,return key */` |
|      35 | 2056 | `				if( pEntry->iType == HASHMAP_INT_NODE){` |
|       - | 2057 | `					/* INT key */` |
|      29 | 2058 | `					ph7_result_int64(pCtx,pEntry->xKey.iKey);` |
|      16 | 2059 | `				}else{` |
|       7 | 2060 | `					SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 2061 | `					/* Blob key */` |
|       7 | 2062 | `					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));` |
|       - | 2063 | `				}` |
|      35 | 2064 | `				return PH7_OK;` |
|       - | 2065 | `			}` |
|      22 | 2066 | `		}` |
|       - | 2067 | `		/* Point to the next entry */` |
|      46 | 2068 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      46 | 2069 | `		n--;` |
|       2 | 2070 | `	}` |
|       - | 2071 | `	/* No such value,return FALSE */` |
|      14 | 2072 | `	ph7_result_bool(pCtx,0);` |
|      14 | 2073 | `	return PH7_OK;` |
|      32 | 2074 | `}` |
|       - | 2075 | `/*` |
|       - | 2076 | ` * array array_diff(array $array1,array $array2,...)` |
|       - | 2077 | ` *  Computes the difference of arrays.` |
|       - | 2078 | ` * Parameters` |
|       - | 2079 | ` *  $array1` |
|       - | 2080 | ` *    The array to compare from` |
|       - | 2081 | ` *  $array2` |
|       - | 2082 | ` *    An array to compare against` |
|       - | 2083 | ` *  $...` |
|       - | 2084 | ` *   More arrays to compare against` |
|       - | 2085 | ` * Return` |
|       - | 2086 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 2087 | ` *  are not present in any of the other arrays.` |
|       - | 2088 | ` */` |
|      70 | 2089 | `PH7_PRIVATE int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2090 | `{` |
|       - | 2091 | `	ph7_hashmap_node *pEntry;` |
|       - | 2092 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2093 | `	ph7_value *pArray;` |
|       - | 2094 | `	ph7_value *pVal;` |
|       - | 2095 | `	sxi32 rc;` |
|       - | 2096 | `	sxu32 n;` |
|       - | 2097 | `	int i;` |
|       - | 2098 | `	/* Validate arguments to mimic PHP behaviour. Earlier versions simply` |
|       - | 2099 | `	 * returned NULL when the caller passed invalid parameters which made` |
|       - | 2100 | `	 * debugging difficult. */` |
|      73 | 2101 | `	if( nArg < 1 ){` |
|     ! 0 | 2102 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2103 | `			"ArgumentCountError",` |
|       - | 2104 | `			"array_diff() expects at least 1 argument, %d given",` |
|     ! 0 | 2105 | `			nArg` |
|       - | 2106 | `			);` |
|       - | 2107 | `	}` |
|      73 | 2108 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2109 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2110 | `			"TypeError",` |
|       - | 2111 | `			"array_diff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2112 | `			ph7_type_name(apArg[0])` |
|       - | 2113 | `			);` |
|       - | 2114 | `	}` |
|     137 | 2115 | `	for(i = 1 ; i < nArg ; i++){` |
|      71 | 2116 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 2117 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2118 | `				"TypeError",` |
|       - | 2119 | `				"array_diff(): Argument #%d must be of type array, %s given",` |
|       1 | 2120 | `				i + 1,` |
|       2 | 2121 | `				ph7_type_name(apArg[i])` |
|       - | 2122 | `				);` |
|       - | 2123 | `		}` |
|      35 | 2124 | `	}` |
|       - | 2125 | `	/* php sorts every input array before diffing, which string-coerces each` |
|       - | 2126 | `	 * element exactly once — that is where its "Array to string conversion"` |
|       - | 2127 | `	 * warnings come from, and why a not-stringable object throws even when an` |
|       - | 2128 | `	 * earlier element already matched. Do that pass first, USER-VISIBLY, so the` |
|       - | 2129 | `	 * comparisons below can render silently (see HashmapValueStrEq).` |
|       - | 2130 | `	 * It runs BEFORE the one-argument shortcut on purpose: php sorts even then,` |
|       - | 2131 | ``	 * so `array_diff([[1]])` warns while `array_intersect([[1]])` — whose sort php`` |
|       - | 2132 | `	 * skips — does not. Asymmetric, and matched deliberately. */` |
|     192 | 2133 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     132 | 2134 | `		sxi32 rcStr = HashmapStringifyElems((ph7_hashmap *)apArg[i]->x.pOther);` |
|     132 | 2135 | `		if( rcStr != SXRET_OK ){` |
|       7 | 2136 | `			pCtx->nThrowRc = rcStr;` |
|       7 | 2137 | `			return rcStr;` |
|       - | 2138 | `		}` |
|      64 | 2139 | `	}` |
|      62 | 2140 | `	if( nArg == 1 ){` |
|       - | 2141 | `		/* Return the first array since we cannot perform a diff */` |
|       6 | 2142 | `		ph7_result_value(pCtx,apArg[0]);` |
|       6 | 2143 | `		return PH7_OK;` |
|       - | 2144 | `	}` |
|       - | 2145 | `	/* Create a new array */` |
|      58 | 2146 | `	pArray = ph7_context_new_array(pCtx);` |
|      58 | 2147 | `	if( pArray == 0 ){` |
|     ! 0 | 2148 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2149 | `		return PH7_OK;` |
|       - | 2150 | `	}` |
|       - | 2151 | `	/* Point to the internal representation of the source hashmap */` |
|      58 | 2152 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2153 | `	/* Perform the diff */` |
|      58 | 2154 | `	pEntry = pSrc->pFirst;` |
|      58 | 2155 | `	n = pSrc->nEntry;` |
|      89 | 2156 | `	for(;;){` |
|     180 | 2157 | `		if( n < 1 ){` |
|      58 | 2158 | `			break;` |
|       - | 2159 | `		}` |
|       - | 2160 | `		/* Extract the node value */` |
|     124 | 2161 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|     124 | 2162 | `		if( pVal ){` |
|     192 | 2163 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 2164 | `				sxi32 rcStr;` |
|       - | 2165 | `				/* Point to the internal representation of the hashmap */` |
|     132 | 2166 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2167 | `				/* Perform the lookup */` |
|     132 | 2168 | `				rc = HashmapFindStringValue(pMap,pVal,0,&rcStr);` |
|     132 | 2169 | `				if( rcStr != SXRET_OK ){` |
|     ! 0 | 2170 | `					pCtx->nThrowRc = rcStr;` |
|     ! 0 | 2171 | `					return rcStr;` |
|       - | 2172 | `				}` |
|     132 | 2173 | `				if( rc == SXRET_OK ){` |
|       - | 2174 | `					/* Value exist */` |
|      64 | 2175 | `					break;` |
|       - | 2176 | `				}` |
|      36 | 2177 | `			}` |
|     124 | 2178 | `			if( i >= nArg ){` |
|       - | 2179 | `				/* Perform the insertion */` |
|      62 | 2180 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      30 | 2181 | `			}` |
|      61 | 2182 | `		}` |
|       - | 2183 | `		/* Point to the next entry */` |
|     124 | 2184 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     124 | 2185 | `		n--;` |
|       2 | 2186 | `	}` |
|       - | 2187 | `	/* Return the freshly created array */` |
|      58 | 2188 | `	ph7_result_value(pCtx,pArray);` |
|      58 | 2189 | `	return PH7_OK;` |
|      38 | 2190 | `}` |
|       - | 2191 | `/*` |
|       - | 2192 | ` * array array_udiff(array $array1,array $array2,...,$callback)` |
|       - | 2193 | ` *  Computes the difference of arrays by using a callback function for data comparison.` |
|       - | 2194 | ` * Parameters` |
|       - | 2195 | ` *  $array1` |
|       - | 2196 | ` *    The array to compare from` |
|       - | 2197 | ` *  $array2` |
|       - | 2198 | ` *    An array to compare against` |
|       - | 2199 | ` *  $...` |
|       - | 2200 | ` *   More arrays to compare against.` |
|       - | 2201 | ` * $callback` |
|       - | 2202 | ` *  The callback comparison function.` |
|       - | 2203 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 2204 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 2205 | ` *  than the second.` |
|       - | 2206 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 2207 | ` * Return` |
|       - | 2208 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 2209 | ` *  are not present in any of the other arrays.` |
|       - | 2210 | ` */` |
|      28 | 2211 | `PH7_PRIVATE int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2212 | `{` |
|       - | 2213 | `	ph7_hashmap_node *pEntry;` |
|       - | 2214 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2215 | `	ph7_value *pCallback;` |
|       - | 2216 | `	ph7_value *pArray;` |
|       - | 2217 | `	ph7_value *pVal;` |
|       - | 2218 | `	sxi32 rc;` |
|       - | 2219 | `	sxu32 n;` |
|       - | 2220 | `	int i;` |
|       - | 2221 |  |
|       - | 2222 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      33 | 2223 | `	if( nArg < 2 ){` |
|     ! 0 | 2224 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2225 | `			"ArgumentCountError",` |
|       - | 2226 | `			"array_udiff() expects at least 2 arguments, %d given",` |
|     ! 0 | 2227 | `			nArg` |
|       - | 2228 | `			);` |
|       - | 2229 | `	}` |
|      33 | 2230 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2231 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2232 | `			"TypeError",` |
|       - | 2233 | `			"array_udiff(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2234 | `			ph7_type_name(apArg[0])` |
|       - | 2235 | `			);` |
|       - | 2236 | `	}` |
|       - | 2237 |  |
|       - | 2238 | `	/* php validates the CALLBACK (the last argument) before the intermediary` |
|       - | 2239 | ``	 * arrays: `array_udiff([1],"x",123)` reports Argument #3 (the bad callback),`` |
|       - | 2240 | `	 * not Argument #2 (the non-array). PHL had the middle-array loop first, so it` |
|       - | 2241 | `	 * named the wrong argument whenever both were invalid. */` |
|      31 | 2242 | `	pCallback = apArg[nArg - 1];` |
|       - | 2243 | `	/* php names the reason it cannot be called; one shared builder answers for every` |
|       - | 2244 | `	 * callback argument (vm_arg_check.c). */` |
|       - | 2245 | `	{` |
|      31 | 2246 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,pCallback,nArg,0,FALSE);` |
|      31 | 2247 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 2248 | `	}` |
|       - | 2249 |  |
|       - | 2250 | `	/* Now the intermediary arguments (arrays), left to right. */` |
|      25 | 2251 | `	for( i = 1 ; i < nArg - 1; i++ ){` |
|      15 | 2252 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       7 | 2253 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2254 | `				"TypeError",` |
|       - | 2255 | `				"array_udiff(): Argument #%d must be of type array, %s given",` |
|       2 | 2256 | `				i + 1,` |
|       4 | 2257 | `				ph7_type_name(apArg[i])` |
|       - | 2258 | `				);` |
|       - | 2259 | `		}` |
|       7 | 2260 | `	}` |
|       - | 2261 |  |
|      13 | 2262 | `	if( nArg == 2 ){` |
|       - | 2263 | `		/* Only the original array and the callback were provided. */` |
|       3 | 2264 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2265 | `		return PH7_OK;` |
|       - | 2266 | `	}` |
|       - | 2267 |  |
|       - | 2268 | `	/* Create a new array */` |
|      11 | 2269 | `	pArray = ph7_context_new_array(pCtx);` |
|      11 | 2270 | `	if( pArray == 0 ){` |
|     ! 0 | 2271 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2272 | `		return PH7_OK;` |
|       - | 2273 | `	}` |
|       - | 2274 | `	/* Point to the internal representation of the source hashmap */` |
|      11 | 2275 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2276 | `	/* Perform the diff */` |
|      11 | 2277 | `	pEntry = pSrc->pFirst;` |
|      11 | 2278 | `	n = pSrc->nEntry;` |
|      11 | 2279 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|      13 | 2280 | `	for(;;){` |
|      29 | 2281 | `		if( n < 1 ){` |
|       9 | 2282 | `			break;` |
|       - | 2283 | `		}` |
|       - | 2284 | `		/* Extract the node value */` |
|      23 | 2285 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      23 | 2286 | `		if( pVal ){` |
|      35 | 2287 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 2288 | `				/* Point to the internal representation of the hashmap */` |
|      23 | 2289 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2290 | `				/* Perform the lookup */` |
|      23 | 2291 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|      23 | 2292 | `				if( rc == SXRET_OK ){` |
|       - | 2293 | `					/* Value exist */` |
|      11 | 2294 | `					break;` |
|       - | 2295 | `				}` |
|       9 | 2296 | `			}` |
|      23 | 2297 | `			if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 2298 | `				/* The comparison callback raised: propagate so the dispatcher` |
|       - | 2299 | `				 * unwinds, before any spurious insertion into the result. */` |
|       3 | 2300 | `				pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 2301 | `				return PH7_EXCEPTION;` |
|       - | 2302 | `			}` |
|      21 | 2303 | `			if( i >= (nArg - 1)){` |
|       - | 2304 | `				/* Perform the insertion */` |
|      13 | 2305 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       5 | 2306 | `			}` |
|       9 | 2307 | `		}` |
|       - | 2308 | `		/* Point to the next entry */` |
|      21 | 2309 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      21 | 2310 | `		n--;` |
|       3 | 2311 | `	}` |
|       - | 2312 | `	/* Return the freshly created array */` |
|       9 | 2313 | `	ph7_result_value(pCtx,pArray);` |
|       9 | 2314 | `	return PH7_OK;` |
|      19 | 2315 | `}` |
|       - | 2316 | `/*` |
|       - | 2317 | ` * array array_diff_assoc(array $array1,array $array2,...)` |
|       - | 2318 | ` *  Computes the difference of arrays with additional index check.` |
|       - | 2319 | ` * Parameters` |
|       - | 2320 | ` *  $array1` |
|       - | 2321 | ` *    The array to compare from` |
|       - | 2322 | ` *  $array2` |
|       - | 2323 | ` *    An array to compare against` |
|       - | 2324 | ` *  $...` |
|       - | 2325 | ` *   More arrays to compare against` |
|       - | 2326 | ` * Return` |
|       - | 2327 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 2328 | ` *  are not present in any of the other arrays.` |
|       - | 2329 | ` */` |
|      36 | 2330 | `PH7_PRIVATE int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 2331 | `{` |
|       - | 2332 | `	ph7_hashmap_node *pN1,*pN2,*pEntry;` |
|       - | 2333 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2334 | `	ph7_value *pArray;` |
|       - | 2335 | `	ph7_value *pVal;` |
|       - | 2336 | `	sxi32 rc;` |
|       - | 2337 | `	sxu32 n;` |
|       - | 2338 | `	int i;` |
|       - | 2339 | `	/* Ensure the argument list is valid, emitting the same errors PHP` |
|       - | 2340 | `	 * would produce. This makes behaviour predictable and allows the` |
|       - | 2341 | `	 * accompanying integration tests to pass. */` |
|      40 | 2342 | `	if( nArg < 1 ){` |
|     ! 0 | 2343 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2344 | `			"ArgumentCountError",` |
|       - | 2345 | `			"array_diff_assoc() expects at least 1 argument, %d given",` |
|     ! 0 | 2346 | `			nArg` |
|       - | 2347 | `			);` |
|       - | 2348 | `	}` |
|      40 | 2349 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2350 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2351 | `			"TypeError",` |
|       - | 2352 | `			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2353 | `			ph7_type_name(apArg[0])` |
|       - | 2354 | `			);` |
|       - | 2355 | `	}` |
|      69 | 2356 | `	for(i = 1 ; i < nArg ; i++){` |
|      39 | 2357 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       8 | 2358 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2359 | `				"TypeError",` |
|       - | 2360 | `				"array_diff_assoc(): Argument #%d must be of type array, %s given",` |
|       2 | 2361 | `				i + 1,` |
|       4 | 2362 | `				ph7_type_name(apArg[i])` |
|       - | 2363 | `				);` |
|       - | 2364 | `		}` |
|      19 | 2365 | `	}` |
|      32 | 2366 | `	if( nArg == 1 ){` |
|       - | 2367 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 2368 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2369 | `		return PH7_OK;` |
|       - | 2370 | `	}` |
|       - | 2371 | `	/* Create a new array */` |
|      30 | 2372 | `	pArray = ph7_context_new_array(pCtx);` |
|      30 | 2373 | `	if( pArray == 0 ){` |
|     ! 0 | 2374 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2375 | `		return PH7_OK;` |
|       - | 2376 | `	}` |
|       - | 2377 | `	/* Point to the internal representation of the source hashmap */` |
|      30 | 2378 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2379 | `	/* Perform the diff */` |
|      30 | 2380 | `	pEntry = pSrc->pFirst;` |
|      30 | 2381 | `	n = pSrc->nEntry;` |
|      30 | 2382 | `	pN1 = pN2 = 0;` |
|      62 | 2383 | `	for(;;){` |
|       - | 2384 | `		int keep;` |
|      78 | 2385 | `		if( n < 1 ){` |
|      28 | 2386 | `			break;` |
|       - | 2387 | `		}` |
|       - | 2388 | `		/* assume the element should be kept until we find a match */` |
|      52 | 2389 | `		keep = 1;` |
|      76 | 2390 | `		for( i = 1 ; i < nArg ; i++ ){` |
|       - | 2391 | `			/* all arguments have been validated already, so cast directly */` |
|      56 | 2392 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2393 | `			/* Perform a key lookup first */` |
|      56 | 2394 | `			if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      18 | 2395 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|      10 | 2396 | `			}else{` |
|      40 | 2397 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 2398 | `			}` |
|      56 | 2399 | `			if( rc != SXRET_OK ){` |
|       - | 2400 | `				/* this array does not contain the key, continue checking others */` |
|      24 | 2401 | `				continue;` |
|       - | 2402 | `			}` |
|       - | 2403 | `			/* key exists; check that value stored in the matching node is equal */` |
|      34 | 2404 | `			pVal = HashmapExtractNodeValue(pEntry);` |
|      34 | 2405 | `			if( pVal ){` |
|       - | 2406 | `				/* directly compare with value at pN1 rather than searching again */` |
|      34 | 2407 | `				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      34 | 2408 | `				if( pVal2 ){` |
|       - | 2409 | `					sxi32 rcStr;` |
|       - | 2410 | `					/* php compares the two values as (string)$a === (string)$b` |
|       - | 2411 | `					 * (HashmapValueStrEq, which works on copies — these are LIVE` |
|       - | 2412 | `					 * array elements). It converts LAZILY, only for a key that` |
|       - | 2413 | `					 * matched, so a not-stringable object under a key nobody else` |
|       - | 2414 | `					 * has never throws. */` |
|      34 | 2415 | `					int bEq = HashmapValueStrEq(pVal,pVal2,/*bUserVisible*/1,&rcStr);` |
|      34 | 2416 | `					if( rcStr != SXRET_OK ){` |
|       3 | 2417 | `						pCtx->nThrowRc = rcStr;` |
|       3 | 2418 | `						return rcStr;` |
|       - | 2419 | `					}` |
|      32 | 2420 | `					if( bEq ){` |
|       - | 2421 | `						/* identical key+value found in one of the arrays => drop it */` |
|      30 | 2422 | `						keep = 0;` |
|      30 | 2423 | `						break;` |
|       - | 2424 | `					}` |
|       1 | 2425 | `				}` |
|       1 | 2426 | `			}` |
|       2 | 2427 | `		}` |
|      50 | 2428 | `		if( keep ){` |
|       - | 2429 | `			/* Perform the insertion */` |
|      22 | 2430 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      10 | 2431 | `		}` |
|       - | 2432 | `		/* Point to the next entry */` |
|      50 | 2433 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      50 | 2434 | `		n--;` |
|       2 | 2435 | `	}` |
|       - | 2436 | `	/* Return the freshly created array */` |
|      28 | 2437 | `	ph7_result_value(pCtx,pArray);` |
|      28 | 2438 | `	return PH7_OK;` |
|      22 | 2439 | `}` |
|       - | 2440 | `/*` |
|       - | 2441 | ` * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)` |
|       - | 2442 | ` *  Computes the difference of arrays with additional index check which is performed` |
|       - | 2443 | ` *  by a user supplied callback function.` |
|       - | 2444 | ` * Parameters` |
|       - | 2445 | ` *  $array1` |
|       - | 2446 | ` *    The array to compare from` |
|       - | 2447 | ` *  $array2` |
|       - | 2448 | ` *    An array to compare against` |
|       - | 2449 | ` *  $...` |
|       - | 2450 | ` *   More arrays to compare against.` |
|       - | 2451 | ` *  $key_compare_func` |
|       - | 2452 | ` *   Callback function to use. The callback function must return an integer` |
|       - | 2453 | ` *   less than, equal to, or greater than zero if the first argument is considered` |
|       - | 2454 | ` *   to be respectively less than, equal to, or greater than the second.` |
|       - | 2455 | ` * Return` |
|       - | 2456 | ` *  Returns an array containing all the entries from array1 that` |
|       - | 2457 | ` *  are not present in any of the other arrays.` |
|       - | 2458 | ` */` |
|      32 | 2459 | `PH7_PRIVATE int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2460 | `{` |
|       - | 2461 | `	ph7_hashmap_node *pEntry;` |
|       - | 2462 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2463 | `	ph7_value *pCallback;` |
|       - | 2464 | `	ph7_value *pArray;` |
|       - | 2465 | `	sxi32 rc;` |
|       - | 2466 | `	sxu32 n;` |
|       - | 2467 | `	int i;` |
|       - | 2468 |  |
|       - | 2469 | `	/* Argument validation mimicking PHP errors. */` |
|      37 | 2470 | `	if( nArg < 2 ){` |
|     ! 0 | 2471 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2472 | `			"ArgumentCountError",` |
|       - | 2473 | `			"array_diff_uassoc() expects at least 2 arguments, %d given",` |
|     ! 0 | 2474 | `			nArg` |
|       - | 2475 | `			);` |
|       - | 2476 | `	}` |
|      37 | 2477 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2478 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2479 | `			"TypeError",` |
|       - | 2480 | `			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2481 | `			ph7_type_name(apArg[0])` |
|       - | 2482 | `			);` |
|       - | 2483 | `	}` |
|       - | 2484 | `	/* Intermediate arguments (except last) must be arrays. Last argument is` |
|       - | 2485 | `	 * expected to be a callback. */` |
|       - | 2486 | `	/* php checks the CALLBACK before the intermediary arrays (see array_udiff). */` |
|      35 | 2487 | `	pCallback = apArg[nArg - 1];` |
|       - | 2488 | `	{` |
|      35 | 2489 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,pCallback,nArg,0,FALSE);` |
|      35 | 2490 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 2491 | `	}` |
|       - | 2492 | `	/* Now the intermediary arrays, left to right. */` |
|      41 | 2493 | `	for(i = 1 ; i < nArg - 1; i++){` |
|      23 | 2494 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 2495 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2496 | `				"TypeError",` |
|       - | 2497 | `				"array_diff_uassoc(): Argument #%d must be of type array, %s given",` |
|       1 | 2498 | `				i + 1,` |
|       2 | 2499 | `				ph7_type_name(apArg[i])` |
|       - | 2500 | `				);` |
|       - | 2501 | `		}` |
|      11 | 2502 | `	}` |
|      20 | 2503 | `	if( nArg == 2 ){` |
|       - | 2504 | `		/* If we only have the first array and the callback, just return the` |
|       - | 2505 | `		 * input array. */` |
|       3 | 2506 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2507 | `		return PH7_OK;` |
|       - | 2508 | `	}` |
|       - | 2509 | `	/* Create a new array */` |
|      18 | 2510 | `	pArray = ph7_context_new_array(pCtx);` |
|      18 | 2511 | `	if( pArray == 0 ){` |
|     ! 0 | 2512 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2513 | `		return PH7_OK;` |
|       - | 2514 | `	}` |
|       - | 2515 | `	/* Point to the internal representation of the source hashmap */` |
|      18 | 2516 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2517 | `	/* Perform the diff */` |
|      18 | 2518 | `	pEntry = pSrc->pFirst;` |
|      18 | 2519 | `	n = pSrc->nEntry;` |
|      30 | 2520 | `	for(;;){` |
|       - | 2521 | `		int keep;` |
|      40 | 2522 | `		if( n < 1 ){` |
|      16 | 2523 | `			break;` |
|       - | 2524 | `		}` |
|      26 | 2525 | `		keep = 1;` |
|      40 | 2526 | `		for( i = 1 ; i < nArg - 1; i++ ){` |
|       - | 2527 | `			/* each of these must already be arrays thanks to earlier validation */` |
|      30 | 2528 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2529 | `			/* we must compare keys via callback, not by direct lookup */` |
|      30 | 2530 | `			ph7_hashmap_node *pIt = pMap->pFirst;` |
|      54 | 2531 | `			while( pIt ){` |
|       - | 2532 | `				/* build temporary key values for callback */` |
|       - | 2533 | `				ph7_value key1, key2, result;` |
|       - | 2534 | `				/* initialise only once using the appropriate helper */` |
|      40 | 2535 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 2536 | `					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);` |
|     ! 0 | 2537 | `				}else{` |
|       - | 2538 | `					SyString sStr;` |
|      40 | 2539 | `					SyStringInitFromBuf(&sStr,` |
|       - | 2540 | `						SyBlobData(&pEntry->xKey.sKey),` |
|       - | 2541 | `						SyBlobLength(&pEntry->xKey.sKey));` |
|      40 | 2542 | `					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);` |
|       - | 2543 | `				}` |
|      40 | 2544 | `				if( pIt->iType == HASHMAP_INT_NODE ){` |
|     ! 0 | 2545 | `					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);` |
|     ! 0 | 2546 | `				}else{` |
|       - | 2547 | `					SyString sStr;` |
|      40 | 2548 | `					SyStringInitFromBuf(&sStr,` |
|       - | 2549 | `						SyBlobData(&pIt->xKey.sKey),` |
|       - | 2550 | `						SyBlobLength(&pIt->xKey.sKey));` |
|      40 | 2551 | `					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);` |
|       - | 2552 | `				}` |
|      40 | 2553 | `				PH7_MemObjInit(pMap->pVm,&result);` |
|       - | 2554 | `				/* call user callback with (key1, key2) */` |
|       - | 2555 | `				{` |
|       - | 2556 | `					ph7_value *apK[2];` |
|      40 | 2557 | `					apK[0] = &key1;` |
|      40 | 2558 | `					apK[1] = &key2;` |
|      40 | 2559 | `					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);` |
|       - | 2560 | `				}` |
|      40 | 2561 | `				if( rc == PH7_EXCEPTION ){` |
|       - | 2562 | `					/* The key comparison callback raised. Unlike array_udiff/` |
|       - | 2563 | `					 * array_uintersect (which signal back from` |
|       - | 2564 | `					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this` |
|       - | 2565 | `					 * function invokes the callback inline, so it cleans up its own` |
|       - | 2566 | `					 * temporaries and propagates the exception directly. */` |
|       3 | 2567 | `					PH7_MemObjRelease(&result);` |
|       3 | 2568 | `					PH7_MemObjRelease(&key1);` |
|       3 | 2569 | `					PH7_MemObjRelease(&key2);` |
|       3 | 2570 | `					return PH7_EXCEPTION;` |
|       - | 2571 | `				}` |
|      38 | 2572 | `				if( rc == SXRET_OK ){` |
|      38 | 2573 | `					if( (result.iFlags & MEMOBJ_INT) == 0 ){` |
|     ! 0 | 2574 | `						PH7_MemObjToInteger(&result);` |
|     ! 0 | 2575 | `					}` |
|      38 | 2576 | `					if( result.x.iVal == 0 ){` |
|       - | 2577 | `						/* keys considered equal by callback; now compare values */` |
|      20 | 2578 | `						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);` |
|      20 | 2579 | `						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);` |
|      20 | 2580 | `						if( pVal1 && pVal2 ){` |
|       - | 2581 | `							sxi32 rcStr;` |
|       - | 2582 | `							/* Only the KEYS go through the callback here; the VALUES` |
|       - | 2583 | `							 * take php's own array_diff comparison,` |
|       - | 2584 | `							 * (string)$a === (string)$b (HashmapValueStrEq, on` |
|       - | 2585 | `							 * copies — these are LIVE array elements). */` |
|      20 | 2586 | `							int bEq = HashmapValueStrEq(pVal1,pVal2,/*bUserVisible*/1,&rcStr);` |
|      20 | 2587 | `							if( rcStr != SXRET_OK ){` |
|     ! 0 | 2588 | `								PH7_MemObjRelease(&result);` |
|     ! 0 | 2589 | `								PH7_MemObjRelease(&key1);` |
|     ! 0 | 2590 | `								PH7_MemObjRelease(&key2);` |
|     ! 0 | 2591 | `								pCtx->nThrowRc = rcStr;` |
|     ! 0 | 2592 | `								return rcStr;` |
|       - | 2593 | `							}` |
|      20 | 2594 | `							if( bEq ){` |
|      14 | 2595 | `								keep = 0;` |
|      14 | 2596 | `								PH7_MemObjRelease(&result);` |
|       - | 2597 | `								/* release keys too before breaking */` |
|      14 | 2598 | `								PH7_MemObjRelease(&key1);` |
|      14 | 2599 | `								PH7_MemObjRelease(&key2);` |
|      14 | 2600 | `								break;` |
|       - | 2601 | `							}` |
|       3 | 2602 | `						}` |
|       3 | 2603 | `					}` |
|      12 | 2604 | `				}` |
|      26 | 2605 | `				PH7_MemObjRelease(&result);` |
|      26 | 2606 | `				PH7_MemObjRelease(&key1);` |
|      26 | 2607 | `				PH7_MemObjRelease(&key2);` |
|       - | 2608 | `				/* move to next node */` |
|      26 | 2609 | `				pIt = pIt->pPrev;` |
|      26 | 2610 | `				if( keep == 0 ) break;` |
|       2 | 2611 | `			}` |
|      28 | 2612 | `			if( keep == 0 ) break;` |
|       9 | 2613 | `		}` |
|      24 | 2614 | `		if( keep ){` |
|       - | 2615 | `			/* Perform the insertion */` |
|      12 | 2616 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       5 | 2617 | `		}` |
|       - | 2618 | `		/* Point to the next entry */` |
|      24 | 2619 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      24 | 2620 | `		n--;` |
|       2 | 2621 | `	}` |
|       - | 2622 | `	/* Return the freshly created array */` |
|      16 | 2623 | `	ph7_result_value(pCtx,pArray);` |
|      16 | 2624 | `	return PH7_OK;` |
|      21 | 2625 | `}` |
|       - | 2626 | `/*` |
|       - | 2627 | ` * array array_diff_key(array $array1 ,array $array2,...)` |
|       - | 2628 | ` *  Computes the difference of arrays using keys for comparison.` |
|       - | 2629 | ` * Parameters` |
|       - | 2630 | ` *  $array1` |
|       - | 2631 | ` *    The array to compare from` |
|       - | 2632 | ` *  $array2` |
|       - | 2633 | ` *    An array to compare against` |
|       - | 2634 | ` *  $...` |
|       - | 2635 | ` *   More arrays to compare against` |
|       - | 2636 | ` * Return` |
|       - | 2637 | ` *  Returns an array containing all the entries from array1 whose keys are not present` |
|       - | 2638 | ` *  in any of the other arrays.` |
|       - | 2639 | ` * Note that NULL is returned on failure.` |
|       - | 2640 | ` */` |
|      16 | 2641 | `PH7_PRIVATE int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2642 | `{` |
|       - | 2643 | `	ph7_hashmap_node *pEntry;` |
|       - | 2644 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2645 | `	ph7_value *pArray;` |
|       - | 2646 | `	sxi32 rc;` |
|       - | 2647 | `	sxu32 n;` |
|       - | 2648 | `	int i;` |
|       - | 2649 | `	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs` |
|       - | 2650 | `	 * would quietly return NULL which is inconsistent with other hashmap` |
|       - | 2651 | `	 * helpers. */` |
|      19 | 2652 | `	if( nArg < 1 ){` |
|     ! 0 | 2653 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2654 | `			"ArgumentCountError",` |
|       - | 2655 | `			"array_diff_key() expects at least 1 argument, %d given",` |
|     ! 0 | 2656 | `			nArg` |
|       - | 2657 | `			);` |
|       - | 2658 | `	}` |
|      19 | 2659 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2660 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2661 | `			"TypeError",` |
|       - | 2662 | `			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2663 | `			ph7_type_name(apArg[0])` |
|       - | 2664 | `			);` |
|       - | 2665 | `	}` |
|      28 | 2666 | `	for(i = 1 ; i < nArg ; i++){` |
|      16 | 2667 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 2668 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2669 | `				"TypeError",` |
|       - | 2670 | `				"array_diff_key(): Argument #%d must be of type array, %s given",` |
|       1 | 2671 | `				i + 1,` |
|       2 | 2672 | `				ph7_type_name(apArg[i])` |
|       - | 2673 | `				);` |
|       - | 2674 | `		}` |
|       8 | 2675 | `	}` |
|      14 | 2676 | `	if( nArg == 1 ){` |
|       - | 2677 | `		/* Return the first array since we cannot perform a diff */` |
|       3 | 2678 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2679 | `		return PH7_OK;` |
|       - | 2680 | `	}` |
|       - | 2681 | `	/* Create a new array */` |
|      12 | 2682 | `	pArray = ph7_context_new_array(pCtx);` |
|      12 | 2683 | `	if( pArray == 0 ){` |
|     ! 0 | 2684 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2685 | `		return PH7_OK;` |
|       - | 2686 | `	}` |
|       - | 2687 | `	/* Point to the internal representation of the main hashmap */` |
|      12 | 2688 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2689 | `	/* Perfrom the diff */` |
|      12 | 2690 | `	pEntry = pSrc->pFirst;` |
|      12 | 2691 | `	n = pSrc->nEntry;` |
|     272 | 2692 | `	for(;;){` |
|     546 | 2693 | `		if( n < 1 ){` |
|      12 | 2694 | `			break;` |
|       - | 2695 | `		}` |
|    1054 | 2696 | `		for( i = 1 ; i < nArg ; i++ ){` |
|     540 | 2697 | `			if( !ph7_value_is_array(apArg[i])) {` |
|       - | 2698 | `				/* ignore */` |
|     ! 0 | 2699 | `				continue;` |
|       - | 2700 | `			}` |
|     540 | 2701 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|     540 | 2702 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      22 | 2703 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 2704 | `				/* Blob lookup */` |
|      22 | 2705 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      12 | 2706 | `			}else{` |
|       - | 2707 | `				/* Int lookup */` |
|     519 | 2708 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 2709 | `			}` |
|     540 | 2710 | `			if( rc == SXRET_OK ){` |
|       - | 2711 | `				/* Key exists,break immediately */` |
|      22 | 2712 | `				break;` |
|       - | 2713 | `			}` |
|     261 | 2714 | `		}` |
|     536 | 2715 | `		if( i >= nArg ){` |
|       - | 2716 | `			/* Perform the insertion */` |
|     516 | 2717 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|     257 | 2718 | `		}` |
|       - | 2719 | `		/* Point to the next entry */` |
|     536 | 2720 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|     536 | 2721 | `		n--;` |
|       2 | 2722 | `	}` |
|       - | 2723 | `	/* Return the freshly created array */` |
|      12 | 2724 | `	ph7_result_value(pCtx,pArray);` |
|      12 | 2725 | `	return PH7_OK;` |
|      11 | 2726 | `}` |
|       - | 2727 | `/*` |
|       - | 2728 | ` * array array_intersect(array $array1 ,array $array2,...)` |
|       - | 2729 | ` *  Computes the intersection of arrays.` |
|       - | 2730 | ` * Parameters` |
|       - | 2731 | ` *  $array1` |
|       - | 2732 | ` *    The array to compare from` |
|       - | 2733 | ` *  $array2` |
|       - | 2734 | ` *    An array to compare against` |
|       - | 2735 | ` *  $...` |
|       - | 2736 | ` *   More arrays to compare against` |
|       - | 2737 | ` * Return` |
|       - | 2738 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 2739 | ` *  in all of the parameters.` |
|       - | 2740 | ` * Throws ArgumentCountError if no arguments are given.` |
|       - | 2741 | ` * Throws TypeError if any argument is not an array.` |
|       - | 2742 | ` */` |
|      32 | 2743 | `PH7_PRIVATE int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2744 | `{` |
|       - | 2745 | `	ph7_hashmap_node *pEntry;` |
|       - | 2746 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2747 | `	ph7_value *pArray;` |
|       - | 2748 | `	ph7_value *pVal;` |
|       - | 2749 | `	sxi32 rc;` |
|       - | 2750 | `	sxu32 n;` |
|       - | 2751 | `	int i;` |
|      35 | 2752 | `	if( nArg < 1 ){` |
|     ! 0 | 2753 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2754 | `			"ArgumentCountError",` |
|       - | 2755 | `			"array_intersect() expects at least 1 argument, %d given",` |
|     ! 0 | 2756 | `			nArg` |
|       - | 2757 | `			);` |
|       - | 2758 | `	}` |
|      35 | 2759 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2760 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2761 | `			"TypeError",` |
|       - | 2762 | `			"array_intersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2763 | `			ph7_type_name(apArg[0])` |
|       - | 2764 | `			);` |
|       - | 2765 | `	}` |
|      61 | 2766 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      33 | 2767 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 2768 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2769 | `				"TypeError",` |
|       - | 2770 | `				"array_intersect(): Argument #%d must be of type array, %s given",` |
|       1 | 2771 | `				i + 1,` |
|       2 | 2772 | `				ph7_type_name(apArg[i])` |
|       - | 2773 | `				);` |
|       - | 2774 | `		}` |
|      16 | 2775 | `	}` |
|      30 | 2776 | `	if( nArg == 1 ){` |
|       - | 2777 | `		/* Return the first array since we cannot perform a diff */` |
|       6 | 2778 | `		ph7_result_value(pCtx,apArg[0]);` |
|       6 | 2779 | `		return PH7_OK;` |
|       - | 2780 | `	}` |
|       - | 2781 | `	/* Create a new array */` |
|      26 | 2782 | `	pArray = ph7_context_new_array(pCtx);` |
|      26 | 2783 | `	if( pArray == 0 ){` |
|     ! 0 | 2784 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2785 | `		return PH7_OK;` |
|       - | 2786 | `	}` |
|       - | 2787 | `	/* Same pre-pass as array_diff: php's sort of every input array is what` |
|       - | 2788 | `	 * converts each element once (see HashmapStringifyElems). */` |
|      76 | 2789 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      54 | 2790 | `		sxi32 rcStr = HashmapStringifyElems((ph7_hashmap *)apArg[i]->x.pOther);` |
|      54 | 2791 | `		if( rcStr != SXRET_OK ){` |
|       3 | 2792 | `			pCtx->nThrowRc = rcStr;` |
|       3 | 2793 | `			return rcStr;` |
|       - | 2794 | `		}` |
|      27 | 2795 | `	}` |
|       - | 2796 | `	/* Point to the internal representation of the source hashmap */` |
|      24 | 2797 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2798 | `	/* Perform the intersection */` |
|      24 | 2799 | `	pEntry = pSrc->pFirst;` |
|      24 | 2800 | `	n = pSrc->nEntry;` |
|      43 | 2801 | `	for(;;){` |
|      88 | 2802 | `		if( n < 1 ){` |
|      24 | 2803 | `			break;` |
|       - | 2804 | `		}` |
|       - | 2805 | `		/* Extract the node value */` |
|      66 | 2806 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      66 | 2807 | `		if( pVal ){` |
|     108 | 2808 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 2809 | `				sxi32 rcStr;` |
|       - | 2810 | `				/* Point to the internal representation of the hashmap */` |
|      76 | 2811 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2812 | `				/* Perform the lookup */` |
|      76 | 2813 | `				rc = HashmapFindStringValue(pMap,pVal,0,&rcStr);` |
|      76 | 2814 | `				if( rcStr != SXRET_OK ){` |
|     ! 0 | 2815 | `					pCtx->nThrowRc = rcStr;` |
|     ! 0 | 2816 | `					return rcStr;` |
|       - | 2817 | `				}` |
|      76 | 2818 | `				if( rc != SXRET_OK ){` |
|       - | 2819 | `					/* Value does not exist */` |
|      34 | 2820 | `					break;` |
|       - | 2821 | `				}` |
|      23 | 2822 | `			}` |
|      66 | 2823 | `			if( i >= nArg ){` |
|       - | 2824 | `				/* Perform the insertion */` |
|      34 | 2825 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      16 | 2826 | `			}` |
|      32 | 2827 | `		}` |
|       - | 2828 | `		/* Point to the next entry */` |
|      66 | 2829 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      66 | 2830 | `		n--;` |
|       2 | 2831 | `	}` |
|       - | 2832 | `	/* Return the freshly created array */` |
|      24 | 2833 | `	ph7_result_value(pCtx,pArray);` |
|      24 | 2834 | `	return PH7_OK;` |
|      19 | 2835 | `}` |
|       - | 2836 | `/*` |
|       - | 2837 | ` * array array_intersect_assoc(array $array1 ,array $array2,...)` |
|       - | 2838 | ` *  Computes the intersection of arrays with additional index check.` |
|       - | 2839 | ` * Parameters` |
|       - | 2840 | ` *  $array1` |
|       - | 2841 | ` *    The array to compare from` |
|       - | 2842 | ` *  $array2` |
|       - | 2843 | ` *    An array to compare against` |
|       - | 2844 | ` *  $...` |
|       - | 2845 | ` *   More arrays to compare against` |
|       - | 2846 | ` * Return` |
|       - | 2847 | ` *  Returns an array containing all the values of array1 that are present` |
|       - | 2848 | ` *  in all the arguments, with matching keys.` |
|       - | 2849 | ` */` |
|      28 | 2850 | `PH7_PRIVATE int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2851 | `{` |
|       - | 2852 | `	ph7_hashmap_node *pEntry,*pN1,*pN2;` |
|       - | 2853 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2854 | `	ph7_value *pArray;` |
|       - | 2855 | `	ph7_value *pVal;` |
|       - | 2856 | `	sxi32 rc;` |
|       - | 2857 | `	sxu32 n;` |
|       - | 2858 | `	int i;` |
|      31 | 2859 | `	if( nArg < 1 ){` |
|     ! 0 | 2860 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2861 | `			"ArgumentCountError",` |
|       - | 2862 | `			"array_intersect_assoc() expects at least 1 argument, %d given",` |
|     ! 0 | 2863 | `			nArg` |
|       - | 2864 | `			);` |
|       - | 2865 | `	}` |
|      31 | 2866 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2867 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2868 | `			"TypeError",` |
|       - | 2869 | `			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2870 | `			ph7_type_name(apArg[0])` |
|       - | 2871 | `			);` |
|       - | 2872 | `	}` |
|      52 | 2873 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      28 | 2874 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 2875 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2876 | `				"TypeError",` |
|       - | 2877 | `				"array_intersect_assoc(): Argument #%d must be of type array, %s given",` |
|       1 | 2878 | `				i + 1,` |
|       2 | 2879 | `				ph7_type_name(apArg[i])` |
|       - | 2880 | `				);` |
|       - | 2881 | `		}` |
|      14 | 2882 | `	}` |
|      26 | 2883 | `	if( nArg == 1 ){` |
|       - | 2884 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 2885 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 2886 | `		return PH7_OK;` |
|       - | 2887 | `	}` |
|       - | 2888 | `	/* Create a new array */` |
|      24 | 2889 | `	pArray = ph7_context_new_array(pCtx);` |
|      24 | 2890 | `	if( pArray == 0 ){` |
|     ! 0 | 2891 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2892 | `		return PH7_OK;` |
|       - | 2893 | `	}` |
|       - | 2894 | `	/* Point to the internal representation of the source hashmap */` |
|      24 | 2895 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 2896 | `	/* Perform the intersection */` |
|      24 | 2897 | `	pEntry = pSrc->pFirst;` |
|      24 | 2898 | `	n = pSrc->nEntry;` |
|      24 | 2899 | `	pN1 = pN2 = 0; /* cc warning */` |
|      34 | 2900 | `	for(;;){` |
|      70 | 2901 | `		if( n < 1 ){` |
|      24 | 2902 | `			break;` |
|       - | 2903 | `		}` |
|       - | 2904 | `		/* Extract the node value */` |
|      48 | 2905 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      48 | 2906 | `		if( pVal ){` |
|      80 | 2907 | `			for( i = 1 ; i < nArg ; i++ ){` |
|       - | 2908 | `				/* Point to the internal representation of the hashmap */` |
|      52 | 2909 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 2910 | `				/* Perform a key lookup first */` |
|      52 | 2911 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|      18 | 2912 | `					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);` |
|      10 | 2913 | `				}else{` |
|      36 | 2914 | `					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);` |
|       - | 2915 | `				}` |
|      52 | 2916 | `				if( rc != SXRET_OK ){` |
|       - | 2917 | `					/* No such key,break immediately */` |
|       7 | 2918 | `					break;` |
|       - | 2919 | `				}` |
|       - | 2920 | `				/* The key matched, so compare THAT node's value — php compares` |
|       - | 2921 | `				 * (string)$a === (string)$b here (HashmapValueStrEq), and lazily:` |
|       - | 2922 | `				 * a key that matched nowhere never coerces anything. Scanning the` |
|       - | 2923 | `				 * whole map for an equal value and then demanding it be the` |
|       - | 2924 | `				 * key-matched node answered the same question the long way. */` |
|       - | 2925 | `				{` |
|      46 | 2926 | `					ph7_value *pVal2 = HashmapExtractNodeValue(pN1);` |
|      46 | 2927 | `					sxi32 rcStr = SXRET_OK;` |
|      46 | 2928 | `					int bEq = pVal2 != 0 && HashmapValueStrEq(pVal,pVal2,/*bUserVisible*/1,&rcStr);` |
|      46 | 2929 | `					if( rcStr != SXRET_OK ){` |
|     ! 0 | 2930 | `						pCtx->nThrowRc = rcStr;` |
|     ! 0 | 2931 | `						return rcStr;` |
|       - | 2932 | `					}` |
|      46 | 2933 | `					if( !bEq ){` |
|       - | 2934 | `						/* Value does not exist */` |
|      14 | 2935 | `						break;` |
|       - | 2936 | `					}` |
|       - | 2937 | `				}` |
|      18 | 2938 | `			}` |
|      48 | 2939 | `			if( i >= nArg ){` |
|       - | 2940 | `				/* Perform the insertion */` |
|      30 | 2941 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      14 | 2942 | `			}` |
|      23 | 2943 | `		}` |
|       - | 2944 | `		/* Point to the next entry */` |
|      48 | 2945 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      48 | 2946 | `		n--;` |
|       2 | 2947 | `	}` |
|       - | 2948 | `	/* Return the freshly created array */` |
|      24 | 2949 | `	ph7_result_value(pCtx,pArray);` |
|      24 | 2950 | `	return PH7_OK;` |
|      17 | 2951 | `}` |
|       - | 2952 | `/*` |
|       - | 2953 | ` * array array_intersect_key(array $array1 ,...)` |
|       - | 2954 | ` *  Computes the intersection of arrays using keys for comparison.` |
|       - | 2955 | ` * Parameters` |
|       - | 2956 | ` *  $array1` |
|       - | 2957 | ` *    The array to compare from` |
|       - | 2958 | ` *  $...` |
|       - | 2959 | ` *   More arrays to compare against` |
|       - | 2960 | ` * Return` |
|       - | 2961 | ` *  Returns an associative array containing all the entries of array1 which` |
|       - | 2962 | ` *  have keys that are present in all arguments.` |
|       - | 2963 | ` * Note that NULL is returned on failure.` |
|       - | 2964 | ` */` |
|      22 | 2965 | `PH7_PRIVATE int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 2966 | `{` |
|       - | 2967 | `	ph7_hashmap_node *pEntry;` |
|       - | 2968 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 2969 | `	ph7_value *pArray;` |
|       - | 2970 | `	sxi32 rc;` |
|       - | 2971 | `	sxu32 n;` |
|       - | 2972 | `	int i;` |
|      26 | 2973 | `	if( nArg < 1 ){` |
|     ! 0 | 2974 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2975 | `			"ArgumentCountError",` |
|       - | 2976 | `			"array_intersect_key() expects at least 1 argument, %d given",` |
|     ! 0 | 2977 | `			nArg` |
|       - | 2978 | `			);` |
|       - | 2979 | `	}` |
|      26 | 2980 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 2981 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2982 | `			"TypeError",` |
|       - | 2983 | `			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 2984 | `			ph7_type_name(apArg[0])` |
|       - | 2985 | `			);` |
|       - | 2986 | `	}` |
|      41 | 2987 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      23 | 2988 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 2989 | `			return PH7_VmThrowException(pCtx,` |
|       - | 2990 | `				"TypeError",` |
|       - | 2991 | `				"array_intersect_key(): Argument #%d must be of type array, %s given",` |
|       1 | 2992 | `				i + 1,` |
|       2 | 2993 | `				ph7_type_name(apArg[i])` |
|       - | 2994 | `				);` |
|       - | 2995 | `		}` |
|      11 | 2996 | `	}` |
|      20 | 2997 | `	if( nArg == 1 ){` |
|       - | 2998 | `		/* Return the first array since we cannot perform an intersection */` |
|       3 | 2999 | `		ph7_result_value(pCtx,apArg[0]);` |
|       3 | 3000 | `		return PH7_OK;` |
|       - | 3001 | `	}` |
|       - | 3002 | `	/* Create a new array */` |
|      18 | 3003 | `	pArray = ph7_context_new_array(pCtx);` |
|      18 | 3004 | `	if( pArray == 0 ){` |
|     ! 0 | 3005 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3006 | `		return PH7_OK;` |
|       - | 3007 | `	}` |
|       - | 3008 | `	/* Point to the internal representation of the main hashmap */` |
|      18 | 3009 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3010 | `	/* Perform the intersection */` |
|      18 | 3011 | `	pEntry = pSrc->pFirst;` |
|      18 | 3012 | `	n = pSrc->nEntry;` |
|      27 | 3013 | `	for(;;){` |
|      56 | 3014 | `		if( n < 1 ){` |
|      18 | 3015 | `			break;` |
|       - | 3016 | `		}` |
|      64 | 3017 | `		for( i = 1 ; i < nArg ; i++ ){` |
|      44 | 3018 | `			pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      44 | 3019 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      32 | 3020 | `				SyBlob *pKey = &pEntry->xKey.sKey;` |
|       - | 3021 | `				/* Blob lookup */` |
|      32 | 3022 | `				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);` |
|      17 | 3023 | `			}else{` |
|       - | 3024 | `				/* Int key */` |
|      13 | 3025 | `				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);` |
|       - | 3026 | `			}` |
|      44 | 3027 | `			if( rc != SXRET_OK ){` |
|       - | 3028 | `				/* Key does not exist, break immediately */` |
|      20 | 3029 | `				break;` |
|       - | 3030 | `			}` |
|      14 | 3031 | `		}` |
|      40 | 3032 | `		if( i >= nArg ){` |
|       - | 3033 | `			/* Perform the insertion */` |
|      22 | 3034 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      10 | 3035 | `		}` |
|       - | 3036 | `		/* Point to the next entry */` |
|      40 | 3037 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      40 | 3038 | `		n--;` |
|       2 | 3039 | `	}` |
|       - | 3040 | `	/* Return the freshly created array */` |
|      18 | 3041 | `	ph7_result_value(pCtx,pArray);` |
|      18 | 3042 | `	return PH7_OK;` |
|      15 | 3043 | `}` |
|       - | 3044 | `/*` |
|       - | 3045 | ` * array array_uintersect(array $array1 ,array $array2,...,$callback)` |
|       - | 3046 | ` *  Computes the intersection of arrays.` |
|       - | 3047 | ` * Parameters` |
|       - | 3048 | ` *  $array1` |
|       - | 3049 | ` *    The array to compare from` |
|       - | 3050 | ` *  $array2` |
|       - | 3051 | ` *    An array to compare against` |
|       - | 3052 | ` *  $...` |
|       - | 3053 | ` *   More arrays to compare against` |
|       - | 3054 | ` * $callback` |
|       - | 3055 | ` *  The callback comparison function.` |
|       - | 3056 | ` *  The comparison function must return an integer less than, equal to, or greater than zero` |
|       - | 3057 | ` *  if the first argument is considered to be respectively less than, equal to, or greater` |
|       - | 3058 | ` *  than the second.` |
|       - | 3059 | ` *     int callback ( mixed $a, mixed $b )` |
|       - | 3060 | ` * Return` |
|       - | 3061 | ` *  Returns an array containing all of the values in array1 whose values exist` |
|       - | 3062 | ` *  in all of the parameters. .` |
|       - | 3063 | ` * Note that NULL is returned on failure.` |
|       - | 3064 | ` */` |
|      30 | 3065 | `PH7_PRIVATE int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3066 | `{` |
|       - | 3067 | `	ph7_hashmap_node *pEntry;` |
|       - | 3068 | `	ph7_hashmap *pSrc,*pMap;` |
|       - | 3069 | `	ph7_value *pCallback;` |
|       - | 3070 | `	ph7_value *pArray;` |
|       - | 3071 | `	ph7_value *pVal;` |
|       - | 3072 | `	sxi32 rc;` |
|       - | 3073 | `	sxu32 n;` |
|       - | 3074 | `	int i;` |
|       - | 3075 |  |
|       - | 3076 | `	/* Ensure the argument count matches PHP behaviour. */` |
|      35 | 3077 | `	if( nArg < 2 ){` |
|     ! 0 | 3078 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3079 | `			"ArgumentCountError",` |
|       - | 3080 | `			"array_uintersect() expects at least 2 arguments, %d given",` |
|     ! 0 | 3081 | `			nArg` |
|       - | 3082 | `			);` |
|       - | 3083 | `	}` |
|      35 | 3084 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3085 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3086 | `			"TypeError",` |
|       - | 3087 | `			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3088 | `			ph7_type_name(apArg[0])` |
|       - | 3089 | `			);` |
|       - | 3090 | `	}` |
|       - | 3091 |  |
|       - | 3092 | `	/* php checks the CALLBACK before the intermediary arrays (see array_udiff). */` |
|      33 | 3093 | `	pCallback = apArg[nArg - 1];` |
|       - | 3094 | `	{` |
|      33 | 3095 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,pCallback,nArg,0,FALSE);` |
|      33 | 3096 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 3097 | `	}` |
|       - | 3098 |  |
|       - | 3099 | `	/* Now the intermediary arrays, left to right. */` |
|      25 | 3100 | `	for( i = 1 ; i < nArg - 1; i++ ){` |
|      13 | 3101 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       4 | 3102 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3103 | `				"TypeError",` |
|       - | 3104 | `				"array_uintersect(): Argument #%d must be of type array, %s given",` |
|       1 | 3105 | `				i + 1,` |
|       2 | 3106 | `				ph7_type_name(apArg[i])` |
|       - | 3107 | `				);` |
|       - | 3108 | `		}` |
|       6 | 3109 | `	}` |
|       - | 3110 |  |
|      14 | 3111 | `	if( nArg == 2 ){` |
|       - | 3112 | `		/* Only the original array and the callback were provided. */` |
|       5 | 3113 | `		ph7_result_value(pCtx,apArg[0]);` |
|       5 | 3114 | `		return PH7_OK;` |
|       - | 3115 | `	}` |
|       - | 3116 |  |
|       - | 3117 | `	/* Create a new array */` |
|      10 | 3118 | `	pArray = ph7_context_new_array(pCtx);` |
|      10 | 3119 | `	if( pArray == 0 ){` |
|     ! 0 | 3120 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3121 | `		return PH7_OK;` |
|       - | 3122 | `	}` |
|       - | 3123 | `	/* Point to the internal representation of the source hashmap */` |
|      10 | 3124 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3125 | `	/* Perform the intersection */` |
|      10 | 3126 | `	pEntry = pSrc->pFirst;` |
|      10 | 3127 | `	n = pSrc->nEntry;` |
|      10 | 3128 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|      13 | 3129 | `	for(;;){` |
|      28 | 3130 | `		if( n < 1 ){` |
|       8 | 3131 | `			break;` |
|       - | 3132 | `		}` |
|       - | 3133 | `		/* Extract the node value */` |
|      22 | 3134 | `		pVal = HashmapExtractNodeValue(pEntry);` |
|      22 | 3135 | `		if( pVal ){` |
|      34 | 3136 | `			for( i = 1 ; i < nArg - 1; i++ ){` |
|      22 | 3137 | `				if( !ph7_value_is_array(apArg[i])) {` |
|       - | 3138 | `					/* ignore */` |
|     ! 0 | 3139 | `					continue;` |
|       - | 3140 | `				}` |
|       - | 3141 | `				/* Point to the internal representation of the hashmap */` |
|      22 | 3142 | `				pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|       - | 3143 | `				/* Perform the lookup */` |
|      22 | 3144 | `				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);` |
|      22 | 3145 | `				if( rc != SXRET_OK ){` |
|       - | 3146 | `					/* Value does not exist */` |
|      10 | 3147 | `					break;` |
|       - | 3148 | `				}` |
|       8 | 3149 | `			}` |
|      22 | 3150 | `			if( i >= (nArg-1) ){` |
|       - | 3151 | `				/* Perform the insertion */` |
|      14 | 3152 | `				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|       6 | 3153 | `			}` |
|      10 | 3154 | `		}` |
|      22 | 3155 | `		if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 3156 | `			/* The comparison callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 3157 | `			pCtx->pVm->iCmpCallbackExc = 0;` |
|       3 | 3158 | `			return PH7_EXCEPTION;` |
|       - | 3159 | `		}` |
|       - | 3160 | `		/* Point to the next entry */` |
|      20 | 3161 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      20 | 3162 | `		n--;` |
|       2 | 3163 | `	}` |
|       - | 3164 | `	/* Return the freshly created array */` |
|       8 | 3165 | `	ph7_result_value(pCtx,pArray);` |
|       8 | 3166 | `	return PH7_OK;` |
|      20 | 3167 | `}` |
|       - | 3168 | `/*` |
|       - | 3169 | ` * array array_fill(int $start_index,int $num,var $value)` |
|       - | 3170 | ` *  Fill an array with values.` |
|       - | 3171 | ` * Parameters` |
|       - | 3172 | ` *  $start_index` |
|       - | 3173 | ` *    The first index of the returned array.` |
|       - | 3174 | ` *  $num` |
|       - | 3175 | ` *   Number of elements to insert.` |
|       - | 3176 | ` *  $value` |
|       - | 3177 | ` *    Value to use for filling.` |
|       - | 3178 | ` * Return` |
|       - | 3179 | ` *  The filled array or null on failure.` |
|       - | 3180 | ` */` |
|     248 | 3181 | `PH7_PRIVATE int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 3182 | `{` |
|       - | 3183 | `	ph7_value *pArray;` |
|       - | 3184 | `	int i,nEntry;` |
|       - | 3185 |  |
|       - | 3186 | `	/* PHP enforces argument count and type checks. */` |
|     252 | 3187 | `	if( nArg != 3 ){` |
|       - | 3188 | `		/* wrong number of arguments -> ArgumentCountError */` |
|     ! 0 | 3189 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3190 | `			"ArgumentCountError",` |
|       - | 3191 | `			"array_fill() expects exactly 3 arguments, %d given",` |
|     ! 0 | 3192 | `			nArg` |
|       - | 3193 | `			);` |
|       - | 3194 | `	}` |
|       - | 3195 |  |
|       - | 3196 | `	/* Argument #1: start index must be convertible to int.  Accept booleans,` |
|       - | 3197 | `	 * floats, and numeric strings (including those with decimal point) by` |
|       - | 3198 | `	 * allowing them through the conversion.  Only arrays, objects, resources` |
|       - | 3199 | `	 * and NULLs are rejected outright. */` |
|     372 | 3200 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_object(apArg[0]) \|\|` |
|     376 | 3201 | `		ph7_value_is_resource(apArg[0]) \|\| ph7_value_is_null(apArg[0]) ){` |
|     ! 0 | 3202 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3203 | `			"TypeError",` |
|       - | 3204 | `			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",` |
|     ! 0 | 3205 | `			ph7_type_name(apArg[0])` |
|       - | 3206 | `			);` |
|       - | 3207 | `	}` |
|     252 | 3208 | `	if( ph7_value_is_string(apArg[0]) ){` |
|       - | 3209 | `		int len;` |
|       8 | 3210 | `		sxu8 bReal = FALSE;` |
|       8 | 3211 | `		const char *zStr = ph7_value_to_string(apArg[0], &len);` |
|       8 | 3212 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       - | 3213 | `			/* Non‑numeric string is an error. */` |
|       4 | 3214 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3215 | `				"TypeError",` |
|       - | 3216 | `				"array_fill(): Argument #1 ($start_index) must be of type int, string given"` |
|       - | 3217 | `				);` |
|       - | 3218 | `		}` |
|       5 | 3219 | `		if( bReal ){` |
|       - | 3220 | `			/* php only DEPRECATES the lossy float-string -> int narrowing; §10 rejects` |
|       - | 3221 | `			 * it loudly, and the throw ABORTS the call (php would fill the array).` |
|       - | 3222 | `			 * Twin-pinned by array_lossy_int_arg_abort{,_zend}.phpt. */` |
|       3 | 3223 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 3224 | `				"Implicit conversion from float-string to int loses precision");` |
|       - | 3225 | `		}` |
|       1 | 3226 | `	}` |
|       - | 3227 |  |
|       - | 3228 | `	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,` |
|       - | 3229 | `	 * floats and numeric strings; reject arrays, objects, resources and NULL. */` |
|     366 | 3230 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|     369 | 3231 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 3232 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3233 | `			"TypeError",` |
|       - | 3234 | `			"array_fill(): Argument #2 ($count) must be of type int, %s given",` |
|     ! 0 | 3235 | `			ph7_type_name(apArg[1])` |
|       - | 3236 | `			);` |
|       - | 3237 | `	}` |
|     247 | 3238 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 3239 | `		int len;` |
|       3 | 3240 | `		sxu8 bReal = FALSE;` |
|       3 | 3241 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       3 | 3242 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       3 | 3243 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3244 | `				"TypeError",` |
|       - | 3245 | `				"array_fill(): Argument #2 ($count) must be of type int, string given"` |
|       - | 3246 | `				);` |
|       - | 3247 | `		}` |
|     ! 0 | 3248 | `	}` |
|       - | 3249 | `	/* Note: booleans and WHOLE floats are accepted and converted by ph7_value_to_int` |
|       - | 3250 | `	 * below; a FRACTIONAL float is the §10 lossy narrowing and aborts the call. */` |
|     245 | 3251 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       8 | 3252 | `		double d = ph7_value_to_double(apArg[1]);` |
|       - | 3253 | `		/* avoid hiding outer 'i' (loop index) */` |
|       8 | 3254 | `		sxi64 i64 = (sxi64)d;` |
|       8 | 3255 | `		if( d != (double)i64 ){` |
|       6 | 3256 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 3257 | `				"Implicit conversion from float to int loses precision");` |
|       - | 3258 | `		}` |
|       1 | 3259 | `	}` |
|       - | 3260 |  |
|       - | 3261 | `	/* Total number of entries to insert. Read as 64-bit FIRST: the old 32-bit` |
|       - | 3262 | `	 * read truncated array_fill(0, PHP_INT_MAX, x) to -1 and reported the` |
|       - | 3263 | `	 * negative-count message where php says "is too large". */` |
|     240 | 3264 | `	sxi64 nEntry64 = ph7_value_to_int64(apArg[1]);` |
|       - | 3265 | `	/* Reject negative counts with a ValueError like PHP. */` |
|     240 | 3266 | `	if( nEntry64 < 0 ){` |
|       6 | 3267 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3268 | `			"ValueError",` |
|       - | 3269 | `			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"` |
|       - | 3270 | `			);` |
|       - | 3271 | `	}` |
|     235 | 3272 | `	if( nEntry64 > 0x7fffffff ){` |
|       - | 3273 | `		/* php's threshold (probed 8.5.8): count > INT32_MAX is the distinct` |
|       - | 3274 | `		 * "is too large" ValueError; INT32_MAX itself proceeds to allocation` |
|       - | 3275 | `		 * (php then dies on the overflowing allocation, PHL OOMs gracefully). */` |
|       5 | 3276 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3277 | `			"ValueError",` |
|       - | 3278 | `			"array_fill(): Argument #2 ($count) is too large"` |
|       - | 3279 | `			);` |
|       - | 3280 | `	}` |
|     231 | 3281 | `	nEntry = (int)nEntry64;` |
|       - | 3282 |  |
|       - | 3283 | `	/* If zero elements were requested, return an empty array without allocating */` |
|     231 | 3284 | `	if( nEntry == 0 ){` |
|       5 | 3285 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|       5 | 3286 | `		return PH7_OK;` |
|       - | 3287 | `	}` |
|       - | 3288 |  |
|       - | 3289 | `	/* Create a new array */` |
|     227 | 3290 | `	pArray = ph7_context_new_array(pCtx);` |
|     227 | 3291 | `	if( pArray == 0 ){` |
|     ! 0 | 3292 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 3293 | `	}` |
|       - | 3294 |  |
|       - | 3295 | `	/* PHP 8 fills consecutive integer keys start_index, start_index+1, … even` |
|       - | 3296 | `	 * when start_index is negative (PHP 7 restarted the remaining keys from 0,` |
|       - | 3297 | `	 * so array_fill(-5,3) gave -5,0,1 instead of -5,-4,-3). Assign each key` |
|       - | 3298 | `	 * explicitly rather than relying on automatic (append) indexing. */` |
|     227 | 3299 | `	int iStart = ph7_value_to_int(apArg[0]);` |
| 2117833 | 3300 | `	for( i = 0 ; i < nEntry ; i++ ){` |
| 2117607 | 3301 | `		if( ph7_array_add_intkey_elem(pArray, iStart + i, apArg[2]) != SXRET_OK ){` |
|       - | 3302 | `			/* Allocation failure: surface a fatal instead of a partial array */` |
|     ! 0 | 3303 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 3304 | `		}` |
| 1058804 | 3305 | `	}` |
|       - | 3306 | `	/* Return the filled array */` |
|     227 | 3307 | `	ph7_result_value(pCtx, pArray);` |
|     227 | 3308 | `	return PH7_OK;` |
|     128 | 3309 | `}` |
|       - | 3310 | `/*` |
|       - | 3311 | ` * array array_fill_keys(array $input,mixed $value)` |
|       - | 3312 | ` *  Fill an array with values, specifying keys.` |
|       - | 3313 | ` * Parameters` |
|       - | 3314 | ` *  $input` |
|       - | 3315 | ` *   Array of values that will be used as key.` |
|       - | 3316 | ` *  $value` |
|       - | 3317 | ` *    Value to use for filling.` |
|       - | 3318 | ` * Return` |
|       - | 3319 | ` *  The filled array.` |
|       - | 3320 | ` * Throws` |
|       - | 3321 | ` *  ValueError if $input is not an array.` |
|       - | 3322 | ` */` |
|      32 | 3323 | `PH7_PRIVATE int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 3324 | `{` |
|       - | 3325 | `	ph7_hashmap_node *pEntry;` |
|       - | 3326 | `	ph7_hashmap *pSrc;` |
|       - | 3327 | `	ph7_value *pArray;` |
|       - | 3328 | `	sxu32 n;` |
|       - | 3329 | `	/* PHP enforces exactly 2 arguments. */` |
|      36 | 3330 | `	if( nArg != 2 ){` |
|     ! 0 | 3331 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3332 | `			"ArgumentCountError",` |
|       - | 3333 | `			"array_fill_keys() expects exactly 2 arguments, %d given",` |
|     ! 0 | 3334 | `			nArg` |
|       - | 3335 | `			);` |
|       - | 3336 | `	}` |
|       - | 3337 | `	/* Make sure we are dealing with a valid hashmap */` |
|      36 | 3338 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       8 | 3339 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3340 | `			"TypeError",` |
|       - | 3341 | `			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",` |
|       2 | 3342 | `			ph7_type_name(apArg[0])` |
|       - | 3343 | `			);` |
|       - | 3344 | `	}` |
|       - | 3345 | `	/* Point to the internal representation of the input hashmap */` |
|      30 | 3346 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3347 | `	/* Create a new array */` |
|      30 | 3348 | `	pArray = ph7_context_new_array(pCtx);` |
|      30 | 3349 | `	if( pArray == 0 ){` |
|     ! 0 | 3350 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3351 | `		return PH7_OK;` |
|       - | 3352 | `	}` |
|       - | 3353 | `	/* Perform the requested operation. php has its own key rule here and it is` |
|       - | 3354 | `	 * NOT the generic subscript canonicalisation: an INT goes in as an index, and` |
|       - | 3355 | `	 * everything else takes the USER-VISIBLE (string) cast — so 1.5 becomes the` |
|       - | 3356 | `	 * string key "1.5" (PHL made it the index 1), null becomes "" (PHL made it 0),` |
|       - | 3357 | `	 * an array warns "Array to string conversion", and an object with no` |
|       - | 3358 | `	 * __toString() throws php's Error (PHL keyed it under the literal "Object").` |
|       - | 3359 | `	 * The resulting string then re-normalises the usual way, which is what turns` |
|       - | 3360 | ``	 * `true` into the index 1. */`` |
|      30 | 3361 | `	pEntry = pSrc->pFirst;` |
|      72 | 3362 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|      46 | 3363 | `		ph7_value *pKey = HashmapExtractNodeValue(pEntry);` |
|      44 | 3364 | `		if( pKey == 0 \|\| (pKey->iFlags & (MEMOBJ_INT\|MEMOBJ_REAL\|MEMOBJ_STRING)) == MEMOBJ_INT` |
|      43 | 3365 | `		 \|\| (pKey->iFlags & MEMOBJ_STRING) != 0 ){` |
|      25 | 3366 | `			ph7_array_add_elem(pArray,pKey,apArg[1]);` |
|      13 | 3367 | `		}else{` |
|       - | 3368 | `			ph7_value sKey;` |
|       - | 3369 | `			sxi32 rcSv;` |
|       - | 3370 | `			/* Coerce a COPY: pKey is a live element of the caller's array. */` |
|      22 | 3371 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|      22 | 3372 | `			PH7_MemObjLoad(pKey,&sKey);` |
|      22 | 3373 | `			rcSv = PH7_ValueToStringUV(pCtx,&sKey,0,0);` |
|      22 | 3374 | `			if( rcSv != SXRET_OK ){` |
|       3 | 3375 | `				PH7_MemObjRelease(&sKey);` |
|       3 | 3376 | `				return rcSv;` |
|       - | 3377 | `			}` |
|      20 | 3378 | `			ph7_array_add_elem(pArray,&sKey,apArg[1]);` |
|      20 | 3379 | `			PH7_MemObjRelease(&sKey);` |
|       - | 3380 | `		}` |
|       - | 3381 | `		/* Point to the next entry */` |
|      44 | 3382 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      23 | 3383 | `	}` |
|       - | 3384 | `	/* Return the filled array */` |
|      28 | 3385 | `	ph7_result_value(pCtx,pArray);` |
|      28 | 3386 | `	return PH7_OK;` |
|      20 | 3387 | `}` |
|       - | 3388 | `/*` |
|       - | 3389 | ` * array array_combine(array $keys,array $values)` |
|       - | 3390 | ` *  Creates an array by using one array for keys and another for its values.` |
|       - | 3391 | ` * Parameters` |
|       - | 3392 | ` *  $keys` |
|       - | 3393 | ` *    Array of keys to be used.` |
|       - | 3394 | ` * $values` |
|       - | 3395 | ` *   Array of values to be used.` |
|       - | 3396 | ` * Return` |
|       - | 3397 | ` *  Returns the combined array. Otherwise FALSE if the number of elements` |
|       - | 3398 | ` *  for each array isn't equal or if one of the given arguments is` |
|       - | 3399 | ` *  not an array.` |
|       - | 3400 | ` */` |
|      26 | 3401 | `PH7_PRIVATE int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 3402 | `{` |
|       - | 3403 | `	ph7_hashmap_node *pKe,*pVe;` |
|       - | 3404 | `	ph7_hashmap *pKey,*pValue;` |
|       - | 3405 | `	ph7_value *pArray;` |
|       - | 3406 | `	sxu32 n;` |
|       - | 3407 | `	/* PHP enforces argument count and type checks. */` |
|      30 | 3408 | `	if( nArg != 2 ){` |
|       - | 3409 | `		/* wrong number of arguments -> ArgumentCountError */` |
|     ! 0 | 3410 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3411 | `			"ArgumentCountError",` |
|       - | 3412 | `			"array_combine() expects exactly 2 arguments, %d given",` |
|     ! 0 | 3413 | `			nArg` |
|       - | 3414 | `			);` |
|       - | 3415 | `	}` |
|       - | 3416 | `	/* Validate argument types individually so we can report the correct` |
|       - | 3417 | `	 * argument index in the error message. */` |
|      30 | 3418 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3419 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3420 | `			"TypeError",` |
|       - | 3421 | `			"array_combine(): Argument #1 ($keys) must be of type array, %s given",` |
|       1 | 3422 | `			ph7_type_name(apArg[0])` |
|       - | 3423 | `			);` |
|       - | 3424 | `	}` |
|      27 | 3425 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|       4 | 3426 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3427 | `			"TypeError",` |
|       - | 3428 | `			"array_combine(): Argument #2 ($values) must be of type array, %s given",` |
|       2 | 3429 | `			ph7_type_name(apArg[1])` |
|       - | 3430 | `			);` |
|       - | 3431 | `	}` |
|       - | 3432 | `	/* Point to the internal representation of the input hashmaps */` |
|      24 | 3433 | `	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      24 | 3434 | `	pValue = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      24 | 3435 | `	if( pKey->nEntry != pValue->nEntry ){` |
|       - | 3436 | `		/* Length mismatch -> ValueError */` |
|       3 | 3437 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3438 | `			"ValueError",` |
|       - | 3439 | `			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"` |
|       - | 3440 | `			);` |
|       - | 3441 | `	}` |
|       - | 3442 | `	/* Create a new array */` |
|      22 | 3443 | `	pArray = ph7_context_new_array(pCtx);` |
|      22 | 3444 | `	if( pArray == 0 ){` |
|     ! 0 | 3445 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3446 | `		return PH7_OK;` |
|       - | 3447 | `	}` |
|       - | 3448 | `	/* Perform the requested operation */` |
|      22 | 3449 | `	pKe = pKey->pFirst;` |
|      22 | 3450 | `	pVe = pValue->pFirst;` |
|      54 | 3451 | `	for( n = 0 ; n < pKey->nEntry ; n++ ){` |
|      36 | 3452 | `		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);` |
|      36 | 3453 | `		ph7_value *pValVal = HashmapExtractNodeValue(pVe);` |
|       - | 3454 | `		/* php's key rule here is array_fill_keys()'s, not the ordinary offset` |
|       - | 3455 | `		 * canonicalisation: an INT goes in as an index and everything else takes` |
|       - | 3456 | `		 * the USER-VISIBLE (string) cast. Floats were already handled that way` |
|       - | 3457 | `		 * (1.5 becomes the key "1.5", not the index 1); null now becomes "" rather` |
|       - | 3458 | `		 * than 0, an array warns "Array to string conversion", and an object with` |
|       - | 3459 | `		 * no __toString() throws php's Error instead of keying under the literal` |
|       - | 3460 | `		 * "Object". The copy matters: the caller's array must not be mutated. */` |
|      36 | 3461 | `		ph7_value *pKeyCopy = pKeyVal;` |
|       - | 3462 | `		ph7_value sKeyTmp;` |
|      36 | 3463 | `		int bKeyTmp = 0;` |
|      36 | 3464 | `		if( pKeyVal && (pKeyVal->iFlags & (MEMOBJ_INT\|MEMOBJ_STRING)) == 0 ){` |
|       - | 3465 | `			sxi32 rcSv;` |
|      14 | 3466 | `			PH7_MemObjInit(pCtx->pVm,&sKeyTmp);` |
|      14 | 3467 | `			PH7_MemObjLoad(pKeyVal,&sKeyTmp);` |
|      14 | 3468 | `			bKeyTmp = 1;` |
|      14 | 3469 | `			rcSv = PH7_ValueToStringUV(pCtx,&sKeyTmp,0,0);` |
|      14 | 3470 | `			if( rcSv != SXRET_OK ){` |
|       3 | 3471 | `				PH7_MemObjRelease(&sKeyTmp);` |
|       3 | 3472 | `				return rcSv;` |
|       - | 3473 | `			}` |
|      12 | 3474 | `			pKeyCopy = &sKeyTmp;` |
|       5 | 3475 | `		}` |
|      34 | 3476 | `		ph7_array_add_elem(pArray,pKeyCopy,pValVal);` |
|      34 | 3477 | `		if( bKeyTmp ){` |
|      12 | 3478 | `			PH7_MemObjRelease(&sKeyTmp);` |
|       5 | 3479 | `		}` |
|       - | 3480 | `		/* Point to the next entry */` |
|      34 | 3481 | `		pKe = pKe->pPrev; /* Reverse link */` |
|      34 | 3482 | `		pVe = pVe->pPrev;` |
|      18 | 3483 | `	}` |
|       - | 3484 | `	/* Return the filled array */` |
|      20 | 3485 | `	ph7_result_value(pCtx,pArray);` |
|      20 | 3486 | `	return PH7_OK;` |
|      17 | 3487 | `}` |
|       - | 3488 | `/*` |
|       - | 3489 | ` * array array_reverse(array $array [,bool $preserve_keys = false ])` |
|       - | 3490 | ` *  Return an array with elements in reverse order.` |
|       - | 3491 | ` * Parameters` |
|       - | 3492 | ` *  $array` |
|       - | 3493 | ` *   The input array.` |
|       - | 3494 | ` *  $preserve_keys (optional)` |
|       - | 3495 | ` *   If set to TRUE keys are preserved.` |
|       - | 3496 | ` * Return` |
|       - | 3497 | ` *  The reversed array.` |
|       - | 3498 | ` */` |
|      18 | 3499 | `PH7_PRIVATE int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3500 | `{` |
|       - | 3501 | `	ph7_hashmap_node *pEntry;` |
|       - | 3502 | `	ph7_hashmap *pSrc;` |
|       - | 3503 | `	ph7_value *pArray;` |
|       - | 3504 | `	int bPreserve;` |
|       - | 3505 | `	sxu32 n;` |
|      20 | 3506 | `	if( nArg < 1 ){` |
|     ! 0 | 3507 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3508 | `			"ArgumentCountError",` |
|       - | 3509 | `			"array_reverse() expects at least 1 argument, %d given",` |
|     ! 0 | 3510 | `			nArg` |
|       - | 3511 | `			);` |
|       - | 3512 | `	}` |
|       - | 3513 | `	/* Make sure we are dealing with a valid hashmap */` |
|      20 | 3514 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 3515 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3516 | `			"TypeError",` |
|       - | 3517 | `			"array_reverse(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3518 | `			ph7_type_name(apArg[0])` |
|       - | 3519 | `			);` |
|       - | 3520 | `	}` |
|      17 | 3521 | `	bPreserve = FALSE;` |
|      17 | 3522 | `	if( nArg > 1 ){` |
|       7 | 3523 | `		bPreserve = ph7_value_to_bool(apArg[1]);` |
|       3 | 3524 | `	}` |
|       - | 3525 | `	/* Point to the internal representation of the input hashmap */` |
|      17 | 3526 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3527 | `	/* Create a new array */` |
|      17 | 3528 | `	pArray = ph7_context_new_array(pCtx);` |
|      17 | 3529 | `	if( pArray == 0 ){` |
|     ! 0 | 3530 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3531 | `		return PH7_OK;` |
|       - | 3532 | `	}` |
|       - | 3533 | `	/* Perform the requested operation */` |
|      17 | 3534 | `	pEntry = pSrc->pLast;` |
|      55 | 3535 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 3536 | `		/* String keys are always preserved; preserve_keys only affects int keys */` |
|      39 | 3537 | `		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;` |
|      39 | 3538 | `		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);` |
|       - | 3539 | `		/* Point to the previous entry */` |
|      39 | 3540 | `		pEntry = pEntry->pNext; /* Reverse link */` |
|      20 | 3541 | `	}` |
|      17 | 3542 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 3543 | `	return PH7_OK;` |
|      11 | 3544 | `}` |
|       - | 3545 | `/*` |
|       - | 3546 | ` * array array_unique(array $array, int $flags = SORT_STRING)` |
|       - | 3547 | ` *  Removes duplicate values from an array.` |
|       - | 3548 | ` * Parameters` |
|       - | 3549 | ` *  $array` |
|       - | 3550 | ` *   The input array.` |
|       - | 3551 | ` *  $flags` |
|       - | 3552 | ` *   The optional second parameter may be used to modify the comparison` |
|       - | 3553 | ` *   behavior using these values:` |
|       - | 3554 | ` *     SORT_REGULAR - compare items normally (don't change types)` |
|       - | 3555 | ` *     SORT_NUMERIC - compare items numerically` |
|       - | 3556 | ` *     SORT_STRING  - compare items as strings` |
|       - | 3557 | ` * Return` |
|       - | 3558 | ` *  The filtered array.` |
|       - | 3559 | ` */` |
|      58 | 3560 | `PH7_PRIVATE int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3561 | `{` |
|       - | 3562 | `	ph7_hashmap_node *pEntry;` |
|       - | 3563 | `	ph7_value *pNeedle;` |
|       - | 3564 | `	ph7_hashmap *pSrc;` |
|       - | 3565 | `	ph7_value *pArray;` |
|       - | 3566 | `	int iFlags,base,bFold;` |
|       - | 3567 | `	sxu32 n;` |
|      63 | 3568 | `	if( nArg < 1 ){` |
|       - | 3569 | `		/* Missing arguments, throw ArgumentCountError */` |
|     ! 0 | 3570 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3571 | `			"ArgumentCountError",` |
|       - | 3572 | `			"array_unique() expects at least 1 argument, 0 given"` |
|       - | 3573 | `			);` |
|       - | 3574 | `	}` |
|      63 | 3575 | `	if( nArg > 2 ){` |
|       - | 3576 | `		/* Too many arguments, throw ArgumentCountError */` |
|     ! 0 | 3577 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3578 | `			"ArgumentCountError",` |
|       - | 3579 | `			"array_unique() expects at most 2 arguments, %d given",` |
|     ! 0 | 3580 | `			nArg` |
|       - | 3581 | `			);` |
|       - | 3582 | `	}` |
|       - | 3583 | `	/* Make sure we are dealing with a valid hashmap */` |
|      63 | 3584 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3585 | `		/* Type mismatch, throw TypeError */` |
|       4 | 3586 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3587 | `			"TypeError",` |
|       - | 3588 | `			"array_unique(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3589 | `			ph7_type_name(apArg[0])` |
|       - | 3590 | `			);` |
|       - | 3591 | `	}` |
|       - | 3592 | `	/* php's default is SORT_STRING (2): elements compare as strings. Explicit` |
|       - | 3593 | `	 * flags select numeric / natural / case-insensitive comparison. */` |
|      61 | 3594 | `	iFlags = nArg > 1 ? ph7_value_to_int(apArg[1]) : 2 /* SORT_STRING */;` |
|      61 | 3595 | `	base = iFlags & ~8;` |
|      61 | 3596 | `	bFold = (iFlags & 8) != 0;` |
|       - | 3597 | `	/* Point to the internal representation of the input hashmap */` |
|      61 | 3598 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3599 | `	/* Create a new array */` |
|      61 | 3600 | `	pArray = ph7_context_new_array(pCtx);` |
|      61 | 3601 | `	if( pArray == 0 ){` |
|     ! 0 | 3602 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3603 | `		return PH7_OK;` |
|       - | 3604 | `	}` |
|       - | 3605 | `	/* Perform the requested operation. The string flags coerce their operands` |
|       - | 3606 | `	 * user-visibly, and a not-stringable object raises php's Error inside the` |
|       - | 3607 | `	 * comparison, which has no status channel: HashmapValueFlagEqual flags the VM` |
|       - | 3608 | `	 * (the rail the throwing user-callback sorts use), so clear it before the walk` |
|       - | 3609 | `	 * and report it after. Skipping the clear leaks the flag into the NEXT` |
|       - | 3610 | `	 * comparison-based call, which then calls every pair equal. */` |
|      61 | 3611 | `	pCtx->pVm->iCmpCallbackExc = 0;` |
|      61 | 3612 | `	pEntry = pSrc->pFirst;` |
|     239 | 3613 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|     183 | 3614 | `		pNeedle = HashmapExtractNodeValue(pEntry);` |
|     183 | 3615 | `		if( pNeedle ){` |
|       - | 3616 | `			/* Keep this element unless a flag-equal one is already present. */` |
|     183 | 3617 | `			ph7_hashmap *pKept = (ph7_hashmap *)pArray->x.pOther;` |
|     183 | 3618 | `			ph7_hashmap_node *pK = pKept->pFirst;` |
|     183 | 3619 | `			int bDup = 0;` |
|       - | 3620 | `			sxu32 i;` |
|       - | 3621 | `			/* Forward iteration in this map uses the pPrev link (see the outer` |
|       - | 3622 | `			 * loop over pSrc). */` |
|     263 | 3623 | `			for( i = 0 ; i < pKept->nEntry && pK ; ++i ){` |
|     169 | 3624 | `				ph7_value *pV = HashmapExtractNodeValue(pK);` |
|     169 | 3625 | `				if( pV && HashmapValueFlagEqual(pCtx->pVm,pNeedle,pV,base,bFold) ){` |
|      89 | 3626 | `					bDup = 1;` |
|      89 | 3627 | `					break;` |
|       - | 3628 | `				}` |
|      83 | 3629 | `				pK = pK->pPrev;` |
|      43 | 3630 | `			}` |
|     183 | 3631 | `			if( !bDup ){` |
|      99 | 3632 | `				HashmapInsertNode(pKept,pEntry,TRUE);` |
|      47 | 3633 | `			}` |
|      89 | 3634 | `		}` |
|       - | 3635 | `		/* Point to the next entry */` |
|     183 | 3636 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      94 | 3637 | `	}` |
|      61 | 3638 | `	if( pCtx->pVm->iCmpCallbackExc ){` |
|       - | 3639 | `		/* A comparison raised the coercion Error: answer the throw, not an array. */` |
|       7 | 3640 | `		pCtx->pVm->iCmpCallbackExc = 0;` |
|       7 | 3641 | `		pCtx->nThrowRc = PH7_EXCEPTION;` |
|       7 | 3642 | `		return PH7_EXCEPTION;` |
|       - | 3643 | `	}` |
|       - | 3644 | `	/* Return the freshly created array */` |
|      55 | 3645 | `	ph7_result_value(pCtx,pArray);` |
|      55 | 3646 | `	return PH7_OK;` |
|      34 | 3647 | `}` |
|       - | 3648 | `/*` |
|       - | 3649 | ` * array array_flip(array $input)` |
|       - | 3650 | ` *  Exchanges all keys with their associated values in an array.` |
|       - | 3651 | ` * Parameter` |
|       - | 3652 | ` *  $input` |
|       - | 3653 | ` *   Input array.` |
|       - | 3654 | ` * Return` |
|       - | 3655 | ` *   The flipped array on success or NULL on failure.` |
|       - | 3656 | ` */` |
|      34 | 3657 | `PH7_PRIVATE int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3658 | `{` |
|       - | 3659 | `	ph7_hashmap_node *pEntry;` |
|       - | 3660 | `	ph7_hashmap *pSrc;` |
|       - | 3661 | `	ph7_value *pArray;` |
|       - | 3662 | `	ph7_value *pKey;` |
|       - | 3663 | `	ph7_value sVal;` |
|       - | 3664 | `	sxu32 n;` |
|       - | 3665 |  |
|       - | 3666 | `	/* PHP requires exactly one argument */` |
|      36 | 3667 | `	if( nArg != 1 ){` |
|       - | 3668 | `		/* Use ArgumentCountError like other array helpers */` |
|     ! 0 | 3669 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3670 | `			"ArgumentCountError",` |
|       - | 3671 | `			"array_flip() expects exactly 1 argument, %d given",` |
|     ! 0 | 3672 | `			nArg` |
|       - | 3673 | `			);` |
|       - | 3674 | `	}` |
|       - | 3675 | `	/* Make sure we are dealing with a valid hashmap */` |
|      36 | 3676 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3677 | `		/* Type mismatch -> TypeError */` |
|       4 | 3678 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3679 | `			"TypeError",` |
|       - | 3680 | `			"array_flip(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 3681 | `			ph7_type_name(apArg[0])` |
|       - | 3682 | `			);` |
|       - | 3683 | `	}` |
|       - | 3684 | `	/* Point to the internal representation of the input hashmap */` |
|      33 | 3685 | `	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 3686 | `	/* Create a new array */` |
|      33 | 3687 | `	pArray = ph7_context_new_array(pCtx);` |
|      33 | 3688 | `	if( pArray == 0 ){` |
|     ! 0 | 3689 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3690 | `		return PH7_OK;` |
|       - | 3691 | `	}` |
|       - | 3692 | `	/* Start processing */` |
|      33 | 3693 | `	pEntry = pSrc->pFirst;` |
|   22283 | 3694 | `	for( n = 0 ; n < pSrc->nEntry ; n++ ){` |
|       - | 3695 | `		/* Extract the node value (will become a key in the result) */` |
|   22251 | 3696 | `		pKey = HashmapExtractNodeValue(pEntry);` |
|   22251 | 3697 | `		if( pKey ){` |
|       - | 3698 | `			/* NULL values are not valid keys either, PHP emits a warning */` |
|   22251 | 3699 | `			if( pKey->iFlags & MEMOBJ_NULL ){` |
|       3 | 3700 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3701 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 3702 | `					);` |
|   22250 | 3703 | `			}else if( (pKey->iFlags & MEMOBJ_INT) \|\| (pKey->iFlags & MEMOBJ_STRING) ){` |
|       - | 3704 | `				/* Prepare the value for insertion (original key) */` |
|   22237 | 3705 | `				if( pEntry->iType == HASHMAP_INT_NODE ){` |
|   20003 | 3706 | `					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);` |
|   10002 | 3707 | `				}else{` |
|       - | 3708 | `					SyString sStr;` |
|    2235 | 3709 | `					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));` |
|    2235 | 3710 | `					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);` |
|       - | 3711 | `				}` |
|       - | 3712 | `				/* Perform the insertion */` |
|   22237 | 3713 | `				ph7_array_add_elem(pArray,pKey,&sVal);` |
|       - | 3714 | `				/* Safely release the value because each inserted entry` |
|       - | 3715 | `				 * has its own private copy of the value.` |
|       - | 3716 | `				 */` |
|   22237 | 3717 | `				PH7_MemObjRelease(&sVal);` |
|   11119 | 3718 | `			}else{` |
|       - | 3719 | `				/* Unsupported value type -> emit warning and skip the entry */` |
|      13 | 3720 | `				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3721 | `					"array_flip(): Can only flip string and integer values, entry skipped"` |
|       - | 3722 | `					);` |
|       - | 3723 | `			}` |
|   11125 | 3724 | `		}` |
|       - | 3725 | `		/* Point to the next entry */` |
|   22251 | 3726 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|   11126 | 3727 | `	}` |
|       - | 3728 | `	/* Return the freshly created array */` |
|      33 | 3729 | `	ph7_result_value(pCtx,pArray);` |
|      33 | 3730 | `	return PH7_OK;` |
|      19 | 3731 | `}` |
|       - | 3732 | `/*` |
|       - | 3733 | ` * number array_sum(array $array )` |
|       - | 3734 | ` *  Calculate the sum of values in an array.` |
|       - | 3735 | ` * Parameters` |
|       - | 3736 | ` *  $array: The input array.` |
|       - | 3737 | ` * Return` |
|       - | 3738 | ` *  Returns the sum of values as an integer or float.` |
|       - | 3739 | ` */` |
|       - | 3740 | `/*` |
|       - | 3741 | `` * array_sum() and array_product() are php's `+` and `*` FOLDED over the elements`` |
|       - | 3742 | ` * from an int identity (0 / 1), and every answer they give follows from that:` |
|       - | 3743 | ` *` |
|       - | 3744 | ` *  - The accumulator promotes to float the moment the int result would not fit,` |
|       - | 3745 | ` *    exactly as the operator does. PH7's two-function split -- a first pass` |
|       - | 3746 | ` *    guessing int-vs-float, then a pure int64 or pure double fold -- had no way` |
|       - | 3747 | ` *    to express this, so the int fold WRAPPED: array_sum([PHP_INT_MAX, 1])` |
|       - | 3748 | ` *    answered PHP_INT_MIN and array_product([PHP_INT_MAX, PHP_INT_MAX, 2])` |
|       - | 3749 | ` *    answered 1.` |
|       - | 3750 | ` *  - Every element is classified on its own. array_product()'s guess looked only` |
|       - | 3751 | ` *    at the FIRST element, so array_product([1, 2.5]) truncated to int(2) and` |
|       - | 3752 | ` *    array_product(["2.5", 2]) to int(4) -- wrong answers on ordinary input.` |
|       - | 3753 | ` *  - A numeric string contributes the number the operator reads from it, through` |
|       - | 3754 | ` *    the engine's ONE string->number conversion (so an integer-shaped digit run` |
|       - | 3755 | ` *    past the int64 range contributes a float, like everywhere else). A` |
|       - | 3756 | ` *    LEADING-numeric string contributes its prefix behind php's unprefixed` |
|       - | 3757 | `` *    `A non-numeric value encountered` warning; array_sum() used to SKIP it, so`` |
|       - | 3758 | ` *    array_sum(["3abc", 2]) answered 2 where php answers 5.` |
|       - | 3759 | ` *  - The operands the operator refuses report` |
|       - | 3760 | `` *    `array_sum(): Addition is not supported on type X` (php names the CLASS for`` |
|       - | 3761 | ` *    an object). Of those, an array and an object are SKIPPED, while a resource` |
|       - | 3762 | ` *    contributes its id and a string with no numeric prefix at all contributes 0` |
|       - | 3763 | ` *    -- which is why array_product(["abc", 2]) is 0 and array_product([[1], 2])` |
|       - | 3764 | ` *    is 2. array_product() reported none of these at all.` |
|       - | 3765 | ` */` |
|     800 | 3766 | `static void HashmapArithFold(ph7_context *pCtx,ph7_hashmap *pMap,int bProduct)` |
|       3 | 3767 | `{` |
|     803 | 3768 | `	const char *zOp = bProduct ? "Multiplication" : "Addition";` |
|       - | 3769 | `	ph7_hashmap_node *pEntry;` |
|       - | 3770 | `	ph7_value *pObj;` |
|     803 | 3771 | `	sxi64 iAcc = bProduct ? 1 : 0;   /* the accumulator while bReal is clear */` |
|     803 | 3772 | `	double dAcc = 0;                 /* ... and after it is set */` |
|     803 | 3773 | `	int bReal = 0;` |
|       - | 3774 | `	sxu32 n;` |
|     803 | 3775 | `	pEntry = pMap->pFirst;` |
|    7057 | 3776 | `	for( n = 0 ; n < pMap->nEntry ; n++, pEntry = pEntry->pPrev /* Reverse link */ ){` |
|    6257 | 3777 | `		sxi64 iVal = 0;` |
|    6257 | 3778 | `		double dVal = 0;` |
|    6257 | 3779 | `		int bValReal = 0;` |
|    6257 | 3780 | `		pObj = HashmapExtractNodeValue(pEntry);` |
|    6257 | 3781 | `		if( pObj == 0 ){` |
|     ! 0 | 3782 | `			continue;` |
|       - | 3783 | `		}` |
|    6257 | 3784 | `		if( pObj->iFlags & MEMOBJ_REAL ){` |
|      40 | 3785 | `			dVal = (double)pObj->rVal;` |
|      40 | 3786 | `			bValReal = 1;` |
|    6238 | 3787 | `		}else if( pObj->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|    6125 | 3788 | `			iVal = pObj->x.iVal;` |
|    3157 | 3789 | `		}else if( pObj->iFlags & MEMOBJ_NULL ){` |
|      12 | 3790 | `			iVal = 0;  /* php folds null in as 0, in silence */` |
|      91 | 3791 | `		}else if( pObj->iFlags & MEMOBJ_STRING ){` |
|      62 | 3792 | `			const char *zTail = 0;` |
|      62 | 3793 | `			if( !PH7_MemObjStringNumericPrefix(pObj,&zTail) ){` |
|       - | 3794 | `				/* No numeric prefix at all ("abc", ""): the refused operand, folded` |
|       - | 3795 | `				 * in as 0. */` |
|      23 | 3796 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       7 | 3797 | `					"%s is not supported on type string",zOp);` |
|      16 | 3798 | `				iVal = 0;` |
|       9 | 3799 | `			}else{` |
|       - | 3800 | `				ph7_value sNum;` |
|      48 | 3801 | `				if( !PH7_MemObjStringIsNumeric(pObj) ){` |
|       - | 3802 | `					/* Leading-numeric: php's operator warning, then the prefix. */` |
|       5 | 3803 | `					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,` |
|       - | 3804 | `						"A non-numeric value encountered");` |
|       2 | 3805 | `				}` |
|       - | 3806 | `				/* Convert a DUPLICATE: PH7_MemObjToNumeric converts in place, and the` |
|       - | 3807 | `				 * element belongs to the caller's array. */` |
|      48 | 3808 | `				PH7_MemObjInit(pCtx->pVm,&sNum);` |
|      48 | 3809 | `				PH7_MemObjLoad(pObj,&sNum);` |
|      48 | 3810 | `				PH7_MemObjToNumeric(&sNum);` |
|      48 | 3811 | `				if( sNum.iFlags & MEMOBJ_REAL ){` |
|      25 | 3812 | `					dVal = (double)sNum.rVal;` |
|      25 | 3813 | `					bValReal = 1;` |
|      13 | 3814 | `				}else{` |
|      24 | 3815 | `					iVal = sNum.x.iVal;` |
|       - | 3816 | `				}` |
|      48 | 3817 | `				PH7_MemObjRelease(&sNum);` |
|       2 | 3818 | `			}` |
|      56 | 3819 | `		}else if( pObj->iFlags & MEMOBJ_HASHMAP ){` |
|      23 | 3820 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       7 | 3821 | `				"%s is not supported on type array",zOp);` |
|      16 | 3822 | `			continue;` |
|      12 | 3823 | `		}else if( pObj->iFlags & MEMOBJ_OBJ ){` |
|       - | 3824 | `			/* php names the CLASS here, not the literal word "object" */` |
|       8 | 3825 | `			ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;` |
|       8 | 3826 | `			if( pInst && pInst->pClass ){` |
|      11 | 3827 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       6 | 3828 | `					"%s is not supported on type %z",zOp,&pInst->pClass->sName);` |
|       5 | 3829 | `			}else{` |
|     ! 0 | 3830 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     ! 0 | 3831 | `					"%s is not supported on type object",zOp);` |
|       - | 3832 | `			}` |
|       8 | 3833 | `			continue;` |
|       5 | 3834 | `		}else if( pObj->iFlags & MEMOBJ_RES ){` |
|       7 | 3835 | `			ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       2 | 3836 | `				"%s is not supported on type resource",zOp);` |
|       5 | 3837 | `			iVal = (sxi64)PH7_VmResourceId(pCtx->pVm,pObj->x.pOther);` |
|       3 | 3838 | `		}else{` |
|     ! 0 | 3839 | `			continue;` |
|       - | 3840 | `		}` |
|       - | 3841 | `		/* Fold the contribution in */` |
|    6237 | 3842 | `		if( bReal \|\| bValReal ){` |
|     106 | 3843 | `			if( !bReal ){` |
|      52 | 3844 | `				dAcc = (double)iAcc;` |
|      52 | 3845 | `				bReal = 1;` |
|      25 | 3846 | `			}` |
|     106 | 3847 | `			if( !bValReal ){` |
|      44 | 3848 | `				dVal = (double)iVal;` |
|      21 | 3849 | `			}` |
|     106 | 3850 | `			dAcc = bProduct ? dAcc * dVal : dAcc + dVal;` |
|      54 | 3851 | `		}else{` |
|       - | 3852 | `			sxi64 iRes;` |
|    6162 | 3853 | `			int bOv = bProduct ? PH7_MUL_OVERFLOW64(iAcc,iVal,&iRes)` |
|    6101 | 3854 | `			                   : PH7_ADD_OVERFLOW64(iAcc,iVal,&iRes);` |
|    6133 | 3855 | `			if( bOv ){` |
|       - | 3856 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      11 | 3857 | `				dAcc = bProduct ? (double)iAcc * (double)iVal : (double)iAcc + (double)iVal;` |
|      11 | 3858 | `				bReal = 1;` |
|       - | 3859 | `#else` |
|       - | 3860 | `				/* The integer-only build has no float to promote to, so it wraps --` |
|       - | 3861 | `				 * the same choice OP_ADD's overflow arm makes there. */` |
|       - | 3862 | `				iAcc = iRes;` |
|       - | 3863 | `#endif` |
|       6 | 3864 | `			}else{` |
|    6123 | 3865 | `				iAcc = iRes;` |
|       - | 3866 | `			}` |
|       - | 3867 | `		}` |
|    3120 | 3868 | `	}` |
|     803 | 3869 | `	if( bReal ){` |
|      62 | 3870 | `		ph7_result_double(pCtx,dAcc);` |
|      32 | 3871 | `	}else{` |
|     743 | 3872 | `		ph7_result_int64(pCtx,iAcc);` |
|       - | 3873 | `	}` |
|     803 | 3874 | `}` |
|       - | 3875 | `/* number array_sum(array $array )` |
|       - | 3876 | ` * (See block-coment above)` |
|       - | 3877 | ` */` |
|     768 | 3878 | `PH7_PRIVATE int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 3879 | `{` |
|       - | 3880 | `	ph7_hashmap *pMap;` |
|       - | 3881 | `	/* PHP requires exactly one argument */` |
|     772 | 3882 | `	if( nArg != 1 ){` |
|     ! 0 | 3883 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3884 | `			"ArgumentCountError",` |
|       - | 3885 | `			"array_sum() expects exactly 1 argument, %d given",` |
|     ! 0 | 3886 | `			nArg` |
|       - | 3887 | `			);` |
|       - | 3888 | `	}` |
|       - | 3889 | `	/* Make sure we are dealing with a valid hashmap */` |
|     772 | 3890 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3891 | `		/* Type mismatch -> TypeError (php's true/false/class-name convention). */` |
|       - | 3892 | `		char zBuf[64];` |
|       8 | 3893 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3894 | `			"TypeError",` |
|       - | 3895 | `			"array_sum(): Argument #1 ($array) must be of type array, %s given",` |
|       2 | 3896 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 3897 | `			);` |
|       - | 3898 | `	}` |
|     767 | 3899 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     767 | 3900 | `	if( pMap->nEntry < 1 ){` |
|       - | 3901 | `		/* Nothing to compute,return 0 */` |
|       9 | 3902 | `		ph7_result_int(pCtx,0);` |
|       9 | 3903 | `		return PH7_OK;` |
|       - | 3904 | `	}` |
|     759 | 3905 | `	HashmapArithFold(pCtx,pMap,0);` |
|     759 | 3906 | `	return PH7_OK;` |
|     388 | 3907 | `}` |
|       - | 3908 | `/*` |
|       - | 3909 | ` * number array_product(array $array )` |
|       - | 3910 | ` *  Calculate the product of values in an array.` |
|       - | 3911 | ` * Parameters` |
|       - | 3912 | ` *  $array: The input array.` |
|       - | 3913 | ` * Return` |
|       - | 3914 | ` *  Returns the product of values as an integer or float.` |
|       - | 3915 | ` */` |
|       - | 3916 | `/* number array_product(array $array )` |
|       - | 3917 | ` * (See block-block comment above)` |
|       - | 3918 | ` */` |
|      56 | 3919 | `PH7_PRIVATE int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3920 | `{` |
|       - | 3921 | `	ph7_hashmap *pMap;` |
|      57 | 3922 | `	if( nArg < 1 ){` |
|       - | 3923 | `		/* Missing arguments (arity is enforced upstream; defensive). */` |
|     ! 0 | 3924 | `		ph7_result_int(pCtx,1);` |
|     ! 0 | 3925 | `		return PH7_OK;` |
|       - | 3926 | `	}` |
|       - | 3927 | `	/* PHP 8: a non-array $array is a catchable TypeError, not a silent 0. */` |
|      57 | 3928 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3929 | `		char zBuf[64];` |
|      13 | 3930 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3931 | `			"TypeError",` |
|       - | 3932 | `			"array_product(): Argument #1 ($array) must be of type array, %s given",` |
|       4 | 3933 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 3934 | `			);` |
|       - | 3935 | `	}` |
|      49 | 3936 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      49 | 3937 | `	if( pMap->nEntry < 1 ){` |
|       - | 3938 | `		/* The product of an empty array is the multiplicative identity 1 (PHP). */` |
|       5 | 3939 | `		ph7_result_int(pCtx,1);` |
|       5 | 3940 | `		return PH7_OK;` |
|       - | 3941 | `	}` |
|      45 | 3942 | `	HashmapArithFold(pCtx,pMap,1);` |
|      45 | 3943 | `	return PH7_OK;` |
|      29 | 3944 | `}` |
|       - | 3945 | `/*` |
|       - | 3946 | ` * value array_rand(array $input[,int $num_req = 1 ])` |
|       - | 3947 | ` *  Pick one or more random entries out of an array.` |
|       - | 3948 | ` * Parameters` |
|       - | 3949 | ` * $input` |
|       - | 3950 | ` *  The input array.` |
|       - | 3951 | ` * $num_req` |
|       - | 3952 | ` *  Specifies how many entries you want to pick.` |
|       - | 3953 | ` * Return` |
|       - | 3954 | ` *  If you are picking only one entry, array_rand() returns the key for a random entry.` |
|       - | 3955 | ` *  Otherwise, it returns an array of keys for the random entries.` |
|       - | 3956 | ` *  NULL is returned on failure.` |
|       - | 3957 | ` */` |
|      36 | 3958 | `PH7_PRIVATE int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3959 | `{` |
|       - | 3960 | `	ph7_hashmap_node *pNode;` |
|       - | 3961 | `	ph7_hashmap *pMap;` |
|      37 | 3962 | `	int nItem = 1;` |
|      37 | 3963 | `	if( nArg < 1 ){` |
|       - | 3964 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|     ! 0 | 3965 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3966 | `		return PH7_OK;` |
|       - | 3967 | `	}` |
|       - | 3968 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|      37 | 3969 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 3970 | `		char zBuf[64];` |
|      10 | 3971 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3972 | `			"TypeError",` |
|       - | 3973 | `			"array_rand(): Argument #1 ($array) must be of type array, %s given",` |
|       3 | 3974 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 3975 | `			);` |
|       - | 3976 | `	}` |
|       - | 3977 | `	/* php validates $num (and weak-coerces it) BEFORE the empty-array body` |
|       - | 3978 | `	 * check, matching its ZPP-before-body ordering. */` |
|      31 | 3979 | `	if( nArg > 1 ){` |
|      23 | 3980 | `		ph7_value *pNum = apArg[1];` |
|      22 | 3981 | `		if( ph7_value_is_array(pNum) \|\| ph7_value_is_object(pNum)` |
|      23 | 3982 | `			\|\| ph7_value_is_resource(pNum) ){` |
|       - | 3983 | `			char zBuf[64];` |
|     ! 0 | 3984 | `			return PH7_VmThrowException(pCtx,` |
|       - | 3985 | `				"TypeError",` |
|       - | 3986 | `				"array_rand(): Argument #2 ($num) must be of type int, %s given",` |
|     ! 0 | 3987 | `				VmValueGivenName(pNum,zBuf,sizeof(zBuf))` |
|       - | 3988 | `				);` |
|       - | 3989 | `		}` |
|      23 | 3990 | `		if( ph7_value_is_string(pNum) ){` |
|       - | 3991 | `			/* Weak int coercion of a string $num follows php's numeric-string` |
|       - | 3992 | `			 * grammar (whole string, int or float): a non-numeric string` |
|       - | 3993 | `			 * (incl. leading-numeric junk like "2abc" or "0x1A") is a TypeError,` |
|       - | 3994 | `			 * a well-formed float-string ("1e3") coerces like a float value.` |
|       - | 3995 | `			 * Reuses the range() ZPP number parser (§3.9 shared-helper note). */` |
|       - | 3996 | `			int len;` |
|       9 | 3997 | `			const char *zStr = ph7_value_to_string(pNum, &len);` |
|       - | 3998 | `			sxi64 iLong; double dReal;` |
|       9 | 3999 | `			sxu8 iKind = RangeStrToNumber(zStr, (sxu32)len, &iLong, &dReal);` |
|       9 | 4000 | `			if( iKind == RANGE_IN_ERROR ){` |
|       7 | 4001 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4002 | `					"TypeError",` |
|       - | 4003 | `					"array_rand(): Argument #2 ($num) must be of type int, string given"` |
|       - | 4004 | `					);` |
|       - | 4005 | `			}` |
|       - | 4006 | `			/* Clamp into a signed-int band so an absurd magnitude still yields` |
|       - | 4007 | `			 * the out-of-range ValueError below without an out-of-int cast. */` |
|       3 | 4008 | `			if( iKind == RANGE_IN_DOUBLE ){` |
|       3 | 4009 | `				iLong = dReal <= 0.0 ? 0 : (dReal >= 2147483647.0 ? 2147483647 : (sxi64)dReal);` |
|       1 | 4010 | `			}` |
|       3 | 4011 | `			if( iLong > 2147483647 ){ iLong = 2147483647; }` |
|       3 | 4012 | `			else if( iLong < -2147483647 ){ iLong = -2147483647; }` |
|       3 | 4013 | `			nItem = (int)iLong;` |
|       2 | 4014 | `		}else{` |
|      15 | 4015 | `			nItem = ph7_value_to_int(pNum);` |
|       - | 4016 | `		}` |
|       8 | 4017 | `	}` |
|       - | 4018 | `	/* Point to the internal representation of the input hashmap */` |
|      25 | 4019 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4020 | `	/* php 8: an empty array is a ValueError, not a NULL return */` |
|      25 | 4021 | `	if( pMap->nEntry < 1 ){` |
|       5 | 4022 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4023 | `			"ValueError",` |
|       - | 4024 | `			"array_rand(): Argument #1 ($array) must not be empty"` |
|       - | 4025 | `			);` |
|       - | 4026 | `	}` |
|       - | 4027 | `	/* php 8: $num outside [1, count] is a ValueError, not a clamp/wrong value */` |
|      21 | 4028 | `	if( nItem < 1 \|\| nItem > (int)pMap->nEntry ){` |
|       9 | 4029 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4030 | `			"ValueError",` |
|       - | 4031 | `			"array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)"` |
|       - | 4032 | `			);` |
|       - | 4033 | `	}` |
|      13 | 4034 | `	if( nItem < 2 ){` |
|       - | 4035 | `		sxu32 nEntry;` |
|       - | 4036 | `		/* Pick a random slot through the MT19937 generator so array_rand()` |
|       - | 4037 | `		 * responds to srand()/mt_srand() (reproducible), like php. The exact` |
|       - | 4038 | `		 * index php lands on differs (php samples its internal hashtable` |
|       - | 4039 | `		 * buckets), so this is deterministic-under-seed but not value-parity. */` |
|       9 | 4040 | `		nEntry = (sxu32)PH7_VmMtRandRange(pMap->pVm,0,(sxi64)pMap->nEntry - 1);` |
|       - | 4041 | `		/* Extract the desired entry.` |
|       - | 4042 | `		 * Note that we perform a linear lookup here (later version must change this)` |
|       - | 4043 | `		 */` |
|       9 | 4044 | `		if( nEntry > pMap->nEntry / 2 ){` |
|       3 | 4045 | `			pNode = pMap->pLast;` |
|       3 | 4046 | `			nEntry = pMap->nEntry - nEntry;` |
|       3 | 4047 | `			if( nEntry > 1 ){` |
|     ! 0 | 4048 | `				for(;;){` |
|     ! 0 | 4049 | `					if( nEntry == 0 ){` |
|     ! 0 | 4050 | `						break;` |
|       - | 4051 | `					}` |
|       - | 4052 | `					/* Point to the previous entry */` |
|     ! 0 | 4053 | `					pNode = pNode->pNext; /* Reverse link */` |
|     ! 0 | 4054 | `					nEntry--;` |
|     ! 0 | 4055 | `				}` |
|     ! 0 | 4056 | `			}` |
|       3 | 4057 | `		}else{` |
|       7 | 4058 | `			pNode = pMap->pFirst;` |
|       3 | 4059 | `			for(;;){` |
|      10 | 4060 | `				if( nEntry == 0 ){` |
|       7 | 4061 | `					break;` |
|       - | 4062 | `				}` |
|       - | 4063 | `				/* Point to the next entry */` |
|       3 | 4064 | `				pNode = pNode->pPrev; /* Reverse link */` |
|       3 | 4065 | `				nEntry--;` |
|     ! 0 | 4066 | `			}` |
|       - | 4067 | `		}` |
|       9 | 4068 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - | 4069 | `			/* Int key */` |
|       7 | 4070 | `			ph7_result_int64(pCtx,pNode->xKey.iKey);` |
|       4 | 4071 | `		}else{` |
|       - | 4072 | `			/* Blob key */` |
|       3 | 4073 | `			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));` |
|       - | 4074 | `		}` |
|       5 | 4075 | `	}else{` |
|       - | 4076 | `		ph7_value sKey,*pArray;` |
|       - | 4077 | `		ph7_hashmap *pDest;` |
|       - | 4078 | `		/* Create a new array */` |
|       5 | 4079 | `		pArray = ph7_context_new_array(pCtx);` |
|       5 | 4080 | `		if( pArray == 0 ){` |
|     ! 0 | 4081 | `			ph7_result_null(pCtx);` |
|     ! 0 | 4082 | `			return PH7_OK;` |
|       - | 4083 | `		}` |
|       - | 4084 | `		/* Point to the internal representation of the hashmap */` |
|       5 | 4085 | `		pDest = (ph7_hashmap *)pArray->x.pOther;` |
|       5 | 4086 | `		PH7_MemObjInit(pDest->pVm,&sKey);` |
|       - | 4087 | `		/* Copy the first n items */` |
|       5 | 4088 | `		pNode = pMap->pFirst;` |
|       5 | 4089 | `		if( nItem > (int)pMap->nEntry ){` |
|     ! 0 | 4090 | `			nItem = (int)pMap->nEntry;` |
|     ! 0 | 4091 | `		}` |
|      15 | 4092 | `		while( nItem > 0){` |
|      11 | 4093 | `			PH7_HashmapExtractNodeKey(pNode,&sKey);` |
|      11 | 4094 | `			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);` |
|      11 | 4095 | `			PH7_MemObjRelease(&sKey);` |
|       - | 4096 | `			/* Point to the next entry */` |
|      11 | 4097 | `			pNode = pNode->pPrev; /* Reverse link */` |
|      11 | 4098 | `			nItem--;` |
|       1 | 4099 | `		}` |
|       - | 4100 | `		/* Shuffle the array */` |
|       5 | 4101 | `		HashmapMergeSort(pDest,HashmapCmpCallback7,0);` |
|       - | 4102 | `		/* Rehash node */` |
|       5 | 4103 | `		HashmapSortRehash(pDest);` |
|       - | 4104 | `		/* Return the random array */` |
|       5 | 4105 | `		ph7_result_value(pCtx,pArray);` |
|       - | 4106 | `	}` |
|      13 | 4107 | `	return PH7_OK;` |
|      19 | 4108 | `}` |
|       - | 4109 | `/*` |
|       - | 4110 | ` * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])` |
|       - | 4111 | ` *  Split an array into chunks.` |
|       - | 4112 | ` * Parameters` |
|       - | 4113 | ` * $input` |
|       - | 4114 | ` *   The array to work on` |
|       - | 4115 | ` * $size` |
|       - | 4116 | ` *   The size of each chunk` |
|       - | 4117 | ` * $preserve_keys` |
|       - | 4118 | ` *   When set to TRUE keys will be preserved. Default is FALSE which will reindex` |
|       - | 4119 | ` *   the chunk numerically.` |
|       - | 4120 | ` * Return` |
|       - | 4121 | ` *  Returns a multidimensional numerically indexed array, starting with` |
|       - | 4122 | ` *  zero, with each dimension containing size elements.` |
|       - | 4123 | ` */` |
|      40 | 4124 | `PH7_PRIVATE int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4125 | `{` |
|       - | 4126 | `	ph7_value *pArray,*pChunk;` |
|       - | 4127 | `	ph7_hashmap_node *pEntry;` |
|       - | 4128 | `	ph7_hashmap *pMap;` |
|       - | 4129 | `	int bPreserve;` |
|       - | 4130 | `	sxu32 nChunk;` |
|       - | 4131 | `	sxu32 nSize;` |
|       - | 4132 | `	sxu32 n;` |
|       - | 4133 | `	/* Argument count and types follow PHP semantics. */` |
|      45 | 4134 | `	if( nArg < 2 ){` |
|       - | 4135 | `		/* fewer than required arguments -> ArgumentCountError */` |
|     ! 0 | 4136 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4137 | `			"ArgumentCountError",` |
|       - | 4138 | `			"array_chunk() expects at least 2 arguments, %d given",` |
|     ! 0 | 4139 | `			nArg` |
|       - | 4140 | `			);` |
|       - | 4141 | `	}` |
|      45 | 4142 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4143 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4144 | `			"TypeError",` |
|       - | 4145 | `			"array_chunk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4146 | `			ph7_type_name(apArg[0])` |
|       - | 4147 | `			);` |
|       - | 4148 | `	}` |
|       - | 4149 | `	/* Create a new array */` |
|      42 | 4150 | `	pArray = ph7_context_new_array(pCtx);` |
|      42 | 4151 | `	if( pArray == 0 ){` |
|     ! 0 | 4152 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4153 | `		return PH7_OK;` |
|       - | 4154 | `	}` |
|       - | 4155 | `	/* Point to the internal representation of the input hashmap */` |
|      42 | 4156 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4157 | `	/* Extract and validate the chunk size argument. */` |
|       - | 4158 | `	/* Reject types that cannot be sensibly converted to an integer. */` |
|      57 | 4159 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1]) \|\|` |
|      80 | 4160 | `		ph7_value_is_resource(apArg[1]) \|\| ph7_value_is_null(apArg[1]) \|\|` |
|      38 | 4161 | `		ph7_value_is_bool(apArg[1]) ){` |
|     ! 0 | 4162 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4163 | `			"TypeError",` |
|       - | 4164 | `			"array_chunk(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 4165 | `			ph7_type_name(apArg[1])` |
|       - | 4166 | `			);` |
|       - | 4167 | `	}` |
|       - | 4168 | `	/* Strings that are non-numeric produce a TypeError.  Numeric` |
|       - | 4169 | `	 * strings are permitted; however those representing floats lose` |
|       - | 4170 | `	 * precision and PHP emits a deprecation warning. */` |
|      42 | 4171 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 4172 | `		int len;` |
|       6 | 4173 | `		sxu8 bReal = FALSE;` |
|       6 | 4174 | `		const char *zStr = ph7_value_to_string(apArg[1], &len);` |
|       6 | 4175 | `		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){` |
|       4 | 4176 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4177 | `				"TypeError",` |
|       - | 4178 | `				"array_chunk(): Argument #2 ($length) must be of type int, string given"` |
|       - | 4179 | `				);` |
|       - | 4180 | `		}` |
|       3 | 4181 | `		if( bReal ){` |
|       - | 4182 | `			/* php only DEPRECATES the lossy float-string -> int narrowing; §10 rejects` |
|       - | 4183 | `			 * it loudly, and the throw ABORTS the call (php would chunk the array).` |
|       - | 4184 | `			 * Twin-pinned by array_lossy_int_arg_abort{,_zend}.phpt. */` |
|       3 | 4185 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 4186 | `				"Implicit conversion from float-string to int loses precision");` |
|       - | 4187 | `		}` |
|     ! 0 | 4188 | `	}` |
|       - | 4189 | `	/* A FRACTIONAL float is the same §10 lossy narrowing and aborts the call; a whole` |
|       - | 4190 | `	 * float falls through to the ph7_value_to_int conversion below. */` |
|      37 | 4191 | `	if( ph7_value_is_float(apArg[1]) ){` |
|       6 | 4192 | `		double d = ph7_value_to_double(apArg[1]);` |
|       6 | 4193 | `		sxi64 i = (sxi64)d;` |
|       6 | 4194 | `		if( d != (double)i ){` |
|       6 | 4195 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 4196 | `				"Implicit conversion from float to int loses precision");` |
|       - | 4197 | `		}` |
|     ! 0 | 4198 | `	}` |
|       - | 4199 | `	/* Convert using ph7_value_to_int; now that float fractions are` |
|       - | 4200 | `	 * eliminated, this will not produce a warning. */` |
|       - | 4201 | `	{` |
|      33 | 4202 | `		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);` |
|      33 | 4203 | `		if( nSizeSigned < 1 ){` |
|       - | 4204 | `			/* size <= 0 -> ValueError */` |
|       6 | 4205 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4206 | `				"ValueError",` |
|       - | 4207 | `				"array_chunk(): Argument #2 ($length) must be greater than 0"` |
|       - | 4208 | `				);` |
|       - | 4209 | `		}` |
|      27 | 4210 | `		nSize = (sxu32)nSizeSigned;` |
|       - | 4211 | `	}` |
|      27 | 4212 | `	if( nSize >= pMap->nEntry ){` |
|       - | 4213 | `		/* Return the whole array */` |
|       3 | 4214 | `		ph7_array_add_elem(pArray,0,apArg[0]);` |
|       3 | 4215 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 4216 | `		return PH7_OK;` |
|       - | 4217 | `	}` |
|      25 | 4218 | `	bPreserve = 0;` |
|      25 | 4219 | `	if( nArg > 2 ){` |
|       - | 4220 | `		/* The third argument has a bool type hint in PHP.  Values that` |
|       - | 4221 | `		 * cannot be sensibly converted (arrays, objects, resources) are` |
|       - | 4222 | `		 * rejected with a TypeError.  Scalars and null coerce to bool` |
|       - | 4223 | `		 * normally, matching PHP behaviour. */` |
|      30 | 4224 | `		if( ph7_value_is_array(apArg[2]) \|\|` |
|      31 | 4225 | `			ph7_value_is_object(apArg[2]) \|\|` |
|      20 | 4226 | `			ph7_value_is_resource(apArg[2]) ){` |
|     ! 0 | 4227 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4228 | `				"TypeError",` |
|       - | 4229 | `				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",` |
|     ! 0 | 4230 | `				ph7_type_name(apArg[2])` |
|       - | 4231 | `				);` |
|       - | 4232 | `		}` |
|      21 | 4233 | `		bPreserve = ph7_value_to_bool(apArg[2]);` |
|      10 | 4234 | `	}` |
|       - | 4235 | `	/* Start processing */` |
|      25 | 4236 | `	pEntry = pMap->pFirst;` |
|      25 | 4237 | `	nChunk = 0;` |
|      25 | 4238 | `	pChunk = 0;` |
|      25 | 4239 | `	n = pMap->nEntry;` |
|      51 | 4240 | `	for( ;; ){` |
|     103 | 4241 | `		if( n < 1 ){` |
|       - | 4242 | `			/* When the loop terminates we may still have a current chunk` |
|       - | 4243 | `			 * that hasn't been added to the result array.  The previous` |
|       - | 4244 | `			 * implementation only pushed it if nChunk>0 which dropped the` |
|       - | 4245 | `			 * final chunk when the input size was an exact multiple of` |
|       - | 4246 | `			 * the chunk length.  Always append the pending chunk if it` |
|       - | 4247 | `			 * exists. */` |
|      25 | 4248 | `			if( pChunk ){` |
|      25 | 4249 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */` |
|      12 | 4250 | `			}` |
|      25 | 4251 | `			break;` |
|       - | 4252 | `		}` |
|      79 | 4253 | `		if( nChunk < 1 ){` |
|      67 | 4254 | `			if( pChunk ){` |
|       - | 4255 | `				/* Put the first chunk */` |
|      43 | 4256 | `				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */` |
|      21 | 4257 | `			}` |
|       - | 4258 | `			/* Create a new dimension */` |
|      67 | 4259 | `			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything` |
|       - | 4260 | `												   * will be automatically released as soon we return` |
|       - | 4261 | `												   * from this function */` |
|      67 | 4262 | `			if( pChunk == 0 ){` |
|     ! 0 | 4263 | `				break;` |
|       - | 4264 | `			}` |
|      67 | 4265 | `			nChunk = nSize;` |
|      33 | 4266 | `		}` |
|       - | 4267 | `		/* Insert the entry */` |
|      79 | 4268 | `		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);` |
|       - | 4269 | `		/* Point to the next entry */` |
|      79 | 4270 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      79 | 4271 | `		nChunk--;` |
|      79 | 4272 | `		n--;` |
|       1 | 4273 | `	}` |
|       - | 4274 | `	/* Return the multidimensional array */` |
|      25 | 4275 | `	ph7_result_value(pCtx,pArray);` |
|      25 | 4276 | `	return PH7_OK;` |
|      25 | 4277 | `}` |
|       - | 4278 | `/*` |
|       - | 4279 | ` * array array_pad(array $input,int $pad_size,value $pad_value)` |
|       - | 4280 | ` *  Pad array to the specified length with a value.` |
|       - | 4281 | ` * $input` |
|       - | 4282 | ` *   Initial array of values to pad.` |
|       - | 4283 | ` * $pad_size` |
|       - | 4284 | ` *   New size of the array.` |
|       - | 4285 | ` * $pad_value` |
|       - | 4286 | ` *   Value to pad if input is less than pad_size.` |
|       - | 4287 | ` */` |
|       - | 4288 | `/*` |
|       - | 4289 | ` * Shared "requested array size too large" guard (band A #8). php throws a` |
|       - | 4290 | ` * catchable ValueError when a builtin's caller-controlled target length` |
|       - | 4291 | ` * exceeds its hashtable capacity HT_MAX_SIZE (2^30 elements; probed against` |
|       - | 4292 | ` * php 8.5.7 — the boundary sits exactly between 1073741824 and 1073741825,` |
|       - | 4293 | ` * independent of the input array's size and symmetric for negative lengths).` |
|       - | 4294 | ` * Without this, a call like array_pad([1,2], 2000000000, 0) sits in the fill` |
|       - | 4295 | ` * loop for minutes and then OOMs. nRequested is the ABSOLUTE requested` |
|       - | 4296 | ` * length; pass a still-negative value (e.g. the unnegatable INT64_MIN,` |
|       - | 4297 | ` * mirroring php's ZEND_ABS overflow) to fail the guard unconditionally.` |
|       - | 4298 | ` * Returns SXRET_OK when the size is acceptable, else the throw status to` |
|       - | 4299 | ` * propagate. The cap constant is shared with range()'s guards` |
|       - | 4300 | ` * (PH7_RANGE_HT_MAX_SIZE above).` |
|       - | 4301 | ` */` |
|      50 | 4302 | `static sxi32 HashmapGuardArraySize(` |
|       - | 4303 | `	ph7_context *pCtx,` |
|       - | 4304 | `	const char *zFunc,     /* Function name for the message */` |
|       - | 4305 | `	int iArg,              /* 1-based argument position */` |
|       - | 4306 | `	const char *zParam     /* "$length"-style parameter name */,` |
|       - | 4307 | `	sxi64 nRequested       /* Absolute requested element count */` |
|       - | 4308 | `	)` |
|       1 | 4309 | `{` |
|      51 | 4310 | `	if( nRequested < 0 \|\| nRequested > PH7_RANGE_HT_MAX_SIZE ){` |
|      22 | 4311 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4312 | `			"ValueError",` |
|       - | 4313 | `			"%s(): Argument #%d (%s) must not exceed the maximum allowed array size",` |
|       7 | 4314 | `			zFunc,iArg,zParam` |
|       - | 4315 | `			);` |
|       - | 4316 | `	}` |
|      37 | 4317 | `	return SXRET_OK;` |
|      26 | 4318 | `}` |
|      60 | 4319 | `PH7_PRIVATE int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4320 | `{` |
|       - | 4321 | `	ph7_hashmap *pMap;` |
|       - | 4322 | `	ph7_value *pArray;` |
|       - | 4323 | `	sxi64 iLen,iAbs;` |
|       - | 4324 | `	int nEntry;` |
|       - | 4325 | `	sxi32 rc;` |
|      62 | 4326 | `	if( nArg != 3 ){` |
|     ! 0 | 4327 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4328 | `			"ArgumentCountError",` |
|       - | 4329 | `			"array_pad() expects exactly 3 arguments, %d given",` |
|     ! 0 | 4330 | `			nArg` |
|       - | 4331 | `			);` |
|       - | 4332 | `	}` |
|      62 | 4333 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4334 | `		char zBuf[64];` |
|      11 | 4335 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4336 | `			"TypeError",` |
|       - | 4337 | `			"array_pad(): Argument #1 ($array) must be of type array, %s given",` |
|       3 | 4338 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4339 | `			);` |
|       - | 4340 | `	}` |
|       - | 4341 | `	/* php 8: $length must be int-coercible. An array/object/resource or a` |
|       - | 4342 | `	 * non-numeric string throws a TypeError instead of silently padding to 0;` |
|       - | 4343 | `	 * a numeric string is weak-coerced via php's is_numeric_string grammar` |
|       - | 4344 | `	 * (reusing the shared RangeStrToNumber, like array_rand's $num). */` |
|      54 | 4345 | `	if( ph7_value_is_array(apArg[1]) \|\| ph7_value_is_object(apArg[1])` |
|      55 | 4346 | `		\|\| ph7_value_is_resource(apArg[1]) ){` |
|       - | 4347 | `		char zBuf[64];` |
|     ! 0 | 4348 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4349 | `			"TypeError",` |
|       - | 4350 | `			"array_pad(): Argument #2 ($length) must be of type int, %s given",` |
|     ! 0 | 4351 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))` |
|       - | 4352 | `			);` |
|       - | 4353 | `	}` |
|      55 | 4354 | `	if( ph7_value_is_string(apArg[1]) ){` |
|       - | 4355 | `		int nStr;` |
|      11 | 4356 | `		const char *zStr = ph7_value_to_string(apArg[1],&nStr);` |
|       - | 4357 | `		sxi64 iLong; double dReal;` |
|      11 | 4358 | `		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);` |
|      11 | 4359 | `		if( iKind == RANGE_IN_ERROR ){` |
|       5 | 4360 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4361 | `				"TypeError",` |
|       - | 4362 | `				"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|       - | 4363 | `				);` |
|       - | 4364 | `		}` |
|       7 | 4365 | `		if( iKind == RANGE_IN_DOUBLE ){` |
|       - | 4366 | `			/* php ZPP: a float-string outside the int64 range (or NaN) fails` |
|       - | 4367 | `			 * outright — also keeps the (sxi64) cast below UB-free. */` |
|       3 | 4368 | `			if( dReal != dReal \|\| dReal >= 9223372036854775808.0 \|\| dReal < -9223372036854775808.0 ){` |
|     ! 0 | 4369 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4370 | `					"TypeError",` |
|       - | 4371 | `					"array_pad(): Argument #2 ($length) must be of type int, string given"` |
|       - | 4372 | `					);` |
|       - | 4373 | `			}` |
|       3 | 4374 | `			iLen = (sxi64)dReal;` |
|       3 | 4375 | `			if( (double)iLen != dReal ){` |
|     ! 0 | 4376 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 4377 | `					"array_pad(): Argument #2 ($length) must be of type int, string given");` |
|       - | 4378 | `			}` |
|       2 | 4379 | `		}else{` |
|       5 | 4380 | `			iLen = iLong;` |
|       - | 4381 | `		}` |
|       4 | 4382 | `	}else{` |
|      45 | 4383 | `		iLen = ph7_value_to_int64(apArg[1]);` |
|       - | 4384 | `	}` |
|       - | 4385 | `	/* Point to the internal representation of the input hashmap */` |
|      51 | 4386 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4387 | `	/* php caps abs($length) at HT_MAX_SIZE either direction (INT64_MIN stays` |
|       - | 4388 | `	 * negative through the ABS, failing the guard like php's own ZEND_ABS` |
|       - | 4389 | `	 * overflow). */` |
|      51 | 4390 | `	iAbs = iLen;` |
|      51 | 4391 | `	if( iAbs < 0 && iAbs != (sxi64)-9223372036854775807LL - 1 ){` |
|      15 | 4392 | `		iAbs = -iAbs;` |
|       7 | 4393 | `	}` |
|      51 | 4394 | `	rc = HashmapGuardArraySize(pCtx,"array_pad",2,"$length",iAbs);` |
|      51 | 4395 | `	if( rc != SXRET_OK ){` |
|      15 | 4396 | `		return rc;` |
|       - | 4397 | `	}` |
|      37 | 4398 | `	nEntry = (int)iLen;` |
|       - | 4399 | `	/* Create a new array */` |
|      37 | 4400 | `	pArray = ph7_context_new_array(pCtx);` |
|      37 | 4401 | `	if( pArray == 0 ){` |
|     ! 0 | 4402 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 4403 | `	}` |
|      37 | 4404 | `	if( nEntry < 0 ){` |
|      11 | 4405 | `		nEntry = -nEntry;` |
|      11 | 4406 | `		if( nEntry > (int)pMap->nEntry ){` |
|       7 | 4407 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 4408 | `			/* Insert given items first */` |
|      25 | 4409 | `			while( nEntry > 0 ){` |
|      19 | 4410 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 4411 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 4412 | `				}` |
|      19 | 4413 | `				nEntry--;` |
|       1 | 4414 | `			}` |
|       - | 4415 | `			/* Merge the two arrays */` |
|       7 | 4416 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       4 | 4417 | `		}else{` |
|       5 | 4418 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       1 | 4419 | `		}` |
|      32 | 4420 | `	}else if( nEntry > 0 ){` |
|      25 | 4421 | `		if( nEntry > (int)pMap->nEntry ){` |
|      19 | 4422 | `			nEntry -= (int)pMap->nEntry;` |
|       - | 4423 | `			/* Merge the two arrays first */` |
|      19 | 4424 | `			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 4425 | `			/* Insert given items */` |
|     275 | 4426 | `			while( nEntry > 0 ){` |
|     257 | 4427 | `				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){` |
|     ! 0 | 4428 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 4429 | `				}` |
|     257 | 4430 | `				nEntry--;` |
|       1 | 4431 | `			}` |
|      10 | 4432 | `		}else{` |
|       7 | 4433 | `			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 4434 | `		}` |
|      13 | 4435 | `	}else{` |
|       - | 4436 | `		/* nEntry == 0: return a copy of the input array */` |
|       3 | 4437 | `		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 4438 | `	}` |
|       - | 4439 | `	/* Return the new array */` |
|      37 | 4440 | `	ph7_result_value(pCtx,pArray);` |
|      37 | 4441 | `	return PH7_OK;` |
|      32 | 4442 | `}` |
|       - | 4443 | `/*` |
|       - | 4444 | ` * array array_replace(array &$array,array &$array1,...)` |
|       - | 4445 | ` *  Replaces elements from passed arrays into the first array.` |
|       - | 4446 | ` * Parameters` |
|       - | 4447 | ` * $array` |
|       - | 4448 | ` *   The array in which elements are replaced.` |
|       - | 4449 | ` * $array1` |
|       - | 4450 | ` *   The array from which elements will be extracted.` |
|       - | 4451 | ` * ....` |
|       - | 4452 | ` *  More arrays from which elements will be extracted.` |
|       - | 4453 | ` *  Values from later arrays overwrite the previous values.` |
|       - | 4454 | ` * Return` |
|       - | 4455 | ` *  Returns an array.` |
|       - | 4456 | ` *  Throws ArgumentCountError if no arguments are given.` |
|       - | 4457 | ` *  Throws TypeError if any argument is not an array.` |
|       - | 4458 | ` */` |
|      20 | 4459 | `PH7_PRIVATE int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 4460 | `{` |
|       - | 4461 | `	ph7_hashmap *pMap;` |
|       - | 4462 | `	ph7_value *pArray;` |
|       - | 4463 | `	int i;` |
|      23 | 4464 | `	if( nArg < 1 ){` |
|     ! 0 | 4465 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4466 | `			"ArgumentCountError",` |
|       - | 4467 | `			"array_replace() expects at least 1 argument, 0 given"` |
|       - | 4468 | `			);` |
|       - | 4469 | `	}` |
|      23 | 4470 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4471 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4472 | `			"TypeError",` |
|       - | 4473 | `			"array_replace(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4474 | `			ph7_type_name(apArg[0])` |
|       - | 4475 | `			);` |
|       - | 4476 | `	}` |
|       - | 4477 | `	/* Create a new array */` |
|      20 | 4478 | `	pArray = ph7_context_new_array(pCtx);` |
|      20 | 4479 | `	if( pArray == 0 ){` |
|     ! 0 | 4480 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4481 | `		return PH7_OK;` |
|       - | 4482 | `	}` |
|       - | 4483 | `	/* Overwrite from the first array */` |
|      20 | 4484 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      20 | 4485 | `	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       - | 4486 | `	/* Perform the requested operation for remaining arrays */` |
|      36 | 4487 | `	for( i = 1 ; i < nArg ; i++ ){` |
|      20 | 4488 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       - | 4489 | `			/* Type mismatch -> TypeError */` |
|       4 | 4490 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4491 | `				"TypeError",` |
|       - | 4492 | `				"array_replace(): Argument #%d must be of type array, %s given",` |
|       1 | 4493 | `				i + 1,` |
|       2 | 4494 | `				ph7_type_name(apArg[i])` |
|       - | 4495 | `				);` |
|       - | 4496 | `		}` |
|       - | 4497 | `		/* Point to the internal representation of the input hashmap */` |
|      17 | 4498 | `		pMap = (ph7_hashmap *)apArg[i]->x.pOther;` |
|      17 | 4499 | `		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);` |
|       9 | 4500 | `	}` |
|       - | 4501 | `	/* Return the new array */` |
|      17 | 4502 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 4503 | `	return PH7_OK;` |
|      13 | 4504 | `}` |
|       - | 4505 | `/*` |
|       - | 4506 | ` * array array_filter(array $input [,callback $callback ])` |
|       - | 4507 | ` *  Filters elements of an array using a callback function.` |
|       - | 4508 | ` * Parameters` |
|       - | 4509 | ` *  $input` |
|       - | 4510 | ` *    The array to iterate over` |
|       - | 4511 | ` * $callback` |
|       - | 4512 | ` *    The callback function to use` |
|       - | 4513 | ` *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)` |
|       - | 4514 | ` *    will be removed.` |
|       - | 4515 | ` * Return` |
|       - | 4516 | ` *  The filtered array.` |
|       - | 4517 | ` */` |
|      34 | 4518 | `PH7_PRIVATE int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4519 | `{` |
|       - | 4520 | `	ph7_hashmap_node *pEntry;` |
|       - | 4521 | `	ph7_hashmap *pMap;` |
|       - | 4522 | `	ph7_value *pArray;` |
|       - | 4523 | `	ph7_value sResult;   /* Callback result */` |
|       - | 4524 | `	ph7_value *pValue;` |
|       - | 4525 | `	sxi32 rc;` |
|       - | 4526 | `	int keep;` |
|       - | 4527 | `	sxu32 n;` |
|      36 | 4528 | `	if( nArg < 1 ){` |
|       - | 4529 | `		/* Missing argument (arity is enforced upstream; defensive) */` |
|     ! 0 | 4530 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4531 | `		return PH7_OK;` |
|       - | 4532 | `	}` |
|       - | 4533 | `	/* php 8: $array must be an array (TypeError, not a silent NULL return) */` |
|      36 | 4534 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       - | 4535 | `		char zBuf[64];` |
|      16 | 4536 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4537 | `			"TypeError",` |
|       - | 4538 | `			"array_filter(): Argument #1 ($array) must be of type array, %s given",` |
|       5 | 4539 | `			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))` |
|       - | 4540 | `			);` |
|       - | 4541 | `	}` |
|       - | 4542 | ``	/* php validates the callback UP FRONT, so `array_filter([], 'nosuchfn')` throws too —`` |
|       - | 4543 | `	 * PHL checked inside the element loop, which an empty array never entered. */` |
|      26 | 4544 | `	if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|      20 | 4545 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",TRUE);` |
|      20 | 4546 | `		if( rcCb != PH7_OK ){` |
|       8 | 4547 | `			return rcCb;` |
|       - | 4548 | `		}` |
|       6 | 4549 | `	}` |
|       - | 4550 | `	/* Create a new array */` |
|      19 | 4551 | `	pArray = ph7_context_new_array(pCtx);` |
|      19 | 4552 | `	if( pArray == 0 ){` |
|     ! 0 | 4553 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4554 | `		return PH7_OK;` |
|       - | 4555 | `	}` |
|       - | 4556 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 4557 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      19 | 4558 | `	pEntry = pMap->pFirst;` |
|      19 | 4559 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      19 | 4560 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 4561 | `	/* Perform the requested operation */` |
|      83 | 4562 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 4563 | `		/* Extract node value (may be NULL if allocation failed) */` |
|      67 | 4564 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      67 | 4565 | `		if( pValue == 0 ){` |
|       - | 4566 | `			/* Can happen if SySetAt() failed earlier; drop the entry. */` |
|     ! 0 | 4567 | `			keep = FALSE;` |
|      67 | 4568 | `		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){` |
|       - | 4569 | `			/* Callback supplied (not NULL) and already validated above. */` |
|      39 | 4570 | `			keep = FALSE;` |
|      39 | 4571 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);` |
|      39 | 4572 | `			if( rc == PH7_EXCEPTION ){` |
|       - | 4573 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 4574 | `				PH7_MemObjRelease(&sResult);` |
|       3 | 4575 | `				return PH7_EXCEPTION;` |
|       - | 4576 | `			}` |
|      37 | 4577 | `			if( rc == SXRET_OK ){` |
|       - | 4578 | `				/* Perform a boolean cast */` |
|      37 | 4579 | `				keep = ph7_value_to_bool(&sResult);` |
|      18 | 4580 | `			}` |
|      37 | 4581 | `			PH7_MemObjRelease(&sResult);` |
|      19 | 4582 | `		}else{` |
|       - | 4583 | `			/* No callback provided or callback explicitly NULL: use default` |
|       - | 4584 | `			 * behaviour where "empty" values are removed. This also covers` |
|       - | 4585 | `			 * the case where the callback argument is missing entirely.` |
|       - | 4586 | `			 */` |
|      29 | 4587 | `			keep = !PH7_MemObjIsEmpty(pValue);` |
|       - | 4588 | `		}` |
|      65 | 4589 | `		if( keep ){` |
|       - | 4590 | `			/* Perform the insertion,now the callback returned true */` |
|      25 | 4591 | `			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);` |
|      12 | 4592 | `		}` |
|       - | 4593 | `		/* Point to the next entry */` |
|      65 | 4594 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      33 | 4595 | `	}` |
|      17 | 4596 | `	ph7_result_value(pCtx,pArray);` |
|      17 | 4597 | `	return PH7_OK;` |
|      19 | 4598 | `}` |
|       - | 4599 | `/*` |
|       - | 4600 | ` * array array_map(?callable $callback, array $array, array ...$arrays)` |
|       - | 4601 | ` *  Applies the callback to the elements of the given arrays.` |
|       - | 4602 | ` * Parameters` |
|       - | 4603 | ` *  $callback` |
|       - | 4604 | ` *   A callable to run for each element in each array, or NULL. With a single` |
|       - | 4605 | ` *   array and a NULL callback this is the identity function (the array is` |
|       - | 4606 | ` *   returned unchanged); with several arrays and a NULL callback the arrays` |
|       - | 4607 | ` *   are zipped together.` |
|       - | 4608 | ` *  $array` |
|       - | 4609 | ` *   The first array to run through the callback function.` |
|       - | 4610 | ` *  $arrays` |
|       - | 4611 | ` *   Zero or more additional arrays to process in parallel.` |
|       - | 4612 | ` * Return` |
|       - | 4613 | ` *  Returns an array containing the results of applying the callback function.` |
|       - | 4614 | ` *  With a single array the keys are preserved; with several arrays the result` |
|       - | 4615 | ` *  is re-indexed and the iteration runs to the length of the longest array,` |
|       - | 4616 | ` *  padding shorter arrays with NULL.` |
|       - | 4617 | ` */` |
|     150 | 4618 | `PH7_PRIVATE int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4619 | `{` |
|       - | 4620 | `	ph7_value *pArray,*pValue,sKey,sResult;` |
|       - | 4621 | `	ph7_hashmap_node *pEntry;` |
|       - | 4622 | `	ph7_hashmap *pMap;` |
|       - | 4623 | `	ph7_vm *pVm;` |
|       - | 4624 | `	int bNullCallback;` |
|       - | 4625 | `	sxi32 rc;` |
|       - | 4626 | `	int i;` |
|       - | 4627 | `	sxu32 n;` |
|     155 | 4628 | `	if( nArg < 2 ){` |
|     ! 0 | 4629 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4630 | `			"ArgumentCountError",` |
|       - | 4631 | `			"array_map() expects at least 2 arguments, %d given",` |
|     ! 0 | 4632 | `			nArg` |
|       - | 4633 | `			);` |
|       - | 4634 | `	}` |
|     155 | 4635 | `	bNullCallback = ph7_value_is_null(apArg[0]);` |
|     155 | 4636 | `	if( !bNullCallback ){` |
|     149 | 4637 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[0],1,"callback",TRUE);` |
|     149 | 4638 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|      68 | 4639 | `	}` |
|       - | 4640 | `	/* Every remaining argument must be an array */` |
|     299 | 4641 | `	for( i = 1 ; i < nArg ; i++ ){` |
|     159 | 4642 | `		if( !ph7_value_is_array(apArg[i]) ){` |
|       3 | 4643 | `			if( i == 1 ){` |
|       4 | 4644 | `				return PH7_VmThrowException(pCtx,` |
|       - | 4645 | `					"TypeError",` |
|       - | 4646 | `					"array_map(): Argument #2 ($array) must be of type array, %s given",` |
|       2 | 4647 | `					ph7_type_name(apArg[1])` |
|       - | 4648 | `					);` |
|       - | 4649 | `			}` |
|     ! 0 | 4650 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4651 | `				"TypeError",` |
|       - | 4652 | `				"array_map(): Argument #%d must be of type array, %s given",` |
|     ! 0 | 4653 | `				i+1,ph7_type_name(apArg[i])` |
|       - | 4654 | `				);` |
|       - | 4655 | `		}` |
|      81 | 4656 | `	}` |
|     145 | 4657 | `	pVm = pCtx->pVm;` |
|       - | 4658 | `	/* Create a new array */` |
|     145 | 4659 | `	pArray = ph7_context_new_array(pCtx);` |
|     145 | 4660 | `	if( pArray == 0 ){` |
|     ! 0 | 4661 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4662 | `		return PH7_OK;` |
|       - | 4663 | `	}` |
|     145 | 4664 | `	PH7_MemObjInit(pVm,&sResult);` |
|     145 | 4665 | `	PH7_MemObjInit(pVm,&sKey);` |
|     145 | 4666 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|     145 | 4667 | `	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */` |
|     145 | 4668 | `	if( nArg == 2 ){` |
|       - | 4669 | `		/* Single-array mode: keys are preserved (PHP semantics). */` |
|     135 | 4670 | `		pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|     135 | 4671 | `		pEntry = pMap->pFirst;` |
|     945 | 4672 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 4673 | `			/* Extract the node value */` |
|     819 | 4674 | `			pValue = HashmapExtractNodeValue(pEntry);` |
|     819 | 4675 | `			if( pValue ){` |
|       - | 4676 | `				/* Extract the node key */` |
|     819 | 4677 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|     819 | 4678 | `				if( bNullCallback ){` |
|       - | 4679 | `					/* NULL callback: identity function, keep original value */` |
|      11 | 4680 | `					ph7_array_add_elem(pArray,&sKey,pValue);` |
|       6 | 4681 | `				}else{` |
|       - | 4682 | `					/* Invoke the supplied callback */` |
|     809 | 4683 | `					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);` |
|     809 | 4684 | `					if( rc == PH7_EXCEPTION ){` |
|       - | 4685 | `						/* Callback raised: abort and let the foreign-function` |
|       - | 4686 | `						 * dispatcher unwind through the nearest try/catch. */` |
|       5 | 4687 | `						PH7_MemObjRelease(&sKey);` |
|       5 | 4688 | `						PH7_MemObjRelease(&sResult);` |
|       5 | 4689 | `						return PH7_EXCEPTION;` |
|       - | 4690 | `					}` |
|       - | 4691 | `					/* Insert the callback return value */` |
|     805 | 4692 | `					ph7_array_add_elem(pArray,&sKey,&sResult);` |
|       - | 4693 | `				}` |
|     815 | 4694 | `				PH7_MemObjRelease(&sKey);` |
|     815 | 4695 | `				PH7_MemObjRelease(&sResult);` |
|     405 | 4696 | `			}` |
|       - | 4697 | `			/* Point to the next entry */` |
|     815 | 4698 | `			pEntry = pEntry->pPrev; /* Reverse link */` |
|     410 | 4699 | `		}` |
|      68 | 4700 | `	}else{` |
|       - | 4701 | `		/* Multi-array mode: walk every array in parallel to the length of the` |
|       - | 4702 | `		 * longest one, pad shorter arrays with NULL, and re-index the result. */` |
|      11 | 4703 | `		int nArrays = nArg - 1;` |
|       - | 4704 | `		ph7_hashmap_node **apCur;` |
|       - | 4705 | `		ph7_value **apCallArg;` |
|       - | 4706 | `		ph7_value sNull;` |
|      11 | 4707 | `		sxu32 nMax = 0;` |
|      11 | 4708 | `		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));` |
|      11 | 4709 | `		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));` |
|      11 | 4710 | `		if( apCur == 0 \|\| apCallArg == 0 ){` |
|     ! 0 | 4711 | `			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }` |
|     ! 0 | 4712 | `			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }` |
|     ! 0 | 4713 | `			PH7_MemObjRelease(&sKey);` |
|     ! 0 | 4714 | `			PH7_MemObjRelease(&sResult);` |
|     ! 0 | 4715 | `			ph7_result_value(pCtx,pArray);` |
|     ! 0 | 4716 | `			return PH7_OK;` |
|       - | 4717 | `		}` |
|      11 | 4718 | `		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */` |
|      11 | 4719 | `		sNull.nIdx = SXU32_HIGH;` |
|      33 | 4720 | `		for( i = 0 ; i < nArrays ; i++ ){` |
|      23 | 4721 | `			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;` |
|      23 | 4722 | `			apCur[i] = pMap->pFirst;` |
|      23 | 4723 | `			if( pMap->nEntry > nMax ){` |
|      13 | 4724 | `				nMax = pMap->nEntry;` |
|       6 | 4725 | `			}` |
|      12 | 4726 | `		}` |
|      35 | 4727 | `		for( n = 0 ; n < nMax ; n++ ){` |
|      25 | 4728 | `			ph7_value *pZip = 0;` |
|      25 | 4729 | `			if( bNullCallback ){` |
|       - | 4730 | `				/* zip: each result element is an array of the i-th values */` |
|       5 | 4731 | `				pZip = ph7_context_new_array(pCtx);` |
|       2 | 4732 | `			}` |
|      79 | 4733 | `			for( i = 0 ; i < nArrays ; i++ ){` |
|      55 | 4734 | `				ph7_value *pv = &sNull;` |
|      55 | 4735 | `				if( apCur[i] ){` |
|      53 | 4736 | `					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);` |
|      53 | 4737 | `					if( pNodeVal ){` |
|      53 | 4738 | `						pv = pNodeVal;` |
|      26 | 4739 | `					}` |
|      53 | 4740 | `					apCur[i] = apCur[i]->pPrev; /* Reverse link */` |
|      26 | 4741 | `				}` |
|      55 | 4742 | `				if( bNullCallback ){` |
|       9 | 4743 | `					if( pZip ){` |
|       9 | 4744 | `						ph7_array_add_elem(pZip,0,pv);` |
|       4 | 4745 | `					}` |
|       5 | 4746 | `				}else{` |
|      47 | 4747 | `					apCallArg[i] = pv;` |
|       - | 4748 | `				}` |
|      28 | 4749 | `			}` |
|      25 | 4750 | `			if( bNullCallback ){` |
|       5 | 4751 | `				if( pZip ){` |
|       5 | 4752 | `					ph7_array_add_elem(pArray,0,pZip);` |
|       2 | 4753 | `				}` |
|       3 | 4754 | `			}else{` |
|      21 | 4755 | `				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);` |
|      21 | 4756 | `				if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 4757 | `					SyMemBackendFree(&pVm->sAllocator,apCur);` |
|     ! 0 | 4758 | `					SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|     ! 0 | 4759 | `					PH7_MemObjRelease(&sNull);` |
|     ! 0 | 4760 | `					PH7_MemObjRelease(&sKey);` |
|     ! 0 | 4761 | `					PH7_MemObjRelease(&sResult);` |
|     ! 0 | 4762 | `					return PH7_EXCEPTION;` |
|       - | 4763 | `				}` |
|      21 | 4764 | `				ph7_array_add_elem(pArray,0,&sResult);` |
|      21 | 4765 | `				PH7_MemObjRelease(&sResult);` |
|       - | 4766 | `			}` |
|      13 | 4767 | `		}` |
|      11 | 4768 | `		SyMemBackendFree(&pVm->sAllocator,apCur);` |
|      11 | 4769 | `		SyMemBackendFree(&pVm->sAllocator,apCallArg);` |
|      11 | 4770 | `		PH7_MemObjRelease(&sNull);` |
|       - | 4771 | `	}` |
|     141 | 4772 | `	PH7_MemObjRelease(&sKey);` |
|     141 | 4773 | `	PH7_MemObjRelease(&sResult);` |
|     141 | 4774 | `	ph7_result_value(pCtx,pArray);` |
|     141 | 4775 | `	return PH7_OK;` |
|      80 | 4776 | `}` |
|       - | 4777 | `/*` |
|       - | 4778 | ` * value array_reduce(array $array, callable $callback[, value $initial = NULL])` |
|       - | 4779 | ` *  Iteratively reduce the array to a single value using a callback function.` |
|       - | 4780 | ` * Parameters` |
|       - | 4781 | ` *  $array` |
|       - | 4782 | ` *   The input array.` |
|       - | 4783 | ` *  $callback` |
|       - | 4784 | ` *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed` |
|       - | 4785 | ` *  $initial` |
|       - | 4786 | ` *   If the optional initial is available, it will be used at the beginning` |
|       - | 4787 | ` *   of the process, or as a final result in case the array is empty.` |
|       - | 4788 | ` * Return` |
|       - | 4789 | ` *  Returns the resulting value.` |
|       - | 4790 | ` *  If the array is empty and initial is not passed, array_reduce() returns NULL.` |
|       - | 4791 | ` */` |
|      30 | 4792 | `PH7_PRIVATE int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4793 | `{` |
|       - | 4794 | `	ph7_hashmap_node *pEntry;` |
|       - | 4795 | `	ph7_hashmap *pMap;` |
|       - | 4796 | `	ph7_value *pValue;` |
|       - | 4797 | `	ph7_value sResult;` |
|       - | 4798 | `	sxi32 rc;` |
|       - | 4799 | `	sxu32 n;` |
|      35 | 4800 | `	if( nArg < 2 ){` |
|     ! 0 | 4801 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4802 | `			"ArgumentCountError",` |
|       - | 4803 | `			"array_reduce() expects at least 2 arguments, %d given",` |
|     ! 0 | 4804 | `			nArg` |
|       - | 4805 | `			);` |
|       - | 4806 | `	}` |
|      35 | 4807 | `	if( nArg > 3 ){` |
|     ! 0 | 4808 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4809 | `			"ArgumentCountError",` |
|       - | 4810 | `			"array_reduce() expects at most 3 arguments, %d given",` |
|     ! 0 | 4811 | `			nArg` |
|       - | 4812 | `			);` |
|       - | 4813 | `	}` |
|      35 | 4814 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4815 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4816 | `			"TypeError",` |
|       - | 4817 | `			"array_reduce(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4818 | `			ph7_type_name(apArg[0])` |
|       - | 4819 | `			);` |
|       - | 4820 | `	}` |
|       - | 4821 | `	{` |
|      33 | 4822 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|      33 | 4823 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 4824 | `	}` |
|       - | 4825 | `	/* Point to the internal representation of the input hashmap */` |
|      19 | 4826 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 4827 | `	/* Assume a NULL initial value */` |
|      19 | 4828 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      19 | 4829 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      19 | 4830 | `	if( nArg > 2 ){` |
|       - | 4831 | `		/* Set the initial value */` |
|      13 | 4832 | `		PH7_MemObjLoad(apArg[2],&sResult);` |
|       6 | 4833 | `	}` |
|       - | 4834 | `	/* Perform the requested operation */` |
|      19 | 4835 | `	pEntry = pMap->pFirst;` |
|      55 | 4836 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 4837 | `		/* Extract the node value */` |
|      39 | 4838 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|       - | 4839 | `		/* Invoke the supplied callback */` |
|      39 | 4840 | `		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);` |
|      39 | 4841 | `		if( rc == PH7_EXCEPTION ){` |
|       - | 4842 | `			/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 4843 | `			PH7_MemObjRelease(&sResult);` |
|       3 | 4844 | `			return PH7_EXCEPTION;` |
|       - | 4845 | `		}` |
|       - | 4846 | `		/* Point to the next entry */` |
|      37 | 4847 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 4848 | `	}` |
|      17 | 4849 | `	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */` |
|      17 | 4850 | `	PH7_MemObjRelease(&sResult);` |
|      17 | 4851 | `	return PH7_OK;` |
|      20 | 4852 | `}` |
|       - | 4853 | `/*` |
|       - | 4854 | ` * bool array_walk(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 4855 | ` *  Apply a user function to every member of an array.` |
|       - | 4856 | ` * Parameters` |
|       - | 4857 | ` *  $array` |
|       - | 4858 | ` *   The input array.` |
|       - | 4859 | ` *  $funcname` |
|       - | 4860 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 4861 | ` *   the first, and the key/index second.` |
|       - | 4862 | ` * Note:` |
|       - | 4863 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 4864 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 4865 | ` *  be made in the original array itself.` |
|       - | 4866 | ` *  $userdata` |
|       - | 4867 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 4868 | ` *   to the callback funcname.` |
|       - | 4869 | ` * Return` |
|       - | 4870 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 4871 | ` */` |
|      38 | 4872 | `PH7_PRIVATE int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4873 | `{` |
|       - | 4874 | `	ph7_value *pValue,*pUserData,sKey;` |
|       - | 4875 | `	ph7_hashmap_node *pEntry;` |
|       - | 4876 | `	ph7_hashmap *pMap;` |
|       - | 4877 | `	sxu32 n;` |
|      43 | 4878 | `	if( nArg < 2 ){` |
|     ! 0 | 4879 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4880 | `			"ArgumentCountError",` |
|       - | 4881 | `			"array_walk() expects at least 2 arguments, %d given",` |
|     ! 0 | 4882 | `			nArg` |
|       - | 4883 | `			);` |
|       - | 4884 | `	}` |
|      43 | 4885 | `	if( nArg > 3 ){` |
|     ! 0 | 4886 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4887 | `			"ArgumentCountError",` |
|       - | 4888 | `			"array_walk() expects at most 3 arguments, %d given",` |
|     ! 0 | 4889 | `			nArg` |
|       - | 4890 | `			);` |
|       - | 4891 | `	}` |
|      43 | 4892 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 4893 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4894 | `			"TypeError",` |
|       - | 4895 | `			"array_walk(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 4896 | `			ph7_type_name(apArg[0])` |
|       - | 4897 | `			);` |
|       - | 4898 | `	}` |
|       - | 4899 | `	{` |
|      41 | 4900 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|      41 | 4901 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 4902 | `	}` |
|      23 | 4903 | `	pUserData = nArg > 2 ? apArg[2] : 0;` |
|       - | 4904 | `	/* Point to the internal representation of the input hashmap */` |
|      23 | 4905 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      23 | 4906 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      23 | 4907 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      23 | 4908 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|       - | 4909 | `	/* Perform the desired operation */` |
|      23 | 4910 | `	pEntry = pMap->pFirst;` |
|      69 | 4911 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 4912 | `		/* Extract the node value */` |
|      49 | 4913 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      49 | 4914 | `		if( pValue ){` |
|       - | 4915 | `			sxi32 rcW;` |
|       - | 4916 | `			/* Extract the entry key */` |
|      49 | 4917 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 4918 | `			/* Invoke the supplied callback */` |
|      49 | 4919 | `			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);` |
|      49 | 4920 | `			PH7_MemObjRelease(&sKey);` |
|      49 | 4921 | `			if( rcW == PH7_EXCEPTION ){` |
|       - | 4922 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|       3 | 4923 | `				return PH7_EXCEPTION;` |
|       - | 4924 | `			}` |
|      23 | 4925 | `		}` |
|       - | 4926 | `		/* Point to the next entry */` |
|      47 | 4927 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      24 | 4928 | `	}` |
|       - | 4929 | `	/* All done, return TRUE */` |
|      21 | 4930 | `	ph7_result_bool(pCtx,1);` |
|      21 | 4931 | `	return PH7_OK;` |
|      24 | 4932 | `}` |
|       - | 4933 | `/*` |
|       - | 4934 | ` * Apply a user function to every member of an array.(Recurse on array's).` |
|       - | 4935 | ` * Refer to the [array_walk_recursive()] implementation for more information.` |
|       - | 4936 | ` */` |
|      22 | 4937 | `static sxi32 HashmapWalkRecursive(` |
|       - | 4938 | `	ph7_hashmap *pMap,    /* Target hashmap */` |
|       - | 4939 | `	ph7_value *pCallback, /* User callback */` |
|       - | 4940 | `	ph7_value *pUserData, /* Callback private data */` |
|       - | 4941 | `	int iNest             /* Nesting level */` |
|       - | 4942 | `	)` |
|       1 | 4943 | `{` |
|       - | 4944 | `	ph7_hashmap_node *pEntry;` |
|       - | 4945 | `	ph7_value *pValue,sKey;` |
|       - | 4946 | `	sxi32 rc;` |
|       - | 4947 | `	sxu32 n;` |
|       - | 4948 | `	/* Iterate through hashmap entries */` |
|      23 | 4949 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      23 | 4950 | `	sKey.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      23 | 4951 | `	pEntry = pMap->pFirst;` |
|      59 | 4952 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 4953 | `		/* Extract the node value */` |
|      37 | 4954 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      37 | 4955 | `		if( pValue ){` |
|      37 | 4956 | `			if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      11 | 4957 | `				if( iNest < 32 ){` |
|       - | 4958 | `					/* Recurse */` |
|      11 | 4959 | `					iNest++;` |
|      11 | 4960 | `					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);` |
|      11 | 4961 | `					iNest--;` |
|      11 | 4962 | `					if( rc == PH7_EXCEPTION ){` |
|     ! 0 | 4963 | `						return PH7_EXCEPTION;` |
|       - | 4964 | `					}` |
|       5 | 4965 | `				}` |
|       6 | 4966 | `			}else{` |
|       - | 4967 | `				/* Extract the node key */` |
|      27 | 4968 | `				PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       - | 4969 | `				/* Invoke the supplied callback */` |
|      27 | 4970 | `				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);` |
|      27 | 4971 | `				PH7_MemObjRelease(&sKey);` |
|      27 | 4972 | `				if( rc == PH7_EXCEPTION ){` |
|       - | 4973 | `					/* The callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 4974 | `					return PH7_EXCEPTION;` |
|       - | 4975 | `				}` |
|       - | 4976 | `			}` |
|      18 | 4977 | `		}` |
|       - | 4978 | `		/* Point to the next entry */` |
|      37 | 4979 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      19 | 4980 | `	}` |
|      23 | 4981 | `	return PH7_OK;` |
|      12 | 4982 | `}` |
|       - | 4983 | `/*` |
|       - | 4984 | ` * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])` |
|       - | 4985 | ` *  Apply a user function recursively to every member of an array.` |
|       - | 4986 | ` * Parameters` |
|       - | 4987 | ` *  $array` |
|       - | 4988 | ` *   The input array.` |
|       - | 4989 | ` *  $funcname` |
|       - | 4990 | ` *   Typically, funcname takes on two parameters. The array parameter's value being` |
|       - | 4991 | ` *   the first, and the key/index second.` |
|       - | 4992 | ` * Note:` |
|       - | 4993 | ` *  If funcname needs to be working with the actual values of the array, specify the first` |
|       - | 4994 | ` *  parameter of funcname as a reference. Then, any changes made to those elements will` |
|       - | 4995 | ` *  be made in the original array itself.` |
|       - | 4996 | ` *  $userdata` |
|       - | 4997 | ` *   If the optional userdata parameter is supplied, it will be passed as the third parameter` |
|       - | 4998 | ` *   to the callback funcname.` |
|       - | 4999 | ` * Return` |
|       - | 5000 | ` *  Returns TRUE on success or FALSE on failure.` |
|       - | 5001 | ` */` |
|      24 | 5002 | `PH7_PRIVATE int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5003 | `{` |
|       - | 5004 | `	ph7_hashmap *pMap;` |
|      29 | 5005 | `	if( nArg < 2 ){` |
|     ! 0 | 5006 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5007 | `			"ArgumentCountError",` |
|       - | 5008 | `			"array_walk_recursive() expects at least 2 arguments, %d given",` |
|     ! 0 | 5009 | `			nArg` |
|       - | 5010 | `			);` |
|       - | 5011 | `	}` |
|      29 | 5012 | `	if( nArg > 3 ){` |
|     ! 0 | 5013 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5014 | `			"ArgumentCountError",` |
|       - | 5015 | `			"array_walk_recursive() expects at most 3 arguments, %d given",` |
|     ! 0 | 5016 | `			nArg` |
|       - | 5017 | `			);` |
|       - | 5018 | `	}` |
|      29 | 5019 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5020 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5021 | `			"TypeError",` |
|       - | 5022 | `			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5023 | `			ph7_type_name(apArg[0])` |
|       - | 5024 | `			);` |
|       - | 5025 | `	}` |
|       - | 5026 | `	{` |
|      27 | 5027 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|      27 | 5028 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 5029 | `	}` |
|       - | 5030 | `	/* Point to the internal representation of the input hashmap */` |
|      13 | 5031 | `	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);` |
|      13 | 5032 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       - | 5033 | `	/* Perform the desired operation */` |
|      13 | 5034 | `	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){` |
|       - | 5035 | `		/* A callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 5036 | `		return PH7_EXCEPTION;` |
|       - | 5037 | `	}` |
|       - | 5038 | `	/* All done, return TRUE */` |
|      13 | 5039 | `	ph7_result_bool(pCtx,1);` |
|      13 | 5040 | `	return PH7_OK;` |
|      17 | 5041 | `}` |
|       - | 5042 | `/*` |
|       - | 5043 | ` * bool array_is_list(array $array)` |
|       - | 5044 | ` *  Checks whether a given array is a list: its keys consist of consecutive` |
|       - | 5045 | ` *  integers starting at 0. An empty array is a list.` |
|       - | 5046 | ` * Return` |
|       - | 5047 | ` *  TRUE if the array is a list, FALSE otherwise.` |
|       - | 5048 | ` */` |
|       - | 5049 | `/*` |
|       - | 5050 | ` * Return TRUE if the given hashmap is a "list" [i.e: its keys are the` |
|       - | 5051 | ` * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.` |
|       - | 5052 | ` * Shared by array_is_list() and the JSON encoder (vm_json.c).` |
|       - | 5053 | ` */` |
|     702 | 5054 | `PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)` |
|       5 | 5055 | `{` |
|     707 | 5056 | `	ph7_hashmap_node *pNode = pMap->pFirst;` |
|     707 | 5057 | `	sxi64 iExpect = 0;` |
|       - | 5058 | `	sxu32 n;` |
|    1765 | 5059 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|    1287 | 5060 | `		if( pNode->iType != HASHMAP_INT_NODE \|\| pNode->xKey.iKey != iExpect ){` |
|       - | 5061 | `			/* A non-integer key or a gap in the sequence: not a list */` |
|     227 | 5062 | `			return 0;` |
|       - | 5063 | `		}` |
|    1063 | 5064 | `		++iExpect;` |
|    1063 | 5065 | `		pNode = pNode->pPrev; /* Reverse link */` |
|     534 | 5066 | `	}` |
|     483 | 5067 | `	return 1;` |
|     356 | 5068 | `}` |
|      12 | 5069 | `PH7_PRIVATE int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5070 | `{` |
|      13 | 5071 | `	if( nArg < 1 ){` |
|     ! 0 | 5072 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5073 | `			"ArgumentCountError",` |
|       - | 5074 | `			"array_is_list() expects exactly 1 argument, 0 given"` |
|       - | 5075 | `			);` |
|       - | 5076 | `	}` |
|      13 | 5077 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5078 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5079 | `			"TypeError",` |
|       - | 5080 | `			"array_is_list(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5081 | `			ph7_type_name(apArg[0])` |
|       - | 5082 | `			);` |
|       - | 5083 | `	}` |
|      13 | 5084 | `	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));` |
|      13 | 5085 | `	return PH7_OK;` |
|       7 | 5086 | `}` |
|       - | 5087 | `/*` |
|       - | 5088 | ` * mixed array_first(array $array)` |
|       - | 5089 | ` * mixed array_last(array $array)` |
|       - | 5090 | ` *  Return the value of the first (respectively last) element of the array,` |
|       - | 5091 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|       - | 5092 | ` *  untouched (unlike reset()/end()).` |
|       - | 5093 | ` */` |
|      18 | 5094 | `static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|       1 | 5095 | `{` |
|       - | 5096 | `	ph7_hashmap *pMap;` |
|       - | 5097 | `	ph7_hashmap_node *pNode;` |
|       - | 5098 | `	ph7_value *pVal;` |
|      19 | 5099 | `	const char *zName = bLast ? "array_last" : "array_first";` |
|      19 | 5100 | `	if( nArg < 1 ){` |
|     ! 0 | 5101 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5102 | `			"ArgumentCountError",` |
|       - | 5103 | `			"%s() expects exactly 1 argument, 0 given",` |
|     ! 0 | 5104 | `			zName` |
|       - | 5105 | `			);` |
|       - | 5106 | `	}` |
|      19 | 5107 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5108 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5109 | `			"TypeError",` |
|       - | 5110 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5111 | `			zName,` |
|       1 | 5112 | `			ph7_type_name(apArg[0])` |
|       - | 5113 | `			);` |
|       - | 5114 | `	}` |
|      17 | 5115 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      17 | 5116 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|      17 | 5117 | `	if( pNode == 0 ){` |
|       - | 5118 | `		/* Empty array: PHP returns NULL */` |
|       5 | 5119 | `		ph7_result_null(pCtx);` |
|       5 | 5120 | `		return PH7_OK;` |
|       - | 5121 | `	}` |
|      13 | 5122 | `	pVal = HashmapExtractNodeValue(pNode);` |
|      13 | 5123 | `	if( pVal ){` |
|      13 | 5124 | `		ph7_result_value(pCtx,pVal);` |
|       7 | 5125 | `	}else{` |
|     ! 0 | 5126 | `		ph7_result_null(pCtx);` |
|       - | 5127 | `	}` |
|      13 | 5128 | `	return PH7_OK;` |
|      10 | 5129 | `}` |
|       8 | 5130 | `PH7_PRIVATE int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5131 | `{` |
|       9 | 5132 | `	return HashmapFirstLast(pCtx,nArg,apArg,0);` |
|       1 | 5133 | `}` |
|      10 | 5134 | `PH7_PRIVATE int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5135 | `{` |
|      11 | 5136 | `	return HashmapFirstLast(pCtx,nArg,apArg,1);` |
|       1 | 5137 | `}` |
|       - | 5138 | `/*` |
|       - | 5139 | ` * int\|string\|null array_key_first(array $array)` |
|       - | 5140 | ` * int\|string\|null array_key_last(array $array)` |
|       - | 5141 | ` *  Return the key of the first (respectively last) element of the array,` |
|       - | 5142 | ` *  or NULL when the array is empty. The internal array pointer is left` |
|       - | 5143 | ` *  untouched.` |
|       - | 5144 | ` */` |
|      22 | 5145 | `static int HashmapKeyFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)` |
|       1 | 5146 | `{` |
|       - | 5147 | `	ph7_hashmap *pMap;` |
|       - | 5148 | `	ph7_hashmap_node *pNode;` |
|      23 | 5149 | `	const char *zName = bLast ? "array_key_last" : "array_key_first";` |
|      23 | 5150 | `	if( nArg < 1 ){` |
|     ! 0 | 5151 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5152 | `			"ArgumentCountError",` |
|       - | 5153 | `			"%s() expects exactly 1 argument, 0 given",` |
|     ! 0 | 5154 | `			zName` |
|       - | 5155 | `			);` |
|       - | 5156 | `	}` |
|      23 | 5157 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|       4 | 5158 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5159 | `			"TypeError",` |
|       - | 5160 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|       1 | 5161 | `			zName,` |
|       1 | 5162 | `			ph7_type_name(apArg[0])` |
|       - | 5163 | `			);` |
|       - | 5164 | `	}` |
|      21 | 5165 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      21 | 5166 | `	pNode = bLast ? pMap->pLast : pMap->pFirst;` |
|      21 | 5167 | `	if( pNode == 0 ){` |
|       - | 5168 | `		/* Empty array: PHP returns NULL */` |
|       5 | 5169 | `		ph7_result_null(pCtx);` |
|       5 | 5170 | `		return PH7_OK;` |
|       - | 5171 | `	}` |
|      17 | 5172 | `	HashmapResultNodeKey(pCtx,pNode);` |
|      17 | 5173 | `	return PH7_OK;` |
|      12 | 5174 | `}` |
|      10 | 5175 | `PH7_PRIVATE int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5176 | `{` |
|      11 | 5177 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,0);` |
|       1 | 5178 | `}` |
|      12 | 5179 | `PH7_PRIVATE int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5180 | `{` |
|      13 | 5181 | `	return HashmapKeyFirstLast(pCtx,nArg,apArg,1);` |
|       1 | 5182 | `}` |
|       - | 5183 | `/*` |
|       - | 5184 | ` * Fetch the element identified by 'pKey' from 'pRow' which may be either an` |
|       - | 5185 | ` * array (hashmap lookup) or an object (public attribute lookup). Used by` |
|       - | 5186 | ` * array_column() for both the column value and the index key.` |
|       - | 5187 | ` * Returns a borrowed pointer to the value, or NULL when the row is not a` |
|       - | 5188 | ` * container or the key is absent.` |
|       - | 5189 | ` */` |
|      32 | 5190 | `static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)` |
|       1 | 5191 | `{` |
|      33 | 5192 | `	if( ph7_value_is_array(pRow) ){` |
|       - | 5193 | `		ph7_hashmap_node *pNode;` |
|      25 | 5194 | `		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){` |
|      21 | 5195 | `			return HashmapExtractNodeValue(pNode);` |
|       1 | 5196 | `		}` |
|      11 | 5197 | `	}else if( ph7_value_is_object(pRow) ){` |
|       - | 5198 | `		ph7_value sName;` |
|       - | 5199 | `		const char *zName;` |
|       - | 5200 | `		ph7_value *pAttr;` |
|       - | 5201 | `		/* Stringify a *copy* of the key (objects address attributes by name);` |
|       - | 5202 | `		 * never mutate pKey itself or the array-lookup path would break. */` |
|       9 | 5203 | `		PH7_MemObjInit(pVm,&sName);` |
|       9 | 5204 | `		PH7_MemObjStore(pKey,&sName);` |
|       9 | 5205 | `		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */` |
|       9 | 5206 | `		pAttr = ph7_object_fetch_attr(pRow,zName);` |
|       9 | 5207 | `		PH7_MemObjRelease(&sName);` |
|       9 | 5208 | `		return pAttr;` |
|       - | 5209 | `	}` |
|       5 | 5210 | `	return 0;` |
|      17 | 5211 | `}` |
|       - | 5212 | `/*` |
|       - | 5213 | ` * array array_column(array $array, int\|string\|null $column_key, int\|string\|null $index_key = null)` |
|       - | 5214 | ` *  Returns the values from a single column of the input, identified by` |
|       - | 5215 | ` *  $column_key. Optionally indexes the result by the $index_key column.` |
|       - | 5216 | ` *  A NULL $column_key collects the whole row. Rows missing the column are` |
|       - | 5217 | ` *  skipped; rows missing the index key are appended with a numeric key.` |
|       - | 5218 | ` *  Each row may be an array or an object.` |
|       - | 5219 | ` */` |
|      12 | 5220 | `PH7_PRIVATE int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5221 | `{` |
|       - | 5222 | `	ph7_hashmap_node *pNode;` |
|       - | 5223 | `	ph7_hashmap *pMap;` |
|       - | 5224 | `	ph7_value *pArray;` |
|       - | 5225 | `	ph7_value *pRow;` |
|       - | 5226 | `	ph7_value *pCol;` |
|       - | 5227 | `	ph7_value *pIdx;` |
|       - | 5228 | `	int bWantCol;` |
|       - | 5229 | `	int bWantIdx;` |
|       - | 5230 | `	sxu32 n;` |
|      13 | 5231 | `	if( nArg < 2 ){` |
|     ! 0 | 5232 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5233 | `			"ArgumentCountError",` |
|       - | 5234 | `			"array_column() expects at least 2 arguments, %d given",` |
|     ! 0 | 5235 | `			nArg` |
|       - | 5236 | `			);` |
|       - | 5237 | `	}` |
|      13 | 5238 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5239 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5240 | `			"TypeError",` |
|       - | 5241 | `			"array_column(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5242 | `			ph7_type_name(apArg[0])` |
|       - | 5243 | `			);` |
|       - | 5244 | `	}` |
|      13 | 5245 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      13 | 5246 | `	pArray = ph7_context_new_array(pCtx);` |
|      13 | 5247 | `	if( pArray == 0 ){` |
|     ! 0 | 5248 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5249 | `		return PH7_OK;` |
|       - | 5250 | `	}` |
|       - | 5251 | `	/* A NULL column_key means "collect the entire row". */` |
|      13 | 5252 | `	bWantCol = !ph7_value_is_null(apArg[1]);` |
|      13 | 5253 | `	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));` |
|      13 | 5254 | `	pNode = pMap->pFirst;` |
|      33 | 5255 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      21 | 5256 | `		pRow = HashmapExtractNodeValue(pNode);` |
|      21 | 5257 | `		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */` |
|      21 | 5258 | `		if( pRow == 0 ){` |
|     ! 0 | 5259 | `			continue;` |
|       - | 5260 | `		}` |
|      21 | 5261 | `		if( bWantCol ){` |
|      19 | 5262 | `			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);` |
|      19 | 5263 | `			if( pCol == 0 ){` |
|       - | 5264 | `				/* Row lacks the requested column: skip it (PHP semantics). */` |
|       3 | 5265 | `				continue;` |
|       - | 5266 | `			}` |
|       9 | 5267 | `		}else{` |
|       3 | 5268 | `			pCol = pRow;` |
|       - | 5269 | `		}` |
|      19 | 5270 | `		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;` |
|      19 | 5271 | `		if( pIdx ){` |
|      13 | 5272 | `			ph7_array_add_elem(pArray,pIdx,pCol);` |
|       7 | 5273 | `		}else{` |
|       7 | 5274 | `			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */` |
|       - | 5275 | `		}` |
|      10 | 5276 | `	}` |
|      13 | 5277 | `	ph7_result_value(pCtx,pArray);` |
|      13 | 5278 | `	return PH7_OK;` |
|       7 | 5279 | `}` |
|       - | 5280 | `/*` |
|       - | 5281 | ` * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).` |
|       - | 5282 | ` * Invokes $callback($value, $key) over each entry and reports the first node` |
|       - | 5283 | ` * whose truthiness equals 'bWant'. Propagates a callback exception as` |
|       - | 5284 | ` * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).` |
|       - | 5285 | ` */` |
|      30 | 5286 | `static sxi32 HashmapCallbackSearch(` |
|       - | 5287 | `	ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|       - | 5288 | `	const char *zName,            /* Function name for diagnostics */` |
|       - | 5289 | `	int bWant,                    /* Truthiness being hunted for */` |
|       - | 5290 | `	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */` |
|       - | 5291 | `	)` |
|       1 | 5292 | `{` |
|       - | 5293 | `	ph7_hashmap_node *pEntry;` |
|       - | 5294 | `	ph7_hashmap *pMap;` |
|       - | 5295 | `	ph7_value *pValue;` |
|       - | 5296 | `	ph7_value *apCbArg[2];` |
|       - | 5297 | `	ph7_value sKey;` |
|       - | 5298 | `	ph7_value sResult;` |
|       - | 5299 | `	sxi32 rc;` |
|       - | 5300 | `	sxu32 n;` |
|      31 | 5301 | `	*ppMatch = 0;` |
|      31 | 5302 | `	if( nArg < 2 ){` |
|     ! 0 | 5303 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5304 | `			"ArgumentCountError",` |
|       - | 5305 | `			"%s() expects exactly 2 arguments, %d given",` |
|     ! 0 | 5306 | `			zName,nArg` |
|       - | 5307 | `			);` |
|       - | 5308 | `	}` |
|      31 | 5309 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     ! 0 | 5310 | `		return PH7_VmThrowException(pCtx,` |
|       - | 5311 | `			"TypeError",` |
|       - | 5312 | `			"%s(): Argument #1 ($array) must be of type array, %s given",` |
|     ! 0 | 5313 | `			zName,ph7_type_name(apArg[0])` |
|       - | 5314 | `			);` |
|       - | 5315 | `	}` |
|       - | 5316 | `	{` |
|      31 | 5317 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|      31 | 5318 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 5319 | `	}` |
|      29 | 5320 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      29 | 5321 | `	pEntry = pMap->pFirst;` |
|      29 | 5322 | `	PH7_MemObjInit(pMap->pVm,&sKey);` |
|      29 | 5323 | `	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */` |
|      29 | 5324 | `	PH7_MemObjInit(pMap->pVm,&sResult);` |
|      29 | 5325 | `	sResult.nIdx = SXU32_HIGH; /* Mark as constant */` |
|      73 | 5326 | `	for( n = 0 ; n < pMap->nEntry ; ++n ){` |
|      59 | 5327 | `		pValue = HashmapExtractNodeValue(pEntry);` |
|      59 | 5328 | `		if( pValue ){` |
|       - | 5329 | `			/* The callback receives ($value, $key). */` |
|      59 | 5330 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|      59 | 5331 | `			apCbArg[0] = pValue;` |
|      59 | 5332 | `			apCbArg[1] = &sKey;` |
|      59 | 5333 | `			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);` |
|      59 | 5334 | `			if( rc == PH7_EXCEPTION ){` |
|       - | 5335 | `				/* The callback raised: propagate so the dispatcher unwinds. */` |
|     ! 0 | 5336 | `				PH7_MemObjRelease(&sKey);` |
|     ! 0 | 5337 | `				PH7_MemObjRelease(&sResult);` |
|     ! 0 | 5338 | `				return PH7_EXCEPTION;` |
|       - | 5339 | `			}` |
|      59 | 5340 | `			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){` |
|      15 | 5341 | `				*ppMatch = pEntry;` |
|      15 | 5342 | `				break;` |
|       - | 5343 | `			}` |
|      22 | 5344 | `		}` |
|      45 | 5345 | `		pEntry = pEntry->pPrev; /* Reverse link */` |
|      23 | 5346 | `	}` |
|      29 | 5347 | `	PH7_MemObjRelease(&sKey);` |
|      29 | 5348 | `	PH7_MemObjRelease(&sResult);` |
|      29 | 5349 | `	return PH7_OK;` |
|      16 | 5350 | `}` |
|       - | 5351 | `/*` |
|       - | 5352 | ` * mixed array_find(array $array, callable $callback)` |
|       - | 5353 | ` *  Returns the value of the first element for which $callback($value,$key)` |
|       - | 5354 | ` *  is truthy, or NULL if none match.` |
|       - | 5355 | ` */` |
|       8 | 5356 | `PH7_PRIVATE int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5357 | `{` |
|       - | 5358 | `	ph7_hashmap_node *pMatch;` |
|       - | 5359 | `	ph7_value *pVal;` |
|       - | 5360 | `	sxi32 rc;` |
|       9 | 5361 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);` |
|       9 | 5362 | `	if( rc != PH7_OK ){` |
|       3 | 5363 | `		return rc;` |
|       - | 5364 | `	}` |
|       7 | 5365 | `	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){` |
|       5 | 5366 | `		ph7_result_value(pCtx,pVal);` |
|       3 | 5367 | `	}else{` |
|       3 | 5368 | `		ph7_result_null(pCtx);` |
|       - | 5369 | `	}` |
|       7 | 5370 | `	return PH7_OK;` |
|       5 | 5371 | `}` |
|       - | 5372 | `/*` |
|       - | 5373 | ` * mixed array_find_key(array $array, callable $callback)` |
|       - | 5374 | ` *  Returns the key of the first element for which $callback($value,$key)` |
|       - | 5375 | ` *  is truthy, or NULL if none match.` |
|       - | 5376 | ` */` |
|       6 | 5377 | `PH7_PRIVATE int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5378 | `{` |
|       - | 5379 | `	ph7_hashmap_node *pMatch;` |
|       - | 5380 | `	sxi32 rc;` |
|       7 | 5381 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);` |
|       7 | 5382 | `	if( rc != PH7_OK ){` |
|     ! 0 | 5383 | `		return rc;` |
|       - | 5384 | `	}` |
|       7 | 5385 | `	if( pMatch == 0 ){` |
|       3 | 5386 | `		ph7_result_null(pCtx);` |
|       6 | 5387 | `	}else if( pMatch->iType == HASHMAP_INT_NODE ){` |
|       3 | 5388 | `		ph7_result_int64(pCtx,pMatch->xKey.iKey);` |
|       2 | 5389 | `	}else{` |
|       4 | 5390 | `		ph7_result_string(pCtx,` |
|       2 | 5391 | `			(const char *)SyBlobData(&pMatch->xKey.sKey),` |
|       2 | 5392 | `			(int)SyBlobLength(&pMatch->xKey.sKey));` |
|       - | 5393 | `	}` |
|       7 | 5394 | `	return PH7_OK;` |
|       4 | 5395 | `}` |
|       - | 5396 | `/*` |
|       - | 5397 | ` * bool array_any(array $array, callable $callback)` |
|       - | 5398 | ` *  Returns TRUE if $callback($value,$key) is truthy for at least one element.` |
|       - | 5399 | ` *  FALSE for an empty array.` |
|       - | 5400 | ` */` |
|       8 | 5401 | `PH7_PRIVATE int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5402 | `{` |
|       - | 5403 | `	ph7_hashmap_node *pMatch;` |
|       - | 5404 | `	sxi32 rc;` |
|       9 | 5405 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);` |
|       9 | 5406 | `	if( rc != PH7_OK ){` |
|     ! 0 | 5407 | `		return rc;` |
|       - | 5408 | `	}` |
|       9 | 5409 | `	ph7_result_bool(pCtx,pMatch != 0);` |
|       9 | 5410 | `	return PH7_OK;` |
|       5 | 5411 | `}` |
|       - | 5412 | `/*` |
|       - | 5413 | ` * bool array_all(array $array, callable $callback)` |
|       - | 5414 | ` *  Returns TRUE if $callback($value,$key) is truthy for every element (and for` |
|       - | 5415 | ` *  an empty array). Hunts for the first falsy element: its absence means "all".` |
|       - | 5416 | ` */` |
|       8 | 5417 | `PH7_PRIVATE int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5418 | `{` |
|       - | 5419 | `	ph7_hashmap_node *pMatch;` |
|       - | 5420 | `	sxi32 rc;` |
|       9 | 5421 | `	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);` |
|       9 | 5422 | `	if( rc != PH7_OK ){` |
|     ! 0 | 5423 | `		return rc;` |
|       - | 5424 | `	}` |
|       9 | 5425 | `	ph7_result_bool(pCtx,pMatch == 0);` |
|       9 | 5426 | `	return PH7_OK;` |
|       5 | 5427 | `}` |
|       - | 5428 | `/*` |
|       - | 5429 | ` * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk` |
|       - | 5430 | ` * helper (the reusable form of the foreach Iterator protocol).` |
|       - | 5431 | ` */` |
|       - | 5432 | `/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */` |
|       - | 5433 | `struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };` |
|      80 | 5434 | `static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       4 | 5435 | `{` |
|      84 | 5436 | `	struct IterCollect *p = (struct IterCollect *)pUserData;` |
|      40 | 5437 | `	(void)pVm;` |
|      84 | 5438 | `	p->nCount++;` |
|      84 | 5439 | `	if( p->pArray ){` |
|       - | 5440 | `		/* preserve_keys: insert with the iterator key (later wins on collision);` |
|       - | 5441 | `		 * otherwise append with an auto-assigned int index. */` |
|      70 | 5442 | `		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);` |
|      33 | 5443 | `	}` |
|      84 | 5444 | `	return SXRET_OK;` |
|       4 | 5445 | `}` |
|       - | 5446 | `/*` |
|       - | 5447 | ` * array iterator_to_array(Traversable\|array $iterator, bool $preserve_keys = true)` |
|       - | 5448 | ` */` |
|      30 | 5449 | `PH7_PRIVATE int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       4 | 5450 | `{` |
|       - | 5451 | `	struct IterCollect sCol;` |
|       - | 5452 | `	ph7_value *pArray;` |
|       - | 5453 | `	sxi32 rc;` |
|      34 | 5454 | `	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      34 | 5455 | `	pArray = ph7_context_new_array(pCtx);` |
|      34 | 5456 | `	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      34 | 5457 | `	sCol.pArray = pArray;` |
|      34 | 5458 | `	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;` |
|      34 | 5459 | `	sCol.nCount = 0;` |
|      34 | 5460 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       - | 5461 | `		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */` |
|       3 | 5462 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|       3 | 5463 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 5464 | `		sxu32 n;` |
|       9 | 5465 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       - | 5466 | `			ph7_value sKey, *pVal;` |
|       7 | 5467 | `			PH7_MemObjInit(pCtx->pVm,&sKey);` |
|       7 | 5468 | `			PH7_HashmapExtractNodeKey(pEntry,&sKey);` |
|       7 | 5469 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);` |
|       7 | 5470 | `			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }` |
|       7 | 5471 | `			PH7_MemObjRelease(&sKey);` |
|       7 | 5472 | `			pEntry = pEntry->pPrev;` |
|       4 | 5473 | `		}` |
|       3 | 5474 | `		ph7_result_value(pCtx,pArray);` |
|       3 | 5475 | `		return PH7_OK;` |
|       - | 5476 | `	}` |
|      32 | 5477 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|      32 | 5478 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|      30 | 5479 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 5480 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 5481 | `			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|     ! 0 | 5482 | `			ph7_type_name(apArg[0]));` |
|       - | 5483 | `	}` |
|      30 | 5484 | `	ph7_result_value(pCtx,pArray);` |
|      30 | 5485 | `	return PH7_OK;` |
|      19 | 5486 | `}` |
|       - | 5487 | `/*` |
|       - | 5488 | ` * int iterator_count(Traversable\|array $iterator)` |
|       - | 5489 | ` */` |
|       8 | 5490 | `PH7_PRIVATE int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 5491 | `{` |
|       - | 5492 | `	struct IterCollect sCol;` |
|       - | 5493 | `	sxi32 rc;` |
|       9 | 5494 | `	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|       9 | 5495 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       3 | 5496 | `		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);` |
|       3 | 5497 | `		return PH7_OK;` |
|       - | 5498 | `	}` |
|       7 | 5499 | `	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;` |
|       7 | 5500 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);` |
|       7 | 5501 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|       7 | 5502 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 5503 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 5504 | `			"iterator_count(): Argument #1 ($iterator) must be of type Traversable\|array, %s given",` |
|     ! 0 | 5505 | `			ph7_type_name(apArg[0]));` |
|       - | 5506 | `	}` |
|       7 | 5507 | `	ph7_result_int64(pCtx, sCol.nCount);` |
|       7 | 5508 | `	return PH7_OK;` |
|       5 | 5509 | `}` |
|       - | 5510 | `/* iterator_apply step: call the fixed callback with $args each iteration. The` |
|       - | 5511 | ` * arg pointers are resolved fresh per step because the iterator's own methods` |
|       - | 5512 | ` * run user code between iterations and may reallocate the aMemObj pool. */` |
|       - | 5513 | `struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };` |
|      32 | 5514 | `static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)` |
|       1 | 5515 | `{` |
|      33 | 5516 | `	struct IterApply *p = (struct IterApply *)pUserData;` |
|       - | 5517 | `	ph7_value sResult;` |
|       - | 5518 | `	SySet aArg;` |
|       - | 5519 | `	sxi32 rc;` |
|       - | 5520 | `	int bContinue;` |
|      16 | 5521 | `	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */` |
|      33 | 5522 | `	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|      33 | 5523 | `	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){` |
|       9 | 5524 | `		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;` |
|       9 | 5525 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|       - | 5526 | `		sxu32 n;` |
|      17 | 5527 | `		for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|       9 | 5528 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);` |
|       9 | 5529 | `			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }` |
|       9 | 5530 | `			pEntry = pEntry->pPrev;` |
|       5 | 5531 | `		}` |
|       4 | 5532 | `	}` |
|      33 | 5533 | `	PH7_MemObjInit(pVm,&sResult);` |
|      49 | 5534 | `	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),` |
|      32 | 5535 | `		(ph7_value **)SySetBasePtr(&aArg), &sResult);` |
|      33 | 5536 | `	SySetRelease(&aArg);` |
|      33 | 5537 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }` |
|      31 | 5538 | `	p->nCount++;` |
|      31 | 5539 | `	PH7_MemObjToBool(&sResult);` |
|      31 | 5540 | `	bContinue = (sResult.x.iVal != 0);` |
|      31 | 5541 | `	PH7_MemObjRelease(&sResult);` |
|      31 | 5542 | `	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */` |
|      17 | 5543 | `}` |
|       - | 5544 | `/*` |
|       - | 5545 | ` * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])` |
|       - | 5546 | ` */` |
|      12 | 5547 | `PH7_PRIVATE int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 5548 | `{` |
|       - | 5549 | `	struct IterApply sApp;` |
|       - | 5550 | `	sxi32 rc;` |
|      13 | 5551 | `	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }` |
|       - | 5552 | `	{` |
|      13 | 5553 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx,apArg[1],2,"callback",FALSE);` |
|      13 | 5554 | `		if( rcCb != PH7_OK ){ return rcCb; }` |
|       - | 5555 | `	}` |
|      13 | 5556 | `	sApp.pCallback = apArg[1];` |
|      13 | 5557 | `	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;` |
|      13 | 5558 | `	sApp.nCount = 0;` |
|      13 | 5559 | `	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);` |
|      13 | 5560 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){ return rc; }` |
|      11 | 5561 | `	if( rc == SXERR_NOTIMPLEMENTED ){` |
|     ! 0 | 5562 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 5563 | `			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",` |
|     ! 0 | 5564 | `			ph7_type_name(apArg[0]));` |
|       - | 5565 | `	}` |
|      11 | 5566 | `	ph7_result_int64(pCtx, sApp.nCount);` |
|      11 | 5567 | `	return PH7_OK;` |
|       7 | 5568 | `}` |
|       - | 5569 |  |
