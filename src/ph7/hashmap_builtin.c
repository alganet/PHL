/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include <stdlib.h>  /* strtod (range() endpoint parsing) */
#include <stdio.h>   /* snprintf (range() float step formatting) */
/*
 * Section:
 *    The array_* builtin function family (everything but the sort family,
 *    which lives in hashmap_sort.c). The core hashmap engine and the
 *    aHashmapFunc[] registration table stay in hashmap.c.
 * Status:
 *    Stable.
 */
/*
 * bool shuffle(array &$array)
 *  shuffles (randomizes the order of the elements in) an array.
 * Parameters
 *  $array
 *   The input array.
 * Return
 *  TRUE on success or FALSE on failure.
 *
 */
PH7_PRIVATE int ph7_hashmap_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	/* Make sure we are dealing with a valid hashmap */
	if( nArg < 1 || !ph7_value_is_array(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry > 1 ){
		/* Do the merge sort */
		HashmapMergeSort(pMap,HashmapCmpCallback7,0);
		/* Fix the last link broken by the merge */
		while(pMap->pLast->pPrev){
			pMap->pLast = pMap->pLast->pPrev;
		}
	}
	/* All done,return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * int count(array $var [, int $mode = COUNT_NORMAL ])
 *   Count all elements in an array, or something in an object.
 * Parameters
 *  $var
 *   The array or the object.
 * $mode
 *  If the optional mode parameter is set to COUNT_RECURSIVE (or 1), count()
 *  will recursively count the array. This is particularly useful for counting
 *  all the elements of a multidimensional array.
 * Return
 *  Returns the number of elements in the array.
 */
PH7_PRIVATE int ph7_hashmap_count(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int bRecursive = FALSE;
	int bCycleDetected = FALSE;
	sxi64 iCount;
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"count() expects at least 1 argument, 0 given"
			);
	}
	if( nArg > 2 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"count() expects at most 2 arguments, %d given",
			nArg
			);
	}
	/* PHP validates $mode right after parsing, before the type dispatch, so
	 * an invalid mode raises ValueError whether $value is an array or a
	 * Countable object (the mode is then ignored for the Countable path). */
	if( nArg > 1 ){
		sxi32 iMode = ph7_value_to_int(apArg[1]);
		if( iMode != 0 /* COUNT_NORMAL */ && iMode != 1 /* COUNT_RECURSIVE */ ){
			return PH7_VmThrowException(pCtx,
				"ValueError",
				"count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE"
				);
		}
		bRecursive = iMode == 1;
	}
	if( !ph7_value_is_array(apArg[0]) ){
		/* Countable object: dispatch to ->count() */
		if( apArg[0]->iFlags & MEMOBJ_OBJ ){
			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;
			ph7_class *pCountable = pCtx->pVm->pCountableClass;
			if( pCountable && PH7_VmInstanceOf(pInst->pClass,pCountable) ){
				ph7_class_method *pMeth = PH7_ClassExtractMethod(pInst->pClass,
					"count",sizeof("count")-1);
				if( pMeth ){
					ph7_value sResult;
					PH7_MemObjInit(pCtx->pVm,&sResult);
					PH7_VmCallClassMethod(pCtx->pVm,pInst,pMeth,&sResult,0,0);
					ph7_result_int64(pCtx,ph7_value_to_int64(&sResult));
					PH7_MemObjRelease(&sResult);
					return PH7_OK;
				}
			}
		}
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"count(): Argument #1 ($value) must be of type Countable|array, %s given",
			ph7_type_name(apArg[0])
			);
	}
	/* Count */
	iCount = HashmapCount((ph7_hashmap *)apArg[0]->x.pOther,bRecursive,&bCycleDetected);
	if( bCycleDetected ){
		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,"count(): Recursion detected");
	}
	ph7_result_int64(pCtx,iCount);
	return PH7_OK;
}
/*
 * bool array_key_exists(value $key,array $search)
 *  Checks if the given key or index exists in the array.
 * Parameters
 * $key
 *   Value to check.
 * $search
 *  An array with keys to check.
 * Return
 *  TRUE on success or FALSE on failure.
 */
PH7_PRIVATE int ph7_hashmap_key_exists(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	sxi32 rc;
	if( nArg != 2 ){
		/* PHP requires exactly two arguments */
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_key_exists() expects exactly 2 arguments, %d given",
			nArg
			);
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[1]) ){
		/* Type mismatch -> TypeError */
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_key_exists(): Argument #2 ($array) must be of type array, %s given",
			ph7_type_name(apArg[1])
			);
	}
	/* php only DEPRECATES a null / lossy-float key here; PHL rejects it. */
	if( apArg[0]->iFlags & MEMOBJ_NULL ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"array_key_exists(): Argument #1 ($key) must be of type string|int, null given");
	}else if( apArg[0]->iFlags & MEMOBJ_REAL ){
		ph7_real rVal = apArg[0]->rVal;
		if( rVal != (ph7_real)(sxi64)rVal ){
			return PH7_VmThrowException(pCtx,"TypeError",
				"array_key_exists(): Argument #1 ($key) must be of type string|int, float given");
		}
	}
	/* Perform the lookup */
	rc = PH7_HashmapLookup((ph7_hashmap *)apArg[1]->x.pOther,apArg[0],0);
	/* lookup result */
	ph7_result_bool(pCtx,rc == SXRET_OK ? 1 : 0);
	return PH7_OK;
}
/*
 * value array_pop(array $array)
 *   POP the last inserted element from the array.
 * Parameter
 *  The array to get the value from.
 * Return
 *  Poped value or NULL on failure.
 */
PH7_PRIVATE int ph7_hashmap_pop(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	/* PHP requires exactly one argument and it must be passed by reference */
	if( nArg != 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_pop() expects exactly 1 argument, %d given",
			nArg
			);
	}
	/* Passing a constant (including literals) or non-variable triggers the same
	 * error message as official PHP. Check the index to detect constants. */
	if( apArg[0]->nIdx == SXU32_HIGH ){
		return PH7_VmThrowException(pCtx,
			"Error",
			"array_pop(): Argument #1 ($array) could not be passed by reference"
			);
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_pop(): Argument #1 ($array) must be of type array, %s given",
			ph7_type_name(apArg[0])
			);
	}
	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry < 1 ){
		/* Nothing to pop,return NULL */
		ph7_result_null(pCtx);
	}else{
		ph7_hashmap_node *pLast = pMap->pLast;
		ph7_value *pObj;
		pObj = HashmapExtractNodeValue(pLast);
		if( pObj ){
			/* Node value */
			ph7_result_value(pCtx,pObj);
			/* Unlink the node */
			PH7_HashmapUnlinkNode(pLast,TRUE);
		}else{
			ph7_result_null(pCtx);
		}
		/* Reset the cursor */
		pMap->pCur = pMap->pFirst;
	}
	return PH7_OK;
}
/*
 * int array_push($array,$var,...)
 *   Push one or more elements onto the end of array. (Stack insertion)
 * Parameters
 *  array
 *    The input array.
 *  var
 *   On or more value to push.
 * Return
 *  New array count (including old items).
 */
PH7_PRIVATE int ph7_hashmap_push(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	sxi32 rc;
	int i;
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_push() expects at least 1 argument, %d given",
			nArg
			);
	}
	/* Passing a constant (including literals) or non-variable triggers the same
	 * error message as official PHP. Check the index to detect constants. */
	if( apArg[0]->nIdx == SXU32_HIGH ){
		return PH7_VmThrowException(pCtx,
			"Error",
			"array_push(): Argument #1 ($array) could not be passed by reference"
			);
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_push(): Argument #1 ($array) must be of type array, %s given",
			ph7_type_name(apArg[0])
			);
	}
	/* Point to the internal representation of the input hashmap */
	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Start pushing given values */
	for( i = 1 ; i < nArg ; ++i ){
		rc = PH7_HashmapInsert(pMap,0,apArg[i]);
		if( rc != SXRET_OK ){
			if( rc == PH7_EXCEPTION || rc == PH7_ABORT ){
				/* Saturated-append Error (php: array_push throws, no result) */
				return rc;
			}
			break;
		}
	}
	/* Return the new count */
	ph7_result_int64(pCtx,(sxi64)pMap->nEntry);
	return PH7_OK;
}
/*
 * value array_shift(array $array)
 *   Shift an element off the beginning of array.
 * Parameter
 *  The array to get the value from.
 * Return
 *  Shifted value or NULL on failure.
 */
PH7_PRIVATE int ph7_hashmap_shift(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	/* PHP requires exactly one argument and it must be passed by reference */
	if( nArg != 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_shift() expects exactly 1 argument, %d given",
			nArg
			);
	}
	/* Detect constants or literals, which cannot be passed by reference. */
	if( apArg[0]->nIdx == SXU32_HIGH ){
		return PH7_VmThrowException(pCtx,
			"Error",
			"array_shift(): Argument #1 ($array) could not be passed by reference"
			);
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_shift(): Argument #1 ($array) must be of type array, %s given",
			ph7_type_name(apArg[0])
			);
	}
	/* Point to the internal representation of the hashmap */
	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry < 1 ){
		/* Empty hashmap,return NULL */
		ph7_result_null(pCtx);
	}else{
		ph7_hashmap_node *pEntry = pMap->pFirst;
		ph7_value *pObj;
		sxu32 n;
		pObj = HashmapExtractNodeValue(pEntry);
		if( pObj ){
			/* Node value */
			ph7_result_value(pCtx,pObj);
			/* Unlink the first node */
			PH7_HashmapUnlinkNode(pEntry,TRUE);
		}else{
			ph7_result_null(pCtx);
		}
		/* Rehash all int keys */
		n = pMap->nEntry;
		pEntry = pMap->pFirst;
		pMap->iNextIdx = 0;
	pMap->bIntKeySeen = 0; /* Reset the automatic index */
		for(;;){
			if( n < 1 ){
				break;
			}
			if( pEntry->iType == HASHMAP_INT_NODE ){
				HashmapRehashIntNode(pEntry);
			}
			/* Point to the next entry */
			pEntry = pEntry->pPrev; /* Reverse link */
			n--;
		}
		/* Reset the cursor */
		pMap->pCur = pMap->pFirst;
	}
	return PH7_OK;
}
/*
 * Extract the node cursor value.
 */
static sxi32 HashmapCurrentValue(ph7_context *pCtx,ph7_hashmap *pMap,int iDirection)
{
	ph7_hashmap_node *pCur = pMap->pCur;
	ph7_value *pVal;
	if( pCur == 0 ){
		/* Cursor does not point to anything,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( iDirection != 0 ){
		if( iDirection > 0 ){
			/* Point to the next entry */
			pMap->pCur = pCur->pPrev; /* Reverse link */
			pCur = pMap->pCur;
		}else{
			/* Point to the previous entry */
			pMap->pCur = pCur->pNext; /* Reverse link */
			pCur = pMap->pCur;
		}
		if( pCur == 0 ){
			/* End of input reached,return FALSE */
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
	}
	/* Point to the desired element */
	pVal = HashmapExtractNodeValue(pCur);
	if( pVal ){
		ph7_result_value(pCtx,pVal);
	}else{
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * value current(array $array)
 *  Return the current element in an array.
 * Parameter
 *  $input: The input array.
 * Return
 *  The current() function simply returns the value of the array element that's currently
 *  being pointed to by the internal pointer. It does not move the pointer in any way.
 *  If the internal pointer points beyond the end of the elements list or the array
 *  is empty, current() returns FALSE.
 */
PH7_PRIVATE int ph7_hashmap_current(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		/* Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,0);
	return PH7_OK;
}
/*
 * value next(array $input)
 *  Advance the internal array pointer of an array.
 * Parameter
 *  $input: The input array.
 * Return
 *  next() behaves like current(), with one difference. It advances the internal array
 *  pointer one place forward before returning the element value. That means it returns
 *  the next array value and advances the internal array pointer by one.
 */
PH7_PRIVATE int ph7_hashmap_next(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		/* Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,1);
	return PH7_OK;
}
/*
 * value prev(array $input)
 *  Rewind the internal array pointer.
 * Parameter
 *  $input: The input array.
 * Return
 *  Returns the array value in the previous place that's pointed
 *  to by the internal array pointer, or FALSE if there are no more
 *  elements.
 */
PH7_PRIVATE int ph7_hashmap_prev(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		/* Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	HashmapCurrentValue(&(*pCtx),(ph7_hashmap *)apArg[0]->x.pOther,-1);
	return PH7_OK;
}
/*
 * value end(array $input)
 *  Set the internal pointer of an array to its last element.
 * Parameter
 *  $input: The input array.
 * Return
 *  Returns the value of the last element or FALSE for empty array.
 */
PH7_PRIVATE int ph7_hashmap_end(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		/* Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Point to the last node */
	pMap->pCur = pMap->pLast;
	/* Return the last node value */
	HashmapCurrentValue(&(*pCtx),pMap,0);
	return PH7_OK;
}
/*
 * value reset(array $array )
 *  Set the internal pointer of an array to its first element.
 * Parameter
 *  $input: The input array.
 * Return
 *  Returns the value of the first array element,or FALSE if the array is empty.
 */
PH7_PRIVATE int ph7_hashmap_reset(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		/* Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Point to the first node */
	pMap->pCur = pMap->pFirst;
	/* Return the last node value if available */
	HashmapCurrentValue(&(*pCtx),pMap,0);
	return PH7_OK;
}
/*
 * Emit a node's key (integer or blob) as the call result — shared by key(),
 * array_key_first() and array_key_last().
 */
static void HashmapResultNodeKey(ph7_context *pCtx,ph7_hashmap_node *pNode)
{
	if( pNode->iType == HASHMAP_INT_NODE ){
		/* Key is integer */
		ph7_result_int64(pCtx,pNode->xKey.iKey);
	}else{
		/* Key is blob */
		ph7_result_string(pCtx,
			(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));
	}
}
/*
 * value key(array $array)
 *   Fetch a key from an array
 * Parameter
 *  $input
 *   The input array.
 * Return
 *  The key() function simply returns the key of the array element that's currently
 *  being pointed to by the internal pointer. It does not move the pointer in any way.
 *  If the internal pointer points beyond the end of the elements list or the array
 *  is empty, key() returns NULL.
 */
PH7_PRIVATE int ph7_hashmap_simple_key(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pCur;
	ph7_hashmap *pMap;
	if( nArg < 1 ){
		/* Missing arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		/* Invalid argument,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	pCur = pMap->pCur;
	if( pCur == 0 ){
		/* Cursor does not point to anything,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	HashmapResultNodeKey(pCtx,pCur);
	return PH7_OK;
}
/*
 * array each(array $input)
 *  Return the current key and value pair from an array and advance the array cursor.
 * Parameter
 *  $input
 *    The input array.
 * Return
 *  Returns the current key and value pair from the array array. This pair is returned
 *  in a four-element array, with the keys 0, 1, key, and value. Elements 0 and key
 *  contain the key name of the array element, and 1 and value contain the data.
 *  If the internal pointer for the array points past the end of the array contents
 *  each() returns FALSE.
 */
PH7_PRIVATE int ph7_hashmap_each(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pCur;
	ph7_hashmap *pMap;
	ph7_value *pArray;
	ph7_value *pVal;
	ph7_value sKey;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		/* Invalid argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the internal representation that describe the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->pCur == 0 ){
		/* Cursor does not point to anything,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pCur = pMap->pCur;
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pVal = HashmapExtractNodeValue(pCur);
	/* Insert the current value */
	ph7_array_add_intkey_elem(pArray,1,pVal);
	ph7_array_add_strkey_elem(pArray,"value",pVal);
	/* Make the key */
	if( pCur->iType == HASHMAP_INT_NODE ){
		PH7_MemObjInitFromInt(pMap->pVm,&sKey,pCur->xKey.iKey);
	}else{
		PH7_MemObjInitFromString(pMap->pVm,&sKey,0);
		PH7_MemObjStringAppend(&sKey,(const char *)SyBlobData(&pCur->xKey.sKey),SyBlobLength(&pCur->xKey.sKey));
	}
	/* Insert the current key */
	ph7_array_add_intkey_elem(pArray,0,&sKey);
	ph7_array_add_strkey_elem(pArray,"key",&sKey);
	PH7_MemObjRelease(&sKey);
	/* Advance the cursor */
	pMap->pCur = pCur->pPrev; /* Reverse link */
	/* Return the current entry */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * range() — a faithful port of php 8.5's ext/standard/array.c implementation
 * (php_range_process_input + PHP_FUNCTION(range)), so the value semantics,
 * diagnostics, and their ordering are byte-exact: decreasing ranges, float
 * ranges, character ranges, the step/endpoint ValueErrors, the ZPP TypeErrors
 * and null deprecations, and the string-endpoint warnings.
 */
#define PH7_RANGE_HT_MAX_SIZE 1073741824 /* php's HT_MAX_SIZE (2^30 entries) */
/*
 * Endpoint classification, mirroring php_range_process_input's return
 * contract. php returns zval type tags whose ORDER encodes the logic
 * (IS_LONG < IS_DOUBLE < IS_STRING < IS_ARRAY); the >=/< comparisons in
 * ph7_hashmap_range depend on the same ordering here.
 *   RANGE_IN_LONG/DOUBLE : only interpretable as int / float
 *   RANGE_IN_STRING      : only interpretable as a (char-range) string
 *   RANGE_IN_DIGIT       : single-byte numeric string — valid as both a char
 *                          and a number (php returns IS_ARRAY for this)
 * The RANGE_IN_* codes and RangeStrToNumber are declared in ph7int.h so the
 * stage-2 ZPP domain-error sweep can reuse the classifier (PLAN §3.9(a)).
 */
/* IEEE special-value tests: the engine-wide bit-pattern macros from
 * sxtypes.h (via ph7int.h) — same ones the printf/serialize paths use. */
/*
 * The type name php's ZPP prints after "must be of type ..., X given":
 * the concrete class name for objects, the usual type name otherwise.
 */
static const char * RangeArgTypeName(ph7_value *pVal,char *zBuf,sxu32 nBufLen)
{
	if( pVal->iFlags & MEMOBJ_OBJ ){
		ph7_class_instance *pThis = (ph7_class_instance *)pVal->x.pOther;
		sxu32 n = SXMIN(pThis->pClass->sName.nByte,nBufLen - 1);
		SyMemcpy((const void *)pThis->pClass->sName.zString,zBuf,n);
		zBuf[n] = 0;
		return zBuf;
	}
	return ph7_type_name(pVal);
}
/*
 * Classify a string with php's is_numeric_string() grammar:
 *   [ws] [sign] ( D+ [ . D* ] | . D+ ) [ (e|E) [sign] D+ ] [ws]
 * — the whole string must be consumed; hex/binary/"INF"/"NAN" are NOT
 * numeric. Returns RANGE_IN_LONG with *pLong set, RANGE_IN_DOUBLE with
 * *pDouble set (a fractional/exponent form, or an integer too wide for an
 * sxi64 — php reclassifies those as float), or RANGE_IN_ERROR when the
 * string is not numeric. The float value comes from libc strtod, like
 * php's zend_strtod (byte-exact-floats rule). zIn must be NUL-terminated
 * at zIn[nLen] — ph7_value_to_string guarantees this (SyBlobNullAppend) —
 * so strtod can parse it in place once the grammar has validated it.
 */
PH7_PRIVATE sxu8 RangeStrToNumber(const char *zIn,sxu32 nLen,sxi64 *pLong,double *pDouble)
{
	const char *z = zIn,*zEnd = &zIn[nLen];
	sxu64 uVal = 0;
	int bNeg = 0,bDigit = 0,bReal = 0,bOverflow = 0;
	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }
	if( z < zEnd && (z[0] == '+' || z[0] == '-') ){
		bNeg = (z[0] == '-');
		z++;
	}
	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){
		int d = z[0] - '0';
		/* Track overflow past 2^63, the widest magnitude an sxi64 can carry
		 * (as LONG_MIN); overflowing integers become floats like in php. */
		if( uVal > 922337203685477580ULL || (uVal == 922337203685477580ULL && d > 8) ){
			bOverflow = 1;
		}else{
			uVal = uVal * 10 + (sxu64)d;
		}
		bDigit = 1;
		z++;
	}
	if( z < zEnd && z[0] == '.' ){
		bReal = 1;
		z++;
		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){
			bDigit = 1;
			z++;
		}
	}
	/* At least one mantissa digit required (rejects "", ".", "+", "e5"). */
	if( !bDigit ){
		return RANGE_IN_ERROR;
	}
	/* Optional exponent — needs at least one digit (rejects "1e", "1e+"). */
	if( z < zEnd && (z[0] == 'e' || z[0] == 'E') ){
		z++;
		if( z < zEnd && (z[0] == '+' || z[0] == '-') ){ z++; }
		if( z >= zEnd || (unsigned char)z[0] >= 0xc0 || !SyisDigit(z[0]) ){
			return RANGE_IN_ERROR;
		}
		bReal = 1;
		while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisDigit(z[0]) ){ z++; }
	}
	/* Trailing whitespace allowed; anything else means not numeric. */
	while( z < zEnd && (unsigned char)z[0] < 0xc0 && SyisSpace(z[0]) ){ z++; }
	if( z != zEnd ){
		return RANGE_IN_ERROR;
	}
	if( bOverflow || (!bNeg && uVal > (sxu64)LARGEST_INT64)
	 || (bNeg && uVal > (sxu64)LARGEST_INT64 + 1) ){
		bReal = 1;
	}
	if( bReal ){
		*pDouble = strtod(zIn,0);
		return RANGE_IN_DOUBLE;
	}
	/* Negate in unsigned space so 2^63 lands on LONG_MIN without overflow. */
	*pLong = bNeg ? (sxi64)((sxu64)0 - uVal) : (sxi64)uVal;
	return RANGE_IN_LONG;
}
/*
 * ZPP emulation for $start/$end (php's Z_PARAM_NUMBER_OR_STR, weak mode):
 * reject array/object/resource with php's TypeError, deprecate null (the
 * value then reads as int 0 — *pbNullCoerced). php runs this for all
 * arguments BEFORE any value/domain check, hence the split from
 * RangeProcessInput below. Returns FALSE after throwing (*pRc set).
 */
static int RangeEndpointZpp(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,int *pbNullCoerced,sxi32 *pRc)
{
	SXUNUSED(pbNullCoerced); /* php coerces null to 0 with a deprecation; PHL rejects it */
	*pRc = PH7_OK;
	if( pIn->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES) ){
		char zType[80];
		*pRc = PH7_VmThrowException(pCtx,"TypeError",
			"range(): Argument #%d ($%s) must be of type string|int|float, %s given",
			iArg,zName,RangeArgTypeName(pIn,zType,sizeof(zType)));
		return FALSE;
	}
	if( pIn->iFlags & MEMOBJ_NULL ){
		/* php only DEPRECATES null here; PHL rejects it. */
		*pRc = PH7_VmThrowException(pCtx,"TypeError",
			"range(): Argument #%d ($%s) must be of type string|int|float, null given",
			iArg,zName);
		return FALSE;
	}
	return TRUE;
}
/*
 * ZPP emulation for $step (php's Z_PARAM_NUMBER, weak mode): int/float pass
 * through, bool coerces to int, null deprecates to int 0 (which then trips
 * the "cannot be 0" ValueError like php), a numeric string coerces to its
 * number, anything else is a TypeError. Returns RANGE_IN_LONG/DOUBLE, or
 * RANGE_IN_ERROR after throwing (*pRc set).
 */
static sxu8 RangeStepInput(ph7_context *pCtx,ph7_value *pIn,sxi64 *pLong,double *pDouble,sxi32 *pRc)
{
	*pRc = PH7_OK;
	if( pIn->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES) ){
		char zType[80];
		*pRc = PH7_VmThrowException(pCtx,"TypeError",
			"range(): Argument #3 ($step) must be of type int|float, %s given",
			RangeArgTypeName(pIn,zType,sizeof(zType)));
		return RANGE_IN_ERROR;
	}
	if( pIn->iFlags & MEMOBJ_NULL ){
		/* php only DEPRECATES null here; PHL rejects it. */
		*pRc = PH7_VmThrowException(pCtx,"TypeError",
			"range(): Argument #3 ($step) must be of type int|float, null given");
		return RANGE_IN_ERROR;
	}
	if( pIn->iFlags & MEMOBJ_REAL ){
		*pDouble = ph7_value_to_double(pIn);
		return RANGE_IN_DOUBLE;
	}
	if( pIn->iFlags & MEMOBJ_STRING ){
		const char *zStr;
		int nLen;
		sxu8 iKind;
		zStr = ph7_value_to_string(pIn,&nLen);
		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);
		if( iKind == RANGE_IN_ERROR ){
			*pRc = PH7_VmThrowException(pCtx,"TypeError",
				"range(): Argument #3 ($step) must be of type int|float, string given");
		}
		return iKind;
	}
	/* int / bool */
	*pLong = ph7_value_to_int64(pIn);
	return RANGE_IN_LONG;
}
/*
 * php_range_process_input port: resolve $start/$end into a number and/or a
 * char-range byte, emitting php's exact warnings (empty string, multi-byte
 * string) and ValueErrors (INF/NAN). Returns a RANGE_IN_* code, or
 * RANGE_IN_ERROR after throwing (*pRc set).
 */
static sxu8 RangeProcessInput(ph7_context *pCtx,ph7_value *pIn,int iArg,const char *zName,
	int bNullCoerced,sxi64 *pLong,double *pDouble,unsigned char *pChar,sxi32 *pRc)
{
	char zMsg[160];
	double r;
	*pRc = PH7_OK;
	if( bNullCoerced ){
		/* ZPP already deprecated the null; it reads as int 0. */
		*pLong = 0;
		*pDouble = 0.0;
		return RANGE_IN_LONG;
	}
	if( pIn->iFlags & MEMOBJ_REAL ){
		r = ph7_value_to_double(pIn);
check_dval:
		if( PH7_IS_INF(r) ){
			*pRc = PH7_VmThrowException(pCtx,"ValueError",
				"range(): Argument #%d ($%s) must be a finite number, INF provided",iArg,zName);
			return RANGE_IN_ERROR;
		}
		if( PH7_IS_NAN(r) ){
			*pRc = PH7_VmThrowException(pCtx,"ValueError",
				"range(): Argument #%d ($%s) must be a finite number, NAN provided",iArg,zName);
			return RANGE_IN_ERROR;
		}
		*pDouble = r;
		return RANGE_IN_DOUBLE;
	}
	if( pIn->iFlags & MEMOBJ_STRING ){
		const char *zStr;
		int nLen;
		sxu8 iKind;
		zStr = ph7_value_to_string(pIn,&nLen);
		if( nLen == 0 ){
			SyBufferFormat(zMsg,sizeof(zMsg),
				"range(): Argument #%d ($%s) must not be empty, casted to 0",iArg,zName);
			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);
			*pLong = 0;
			*pDouble = 0.0;
			return RANGE_IN_LONG;
		}
		iKind = RangeStrToNumber(zStr,(sxu32)nLen,pLong,pDouble);
		if( iKind == RANGE_IN_DOUBLE ){
			r = *pDouble;
			goto check_dval;
		}
		if( iKind == RANGE_IN_LONG ){
			*pDouble = (double)*pLong;
			if( nLen == 1 ){
				/* A single numeric digit works as both a char and a number. */
				*pChar = (unsigned char)zStr[0];
				return RANGE_IN_DIGIT;
			}
			return RANGE_IN_LONG;
		}
		if( nLen != 1 ){
			SyBufferFormat(zMsg,sizeof(zMsg),
				"range(): Argument #%d ($%s) must be a single byte, subsequent bytes are ignored",iArg,zName);
			PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,zMsg);
		}
		*pChar = (unsigned char)zStr[0];
		/* Fall-back numeric value in case the other argument is not a string. */
		*pLong = 0;
		*pDouble = 0.0;
		return RANGE_IN_STRING;
	}
	/* int / bool */
	*pLong = ph7_value_to_int64(pIn);
	*pDouble = (double)*pLong;
	return RANGE_IN_LONG;
}
/*
 * The two "supplied range exceeds the maximum array size" ValueErrors.
 * Both php messages print the macro's (start,end) parameters, which its
 * callers pass SWAPPED for a decreasing range — a php quirk kept for
 * byte-parity (callers below pass the values to *print*). The int and
 * float variants differ in wording ("Maximum size: N." vs "Max size: N")
 * exactly like php's two macros.
 */
static sxi32 RangeLongSizeError(ph7_context *pCtx,sxu64 nCalc,sxi64 iStart,sxi64 iEnd,sxi64 iStep)
{
	return PH7_VmThrowException(pCtx,"ValueError",
		"The supplied range exceeds the maximum array size by %qu elements: "
		"start=%qd, end=%qd, step=%qd. Calculated size: %qu. Maximum size: %qu.",
		nCalc - (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1),iStart,iEnd,iStep,
		nCalc,(sxu64)PH7_RANGE_HT_MAX_SIZE);
}
static sxi32 RangeDoubleSizeError(ph7_context *pCtx,double rCalc,double rStart,double rEnd,double rStep)
{
	/* Four %.1f doubles can reach ~313 bytes each near DBL_MAX, so format on
	 * the VM heap (auto-released with the call context) rather than parking
	 * ~1.5 KB on the native stack of a small-stack embedded port. */
	const unsigned int nBuf = 1500;
	char *zMsg = (char *)ph7_context_alloc_chunk(pCtx,nBuf,FALSE,TRUE/* Auto-release */);
	if( zMsg == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	snprintf(zMsg,nBuf,
		"The supplied range exceeds the maximum array size by %.1f elements: "
		"start=%.1f, end=%.1f, step=%.1f. Max size: 1073741824",
		rCalc - (double)PH7_RANGE_HT_MAX_SIZE,rStart,rEnd,rStep);
	return PH7_VmThrowException(pCtx,"ValueError","%s",zMsg);
}
/*
 * Set the element container to the next range element and append it to the
 * result array, surfacing allocation failure as the OOM fatal (never a
 * silently-truncated array). One helper per element type so the fill loops
 * below stay one line per iteration.
 */
static sxi32 RangeAppendInt(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,sxi64 iVal)
{
	ph7_value_int64(pValue,iVal);
	if( ph7_array_add_elem(pArray,0/* Automatic index assign*/,pValue) != SXRET_OK ){
		return PH7_ContextMemoryError(pCtx);
	}
	return PH7_OK;
}
static sxi32 RangeAppendDouble(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,double rVal)
{
	ph7_value_double(pValue,rVal);
	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){
		return PH7_ContextMemoryError(pCtx);
	}
	return PH7_OK;
}
static sxi32 RangeAppendChar(ph7_context *pCtx,ph7_value *pArray,ph7_value *pValue,char c)
{
	ph7_value_string(pValue,&c,1);
	if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){
		return PH7_ContextMemoryError(pCtx);
	}
	ph7_value_reset_string_cursor(pValue);
	return PH7_OK;
}
/*
 * array range(string|int|float $start,string|int|float $end,int|float $step = 1)
 *  Create an array containing a range of elements.
 * Return
 *  An array of elements from start to end, inclusive; int, float, or
 *  single-character string elements depending on the inputs, like php 8.
 */
PH7_PRIVATE int ph7_hashmap_range(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pValue,*pArray;
	sxi32 rc = PH7_OK;
	int is_step_double = 0,is_step_negative = 0;
	double step_double = 1.0;
	sxi64 step = 1;
	sxu8 start_type,end_type;
	sxi64 start_long = 0,end_long = 0;
	double start_double = 0.0,end_double = 0.0;
	unsigned char cStart = 0,cEnd = 0;
	int bStartNull = FALSE,bEndNull = FALSE;
	sxu32 i,size;

	/* php ZPP arity: at least 2 (enforced centrally, aBuiltinArity), at most 3. */
	if( nArg > 3 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"range() expects at most 3 arguments, %d given",nArg);
	}
	if( nArg < 2 ){
		/* Defensive only: the central arity table throws before we run. */
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"range() expects at least 2 arguments, %d given",nArg);
	}
	/* ZPP pass in argument order: type errors and null deprecations fire
	 * before any value/domain check, like php's zend_parse_parameters. */
	if( !RangeEndpointZpp(pCtx,apArg[0],1,"start",&bStartNull,&rc) ){
		return rc;
	}
	if( !RangeEndpointZpp(pCtx,apArg[1],2,"end",&bEndNull,&rc) ){
		return rc;
	}
	if( nArg > 2 ){
		sxu8 iStepKind = RangeStepInput(pCtx,apArg[2],&step,&step_double,&rc);
		if( iStepKind == RANGE_IN_ERROR ){
			return rc;
		}
		if( iStepKind == RANGE_IN_DOUBLE ){
			if( PH7_IS_INF(step_double) ){
				return PH7_VmThrowException(pCtx,"ValueError",
					"range(): Argument #3 ($step) must be a finite number, INF provided");
			}
			if( PH7_IS_NAN(step_double) ){
				return PH7_VmThrowException(pCtx,"ValueError",
					"range(): Argument #3 ($step) must be a finite number, NAN provided");
			}
			/* We only want positive step values. */
			if( step_double < 0.0 ){
				is_step_negative = 1;
				step_double *= -1;
			}
			/* zend_dval_to_lval_silent + zend_is_long_compatible: an integral
			 * in-sxi64-range float step behaves as an int (char ranges accept
			 * it, int endpoints stay int); anything else is a float step. */
			if( step_double < 9223372036854775808.0 ){
				step = (sxi64)step_double;
				if( (double)step != step_double ){
					is_step_double = 1;
				}
			}else{
				/* Casting out-of-range would be UB; `step` stays unread —
				 * every reader is gated behind !is_step_double. */
				is_step_double = 1;
			}
		}else{
			/* We only want positive step values. */
			if( step < 0 ){
				if( step == SMALLEST_INT64 ){
					/* -step would overflow */
					return PH7_VmThrowException(pCtx,"ValueError",
						"range(): Argument #3 ($step) must be greater than %qd",step);
				}
				is_step_negative = 1;
				step = -step;
			}
			step_double = (double)step;
		}
		if( step_double == 0.0 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"range(): Argument #3 ($step) cannot be 0");
		}
	}
	start_type = RangeProcessInput(pCtx,apArg[0],1,"start",bStartNull,&start_long,&start_double,&cStart,&rc);
	if( start_type == RANGE_IN_ERROR ){
		return rc;
	}
	end_type = RangeProcessInput(pCtx,apArg[1],2,"end",bEndNull,&end_long,&end_double,&cEnd,&rc);
	if( end_type == RANGE_IN_ERROR ){
		return rc;
	}
	/* Element container + result array */
	pValue = ph7_context_new_scalar(pCtx);
	pArray = ph7_context_new_array(pCtx);
	if( pValue == 0 || pArray == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	/* If the range is given as strings, generate an array of characters. */
	if( start_type >= RANGE_IN_STRING || end_type >= RANGE_IN_STRING ){
		if( start_type < RANGE_IN_STRING || end_type < RANGE_IN_STRING ){
			/* Only one side is a string: the char side converts to 0 (with a
			 * warning unless the numeric side is an ambiguous single digit)
			 * and the range is numeric. */
			if( start_type < RANGE_IN_STRING ){
				if( end_type != RANGE_IN_DIGIT ){
					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,
						"range(): Argument #1 ($start) must be a single byte string if"
						" argument #2 ($end) is a single byte string, argument #2 ($end) converted to 0");
				}
				end_type = RANGE_IN_LONG;
			}else{
				if( start_type != RANGE_IN_DIGIT ){
					PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,
						"range(): Argument #2 ($end) must be a single byte string if"
						" argument #1 ($start) is a single byte string, argument #1 ($start) converted to 0");
				}
				start_type = RANGE_IN_LONG;
			}
			goto handle_numeric_inputs;
		}
		if( is_step_double ){
			/* Only emit the warning if one of the inputs is not a numeric digit. */
			if( start_type == RANGE_IN_STRING || end_type == RANGE_IN_STRING ){
				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,
					"range(): Argument #3 ($step) must be of type int when generating an array"
					" of characters, inputs converted to 0");
			}
			start_type = RANGE_IN_LONG;
			end_type = RANGE_IN_LONG;
			goto handle_numeric_inputs;
		}
		/* Generate an array of characters */
		if( cStart > cEnd ){
			/* Decreasing char range */
			int iCur;
			if( (sxi64)(cStart - cEnd) < step ){
				goto boundary_error;
			}
			for( iCur = (int)cStart ; iCur >= (int)cEnd ; iCur -= (int)step ){
				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){
					return rc;
				}
			}
		}else if( cEnd > cStart ){
			/* Increasing char range */
			int iCur;
			if( is_step_negative ){
				goto negative_step_error;
			}
			if( (sxi64)(cEnd - cStart) < step ){
				goto boundary_error;
			}
			for( iCur = (int)cStart ; iCur <= (int)cEnd ; iCur += (int)step ){
				if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)iCur)) != PH7_OK ){
					return rc;
				}
			}
		}else{
			if( (rc = RangeAppendChar(pCtx,pArray,pValue,(char)cStart)) != PH7_OK ){
				return rc;
			}
		}
		ph7_result_value(pCtx,pArray);
		return PH7_OK;
	}
handle_numeric_inputs:
	if( start_type == RANGE_IN_DOUBLE || end_type == RANGE_IN_DOUBLE || is_step_double ){
		/* Float range */
		double elem,calc;
		if( start_double > end_double ){
			/* Decreasing float range */
			if( start_double - end_double < step_double ){
				goto boundary_error;
			}
			calc = ((start_double - end_double) / step_double) + 1;
			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){
				/* php prints start/end swapped here (see RangeDoubleSizeError). */
				return RangeDoubleSizeError(pCtx,calc,end_double,start_double,step_double);
			}
			size = (sxu32)(calc + 0.5); /* _php_math_round(...,0,HALF_UP) */
			for( i = 0,elem = start_double ; i < size && elem >= end_double ; ++i,elem = start_double - ((double)i * step_double) ){
				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){
					return rc;
				}
			}
		}else if( end_double > start_double ){
			/* Increasing float range */
			if( is_step_negative ){
				goto negative_step_error;
			}
			if( end_double - start_double < step_double ){
				goto boundary_error;
			}
			calc = ((end_double - start_double) / step_double) + 1;
			if( calc >= (double)PH7_RANGE_HT_MAX_SIZE ){
				return RangeDoubleSizeError(pCtx,calc,start_double,end_double,step_double);
			}
			size = (sxu32)(calc + 0.5);
			for( i = 0,elem = start_double ; i < size && elem <= end_double ; ++i,elem = start_double + ((double)i * step_double) ){
				if( (rc = RangeAppendDouble(pCtx,pArray,pValue,elem)) != PH7_OK ){
					return rc;
				}
			}
		}else{
			if( (rc = RangeAppendDouble(pCtx,pArray,pValue,start_double)) != PH7_OK ){
				return rc;
			}
		}
	}else{
		/* Int range. All arithmetic in unsigned space so a span wider than
		 * LARGEST_INT64 (e.g. -PHP_INT_MAX..PHP_INT_MAX) wraps correctly
		 * instead of overflowing, exactly like php's zend_ulong math. */
		sxu64 ustep = (sxu64)step;
		sxu64 calc;
		if( start_long > end_long ){
			/* Decreasing int range */
			if( (sxu64)start_long - (sxu64)end_long < ustep ){
				goto boundary_error;
			}
			calc = ((sxu64)start_long - (sxu64)end_long) / ustep;
			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){
				/* php prints start/end swapped here (see RangeLongSizeError). */
				return RangeLongSizeError(pCtx,calc,end_long,start_long,step);
			}
			size = (sxu32)(calc + 1);
			for( i = 0 ; i < size ; ++i ){
				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long - (sxu64)i * ustep))) != PH7_OK ){
					return rc;
				}
			}
		}else if( end_long > start_long ){
			/* Increasing int range */
			if( is_step_negative ){
				goto negative_step_error;
			}
			if( (sxu64)end_long - (sxu64)start_long < ustep ){
				goto boundary_error;
			}
			calc = ((sxu64)end_long - (sxu64)start_long) / ustep;
			if( calc >= (sxu64)(PH7_RANGE_HT_MAX_SIZE - 1) ){
				return RangeLongSizeError(pCtx,calc,start_long,end_long,step);
			}
			size = (sxu32)(calc + 1);
			for( i = 0 ; i < size ; ++i ){
				if( (rc = RangeAppendInt(pCtx,pArray,pValue,(sxi64)((sxu64)start_long + (sxu64)i * ustep))) != PH7_OK ){
					return rc;
				}
			}
		}else{
			if( (rc = RangeAppendInt(pCtx,pArray,pValue,start_long)) != PH7_OK ){
				return rc;
			}
		}
	}
	/* Return the new array. 'pValue' is released automatically by the
	 * virtual machine as soon as we return from this foreign function. */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
negative_step_error:
	return PH7_VmThrowException(pCtx,"ValueError",
		"range(): Argument #3 ($step) must be greater than 0 for increasing ranges");
boundary_error:
	return PH7_VmThrowException(pCtx,"ValueError",
		"range(): Argument #3 ($step) must be less than the range spanned by argument #1 ($start) and argument #2 ($end)");
}
/*
 * array array_values(array $array)
 *  Return all the values of an array, indexed numerically.
 * Parameters
 *  $array
 *   The input array.
 * Return
 *  An indexed array of values or NULL on allocation failure.
 */
PH7_PRIVATE int ph7_hashmap_values(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pNode;
	ph7_hashmap *pMap;
	ph7_value *pArray;
	ph7_value *pObj;
	sxu32 n;
	if( nArg != 1 ){
		/* Wrong argument count, throw ArgumentCountError */
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_values() expects exactly 1 argument, %d given",
			nArg
			);
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		/* Type mismatch, throw TypeError */
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_values(): Argument #1 ($array) must be of type array, %s given",
			ph7_type_name(apArg[0])
			);
	}
	/* Point to the internal representation that describe the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Perform the requested operation */
	pNode = pMap->pFirst;
	for( n = 0 ; n < pMap->nEntry ; ++n ){
		pObj = HashmapExtractNodeValue(pNode);
		if( pObj ){
			/* perform the insertion */
			ph7_array_add_elem(pArray,0/* Automatic index assign */,pObj);
		}
		/* Point to the next entry */
		pNode = pNode->pPrev; /* Reverse link */
	}
	/* return the new array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_keys(array $input [, val $search_value [, bool $strict = false ]] )
 *  Return all the keys or a subset of the keys of an array.
 * Parameters
 *  $input
 *   An array containing keys to return.
 * $search_value
 *   If specified, then only keys containing these values are returned.
 * $strict
 *   Determines if strict comparison (===) should be used during the search.
 * Return
 *  An array of all the keys in input or NULL on failure.
 */
PH7_PRIVATE int ph7_hashmap_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pNode;
	ph7_hashmap *pMap;
	ph7_value *pArray;
	ph7_value sObj;
	ph7_value sVal;
	SyString sKey;
	int bStrict;
	sxi32 rc;
	sxu32 n;
	if( nArg < 1 ){
		/* Missing argument,throw ArgumentCountError */
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_keys() expects at least 1 argument, 0 given"
			);
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		/* haystack must be an array,throw TypeError */
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_keys(): Argument #1 ($array) must be of type array, %s given",
			ph7_type_name(apArg[0])
			);
	}
	/* Point to the internal representation of the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	bStrict = FALSE;
	if( nArg > 2 ){
		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */
		if( ph7_value_is_array(apArg[2]) || ph7_value_is_object(apArg[2]) || ph7_value_is_resource(apArg[2]) ){
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_keys(): Argument #3 ($strict) must be of type bool, %s given",
				ph7_type_name(apArg[2])
				);
		}
		bStrict = ph7_value_to_bool(apArg[2]);
	}
	/* Perform the requested operation */
	pNode = pMap->pFirst;
	PH7_MemObjInit(pMap->pVm,&sVal);
	for( n = 0 ; n < pMap->nEntry ; ++n ){
		if( pNode->iType == HASHMAP_INT_NODE ){
			PH7_MemObjInitFromInt(pMap->pVm,&sObj,pNode->xKey.iKey);
		}else{
			SyStringInitFromBuf(&sKey,SyBlobData(&pNode->xKey.sKey),SyBlobLength(&pNode->xKey.sKey));
			PH7_MemObjInitFromString(pMap->pVm,&sObj,&sKey);
		}
		rc = 0;
		if( nArg > 1 ){
			ph7_value *pValue = HashmapExtractNodeValue(pNode);
			if( pValue ){
				ph7_value sNeedle;
				PH7_MemObjInit(pMap->pVm,&sNeedle);
				PH7_MemObjLoad(pValue,&sVal);
				/* Filter key — compare on duplicates of BOTH sides:
				 * PH7_MemObjCmp converts its operands in place, and a needle
				 * mutated on the first element (e.g. null coerced) would
				 * corrupt every later comparison. */
				PH7_MemObjLoad(apArg[1],&sNeedle);
				rc = ph7_value_compare(&sVal,&sNeedle,bStrict);
				PH7_MemObjRelease(&sNeedle);
				PH7_MemObjRelease(&sVal);
			}
		}
		if( rc == 0 ){
			/* Perform the insertion */
			ph7_array_add_elem(pArray,0,&sObj);
		}
		PH7_MemObjRelease(&sObj);
		/* Point to the next entry */
		pNode = pNode->pPrev; /* Reverse link */
	}
	/* return the new array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * bool array_same(array $arr1,array $arr2)
 *  Return TRUE if the given arrays are the same instance.
 *  This function is useful under PH7 since arrays are passed
 *  by reference unlike the zend engine which use pass by values.
 * Parameters
 *  $arr1
 *   First array
 *  $arr2
 *   Second array
 * Return
 *  TRUE if the arrays are the same instance.FALSE otherwise.
 * Note
 *  This function is a symisc eXtension.
 */
PH7_PRIVATE int ph7_hashmap_same(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *p1,*p2;
	int rc;
	if( nArg < 2 || !ph7_value_is_array(apArg[0]) || !ph7_value_is_array(apArg[1]) ){
		/* Missing or invalid arguments,return FALSE*/
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the hashmaps */
	p1 = (ph7_hashmap *)apArg[0]->x.pOther;
	p2 = (ph7_hashmap *)apArg[1]->x.pOther;
	rc = (p1 == p2);
	/* Same instance? */
	ph7_result_bool(pCtx,rc);
	return PH7_OK;
}
/*
 * array array_merge(array ...$arrays)
 *  Merge one or more arrays.
 * Parameters
 *  ...$arrays
 *   Variable list of arrays to merge. Each argument must be an array;
 *   passing a non-array argument throws a TypeError.
 * Return
 *  The resulting merged array. Returns an empty array when called
 *  with no arguments.
 */
PH7_PRIVATE int ph7_hashmap_merge(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap,*pSrc;
	ph7_value *pArray;
	int i;
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the hashmap */
	pMap = (ph7_hashmap *)pArray->x.pOther;
	/* Start merging */
	for( i = 0 ; i < nArg ; i++ ){
		/* Make sure we are dealing with a valid hashmap */
		if( !ph7_value_is_array(apArg[i]) ){
			/* Type mismatch -> TypeError */
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_merge(): Argument #%d must be of type array, %s given",
				i + 1,
				ph7_type_name(apArg[i])
				);
		}else{
			pSrc = (ph7_hashmap *)apArg[i]->x.pOther;
			/* Merge the two hashmaps */
			HashmapMerge(pSrc,pMap);
		}
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_copy(array $source)
 *  Make a blind copy of the target array.
 * Parameters
 *  $source
 *   Target array
 * Return
 *  Copy of the target array on success.NULL otherwise.
 * Note
 *  This function is a symisc eXtension.
 */
PH7_PRIVATE int ph7_hashmap_copy(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	ph7_value *pArray;
	if( nArg < 1 ){
		/* Missing arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the hashmap */
	pMap = (ph7_hashmap *)pArray->x.pOther;
	if( ph7_value_is_array(apArg[0])){
		/* Point to the internal representation of the source */
		ph7_hashmap *pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
		/* Perform the copy */
		PH7_HashmapDup(pSrc,pMap);
	}else{
		/* Simple insertion */
		PH7_HashmapInsert(pMap,0/* Automatic index assign*/,apArg[0]);
	}
	/* Return the duplicated array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * bool array_erase(array $source)
 *  Remove all elements from a given array.
 * Parameters
 *  $source
 *   Target array
 * Return
 *  TRUE on success.FALSE otherwise.
 * Note
 *  This function is a symisc eXtension.
 */
PH7_PRIVATE int ph7_hashmap_erase(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	if( nArg < 1 ){
		/* Missing arguments */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the target hashmap */
	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Erase */
	PH7_HashmapRelease(pMap,FALSE);
	return PH7_OK;
}
/*
 * array array_slice(array $array, int $offset [, ?int $length = null [, bool $preserve_keys = false ]])
 *  Extract a slice of the array.
 * Parameters
 *  $array
 *    The input array.
 * $offset
 *    If offset is non-negative, the sequence will start at that offset in the array.
 *    If offset is negative, the sequence will start that far from the end of the array.
 * $length (optional, nullable)
 *    If length is given and is positive, then the sequence will have that many elements
 *    in it. If length is given and is negative then the sequence will stop that many
 *    elements from the end of the array. If it is omitted or NULL, then the sequence
 *    will have everything from offset up until the end of the array.
 * $preserve_keys (optional)
 *    Note that array_slice() will reorder and reset the array indices by default.
 *    You can change this behaviour by setting preserve_keys to TRUE.
 * Return
 *   The new slice.
 */
PH7_PRIVATE int ph7_hashmap_slice(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap,*pSrc;
	ph7_hashmap_node *pCur;
	ph7_value *pArray;
	int iLength,iOfft;
	int bPreserve;
	sxi32 rc;
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_slice() expects at least 2 arguments, %d given",
			nArg
			);
	}
	if( nArg > 4 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_slice() expects at most 4 arguments, %d given",
			nArg
			);
	}
	if( !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_slice(): Argument #1 ($array) must be of type array, %s given",
			ph7_type_name(apArg[0])
			);
	}
	/* Validate $offset type: reject string, array, object, resource */
	if( ph7_value_is_string(apArg[1]) || ph7_value_is_array(apArg[1]) ||
		ph7_value_is_object(apArg[1]) || ph7_value_is_resource(apArg[1]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_slice(): Argument #2 ($offset) must be of type int, %s given",
			ph7_type_name(apArg[1])
			);
	}
	/* Validate $length type if provided: nullable int */
	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){
		if( ph7_value_is_string(apArg[2]) || ph7_value_is_array(apArg[2]) ||
			ph7_value_is_object(apArg[2]) || ph7_value_is_resource(apArg[2]) ){
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_slice(): Argument #3 ($length) must be of type ?int, %s given",
				ph7_type_name(apArg[2])
				);
		}
	}
	/* Validate $preserve_keys type if provided: reject array, object, resource */
	if( nArg > 3 ){
		if( ph7_value_is_array(apArg[3]) || ph7_value_is_object(apArg[3]) ||
			ph7_value_is_resource(apArg[3]) ){
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_slice(): Argument #4 ($preserve_keys) must be of type bool, %s given",
				ph7_type_name(apArg[3])
				);
		}
	}
	/* Point the internal representation of the target array */
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	bPreserve = FALSE;
	/* Get the offset */
	{
		sxi64 iTmp = 0;
		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"array_slice",2,"$offset","int",&iTmp);
		if( rcArg != PH7_OK ){
			return rcArg;
		}
		iOfft = (int)iTmp;
	}
	if( iOfft < 0 ){
		iOfft = (int)pSrc->nEntry + iOfft;
		if( iOfft < 0 ){
			iOfft = 0;
		}
	}
	if( iOfft >= (int)pSrc->nEntry ){
		/* Offset past end of array, return empty array */
		pArray = ph7_context_new_array(pCtx);
		if( pArray == 0 ){
			ph7_result_null(pCtx);
			return PH7_OK;
		}
		ph7_result_value(pCtx,pArray);
		return PH7_OK;
	}
	/* Get the length: NULL means "all remaining" (same as omitting) */
	iLength = (int)pSrc->nEntry - iOfft;
	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){
		iLength = ph7_value_to_int(apArg[2]);
		if( iLength < 0 ){
			iLength = ((int)pSrc->nEntry + iLength) - iOfft;
		}
		if( iLength < 0 ){
			iLength = 0;
		}
		if( iOfft + iLength > (int)pSrc->nEntry ){
			iLength = (int)pSrc->nEntry - iOfft;
		}
	}
	if( nArg > 3 ){
		bPreserve = ph7_value_to_bool(apArg[3]);
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( iLength < 1 ){
		/* Don't bother processing,return the empty array */
		ph7_result_value(pCtx,pArray);
		return PH7_OK;
	}
	/* Point to the desired entry */
	pCur = pSrc->pFirst;
	for(;;){
		if( iOfft < 1 ){
			break;
		}
		/* Point to the next entry */
		pCur = pCur->pPrev; /* Reverse link */
		iOfft--;
	}
	/* Point to the internal representation of the hashmap */
	pMap = (ph7_hashmap *)pArray->x.pOther;
	for(;;){
		if( iLength < 1 ){
			break;
		}
		/* String keys are always preserved; preserve_keys only affects int keys */
		{
			int bKeep = (pCur->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;
			rc = HashmapInsertNode(pMap,pCur,bKeep);
		}
		if( rc != SXRET_OK ){
			break;
		}
		/* Point to the next entry */
		pCur = pCur->pPrev; /* Reverse link */
		iLength--;
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * Move the last node in the hashmap linked list to immediately after pAfter
 * in iteration order.  If pAfter is NULL the node is moved to the very
 * beginning (becomes the new pFirst).
 */
static void HashmapMoveLastAfter(ph7_hashmap *pMap,ph7_hashmap_node *pAfter)
{
	ph7_hashmap_node *pNode;
	ph7_hashmap_node *pOldNext;
	pNode = pMap->pLast;
	if( pNode == 0 ){
		return;
	}
	if( pNode->pNext == 0 ){
		/* Only node in the list, nothing to move */
		return;
	}
	if( pAfter != 0 && pAfter->pPrev == pNode ){
		/* Already in the correct position */
		return;
	}
	/* Unlink pNode from the end of the list */
	pMap->pLast = pNode->pNext;
	pMap->pLast->pPrev = 0;
	/* Insert pNode after pAfter in iteration order */
	if( pAfter == 0 ){
		/* Insert at the very beginning, before pFirst */
		pNode->pNext = 0;
		pNode->pPrev = pMap->pFirst;
		if( pMap->pFirst ){
			pMap->pFirst->pNext = pNode;
		}
		pMap->pFirst = pNode;
	}else{
		pOldNext = pAfter->pPrev;
		pNode->pPrev = pOldNext;
		pNode->pNext = pAfter;
		pAfter->pPrev = pNode;
		if( pOldNext ){
			pOldNext->pNext = pNode;
		}else{
			pMap->pLast = pNode;
		}
	}
}
/*
 * array array_splice(array $array, int $offset [, int $length [, value $replacement]])
 *  Remove a portion of the array and replace it with something else.
 * Parameters
 *  $array
 *    The input array.
 *  $offset
 *    If offset is positive then the start of removed portion is at that offset
 *    from the beginning of the input array.  If offset is negative then it
 *    starts that far from the end of the input array.  If the absolute value of
 *    a negative offset exceeds the array length, offset is clamped to 0.  If a
 *    positive offset exceeds the array length, offset is clamped to the array
 *    length (i.e. nothing is removed, but replacement is appended).
 *  $length (optional)
 *    If length is omitted, removes everything from offset to the end of the
 *    array.  If length is specified and is positive, then that many elements
 *    will be removed.  If length is specified and is negative then the end of
 *    the removed portion will be that many elements from the end of the array.
 *    If the resulting length is negative it is clamped to 0.
 *  $replacement (optional)
 *    If replacement array is specified, then the removed elements are replaced
 *    with elements from this array.
 *    If offset and length are such that nothing is removed, then the elements
 *    from the replacement array are inserted in the place specified by the
 *    offset.
 *    Note that keys in replacement array are not preserved.
 *    If replacement is just one element it is not necessary to put array()
 *    around it, unless the element is an array itself, an object or NULL.
 * Return
 *   A new array consisting of the extracted elements.
 */
PH7_PRIVATE int ph7_hashmap_splice(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pCur,*pPrev,*pRnode,*pInsertAfter,*pNewNode;
	ph7_value *pArray,*pRvalue;
	ph7_hashmap *pMap,*pSrc,*pRep;
	int iLength,iOfft,i;
	sxi32 rc;
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_splice() expects at least 2 arguments, %d given",
			nArg
			);
	}
	if( !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_splice(): Argument #1 ($array) must be of type array, %s given",
			ph7_type_name(apArg[0])
			);
	}
	/* Point to the internal representation of the target array */
	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Get the offset and clamp to valid range */
	iOfft = ph7_value_to_int(apArg[1]);
	if( iOfft < 0 ){
		iOfft = (int)pSrc->nEntry + iOfft;
		if( iOfft < 0 ){
			iOfft = 0;
		}
	}else if( iOfft > (int)pSrc->nEntry ){
		iOfft = (int)pSrc->nEntry;
	}
	/* Get the length and clamp to valid range.
	 * NULL means "all remaining" (same as omitting the argument). */
	iLength = (int)pSrc->nEntry - iOfft;
	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){
		iLength = ph7_value_to_int(apArg[2]);
		if( iLength < 0 ){
			iLength = ((int)pSrc->nEntry + iLength) - iOfft;
			if( iLength < 0 ){
				iLength = 0;
			}
		}
		if( iOfft + iLength > (int)pSrc->nEntry ){
			iLength = (int)pSrc->nEntry - iOfft;
		}
	}
	/* Create the result array for removed elements */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Get replacement array if provided */
	pRep = 0;
	if( nArg > 3 ){
		if( !ph7_value_is_array(apArg[3]) ){
			/* Perform an array cast */
			PH7_MemObjToHashmap(apArg[3]);
			if( ph7_value_is_array(apArg[3]) ){
				pRep = (ph7_hashmap *)apArg[3]->x.pOther;
			}
		}else{
			pRep = (ph7_hashmap *)apArg[3]->x.pOther;
		}
		if( pRep ){
			/* Reset the loop cursor */
			pRep->pCur = pRep->pFirst;
		}
	}
	/* No early return for the nothing-to-do case: php reindexes the input
	 * array's integer keys on EVERY splice, even a no-op one. */
	/* Navigate to the offset position */
	pCur = pSrc->pFirst;
	for( i = 0 ; i < iOfft && pCur ; i++ ){
		pCur = pCur->pPrev; /* Reverse link */
	}
	/* Save the node just before the splice range as the insertion anchor.
	 * pCur->pNext is the backward link (previous node in iteration order).
	 * If pCur is NULL (offset == nEntry), the anchor is the last node. */
	pInsertAfter = (pCur != 0) ? pCur->pNext : pSrc->pLast;
	/* Remove nodes in the splice range and copy them to the result array */
	pMap = (ph7_hashmap *)pArray->x.pOther;
	for( i = 0 ; i < iLength && pCur ; i++ ){
		pPrev = pCur->pPrev;
		rc = HashmapInsertNode(pMap,pCur,FALSE);
		PH7_HashmapUnlinkNode(pCur,TRUE);
		if( rc != SXRET_OK ){
			break;
		}
		pCur = pPrev; /* Reverse link */
	}
	/* Insert replacement elements at the correct position */
	if( pRep ){
		ph7_value sSafeVal;
		while( (pRnode = PH7_HashmapGetNextEntry(pRep)) != 0 ){
			pRvalue = HashmapExtractNodeValue(pRnode);
			if( pRvalue ){
				/* Make a stack copy before inserting.  HashmapInsert() may
				 * grow the VM memobj pool, which would invalidate pRvalue
				 * since it points into that same pool. */
				sSafeVal = *pRvalue;
				rc = HashmapInsert(pSrc,0,&sSafeVal);
				if( rc == SXRET_OK && pSrc->pLast != 0 ){
					pNewNode = pSrc->pLast;
					HashmapMoveLastAfter(pSrc,pInsertAfter);
					pInsertAfter = pNewNode;
				}
			}
		}
	}
	/* php renumbers ALL integer keys of the input array in iteration order
	 * (string keys preserved) — same pass as array_shift. Pre-fix the spliced
	 * array kept its old keys, so inserts landed with out-of-sequence keys
	 * and removals left gaps. */
	{
		ph7_hashmap_node *pEntry = pSrc->pFirst;
		sxu32 n = pSrc->nEntry;
		pSrc->iNextIdx = 0;
		while( n > 0 ){
			if( pEntry->iType == HASHMAP_INT_NODE ){
				HashmapRehashIntNode(pEntry);
			}
			pEntry = pEntry->pPrev; /* Reverse link */
			n--;
		}
		pSrc->pCur = pSrc->pFirst;
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * bool in_array(value $needle,array $haystack[,bool $strict = FALSE ])
 *  Checks if a value exists in an array.
 * Parameters
 *  $needle
 *   The searched value.
 *   Note:
 *    If needle is a string, the comparison is done in a case-sensitive manner.
 * $haystack
 *  The target array.
 * $strict
 *  If the third parameter strict is set to TRUE then the in_array() function
 *  will also check the types of the needle in the haystack.
 */
PH7_PRIVATE int ph7_hashmap_in_array(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pNeedle;
	int bStrict;
	int rc;
	if( nArg < 2 ){
		/* Missing argument,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pNeedle = apArg[0];
	bStrict = 0;
	if( !ph7_value_is_array(apArg[1]) ){
		/* haystack must be an array,throw TypeError (matches array_search) */
		char zBuf[64];
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"in_array(): Argument #2 ($haystack) must be of type array, %s given",
			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))
			);
	}
	if( nArg > 2 ){
		bStrict = ph7_value_to_bool(apArg[2]);
	}
	/* Perform the lookup */
	rc = HashmapFindValue((ph7_hashmap *)apArg[1]->x.pOther,pNeedle,0,bStrict);
	/* Lookup result */
	ph7_result_bool(pCtx,rc == SXRET_OK);
	return PH7_OK;
}
/*
 * value array_search(value $needle,array $haystack[,bool $strict = false ])
 *  Searches the array for a given value and returns the corresponding key if successful.
 * Parameters
 * $needle
 *   The searched value.
 * $haystack
 *   The array.
 * $strict
 *  If the third parameter strict is set to TRUE then the array_search() function
 *  will search for identical elements in the haystack. This means it will also check
 *  the types of the needle in the haystack, and objects must be the same instance.
 * Return
 *  Returns the key for needle if it is found in the array, FALSE otherwise.
 */
PH7_PRIVATE int ph7_hashmap_search(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry;
	ph7_value *pVal,sNeedle;
	ph7_hashmap *pMap;
	ph7_value sVal;
	int bStrict;
	sxu32 n;
	int rc;
	if( nArg < 2 ){
		/* Missing argument,throw ArgumentCountError */
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_search() expects at least 2 arguments, %d given",
			nArg
			);
	}
	bStrict = FALSE;
	if( !ph7_value_is_array(apArg[1]) ){
		/* haystack must be an array,throw TypeError. VmValueGivenName gives php's
		 * ZPP value-name (true/false for bools, not ph7_type_name's "bool") */
		char zBuf[64];
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_search(): Argument #2 ($haystack) must be of type array, %s given",
			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))
			);
	}
	if( nArg > 2 ){
		/* In PHP, non-scalar values for a bool-hinted parameter raise TypeError */
		if( ph7_value_is_array(apArg[2]) || ph7_value_is_object(apArg[2]) || ph7_value_is_resource(apArg[2]) ){
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_search(): Argument #3 ($strict) must be of type bool, %s given",
				ph7_type_name(apArg[2])
				);
		}
		bStrict = ph7_value_to_bool(apArg[2]);
	}
	/* Point to the internal representation of the internal hashmap */
	pMap = (ph7_hashmap *)apArg[1]->x.pOther;
	/* Perform a linear search since we cannot sort the hashmap based on values */
	PH7_MemObjInit(pMap->pVm,&sVal);
	PH7_MemObjInit(pMap->pVm,&sNeedle);
	pEntry = pMap->pFirst;
	n = pMap->nEntry;
	for(;;){
		if( !n ){
			break;
		}
		/* Extract node value */
		pVal = HashmapExtractNodeValue(pEntry);
		if( pVal ){
			/* Make a copy of the vuurent values since the comparison routine
			 * can change their type.
			 */
			PH7_MemObjLoad(pVal,&sVal);
			PH7_MemObjLoad(apArg[0],&sNeedle);
			rc = PH7_MemObjCmp(&sNeedle,&sVal,bStrict,0);
			PH7_MemObjRelease(&sVal);
			PH7_MemObjRelease(&sNeedle);
			if( rc == 0 ){
				/* Match found,return key */
				if( pEntry->iType == HASHMAP_INT_NODE){
					/* INT key */
					ph7_result_int64(pCtx,pEntry->xKey.iKey);
				}else{
					SyBlob *pKey = &pEntry->xKey.sKey;
					/* Blob key */
					ph7_result_string(pCtx,(const char *)SyBlobData(pKey),(int)SyBlobLength(pKey));
				}
				return PH7_OK;
			}
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
		n--;
	}
	/* No such value,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * array array_diff(array $array1,array $array2,...)
 *  Computes the difference of arrays.
 * Parameters
 *  $array1
 *    The array to compare from
 *  $array2
 *    An array to compare against
 *  $...
 *   More arrays to compare against
 * Return
 *  Returns an array containing all the entries from array1 that
 *  are not present in any of the other arrays.
 */
PH7_PRIVATE int ph7_hashmap_diff(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pSrc,*pMap;
	ph7_value *pArray;
	ph7_value *pVal;
	sxi32 rc;
	sxu32 n;
	int i;
	/* Validate arguments to mimic PHP behaviour. Earlier versions simply
	 * returned NULL when the caller passed invalid parameters which made
	 * debugging difficult. */
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_diff() expects at least 1 argument, %d given",
			nArg
			);
	}
	if( !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_diff(): Argument #1 ($array) must be of type array, %s given",
			ph7_type_name(apArg[0])
			);
	}
	for(i = 1 ; i < nArg ; i++){
		if( !ph7_value_is_array(apArg[i]) ){
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_diff(): Argument #%d must be of type array, %s given",
				i + 1,
				ph7_type_name(apArg[i])
				);
		}
	}
	if( nArg == 1 ){
		/* Return the first array since we cannot perform a diff */
		ph7_result_value(pCtx,apArg[0]);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the source hashmap */
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Perform the diff */
	pEntry = pSrc->pFirst;
	n = pSrc->nEntry;
	for(;;){
		if( n < 1 ){
			break;
		}
		/* Extract the node value */
		pVal = HashmapExtractNodeValue(pEntry);
		if( pVal ){
			for( i = 1 ; i < nArg ; i++ ){
				/* Point to the internal representation of the hashmap */
				pMap = (ph7_hashmap *)apArg[i]->x.pOther;
				/* Perform the lookup */
				rc = HashmapFindValue(pMap,pVal,0,TRUE);
				if( rc == SXRET_OK ){
					/* Value exist */
					break;
				}
			}
			if( i >= nArg ){
				/* Perform the insertion */
				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);
			}
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
		n--;
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_udiff(array $array1,array $array2,...,$callback)
 *  Computes the difference of arrays by using a callback function for data comparison.
 * Parameters
 *  $array1
 *    The array to compare from
 *  $array2
 *    An array to compare against
 *  $...
 *   More arrays to compare against.
 * $callback
 *  The callback comparison function.
 *  The comparison function must return an integer less than, equal to, or greater than zero
 *  if the first argument is considered to be respectively less than, equal to, or greater
 *  than the second.
 *     int callback ( mixed $a, mixed $b )
 * Return
 *  Returns an array containing all the entries from array1 that
 *  are not present in any of the other arrays.
 */
PH7_PRIVATE int ph7_hashmap_udiff(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pSrc,*pMap;
	ph7_value *pCallback;
	ph7_value *pArray;
	ph7_value *pVal;
	sxi32 rc;
	sxu32 n;
	int i;

	/* Ensure the argument count matches PHP behaviour. */
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_udiff() expects at least 2 arguments, %d given",
			nArg
			);
	}
	if( !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_udiff(): Argument #1 ($array) must be of type array, %s given",
			ph7_type_name(apArg[0])
			);
	}

	/* php validates the CALLBACK (the last argument) before the intermediary
	 * arrays: `array_udiff([1],"x",123)` reports Argument #3 (the bad callback),
	 * not Argument #2 (the non-array). PHL had the middle-array loop first, so it
	 * named the wrong argument whenever both were invalid. */
	pCallback = apArg[nArg - 1];
	if( !ph7_value_is_callable(pCallback) ){
		if( ph7_value_is_array(pCallback) ){
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_udiff(): Argument #%d must be a valid callback, array callback must have exactly two members",
				nArg
				);
		}
		if( ph7_value_is_string(pCallback) ){
			int len;
			const char *zName = ph7_value_to_string(pCallback, &len);
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_udiff(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",
				nArg,
				zName
				);
		}
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_udiff(): Argument #%d must be a valid callback, no array or string given",
			nArg
			);
	}

	/* Now the intermediary arguments (arrays), left to right. */
	for( i = 1 ; i < nArg - 1; i++ ){
		if( !ph7_value_is_array(apArg[i]) ){
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_udiff(): Argument #%d must be of type array, %s given",
				i + 1,
				ph7_type_name(apArg[i])
				);
		}
	}

	if( nArg == 2 ){
		/* Only the original array and the callback were provided. */
		ph7_result_value(pCtx,apArg[0]);
		return PH7_OK;
	}

	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the source hashmap */
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Perform the diff */
	pEntry = pSrc->pFirst;
	n = pSrc->nEntry;
	pCtx->pVm->iCmpCallbackExc = 0;
	for(;;){
		if( n < 1 ){
			break;
		}
		/* Extract the node value */
		pVal = HashmapExtractNodeValue(pEntry);
		if( pVal ){
			for( i = 1 ; i < nArg - 1; i++ ){
				/* Point to the internal representation of the hashmap */
				pMap = (ph7_hashmap *)apArg[i]->x.pOther;
				/* Perform the lookup */
				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);
				if( rc == SXRET_OK ){
					/* Value exist */
					break;
				}
			}
			if( pCtx->pVm->iCmpCallbackExc ){
				/* The comparison callback raised: propagate so the dispatcher
				 * unwinds, before any spurious insertion into the result. */
				pCtx->pVm->iCmpCallbackExc = 0;
				return PH7_EXCEPTION;
			}
			if( i >= (nArg - 1)){
				/* Perform the insertion */
				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);
			}
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
		n--;
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_diff_assoc(array $array1,array $array2,...)
 *  Computes the difference of arrays with additional index check.
 * Parameters
 *  $array1
 *    The array to compare from
 *  $array2
 *    An array to compare against
 *  $...
 *   More arrays to compare against
 * Return
 *  Returns an array containing all the entries from array1 that
 *  are not present in any of the other arrays.
 */
PH7_PRIVATE int ph7_hashmap_diff_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pN1,*pN2,*pEntry;
	ph7_hashmap *pSrc,*pMap;
	ph7_value *pArray;
	ph7_value *pVal;
	sxi32 rc;
	sxu32 n;
	int i;
	/* Ensure the argument list is valid, emitting the same errors PHP
	 * would produce. This makes behaviour predictable and allows the
	 * accompanying integration tests to pass. */
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_diff_assoc() expects at least 1 argument, %d given",
			nArg
			);
	}
	if( !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_diff_assoc(): Argument #1 ($array) must be of type array, %s given",
			ph7_type_name(apArg[0])
			);
	}
	for(i = 1 ; i < nArg ; i++){
		if( !ph7_value_is_array(apArg[i]) ){
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_diff_assoc(): Argument #%d must be of type array, %s given",
				i + 1,
				ph7_type_name(apArg[i])
				);
		}
	}
	if( nArg == 1 ){
		/* Return the first array since we cannot perform a diff */
		ph7_result_value(pCtx,apArg[0]);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the source hashmap */
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Perform the diff */
	pEntry = pSrc->pFirst;
	n = pSrc->nEntry;
	pN1 = pN2 = 0;
	for(;;){
		int keep;
		if( n < 1 ){
			break;
		}
		/* assume the element should be kept until we find a match */
		keep = 1;
		for( i = 1 ; i < nArg ; i++ ){
			/* all arguments have been validated already, so cast directly */
			pMap = (ph7_hashmap *)apArg[i]->x.pOther;
			/* Perform a key lookup first */
			if( pEntry->iType == HASHMAP_INT_NODE ){
				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);
			}else{
				rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);
			}
			if( rc != SXRET_OK ){
				/* this array does not contain the key, continue checking others */
				continue;
			}
			/* key exists; check that value stored in the matching node is equal */
			pVal = HashmapExtractNodeValue(pEntry);
			if( pVal ){
				/* directly compare with value at pN1 rather than searching again */
				ph7_value *pVal2 = HashmapExtractNodeValue(pN1);
				if( pVal2 ){
					ph7_value sV1,sV2;
					sxi32 cmp;
					/* Compare on duplicates: PH7_MemObjCmp converts its
					 * operands in place and these are LIVE array elements (a
					 * null element used to come back bool(false) in the
					 * caller's array). */
					PH7_MemObjInit(pEntry->pMap->pVm,&sV1);
					PH7_MemObjInit(pEntry->pMap->pVm,&sV2);
					PH7_MemObjLoad(pVal,&sV1);
					PH7_MemObjLoad(pVal2,&sV2);
					cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);
					PH7_MemObjRelease(&sV1);
					PH7_MemObjRelease(&sV2);
					if( cmp == 0 ){
						/* identical key+value found in one of the arrays => drop it */
						keep = 0;
						break;
					}
				}
			}
		}
		if( keep ){
			/* Perform the insertion */
			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
		n--;
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_diff_uassoc(array $array1,array $array2,...,callback $key_compare_func)
 *  Computes the difference of arrays with additional index check which is performed
 *  by a user supplied callback function.
 * Parameters
 *  $array1
 *    The array to compare from
 *  $array2
 *    An array to compare against
 *  $...
 *   More arrays to compare against.
 *  $key_compare_func
 *   Callback function to use. The callback function must return an integer
 *   less than, equal to, or greater than zero if the first argument is considered
 *   to be respectively less than, equal to, or greater than the second.
 * Return
 *  Returns an array containing all the entries from array1 that
 *  are not present in any of the other arrays.
 */
PH7_PRIVATE int ph7_hashmap_diff_uassoc(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pSrc,*pMap;
	ph7_value *pCallback;
	ph7_value *pArray;
	sxi32 rc;
	sxu32 n;
	int i;

	/* Argument validation mimicking PHP errors. */
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_diff_uassoc() expects at least 2 arguments, %d given",
			nArg
			);
	}
	if( !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_diff_uassoc(): Argument #1 ($array) must be of type array, %s given",
			ph7_type_name(apArg[0])
			);
	}
	/* Intermediate arguments (except last) must be arrays. Last argument is
	 * expected to be a callback. */
	/* php checks the CALLBACK before the intermediary arrays (see array_udiff). */
	pCallback = apArg[nArg - 1];
	if( !ph7_value_is_callable(pCallback) ){
		/* Compose an error message that closely matches PHP output. When the
		 * argument is an array of the wrong shape we include an extra clause.
		 * If the value is neither array nor string, PHP says "no array or
		 * string given" which we also reproduce. */
		if( ph7_value_is_array(pCallback) ){
			/* ARRAY CALLBACK must have exactly two members */
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_diff_uassoc(): Argument #%d must be a valid callback, array callback must have exactly two members",
				nArg
				);
		}
		if( ph7_value_is_string(pCallback) ){
			/* A non-callable string names the function, like array_udiff. */
			int len;
			const char *zName = ph7_value_to_string(pCallback,&len);
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_diff_uassoc(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",
				nArg,
				zName
				);
		}
		/* neither array nor string */
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_diff_uassoc(): Argument #%d must be a valid callback, no array or string given",
			nArg
			);
	}
	/* Now the intermediary arrays, left to right. */
	for(i = 1 ; i < nArg - 1; i++){
		if( !ph7_value_is_array(apArg[i]) ){
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_diff_uassoc(): Argument #%d must be of type array, %s given",
				i + 1,
				ph7_type_name(apArg[i])
				);
		}
	}
	if( nArg == 2 ){
		/* If we only have the first array and the callback, just return the
		 * input array. */
		ph7_result_value(pCtx,apArg[0]);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the source hashmap */
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Perform the diff */
	pEntry = pSrc->pFirst;
	n = pSrc->nEntry;
	for(;;){
		int keep;
		if( n < 1 ){
			break;
		}
		keep = 1;
		for( i = 1 ; i < nArg - 1; i++ ){
			/* each of these must already be arrays thanks to earlier validation */
			pMap = (ph7_hashmap *)apArg[i]->x.pOther;
			/* we must compare keys via callback, not by direct lookup */
			ph7_hashmap_node *pIt = pMap->pFirst;
			while( pIt ){
				/* build temporary key values for callback */
				ph7_value key1, key2, result;
				/* initialise only once using the appropriate helper */
				if( pEntry->iType == HASHMAP_INT_NODE ){
					PH7_MemObjInitFromInt(pMap->pVm,&key1,pEntry->xKey.iKey);
				}else{
					SyString sStr;
					SyStringInitFromBuf(&sStr,
						SyBlobData(&pEntry->xKey.sKey),
						SyBlobLength(&pEntry->xKey.sKey));
					PH7_MemObjInitFromString(pMap->pVm,&key1,&sStr);
				}
				if( pIt->iType == HASHMAP_INT_NODE ){
					PH7_MemObjInitFromInt(pMap->pVm,&key2,pIt->xKey.iKey);
				}else{
					SyString sStr;
					SyStringInitFromBuf(&sStr,
						SyBlobData(&pIt->xKey.sKey),
						SyBlobLength(&pIt->xKey.sKey));
					PH7_MemObjInitFromString(pMap->pVm,&key2,&sStr);
				}
				PH7_MemObjInit(pMap->pVm,&result);
				/* call user callback with (key1, key2) */
				{
					ph7_value *apK[2];
					apK[0] = &key1;
					apK[1] = &key2;
					rc = PH7_VmCallUserFunction(pMap->pVm,pCallback,2,apK,&result);
				}
				if( rc == PH7_EXCEPTION ){
					/* The key comparison callback raised. Unlike array_udiff/
					 * array_uintersect (which signal back from
					 * HashmapFindValueByCallback via pVm->iCmpCallbackExc), this
					 * function invokes the callback inline, so it cleans up its own
					 * temporaries and propagates the exception directly. */
					PH7_MemObjRelease(&result);
					PH7_MemObjRelease(&key1);
					PH7_MemObjRelease(&key2);
					return PH7_EXCEPTION;
				}
				if( rc == SXRET_OK ){
					if( (result.iFlags & MEMOBJ_INT) == 0 ){
						PH7_MemObjToInteger(&result);
					}
					if( result.x.iVal == 0 ){
						/* keys considered equal by callback; now compare values */
						ph7_value *pVal1 = HashmapExtractNodeValue(pEntry);
						ph7_value *pVal2 = HashmapExtractNodeValue(pIt);
						if( pVal1 && pVal2 ){
							ph7_value sV1,sV2;
							sxi32 cmp;
							/* Compare on duplicates: PH7_MemObjCmp converts in
							 * place and these are LIVE array elements. */
							PH7_MemObjInit(pEntry->pMap->pVm,&sV1);
							PH7_MemObjInit(pEntry->pMap->pVm,&sV2);
							PH7_MemObjLoad(pVal1,&sV1);
							PH7_MemObjLoad(pVal2,&sV2);
							cmp = PH7_MemObjCmp(&sV1,&sV2,TRUE,0);
							PH7_MemObjRelease(&sV1);
							PH7_MemObjRelease(&sV2);
							if( cmp == 0 ){
								keep = 0;
								PH7_MemObjRelease(&result);
								/* release keys too before breaking */
								PH7_MemObjRelease(&key1);
								PH7_MemObjRelease(&key2);
								break;
							}
						}
					}
				}
				PH7_MemObjRelease(&result);
				PH7_MemObjRelease(&key1);
				PH7_MemObjRelease(&key2);
				/* move to next node */
				pIt = pIt->pPrev;
				if( keep == 0 ) break;
			}
			if( keep == 0 ) break;
		}
		if( keep ){
			/* Perform the insertion */
			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
		n--;
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_diff_key(array $array1 ,array $array2,...)
 *  Computes the difference of arrays using keys for comparison.
 * Parameters
 *  $array1
 *    The array to compare from
 *  $array2
 *    An array to compare against
 *  $...
 *   More arrays to compare against
 * Return
 *  Returns an array containing all the entries from array1 whose keys are not present
 *  in any of the other arrays.
 * Note that NULL is returned on failure.
 */
PH7_PRIVATE int ph7_hashmap_diff_key(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pSrc,*pMap;
	ph7_value *pArray;
	sxi32 rc;
	sxu32 n;
	int i;
	/* Validate arguments to mirror PHP behaviour. Previously invalid inputs
	 * would quietly return NULL which is inconsistent with other hashmap
	 * helpers. */
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_diff_key() expects at least 1 argument, %d given",
			nArg
			);
	}
	if( !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_diff_key(): Argument #1 ($array) must be of type array, %s given",
			ph7_type_name(apArg[0])
			);
	}
	for(i = 1 ; i < nArg ; i++){
		if( !ph7_value_is_array(apArg[i]) ){
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_diff_key(): Argument #%d must be of type array, %s given",
				i + 1,
				ph7_type_name(apArg[i])
				);
		}
	}
	if( nArg == 1 ){
		/* Return the first array since we cannot perform a diff */
		ph7_result_value(pCtx,apArg[0]);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the main hashmap */
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Perfrom the diff */
	pEntry = pSrc->pFirst;
	n = pSrc->nEntry;
	for(;;){
		if( n < 1 ){
			break;
		}
		for( i = 1 ; i < nArg ; i++ ){
			if( !ph7_value_is_array(apArg[i])) {
				/* ignore */
				continue;
			}
			pMap = (ph7_hashmap *)apArg[i]->x.pOther;
			if( pEntry->iType == HASHMAP_BLOB_NODE ){
				SyBlob *pKey = &pEntry->xKey.sKey;
				/* Blob lookup */
				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);
			}else{
				/* Int lookup */
				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);
			}
			if( rc == SXRET_OK ){
				/* Key exists,break immediately */
				break;
			}
		}
		if( i >= nArg ){
			/* Perform the insertion */
			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
		n--;
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_intersect(array $array1 ,array $array2,...)
 *  Computes the intersection of arrays.
 * Parameters
 *  $array1
 *    The array to compare from
 *  $array2
 *    An array to compare against
 *  $...
 *   More arrays to compare against
 * Return
 *  Returns an array containing all of the values in array1 whose values exist
 *  in all of the parameters.
 * Throws ArgumentCountError if no arguments are given.
 * Throws TypeError if any argument is not an array.
 */
PH7_PRIVATE int ph7_hashmap_intersect(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pSrc,*pMap;
	ph7_value *pArray;
	ph7_value *pVal;
	sxi32 rc;
	sxu32 n;
	int i;
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_intersect() expects at least 1 argument, %d given",
			nArg
			);
	}
	if( !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_intersect(): Argument #1 ($array) must be of type array, %s given",
			ph7_type_name(apArg[0])
			);
	}
	for( i = 1 ; i < nArg ; i++ ){
		if( !ph7_value_is_array(apArg[i]) ){
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_intersect(): Argument #%d must be of type array, %s given",
				i + 1,
				ph7_type_name(apArg[i])
				);
		}
	}
	if( nArg == 1 ){
		/* Return the first array since we cannot perform a diff */
		ph7_result_value(pCtx,apArg[0]);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the source hashmap */
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Perform the intersection */
	pEntry = pSrc->pFirst;
	n = pSrc->nEntry;
	for(;;){
		if( n < 1 ){
			break;
		}
		/* Extract the node value */
		pVal = HashmapExtractNodeValue(pEntry);
		if( pVal ){
			for( i = 1 ; i < nArg ; i++ ){
				/* Point to the internal representation of the hashmap */
				pMap = (ph7_hashmap *)apArg[i]->x.pOther;
				/* Perform the lookup */
				rc = HashmapFindValue(pMap,pVal,0,TRUE);
				if( rc != SXRET_OK ){
					/* Value does not exist */
					break;
				}
			}
			if( i >= nArg ){
				/* Perform the insertion */
				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);
			}
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
		n--;
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_intersect_assoc(array $array1 ,array $array2,...)
 *  Computes the intersection of arrays with additional index check.
 * Parameters
 *  $array1
 *    The array to compare from
 *  $array2
 *    An array to compare against
 *  $...
 *   More arrays to compare against
 * Return
 *  Returns an array containing all the values of array1 that are present
 *  in all the arguments, with matching keys.
 */
PH7_PRIVATE int ph7_hashmap_intersect_assoc(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry,*pN1,*pN2;
	ph7_hashmap *pSrc,*pMap;
	ph7_value *pArray;
	ph7_value *pVal;
	sxi32 rc;
	sxu32 n;
	int i;
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_intersect_assoc() expects at least 1 argument, %d given",
			nArg
			);
	}
	if( !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_intersect_assoc(): Argument #1 ($array) must be of type array, %s given",
			ph7_type_name(apArg[0])
			);
	}
	for( i = 1 ; i < nArg ; i++ ){
		if( !ph7_value_is_array(apArg[i]) ){
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_intersect_assoc(): Argument #%d must be of type array, %s given",
				i + 1,
				ph7_type_name(apArg[i])
				);
		}
	}
	if( nArg == 1 ){
		/* Return the first array since we cannot perform an intersection */
		ph7_result_value(pCtx,apArg[0]);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the source hashmap */
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Perform the intersection */
	pEntry = pSrc->pFirst;
	n = pSrc->nEntry;
	pN1 = pN2 = 0; /* cc warning */
	for(;;){
		if( n < 1 ){
			break;
		}
		/* Extract the node value */
		pVal = HashmapExtractNodeValue(pEntry);
		if( pVal ){
			for( i = 1 ; i < nArg ; i++ ){
				/* Point to the internal representation of the hashmap */
				pMap = (ph7_hashmap *)apArg[i]->x.pOther;
				/* Perform a key lookup first */
				if( pEntry->iType == HASHMAP_INT_NODE ){
					rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,&pN1);
				}else{
					rc = HashmapLookupBlobKey(pMap,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey),&pN1);
				}
				if( rc != SXRET_OK ){
					/* No such key,break immediately */
					break;
				}
				/* Perform the lookup */
				rc = HashmapFindValue(pMap,pVal,&pN2,TRUE);
				if( rc != SXRET_OK || pN1 != pN2 ){
					/* Value does not exist */
					break;
				}
			}
			if( i >= nArg ){
				/* Perform the insertion */
				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);
			}
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
		n--;
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_intersect_key(array $array1 ,...)
 *  Computes the intersection of arrays using keys for comparison.
 * Parameters
 *  $array1
 *    The array to compare from
 *  $...
 *   More arrays to compare against
 * Return
 *  Returns an associative array containing all the entries of array1 which
 *  have keys that are present in all arguments.
 * Note that NULL is returned on failure.
 */
PH7_PRIVATE int ph7_hashmap_intersect_key(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pSrc,*pMap;
	ph7_value *pArray;
	sxi32 rc;
	sxu32 n;
	int i;
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_intersect_key() expects at least 1 argument, %d given",
			nArg
			);
	}
	if( !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_intersect_key(): Argument #1 ($array) must be of type array, %s given",
			ph7_type_name(apArg[0])
			);
	}
	for( i = 1 ; i < nArg ; i++ ){
		if( !ph7_value_is_array(apArg[i]) ){
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_intersect_key(): Argument #%d must be of type array, %s given",
				i + 1,
				ph7_type_name(apArg[i])
				);
		}
	}
	if( nArg == 1 ){
		/* Return the first array since we cannot perform an intersection */
		ph7_result_value(pCtx,apArg[0]);
		return PH7_OK;
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the main hashmap */
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Perform the intersection */
	pEntry = pSrc->pFirst;
	n = pSrc->nEntry;
	for(;;){
		if( n < 1 ){
			break;
		}
		for( i = 1 ; i < nArg ; i++ ){
			pMap = (ph7_hashmap *)apArg[i]->x.pOther;
			if( pEntry->iType == HASHMAP_BLOB_NODE ){
				SyBlob *pKey = &pEntry->xKey.sKey;
				/* Blob lookup */
				rc = HashmapLookupBlobKey(pMap,SyBlobData(pKey),SyBlobLength(pKey),0);
			}else{
				/* Int key */
				rc = HashmapLookupIntKey(pMap,pEntry->xKey.iKey,0);
			}
			if( rc != SXRET_OK ){
				/* Key does not exist, break immediately */
				break;
			}
		}
		if( i >= nArg ){
			/* Perform the insertion */
			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
		n--;
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_uintersect(array $array1 ,array $array2,...,$callback)
 *  Computes the intersection of arrays.
 * Parameters
 *  $array1
 *    The array to compare from
 *  $array2
 *    An array to compare against
 *  $...
 *   More arrays to compare against
 * $callback
 *  The callback comparison function.
 *  The comparison function must return an integer less than, equal to, or greater than zero
 *  if the first argument is considered to be respectively less than, equal to, or greater
 *  than the second.
 *     int callback ( mixed $a, mixed $b )
 * Return
 *  Returns an array containing all of the values in array1 whose values exist
 *  in all of the parameters. .
 * Note that NULL is returned on failure.
 */
PH7_PRIVATE int ph7_hashmap_uintersect(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pSrc,*pMap;
	ph7_value *pCallback;
	ph7_value *pArray;
	ph7_value *pVal;
	sxi32 rc;
	sxu32 n;
	int i;

	/* Ensure the argument count matches PHP behaviour. */
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_uintersect() expects at least 2 arguments, %d given",
			nArg
			);
	}
	if( !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_uintersect(): Argument #1 ($array) must be of type array, %s given",
			ph7_type_name(apArg[0])
			);
	}

	/* php checks the CALLBACK before the intermediary arrays (see array_udiff). */
	pCallback = apArg[nArg - 1];
	if( !ph7_value_is_callable(pCallback) ){
		if( ph7_value_is_array(pCallback) ){
			/* PHP emits a special message when the array length is wrong.
			 * If the array has two elements but is still not callable (e.g. missing
			 * method / missing class), we must emit a more general error instead.
			 */
			ph7_hashmap *pCb = (ph7_hashmap *)pCallback->x.pOther;
			if( pCb->nEntry != 2 ){
				return PH7_VmThrowException(pCtx,
					"TypeError",
					"array_uintersect(): Argument #%d must be a valid callback, array callback must have exactly two members",
					nArg
					);
			}
			/* Try to provide a more precise error like PHP does for missing classes/methods. */
			{
				ph7_value *pKey = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->nValIdx);
				ph7_value *pMethod = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pCb->pFirst->pPrev->nValIdx);
				if( pKey && pMethod && (pMethod->iFlags & MEMOBJ_STRING) ){
					int nMethodLen;
					const char *zMethod = ph7_value_to_string(pMethod,&nMethodLen);
					ph7_class *pClass = PH7_VmExtractClassFromValue(pCtx->pVm,pKey);
					if( pClass ){
						/* Class exists but method is missing. */
						return PH7_VmThrowException(pCtx,
							"TypeError",
							"array_uintersect(): Argument #%d must be a valid callback, class %s does not have a method \"%s\"",
							nArg,
							(const char *)SyStringData(&pClass->sName),
							zMethod
							);
					}
					/* Class not found */
					{
						int nName;
						const char *zName = ph7_value_to_string(pKey,&nName);
						return PH7_VmThrowException(pCtx,
							"TypeError",
							"array_uintersect(): Argument #%d must be a valid callback, class \"%s\" not found",
							nArg,
							zName
							);
					}
				}
			}
			/* Fallback message */
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_uintersect(): Argument #%d must be a valid callback, no array or string given",
				nArg
				);
		}
		if( ph7_value_is_string(pCallback) ){
			int len;
			const char *zName = ph7_value_to_string(pCallback, &len);
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_uintersect(): Argument #%d must be a valid callback, function \"%s\" not found or invalid function name",
				nArg,
				zName
				);
		}
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_uintersect(): Argument #%d must be a valid callback, no array or string given",
			nArg
			);
	}

	/* Now the intermediary arrays, left to right. */
	for( i = 1 ; i < nArg - 1; i++ ){
		if( !ph7_value_is_array(apArg[i]) ){
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_uintersect(): Argument #%d must be of type array, %s given",
				i + 1,
				ph7_type_name(apArg[i])
				);
		}
	}

	if( nArg == 2 ){
		/* Only the original array and the callback were provided. */
		ph7_result_value(pCtx,apArg[0]);
		return PH7_OK;
	}

	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the source hashmap */
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Perform the intersection */
	pEntry = pSrc->pFirst;
	n = pSrc->nEntry;
	pCtx->pVm->iCmpCallbackExc = 0;
	for(;;){
		if( n < 1 ){
			break;
		}
		/* Extract the node value */
		pVal = HashmapExtractNodeValue(pEntry);
		if( pVal ){
			for( i = 1 ; i < nArg - 1; i++ ){
				if( !ph7_value_is_array(apArg[i])) {
					/* ignore */
					continue;
				}
				/* Point to the internal representation of the hashmap */
				pMap = (ph7_hashmap *)apArg[i]->x.pOther;
				/* Perform the lookup */
				rc = HashmapFindValueByCallback(pMap,pVal,pCallback,0);
				if( rc != SXRET_OK ){
					/* Value does not exist */
					break;
				}
			}
			if( i >= (nArg-1) ){
				/* Perform the insertion */
				HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);
			}
		}
		if( pCtx->pVm->iCmpCallbackExc ){
			/* The comparison callback raised: propagate so the dispatcher unwinds. */
			pCtx->pVm->iCmpCallbackExc = 0;
			return PH7_EXCEPTION;
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
		n--;
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_fill(int $start_index,int $num,var $value)
 *  Fill an array with values.
 * Parameters
 *  $start_index
 *    The first index of the returned array.
 *  $num
 *   Number of elements to insert.
 *  $value
 *    Value to use for filling.
 * Return
 *  The filled array or null on failure.
 */
PH7_PRIVATE int ph7_hashmap_fill(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray;
	int i,nEntry;

	/* PHP enforces argument count and type checks. */
	if( nArg != 3 ){
		/* wrong number of arguments -> ArgumentCountError */
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_fill() expects exactly 3 arguments, %d given",
			nArg
			);
	}

	/* Argument #1: start index must be convertible to int.  Accept booleans,
	 * floats, and numeric strings (including those with decimal point) by
	 * allowing them through the conversion.  Only arrays, objects, resources
	 * and NULLs are rejected outright. */
	if( ph7_value_is_array(apArg[0]) || ph7_value_is_object(apArg[0]) ||
		ph7_value_is_resource(apArg[0]) || ph7_value_is_null(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_fill(): Argument #1 ($start_index) must be of type int, %s given",
			ph7_type_name(apArg[0])
			);
	}
	if( ph7_value_is_string(apArg[0]) ){
		int len;
		sxu8 bReal = FALSE;
		const char *zStr = ph7_value_to_string(apArg[0], &len);
		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){
			/* Non‑numeric string is an error. */
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_fill(): Argument #1 ($start_index) must be of type int, string given"
				);
		}
		if( bReal ){
			/* float-string -> deprecation warning */
			PH7_VmThrowException(pCtx,"TypeError",
				"Implicit conversion from float-string to int loses precision");
		}
	}

	/* Argument #2: count must be convertible to non-negative int.  Allow booleans,
	 * floats and numeric strings; reject arrays, objects, resources and NULL. */
	if( ph7_value_is_array(apArg[1]) || ph7_value_is_object(apArg[1]) ||
		ph7_value_is_resource(apArg[1]) || ph7_value_is_null(apArg[1]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_fill(): Argument #2 ($count) must be of type int, %s given",
			ph7_type_name(apArg[1])
			);
	}
	if( ph7_value_is_string(apArg[1]) ){
		int len;
		sxu8 bReal = FALSE;
		const char *zStr = ph7_value_to_string(apArg[1], &len);
		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_fill(): Argument #2 ($count) must be of type int, string given"
				);
		}
	}
	/* Note: booleans and floats (including fractional) are now accepted; they
	 * will be converted by ph7_value_to_int below. */
	if( ph7_value_is_float(apArg[1]) ){
		double d = ph7_value_to_double(apArg[1]);
		/* avoid hiding outer 'i' (loop index) */
		sxi64 i64 = (sxi64)d;
		if( d != (double)i64 ){
			PH7_VmThrowException(pCtx,"TypeError",
				"Implicit conversion from float to int loses precision");
		}
	}

	/* Total number of entries to insert. Read as 64-bit FIRST: the old 32-bit
	 * read truncated array_fill(0, PHP_INT_MAX, x) to -1 and reported the
	 * negative-count message where php says "is too large". */
	sxi64 nEntry64 = ph7_value_to_int64(apArg[1]);
	/* Reject negative counts with a ValueError like PHP. */
	if( nEntry64 < 0 ){
		return PH7_VmThrowException(pCtx,
			"ValueError",
			"array_fill(): Argument #2 ($count) must be greater than or equal to 0"
			);
	}
	if( nEntry64 > 0x7fffffff ){
		/* php's threshold (probed 8.5.8): count > INT32_MAX is the distinct
		 * "is too large" ValueError; INT32_MAX itself proceeds to allocation
		 * (php then dies on the overflowing allocation, PHL OOMs gracefully). */
		return PH7_VmThrowException(pCtx,
			"ValueError",
			"array_fill(): Argument #2 ($count) is too large"
			);
	}
	nEntry = (int)nEntry64;

	/* If zero elements were requested, return an empty array without allocating */
	if( nEntry == 0 ){
		ph7_result_value(pCtx, ph7_context_new_array(pCtx));
		return PH7_OK;
	}

	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}

	/* PHP 8 fills consecutive integer keys start_index, start_index+1, … even
	 * when start_index is negative (PHP 7 restarted the remaining keys from 0,
	 * so array_fill(-5,3) gave -5,0,1 instead of -5,-4,-3). Assign each key
	 * explicitly rather than relying on automatic (append) indexing. */
	int iStart = ph7_value_to_int(apArg[0]);
	for( i = 0 ; i < nEntry ; i++ ){
		if( ph7_array_add_intkey_elem(pArray, iStart + i, apArg[2]) != SXRET_OK ){
			/* Allocation failure: surface a fatal instead of a partial array */
			return PH7_ContextMemoryError(pCtx);
		}
	}
	/* Return the filled array */
	ph7_result_value(pCtx, pArray);
	return PH7_OK;
}
/*
 * array array_fill_keys(array $input,mixed $value)
 *  Fill an array with values, specifying keys.
 * Parameters
 *  $input
 *   Array of values that will be used as key.
 *  $value
 *    Value to use for filling.
 * Return
 *  The filled array.
 * Throws
 *  ValueError if $input is not an array.
 */
PH7_PRIVATE int ph7_hashmap_fill_keys(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pSrc;
	ph7_value *pArray;
	sxu32 n;
	/* PHP enforces exactly 2 arguments. */
	if( nArg != 2 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_fill_keys() expects exactly 2 arguments, %d given",
			nArg
			);
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_fill_keys(): Argument #1 ($keys) must be of type array, %s given",
			ph7_type_name(apArg[0])
			);
	}
	/* Point to the internal representation of the input hashmap */
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Perform the requested operation */
	pEntry = pSrc->pFirst;
	for( n = 0 ; n < pSrc->nEntry ; n++ ){
		ph7_array_add_elem(pArray,HashmapExtractNodeValue(pEntry),apArg[1]);
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	/* Return the filled array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_combine(array $keys,array $values)
 *  Creates an array by using one array for keys and another for its values.
 * Parameters
 *  $keys
 *    Array of keys to be used.
 * $values
 *   Array of values to be used.
 * Return
 *  Returns the combined array. Otherwise FALSE if the number of elements
 *  for each array isn't equal or if one of the given arguments is
 *  not an array.
 */
PH7_PRIVATE int ph7_hashmap_combine(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pKe,*pVe;
	ph7_hashmap *pKey,*pValue;
	ph7_value *pArray;
	sxu32 n;
	/* PHP enforces argument count and type checks. */
	if( nArg != 2 ){
		/* wrong number of arguments -> ArgumentCountError */
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_combine() expects exactly 2 arguments, %d given",
			nArg
			);
	}
	/* Validate argument types individually so we can report the correct
	 * argument index in the error message. */
	if( !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_combine(): Argument #1 ($keys) must be of type array, %s given",
			ph7_type_name(apArg[0])
			);
	}
	if( !ph7_value_is_array(apArg[1]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_combine(): Argument #2 ($values) must be of type array, %s given",
			ph7_type_name(apArg[1])
			);
	}
	/* Point to the internal representation of the input hashmaps */
	pKey   = (ph7_hashmap *)apArg[0]->x.pOther;
	pValue = (ph7_hashmap *)apArg[1]->x.pOther;
	if( pKey->nEntry != pValue->nEntry ){
		/* Length mismatch -> ValueError */
		return PH7_VmThrowException(pCtx,
			"ValueError",
			"array_combine(): Argument #1 ($keys) and argument #2 ($values) must have the same number of elements"
			);
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	pKe = pKey->pFirst;
	pVe = pValue->pFirst;
	for( n = 0 ; n < pKey->nEntry ; n++ ){
		ph7_value *pKeyVal = HashmapExtractNodeValue(pKe);
		ph7_value *pValVal = HashmapExtractNodeValue(pVe);
		/* PHP treats floats used as keys in array_combine differently than
		 * ordinary offset access: the float is stringified rather than
		 * truncated.  To emulate this behavior we create a temporary copy of
		 * the value when it is a float and convert the copy to string.  The
		 * original array must not be mutated. */
		ph7_value *pKeyCopy = pKeyVal;
		if( ph7_value_is_float(pKeyVal) ){
			ph7_value *pTmpKey = ph7_context_new_scalar(pCtx);
			if( pTmpKey ){
				PH7_MemObjStore(pKeyVal,pTmpKey);
				/* Convert copy to string so it becomes "1.5" or "2" etc. */
				PH7_MemObjToString(pTmpKey);
				pKeyCopy = pTmpKey;
			}
		}
		ph7_array_add_elem(pArray,pKeyCopy,pValVal);
		/* Point to the next entry */
		pKe = pKe->pPrev; /* Reverse link */
		pVe = pVe->pPrev;
	}
	/* Return the filled array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_reverse(array $array [,bool $preserve_keys = false ])
 *  Return an array with elements in reverse order.
 * Parameters
 *  $array
 *   The input array.
 *  $preserve_keys (optional)
 *   If set to TRUE keys are preserved.
 * Return
 *  The reversed array.
 */
PH7_PRIVATE int ph7_hashmap_reverse(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pSrc;
	ph7_value *pArray;
	int bPreserve;
	sxu32 n;
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_reverse() expects at least 1 argument, %d given",
			nArg
			);
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_reverse(): Argument #1 ($array) must be of type array, %s given",
			ph7_type_name(apArg[0])
			);
	}
	bPreserve = FALSE;
	if( nArg > 1 ){
		bPreserve = ph7_value_to_bool(apArg[1]);
	}
	/* Point to the internal representation of the input hashmap */
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Perform the requested operation */
	pEntry = pSrc->pLast;
	for( n = 0 ; n < pSrc->nEntry ; n++ ){
		/* String keys are always preserved; preserve_keys only affects int keys */
		int bKeep = (pEntry->iType == HASHMAP_INT_NODE) ? bPreserve : TRUE;
		HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,bKeep);
		/* Point to the previous entry */
		pEntry = pEntry->pNext; /* Reverse link */
	}
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_unique(array $array, int $flags = SORT_STRING)
 *  Removes duplicate values from an array.
 * Parameters
 *  $array
 *   The input array.
 *  $flags
 *   The optional second parameter may be used to modify the comparison
 *   behavior using these values:
 *     SORT_REGULAR - compare items normally (don't change types)
 *     SORT_NUMERIC - compare items numerically
 *     SORT_STRING  - compare items as strings
 * Return
 *  The filtered array.
 */
PH7_PRIVATE int ph7_hashmap_unique(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry;
	ph7_value *pNeedle;
	ph7_hashmap *pSrc;
	ph7_value *pArray;
	int iFlags,base,bFold;
	sxu32 n;
	if( nArg < 1 ){
		/* Missing arguments, throw ArgumentCountError */
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_unique() expects at least 1 argument, 0 given"
			);
	}
	if( nArg > 2 ){
		/* Too many arguments, throw ArgumentCountError */
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_unique() expects at most 2 arguments, %d given",
			nArg
			);
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		/* Type mismatch, throw TypeError */
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_unique(): Argument #1 ($array) must be of type array, %s given",
			ph7_type_name(apArg[0])
			);
	}
	/* php's default is SORT_STRING (2): elements compare as strings. Explicit
	 * flags select numeric / natural / case-insensitive comparison. */
	iFlags = nArg > 1 ? ph7_value_to_int(apArg[1]) : 2 /* SORT_STRING */;
	base = iFlags & ~8;
	bFold = (iFlags & 8) != 0;
	/* Point to the internal representation of the input hashmap */
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Perform the requested operation */
	pEntry = pSrc->pFirst;
	for( n = 0 ; n < pSrc->nEntry ; n++ ){
		pNeedle = HashmapExtractNodeValue(pEntry);
		if( pNeedle ){
			/* Keep this element unless a flag-equal one is already present. */
			ph7_hashmap *pKept = (ph7_hashmap *)pArray->x.pOther;
			ph7_hashmap_node *pK = pKept->pFirst;
			int bDup = 0;
			sxu32 i;
			/* Forward iteration in this map uses the pPrev link (see the outer
			 * loop over pSrc). */
			for( i = 0 ; i < pKept->nEntry && pK ; ++i ){
				ph7_value *pV = HashmapExtractNodeValue(pK);
				if( pV && HashmapValueFlagEqual(pCtx->pVm,pNeedle,pV,base,bFold) ){
					bDup = 1;
					break;
				}
				pK = pK->pPrev;
			}
			if( !bDup ){
				HashmapInsertNode(pKept,pEntry,TRUE);
			}
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_flip(array $input)
 *  Exchanges all keys with their associated values in an array.
 * Parameter
 *  $input
 *   Input array.
 * Return
 *   The flipped array on success or NULL on failure.
 */
PH7_PRIVATE int ph7_hashmap_flip(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pSrc;
	ph7_value *pArray;
	ph7_value *pKey;
	ph7_value sVal;
	sxu32 n;

	/* PHP requires exactly one argument */
	if( nArg != 1 ){
		/* Use ArgumentCountError like other array helpers */
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_flip() expects exactly 1 argument, %d given",
			nArg
			);
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		/* Type mismatch -> TypeError */
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_flip(): Argument #1 ($array) must be of type array, %s given",
			ph7_type_name(apArg[0])
			);
	}
	/* Point to the internal representation of the input hashmap */
	pSrc = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Start processing */
	pEntry = pSrc->pFirst;
	for( n = 0 ; n < pSrc->nEntry ; n++ ){
		/* Extract the node value (will become a key in the result) */
		pKey = HashmapExtractNodeValue(pEntry);
		if( pKey ){
			/* NULL values are not valid keys either, PHP emits a warning */
			if( pKey->iFlags & MEMOBJ_NULL ){
				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,
					"array_flip(): Can only flip string and integer values, entry skipped"
					);
			}else if( (pKey->iFlags & MEMOBJ_INT) || (pKey->iFlags & MEMOBJ_STRING) ){
				/* Prepare the value for insertion (original key) */
				if( pEntry->iType == HASHMAP_INT_NODE ){
					PH7_MemObjInitFromInt(pSrc->pVm,&sVal,pEntry->xKey.iKey);
				}else{
					SyString sStr;
					SyStringInitFromBuf(&sStr,SyBlobData(&pEntry->xKey.sKey),SyBlobLength(&pEntry->xKey.sKey));
					PH7_MemObjInitFromString(pSrc->pVm,&sVal,&sStr);
				}
				/* Perform the insertion */
				ph7_array_add_elem(pArray,pKey,&sVal);
				/* Safely release the value because each inserted entry
				 * has its own private copy of the value.
				 */
				PH7_MemObjRelease(&sVal);
			}else{
				/* Unsupported value type -> emit warning and skip the entry */
				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,
					"array_flip(): Can only flip string and integer values, entry skipped"
					);
			}
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * number array_sum(array $array )
 *  Calculate the sum of values in an array.
 * Parameters
 *  $array: The input array.
 * Return
 *  Returns the sum of values as an integer or float.
 */
static void DoubleSum(ph7_context *pCtx,ph7_hashmap *pMap)
{
	ph7_hashmap_node *pEntry;
	ph7_value *pObj;
	double dSum = 0;
	sxu32 n;
	pEntry = pMap->pFirst;
	for( n = 0 ; n < pMap->nEntry ; n++ ){
		pObj = HashmapExtractNodeValue(pEntry);
		if( pObj ){
			if( pObj->iFlags & MEMOBJ_REAL ){
				dSum += pObj->rVal;
			}else if( pObj->iFlags & (MEMOBJ_INT|MEMOBJ_BOOL) ){
				dSum += (double)pObj->x.iVal;
			}else if( pObj->iFlags & MEMOBJ_STRING ){
				if( !PH7_MemObjStringIsNumeric(pObj) ){
					/* php warns and SKIPS a non-numeric string (the array/object/
					 * resource cases below already did; only this one was silent) */
					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
						"Addition is not supported on type string");
				}else if( SyBlobLength(&pObj->sBlob) > 0 ){
					double dv = 0;
					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);
					dSum += dv;
				}
			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){
				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,
					"array_sum(): Addition is not supported on type array");
			}else if( pObj->iFlags & MEMOBJ_OBJ ){
				/* php names the CLASS here, not the literal word "object" */
				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;
				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
					"Addition is not supported on type %s",
					pInst && pInst->pClass ? pInst->pClass->sName.zString : "object");
			}else if( pObj->iFlags & MEMOBJ_RES ){
				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,
					"array_sum(): Addition is not supported on type resource");
			}
			/* NULL is silently treated as 0 (matches PHP) */
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	/* Return sum */
	ph7_result_double(pCtx,dSum);
}
static void Int64Sum(ph7_context *pCtx,ph7_hashmap *pMap)
{
	ph7_hashmap_node *pEntry;
	ph7_value *pObj;
	sxi64 nSum = 0;
	sxu32 n;
	pEntry = pMap->pFirst;
	for( n = 0 ; n < pMap->nEntry ; n++ ){
		pObj = HashmapExtractNodeValue(pEntry);
		if( pObj ){
			if( pObj->iFlags & (MEMOBJ_INT|MEMOBJ_BOOL) ){
				nSum += pObj->x.iVal;
			}else if( pObj->iFlags & MEMOBJ_STRING ){
				if( !PH7_MemObjStringIsNumeric(pObj) ){
					/* php warns and SKIPS a non-numeric string */
					ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
						"Addition is not supported on type string");
				}else if( SyBlobLength(&pObj->sBlob) > 0 ){
					sxi64 nv = 0;
					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);
					nSum += nv;
				}
			}else if( pObj->iFlags & MEMOBJ_HASHMAP ){
				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,
					"array_sum(): Addition is not supported on type array");
			}else if( pObj->iFlags & MEMOBJ_OBJ ){
				/* php names the CLASS here, not the literal word "object" */
				ph7_class_instance *pInst = (ph7_class_instance *)pObj->x.pOther;
				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
					"Addition is not supported on type %s",
					pInst && pInst->pClass ? pInst->pClass->sName.zString : "object");
			}else if( pObj->iFlags & MEMOBJ_RES ){
				PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,
					"array_sum(): Addition is not supported on type resource");
			}
			/* NULL is silently treated as 0 (matches PHP) */
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	/* Return sum */
	ph7_result_int64(pCtx,nSum);
}
/* number array_sum(array $array )
 * (See block-coment above)
 */
PH7_PRIVATE int ph7_hashmap_sum(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pMap;
	ph7_value *pObj;
	int useDouble = 0;
	sxu32 n;
	/* PHP requires exactly one argument */
	if( nArg != 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_sum() expects exactly 1 argument, %d given",
			nArg
			);
	}
	/* Make sure we are dealing with a valid hashmap */
	if( !ph7_value_is_array(apArg[0]) ){
		/* Type mismatch -> TypeError (php's true/false/class-name convention). */
		char zBuf[64];
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_sum(): Argument #1 ($array) must be of type array, %s given",
			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))
			);
	}
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry < 1 ){
		/* Nothing to compute,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Scan all elements: if any value is a float, use floating-point
	 * arithmetic for the entire sum (matches PHP behaviour).
	 */
	pEntry = pMap->pFirst;
	for( n = 0 ; n < pMap->nEntry ; n++ ){
		pObj = HashmapExtractNodeValue(pEntry);
		if( pObj ){
			if( pObj->iFlags & MEMOBJ_REAL ){
				useDouble = 1;
				break;
			}
			if( pObj->iFlags & MEMOBJ_STRING ){
				const char *zStr = (const char *)SyBlobData(&pObj->sBlob);
				sxu32 nLen = SyBlobLength(&pObj->sBlob);
				sxu32 i;
				for( i = 0 ; i < nLen ; i++ ){
					if( zStr[i] == '.' || zStr[i] == 'e' || zStr[i] == 'E' ){
						useDouble = 1;
						break;
					}
				}
				if( useDouble ){
					break;
				}
			}
		}
		pEntry = pEntry->pPrev;
	}
	if( useDouble ){
		DoubleSum(pCtx,pMap);
	}else{
		Int64Sum(pCtx,pMap);
	}
	return PH7_OK;
}
/*
 * number array_product(array $array )
 *  Calculate the product of values in an array.
 * Parameters
 *  $array: The input array.
 * Return
 *  Returns the product of values as an integer or float.
 */
static void DoubleProd(ph7_context *pCtx,ph7_hashmap *pMap)
{
	ph7_hashmap_node *pEntry;
	ph7_value *pObj;
	double dProd;
	sxu32 n;
	pEntry = pMap->pFirst;
	dProd = 1;
	for( n = 0 ; n < pMap->nEntry ; n++ ){
		pObj = HashmapExtractNodeValue(pEntry);
		if( pObj && (pObj->iFlags & (MEMOBJ_NULL|MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES)) == 0){
			if( pObj->iFlags & MEMOBJ_REAL ){
				dProd *= pObj->rVal;
			}else if( pObj->iFlags & (MEMOBJ_INT|MEMOBJ_BOOL) ){
				dProd *= (double)pObj->x.iVal;
			}else if( pObj->iFlags & MEMOBJ_STRING ){
				if( SyBlobLength(&pObj->sBlob) > 0 ){
					double dv = 0;
					SyStrToReal((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&dv,0);
					dProd *= dv;
				}
			}
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	/* Return product */
	ph7_result_double(pCtx,dProd);
}
static void Int64Prod(ph7_context *pCtx,ph7_hashmap *pMap)
{
	ph7_hashmap_node *pEntry;
	ph7_value *pObj;
	sxi64 nProd;
	sxu32 n;
	pEntry = pMap->pFirst;
	nProd = 1;
	for( n = 0 ; n < pMap->nEntry ; n++ ){
		pObj = HashmapExtractNodeValue(pEntry);
		if( pObj && (pObj->iFlags & (MEMOBJ_NULL|MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES)) == 0){
			if( pObj->iFlags & MEMOBJ_REAL ){
				nProd *= (sxi64)pObj->rVal;
			}else if( pObj->iFlags & (MEMOBJ_INT|MEMOBJ_BOOL) ){
				nProd *= pObj->x.iVal;
			}else if( pObj->iFlags & MEMOBJ_STRING ){
				if( SyBlobLength(&pObj->sBlob) > 0 ){
					sxi64 nv = 0;
					SyStrToInt64((const char *)SyBlobData(&pObj->sBlob),SyBlobLength(&pObj->sBlob),(void *)&nv,0);
					nProd *= nv;
				}
			}
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	/* Return product */
	ph7_result_int64(pCtx,nProd);
}
/* number array_product(array $array )
 * (See block-block comment above)
 */
PH7_PRIVATE int ph7_hashmap_product(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	ph7_value *pObj;
	if( nArg < 1 ){
		/* Missing arguments (arity is enforced upstream; defensive). */
		ph7_result_int(pCtx,1);
		return PH7_OK;
	}
	/* PHP 8: a non-array $array is a catchable TypeError, not a silent 0. */
	if( !ph7_value_is_array(apArg[0]) ){
		char zBuf[64];
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_product(): Argument #1 ($array) must be of type array, %s given",
			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))
			);
	}
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( pMap->nEntry < 1 ){
		/* The product of an empty array is the multiplicative identity 1 (PHP). */
		ph7_result_int(pCtx,1);
		return PH7_OK;
	}
	/* If the first element is of type float,then perform floating
	 * point computaion.Otherwise switch to int64 computaion.
	 */
	pObj = HashmapExtractNodeValue(pMap->pFirst);
	if( pObj == 0 ){
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	if( pObj->iFlags & MEMOBJ_REAL ){
		DoubleProd(pCtx,pMap);
	}else{
		Int64Prod(pCtx,pMap);
	}
	return PH7_OK;
}
/*
 * value array_rand(array $input[,int $num_req = 1 ])
 *  Pick one or more random entries out of an array.
 * Parameters
 * $input
 *  The input array.
 * $num_req
 *  Specifies how many entries you want to pick.
 * Return
 *  If you are picking only one entry, array_rand() returns the key for a random entry.
 *  Otherwise, it returns an array of keys for the random entries.
 *  NULL is returned on failure.
 */
PH7_PRIVATE int ph7_hashmap_rand(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pNode;
	ph7_hashmap *pMap;
	int nItem = 1;
	if( nArg < 1 ){
		/* Missing argument (arity is enforced upstream; defensive) */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* php 8: $array must be an array (TypeError, not a silent NULL return) */
	if( !ph7_value_is_array(apArg[0]) ){
		char zBuf[64];
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_rand(): Argument #1 ($array) must be of type array, %s given",
			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))
			);
	}
	/* php validates $num (and weak-coerces it) BEFORE the empty-array body
	 * check, matching its ZPP-before-body ordering. */
	if( nArg > 1 ){
		ph7_value *pNum = apArg[1];
		if( ph7_value_is_array(pNum) || ph7_value_is_object(pNum)
			|| ph7_value_is_resource(pNum) ){
			char zBuf[64];
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_rand(): Argument #2 ($num) must be of type int, %s given",
				VmValueGivenName(pNum,zBuf,sizeof(zBuf))
				);
		}
		if( ph7_value_is_string(pNum) ){
			/* Weak int coercion of a string $num follows php's numeric-string
			 * grammar (whole string, int or float): a non-numeric string
			 * (incl. leading-numeric junk like "2abc" or "0x1A") is a TypeError,
			 * a well-formed float-string ("1e3") coerces like a float value.
			 * Reuses the range() ZPP number parser (§3.9 shared-helper note). */
			int len;
			const char *zStr = ph7_value_to_string(pNum, &len);
			sxi64 iLong; double dReal;
			sxu8 iKind = RangeStrToNumber(zStr, (sxu32)len, &iLong, &dReal);
			if( iKind == RANGE_IN_ERROR ){
				return PH7_VmThrowException(pCtx,
					"TypeError",
					"array_rand(): Argument #2 ($num) must be of type int, string given"
					);
			}
			/* Clamp into a signed-int band so an absurd magnitude still yields
			 * the out-of-range ValueError below without an out-of-int cast. */
			if( iKind == RANGE_IN_DOUBLE ){
				iLong = dReal <= 0.0 ? 0 : (dReal >= 2147483647.0 ? 2147483647 : (sxi64)dReal);
			}
			if( iLong > 2147483647 ){ iLong = 2147483647; }
			else if( iLong < -2147483647 ){ iLong = -2147483647; }
			nItem = (int)iLong;
		}else{
			nItem = ph7_value_to_int(pNum);
		}
	}
	/* Point to the internal representation of the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	/* php 8: an empty array is a ValueError, not a NULL return */
	if( pMap->nEntry < 1 ){
		return PH7_VmThrowException(pCtx,
			"ValueError",
			"array_rand(): Argument #1 ($array) must not be empty"
			);
	}
	/* php 8: $num outside [1, count] is a ValueError, not a clamp/wrong value */
	if( nItem < 1 || nItem > (int)pMap->nEntry ){
		return PH7_VmThrowException(pCtx,
			"ValueError",
			"array_rand(): Argument #2 ($num) must be between 1 and the number of elements in argument #1 ($array)"
			);
	}
	if( nItem < 2 ){
		sxu32 nEntry;
		/* Pick a random slot through the MT19937 generator so array_rand()
		 * responds to srand()/mt_srand() (reproducible), like php. The exact
		 * index php lands on differs (php samples its internal hashtable
		 * buckets), so this is deterministic-under-seed but not value-parity. */
		nEntry = (sxu32)PH7_VmMtRandRange(pMap->pVm,0,(sxi64)pMap->nEntry - 1);
		/* Extract the desired entry.
		 * Note that we perform a linear lookup here (later version must change this)
		 */
		if( nEntry > pMap->nEntry / 2 ){
			pNode = pMap->pLast;
			nEntry = pMap->nEntry - nEntry;
			if( nEntry > 1 ){
				for(;;){
					if( nEntry == 0 ){
						break;
					}
					/* Point to the previous entry */
					pNode = pNode->pNext; /* Reverse link */
					nEntry--;
				}
			}
		}else{
			pNode = pMap->pFirst;
			for(;;){
				if( nEntry == 0 ){
					break;
				}
				/* Point to the next entry */
				pNode = pNode->pPrev; /* Reverse link */
				nEntry--;
			}
		}
		if( pNode->iType == HASHMAP_INT_NODE ){
			/* Int key */
			ph7_result_int64(pCtx,pNode->xKey.iKey);
		}else{
			/* Blob key */
			ph7_result_string(pCtx,(const char *)SyBlobData(&pNode->xKey.sKey),(int)SyBlobLength(&pNode->xKey.sKey));
		}
	}else{
		ph7_value sKey,*pArray;
		ph7_hashmap *pDest;
		/* Create a new array */
		pArray = ph7_context_new_array(pCtx);
		if( pArray == 0 ){
			ph7_result_null(pCtx);
			return PH7_OK;
		}
		/* Point to the internal representation of the hashmap */
		pDest = (ph7_hashmap *)pArray->x.pOther;
		PH7_MemObjInit(pDest->pVm,&sKey);
		/* Copy the first n items */
		pNode = pMap->pFirst;
		if( nItem > (int)pMap->nEntry ){
			nItem = (int)pMap->nEntry;
		}
		while( nItem > 0){
			PH7_HashmapExtractNodeKey(pNode,&sKey);
			PH7_HashmapInsert(pDest,0/* Automatic index assign*/,&sKey);
			PH7_MemObjRelease(&sKey);
			/* Point to the next entry */
			pNode = pNode->pPrev; /* Reverse link */
			nItem--;
		}
		/* Shuffle the array */
		HashmapMergeSort(pDest,HashmapCmpCallback7,0);
		/* Rehash node */
		HashmapSortRehash(pDest);
		/* Return the random array */
		ph7_result_value(pCtx,pArray);
	}
	return PH7_OK;
}
/*
 * array array_chunk (array $input,int $size [,bool $preserve_keys = false ])
 *  Split an array into chunks.
 * Parameters
 * $input
 *   The array to work on
 * $size
 *   The size of each chunk
 * $preserve_keys
 *   When set to TRUE keys will be preserved. Default is FALSE which will reindex
 *   the chunk numerically.
 * Return
 *  Returns a multidimensional numerically indexed array, starting with
 *  zero, with each dimension containing size elements.
 */
PH7_PRIVATE int ph7_hashmap_chunk(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray,*pChunk;
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pMap;
	int bPreserve;
	sxu32 nChunk;
	sxu32 nSize;
	sxu32 n;
	/* Argument count and types follow PHP semantics. */
	if( nArg < 2 ){
		/* fewer than required arguments -> ArgumentCountError */
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_chunk() expects at least 2 arguments, %d given",
			nArg
			);
	}
	if( !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_chunk(): Argument #1 ($array) must be of type array, %s given",
			ph7_type_name(apArg[0])
			);
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Extract and validate the chunk size argument. */
	/* Reject types that cannot be sensibly converted to an integer. */
	if( ph7_value_is_array(apArg[1]) || ph7_value_is_object(apArg[1]) ||
		ph7_value_is_resource(apArg[1]) || ph7_value_is_null(apArg[1]) ||
		ph7_value_is_bool(apArg[1]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_chunk(): Argument #2 ($length) must be of type int, %s given",
			ph7_type_name(apArg[1])
			);
	}
	/* Strings that are non-numeric produce a TypeError.  Numeric
	 * strings are permitted; however those representing floats lose
	 * precision and PHP emits a deprecation warning. */
	if( ph7_value_is_string(apArg[1]) ){
		int len;
		sxu8 bReal = FALSE;
		const char *zStr = ph7_value_to_string(apArg[1], &len);
		if( SyStrIsNumeric(zStr, len, &bReal, 0) != SXRET_OK ){
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_chunk(): Argument #2 ($length) must be of type int, string given"
				);
		}
		if( bReal ){
			/* float-string -> warn but allow */
			PH7_VmThrowException(pCtx,"TypeError",
				"Implicit conversion from float-string to int loses precision");
		}
	}
	/* If the value is a float with a fractional component, emit a
	 * deprecation warning but continue.  The following conversion occurs
	 * later via ph7_value_to_int. */
	if( ph7_value_is_float(apArg[1]) ){
		double d = ph7_value_to_double(apArg[1]);
		sxi64 i = (sxi64)d;
		if( d != (double)i ){
			PH7_VmThrowException(pCtx,"TypeError",
				"Implicit conversion from float to int loses precision");
		}
	}
	/* Convert using ph7_value_to_int; now that float fractions are
	 * eliminated, this will not produce a warning. */
	{
		sxi64 nSizeSigned = ph7_value_to_int(apArg[1]);
		if( nSizeSigned < 1 ){
			/* size <= 0 -> ValueError */
			return PH7_VmThrowException(pCtx,
				"ValueError",
				"array_chunk(): Argument #2 ($length) must be greater than 0"
				);
		}
		nSize = (sxu32)nSizeSigned;
	}
	if( nSize >= pMap->nEntry ){
		/* Return the whole array */
		ph7_array_add_elem(pArray,0,apArg[0]);
		ph7_result_value(pCtx,pArray);
		return PH7_OK;
	}
	bPreserve = 0;
	if( nArg > 2 ){
		/* The third argument has a bool type hint in PHP.  Values that
		 * cannot be sensibly converted (arrays, objects, resources) are
		 * rejected with a TypeError.  Scalars and null coerce to bool
		 * normally, matching PHP behaviour. */
		if( ph7_value_is_array(apArg[2]) ||
			ph7_value_is_object(apArg[2]) ||
			ph7_value_is_resource(apArg[2]) ){
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_chunk(): Argument #3 ($preserve_keys) must be of type bool, %s given",
				ph7_type_name(apArg[2])
				);
		}
		bPreserve = ph7_value_to_bool(apArg[2]);
	}
	/* Start processing */
	pEntry = pMap->pFirst;
	nChunk = 0;
	pChunk = 0;
	n = pMap->nEntry;
	for( ;; ){
		if( n < 1 ){
			/* When the loop terminates we may still have a current chunk
			 * that hasn't been added to the result array.  The previous
			 * implementation only pushed it if nChunk>0 which dropped the
			 * final chunk when the input size was an exact multiple of
			 * the chunk length.  Always append the pending chunk if it
			 * exists. */
			if( pChunk ){
				ph7_array_add_elem(pArray,0,pChunk); /* Will have its own copy */
			}
			break;
		}
		if( nChunk < 1 ){
			if( pChunk ){
				/* Put the first chunk */
				ph7_array_add_elem(pArray,0,pChunk); /* Will have it's own copy */
			}
			/* Create a new dimension */
			pChunk = ph7_context_new_array(pCtx); /* Don't worry about freeing memory here,everything
												   * will be automatically released as soon we return
												   * from this function */
			if( pChunk == 0 ){
				break;
			}
			nChunk = nSize;
		}
		/* Insert the entry */
		HashmapInsertNode((ph7_hashmap *)pChunk->x.pOther,pEntry,bPreserve);
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
		nChunk--;
		n--;
	}
	/* Return the multidimensional array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_pad(array $input,int $pad_size,value $pad_value)
 *  Pad array to the specified length with a value.
 * $input
 *   Initial array of values to pad.
 * $pad_size
 *   New size of the array.
 * $pad_value
 *   Value to pad if input is less than pad_size.
 */
/*
 * Shared "requested array size too large" guard (band A #8). php throws a
 * catchable ValueError when a builtin's caller-controlled target length
 * exceeds its hashtable capacity HT_MAX_SIZE (2^30 elements; probed against
 * php 8.5.7 — the boundary sits exactly between 1073741824 and 1073741825,
 * independent of the input array's size and symmetric for negative lengths).
 * Without this, a call like array_pad([1,2], 2000000000, 0) sits in the fill
 * loop for minutes and then OOMs. nRequested is the ABSOLUTE requested
 * length; pass a still-negative value (e.g. the unnegatable INT64_MIN,
 * mirroring php's ZEND_ABS overflow) to fail the guard unconditionally.
 * Returns SXRET_OK when the size is acceptable, else the throw status to
 * propagate. The cap constant is shared with range()'s guards
 * (PH7_RANGE_HT_MAX_SIZE above).
 */
static sxi32 HashmapGuardArraySize(
	ph7_context *pCtx,
	const char *zFunc,     /* Function name for the message */
	int iArg,              /* 1-based argument position */
	const char *zParam     /* "$length"-style parameter name */,
	sxi64 nRequested       /* Absolute requested element count */
	)
{
	if( nRequested < 0 || nRequested > PH7_RANGE_HT_MAX_SIZE ){
		return PH7_VmThrowException(pCtx,
			"ValueError",
			"%s(): Argument #%d (%s) must not exceed the maximum allowed array size",
			zFunc,iArg,zParam
			);
	}
	return SXRET_OK;
}
PH7_PRIVATE int ph7_hashmap_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	ph7_value *pArray;
	sxi64 iLen,iAbs;
	int nEntry;
	sxi32 rc;
	if( nArg != 3 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_pad() expects exactly 3 arguments, %d given",
			nArg
			);
	}
	if( !ph7_value_is_array(apArg[0]) ){
		char zBuf[64];
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_pad(): Argument #1 ($array) must be of type array, %s given",
			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))
			);
	}
	/* php 8: $length must be int-coercible. An array/object/resource or a
	 * non-numeric string throws a TypeError instead of silently padding to 0;
	 * a numeric string is weak-coerced via php's is_numeric_string grammar
	 * (reusing the shared RangeStrToNumber, like array_rand's $num). */
	if( ph7_value_is_array(apArg[1]) || ph7_value_is_object(apArg[1])
		|| ph7_value_is_resource(apArg[1]) ){
		char zBuf[64];
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_pad(): Argument #2 ($length) must be of type int, %s given",
			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf))
			);
	}
	if( ph7_value_is_string(apArg[1]) ){
		int nStr;
		const char *zStr = ph7_value_to_string(apArg[1],&nStr);
		sxi64 iLong; double dReal;
		sxu8 iKind = RangeStrToNumber(zStr,(sxu32)nStr,&iLong,&dReal);
		if( iKind == RANGE_IN_ERROR ){
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_pad(): Argument #2 ($length) must be of type int, string given"
				);
		}
		if( iKind == RANGE_IN_DOUBLE ){
			/* php ZPP: a float-string outside the int64 range (or NaN) fails
			 * outright — also keeps the (sxi64) cast below UB-free. */
			if( dReal != dReal || dReal >= 9223372036854775808.0 || dReal < -9223372036854775808.0 ){
				return PH7_VmThrowException(pCtx,
					"TypeError",
					"array_pad(): Argument #2 ($length) must be of type int, string given"
					);
			}
			iLen = (sxi64)dReal;
			if( (double)iLen != dReal ){
				return PH7_VmThrowException(pCtx,"TypeError",
					"array_pad(): Argument #2 ($length) must be of type int, string given");
			}
		}else{
			iLen = iLong;
		}
	}else{
		iLen = ph7_value_to_int64(apArg[1]);
	}
	/* Point to the internal representation of the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	/* php caps abs($length) at HT_MAX_SIZE either direction (INT64_MIN stays
	 * negative through the ABS, failing the guard like php's own ZEND_ABS
	 * overflow). */
	iAbs = iLen;
	if( iAbs < 0 && iAbs != (sxi64)-9223372036854775807LL - 1 ){
		iAbs = -iAbs;
	}
	rc = HashmapGuardArraySize(pCtx,"array_pad",2,"$length",iAbs);
	if( rc != SXRET_OK ){
		return rc;
	}
	nEntry = (int)iLen;
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( nEntry < 0 ){
		nEntry = -nEntry;
		if( nEntry > (int)pMap->nEntry ){
			nEntry -= (int)pMap->nEntry;
			/* Insert given items first */
			while( nEntry > 0 ){
				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){
					return PH7_ContextMemoryError(pCtx);
				}
				nEntry--;
			}
			/* Merge the two arrays */
			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);
		}else{
			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);
		}
	}else if( nEntry > 0 ){
		if( nEntry > (int)pMap->nEntry ){
			nEntry -= (int)pMap->nEntry;
			/* Merge the two arrays first */
			HashmapMerge(pMap,(ph7_hashmap *)pArray->x.pOther);
			/* Insert given items */
			while( nEntry > 0 ){
				if( ph7_array_add_elem(pArray,0,apArg[2]) != SXRET_OK ){
					return PH7_ContextMemoryError(pCtx);
				}
				nEntry--;
			}
		}else{
			PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);
		}
	}else{
		/* nEntry == 0: return a copy of the input array */
		PH7_HashmapDup(pMap,(ph7_hashmap *)pArray->x.pOther);
	}
	/* Return the new array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_replace(array &$array,array &$array1,...)
 *  Replaces elements from passed arrays into the first array.
 * Parameters
 * $array
 *   The array in which elements are replaced.
 * $array1
 *   The array from which elements will be extracted.
 * ....
 *  More arrays from which elements will be extracted.
 *  Values from later arrays overwrite the previous values.
 * Return
 *  Returns an array.
 *  Throws ArgumentCountError if no arguments are given.
 *  Throws TypeError if any argument is not an array.
 */
PH7_PRIVATE int ph7_hashmap_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	ph7_value *pArray;
	int i;
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_replace() expects at least 1 argument, 0 given"
			);
	}
	if( !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_replace(): Argument #1 ($array) must be of type array, %s given",
			ph7_type_name(apArg[0])
			);
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Overwrite from the first array */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);
	/* Perform the requested operation for remaining arrays */
	for( i = 1 ; i < nArg ; i++ ){
		if( !ph7_value_is_array(apArg[i]) ){
			/* Type mismatch -> TypeError */
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_replace(): Argument #%d must be of type array, %s given",
				i + 1,
				ph7_type_name(apArg[i])
				);
		}
		/* Point to the internal representation of the input hashmap */
		pMap = (ph7_hashmap *)apArg[i]->x.pOther;
		HashmapOverwrite(pMap,(ph7_hashmap *)pArray->x.pOther);
	}
	/* Return the new array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_filter(array $input [,callback $callback ])
 *  Filters elements of an array using a callback function.
 * Parameters
 *  $input
 *    The array to iterate over
 * $callback
 *    The callback function to use
 *    If no callback is supplied, all entries of input equal to FALSE (see converting to boolean)
 *    will be removed.
 * Return
 *  The filtered array.
 */
PH7_PRIVATE int ph7_hashmap_filter(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pMap;
	ph7_value *pArray;
	ph7_value sResult;   /* Callback result */
	ph7_value *pValue;
	sxi32 rc;
	int keep;
	sxu32 n;
	if( nArg < 1 ){
		/* Missing argument (arity is enforced upstream; defensive) */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* php 8: $array must be an array (TypeError, not a silent NULL return) */
	if( !ph7_value_is_array(apArg[0]) ){
		char zBuf[64];
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_filter(): Argument #1 ($array) must be of type array, %s given",
			VmValueGivenName(apArg[0],zBuf,sizeof(zBuf))
			);
	}
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Point to the internal representation of the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	pEntry = pMap->pFirst;
	PH7_MemObjInit(pMap->pVm,&sResult);
	sResult.nIdx = SXU32_HIGH; /* Mark as constant */
	/* Perform the requested operation */
	for( n = 0 ; n < pMap->nEntry ; n++ ){
		/* Extract node value (may be NULL if allocation failed) */
		pValue = HashmapExtractNodeValue(pEntry);
		if( pValue == 0 ){
			/* Can happen if SySetAt() failed earlier; drop the entry. */
			keep = FALSE;
		}else if( nArg > 1 && !ph7_value_is_null(apArg[1]) ){
			/* Callback was supplied (not NULL).  PHP 8 throws a
				* TypeError when the value is not callable or null; prior PH7
				* silently dropped the element.  Emit similar message. */
			if( !ph7_value_is_callable(apArg[1]) ){
				if( ph7_value_is_string(apArg[1]) ){
					int len;
					const char *zName = ph7_value_to_string(apArg[1], &len);
					return PH7_VmThrowException(pCtx,
						"TypeError",
						"array_filter(): Argument #2 ($callback) must be a valid callback or null, function \"%s\" not found or invalid function name",
						zName
						);
				}else{
					return PH7_VmThrowException(pCtx,
						"TypeError",
						"array_filter(): Argument #2 ($callback) must be a valid callback or null, %s given",
						ph7_type_name(apArg[1])
						);
				}
			}
			keep = FALSE;
			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],1,&pValue,&sResult);
			if( rc == PH7_EXCEPTION ){
				/* The callback raised: propagate so the dispatcher unwinds. */
				PH7_MemObjRelease(&sResult);
				return PH7_EXCEPTION;
			}
			if( rc == SXRET_OK ){
				/* Perform a boolean cast */
				keep = ph7_value_to_bool(&sResult);
			}
			PH7_MemObjRelease(&sResult);
		}else{
			/* No callback provided or callback explicitly NULL: use default
			 * behaviour where "empty" values are removed. This also covers
			 * the case where the callback argument is missing entirely.
			 */
			keep = !PH7_MemObjIsEmpty(pValue);
		}
		if( keep ){
			/* Perform the insertion,now the callback returned true */
			HashmapInsertNode((ph7_hashmap *)pArray->x.pOther,pEntry,TRUE);
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * array array_map(?callable $callback, array $array, array ...$arrays)
 *  Applies the callback to the elements of the given arrays.
 * Parameters
 *  $callback
 *   A callable to run for each element in each array, or NULL. With a single
 *   array and a NULL callback this is the identity function (the array is
 *   returned unchanged); with several arrays and a NULL callback the arrays
 *   are zipped together.
 *  $array
 *   The first array to run through the callback function.
 *  $arrays
 *   Zero or more additional arrays to process in parallel.
 * Return
 *  Returns an array containing the results of applying the callback function.
 *  With a single array the keys are preserved; with several arrays the result
 *  is re-indexed and the iteration runs to the length of the longest array,
 *  padding shorter arrays with NULL.
 */
PH7_PRIVATE int ph7_hashmap_map(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray,*pValue,sKey,sResult;
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pMap;
	ph7_vm *pVm;
	int bNullCallback;
	sxi32 rc;
	int i;
	sxu32 n;
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_map() expects at least 2 arguments, %d given",
			nArg
			);
	}
	bNullCallback = ph7_value_is_null(apArg[0]);
	if( !bNullCallback && !ph7_value_is_callable(apArg[0]) ){
		if( ph7_value_is_string(apArg[0]) ){
			const char *zFunc = ph7_value_to_string(apArg[0],0);
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_map(): Argument #1 ($callback) must be a valid callback or null, "
				"function \"%s\" not found or invalid function name",
				zFunc
				);
		}
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_map(): Argument #1 ($callback) must be a valid callback or null, "
			"no array or string given"
			);
	}
	/* Every remaining argument must be an array */
	for( i = 1 ; i < nArg ; i++ ){
		if( !ph7_value_is_array(apArg[i]) ){
			if( i == 1 ){
				return PH7_VmThrowException(pCtx,
					"TypeError",
					"array_map(): Argument #2 ($array) must be of type array, %s given",
					ph7_type_name(apArg[1])
					);
			}
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_map(): Argument #%d must be of type array, %s given",
				i+1,ph7_type_name(apArg[i])
				);
		}
	}
	pVm = pCtx->pVm;
	/* Create a new array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	PH7_MemObjInit(pVm,&sResult);
	PH7_MemObjInit(pVm,&sKey);
	sResult.nIdx = SXU32_HIGH; /* Mark as constant */
	sKey.nIdx    = SXU32_HIGH; /* Mark as constant */
	if( nArg == 2 ){
		/* Single-array mode: keys are preserved (PHP semantics). */
		pMap = (ph7_hashmap *)apArg[1]->x.pOther;
		pEntry = pMap->pFirst;
		for( n = 0 ; n < pMap->nEntry ; n++ ){
			/* Extract the node value */
			pValue = HashmapExtractNodeValue(pEntry);
			if( pValue ){
				/* Extract the node key */
				PH7_HashmapExtractNodeKey(pEntry,&sKey);
				if( bNullCallback ){
					/* NULL callback: identity function, keep original value */
					ph7_array_add_elem(pArray,&sKey,pValue);
				}else{
					/* Invoke the supplied callback */
					rc = PH7_VmCallUserFunction(pVm,apArg[0],1,&pValue,&sResult);
					if( rc == PH7_EXCEPTION ){
						/* Callback raised: abort and let the foreign-function
						 * dispatcher unwind through the nearest try/catch. */
						PH7_MemObjRelease(&sKey);
						PH7_MemObjRelease(&sResult);
						return PH7_EXCEPTION;
					}
					/* Insert the callback return value */
					ph7_array_add_elem(pArray,&sKey,&sResult);
				}
				PH7_MemObjRelease(&sKey);
				PH7_MemObjRelease(&sResult);
			}
			/* Point to the next entry */
			pEntry = pEntry->pPrev; /* Reverse link */
		}
	}else{
		/* Multi-array mode: walk every array in parallel to the length of the
		 * longest one, pad shorter arrays with NULL, and re-index the result. */
		int nArrays = nArg - 1;
		ph7_hashmap_node **apCur;
		ph7_value **apCallArg;
		ph7_value sNull;
		sxu32 nMax = 0;
		apCur     = (ph7_hashmap_node **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_hashmap_node *)));
		apCallArg = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,(sxu32)(nArrays*sizeof(ph7_value *)));
		if( apCur == 0 || apCallArg == 0 ){
			if( apCur ){ SyMemBackendFree(&pVm->sAllocator,apCur); }
			if( apCallArg ){ SyMemBackendFree(&pVm->sAllocator,apCallArg); }
			PH7_MemObjRelease(&sKey);
			PH7_MemObjRelease(&sResult);
			ph7_result_value(pCtx,pArray);
			return PH7_OK;
		}
		PH7_MemObjInit(pVm,&sNull); /* shared NULL pad for short arrays */
		sNull.nIdx = SXU32_HIGH;
		for( i = 0 ; i < nArrays ; i++ ){
			pMap = (ph7_hashmap *)apArg[i+1]->x.pOther;
			apCur[i] = pMap->pFirst;
			if( pMap->nEntry > nMax ){
				nMax = pMap->nEntry;
			}
		}
		for( n = 0 ; n < nMax ; n++ ){
			ph7_value *pZip = 0;
			if( bNullCallback ){
				/* zip: each result element is an array of the i-th values */
				pZip = ph7_context_new_array(pCtx);
			}
			for( i = 0 ; i < nArrays ; i++ ){
				ph7_value *pv = &sNull;
				if( apCur[i] ){
					ph7_value *pNodeVal = HashmapExtractNodeValue(apCur[i]);
					if( pNodeVal ){
						pv = pNodeVal;
					}
					apCur[i] = apCur[i]->pPrev; /* Reverse link */
				}
				if( bNullCallback ){
					if( pZip ){
						ph7_array_add_elem(pZip,0,pv);
					}
				}else{
					apCallArg[i] = pv;
				}
			}
			if( bNullCallback ){
				if( pZip ){
					ph7_array_add_elem(pArray,0,pZip);
				}
			}else{
				rc = PH7_VmCallUserFunction(pVm,apArg[0],nArrays,apCallArg,&sResult);
				if( rc == PH7_EXCEPTION ){
					SyMemBackendFree(&pVm->sAllocator,apCur);
					SyMemBackendFree(&pVm->sAllocator,apCallArg);
					PH7_MemObjRelease(&sNull);
					PH7_MemObjRelease(&sKey);
					PH7_MemObjRelease(&sResult);
					return PH7_EXCEPTION;
				}
				ph7_array_add_elem(pArray,0,&sResult);
				PH7_MemObjRelease(&sResult);
			}
		}
		SyMemBackendFree(&pVm->sAllocator,apCur);
		SyMemBackendFree(&pVm->sAllocator,apCallArg);
		PH7_MemObjRelease(&sNull);
	}
	PH7_MemObjRelease(&sKey);
	PH7_MemObjRelease(&sResult);
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * value array_reduce(array $array, callable $callback[, value $initial = NULL])
 *  Iteratively reduce the array to a single value using a callback function.
 * Parameters
 *  $array
 *   The input array.
 *  $callback
 *   The callback function. Signature: callback(mixed $carry, mixed $item): mixed
 *  $initial
 *   If the optional initial is available, it will be used at the beginning
 *   of the process, or as a final result in case the array is empty.
 * Return
 *  Returns the resulting value.
 *  If the array is empty and initial is not passed, array_reduce() returns NULL.
 */
PH7_PRIVATE int ph7_hashmap_reduce(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pMap;
	ph7_value *pValue;
	ph7_value sResult;
	sxi32 rc;
	sxu32 n;
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_reduce() expects at least 2 arguments, %d given",
			nArg
			);
	}
	if( nArg > 3 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_reduce() expects at most 3 arguments, %d given",
			nArg
			);
	}
	if( !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_reduce(): Argument #1 ($array) must be of type array, %s given",
			ph7_type_name(apArg[0])
			);
	}
	if( !ph7_value_is_callable(apArg[1]) ){
		if( ph7_value_is_string(apArg[1]) ){
			const char *zFunc = ph7_value_to_string(apArg[1],0);
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_reduce(): Argument #2 ($callback) must be a valid callback, "
				"function \"%s\" not found or invalid function name",
				zFunc
				);
		}
		if( ph7_value_is_array(apArg[1]) ){
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_reduce(): Argument #2 ($callback) must be a valid callback, "
				"array callback must have exactly two members"
				);
		}
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_reduce(): Argument #2 ($callback) must be a valid callback, "
			"no array or string given"
			);
	}
	/* Point to the internal representation of the input hashmap */
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Assume a NULL initial value */
	PH7_MemObjInit(pMap->pVm,&sResult);
	sResult.nIdx = SXU32_HIGH; /* Mark as constant */
	if( nArg > 2 ){
		/* Set the initial value */
		PH7_MemObjLoad(apArg[2],&sResult);
	}
	/* Perform the requested operation */
	pEntry = pMap->pFirst;
	for( n = 0 ; n < pMap->nEntry ; n++ ){
		/* Extract the node value */
		pValue = HashmapExtractNodeValue(pEntry);
		/* Invoke the supplied callback */
		rc = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],&sResult,&sResult,pValue,0);
		if( rc == PH7_EXCEPTION ){
			/* The callback raised: propagate so the dispatcher unwinds. */
			PH7_MemObjRelease(&sResult);
			return PH7_EXCEPTION;
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	ph7_result_value(pCtx,&sResult); /* Will make it's own copy */
	PH7_MemObjRelease(&sResult);
	return PH7_OK;
}
/*
 * bool array_walk(array &$array, callback $funcname [, mixed $userdata])
 *  Apply a user function to every member of an array.
 * Parameters
 *  $array
 *   The input array.
 *  $funcname
 *   Typically, funcname takes on two parameters. The array parameter's value being
 *   the first, and the key/index second.
 * Note:
 *  If funcname needs to be working with the actual values of the array, specify the first
 *  parameter of funcname as a reference. Then, any changes made to those elements will
 *  be made in the original array itself.
 *  $userdata
 *   If the optional userdata parameter is supplied, it will be passed as the third parameter
 *   to the callback funcname.
 * Return
 *  Returns TRUE on success or FALSE on failure.
 */
PH7_PRIVATE int ph7_hashmap_walk(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pValue,*pUserData,sKey;
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pMap;
	sxu32 n;
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_walk() expects at least 2 arguments, %d given",
			nArg
			);
	}
	if( nArg > 3 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_walk() expects at most 3 arguments, %d given",
			nArg
			);
	}
	if( !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_walk(): Argument #1 ($array) must be of type array, %s given",
			ph7_type_name(apArg[0])
			);
	}
	if( !ph7_value_is_callable(apArg[1]) ){
		if( ph7_value_is_string(apArg[1]) ){
			const char *zFunc = ph7_value_to_string(apArg[1],0);
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_walk(): Argument #2 ($callback) must be a valid callback, "
				"function \"%s\" not found or invalid function name",
				zFunc
				);
		}
		if( ph7_value_is_array(apArg[1]) ){
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_walk(): Argument #2 ($callback) must be a valid callback, "
				"array callback must have exactly two members"
				);
		}
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_walk(): Argument #2 ($callback) must be a valid callback, "
			"no array or string given"
			);
	}
	pUserData = nArg > 2 ? apArg[2] : 0;
	/* Point to the internal representation of the input hashmap */
	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	PH7_MemObjInit(pMap->pVm,&sKey);
	sKey.nIdx = SXU32_HIGH; /* Mark as constant */
	/* Perform the desired operation */
	pEntry = pMap->pFirst;
	for( n = 0 ; n < pMap->nEntry ; n++ ){
		/* Extract the node value */
		pValue = HashmapExtractNodeValue(pEntry);
		if( pValue ){
			sxi32 rcW;
			/* Extract the entry key */
			PH7_HashmapExtractNodeKey(pEntry,&sKey);
			/* Invoke the supplied callback */
			rcW = PH7_VmCallUserFunctionAp(pMap->pVm,apArg[1],0,pValue,&sKey,pUserData,0);
			PH7_MemObjRelease(&sKey);
			if( rcW == PH7_EXCEPTION ){
				/* The callback raised: propagate so the dispatcher unwinds. */
				return PH7_EXCEPTION;
			}
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	/* All done, return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * Apply a user function to every member of an array.(Recurse on array's).
 * Refer to the [array_walk_recursive()] implementation for more information.
 */
static sxi32 HashmapWalkRecursive(
	ph7_hashmap *pMap,    /* Target hashmap */
	ph7_value *pCallback, /* User callback */
	ph7_value *pUserData, /* Callback private data */
	int iNest             /* Nesting level */
	)
{
	ph7_hashmap_node *pEntry;
	ph7_value *pValue,sKey;
	sxi32 rc;
	sxu32 n;
	/* Iterate through hashmap entries */
	PH7_MemObjInit(pMap->pVm,&sKey);
	sKey.nIdx = SXU32_HIGH; /* Mark as constant */
	pEntry = pMap->pFirst;
	for( n = 0 ; n < pMap->nEntry ; n++ ){
		/* Extract the node value */
		pValue = HashmapExtractNodeValue(pEntry);
		if( pValue ){
			if( pValue->iFlags & MEMOBJ_HASHMAP ){
				if( iNest < 32 ){
					/* Recurse */
					iNest++;
					rc = HashmapWalkRecursive((ph7_hashmap *)pValue->x.pOther,pCallback,pUserData,iNest);
					iNest--;
					if( rc == PH7_EXCEPTION ){
						return PH7_EXCEPTION;
					}
				}
			}else{
				/* Extract the node key */
				PH7_HashmapExtractNodeKey(pEntry,&sKey);
				/* Invoke the supplied callback */
				rc = PH7_VmCallUserFunctionAp(pMap->pVm,pCallback,0,pValue,&sKey,pUserData,0);
				PH7_MemObjRelease(&sKey);
				if( rc == PH7_EXCEPTION ){
					/* The callback raised: propagate so the dispatcher unwinds. */
					return PH7_EXCEPTION;
				}
			}
		}
		/* Point to the next entry */
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	return PH7_OK;
}
/*
 * bool array_walk_recursive(array &$array, callback $funcname [, mixed $userdata])
 *  Apply a user function recursively to every member of an array.
 * Parameters
 *  $array
 *   The input array.
 *  $funcname
 *   Typically, funcname takes on two parameters. The array parameter's value being
 *   the first, and the key/index second.
 * Note:
 *  If funcname needs to be working with the actual values of the array, specify the first
 *  parameter of funcname as a reference. Then, any changes made to those elements will
 *  be made in the original array itself.
 *  $userdata
 *   If the optional userdata parameter is supplied, it will be passed as the third parameter
 *   to the callback funcname.
 * Return
 *  Returns TRUE on success or FALSE on failure.
 */
PH7_PRIVATE int ph7_hashmap_walk_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap *pMap;
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_walk_recursive() expects at least 2 arguments, %d given",
			nArg
			);
	}
	if( nArg > 3 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_walk_recursive() expects at most 3 arguments, %d given",
			nArg
			);
	}
	if( !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_walk_recursive(): Argument #1 ($array) must be of type array, %s given",
			ph7_type_name(apArg[0])
			);
	}
	if( !ph7_value_is_callable(apArg[1]) ){
		if( ph7_value_is_string(apArg[1]) ){
			const char *zFunc = ph7_value_to_string(apArg[1],0);
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "
				"function \"%s\" not found or invalid function name",
				zFunc
				);
		}
		if( ph7_value_is_array(apArg[1]) ){
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "
				"array callback must have exactly two members"
				);
		}
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_walk_recursive(): Argument #2 ($callback) must be a valid callback, "
			"no array or string given"
			);
	}
	/* Point to the internal representation of the input hashmap */
	PH7_HashmapCowSeparate(pCtx->pVm, apArg[0]);
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	/* Perform the desired operation */
	if( HashmapWalkRecursive(pMap,apArg[1],nArg > 2 ? apArg[2] : 0,0) == PH7_EXCEPTION ){
		/* A callback raised: propagate so the dispatcher unwinds. */
		return PH7_EXCEPTION;
	}
	/* All done, return TRUE */
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * bool array_is_list(array $array)
 *  Checks whether a given array is a list: its keys consist of consecutive
 *  integers starting at 0. An empty array is a list.
 * Return
 *  TRUE if the array is a list, FALSE otherwise.
 */
/*
 * Return TRUE if the given hashmap is a "list" [i.e: its keys are the
 * consecutive integers 0,1,2,... with no gaps]. An empty map is a list.
 * Shared by array_is_list() and the JSON encoder (vm_json.c).
 */
PH7_PRIVATE int PH7_HashmapIsList(ph7_hashmap *pMap)
{
	ph7_hashmap_node *pNode = pMap->pFirst;
	sxi64 iExpect = 0;
	sxu32 n;
	for( n = 0 ; n < pMap->nEntry ; ++n ){
		if( pNode->iType != HASHMAP_INT_NODE || pNode->xKey.iKey != iExpect ){
			/* A non-integer key or a gap in the sequence: not a list */
			return 0;
		}
		++iExpect;
		pNode = pNode->pPrev; /* Reverse link */
	}
	return 1;
}
PH7_PRIVATE int ph7_hashmap_is_list(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_is_list() expects exactly 1 argument, 0 given"
			);
	}
	if( !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_is_list(): Argument #1 ($array) must be of type array, %s given",
			ph7_type_name(apArg[0])
			);
	}
	ph7_result_bool(pCtx,PH7_HashmapIsList((ph7_hashmap *)apArg[0]->x.pOther));
	return PH7_OK;
}
/*
 * mixed array_first(array $array)
 * mixed array_last(array $array)
 *  Return the value of the first (respectively last) element of the array,
 *  or NULL when the array is empty. The internal array pointer is left
 *  untouched (unlike reset()/end()).
 */
static int HashmapFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)
{
	ph7_hashmap *pMap;
	ph7_hashmap_node *pNode;
	ph7_value *pVal;
	const char *zName = bLast ? "array_last" : "array_first";
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"%s() expects exactly 1 argument, 0 given",
			zName
			);
	}
	if( !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"%s(): Argument #1 ($array) must be of type array, %s given",
			zName,
			ph7_type_name(apArg[0])
			);
	}
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	pNode = bLast ? pMap->pLast : pMap->pFirst;
	if( pNode == 0 ){
		/* Empty array: PHP returns NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pVal = HashmapExtractNodeValue(pNode);
	if( pVal ){
		ph7_result_value(pCtx,pVal);
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
PH7_PRIVATE int ph7_hashmap_first(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return HashmapFirstLast(pCtx,nArg,apArg,0);
}
PH7_PRIVATE int ph7_hashmap_last(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return HashmapFirstLast(pCtx,nArg,apArg,1);
}
/*
 * int|string|null array_key_first(array $array)
 * int|string|null array_key_last(array $array)
 *  Return the key of the first (respectively last) element of the array,
 *  or NULL when the array is empty. The internal array pointer is left
 *  untouched.
 */
static int HashmapKeyFirstLast(ph7_context *pCtx,int nArg,ph7_value **apArg,int bLast)
{
	ph7_hashmap *pMap;
	ph7_hashmap_node *pNode;
	const char *zName = bLast ? "array_key_last" : "array_key_first";
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"%s() expects exactly 1 argument, 0 given",
			zName
			);
	}
	if( !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"%s(): Argument #1 ($array) must be of type array, %s given",
			zName,
			ph7_type_name(apArg[0])
			);
	}
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	pNode = bLast ? pMap->pLast : pMap->pFirst;
	if( pNode == 0 ){
		/* Empty array: PHP returns NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	HashmapResultNodeKey(pCtx,pNode);
	return PH7_OK;
}
PH7_PRIVATE int ph7_hashmap_key_first(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return HashmapKeyFirstLast(pCtx,nArg,apArg,0);
}
PH7_PRIVATE int ph7_hashmap_key_last(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return HashmapKeyFirstLast(pCtx,nArg,apArg,1);
}
/*
 * Fetch the element identified by 'pKey' from 'pRow' which may be either an
 * array (hashmap lookup) or an object (public attribute lookup). Used by
 * array_column() for both the column value and the index key.
 * Returns a borrowed pointer to the value, or NULL when the row is not a
 * container or the key is absent.
 */
static ph7_value * HashmapColumnFetch(ph7_vm *pVm,ph7_value *pRow,ph7_value *pKey)
{
	if( ph7_value_is_array(pRow) ){
		ph7_hashmap_node *pNode;
		if( PH7_HashmapLookup((ph7_hashmap *)pRow->x.pOther,pKey,&pNode) == SXRET_OK ){
			return HashmapExtractNodeValue(pNode);
		}
	}else if( ph7_value_is_object(pRow) ){
		ph7_value sName;
		const char *zName;
		ph7_value *pAttr;
		/* Stringify a *copy* of the key (objects address attributes by name);
		 * never mutate pKey itself or the array-lookup path would break. */
		PH7_MemObjInit(pVm,&sName);
		PH7_MemObjStore(pKey,&sName);
		zName = ph7_value_to_string(&sName,0); /* NUL-terminated */
		pAttr = ph7_object_fetch_attr(pRow,zName);
		PH7_MemObjRelease(&sName);
		return pAttr;
	}
	return 0;
}
/*
 * array array_column(array $array, int|string|null $column_key, int|string|null $index_key = null)
 *  Returns the values from a single column of the input, identified by
 *  $column_key. Optionally indexes the result by the $index_key column.
 *  A NULL $column_key collects the whole row. Rows missing the column are
 *  skipped; rows missing the index key are appended with a numeric key.
 *  Each row may be an array or an object.
 */
PH7_PRIVATE int ph7_hashmap_column(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pNode;
	ph7_hashmap *pMap;
	ph7_value *pArray;
	ph7_value *pRow;
	ph7_value *pCol;
	ph7_value *pIdx;
	int bWantCol;
	int bWantIdx;
	sxu32 n;
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"array_column() expects at least 2 arguments, %d given",
			nArg
			);
	}
	if( !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"array_column(): Argument #1 ($array) must be of type array, %s given",
			ph7_type_name(apArg[0])
			);
	}
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* A NULL column_key means "collect the entire row". */
	bWantCol = !ph7_value_is_null(apArg[1]);
	bWantIdx = (nArg > 2 && !ph7_value_is_null(apArg[2]));
	pNode = pMap->pFirst;
	for( n = 0 ; n < pMap->nEntry ; ++n ){
		pRow = HashmapExtractNodeValue(pNode);
		pNode = pNode->pPrev; /* Advance now so 'continue' is safe */
		if( pRow == 0 ){
			continue;
		}
		if( bWantCol ){
			pCol = HashmapColumnFetch(pMap->pVm,pRow,apArg[1]);
			if( pCol == 0 ){
				/* Row lacks the requested column: skip it (PHP semantics). */
				continue;
			}
		}else{
			pCol = pRow;
		}
		pIdx = bWantIdx ? HashmapColumnFetch(pMap->pVm,pRow,apArg[2]) : 0;
		if( pIdx ){
			ph7_array_add_elem(pArray,pIdx,pCol);
		}else{
			ph7_array_add_elem(pArray,0,pCol); /* Auto-index */
		}
	}
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * Shared core for array_find/array_find_key/array_any/array_all (PHP 8.4).
 * Invokes $callback($value, $key) over each entry and reports the first node
 * whose truthiness equals 'bWant'. Propagates a callback exception as
 * PH7_EXCEPTION; sets *ppMatch to the matching node (or NULL if none).
 */
static sxi32 HashmapCallbackSearch(
	ph7_context *pCtx,int nArg,ph7_value **apArg,
	const char *zName,            /* Function name for diagnostics */
	int bWant,                    /* Truthiness being hunted for */
	ph7_hashmap_node **ppMatch    /* OUT: first matching node or NULL */
	)
{
	ph7_hashmap_node *pEntry;
	ph7_hashmap *pMap;
	ph7_value *pValue;
	ph7_value *apCbArg[2];
	ph7_value sKey;
	ph7_value sResult;
	sxi32 rc;
	sxu32 n;
	*ppMatch = 0;
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"%s() expects exactly 2 arguments, %d given",
			zName,nArg
			);
	}
	if( !ph7_value_is_array(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"%s(): Argument #1 ($array) must be of type array, %s given",
			zName,ph7_type_name(apArg[0])
			);
	}
	if( !ph7_value_is_callable(apArg[1]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"%s(): Argument #2 ($callback) must be a valid callback, %s given",
			zName,ph7_type_name(apArg[1])
			);
	}
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	pEntry = pMap->pFirst;
	PH7_MemObjInit(pMap->pVm,&sKey);
	sKey.nIdx = SXU32_HIGH;    /* Mark as constant */
	PH7_MemObjInit(pMap->pVm,&sResult);
	sResult.nIdx = SXU32_HIGH; /* Mark as constant */
	for( n = 0 ; n < pMap->nEntry ; ++n ){
		pValue = HashmapExtractNodeValue(pEntry);
		if( pValue ){
			/* The callback receives ($value, $key). */
			PH7_HashmapExtractNodeKey(pEntry,&sKey);
			apCbArg[0] = pValue;
			apCbArg[1] = &sKey;
			rc = PH7_VmCallUserFunction(pMap->pVm,apArg[1],2,apCbArg,&sResult);
			if( rc == PH7_EXCEPTION ){
				/* The callback raised: propagate so the dispatcher unwinds. */
				PH7_MemObjRelease(&sKey);
				PH7_MemObjRelease(&sResult);
				return PH7_EXCEPTION;
			}
			if( rc == SXRET_OK && (ph7_value_to_bool(&sResult) ? 1 : 0) == bWant ){
				*ppMatch = pEntry;
				break;
			}
		}
		pEntry = pEntry->pPrev; /* Reverse link */
	}
	PH7_MemObjRelease(&sKey);
	PH7_MemObjRelease(&sResult);
	return PH7_OK;
}
/*
 * mixed array_find(array $array, callable $callback)
 *  Returns the value of the first element for which $callback($value,$key)
 *  is truthy, or NULL if none match.
 */
PH7_PRIVATE int ph7_hashmap_find(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pMatch;
	ph7_value *pVal;
	sxi32 rc;
	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find",1,&pMatch);
	if( rc != PH7_OK ){
		return rc;
	}
	if( pMatch && (pVal = HashmapExtractNodeValue(pMatch)) != 0 ){
		ph7_result_value(pCtx,pVal);
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
/*
 * mixed array_find_key(array $array, callable $callback)
 *  Returns the key of the first element for which $callback($value,$key)
 *  is truthy, or NULL if none match.
 */
PH7_PRIVATE int ph7_hashmap_find_key(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pMatch;
	sxi32 rc;
	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_find_key",1,&pMatch);
	if( rc != PH7_OK ){
		return rc;
	}
	if( pMatch == 0 ){
		ph7_result_null(pCtx);
	}else if( pMatch->iType == HASHMAP_INT_NODE ){
		ph7_result_int64(pCtx,pMatch->xKey.iKey);
	}else{
		ph7_result_string(pCtx,
			(const char *)SyBlobData(&pMatch->xKey.sKey),
			(int)SyBlobLength(&pMatch->xKey.sKey));
	}
	return PH7_OK;
}
/*
 * bool array_any(array $array, callable $callback)
 *  Returns TRUE if $callback($value,$key) is truthy for at least one element.
 *  FALSE for an empty array.
 */
PH7_PRIVATE int ph7_hashmap_any(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pMatch;
	sxi32 rc;
	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_any",1,&pMatch);
	if( rc != PH7_OK ){
		return rc;
	}
	ph7_result_bool(pCtx,pMatch != 0);
	return PH7_OK;
}
/*
 * bool array_all(array $array, callable $callback)
 *  Returns TRUE if $callback($value,$key) is truthy for every element (and for
 *  an empty array). Hunts for the first falsy element: its absence means "all".
 */
PH7_PRIVATE int ph7_hashmap_all(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_hashmap_node *pMatch;
	sxi32 rc;
	rc = HashmapCallbackSearch(pCtx,nArg,apArg,"array_all",0,&pMatch);
	if( rc != PH7_OK ){
		return rc;
	}
	ph7_result_bool(pCtx,pMatch == 0);
	return PH7_OK;
}
/*
 * The iterator_*() family — walk a Traversable via the shared PH7_VmIteratorWalk
 * helper (the reusable form of the foreach Iterator protocol).
 */
/* Step shared by iterator_to_array (pArray set) and iterator_count (pArray NULL). */
struct IterCollect { ph7_value *pArray; int bPreserve; sxi64 nCount; };
static sxi32 IterCollectStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)
{
	struct IterCollect *p = (struct IterCollect *)pUserData;
	(void)pVm;
	p->nCount++;
	if( p->pArray ){
		/* preserve_keys: insert with the iterator key (later wins on collision);
		 * otherwise append with an auto-assigned int index. */
		ph7_array_add_elem(p->pArray, p->bPreserve ? pKey : 0, pValue);
	}
	return SXRET_OK;
}
/*
 * array iterator_to_array(Traversable|array $iterator, bool $preserve_keys = true)
 */
PH7_PRIVATE int ph7_iterator_to_array(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	struct IterCollect sCol;
	ph7_value *pArray;
	sxi32 rc;
	if( nArg < 1 ){ ph7_result_null(pCtx); return PH7_OK; }
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){ ph7_result_null(pCtx); return PH7_OK; }
	sCol.pArray = pArray;
	sCol.bPreserve = (nArg > 1) ? ph7_value_to_bool(apArg[1]) : 1;
	sCol.nCount = 0;
	if( ph7_value_is_array(apArg[0]) ){
		/* PHP 8.2 accepts a plain array: copy it (preserving or renumbering keys). */
		ph7_hashmap *pMap = (ph7_hashmap *)apArg[0]->x.pOther;
		ph7_hashmap_node *pEntry = pMap->pFirst;
		sxu32 n;
		for( n = 0 ; n < pMap->nEntry ; n++ ){
			ph7_value sKey, *pVal;
			PH7_MemObjInit(pCtx->pVm,&sKey);
			PH7_HashmapExtractNodeKey(pEntry,&sKey);
			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pEntry->nValIdx);
			if( pVal ){ ph7_array_add_elem(pArray, sCol.bPreserve ? &sKey : 0, pVal); }
			PH7_MemObjRelease(&sKey);
			pEntry = pEntry->pPrev;
		}
		ph7_result_value(pCtx,pArray);
		return PH7_OK;
	}
	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);
	if( rc == PH7_EXCEPTION || rc == PH7_ABORT ){ return rc; }
	if( rc == SXERR_NOTIMPLEMENTED ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"iterator_to_array(): Argument #1 ($iterator) must be of type Traversable|array, %s given",
			ph7_type_name(apArg[0]));
	}
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * int iterator_count(Traversable|array $iterator)
 */
PH7_PRIVATE int ph7_iterator_count(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	struct IterCollect sCol;
	sxi32 rc;
	if( nArg < 1 ){ ph7_result_int(pCtx,0); return PH7_OK; }
	if( ph7_value_is_array(apArg[0]) ){
		ph7_result_int64(pCtx, (ph7_int64)((ph7_hashmap *)apArg[0]->x.pOther)->nEntry);
		return PH7_OK;
	}
	sCol.pArray = 0; sCol.bPreserve = 0; sCol.nCount = 0;
	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterCollectStep, &sCol);
	if( rc == PH7_EXCEPTION || rc == PH7_ABORT ){ return rc; }
	if( rc == SXERR_NOTIMPLEMENTED ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"iterator_count(): Argument #1 ($iterator) must be of type Traversable|array, %s given",
			ph7_type_name(apArg[0]));
	}
	ph7_result_int64(pCtx, sCol.nCount);
	return PH7_OK;
}
/* iterator_apply step: call the fixed callback with $args each iteration. The
 * arg pointers are resolved fresh per step because the iterator's own methods
 * run user code between iterations and may reallocate the aMemObj pool. */
struct IterApply { ph7_value *pCallback; ph7_value *pArgsArray; sxi64 nCount; };
static sxi32 IterApplyStep(ph7_vm *pVm, ph7_value *pKey, ph7_value *pValue, void *pUserData)
{
	struct IterApply *p = (struct IterApply *)pUserData;
	ph7_value sResult;
	SySet aArg;
	sxi32 rc;
	int bContinue;
	(void)pKey; (void)pValue; /* iterator_apply does NOT pass the element to the callback */
	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));
	if( p->pArgsArray && (p->pArgsArray->iFlags & MEMOBJ_HASHMAP) ){
		ph7_hashmap *pMap = (ph7_hashmap *)p->pArgsArray->x.pOther;
		ph7_hashmap_node *pEntry = pMap->pFirst;
		sxu32 n;
		for( n = 0 ; n < pMap->nEntry ; n++ ){
			ph7_value *pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pEntry->nValIdx);
			if( pVal ){ SySetPut(&aArg,(const void *)&pVal); }
			pEntry = pEntry->pPrev;
		}
	}
	PH7_MemObjInit(pVm,&sResult);
	rc = PH7_VmCallUserFunction(pVm, p->pCallback, (int)SySetUsed(&aArg),
		(ph7_value **)SySetBasePtr(&aArg), &sResult);
	SySetRelease(&aArg);
	if( rc == PH7_EXCEPTION || rc == PH7_ABORT ){ PH7_MemObjRelease(&sResult); return rc; }
	p->nCount++;
	PH7_MemObjToBool(&sResult);
	bContinue = (sResult.x.iVal != 0);
	PH7_MemObjRelease(&sResult);
	return bContinue ? SXRET_OK : SXERR_EOF; /* falsy return stops iteration */
}
/*
 * int iterator_apply(Traversable $iterator, callable $callback, array $args = [])
 */
PH7_PRIVATE int ph7_iterator_apply(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	struct IterApply sApp;
	sxi32 rc;
	if( nArg < 2 ){ ph7_result_int(pCtx,0); return PH7_OK; }
	if( !ph7_value_is_callable(apArg[1]) ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"iterator_apply(): Argument #2 ($callback) must be a valid callback");
	}
	sApp.pCallback = apArg[1];
	sApp.pArgsArray = (nArg > 2 && ph7_value_is_array(apArg[2])) ? apArg[2] : 0;
	sApp.nCount = 0;
	rc = PH7_VmIteratorWalk(pCtx->pVm, apArg[0], IterApplyStep, &sApp);
	if( rc == PH7_EXCEPTION || rc == PH7_ABORT ){ return rc; }
	if( rc == SXERR_NOTIMPLEMENTED ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"iterator_apply(): Argument #1 ($iterator) must be of type Traversable, %s given",
			ph7_type_name(apArg[0]));
	}
	ph7_result_int64(pCtx, sApp.nCount);
	return PH7_OK;
}
