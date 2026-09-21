/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include <stdlib.h>  /* strtod */
#include <math.h>    /* HUGE_VAL */
#include <errno.h>   /* ERANGE (strtod range-error signal) */
#include <stdio.h>   /* snprintf (printf-family float conversions — correctly
                      * rounded shortest-representation output) */
/*
 * Section:
 *    String handling functions.
 * Status:
 *    Stable.
 */
#ifndef PH7_DISABLE_BUILTIN_FUNC
#define PH7_NEED_BUILTIN_REG 1
#endif
#ifndef PH7_DISABLE_DISK_IO
#define PH7_NEED_FMT_AND_INI 1
#endif
#ifdef PH7_NEED_BUILTIN_REG
/* Forward decl: null-to-string ZPP deprecation notice (defined near the ZPP
 * helpers; both live inside the same DISABLE_BUILTIN_FUNC region as every
 * caller — the tiny build compiles none of them). */
static void StrNullArgNotice(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNum,const char *zParamName);
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
PH7_PRIVATE int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
/* php only DEPRECATES passing null to a non-nullable string param; PHL targets php's
 * non-deprecated surface and rejects it with a TypeError. Every caller of this helper
 * now also carries a `string $…` row in the vm_arg_check.c signature table, so
 * VmEnforceBuiltinArgTypes raises that TypeError BEFORE the routine runs and this is a
 * backstop rather than the live path. It stays correct either way: the throw records
 * its status on the call context and the OP_CALL boundary (VmHostFuncThrowRc) reports
 * it in place of the routine's own, so the call ABORTS as php's would without the
 * callers threading a status back. */
static void StrNullArgNotice(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNum,const char *zParamName)
{
	if( ph7_value_is_null(pArg) ){
		PH7_VmThrowException(pCtx,"TypeError",
			"%s(): Argument #%d (%s) must be of type string, null given",
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
PH7_PRIVATE int PH7_builtin_substr_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_levenshtein(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_similar_text(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_str_word_count(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
	/* php only DEPRECATES null here; PHL rejects it. */
	if( ph7_value_is_null(apArg[0]) ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"addslashes(): Argument #1 ($string) must be of type string, null given"
			);
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
PH7_PRIVATE int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
	/* php only DEPRECATES null here; PHL rejects it. */
	if( ph7_value_is_null(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"addcslashes(): Argument #1 ($string) must be of type string, null given"
			);
	} else if( ph7_value_is_array(apArg[0]) ||
	          ph7_value_is_object(apArg[0]) ||
	          ph7_value_is_resource(apArg[0]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"addcslashes(): Argument #1 ($string) must be of type string, %s given",
			ph7_type_name(apArg[0])
			);
	}
	/* php only DEPRECATES null here; PHL rejects it. */
	if( ph7_value_is_null(apArg[1]) ){
		return PH7_VmThrowException(pCtx,
			"TypeError",
			"addcslashes(): Argument #2 ($characters) must be of type string, null given"
			);
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
PH7_PRIVATE int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
	/* Seed an empty string result: the loop below only ever APPENDS, so without
	 * this an empty input would leave the return value untouched and answer
	 * NULL where php answers "". */
	ph7_result_string(pCtx,"",0);
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
/*
 * string htmlspecialchars(string $string [, int $flags = ENT_QUOTES|ENT_SUBSTITUTE|ENT_HTML401
 *                         [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])
 *  Convert the special characters & < > " ' to HTML entities.
 * Return
 *  The escaped string or NULL on failure.
 */
PH7_PRIVATE int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
#endif /* PH7_DISABLE_BUILTIN_FUNC */
/*
 * The natural-order comparison core lives OUTSIDE the PH7_DISABLE_BUILTIN_FUNC
 * guard: hashmap.c's SORT_NATURAL path (always compiled) calls PH7_StrNatCmp, so
 * it must exist in the tiny build too. [[tiny-build-disk-io-guard-fragility]]
 */
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
PH7_PRIVATE int PH7_StrNatCmp(const char *zA,int nA,const char *zB,int nB,int bFold)
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
#ifndef PH7_DISABLE_BUILTIN_FUNC
/*
 * int strnatcmp(string $string1, string $string2)
 * int strnatcasecmp(string $string1, string $string2)
 *  Natural-order string comparison ("img2" < "img10"), case folded for the
 *  latter. php 8.2+ normalizes the result to -1/0/1.
 */
PH7_PRIVATE int PH7_builtin_strnatcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
	ph7_result_int(pCtx,PH7_StrNatCmp(z1,n1,z2,n2,bFold));
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
PH7_PRIVATE int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
	/* php's user-visible array->string warning: implode of an array element
	 * that is itself an array renders it as "Array" and warns (§2). */
	if( pValue->iFlags & MEMOBJ_HASHMAP ){
		PH7_VmThrowError(pData->pCtx->pVm,0,PH7_CTX_WARNING,"Array to string conversion");
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
PH7_PRIVATE int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
		if( nArg > 1 && !ph7_value_is_array(apArg[1]) && !ph7_value_is_null(apArg[1]) ){
			/* php: implode($glue, $pieces) requires an ARRAY. PH7 stringified whatever it
			 * was handed, so implode(",", 5) quietly returned "5". */
			char zBuf[64];
			return PH7_VmThrowException(pCtx,"TypeError",
				"implode(): Argument #2 ($array) must be of type ?array, %s given",
				VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));
		}
	}else{
		if( nArg > 1 ){
			/* php 8 removed the legacy swapped order: implode($pieces, $glue)
			 * is a TypeError (PHL used to swap silently, a wrong ANSWER when
			 * the caller meant php's signature). One array argument alone
			 * stays the legal ""-glue form. */
			return PH7_VmThrowException(pCtx,"TypeError",
				"implode(): Argument #1 ($separator) must be of type string, array given");
		}
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
PH7_PRIVATE int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
	/* php only DEPRECATES null here; PHL rejects it. */
	if( ph7_value_is_null(apArg[0]) ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"ord(): Argument #1 ($character) must be of type string, null given"
			);
	}
	/* Extract the target string */
	zString = ph7_value_to_string(apArg[0],&nLen);
	if( nLen < 1 ){
		/* php only DEPRECATES an empty string here; PHL rejects it. */
		return PH7_VmThrowException(pCtx,"ValueError",
			"ord(): Argument #1 ($character) must not be empty"
			);
	}
	/* A string longer than one byte: php DEPRECATES it; PHL rejects it. */
	if( nLen > 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"ord(): Argument #1 ($character) must be a single byte, use ord($str[0]) instead"
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
 *  by the given codepoint, which must be in the [0, 255] range.
 * Parameters
 *  $codepoint
 *   An integer codepoint in [0, 255]. php merely deprecates values
 *   outside that range (constraining them with % 256); PHL rejects
 *   them with a ValueError (scope policy).
 * Returns
 *  A single-character string.
 */
PH7_PRIVATE int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
		double d = ph7_value_to_double(apArg[0]);
		if( d != (double)(sxi64)d ){
			/* php only DEPRECATES a lossy float->int here; PHL rejects it. */
			return PH7_VmThrowException(pCtx,"TypeError",
				"chr(): Argument #1 ($codepoint) must be of type int, float given");
		}
	}
	/* Extract the codepoint. */
	c = ph7_value_to_int(apArg[0]);
	/* php only DEPRECATES an out-of-range codepoint (constraining it with % 256);
	 * PHL targets php's non-deprecated surface and rejects it loudly, matching the
	 * lossy-float branch above. This was the last engine site still emitting
	 * E_DEPRECATED — the scope policy says none remain. */
	if( c < 0 || c > 255 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"chr(): Argument #1 ($codepoint) must be between 0 and 255");
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
PH7_PRIVATE int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)
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
PH7_PRIVATE int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
	if( nPatLen < 1 ){
		/* php 8: the empty needle matches at position 0, so the whole haystack
		 * is returned (and nothing at all when $before_needle is set). */
		if( nArg > 2 && ph7_value_to_bool(apArg[2]) ){
			ph7_result_string(pCtx,"",0);
		}else{
			ph7_result_string(pCtx,zBlob,nLen);
		}
		return PH7_OK;
	}
	if( nLen > 0 ){
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
PH7_PRIVATE int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
	if( nPatLen < 1 ){
		/* php 8: the empty needle matches at position 0, so the whole haystack
		 * is returned (and nothing at all when $before_needle is set). */
		if( nArg > 2 && ph7_value_to_bool(apArg[2]) ){
			ph7_result_string(pCtx,"",0);
		}else{
			ph7_result_string(pCtx,zBlob,nLen);
		}
		return PH7_OK;
	}
	if( nLen > 0 ){
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
 * Resolve the $offset argument shared by strpos()/stripos().
 *
 * php requires -strlen($haystack) <= $offset <= strlen($haystack) and throws
 * ValueError otherwise; a negative offset counts back from the end. PHL used to
 * negate a negative offset and silently clamp an out-of-range one to zero, so
 * strpos("Hello","l",100) answered 2 where php raises — an argument error
 * turned into a wrong answer.
 *
 * On success *pnStart receives the resolved non-negative offset.
 */
static sxi32 StrSearchOffset(
	ph7_context *pCtx,
	ph7_value *pArg,
	int nLen,
	const char *zFunc,
	int *pnStart
	)
{
	ph7_int64 iOfft = ph7_value_to_int64(pArg);
	/* Compare without negating iOfft: -INT64_MIN would overflow. */
	if( iOfft < 0 ? (iOfft < -(ph7_int64)nLen) : (iOfft > (ph7_int64)nLen) ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #3 ($offset) must be contained in argument #1 ($haystack)",zFunc);
	}
	*pnStart = (int)(iOfft < 0 ? (ph7_int64)nLen + iOfft : iOfft);
	return PH7_OK;
}
/*
 * Resolve the window of match START positions for strrpos()/strripos().
 *
 * php's rule is asymmetric in the sign of $offset: a non-negative offset is a
 * LOWER bound on where the match may start, while a negative one is an UPPER
 * bound counted back from the end of the haystack (zend_memnrstr). The range
 * check is the same as StrSearchOffset()'s.
 *
 * On success the closed interval [*pnMin,*pnMax] holds every position at which
 * a match is allowed to begin; it is empty (max < min) when the needle cannot
 * fit, which the caller reports as FALSE.
 */
static sxi32 StrRSearchWindow(
	ph7_context *pCtx,
	ph7_value *pArg, /* The $offset argument, or NULL when it was omitted */
	int nLen,
	int nPatLen,
	const char *zFunc,
	int *pnMin,
	int *pnMax
	)
{
	int nMin = 0;
	int nMax = nLen - nPatLen;
	if( pArg ){
		ph7_int64 iOfft = ph7_value_to_int64(pArg);
		if( iOfft < 0 ? (iOfft < -(ph7_int64)nLen) : (iOfft > (ph7_int64)nLen) ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"%s(): Argument #3 ($offset) must be contained in argument #1 ($haystack)",zFunc);
		}
		if( iOfft < 0 ){
			int nLimit = nLen + (int)iOfft;
			if( nMax > nLimit ){
				nMax = nLimit;
			}
		}else{
			nMin = (int)iOfft;
		}
	}
	*pnMin = nMin;
	*pnMax = nMax;
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
PH7_PRIVATE int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
		rc = StrSearchOffset(pCtx,apArg[2],nLen,"strpos",&nStart);
		if( rc != PH7_OK ){
			return rc;
		}
	}
	if( nPatLen < 1 ){
		/* php 8 treats the empty needle as matching at the search offset. */
		ph7_result_int64(pCtx,(ph7_int64)nStart);
		return PH7_OK;
	}
	zBlob += nStart;
	nLen -= nStart;
	if( nLen > 0 ){
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
	SXUNUSED(zNullMsg); /* php's deprecation text — PHL rejects null instead of coercing */
	if( ph7_value_is_null(pArg) ){
		/* php only DEPRECATES null here; PHL rejects it with the TypeError php will
		 * eventually raise. */
		return PH7_VmThrowException(pCtx,"TypeError",
			"%s(): Argument #%d (%s) must be of type %s, null given",
			zFunc,iArgNum,zParamName,zTypeStr);
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
PH7_PRIVATE int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
		rc = StrSearchOffset(pCtx,apArg[2],nLen,"stripos",&nStart);
		if( rc != PH7_OK ){
			return rc;
		}
	}
	if( nPatLen < 1 ){
		/* php 8 treats the empty needle as matching at the search offset. */
		ph7_result_int64(pCtx,(ph7_int64)nStart);
		return PH7_OK;
	}
	zBlob += nStart;
	nLen -= nStart;
	if( nLen > 0 ){
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
PH7_PRIVATE int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zBlob,*zPattern;
	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */
	int nLen,nPatLen,i;
	int nMin = 0,nMax = 0;
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
	/* Resolve the range of positions the match may start at */
	rc = StrRSearchWindow(pCtx,nArg > 2 ? apArg[2] : 0,nLen,nPatLen,"strrpos",&nMin,&nMax);
	if( rc != PH7_OK ){
		return rc;
	}
	if( nPatLen < 1 ){
		/* php 8: the empty needle matches everywhere, so the LAST match is the
		 * highest position the window allows. */
		ph7_result_int64(pCtx,(ph7_int64)nMax);
		return PH7_OK;
	}
	/* Walk backwards, comparing at each candidate position. Searching a window
	 * exactly as long as the needle makes the match test an equality test while
	 * still going through xPatternMatch, which carries the case folding. */
	for( i = nMax ; i >= nMin ; --i ){
		rc = xPatternMatch((const void *)&zBlob[i],(sxu32)nPatLen,(const void *)zPattern,(sxu32)nPatLen,&nOfft);
		if( rc == SXRET_OK ){
			/* Pattern found,return it's position */
			ph7_result_int64(pCtx,(ph7_int64)i);
			return PH7_OK;
		}
	}
	/* Pattern not found,return FALSE */
	ph7_result_bool(pCtx,0);
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
PH7_PRIVATE int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zBlob,*zPattern;
	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */
	int nLen,nPatLen,i;
	int nMin = 0,nMax = 0;
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
	/* Resolve the range of positions the match may start at */
	rc = StrRSearchWindow(pCtx,nArg > 2 ? apArg[2] : 0,nLen,nPatLen,"strripos",&nMin,&nMax);
	if( rc != PH7_OK ){
		return rc;
	}
	if( nPatLen < 1 ){
		/* php 8: the empty needle matches everywhere, so the LAST match is the
		 * highest position the window allows. */
		ph7_result_int64(pCtx,(ph7_int64)nMax);
		return PH7_OK;
	}
	/* Walk backwards, comparing at each candidate position (see strrpos). */
	for( i = nMax ; i >= nMin ; --i ){
		rc = xPatternMatch((const void *)&zBlob[i],(sxu32)nPatLen,(const void *)zPattern,(sxu32)nPatLen,&nOfft);
		if( rc == SXRET_OK ){
			/* Pattern found,return it's position */
			ph7_result_int64(pCtx,(ph7_int64)i);
			return PH7_OK;
		}
	}
	/* Pattern not found,return FALSE */
	ph7_result_bool(pCtx,0);
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
PH7_PRIVATE int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
		const char *zPattern;
		int nPatLen;
		sxu32 nOfft;
		sxi32 rc;
		/* php 8 casts the needle to string and uses only its first character.
		 * The old "if not a string, take it as an ordinal" reading was php 7
		 * behaviour, removed in php 8: strrchr("hello world",111) now looks for
		 * "1", not "o". An empty needle matches nothing. */
		zPattern = ph7_value_to_string(apArg[1],&nPatLen);
		if( nPatLen < 1 ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		c = zPattern[0];
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
PH7_PRIVATE int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
#endif /* PH7_NEED_BUILTIN_REG */
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
PH7_PRIVATE int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
	/* Shuffle the string. Draw through the MT19937 generator so str_shuffle()
	 * responds to srand()/mt_srand() (reproducible under a seed), like php; the
	 * sampling differs from php's Fisher-Yates so it is not value-parity. */
	for( i = 0 ; i < nLen ; ++i ){
		/* Generate a random number first */
		iR = PH7_VmMtRand(pCtx->pVm);
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
PH7_PRIVATE int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
 * Shared body of strspn()/strcspn(): resolve php's ($offset,$length) window over
 * $string, then measure the span from the window's first byte.
 *
 * php's window rules (ext/standard/string.c, php_spn_common_handler) — a negative
 * $offset counts back from the end and CLAMPS to 0 (it is never "invalid"); an
 * $offset past the end clamps to the end, so the window is empty and the answer is
 * 0; a negative $length leaves that many bytes off the end of the remaining span
 * and clamps to 0; a zero-length window answers 0. PH7 answered 0 for a negative
 * offset that reached past the start, IGNORED a zero or negative $length entirely
 * (measuring the whole rest of the string instead), and truncated the offset to
 * `int`, so a 64-bit offset wrapped into a valid one.
 *
 * PH7 also ran the scan over the first WHITESPACE-DELIMITED TOKEN rather than over
 * the raw window (leading spaces skipped, scan stopped at the next space), so
 * strspn("a b c","abc ") answered 1 where php answers 5 and strspn("  abc","abc")
 * answered 3 where php answers 0 — silent wrong answers on ordinary input. php
 * scans raw bytes; so does this.
 *
 * An empty $mask needs no special case: the mask lookup fails for every byte, so
 * strspn stops at once (0) and strcspn runs to the end of the window (its length),
 * which is exactly what php answers.
 */
static int StrSpnCommonHandler(
	ph7_context *pCtx,    /* Call context */
	int nArg,             /* Argument count */
	ph7_value **apArg,    /* Arguments */
	int bComplement       /* TRUE for strcspn() */
	)
{
	const char *zFunc = bComplement ? "strcspn" : "strspn";
	const char *zString,*zMask;
	int iMasklen,iLen;
	sxi64 iStart,iSpan;
	if( nArg < 2 ){
		/* Arity is enforced at the call boundary; nothing sensible to return here. */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	/* Extract the target string and the mask */
	zString = ph7_value_to_string(apArg[0],&iLen);
	zMask = ph7_value_to_string(apArg[1],&iMasklen);
	if( iLen < 0 ){
		iLen = 0;
	}
	if( iMasklen < 0 ){
		iMasklen = 0;
	}
	iStart = 0;
	if( nArg > 2 ){
		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],zFunc,3,"$offset","int",&iStart);
		if( rcArg != PH7_OK ){
			return rcArg;
		}
		if( iStart < 0 ){
			/* Count back from the end, clamped to the start (guarded so an
			 * INT64_MIN offset cannot overflow the addition). */
			iStart = ( iStart < -(sxi64)iLen ) ? 0 : iStart + iLen;
		}else if( iStart > (sxi64)iLen ){
			iStart = iLen;
		}
	}
	iSpan = (sxi64)iLen - iStart;
	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){
		sxi64 iUserlen = 0;
		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[3],zFunc,4,"$length","?int",&iUserlen);
		if( rcArg != PH7_OK ){
			return rcArg;
		}
		if( iUserlen < 0 ){
			/* Leave |$length| bytes off the end of the remaining span (guarded
			 * against an INT64_MIN underflow the same way). */
			iSpan = ( iUserlen < -iSpan ) ? 0 : iSpan + iUserlen;
		}else if( iUserlen < iSpan ){
			iSpan = iUserlen;
		}
	}
	ph7_result_int(pCtx,bComplement
		? LongestStringMask2(&zString[iStart],(int)iSpan,zMask,iMasklen)
		: LongestStringMask(&zString[iStart],(int)iSpan,zMask,iMasklen));
	return PH7_OK;
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
PH7_PRIVATE int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return StrSpnCommonHandler(pCtx,nArg,apArg,0);
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
PH7_PRIVATE int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	return StrSpnCommonHandler(pCtx,nArg,apArg,1);
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
PH7_PRIVATE int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
	if( iListLen < 1 ){
		/* An empty set can never match, so php rejects it rather than answering
		 * a FALSE indistinguishable from "not found" (checked BEFORE the haystack,
		 * so strpbrk("","") throws too). */
		return PH7_VmThrowException(pCtx,"ValueError",
			"strpbrk(): Argument #2 ($characters) must be a non-empty string");
	}
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
PH7_PRIVATE int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
PH7_PRIVATE int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)
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
			/* Padd type. php 8: anything outside LEFT(0)/RIGHT(1)/BOTH(2) is a
			 * catchable ValueError (PHL used to fall back to RIGHT silently);
			 * like the empty-pad check above, php only reaches it once padding
			 * is actually required (probed: str_pad("abc",2," ",9) is "abc"). */
			iType = ph7_value_to_int(apArg[3]);
			if( iType < 0 || iType > 2 ){
				return PH7_VmThrowException(pCtx,"ValueError",
					"str_pad(): Argument #4 ($pad_type) must be STR_PAD_LEFT, STR_PAD_RIGHT, or STR_PAD_BOTH");
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
 * Run the collected search/replace pairs over a single subject string, writing
 * the transformed bytes into pOut (reset here). Shared by the scalar-subject and
 * the array-subject (element-wise) paths. The search/replace SySets are walked
 * fresh on every call — cursors are reset here — so each array element is
 * transformed independently, exactly like php. Returns SXRET_OK, or SXERR_MEM
 * on an allocation failure inside StringReplace.
 */
static sxi32 StrReplaceOneSubject(
	SyBlob *pOut,             /* Output buffer (reset then filled here) */
	const char *zSubject,     /* Subject bytes */
	sxu32 nSubject,           /* Subject length */
	SySet *pSearch,           /* Collected search terms */
	SySet *pReplace,          /* Collected replacement terms */
	int rep_str,              /* TRUE: a single replacement reused for every search */
	ProcStringMatch xMatch    /* SyBlobSearch (str_replace) / iPatternMatch (str_ireplace) */
	)
{
	SyString *pSearch_,*pReplace_,sEmpty;
	sxi32 rc;
	SyBlobReset(pOut);
	if( nSubject > 0 ){
		rc = SyBlobAppend(pOut,(const void *)zSubject,nSubject);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	SyStringInitFromBuf(&sEmpty,"",0);
	SySetResetCursor(pSearch);
	SySetResetCursor(pReplace);
	pSearch_ = pReplace_ = 0; /* cc warning */
	while( SXRET_OK == SySetGetNextEntry(pSearch,(void **)&pSearch_) ){
		sxu32 nCount,nOfft;
		if( rep_str ){
			/* Single replacement string reused for every search term */
			pReplace_ = (SyString *)SySetPeek(pReplace);
		}else if( SXRET_OK != SySetGetNextEntry(pReplace,(void **)&pReplace_) ){
			/* 'replace set' has fewer values than the search set: an empty
			 * string is used for the rest of the replacement values. */
			pReplace_ = 0;
		}
		if( pReplace_ == 0 ){
			pReplace_ = &sEmpty;
		}
		if( pSearch_->nByte < 1 ){
			/* php ignores an empty search string, but it still CONSUMED a replace
			 * slot above so the remaining pairs stay aligned. */
			continue;
		}
		nOfft = nCount = 0;
		for(;;){
			if( nCount >= SyBlobLength(pOut) ){
				break;
			}
			/* Perform a pattern lookup */
			rc = xMatch(SyBlobDataAt(pOut,nCount),SyBlobLength(pOut) - nCount,
				(const void *)pSearch_->zString,pSearch_->nByte,&nOfft);
			if( rc != SXRET_OK ){
				/* Pattern not found */
				break;
			}
			/* Perform the replace operation */
			rc = StringReplace(pOut,nCount+nOfft,(int)pSearch_->nByte,
				pReplace_->zString,(int)pReplace_->nByte);
			if( rc != SXRET_OK ){
				/* Propagate an allocation failure so the caller raises a fatal
				 * instead of returning a partially-replaced result. */
				return rc;
			}
			/* Increment offset counter */
			nCount += nOfft + pReplace_->nByte;
		}
	}
	return SXRET_OK;
}
/* Per-call state for the array-subject form of str_replace()/str_ireplace(). */
typedef struct str_replace_subject str_replace_subject;
struct str_replace_subject
{
	ph7_value *pResult;    /* Result array (keys preserved) */
	ph7_value *pScratch;   /* Reusable string value for each element */
	SyBlob *pWorker;       /* Scratch output buffer for one element */
	SySet *pSearch;        /* Collected search terms */
	SySet *pReplace;       /* Collected replacement terms */
	ProcStringMatch xMatch;/* Match routine (case-sensitive or not) */
	int rep_str;           /* TRUE: scalar $replace */
	sxi32 rc;              /* SXRET_OK or SXERR_MEM */
};
/*
 * ph7_array_walk() callback over an array $subject: string-cast one element, run
 * the search/replace over it, and insert the result under the element's original
 * key. A non-string element is coerced exactly like php (int/float/bool/null via
 * their string form). A nested-array element becomes "Array" — the value matches
 * php, but PHL does not emit php's "Array to string conversion" warning here (the
 * engine raises it at echo/interpolation sites, not this C-level cast; a
 * recorded divergence).
 */
static int StrReplaceSubjectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)
{
	str_replace_subject *pS = (str_replace_subject *)pUserData;
	const char *zSub;
	int nSub;
	/* php coerces every element to string (same cast used everywhere). */
	zSub = ph7_value_to_string(pData,&nSub);
	if( StrReplaceOneSubject(pS->pWorker,zSub,(sxu32)(nSub > 0 ? nSub : 0),
			pS->pSearch,pS->pReplace,pS->rep_str,pS->xMatch) != SXRET_OK ){
		pS->rc = SXERR_MEM;
		return SXERR_ABORT;
	}
	/* Publish the transformed bytes as a string under the original key. */
	ph7_value_reset_string_cursor(pS->pScratch);
	if( SyBlobLength(pS->pWorker) > 0
	 && ph7_value_string(pS->pScratch,(const char *)SyBlobData(pS->pWorker),
			(int)SyBlobLength(pS->pWorker)) != SXRET_OK ){
		pS->rc = SXERR_MEM;
		return SXERR_ABORT;
	}
	if( ph7_array_add_elem(pS->pResult,pKey,pS->pScratch) != SXRET_OK ){
		pS->rc = SXERR_MEM;
		return SXERR_ABORT;
	}
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
PH7_PRIVATE int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	SyString sTemp;
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
	/* Collect the search term(s) — independent of the subject. */
	if( ph7_value_is_array(apArg[0]) ){
		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);
	}else{
		zIn = ph7_value_to_string(apArg[0],&nByte);
		SyStringInitFromBuf(&sTemp,zIn,nByte > 0 ? nByte : 0);
		SySetPut(&sSearch,(const void *)&sTemp);
	}
	/* Collect the replacement term(s). */
	if( ph7_value_is_array(apArg[1]) ){
		sRep.pCollector = &sReplace;
		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);
	}else{
		zIn = ph7_value_to_string(apArg[1],&nByte);
		rep_str = 1;
		SyStringInitFromBuf(&sTemp,zIn,nByte > 0 ? nByte : 0);
		SySetPut(&sReplace,(const void *)&sTemp);
	}
	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */
	if( sRep.rc != SXRET_OK ){
		SySetRelease(&sSearch);
		SySetRelease(&sReplace);
		SyBlobRelease(&sWorker);
		return PH7_ContextMemoryError(pCtx);
	}
	/* Pick the match routine by function name */
	zFunc = ph7_function_name(pCtx);
	xMatch = SyBlobSearch;
	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){
		/* Case insensitive pattern match */
		xMatch = iPatternMatch;
	}
	if( ph7_value_is_array(apArg[2]) ){
		/* Array subject: replace element-wise and RETURN AN ARRAY whose keys
		 * mirror the subject's (php semantics). */
		str_replace_subject sSub;
		ph7_value *pResult,*pScratch;
		pResult = ph7_context_new_array(pCtx);
		pScratch = ph7_context_new_scalar(pCtx);
		if( pResult == 0 || pScratch == 0 ){
			SySetRelease(&sSearch);
			SySetRelease(&sReplace);
			SyBlobRelease(&sWorker);
			return PH7_ContextMemoryError(pCtx);
		}
		ph7_value_string(pScratch,"",0); /* force string representation */
		SyZero(&sSub,sizeof(sSub));
		sSub.pResult  = pResult;
		sSub.pScratch = pScratch;
		sSub.pWorker  = &sWorker;
		sSub.pSearch  = &sSearch;
		sSub.pReplace = &sReplace;
		sSub.xMatch   = xMatch;
		sSub.rep_str  = rep_str;
		ph7_array_walk(apArg[2],StrReplaceSubjectWalker,&sSub);
		SySetRelease(&sSearch);
		SySetRelease(&sReplace);
		SyBlobRelease(&sWorker);
		if( sSub.rc != SXRET_OK ){
			return PH7_ContextMemoryError(pCtx);
		}
		ph7_result_value(pCtx,pResult);
		return PH7_OK;
	}
	/* Scalar subject: run once and return a string. An empty subject yields the
	 * empty string, and a lone empty search term leaves the subject untouched —
	 * both fall out of StrReplaceOneSubject's empty-term skip. */
	zIn = ph7_value_to_string(apArg[2],&nByte);
	rc = StrReplaceOneSubject(&sWorker,zIn,(sxu32)(nByte > 0 ? nByte : 0),
		&sSearch,&sReplace,rep_str,xMatch);
	if( rc != SXRET_OK ){
		SySetRelease(&sSearch);
		SySetRelease(&sReplace);
		SyBlobRelease(&sWorker);
		return PH7_ContextMemoryError(pCtx);
	}
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
	ph7_context *pCtx; /* Needed to warn about an empty key */
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
		/* PHP ignores an empty-string key, and warns that it did so. */
		ph7_context_throw_error_format(pCol->pCtx,PH7_CTX_WARNING,
			"Ignoring replacement of empty string");
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
PH7_PRIVATE int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	const char *zIn;
	char zGiven[64];
	int nLen;
	if( nArg < 1 ){
		/* Nothing to replace,return FALSE */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/*
	 * php dispatches strtr() on ARITY between two overloads — strtr(string, array)
	 * and strtr(string, string, string) — so $from's expected type is `array` with
	 * two arguments and `string` with three, and the stub's `array|string` union is
	 * a wording php itself never emits. One signature cannot express that, so the
	 * shared ZPP screen skips this builtin (azSelfChecked[] in vm_arg_check.c) and
	 * the dispatch happens here, in php's left-to-right argument order.
	 *
	 * Both directions used to pass silently: a 2-argument string $from
	 * (strtr("abc","ab")) returned the subject UNCHANGED, and a 3-argument array
	 * $from was likewise ignored — the caller got its input back as if it had been
	 * translated.
	 */
	if( !PH7_ArgSatisfiesString(apArg[0]) ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"strtr(): Argument #1 ($string) must be of type string, %s given",
			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));
	}
	if( nArg == 2 ){
		if( !ph7_value_is_array(apArg[1]) ){
			return PH7_VmThrowException(pCtx,"TypeError",
				"strtr(): Argument #2 ($from) must be of type array, %s given",
				VmValueGivenName(apArg[1],zGiven,sizeof(zGiven)));
		}
	}else if( nArg > 2 ){
		if( !PH7_ArgSatisfiesString(apArg[1]) ){
			return PH7_VmThrowException(pCtx,"TypeError",
				"strtr(): Argument #2 ($from) must be of type string, %s given",
				VmValueGivenName(apArg[1],zGiven,sizeof(zGiven)));
		}
		/* $to is php's `string`, but a null one stays accepted (php coerces it to
		 * "" with a deprecation, and both engines answer the subject unchanged). */
		if( !ph7_value_is_null(apArg[2]) && !PH7_ArgSatisfiesString(apArg[2]) ){
			return PH7_VmThrowException(pCtx,"TypeError",
				"strtr(): Argument #3 ($to) must be of type string, %s given",
				VmValueGivenName(apArg[2],zGiven,sizeof(zGiven)));
		}
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
		sCol.pCtx   = pCtx;
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
