/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/* filter_var(FILTER_VALIDATE_FLOAT) parses with libc strtod directly because it
 * needs errno==ERANGE to reject out-of-range magnitudes; SyStrToReal (also
 * strtod-backed nowadays) exposes no range-error signal. */
#include <stdlib.h>  /* strtod */
#include <math.h>    /* HUGE_VAL */
#include <errno.h>   /* ERANGE (strtod range-error signal) */
#include <stdio.h>   /* snprintf (printf-family float conversions — correctly
                      * rounded digits like php's zend_dtoa; see PH7_InputFormat) */
/* Shared ZPP helper for `int` parameters — defined OUTSIDE the
 * PH7_DISABLE_BUILTIN_FUNC guard because hashmap.c (array_slice) and
 * builtin_math.c (intdiv) call it and both compile in the tiny build. */
PH7_PRIVATE sxi32 PH7_IntArgResolve(
	ph7_context *pCtx,
	ph7_value *pArg,
	const char *zFunc,
	int iArgNum,
	const char *zParamName,
	const char *zTypeStr,
	sxi64 *pOut
){
	if( ph7_value_is_null(pArg) ){
		PH7_VmThrowDeprecatedFmt(pCtx->pVm,
			"%s(): Passing null to parameter #%d (%s) of type %s is deprecated",
			zFunc,iArgNum,zParamName,zTypeStr
			);
		*pOut = 0;
		return PH7_OK;
	}
	if( ph7_value_is_float(pArg) ){
		double dVal = ph7_value_to_double(pArg);
		sxi64 iVal;
		/* php: NAN/INF/out-of-int64-range floats fail ZPP outright */
		if( dVal != dVal || dVal >= 9223372036854775808.0 || dVal < -9223372036854775808.0 ){
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"%s(): Argument #%d (%s) must be of type %s, float given",
				zFunc,iArgNum,zParamName,zTypeStr
				);
		}
		iVal = (sxi64)dVal;
		if( (double)iVal != dVal ){
			PH7_VmThrowDeprecatedFmt(pCtx->pVm,
				"Implicit conversion from float %s to int loses precision",
				ph7_value_to_string(pArg,0)
				);
		}
		*pOut = iVal;
		return PH7_OK;
	}
	if( ph7_value_is_string(pArg) ){
		const char *zNum;
		int nSlen;
		int i,bFloat = 0;
		if( !PH7_MemObjStringIsNumeric(pArg) ){
			return PH7_VmThrowException(pCtx,
				"TypeError",
				"%s(): Argument #%d (%s) must be of type %s, string given",
				zFunc,iArgNum,zParamName,zTypeStr
				);
		}
		zNum = ph7_value_to_string(pArg,&nSlen);
		for( i = 0 ; i < nSlen ; i++ ){
			if( zNum[i] == '.' || zNum[i] == 'e' || zNum[i] == 'E' ){
				bFloat = 1;
				break;
			}
		}
		if( bFloat ){
			double dVal = 0;
			sxi64 iVal;
			SyStrToReal(zNum,(sxu32)nSlen,(void *)&dVal,0);
			if( dVal != dVal || dVal >= 9223372036854775808.0 || dVal < -9223372036854775808.0 ){
				return PH7_VmThrowException(pCtx,
					"TypeError",
					"%s(): Argument #%d (%s) must be of type %s, string given",
					zFunc,iArgNum,zParamName,zTypeStr
					);
			}
			iVal = (sxi64)dVal;
			if( (double)iVal != dVal ){
				PH7_VmThrowDeprecatedFmt(pCtx->pVm,
					"Implicit conversion from float-string \"%s\" to int loses precision",
					zNum
					);
			}
			*pOut = iVal;
			return PH7_OK;
		}
		*pOut = ph7_value_to_int64(pArg);
		return PH7_OK;
	}
	if( !ph7_value_is_int(pArg) && !ph7_value_is_bool(pArg) ){
		/* Arrays, resources and objects: php names the class for objects */
		const char *zType = ph7_type_name(pArg);
		if( ph7_value_is_object(pArg) ){
			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;
			if( pInst && pInst->pClass ){
				zType = SyStringData(&pInst->pClass->sName);
			}
		}
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"%s(): Argument #%d (%s) must be of type %s, %s given",
			zFunc,iArgNum,zParamName,zTypeStr,zType
			);
	}
	*pOut = ph7_value_to_int64(pArg);
	return PH7_OK;
}

#ifndef PH7_DISABLE_BUILTIN_FUNC
/* Forward decl: null-to-string ZPP deprecation notice (defined near the ZPP
 * helpers; both live inside the same DISABLE_BUILTIN_FUNC region as every
 * caller — the tiny build compiles none of them). */
static void StrNullArgNotice(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNum,const char *zParamName);
#endif /* PH7_DISABLE_BUILTIN_FUNC */
/* This file implement built-in 'foreign' functions for the PH7 engine */
/*
 * Section:
 *    Variable handling Functions.
 * Status:
 *    Stable.
 */
/*
 * bool is_bool($var)
 *  Finds out whether a variable is a boolean.
 * Parameters
 *   $var: The variable being evaluated.
 * Return
 *  TRUE if var is a boolean. False otherwise.
 */
static int PH7_builtin_is_bool(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume false by default */
	if( nArg > 0 ){
		res = ph7_value_is_bool(apArg[0]);
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool is_float($var)
 * bool is_real($var)
 * bool is_double($var)
 *  Finds out whether a variable is a float.
 * Parameters
 *   $var: The variable being evaluated.
 * Return
 *  TRUE if var is a float. False otherwise.
 */
static int PH7_builtin_is_float(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume false by default */
	if( nArg > 0 ){
		res = ph7_value_is_float(apArg[0]);
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool is_int($var)
 * bool is_integer($var)
 * bool is_long($var)
 *  Finds out whether a variable is an integer.
 * Parameters
 *   $var: The variable being evaluated.
 * Return
 *  TRUE if var is an integer. False otherwise.
 */
static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume false by default */
	if( nArg > 0 ){
		/* Strict PHP identity: a float is never an int, even when it holds an
		 * integer value (1.0). An integer-valued real carries both MEMOBJ_INT
		 * (cached) and MEMOBJ_REAL, so REAL must be excluded here. */
		res = ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]);
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool is_string($var)
 *  Finds out whether a variable is a string.
 * Parameters
 *   $var: The variable being evaluated.
 * Return
 *  TRUE if var is string. False otherwise.
 */
static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume false by default */
	if( nArg > 0 ){
		res = ph7_value_is_string(apArg[0]);
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool is_null($var)
 *  Finds out whether a variable is NULL.
 * Parameters
 *   $var: The variable being evaluated.
 * Return
 *  TRUE if var is NULL. False otherwise.
 */
static int PH7_builtin_is_null(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume false by default */
	if( nArg > 0 ){
		res = ph7_value_is_null(apArg[0]);
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool is_numeric($var)
 *  Find out whether a variable is NULL.
 * Parameters
 *  $var: The variable being evaluated.
 * Return
 *  True if var is numeric. False otherwise.
 */
static int PH7_builtin_is_numeric(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume false by default */
	if( nArg > 0 ){
		res = ph7_value_is_numeric(apArg[0]);
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool is_scalar($var)
 *  Find out whether a variable is a scalar.
 * Parameters
 *  $var: The variable being evaluated.
 * Return
 *  True if var is scalar. False otherwise.
 */
static int PH7_builtin_is_scalar(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume false by default */
	if( nArg > 0 ){
		res = ph7_value_is_scalar(apArg[0]);
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool is_array($var)
 *  Find out whether a variable is an array.
 * Parameters
 *  $var: The variable being evaluated.
 * Return
 *  True if var is an array. False otherwise.
 */
static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume false by default */
	if( nArg > 0 ){
		res = ph7_value_is_array(apArg[0]);
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool is_object($var)
 *  Find out whether a variable is an object.
 * Parameters
 *  $var: The variable being evaluated.
 * Return
 *  True if var is an object. False otherwise.
 */
static int PH7_builtin_is_object(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume false by default */
	if( nArg > 0 ){
		res = ph7_value_is_object(apArg[0]);
	}
	/* Query result */
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * bool is_resource($var)
 *  Find out whether a variable is a resource.
 * Parameters
 *  $var: The variable being evaluated.
 * Return
 *  True if a resource. False otherwise.
 */
static int PH7_builtin_is_resource(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 0; /* Assume false by default */
	if( nArg > 0 ){
		res = ph7_value_is_resource(apArg[0]);
	}
	ph7_result_bool(pCtx,res);
	return PH7_OK;
}
/*
 * float floatval($var)
 *  Get float value of a variable.
 * Parameter
 *  $var: The variable being processed.
 * Return
 *  the float value of a variable.
 */
static int PH7_builtin_floatval(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 1 ){
		/* return 0.0 */
		ph7_result_double(pCtx,0);
	}else{
		double dval;
		/* Perform the cast */
		dval = ph7_value_to_double(apArg[0]);
		ph7_result_double(pCtx,dval);
	}
	return PH7_OK;
}
/*
 * int intval($var)
 *  Get integer value of a variable.
 * Parameter
 *  $var: The variable being processed.
 * Return
 *  the int value of a variable.
 */
static int PH7_builtin_intval(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 1 ){
		/* return 0 */
		ph7_result_int(pCtx,0);
	}else{
		sxi64 iVal;
		/* Perform the cast */
		iVal = ph7_value_to_int64(apArg[0]);
		ph7_result_int64(pCtx,iVal);
	}
	return PH7_OK;
}
/*
 * string strval($var)
 *  Get the string representation of a variable.
 * Parameter
 *  $var: The variable being processed.
 * Return
 *  the string value of a variable.
 */
static int PH7_builtin_strval(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg < 1 ){
		/* return NULL */
		ph7_result_null(pCtx);
	}else{
		const char *zVal;
		int iLen = 0; /* cc -O6 warning */
		/* Perform the cast */
		zVal = ph7_value_to_string(apArg[0],&iLen);
		ph7_result_string(pCtx,zVal,iLen);
	}
	return PH7_OK;
}
/*
 * bool boolval($var)
 *  Get the boolean value of a variable.
 * Parameter
 *  $var: The variable being processed.
 * Return
 *  the bool value of a variable.
 */
static int PH7_builtin_boolval(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int bVal;
	if( nArg != 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"boolval() expects exactly 1 argument, %d given",
			nArg
			);
	}
	/* Perform the cast */
	bVal = ph7_value_to_bool(apArg[0]);
	ph7_result_bool(pCtx,bVal);
	return PH7_OK;
}
/*
 * bool empty($var)
 *  Determine whether a variable is empty.
 * Parameters
 *   $var: The variable being checked.
 * Return
 *  0 if var has a non-empty and non-zero value.1 otherwise.
 */
static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int res = 1; /* Assume empty by default */
	if( nArg > 0 ){
		res = ph7_value_is_empty(apArg[0]);
	}
	ph7_result_bool(pCtx,res);
	return PH7_OK;

}
#ifndef PH7_DISABLE_BUILTIN_FUNC
#define PH7_NEED_BUILTIN_REG 1
#endif
#ifndef PH7_DISABLE_DISK_IO
#define PH7_NEED_FMT_AND_INI 1
#endif

/* Math functions moved to builtin_math.c */
#ifdef PH7_NEED_BUILTIN_REG
/*
 * Section:
 *    String handling Functions.
 * Status:
 *    Stable.
 */
/*
 * string substr(string $string,int $start[, int $length ])
 *  Return part of a string.
 * Parameters
 *  $string
 *   The input string. Must be one character or longer.
 * $start
 *   If start is non-negative, the returned string will start at the start'th position
 *   in string, counting from zero. For instance, in the string 'abcdef', the character
 *   at position 0 is 'a', the character at position 2 is 'c', and so forth.
 *   If start is negative, the returned string will start at the start'th character
 *   from the end of string.
 *   If string is less than or equal to start characters long, FALSE will be returned.
 * $length
 *   If length is given and is positive, the string returned will contain at most length
 *   characters beginning from start (depending on the length of string).
 *   If length is given and is negative, then that many characters will be omitted from
 *   the end of string (after the start position has been calculated when a start is negative).
 *   If start denotes the position of this truncation or beyond, false will be returned.
 *   If length is given and is 0, FALSE or NULL an empty string will be returned.
 *   If length is omitted, the substring starting from start until the end of the string
 *   will be returned.
 * Return
 *  Returns the extracted part of string, or FALSE on failure or an empty string.
 */
static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zSource;
	int nSrcLen;
	sxi64 iStart,iEnd;
	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"substr",1,"$string"); }
	if( nArg < 2 ){
		/* Arity is enforced at the call boundary; nothing sensible to return here. */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Extract the target string */
	zSource = ph7_value_to_string(apArg[0],&nSrcLen);
	/* Extract the offset */
	{
		sxi64 iTmp = 0;
		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"substr",2,"$offset","int",&iTmp);
		if( rcArg != PH7_OK ){
			return rcArg;
		}
		iStart = iTmp;
	}
	/*
	 * php 8 never answers substr() with FALSE — every out-of-range window simply
	 * clamps to the empty string (substr("",0), substr("abc",5) and
	 * substr("abc",1,-5) are all ""). PH7 returned FALSE for each of those, which
	 * then flowed on as a bool into string context.
	 *
	 * A negative offset counts back from the end (clamped to 0); a negative length
	 * leaves that many bytes off the end. Computed in sxi64 so an INT64 offset or
	 * length cannot overflow the window arithmetic.
	 */
	if( iStart < 0 ){
		iStart += nSrcLen;
		if( iStart < 0 ){
			iStart = 0;
		}
	}else if( iStart > nSrcLen ){
		iStart = nSrcLen;
	}
	iEnd = nSrcLen;
	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){
		sxi64 iLen = 0;
		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],"substr",3,"$length","?int",&iLen);
		if( rcArg != PH7_OK ){
			return rcArg;
		}
		if( iLen < 0 ){
			iEnd = (sxi64)nSrcLen + iLen;
		}else if( iLen > (sxi64)nSrcLen - iStart ){
			iEnd = nSrcLen;
		}else{
			iEnd = iStart + iLen;
		}
	}
	if( iEnd < iStart ){
		iEnd = iStart;
	}
	ph7_result_string(pCtx,&zSource[iStart],(int)(iEnd - iStart));
	return PH7_OK;
}
/*
 * int substr_compare(string $main_str,string $str ,int $offset[,int $length[,bool $case_insensitivity = false ]])
 *  Binary safe comparison of two strings from an offset, up to length characters.
 * Parameters
 *  $main_str
 *  The main string being compared.
 *  $str
 *   The secondary string being compared.
 * $offset
 *  The start position for the comparison. If negative, it starts counting from
 *  the end of the string.
 * $length
 *  The length of the comparison. The default value is the largest of the length
 *  of the str compared to the length of main_str less the offset.
 * $case_insensitivity
 *  If case_insensitivity is TRUE, comparison is case insensitive.
 * Return
 *  Returns < 0 if main_str from position offset is less than str, > 0 if it is greater than
 *  str, and 0 if they are equal. If offset is equal to or greater than the length of main_str
 *  or length is set and is less than 1, substr_compare() prints a warning and returns FALSE.
 */
static int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zSource,*zSub;
	int nSrcLen,nSubLen;
	sxi64 iOfft,iLen,l1,l2,nCmp;
	int iCase = 0;
	int rc;
	if( nArg < 3 ){
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	zSource = ph7_value_to_string(apArg[0],&nSrcLen);
	zSub    = ph7_value_to_string(apArg[1],&nSubLen);
	{
		sxi64 iTmp = 0;
		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],"substr_compare",3,"$offset","int",&iTmp);
		if( rcArg != PH7_OK ){
			return rcArg;
		}
		iOfft = iTmp;
	}
	if( iOfft < 0 ){
		iOfft += nSrcLen;
		if( iOfft < 0 ){
			iOfft = 0;
		}
	}
	if( iOfft > nSrcLen ){
		/* php rejects an offset past the end of the haystack outright */
		return PH7_VmThrowException(pCtx,"ValueError",
			"substr_compare(): Argument #3 ($offset) must be contained in argument #1 ($haystack)");
	}
	/* A NULL/absent length compares as far as the longer of the two operands reaches */
	iLen = (sxi64)nSrcLen - iOfft;
	if( iLen < nSubLen ){
		iLen = nSubLen;
	}
	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){
		sxi64 iTmp = 0;
		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[3],"substr_compare",4,"$length","?int",&iTmp);
		if( rcArg != PH7_OK ){
			return rcArg;
		}
		if( iTmp < 0 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"substr_compare(): Argument #4 ($length) must be greater than or equal to 0");
		}
		iLen = iTmp;
	}
	if( nArg > 4 ){
		iCase = ph7_value_to_bool(apArg[4]);
	}
	/* Each side contributes at most what it actually has left */
	l1 = (sxi64)nSrcLen - iOfft;
	if( l1 > iLen ){ l1 = iLen; }
	l2 = nSubLen;
	if( l2 > iLen ){ l2 = iLen; }
	nCmp = (l1 < l2) ? l1 : l2;
	if( iCase ){
		rc = SyStrnicmp(&zSource[iOfft],zSub,(sxu32)nCmp);
	}else{
		rc = SyStrncmp(&zSource[iOfft],zSub,(sxu32)nCmp);
	}
	if( rc == 0 ){
		/* Prefixes equal: php falls back to a THREE-WAY compare of the lengths, so this
		 * arm is normalized to -1/0/1 (substr_compare("abc","",0) is 1, not 3). */
		rc = (l1 == l2) ? 0 : (l1 < l2 ? -1 : 1);
	}
	/* ...but when the prefixes differ php returns the RAW byte difference, not its sign:
	 * substr_compare("abc","def",1,10) is -2 ('b' - 'd'), which is what SyMemcmp gives. */
	ph7_result_int(pCtx,rc);
	return PH7_OK;
}
/*
 * int substr_count(string $haystack,string $needle[,int $offset = 0 [,int $length ]])
 *  Count the number of substring occurrences.
 * Parameters
 * $haystack
 *   The string to search in
 * $needle
 *   The substring to search for
 * $offset
 *  The offset where to start counting
 * $length (NOT USED)
 *  The maximum length after the specified offset to search for the substring.
 *  It outputs a warning if the offset plus the length is greater than the haystack length.
 * Return
 *  Toral number of substring occurrences.
 */
static int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zText,*zPattern,*zEnd;
	int nTextlen,nPatlen;
	int iCount = 0;
	sxu32 nOfft;
	sxi32 rc;
	if( nArg < 2 ){
		/* Missing arguments */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Point to the haystack */
	zText = ph7_value_to_string(apArg[0],&nTextlen);
	/* Point to the neddle */
	zPattern = ph7_value_to_string(apArg[1],&nPatlen);
	if( nPatlen < 1 ){
		/* Empty needle: PHP 8 throws a catchable ValueError. */
		return PH7_VmThrowException(pCtx,"ValueError",
			"substr_count(): Argument #2 ($needle) must not be empty");
	}
	/* Apply the optional $offset/$length window before searching. PHP 8 validates
	 * both against the haystack (a negative value counts from the end) and throws a
	 * catchable ValueError when the result falls outside it — this happens before the
	 * needle-fits check, so it fires even when the needle is longer than the haystack. */
	if( nArg > 2 ){
		ph7_int64 iOfft = ph7_value_to_int64(apArg[2]);
		if( iOfft < 0 ){
			iOfft += nTextlen;
		}
		if( iOfft < 0 || iOfft > nTextlen ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"substr_count(): Argument #3 ($offset) must be contained in argument #1 ($haystack)");
		}
		/* Point to the desired offset and shrink the remaining region */
		zText = &zText[iOfft];
		nTextlen -= (int)iOfft;
	}
	if( nArg > 3 ){
		ph7_int64 nLen = ph7_value_to_int64(apArg[3]);
		if( nLen < 0 ){
			/* Negative length is relative to the end of the (offset) haystack */
			nLen += nTextlen;
		}
		if( nLen < 0 || nLen > nTextlen ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"substr_count(): Argument #4 ($length) must be contained in argument #1 ($haystack)");
		}
		nTextlen = (int)nLen;
	}
	if( nTextlen < 1 || nPatlen > nTextlen ){
		/* The windowed haystack can't contain the needle: zero matches */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Point to the end of the windowed haystack */
	zEnd = &zText[nTextlen];
	/* Perform the search */
	for(;;){
		rc = SyBlobSearch((const void *)zText,(sxu32)(zEnd-zText),(const void *)zPattern,nPatlen,&nOfft);
		if( rc != SXRET_OK ){
			/* Pattern not found,break immediately */
			break;
		}
		/* Increment counter and update the offset */
		iCount++;
		zText += nOfft + nPatlen;
		if( zText >= zEnd ){
			break;
		}
	}
	/* Pattern count */
	ph7_result_int(pCtx,iCount);
	return PH7_OK;
}
/* Forward declarations: defined with the trim/addcslashes and str_contains
 * families below. */
static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256]);
/*
 * php 8.1 null-to-non-nullable ZPP deprecation, notice-only form for the
 * legacy string builtins that still coerce null to "" themselves: emit
 * `f(): Passing null to parameter #N ($name) of type string is deprecated`
 * when the arg is an actual null, leaving the resolution unchanged.
 */
static void StrNullArgNotice(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNum,const char *zParamName)
{
	if( ph7_value_is_null(pArg) ){
		PH7_VmThrowDeprecatedFmt(pCtx->pVm,
			"%s(): Passing null to parameter #%d (%s) of type string is deprecated",
			zFunc,iArgNum,zParamName);
	}
}
static sxi32 StrPredicateResolveArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,
	int iArgNum,const char *zParamName,const char *zTypeStr,const char *zNullMsg,
	ph7_value *pTmp,const char **pzOut,int *pnOut);
/*
 * Validate and resolve an int-typed builtin parameter with php-8 ZPP weak-mode
 * semantics: ints and bools pass through; null emits the 8.1 deprecation and
 * resolves to 0; floats and float-strings convert, with the implicit-conversion
 * E_DEPRECATED when lossy and a TypeError when NAN/INF/out of int range;
 * integral numeric strings convert exactly; everything else (arrays, resources,
 * objects, non-numeric strings) is a TypeError naming zTypeStr (e.g. "int",
 * "array|int"). Returns PH7_OK with *pOut set, or the throw status.
 */
/*
 * Normalize a substr_replace() offset/length pair against a string of nStrLen
 * bytes, exactly like PHP: a negative offset counts from the end (clamped to 0),
 * an offset past the end clamps to the end; a negative length leaves that many
 * bytes off the end of the remaining region (clamped to 0), and the length is
 * finally clamped to the remaining region. Written without f+l additions so an
 * INT64_MAX length cannot overflow.
 */
static void SubstrReplaceWindow(sxi64 *pF,sxi64 *pL,int nStrLen)
{
	sxi64 f = *pF,l = *pL;
	if( f < 0 ){
		f += nStrLen;
		if( f < 0 ){
			f = 0;
		}
	}else if( f > nStrLen ){
		f = nStrLen;
	}
	if( l < 0 ){
		l += nStrLen - f;
		if( l < 0 ){
			l = 0;
		}
	}
	if( l > nStrLen - f ){
		l = nStrLen - f;
	}
	*pF = f;
	*pL = l;
}
/* A replacement string collected out of substr_replace()'s $replace array.
 * The bytes live in a shared pool blob (walker values are transient), so the
 * item stores pool offsets, mirroring the strtr_entry technique. */
typedef struct substr_repl_item substr_repl_item;
struct substr_repl_item
{
	sxu32 nOfft; /* Offset of the string inside the pool */
	sxu32 nLen;  /* Length of the string */
};
typedef struct substr_replace_collect substr_replace_collect;
struct substr_replace_collect
{
	SyBlob *pPool;  /* Byte pool for string items (string walker only) */
	SySet *pSet;    /* substr_repl_item set (string) or sxi64 set (int) */
	sxi32 rc;       /* SXRET_OK or SXERR_MEM on collector failure */
};
/* ph7_array_walk() callback: append one $replace element to the pool. */
static int SubstrReplaceStrWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)
{
	substr_replace_collect *pCol = (substr_replace_collect *)pUserData;
	substr_repl_item sItem;
	const char *zStr;
	int nLen;
	SXUNUSED(pKey);
	zStr = ph7_value_to_string(pData,&nLen);
	sItem.nOfft = SyBlobLength(pCol->pPool);
	sItem.nLen = (sxu32)nLen;
	if( nLen > 0 && SXRET_OK != SyBlobAppend(pCol->pPool,(const void *)zStr,(sxu32)nLen) ){
		pCol->rc = SXERR_MEM;
		return SXERR_ABORT;
	}
	if( SXRET_OK != SySetPut(pCol->pSet,(const void *)&sItem) ){
		pCol->rc = SXERR_MEM;
		return SXERR_ABORT;
	}
	return PH7_OK;
}
/* ph7_array_walk() callback: collect one $offset/$length element as an int. */
static int SubstrReplaceIntWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)
{
	substr_replace_collect *pCol = (substr_replace_collect *)pUserData;
	sxi64 iVal = ph7_value_to_int64(pData);
	SXUNUSED(pKey);
	if( SXRET_OK != SySetPut(pCol->pSet,(const void *)&iVal) ){
		pCol->rc = SXERR_MEM;
		return SXERR_ABORT;
	}
	return PH7_OK;
}
/* Per-element state while walking substr_replace()'s array $string. */
typedef struct substr_replace_ctx substr_replace_ctx;
struct substr_replace_ctx
{
	ph7_value *pResult;   /* Result array (keys preserved) */
	ph7_value *pScratch;  /* Reusable string value for each element */
	SyBlob *pReplPool;    /* Pool behind aRepl items */
	SySet *pRepl;         /* substr_repl_item set or NULL when $replace is scalar */
	SySet *pFrom;         /* sxi64 set or NULL when $offset is scalar */
	SySet *pLen;          /* sxi64 set or NULL when $length is scalar/absent */
	sxu32 iReplCur;       /* Next-position cursors into the three sets */
	sxu32 iFromCur;
	sxu32 iLenCur;
	const char *zRepl;    /* Scalar $replace */
	int nRepl;
	sxi64 iFrom;          /* Scalar $offset */
	sxi64 iLen;           /* Scalar $length */
	int bLenGiven;        /* FALSE: $length absent/null -> element length */
	sxi32 rc;             /* SXRET_OK or SXERR_MEM */
};
/*
 * ph7_array_walk() callback over the array $string: replace the window of one
 * element and insert the result under the element's original key. Array-form
 * $replace/$offset/$length are consumed positionally; when a set runs out PHP
 * falls back to ""/0/element-length respectively.
 */
static int SubstrReplaceElemWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)
{
	substr_replace_ctx *pRep = (substr_replace_ctx *)pUserData;
	const char *zStr,*zRepl;
	sxi64 f,l;
	int nLen,nRepl;
	zStr = ph7_value_to_string(pData,&nLen);
	/* Positional $replace element ("" when exhausted) */
	if( pRep->pRepl ){
		if( pRep->iReplCur < SySetUsed(pRep->pRepl) ){
			substr_repl_item *pItem = (substr_repl_item *)SySetAt(pRep->pRepl,pRep->iReplCur++);
			zRepl = (const char *)SyBlobDataAt(pRep->pReplPool,pItem->nOfft);
			nRepl = (int)pItem->nLen;
		}else{
			zRepl = "";
			nRepl = 0;
		}
	}else{
		zRepl = pRep->zRepl;
		nRepl = pRep->nRepl;
	}
	/* Positional $offset element (0 when exhausted) */
	if( pRep->pFrom ){
		sxi64 *pVal = 0;
		if( pRep->iFromCur < SySetUsed(pRep->pFrom) ){
			pVal = (sxi64 *)SySetAt(pRep->pFrom,pRep->iFromCur++);
		}
		f = pVal ? *pVal : 0;
	}else{
		f = pRep->iFrom;
	}
	/* Positional $length element (element length when exhausted) */
	if( pRep->pLen ){
		sxi64 *pVal = 0;
		if( pRep->iLenCur < SySetUsed(pRep->pLen) ){
			pVal = (sxi64 *)SySetAt(pRep->pLen,pRep->iLenCur++);
		}
		l = pVal ? *pVal : nLen;
	}else{
		l = pRep->bLenGiven ? pRep->iLen : nLen;
	}
	SubstrReplaceWindow(&f,&l,nLen);
	/* Assemble prefix + replacement + suffix in the scratch value */
	ph7_value_reset_string_cursor(pRep->pScratch);
	if( (f > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,zStr,(int)f))
	 || (nRepl > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,zRepl,nRepl))
	 || (nLen - (int)(f+l) > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,&zStr[f+l],nLen - (int)(f+l))) ){
		pRep->rc = SXERR_MEM;
		return SXERR_ABORT;
	}
	if( SXRET_OK != ph7_array_add_elem(pRep->pResult,pKey,pRep->pScratch) ){
		pRep->rc = SXERR_MEM;
		return SXERR_ABORT;
	}
	return PH7_OK;
}
/*
 * mixed substr_replace(array|string $string,array|string $replace,array|int $offset[,array|int|null $length = null])
 *  Replace text within a portion of a string.
 * Parameters
 *  $string
 *   The input string or an array of strings (each element is processed with
 *   its own positional replace/offset/length when those are arrays too).
 *  $replace
 *   The replacement string. When $string is scalar and $replace is an array,
 *   only its first element is used (PHP quirk).
 *  $offset
 *   Window start; negative counts from the end of the string.
 *  $length
 *   Window length; negative leaves that many bytes at the end; null/absent
 *   means "to the end of the string".
 * Return
 *  The processed string, or an array of processed strings (keys preserved).
 * Errors
 *  ArgumentCountError on fewer than 3 arguments; TypeError when an array
 *  $offset/$length is combined with a scalar $string.
 */
static int PH7_builtin_substr_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value sStrTmp,sReplTmp;
	const char *zStr = 0,*zRepl = 0;
	int nLen = 0,nRepl = 0;
	int bLenGiven;
	sxi64 f = 0,l = 0;
	sxi32 rc;
	if( nArg < 3 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"substr_replace() expects at least 3 arguments, %d given",
			nArg
			);
	}
	/* $length counts as given unless absent or null (php: ?null semantics) */
	bLenGiven = (nArg > 3 && !ph7_value_is_null(apArg[3]));
	/* php ZPP validates all four args, in order, before the body runs: the
	 * non-array forms resolve here (null deprecation, __toString objects,
	 * numeric strings), arrays pass through to the per-mode handling. */
	PH7_MemObjInit(pCtx->pVm,&sStrTmp);
	PH7_MemObjInit(pCtx->pVm,&sReplTmp);
	if( !ph7_value_is_array(apArg[0]) ){
		rc = StrPredicateResolveArg(pCtx,apArg[0],"substr_replace",1,"$string","array|string",
			"substr_replace(): Passing null to parameter #1 ($string) "
			"of type array|string is deprecated",
			&sStrTmp,&zStr,&nLen);
		if( rc != PH7_OK ) goto out;
	}
	if( !ph7_value_is_array(apArg[1]) ){
		rc = StrPredicateResolveArg(pCtx,apArg[1],"substr_replace",2,"$replace","array|string",
			"substr_replace(): Passing null to parameter #2 ($replace) "
			"of type array|string is deprecated",
			&sReplTmp,&zRepl,&nRepl);
		if( rc != PH7_OK ) goto out;
	}
	if( !ph7_value_is_array(apArg[2]) ){
		rc = PH7_IntArgResolve(pCtx,apArg[2],"substr_replace",3,"$offset","array|int",&f);
		if( rc != PH7_OK ) goto out;
	}
	if( bLenGiven && !ph7_value_is_array(apArg[3]) ){
		rc = PH7_IntArgResolve(pCtx,apArg[3],"substr_replace",4,"$length","array|int|null",&l);
		if( rc != PH7_OK ) goto out;
	}
	if( ph7_value_is_array(apArg[0]) ){
		/* Array form: process each element, preserving keys */
		substr_replace_ctx sRep;
		substr_replace_collect sCol;
		SyBlob sReplPool;
		SySet sRepl,sFrom,sLen;
		ph7_value *pResult,*pScratch;
		sxi32 rcWalk = SXRET_OK;
		SyBlobInit(&sReplPool,&pCtx->pVm->sAllocator);
		SySetInit(&sRepl,&pCtx->pVm->sAllocator,sizeof(substr_repl_item));
		SySetInit(&sFrom,&pCtx->pVm->sAllocator,sizeof(sxi64));
		SySetInit(&sLen,&pCtx->pVm->sAllocator,sizeof(sxi64));
		SyZero(&sRep,sizeof(substr_replace_ctx));
		sRep.bLenGiven = bLenGiven;
		sCol.rc = SXRET_OK;
		/* Collect array-form $replace/$offset/$length positionally; the
		 * scalar forms were already resolved above. */
		if( ph7_value_is_array(apArg[1]) ){
			sCol.pPool = &sReplPool;
			sCol.pSet = &sRepl;
			ph7_array_walk(apArg[1],SubstrReplaceStrWalker,&sCol);
			sRep.pRepl = &sRepl;
			sRep.pReplPool = &sReplPool;
		}else{
			sRep.zRepl = zRepl;
			sRep.nRepl = nRepl;
		}
		if( sCol.rc == SXRET_OK && ph7_value_is_array(apArg[2]) ){
			sCol.pSet = &sFrom;
			ph7_array_walk(apArg[2],SubstrReplaceIntWalker,&sCol);
			sRep.pFrom = &sFrom;
		}else{
			sRep.iFrom = f;
		}
		if( sCol.rc == SXRET_OK && bLenGiven ){
			if( ph7_value_is_array(apArg[3]) ){
				sCol.pSet = &sLen;
				ph7_array_walk(apArg[3],SubstrReplaceIntWalker,&sCol);
				sRep.pLen = &sLen;
			}else{
				sRep.iLen = l;
			}
		}
		pResult = ph7_context_new_array(pCtx);
		pScratch = ph7_context_new_scalar(pCtx);
		if( sCol.rc != SXRET_OK || pResult == 0 || pScratch == 0 ){
			rcWalk = SXERR_MEM;
		}else{
			sRep.pResult = pResult;
			sRep.pScratch = pScratch;
			ph7_value_string(pScratch,"",0); /* Force string representation */
			ph7_array_walk(apArg[0],SubstrReplaceElemWalker,&sRep);
			rcWalk = sRep.rc;
		}
		SyBlobRelease(&sReplPool);
		SySetRelease(&sRepl);
		SySetRelease(&sFrom);
		SySetRelease(&sLen);
		if( rcWalk != SXRET_OK ){
			rc = PH7_ContextMemoryError(pCtx);
			goto out;
		}
		ph7_result_value(pCtx,pResult);
		rc = PH7_OK;
		goto out;
	}
	/* Scalar form: array $offset/$length are a TypeError, array $replace
	 * degrades to its first element (php quirk). */
	if( ph7_value_is_array(apArg[2]) ){
		rc = PH7_VmThrowException(pCtx,
			"TypeError",
			"substr_replace(): Argument #3 ($offset) cannot be an array when working on a single string"
			);
		goto out;
	}
	if( bLenGiven && ph7_value_is_array(apArg[3]) ){
		rc = PH7_VmThrowException(pCtx,
			"TypeError",
			"substr_replace(): Argument #4 ($length) cannot be an array when working on a single string"
			);
		goto out;
	}
	if( ph7_value_is_array(apArg[1]) ){
		/* First element of the replace array, or "" when empty */
		ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;
		zRepl = "";
		nRepl = 0;
		if( pMap->pFirst ){
			ph7_value *pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pMap->pFirst->nValIdx);
			if( pVal ){
				zRepl = ph7_value_to_string(pVal,&nRepl);
			}
		}
	}
	if( !bLenGiven ){
		l = nLen;
	}
	SubstrReplaceWindow(&f,&l,nLen);
	/* Assemble prefix + replacement + suffix straight into the call result
	 * (ph7_result_string appends), no scratch buffer needed. */
	rc = SXRET_OK;
	if( f > 0 ){
		rc = ph7_result_string(pCtx,zStr,(int)f);
	}
	if( rc == SXRET_OK && nRepl > 0 ){
		rc = ph7_result_string(pCtx,zRepl,nRepl);
	}
	if( rc == SXRET_OK && nLen - (int)(f+l) > 0 ){
		rc = ph7_result_string(pCtx,&zStr[f+l],nLen - (int)(f+l));
	}
	if( rc != SXRET_OK ){
		rc = PH7_ContextMemoryError(pCtx);
		goto out;
	}
	/* Force a string result even when all three segments are empty */
	rc = ph7_result_string(pCtx,"",0);
	if( rc != SXRET_OK ){
		rc = PH7_ContextMemoryError(pCtx);
		goto out;
	}
	rc = PH7_OK;
out:
	PH7_MemObjRelease(&sStrTmp);
	PH7_MemObjRelease(&sReplTmp);
	return rc;
}
/*
 * int levenshtein(string $string1,string $string2[,int $insertion_cost = 1[,int $replacement_cost = 1[,int $deletion_cost = 1]]])
 *  Calculate the Levenshtein distance between two strings, byte per byte
 *  (case-sensitive), with optional per-operation costs. Mirrors PHP's
 *  reference_levdist(): two rolling rows over string2.
 * Return
 *  The minimal number of weighted edit operations turning $string1 into
 *  $string2.
 */
static int PH7_builtin_levenshtein(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	static const char *azParam[] = { "$insertion_cost","$replacement_cost","$deletion_cost" };
	const char *zStr1,*zStr2;
	sxi64 iCostIns = 1,iCostRep = 1,iCostDel = 1;
	sxi64 *p1,*p2,*pTmp;
	sxi64 c0,c1,c2;
	ph7_value sTmp1,sTmp2;
	int nLen1,nLen2;
	int i1,i2;
	sxi32 rc;
	int i;
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"levenshtein() expects at least 2 arguments, %d given",
			nArg
			);
	}
	/* $string1/$string2: null deprecates to "", __toString objects resolve,
	 * everything non-stringish is a TypeError (php ZPP weak mode). */
	PH7_MemObjInit(pCtx->pVm,&sTmp1);
	PH7_MemObjInit(pCtx->pVm,&sTmp2);
	rc = StrPredicateResolveArg(pCtx,apArg[0],"levenshtein",1,"$string1","string",
		"levenshtein(): Passing null to parameter #1 ($string1) "
		"of type string is deprecated",
		&sTmp1,&zStr1,&nLen1);
	if( rc != PH7_OK ) goto out;
	rc = StrPredicateResolveArg(pCtx,apArg[1],"levenshtein",2,"$string2","string",
		"levenshtein(): Passing null to parameter #2 ($string2) "
		"of type string is deprecated",
		&sTmp2,&zStr2,&nLen2);
	if( rc != PH7_OK ) goto out;
	/* Optional integer costs */
	for( i = 2 ; i < nArg && i < 5 ; i++ ){
		sxi64 iVal;
		rc = PH7_IntArgResolve(pCtx,apArg[i],"levenshtein",i+1,azParam[i-2],"int",&iVal);
		if( rc != PH7_OK ) goto out;
		if( i == 2 ){
			iCostIns = iVal;
		}else if( i == 3 ){
			iCostRep = iVal;
		}else{
			iCostDel = iVal;
		}
	}
	if( nLen1 == 0 ){
		ph7_result_int64(pCtx,(sxi64)nLen2 * iCostIns);
		rc = PH7_OK;
		goto out;
	}
	if( nLen2 == 0 ){
		ph7_result_int64(pCtx,(sxi64)nLen1 * iCostDel);
		rc = PH7_OK;
		goto out;
	}
	/* Two rolling DP rows over string2 (auto-released on return). Reject a
	 * string2 long enough to overflow the 32-bit allocation size. */
	if( (sxu32)nLen2 >= (SXU32_HIGH / sizeof(sxi64)) - 1 ){
		rc = PH7_ContextMemoryError(pCtx);
		goto out;
	}
	p1 = (sxi64 *)ph7_context_alloc_chunk(pCtx,(unsigned int)(sizeof(sxi64) * (sxu32)(nLen2 + 1)),FALSE,TRUE);
	p2 = (sxi64 *)ph7_context_alloc_chunk(pCtx,(unsigned int)(sizeof(sxi64) * (sxu32)(nLen2 + 1)),FALSE,TRUE);
	if( p1 == 0 || p2 == 0 ){
		rc = PH7_ContextMemoryError(pCtx);
		goto out;
	}
	for( i2 = 0 ; i2 <= nLen2 ; i2++ ){
		p1[i2] = (sxi64)i2 * iCostIns;
	}
	for( i1 = 0 ; i1 < nLen1 ; i1++ ){
		p2[0] = p1[0] + iCostDel;
		for( i2 = 0 ; i2 < nLen2 ; i2++ ){
			c0 = p1[i2] + ((zStr1[i1] == zStr2[i2]) ? 0 : iCostRep);
			c1 = p1[i2 + 1] + iCostDel;
			if( c1 < c0 ){
				c0 = c1;
			}
			c2 = p2[i2] + iCostIns;
			if( c2 < c0 ){
				c0 = c2;
			}
			p2[i2 + 1] = c0;
		}
		pTmp = p1;
		p1 = p2;
		p2 = pTmp;
	}
	ph7_result_int64(pCtx,p1[nLen2]);
	rc = PH7_OK;
out:
	PH7_MemObjRelease(&sTmp1);
	PH7_MemObjRelease(&sTmp2);
	return rc;
}
/*
 * Longest common substring scan behind similar_text() — a faithful port of
 * PHP's php_similar_str(): O(n*m) scan recording the first longest run.
 */
static void SimilarStr(const char *zTxt1,int nLen1,const char *zTxt2,int nLen2,
	int *pPos1,int *pPos2,int *pMax,int *pCount)
{
	const char *p,*q;
	const char *zEnd1 = &zTxt1[nLen1];
	const char *zEnd2 = &zTxt2[nLen2];
	int l;
	*pMax = 0;
	*pCount = 0;
	for( p = zTxt1 ; p < zEnd1 ; p++ ){
		for( q = zTxt2 ; q < zEnd2 ; q++ ){
			for( l = 0 ; (p+l < zEnd1) && (q+l < zEnd2) && (p[l] == q[l]) ; l++ );
			if( l > *pMax ){
				*pMax = l;
				*pCount += 1;
				*pPos1 = (int)(p - zTxt1);
				*pPos2 = (int)(q - zTxt2);
			}
		}
	}
}
/*
 * Recursive divide-and-conquer behind similar_text() — a faithful port of
 * PHP's php_similar_char(), including its quirky `count > 1` guard on the
 * left-side recursion.
 */
static int SimilarChar(const char *zTxt1,int nLen1,const char *zTxt2,int nLen2)
{
	int nSum;
	int nPos1 = 0,nPos2 = 0,nMax,nCount;
	SimilarStr(zTxt1,nLen1,zTxt2,nLen2,&nPos1,&nPos2,&nMax,&nCount);
	if( (nSum = nMax) != 0 ){
		if( nPos1 && nPos2 && nCount > 1 ){
			nSum += SimilarChar(zTxt1,nPos1,zTxt2,nPos2);
		}
		if( (nPos1 + nMax < nLen1) && (nPos2 + nMax < nLen2) ){
			nSum += SimilarChar(&zTxt1[nPos1 + nMax],nLen1 - nPos1 - nMax,
				&zTxt2[nPos2 + nMax],nLen2 - nPos2 - nMax);
		}
	}
	return nSum;
}
/*
 * int similar_text(string $string1,string $string2[,float &$percent])
 *  Calculate the similarity between two strings, as the number of matching
 *  characters found by PHP's greedy longest-common-substring recursion.
 *  When $percent is given it receives the similarity in percent:
 *  matching * 200 / (len1 + len2).
 * Return
 *  The number of matching characters in both strings.
 */
static int PH7_builtin_similar_text(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zStr1,*zStr2;
	ph7_value sTmp1,sTmp2;
	int nLen1,nLen2;
	int nSim;
	sxi32 rc;
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"similar_text() expects at least 2 arguments, %d given",
			nArg
			);
	}
	PH7_MemObjInit(pCtx->pVm,&sTmp1);
	PH7_MemObjInit(pCtx->pVm,&sTmp2);
	rc = StrPredicateResolveArg(pCtx,apArg[0],"similar_text",1,"$string1","string",
		"similar_text(): Passing null to parameter #1 ($string1) "
		"of type string is deprecated",
		&sTmp1,&zStr1,&nLen1);
	if( rc != PH7_OK ) goto out;
	rc = StrPredicateResolveArg(pCtx,apArg[1],"similar_text",2,"$string2","string",
		"similar_text(): Passing null to parameter #2 ($string2) "
		"of type string is deprecated",
		&sTmp2,&zStr2,&nLen2);
	if( rc != PH7_OK ) goto out;
	if( nLen1 + nLen2 == 0 ){
		nSim = 0;
	}else{
		nSim = SimilarChar(zStr1,nLen1,zStr2,nLen2);
	}
	if( nArg > 2 ){
		/* Write the percentage through the by-ref out-param */
		ph7_value *pPercent = ph7_context_new_scalar(pCtx);
		if( pPercent == 0 ){
			rc = PH7_ContextMemoryError(pCtx);
			goto out;
		}else{
			double dPct = (nLen1 + nLen2 == 0) ? 0.0 : (double)nSim * 200.0 / (double)(nLen1 + nLen2);
			ph7_value_double(pPercent,dPct);
			PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],pPercent);
		}
	}
	ph7_result_int(pCtx,nSim);
	rc = PH7_OK;
out:
	PH7_MemObjRelease(&sTmp1);
	PH7_MemObjRelease(&sTmp2);
	return rc;
}
/*
 * array|int str_word_count(string $string[,int $format = 0[,?string $characters = null]])
 *  Count (or return) the words inside a string. A word is a run of alphabetic
 *  characters, which may contain (but not start the string with) "'" and "-";
 *  $characters adds extra bytes to the word set ("a..z" ranges supported, as
 *  in PHP's php_charmask).
 *  $format: 0 -> word count, 1 -> array of words, 2 -> array of words keyed
 *  by their byte position in $string.
 * Errors
 *  ValueError when $format is not 0, 1 or 2.
 */
static int PH7_builtin_str_word_count(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn,*zEnd,*zPtr;
	ph7_value *pArray = 0,*pValue = 0;
	ph7_value sTmp,sListTmp;
	char aMask[256];
	int bMask = 0;
	int iFormat = 0;
	int nCount = 0;
	int nLen;
	sxi32 rc;
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"str_word_count() expects at least 1 argument, %d given",
			nArg
			);
	}
	PH7_MemObjInit(pCtx->pVm,&sTmp);
	PH7_MemObjInit(pCtx->pVm,&sListTmp);
	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_word_count",1,"$string","string",
		"str_word_count(): Passing null to parameter #1 ($string) "
		"of type string is deprecated",
		&sTmp,&zIn,&nLen);
	if( rc != PH7_OK ) goto out;
	if( nArg > 1 ){
		sxi64 iVal;
		rc = PH7_IntArgResolve(pCtx,apArg[1],"str_word_count",2,"$format","int",&iVal);
		if( rc != PH7_OK ) goto out;
		if( iVal < 0 || iVal > 2 ){
			rc = PH7_VmThrowException(pCtx,
				"ValueError",
				"str_word_count(): Argument #2 ($format) must be a valid format value"
				);
			goto out;
		}
		iFormat = (int)iVal;
	}
	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){
		/* $characters is ?string: null (skipped above) simply keeps the
		 * default word set, no deprecation. */
		const char *zList;
		int nList;
		rc = StrPredicateResolveArg(pCtx,apArg[2],"str_word_count",3,"$characters","?string",
			"" /* unreachable: null never gets here */,
			&sListTmp,&zList,&nList);
		if( rc != PH7_OK ) goto out;
		PH7_BuildCharMask(pCtx,zList,nList,aMask);
		bMask = 1;
	}
	if( iFormat != 0 ){
		pArray = ph7_context_new_array(pCtx);
		pValue = ph7_context_new_scalar(pCtx);
		if( pArray == 0 || pValue == 0 ){
			rc = PH7_ContextMemoryError(pCtx);
			goto out;
		}
	}
	zPtr = zIn;
	zEnd = &zIn[nLen];
	if( nLen > 0 ){
		/* php: the string's first byte cannot be ' or -, and its last byte
		 * cannot be -, unless the charlist explicitly allows them. */
		if( (zPtr[0] == '\'' && (!bMask || !aMask[(unsigned char)'\''])) ||
			(zPtr[0] == '-'  && (!bMask || !aMask[(unsigned char)'-'])) ){
			zPtr++;
		}
		if( zEnd[-1] == '-' && (!bMask || !aMask[(unsigned char)'-']) ){
			zEnd--;
		}
	}
	while( zPtr < zEnd ){
		const char *zStart = zPtr;
		while( zPtr < zEnd && ( SyisAlpha((unsigned char)zPtr[0])
			|| (bMask && aMask[(unsigned char)zPtr[0]])
			|| zPtr[0] == '\'' || zPtr[0] == '-' ) ){
			zPtr++;
		}
		if( zPtr > zStart ){
			if( iFormat == 0 ){
				nCount++;
			}else{
				ph7_value_reset_string_cursor(pValue);
				if( SXRET_OK != ph7_value_string(pValue,zStart,(int)(zPtr-zStart)) ){
					rc = PH7_ContextMemoryError(pCtx);
					goto out;
				}
				if( iFormat == 1 ){
					if( SXRET_OK != ph7_array_add_elem(pArray,0,pValue) ){
						rc = PH7_ContextMemoryError(pCtx);
						goto out;
					}
				}else{
					if( SXRET_OK != ph7_array_add_intkey_elem(pArray,(int)(zStart-zIn),pValue) ){
						rc = PH7_ContextMemoryError(pCtx);
						goto out;
					}
				}
			}
		}
		zPtr++;
	}
	if( iFormat == 0 ){
		ph7_result_int(pCtx,nCount);
	}else{
		ph7_result_value(pCtx,pArray);
	}
	rc = PH7_OK;
out:
	PH7_MemObjRelease(&sTmp);
	PH7_MemObjRelease(&sListTmp);
	return rc;
}
/*
 * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])
 *   Split a string into smaller chunks.
 * Parameters
 *  $body
 *   The string to be chunked.
 * $chunklen
 *   The chunk length.
 * $end
 *   The line ending sequence.
 * Return
 *  The chunked string or NULL on failure.
 */
static int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn,*zEnd,*zSep = "\r\n";
	int nSepLen,nChunkLen,nLen;
	/* php coerces a scalar argument to string here (weak mode); the shared ZPP
	 * screen in vm.c has already rejected the values that cannot coerce. */
	if( nArg < 1 ){
		/* Nothing to split,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* initialize/Extract arguments */
	nSepLen = (int)sizeof("\r\n") - 1;
	nChunkLen = 76;
	zIn = ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nArg > 1 ){
		/* Chunk length */
		nChunkLen = ph7_value_to_int(apArg[1]);
		if( nChunkLen < 1 ){
			/* PHP 8 throws a catchable ValueError for a non-positive length. */
			return PH7_VmThrowException(pCtx,"ValueError",
				"chunk_split(): Argument #2 ($length) must be greater than 0");
		}
		if( nArg > 2 ){
			/* Separator */
			zSep = ph7_value_to_string(apArg[2],&nSepLen);
			if( nSepLen < 1 ){
				/* Switch back to the default separator */
				zSep = "\r\n";
				nSepLen = (int)sizeof("\r\n") - 1;
			}
		}
	}
	/* Perform the requested operation */
	if( nChunkLen > nLen ){
		/* Nothing to split,return the string and the separator */
		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);
		return PH7_OK;
	}
	while( zIn < zEnd ){
		if( nChunkLen > (int)(zEnd-zIn) ){
			nChunkLen = (int)(zEnd - zIn);
		}
		/* Append the chunk and the separator */
		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);
		/* Point beyond the chunk */
		zIn += nChunkLen;
	}
	return PH7_OK;
}
/*
 * string addslashes(string $str)
 *  Quote string with slashes.
 *  Returns a string with backslashes before characters that need
 *  to be quoted in database queries etc. These characters are single
 *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).
 * Parameter
 *  str: The string to be escaped.
 * Return
 *  Returns the escaped string
 */
static int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zCur,*zIn,*zEnd;
	int nLen;
	/* PHP enforces exactly one argument. */
	if( nArg != 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"addslashes() expects exactly 1 argument, %d given",
			nArg
			);
	}
	/* NULL is deprecated and treated as an empty string; other invalid
	 * types still produce a TypeError. */
	if( ph7_value_is_null(apArg[0]) ){
		PH7_VmThrowError(pCtx->pVm,0,
			E_DEPRECATED,
			"addslashes(): Passing null to parameter #1 ($string) of type string is deprecated"
			);
		/* fall through so conversion below yields empty string */
	}
	/* Arrays, objects and resources should raise a TypeError like PHP */
	if( ph7_value_is_array(apArg[0]) ||
	    ph7_value_is_object(apArg[0]) ||
	    ph7_value_is_resource(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"addslashes(): Argument #1 ($string) must be of type string, %s given",
			ph7_type_name(apArg[0])
			);
	}
	/* Convert to string representation first and obtain length. */
	zIn  = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	zEnd = &zIn[nLen];
	zCur = 0; /* cc warning */
	for(;;){
		if( zIn >= zEnd ){
			/* No more input */
			break;
		}
		zCur = zIn;
		/* scan until a character that needs escaping (', ", \\, or NUL) */
		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' && zIn[0] != '\0' ){
			zIn++;
		}
		if( zIn > zCur ){
			/* Append raw contents */
			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));
		}
		if( zIn < zEnd ){
			int c = zIn[0];
			if( c == '\0' ){
				/* PHP escapes NUL as "\\0" (two characters) */
				ph7_result_string(pCtx,"\\0",2);
			}else{
				ph7_result_string_format(pCtx,"\\%c",c);
			}
		}
		zIn++;
	}
	return PH7_OK;
}
/*
 * Build a 256-entry membership mask from a PHP charlist, expanding `a..z`
 * byte ranges exactly like PHP's php_charmask(). On return aMask[c] != 0 iff
 * the byte c belongs to the set. Emits the PHP-exact warnings for the three
 * malformed-range shapes (ph7_context_throw_error_format prepends the active
 * function name, so the messages omit it); on a bad range the surrounding
 * bytes are still added and the scan never aborts. Reads only within
 * [zList, zList+nLen).
 *
 * Use ONLY for the builtins whose charlist expands ranges the way PHP's
 * php_charmask() does: trim/ltrim/rtrim/addcslashes (and quotemeta, whose set
 * is a fixed literal with no ".."). Do NOT route strspn/strcspn/strtok/strpbrk
 * through this — PHP treats their charlists literally, so expanding "a..z" here
 * would be a behavior regression plus spurious "Invalid '..'-range" warnings.
 */
static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256])
{
	const unsigned char *zIn  = (const unsigned char *)zList;
	const unsigned char *zEnd = zIn + (nLen > 0 ? nLen : 0);
	SyZero(aMask,256);
	for( ; zIn < zEnd ; zIn++ ){
		int c = zIn[0];
		if( zIn + 3 < zEnd && zIn[1] == '.' && zIn[2] == '.' && zIn[3] >= c ){
			/* Valid incrementing range c..zIn[3] */
			int hi = zIn[3],k;
			for( k = c ; k <= hi ; k++ ){
				aMask[k] = 1;
			}
			zIn += 3; /* the loop's ++ then steps past the range end */
		}else if( zIn + 1 < zEnd && zIn[0] == '.' && zIn[1] == '.' ){
			/* Malformed range: mirror php_charmask's three diagnostics. */
			const char *zMsg;
			if( (const unsigned char *)zList >= zIn ){
				zMsg = "no character to the left of '..'";
			}else if( zIn + 2 >= zEnd ){
				zMsg = "no character to the right of '..'";
			}else if( zIn[-1] > zIn[2] ){
				zMsg = "'..'-range needs to be incrementing";
			}else{
				zMsg = 0; /* catch-all (e.g. a..b..c) */
			}
			if( zMsg ){
				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
					"Invalid '..'-range, %s",zMsg);
			}else{
				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
					"Invalid '..'-range");
			}
			/* Do not consume the dots: the loop's ++ steps one byte so the
			 * dots are re-scanned as literals, exactly like php_charmask. */
		}else{
			aMask[c] = 1;
		}
	}
}
/*
 * string addcslashes(string $str,string $charlist)
 *  Quote string with slashes in a C style.
 * Parameter
 *  $str:
 *    The string to be escaped.
 *  $charlist:
 *    A list of characters to be escaped. If charlist contains characters \n, \r etc.
 *    they are converted in C-like style, while other non-alphanumeric characters
 *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.
 * Return
 *  Returns the escaped string.
 * Note:
 *  Character ranges [i.e: 'A..Z'] are supported (see PH7_BuildCharMask).
 */
static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zCur,*zIn,*zEnd,*zMask;
	char aMask[256];
	int nLen,nMask;
	/* PHP enforces exactly two arguments. */
	if( nArg != 2 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"addcslashes() expects exactly 2 arguments, %d given",
			nArg
			);
	}
	/* First argument must be a string-ish value.  NULL is deprecated and
	 * treated as the empty string (PHP 8.1). */
	if( ph7_value_is_null(apArg[0]) ){
		/* Emit deprecation only once, similar to PHP behaviour. */
		PH7_VmThrowError(pCtx->pVm,0,/* iErr will be patched to 8192 below */
			E_DEPRECATED,
			"addcslashes(): Passing null to parameter #1 ($string) of type string is deprecated"
			);
		/* treat as empty string; fall through to conversion logic */
	} else if( ph7_value_is_array(apArg[0]) ||
	          ph7_value_is_object(apArg[0]) ||
	          ph7_value_is_resource(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"addcslashes(): Argument #1 ($string) must be of type string, %s given",
			ph7_type_name(apArg[0])
			);
	}
	/* Second argument must be a string.  NULL is deprecated and treated as
	 * an empty mask per PHP semantics.  Arrays/objects/resources still
	 * trigger a TypeError. */
	if( ph7_value_is_null(apArg[1]) ){
		PH7_VmThrowError(pCtx->pVm,0,
			E_DEPRECATED,
			"addcslashes(): Passing null to parameter #2 ($characters) of type string is deprecated"
			);
		/* allow through so it becomes empty string below */
	} else if( ph7_value_is_array(apArg[1]) ||
	          ph7_value_is_object(apArg[1]) ||
	          ph7_value_is_resource(apArg[1]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"addcslashes(): Argument #2 ($characters) must be of type string, %s given",
			ph7_type_name(apArg[1])
			);
	}
	/* Extract the string to process */
	zIn  = ph7_value_to_string(apArg[0],&nLen);
	/* NULL would never reach here due to the check above. */
	if( nLen < 1 ){
		/* Empty string returns itself. */
		ph7_result_string(pCtx,zIn,nLen);
		return PH7_OK;
	}
	/* Extract the desired mask and expand any `a..z` ranges into a lookup. */
	zMask = ph7_value_to_string(apArg[1],&nMask);
	PH7_BuildCharMask(pCtx,zMask,nMask,aMask);
	zEnd = &zIn[nLen];
	zCur = 0; /* cc warning */
	for(;;){
		if( zIn >= zEnd ){
			/* No more input */
			break;
		}
		zCur = zIn;
		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){
			zIn++;
		}
		if( zIn > zCur ){
			/* Append raw contents */
			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));
		}
		if( zIn < zEnd ){
			/* Make sure we treat the byte as unsigned to avoid negative values
			 * on platforms where char is signed. */
			int c = (unsigned char)zIn[0];
			/* Handle special C-like escapes for common control characters first.
			 * PHP outputs "\n" "\r" "\t" "\v" "\f" when those chars are
			 * in the mask. NUL is left to the octal conversion below. */
			if( c == '\n' ){
				ph7_result_string(pCtx,"\\n",2);
			}else if( c == '\r' ){
				ph7_result_string(pCtx,"\\r",2);
			}else if( c == '\t' ){
				ph7_result_string(pCtx,"\\t",2);
			}else if( c == '\v' ){
				ph7_result_string(pCtx,"\\v",2);
			}else if( c == '\f' ){
				ph7_result_string(pCtx,"\\f",2);
			}else if( c > 126 || (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){
				/* Convert to octal.  PHP always emits three-digit zero-padded
				 * octal escapes (\001 not \1). */
				ph7_result_string_format(pCtx,"\\%03o",c);
			}else{
				ph7_result_string_format(pCtx,"\\%c",c);
			}
		}
		zIn++;
	}
	return PH7_OK;
}
/*
 * string quotemeta(string $str)
 *  Quote meta characters.
 * Parameter
 *  $str:
 *    The string to be escaped.
 * Return
 *  Returns the escaped string.
*/
static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zCur,*zIn,*zEnd;
	char aMask[256];
	int nLen;
	if( nArg < 1 ){
		/* Nothing to process,retun NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the string to process */
	zIn  = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Fixed meta-character set (no ranges); build the lookup once. */
	PH7_BuildCharMask(pCtx,".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1,aMask);
	zEnd = &zIn[nLen];
	zCur = 0; /* cc warning */
	for(;;){
		if( zIn >= zEnd ){
			/* No more input */
			break;
		}
		zCur = zIn;
		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){
			zIn++;
		}
		if( zIn > zCur ){
			/* Append raw contents */
			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));
		}
		if( zIn < zEnd ){
			int c = zIn[0];
			ph7_result_string_format(pCtx,"\\%c",c);
		}
		zIn++;
	}
	return PH7_OK;
}
/*
 * string stripslashes(string $str)
 *  Un-quotes a quoted string.
 *  Returns a string with backslashes before characters that need
 *  to be quoted in database queries etc. These characters are single
 *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).
 * Parameter
 *  $str
 *   The input string.
 * Return
 *  Returns a string with backslashes stripped off.
 */
static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zCur,*zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Nothing to process,retun NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the string to process */
	zIn  = ph7_value_to_string(apArg[0],&nLen);
	if( zIn == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	zEnd = &zIn[nLen];
	zCur = 0; /* cc warning */
	/* Encode the string */
	for(;;){
		if( zIn >= zEnd ){
			/* No more input */
			break;
		}
		zCur = zIn;
		while( zIn < zEnd && zIn[0] != '\\' ){
			zIn++;
		}
		if( zIn > zCur ){
			/* Append raw contents */
			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));
		}
		if( &zIn[1] < zEnd ){
			int c = zIn[1];
			if( c == '\'' || c == '"' || c == '\\' ){
				/* Ignore the backslash */
				zIn++;
			}
		}else{
			break;
		}
	}
	return PH7_OK;
}
/*
 * UTF-8-aware HTML entity machinery, shared by htmlspecialchars/htmlentities/
 * htmlspecialchars_decode/html_entity_decode/get_html_translation_table.
 * The implementations live further down in this file, next to the filter_var
 * FULL_SPECIAL_CHARS machinery they reuse (aHtml401Ent[]/FvHtml401Lookup()/
 * FvUtf8Next()). Semantics are byte-exact vs php 8.5.7; PHL is UTF-8-only
 * so every charset argument other than a UTF-8 alias gets PHP's
 * unsupported-charset warning and is treated as UTF-8.
 *
 * Flag model (the PHP-exact ENT_* values, see constant.c): bit 1 = encode/
 * decode single quotes, bit 2 = double quotes (ENT_QUOTES=3, ENT_COMPAT=2,
 * ENT_NOQUOTES=0); bits 16|32 select the doctype (0=HTML401, 16=XML1,
 * 32=XHTML, 48=HTML5); ENT_IGNORE=4 drops invalid UTF-8 bytes (wins over
 * ENT_SUBSTITUTE=8, which replaces each with U+FFFD; with neither set the
 * whole result collapses to ""); ENT_DISALLOWED=128 substitutes valid but
 * doctype-disallowed codepoints. The shared default is
 * ENT_QUOTES|ENT_SUBSTITUTE|ENT_HTML401 = 11.
 */
static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bAll,int bDoubleEncode);
static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bFull);
static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx);
static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags);
/*
 * string htmlspecialchars(string $string [, int $flags = ENT_QUOTES|ENT_SUBSTITUTE|ENT_HTML401
 *                         [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])
 *  Convert the special characters & < > " ' to HTML entities.
 * Return
 *  The escaped string or NULL on failure.
 */
static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES|ENT_SUBSTITUTE|ENT_HTML401 */
	const char *zIn;
	int nLen,bDouble = 1;
	/* php coerces a scalar argument to string here (weak mode); the shared ZPP
	 * screen in vm.c has already rejected the values that cannot coerce. */
	if( nArg < 1 ){
		/* Missing/Invalid arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn = ph7_value_to_string(apArg[0],&nLen);
	if( nArg > 1 ){
		iFlags = ph7_value_to_int(apArg[1]);
	}
	HtmlCheckCharset(pCtx,nArg,apArg,2);
	if( nArg > 3 ){
		bDouble = ph7_value_to_bool(apArg[3]);
	}
	HtmlEscape(pCtx,zIn,nLen,iFlags,0,bDouble);
	return PH7_OK;
}
/*
 * string htmlspecialchars_decode(string $string [, int $flags = ENT_QUOTES|ENT_SUBSTITUTE|ENT_HTML401])
 *  Convert the special HTML entities (&amp; &lt; &gt; &quot; and the
 *  numeric/doctype forms of the two quotes) back to characters.
 * Return
 *  The unescaped string or NULL on failure.
 */
static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES|ENT_SUBSTITUTE|ENT_HTML401 */
	const char *zIn;
	int nLen;
	/* php coerces a scalar argument to string here (weak mode); the shared ZPP
	 * screen in vm.c has already rejected the values that cannot coerce. */
	if( nArg < 1 ){
		/* Missing/Invalid arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn = ph7_value_to_string(apArg[0],&nLen);
	if( nArg > 1 ){
		iFlags = ph7_value_to_int(apArg[1]);
	}
	HtmlUnescape(pCtx,zIn,nLen,iFlags,0);
	return PH7_OK;
}
/*
 * array get_html_translation_table(int $table = HTML_SPECIALCHARS
 *      [, int $flags = ENT_QUOTES|ENT_SUBSTITUTE|ENT_HTML401 [, string $encoding = "UTF-8"]])
 *  Return the translation table used by htmlspecialchars() (HTML_SPECIALCHARS)
 *  or htmlentities() (HTML_ENTITIES) as character => entity pairs.
 * Return
 *  The translation table as an array or NULL on failure.
 */
static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int iTable = 0; /* HTML_SPECIALCHARS */
	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES|ENT_SUBSTITUTE|ENT_HTML401 */
	if( nArg > 0 ){
		iTable = ph7_value_to_int(apArg[0]);
	}
	if( nArg > 1 ){
		iFlags = ph7_value_to_int(apArg[1]);
	}
	HtmlCheckCharset(pCtx,nArg,apArg,2);
	HtmlTranslationTable(pCtx,iTable,iFlags);
	return PH7_OK;
}
/*
 * string htmlentities(string $string [, int $flags = ENT_QUOTES|ENT_SUBSTITUTE|ENT_HTML401
 *                     [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])
 *  Convert all applicable characters to HTML entities: the specials plus
 *  every codepoint with an HTML 4.01 named entity (aHtml401Ent[]).
 * Return
 *  The encoded string or NULL on failure.
 */
static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES|ENT_SUBSTITUTE|ENT_HTML401 */
	const char *zIn;
	int nLen,bDouble = 1;
	/* php coerces a scalar argument to string here (weak mode); the shared ZPP
	 * screen in vm.c has already rejected the values that cannot coerce. */
	if( nArg < 1 ){
		/* Missing/Invalid arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn = ph7_value_to_string(apArg[0],&nLen);
	if( nArg > 1 ){
		iFlags = ph7_value_to_int(apArg[1]);
	}
	HtmlCheckCharset(pCtx,nArg,apArg,2);
	if( nArg > 3 ){
		bDouble = ph7_value_to_bool(apArg[3]);
	}
	HtmlEscape(pCtx,zIn,nLen,iFlags,1,bDouble);
	return PH7_OK;
}
/*
 * string html_entity_decode(string $string [, int $flags = ENT_QUOTES|ENT_SUBSTITUTE|ENT_HTML401
 *                           [, string $encoding = "UTF-8"]])
 *  Convert HTML entities (named — case-sensitive — and numeric, decimal or
 *  hex) back to their UTF-8 characters. The reverse of htmlentities().
 * Return
 *  The decoded string or NULL on failure.
 */
static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES|ENT_SUBSTITUTE|ENT_HTML401 */
	const char *zIn;
	int nLen;
	/* php coerces a scalar argument to string here (weak mode); the shared ZPP
	 * screen in vm.c has already rejected the values that cannot coerce. */
	if( nArg < 1 ){
		/* Missing/Invalid arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn = ph7_value_to_string(apArg[0],&nLen);
	if( nArg > 1 ){
		iFlags = ph7_value_to_int(apArg[1]);
	}
	HtmlCheckCharset(pCtx,nArg,apArg,2);
	HtmlUnescape(pCtx,zIn,nLen,iFlags,1);
	return PH7_OK;
}
/*
 * int strlen($string)
 *  return the length of the given string.
 * Parameter
 *  string: The string being measured for length.
 * Return
 *  length of the given string.
 */
static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int iLen = 0;
	if( nArg > 0 ){
		StrNullArgNotice(pCtx,apArg[0],"strlen",1,"$string");
		ph7_value_to_string(apArg[0],&iLen);
	}
	/* String length */
	ph7_result_int(pCtx,iLen);
	return PH7_OK;
}
/*
 * int strcmp(string $str1,string $str2)
 *  Perform a binary safe string comparison.
 * Parameter
 *  str1: The first string
 *  str2: The second string
 * Return
 *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater
 *  than str2, and 0 if they are equal.
 */
static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *z1,*z2;
	int n1,n2;
	int res;
	if( nArg < 2 ){
		res = nArg == 0 ? 0 : 1;
		ph7_result_int(pCtx,res);
		return PH7_OK;
	}
	/* Perform the comparison */
	z1 = ph7_value_to_string(apArg[0],&n1);
	z2 = ph7_value_to_string(apArg[1],&n2);
	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));
	/* Comparison result */
	ph7_result_int(pCtx,res);
	return PH7_OK;
}
/*
 * Natural-order comparison core (Martin Pool's natcompare as adapted by php's
 * ext/standard/strnatcmp.c): digit runs compare numerically — the longer run
 * wins, a leading zero flips to fractional first-difference-wins semantics —
 * everything else compares bytewise with whitespace skipped.
 */
static int StrNatCompareRight(const char **pa,const char *aEnd,const char **pb,const char *bEnd)
{
	int bias = 0;
	for(;;){
		int da = (*pa < aEnd) && SyisDigit(**pa);
		int db = (*pb < bEnd) && SyisDigit(**pb);
		if( !da && !db ){ return bias; }
		if( !da ){ return -1; }
		if( !db ){ return 1; }
		if( **pa < **pb ){ if( !bias ){ bias = -1; } }
		else if( **pa > **pb ){ if( !bias ){ bias = 1; } }
		(*pa)++;
		(*pb)++;
	}
}
static int StrNatCompareLeft(const char **pa,const char *aEnd,const char **pb,const char *bEnd)
{
	for(;;){
		int da = (*pa < aEnd) && SyisDigit(**pa);
		int db = (*pb < bEnd) && SyisDigit(**pb);
		if( !da && !db ){ return 0; }
		if( !da ){ return -1; }
		if( !db ){ return 1; }
		if( **pa < **pb ){ return -1; }
		if( **pa > **pb ){ return 1; }
		(*pa)++;
		(*pb)++;
	}
}
static int StrNatCmpCore(const char *zA,int nA,const char *zB,int nB,int bFold)
{
	const char *a = zA,*aEnd = &zA[nA];
	const char *b = zB,*bEnd = &zB[nB];
	for(;;){
		int ca,cb;
		while( a < aEnd && SyisSpace(a[0]) ){ a++; }
		while( b < bEnd && SyisSpace(b[0]) ){ b++; }
		ca = (a < aEnd) ? (unsigned char)a[0] : 0;
		cb = (b < bEnd) ? (unsigned char)b[0] : 0;
		if( SyisDigit(ca) && SyisDigit(cb) ){
			int r = (ca == '0' || cb == '0')
				? StrNatCompareLeft(&a,aEnd,&b,bEnd)
				: StrNatCompareRight(&a,aEnd,&b,bEnd);
			if( r ){ return r; }
			continue;
		}
		if( ca == 0 && cb == 0 ){ return 0; }
		if( bFold ){
			ca = SyToLower(ca);
			cb = SyToLower(cb);
		}
		if( ca < cb ){ return -1; }
		if( ca > cb ){ return 1; }
		a++;
		b++;
	}
}
/*
 * int strnatcmp(string $string1, string $string2)
 * int strnatcasecmp(string $string1, string $string2)
 *  Natural-order string comparison ("img2" < "img10"), case folded for the
 *  latter. php 8.2+ normalizes the result to -1/0/1.
 */
static int PH7_builtin_strnatcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *z1,*z2,*zFunc;
	int n1,n2,bFold;
	if( nArg < 2 ){
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	zFunc = ph7_function_name(pCtx);
	bFold = zFunc[sizeof("strnat")-1] == 'c'; /* strnatCasecmp */
	z1 = ph7_value_to_string(apArg[0],&n1);
	z2 = ph7_value_to_string(apArg[1],&n2);
	ph7_result_int(pCtx,StrNatCmpCore(z1,n1,z2,n2,bFold));
	return PH7_OK;
}
/*
 * int strncmp(string $str1,string $str2,int n)
 *  Perform a binary safe string comparison of the first n characters.
 * Parameter
 *  str1: The first string
 *  str2: The second string
 * Return
 *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater
 *  than str2, and 0 if they are equal.
 */
static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *z1,*z2;
	int res;
	int n;
	if( nArg < 3 ){
		/* Perform a standard comparison */
		return PH7_builtin_strcmp(pCtx,nArg,apArg);
	}
	/* Desired comparison length */
	n  = ph7_value_to_int(apArg[2]);
	if( n < 0 ){
		/* PHP 8 throws a catchable ValueError for a negative length. */
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #3 ($length) must be greater than or equal to 0",
			ph7_function_name(pCtx));
	}
	/* Perform the comparison */
	z1 = ph7_value_to_string(apArg[0],0);
	z2 = ph7_value_to_string(apArg[1],0);
	res = SyStrncmp(z1,z2,(sxu32)n);
	/* Comparison result */
	ph7_result_int(pCtx,res);
	return PH7_OK;
}
/*
 * int strcasecmp(string $str1,string $str2,int n)
 *  Perform a binary safe case-insensitive string comparison.
 * Parameter
 *  str1: The first string
 *  str2: The second string
 * Return
 *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater
 *  than str2, and 0 if they are equal.
 */
static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *z1,*z2;
	int n1,n2;
	int res;
	if( nArg < 2 ){
		res = nArg == 0 ? 0 : 1;
		ph7_result_int(pCtx,res);
		return PH7_OK;
	}
	/* Perform the comparison */
	z1 = ph7_value_to_string(apArg[0],&n1);
	z2 = ph7_value_to_string(apArg[1],&n2);
	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));
	/* Comparison result */
	ph7_result_int(pCtx,res);
	return PH7_OK;
}
/*
 * int strncasecmp(string $str1,string $str2,int n)
 *  Perform a binary safe case-insensitive string comparison of the first n characters.
 * Parameter
 *  $str1: The first string
 *  $str2: The second string
 *  $len:  The length of strings to be used in the comparison.
 * Return
 *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater
 *  than str2, and 0 if they are equal.
 */
static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *z1,*z2;
	int res;
	int n;
	if( nArg < 3 ){
		/* Perform a standard comparison */
		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);
	}
	/* Desired comparison length */
	n  = ph7_value_to_int(apArg[2]);
	if( n < 0 ){
		/* PHP 8 throws a catchable ValueError for a negative length. */
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #3 ($length) must be greater than or equal to 0",
			ph7_function_name(pCtx));
	}
	/* Perform the comparison */
	z1 = ph7_value_to_string(apArg[0],0);
	z2 = ph7_value_to_string(apArg[1],0);
	res = SyStrnicmp(z1,z2,(sxu32)n);
	/* Comparison result */
	ph7_result_int(pCtx,res);
	return PH7_OK;
}
/*
 * Implode context [i.e: it's private data].
 * A pointer to the following structure is forwarded
 * verbatim to the array walker callback defined below.
 */
struct implode_data {
	ph7_context *pCtx;    /* Call context */
	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */
	const char *zSep;     /* Arguments separator if any */
	int nSeplen;          /* Separator length */
	int bFirst;           /* TRUE if first call */
	int nRecCount;        /* Recursion count to avoid infinite loop */
	sxi32 rc;             /* Captured allocation rc; SXERR_MEM => the builtin raises an OOM fatal */
};
/*
 * Implode walker callback for the [ph7_array_walk()] interface.
 * The following routine is invoked for each array entry passed
 * to the implode() function.
 */
static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)
{
	SXUNUSED(pKey);
	struct implode_data *pData = (struct implode_data *)pUserData;
	const char *zData;
	int nLen;
	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){
		if( pData->nSeplen > 0 ){
			if( !pData->bFirst ){
				/* append the separator first */
				if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){
					pData->rc = SXERR_MEM;
					return PH7_ABORT;
				}
			}else{
				pData->bFirst = 0;
			}
		}
		/* Recurse */
		pData->bFirst = 1;
		pData->nRecCount++;
		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);
		pData->nRecCount--;
		/* Propagate an allocation failure surfaced deeper in the recursion. */
		if( pData->rc != SXRET_OK ){
			return PH7_ABORT;
		}
		return PH7_OK;
	}
	/* Extract the string representation of the entry value */
	zData = ph7_value_to_string(pValue,&nLen);
	/* Manage separator insertion: always mark first seen; append separator for subsequent items */
	if( pData->bFirst ){
		pData->bFirst = 0;
	}else if( pData->nSeplen > 0 ){
		/* append the separator first */
		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){
			pData->rc = SXERR_MEM;
			return PH7_ABORT;
		}
	}
	/* Append the value if non-empty; empty values are represented by the separators */
	if( nLen > 0 ){
		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){
			pData->rc = SXERR_MEM;
			return PH7_ABORT;
		}
	}
	return PH7_OK;
}
/*
 * string implode(string $glue,array $pieces,...)
 * string implode(array $pieces,...)
 *  Join array elements with a string.
 * $glue
 *   Defaults to an empty string. This is not the preferred usage of implode() as glue
 *   would be the second parameter and thus, the bad prototype would be used.
 * $pieces
 *   The array of strings to implode.
 * Return
 *  Returns a string containing a string representation of all the array elements in the same
 *  order, with the glue string between each element.
 */
static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	struct implode_data imp_data;
	int i = 1;
	if( nArg < 1 ){
		/* Missing argument,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Prepare the implode context */
	imp_data.pCtx = pCtx;
	imp_data.bRecursive = 0;
	imp_data.bFirst = 1;
	imp_data.nRecCount = 0;
	imp_data.rc = SXRET_OK;
	if( !ph7_value_is_array(apArg[0]) ){
		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);
	}else{
		imp_data.zSep = 0;
		imp_data.nSeplen = 0;
		i = 0;
	}
	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */
		return PH7_ContextMemoryError(pCtx);
	}
	/* Start the 'join' process */
	while( i < nArg ){
		if( ph7_value_is_array(apArg[i]) ){
			/* Iterate throw array entries */
			ph7_array_walk(apArg[i],implode_callback,&imp_data);
			/* Surface a callback allocation failure as a fatal */
			if( imp_data.rc != SXRET_OK ){
				return PH7_ContextMemoryError(pCtx);
			}
		}else{
			const char *zData;
			int nLen;
			/* Extract the string representation of the ph7 value */
			zData = ph7_value_to_string(apArg[i],&nLen);
			/* Manage separator insertion regardless of string length */
			if( imp_data.bFirst ){
				imp_data.bFirst = 0;
			}else if( imp_data.nSeplen > 0 ){
				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){
					return PH7_ContextMemoryError(pCtx);
				}
			}
			/* Append the value if non-empty; empty values are represented by the separators */
			if( nLen > 0 ){
				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){
					return PH7_ContextMemoryError(pCtx);
				}
			}
		}
		i++;
	}
	return PH7_OK;
}
/*
 * Symisc eXtension:
 * string implode_recursive(string $glue,array $pieces,...)
 * Purpose
 *  Same as implode() but recurse on arrays.
 * Example:
 *   $a = array('usr',array('home','dean'));
 *   echo implode_recursive("/",$a);
 *   Will output
 *     usr/home/dean.
 *   While the standard implode would produce.
 *    usr/Array.
 * Parameter
 *  Refer to implode().
 * Return
 *  Refer to implode().
 */
static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	struct implode_data imp_data;
	int i = 1;
	if( nArg < 1 ){
		/* Missing argument,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Prepare the implode context */
	imp_data.pCtx = pCtx;
	imp_data.bRecursive = 1;
	imp_data.bFirst = 1;
	imp_data.nRecCount = 0;
	imp_data.rc = SXRET_OK;
	if( !ph7_value_is_array(apArg[0]) ){
		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);
	}else{
		imp_data.zSep = 0;
		imp_data.nSeplen = 0;
		i = 0;
	}
	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */
		return PH7_ContextMemoryError(pCtx);
	}
	/* Start the 'join' process */
	while( i < nArg ){
		if( ph7_value_is_array(apArg[i]) ){
			/* Iterate throw array entries */
			ph7_array_walk(apArg[i],implode_callback,&imp_data);
			/* Surface a callback allocation failure as a fatal */
			if( imp_data.rc != SXRET_OK ){
				return PH7_ContextMemoryError(pCtx);
			}
		}else{
			const char *zData;
			int nLen;
			/* Extract the string representation of the ph7 value */
			zData = ph7_value_to_string(apArg[i],&nLen);
			/* Manage separator insertion regardless of string length */
			if( imp_data.bFirst ){
				imp_data.bFirst = 0;
			}else if( imp_data.nSeplen > 0 ){
				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){
					return PH7_ContextMemoryError(pCtx);
				}
			}
			/* Append the value if non-empty; empty values are represented by the separators */
			if( nLen > 0 ){
				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){
					return PH7_ContextMemoryError(pCtx);
				}
			}
		}
		i++;
	}
	return PH7_OK;
}
/*
 * array explode(string $delimiter,string $string[,int $limit ])
 *  Returns an array of strings, each of which is a substring of string
 *  formed by splitting it on boundaries formed by the string delimiter.
 * Parameters
 *  $delimiter
 *   The boundary string.
 * $string
 *   The input string.
 * $limit
 *   If limit is set and positive, the returned array will contain a maximum
 *   of limit elements with the last element containing the rest of string.
 *   If the limit parameter is negative, all fields except the last -limit are returned.
 *   If the limit parameter is zero, then this is treated as 1.
 * Returns
 *  Returns an array of strings created by splitting the string parameter
 *  on boundaries formed by the delimiter.
 *  If delimiter is an empty string (""), explode() will return FALSE.
 *  If delimiter contains a value that is not contained in string and a negative
 *  limit is used, then an empty array will be returned, otherwise an array containing string
 *  will be returned.
 * NOTE:
 *  Negative limit is not supported.
 */
static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zDelim,*zString,*zCur,*zEnd;
	int nDelim,nStrlen,iLimit;
	ph7_value *pArray;
	ph7_value *pValue;
	sxu32 nOfft;
	sxi32 rc;
	if( nArg < 2 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the delimiter */
	zDelim = ph7_value_to_string(apArg[0],&nDelim);
	if( nDelim < 1 ){
		/* Empty delimiter: PHP 8 throws a catchable ValueError. */
		return PH7_VmThrowException(pCtx,"ValueError",
			"explode(): Argument #1 ($separator) must not be empty");
	}
	/* Extract the string */
	zString = ph7_value_to_string(apArg[1],&nStrlen);
	if( nStrlen < 1 ){
		/* Empty string: normally an array with a single empty element (PHP behavior).
		 * A negative limit drops the last -limit components, so the sole empty
		 * component is dropped and the result is an empty array. */
		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);
		if( pArrayTmp == 0 ){
			/* Out of memory,return FALSE */
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		if( !(nArg > 2 && ph7_value_to_int(apArg[2]) < 0) ){
			ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);
			if( pValueTmp == 0 ){
				/* Out of memory,return FALSE */
				ph7_result_bool(pCtx,0);
				return PH7_OK;
			}
			ph7_value_string(pValueTmp, "", 0);
			if( ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp) != SXRET_OK ){
				return PH7_ContextMemoryError(pCtx);
			}
		}
		ph7_result_value(pCtx, pArrayTmp);
		return PH7_OK;
	}
	/* Point to the end of the string */
	zEnd = &zString[nStrlen];
	/* Create the array */
	pArray =  ph7_context_new_array(pCtx);
	pValue = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pValue == 0 ){
		/* Out of memory,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Set a defualt limit */
	iLimit = SXI32_HIGH;
	if( nArg > 2 ){
		iLimit = ph7_value_to_int(apArg[2]);
		if( iLimit < 0 ){
			/* Negative limit: keep all components except the last -iLimit (PHP).
			 * Pre-count the components (delimiters + 1), then emit only the first
			 * nKeep CLEAN components — no trailing-remainder merge (the difference
			 * from the positive path). nKeep <= 0 drops everything -> empty array. */
			int nTotal = 1,nKeep;
			const char *zScan = zString;
			sxu32 nScanOfft;
			while( SyBlobSearch(zScan,(sxu32)(zEnd - zScan),zDelim,nDelim,&nScanOfft) == SXRET_OK ){
				nTotal++;
				zScan = &zScan[nScanOfft + nDelim];
			}
			nKeep = nTotal + iLimit; /* iLimit < 0, so this is nTotal - (-iLimit) */
			while( nKeep > (int)ph7_array_count(pArray)
				&& SyBlobSearch(zString,(sxu32)(zEnd - zString),zDelim,nDelim,&nOfft) == SXRET_OK ){
				/* Emit the next clean component */
				zCur = &zString[nOfft];
				ph7_value_string(pValue, zString, (int)(zCur - zString));
				if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){
					return PH7_ContextMemoryError(pCtx);
				}
				zString = &zCur[nDelim];
				ph7_value_reset_string_cursor(pValue);
			}
			ph7_result_value(pCtx,pArray);
			return PH7_OK;
		}
		if( iLimit == 0 ){
			iLimit = 1;
		}
		iLimit--;
	}
	/* Start exploding */
	for(;;){
		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);
		if( rc != SXRET_OK || iLimit <= (int)ph7_array_count(pArray) ){
			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */
			ph7_value_string(pValue, zString, (int)(zEnd - zString));
			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){
				return PH7_ContextMemoryError(pCtx);
			}
			break;
		}
		/* Point to the desired offset */
		zCur = &zString[nOfft];
		/* Perform the store operation (may be empty) */
		ph7_value_string(pValue, zString, (int)(zCur - zString));
		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){
			return PH7_ContextMemoryError(pCtx);
		}
		/* Point beyond the delimiter */
		zString = &zCur[nDelim];
		/* Reset the cursor */
		ph7_value_reset_string_cursor(pValue);
	}
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	/* NOTE that every allocated ph7_value will be automatically
	 * released as soon we return from this foregin function.
	 */
	return PH7_OK;
}
/*
 * string trim(string $str[,string $charlist ])
 *  Strip whitespace (or other characters) from the beginning and end of a string.
 * Parameters
 *  $str
 *   The string that will be trimmed.
 * $charlist
 *   Optionally, the stripped characters can also be specified using the charlist parameter.
 *   Simply list all characters that you want to be stripped.
 *   With .. you can specify a range of characters.
 * Returns.
 *  Thr processed string.
 * NOTE:
 *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).
 */
static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"trim",1,"$string"); }
	const char *zString;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zString = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string,return */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Start the trim process */
	if( nArg < 2 ){
		SyString sStr;
		/* Remove white spaces and NUL bytes */
		SyStringInitFromBuf(&sStr,zString,nLen);
		SyStringFullTrimSafe(&sStr);
		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);
	}else{
		/* Char list */
		const char *zList;
		int nListlen;
		zList = ph7_value_to_string(apArg[1],&nListlen);
		if( nListlen < 1 ){
			/* Return the string unchanged */
			ph7_result_string(pCtx,zString,nLen);
		}else{
			char aMask[256];
			const char *zEnd = &zString[nLen];
			const char *zCur = zString;
			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);
			/* Left trim */
			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){
				zCur++;
			}
			/* Right trim */
			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){
				zEnd--;
			}
			if( zCur >= zEnd ){
				/* Return the empty string */
				ph7_result_string(pCtx,"",0);
			}else{
				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));
			}
		}
	}
	return PH7_OK;
}
/*
 * string rtrim(string $str[,string $charlist ])
 *  Strip whitespace (or other characters) from the end of a string.
 * Parameters
 *  $str
 *   The string that will be trimmed.
 * $charlist
 *   Optionally, the stripped characters can also be specified using the charlist parameter.
 *   Simply list all characters that you want to be stripped.
 *   With .. you can specify a range of characters.
 * Returns.
 *  Thr processed string.
 * NOTE:
 *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).
 */
static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"rtrim",1,"$string"); }
	const char *zString;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zString = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string,return */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Start the trim process */
	if( nArg < 2 ){
		SyString sStr;
		/* Remove white spaces and NUL bytes*/
		SyStringInitFromBuf(&sStr,zString,nLen);
		SyStringRightTrimSafe(&sStr);
		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);
	}else{
		/* Char list */
		const char *zList;
		int nListlen;
		zList = ph7_value_to_string(apArg[1],&nListlen);
		if( nListlen < 1 ){
			/* Return the string unchanged */
			ph7_result_string(pCtx,zString,nLen);
		}else{
			char aMask[256];
			const char *zEnd = &zString[nLen];
			const char *zCur = zString;
			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);
			/* Right trim */
			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){
				zEnd--;
			}
			if( zEnd <= zCur ){
				/* Return the empty string */
				ph7_result_string(pCtx,"",0);
			}else{
				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));
			}
		}
	}
	return PH7_OK;
}
/*
 * string ltrim(string $str[,string $charlist ])
 *  Strip whitespace (or other characters) from the beginning and end of a string.
 * Parameters
 *  $str
 *   The string that will be trimmed.
 * $charlist
 *   Optionally, the stripped characters can also be specified using the charlist parameter.
 *   Simply list all characters that you want to be stripped.
 *   With .. you can specify a range of characters.
 * Returns.
 *  Thr processed string.
 * NOTE:
 *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).
 */
static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"ltrim",1,"$string"); }
	const char *zString;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zString = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string,return */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Start the trim process */
	if( nArg < 2 ){
		SyString sStr;
		/* Remove white spaces and NUL byte */
		SyStringInitFromBuf(&sStr,zString,nLen);
		SyStringLeftTrimSafe(&sStr);
		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);
	}else{
		/* Char list */
		const char *zList;
		int nListlen;
		zList = ph7_value_to_string(apArg[1],&nListlen);
		if( nListlen < 1 ){
			/* Return the string unchanged */
			ph7_result_string(pCtx,zString,nLen);
		}else{
			char aMask[256];
			const char *zEnd = &zString[nLen];
			const char *zCur = zString;
			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);
			/* Left trim */
			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){
				zCur++;
			}
			if( zCur >= zEnd ){
				/* Return the empty string */
				ph7_result_string(pCtx,"",0);
			}else{
				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));
			}
		}
	}
	return PH7_OK;
}
/*
 * string strtolower(string $str)
 *  Make a string lowercase.
 * Parameters
 *  $str
 *   The input string.
 * Returns.
 *  The lowercased string.
 */
static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtolower",1,"$string"); }
	const char *zString,*zCur,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zString = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string,return */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	zEnd = &zString[nLen];
	for(;;){
		if( zString >= zEnd ){
			/* No more input,break immediately */
			break;
		}
		if( (unsigned char)zString[0] >= 0xc0 ){
			/* UTF-8 stream,output verbatim */
			zCur = zString;
			zString++;
			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){
				zString++;
			}
			/* Append UTF-8 stream */
			ph7_result_string(pCtx,zCur,(int)(zString-zCur));
		}else{
			int c = zString[0];
			if( SyisUpper(c) ){
				c = SyToLower(zString[0]);
			}
			/* Append character */
			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));
			/* Advance the cursor */
			zString++;
		}
	}
	return PH7_OK;
}
/*
 * string strtolower(string $str)
 *  Make a string uppercase.
 * Parameters
 *  $str
 *   The input string.
 * Returns.
 *  The uppercased string.
 */
static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtoupper",1,"$string"); }
	const char *zString,*zCur,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zString = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string,return */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	zEnd = &zString[nLen];
	for(;;){
		if( zString >= zEnd ){
			/* No more input,break immediately */
			break;
		}
		if( (unsigned char)zString[0] >= 0xc0 ){
			/* UTF-8 stream,output verbatim */
			zCur = zString;
			zString++;
			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){
				zString++;
			}
			/* Append UTF-8 stream */
			ph7_result_string(pCtx,zCur,(int)(zString-zCur));
		}else{
			int c = zString[0];
			if( SyisLower(c) ){
				c = SyToUpper(zString[0]);
			}
			/* Append character */
			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));
			/* Advance the cursor */
			zString++;
		}
	}
	return PH7_OK;
}
/*
 * string ucfirst(string $str)
 *  Returns a string with the first character of str capitalized, if that
 *  character is alphabetic.
 * Parameters
 *  $str
 *   The input string.
 * Returns.
 *  The processed string.
 */
static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString,*zEnd;
	int nLen,c;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zString = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string,return */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	zEnd = &zString[nLen];
	c = zString[0];
	if( SyisLower(c) ){
		c = SyToUpper(c);
	}
	/* Append the first character */
	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));
	zString++;
	if( zString < zEnd ){
		/* Append the rest of the input verbatim */
		ph7_result_string(pCtx,zString,(int)(zEnd-zString));
	}
	return PH7_OK;
}
/*
 * string lcfirst(string $str)
 *  Make a string's first character lowercase.
 * Parameters
 *  $str
 *   The input string.
 * Returns.
 *  The processed string.
 */
static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString,*zEnd;
	int nLen,c;
	if( nArg < 1 ){
		/* Missing arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zString = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string,return */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	zEnd = &zString[nLen];
	c = zString[0];
	if( SyisUpper(c) ){
		c = SyToLower(c);
	}
	/* Append the first character */
	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));
	zString++;
	if( zString < zEnd ){
		/* Append the rest of the input verbatim */
		ph7_result_string(pCtx,zString,(int)(zEnd-zString));
	}
	return PH7_OK;
}
/*
 * int ord(string $string)
 *  Returns the ASCII value of the first character of string.
 *  Passing null, an empty string, or a multi-byte string emits
 *  E_DEPRECATED to match PHP 8.4+ behaviour.
 * Parameters
 *  $string
 *   The input string.
 * Returns
 *  The ASCII value as an integer.
 */
static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString;
	int nLen,c;
	/* PHP requires exactly one argument. */
	if( nArg != 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"ord() expects exactly 1 argument, %d given",
			nArg
			);
	}
	/* Passing null is deprecated (E_DEPRECATED).  PHP emits this before
	 * the empty-string deprecation, so we check null first. */
	if( ph7_value_is_null(apArg[0]) ){
		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,
			"ord(): Passing null to parameter #1 ($character) "
			"of type string is deprecated"
			);
	}
	/* Extract the target string */
	zString = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string is deprecated (E_DEPRECATED). */
		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,
			"ord(): Providing an empty string is deprecated"
			);
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* A string longer than one byte is deprecated (E_DEPRECATED). */
	if( nLen > 1 ){
		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,
			"ord(): Providing a string that is not one byte long "
			"is deprecated. Use ord($str[0]) instead"
			);
	}
	/* Extract the ASCII value of the first character */
	c = (unsigned char)zString[0];
	/* Return that value */
	ph7_result_int(pCtx,c);
	return PH7_OK;
}
/*
 * string chr(int $codepoint)
 *  Returns a one-character string containing the character specified
 *  by the given codepoint.  Any integer is accepted; values outside
 *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.
 * Parameters
 *  $codepoint
 *   An integer codepoint.  Values outside 0-255 are deprecated and
 *   will be constrained to a single byte.
 * Returns
 *  A single-character string.
 */
static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int c;
	unsigned char ch;
	/* PHP requires exactly one argument. */
	if( nArg != 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"chr() expects exactly 1 argument, %d given",
			nArg
			);
	}
	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).
	 * PHP does not prefix this message with "chr():", so we call
	 * PH7_VmThrowError() with a NULL function name to avoid the
	 * automatic prefix that ph7_context_throw_error*() would add. */
	if( ph7_value_is_float(apArg[0]) ){
		char zBuf[120];
		SyBufferFormat(zBuf,sizeof(zBuf),
			"Implicit conversion from float %g to int loses precision",
			ph7_value_to_double(apArg[0])
			);
		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);
	}
	/* Extract the codepoint. */
	c = ph7_value_to_int(apArg[0]);
	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.
	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,
	 * so we embed the prefix in the message and pass NULL as the function
	 * name to avoid the API double-prefixing it. */
	if( c < 0 || c > 255 ){
		PH7_VmThrowError(pCtx->pVm,0,
			E_DEPRECATED,
			"chr(): Providing a value not in-between 0 and 255 is deprecated, "
			"this is because a byte value must be in the [0, 255] interval. "
			"The value used will be constrained using % 256"
			);
	}
	/* Store in an unsigned char to avoid endian-dependent behaviour
	 * when taking the address of a wider int. */
	ch = (unsigned char)(c & 0xFF);
	/* Return the specified character */
	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));
	return PH7_OK;
}
/*
 * Binary to hex consumer callback.
 * This callback is the default consumer used by the hash functions
 * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.
 */
static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)
{
	/* Append hex chunk verbatim */
	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);
	return SXRET_OK;
}

/*
 * string bin2hex(string $str)
 *  Convert binary data into hexadecimal representation.
 * Parameters
 *  $str
 *   The input string.
 * Returns.
 *  Returns the hexadecimal representation of the given string.
 */
static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString;
	int nLen;
	/* PHP 8 requires exactly one argument (ArgumentCountError). */
	if( nArg != 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"bin2hex() expects exactly 1 argument, %d given",
			nArg
			);
	}
	/* In PHP 8, bin2hex() is strict about its parameter type.
	 * Array/Resource values are not allowed and trigger a TypeError.
	 * Objects without __toString() must also raise a TypeError.
	 */
	if( ph7_value_is_array(apArg[0]) || ph7_value_is_resource(apArg[0]) ||
		( ph7_value_is_object(apArg[0]) &&
		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&
		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,
			"__toString",sizeof("__toString")-1) == 0
		)
	){
		const char *zType = ph7_type_name(apArg[0]);
		if( ph7_value_is_object(apArg[0]) ){
			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;
			if( pInst && pInst->pClass ){
				zType = SyStringData(&pInst->pClass->sName);
			}
		}
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"bin2hex(): Argument #1 ($string) must be of type string, %s given",
			zType
			);
	}
	/* Extract the target string */
	zString = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string,return */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);
	return PH7_OK;
}

/* Search callback signature */
typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);
/*
 * Case-insensitive pattern match.
 * Brute force is the default search method used here.
 * This is due to the fact that brute-forcing works quite
 * well for short/medium texts on modern hardware.
 */
static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)
{
	const char *zpIn = (const char *)pPattern;
	const char *zIn = (const char *)pText;
	const char *zpEnd = &zpIn[iPatLen];
	const char *zEnd = &zIn[nLen];
	const char *zPtr,*zPtr2;
	int c,d;
	if( iPatLen > nLen ){
		/* Don't bother processing */
		return SXERR_NOTFOUND;
	}
	for(;;){
		if( zIn >= zEnd ){
			break;
		}
		c = SyToLower(zIn[0]);
		d = SyToLower(zpIn[0]);
		if( c == d ){
			zPtr   = &zIn[1];
			zPtr2  = &zpIn[1];
			for(;;){
				if( zPtr2 >= zpEnd ){
					/* Pattern found */
					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }
					return SXRET_OK;
				}
				if( zPtr >= zEnd ){
					break;
				}
				c = SyToLower(zPtr[0]);
				d = SyToLower(zPtr2[0]);
				if( c != d ){
					break;
				}
				zPtr++; zPtr2++;
			}
		}
		zIn++;
	}
	/* Pattern not found */
	return SXERR_NOTFOUND;
}
/*
 * string strstr(string $haystack,string $needle[,bool $before_needle = false ])
 *  Find the first occurrence of a string.
 * Parameters
 *  $haystack
 *   The input string.
 * $needle
 *   Search pattern (must be a string).
 * $before_needle
 *   If TRUE, strstr() returns the part of the haystack before the first occurrence
 *   of the needle (excluding the needle).
 * Return
 *  Returns the portion of string, or FALSE if needle is not found.
 */
static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */
	const char *zBlob,*zPattern;
	int nLen,nPatLen;
	sxu32 nOfft;
	sxi32 rc;
	if( nArg < 2 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the needle and the haystack */
	zBlob = ph7_value_to_string(apArg[0],&nLen);
	zPattern = ph7_value_to_string(apArg[1],&nPatLen);
	nOfft = 0; /* cc warning */
	if( nLen > 0 && nPatLen > 0 ){
		int before = 0;
		/* Perform the lookup */
		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);
		if( rc != SXRET_OK ){
			/* Pattern not found,return FALSE */
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		/* Return the portion of the string */
		if( nArg > 2 ){
			before = ph7_value_to_int(apArg[2]);
		}
		if( before ){
			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));
		}else{
			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));
		}
	}else{
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * string stristr(string $haystack,string $needle[,bool $before_needle = false ])
 *  Case-insensitive strstr().
 * Parameters
 *  $haystack
 *   The input string.
 * $needle
 *   Search pattern (must be a string).
 * $before_needle
 *   If TRUE, strstr() returns the part of the haystack before the first occurrence
 *   of the needle (excluding the needle).
 * Return
 *  Returns the portion of string, or FALSE if needle is not found.
 */
static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */
	const char *zBlob,*zPattern;
	int nLen,nPatLen;
	sxu32 nOfft;
	sxi32 rc;
	if( nArg < 2 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the needle and the haystack */
	zBlob = ph7_value_to_string(apArg[0],&nLen);
	zPattern = ph7_value_to_string(apArg[1],&nPatLen);
	nOfft = 0; /* cc warning */
	if( nLen > 0 && nPatLen > 0 ){
		int before = 0;
		/* Perform the lookup */
		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);
		if( rc != SXRET_OK ){
			/* Pattern not found,return FALSE */
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		/* Return the portion of the string */
		if( nArg > 2 ){
			before = ph7_value_to_int(apArg[2]);
		}
		if( before ){
			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));
		}else{
			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));
		}
	}else{
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * int strpos(string $haystack,string $needle [,int $offset = 0 ] )
 *  Returns the numeric position of the first occurrence of needle in the haystack string.
 * Parameters
 *  $haystack
 *   The input string.
 * $needle
 *   Search pattern (must be a string).
 * $offset
 *   This optional offset parameter allows you to specify which character in haystack
 *   to start searching. The position returned is still relative to the beginning
 *   of haystack.
 * Return
 *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.
 */
static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strpos",1,"$haystack"); }
	if( nArg > 1 ){ StrNullArgNotice(pCtx,apArg[1],"strpos",2,"$needle"); }
	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */
	const char *zBlob,*zPattern;
	int nLen,nPatLen,nStart;
	sxu32 nOfft;
	sxi32 rc;
	if( nArg < 2 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the needle and the haystack */
	zBlob = ph7_value_to_string(apArg[0],&nLen);
	zPattern = ph7_value_to_string(apArg[1],&nPatLen);
	nOfft = 0; /* cc warning */
	nStart = 0;
	/* Peek the starting offset if available */
	if( nArg > 2 ){
		nStart = ph7_value_to_int(apArg[2]);
		if( nStart < 0 ){
			nStart = -nStart;
		}
		if( nStart >= nLen ){
			/* Invalid offset */
			nStart = 0;
		}else{
			zBlob += nStart;
			nLen -= nStart;
		}
	}
	if( nLen > 0 && nPatLen > 0 ){
		/* Perform the lookup */
		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);
		if( rc != SXRET_OK ){
			/* Pattern not found,return FALSE */
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		/* Return the pattern position */
		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));
	}else{
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * Validate and resolve a single string-typed parameter for str_contains/
 * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null
 * (matching PHP 8.1+; falls through with an empty string), and throws
 * TypeError for arrays, resources, and objects without __toString.
 *
 * For objects with __toString, invokes the method directly into pTmp and
 * uses its raw byte buffer. This preserves empty results, which the
 * engine's MemObjStringValue otherwise replaces with the literal "Object".
 *
 * On success, pzOut/pnOut point at the resolved byte buffer; the buffer
 * is valid until pTmp is released or pArg is mutated.
 */
static sxi32 StrPredicateResolveArg(
	ph7_context *pCtx,
	ph7_value *pArg,
	const char *zFunc,
	int iArgNum,
	const char *zParamName,
	const char *zTypeStr, /* Declared type in the TypeError, e.g. "string" / "?string" */
	const char *zNullMsg,
	ph7_value *pTmp,
	const char **pzOut,
	int *pnOut
){
	if( ph7_value_is_null(pArg) ){
		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zNullMsg);
		*pzOut = "";
		*pnOut = 0;
		return PH7_OK;
	}
	if( ph7_value_is_array(pArg) || ph7_value_is_resource(pArg) ||
	    ( ph7_value_is_object(pArg) &&
	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&
	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,
	        "__toString",sizeof("__toString")-1) == 0
	    )
	){
		const char *zType = ph7_type_name(pArg);
		if( ph7_value_is_object(pArg) ){
			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;
			if( pInst && pInst->pClass ){
				zType = SyStringData(&pInst->pClass->sName);
			}
		}
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"%s(): Argument #%d (%s) must be of type %s, %s given",
			zFunc, iArgNum, zParamName, zTypeStr, zType
			);
	}
	if( ph7_value_is_object(pArg) ){
		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;
		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,
			"__toString",sizeof("__toString")-1);
		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);
		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);
		*pnOut = (int)SyBlobLength(&pTmp->sBlob);
		return PH7_OK;
	}
	*pzOut = ph7_value_to_string(pArg,pnOut);
	return PH7_OK;
}
/*
 * bool str_contains(string $haystack, string $needle)
 *  Determine if a string contains a given substring (PHP 8.0).
 * Return
 *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.
 */
static int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zHaystack,*zNeedle;
	int nHayLen,nNeedleLen;
	ph7_value sHayTmp,sNeedleTmp;
	sxi32 rc;
	if( nArg != 2 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"str_contains() expects exactly 2 arguments, %d given",
			nArg
			);
	}
	PH7_MemObjInit(pCtx->pVm,&sHayTmp);
	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);
	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack","string",
		"str_contains(): Passing null to parameter #1 ($haystack) "
		"of type string is deprecated",
		&sHayTmp,&zHaystack,&nHayLen);
	if( rc != PH7_OK ) goto out;
	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle","string",
		"str_contains(): Passing null to parameter #2 ($needle) "
		"of type string is deprecated",
		&sNeedleTmp,&zNeedle,&nNeedleLen);
	if( rc != PH7_OK ) goto out;
	if( nNeedleLen < 1 ){
		ph7_result_bool(pCtx,1);
	}else if( nHayLen < nNeedleLen ){
		ph7_result_bool(pCtx,0);
	}else{
		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,
		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);
		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);
	}
	rc = PH7_OK;
out:
	PH7_MemObjRelease(&sHayTmp);
	PH7_MemObjRelease(&sNeedleTmp);
	return rc;
}
/*
 * bool str_starts_with(string $haystack, string $needle)
 *  Check if a string starts with a given substring (PHP 8.0).
 * Return
 *  TRUE if haystack begins with needle. An empty needle always returns TRUE.
 *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).
 */
static int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zHaystack,*zNeedle;
	int nHayLen,nNeedleLen;
	ph7_value sHayTmp,sNeedleTmp;
	sxi32 rc;
	if( nArg != 2 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"str_starts_with() expects exactly 2 arguments, %d given",
			nArg
			);
	}
	PH7_MemObjInit(pCtx->pVm,&sHayTmp);
	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);
	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack","string",
		"str_starts_with(): Passing null to parameter #1 ($haystack) "
		"of type string is deprecated",
		&sHayTmp,&zHaystack,&nHayLen);
	if( rc != PH7_OK ) goto out;
	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle","string",
		"str_starts_with(): Passing null to parameter #2 ($needle) "
		"of type string is deprecated",
		&sNeedleTmp,&zNeedle,&nNeedleLen);
	if( rc != PH7_OK ) goto out;
	if( nNeedleLen < 1 ){
		ph7_result_bool(pCtx,1);
	}else if( nHayLen < nNeedleLen ){
		ph7_result_bool(pCtx,0);
	}else{
		ph7_result_bool(pCtx,
			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);
	}
	rc = PH7_OK;
out:
	PH7_MemObjRelease(&sHayTmp);
	PH7_MemObjRelease(&sNeedleTmp);
	return rc;
}
/*
 * bool str_ends_with(string $haystack, string $needle)
 *  Check if a string ends with a given substring (PHP 8.0).
 * Return
 *  TRUE if haystack ends with needle. An empty needle always returns TRUE.
 *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).
 */
static int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zHaystack,*zNeedle;
	int nHayLen,nNeedleLen;
	ph7_value sHayTmp,sNeedleTmp;
	sxi32 rc;
	if( nArg != 2 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"str_ends_with() expects exactly 2 arguments, %d given",
			nArg
			);
	}
	PH7_MemObjInit(pCtx->pVm,&sHayTmp);
	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);
	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack","string",
		"str_ends_with(): Passing null to parameter #1 ($haystack) "
		"of type string is deprecated",
		&sHayTmp,&zHaystack,&nHayLen);
	if( rc != PH7_OK ) goto out;
	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle","string",
		"str_ends_with(): Passing null to parameter #2 ($needle) "
		"of type string is deprecated",
		&sNeedleTmp,&zNeedle,&nNeedleLen);
	if( rc != PH7_OK ) goto out;
	if( nNeedleLen < 1 ){
		ph7_result_bool(pCtx,1);
	}else if( nHayLen < nNeedleLen ){
		ph7_result_bool(pCtx,0);
	}else{
		ph7_result_bool(pCtx,
			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);
	}
	rc = PH7_OK;
out:
	PH7_MemObjRelease(&sHayTmp);
	PH7_MemObjRelease(&sNeedleTmp);
	return rc;
}
/*
 * int stripos(string $haystack,string $needle [,int $offset = 0 ] )
 *  Case-insensitive strpos.
 * Parameters
 *  $haystack
 *   The input string.
 * $needle
 *   Search pattern (must be a string).
 * $offset
 *   This optional offset parameter allows you to specify which character in haystack
 *   to start searching. The position returned is still relative to the beginning
 *   of haystack.
 * Return
 *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.
 */
static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */
	const char *zBlob,*zPattern;
	int nLen,nPatLen,nStart;
	sxu32 nOfft;
	sxi32 rc;
	if( nArg < 2 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the needle and the haystack */
	zBlob = ph7_value_to_string(apArg[0],&nLen);
	zPattern = ph7_value_to_string(apArg[1],&nPatLen);
	nOfft = 0; /* cc warning */
	nStart = 0;
	/* Peek the starting offset if available */
	if( nArg > 2 ){
		nStart = ph7_value_to_int(apArg[2]);
		if( nStart < 0 ){
			nStart = -nStart;
		}
		if( nStart >= nLen ){
			/* Invalid offset */
			nStart = 0;
		}else{
			zBlob += nStart;
			nLen -= nStart;
		}
	}
	if( nLen > 0 && nPatLen > 0 ){
		/* Perform the lookup */
		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);
		if( rc != SXRET_OK ){
			/* Pattern not found,return FALSE */
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		/* Return the pattern position */
		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));
	}else{
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )
 *  Find the numeric position of the last occurrence of needle in the haystack string.
 * Parameters
 *  $haystack
 *   The input string.
 * $needle
 *   Search pattern (must be a string).
 * $offset
 *   If specified, search will start this number of characters counted from the beginning
 *   of the string. If the value is negative, search will instead start from that many
 *   characters from the end of the string, searching backwards.
 * Return
 *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.
 */
static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;
	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */
	int nLen,nPatLen;
	sxu32 nOfft;
	sxi32 rc;
	if( nArg < 2 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the needle and the haystack */
	zBlob = ph7_value_to_string(apArg[0],&nLen);
	zPattern = ph7_value_to_string(apArg[1],&nPatLen);
	/* Point to the end of the pattern */
	zPtr = &zBlob[nLen - 1];
	zEnd = &zBlob[nLen];
	/* Save the starting posistion */
	zStart = zBlob;
	nOfft = 0; /* cc warning */
	/* Peek the starting offset if available */
	if( nArg > 2 ){
		int nStart;
		nStart = ph7_value_to_int(apArg[2]);
		if( nStart < 0 ){
			nStart = -nStart;
			if( nStart >= nLen ){
				/* Invalid offset */
				ph7_result_bool(pCtx,0);
				return PH7_OK;
			}else{
				nLen -= nStart;
				zPtr = &zBlob[nLen - 1];
				zEnd = &zBlob[nLen];
			}
		}else{
			if( nStart >= nLen ){
				/* Invalid offset */
				ph7_result_bool(pCtx,0);
				return PH7_OK;
			}else{
				zBlob += nStart;
				nLen -= nStart;
			}
		}
	}
	if( nLen > 0 && nPatLen > 0 ){
		/* Perform the lookup */
		for(;;){
			if( zBlob >= zPtr ){
				break;
			}
			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);
			if( rc == SXRET_OK ){
				/* Pattern found,return it's position */
				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));
				return PH7_OK;
			}
			zPtr--;
		}
		/* Pattern not found,return FALSE */
		ph7_result_bool(pCtx,0);
	}else{
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * int strripos(string $haystack,string $needle [,int $offset = 0 ] )
 *  Case-insensitive strrpos.
 * Parameters
 *  $haystack
 *   The input string.
 * $needle
 *   Search pattern (must be a string).
 * $offset
 *   If specified, search will start this number of characters counted from the beginning
 *   of the string. If the value is negative, search will instead start from that many
 *   characters from the end of the string, searching backwards.
 * Return
 *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.
 */
static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;
	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */
	int nLen,nPatLen;
	sxu32 nOfft;
	sxi32 rc;
	if( nArg < 2 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the needle and the haystack */
	zBlob = ph7_value_to_string(apArg[0],&nLen);
	zPattern = ph7_value_to_string(apArg[1],&nPatLen);
	/* Point to the end of the pattern */
	zPtr = &zBlob[nLen - 1];
	zEnd = &zBlob[nLen];
	/* Save the starting posistion */
	zStart = zBlob;
	nOfft = 0; /* cc warning */
	/* Peek the starting offset if available */
	if( nArg > 2 ){
		int nStart;
		nStart = ph7_value_to_int(apArg[2]);
		if( nStart < 0 ){
			nStart = -nStart;
			if( nStart >= nLen ){
				/* Invalid offset */
				ph7_result_bool(pCtx,0);
				return PH7_OK;
			}else{
				nLen -= nStart;
				zPtr = &zBlob[nLen - 1];
				zEnd = &zBlob[nLen];
			}
		}else{
			if( nStart >= nLen ){
				/* Invalid offset */
				ph7_result_bool(pCtx,0);
				return PH7_OK;
			}else{
				zBlob += nStart;
				nLen -= nStart;
			}
		}
	}
	if( nLen > 0 && nPatLen > 0 ){
		/* Perform the lookup */
		for(;;){
			if( zBlob >= zPtr ){
				break;
			}
			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);
			if( rc == SXRET_OK ){
				/* Pattern found,return it's position */
				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));
				return PH7_OK;
			}
			zPtr--;
		}
		/* Pattern not found,return FALSE */
		ph7_result_bool(pCtx,0);
	}else{
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * int strrchr(string $haystack,mixed $needle)
 *  Find the last occurrence of a character in a string.
 * Parameters
 *  $haystack
 *   The input string.
 * $needle
 *  If needle contains more than one character, only the first is used.
 *  This behavior is different from that of strstr().
 *  If needle is not a string, it is converted to an integer and applied
 *  as the ordinal value of a character.
 * Return
 *  This function returns the portion of string, or FALSE if needle is not found.
 */
static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zBlob;
	int nLen,c;
	if( nArg < 2 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the haystack */
	zBlob = ph7_value_to_string(apArg[0],&nLen);
	c = 0; /* cc warning */
	if( nLen > 0 ){
		sxu32 nOfft;
		sxi32 rc;
		if( ph7_value_is_string(apArg[1]) ){
			const char *zPattern;
			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check
														 * for NULL pointer.
														 */
			c = zPattern[0];
		}else{
			/* Int cast */
			c = ph7_value_to_int(apArg[1]);
		}
		/* Perform the lookup */
		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);
		if( rc != SXRET_OK ){
			/* No such entry,return FALSE */
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		/* Return the string portion */
		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));
	}else{
		ph7_result_bool(pCtx,0);
	}
	return PH7_OK;
}
/*
 * string strrev(string $string)
 *  Reverse a string.
 * Parameters
 *  $string
 *   String to be reversed.
 * Return
 *  The reversed string.
 */
static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn,*zEnd;
	int nLen,c;
	if( nArg < 1 ){
		/* Missing arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string Return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Perform the requested operation */
	zEnd = &zIn[nLen - 1];
	for(;;){
		if( zEnd < zIn ){
			/* No more input to process */
			break;
		}
		/* Append current character */
		c = zEnd[0];
		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));
		zEnd--;
	}
	return PH7_OK;
}
/*
 * string ucwords(string $string [, string $separators = " \t\r\n\f\v"])
 *  Uppercase the first character of each word in a string.
 *  A word begins at the start of the string and after any character present in
 *  $separators. The default separators are the whitespace characters (space,
 *  horizontal tab, carriage return, newline, form-feed and vertical tab); an
 *  explicit $separators argument REPLACES them (an empty string leaves only the
 *  very first character upper-cased). Like PHP, this is byte-based: only ASCII
 *  bytes are upper-cased and a byte is a separator only if it appears in the set.
 * Parameters
 *  $string
 *   The input string.
 *  $separators
 *   The optional word-boundary characters.
 * Return
 *  The modified string.
 */
static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nLen,i,iStart;
	char aDelim[256];
	if( nArg < 1 ){
		/* Missing arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Build the separator membership table: an explicit $separators argument
	 * replaces the default whitespace set (an empty string clears it). */
	SyZero(aDelim,(sxu32)sizeof(aDelim));
	if( nArg > 1 ){
		int nDelim;
		const char *zDelim = ph7_value_to_string(apArg[1],&nDelim);
		for( i = 0 ; i < nDelim ; i++ ){
			aDelim[(unsigned char)zDelim[i]] = 1;
		}
	}else{
		aDelim[(unsigned char)' ']  = 1;
		aDelim[(unsigned char)'\t'] = 1;
		aDelim[(unsigned char)'\r'] = 1;
		aDelim[(unsigned char)'\n'] = 1;
		aDelim[(unsigned char)'\f'] = 1;
		aDelim[(unsigned char)'\v'] = 1;
	}
	/* Extract the target string */
	zIn = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string – match PHP semantics */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Upper-case the first byte of each word (the leading byte, or any byte that
	 * follows a separator), appending the untouched runs in between verbatim. */
	iStart = 0;
	for( i = 0 ; i < nLen ; i++ ){
		int c = (unsigned char)zIn[i];
		if( (i == 0 || aDelim[(unsigned char)zIn[i-1]]) && c < 0x80 && SyisLower(c) ){
			char up = (char)SyToUpper(c);
			if( i > iStart ){
				ph7_result_string(pCtx,&zIn[iStart],i - iStart);
			}
			ph7_result_string(pCtx,&up,1);
			iStart = i + 1;
		}
	}
	if( nLen > iStart ){
		ph7_result_string(pCtx,&zIn[iStart],nLen - iStart);
	}
	return PH7_OK;
}
/*
 * string str_repeat(string $input,int $multiplier)
 *  Returns input repeated multiplier times.
 * Parameters
 *  $string
 *   String to be repeated.
 * $multiplier
 *  Number of time the input string should be repeated.
 *  multiplier has to be greater than or equal to 0. If the multiplier is set
 *  to 0, the function will return an empty string.
 * Return
 *  The repeated string.
 */
static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nLen;
	ph7_int64 nMul;
	int rc;
	if( nArg < 2 ){
		/* Missing arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn = ph7_value_to_string(apArg[0],&nLen);
	/* Resolve $times through the shared ZPP helper so a lossy float / float-string
	 * carries php's precision deprecation and NAN/INF/non-numeric fail with php's
	 * TypeError — a bare ph7_value_to_int64() coerced them silently. */
	{
		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_repeat",2,"$times","int",&nMul);
		if( rcArg != PH7_OK ){
			return rcArg;
		}
	}
	if( nMul < 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"str_repeat(): Argument #2 ($times) must be greater than or equal to 0");
	}
	if( nLen < 1 || nMul < 1 ){
		/* Empty input or a zero multiplier yields the empty string (PHP). */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( !nMul ){
			break;
		}
		/* Append the copy */
		rc = ph7_result_string(pCtx,zIn,nLen);
		if( rc != PH7_OK ){
			/* Allocation failed: surface a fatal instead of returning a
			 * silently-truncated string with a success status. */
			return PH7_ContextMemoryError(pCtx);
		}
		nMul--;
	}
	return PH7_OK;
}
/*
 * string nl2br(string $string[,bool $is_xhtml = true ])
 *  Inserts HTML line breaks before all newlines in a string.
 * Parameters
 *  $string
 *   The input string.
 * $is_xhtml
 *   Whenever to use XHTML compatible line breaks or not.
 * Return
 *  The processed string.
 */
static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn,*zCur,*zEnd;
	int is_xhtml = 1; /* Default to XHTML-style '<br />' like PHP */
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( nArg > 1 ){
		is_xhtml = ph7_value_to_bool(apArg[1]);
	}
	zEnd = &zIn[nLen];
	/* Perform the requested operation */
	for(;;){
		zCur = zIn;
		/* Delimit the string */
		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){
			zIn++;
		}
		if( zCur < zIn ){
			/* Output chunk verbatim */
			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));
		}
		if( zIn >= zEnd ){
			/* No more input to process */
			break;
		}
		/* Output the HTML line break */
		/* Follow PHP semantics: if is_xhtml is true, use '<br />' (space before the slash), otherwise use '<br>' */
		if( is_xhtml ){
			ph7_result_string(pCtx,"<br />",(int)sizeof("<br />")-1);
		}else{
			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);
		}
		zCur = zIn;
		/* Append trailing line */
		while( zIn < zEnd && (zIn[0] == '\n'  || zIn[0] == '\r') ){
			zIn++;
		}
		if( zCur < zIn ){
			/* Output chunk verbatim */
			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));
		}
	}
	return PH7_OK;
}
/*
 * Format a given string and invoke the given callback on each processed chunk.
 *  According to the PHP reference manual.
 * The format string is composed of zero or more directives: ordinary characters
 * (excluding %) that are copied directly to the result, and conversion
 * specifications, each of which results in fetching its own parameter.
 * This applies to both sprintf() and printf().
 * Each conversion specification consists of a percent sign (%), followed by one
 * or more of these elements, in order:
 *   An optional sign specifier that forces a sign (- or +) to be used on a number.
 *   By default, only the - sign is used on a number if it's negative. This specifier forces
 *   positive numbers to have the + sign attached as well.
 *   An optional padding specifier that says what character will be used for padding
 *   the results to the right string size. This may be a space character or a 0 (zero character).
 *   The default is to pad with spaces. An alternate padding character can be specified by prefixing
 *   it with a single quote ('). See the examples below.
 *   An optional alignment specifier that says if the result should be left-justified or right-justified.
 *   The default is right-justified; a - character here will make it left-justified.
 *   An optional number, a width specifier that says how many characters (minimum) this conversion
 *   should result in.
 *   An optional precision specifier in the form of a period (`.') followed by an optional decimal
 *   digit string that says how many decimal digits should be displayed for floating-point numbers.
 *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character
 *   limit to the string.
 *  A type specifier that says what type the argument data should be treated as. Possible types:
 *       % - a literal percent character. No argument is required.
 *       b - the argument is treated as an integer, and presented as a binary number.
 *       c - the argument is treated as an integer, and presented as the character with that ASCII value.
 *       d - the argument is treated as an integer, and presented as a (signed) decimal number.
 *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands
 * 	     for the number of digits after the decimal point.
 *       E - like %e but uses uppercase letter (e.g. 1.2E+2).
 *       u - the argument is treated as an integer, and presented as an unsigned decimal number.
 *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).
 *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).
 *       g - shorter of %e and %f.
 *       G - shorter of %E and %f.
 *       o - the argument is treated as an integer, and presented as an octal number.
 *       s - the argument is treated as and presented as a string.
 *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).
 *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).
 */
#endif /* PH7_NEED_BUILTIN_REG */
#ifdef PH7_NEED_FMT_AND_INI
#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */
/*
** Conversion types fall into various categories as defined by the
** following enumeration.
*/
#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */
#define PH7_FMT_FLOAT       2 /* Floating point.%f */
#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */
#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */
#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */
#define PH7_FMT_STRING      6 /* Strings.%s */
#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */
#define PH7_FMT_CHARX       8 /* Characters.%c */
#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */

/*
** Allowed values for ph7_fmt_info.flags
*/
#define PH7_FMT_FLAG_SIGNED	  0x01
#define PH7_FMT_FLAG_UNSIGNED 0x02
/*
** Each builtin conversion character (ex: the 'd' in "%d") is described
** by an instance of the following structure
*/
typedef struct ph7_fmt_info ph7_fmt_info;
struct ph7_fmt_info
{
  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */
  sxu8 base;     /* The base for radix conversion */
  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */
  sxu8 type;     /* Conversion paradigm */
  char *charset; /* The character set for conversion */
  char *prefix;  /* Prefix on non-zero values in alt format */
};
/* PH7_PhpFloatShape (php's float-shape post-processing) lives in memobj.c —
 * the default float->string cast needs it even when this whole formatting
 * region is compiled out by PH7_DISABLE_DISK_IO. */
/*
 * The following table is searched linearly, so it is good to put the most frequently
 * used conversion types first.
 */
static const ph7_fmt_info aFmt[] = {
  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },
  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },
  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },
  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },
  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },
  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},
  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },
  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },
  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },
  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },
  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },
  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },
  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },
  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },
  /* php's 'h'/'H' are the locale-independent twins of 'g'/'G'; PHL always
   * formats in the C locale, so they behave identically. */
  {  'h',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },
  {  'H',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },
  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }
};
/*
 * PHP 8 raises a catchable ValueError for an unknown conversion specifier
 * (e.g. "%y", or the C-ism "%#x" — '#' is not a php flag). Because printf()
 * and fprintf() stream their output incrementally while sprintf() buffers it,
 * every format builtin calls PH7_FormatValidate (below) to check the whole
 * format string BEFORE formatting so the throw happens with no partial output
 * escaping (php buffers the entire result and only emits it on success). This
 * scan mirrors the specifier-locating logic of the main format loop below.
 * On the first unknown specifier, stores it in *pBad and returns TRUE; returns
 * FALSE when every specifier is known. (A found-flag rather than a sentinel
 * char, so a NUL specifier byte — "%\0" — is still reported, not mistaken for
 * "all valid".)
 */
static int FormatUnknownSpec(const char *zIn,int nByte,int *pBad)
{
	const char *zEnd = &zIn[nByte];
	int c,idx;
	while( zIn < zEnd ){
		if( zIn[0] != '%' ){
			zIn++;
			continue;
		}
		zIn++; /* jump the percent sign */
		/* php-supported flags: '-', '+', ' ', '0' and the "'<pad>'" custom-pad
		 * form. '#' is intentionally NOT treated as a flag so it surfaces as an
		 * unknown specifier, matching php. */
		while( zIn < zEnd ){
			c = zIn[0];
			if( c=='-' || c=='+' || c==' ' || c=='0' ){
				zIn++;
				continue;
			}
			if( c=='\'' ){
				zIn++;
				if( zIn < zEnd ){
					zIn++; /* the custom pad character */
				}
				continue;
			}
			break;
		}
		/* field width */
		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){
			zIn++;
		}
		/* positional specifier ($) — php parses flags AFTER it (e.g. "%1$-10s"),
		 * so skip the full flag set and width again, mirroring the main loop. */
		if( zIn < zEnd && zIn[0]=='$' ){
			zIn++;
			while( zIn < zEnd ){
				c = zIn[0];
				if( c=='-' || c=='+' || c==' ' || c=='0' ){
					zIn++;
					continue;
				}
				if( c=='\'' ){
					zIn++;
					if( zIn < zEnd ){
						zIn++;
					}
					continue;
				}
				break;
			}
			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){
				zIn++;
			}
		}
		/* precision */
		if( zIn < zEnd && zIn[0]=='.' ){
			zIn++;
			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){
				zIn++;
			}
		}
		/* a single 'l' length modifier (ignored, php compat) */
		if( zIn < zEnd && zIn[0]=='l' ){
			zIn++;
		}
		if( zIn >= zEnd ){
			/* A dangling '%' with no specifier: PHL's legacy path silently
			 * truncates here (recorded residual); nothing to validate. */
			break;
		}
		c = zIn[0];
		zIn++; /* jump the conversion specifier */
		for( idx = 0 ; idx < (int)SX_ARRAYSIZE(aFmt) ; idx++ ){
			if( c == aFmt[idx].fmttype ){
				break;
			}
		}
		if( idx >= (int)SX_ARRAYSIZE(aFmt) ){
			*pBad = c; /* unknown specifier */
			return TRUE;
		}
	}
	return FALSE;
}
/*
 * Validate a printf-style format string. PHP 8 raises a catchable ValueError for
 * an unknown conversion specifier, thrown before any output is produced. Every
 * format builtin (sprintf/printf/vprintf/vsprintf/fprintf/vfprintf) calls this
 * up-front, then propagates the returned status verbatim (PH7_EXCEPTION when the
 * throw is caught in place, PH7_ABORT when it goes uncaught).
 * Returns PH7_OK when the format is valid.
 */
PH7_PRIVATE sxi32 PH7_FormatValidate(ph7_context *pCtx,const char *zFormat,int nByte)
{
	int badSpec = 0;
	if( FormatUnknownSpec(zFormat,nByte,&badSpec) ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"Unknown format specifier \"%c\"",badSpec);
	}
	return PH7_OK;
}
/*
 * PHP 8: a printf-family `$format` argument is a `string` parameter — scalars
 * (int/float/bool) and null coerce to a string, but an array/object/resource
 * raises a catchable TypeError. iArg is the 1-based argument position ($format
 * is #1 for sprintf/printf/vprintf/vsprintf, #2 for fprintf/vfprintf). Returns
 * PH7_OK when the value is string-coercible (the caller then uses
 * ph7_value_to_string, which renders scalars/null verbatim).
 */
PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg)
{
	if( ph7_value_is_array(pArg) || ph7_value_is_object(pArg) || ph7_value_is_resource(pArg) ){
		char zBuf[64];
		return PH7_VmThrowException(pCtx,"TypeError",
			"%s(): Argument #%d ($format) must be of type string, %s given",
			ph7_function_name(pCtx),iArg,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));
	}
	return PH7_OK;
}
/*
 * Format a given string.
 * The root program.  All variations call this core.
 * INPUTS:
 *   xConsumer   This is a pointer to a function taking four arguments
 *            1. A pointer to the call context.
 *            2. A pointer to the list of characters to be output
 *               (Note, this list is NOT null terminated.)
 *            3. An integer number of characters to be output.
 *               (Note: This number might be zero.)
 *            4. Upper layer private data.
 *   zIn       This is the format string, as in the usual print.
 *   apArg     This is a pointer to a list of arguments.
 */
PH7_PRIVATE sxi32 PH7_InputFormat(
	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */
	ph7_context *pCtx,  /* call context */
	const char *zIn,    /* Format string */
	int nByte,          /* Format string length */
	int nArg,           /* Total argument of the given arguments */
	ph7_value **apArg,  /* User arguments */
	void *pUserData,    /* Last argument to xConsumer() */
	int vf              /* TRUE if called from vfprintf,vsprintf context */
	)
{
	char spaces[] = "                                                  ";
#define etSPACESIZE ((int)sizeof(spaces)-1)
	const char *zCur,*zEnd = &zIn[nByte];
	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */
	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */
	int flag_alternateform; /* True if "#" flag is present */
	int flag_leftjustify;   /* True if "-" flag is present */
	int flag_blanksign;     /* True if " " flag is present */
	int flag_plussign;      /* True if "+" flag is present */
	int flag_zeropad;       /* True if field width constant starts with zero */
	ph7_value *pArg;         /* Current processed argument */
	ph7_int64 iVal;
	int precision;           /* Precision of the current field */
	/* zExtra (unused) removed to prevent compiler warning. */
	int c,rc,n;
	int length;              /* Length of the field */
	int prefix;
	sxu8 xtype;              /* Conversion paradigm */
	int width;               /* Width of the current field */
	int idx;
	n = (vf == TRUE) ? 0 : 1;
#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )
	/* An unknown conversion specifier is rejected up-front by PH7_FormatValidate()
	 * (called by every format builtin before this routine), so the specifier set
	 * seen here is always valid. */
	/* Start the format process */
	for(;;){
		zCur = zIn;
		while( zIn < zEnd && zIn[0] != '%' ){
			zIn++;
		}
		if( zCur < zIn ){
			/* Consume chunk verbatim */
			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);
			if( rc != SXRET_OK ){
				/* Callback requested an abort (e.g. an allocation failure) */
				break;
			}
		}
		if( zIn >= zEnd ){
			/* No more input to process,break immediately */
			break;
		}
		/* Find out what flags are present */
		flag_leftjustify = flag_plussign = flag_blanksign =
			flag_alternateform = flag_zeropad = 0;
		zIn++; /* Jump the precent sign */
		do{
			c = zIn[0];
			switch( c ){
			case '-':   flag_leftjustify = 1;     c = 0;   break;
			case '+':   flag_plussign = 1;        c = 0;   break;
			case ' ':   flag_blanksign = 1;       c = 0;   break;
			case '0':   flag_zeropad = 1;         c = 0;   break;
			case '\'':
				zIn++;
				if( zIn < zEnd ){
					/* An alternate padding character can be specified by prefixing it with a single quote (') */
					c = zIn[0];
					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){
						spaces[idx] = (char)c;
					}
					c = 0;
				}
				break;
			default:                                       break;
			}
		}while( c==0 && (zIn++ < zEnd) );
		/* Get the field width */
		width = 0;
		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){
			width = width*10 + (zIn[0] - '0');
			zIn++;
		}
		if( zIn < zEnd && zIn[0] == '$' ){
			/* Position specifer */
			if( width > 0 ){
				n = width;
				if( vf && n > 0 ){
					n--;
				}
			}
			zIn++;
			width = 0;
			/* php's grammar is %argnum$<flags><width>: the flags come AFTER the
			 * positional, so re-parse the full flag set here (e.g. "%1$-10s"),
			 * not just zero-padding. */
			do{
				c = zIn[0];
				switch( c ){
				case '-':   flag_leftjustify = 1;     c = 0;   break;
				case '+':   flag_plussign = 1;        c = 0;   break;
				case ' ':   flag_blanksign = 1;       c = 0;   break;
				case '0':   flag_zeropad = 1;         c = 0;   break;
				case '\'':
					zIn++;
					if( zIn < zEnd ){
						c = zIn[0];
						for(idx = 0 ; idx < etSPACESIZE ; ++idx ){
							spaces[idx] = (char)c;
						}
						c = 0;
					}
					break;
				default:                                       break;
				}
			}while( c==0 && (zIn++ < zEnd) );
			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){
				width = width*10 + (zIn[0] - '0');
				zIn++;
			}
		}
		if( width > PH7_FMT_BUFSIZ-10 ){
			width = PH7_FMT_BUFSIZ-10;
		}
		/* Get the precision */
		precision = -1;
		if( zIn < zEnd && zIn[0] == '.' ){
			precision = 0;
			zIn++;
			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){
				precision = precision*10 + (zIn[0] - '0');
				zIn++;
			}
		}
		/* Consume a single 'l' length modifier (a C-ism php accepts and ignores,
		 * e.g. "%ld"); PH7_FormatValidate mirrors this. Exactly one is skipped:
		 * in "%lld" the second 'l' becomes the (unknown) specifier, just like php. */
		if( zIn < zEnd && zIn[0] == 'l' ){
			zIn++;
		}
		if( zIn >= zEnd ){
			/* No more input */
			break;
		}
		/* Fetch the info entry for the field */
		pInfo = 0;
		xtype = PH7_FMT_ERROR;
		c = zIn[0];
		zIn++; /* Jump the format specifer */
		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){
			if( c==aFmt[idx].fmttype ){
				pInfo = &aFmt[idx];
				xtype = pInfo->type;
				break;
			}
		}
		zBuf = zWorker; /* Point to the working buffer */
		length = 0;
		/* zExtra previously assigned here; not used anywhere, removed. */
		 /*
		  ** At this point, variables are initialized as follows:
		  **
		  **   flag_alternateform          TRUE if a '#' is present.
		  **   flag_plussign               TRUE if a '+' is present.
		  **   flag_leftjustify            TRUE if a '-' is present or if the
		  **                               field width was negative.
		  **   flag_zeropad                TRUE if the width began with 0.
		  **                               the conversion character.
		  **   flag_blanksign              TRUE if a ' ' is present.
		  **   width                       The specified field width.  This is
		  **                               always non-negative.  Zero is the default.
		  **   precision                   The specified precision.  The default
		  **                               is -1.
		  */
		switch(xtype){
		case PH7_FMT_PERCENT:
			/* A literal percent character */
			zWorker[0] = '%';
			length = (int)sizeof(char);
			break;
		case PH7_FMT_CHARX:
			/* The argument is treated as an integer, and presented as the character
			 * with that ASCII value
			 */
			pArg = NEXT_ARG;
			if( pArg == 0 ){
				c = 0;
			}else{
				c = ph7_value_to_int(pArg);
			}
			/* NUL byte is an acceptable value */
			zWorker[0] = (char)c;
			length = (int)sizeof(char);
			break;
		case PH7_FMT_STRING:
			/* the argument is treated as and presented as a string */
			pArg = NEXT_ARG;
			if( pArg == 0 ){
				length = 0;
			}else{
				zBuf = (char *)ph7_value_to_string(pArg,&length);
			}
			if( length < 1 ){
				zBuf = " ";
				length = (int)sizeof(char);
			}
			if( precision>=0 && precision<length ){
				length = precision;
			}
			if( flag_zeropad ){
				/* zero-padding works on strings too */
				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){
					spaces[idx] = '0';
				}
			}
			break;
		case PH7_FMT_RADIX:
			pArg = NEXT_ARG;
			if( pArg == 0 ){
				iVal = 0;
			}else{
				iVal = ph7_value_to_int64(pArg);
			}
			/* Limit the precision to prevent overflowing buf[] during conversion */
			if( precision>PH7_FMT_BUFSIZ-40 ){
				precision = PH7_FMT_BUFSIZ-40;
			}
#if 1
        /* For the format %#x, the value zero is printed "0" not "0x0".
        ** I think this is stupid.*/
        if( iVal==0 ) flag_alternateform = 0;
#else
        /* More sensible: turn off the prefix for octal (to prevent "00"),
        ** but leave the prefix for hex.*/
        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;
#endif
        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){
          if( iVal<0 ){
            iVal = -iVal;
			/* Ticket 1433-003 */
			if( iVal < 0 ){
				/* Overflow */
				iVal= 0x7FFFFFFFFFFFFFFF;
			}
            prefix = '-';
          }else if( flag_plussign )  prefix = '+';
          else if( flag_blanksign )  prefix = ' ';
          else                       prefix = 0;
        }else{
			if( iVal<0 ){
				iVal = -iVal;
				/* Ticket 1433-003 */
				if( iVal < 0 ){
					/* Overflow */
					iVal= 0x7FFFFFFFFFFFFFFF;
				}
			}
			prefix = 0;
		}
        if( flag_zeropad && precision<width-(prefix!=0) ){
          precision = width-(prefix!=0);
        }
        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];
        {
          register char *cset;      /* Use registers for speed */
          register int base;
          cset = pInfo->charset;
          base = pInfo->base;
          do{                                           /* Convert to ascii */
            *(--zBuf) = cset[iVal%base];
            iVal = iVal/base;
          }while( iVal>0 );
        }
		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);
        for(idx=precision-length; idx>0; idx--){
          *(--zBuf) = '0';                             /* Zero pad */
        }
        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */
        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */
          char *pre, x;
          pre = pInfo->prefix;
          if( *zBuf!=pre[0] ){
            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;
          }
        }
		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);
		break;
		case PH7_FMT_FLOAT:
		case PH7_FMT_EXP:
		case PH7_FMT_GENERIC:{
#ifndef PH7_OMIT_FLOATING_POINT
		double realvalue;
		char zFmt[8];
		int nOut, nFmt;
		pArg = NEXT_ARG;
		if( pArg == 0 ){
			realvalue = 0;
		}else{
			realvalue = ph7_value_to_double(pArg);
		}
		/* php prints the IEEE specials bare — NaN / INF / -INF with no width
		 * padding, precision, or sign flags (php_sprintf_appenddouble). */
		if( PH7_IS_NAN(realvalue) ){
			zBuf = "NaN";
			length = 3;
			width = 0;
			break;
		}
		if( PH7_IS_INF(realvalue) ){
			if( realvalue < 0.0 ){
				zBuf = "-INF";
				length = 4;
			}else{
				zBuf = "INF";
				length = 3;
			}
			width = 0;
			break;
		}
		if( precision<0 ) precision = 6;         /* Set default precision */
		if( precision > 53 ){
			/* php's FORMAT_CONV_MAX_PRECISION cap, with the same E_NOTICE
			 * (message prefixed with the active function's name, like
			 * php_error_docref). */
			char zMsg[160];
			SyBufferFormat(zMsg,sizeof(zMsg),
				"%z(): Requested precision of %d digits was truncated to PHP maximum of %d digits",
				&pCtx->pFunc->sName,precision,53);
			PH7_VmThrowError(pCtx->pVm,0,E_NOTICE,zMsg);
			precision = 53;
		}
		/* php's %f/%e extract the sign via `num < 0`, so negative zero prints
		 * unsigned there — while %g (php_gcvt on the raw value) keeps "-0". */
		if( xtype!=PH7_FMT_GENERIC && realvalue == 0.0 ){
			realvalue = 0.0;
		}
		/* php's float conversions are correctly rounded (zend_dtoa); use libc
		 * snprintf as the digit engine (the byte-exact-floats rule — the old
		 * hand-rolled vxGetdigit loop stopped at 16 significant digits, so
		 * e.g. %f of 1e308 printed zeros where php prints the exact binary64
		 * expansion), then post-process into php's exact shapes below. */
		nFmt = 0;
		zFmt[nFmt++] = '%';
		if( flag_alternateform ) zFmt[nFmt++] = '#';
		/* php's ' ' flag selects space PADDING (its default), not C's
		 * space-for-positive-sign — so flag_blanksign is NOT forwarded. */
		if( flag_plussign ) zFmt[nFmt++] = '+';
		zFmt[nFmt++] = '.';
		zFmt[nFmt++] = '*';
		zFmt[nFmt++] = (char)(xtype==PH7_FMT_FLOAT ? 'f' :
			(xtype==PH7_FMT_EXP ? ((pInfo->charset[0]=='E') ? 'E' : 'e')
			                    : ((pInfo->charset[0]=='E') ? 'G' : 'g')));
		zFmt[nFmt] = 0;
		nOut = snprintf(zWorker,sizeof(zWorker),zFmt,precision,realvalue);
		if( nOut < 0 || nOut >= (int)sizeof(zWorker) ){
			/* Cannot happen with precision capped at 53 (%f of DBL_MAX is
			 * ~365 bytes); keep the truncated output rather than overrun. */
			nOut = (int)SyStrlen(zWorker);
		}
		nOut = (int)PH7_PhpFloatShape(zWorker,(sxi32)nOut,xtype==PH7_FMT_GENERIC);
		zBuf = zWorker;
		length = nOut;
		/* Let the zero-pad block below insert zeros between the sign (written
		 * by snprintf) and the first digit, as before. */
		prefix = (zWorker[0]=='-' || zWorker[0]=='+' || zWorker[0]==' ') ? zWorker[0] : 0;
        /* Special case:  Add leading zeros if the flag_zeropad flag is
        ** set and we are not left justified */
        if( flag_zeropad && !flag_leftjustify && length < width){
          int i;
          int nPad = width - length;
          for(i=width; i>=nPad; i--){
            zBuf[i] = zBuf[i-nPad];
          }
          i = prefix!=0;
          while( nPad-- ) zBuf[i++] = '0';
          length = width;
        }
#else
         zBuf = " ";
		 length = (int)sizeof(char);
#endif /* PH7_OMIT_FLOATING_POINT */
		 break;
							 }
		default:
			/* Unreachable: PH7_FormatValidate() rejects unknown specifiers with a
			 * catchable ValueError before formatting begins. Kept as a defensive
			 * no-op that emits nothing. */
			length = 0;
			break;
		}
		 /*
		 ** The text of the conversion is pointed to by "zBuf" and is
		 ** "length" characters long.The field width is "width".Do
		 ** the output.
		 */
    if( !flag_leftjustify ){
      register int nspace;
      nspace = width-length;
      if( nspace>0 ){
        while( nspace>=etSPACESIZE ){
			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);
			if( rc != SXRET_OK ){
				return SXERR_ABORT; /* Consumer routine request an operation abort */
			}
			nspace -= etSPACESIZE;
        }
        if( nspace>0 ){
			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);
			if( rc != SXRET_OK ){
				return SXERR_ABORT; /* Consumer routine request an operation abort */
			}
		}
      }
    }
    if( length>0 ){
		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);
		if( rc != SXRET_OK ){
		  return SXERR_ABORT; /* Consumer routine request an operation abort */
		}
    }
    if( flag_leftjustify ){
      register int nspace;
      nspace = width-length;
      if( nspace>0 ){
        while( nspace>=etSPACESIZE ){
			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);
			if( rc != SXRET_OK ){
				return SXERR_ABORT; /* Consumer routine request an operation abort */
			}
			nspace -= etSPACESIZE;
        }
        if( nspace>0 ){
			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);
			if( rc != SXRET_OK ){
				return SXERR_ABORT; /* Consumer routine request an operation abort */
			}
		}
      }
    }
 }/* for(;;) */
	return SXRET_OK;
}
/*
 * Callback [i.e: Formatted input consumer] of the sprintf function.
 */
static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)
{
	/* pUserData points to the caller's allocation-rc slot so an OOM during the
	 * result append is surfaced (the builtin raises a fatal); returning the
	 * non-OK rc also stops the format loop. */
	sxi32 *pRc = (sxi32 *)pUserData;
	*pRc = ph7_result_string(pCtx,zInput,nLen);
	return *pRc;
}
/*
 * string sprintf(string $format[,mixed $args [, mixed $... ]])
 *  Return a formatted string.
 * Parameters
 *  $format
 *    The format string (see block comment above)
 * Return
 *  A string produced according to the formatting string format.
 */
static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zFormat;
	sxi32 rc = SXRET_OK;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */
	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);
	if( rc != PH7_OK ){
		return rc;
	}
	/* Extract the string format (scalars/null coerce). */
	zFormat = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* PHP 8: an unknown format specifier throws a catchable ValueError before any
	 * output; propagate the throw status verbatim. */
	rc = PH7_FormatValidate(pCtx,zFormat,nLen);
	if( rc != PH7_OK ){
		return rc;
	}
	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */
	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);
	if( rc != SXRET_OK ){
		/* The result append ran out of memory: raise a fatal rather than
		 * returning a silently-truncated string. */
		return PH7_ContextMemoryError(pCtx);
	}
	return PH7_OK;
}
/*
 * Callback [i.e: Formatted input consumer] of the printf function.
 */
static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)
{
	ph7_int64 *pCounter = (ph7_int64 *)pUserData;
	/* Call the VM output consumer directly */
	ph7_context_output(pCtx,zInput,nLen);
	/* Increment counter */
	*pCounter += nLen;
	return PH7_OK;
}
/*
 * int64 printf(string $format[,mixed $args[,mixed $... ]])
 *  Output a formatted string.
 * Parameters
 *  $format
 *   See sprintf() for a description of format.
 * Return
 *  The length of the outputted string.
 */
static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_int64 nCounter = 0;
	const char *zFormat;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */
	{
		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);
		if( rcf != PH7_OK ){
			return rcf;
		}
	}
	/* Extract the string format (scalars/null coerce). */
	zFormat = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* PHP 8: an unknown format specifier throws a catchable ValueError before any
	 * output; propagate the throw status verbatim. */
	{
		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);
		if( rcv != PH7_OK ){
			return rcv;
		}
	}
	/* Format the string */
	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);
	/* Return the length of the outputted string */
	ph7_result_int64(pCtx,nCounter);
	return PH7_OK;
}
/*
 * int vprintf(string $format,array $args)
 *  Output a formatted string.
 * Parameters
 *  $format
 *   See sprintf() for a description of format.
 * Return
 *  The length of the outputted string.
 */
static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_int64 nCounter = 0;
	const char *zFormat;
	ph7_hashmap *pMap;
	SySet sArg;
	int nLen,n;
	sxi32 rcFmt;
	if( nArg < 2 ){
		/* Missing arguments,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */
	rcFmt = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);
	if( rcFmt != PH7_OK ){
		return rcFmt;
	}
	if( !ph7_value_is_array(apArg[1]) ){
		/* PHP 8: a non-array $values is a catchable TypeError. */
		char zBuf[64];
		return PH7_VmThrowException(pCtx,"TypeError",
			"vprintf(): Argument #2 ($values) must be of type array, %s given",
			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));
	}
	/* Extract the string format (scalars/null coerce). */
	zFormat = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* PHP 8: an unknown format specifier throws a catchable ValueError before any
	 * output; propagate the throw status verbatim. */
	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);
	if( rcFmt != PH7_OK ){
		return rcFmt;
	}
	/* Point to the hashmap */
	pMap = (ph7_hashmap *)apArg[1]->x.pOther;
	/* Extract arguments from the hashmap */
	n = PH7_HashmapValuesToSet(pMap,&sArg);
	/* Format the string */
	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);
	/* Release the container */
	SySetRelease(&sArg);
	/* Return the length of the outputted string */
	ph7_result_int64(pCtx,nCounter);
	return PH7_OK;
}
/*
 * int vsprintf(string $format,array $args)
 *  Output a formatted string.
 * Parameters
 *  $format
 *   See sprintf() for a description of format.
 * Return
 *  A string produced according to the formatting string format.
 */
static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zFormat;
	ph7_hashmap *pMap;
	SySet sArg;
	sxi32 rc = SXRET_OK;
	sxi32 rcFmt;
	int nLen,n;
	if( nArg < 2 ){
		/* Missing arguments,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */
	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);
	if( rc != PH7_OK ){
		return rc;
	}
	if( !ph7_value_is_array(apArg[1]) ){
		/* PHP 8: a non-array $values is a catchable TypeError. */
		char zBuf[64];
		return PH7_VmThrowException(pCtx,"TypeError",
			"vsprintf(): Argument #2 ($values) must be of type array, %s given",
			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));
	}
	/* Extract the string format (scalars/null coerce). */
	zFormat = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* PHP 8: an unknown format specifier throws a catchable ValueError before any
	 * output; propagate the throw status verbatim. */
	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);
	if( rcFmt != PH7_OK ){
		return rcFmt;
	}
	/* Point to hashmap */
	pMap = (ph7_hashmap *)apArg[1]->x.pOther;
	/* Extract arguments from the hashmap */
	n = PH7_HashmapValuesToSet(pMap,&sArg);
	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */
	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);
	/* Release the container */
	SySetRelease(&sArg);
	if( rc != SXRET_OK ){
		/* The result append ran out of memory: raise a fatal. */
		return PH7_ContextMemoryError(pCtx);
	}
	return PH7_OK;
}
#endif /* PH7_NEED_FMT_AND_INI */
#ifdef PH7_NEED_BUILTIN_REG
/*
 * Symisc eXtension.
 * string size_format(int64 $size)
 *  Return a smart string represenation of the given size [i.e: 64-bit integer]
 *  Example:
 *    echo size_format(1*1024*1024*1024);// 1GB
 *    echo size_format(512*1024*1024); // 512 MB
 *    echo size_format(file_size(/path/to/my/file_8192)); //8KB
 * Parameter
 *  $size
 *    Entity size in bytes.
 * Return
 *   Formatted string representation of the given size.
 */
static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/
	static const char zUnit[] = {"KMGTPEZ"};
	sxi32 nRest,i_32;
	ph7_int64 iSize;
	int c = -1; /* index in zUnit[] */

	if( nArg < 1 ){
		/* Missing argument,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Extract the given size */
	iSize = ph7_value_to_int64(apArg[0]);
	if( iSize < 100 /* Bytes */ ){
		/* Don't bother formatting,return immediately */
		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);
		return PH7_OK;
	}
	for(;;){
		nRest = (sxi32)(iSize & 0x3FF);
		iSize >>= 10;
		c++;
		if( (iSize & (~0 ^ 1023)) == 0 ){
			break;
		}
	}
	nRest /= 100;
	if( nRest > 9 ){
		nRest = 9;
	}
	if( iSize > 999 ){
		c++;
		nRest = 9;
		iSize = 0;
	}
	i_32 = (sxi32)iSize;
	/* Format */
	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);
	return PH7_OK;
}
#if !defined(PH7_DISABLE_HASH_FUNC)
/*
 * string md5(string $str[,bool $raw_output = false])
 *   Calculate the md5 hash of a string.
 * Parameter
 *  $str
 *   Input string
 * $raw_output
 *   If the optional raw_output is set to TRUE, then the md5 digest
 *   is instead returned in raw binary format with a length of 16.
 * Return
 *  MD5 Hash as a 32-character hexadecimal string.
 */
static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	unsigned char zDigest[16];
	int raw_output = FALSE;
	const void *pIn;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Extract the input string (the empty string hashes to a well-defined
	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */
	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);
	if( nArg > 1 && ph7_value_is_bool(apArg[1])){
		raw_output = ph7_value_to_bool(apArg[1]);
	}
	/* Compute the MD5 digest */
	SyMD5Compute(pIn,(sxu32)nLen,zDigest);
	if( raw_output ){
		/* Output raw digest */
		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));
	}else{
		/* Perform a binary to hex conversion */
		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);
	}
	return PH7_OK;
}
/*
 * string sha1(string $str[,bool $raw_output = false])
 *   Calculate the sha1 hash of a string.
 * Parameter
 *  $str
 *   Input string
 * $raw_output
 *   If the optional raw_output is set to TRUE, then the md5 digest
 *   is instead returned in raw binary format with a length of 16.
 * Return
 *  SHA1 Hash as a 40-character hexadecimal string.
 */
static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	unsigned char zDigest[20];
	int raw_output = FALSE;
	const void *pIn;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Extract the input string (the empty string hashes to a well-defined
	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */
	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);
	if( nArg > 1 && ph7_value_is_bool(apArg[1])){
		raw_output = ph7_value_to_bool(apArg[1]);
	}
	/* Compute the SHA1 digest */
	SySha1Compute(pIn,(sxu32)nLen,zDigest);
	if( raw_output ){
		/* Output raw digest */
		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));
	}else{
		/* Perform a binary to hex conversion */
		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);
	}
	return PH7_OK;
}
/*
 * int64 crc32(string $str)
 *   Calculates the crc32 polynomial of a strin.
 * Parameter
 *  $str
 *   Input string
 * Return
 *  CRC32 checksum of the given input (64-bit integer).
 */
static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const void *pIn;
	sxu32 nCRC;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return 0 */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Extract the input string */
	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike
		 * md5()/sha1(), whose empty-string digests are non-zero. */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Calculate the sum */
	nCRC = SyCrc32(pIn,(sxu32)nLen);
	/* Return the CRC32 as 64-bit integer */
	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);
	return PH7_OK;
}
/*
 * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is
 * described by a small record so one dispatch (and one generic HMAC) serves them
 * all. Thin adapters normalize the differing context types and the reversed
 * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.
 */
static void HashMd5Init(HashCtx *c){ MD5Init(&c->md5); }
static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }
static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }
static void HashSha1Init(HashCtx *c){ SHA1Init(&c->sha1); }
static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }
static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }
static void HashSha224Init(HashCtx *c){ SHA224Init(&c->sha256); }
static void HashSha256Init(HashCtx *c){ SHA256Init(&c->sha256); }
static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }
static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }
static void HashSha384Init(HashCtx *c){ SHA384Init(&c->sha512); }
static void HashSha512Init(HashCtx *c){ SHA512Init(&c->sha512); }
static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }
static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }
typedef struct HashAlgo HashAlgo;
struct HashAlgo {
	const char *zName;   /* lowercase canonical name */
	int nDigestLen;      /* output bytes: 16/20/28/32/48/64 */
	int nBlockLen;       /* internal block bytes (for HMAC): 64 or 128 */
	void (*xInit)(HashCtx *);
	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);
	void (*xFinal)(HashCtx *,unsigned char *);
};
static const HashAlgo aHashAlgo[] = {
	{ "md5",    16, 64,  HashMd5Init,    HashMd5Update,    HashMd5Final    },
	{ "sha1",   20, 64,  HashSha1Init,   HashSha1Update,   HashSha1Final   },
	{ "sha224", 28, 64,  HashSha224Init, HashSha256Update, HashSha256Final },
	{ "sha256", 32, 64,  HashSha256Init, HashSha256Update, HashSha256Final },
	{ "sha384", 48, 128, HashSha384Init, HashSha512Update, HashSha512Final },
	{ "sha512", 64, 128, HashSha512Init, HashSha512Update, HashSha512Final },
};
/* Case-insensitive algorithm lookup (PHP accepts 'SHA256' etc.). */
static const HashAlgo * HashFindAlgo(const char *zName,int nLen){
	sxu32 i;
	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){
		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen
			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){
			return &aHashAlgo[i];
		}
	}
	return 0;
}
/*
 * string hash(string $algo,string $data[,bool $binary = false])
 *   Generate a hash value (message digest).
 */
static int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const HashAlgo *pAlgo;
	const char *zAlgo,*zData;
	int nAlgoLen,nDataLen,raw_output = FALSE;
	HashCtx sCtx;
	unsigned char zDigest[64];
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"hash() expects at least 2 arguments, %d given",nArg);
	}
	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);
	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);
	if( pAlgo == 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");
	}
	zData = ph7_value_to_string(apArg[1],&nDataLen);
	if( nArg > 2 ){
		raw_output = ph7_value_to_bool(apArg[2]);
	}
	pAlgo->xInit(&sCtx);
	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);
	pAlgo->xFinal(&sCtx,zDigest);
	if( raw_output ){
		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);
	}else{
		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);
	}
	return PH7_OK;
}
/*
 * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])
 *   Generate a keyed hash value using the HMAC method (RFC 2104).
 */
static int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const HashAlgo *pAlgo;
	const char *zAlgo,*zData,*zKey;
	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;
	HashCtx sCtx;
	unsigned char zKeyBlock[128],zIpad[128],zOpad[128],zInner[64],zDigest[64];
	int i,nBlock,nDigest;
	if( nArg < 3 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"hash_hmac() expects at least 3 arguments, %d given",nArg);
	}
	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);
	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);
	if( pAlgo == 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");
	}
	zData = ph7_value_to_string(apArg[1],&nDataLen);
	zKey = ph7_value_to_string(apArg[2],&nKeyLen);
	if( nArg > 3 ){
		raw_output = ph7_value_to_bool(apArg[3]);
	}
	nBlock = pAlgo->nBlockLen;
	nDigest = pAlgo->nDigestLen;
	/* Reduce the key to a single block: hash it if longer than the block, then
	 * zero-pad (a short or empty key is just zero-padded). */
	SyZero(zKeyBlock,sizeof(zKeyBlock));
	if( nKeyLen > nBlock ){
		pAlgo->xInit(&sCtx);
		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);
		pAlgo->xFinal(&sCtx,zKeyBlock);
	}else if( nKeyLen > 0 ){
		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);
	}
	for( i = 0; i < nBlock; i++ ){
		zIpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);
		zOpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);
	}
	/* inner = H((key ^ ipad) || data) */
	pAlgo->xInit(&sCtx);
	pAlgo->xUpdate(&sCtx,zIpad,(unsigned int)nBlock);
	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);
	pAlgo->xFinal(&sCtx,zInner);
	/* out = H((key ^ opad) || inner) */
	pAlgo->xInit(&sCtx);
	pAlgo->xUpdate(&sCtx,zOpad,(unsigned int)nBlock);
	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)nDigest);
	pAlgo->xFinal(&sCtx,zDigest);
	if( raw_output ){
		ph7_result_string(pCtx,(const char *)zDigest,nDigest);
	}else{
		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);
	}
	return PH7_OK;
}
/*
 * bool hash_equals(string $known_string,string $user_string)
 *   Timing-attack-safe string comparison.
 */
static int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zKnown,*zUser;
	int nKnown,nUser,i;
	volatile unsigned char vDiff = 0;
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"hash_equals() expects exactly 2 arguments, %d given",nArg);
	}
	if( !ph7_value_is_string(apArg[0]) ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",
			ph7_type_name(apArg[0]));
	}
	if( !ph7_value_is_string(apArg[1]) ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",
			ph7_type_name(apArg[1]));
	}
	zKnown = ph7_value_to_string(apArg[0],&nKnown);
	zUser = ph7_value_to_string(apArg[1],&nUser);
	if( nKnown != nUser ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Constant-time: read every byte, never short-circuit. */
	for( i = 0; i < nKnown; i++ ){
		vDiff |= (unsigned char)(zKnown[i] ^ zUser[i]);
	}
	ph7_result_bool(pCtx,vDiff == 0);
	return PH7_OK;
}
/*
 * array hash_algos(void)
 *   Return a list of the registered hashing algorithms.
 */
static int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_value *pArray,*pValue;
	sxu32 i;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	pArray = ph7_context_new_array(pCtx);
	pValue = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pValue == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){
		ph7_value_string(pValue,aHashAlgo[i].zName,-1);
		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);
		ph7_value_reset_string_cursor(pValue);
	}
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
#endif /* PH7_DISABLE_HASH_FUNC */
/*
 * password_* (bcrypt). These live in ext/standard in real PHP — outside the
 * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.
 */
/*
 * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a
 * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).
 */
static int BcryptParseHash(const char *zHash,int nHash,int *piCost)
{
	int iCost;
	if( nHash != 60 || zHash[0] != '$' || zHash[1] != '2' || zHash[3] != '$'
		|| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){
		return FALSE;
	}
	if( zHash[4] < '0' || zHash[4] > '9' || zHash[5] < '0' || zHash[5] > '9' || zHash[6] != '$' ){
		return FALSE;
	}
	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');
	if( iCost < 4 || iCost > 31 ){
		return FALSE;
	}
	if( piCost ){ *piCost = iCost; }
	return TRUE;
}
/*
 * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the
 * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.
 */
static int BcryptIsBcryptAlgo(ph7_value *pAlgo)
{
	if( ph7_value_is_null(pAlgo) ){
		return TRUE;
	}
	if( ph7_value_is_string(pAlgo) ){
		int nAlgo;
		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);
		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );
	}
	return FALSE;
}
/*
 * bool|string password_hash(string $password,string|int|null $algo[,array $options])
 *  Create a bcrypt hash of the password.
 */
static int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPwd;
	int nPwd,iCost = 12;
	unsigned char aSalt[16];
	char zHash[60];
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"password_hash() expects at least 2 arguments, %d given",nArg);
	}
	if( !BcryptIsBcryptAlgo(apArg[1]) ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");
	}
	/* cost from $options['cost'] (default 12). */
	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){
		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);
		if( pCost ){ iCost = ph7_value_to_int(pCost); }
	}
	if( iCost < 4 || iCost > 31 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"Invalid bcrypt cost parameter specified: %d",iCost);
	}
	zPwd = ph7_value_to_string(apArg[0],&nPwd);
	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){
		return PH7_VmThrowException(pCtx,"Exception",
			"password_hash(): unable to gather sufficient entropy for the salt");
	}
	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));
	return PH7_OK;
}
/*
 * bool password_verify(string $password,string $hash)
 *  Verify a password against a bcrypt hash. Never throws on a malformed hash.
 */
static int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zPwd,*zHash;
	int nPwd,nHash,iCost,i;
	unsigned char aSalt[16];
	char zComputed[60];
	volatile unsigned char vDiff = 0;
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"password_verify() expects exactly 2 arguments, %d given",nArg);
	}
	zPwd = ph7_value_to_string(apArg[0],&nPwd);
	zHash = ph7_value_to_string(apArg[1],&nHash);
	if( !BcryptParseHash(zHash,nHash,&iCost) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */
	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps
	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */
	for( i = 29; i < 60; i++ ){
		vDiff |= (unsigned char)(zComputed[i] ^ zHash[i]);
	}
	ph7_result_bool(pCtx,vDiff == 0);
	return PH7_OK;
}
/*
 * array password_get_info(string $hash)
 *  Return ["algo"=>id|null, "algoName"=>name, "options"=>[...]].
 */
static int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zHash = "";
	int nHash,iCost = 0,bBcrypt = 0;
	ph7_value *pArray,*pOptions,*pVal;
	if( nArg > 0 ){
		zHash = ph7_value_to_string(apArg[0],&nHash);
		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);
	}
	pArray = ph7_context_new_array(pCtx);
	pOptions = ph7_context_new_array(pCtx);
	pVal = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pOptions == 0 || pVal == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( bBcrypt ){
		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */
		ph7_array_add_strkey_elem(pArray,"algo",pVal);
		ph7_value_reset_string_cursor(pVal);
		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);
		ph7_array_add_strkey_elem(pArray,"algoName",pVal);
		ph7_value_int(pVal,iCost);
		ph7_array_add_strkey_elem(pOptions,"cost",pVal);
	}else{
		ph7_value_null(pVal);                          /* algo => null */
		ph7_array_add_strkey_elem(pArray,"algo",pVal);
		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);
		ph7_array_add_strkey_elem(pArray,"algoName",pVal);
	}
	ph7_array_add_strkey_elem(pArray,"options",pOptions);
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * bool password_needs_rehash(string $hash,string|int|null $algo[,array $options])
 *  True if the hash was not made with the given algo/options.
 */
static int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zHash;
	int nHash,iCost = 0,iWantCost = 12;
	if( nArg < 2 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);
	}
	zHash = ph7_value_to_string(apArg[0],&nHash);
	if( !BcryptParseHash(zHash,nHash,&iCost) || !BcryptIsBcryptAlgo(apArg[1]) ){
		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */
		ph7_result_bool(pCtx,1);
		return PH7_OK;
	}
	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){
		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);
		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }
	}
	ph7_result_bool(pCtx,iCost != iWantCost);
	return PH7_OK;
}
/*
 * filter_var() — input validation and sanitization (the ext/filter API).
 *
 * Filter and flag identifiers (values match PHP 8.5; the constants themselves
 * are registered in constant.c). The validate filters are hand-rolled rather
 * than delegating to SyStrToInt64/SyStrToReal: the former silently skips leading
 * zeros and cannot signal overflow, and the latter treats ',' as a decimal point
 * unconditionally — neither matches PHP's filter semantics.
 */
#define FV_VALIDATE_INT     257
#define FV_VALIDATE_BOOLEAN 258
#define FV_VALIDATE_FLOAT   259
#define FV_VALIDATE_REGEXP  272
#define FV_VALIDATE_URL     273
#define FV_VALIDATE_EMAIL   274
#define FV_VALIDATE_IP      275
#define FV_VALIDATE_MAC     276
#define FV_VALIDATE_DOMAIN  277
#define FV_SANITIZE_SPECIAL_CHARS      515
#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */
#define FV_SANITIZE_EMAIL   517
#define FV_SANITIZE_URL     518
#define FV_SANITIZE_NUMBER_INT   519
#define FV_SANITIZE_NUMBER_FLOAT 520
#define FV_SANITIZE_FULL_SPECIAL_CHARS 522
#define FV_FLAG_ALLOW_OCTAL  1
#define FV_FLAG_ALLOW_HEX    2
#define FV_FLAG_STRIP_LOW    4
#define FV_FLAG_STRIP_HIGH   8
#define FV_FLAG_ENCODE_LOW   16
#define FV_FLAG_ENCODE_HIGH  32
#define FV_FLAG_ENCODE_AMP   64
#define FV_FLAG_NO_ENCODE_QUOTES 128
#define FV_FLAG_STRIP_BACKTICK   512
#define FV_FLAG_ALLOW_FRACTION   4096
#define FV_FLAG_ALLOW_THOUSAND   8192
#define FV_FLAG_ALLOW_SCIENTIFIC 16384
#define FV_FLAG_IPV4  1048576
#define FV_FLAG_IPV6  2097152
#define FV_NULL_ON_FAILURE 134217728
/* The subset of flags the UNSAFE_RAW/DEFAULT string filter (FvSanitizeString)
 * acts on: when none are set the filter is a verbatim pass-through, so FV_DEFAULT
 * can shortcut. Keep this in sync with FvSanitizeString's flag handling. */
#define FV_FLAG_STRING_MASK (FV_FLAG_STRIP_LOW|FV_FLAG_STRIP_HIGH|FV_FLAG_STRIP_BACKTICK \
                            |FV_FLAG_ENCODE_LOW|FV_FLAG_ENCODE_HIGH|FV_FLAG_ENCODE_AMP)

/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.
 * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */
static void FvTrim(const char **pz,int *pn){
	const char *z = *pz;
	int n = *pn;
	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }
	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }
	*pz = z; *pn = n;
}
/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */
static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){
	int neg = 0, i;
	sxu64 u = 0;
	FvTrim(&z,&n);
	if( n==0 ){ return 0; }
	if( z[0]=='+' || z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }
	if( n==0 ){ return 0; }
	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'||z[1]=='X') ){
		z += 2; n -= 2;
		if( n==0 ){ return 0; }
		for( i=0; i<n; i++ ){
			int h = SyHexToint((unsigned char)z[i]);
			if( h<0 ){ return 0; }
			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }
			u = u*16 + (sxu64)h;
		}
	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){
		for( i=0; i<n; i++ ){
			if( z[i]<'0' || z[i]>'7' ){ return 0; }
			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }
			u = u*8 + (sxu64)(z[i]-'0');
		}
	}else{
		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */
		for( i=0; i<n; i++ ){
			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }
			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }
			u = u*10 + (sxu64)(z[i]-'0');
		}
	}
	if( neg ){
		if( u > 0x8000000000000000ULL ){ return 0; }
		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */
	}else{
		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }
		*pOut = (ph7_int64)u;
	}
	return 1;
}
/* FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure. */
static int FvValidateFloat(const char *z,int n,int flags,double *pOut){
	char zBuf[512];
	int i, m = 0, seenDigit = 0;
	const char *zv; int nv; double d = 0;
	FvTrim(&z,&n);
	/* Bound the input: zBuf[512] holds the thousand-separator-stripped copy, and
	 * the cap also rejects the pathological 500+ digit floats PHP refuses. */
	if( n==0 || n>500 ){ return 0; }
	if( flags & FV_FLAG_ALLOW_THOUSAND ){
		/* Commas are optional, but when present they must group the integer part
		 * into a leading run of 1..3 digits followed by groups of exactly 3
		 * ("1,000" ok, "1,5"/"1234,567" rejected). Strip them into zBuf and reject
		 * a comma anywhere in the fractional/exponent tail. */
		int s = 0, intEnd, segStart, segIdx, hasComma = 0;
		if( s<n && (z[s]=='+'||z[s]=='-') ){ zBuf[m++] = z[s]; s++; }
		intEnd = s;
		while( intEnd<n && z[intEnd]!='.' && z[intEnd]!='e' && z[intEnd]!='E' ){
			if( z[intEnd]==',' ){ hasComma = 1; }
			intEnd++;
		}
		if( hasComma ){
			segStart = s; segIdx = 0;
			for( i=s; i<=intEnd; i++ ){
				if( i==intEnd || z[i]==',' ){
					int segLen = i - segStart, k;
					if( segIdx==0 ){ if( segLen<1 || segLen>3 ){ return 0; } }
					else if( segLen!=3 ){ return 0; }
					for( k=segStart; k<i; k++ ){
						if( !SyisDigit((unsigned char)z[k]) ){ return 0; }
						zBuf[m++] = z[k];
					}
					segStart = i+1; segIdx++;
				}
			}
		}else{
			for( i=s; i<intEnd; i++ ){ zBuf[m++] = z[i]; }
		}
		for( i=intEnd; i<n; i++ ){
			if( z[i]==',' ){ return 0; }
			zBuf[m++] = z[i];
		}
		zv = zBuf; nv = m;
	}else{
		zv = z; nv = n;
	}
	i = 0;
	if( i<nv && (zv[i]=='+'||zv[i]=='-') ){ i++; }
	while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }
	if( i<nv && zv[i]=='.' ){
		i++;
		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }
	}
	if( !seenDigit ){ return 0; }
	if( i<nv && (zv[i]=='e'||zv[i]=='E') ){
		i++;
		if( i<nv && (zv[i]=='+'||zv[i]=='-') ){ i++; }
		if( i>=nv || !SyisDigit((unsigned char)zv[i]) ){ return 0; }
		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; }
	}
	if( i!=nv ){ return 0; } /* trailing junk */
	/* The grammar above guarantees zv[0..nv) is a clean ASCII decimal float (no hex /
	 * inf / nan / trailing junk), so it is safe to hand to libc strtod, which — unlike
	 * SyStrToReal (15 sig-digits + exponent clamped to 308, so it silently saturates
	 * overflowing magnitudes to a finite value) — is overflow/underflow-aware and
	 * correctly rounded. strtod needs a NUL-terminated string: the ALLOW_THOUSAND path
	 * already built the span in zBuf (zv==zBuf); the plain path must copy it there (z is
	 * const + not NUL-terminated). nv <= n <= 500 < sizeof(zBuf) by the cap above.
	 * Matches PHP 8.5 byte-for-byte: reject overflow (-> +/-INF) and total underflow
	 * (-> 0.0), keep subnormals (nonzero, errno==ERANGE) and a genuine "0" (errno==0). */
	if( zv != zBuf ){ SyMemcpy(zv,zBuf,(sxu32)nv); }
	zBuf[nv] = 0;
	errno = 0;
	d = strtod(zBuf,0);
	if( errno == ERANGE && (d == HUGE_VAL || d == -HUGE_VAL || d == 0.0) ){
		return 0;
	}
	*pOut = d;
	return 1;
}
/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),
 * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as
 * false, NOT failures. */
static int FvValidateBool(const char *z,int n,int *pBool){
	FvTrim(&z,&n);
	if( (n==1 && z[0]=='1') || (n==4 && SyStrnicmp(z,"true",4)==0)
	    || (n==2 && SyStrnicmp(z,"on",2)==0) || (n==3 && SyStrnicmp(z,"yes",3)==0) ){
		*pBool = 1; return 1;
	}
	if( n==0 || (n==1 && z[0]=='0') || (n==5 && SyStrnicmp(z,"false",5)==0)
	    || (n==3 && SyStrnicmp(z,"off",3)==0) || (n==2 && SyStrnicmp(z,"no",2)==0) ){
		*pBool = 0; return 1;
	}
	return 0;
}
/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. */
static int FvValidateIp4(const char *z,int n){
	int i = 0, parts = 0;
	while( i<n ){
		int val = 0, digits = 0, start = i;
		while( i<n && SyisDigit((unsigned char)z[i]) ){
			val = val*10 + (z[i]-'0');
			if( val>255 ){ return 0; }
			digits++; i++;
		}
		if( digits==0 || digits>3 ){ return 0; }
		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */
		parts++;
		if( parts>4 ){ return 0; }
		if( i<n ){
			if( z[i]!='.' ){ return 0; }
			i++;
			if( i>=n ){ return 0; } /* trailing dot */
		}
	}
	return parts==4;
}
/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),
 * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1. */
static int FvIp6Hextets(const char *z,int n){
	int i = 0, segStart = 0, groups = 0;
	if( n==0 ){ return 0; }
	while( i<=n ){
		if( i==n || z[i]==':' ){
			int segLen = i - segStart, j, isV4 = 0;
			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */
			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }
			if( isV4 ){
				if( i!=n ){ return -1; } /* IPv4 only as the final token */
				if( !FvValidateIp4(z+segStart,segLen) ){ return -1; }
				groups += 2;
			}else{
				if( segLen>4 ){ return -1; }
				for( j=segStart; j<i; j++ ){ if( SyHexToint((unsigned char)z[j])<0 ){ return -1; } }
				groups++;
			}
			segStart = i+1;
		}
		i++;
	}
	return groups;
}
/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present. */
static int FvValidateIp6(const char *z,int n){
	const char *zDbl = 0;
	int i, ga, gb;
	for( i=0; i+1<n; i++ ){
		if( z[i]==':' && z[i+1]==':' ){
			if( zDbl ){ return 0; } /* a second "::" is invalid */
			zDbl = z+i;
		}
	}
	if( zDbl==0 ){
		return FvIp6Hextets(z,n)==8;
	}else{
		int lenA = (int)(zDbl - z);
		int lenB = n - lenA - 2;
		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA);
		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB);
		if( ga<0 || gb<0 ){ return 0; }
		return (ga+gb)<=7; /* "::" stands for at least one zero group */
	}
}
static int FvValidateIp(const char *z,int n,int flags){
	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);
	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */
	if( v4 && FvValidateIp4(z,n) ){ return 1; }
	if( v6 && FvValidateIp6(z,n) ){ return 1; }
	return 0;
}
/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */
static int FvValidateMac(const char *z,int n){
	char sep;
	int i;
	if( n!=17 ){ return 0; }
	sep = z[2];
	if( sep!=':' && sep!='-' ){ return 0; }
	for( i=0; i<17; i++ ){
		if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }
		else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }
	}
	return 1;
}
/* FILTER_VALIDATE_EMAIL (best-effort: covers the common cases, not quoted local
 * parts or IP-literal domains). */
static int FvValidateEmail(const char *z,int n){
	int at = -1, i, localLen, domLen, labelStart, dotCount = 0;
	const char *zDom;
	if( n==0 || n>320 ){ return 0; }
	for( i=0; i<n; i++ ){
		if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }
	}
	if( at<=0 || at==n-1 ){ return 0; } /* one '@', non-empty local and domain */
	localLen = at;
	zDom = z + at + 1;
	domLen = n - at - 1;
	if( z[0]=='.' || z[at-1]=='.' ){ return 0; }
	for( i=0; i<localLen; i++ ){
		unsigned char c = (unsigned char)z[i];
		if( c<=' ' ){ return 0; }
		if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }
	}
	if( zDom[0]=='.' || zDom[domLen-1]=='.' ){ return 0; }
	labelStart = 0;
	for( i=0; i<=domLen; i++ ){
		if( i==domLen || zDom[i]=='.' ){
			int ll = i - labelStart;
			if( ll==0 ){ return 0; } /* consecutive dots */
			if( zDom[labelStart]=='-' || zDom[i-1]=='-' ){ return 0; }
			if( i<domLen ){ dotCount++; }
			labelStart = i+1;
		}else{
			unsigned char c = (unsigned char)zDom[i];
			if( !((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-') ){ return 0; }
		}
	}
	if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */
	return 1;
}
/* FILTER_VALIDATE_DOMAIN (lenient, matching PHP without FILTER_FLAG_HOSTNAME). */
static int FvValidateDomain(const char *z,int n){
	int i;
	if( n<1 || n>253 || z[0]=='.' ){ return 0; }
	for( i=0; i<n; i++ ){
		unsigned char c = (unsigned char)z[i];
		if( c<=' ' ){ return 0; }
		if( c=='.' && i+1<n && z[i+1]=='.' ){ return 0; }
	}
	return 1;
}
/* FILTER_VALIDATE_URL: require a scheme and a host (PHP's filter is itself
 * parse_url-based, so PH7_VmHttpSplitURI tracks it closely). */
static int FvValidateUrl(const char *z,int n){
	SyhttpUri sUri;
	if( n==0 ){ return 0; }
	SyZero(&sUri,(sxu32)sizeof(sUri));
	if( PH7_VmHttpSplitURI(&sUri,z,(sxu32)n)!=SXRET_OK ){ return 0; }
	return sUri.sScheme.nByte!=0 && sUri.sHost.nByte!=0;
}
/* The Fv sanitizers build their result by appending directly to the call
 * context (ph7_result_string accumulates, like htmlspecialchars), emitting each
 * kept run in one call and seeding "" so an all-stripped input yields "". */
/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */
static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){
	int i, runStart = 0;
	ph7_result_string(pCtx,"",0);
	for( i=0; i<n; i++ ){
		char c = z[i];
		int keep = (c>='0'&&c<='9') || c=='+' || c=='-';
		if( !keep && isFloat ){
			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))
			    || (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))
			    || ((c=='e'||c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));
		}
		if( !keep ){
			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }
			runStart = i+1;
		}
	}
	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }
}
/* Return non-zero when byte c must be stripped under the STRIP_* flags. Shared
 * by the UNSAFE_RAW string filter and SANITIZE_SPECIAL_CHARS. STRIP_LOW drops
 * bytes <32, STRIP_HIGH drops bytes >=127 (incl. DEL), STRIP_BACKTICK drops '`'.
 * Matches php_filter_strip(); verified byte-exact vs php 8.5.7. */
static int FvStripByte(unsigned char c,int flags){
	if( (flags & FV_FLAG_STRIP_LOW)      && c<32 )    { return 1; }
	if( (flags & FV_FLAG_STRIP_HIGH)     && c>=127 )  { return 1; }
	if( (flags & FV_FLAG_STRIP_BACKTICK) && c==0x60 ) { return 1; }
	return 0;
}
/* FILTER_UNSAFE_RAW / FILTER_DEFAULT with flags: no default transform, but the
 * STRIP/ENCODE flags apply. Precedence (per php_filter_unsafe_raw, verified
 * vs php 8.5.7): a byte is first tested for stripping; a surviving byte is then
 * encoded as a decimal numeric entity if ENCODE_LOW (<32) / ENCODE_HIGH (>=127)
 * is set, and '&' becomes "&#38;" under ENCODE_AMP. So STRIP_LOW|ENCODE_LOW
 * strips (nothing left to encode). Bytes are treated individually — ENCODE_HIGH
 * numeric-encodes each byte of a multibyte sequence separately, not the codepoint. */
static void FvSanitizeString(ph7_context *pCtx,const char *z,int n,int flags){
	int i, runStart = 0;
	ph7_result_string(pCtx,"",0);
	for( i=0; i<n; i++ ){
		unsigned char c = (unsigned char)z[i];
		if( FvStripByte(c,flags) ){
			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }
			runStart = i+1;
			continue;
		}
		if( c=='&' && (flags & FV_FLAG_ENCODE_AMP) ){
			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }
			ph7_result_string(pCtx,"&#38;",-1);
			runStart = i+1;
		}else if( (c<32 && (flags & FV_FLAG_ENCODE_LOW))
		       || (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){
			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }
			ph7_result_string_format(pCtx,"&#%d;",(int)c);
			runStart = i+1;
		}
	}
	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }
}
/* FILTER_SANITIZE_SPECIAL_CHARS: encode <>&"' and every control byte <32 as a
 * decimal numeric entity (&#60; &#38; &#34; ...). The STRIP_* flags remove bytes
 * before encoding; ENCODE_HIGH numeric-encodes surviving bytes >=127. Bytes >=128
 * are otherwise passed through verbatim (this filter is NOT UTF-8-aware — only the
 * FULL variant is). Byte-exact vs php 8.5.7. */
static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int flags){
	int i, runStart = 0;
	const char *zEnt;
	ph7_result_string(pCtx,"",0);
	for( i=0; i<n; i++ ){
		unsigned char c = (unsigned char)z[i];
		if( FvStripByte(c,flags) ){
			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }
			runStart = i+1;
			continue;
		}
		switch( c ){
		case '<':  zEnt = "&#60;"; break;
		case '>':  zEnt = "&#62;"; break;
		case '&':  zEnt = "&#38;"; break;
		case '"':  zEnt = "&#34;"; break;
		case '\'': zEnt = "&#39;"; break;
		default:
			/* Control bytes <32 are always numeric-encoded; bytes >=127 only when
			 * ENCODE_HIGH is set. Everything else stays in the current run. */
			if( c<32 || (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){
				if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }
				ph7_result_string_format(pCtx,"&#%d;",(int)c);
				runStart = i+1;
			}
			continue; /* keep in the current run */
		}
		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }
		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */
		runStart = i+1;
	}
	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }
}
/* HTML 4.01 named-entity table (codepoint -> "&name;") used by the UTF-8-aware
 * FULL_SPECIAL_CHARS filter, sorted ascending by codepoint for binary search.
 * Generated from php 8.5.7 (the exact set php_escape_html_entities emits for the
 * default document type); the five inline specials <>&"' are handled separately,
 * so every entry here is a codepoint >=0xA0. 248 rows. */
static const struct { sxu32 cp; const char *zEnt; } aHtml401Ent[] = {
	{0x00A0,"&nbsp;"},{0x00A1,"&iexcl;"},{0x00A2,"&cent;"},{0x00A3,"&pound;"},
	{0x00A4,"&curren;"},{0x00A5,"&yen;"},{0x00A6,"&brvbar;"},{0x00A7,"&sect;"},
	{0x00A8,"&uml;"},{0x00A9,"&copy;"},{0x00AA,"&ordf;"},{0x00AB,"&laquo;"},
	{0x00AC,"&not;"},{0x00AD,"&shy;"},{0x00AE,"&reg;"},{0x00AF,"&macr;"},
	{0x00B0,"&deg;"},{0x00B1,"&plusmn;"},{0x00B2,"&sup2;"},{0x00B3,"&sup3;"},
	{0x00B4,"&acute;"},{0x00B5,"&micro;"},{0x00B6,"&para;"},{0x00B7,"&middot;"},
	{0x00B8,"&cedil;"},{0x00B9,"&sup1;"},{0x00BA,"&ordm;"},{0x00BB,"&raquo;"},
	{0x00BC,"&frac14;"},{0x00BD,"&frac12;"},{0x00BE,"&frac34;"},{0x00BF,"&iquest;"},
	{0x00C0,"&Agrave;"},{0x00C1,"&Aacute;"},{0x00C2,"&Acirc;"},{0x00C3,"&Atilde;"},
	{0x00C4,"&Auml;"},{0x00C5,"&Aring;"},{0x00C6,"&AElig;"},{0x00C7,"&Ccedil;"},
	{0x00C8,"&Egrave;"},{0x00C9,"&Eacute;"},{0x00CA,"&Ecirc;"},{0x00CB,"&Euml;"},
	{0x00CC,"&Igrave;"},{0x00CD,"&Iacute;"},{0x00CE,"&Icirc;"},{0x00CF,"&Iuml;"},
	{0x00D0,"&ETH;"},{0x00D1,"&Ntilde;"},{0x00D2,"&Ograve;"},{0x00D3,"&Oacute;"},
	{0x00D4,"&Ocirc;"},{0x00D5,"&Otilde;"},{0x00D6,"&Ouml;"},{0x00D7,"&times;"},
	{0x00D8,"&Oslash;"},{0x00D9,"&Ugrave;"},{0x00DA,"&Uacute;"},{0x00DB,"&Ucirc;"},
	{0x00DC,"&Uuml;"},{0x00DD,"&Yacute;"},{0x00DE,"&THORN;"},{0x00DF,"&szlig;"},
	{0x00E0,"&agrave;"},{0x00E1,"&aacute;"},{0x00E2,"&acirc;"},{0x00E3,"&atilde;"},
	{0x00E4,"&auml;"},{0x00E5,"&aring;"},{0x00E6,"&aelig;"},{0x00E7,"&ccedil;"},
	{0x00E8,"&egrave;"},{0x00E9,"&eacute;"},{0x00EA,"&ecirc;"},{0x00EB,"&euml;"},
	{0x00EC,"&igrave;"},{0x00ED,"&iacute;"},{0x00EE,"&icirc;"},{0x00EF,"&iuml;"},
	{0x00F0,"&eth;"},{0x00F1,"&ntilde;"},{0x00F2,"&ograve;"},{0x00F3,"&oacute;"},
	{0x00F4,"&ocirc;"},{0x00F5,"&otilde;"},{0x00F6,"&ouml;"},{0x00F7,"&divide;"},
	{0x00F8,"&oslash;"},{0x00F9,"&ugrave;"},{0x00FA,"&uacute;"},{0x00FB,"&ucirc;"},
	{0x00FC,"&uuml;"},{0x00FD,"&yacute;"},{0x00FE,"&thorn;"},{0x00FF,"&yuml;"},
	{0x0152,"&OElig;"},{0x0153,"&oelig;"},{0x0160,"&Scaron;"},{0x0161,"&scaron;"},
	{0x0178,"&Yuml;"},{0x0192,"&fnof;"},{0x02C6,"&circ;"},{0x02DC,"&tilde;"},
	{0x0391,"&Alpha;"},{0x0392,"&Beta;"},{0x0393,"&Gamma;"},{0x0394,"&Delta;"},
	{0x0395,"&Epsilon;"},{0x0396,"&Zeta;"},{0x0397,"&Eta;"},{0x0398,"&Theta;"},
	{0x0399,"&Iota;"},{0x039A,"&Kappa;"},{0x039B,"&Lambda;"},{0x039C,"&Mu;"},
	{0x039D,"&Nu;"},{0x039E,"&Xi;"},{0x039F,"&Omicron;"},{0x03A0,"&Pi;"},
	{0x03A1,"&Rho;"},{0x03A3,"&Sigma;"},{0x03A4,"&Tau;"},{0x03A5,"&Upsilon;"},
	{0x03A6,"&Phi;"},{0x03A7,"&Chi;"},{0x03A8,"&Psi;"},{0x03A9,"&Omega;"},
	{0x03B1,"&alpha;"},{0x03B2,"&beta;"},{0x03B3,"&gamma;"},{0x03B4,"&delta;"},
	{0x03B5,"&epsilon;"},{0x03B6,"&zeta;"},{0x03B7,"&eta;"},{0x03B8,"&theta;"},
	{0x03B9,"&iota;"},{0x03BA,"&kappa;"},{0x03BB,"&lambda;"},{0x03BC,"&mu;"},
	{0x03BD,"&nu;"},{0x03BE,"&xi;"},{0x03BF,"&omicron;"},{0x03C0,"&pi;"},
	{0x03C1,"&rho;"},{0x03C2,"&sigmaf;"},{0x03C3,"&sigma;"},{0x03C4,"&tau;"},
	{0x03C5,"&upsilon;"},{0x03C6,"&phi;"},{0x03C7,"&chi;"},{0x03C8,"&psi;"},
	{0x03C9,"&omega;"},{0x03D1,"&thetasym;"},{0x03D2,"&upsih;"},{0x03D6,"&piv;"},
	{0x2002,"&ensp;"},{0x2003,"&emsp;"},{0x2009,"&thinsp;"},{0x200C,"&zwnj;"},
	{0x200D,"&zwj;"},{0x200E,"&lrm;"},{0x200F,"&rlm;"},{0x2013,"&ndash;"},
	{0x2014,"&mdash;"},{0x2018,"&lsquo;"},{0x2019,"&rsquo;"},{0x201A,"&sbquo;"},
	{0x201C,"&ldquo;"},{0x201D,"&rdquo;"},{0x201E,"&bdquo;"},{0x2020,"&dagger;"},
	{0x2021,"&Dagger;"},{0x2022,"&bull;"},{0x2026,"&hellip;"},{0x2030,"&permil;"},
	{0x2032,"&prime;"},{0x2033,"&Prime;"},{0x2039,"&lsaquo;"},{0x203A,"&rsaquo;"},
	{0x203E,"&oline;"},{0x2044,"&frasl;"},{0x20AC,"&euro;"},{0x2111,"&image;"},
	{0x2118,"&weierp;"},{0x211C,"&real;"},{0x2122,"&trade;"},{0x2135,"&alefsym;"},
	{0x2190,"&larr;"},{0x2191,"&uarr;"},{0x2192,"&rarr;"},{0x2193,"&darr;"},
	{0x2194,"&harr;"},{0x21B5,"&crarr;"},{0x21D0,"&lArr;"},{0x21D1,"&uArr;"},
	{0x21D2,"&rArr;"},{0x21D3,"&dArr;"},{0x21D4,"&hArr;"},{0x2200,"&forall;"},
	{0x2202,"&part;"},{0x2203,"&exist;"},{0x2205,"&empty;"},{0x2207,"&nabla;"},
	{0x2208,"&isin;"},{0x2209,"&notin;"},{0x220B,"&ni;"},{0x220F,"&prod;"},
	{0x2211,"&sum;"},{0x2212,"&minus;"},{0x2217,"&lowast;"},{0x221A,"&radic;"},
	{0x221D,"&prop;"},{0x221E,"&infin;"},{0x2220,"&ang;"},{0x2227,"&and;"},
	{0x2228,"&or;"},{0x2229,"&cap;"},{0x222A,"&cup;"},{0x222B,"&int;"},
	{0x2234,"&there4;"},{0x223C,"&sim;"},{0x2245,"&cong;"},{0x2248,"&asymp;"},
	{0x2260,"&ne;"},{0x2261,"&equiv;"},{0x2264,"&le;"},{0x2265,"&ge;"},
	{0x2282,"&sub;"},{0x2283,"&sup;"},{0x2284,"&nsub;"},{0x2286,"&sube;"},
	{0x2287,"&supe;"},{0x2295,"&oplus;"},{0x2297,"&otimes;"},{0x22A5,"&perp;"},
	{0x22C5,"&sdot;"},{0x2308,"&lceil;"},{0x2309,"&rceil;"},{0x230A,"&lfloor;"},
	{0x230B,"&rfloor;"},{0x2329,"&lang;"},{0x232A,"&rang;"},{0x25CA,"&loz;"},
	{0x2660,"&spades;"},{0x2663,"&clubs;"},{0x2665,"&hearts;"},{0x2666,"&diams;"}
};
/* Binary-search aHtml401Ent[] for cp; return its "&name;" entity or 0. */
static const char *FvHtml401Lookup(sxu32 cp){
	int lo = 0, hi = (int)SX_ARRAYSIZE(aHtml401Ent) - 1;
	while( lo <= hi ){
		int mid = (lo + hi) / 2;
		sxu32 c = aHtml401Ent[mid].cp;
		if( c == cp ){ return aHtml401Ent[mid].zEnt; }
		if( c < cp ){ lo = mid + 1; } else { hi = mid - 1; }
	}
	return 0;
}
/* Decode one strict-UTF-8 sequence at p (< zEnd). On success returns its byte
 * length (1..4) and sets *pCp to the codepoint; on any malformed, overlong,
 * surrogate, truncated or out-of-range (>U+10FFFF) sequence returns 0. Matches
 * PHP's UTF-8 validation used by FULL_SPECIAL_CHARS (verified vs php 8.5.7). */
static int FvUtf8Next(const unsigned char *p,const unsigned char *zEnd,sxu32 *pCp){
	unsigned char c = p[0];
	if( c < 0x80 ){ *pCp = c; return 1; }
	if( c < 0xC2 ){ return 0; }              /* 0x80-0xBF stray cont / 0xC0-0xC1 overlong */
	if( c < 0xE0 ){                          /* 2-byte: U+0080..U+07FF */
		if( zEnd-p < 2 || (p[1]&0xC0)!=0x80 ){ return 0; }
		*pCp = ((sxu32)(c&0x1F)<<6) | (p[1]&0x3F);
		return 2;
	}
	if( c < 0xF0 ){                          /* 3-byte: U+0800..U+FFFF minus surrogates */
		sxu32 cp;
		if( zEnd-p < 3 || (p[1]&0xC0)!=0x80 || (p[2]&0xC0)!=0x80 ){ return 0; }
		cp = ((sxu32)(c&0x0F)<<12) | ((sxu32)(p[1]&0x3F)<<6) | (p[2]&0x3F);
		if( cp < 0x800 || (cp>=0xD800 && cp<=0xDFFF) ){ return 0; }
		*pCp = cp;
		return 3;
	}
	if( c < 0xF5 ){                          /* 4-byte: U+10000..U+10FFFF */
		sxu32 cp;
		if( zEnd-p < 4 || (p[1]&0xC0)!=0x80 || (p[2]&0xC0)!=0x80 || (p[3]&0xC0)!=0x80 ){ return 0; }
		cp = ((sxu32)(c&0x07)<<18) | ((sxu32)(p[1]&0x3F)<<12) | ((sxu32)(p[2]&0x3F)<<6) | (p[3]&0x3F);
		if( cp < 0x10000 || cp > 0x10FFFF ){ return 0; }
		*pCp = cp;
		return 4;
	}
	return 0;                                /* 0xF5-0xFF */
}
/* FILTER_SANITIZE_FULL_SPECIAL_CHARS: htmlentities-style, UTF-8-aware. Encodes
 * <>&"' as named entities ("'" -> &#039;; quotes suppressed under NO_ENCODE_QUOTES),
 * and every valid UTF-8 codepoint with an HTML 4.01 named entity as that entity;
 * valid codepoints without a named entity (and low control bytes) pass through
 * verbatim. If the input contains ANY invalid UTF-8 the whole result is "".
 * The STRIP/ENCODE flags do NOT apply to this filter (only NO_ENCODE_QUOTES).
 * php's filter does NOT re-encode valid pre-existing entities ("&amp;" stays,
 * "&bogus;" becomes "&amp;bogus;"), i.e. double_encode=false semantics —
 * exactly htmlentities(ENT_QUOTES|ENT_HTML401, double_encode: false), so this
 * delegates to the shared encoder. Byte-exact vs php 8.5.7. */
static void FvSanitizeFull(ph7_context *pCtx,const char *z,int n,int flags){
	int iEntFlags = (flags & FV_FLAG_NO_ENCODE_QUOTES) ? 0 : PH7_ENT_QUOTES;
	HtmlEscape(pCtx,z,n,iEntFlags,1/*bAll*/,0/*bDoubleEncode*/);
}
/* ---------------------------------------------------------------------------
 * UTF-8-aware HTML entity core (htmlspecialchars/htmlentities family).
 * Prototyped next to the five builtins earlier in this file; lives here so it
 * can share aHtml401Ent[]/FvHtml401Lookup()/FvUtf8Next() with the filter_var
 * FULL_SPECIAL_CHARS filter above. Byte-exact vs php 8.5.7 (oracle-swept).
 * ------------------------------------------------------------------------ */
/* Encode cp as UTF-8 into zBuf (>= 4 bytes); return the byte length 1..4.
 * Thin wrapper over the engine-wide SX_WRITE_UTF8 (sxmacros.h). */
static int HtmlCpUtf8(sxu32 cp,char *zBuf){
	sxu8 *z = (sxu8 *)zBuf;
	SX_WRITE_UTF8(z,cp);
	return (int)(z - (sxu8 *)zBuf);
}
/* Doctype-allowed codepoint test (php's unicode_cp_is_allowed) — gates what a
 * numeric reference may DECODE to. Oracle-pinned per doctype: HTML401
 * disallows C0 (except TAB/LF/CR) and DEL..U+009F; XML1 and XHTML share the
 * XML rules — DEL..U+009F allowed, U+FFFE/U+FFFF excluded; HTML5 swaps CR
 * for FF (0x0C) and excludes the noncharacters (U+FDD0..U+FDEF and every
 * U+xFFFE/U+xFFFF). Surrogates are disallowed everywhere. */
static int HtmlCpAllowed(sxu32 cp,int iFlags){
	int iDoc = iFlags & PH7_ENT_DOC_MASK;
	if( cp==0x09 || cp==0x0A ){ return 1; }
	if( cp==0x0D ){ return iDoc != PH7_ENT_DOC_HTML5; }
	if( cp==0x0C ){ return iDoc == PH7_ENT_DOC_HTML5; }
	if( cp < 0x20 || cp > 0x10FFFF ){ return 0; }
	if( cp>=0xD800 && cp<=0xDFFF ){ return 0; }
	if( cp>=0x7F && cp<=0x9F ){ return iDoc == PH7_ENT_DOC_XML1 || iDoc == PH7_ENT_DOC_XHTML; }
	if( iDoc == PH7_ENT_DOC_XML1 || iDoc == PH7_ENT_DOC_XHTML ){
		return cp!=0xFFFE && cp!=0xFFFF;
	}
	if( iDoc == PH7_ENT_DOC_HTML5 ){
		if( cp>=0xFDD0 && cp<=0xFDEF ){ return 0; }
		if( (cp & 0xFFFF) >= 0xFFFE ){ return 0; }
	}
	return 1;
}
/* The ENT_DISALLOWED gate for RAW characters on the ENCODE side. Same as the
 * decode gate except CR under HTML5: php's encode-side unicode_cp_is_allowed
 * keeps a literal "\r" verbatim under ENT_HTML5|ENT_DISALLOWED while the
 * decode side leaves "&#13;" un-decoded (oracle-pinned at flags 176). */
static int HtmlCpAllowedEncode(sxu32 cp,int iFlags){
	if( cp==0x0D && (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 ){ return 1; }
	return HtmlCpAllowed(cp,iFlags);
}
/* Numeric-reference validity for the double_encode=false "is this already a
 * valid entity" test — a MUCH looser predicate than the decode gate above:
 * any codepoint <= U+10FFFF is valid (controls and surrogates included, every
 * doctype). ENT_DISALLOWED re-tightens non-HTML401 doctypes to the decode
 * gate, except that HTML5 exempts surrogates. All oracle-pinned: &#0; and
 * &#xD800; stay verbatim at flags 11 and 139; flags -1 (HTML5+DISALLOWED)
 * re-encodes &#0; and &#x10FFFF; but still keeps &#xD800;; flags 144
 * (XML1+DISALLOWED) re-encodes &#xD800;. */
static int HtmlNumericAllowed(sxu32 cp,int iFlags){
	if( cp > 0x10FFFF ){ return 0; }
	if( (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML401 ){ return 1; /* never tightened */ }
	if( (iFlags & PH7_ENT_DISALLOWED)
	 && !((iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 && cp>=0xD800 && cp<=0xDFFF)
	 && !HtmlCpAllowed(cp,iFlags) ){ return 0; }
	return 1;
}
/* How many bytes the malformed UTF-8 sequence at p consumes — php's
 * get_next_char failure step (one U+FFFD substitution / one ENT_IGNORE drop
 * per MAXIMAL invalid subpart, not per byte): a prefix-valid sequence eats
 * its continuation bytes ("\xE0\x80\xAF" is ONE unit) while a byte that could
 * start a new sequence is left for the next round. */
static int HtmlUtf8Trail(unsigned char c){ return c>=0x80 && c<=0xBF; }
static int HtmlUtf8Lead(unsigned char c){ return c<0x80 || (c>=0xC2 && c<=0xF4); }
static int HtmlUtf8FailAdvance(const unsigned char *p,const unsigned char *zEnd){
	unsigned char c = p[0];
	int nAvail = (int)(zEnd - p);
	if( c < 0xC2 || c > 0xF4 ){ return 1; } /* stray trail / C0-C1 / F5-FF */
	if( c < 0xE0 ){
		if( nAvail < 2 ){ return 1; }
		return HtmlUtf8Lead(p[1]) ? 1 : 2;
	}
	if( c < 0xF0 ){
		if( nAvail >= 3 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) ){
			return 3; /* complete but overlong/surrogate */
		}
		if( nAvail < 2 || HtmlUtf8Lead(p[1]) ){ return 1; }
		if( nAvail < 3 || HtmlUtf8Lead(p[2]) ){ return 2; }
		return 3;
	}
	if( nAvail >= 4 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) && HtmlUtf8Trail(p[3]) ){
		return 4; /* complete but overlong / > U+10FFFF */
	}
	if( nAvail < 2 || HtmlUtf8Lead(p[1]) ){ return 1; }
	if( nAvail < 3 || HtmlUtf8Lead(p[2]) ){ return 2; }
	if( nAvail < 4 || HtmlUtf8Lead(p[3]) ){ return 3; }
	return 4;
}
/* The basic special entities, shared by named matching, the hsc_decode
 * numeric whitelist and the translation-table builder so the sets can never
 * drift apart. (&apos; is not an HTML 4.01 entity — doctype-gated below.) */
static const struct { const char *zEnt; int n; sxu32 cp; } aHtmlSpecEnt[] = {
	{"&amp;",5,38},{"&lt;",4,60},{"&gt;",4,62},{"&quot;",6,34},{"&apos;",6,39}
};
/* Does this doctype consult the named-entity table (aHtml401Ent)? XML 1.0 has
 * no named entities beyond the specials; XHTML/HTML5 are approximated by the
 * HTML 4.01 table (documented divergence). */
static int HtmlDocHasNamedTable(int iDoc){
	return iDoc != PH7_ENT_DOC_XML1;
}
/* The single-quote entity per doctype. Oracle-pinned asymmetry: for every
 * non-HTML401 doctype htmlspecialchars emits &apos; while htmlentities
 * (bEntities) keeps &#039; under XHTML too. The translation table mirrors
 * whichever function the requested table belongs to. */
static const char *HtmlAposEntity(int iDoc,int bEntities){
	if( iDoc == PH7_ENT_DOC_HTML401 || (bEntities && iDoc == PH7_ENT_DOC_XHTML) ){
		return "&#039;";
	}
	return "&apos;";
}
/* Try to parse one HTML entity at z (z[0]=='&', z < zEnd). bFull selects the
 * html_entity_decode set (doctype named table + any allowed numeric ref) vs
 * the htmlspecialchars_decode set (the basic specials + quote numerics only).
 * Named matching is case-SENSITIVE and the ';' is required (both PHP-exact);
 * numeric refs accept dec/hex (x or X) with any number of leading zeros but
 * reject out-of-range, surrogate and doctype-disallowed codepoints (the
 * caller then leaves the source verbatim). Quote-flag gating is NOT applied
 * here — the same routine doubles as the "is this a valid entity" test for
 * double_encode=false, which ignores the quote bits (oracle-pinned).
 * bEncodeCheck selects the looser HtmlNumericAllowed predicate used by that
 * double_encode test; decode callers pass 0 for the HtmlCpAllowed gate.
 * On success sets *pCp / *pnConsumed and returns 1. */
static int HtmlParseEntity(const unsigned char *z,const unsigned char *zEnd,
                           int iFlags,int bFull,int bEncodeCheck,sxu32 *pCp,int *pnConsumed){
	int nAvail = (int)(zEnd - z);
	int iDoc = iFlags & PH7_ENT_DOC_MASK;
	sxu32 n;
	if( nAvail < 4 ){ return 0; } /* shortest entities: &lt; &#9; */
	if( z[1] == '#' ){
		/* Numeric reference */
		sxu32 cp = 0;
		int i = 2, bHex = 0, nDig = 0;
		if( z[i]=='x' || z[i]=='X' ){ bHex = 1; i++; }
		for( ; i < nAvail && z[i] != ';' ; i++ ){
			int v;
			unsigned char c = z[i];
			if( c>='0' && c<='9' ){ v = c - '0'; }
			else if( bHex && c>='a' && c<='f' ){ v = c - 'a' + 10; }
			else if( bHex && c>='A' && c<='F' ){ v = c - 'A' + 10; }
			else { return 0; }
			/* Stop accumulating once out of range (keeps validating the shape;
			 * max intermediate is 0x10FFFF*16+15, no sxu32 overflow). */
			if( cp <= 0x10FFFF ){ cp = cp * (bHex ? 16 : 10) + (sxu32)v; }
			nDig++;
		}
		if( nDig == 0 || i >= nAvail ){ return 0; } /* no digits / no ';' */
		if( bEncodeCheck ? !HtmlNumericAllowed(cp,iFlags) : !HtmlCpAllowed(cp,iFlags) ){ return 0; }
		if( !bFull ){
			/* hsc_decode: numeric refs to the five specials only. */
			for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) && aHtmlSpecEnt[n].cp != cp ; n++ ){}
			if( n >= SX_ARRAYSIZE(aHtmlSpecEnt) ){ return 0; }
		}
		*pCp = cp;
		*pnConsumed = i + 1;
		return 1;
	}
	/* Named reference — every entity name starts with a letter, so anything
	 * else can bail out before touching the tables. */
	if( !((z[1]>='a' && z[1]<='z') || (z[1]>='A' && z[1]<='Z')) ){ return 0; }
	for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) ; n++ ){
		if( aHtmlSpecEnt[n].cp == 39 && iDoc == PH7_ENT_DOC_HTML401 ){ continue; }
		if( nAvail >= aHtmlSpecEnt[n].n && SyMemcmp(z,aHtmlSpecEnt[n].zEnt,(sxu32)aHtmlSpecEnt[n].n) == 0 ){
			*pCp = aHtmlSpecEnt[n].cp;
			*pnConsumed = aHtmlSpecEnt[n].n;
			return 1;
		}
	}
	if( bFull && HtmlDocHasNamedTable(iDoc) ){
		/* Linear scan of the 248-row table: runs only at '&'-then-letter
		 * positions and guarantees the decode set can never drift from the
		 * encode table. The first-letter guard skips the SyStrlen/SyMemcmp
		 * for ~96% of rows. */
		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){
			sxu32 nEnt;
			if( z[1] != (unsigned char)aHtml401Ent[n].zEnt[1] ){ continue; }
			nEnt = SyStrlen(aHtml401Ent[n].zEnt);
			if( (sxu32)nAvail >= nEnt && SyMemcmp(z,aHtml401Ent[n].zEnt,nEnt) == 0 ){
				*pCp = aHtml401Ent[n].cp;
				*pnConsumed = (int)nEnt;
				return 1;
			}
		}
	}
	return 0;
}
/* Shared encoder for htmlspecialchars (bAll=0) and htmlentities (bAll=1).
 * Invalid UTF-8 policy: ENT_IGNORE drops the byte (and wins over SUBSTITUTE),
 * ENT_SUBSTITUTE emits one U+FFFD per invalid byte, neither -> the whole
 * result is "" (pre-validated in a first pass: the accumulating result API
 * cannot roll back — same reason FvSanitizeFull is two-pass). */
static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,
                       int iFlags,int bAll,int bDoubleEncode){
	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);
	const unsigned char *p = (const unsigned char *)zIn;
	const unsigned char *runStart;
	int iDoc = iFlags & PH7_ENT_DOC_MASK;
	sxu32 cp;
	if( (iFlags & (PH7_ENT_IGNORE|PH7_ENT_SUBSTITUTE)) == 0 ){
		/* Pass 1: any malformed sequence rejects the entire input. ASCII
		 * bytes cannot be malformed, so skip them without the decoder. */
		while( p < zEnd ){
			int len;
			if( *p < 0x80 ){ p++; continue; }
			len = FvUtf8Next(p,zEnd,&cp);
			if( len == 0 ){ ph7_result_string(pCtx,"",0); return; }
			p += len;
		}
		p = (const unsigned char *)zIn;
	}
	runStart = p;
	ph7_result_string(pCtx,"",0);
	while( p < zEnd ){
		const char *zEnt = 0;
		int len;
		if( *p < 0x80 ){
			len = 1;
			switch( *p ){
			case '<': zEnt = "&lt;"; break;
			case '>': zEnt = "&gt;"; break;
			case '&':
				zEnt = "&amp;";
				if( !bDoubleEncode ){
					sxu32 eCp; int nEat;
					if( HtmlParseEntity(p,zEnd,iFlags,1,1,&eCp,&nEat) ){
						/* A valid existing entity: keep it verbatim. */
						zEnt = 0;
						len = nEat;
					}
				}
				break;
			case '"':
				if( iFlags & PH7_ENT_QUOTE_DOUBLE ){ zEnt = "&quot;"; }
				break;
			case '\'':
				if( iFlags & PH7_ENT_QUOTE_SINGLE ){
					zEnt = HtmlAposEntity(iDoc,bAll);
				}
				break;
			default:
				if( (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode((sxu32)*p,iFlags) ){
					zEnt = "\xEF\xBF\xBD";
				}
				break;
			}
		}else{
			len = FvUtf8Next(p,zEnd,&cp);
			if( len == 0 ){
				/* Malformed subpart (IGNORE or SUBSTITUTE is set, else pass 1
				 * would have rejected): drop it or emit ONE U+FFFD for the
				 * whole unit (php substitutes per maximal invalid subpart). */
				if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }
				if( (iFlags & PH7_ENT_IGNORE) == 0 ){ ph7_result_string(pCtx,"\xEF\xBF\xBD",3); }
				p += HtmlUtf8FailAdvance(p,zEnd);
				runStart = p;
				continue;
			}
			if( bAll && HtmlDocHasNamedTable(iDoc) ){
				zEnt = FvHtml401Lookup(cp);
			}
			if( zEnt == 0 && (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode(cp,iFlags) ){
				zEnt = "\xEF\xBF\xBD";
			}
		}
		if( zEnt ){
			if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }
			ph7_result_string(pCtx,zEnt,-1);
			runStart = p + len;
		}
		p += len;
	}
	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }
}
/* Shared decoder for html_entity_decode (bFull=1) and htmlspecialchars_decode
 * (bFull=0). Quote refs (cp 34/39, named or numeric) are gated by the quote
 * bits and left verbatim when suppressed; an invalid entity leaves its '&'
 * verbatim and rescans right after it, which also yields PHP's no-double-
 * decode behavior ("&amp;lt;" -> "&lt;"). */
static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,
                         int iFlags,int bFull){
	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);
	const unsigned char *p = (const unsigned char *)zIn;
	const unsigned char *runStart = p;
	ph7_result_string(pCtx,"",0);
	while( p < zEnd ){
		sxu32 cp;
		int nEat;
		if( *p != '&' ){ p++; continue; }
		if( !HtmlParseEntity(p,zEnd,iFlags,bFull,0,&cp,&nEat) ){ p++; continue; }
		if( (cp == 34 && (iFlags & PH7_ENT_QUOTE_DOUBLE) == 0)
		 || (cp == 39 && (iFlags & PH7_ENT_QUOTE_SINGLE) == 0) ){
			/* Suppressed quote: leave the entity source verbatim. */
			p += nEat;
			continue;
		}
		if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }
		{
			char zBuf[4];
			int n = HtmlCpUtf8(cp,zBuf);
			ph7_result_string(pCtx,zBuf,n);
		}
		p += nEat;
		runStart = p;
	}
	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }
}
/* Validate the optional charset argument at apArg[idx]: UTF-8 aliases (and
 * ""/NULL meaning the default) are accepted; anything else — including
 * php-supported single-byte charsets like ISO-8859-1, PHL is UTF-8-only by
 * policy — raises PHP's unsupported-charset warning and is treated as
 * UTF-8 (ph7_context_throw_error_format prepends the function name). */
static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx){
	const char *zCs;
	int nCs;
	if( nArg <= idx || ph7_value_is_null(apArg[idx]) ){ return; }
	zCs = ph7_value_to_string(apArg[idx],&nCs);
	if( nCs == 0 ){ return; } /* "" selects the default charset (UTF-8) */
	if( nCs == 5 && SyStrnicmp(zCs,"UTF-8",5) == 0 ){
		return; /* php accepts only "UTF-8" (any case) silently — "UTF8" warns */
	}
	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
		"Charset \"%.*s\" is not supported, assuming UTF-8",nCs,zCs);
}
/* get_html_translation_table() worker: character (UTF-8 bytes) => entity.
 * The five specials come first in byte order, then — for HTML_ENTITIES with a
 * named-table doctype — the 248 aHtml401Ent rows ascending (oracle-pinned
 * ordering; 253 entries under the defaults). */
static void HtmlTableAdd(ph7_value *pArray,ph7_value *pValue,const char *zKey,const char *zEnt){
	ph7_value_string(pValue,zEnt,-1);
	ph7_array_add_strkey_elem(pArray,zKey,pValue);
	ph7_value_reset_string_cursor(pValue);
}
static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags){
	ph7_value *pArray,*pValue;
	int iDoc = iFlags & PH7_ENT_DOC_MASK;
	sxu32 n;
	pValue = ph7_context_new_scalar(pCtx);
	pArray = ph7_context_new_array(pCtx);
	if( pValue == 0 || pArray == 0 ){
		ph7_result_null(pCtx);
		return;
	}
	if( iFlags & PH7_ENT_QUOTE_DOUBLE ){
		HtmlTableAdd(pArray,pValue,"\"","&quot;");
	}
	HtmlTableAdd(pArray,pValue,"&","&amp;");
	if( iFlags & PH7_ENT_QUOTE_SINGLE ){
		/* The apostrophe row mirrors the function each table belongs to:
		 * SPECIALCHARS follows htmlspecialchars, ENTITIES follows
		 * htmlentities (oracle-pinned at flags 35). */
		HtmlTableAdd(pArray,pValue,"'",HtmlAposEntity(iDoc,iTable != 0));
	}
	HtmlTableAdd(pArray,pValue,"<","&lt;");
	HtmlTableAdd(pArray,pValue,">","&gt;");
	if( iTable != 0 /*php: any non-HTML_SPECIALCHARS table => entities*/ && HtmlDocHasNamedTable(iDoc) ){
		char zKey[8];
		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){
			int nK = HtmlCpUtf8(aHtml401Ent[n].cp,zKey);
			zKey[nK] = 0;
			HtmlTableAdd(pArray,pValue,zKey,aHtml401Ent[n].zEnt);
		}
	}
	ph7_result_value(pCtx,pArray);
}
static int FvEmailAllowed(unsigned char c){
	if( (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9') ){ return 1; }
	return c=='!'||c=='#'||c=='$'||c=='%'||c=='&'||c=='\''||c=='*'||c=='+'
	    || c=='-'||c=='='||c=='?'||c=='^'||c=='_'||c=='`'||c=='{'||c=='|'
	    || c=='}'||c=='~'||c=='@'||c=='.'||c=='['||c==']';
}
static int FvUrlAllowed(unsigned char c){
	return c>=33 && c<=126; /* PHP keeps every printable ASCII byte except space */
}
/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */
static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){
	int i, runStart = 0;
	ph7_result_string(pCtx,"",0);
	for( i=0; i<n; i++ ){
		unsigned char c = (unsigned char)z[i];
		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){
			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }
			runStart = i+1;
		}
	}
	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }
}
/*
 * Apply the selected filter to one already-resolved input value and write the
 * result into pCtx. Shared by filter_var() and filter_input(): the caller has
 * already parsed $filter/$flags/$options. On validation failure the 'default'
 * option (if any) is returned, else null when FILTER_NULL_ON_FAILURE is set,
 * else false. A validating filter that passes returns the (string) input
 * unchanged; a sanitizer writes its transformed output directly.
 */
static int FvApplyFilter(ph7_context *pCtx,ph7_value *pInput,
                         int iFilter,int iFlags,ph7_value *pOpts,
                         ph7_value *pDefault)
{
	int bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;
	const char *zVal; int nVal;
	/* An array/object input fails every scalar filter. */
	if( ph7_value_is_array(pInput) ){ goto fail; }
	zVal = ph7_value_to_string(pInput,&nVal);
	switch( iFilter ){
	case FV_VALIDATE_INT: {
		ph7_int64 v;
		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }
		if( pOpts ){
			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);
			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);
			if( pMin && v<ph7_value_to_int64(pMin) ){ goto fail; }
			if( pMax && v>ph7_value_to_int64(pMax) ){ goto fail; }
		}
		ph7_result_int64(pCtx,v);
		return PH7_OK;
	}
	case FV_VALIDATE_FLOAT: {
		double d;
		if( !FvValidateFloat(zVal,nVal,iFlags,&d) ){ goto fail; }
		ph7_result_double(pCtx,d);
		return PH7_OK;
	}
	case FV_VALIDATE_BOOLEAN: {
		int b;
		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }
		ph7_result_bool(pCtx,b);
		return PH7_OK;
	}
	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;
	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;
	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(zVal,nVal) ){ goto fail; }     goto pass;
	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal) ){ goto fail; }    goto pass;
	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal) ){ goto fail; }       goto pass;
	case FV_VALIDATE_REGEXP: {
#ifdef PH7_ENABLE_PCRE
		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;
		const char *zRe; int nRe, matched = 0;
		if( pRe==0 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"filter_var(): \"regexp\" option is missing");
		}
		zRe = ph7_value_to_string(pRe,&nRe);
		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK || !matched ){ goto fail; }
		goto pass;
#else
		goto fail;
#endif
	}
	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;
	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;
	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,iFlags); return PH7_OK;
	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeFull(pCtx,zVal,nVal,iFlags);    return PH7_OK;
	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;
	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;
	case FV_DEFAULT:
		/* FILTER_UNSAFE_RAW / FILTER_DEFAULT: pass through unchanged unless a
		 * STRIP/ENCODE flag is set, in which case apply the string filter. */
		if( iFlags & FV_FLAG_STRING_MASK ){
			FvSanitizeString(pCtx,zVal,nVal,iFlags);
			return PH7_OK;
		}
		goto pass;
	default:
		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,
			"Unknown filter with ID %d",iFilter);
		break; /* unknown filter id -> fail */
	}
fail:
	if( pDefault ){ ph7_result_value(pCtx,pDefault); }
	else if( bNull ){ ph7_result_null(pCtx); }
	else { ph7_result_bool(pCtx,0); }
	return PH7_OK;
pass: /* validation passed: return the (string) input unchanged */
	ph7_result_string(pCtx,zVal,nVal);
	return PH7_OK;
}
/*
 * Parse the ($filter, $options) pair shared by filter_var()/filter_input() out
 * of apArg[iBase] ($filter) and apArg[iBase+1] ($options): $options is either a
 * plain flags int, or an array with 'flags' and an 'options' sub-array (whose
 * 'default' entry is the fallback value). Fills the four output pointers;
 * unset outputs keep the caller-provided defaults.
 */
static void FvParseFilterArgs(int nArg,ph7_value **apArg,int iBase,
                              int *piFilter,int *piFlags,
                              ph7_value **ppOpts,ph7_value **ppDefault)
{
	if( nArg>iBase ){ *piFilter = ph7_value_to_int(apArg[iBase]); }
	if( nArg>iBase+1 ){
		if( ph7_value_is_array(apArg[iBase+1]) ){
			ph7_value *pF = ph7_array_fetch(apArg[iBase+1],"flags",(int)sizeof("flags")-1);
			if( pF ){ *piFlags = ph7_value_to_int(pF); }
			*ppOpts = ph7_array_fetch(apArg[iBase+1],"options",(int)sizeof("options")-1);
			if( *ppOpts && !ph7_value_is_array(*ppOpts) ){ *ppOpts = 0; }
			if( *ppOpts ){ *ppDefault = ph7_array_fetch(*ppOpts,"default",(int)sizeof("default")-1); }
		}else{
			*piFlags = ph7_value_to_int(apArg[iBase+1]);
		}
	}
}
/*
 * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)
 *  Validate or sanitize a value; see FvApplyFilter for the failure semantics.
 */
static int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int iFilter = FV_DEFAULT, iFlags = 0;
	ph7_value *pOpts = 0, *pDefault = 0;
	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }
	FvParseFilterArgs(nArg,apArg,1,&iFilter,&iFlags,&pOpts,&pDefault);
	return FvApplyFilter(pCtx,apArg[0],iFilter,iFlags,pOpts,pDefault);
}
/*
 * filter_input($type, $var_name, $filter = FILTER_DEFAULT, $options = 0)
 *  Look up $var_name in the requested INPUT_* superglobal, then apply the
 *  filter. Semantics verified byte-for-byte against php 8.5:
 *   - variable NOT set: 'default' option wins, else false when
 *     FILTER_NULL_ON_FAILURE is set, else null. (Note the null/false roles are
 *     INVERTED relative to a present value that fails validation, which yields
 *     default > null-if-NULL_ON_FAILURE > false via FvApplyFilter.)
 *   - variable present: delegate to FvApplyFilter.
 *  Divergence: php reads a SAPI snapshot of the original request variables
 *  captured at startup; PHL reads the live superglobal. In CLI they match for
 *  the SAPI-registered keys (SCRIPT_NAME/PHP_SELF/DOCUMENT_ROOT); keys added
 *  only to the live $_SERVER (REQUEST_TIME/PWD/…) are visible here but not in
 *  php's snapshot.
 */
static int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int iType, iFilter = FV_DEFAULT, iFlags = 0;
	ph7_value *pOpts = 0, *pDefault = 0, *pSuper, *pElem;
	const char *zVar, *zSuper; int nVar; sxu32 nSuper;
	if( nArg<2 ){
		return PH7_VmThrowException(pCtx,"ArgumentCountError",
			"filter_input() expects at least 2 arguments, %d given",nArg);
	}
	iType = ph7_value_to_int(apArg[0]);
	switch( iType ){
	case 0: zSuper = "_POST";   nSuper = (sxu32)sizeof("_POST")-1;   break; /* INPUT_POST */
	case 1: zSuper = "_GET";    nSuper = (sxu32)sizeof("_GET")-1;    break; /* INPUT_GET */
	case 2: zSuper = "_COOKIE"; nSuper = (sxu32)sizeof("_COOKIE")-1; break; /* INPUT_COOKIE */
	case 4: zSuper = "_ENV";    nSuper = (sxu32)sizeof("_ENV")-1;    break; /* INPUT_ENV */
	case 5: zSuper = "_SERVER"; nSuper = (sxu32)sizeof("_SERVER")-1; break; /* INPUT_SERVER */
	default:
		return PH7_VmThrowException(pCtx,"ValueError",
			"filter_input(): Argument #1 ($type) must be an INPUT_* constant");
	}
	zVar = ph7_value_to_string(apArg[1],&nVar);
	FvParseFilterArgs(nArg,apArg,2,&iFilter,&iFlags,&pOpts,&pDefault);
	/* Resolve the variable from the superglobal (missing/non-array -> not set). */
	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);
	pElem = (pSuper && ph7_value_is_array(pSuper))
		? ph7_array_fetch(pSuper,zVar,nVar) : 0;
	if( pElem==0 ){
		/* Variable not set: default > false(if NULL_ON_FAILURE) > null. Note the
		 * false/null roles are inverted vs FvApplyFilter's present-but-fails path. */
		if( pDefault ){ ph7_result_value(pCtx,pDefault); }
		else if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_bool(pCtx,0); }
		else { ph7_result_null(pCtx); }
		return PH7_OK;
	}
	return FvApplyFilter(pCtx,pElem,iFilter,iFlags,pOpts,pDefault);
}
#endif /* PH7_NEED_BUILTIN_REG */
#ifdef PH7_NEED_FMT_AND_INI
/*
 * Parse a CSV string and invoke the supplied callback for each processed xhunk.

 */
PH7_PRIVATE sxi32 PH7_ProcessCsv(
	const char *zInput, /* Raw input */
	int nByte,  /* Input length */
	int delim,  /* Delimiter */
	int encl,   /* Enclosure */
	int escape,  /* Escape character */
	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */
	void *pUserData /* Last argument to xConsumer() */
	)
{
	const char *zEnd = &zInput[nByte];
	const char *zIn = zInput;
	const char *zPtr;
	int isEnc;
	/* Start processing */
	for(;;){
		if( zIn >= zEnd ){
			/* No more input to process */
			break;
		}
		isEnc = 0;
		zPtr = zIn;
		/* Find the first delimiter */
		while( zIn < zEnd ){
			if( zIn[0] == delim && !isEnc){
				/* Delimiter found,break imediately */
				break;
			}else if( zIn[0] == encl ){
				/* Inside enclosure? */
				isEnc = !isEnc;
			}else if( zIn[0] == escape ){
				/* Escape sequence */
				zIn++;
			}
			/* Advance the cursor */
			zIn++;
		}
		if( zIn > zPtr ){
			int nByteChunk = (int)(zIn-zPtr);
			sxi32 rc;
			/* Invoke the supllied callback */
			if( zPtr[0] == encl ){
				zPtr++;
				nByteChunk-=2;
			}
			if( nByteChunk > 0 ){
				rc = xConsumer(zPtr,nByteChunk,pUserData);
				if( rc == SXERR_ABORT ){
					/* User callback request an operation abort */
					break;
				}
			}
		}
		/* Ignore trailing delimiter */
		while( zIn < zEnd && zIn[0] == delim ){
			zIn++;
		}
	}
	return SXRET_OK;
}
/*
 * Default consumer callback for the CSV parsing routine defined above.
 * All the processed input is insereted into an array passed as the last
 * argument to this callback.
 */
PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)
{
	ph7_value *pArray = (ph7_value *)pUserData;
	ph7_value sEntry;
	SyString sToken;
	/* Insert the token in the given array */
	SyStringInitFromBuf(&sToken,zToken,nTokenLen);
	/* Remove trailing and leading white spcaces and null bytes */
	SyStringFullTrimSafe(&sToken);
	if( sToken.nByte < 1){
		return SXRET_OK;
	}
	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);
	ph7_array_add_elem(pArray,0,&sEntry);
	PH7_MemObjRelease(&sEntry);
	return SXRET_OK;
}
/*
 * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])
 *  Parse a CSV string into an array.
 * Parameters
 *  $input
 *   The string to parse.
 *  $delimiter
 *   Set the field delimiter (one character only).
 *  $enclosure
 *   Set the field enclosure character (one character only).
 *  $escape
 *   Set the escape character (one character only). Defaults as a backslash (\)
 * Return
 *  An indexed array containing the CSV fields or NULL on failure.
 */
static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zInput,*zPtr;
	ph7_value *pArray;
	int delim  = ',';   /* Delimiter */
	int encl   = '"' ;  /* Enclosure */
	int escape = '\\';  /* Escape character */
	int nLen;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return NULL */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Extract the raw input */
	zInput = ph7_value_to_string(apArg[0],&nLen);
	if( nArg > 1 ){
		int i;
		if( ph7_value_is_string(apArg[1]) ){
			/* Extract the delimiter */
			zPtr = ph7_value_to_string(apArg[1],&i);
			if( i > 0 ){
				delim = zPtr[0];
			}
		}
		if( nArg > 2 ){
			if( ph7_value_is_string(apArg[2]) ){
				/* Extract the enclosure */
				zPtr = ph7_value_to_string(apArg[2],&i);
				if( i > 0 ){
					encl = zPtr[0];
				}
			}
			if( nArg > 3 ){
				if( ph7_value_is_string(apArg[3]) ){
					/* Extract the escape character */
					zPtr = ph7_value_to_string(apArg[3],&i);
					if( i > 0 ){
						escape = zPtr[0];
					}
				}
			}
		}
	}
	/* Create our array */
	pArray = ph7_context_new_array(pCtx);
	if( pArray == 0 ){
		/* Surface a fatal instead of silently returning null on OOM */
		return PH7_ContextMemoryError(pCtx);
	}
	/* Parse the raw input */
	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);
	/* Return the freshly created array */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * Extract a tag name from a raw HTML input and insert it in the given
 * container.
 * Refer to [strip_tags()].
 */
static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)
{
	const char *zEnd = &zTag[nByte];
	const char *zPtr;
	SyString sEntry;
	/* Strip tags */
	for(;;){
		while( zTag < zEnd && (zTag[0] == '<' || zTag[0] == '/' || zTag[0] == '?'
			|| zTag[0] == '!' || zTag[0] == '-' || ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){
				zTag++;
		}
		if( zTag >= zEnd ){
			break;
		}
		zPtr = zTag;
		/* Delimit the tag */
		while(zTag < zEnd ){
			if( (unsigned char)zTag[0] >= 0xc0 ){
				/* UTF-8 stream */
				zTag++;
				SX_JMP_UTF8(zTag,zEnd);
			}else if( !SyisAlphaNum(zTag[0]) ){
				break;
			}else{
				zTag++;
			}
		}
		if( zTag > zPtr ){
			/* Perform the insertion */
			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));
			SyStringFullTrim(&sEntry);
			SySetPut(pSet,(const void *)&sEntry);
		}
		/* Jump the trailing '>' */
		zTag++;
	}
	return SXRET_OK;
}
/*
 * Check if the given HTML tag name is present in the given container.
 * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.
 * Refer to [strip_tags()].
 */
static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)
{
	if( SySetUsed(pSet) > 0 ){
		const char *zCur,*zEnd = &zTag[nByte];
		SyString sTag;
		while( zTag < zEnd &&  (zTag[0] == '<' || zTag[0] == '/' || zTag[0] == '?' ||
			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){
			zTag++;
		}
		/* Delimit the tag */
		zCur = zTag;
		while(zTag < zEnd ){
			if( (unsigned char)zTag[0] >= 0xc0 ){
				/* UTF-8 stream */
				zTag++;
				SX_JMP_UTF8(zTag,zEnd);
			}else if( !SyisAlphaNum(zTag[0]) ){
				break;
			}else{
				zTag++;
			}
		}
		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);
		/* Trim leading white spaces and null bytes */
		SyStringLeftTrimSafe(&sTag);
		if( sTag.nByte > 0 ){
			SyString *aEntry,*pEntry;
			sxi32 rc;
			sxu32 n;
			/* Perform the lookup */
			aEntry = (SyString *)SySetBasePtr(pSet);
			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){
				pEntry = &aEntry[n];
				/* Do the comparison */
				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);
				if( !rc ){
					return SXRET_OK;
				}
			}
		}
	}
	/* No such tag */
	return SXERR_NOTFOUND;
}
/*
 * This function tries to return a string [i.e: in the call context result buffer]
 * with all NUL bytes,HTML and PHP tags stripped from a given string.
 * Refer to [strip_tags()].
 */
PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)
{
	const char *zEnd = &zIn[nByte];
	const char *zPtr,*zTag;
	SySet sSet;
	/* initialize the set of allowed tags */
	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));
	if( nTaglen > 0 ){
		/* Set of allowed tags */
		AddTag(&sSet,zTaglist,nTaglen);
	}
	/* Set the empty string */
	ph7_result_string(pCtx,"",0);
	/* Start processing */
	for(;;){
		if(zIn >= zEnd){
			/* No more input to process */
			break;
		}
		zPtr = zIn;
		/* Find a tag */
		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){
			zIn++;
		}
		if( zIn > zPtr ){
			/* Consume raw input */
			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));
		}
		/* Ignore trailing null bytes */
		while( zIn < zEnd && zIn[0] == 0 ){
			zIn++;
		}
		if(zIn >= zEnd){
			/* No more input to process */
			break;
		}
		if( zIn[0] == '<' ){
			sxi32 rc;
			zTag = zIn++;
			/* Delimit the tag */
			while( zIn < zEnd && zIn[0] != '>' ){
				zIn++;
			}
			if( zIn < zEnd ){
				zIn++; /* Ignore the trailing closing tag */
			}
			/* Query the set */
			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));
			if( rc == SXRET_OK ){
				/* Keep the tag */
				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));
			}
		}
	}
	/* Cleanup */
	SySetRelease(&sSet);
	return SXRET_OK;
}
/*
 * string strip_tags(string $str[,string $allowable_tags])
 *   Strip HTML and PHP tags from a string.
 * Parameters
 *  $str
 *  The input string.
 * $allowable_tags
 *  You can use the optional second parameter to specify tags which should not be stripped.
 * Return
 *  Returns the stripped string.
 */
static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zTaglist = 0;
	const char *zString;
	int nTaglen = 0;
	int nLen;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Point to the raw string */
	zString = ph7_value_to_string(apArg[0],&nLen);
	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){
		/* Allowed tag */
		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);
	}
	/* Process input */
	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);
	return PH7_OK;
}
#endif /* PH7_NEED_FMT_AND_INI */
#ifdef PH7_NEED_BUILTIN_REG
/*
 * string str_shuffle(string $str)

 *  Randomly shuffles a string.
 * Parameters
 *  $str
 *   The input string.
 * Return
 *  Returns the shuffled string.
 */
static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString;
	int nLen,i,c;
	sxu32 iR;
	if( nArg < 1 ){
		/* Missing arguments,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Extract the target string */
	zString = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Nothing to shuffle */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Shuffle the string */
	for( i = 0 ; i < nLen ; ++i ){
		/* Generate a random number first */
		iR = ph7_context_random_num(pCtx);
		/* Extract a random offset */
		c = zString[iR % nLen];
		/* Append it */
		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));
	}
	return PH7_OK;
}
/*
 * array str_split(string $string[,int $split_length = 1 ])
 *  Convert a string to an array.
 * Parameters
 * $string
 *  The input string.
 * $split_length
 *  Maximum length of the chunk.
 * Return
 *  Returns an array of chunks. Each chunk is split_length characters long,
 *  except possibly the last one which may be shorter.
 *  If split_length exceeds the string length, the entire string is returned
 *  as the first (and only) array element.
 *  An empty string returns an empty array.
 * Errors
 *  ArgumentCountError if no arguments are given.
 *  TypeError if $string is an array, object or resource.
 *  ValueError if $split_length is less than 1.
 */
static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString,*zEnd;
	ph7_value *pArray,*pValue;
	int split_len;
	int nLen;
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx,
			"ArgumentCountError",
			"str_split() expects at least 1 argument, %d given",
			nArg
			);
	}
	/* Arrays, objects and resources should raise a TypeError like PHP */
	if( ph7_value_is_array(apArg[0]) ||
	    ph7_value_is_object(apArg[0]) ||
	    ph7_value_is_resource(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"str_split(): Argument #1 ($string) must be of type string, %s given",
			ph7_type_name(apArg[0])
			);
	}
	/* Point to the target string */
	zString = ph7_value_to_string(apArg[0],&nLen);
	split_len = (int)sizeof(char);
	if( nArg > 1 ){
		/* Split length */
		split_len = ph7_value_to_int(apArg[1]);
		if( split_len < 1 ){
			return PH7_VmThrowException(pCtx,
				"ValueError",
				"str_split(): Argument #2 ($length) must be greater than 0"
				);
		}
		if( split_len > nLen && nLen > 0 ){
			split_len = nLen;
		}
	}
	/* Create the array and the scalar value */
	pArray = ph7_context_new_array(pCtx);
	/*Chunk value */
	pValue = ph7_context_new_scalar(pCtx);
	if( pValue == 0 || pArray == 0 ){
		/* Return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the end of the string */
	zEnd = &zString[nLen];
	/* Perform the requested operation */
	for(;;){
		int nMax;
		if( zString >= zEnd ){
			/* No more input to process */
			break;
		}
		nMax = (int)(zEnd-zString);
		if( nMax < split_len ){
			split_len = nMax;
		}
		/* Copy the current chunk */
		ph7_value_string(pValue,zString,split_len);
		/* Insert it */
		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */
			return PH7_ContextMemoryError(pCtx);
		}
		/* reset the string cursor */
		ph7_value_reset_string_cursor(pValue);
		/* Update position */
		zString += split_len;
	}
	/*
	 * Return the array.
	 * Don't worry about freeing memory, everything will be automatically released
	 * upon we return from this function.
	 */
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/*
 * Tokenize a raw string and extract the first non-space token.
 * Refer to [strspn()].
 */
static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)
{
	const char *zIn = *pzIn;
	const char *zPtr;
	/* Ignore leading white spaces */
	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){
		zIn++;
	}
	if( zIn >= zEnd ){
		/* End of input */
		return SXERR_EOF;
	}
	zPtr = zIn;
	/* Extract the token */
	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){
		zIn++;
	}
	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);
	/* Synchronize pointers */
	*pzIn = zIn;
	/* Return to the caller */
	return SXRET_OK;
}
/*
 * Check if the given string contains only characters from the given mask.
 * return the longest match.
 * Refer to [strspn()].
 */
static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)
{
	const char *zEnd = &zString[nLen];
	const char *zIn = zString;
	int i,c;
	for(;;){
		if( zString >= zEnd ){
			break;
		}
		/* Extract current character */
		c = zString[0];
		/* Perform the lookup */
		for( i = 0 ; i < nMaskLen ; i++ ){
			if( c == zMask[i] ){
				/* Character found */
				break;
			}
		}
		if( i >= nMaskLen ){
			/* Character not in the current mask,break immediately */
			break;
		}
		/* Advance cursor */
		zString++;
	}
	/* Longest match */
	return (int)(zString-zIn);
}
/*
 * Do the reverse operation of the previous function [i.e: LongestStringMask()].
 * Refer to [strcspn()].
 */
static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)
{
	const char *zEnd = &zString[nLen];
	const char *zIn = zString;
	int i,c;
	for(;;){
		if( zString >= zEnd ){
			break;
		}
		/* Extract current character */
		c = zString[0];
		/* Perform the lookup */
		for( i = 0 ; i < nMaskLen ; i++ ){
			if( c == zMask[i] ){
				break;
			}
		}
		if( i < nMaskLen ){
			/* Character in the current mask,break immediately */
			break;
		}
		/* Advance cursor */
		zString++;
	}
	/* Longest match */
	return (int)(zString-zIn);
}
/*
 * int strspn(string $str,string $mask[,int $start[,int $length]])
 *  Finds the length of the initial segment of a string consisting entirely
 *  of characters contained within a given mask.
 * Parameters
 * $str
 *  The input string.
 * $mask
 *  The list of allowable characters.
 * $start
 *  The position in subject to start searching.
 *  If start is given and is non-negative, then strspn() will begin examining
 *  subject at the start'th position. For instance, in the string 'abcdef', the character
 *  at position 0 is 'a', the character at position 2 is 'c', and so forth.
 *  If start is given and is negative, then strspn() will begin examining subject at the
 *  start'th position from the end of subject.
 * $length
 *  The length of the segment from subject to examine.
 *  If length is given and is non-negative, then subject will be examined for length
 *  characters after the starting position.
 *  If lengthis given and is negative, then subject will be examined from the starting
 *  position up to length characters from the end of subject.
 * Return
 * Returns the length of the initial segment of subject which consists entirely of characters
 * in mask.
 */
static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString,*zMask,*zEnd;
	int iMasklen,iLen;
	SyString sToken;
	int iCount = 0;
	int rc;
	if( nArg < 2 ){
		/* Missing agruments,return zero */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zString = ph7_value_to_string(apArg[0],&iLen);
	/* Extract the mask */
	zMask = ph7_value_to_string(apArg[1],&iMasklen);
	if( iLen < 1 || iMasklen < 1 ){
		/* Nothing to process,return zero */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	if( nArg > 2 ){
		int nOfft;
		/* Extract the offset */
		nOfft = ph7_value_to_int(apArg[2]);
		if( nOfft < 0 ){
			const char *zBase = &zString[iLen + nOfft];
			if( zBase > zString ){
				iLen = (int)(&zString[iLen]-zBase);
				zString = zBase;
			}else{
				/* Invalid offset */
				ph7_result_int(pCtx,0);
				return PH7_OK;
			}
		}else{
			if( nOfft >= iLen ){
				/* Invalid offset */
				ph7_result_int(pCtx,0);
				return PH7_OK;
			}else{
				/* Update offset */
				zString += nOfft;
				iLen -= nOfft;
			}
		}
		if( nArg > 3 ){
			int iUserlen;
			/* Extract the desired length */
			iUserlen = ph7_value_to_int(apArg[3]);
			if( iUserlen > 0 && iUserlen < iLen ){
				iLen = iUserlen;
			}
		}
	}
	/* Point to the end of the string */
	zEnd = &zString[iLen];
	/* Extract the first non-space token */
	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);
	if( rc == SXRET_OK && sToken.nByte > 0 ){
		/* Compare against the current mask */
		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);
	}
	/* Longest match */
	ph7_result_int(pCtx,iCount);
	return PH7_OK;
}
/*
 * int strcspn(string $str,string $mask[,int $start[,int $length]])
 *  Find length of initial segment not matching mask.
 * Parameters
 * $str
 *  The input string.
 * $mask
 *  The list of not allowed characters.
 * $start
 *  The position in subject to start searching.
 *  If start is given and is non-negative, then strspn() will begin examining
 *  subject at the start'th position. For instance, in the string 'abcdef', the character
 *  at position 0 is 'a', the character at position 2 is 'c', and so forth.
 *  If start is given and is negative, then strspn() will begin examining subject at the
 *  start'th position from the end of subject.
 * $length
 *  The length of the segment from subject to examine.
 *  If length is given and is non-negative, then subject will be examined for length
 *  characters after the starting position.
 *  If lengthis given and is negative, then subject will be examined from the starting
 *  position up to length characters from the end of subject.
 * Return
 *  Returns the length of the segment as an integer.
 */
static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString,*zMask,*zEnd;
	int iMasklen,iLen;
	SyString sToken;
	int iCount = 0;
	int rc;
	if( nArg < 2 ){
		/* Missing agruments,return zero */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zString = ph7_value_to_string(apArg[0],&iLen);
	/* Extract the mask */
	zMask = ph7_value_to_string(apArg[1],&iMasklen);
	if( iLen < 1 ){
		/* Nothing to process,return zero */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	if( iMasklen < 1 ){
		/* No given mask,return the string length */
		ph7_result_int(pCtx,iLen);
		return PH7_OK;
	}
	if( nArg > 2 ){
		int nOfft;
		/* Extract the offset */
		nOfft = ph7_value_to_int(apArg[2]);
		if( nOfft < 0 ){
			const char *zBase = &zString[iLen + nOfft];
			if( zBase > zString ){
				iLen = (int)(&zString[iLen]-zBase);
				zString = zBase;
			}else{
				/* Invalid offset */
				ph7_result_int(pCtx,0);
				return PH7_OK;
			}
		}else{
			if( nOfft >= iLen ){
				/* Invalid offset */
				ph7_result_int(pCtx,0);
				return PH7_OK;
			}else{
				/* Update offset */
				zString += nOfft;
				iLen -= nOfft;
			}
		}
		if( nArg > 3 ){
			int iUserlen;
			/* Extract the desired length */
			iUserlen = ph7_value_to_int(apArg[3]);
			if( iUserlen > 0 && iUserlen < iLen ){
				iLen = iUserlen;
			}
		}
	}
	/* Point to the end of the string */
	zEnd = &zString[iLen];
	/* Extract the first non-space token */
	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);
	if( rc == SXRET_OK && sToken.nByte > 0 ){
		/* Compare against the current mask */
		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);
	}
	/* Longest match */
	ph7_result_int(pCtx,iCount);
	return PH7_OK;
}
/*
 * string strpbrk(string $haystack,string $char_list)
 *  Search a string for any of a set of characters.
 * Parameters
 *  $haystack
 *   The string where char_list is looked for.
 *  $char_list
 *   This parameter is case sensitive.
 * Return
 *  Returns a string starting from the character found, or FALSE if it is not found.
 */
static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zString,*zList,*zEnd;
	int iLen,iListLen,i,c;
	sxu32 nOfft,nMax;
	sxi32 rc;
	if( nArg < 2 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the haystack and the char list */
	zString = ph7_value_to_string(apArg[0],&iLen);
	zList = ph7_value_to_string(apArg[1],&iListLen);
	if( iLen < 1 ){
		/* Nothing to process,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Point to the end of the string */
	zEnd = &zString[iLen];
	nOfft = nMax = SXU32_HIGH;
	/* perform the requested operation */
	for( i = 0 ; i < iListLen ; i++ ){
		c = zList[i];
		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);
		if( rc == SXRET_OK ){
			if( nMax < nOfft ){
				nOfft = nMax;
			}
		}
	}
	if( nOfft == SXU32_HIGH ){
		/* No such substring,return FALSE */
		ph7_result_bool(pCtx,0);
	}else{
		/* Return the substring */
		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));
	}
	return PH7_OK;
}
/* SPDX-SnippetBegin */
/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */
/* SPDX-License-Identifier: blessing */
/*
 * string soundex(string $str)
 *  Calculate the soundex key of a string.
 * Parameters
 *  $str
 *   The input string.
 * Return
 *  Returns the soundex key as a string.
 * Note:
 *  This implementation is based on the one found in the SQLite3
 * source tree.
 */
static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn;
	char zResult[8];
	int i, j;
	static const unsigned char iCode[] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,
		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,
		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,
		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,
	};
	if( nArg < 1 ){
		/* Missing arguments,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);
	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}
	if( zIn[i] ){
		unsigned char prevcode = iCode[zIn[i]&0x7f];
		zResult[0] = (char)SyToUpper(zIn[i]);
		for(j=1; j<4 && zIn[i]; i++){
			int code = iCode[zIn[i]&0x7f];
			if( code>0 ){
				if( code!=prevcode ){
					prevcode = (unsigned char)code;
					zResult[j++] = (char)code + '0';
				}
			}else{
				prevcode = 0;
			}
		}
		while( j<4 ){
			zResult[j++] = '0';
		}
		ph7_result_string(pCtx,zResult,4);
	}else{
	  /* No alphabetic character: PHP returns "0000" (not the SQLite "?000"). */
	  ph7_result_string(pCtx,"0000",4);
	}
	return PH7_OK;
}
/* SPDX-SnippetEnd */
/*
 * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])
 *  Wraps a string to a given number of characters.
 * Parameters
 *  $str
 *   The input string.
 * $width
 *  The column width.
 * $break
 *  The line is broken using the optional break parameter.
 * Return
 *  Returns the given string wrapped at the specified column.
 */
static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn,*zBreak;
	SyBlob sWorker;
	int iLen,iBreaklen,iWidth,iCut,iStart,iSpace,iCur;
	sxi32 rc;
	if( nArg < 1 ){
		/* Missing arguments,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Extract the input string */
	zIn = ph7_value_to_string(apArg[0],&iLen);
	/* Width (default 75; PHP allows 0/negative — break at every space). */
	iWidth = 75;
	if( nArg > 1 ){
		iWidth = ph7_value_to_int(apArg[1]);
	}
	/* Break string (default "\n"). */
	zBreak = "\n";
	iBreaklen = (int)sizeof(char);
	if( nArg > 2 ){
		zBreak = ph7_value_to_string(apArg[2],&iBreaklen);
	}
	/* Cut long words? (default false). */
	iCut = 0;
	if( nArg > 3 ){
		iCut = ph7_value_to_bool(apArg[3]);
	}
	if( iLen < 1 ){
		/* PHP returns the empty string for empty input before validating the other args. */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* PHP 8 domain errors (catchable ValueError). */
	if( iBreaklen < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"wordwrap(): Argument #3 ($break) must not be empty");
	}
	if( iWidth == 0 && iCut ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"wordwrap(): Argument #4 ($cut_long_words) cannot be true when argument #2 ($width) is 0");
	}
	/*
	 * PHP's algorithm: a single left-to-right pass tracking the start of the
	 * current line (iStart) and the position of the last space seen on it
	 * (iSpace). A break is emitted when the line reaches the width, at the last
	 * space if there was one, otherwise (only when cut is enabled) hard at the
	 * boundary. An existing break sequence in the input resets the line.
	 */
	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);
	iStart = iSpace = iCur = 0;
	rc = SXRET_OK;
	while( iCur < iLen ){
		if( iBreaklen <= iLen - iCur && SyMemcmp(&zIn[iCur],zBreak,(sxu32)iBreaklen) == 0 ){
			/* Existing break sequence in the input: copy it verbatim and reset the line. */
			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart + iBreaklen));
			if( rc != SXRET_OK ){ goto oom; }
			iCur += iBreaklen;
			iStart = iSpace = iCur;
			continue;
		}else if( zIn[iCur] == ' ' ){
			if( iCur - iStart >= iWidth ){
				/* The line already fills the width at this space: break here (the space is consumed). */
				rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));
				if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }
				if( rc != SXRET_OK ){ goto oom; }
				iStart = iCur + 1;
			}
			iSpace = iCur;
		}else if( iCut && iCur - iStart >= iWidth && iStart >= iSpace ){
			/* A word longer than the width with no space to break at: hard-cut at the boundary. */
			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));
			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }
			if( rc != SXRET_OK ){ goto oom; }
			iStart = iSpace = iCur;
		}else if( iCur - iStart >= iWidth && iStart < iSpace ){
			/* Past the width mid-word: wrap back to the last space (which is consumed). */
			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iSpace - iStart));
			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }
			if( rc != SXRET_OK ){ goto oom; }
			iStart = iSpace = iSpace + 1;
		}
		iCur++;
	}
	/* Emit the trailing chunk. */
	if( iStart < iCur ){
		rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));
		if( rc != SXRET_OK ){ goto oom; }
	}
	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));
	SyBlobRelease(&sWorker);
	return PH7_OK;
oom:
	SyBlobRelease(&sWorker);
	return PH7_ContextMemoryError(pCtx);
}
/*
 * Check if the given character is a member of the given mask.
 * Return TRUE on success. FALSE otherwise.
 * Refer to [strtok()].
 */
static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)
{
	int i;
	for( i = 0 ; i < nMasklen ; ++i ){
		if( c == zMask[i] ){
			if( pOfft ){
				*pOfft = i;
			}
			return TRUE;
		}
	}
	return FALSE;
}
/*
 * Extract a single token from the input stream.
 * Refer to [strtok()].
 */
static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)
{
	const char *zIn = *pzIn;
	const char *zPtr;
	/* Ignore leading delimiter */
	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){
		zIn++;
	}
	if( zIn >= zEnd ){
		/* End of input */
		return SXERR_EOF;
	}
	zPtr = zIn;
	/* Extract the token */
	while( zIn < zEnd ){
		if( (unsigned char)zIn[0] >= 0xc0 ){
			/* UTF-8 stream */
			zIn++;
			SX_JMP_UTF8(zIn,zEnd);
		}else{
			if( CheckMask(zIn[0],zMask,nMasklen,0) ){
				break;
			}
			zIn++;
		}
	}
	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);
	/* Update the cursor */
	*pzIn = zIn;
	/* Return to the caller */
	return SXRET_OK;
}
/* strtok auxiliary private data */
typedef struct strtok_aux_data strtok_aux_data;
struct strtok_aux_data
{
	const char *zDup;  /* Complete duplicate of the input */
	const char *zIn;   /* Current input stream */
	const char *zEnd;  /* End of input */
};
/*
 * string strtok(string $str,string $token)
 * string strtok(string $token)
 *  strtok() splits a string (str) into smaller strings (tokens), with each token
 *  being delimited by any character from token. That is, if you have a string like
 *  "This is an example string" you could tokenize this string into its individual
 *  words by using the space character as the token.
 *  Note that only the first call to strtok uses the string argument. Every subsequent
 *  call to strtok only needs the token to use, as it keeps track of where it is in
 *  the current string. To start over, or to tokenize a new string you simply call strtok
 *  with the string argument again to initialize it. Note that you may put multiple tokens
 *  in the token parameter. The string will be tokenized when any one of the characters in
 *  the argument are found.
 * Parameters
 *  $str
 *  The string being split up into smaller strings (tokens).
 * $token
 *  The delimiter used when splitting up str.
 * Return
 *   Current token or FALSE on EOF.
 */
static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	strtok_aux_data *pAux;
	const char *zMask;
	SyString sToken;
	int nMasklen;
	sxi32 rc;
	if( nArg < 2 ){
		/* Extract top aux data */
		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);
		if( pAux == 0 ){
			/* No aux data,return FALSE */
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		nMasklen = 0;
		zMask = ""; /* cc warning */
		if( nArg > 0 ){
			/* Extract the mask */
			zMask = ph7_value_to_string(apArg[0],&nMasklen);
		}
		if( nMasklen < 1 ){
			/* Invalid mask,return FALSE */
			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);
			ph7_context_free_chunk(pCtx,pAux);
			(void)ph7_context_pop_aux_data(pCtx);
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		/* Extract the token */
		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);
		if( rc != SXRET_OK ){
			/* EOF ,discard the aux data */
			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);
			ph7_context_free_chunk(pCtx,pAux);
			(void)ph7_context_pop_aux_data(pCtx);
			ph7_result_bool(pCtx,0);
		}else{
			/* Return the extracted token */
			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);
		}
	}else{
		const char *zInput,*zCur;
		char *zDup;
		int nLen;
		/* Extract the raw input */
		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);
		if( nLen < 1 ){
			/* Empty input,return FALSE */
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		/* Extract the mask */
		zMask = ph7_value_to_string(apArg[1],&nMasklen);
		if( nMasklen < 1 ){
			/* Set a default mask */
#define TOK_MASK " \n\t\r\f"
			zMask = TOK_MASK;
			nMasklen = (int)sizeof(TOK_MASK) - 1;
#undef TOK_MASK
		}
		/* Extract a single token */
		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);
		if( rc != SXRET_OK ){
			/* Empty input */
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}else{
			/* Return the extracted token */
			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);
		}
		/* Create our auxilliary data and copy the input */
		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);
		if( pAux ){
			nLen -= (int)(zInput-zCur);
			if( nLen < 1 ){
				ph7_context_free_chunk(pCtx,pAux);
				return PH7_OK;
			}
			/* Duplicate input */
			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);
			if( zDup  ){
				SyMemcpy(zInput,zDup,(sxu32)nLen);
				/* Register the aux data */
				pAux->zDup = pAux->zIn = zDup;
				pAux->zEnd = &zDup[nLen];
				ph7_context_push_aux_data(pCtx,pAux);
			}
		}
	}
	return PH7_OK;
}
/*
 * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])
 *  Pad a string to a certain length with another string
 * Parameters
 *  $input
 *   The input string.
 * $pad_length
 *   If the value of pad_length is negative, less than, or equal to the length of the input
 *   string, no padding takes place.
 * $pad_string
 *   Note:
 *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly
 *    divided by the pad_string's length.
 * $pad_type
 *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type
 *    is not specified it is assumed to be STR_PAD_RIGHT.
 * Return
 *  The padded string.
 */
static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;
	const char *zIn,*zPad;
	if( nArg < 2 ){
		/* Missing arguments,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn = ph7_value_to_string(apArg[0],&iLen);
	/* Padding length */
	{
		sxi64 iTmp = 0;
		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_pad",2,"$length","int",&iTmp);
		if( rcArg != PH7_OK ){
			return rcArg;
		}
		iRealPad = iPadlen = (int)iTmp;
	}
	if( iPadlen > 0 ){
		iPadlen -= iLen;
	}
	if( iPadlen < 1  ){
		/* Return the string verbatim */
		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }
		return PH7_OK;
	}
	zPad = " "; /* Whitespace padding */
	iStrpad = (int)sizeof(char);
	iType = 1 ; /* STR_PAD_RIGHT */
	if( nArg > 2 ){
		/* Padding string */
		zPad = ph7_value_to_string(apArg[2],&iStrpad);
		if( iStrpad < 1 ){
			/* An empty pad string throws a catchable ValueError in PHP 8
			 * (only reached once padding is actually required). */
			return PH7_VmThrowException(pCtx,"ValueError",
				"str_pad(): Argument #3 ($pad_string) must not be empty");
		}
		if( nArg > 3 ){
			/* Padd type */
			iType = ph7_value_to_int(apArg[3]);
			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){
				iType = 1 ; /* STR_PAD_RIGHT */
			}
		}
	}
	iDiv = 1;
	if( iType == 2 ){
		iDiv = 2; /* STR_PAD_BOTH */
	}
	/* Perform the requested operation */
	if( iType == 0 /* STR_PAD_LEFT */ || iType == 2 /* STR_PAD_BOTH */ ){
		jPad = iStrpad;
		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){
			/* Padding */
			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){
				break;
			}
			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }
		}
		if( iType == 0 /* STR_PAD_LEFT */ ){
			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){
				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );
				if( jPad > iStrpad ){
					jPad = iStrpad;
				}
				if( jPad < 1){
					break;
				}
				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }
			}
		}
	}
	if( iLen > 0 ){
		/* Append the input string */
		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }
	}
	if( iType == 1 /* STR_PAD_RIGHT */ || iType == 2 /* STR_PAD_BOTH */ ){
		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){
			/* Padding */
			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){
				break;
			}
			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }
		}
		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){
			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);
			if( jPad > iStrpad ){
				jPad = iStrpad;
			}
			if( jPad < 1){
				break;
			}
			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }
		}
	}
	return PH7_OK;
}
/*
 * String replacement private data.
 */
typedef struct str_replace_data str_replace_data;
struct str_replace_data
{
	/* Used by the str_replace family to collect the search/replace arguments. */
	SySet *pCollector;  /* Argument collector*/
	ph7_context *pCtx;  /* Call context */
	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */
};
/*
 * Remove a substring.
 */
#define STRDEL(SRC,SLEN,OFFT,ILEN){\
	for(;;){\
		if( OFFT + ILEN >= SLEN ) { break; }\
		SRC[OFFT] = SRC[OFFT+ILEN];\
		++OFFT;\
	}\
}
/*
 * Shift right and insert algorithm.
 */
#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\
		sxu32 INLEN = LEN - OFFT;\
		for(;;){\
			if( LEN > 0 ){ LEN--; }\
			if(INLEN < 1 ) { break; }\
			SRC[LEN + ELEN] = SRC[LEN];\
			--INLEN; \
		}\
		for(;;){\
				if(ELEN < 1) { break; }\
				SRC[OFFT] = ENTRY[0];\
				OFFT++;\
				ENTRY++;\
				--ELEN;\
		}\
}
/*
 * Replace all occurrences of the search string at offset (nOfft) with the given
 * replacement string [i.e: zReplace].
 */
static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)
{
	char *zInput = (char *)SyBlobData(pWorker);
	sxu32 n,m;
	n = SyBlobLength(pWorker);
	m = nOfft;
	/* Delete the old entry */
	STRDEL(zInput,n,m,nLen);
	SyBlobLength(pWorker) -= nLen;
	if( nReplen > 0 ){
		sxi32 iRep = nReplen;
		sxi32 rc;
		/*
		 * Make sure the working buffer is big enough to hold the replacement
		 * string.
		 */
		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);
		if( rc != SXRET_OK ){
			/* Propagate the allocation failure so the caller can raise a fatal
			 * instead of returning a partially-replaced string as success. */
			return rc;
		}
		/* Perform the insertion now */
		zInput = (char *)SyBlobData(pWorker);
		n = SyBlobLength(pWorker);
		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);
		SyBlobLength(pWorker) += nReplen;
	}
	return SXRET_OK;
}
/*
 * The following walker callback is invoked by the str_rplace() function inorder
 * to collect search/replace string.
 * This callback is invoked only if the given argument is of type array.
 */
static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)
{
	str_replace_data *pRep = (str_replace_data *)pUserData;
	SyString sWorker;
	const char *zIn;
	int nByte;
	/* Extract a string representation of the given argument */
	zIn = ph7_value_to_string(pData,&nByte);
	SyStringInitFromBuf(&sWorker,0,0);
	if( nByte > 0 ){
		char *zDup;
		/* Duplicate the chunk */
		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,
			TRUE /* Release the chunk automatically,upon this context is destroyd */
			);
		if( zDup == 0 ){
			/* Allocation failure: carry it out and stop the walk so the caller
			 * raises a fatal instead of silently dropping a search/replace term. */
			pRep->rc = SXERR_MEM;
			return SXERR_MEM;
		}
		SyMemcpy(zIn,zDup,(sxu32)nByte);
		/* Save the chunk */
		SyStringInitFromBuf(&sWorker,zDup,nByte);
	}
	/* Save for later processing */
	SySetPut(pRep->pCollector,(const void *)&sWorker);
	/* All done */
	SXUNUSED(pKey); /* cc warning */
	return PH7_OK;
}
/*
 * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])
 * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])
 *  Replace all occurrences of the search string with the replacement string.
 * Parameters
 *  If search and replace are arrays, then str_replace() takes a value from each
 *  array and uses them to search and replace on subject. If replace has fewer values
 *  than search, then an empty string is used for the rest of replacement values.
 *  If search is an array and replace is a string, then this replacement string is used
 *  for every value of search. The converse would not make sense, though.
 *  If search or replace are arrays, their elements are processed first to last.
 * $search
 *  The value being searched for, otherwise known as the needle. An array may be used
 *  to designate multiple needles.
 * $replace
 *  The replacement value that replaces found search values. An array may be used
 *  to designate multiple replacements.
 * $subject
 *  The string or array being searched and replaced on, otherwise known as the haystack.
 *  If subject is an array, then the search and replace is performed with every entry
 *  of subject, and the return value is an array as well.
 * $count (Not used)
 *  If passed, this will be set to the number of replacements performed.
 * Return
 * This function returns a string or an array with the replaced values.
 */
static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyString sTemp,*pSearch,*pReplace;
	ProcStringMatch xMatch;
	const char *zIn,*zFunc;
	str_replace_data sRep;
	SyBlob sWorker;
	SySet sReplace;
	SySet sSearch;
	int rep_str;
	int nByte;
	sxi32 rc;
	if( nArg < 3 ){
		/* Missing/Invalid arguments,return null */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Initialize fields */
	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));
	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));
	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);
	SyZero(&sRep,sizeof(str_replace_data));
	sRep.pCtx = pCtx;
	sRep.pCollector = &sSearch;
	rep_str = 0;
	/* Extract the subject */
	zIn = ph7_value_to_string(apArg[2],&nByte);
	if( nByte < 1 ){
		/* Nothing to replace,return the empty string */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* Copy the subject */
	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);
	/* Search string */
	if( ph7_value_is_array(apArg[0]) ){
		/* Collect search string */
		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);
	}else{
		/* Single pattern */
		zIn = ph7_value_to_string(apArg[0],&nByte);
		if( nByte < 1 ){
			/* Return the subject untouched since no search string is available */
			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);
			return PH7_OK;
		}
		SyStringInitFromBuf(&sTemp,zIn,nByte);
		/* Save for later processing */
		SySetPut(&sSearch,(const void *)&sTemp);
	}
	/* Replace string */
	if( ph7_value_is_array(apArg[1]) ){
		/* Collect replace string */
		sRep.pCollector = &sReplace;
		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);
	}else{
		/* Single needle */
		zIn = ph7_value_to_string(apArg[1],&nByte);
		rep_str = 1;
		SyStringInitFromBuf(&sTemp,zIn,nByte);
		/* Save for later processing */
		SySetPut(&sReplace,(const void *)&sTemp);
	}
	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */
	if( sRep.rc != SXRET_OK ){
		SySetRelease(&sSearch);
		SySetRelease(&sReplace);
		SyBlobRelease(&sWorker);
		return PH7_ContextMemoryError(pCtx);
	}
	/* Reset loop cursors */
	SySetResetCursor(&sSearch);
	SySetResetCursor(&sReplace);
	pReplace = pSearch = 0; /* cc warning */
	SyStringInitFromBuf(&sTemp,"",0);
	/* Extract function name */
	zFunc = ph7_function_name(pCtx);
	/* Set the default pattern match routine */
	xMatch = SyBlobSearch;
	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){
		/* Case insensitive pattern match */
		xMatch = iPatternMatch;
	}
	/* Start the replace process */
	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){
		sxu32 nCount,nOfft;
		if( pSearch->nByte <  1 ){
			/* Empty string,ignore */
			continue;
		}
		/* Extract the replace string */
		if( rep_str ){
			pReplace = (SyString *)SySetPeek(&sReplace);
		}else{
			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){
				/* Sepecial case when 'replace set' has fewer values than the search set.
				 * An empty string is used for the rest of replacement values
				 */
				pReplace = 0;
			}
		}
		if( pReplace == 0 ){
			/* Use an empty string instead */
			pReplace = &sTemp;
		}
		nOfft = nCount = 0;
		for(;;){
			if( nCount >= SyBlobLength(&sWorker) ){
				break;
			}
			/* Perform a pattern lookup */
			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,
				pSearch->nByte,&nOfft);
			if( rc != SXRET_OK ){
				/* Pattern not found */
				break;
			}
			/* Perform the replace operation */
			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);
			if( rc != SXRET_OK ){
				/* Allocation failure: surface a fatal instead of a partial result */
				SySetRelease(&sSearch);
				SySetRelease(&sReplace);
				SyBlobRelease(&sWorker);
				return PH7_ContextMemoryError(pCtx);
			}
			/* Increment offset counter */
			nCount += nOfft + pReplace->nByte;
		}
	}
	/* All done,clean-up the mess left behind */
	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));
	SySetRelease(&sSearch);
	SySetRelease(&sReplace);
	SyBlobRelease(&sWorker);
	if( rc != PH7_OK ){
		return PH7_ContextMemoryError(pCtx);
	}
	return PH7_OK;
}
/*
 * strtr() array form: a single (key,value) pair copied out of the replace_pairs
 * array. The bytes are owned by a persistent pool (see strtr_collect) rather than
 * the transient walker values, which HashmapWalk releases after each callback, so
 * we store byte offsets into that pool instead of raw pointers.
 */
typedef struct strtr_entry strtr_entry;
struct strtr_entry
{
	sxu32 nKeyOfft; /* Offset of the search key inside the pool */
	sxu32 nKeyLen;  /* Length of the search key */
	sxu32 nValOfft; /* Offset of the replacement inside the pool */
	sxu32 nValLen;  /* Length of the replacement */
};
typedef struct strtr_collect strtr_collect;
struct strtr_collect
{
	SyBlob *pPool;  /* Byte pool holding copied key + value bytes */
	SySet  *pTable; /* Set of strtr_entry (parallel offsets into pPool) */
	sxi32   rc;     /* Carries an allocation failure (SXERR_MEM) out of the walker */
};
/*
 * Collect one replace_pairs entry into the persistent pool/offset table.
 * PHP coerces both the key and the value to string (an integer key becomes its
 * decimal form) and ignores an empty-string key.
 */
static int StrtrCollectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)
{
	strtr_collect *pCol = (strtr_collect *)pUserData;
	const char *zKey,*zVal;
	strtr_entry sEnt;
	int nKey,nVal;
	zKey = ph7_value_to_string(pKey,&nKey);
	if( nKey < 1 ){
		/* PHP ignores an empty-string key (it also emits a warning we do not replicate). */
		return PH7_OK;
	}
	zVal = ph7_value_to_string(pData,&nVal);
	sEnt.nKeyOfft = SyBlobLength(pCol->pPool);
	sEnt.nKeyLen  = (sxu32)nKey;
	if( SyBlobAppend(pCol->pPool,(const void *)zKey,(sxu32)nKey) != SXRET_OK ){
		pCol->rc = SXERR_MEM;
		return SXERR_ABORT;
	}
	sEnt.nValOfft = SyBlobLength(pCol->pPool);
	sEnt.nValLen  = (sxu32)nVal;
	if( nVal > 0 && SyBlobAppend(pCol->pPool,(const void *)zVal,(sxu32)nVal) != SXRET_OK ){
		pCol->rc = SXERR_MEM;
		return SXERR_ABORT;
	}
	if( SySetPut(pCol->pTable,(const void *)&sEnt) != SXRET_OK ){
		pCol->rc = SXERR_MEM;
		return SXERR_ABORT;
	}
	return PH7_OK;
}
/*
 * string strtr(string $str,string $from,string $to)
 * string strtr(string $str,array $replace_pairs)
 *  Translate characters or replace substrings.
 * Parameters
 *  $str
 *  The string being translated.
 * $from
 *  The string being translated to to.
 * $to
 *  The string replacing from.
 * $replace_pairs
 *  The replace_pairs parameter may be used instead of to and
 *  from, in which case it's an array in the form array('from' => 'to', ...).
 * Return
 *  The translated string.
 *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.
 */
static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nLen;
	if( nArg < 1 ){
		/* Nothing to replace,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zIn = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 || nArg < 2 ){
		/* Invalid arguments */
		ph7_result_string(pCtx,zIn,nLen);
		return PH7_OK;
	}
	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){
		strtr_collect sCol;
		SyBlob sPool,sWorker;
		SySet sTable;
		const char *zPool;
		strtr_entry *pEnt;
		sxi32 rc;
		int i,iRun;
		/*
		 * PHP's array-form strtr is a single left-to-right pass over the subject:
		 * at every position it substitutes the LONGEST replace_pairs key that
		 * matches there, then advances past the key (replacements are never
		 * rescanned). It is not a sequential per-key global replace. First copy
		 * the pairs into a persistent pool, then run that scan.
		 */
		SyBlobInit(&sPool,&pCtx->pVm->sAllocator);
		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);
		SySetInit(&sTable,&pCtx->pVm->sAllocator,sizeof(strtr_entry));
		sCol.pPool  = &sPool;
		sCol.pTable = &sTable;
		sCol.rc     = SXRET_OK;
		ph7_array_walk(apArg[1],StrtrCollectWalker,&sCol);
		if( sCol.rc != SXRET_OK ){
			/* Allocation failure while collecting the pairs: surface a fatal */
			SyBlobRelease(&sPool);
			SyBlobRelease(&sWorker);
			SySetRelease(&sTable);
			return PH7_ContextMemoryError(pCtx);
		}
		/* The pool is now stable, so offsets can be resolved against its base. */
		zPool = (const char *)SyBlobData(&sPool);
		rc = SXRET_OK;
		iRun = 0; /* Start of the pending run of unmatched bytes copied verbatim. */
		for( i = 0 ; i < nLen ; ){
			strtr_entry *pBest = 0;
			sxu32 nBest = 0;
			/* Pick the longest key that matches at the current position. */
			SySetResetCursor(&sTable);
			while( SXRET_OK == SySetGetNextEntry(&sTable,(void **)&pEnt) ){
				if( pEnt->nKeyLen > nBest
					&& pEnt->nKeyLen <= (sxu32)(nLen - i)
					&& SyMemcmp(zPool + pEnt->nKeyOfft,zIn + i,pEnt->nKeyLen) == 0 ){
					nBest = pEnt->nKeyLen;
					pBest = pEnt;
				}
			}
			if( pBest == 0 ){
				/* No key here: extend the literal run and copy it in one shot later. */
				i++;
				continue;
			}
			/* Flush the pending literal run, then the replacement. */
			if( i > iRun ){
				rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(i - iRun));
			}
			if( rc == SXRET_OK && pBest->nValLen > 0 ){
				rc = SyBlobAppend(&sWorker,zPool + pBest->nValOfft,pBest->nValLen);
			}
			if( rc != SXRET_OK ){
				SyBlobRelease(&sPool);
				SyBlobRelease(&sWorker);
				SySetRelease(&sTable);
				return PH7_ContextMemoryError(pCtx);
			}
			i += (int)pBest->nKeyLen;
			iRun = i;
		}
		/* Flush the trailing literal run. */
		if( nLen > iRun ){
			rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(nLen - iRun));
			if( rc != SXRET_OK ){
				SyBlobRelease(&sPool);
				SyBlobRelease(&sWorker);
				SySetRelease(&sTable);
				return PH7_ContextMemoryError(pCtx);
			}
		}
		/* All done, return the result string */
		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),
			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */
		/* Clean-up */
		SyBlobRelease(&sPool);
		SyBlobRelease(&sWorker);
		SySetRelease(&sTable);
		if( rc != PH7_OK ){
			return PH7_ContextMemoryError(pCtx);
		}
	}else{
		int i,flen,tlen,c,iOfft;
		const char *zFrom,*zTo;
		if( nArg < 3 ){
			/* Nothing to replace */
			ph7_result_string(pCtx,zIn,nLen);
			return PH7_OK;
		}
		/* Extract given arguments */
		zFrom = ph7_value_to_string(apArg[1],&flen);
		zTo = ph7_value_to_string(apArg[2],&tlen);
		if( flen < 1 || tlen < 1 ){
			/* Nothing to replace */
			ph7_result_string(pCtx,zIn,nLen);
			return PH7_OK;
		}
		/* Start the replace process */
		for( i = 0 ; i < nLen ; ++i ){
			c = zIn[i];
			if( CheckMask(c,zFrom,flen,&iOfft) ){
				if ( iOfft < tlen ){
					c = zTo[iOfft];
				}
			}
			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));

		}
	}
	return PH7_OK;
}
#endif /* PH7_NEED_BUILTIN_REG */
#ifdef PH7_NEED_FMT_AND_INI
/*
 * Parse an INI string.

 * According to wikipedia
 *  The INI file format is an informal standard for configuration files for some platforms or software.
 *  INI files are simple text files with a basic structure composed of "sections" and "properties".
 *  Format
*    Properties
*     The basic element contained in an INI file is the property. Every property has a name and a value
*     delimited by an equals sign (=). The name appears to the left of the equals sign.
*     Example:
*      name=value
*    Sections
*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself
*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.
*     There is no explicit "end of section" delimiter; sections end at the next section declaration
*     or the end of the file. Sections may not be nested.
*     Example:
*      [section]
*   Comments
*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.
* This function return an array holding parsed values on success.FALSE otherwise.
*/
PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)
{
	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;
	const char *zCur,*zEnd = &zIn[nByte];
	SyHashEntry *pEntry;
	SyString sEntry;
	SyHash sHash;
	int c;
	/* Create an empty array and worker variables */
	pArray = ph7_context_new_array(pCtx);
	pWorker = ph7_context_new_scalar(pCtx);
	pValue = ph7_context_new_scalar(pCtx);
	if( pArray == 0 || pWorker == 0 || pValue == 0){
		/* Out of memory: surface a fatal instead of returning FALSE */
		return PH7_ContextMemoryError(pCtx);
	}
	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);
	pCur = pArray;
	/* Start the parse process */
	for(;;){
		/* Ignore leading white spaces */
		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){
			zIn++;
		}
		if( zIn >= zEnd ){
			/* No more input to process */
			break;
		}
		if( zIn[0] == ';' || zIn[0] == '#' ){
			/* Comment til the end of line */
			zIn++;
			while(zIn < zEnd && zIn[0] != '\n' ){
				zIn++;
			}
			continue;
		}
		/* Reset the string cursor of the working variable */
		ph7_value_reset_string_cursor(pWorker);
		if( zIn[0] == '[' ){
			/* Section: Extract the section name */
			zIn++;
			zCur = zIn;
			while( zIn < zEnd && zIn[0] != ']' ){
				zIn++;
			}
			if( zIn > zCur && bProcessSection ){
				/* Save the section name */
				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));
				SyStringFullTrim(&sEntry);
				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);
				if( sEntry.nByte > 0 ){
					/* Associate an array with the section */
					pSection = ph7_context_new_array(pCtx);
					if( pSection ){
						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);
						pCur = pSection;
					}
				}
			}
			zIn++; /* Trailing square brackets ']' */
		}else{
			ph7_value *pOldCur;
			int is_array;
			int iLen;
			/* Properties */
			is_array = 0;
			zCur = zIn;
			iLen = 0; /* cc warning */
			pOldCur = pCur;
			while( zIn < zEnd && zIn[0] != '=' ){
				if( zIn[0] == '[' && !is_array ){
					/* Array */
					iLen = (int)(zIn-zCur);
					is_array = 1;
					if( iLen > 0 ){
						ph7_value *pvArr = 0; /* cc warning */
						/* Query the hashtable */
						SyStringInitFromBuf(&sEntry,zCur,iLen);
						SyStringFullTrim(&sEntry);
						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);
						if( pEntry ){
							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);
						}else{
							/* Create an empty array */
							pvArr = ph7_context_new_array(pCtx);
							if( pvArr ){
								/* Save the entry */
								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);
								/* Insert the entry */
								ph7_value_reset_string_cursor(pWorker);
								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);
								ph7_array_add_elem(pCur,pWorker,pvArr);
								ph7_value_reset_string_cursor(pWorker);
							}
						}
						if( pvArr ){
							pCur = pvArr;
						}
					}
					while ( zIn < zEnd && zIn[0] != ']' ){
						zIn++;
					}
				}
				zIn++;
			}
			if( !is_array ){
				iLen = (int)(zIn-zCur);
			}
			/* Trim the key */
			SyStringInitFromBuf(&sEntry,zCur,iLen);
			SyStringFullTrim(&sEntry);
			if( sEntry.nByte > 0 ){
				if( !is_array ){
					/* Save the key name */
					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);
				}
				/* extract key value */
				ph7_value_reset_string_cursor(pValue);
				zIn++; /* '=' */
				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){
					zIn++;
				}
				if( zIn < zEnd ){
					zCur = zIn;
					c = zIn[0];
					if( c == '"' || c == '\'' ){
						zIn++;
						/* Delimit the value */
						while( zIn < zEnd ){
							if ( zIn[0] == c && zIn[-1] != '\\' ){
								break;
							}
							zIn++;
						}
						if( zIn < zEnd ){
							zIn++;
						}
					}else{
						while( zIn < zEnd ){
							if( zIn[0] == '\n' ){
								if( zIn[-1] != '\\' ){
									break;
								}
							}else if( zIn[0] == ';' || zIn[0] == '#' ){
								/* Inline comments */
								break;
							}
							zIn++;
						}
					}
					/* Trim the value */
					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));
					SyStringFullTrim(&sEntry);
					if( c == '"' || c == '\'' ){
						SyStringTrimLeadingChar(&sEntry,c);
						SyStringTrimTrailingChar(&sEntry,c);
					}
					if( sEntry.nByte > 0 ){
						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);
					}
					/* Insert the key and it's value */
					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);
				}
			}else{
				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) || zIn[0] == '=' ) ){
					zIn++;
				}
			}
			pCur = pOldCur;
		}
	}
	SyHashRelease(&sHash);
	/* Return the parse of the INI string */
	ph7_result_value(pCtx,pArray);
	return SXRET_OK;
}
/*
 * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])
 *  Parse a configuration string.
 * Parameters
 *  $ini
 *   The contents of the ini file being parsed.
 *  $process_sections
 *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names
 *   and settings included. The default for process_sections is FALSE.
 *  $scanner_mode (Not used)
 *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied
 *   then option values will not be parsed.
 * Return
 *  The settings are returned as an associative array on success, and FALSE on failure.
 */
static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIni;
	int nByte;
	if( nArg < 1 || !ph7_value_is_string(apArg[0]) ){
		/* Missing/Invalid arguments,return FALSE*/
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the raw INI buffer */
	zIni = ph7_value_to_string(apArg[0],&nByte);
	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */
	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);
}
#endif /* PH7_NEED_FMT_AND_INI */

#ifdef PH7_NEED_BUILTIN_REG

/*
 * Ctype Functions.
 * Status:
 *    Stable.
 */
/*
 * bool ctype_alnum(string $text)
 *  Checks if all of the characters in the provided string, text, are alphanumeric.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.
 */
static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( !SyisAlphaNum(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * bool ctype_alpha(string $text)
 *  Checks if all of the characters in the provided string, text, are alphabetic.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.
 */
static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( !SyisAlpha(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * bool ctype_cntrl(string $text)
 *  Checks if all of the characters in the provided string, text, are control characters.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *  TRUE if every character in text is a control characters,FALSE otherwise.
 */
static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( zIn[0] >= 0xc0 ){
			/* UTF-8 stream  */
			break;
		}
		if( !SyisCtrl(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * bool ctype_digit(string $text)
 *  Checks if all of the characters in the provided string, text, are numerical.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.
 */
static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( zIn[0] >= 0xc0 ){
			/* UTF-8 stream  */
			break;
		}
		if( !SyisDigit(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * bool ctype_xdigit(string $text)
 *  Check for character(s) representing a hexadecimal digit.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *  Returns TRUE if every character in text is a hexadecimal 'digit', that is
 * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.
 */
static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( zIn[0] >= 0xc0 ){
			/* UTF-8 stream  */
			break;
		}
		if( !SyisHex(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * bool ctype_graph(string $text)
 *  Checks if all of the characters in the provided string, text, creates visible output.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *  Returns TRUE if every character in text is printable and actually creates visible output
 * (no white space), FALSE otherwise.
 */
static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( zIn[0] >= 0xc0 ){
			/* UTF-8 stream  */
			break;
		}
		if( !SyisGraph(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * bool ctype_print(string $text)
 *  Checks if all of the characters in the provided string, text, are printable.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *  Returns TRUE if every character in text will actually create output (including blanks).
 *  Returns FALSE if text contains control characters or characters that do not have any output
 *  or control function at all.
 */
static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( zIn[0] >= 0xc0 ){
			/* UTF-8 stream  */
			break;
		}
		if( !SyisPrint(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * bool ctype_punct(string $text)
 *  Checks if all of the characters in the provided string, text, are punctuation character.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *  Returns TRUE if every character in text is printable, but neither letter
 *  digit or blank, FALSE otherwise.
 */
static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( zIn[0] >= 0xc0 ){
			/* UTF-8 stream  */
			break;
		}
		if( !SyisPunct(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * bool ctype_space(string $text)
 *  Checks if all of the characters in the provided string, text, creates whitespace.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.
 *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return
 *  and form feed characters.
 */
static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( zIn[0] >= 0xc0 ){
			/* UTF-8 stream  */
			break;
		}
		if( !SyisSpace(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * bool ctype_lower(string $text)
 *  Checks if all of the characters in the provided string, text, are lowercase letters.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *  Returns TRUE if every character in text is a lowercase letter in the current locale.
 */
static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( !SyisLower(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * bool ctype_upper(string $text)
 *  Checks if all of the characters in the provided string, text, are uppercase letters.
 * Parameters
 *  $text
 *   The tested string.
 * Return
 *  Returns TRUE if every character in text is a uppercase letter in the current locale.
 */
static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const unsigned char *zIn,*zEnd;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string */
	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);
	zEnd = &zIn[nLen];
	if( nLen < 1 ){
		/* Empty string,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the requested operation */
	for(;;){
		if( zIn >= zEnd ){
			/* If we reach the end of the string,then the test succeeded. */
			ph7_result_bool(pCtx,1);
			return PH7_OK;
		}
		if( !SyisUpper(zIn[0]) ){
			break;
		}
		/* Point to the next character */
		zIn++;
	}
	/* The test failed,return FALSE */
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/* Date/Time functions moved to builtin_date.c */
/*
 * Section:
 *    URL handling Functions.
 * Status:
 *    Stable.
 */
/*
 * Output consumer callback for the standard Symisc routines.
 * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].
 */
static int Consumer(const void *pData,unsigned int nLen,void *pUserData)
{
	/* Store in the call context result buffer */
	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);
	return SXRET_OK;
}
/*
 * string base64_encode(string $data)
 * string convert_uuencode(string $data)
 *  Encodes data with MIME base64
 * Parameter
 *  $data
 *    Data to encode
 * Return
 *  Encoded data or FALSE on failure.
 */
static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the input string */
	zIn = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Nothing to process,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the BASE64 encoding */
	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);
	return PH7_OK;
}
/*
 * string base64_decode(string $data)
 * string convert_uudecode(string $data)
 *  Decodes data encoded with MIME base64
 * Parameter
 *  $data
 *    Encoded data.
 * Return
 *  Returns the original data or FALSE on failure.
 */
static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the input string */
	zIn = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Nothing to process,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the BASE64 decoding */
	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);
	return PH7_OK;
}
/*
 * string urlencode(string $str)
 *  URL encoding
 * Parameter
 *  $data
 *   Input string.
 * Return
 *  Returns a string in which all non-alphanumeric characters except -_. have
 *  been replaced with a percent (%) sign followed by two hex digits and spaces
 *  encoded as plus (+) signs.
 */
static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the input string */
	zIn = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Nothing to process,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the URL encoding */
	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);
	return PH7_OK;
}
/*
 * string urldecode(string $str)
 *  Decodes any %## encoding in the given string.
 *  Plus symbols ('+') are decoded to a space character.
 * Parameter
 *  $data
 *    Input string.
 * Return
 *  Decoded URL or FALSE on failure.
 */
static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	int nLen;
	if( nArg < 1 ){
		/* Missing arguments,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Extract the input string */
	zIn = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* Nothing to process,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* Perform the URL decoding */
	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);
	return PH7_OK;
}
#endif /* PH7_NEED_BUILTIN_REG */
/* Table of the built-in functions */
static const ph7_builtin_func aBuiltInFunc[] = {
	   /* Variable handling functions */
	{ "is_bool"    , PH7_builtin_is_bool     },
	{ "is_float"   , PH7_builtin_is_float    },
	{ "is_real"    , PH7_builtin_is_float    },
	{ "is_double"  , PH7_builtin_is_float    },
	{ "is_int"     , PH7_builtin_is_int      },
	{ "is_integer" , PH7_builtin_is_int      },
	{ "is_long"    , PH7_builtin_is_int      },
	{ "is_string"  , PH7_builtin_is_string   },
	{ "is_null"    , PH7_builtin_is_null     },
	{ "is_numeric" , PH7_builtin_is_numeric  },
	{ "is_scalar"  , PH7_builtin_is_scalar   },
	{ "is_array"   , PH7_builtin_is_array    },
	{ "is_object"  , PH7_builtin_is_object   },
	{ "is_resource", PH7_builtin_is_resource },
	{ "douleval"   , PH7_builtin_floatval    },
	{ "floatval"   , PH7_builtin_floatval    },
	{ "intval"     , PH7_builtin_intval      },
	{ "strval"     , PH7_builtin_strval      },
	{ "boolval"    , PH7_builtin_boolval     },
	{ "empty"      , PH7_builtin_empty       },
#ifdef PH7_NEED_BUILTIN_REG
#ifdef PH7_ENABLE_MATH_FUNC
	   /* Math functions */
	{ "abs"  ,    PH7_builtin_abs          },
	{ "sqrt" ,    PH7_builtin_sqrt         },
	{ "exp"  ,    PH7_builtin_exp          },
	{ "floor",    PH7_builtin_floor        },
	{ "cos"  ,    PH7_builtin_cos          },
	{ "sin"  ,    PH7_builtin_sin          },
	{ "acos" ,    PH7_builtin_acos         },
	{ "asin" ,    PH7_builtin_asin         },
	{ "cosh" ,    PH7_builtin_cosh         },
	{ "sinh" ,    PH7_builtin_sinh         },
	{ "ceil" ,    PH7_builtin_ceil         },
	{ "tan"  ,    PH7_builtin_tan          },
	{ "tanh" ,    PH7_builtin_tanh         },
	{ "atan" ,    PH7_builtin_atan         },
	{ "atan2",    PH7_builtin_atan2        },
	{ "log"  ,    PH7_builtin_log          },
	{ "log10" ,   PH7_builtin_log10        },
	{ "pow"  ,    PH7_builtin_pow          },
	{ "pi",       PH7_builtin_pi           },
	{ "fmod",     PH7_builtin_fmod         },
	{ "hypot",    PH7_builtin_hypot        },
#endif /* PH7_ENABLE_MATH_FUNC */
	{ "round",    PH7_builtin_round        },
	{ "intdiv",   PH7_builtin_intdiv       },
	{ "dechex", PH7_builtin_dechex         },
	{ "decoct", PH7_builtin_decoct         },
	{ "decbin", PH7_builtin_decbin         },
	{ "hexdec", PH7_builtin_hexdec         },
	{ "bindec", PH7_builtin_bindec         },
	{ "octdec", PH7_builtin_octdec         },
	{ "srand",  PH7_builtin_srand          },
	{ "mt_srand",PH7_builtin_srand         },
#endif /* PH7_NEED_BUILTIN_REG */
#ifdef PH7_NEED_FMT_AND_INI
	{ "base_convert", PH7_builtin_base_convert },
#endif /* PH7_NEED_FMT_AND_INI */
#ifdef PH7_NEED_BUILTIN_REG
	   /* String handling functions */

	{ "substr",          PH7_builtin_substr     },
	{ "substr_compare",  PH7_builtin_substr_compare },
	{ "substr_count",    PH7_builtin_substr_count },
	{ "substr_replace",  PH7_builtin_substr_replace },
	{ "levenshtein",     PH7_builtin_levenshtein },
	{ "similar_text",    PH7_builtin_similar_text },
	{ "str_word_count",  PH7_builtin_str_word_count },
	{ "chunk_split",     PH7_builtin_chunk_split},
	{ "addslashes" ,     PH7_builtin_addslashes },
	{ "addcslashes",     PH7_builtin_addcslashes},
	{ "quotemeta",       PH7_builtin_quotemeta  },
	{ "stripslashes",    PH7_builtin_stripslashes },
	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },
	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },
	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },
	{ "htmlentities",PH7_builtin_htmlentities},
	{ "html_entity_decode", PH7_builtin_html_entity_decode},
	{ "strlen"     , PH7_builtin_strlen     },
	{ "strcmp"     , PH7_builtin_strcmp     },
	{ "strcoll"    , PH7_builtin_strcmp     },
	{ "strnatcmp"  , PH7_builtin_strnatcmp  },
	{ "strnatcasecmp", PH7_builtin_strnatcmp },
	{ "strncmp"    , PH7_builtin_strncmp    },
	{ "strcasecmp" , PH7_builtin_strcasecmp },
	{ "strncasecmp", PH7_builtin_strncasecmp},
	{ "implode"    , PH7_builtin_implode    },
	{ "join"       , PH7_builtin_implode    },
	{ "implode_recursive" , PH7_builtin_implode_recursive },
	{ "join_recursive"    , PH7_builtin_implode_recursive },
	{ "explode"     , PH7_builtin_explode    },
	{ "trim"        , PH7_builtin_trim       },
	{ "rtrim"       , PH7_builtin_rtrim      },
	{ "chop"        , PH7_builtin_rtrim      },
	{ "ltrim"       , PH7_builtin_ltrim      },
	{ "strtolower",   PH7_builtin_strtolower },
	{ "mb_strtolower",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */
	{ "strtoupper",   PH7_builtin_strtoupper },
	{ "mb_strtoupper",PH7_builtin_mb_case_f }, /* UTF-8 only (builtin_mb.c) */
	{ "mb_strlen",    PH7_builtin_mb_strlen_f },
	{ "mb_substr",    PH7_builtin_mb_substr_f },
	{ "mb_convert_case", PH7_builtin_mb_convert_case_f },
	{ "mb_strpos",    PH7_builtin_mb_strpos_f },
	{ "mb_stripos",   PH7_builtin_mb_strpos_f },
	{ "mb_strrpos",   PH7_builtin_mb_strpos_f },
	{ "mb_str_split", PH7_builtin_mb_str_split_f },
	{ "mb_internal_encoding", PH7_builtin_mb_internal_encoding_f },
	{ "mb_check_encoding",    PH7_builtin_mb_check_encoding_f },
	{ "mb_strwidth",  PH7_builtin_mb_strwidth_f },
	{ "ucfirst",      PH7_builtin_ucfirst    },
	{ "lcfirst",      PH7_builtin_lcfirst    },
	{ "ord",          PH7_builtin_ord        },
	{ "chr",          PH7_builtin_chr        },
	{ "bin2hex",      PH7_builtin_bin2hex    },
	{ "strstr",       PH7_builtin_strstr     },
	{ "stristr",      PH7_builtin_stristr    },
	{ "strchr",       PH7_builtin_strstr     },
	{ "strpos",       PH7_builtin_strpos     },
	{ "stripos",      PH7_builtin_stripos    },
	{ "strrpos",      PH7_builtin_strrpos    },
	{ "strripos",     PH7_builtin_strripos   },
	{ "strrchr",      PH7_builtin_strrchr    },
	{ "strrev",       PH7_builtin_strrev     },
	{ "ucwords",      PH7_builtin_ucwords    },
	{ "str_repeat",   PH7_builtin_str_repeat },
	{ "str_contains", PH7_builtin_str_contains },
	{ "str_starts_with", PH7_builtin_str_starts_with },
	{ "str_ends_with", PH7_builtin_str_ends_with },
	{ "nl2br",        PH7_builtin_nl2br      },
#endif /* PH7_NEED_BUILTIN_REG */
#ifdef PH7_NEED_FMT_AND_INI
	{ "sprintf",      PH7_builtin_sprintf    },
	{ "printf",       PH7_builtin_printf     },
	{ "vprintf",      PH7_builtin_vprintf    },
	{ "vsprintf",     PH7_builtin_vsprintf   },
#endif /* PH7_NEED_FMT_AND_INI */
#ifdef PH7_NEED_BUILTIN_REG
	{ "size_format",  PH7_builtin_size_format},


#ifndef PH7_DISABLE_HASH_FUNC
	{ "md5",          PH7_builtin_md5       },
	{ "sha1",         PH7_builtin_sha1      },
	{ "crc32",        PH7_builtin_crc32     },
	{ "hash",         PH7_builtin_hash      },
	{ "hash_hmac",    PH7_builtin_hash_hmac },
	{ "hash_equals",  PH7_builtin_hash_equals },
	{ "hash_algos",   PH7_builtin_hash_algos },
#endif /* PH7_DISABLE_HASH_FUNC */
	{ "password_hash",         PH7_builtin_password_hash },
	{ "password_verify",       PH7_builtin_password_verify },
	{ "password_get_info",     PH7_builtin_password_get_info },
	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },
	{ "filter_var",            PH7_builtin_filter_var },
	{ "filter_input",          PH7_builtin_filter_input },
#endif /* PH7_NEED_BUILTIN_REG */
#ifdef PH7_NEED_FMT_AND_INI
	{ "str_getcsv",   PH7_builtin_str_getcsv },
	{ "strip_tags",   PH7_builtin_strip_tags },
#endif /* PH7_NEED_FMT_AND_INI */
#ifdef PH7_NEED_BUILTIN_REG

	{ "str_shuffle",  PH7_builtin_str_shuffle},
	{ "str_split",    PH7_builtin_str_split  },
	{ "strspn",       PH7_builtin_strspn     },
	{ "strcspn",      PH7_builtin_strcspn    },
	{ "strpbrk",      PH7_builtin_strpbrk    },
	{ "soundex",      PH7_builtin_soundex    },
	{ "wordwrap",     PH7_builtin_wordwrap   },
	{ "strtok",       PH7_builtin_strtok     },
	{ "str_pad",      PH7_builtin_str_pad    },
	{ "str_replace",  PH7_builtin_str_replace},
	{ "str_ireplace", PH7_builtin_str_replace},
	{ "strtr",        PH7_builtin_strtr      },
#endif /* PH7_NEED_BUILTIN_REG */
#ifdef PH7_NEED_FMT_AND_INI
	{ "parse_ini_string", PH7_builtin_parse_ini_string},
#endif /* PH7_NEED_FMT_AND_INI */
#ifdef PH7_NEED_BUILTIN_REG

	         /* Ctype functions */
	{ "ctype_alnum", PH7_builtin_ctype_alnum },
	{ "ctype_alpha", PH7_builtin_ctype_alpha },
	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },
	{ "ctype_digit", PH7_builtin_ctype_digit },
	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},
	{ "ctype_graph", PH7_builtin_ctype_graph },
	{ "ctype_print", PH7_builtin_ctype_print },
	{ "ctype_punct", PH7_builtin_ctype_punct },
	{ "ctype_space", PH7_builtin_ctype_space },
	{ "ctype_lower", PH7_builtin_ctype_lower },
	{ "ctype_upper", PH7_builtin_ctype_upper },
	         /* Time functions */
	{ "time"    ,    PH7_builtin_time         },
	{ "microtime",   PH7_builtin_microtime    },
	{ "getdate" ,    PH7_builtin_getdate      },
	{ "gettimeofday",PH7_builtin_gettimeofday },
	{ "date",        PH7_builtin_date         },
	{ "strftime",    PH7_builtin_strftime     },
	{ "idate",       PH7_builtin_idate        },
	{ "gmdate",      PH7_builtin_gmdate       },
	{ "localtime",   PH7_builtin_localtime    },
	{ "mktime",      PH7_builtin_mktime       },
	{ "gmmktime",    PH7_builtin_mktime       },
	{ "date_default_timezone_get", PH7_builtin_date_default_timezone_get },
	{ "date_default_timezone_set", PH7_builtin_date_default_timezone_set },
	        /* URL functions */
	{ "base64_encode",PH7_builtin_base64_encode },
	{ "base64_decode",PH7_builtin_base64_decode },
	{ "convert_uuencode",PH7_builtin_base64_encode },
	{ "convert_uudecode",PH7_builtin_base64_decode },
	{ "urlencode",    PH7_builtin_urlencode },
	{ "urldecode",    PH7_builtin_urldecode },
	{ "rawurlencode", PH7_builtin_urlencode },
	{ "rawurldecode", PH7_builtin_urldecode },
#endif /* PH7_NEED_BUILTIN_REG */
};
/*
 * Register the built-in functions defined above,the array functions
 * defined in hashmap.c and the IO functions defined in vfs.c.
 */
PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)
{
	sxu32 n;
	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){
		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);
	}
	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */
	PH7_RegisterHashmapFunctions(&(*pVm));
	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */
	PH7_RegisterIORoutine(&(*pVm));
}
