# src/ph7/builtin_string.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2293/2692 lines (85.18%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `#include <stdlib.h>  /* strtod */` |
|      - |    8 | `#include <math.h>    /* HUGE_VAL */` |
|      - |    9 | `#include <errno.h>   /* ERANGE (strtod range-error signal) */` |
|      - |   10 | `#include <stdio.h>   /* snprintf (printf-family float conversions — correctly` |
|      - |   11 | `                      * rounded shortest-representation output) */` |
|      - |   12 | `/*` |
|      - |   13 | ` * Section:` |
|      - |   14 | ` *    String handling functions.` |
|      - |   15 | ` * Status:` |
|      - |   16 | ` *    Stable.` |
|      - |   17 | ` */` |
|      - |   18 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |   19 | `#define PH7_NEED_BUILTIN_REG 1` |
|      - |   20 | `#endif` |
|      - |   21 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |   22 | `#define PH7_NEED_FMT_AND_INI 1` |
|      - |   23 | `#endif` |
|      - |   24 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - |   25 | `/* Forward decl: null-to-string ZPP deprecation notice (defined near the ZPP` |
|      - |   26 | ` * helpers; both live inside the same DISABLE_BUILTIN_FUNC region as every` |
|      - |   27 | ` * caller — the tiny build compiles none of them). */` |
|      - |   28 | `static void StrNullArgNotice(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNum,const char *zParamName);` |
|      - |   29 | `/*` |
|      - |   30 | ` * Section:` |
|      - |   31 | ` *    String handling Functions.` |
|      - |   32 | ` * Status:` |
|      - |   33 | ` *    Stable.` |
|      - |   34 | ` */` |
|      - |   35 | `/*` |
|      - |   36 | ` * string substr(string $string,int $start[, int $length ])` |
|      - |   37 | ` *  Return part of a string.` |
|      - |   38 | ` * Parameters` |
|      - |   39 | ` *  $string` |
|      - |   40 | ` *   The input string. Must be one character or longer.` |
|      - |   41 | ` * $start` |
|      - |   42 | ` *   If start is non-negative, the returned string will start at the start'th position` |
|      - |   43 | ` *   in string, counting from zero. For instance, in the string 'abcdef', the character` |
|      - |   44 | ` *   at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - |   45 | ` *   If start is negative, the returned string will start at the start'th character` |
|      - |   46 | ` *   from the end of string.` |
|      - |   47 | ` *   If string is less than or equal to start characters long, FALSE will be returned.` |
|      - |   48 | ` * $length` |
|      - |   49 | ` *   If length is given and is positive, the string returned will contain at most length` |
|      - |   50 | ` *   characters beginning from start (depending on the length of string).` |
|      - |   51 | ` *   If length is given and is negative, then that many characters will be omitted from` |
|      - |   52 | ` *   the end of string (after the start position has been calculated when a start is negative).` |
|      - |   53 | ` *   If start denotes the position of this truncation or beyond, false will be returned.` |
|      - |   54 | ` *   If length is given and is 0, FALSE or NULL an empty string will be returned.` |
|      - |   55 | ` *   If length is omitted, the substring starting from start until the end of the string` |
|      - |   56 | ` *   will be returned.` |
|      - |   57 | ` * Return` |
|      - |   58 | ` *  Returns the extracted part of string, or FALSE on failure or an empty string.` |
|      - |   59 | ` */` |
| 278052 |   60 | `PH7_PRIVATE int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |   61 | `{` |
|      - |   62 | `	const char *zSource;` |
|      - |   63 | `	int nSrcLen;` |
|      - |   64 | `	sxi64 iStart,iEnd;` |
| 278057 |   65 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"substr",1,"$string"); }` |
| 278057 |   66 | `	if( nArg < 2 ){` |
|      - |   67 | `		/* Arity is enforced at the call boundary; nothing sensible to return here. */` |
|    ! 0 |   68 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 |   69 | `		return PH7_OK;` |
|      - |   70 | `	}` |
|      - |   71 | `	/* Extract the target string */` |
| 278057 |   72 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|      - |   73 | `	/* Extract the offset */` |
|      - |   74 | `	{` |
| 278057 |   75 | `		sxi64 iTmp = 0;` |
| 278057 |   76 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"substr",2,"$offset","int",&iTmp);` |
| 278057 |   77 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |   78 | `			return rcArg;` |
|      - |   79 | `		}` |
| 278057 |   80 | `		iStart = iTmp;` |
|      - |   81 | `	}` |
|      - |   82 | `	/*` |
|      - |   83 | `	 * php 8 never answers substr() with FALSE — every out-of-range window simply` |
|      - |   84 | `	 * clamps to the empty string (substr("",0), substr("abc",5) and` |
|      - |   85 | `	 * substr("abc",1,-5) are all ""). PH7 returned FALSE for each of those, which` |
|      - |   86 | `	 * then flowed on as a bool into string context.` |
|      - |   87 | `	 *` |
|      - |   88 | `	 * A negative offset counts back from the end (clamped to 0); a negative length` |
|      - |   89 | `	 * leaves that many bytes off the end. Computed in sxi64 so an INT64 offset or` |
|      - |   90 | `	 * length cannot overflow the window arithmetic.` |
|      - |   91 | `	 */` |
| 278057 |   92 | `	if( iStart < 0 ){` |
|  32979 |   93 | `		iStart += nSrcLen;` |
|  32979 |   94 | `		if( iStart < 0 ){` |
|      5 |   95 | `			iStart = 0;` |
|      7 |   96 | `		}` |
| 261570 |   97 | `	}else if( iStart > nSrcLen ){` |
|      7 |   98 | `		iStart = nSrcLen;` |
|      3 |   99 | `	}` |
| 278057 |  100 | `	iEnd = nSrcLen;` |
| 278057 |  101 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
| 197211 |  102 | `		sxi64 iLen = 0;` |
| 197211 |  103 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],"substr",3,"$length","?int",&iLen);` |
| 197211 |  104 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  105 | `			return rcArg;` |
|      - |  106 | `		}` |
| 197211 |  107 | `		if( iLen < 0 ){` |
|  32611 |  108 | `			iEnd = (sxi64)nSrcLen + iLen;` |
| 180908 |  109 | `		}else if( iLen > (sxi64)nSrcLen - iStart ){` |
|  18343 |  110 | `			iEnd = nSrcLen;` |
|   9174 |  111 | `		}else{` |
| 146267 |  112 | `			iEnd = iStart + iLen;` |
|      - |  113 | `		}` |
|  98603 |  114 | `	}` |
| 278057 |  115 | `	if( iEnd < iStart ){` |
|      3 |  116 | `		iEnd = iStart;` |
|      1 |  117 | `	}` |
| 278057 |  118 | `	ph7_result_string(pCtx,&zSource[iStart],(int)(iEnd - iStart));` |
| 278057 |  119 | `	return PH7_OK;` |
| 139133 |  120 | `}` |
|      - |  121 | `/*` |
|      - |  122 | ` * int substr_compare(string $main_str,string $str ,int $offset[,int $length[,bool $case_insensitivity = false ]])` |
|      - |  123 | ` *  Binary safe comparison of two strings from an offset, up to length characters.` |
|      - |  124 | ` * Parameters` |
|      - |  125 | ` *  $main_str` |
|      - |  126 | ` *  The main string being compared.` |
|      - |  127 | ` *  $str` |
|      - |  128 | ` *   The secondary string being compared.` |
|      - |  129 | ` * $offset` |
|      - |  130 | ` *  The start position for the comparison. If negative, it starts counting from` |
|      - |  131 | ` *  the end of the string.` |
|      - |  132 | ` * $length` |
|      - |  133 | ` *  The length of the comparison. The default value is the largest of the length` |
|      - |  134 | ` *  of the str compared to the length of main_str less the offset.` |
|      - |  135 | ` * $case_insensitivity` |
|      - |  136 | ` *  If case_insensitivity is TRUE, comparison is case insensitive.` |
|      - |  137 | ` * Return` |
|      - |  138 | ` *  Returns < 0 if main_str from position offset is less than str, > 0 if it is greater than` |
|      - |  139 | ` *  str, and 0 if they are equal. If offset is equal to or greater than the length of main_str` |
|      - |  140 | ` *  or length is set and is less than 1, substr_compare() prints a warning and returns FALSE.` |
|      - |  141 | ` */` |
|     20 |  142 | `PH7_PRIVATE int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  143 | `{` |
|      - |  144 | `	const char *zSource,*zSub;` |
|      - |  145 | `	int nSrcLen,nSubLen;` |
|      - |  146 | `	sxi64 iOfft,iLen,l1,l2,nCmp;` |
|     21 |  147 | `	int iCase = 0;` |
|      - |  148 | `	int rc;` |
|     21 |  149 | `	if( nArg < 3 ){` |
|    ! 0 |  150 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  151 | `		return PH7_OK;` |
|      - |  152 | `	}` |
|     21 |  153 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|     21 |  154 | `	zSub    = ph7_value_to_string(apArg[1],&nSubLen);` |
|      - |  155 | `	{` |
|     21 |  156 | `		sxi64 iTmp = 0;` |
|     21 |  157 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],"substr_compare",3,"$offset","int",&iTmp);` |
|     21 |  158 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  159 | `			return rcArg;` |
|      - |  160 | `		}` |
|     21 |  161 | `		iOfft = iTmp;` |
|      - |  162 | `	}` |
|     21 |  163 | `	if( iOfft < 0 ){` |
|      5 |  164 | `		iOfft += nSrcLen;` |
|      5 |  165 | `		if( iOfft < 0 ){` |
|      3 |  166 | `			iOfft = 0;` |
|      1 |  167 | `		}` |
|      2 |  168 | `	}` |
|     21 |  169 | `	if( iOfft > nSrcLen ){` |
|      - |  170 | `		/* php rejects an offset past the end of the haystack outright */` |
|    ! 0 |  171 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  172 | `			"substr_compare(): Argument #3 ($offset) must be contained in argument #1 ($haystack)");` |
|      - |  173 | `	}` |
|      - |  174 | `	/* A NULL/absent length compares as far as the longer of the two operands reaches */` |
|     21 |  175 | `	iLen = (sxi64)nSrcLen - iOfft;` |
|     21 |  176 | `	if( iLen < nSubLen ){` |
|      5 |  177 | `		iLen = nSubLen;` |
|      2 |  178 | `	}` |
|     21 |  179 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|     11 |  180 | `		sxi64 iTmp = 0;` |
|     11 |  181 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[3],"substr_compare",4,"$length","?int",&iTmp);` |
|     11 |  182 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  183 | `			return rcArg;` |
|      - |  184 | `		}` |
|     11 |  185 | `		if( iTmp < 0 ){` |
|      3 |  186 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  187 | `				"substr_compare(): Argument #4 ($length) must be greater than or equal to 0");` |
|      - |  188 | `		}` |
|      9 |  189 | `		iLen = iTmp;` |
|      4 |  190 | `	}` |
|     19 |  191 | `	if( nArg > 4 ){` |
|      5 |  192 | `		iCase = ph7_value_to_bool(apArg[4]);` |
|      2 |  193 | `	}` |
|      - |  194 | `	/* Each side contributes at most what it actually has left */` |
|     19 |  195 | `	l1 = (sxi64)nSrcLen - iOfft;` |
|     19 |  196 | `	if( l1 > iLen ){ l1 = iLen; }` |
|     19 |  197 | `	l2 = nSubLen;` |
|     19 |  198 | `	if( l2 > iLen ){ l2 = iLen; }` |
|     19 |  199 | `	nCmp = (l1 < l2) ? l1 : l2;` |
|     19 |  200 | `	if( iCase ){` |
|      3 |  201 | `		rc = SyStrnicmp(&zSource[iOfft],zSub,(sxu32)nCmp);` |
|      2 |  202 | `	}else{` |
|     17 |  203 | `		rc = SyStrncmp(&zSource[iOfft],zSub,(sxu32)nCmp);` |
|      - |  204 | `	}` |
|     19 |  205 | `	if( rc == 0 ){` |
|      - |  206 | `		/* Prefixes equal: php falls back to a THREE-WAY compare of the lengths, so this` |
|      - |  207 | `		 * arm is normalized to -1/0/1 (substr_compare("abc","",0) is 1, not 3). */` |
|      9 |  208 | `		rc = (l1 == l2) ? 0 : (l1 < l2 ? -1 : 1);` |
|      4 |  209 | `	}` |
|      - |  210 | `	/* ...but when the prefixes differ php returns the RAW byte difference, not its sign:` |
|      - |  211 | `	 * substr_compare("abc","def",1,10) is -2 ('b' - 'd'), which is what SyMemcmp gives. */` |
|     19 |  212 | `	ph7_result_int(pCtx,rc);` |
|     19 |  213 | `	return PH7_OK;` |
|     11 |  214 | `}` |
|      - |  215 | `/*` |
|      - |  216 | ` * int substr_count(string $haystack,string $needle[,int $offset = 0 [,int $length ]])` |
|      - |  217 | ` *  Count the number of substring occurrences.` |
|      - |  218 | ` * Parameters` |
|      - |  219 | ` * $haystack` |
|      - |  220 | ` *   The string to search in` |
|      - |  221 | ` * $needle` |
|      - |  222 | ` *   The substring to search for` |
|      - |  223 | ` * $offset` |
|      - |  224 | ` *  The offset where to start counting` |
|      - |  225 | ` * $length (NOT USED)` |
|      - |  226 | ` *  The maximum length after the specified offset to search for the substring.` |
|      - |  227 | ` *  It outputs a warning if the offset plus the length is greater than the haystack length.` |
|      - |  228 | ` * Return` |
|      - |  229 | ` *  Toral number of substring occurrences.` |
|      - |  230 | ` */` |
|     26 |  231 | `PH7_PRIVATE int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  232 | `{` |
|      - |  233 | `	const char *zText,*zPattern,*zEnd;` |
|      - |  234 | `	int nTextlen,nPatlen;` |
|     27 |  235 | `	int iCount = 0;` |
|      - |  236 | `	sxu32 nOfft;` |
|      - |  237 | `	sxi32 rc;` |
|     27 |  238 | `	if( nArg < 2 ){` |
|      - |  239 | `		/* Missing arguments */` |
|    ! 0 |  240 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  241 | `		return PH7_OK;` |
|      - |  242 | `	}` |
|      - |  243 | `	/* Point to the haystack */` |
|     27 |  244 | `	zText = ph7_value_to_string(apArg[0],&nTextlen);` |
|      - |  245 | `	/* Point to the neddle */` |
|     27 |  246 | `	zPattern = ph7_value_to_string(apArg[1],&nPatlen);` |
|     27 |  247 | `	if( nPatlen < 1 ){` |
|      - |  248 | `		/* Empty needle: PHP 8 throws a catchable ValueError. */` |
|      3 |  249 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  250 | `			"substr_count(): Argument #2 ($needle) must not be empty");` |
|      - |  251 | `	}` |
|      - |  252 | `	/* Apply the optional $offset/$length window before searching. PHP 8 validates` |
|      - |  253 | `	 * both against the haystack (a negative value counts from the end) and throws a` |
|      - |  254 | `	 * catchable ValueError when the result falls outside it — this happens before the` |
|      - |  255 | `	 * needle-fits check, so it fires even when the needle is longer than the haystack. */` |
|     25 |  256 | `	if( nArg > 2 ){` |
|     19 |  257 | `		ph7_int64 iOfft = ph7_value_to_int64(apArg[2]);` |
|     19 |  258 | `		if( iOfft < 0 ){` |
|      5 |  259 | `			iOfft += nTextlen;` |
|      2 |  260 | `		}` |
|     19 |  261 | `		if( iOfft < 0 \|\| iOfft > nTextlen ){` |
|      3 |  262 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  263 | `				"substr_count(): Argument #3 ($offset) must be contained in argument #1 ($haystack)");` |
|      - |  264 | `		}` |
|      - |  265 | `		/* Point to the desired offset and shrink the remaining region */` |
|     17 |  266 | `		zText = &zText[iOfft];` |
|     17 |  267 | `		nTextlen -= (int)iOfft;` |
|      8 |  268 | `	}` |
|     23 |  269 | `	if( nArg > 3 ){` |
|     15 |  270 | `		ph7_int64 nLen = ph7_value_to_int64(apArg[3]);` |
|     15 |  271 | `		if( nLen < 0 ){` |
|      - |  272 | `			/* Negative length is relative to the end of the (offset) haystack */` |
|      5 |  273 | `			nLen += nTextlen;` |
|      2 |  274 | `		}` |
|     15 |  275 | `		if( nLen < 0 \|\| nLen > nTextlen ){` |
|      5 |  276 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  277 | `				"substr_count(): Argument #4 ($length) must be contained in argument #1 ($haystack)");` |
|      - |  278 | `		}` |
|     11 |  279 | `		nTextlen = (int)nLen;` |
|      5 |  280 | `	}` |
|     19 |  281 | `	if( nTextlen < 1 \|\| nPatlen > nTextlen ){` |
|      - |  282 | `		/* The windowed haystack can't contain the needle: zero matches */` |
|      3 |  283 | `		ph7_result_int(pCtx,0);` |
|      3 |  284 | `		return PH7_OK;` |
|      - |  285 | `	}` |
|      - |  286 | `	/* Point to the end of the windowed haystack */` |
|     17 |  287 | `	zEnd = &zText[nTextlen];` |
|      - |  288 | `	/* Perform the search */` |
|     17 |  289 | `	for(;;){` |
|     35 |  290 | `		rc = SyBlobSearch((const void *)zText,(sxu32)(zEnd-zText),(const void *)zPattern,nPatlen,&nOfft);` |
|     35 |  291 | `		if( rc != SXRET_OK ){` |
|      - |  292 | `			/* Pattern not found,break immediately */` |
|     13 |  293 | `			break;` |
|      - |  294 | `		}` |
|      - |  295 | `		/* Increment counter and update the offset */` |
|     23 |  296 | `		iCount++;` |
|     23 |  297 | `		zText += nOfft + nPatlen;` |
|     23 |  298 | `		if( zText >= zEnd ){` |
|      5 |  299 | `			break;` |
|      - |  300 | `		}` |
|      1 |  301 | `	}` |
|      - |  302 | `	/* Pattern count */` |
|     17 |  303 | `	ph7_result_int(pCtx,iCount);` |
|     17 |  304 | `	return PH7_OK;` |
|     14 |  305 | `}` |
|      - |  306 | `/* Forward declarations: defined with the trim/addcslashes and str_contains` |
|      - |  307 | ` * families below. */` |
|      - |  308 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256]);` |
|      - |  309 | `/*` |
|      - |  310 | ` * php 8.1 null-to-non-nullable ZPP deprecation, notice-only form for the` |
|      - |  311 | ` * legacy string builtins that still coerce null to "" themselves: emit` |
|      - |  312 | ``  * `f(): Passing null to parameter #N ($name) of type string is deprecated` `` |
|      - |  313 | ` * when the arg is an actual null, leaving the resolution unchanged.` |
|      - |  314 | ` */` |
|      - |  315 | `/* php only DEPRECATES passing null to a non-nullable string param; PHL targets php's` |
|      - |  316 | ` * non-deprecated surface and rejects it with a TypeError. The throw parks a pending` |
|      - |  317 | ` * exception that supersedes the builtin's result when it returns, so the callers can` |
|      - |  318 | ` * keep calling this without threading a status back. */` |
| 421650 |  319 | `static void StrNullArgNotice(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNum,const char *zParamName)` |
|      5 |  320 | `{` |
| 421655 |  321 | `	if( ph7_value_is_null(pArg) ){` |
|    ! 0 |  322 | `		PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  323 | `			"%s(): Argument #%d (%s) must be of type string, null given",` |
|    ! 0 |  324 | `			zFunc,iArgNum,zParamName);` |
|    ! 0 |  325 | `	}` |
| 421655 |  326 | `}` |
|      - |  327 | `static sxi32 StrPredicateResolveArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,` |
|      - |  328 | `	int iArgNum,const char *zParamName,const char *zTypeStr,const char *zNullMsg,` |
|      - |  329 | `	ph7_value *pTmp,const char **pzOut,int *pnOut);` |
|      - |  330 | `/*` |
|      - |  331 | ` * Validate and resolve an int-typed builtin parameter with php-8 ZPP weak-mode` |
|      - |  332 | ` * semantics: ints and bools pass through; null emits the 8.1 deprecation and` |
|      - |  333 | ` * resolves to 0; floats and float-strings convert, with the implicit-conversion` |
|      - |  334 | ` * E_DEPRECATED when lossy and a TypeError when NAN/INF/out of int range;` |
|      - |  335 | ` * integral numeric strings convert exactly; everything else (arrays, resources,` |
|      - |  336 | ` * objects, non-numeric strings) is a TypeError naming zTypeStr (e.g. "int",` |
|      - |  337 | ` * "array\|int"). Returns PH7_OK with *pOut set, or the throw status.` |
|      - |  338 | ` */` |
|      - |  339 | `/*` |
|      - |  340 | ` * Normalize a substr_replace() offset/length pair against a string of nStrLen` |
|      - |  341 | ` * bytes, exactly like PHP: a negative offset counts from the end (clamped to 0),` |
|      - |  342 | ` * an offset past the end clamps to the end; a negative length leaves that many` |
|      - |  343 | ` * bytes off the end of the remaining region (clamped to 0), and the length is` |
|      - |  344 | ` * finally clamped to the remaining region. Written without f+l additions so an` |
|      - |  345 | ` * INT64_MAX length cannot overflow.` |
|      - |  346 | ` */` |
|     60 |  347 | `static void SubstrReplaceWindow(sxi64 *pF,sxi64 *pL,int nStrLen)` |
|      1 |  348 | `{` |
|     61 |  349 | `	sxi64 f = *pF,l = *pL;` |
|     61 |  350 | `	if( f < 0 ){` |
|      9 |  351 | `		f += nStrLen;` |
|      9 |  352 | `		if( f < 0 ){` |
|      5 |  353 | `			f = 0;` |
|      3 |  354 | `		}` |
|     57 |  355 | `	}else if( f > nStrLen ){` |
|      5 |  356 | `		f = nStrLen;` |
|      2 |  357 | `	}` |
|     61 |  358 | `	if( l < 0 ){` |
|      7 |  359 | `		l += nStrLen - f;` |
|      7 |  360 | `		if( l < 0 ){` |
|      5 |  361 | `			l = 0;` |
|      2 |  362 | `		}` |
|      3 |  363 | `	}` |
|     61 |  364 | `	if( l > nStrLen - f ){` |
|     25 |  365 | `		l = nStrLen - f;` |
|     12 |  366 | `	}` |
|     61 |  367 | `	*pF = f;` |
|     61 |  368 | `	*pL = l;` |
|     61 |  369 | `}` |
|      - |  370 | `/* A replacement string collected out of substr_replace()'s $replace array.` |
|      - |  371 | ` * The bytes live in a shared pool blob (walker values are transient), so the` |
|      - |  372 | ` * item stores pool offsets, mirroring the strtr_entry technique. */` |
|      - |  373 | `typedef struct substr_repl_item substr_repl_item;` |
|      - |  374 | `struct substr_repl_item` |
|      - |  375 | `{` |
|      - |  376 | `	sxu32 nOfft; /* Offset of the string inside the pool */` |
|      - |  377 | `	sxu32 nLen;  /* Length of the string */` |
|      - |  378 | `};` |
|      - |  379 | `typedef struct substr_replace_collect substr_replace_collect;` |
|      - |  380 | `struct substr_replace_collect` |
|      - |  381 | `{` |
|      - |  382 | `	SyBlob *pPool;  /* Byte pool for string items (string walker only) */` |
|      - |  383 | `	SySet *pSet;    /* substr_repl_item set (string) or sxi64 set (int) */` |
|      - |  384 | `	sxi32 rc;       /* SXRET_OK or SXERR_MEM on collector failure */` |
|      - |  385 | `};` |
|      - |  386 | `/* ph7_array_walk() callback: append one $replace element to the pool. */` |
|      6 |  387 | `static int SubstrReplaceStrWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  388 | `{` |
|      7 |  389 | `	substr_replace_collect *pCol = (substr_replace_collect *)pUserData;` |
|      - |  390 | `	substr_repl_item sItem;` |
|      - |  391 | `	const char *zStr;` |
|      - |  392 | `	int nLen;` |
|      3 |  393 | `	SXUNUSED(pKey);` |
|      7 |  394 | `	zStr = ph7_value_to_string(pData,&nLen);` |
|      7 |  395 | `	sItem.nOfft = SyBlobLength(pCol->pPool);` |
|      7 |  396 | `	sItem.nLen = (sxu32)nLen;` |
|      7 |  397 | `	if( nLen > 0 && SXRET_OK != SyBlobAppend(pCol->pPool,(const void *)zStr,(sxu32)nLen) ){` |
|    ! 0 |  398 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  399 | `		return SXERR_ABORT;` |
|      - |  400 | `	}` |
|      7 |  401 | `	if( SXRET_OK != SySetPut(pCol->pSet,(const void *)&sItem) ){` |
|    ! 0 |  402 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  403 | `		return SXERR_ABORT;` |
|      - |  404 | `	}` |
|      7 |  405 | `	return PH7_OK;` |
|      4 |  406 | `}` |
|      - |  407 | `/* ph7_array_walk() callback: collect one $offset/$length element as an int. */` |
|     12 |  408 | `static int SubstrReplaceIntWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  409 | `{` |
|     13 |  410 | `	substr_replace_collect *pCol = (substr_replace_collect *)pUserData;` |
|     13 |  411 | `	sxi64 iVal = ph7_value_to_int64(pData);` |
|      6 |  412 | `	SXUNUSED(pKey);` |
|     13 |  413 | `	if( SXRET_OK != SySetPut(pCol->pSet,(const void *)&iVal) ){` |
|    ! 0 |  414 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  415 | `		return SXERR_ABORT;` |
|      - |  416 | `	}` |
|     13 |  417 | `	return PH7_OK;` |
|      7 |  418 | `}` |
|      - |  419 | `/* Per-element state while walking substr_replace()'s array $string. */` |
|      - |  420 | `typedef struct substr_replace_ctx substr_replace_ctx;` |
|      - |  421 | `struct substr_replace_ctx` |
|      - |  422 | `{` |
|      - |  423 | `	ph7_value *pResult;   /* Result array (keys preserved) */` |
|      - |  424 | `	ph7_value *pScratch;  /* Reusable string value for each element */` |
|      - |  425 | `	SyBlob *pReplPool;    /* Pool behind aRepl items */` |
|      - |  426 | `	SySet *pRepl;         /* substr_repl_item set or NULL when $replace is scalar */` |
|      - |  427 | `	SySet *pFrom;         /* sxi64 set or NULL when $offset is scalar */` |
|      - |  428 | `	SySet *pLen;          /* sxi64 set or NULL when $length is scalar/absent */` |
|      - |  429 | `	sxu32 iReplCur;       /* Next-position cursors into the three sets */` |
|      - |  430 | `	sxu32 iFromCur;` |
|      - |  431 | `	sxu32 iLenCur;` |
|      - |  432 | `	const char *zRepl;    /* Scalar $replace */` |
|      - |  433 | `	int nRepl;` |
|      - |  434 | `	sxi64 iFrom;          /* Scalar $offset */` |
|      - |  435 | `	sxi64 iLen;           /* Scalar $length */` |
|      - |  436 | `	int bLenGiven;        /* FALSE: $length absent/null -> element length */` |
|      - |  437 | `	sxi32 rc;             /* SXRET_OK or SXERR_MEM */` |
|      - |  438 | `};` |
|      - |  439 | `/*` |
|      - |  440 | ` * ph7_array_walk() callback over the array $string: replace the window of one` |
|      - |  441 | ` * element and insert the result under the element's original key. Array-form` |
|      - |  442 | ` * $replace/$offset/$length are consumed positionally; when a set runs out PHP` |
|      - |  443 | ` * falls back to ""/0/element-length respectively.` |
|      - |  444 | ` */` |
|     24 |  445 | `static int SubstrReplaceElemWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  446 | `{` |
|     25 |  447 | `	substr_replace_ctx *pRep = (substr_replace_ctx *)pUserData;` |
|      - |  448 | `	const char *zStr,*zRepl;` |
|      - |  449 | `	sxi64 f,l;` |
|      - |  450 | `	int nLen,nRepl;` |
|     25 |  451 | `	zStr = ph7_value_to_string(pData,&nLen);` |
|      - |  452 | `	/* Positional $replace element ("" when exhausted) */` |
|     25 |  453 | `	if( pRep->pRepl ){` |
|     11 |  454 | `		if( pRep->iReplCur < SySetUsed(pRep->pRepl) ){` |
|      7 |  455 | `			substr_repl_item *pItem = (substr_repl_item *)SySetAt(pRep->pRepl,pRep->iReplCur++);` |
|      7 |  456 | `			zRepl = (const char *)SyBlobDataAt(pRep->pReplPool,pItem->nOfft);` |
|      7 |  457 | `			nRepl = (int)pItem->nLen;` |
|      4 |  458 | `		}else{` |
|      5 |  459 | `			zRepl = "";` |
|      5 |  460 | `			nRepl = 0;` |
|      - |  461 | `		}` |
|      6 |  462 | `	}else{` |
|     15 |  463 | `		zRepl = pRep->zRepl;` |
|     15 |  464 | `		nRepl = pRep->nRepl;` |
|      - |  465 | `	}` |
|      - |  466 | `	/* Positional $offset element (0 when exhausted) */` |
|     25 |  467 | `	if( pRep->pFrom ){` |
|     13 |  468 | `		sxi64 *pVal = 0;` |
|     13 |  469 | `		if( pRep->iFromCur < SySetUsed(pRep->pFrom) ){` |
|      9 |  470 | `			pVal = (sxi64 *)SySetAt(pRep->pFrom,pRep->iFromCur++);` |
|      4 |  471 | `		}` |
|     13 |  472 | `		f = pVal ? *pVal : 0;` |
|      7 |  473 | `	}else{` |
|     13 |  474 | `		f = pRep->iFrom;` |
|      - |  475 | `	}` |
|      - |  476 | `	/* Positional $length element (element length when exhausted) */` |
|     25 |  477 | `	if( pRep->pLen ){` |
|      7 |  478 | `		sxi64 *pVal = 0;` |
|      7 |  479 | `		if( pRep->iLenCur < SySetUsed(pRep->pLen) ){` |
|      5 |  480 | `			pVal = (sxi64 *)SySetAt(pRep->pLen,pRep->iLenCur++);` |
|      2 |  481 | `		}` |
|      7 |  482 | `		l = pVal ? *pVal : nLen;` |
|      4 |  483 | `	}else{` |
|     19 |  484 | `		l = pRep->bLenGiven ? pRep->iLen : nLen;` |
|      - |  485 | `	}` |
|     25 |  486 | `	SubstrReplaceWindow(&f,&l,nLen);` |
|      - |  487 | `	/* Assemble prefix + replacement + suffix in the scratch value */` |
|     25 |  488 | `	ph7_value_reset_string_cursor(pRep->pScratch);` |
|     24 |  489 | `	if( (f > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,zStr,(int)f))` |
|     24 |  490 | `	 \|\| (nRepl > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,zRepl,nRepl))` |
|     40 |  491 | `	 \|\| (nLen - (int)(f+l) > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,&zStr[f+l],nLen - (int)(f+l))) ){` |
|     30 |  492 | `		pRep->rc = SXERR_MEM;` |
|     30 |  493 | `		return SXERR_ABORT;` |
|      - |  494 | `	}` |
|     25 |  495 | `	if( SXRET_OK != ph7_array_add_elem(pRep->pResult,pKey,pRep->pScratch) ){` |
|    ! 0 |  496 | `		pRep->rc = SXERR_MEM;` |
|    ! 0 |  497 | `		return SXERR_ABORT;` |
|      - |  498 | `	}` |
|     25 |  499 | `	return PH7_OK;` |
|     43 |  500 | `}` |
|      - |  501 | `/*` |
|      - |  502 | ` * mixed substr_replace(array\|string $string,array\|string $replace,array\|int $offset[,array\|int\|null $length = null])` |
|      - |  503 | ` *  Replace text within a portion of a string.` |
|      - |  504 | ` * Parameters` |
|      - |  505 | ` *  $string` |
|      - |  506 | ` *   The input string or an array of strings (each element is processed with` |
|      - |  507 | ` *   its own positional replace/offset/length when those are arrays too).` |
|      - |  508 | ` *  $replace` |
|      - |  509 | ` *   The replacement string. When $string is scalar and $replace is an array,` |
|      - |  510 | ` *   only its first element is used (PHP quirk).` |
|      - |  511 | ` *  $offset` |
|      - |  512 | ` *   Window start; negative counts from the end of the string.` |
|      - |  513 | ` *  $length` |
|      - |  514 | ` *   Window length; negative leaves that many bytes at the end; null/absent` |
|      - |  515 | ` *   means "to the end of the string".` |
|      - |  516 | ` * Return` |
|      - |  517 | ` *  The processed string, or an array of processed strings (keys preserved).` |
|      - |  518 | ` * Errors` |
|      - |  519 | ` *  ArgumentCountError on fewer than 3 arguments; TypeError when an array` |
|      - |  520 | ` *  $offset/$length is combined with a scalar $string.` |
|      - |  521 | ` */` |
|     58 |  522 | `PH7_PRIVATE int PH7_builtin_substr_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  523 | `{` |
|      - |  524 | `	ph7_value sStrTmp,sReplTmp;` |
|     59 |  525 | `	const char *zStr = 0,*zRepl = 0;` |
|     59 |  526 | `	int nLen = 0,nRepl = 0;` |
|      - |  527 | `	int bLenGiven;` |
|     59 |  528 | `	sxi64 f = 0,l = 0;` |
|      - |  529 | `	sxi32 rc;` |
|     59 |  530 | `	if( nArg < 3 ){` |
|    ! 0 |  531 | `		return PH7_VmThrowException(pCtx,` |
|      - |  532 | `			"ArgumentCountError",` |
|      - |  533 | `			"substr_replace() expects at least 3 arguments, %d given",` |
|    ! 0 |  534 | `			nArg` |
|      - |  535 | `			);` |
|      - |  536 | `	}` |
|      - |  537 | `	/* $length counts as given unless absent or null (php: ?null semantics) */` |
|     59 |  538 | `	bLenGiven = (nArg > 3 && !ph7_value_is_null(apArg[3]));` |
|      - |  539 | `	/* php ZPP validates all four args, in order, before the body runs: the` |
|      - |  540 | `	 * non-array forms resolve here (null deprecation, __toString objects,` |
|      - |  541 | `	 * numeric strings), arrays pass through to the per-mode handling. */` |
|     59 |  542 | `	PH7_MemObjInit(pCtx->pVm,&sStrTmp);` |
|     59 |  543 | `	PH7_MemObjInit(pCtx->pVm,&sReplTmp);` |
|     59 |  544 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     45 |  545 | `		rc = StrPredicateResolveArg(pCtx,apArg[0],"substr_replace",1,"$string","array\|string",` |
|      - |  546 | `			"substr_replace(): Passing null to parameter #1 ($string) "` |
|      - |  547 | `			"of type array\|string is deprecated",` |
|      - |  548 | `			&sStrTmp,&zStr,&nLen);` |
|     45 |  549 | `		if( rc != PH7_OK ) goto out;` |
|     22 |  550 | `	}` |
|     59 |  551 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|     51 |  552 | `		rc = StrPredicateResolveArg(pCtx,apArg[1],"substr_replace",2,"$replace","array\|string",` |
|      - |  553 | `			"substr_replace(): Passing null to parameter #2 ($replace) "` |
|      - |  554 | `			"of type array\|string is deprecated",` |
|      - |  555 | `			&sReplTmp,&zRepl,&nRepl);` |
|     51 |  556 | `		if( rc != PH7_OK ) goto out;` |
|     25 |  557 | `	}` |
|     59 |  558 | `	if( !ph7_value_is_array(apArg[2]) ){` |
|     51 |  559 | `		rc = PH7_IntArgResolve(pCtx,apArg[2],"substr_replace",3,"$offset","array\|int",&f);` |
|     51 |  560 | `		if( rc != PH7_OK ) goto out;` |
|     24 |  561 | `	}` |
|     57 |  562 | `	if( bLenGiven && !ph7_value_is_array(apArg[3]) ){` |
|     31 |  563 | `		rc = PH7_IntArgResolve(pCtx,apArg[3],"substr_replace",4,"$length","array\|int\|null",&l);` |
|     31 |  564 | `		if( rc != PH7_OK ) goto out;` |
|     14 |  565 | `	}` |
|     55 |  566 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - |  567 | `		/* Array form: process each element, preserving keys */` |
|      - |  568 | `		substr_replace_ctx sRep;` |
|      - |  569 | `		substr_replace_collect sCol;` |
|      - |  570 | `		SyBlob sReplPool;` |
|      - |  571 | `		SySet sRepl,sFrom,sLen;` |
|      - |  572 | `		ph7_value *pResult,*pScratch;` |
|     15 |  573 | `		sxi32 rcWalk = SXRET_OK;` |
|     15 |  574 | `		SyBlobInit(&sReplPool,&pCtx->pVm->sAllocator);` |
|     15 |  575 | `		SySetInit(&sRepl,&pCtx->pVm->sAllocator,sizeof(substr_repl_item));` |
|     15 |  576 | `		SySetInit(&sFrom,&pCtx->pVm->sAllocator,sizeof(sxi64));` |
|     15 |  577 | `		SySetInit(&sLen,&pCtx->pVm->sAllocator,sizeof(sxi64));` |
|     15 |  578 | `		SyZero(&sRep,sizeof(substr_replace_ctx));` |
|     15 |  579 | `		sRep.bLenGiven = bLenGiven;` |
|     15 |  580 | `		sCol.rc = SXRET_OK;` |
|      - |  581 | `		/* Collect array-form $replace/$offset/$length positionally; the` |
|      - |  582 | `		 * scalar forms were already resolved above. */` |
|     15 |  583 | `		if( ph7_value_is_array(apArg[1]) ){` |
|      5 |  584 | `			sCol.pPool = &sReplPool;` |
|      5 |  585 | `			sCol.pSet = &sRepl;` |
|      5 |  586 | `			ph7_array_walk(apArg[1],SubstrReplaceStrWalker,&sCol);` |
|      5 |  587 | `			sRep.pRepl = &sRepl;` |
|      5 |  588 | `			sRep.pReplPool = &sReplPool;` |
|      3 |  589 | `		}else{` |
|     11 |  590 | `			sRep.zRepl = zRepl;` |
|     11 |  591 | `			sRep.nRepl = nRepl;` |
|      - |  592 | `		}` |
|     15 |  593 | `		if( sCol.rc == SXRET_OK && ph7_value_is_array(apArg[2]) ){` |
|      7 |  594 | `			sCol.pSet = &sFrom;` |
|      7 |  595 | `			ph7_array_walk(apArg[2],SubstrReplaceIntWalker,&sCol);` |
|      7 |  596 | `			sRep.pFrom = &sFrom;` |
|      4 |  597 | `		}else{` |
|      9 |  598 | `			sRep.iFrom = f;` |
|      - |  599 | `		}` |
|     15 |  600 | `		if( sCol.rc == SXRET_OK && bLenGiven ){` |
|      9 |  601 | `			if( ph7_value_is_array(apArg[3]) ){` |
|      5 |  602 | `				sCol.pSet = &sLen;` |
|      5 |  603 | `				ph7_array_walk(apArg[3],SubstrReplaceIntWalker,&sCol);` |
|      5 |  604 | `				sRep.pLen = &sLen;` |
|      3 |  605 | `			}else{` |
|      5 |  606 | `				sRep.iLen = l;` |
|      - |  607 | `			}` |
|      4 |  608 | `		}` |
|     15 |  609 | `		pResult = ph7_context_new_array(pCtx);` |
|     15 |  610 | `		pScratch = ph7_context_new_scalar(pCtx);` |
|     15 |  611 | `		if( sCol.rc != SXRET_OK \|\| pResult == 0 \|\| pScratch == 0 ){` |
|    ! 0 |  612 | `			rcWalk = SXERR_MEM;` |
|    ! 0 |  613 | `		}else{` |
|     15 |  614 | `			sRep.pResult = pResult;` |
|     15 |  615 | `			sRep.pScratch = pScratch;` |
|     15 |  616 | `			ph7_value_string(pScratch,"",0); /* Force string representation */` |
|     15 |  617 | `			ph7_array_walk(apArg[0],SubstrReplaceElemWalker,&sRep);` |
|     15 |  618 | `			rcWalk = sRep.rc;` |
|      - |  619 | `		}` |
|     15 |  620 | `		SyBlobRelease(&sReplPool);` |
|     15 |  621 | `		SySetRelease(&sRepl);` |
|     15 |  622 | `		SySetRelease(&sFrom);` |
|     15 |  623 | `		SySetRelease(&sLen);` |
|     15 |  624 | `		if( rcWalk != SXRET_OK ){` |
|    ! 0 |  625 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 |  626 | `			goto out;` |
|      - |  627 | `		}` |
|     15 |  628 | `		ph7_result_value(pCtx,pResult);` |
|     15 |  629 | `		rc = PH7_OK;` |
|     15 |  630 | `		goto out;` |
|      - |  631 | `	}` |
|      - |  632 | `	/* Scalar form: array $offset/$length are a TypeError, array $replace` |
|      - |  633 | `	 * degrades to its first element (php quirk). */` |
|     41 |  634 | `	if( ph7_value_is_array(apArg[2]) ){` |
|      3 |  635 | `		rc = PH7_VmThrowException(pCtx,` |
|      - |  636 | `			"TypeError",` |
|      - |  637 | `			"substr_replace(): Argument #3 ($offset) cannot be an array when working on a single string"` |
|      - |  638 | `			);` |
|      3 |  639 | `		goto out;` |
|      - |  640 | `	}` |
|     39 |  641 | `	if( bLenGiven && ph7_value_is_array(apArg[3]) ){` |
|      3 |  642 | `		rc = PH7_VmThrowException(pCtx,` |
|      - |  643 | `			"TypeError",` |
|      - |  644 | `			"substr_replace(): Argument #4 ($length) cannot be an array when working on a single string"` |
|      - |  645 | `			);` |
|      3 |  646 | `		goto out;` |
|      - |  647 | `	}` |
|     37 |  648 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - |  649 | `		/* First element of the replace array, or "" when empty */` |
|      5 |  650 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      5 |  651 | `		zRepl = "";` |
|      5 |  652 | `		nRepl = 0;` |
|      5 |  653 | `		if( pMap->pFirst ){` |
|      3 |  654 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pMap->pFirst->nValIdx);` |
|      3 |  655 | `			if( pVal ){` |
|      3 |  656 | `				zRepl = ph7_value_to_string(pVal,&nRepl);` |
|      1 |  657 | `			}` |
|      1 |  658 | `		}` |
|      2 |  659 | `	}` |
|     37 |  660 | `	if( !bLenGiven ){` |
|     15 |  661 | `		l = nLen;` |
|      7 |  662 | `	}` |
|     37 |  663 | `	SubstrReplaceWindow(&f,&l,nLen);` |
|      - |  664 | `	/* Assemble prefix + replacement + suffix straight into the call result` |
|      - |  665 | `	 * (ph7_result_string appends), no scratch buffer needed. */` |
|     37 |  666 | `	rc = SXRET_OK;` |
|     37 |  667 | `	if( f > 0 ){` |
|     29 |  668 | `		rc = ph7_result_string(pCtx,zStr,(int)f);` |
|     14 |  669 | `	}` |
|     37 |  670 | `	if( rc == SXRET_OK && nRepl > 0 ){` |
|     33 |  671 | `		rc = ph7_result_string(pCtx,zRepl,nRepl);` |
|     16 |  672 | `	}` |
|     37 |  673 | `	if( rc == SXRET_OK && nLen - (int)(f+l) > 0 ){` |
|     17 |  674 | `		rc = ph7_result_string(pCtx,&zStr[f+l],nLen - (int)(f+l));` |
|      8 |  675 | `	}` |
|     37 |  676 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  677 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 |  678 | `		goto out;` |
|      - |  679 | `	}` |
|      - |  680 | `	/* Force a string result even when all three segments are empty */` |
|     37 |  681 | `	rc = ph7_result_string(pCtx,"",0);` |
|     37 |  682 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  683 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 |  684 | `		goto out;` |
|      - |  685 | `	}` |
|     37 |  686 | `	rc = PH7_OK;` |
|     29 |  687 | `out:` |
|     59 |  688 | `	PH7_MemObjRelease(&sStrTmp);` |
|     59 |  689 | `	PH7_MemObjRelease(&sReplTmp);` |
|     59 |  690 | `	return rc;` |
|     30 |  691 | `}` |
|      - |  692 | `/*` |
|      - |  693 | ` * int levenshtein(string $string1,string $string2[,int $insertion_cost = 1[,int $replacement_cost = 1[,int $deletion_cost = 1]]])` |
|      - |  694 | ` *  Calculate the Levenshtein distance between two strings, byte per byte` |
|      - |  695 | ` *  (case-sensitive), with optional per-operation costs. Mirrors PHP's` |
|      - |  696 | ` *  reference_levdist(): two rolling rows over string2.` |
|      - |  697 | ` * Return` |
|      - |  698 | ` *  The minimal number of weighted edit operations turning $string1 into` |
|      - |  699 | ` *  $string2.` |
|      - |  700 | ` */` |
|     34 |  701 | `PH7_PRIVATE int PH7_builtin_levenshtein(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  702 | `{` |
|      - |  703 | `	static const char *azParam[] = { "$insertion_cost","$replacement_cost","$deletion_cost" };` |
|      - |  704 | `	const char *zStr1,*zStr2;` |
|     35 |  705 | `	sxi64 iCostIns = 1,iCostRep = 1,iCostDel = 1;` |
|      - |  706 | `	sxi64 *p1,*p2,*pTmp;` |
|      - |  707 | `	sxi64 c0,c1,c2;` |
|      - |  708 | `	ph7_value sTmp1,sTmp2;` |
|      - |  709 | `	int nLen1,nLen2;` |
|      - |  710 | `	int i1,i2;` |
|      - |  711 | `	sxi32 rc;` |
|      - |  712 | `	int i;` |
|     35 |  713 | `	if( nArg < 2 ){` |
|    ! 0 |  714 | `		return PH7_VmThrowException(pCtx,` |
|      - |  715 | `			"ArgumentCountError",` |
|      - |  716 | `			"levenshtein() expects at least 2 arguments, %d given",` |
|    ! 0 |  717 | `			nArg` |
|      - |  718 | `			);` |
|      - |  719 | `	}` |
|      - |  720 | `	/* $string1/$string2: null deprecates to "", __toString objects resolve,` |
|      - |  721 | `	 * everything non-stringish is a TypeError (php ZPP weak mode). */` |
|     35 |  722 | `	PH7_MemObjInit(pCtx->pVm,&sTmp1);` |
|     35 |  723 | `	PH7_MemObjInit(pCtx->pVm,&sTmp2);` |
|     35 |  724 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"levenshtein",1,"$string1","string",` |
|      - |  725 | `		"levenshtein(): Passing null to parameter #1 ($string1) "` |
|      - |  726 | `		"of type string is deprecated",` |
|      - |  727 | `		&sTmp1,&zStr1,&nLen1);` |
|     35 |  728 | `	if( rc != PH7_OK ) goto out;` |
|     35 |  729 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"levenshtein",2,"$string2","string",` |
|      - |  730 | `		"levenshtein(): Passing null to parameter #2 ($string2) "` |
|      - |  731 | `		"of type string is deprecated",` |
|      - |  732 | `		&sTmp2,&zStr2,&nLen2);` |
|     35 |  733 | `	if( rc != PH7_OK ) goto out;` |
|      - |  734 | `	/* Optional integer costs */` |
|     57 |  735 | `	for( i = 2 ; i < nArg && i < 5 ; i++ ){` |
|      - |  736 | `		sxi64 iVal;` |
|     31 |  737 | `		rc = PH7_IntArgResolve(pCtx,apArg[i],"levenshtein",i+1,azParam[i-2],"int",&iVal);` |
|     31 |  738 | `		if( rc != PH7_OK ) goto out;` |
|     23 |  739 | `		if( i == 2 ){` |
|     11 |  740 | `			iCostIns = iVal;` |
|     18 |  741 | `		}else if( i == 3 ){` |
|      7 |  742 | `			iCostRep = iVal;` |
|      4 |  743 | `		}else{` |
|      7 |  744 | `			iCostDel = iVal;` |
|      - |  745 | `		}` |
|     12 |  746 | `	}` |
|     27 |  747 | `	if( nLen1 == 0 ){` |
|      3 |  748 | `		ph7_result_int64(pCtx,(sxi64)nLen2 * iCostIns);` |
|      3 |  749 | `		rc = PH7_OK;` |
|      3 |  750 | `		goto out;` |
|      - |  751 | `	}` |
|     25 |  752 | `	if( nLen2 == 0 ){` |
|      3 |  753 | `		ph7_result_int64(pCtx,(sxi64)nLen1 * iCostDel);` |
|      3 |  754 | `		rc = PH7_OK;` |
|      3 |  755 | `		goto out;` |
|      - |  756 | `	}` |
|      - |  757 | `	/* Two rolling DP rows over string2 (auto-released on return). Reject a` |
|      - |  758 | `	 * string2 long enough to overflow the 32-bit allocation size. */` |
|     23 |  759 | `	if( (sxu32)nLen2 >= (SXU32_HIGH / sizeof(sxi64)) - 1 ){` |
|    ! 0 |  760 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 |  761 | `		goto out;` |
|      - |  762 | `	}` |
|     23 |  763 | `	p1 = (sxi64 *)ph7_context_alloc_chunk(pCtx,(unsigned int)(sizeof(sxi64) * (sxu32)(nLen2 + 1)),FALSE,TRUE);` |
|     23 |  764 | `	p2 = (sxi64 *)ph7_context_alloc_chunk(pCtx,(unsigned int)(sizeof(sxi64) * (sxu32)(nLen2 + 1)),FALSE,TRUE);` |
|     23 |  765 | `	if( p1 == 0 \|\| p2 == 0 ){` |
|    ! 0 |  766 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 |  767 | `		goto out;` |
|      - |  768 | `	}` |
|    733 |  769 | `	for( i2 = 0 ; i2 <= nLen2 ; i2++ ){` |
|    711 |  770 | `		p1[i2] = (sxi64)i2 * iCostIns;` |
|    356 |  771 | `	}` |
|    707 |  772 | `	for( i1 = 0 ; i1 < nLen1 ; i1++ ){` |
|    685 |  773 | `		p2[0] = p1[0] + iCostDel;` |
| 181111 |  774 | `		for( i2 = 0 ; i2 < nLen2 ; i2++ ){` |
| 180427 |  775 | `			c0 = p1[i2] + ((zStr1[i1] == zStr2[i2]) ? 0 : iCostRep);` |
| 180427 |  776 | `			c1 = p1[i2 + 1] + iCostDel;` |
| 180427 |  777 | `			if( c1 < c0 ){` |
|  45393 |  778 | `				c0 = c1;` |
|  22696 |  779 | `			}` |
| 180427 |  780 | `			c2 = p2[i2] + iCostIns;` |
| 180427 |  781 | `			if( c2 < c0 ){` |
|  44809 |  782 | `				c0 = c2;` |
|  22404 |  783 | `			}` |
| 180427 |  784 | `			p2[i2 + 1] = c0;` |
|  90214 |  785 | `		}` |
|    685 |  786 | `		pTmp = p1;` |
|    685 |  787 | `		p1 = p2;` |
|    685 |  788 | `		p2 = pTmp;` |
|    343 |  789 | `	}` |
|     23 |  790 | `	ph7_result_int64(pCtx,p1[nLen2]);` |
|     23 |  791 | `	rc = PH7_OK;` |
|     17 |  792 | `out:` |
|     35 |  793 | `	PH7_MemObjRelease(&sTmp1);` |
|     35 |  794 | `	PH7_MemObjRelease(&sTmp2);` |
|     35 |  795 | `	return rc;` |
|     18 |  796 | `}` |
|      - |  797 | `/*` |
|      - |  798 | ` * Longest common substring scan behind similar_text() — a faithful port of` |
|      - |  799 | ` * PHP's php_similar_str(): O(n*m) scan recording the first longest run.` |
|      - |  800 | ` */` |
|     26 |  801 | `static void SimilarStr(const char *zTxt1,int nLen1,const char *zTxt2,int nLen2,` |
|      - |  802 | `	int *pPos1,int *pPos2,int *pMax,int *pCount)` |
|      1 |  803 | `{` |
|      - |  804 | `	const char *p,*q;` |
|     27 |  805 | `	const char *zEnd1 = &zTxt1[nLen1];` |
|     27 |  806 | `	const char *zEnd2 = &zTxt2[nLen2];` |
|      - |  807 | `	int l;` |
|     27 |  808 | `	*pMax = 0;` |
|     27 |  809 | `	*pCount = 0;` |
|    143 |  810 | `	for( p = zTxt1 ; p < zEnd1 ; p++ ){` |
|    843 |  811 | `		for( q = zTxt2 ; q < zEnd2 ; q++ ){` |
|    999 |  812 | `			for( l = 0 ; (p+l < zEnd1) && (q+l < zEnd2) && (p[l] == q[l]) ; l++ );` |
|    727 |  813 | `			if( l > *pMax ){` |
|     25 |  814 | `				*pMax = l;` |
|     25 |  815 | `				*pCount += 1;` |
|     25 |  816 | `				*pPos1 = (int)(p - zTxt1);` |
|     25 |  817 | `				*pPos2 = (int)(q - zTxt2);` |
|     12 |  818 | `			}` |
|    364 |  819 | `		}` |
|     59 |  820 | `	}` |
|     27 |  821 | `}` |
|      - |  822 | `/*` |
|      - |  823 | ` * Recursive divide-and-conquer behind similar_text() — a faithful port of` |
|      - |  824 | `` * PHP's php_similar_char(), including its quirky `count > 1` guard on the`` |
|      - |  825 | ` * left-side recursion.` |
|      - |  826 | ` */` |
|     26 |  827 | `static int SimilarChar(const char *zTxt1,int nLen1,const char *zTxt2,int nLen2)` |
|      1 |  828 | `{` |
|      - |  829 | `	int nSum;` |
|     27 |  830 | `	int nPos1 = 0,nPos2 = 0,nMax,nCount;` |
|     27 |  831 | `	SimilarStr(zTxt1,nLen1,zTxt2,nLen2,&nPos1,&nPos2,&nMax,&nCount);` |
|     27 |  832 | `	if( (nSum = nMax) != 0 ){` |
|     25 |  833 | `		if( nPos1 && nPos2 && nCount > 1 ){` |
|    ! 0 |  834 | `			nSum += SimilarChar(zTxt1,nPos1,zTxt2,nPos2);` |
|    ! 0 |  835 | `		}` |
|     25 |  836 | `		if( (nPos1 + nMax < nLen1) && (nPos2 + nMax < nLen2) ){` |
|     13 |  837 | `			nSum += SimilarChar(&zTxt1[nPos1 + nMax],nLen1 - nPos1 - nMax,` |
|      8 |  838 | `				&zTxt2[nPos2 + nMax],nLen2 - nPos2 - nMax);` |
|      4 |  839 | `		}` |
|     12 |  840 | `	}` |
|     27 |  841 | `	return nSum;` |
|      1 |  842 | `}` |
|      - |  843 | `/*` |
|      - |  844 | ` * int similar_text(string $string1,string $string2[,float &$percent])` |
|      - |  845 | ` *  Calculate the similarity between two strings, as the number of matching` |
|      - |  846 | ` *  characters found by PHP's greedy longest-common-substring recursion.` |
|      - |  847 | ` *  When $percent is given it receives the similarity in percent:` |
|      - |  848 | ` *  matching * 200 / (len1 + len2).` |
|      - |  849 | ` * Return` |
|      - |  850 | ` *  The number of matching characters in both strings.` |
|      - |  851 | ` */` |
|     22 |  852 | `PH7_PRIVATE int PH7_builtin_similar_text(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  853 | `{` |
|      - |  854 | `	const char *zStr1,*zStr2;` |
|      - |  855 | `	ph7_value sTmp1,sTmp2;` |
|      - |  856 | `	int nLen1,nLen2;` |
|      - |  857 | `	int nSim;` |
|      - |  858 | `	sxi32 rc;` |
|     23 |  859 | `	if( nArg < 2 ){` |
|    ! 0 |  860 | `		return PH7_VmThrowException(pCtx,` |
|      - |  861 | `			"ArgumentCountError",` |
|      - |  862 | `			"similar_text() expects at least 2 arguments, %d given",` |
|    ! 0 |  863 | `			nArg` |
|      - |  864 | `			);` |
|      - |  865 | `	}` |
|     23 |  866 | `	PH7_MemObjInit(pCtx->pVm,&sTmp1);` |
|     23 |  867 | `	PH7_MemObjInit(pCtx->pVm,&sTmp2);` |
|     23 |  868 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"similar_text",1,"$string1","string",` |
|      - |  869 | `		"similar_text(): Passing null to parameter #1 ($string1) "` |
|      - |  870 | `		"of type string is deprecated",` |
|      - |  871 | `		&sTmp1,&zStr1,&nLen1);` |
|     23 |  872 | `	if( rc != PH7_OK ) goto out;` |
|     23 |  873 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"similar_text",2,"$string2","string",` |
|      - |  874 | `		"similar_text(): Passing null to parameter #2 ($string2) "` |
|      - |  875 | `		"of type string is deprecated",` |
|      - |  876 | `		&sTmp2,&zStr2,&nLen2);` |
|     23 |  877 | `	if( rc != PH7_OK ) goto out;` |
|     23 |  878 | `	if( nLen1 + nLen2 == 0 ){` |
|      5 |  879 | `		nSim = 0;` |
|      3 |  880 | `	}else{` |
|     19 |  881 | `		nSim = SimilarChar(zStr1,nLen1,zStr2,nLen2);` |
|      - |  882 | `	}` |
|     23 |  883 | `	if( nArg > 2 ){` |
|      - |  884 | `		/* Write the percentage through the by-ref out-param */` |
|      7 |  885 | `		ph7_value *pPercent = ph7_context_new_scalar(pCtx);` |
|      7 |  886 | `		if( pPercent == 0 ){` |
|    ! 0 |  887 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 |  888 | `			goto out;` |
|    ! 0 |  889 | `		}else{` |
|      7 |  890 | `			double dPct = (nLen1 + nLen2 == 0) ? 0.0 : (double)nSim * 200.0 / (double)(nLen1 + nLen2);` |
|      7 |  891 | `			ph7_value_double(pPercent,dPct);` |
|      7 |  892 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],pPercent);` |
|      - |  893 | `		}` |
|      3 |  894 | `	}` |
|     23 |  895 | `	ph7_result_int(pCtx,nSim);` |
|     23 |  896 | `	rc = PH7_OK;` |
|     11 |  897 | `out:` |
|     23 |  898 | `	PH7_MemObjRelease(&sTmp1);` |
|     23 |  899 | `	PH7_MemObjRelease(&sTmp2);` |
|     23 |  900 | `	return rc;` |
|     12 |  901 | `}` |
|      - |  902 | `/*` |
|      - |  903 | ` * array\|int str_word_count(string $string[,int $format = 0[,?string $characters = null]])` |
|      - |  904 | ` *  Count (or return) the words inside a string. A word is a run of alphabetic` |
|      - |  905 | ` *  characters, which may contain (but not start the string with) "'" and "-";` |
|      - |  906 | ` *  $characters adds extra bytes to the word set ("a..z" ranges supported, as` |
|      - |  907 | ` *  in PHP's php_charmask).` |
|      - |  908 | ` *  $format: 0 -> word count, 1 -> array of words, 2 -> array of words keyed` |
|      - |  909 | ` *  by their byte position in $string.` |
|      - |  910 | ` * Errors` |
|      - |  911 | ` *  ValueError when $format is not 0, 1 or 2.` |
|      - |  912 | ` */` |
|     44 |  913 | `PH7_PRIVATE int PH7_builtin_str_word_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  914 | `{` |
|      - |  915 | `	const char *zIn,*zEnd,*zPtr;` |
|     45 |  916 | `	ph7_value *pArray = 0,*pValue = 0;` |
|      - |  917 | `	ph7_value sTmp,sListTmp;` |
|      - |  918 | `	char aMask[256];` |
|     45 |  919 | `	int bMask = 0;` |
|     45 |  920 | `	int iFormat = 0;` |
|     45 |  921 | `	int nCount = 0;` |
|      - |  922 | `	int nLen;` |
|      - |  923 | `	sxi32 rc;` |
|     45 |  924 | `	if( nArg < 1 ){` |
|    ! 0 |  925 | `		return PH7_VmThrowException(pCtx,` |
|      - |  926 | `			"ArgumentCountError",` |
|      - |  927 | `			"str_word_count() expects at least 1 argument, %d given",` |
|    ! 0 |  928 | `			nArg` |
|      - |  929 | `			);` |
|      - |  930 | `	}` |
|     45 |  931 | `	PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|     45 |  932 | `	PH7_MemObjInit(pCtx->pVm,&sListTmp);` |
|     45 |  933 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_word_count",1,"$string","string",` |
|      - |  934 | `		"str_word_count(): Passing null to parameter #1 ($string) "` |
|      - |  935 | `		"of type string is deprecated",` |
|      - |  936 | `		&sTmp,&zIn,&nLen);` |
|     45 |  937 | `	if( rc != PH7_OK ) goto out;` |
|     45 |  938 | `	if( nArg > 1 ){` |
|      - |  939 | `		sxi64 iVal;` |
|     31 |  940 | `		rc = PH7_IntArgResolve(pCtx,apArg[1],"str_word_count",2,"$format","int",&iVal);` |
|     33 |  941 | `		if( rc != PH7_OK ) goto out;` |
|     29 |  942 | `		if( iVal < 0 \|\| iVal > 2 ){` |
|      5 |  943 | `			rc = PH7_VmThrowException(pCtx,` |
|      - |  944 | `				"ValueError",` |
|      - |  945 | `				"str_word_count(): Argument #2 ($format) must be a valid format value"` |
|      - |  946 | `				);` |
|      5 |  947 | `			goto out;` |
|      - |  948 | `		}` |
|     25 |  949 | `		iFormat = (int)iVal;` |
|     12 |  950 | `	}` |
|     39 |  951 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      - |  952 | `		/* $characters is ?string: null (skipped above) simply keeps the` |
|      - |  953 | `		 * default word set, no deprecation. */` |
|      - |  954 | `		const char *zList;` |
|      - |  955 | `		int nList;` |
|     13 |  956 | `		rc = StrPredicateResolveArg(pCtx,apArg[2],"str_word_count",3,"$characters","?string",` |
|      - |  957 | `			"" /* unreachable: null never gets here */,` |
|      - |  958 | `			&sListTmp,&zList,&nList);` |
|     13 |  959 | `		if( rc != PH7_OK ) goto out;` |
|     13 |  960 | `		PH7_BuildCharMask(pCtx,zList,nList,aMask);` |
|     13 |  961 | `		bMask = 1;` |
|      6 |  962 | `	}` |
|     39 |  963 | `	if( iFormat != 0 ){` |
|     25 |  964 | `		pArray = ph7_context_new_array(pCtx);` |
|     25 |  965 | `		pValue = ph7_context_new_scalar(pCtx);` |
|     25 |  966 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 |  967 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 |  968 | `			goto out;` |
|      - |  969 | `		}` |
|     12 |  970 | `	}` |
|     39 |  971 | `	zPtr = zIn;` |
|     39 |  972 | `	zEnd = &zIn[nLen];` |
|     39 |  973 | `	if( nLen > 0 ){` |
|      - |  974 | `		/* php: the string's first byte cannot be ' or -, and its last byte` |
|      - |  975 | `		 * cannot be -, unless the charlist explicitly allows them. */` |
|     33 |  976 | `		if( (zPtr[0] == '\'' && (!bMask \|\| !aMask[(unsigned char)'\''])) \|\|` |
|     28 |  977 | `			(zPtr[0] == '-'  && (!bMask \|\| !aMask[(unsigned char)'-'])) ){` |
|      9 |  978 | `			zPtr++;` |
|      4 |  979 | `		}` |
|     33 |  980 | `		if( zEnd[-1] == '-' && (!bMask \|\| !aMask[(unsigned char)'-']) ){` |
|      9 |  981 | `			zEnd--;` |
|      4 |  982 | `		}` |
|     16 |  983 | `	}` |
|    135 |  984 | `	while( zPtr < zEnd ){` |
|     91 |  985 | `		const char *zStart = zPtr;` |
|    477 |  986 | `		while( zPtr < zEnd && ( SyisAlpha((unsigned char)zPtr[0])` |
|    253 |  987 | `			\|\| (bMask && aMask[(unsigned char)zPtr[0]])` |
|     98 |  988 | `			\|\| zPtr[0] == '\'' \|\| zPtr[0] == '-' ) ){` |
|    339 |  989 | `			zPtr++;` |
|      1 |  990 | `		}` |
|     97 |  991 | `		if( zPtr > zStart ){` |
|     91 |  992 | `			if( iFormat == 0 ){` |
|     19 |  993 | `				nCount++;` |
|     10 |  994 | `			}else{` |
|     73 |  995 | `				ph7_value_reset_string_cursor(pValue);` |
|     73 |  996 | `				if( SXRET_OK != ph7_value_string(pValue,zStart,(int)(zPtr-zStart)) ){` |
|    ! 0 |  997 | `					rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 |  998 | `					goto out;` |
|      - |  999 | `				}` |
|     73 | 1000 | `				if( iFormat == 1 ){` |
|     59 | 1001 | `					if( SXRET_OK != ph7_array_add_elem(pArray,0,pValue) ){` |
|    ! 0 | 1002 | `						rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1003 | `						goto out;` |
|      - | 1004 | `					}` |
|     30 | 1005 | `				}else{` |
|     15 | 1006 | `					if( SXRET_OK != ph7_array_add_intkey_elem(pArray,(int)(zStart-zIn),pValue) ){` |
|    ! 0 | 1007 | `						rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1008 | `						goto out;` |
|      - | 1009 | `					}` |
|      - | 1010 | `				}` |
|      - | 1011 | `			}` |
|     45 | 1012 | `		}` |
|     97 | 1013 | `		zPtr++;` |
|      1 | 1014 | `	}` |
|     37 | 1015 | `	if( iFormat == 0 ){` |
|     13 | 1016 | `		ph7_result_int(pCtx,nCount);` |
|      7 | 1017 | `	}else{` |
|     25 | 1018 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1019 | `	}` |
|     37 | 1020 | `	rc = PH7_OK;` |
|     21 | 1021 | `out:` |
|     43 | 1022 | `	PH7_MemObjRelease(&sTmp);` |
|     43 | 1023 | `	PH7_MemObjRelease(&sListTmp);` |
|     43 | 1024 | `	return rc;` |
|     22 | 1025 | `}` |
|      - | 1026 | `/*` |
|      - | 1027 | ` * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])` |
|      - | 1028 | ` *   Split a string into smaller chunks.` |
|      - | 1029 | ` * Parameters` |
|      - | 1030 | ` *  $body` |
|      - | 1031 | ` *   The string to be chunked.` |
|      - | 1032 | ` * $chunklen` |
|      - | 1033 | ` *   The chunk length.` |
|      - | 1034 | ` * $end` |
|      - | 1035 | ` *   The line ending sequence.` |
|      - | 1036 | ` * Return` |
|      - | 1037 | ` *  The chunked string or NULL on failure.` |
|      - | 1038 | ` */` |
|     14 | 1039 | `PH7_PRIVATE int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1040 | `{` |
|     15 | 1041 | `	const char *zIn,*zEnd,*zSep = "\r\n";` |
|      - | 1042 | `	int nSepLen,nChunkLen,nLen;` |
|      - | 1043 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1044 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     15 | 1045 | `	if( nArg < 1 ){` |
|      - | 1046 | `		/* Nothing to split,return null */` |
|    ! 0 | 1047 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1048 | `		return PH7_OK;` |
|      - | 1049 | `	}` |
|      - | 1050 | `	/* initialize/Extract arguments */` |
|     15 | 1051 | `	nSepLen = (int)sizeof("\r\n") - 1;` |
|     15 | 1052 | `	nChunkLen = 76;` |
|     15 | 1053 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 1054 | `	zEnd = &zIn[nLen];` |
|     15 | 1055 | `	if( nArg > 1 ){` |
|      - | 1056 | `		/* Chunk length */` |
|     13 | 1057 | `		nChunkLen = ph7_value_to_int(apArg[1]);` |
|     13 | 1058 | `		if( nChunkLen < 1 ){` |
|      - | 1059 | `			/* PHP 8 throws a catchable ValueError for a non-positive length. */` |
|      3 | 1060 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1061 | `				"chunk_split(): Argument #2 ($length) must be greater than 0");` |
|      - | 1062 | `		}` |
|     11 | 1063 | `		if( nArg > 2 ){` |
|      - | 1064 | `			/* Separator */` |
|      9 | 1065 | `			zSep = ph7_value_to_string(apArg[2],&nSepLen);` |
|      9 | 1066 | `			if( nSepLen < 1 ){` |
|      - | 1067 | `				/* Switch back to the default separator */` |
|      3 | 1068 | `				zSep = "\r\n";` |
|      3 | 1069 | `				nSepLen = (int)sizeof("\r\n") - 1;` |
|      1 | 1070 | `			}` |
|      4 | 1071 | `		}` |
|      5 | 1072 | `	}` |
|      - | 1073 | `	/* Perform the requested operation */` |
|     13 | 1074 | `	if( nChunkLen > nLen ){` |
|      - | 1075 | `		/* Nothing to split,return the string and the separator */` |
|      9 | 1076 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);` |
|      9 | 1077 | `		return PH7_OK;` |
|      - | 1078 | `	}` |
|     17 | 1079 | `	while( zIn < zEnd ){` |
|     13 | 1080 | `		if( nChunkLen > (int)(zEnd-zIn) ){` |
|      3 | 1081 | `			nChunkLen = (int)(zEnd - zIn);` |
|      1 | 1082 | `		}` |
|      - | 1083 | `		/* Append the chunk and the separator */` |
|     13 | 1084 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);` |
|      - | 1085 | `		/* Point beyond the chunk */` |
|     13 | 1086 | `		zIn += nChunkLen;` |
|      1 | 1087 | `	}` |
|      5 | 1088 | `	return PH7_OK;` |
|      8 | 1089 | `}` |
|      - | 1090 | `/*` |
|      - | 1091 | ` * string addslashes(string $str)` |
|      - | 1092 | ` *  Quote string with slashes.` |
|      - | 1093 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1094 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1095 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1096 | ` * Parameter` |
|      - | 1097 | ` *  str: The string to be escaped.` |
|      - | 1098 | ` * Return` |
|      - | 1099 | ` *  Returns the escaped string` |
|      - | 1100 | ` */` |
|     16 | 1101 | `PH7_PRIVATE int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1102 | `{` |
|      - | 1103 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1104 | `	int nLen;` |
|      - | 1105 | `	/* PHP enforces exactly one argument. */` |
|     17 | 1106 | `	if( nArg != 1 ){` |
|    ! 0 | 1107 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1108 | `			"ArgumentCountError",` |
|      - | 1109 | `			"addslashes() expects exactly 1 argument, %d given",` |
|    ! 0 | 1110 | `			nArg` |
|      - | 1111 | `			);` |
|      - | 1112 | `	}` |
|      - | 1113 | `	/* php only DEPRECATES null here; PHL rejects it. */` |
|     17 | 1114 | `	if( ph7_value_is_null(apArg[0]) ){` |
|    ! 0 | 1115 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1116 | `			"addslashes(): Argument #1 ($string) must be of type string, null given"` |
|      - | 1117 | `			);` |
|      - | 1118 | `	}` |
|      - | 1119 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     24 | 1120 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     25 | 1121 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     16 | 1122 | `	    ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 1123 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1124 | `			"TypeError",` |
|      - | 1125 | `			"addslashes(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 1126 | `			ph7_type_name(apArg[0])` |
|      - | 1127 | `			);` |
|      - | 1128 | `	}` |
|      - | 1129 | `	/* Convert to string representation first and obtain length. */` |
|     17 | 1130 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 1131 | `	if( nLen < 1 ){` |
|      - | 1132 | `		/* Return the empty string */` |
|      3 | 1133 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1134 | `		return PH7_OK;` |
|      - | 1135 | `	}` |
|     15 | 1136 | `	zEnd = &zIn[nLen];` |
|     15 | 1137 | `	zCur = 0; /* cc warning */` |
|     20 | 1138 | `	for(;;){` |
|     41 | 1139 | `		if( zIn >= zEnd ){` |
|      - | 1140 | `			/* No more input */` |
|     15 | 1141 | `			break;` |
|      - | 1142 | `		}` |
|     27 | 1143 | `		zCur = zIn;` |
|      - | 1144 | `		/* scan until a character that needs escaping (', ", \\, or NUL) */` |
|     89 | 1145 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' && zIn[0] != '\0' ){` |
|     63 | 1146 | `			zIn++;` |
|      1 | 1147 | `		}` |
|     27 | 1148 | `		if( zIn > zCur ){` |
|      - | 1149 | `			/* Append raw contents */` |
|     23 | 1150 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 1151 | `		}` |
|     27 | 1152 | `		if( zIn < zEnd ){` |
|     17 | 1153 | `			int c = zIn[0];` |
|     17 | 1154 | `			if( c == '\0' ){` |
|      - | 1155 | `				/* PHP escapes NUL as "\\0" (two characters) */` |
|      3 | 1156 | `				ph7_result_string(pCtx,"\\0",2);` |
|      2 | 1157 | `			}else{` |
|     15 | 1158 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1159 | `			}` |
|      8 | 1160 | `		}` |
|     27 | 1161 | `		zIn++;` |
|      1 | 1162 | `	}` |
|     15 | 1163 | `	return PH7_OK;` |
|      9 | 1164 | `}` |
|      - | 1165 | `/*` |
|      - | 1166 | ``  * Build a 256-entry membership mask from a PHP charlist, expanding `a..z` `` |
|      - | 1167 | ` * byte ranges exactly like PHP's php_charmask(). On return aMask[c] != 0 iff` |
|      - | 1168 | ` * the byte c belongs to the set. Emits the PHP-exact warnings for the three` |
|      - | 1169 | ` * malformed-range shapes (ph7_context_throw_error_format prepends the active` |
|      - | 1170 | ` * function name, so the messages omit it); on a bad range the surrounding` |
|      - | 1171 | ` * bytes are still added and the scan never aborts. Reads only within` |
|      - | 1172 | ` * [zList, zList+nLen).` |
|      - | 1173 | ` *` |
|      - | 1174 | ` * Use ONLY for the builtins whose charlist expands ranges the way PHP's` |
|      - | 1175 | ` * php_charmask() does: trim/ltrim/rtrim/addcslashes (and quotemeta, whose set` |
|      - | 1176 | ` * is a fixed literal with no ".."). Do NOT route strspn/strcspn/strtok/strpbrk` |
|      - | 1177 | ` * through this — PHP treats their charlists literally, so expanding "a..z" here` |
|      - | 1178 | ` * would be a behavior regression plus spurious "Invalid '..'-range" warnings.` |
|      - | 1179 | ` */` |
|    240 | 1180 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256])` |
|      5 | 1181 | `{` |
|    245 | 1182 | `	const unsigned char *zIn  = (const unsigned char *)zList;` |
|    245 | 1183 | `	const unsigned char *zEnd = zIn + (nLen > 0 ? nLen : 0);` |
|    245 | 1184 | `	SyZero(aMask,256);` |
|    679 | 1185 | `	for( ; zIn < zEnd ; zIn++ ){` |
|    439 | 1186 | `		int c = zIn[0];` |
|    439 | 1187 | `		if( zIn + 3 < zEnd && zIn[1] == '.' && zIn[2] == '.' && zIn[3] >= c ){` |
|      - | 1188 | `			/* Valid incrementing range c..zIn[3] */` |
|     22 | 1189 | `			int hi = zIn[3],k;` |
|    386 | 1190 | `			for( k = c ; k <= hi ; k++ ){` |
|    366 | 1191 | `				aMask[k] = 1;` |
|    184 | 1192 | `			}` |
|     22 | 1193 | `			zIn += 3; /* the loop's ++ then steps past the range end */` |
|    438 | 1194 | `		}else if( zIn + 1 < zEnd && zIn[0] == '.' && zIn[1] == '.' ){` |
|      - | 1195 | `			/* Malformed range: mirror php_charmask's three diagnostics. */` |
|      - | 1196 | `			const char *zMsg;` |
|     20 | 1197 | `			if( (const unsigned char *)zList >= zIn ){` |
|      6 | 1198 | `				zMsg = "no character to the left of '..'";` |
|     18 | 1199 | `			}else if( zIn + 2 >= zEnd ){` |
|      6 | 1200 | `				zMsg = "no character to the right of '..'";` |
|     14 | 1201 | `			}else if( zIn[-1] > zIn[2] ){` |
|     12 | 1202 | `				zMsg = "'..'-range needs to be incrementing";` |
|      7 | 1203 | `			}else{` |
|    ! 0 | 1204 | `				zMsg = 0; /* catch-all (e.g. a..b..c) */` |
|      - | 1205 | `			}` |
|     20 | 1206 | `			if( zMsg ){` |
|     29 | 1207 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      9 | 1208 | `					"Invalid '..'-range, %s",zMsg);` |
|     11 | 1209 | `			}else{` |
|    ! 0 | 1210 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1211 | `					"Invalid '..'-range");` |
|      - | 1212 | `			}` |
|      - | 1213 | `			/* Do not consume the dots: the loop's ++ steps one byte so the` |
|      - | 1214 | `			 * dots are re-scanned as literals, exactly like php_charmask. */` |
|     11 | 1215 | `		}else{` |
|    401 | 1216 | `			aMask[c] = 1;` |
|      - | 1217 | `		}` |
|    222 | 1218 | `	}` |
|    245 | 1219 | `}` |
|      - | 1220 | `/*` |
|      - | 1221 | ` * string addcslashes(string $str,string $charlist)` |
|      - | 1222 | ` *  Quote string with slashes in a C style.` |
|      - | 1223 | ` * Parameter` |
|      - | 1224 | ` *  $str:` |
|      - | 1225 | ` *    The string to be escaped.` |
|      - | 1226 | ` *  $charlist:` |
|      - | 1227 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|      - | 1228 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|      - | 1229 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|      - | 1230 | ` * Return` |
|      - | 1231 | ` *  Returns the escaped string.` |
|      - | 1232 | ` * Note:` |
|      - | 1233 | ` *  Character ranges [i.e: 'A..Z'] are supported (see PH7_BuildCharMask).` |
|      - | 1234 | ` */` |
|     28 | 1235 | `PH7_PRIVATE int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1236 | `{` |
|      - | 1237 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - | 1238 | `	char aMask[256];` |
|      - | 1239 | `	int nLen,nMask;` |
|      - | 1240 | `	/* PHP enforces exactly two arguments. */` |
|     31 | 1241 | `	if( nArg != 2 ){` |
|    ! 0 | 1242 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1243 | `			"ArgumentCountError",` |
|      - | 1244 | `			"addcslashes() expects exactly 2 arguments, %d given",` |
|    ! 0 | 1245 | `			nArg` |
|      - | 1246 | `			);` |
|      - | 1247 | `	}` |
|      - | 1248 | `	/* php only DEPRECATES null here; PHL rejects it. */` |
|     31 | 1249 | `	if( ph7_value_is_null(apArg[0]) ){` |
|    ! 0 | 1250 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1251 | `			"TypeError",` |
|      - | 1252 | `			"addcslashes(): Argument #1 ($string) must be of type string, null given"` |
|      - | 1253 | `			);` |
|     42 | 1254 | `	} else if( ph7_value_is_array(apArg[0]) \|\|` |
|     45 | 1255 | `	          ph7_value_is_object(apArg[0]) \|\|` |
|     28 | 1256 | `	          ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 1257 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1258 | `			"TypeError",` |
|      - | 1259 | `			"addcslashes(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 1260 | `			ph7_type_name(apArg[0])` |
|      - | 1261 | `			);` |
|      - | 1262 | `	}` |
|      - | 1263 | `	/* php only DEPRECATES null here; PHL rejects it. */` |
|     31 | 1264 | `	if( ph7_value_is_null(apArg[1]) ){` |
|    ! 0 | 1265 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1266 | `			"TypeError",` |
|      - | 1267 | `			"addcslashes(): Argument #2 ($characters) must be of type string, null given"` |
|      - | 1268 | `			);` |
|     42 | 1269 | `	} else if( ph7_value_is_array(apArg[1]) \|\|` |
|     45 | 1270 | `	          ph7_value_is_object(apArg[1]) \|\|` |
|     28 | 1271 | `	          ph7_value_is_resource(apArg[1]) ){` |
|    ! 0 | 1272 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1273 | `			"TypeError",` |
|      - | 1274 | `			"addcslashes(): Argument #2 ($characters) must be of type string, %s given",` |
|    ! 0 | 1275 | `			ph7_type_name(apArg[1])` |
|      - | 1276 | `			);` |
|      - | 1277 | `	}` |
|      - | 1278 | `	/* Extract the string to process */` |
|     31 | 1279 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1280 | `	/* NULL would never reach here due to the check above. */` |
|     31 | 1281 | `	if( nLen < 1 ){` |
|      - | 1282 | `		/* Empty string returns itself. */` |
|      3 | 1283 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      3 | 1284 | `		return PH7_OK;` |
|      - | 1285 | `	}` |
|      - | 1286 | ``	/* Extract the desired mask and expand any `a..z` ranges into a lookup. */`` |
|     29 | 1287 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|     29 | 1288 | `	PH7_BuildCharMask(pCtx,zMask,nMask,aMask);` |
|     29 | 1289 | `	zEnd = &zIn[nLen];` |
|     29 | 1290 | `	zCur = 0; /* cc warning */` |
|     35 | 1291 | `	for(;;){` |
|     73 | 1292 | `		if( zIn >= zEnd ){` |
|      - | 1293 | `			/* No more input */` |
|     29 | 1294 | `			break;` |
|      - | 1295 | `		}` |
|     47 | 1296 | `		zCur = zIn;` |
|    117 | 1297 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     73 | 1298 | `			zIn++;` |
|      3 | 1299 | `		}` |
|     47 | 1300 | `		if( zIn > zCur ){` |
|      - | 1301 | `			/* Append raw contents */` |
|     41 | 1302 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     19 | 1303 | `		}` |
|     47 | 1304 | `		if( zIn < zEnd ){` |
|      - | 1305 | `			/* Make sure we treat the byte as unsigned to avoid negative values` |
|      - | 1306 | `			 * on platforms where char is signed. */` |
|     29 | 1307 | `			int c = (unsigned char)zIn[0];` |
|      - | 1308 | `			/* Handle special C-like escapes for common control characters first.` |
|      - | 1309 | `			 * PHP outputs "\n" "\r" "\t" "\v" "\f" when those chars are` |
|      - | 1310 | `			 * in the mask. NUL is left to the octal conversion below. */` |
|     29 | 1311 | `			if( c == '\n' ){` |
|      3 | 1312 | `				ph7_result_string(pCtx,"\\n",2);` |
|     28 | 1313 | `			}else if( c == '\r' ){` |
|      3 | 1314 | `				ph7_result_string(pCtx,"\\r",2);` |
|     26 | 1315 | `			}else if( c == '\t' ){` |
|      3 | 1316 | `				ph7_result_string(pCtx,"\\t",2);` |
|     24 | 1317 | `			}else if( c == '\v' ){` |
|      3 | 1318 | `				ph7_result_string(pCtx,"\\v",2);` |
|     22 | 1319 | `			}else if( c == '\f' ){` |
|      3 | 1320 | `				ph7_result_string(pCtx,"\\f",2);` |
|     20 | 1321 | `			}else if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - | 1322 | `				/* Convert to octal.  PHP always emits three-digit zero-padded` |
|      - | 1323 | `				 * octal escapes (\001 not \1). */` |
|      7 | 1324 | `				ph7_result_string_format(pCtx,"\\%03o",c);` |
|      4 | 1325 | `			}else{` |
|     13 | 1326 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1327 | `			}` |
|     13 | 1328 | `		}` |
|     47 | 1329 | `		zIn++;` |
|      3 | 1330 | `	}` |
|     29 | 1331 | `	return PH7_OK;` |
|     17 | 1332 | `}` |
|      - | 1333 | `/*` |
|      - | 1334 | ` * string quotemeta(string $str)` |
|      - | 1335 | ` *  Quote meta characters.` |
|      - | 1336 | ` * Parameter` |
|      - | 1337 | ` *  $str:` |
|      - | 1338 | ` *    The string to be escaped.` |
|      - | 1339 | ` * Return` |
|      - | 1340 | ` *  Returns the escaped string.` |
|      - | 1341 | `*/` |
|     10 | 1342 | `PH7_PRIVATE int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1343 | `{` |
|      - | 1344 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1345 | `	char aMask[256];` |
|      - | 1346 | `	int nLen;` |
|     12 | 1347 | `	if( nArg < 1 ){` |
|      - | 1348 | `		/* Nothing to process,retun NULL */` |
|    ! 0 | 1349 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1350 | `		return PH7_OK;` |
|      - | 1351 | `	}` |
|      - | 1352 | `	/* Extract the string to process */` |
|     12 | 1353 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     12 | 1354 | `	if( nLen < 1 ){` |
|      - | 1355 | `		/* Return the empty string */` |
|      3 | 1356 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1357 | `		return PH7_OK;` |
|      - | 1358 | `	}` |
|      - | 1359 | `	/* Fixed meta-character set (no ranges); build the lookup once. */` |
|     10 | 1360 | `	PH7_BuildCharMask(pCtx,".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1,aMask);` |
|     10 | 1361 | `	zEnd = &zIn[nLen];` |
|     10 | 1362 | `	zCur = 0; /* cc warning */` |
|     22 | 1363 | `	for(;;){` |
|     46 | 1364 | `		if( zIn >= zEnd ){` |
|      - | 1365 | `			/* No more input */` |
|     10 | 1366 | `			break;` |
|      - | 1367 | `		}` |
|     38 | 1368 | `		zCur = zIn;` |
|     76 | 1369 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     40 | 1370 | `			zIn++;` |
|      2 | 1371 | `		}` |
|     38 | 1372 | `		if( zIn > zCur ){` |
|      - | 1373 | `			/* Append raw contents */` |
|     20 | 1374 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      9 | 1375 | `		}` |
|     38 | 1376 | `		if( zIn < zEnd ){` |
|     36 | 1377 | `			int c = zIn[0];` |
|     36 | 1378 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     17 | 1379 | `		}` |
|     38 | 1380 | `		zIn++;` |
|      2 | 1381 | `	}` |
|     10 | 1382 | `	return PH7_OK;` |
|      7 | 1383 | `}` |
|      - | 1384 | `/*` |
|      - | 1385 | ` * string stripslashes(string $str)` |
|      - | 1386 | ` *  Un-quotes a quoted string.` |
|      - | 1387 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1388 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1389 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1390 | ` * Parameter` |
|      - | 1391 | ` *  $str` |
|      - | 1392 | ` *   The input string.` |
|      - | 1393 | ` * Return` |
|      - | 1394 | ` *  Returns a string with backslashes stripped off.` |
|      - | 1395 | ` */` |
|      6 | 1396 | `PH7_PRIVATE int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1397 | `{` |
|      - | 1398 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1399 | `	int nLen;` |
|      7 | 1400 | `	if( nArg < 1 ){` |
|      - | 1401 | `		/* Nothing to process,retun NULL */` |
|    ! 0 | 1402 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1403 | `		return PH7_OK;` |
|      - | 1404 | `	}` |
|      - | 1405 | `	/* Extract the string to process */` |
|      7 | 1406 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 1407 | `	if( zIn == 0 ){` |
|    ! 0 | 1408 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1409 | `		return PH7_OK;` |
|      - | 1410 | `	}` |
|      7 | 1411 | `	zEnd = &zIn[nLen];` |
|      7 | 1412 | `	zCur = 0; /* cc warning */` |
|      - | 1413 | `	/* Seed an empty string result: the loop below only ever APPENDS, so without` |
|      - | 1414 | `	 * this an empty input would leave the return value untouched and answer` |
|      - | 1415 | `	 * NULL where php answers "". */` |
|      7 | 1416 | `	ph7_result_string(pCtx,"",0);` |
|      - | 1417 | `	/* Encode the string */` |
|      4 | 1418 | `	for(;;){` |
|      9 | 1419 | `		if( zIn >= zEnd ){` |
|      - | 1420 | `			/* No more input */` |
|      3 | 1421 | `			break;` |
|      - | 1422 | `		}` |
|      7 | 1423 | `		zCur = zIn;` |
|     19 | 1424 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 | 1425 | `			zIn++;` |
|      1 | 1426 | `		}` |
|      7 | 1427 | `		if( zIn > zCur ){` |
|      - | 1428 | `			/* Append raw contents */` |
|      5 | 1429 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 1430 | `		}` |
|      7 | 1431 | `		if( &zIn[1] < zEnd ){` |
|      3 | 1432 | `			int c = zIn[1];` |
|      3 | 1433 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - | 1434 | `				/* Ignore the backslash */` |
|      3 | 1435 | `				zIn++;` |
|      1 | 1436 | `			}` |
|      2 | 1437 | `		}else{` |
|      5 | 1438 | `			break;` |
|      - | 1439 | `		}` |
|      1 | 1440 | `	}` |
|      7 | 1441 | `	return PH7_OK;` |
|      4 | 1442 | `}` |
|      - | 1443 | `/*` |
|      - | 1444 | ` * UTF-8-aware HTML entity machinery, shared by htmlspecialchars/htmlentities/` |
|      - | 1445 | ` * htmlspecialchars_decode/html_entity_decode/get_html_translation_table.` |
|      - | 1446 | ` * The implementations live further down in this file, next to the filter_var` |
|      - | 1447 | ` * FULL_SPECIAL_CHARS machinery they reuse (aHtml401Ent[]/FvHtml401Lookup()/` |
|      - | 1448 | ` * FvUtf8Next()). Semantics are byte-exact vs php 8.5.7; PHL is UTF-8-only` |
|      - | 1449 | ` * so every charset argument other than a UTF-8 alias gets PHP's` |
|      - | 1450 | ` * unsupported-charset warning and is treated as UTF-8.` |
|      - | 1451 | ` *` |
|      - | 1452 | ` * Flag model (the PHP-exact ENT_* values, see constant.c): bit 1 = encode/` |
|      - | 1453 | ` * decode single quotes, bit 2 = double quotes (ENT_QUOTES=3, ENT_COMPAT=2,` |
|      - | 1454 | ` * ENT_NOQUOTES=0); bits 16\|32 select the doctype (0=HTML401, 16=XML1,` |
|      - | 1455 | ` * 32=XHTML, 48=HTML5); ENT_IGNORE=4 drops invalid UTF-8 bytes (wins over` |
|      - | 1456 | ` * ENT_SUBSTITUTE=8, which replaces each with U+FFFD; with neither set the` |
|      - | 1457 | ` * whole result collapses to ""); ENT_DISALLOWED=128 substitutes valid but` |
|      - | 1458 | ` * doctype-disallowed codepoints. The shared default is` |
|      - | 1459 | ` * ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 = 11.` |
|      - | 1460 | ` */` |
|      - | 1461 | `/*` |
|      - | 1462 | ` * string htmlspecialchars(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1463 | ` *                         [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1464 | ` *  Convert the special characters & < > " ' to HTML entities.` |
|      - | 1465 | ` * Return` |
|      - | 1466 | ` *  The escaped string or NULL on failure.` |
|      - | 1467 | ` */` |
|     42 | 1468 | `PH7_PRIVATE int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1469 | `{` |
|     43 | 1470 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1471 | `	const char *zIn;` |
|     43 | 1472 | `	int nLen,bDouble = 1;` |
|      - | 1473 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1474 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     43 | 1475 | `	if( nArg < 1 ){` |
|      - | 1476 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1477 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1478 | `		return PH7_OK;` |
|      - | 1479 | `	}` |
|      - | 1480 | `	/* Extract the target string */` |
|     43 | 1481 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     43 | 1482 | `	if( nArg > 1 ){` |
|     35 | 1483 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     17 | 1484 | `	}` |
|     43 | 1485 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     43 | 1486 | `	if( nArg > 3 ){` |
|      7 | 1487 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      3 | 1488 | `	}` |
|     43 | 1489 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,0,bDouble);` |
|     43 | 1490 | `	return PH7_OK;` |
|     22 | 1491 | `}` |
|      - | 1492 | `/*` |
|      - | 1493 | ` * string htmlspecialchars_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401])` |
|      - | 1494 | ` *  Convert the special HTML entities (&amp; &lt; &gt; &quot; and the` |
|      - | 1495 | ` *  numeric/doctype forms of the two quotes) back to characters.` |
|      - | 1496 | ` * Return` |
|      - | 1497 | ` *  The unescaped string or NULL on failure.` |
|      - | 1498 | ` */` |
|     22 | 1499 | `PH7_PRIVATE int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1500 | `{` |
|     23 | 1501 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1502 | `	const char *zIn;` |
|      - | 1503 | `	int nLen;` |
|      - | 1504 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1505 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     23 | 1506 | `	if( nArg < 1 ){` |
|      - | 1507 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1508 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1509 | `		return PH7_OK;` |
|      - | 1510 | `	}` |
|      - | 1511 | `	/* Extract the target string */` |
|     23 | 1512 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 1513 | `	if( nArg > 1 ){` |
|      9 | 1514 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1515 | `	}` |
|     23 | 1516 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,0);` |
|     23 | 1517 | `	return PH7_OK;` |
|     12 | 1518 | `}` |
|      - | 1519 | `/*` |
|      - | 1520 | ` * array get_html_translation_table(int $table = HTML_SPECIALCHARS` |
|      - | 1521 | ` *      [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 [, string $encoding = "UTF-8"]])` |
|      - | 1522 | ` *  Return the translation table used by htmlspecialchars() (HTML_SPECIALCHARS)` |
|      - | 1523 | ` *  or htmlentities() (HTML_ENTITIES) as character => entity pairs.` |
|      - | 1524 | ` * Return` |
|      - | 1525 | ` *  The translation table as an array or NULL on failure.` |
|      - | 1526 | ` */` |
|     12 | 1527 | `PH7_PRIVATE int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1528 | `{` |
|     13 | 1529 | `	int iTable = 0; /* HTML_SPECIALCHARS */` |
|     13 | 1530 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|     13 | 1531 | `	if( nArg > 0 ){` |
|     11 | 1532 | `		iTable = ph7_value_to_int(apArg[0]);` |
|      5 | 1533 | `	}` |
|     13 | 1534 | `	if( nArg > 1 ){` |
|      9 | 1535 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1536 | `	}` |
|     13 | 1537 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     13 | 1538 | `	HtmlTranslationTable(pCtx,iTable,iFlags);` |
|     13 | 1539 | `	return PH7_OK;` |
|      1 | 1540 | `}` |
|      - | 1541 | `/*` |
|      - | 1542 | ` * string htmlentities(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1543 | ` *                     [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1544 | ` *  Convert all applicable characters to HTML entities: the specials plus` |
|      - | 1545 | ` *  every codepoint with an HTML 4.01 named entity (aHtml401Ent[]).` |
|      - | 1546 | ` * Return` |
|      - | 1547 | ` *  The encoded string or NULL on failure.` |
|      - | 1548 | ` */` |
|     30 | 1549 | `PH7_PRIVATE int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1550 | `{` |
|     31 | 1551 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1552 | `	const char *zIn;` |
|     31 | 1553 | `	int nLen,bDouble = 1;` |
|      - | 1554 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1555 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     31 | 1556 | `	if( nArg < 1 ){` |
|      - | 1557 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1558 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1559 | `		return PH7_OK;` |
|      - | 1560 | `	}` |
|      - | 1561 | `	/* Extract the target string */` |
|     31 | 1562 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 1563 | `	if( nArg > 1 ){` |
|     19 | 1564 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1565 | `	}` |
|     31 | 1566 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     31 | 1567 | `	if( nArg > 3 ){` |
|      3 | 1568 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      1 | 1569 | `	}` |
|     31 | 1570 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,1,bDouble);` |
|     31 | 1571 | `	return PH7_OK;` |
|     16 | 1572 | `}` |
|      - | 1573 | `/*` |
|      - | 1574 | ` * string html_entity_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1575 | ` *                           [, string $encoding = "UTF-8"]])` |
|      - | 1576 | ` *  Convert HTML entities (named — case-sensitive — and numeric, decimal or` |
|      - | 1577 | ` *  hex) back to their UTF-8 characters. The reverse of htmlentities().` |
|      - | 1578 | ` * Return` |
|      - | 1579 | ` *  The decoded string or NULL on failure.` |
|      - | 1580 | ` */` |
|     58 | 1581 | `PH7_PRIVATE int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1582 | `{` |
|     59 | 1583 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1584 | `	const char *zIn;` |
|      - | 1585 | `	int nLen;` |
|      - | 1586 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1587 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     59 | 1588 | `	if( nArg < 1 ){` |
|      - | 1589 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1590 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1591 | `		return PH7_OK;` |
|      - | 1592 | `	}` |
|      - | 1593 | `	/* Extract the target string */` |
|     59 | 1594 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     59 | 1595 | `	if( nArg > 1 ){` |
|     27 | 1596 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     13 | 1597 | `	}` |
|     59 | 1598 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     59 | 1599 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,1);` |
|     59 | 1600 | `	return PH7_OK;` |
|     30 | 1601 | `}` |
|      - | 1602 | `/*` |
|      - | 1603 | ` * int strlen($string)` |
|      - | 1604 | ` *  return the length of the given string.` |
|      - | 1605 | ` * Parameter` |
|      - | 1606 | ` *  string: The string being measured for length.` |
|      - | 1607 | ` * Return` |
|      - | 1608 | ` *  length of the given string.` |
|      - | 1609 | ` */` |
|  92710 | 1610 | `PH7_PRIVATE int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1611 | `{` |
|  92715 | 1612 | `	int iLen = 0;` |
|  92715 | 1613 | `	if( nArg > 0 ){` |
|  92715 | 1614 | `		StrNullArgNotice(pCtx,apArg[0],"strlen",1,"$string");` |
|  92715 | 1615 | `		ph7_value_to_string(apArg[0],&iLen);` |
|  46559 | 1616 | `	}` |
|      - | 1617 | `	/* String length */` |
|  92715 | 1618 | `	ph7_result_int(pCtx,iLen);` |
|  92715 | 1619 | `	return PH7_OK;` |
|      5 | 1620 | `}` |
|      - | 1621 | `/*` |
|      - | 1622 | ` * int strcmp(string $str1,string $str2)` |
|      - | 1623 | ` *  Perform a binary safe string comparison.` |
|      - | 1624 | ` * Parameter` |
|      - | 1625 | ` *  str1: The first string` |
|      - | 1626 | ` *  str2: The second string` |
|      - | 1627 | ` * Return` |
|      - | 1628 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1629 | ` *  than str2, and 0 if they are equal.` |
|      - | 1630 | ` */` |
|     72 | 1631 | `PH7_PRIVATE int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1632 | `{` |
|      - | 1633 | `	const char *z1,*z2;` |
|      - | 1634 | `	int n1,n2;` |
|      - | 1635 | `	int res;` |
|     73 | 1636 | `	if( nArg < 2 ){` |
|    ! 0 | 1637 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 1638 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 1639 | `		return PH7_OK;` |
|      - | 1640 | `	}` |
|      - | 1641 | `	/* Perform the comparison */` |
|     73 | 1642 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     73 | 1643 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     73 | 1644 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1645 | `	/* Comparison result */` |
|     73 | 1646 | `	ph7_result_int(pCtx,res);` |
|     73 | 1647 | `	return PH7_OK;` |
|     37 | 1648 | `}` |
|      - | 1649 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 1650 | `/*` |
|      - | 1651 | ` * The natural-order comparison core lives OUTSIDE the PH7_DISABLE_BUILTIN_FUNC` |
|      - | 1652 | ` * guard: hashmap.c's SORT_NATURAL path (always compiled) calls PH7_StrNatCmp, so` |
|      - | 1653 | ` * it must exist in the tiny build too. [[tiny-build-disk-io-guard-fragility]]` |
|      - | 1654 | ` */` |
|      - | 1655 | `/*` |
|      - | 1656 | ` * Natural-order comparison core (Martin Pool's natcompare as adapted by php's` |
|      - | 1657 | ` * ext/standard/strnatcmp.c): digit runs compare numerically — the longer run` |
|      - | 1658 | ` * wins, a leading zero flips to fractional first-difference-wins semantics —` |
|      - | 1659 | ` * everything else compares bytewise with whitespace skipped.` |
|      - | 1660 | ` */` |
|     42 | 1661 | `static int StrNatCompareRight(const char **pa,const char *aEnd,const char **pb,const char *bEnd)` |
|      1 | 1662 | `{` |
|     43 | 1663 | `	int bias = 0;` |
|     71 | 1664 | `	for(;;){` |
|     93 | 1665 | `		int da = (*pa < aEnd) && SyisDigit(**pa);` |
|     93 | 1666 | `		int db = (*pb < bEnd) && SyisDigit(**pb);` |
|     93 | 1667 | `		if( !da && !db ){ return bias; }` |
|     73 | 1668 | `		if( !da ){ return -1; }` |
|     63 | 1669 | `		if( !db ){ return 1; }` |
|     51 | 1670 | `		if( **pa < **pb ){ if( !bias ){ bias = -1; } }` |
|     37 | 1671 | `		else if( **pa > **pb ){ if( !bias ){ bias = 1; } }` |
|     51 | 1672 | `		(*pa)++;` |
|     51 | 1673 | `		(*pb)++;` |
|      1 | 1674 | `	}` |
|     22 | 1675 | `}` |
|      4 | 1676 | `static int StrNatCompareLeft(const char **pa,const char *aEnd,const char **pb,const char *bEnd)` |
|      1 | 1677 | `{` |
|      2 | 1678 | `	for(;;){` |
|      5 | 1679 | `		int da = (*pa < aEnd) && SyisDigit(**pa);` |
|      5 | 1680 | `		int db = (*pb < bEnd) && SyisDigit(**pb);` |
|      5 | 1681 | `		if( !da && !db ){ return 0; }` |
|      5 | 1682 | `		if( !da ){ return -1; }` |
|      5 | 1683 | `		if( !db ){ return 1; }` |
|      5 | 1684 | `		if( **pa < **pb ){ return -1; }` |
|    ! 0 | 1685 | `		if( **pa > **pb ){ return 1; }` |
|    ! 0 | 1686 | `		(*pa)++;` |
|    ! 0 | 1687 | `		(*pb)++;` |
|    ! 0 | 1688 | `	}` |
|      3 | 1689 | `}` |
|     48 | 1690 | `PH7_PRIVATE int PH7_StrNatCmp(const char *zA,int nA,const char *zB,int nB,int bFold)` |
|      1 | 1691 | `{` |
|     49 | 1692 | `	const char *a = zA,*aEnd = &zA[nA];` |
|     49 | 1693 | `	const char *b = zB,*bEnd = &zB[nB];` |
|    146 | 1694 | `	for(;;){` |
|      - | 1695 | `		int ca,cb;` |
|    175 | 1696 | `		while( a < aEnd && SyisSpace(a[0]) ){ a++; }` |
|    173 | 1697 | `		while( b < bEnd && SyisSpace(b[0]) ){ b++; }` |
|    173 | 1698 | `		ca = (a < aEnd) ? (unsigned char)a[0] : 0;` |
|    173 | 1699 | `		cb = (b < bEnd) ? (unsigned char)b[0] : 0;` |
|    173 | 1700 | `		if( SyisDigit(ca) && SyisDigit(cb) ){` |
|     45 | 1701 | `			int r = (ca == '0' \|\| cb == '0')` |
|      4 | 1702 | `				? StrNatCompareLeft(&a,aEnd,&b,bEnd)` |
|     65 | 1703 | `				: StrNatCompareRight(&a,aEnd,&b,bEnd);` |
|     47 | 1704 | `			if( r ){ return r; }` |
|      5 | 1705 | `			continue;` |
|      - | 1706 | `		}` |
|    127 | 1707 | `		if( ca == 0 && cb == 0 ){ return 0; }` |
|    121 | 1708 | `		if( bFold ){` |
|     67 | 1709 | `			ca = SyToLower(ca);` |
|     67 | 1710 | `			cb = SyToLower(cb);` |
|     33 | 1711 | `		}` |
|    121 | 1712 | `		if( ca < cb ){ return -1; }` |
|    121 | 1713 | `		if( ca > cb ){ return 1; }` |
|    121 | 1714 | `		a++;` |
|    121 | 1715 | `		b++;` |
|      1 | 1716 | `	}` |
|     25 | 1717 | `}` |
|      - | 1718 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 1719 | `/*` |
|      - | 1720 | ` * int strnatcmp(string $string1, string $string2)` |
|      - | 1721 | ` * int strnatcasecmp(string $string1, string $string2)` |
|      - | 1722 | ` *  Natural-order string comparison ("img2" < "img10"), case folded for the` |
|      - | 1723 | ` *  latter. php 8.2+ normalizes the result to -1/0/1.` |
|      - | 1724 | ` */` |
|     20 | 1725 | `PH7_PRIVATE int PH7_builtin_strnatcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1726 | `{` |
|      - | 1727 | `	const char *z1,*z2,*zFunc;` |
|      - | 1728 | `	int n1,n2,bFold;` |
|     21 | 1729 | `	if( nArg < 2 ){` |
|    ! 0 | 1730 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 1731 | `		return PH7_OK;` |
|      - | 1732 | `	}` |
|     21 | 1733 | `	zFunc = ph7_function_name(pCtx);` |
|     21 | 1734 | `	bFold = zFunc[sizeof("strnat")-1] == 'c'; /* strnatCasecmp */` |
|     21 | 1735 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     21 | 1736 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     21 | 1737 | `	ph7_result_int(pCtx,PH7_StrNatCmp(z1,n1,z2,n2,bFold));` |
|     21 | 1738 | `	return PH7_OK;` |
|     11 | 1739 | `}` |
|      - | 1740 | `/*` |
|      - | 1741 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 1742 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 1743 | ` * Parameter` |
|      - | 1744 | ` *  str1: The first string` |
|      - | 1745 | ` *  str2: The second string` |
|      - | 1746 | ` * Return` |
|      - | 1747 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1748 | ` *  than str2, and 0 if they are equal.` |
|      - | 1749 | ` */` |
|    380 | 1750 | `PH7_PRIVATE int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1751 | `{` |
|      - | 1752 | `	const char *z1,*z2;` |
|      - | 1753 | `	int res;` |
|      - | 1754 | `	int n;` |
|    382 | 1755 | `	if( nArg < 3 ){` |
|      - | 1756 | `		/* Perform a standard comparison */` |
|    ! 0 | 1757 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 1758 | `	}` |
|      - | 1759 | `	/* Desired comparison length */` |
|    382 | 1760 | `	n  = ph7_value_to_int(apArg[2]);` |
|    382 | 1761 | `	if( n < 0 ){` |
|      - | 1762 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 1763 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1764 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 1765 | `			ph7_function_name(pCtx));` |
|      - | 1766 | `	}` |
|      - | 1767 | `	/* Perform the comparison */` |
|    380 | 1768 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|    380 | 1769 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|    380 | 1770 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 1771 | `	/* Comparison result */` |
|    380 | 1772 | `	ph7_result_int(pCtx,res);` |
|    380 | 1773 | `	return PH7_OK;` |
|    192 | 1774 | `}` |
|      - | 1775 | `/*` |
|      - | 1776 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 1777 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 1778 | ` * Parameter` |
|      - | 1779 | ` *  str1: The first string` |
|      - | 1780 | ` *  str2: The second string` |
|      - | 1781 | ` * Return` |
|      - | 1782 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1783 | ` *  than str2, and 0 if they are equal.` |
|      - | 1784 | ` */` |
|    152 | 1785 | `PH7_PRIVATE int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1786 | `{` |
|      - | 1787 | `	const char *z1,*z2;` |
|      - | 1788 | `	int n1,n2;` |
|      - | 1789 | `	int res;` |
|    153 | 1790 | `	if( nArg < 2 ){` |
|    ! 0 | 1791 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 1792 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 1793 | `		return PH7_OK;` |
|      - | 1794 | `	}` |
|      - | 1795 | `	/* Perform the comparison */` |
|    153 | 1796 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|    153 | 1797 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|    153 | 1798 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1799 | `	/* Comparison result */` |
|    153 | 1800 | `	ph7_result_int(pCtx,res);` |
|    153 | 1801 | `	return PH7_OK;` |
|     77 | 1802 | `}` |
|      - | 1803 | `/*` |
|      - | 1804 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 1805 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 1806 | ` * Parameter` |
|      - | 1807 | ` *  $str1: The first string` |
|      - | 1808 | ` *  $str2: The second string` |
|      - | 1809 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 1810 | ` * Return` |
|      - | 1811 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1812 | ` *  than str2, and 0 if they are equal.` |
|      - | 1813 | ` */` |
|     48 | 1814 | `PH7_PRIVATE int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1815 | `{` |
|      - | 1816 | `	const char *z1,*z2;` |
|      - | 1817 | `	int res;` |
|      - | 1818 | `	int n;` |
|     53 | 1819 | `	if( nArg < 3 ){` |
|      - | 1820 | `		/* Perform a standard comparison */` |
|    ! 0 | 1821 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 1822 | `	}` |
|      - | 1823 | `	/* Desired comparison length */` |
|     53 | 1824 | `	n  = ph7_value_to_int(apArg[2]);` |
|     53 | 1825 | `	if( n < 0 ){` |
|      - | 1826 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 1827 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1828 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 1829 | `			ph7_function_name(pCtx));` |
|      - | 1830 | `	}` |
|      - | 1831 | `	/* Perform the comparison */` |
|     51 | 1832 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     51 | 1833 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     51 | 1834 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 1835 | `	/* Comparison result */` |
|     51 | 1836 | `	ph7_result_int(pCtx,res);` |
|     51 | 1837 | `	return PH7_OK;` |
|     29 | 1838 | `}` |
|      - | 1839 | `/*` |
|      - | 1840 | ` * Implode context [i.e: it's private data].` |
|      - | 1841 | ` * A pointer to the following structure is forwarded` |
|      - | 1842 | ` * verbatim to the array walker callback defined below.` |
|      - | 1843 | ` */` |
|      - | 1844 | `struct implode_data {` |
|      - | 1845 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 1846 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 1847 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 1848 | `	int nSeplen;          /* Separator length */` |
|      - | 1849 | `	int bFirst;           /* TRUE if first call */` |
|      - | 1850 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 1851 | `	sxi32 rc;             /* Captured allocation rc; SXERR_MEM => the builtin raises an OOM fatal */` |
|      - | 1852 | `};` |
|      - | 1853 | `/*` |
|      - | 1854 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 1855 | ` * The following routine is invoked for each array entry passed` |
|      - | 1856 | ` * to the implode() function.` |
|      - | 1857 | ` */` |
| 155960 | 1858 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 1859 | `{` |
|  77980 | 1860 | `	SXUNUSED(pKey);` |
| 155965 | 1861 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 1862 | `	const char *zData;` |
|      - | 1863 | `	int nLen;` |
| 155965 | 1864 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 1865 | `		if( pData->nSeplen > 0 ){` |
|      3 | 1866 | `			if( !pData->bFirst ){` |
|      - | 1867 | `				/* append the separator first */` |
|      3 | 1868 | `				if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 1869 | `					pData->rc = SXERR_MEM;` |
|    ! 0 | 1870 | `					return PH7_ABORT;` |
|      - | 1871 | `				}` |
|      2 | 1872 | `			}else{` |
|    ! 0 | 1873 | `				pData->bFirst = 0;` |
|      - | 1874 | `			}` |
|      1 | 1875 | `		}` |
|      - | 1876 | `		/* Recurse */` |
|      3 | 1877 | `		pData->bFirst = 1;` |
|      3 | 1878 | `		pData->nRecCount++;` |
|      3 | 1879 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 1880 | `		pData->nRecCount--;` |
|      - | 1881 | `		/* Propagate an allocation failure surfaced deeper in the recursion. */` |
|      3 | 1882 | `		if( pData->rc != SXRET_OK ){` |
|    ! 0 | 1883 | `			return PH7_ABORT;` |
|      - | 1884 | `		}` |
|      3 | 1885 | `		return PH7_OK;` |
|      - | 1886 | `	}` |
|      - | 1887 | `	/* Extract the string representation of the entry value */` |
| 155963 | 1888 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 1889 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 155963 | 1890 | `	if( pData->bFirst ){` |
|  33291 | 1891 | `		pData->bFirst = 0;` |
| 139320 | 1892 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 1893 | `		/* append the separator first */` |
| 122597 | 1894 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 1895 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1896 | `			return PH7_ABORT;` |
|      - | 1897 | `		}` |
|  61296 | 1898 | `	}` |
|      - | 1899 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 155963 | 1900 | `	if( nLen > 0 ){` |
| 143805 | 1901 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1902 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1903 | `			return PH7_ABORT;` |
|      - | 1904 | `		}` |
|  71900 | 1905 | `	}` |
| 155963 | 1906 | `	return PH7_OK;` |
|  77985 | 1907 | `}` |
|      - | 1908 | `/*` |
|      - | 1909 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 1910 | ` * string implode(array $pieces,...)` |
|      - | 1911 | ` *  Join array elements with a string.` |
|      - | 1912 | ` * $glue` |
|      - | 1913 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 1914 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 1915 | ` * $pieces` |
|      - | 1916 | ` *   The array of strings to implode.` |
|      - | 1917 | ` * Return` |
|      - | 1918 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 1919 | ` *  order, with the glue string between each element.` |
|      - | 1920 | ` */` |
|  33352 | 1921 | `PH7_PRIVATE int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1922 | `{` |
|      - | 1923 | `	struct implode_data imp_data;` |
|  33357 | 1924 | `	int i = 1;` |
|  33357 | 1925 | `	if( nArg < 1 ){` |
|      - | 1926 | `		/* Missing argument,return NULL */` |
|    ! 0 | 1927 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1928 | `		return PH7_OK;` |
|      - | 1929 | `	}` |
|      - | 1930 | `	/* Prepare the implode context */` |
|  33357 | 1931 | `	imp_data.pCtx = pCtx;` |
|  33357 | 1932 | `	imp_data.bRecursive = 0;` |
|  33357 | 1933 | `	imp_data.bFirst = 1;` |
|  33357 | 1934 | `	imp_data.nRecCount = 0;` |
|  33357 | 1935 | `	imp_data.rc = SXRET_OK;` |
|  33357 | 1936 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  33355 | 1937 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  33355 | 1938 | `		if( nArg > 1 && !ph7_value_is_array(apArg[1]) && !ph7_value_is_null(apArg[1]) ){` |
|      - | 1939 | `			/* php: implode($glue, $pieces) requires an ARRAY. PH7 stringified whatever it` |
|      - | 1940 | `			 * was handed, so implode(",", 5) quietly returned "5". */` |
|      - | 1941 | `			char zBuf[64];` |
|      4 | 1942 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1943 | `				"implode(): Argument #2 ($array) must be of type ?array, %s given",` |
|      2 | 1944 | `				VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 1945 | `		}` |
|  16679 | 1946 | `	}else{` |
|      3 | 1947 | `		imp_data.zSep = 0;` |
|      3 | 1948 | `		imp_data.nSeplen = 0;` |
|      3 | 1949 | `		i = 0;` |
|      - | 1950 | `	}` |
|  33355 | 1951 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 1952 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1953 | `	}` |
|      - | 1954 | `	/* Start the 'join' process */` |
|  66705 | 1955 | `	while( i < nArg ){` |
|  33355 | 1956 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1957 | `			/* Iterate throw array entries */` |
|  33355 | 1958 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 1959 | `			/* Surface a callback allocation failure as a fatal */` |
|  33355 | 1960 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 1961 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1962 | `			}` |
|  16680 | 1963 | `		}else{` |
|      - | 1964 | `			const char *zData;` |
|      - | 1965 | `			int nLen;` |
|      - | 1966 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 1967 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1968 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 1969 | `			if( imp_data.bFirst ){` |
|    ! 0 | 1970 | `				imp_data.bFirst = 0;` |
|    ! 0 | 1971 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1972 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 1973 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1974 | `				}` |
|    ! 0 | 1975 | `			}` |
|      - | 1976 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 1977 | `			if( nLen > 0 ){` |
|    ! 0 | 1978 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1979 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1980 | `				}` |
|    ! 0 | 1981 | `			}` |
|      - | 1982 | `		}` |
|  33355 | 1983 | `		i++;` |
|      5 | 1984 | `	}` |
|  33355 | 1985 | `	return PH7_OK;` |
|  16681 | 1986 | `}` |
|      - | 1987 | `/*` |
|      - | 1988 | ` * Symisc eXtension:` |
|      - | 1989 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 1990 | ` * Purpose` |
|      - | 1991 | ` *  Same as implode() but recurse on arrays.` |
|      - | 1992 | ` * Example:` |
|      - | 1993 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 1994 | ` *   echo implode_recursive("/",$a);` |
|      - | 1995 | ` *   Will output` |
|      - | 1996 | ` *     usr/home/dean.` |
|      - | 1997 | ` *   While the standard implode would produce.` |
|      - | 1998 | ` *    usr/Array.` |
|      - | 1999 | ` * Parameter` |
|      - | 2000 | ` *  Refer to implode().` |
|      - | 2001 | ` * Return` |
|      - | 2002 | ` *  Refer to implode().` |
|      - | 2003 | ` */` |
|     12 | 2004 | `PH7_PRIVATE int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2005 | `{` |
|      - | 2006 | `	struct implode_data imp_data;` |
|     13 | 2007 | `	int i = 1;` |
|     13 | 2008 | `	if( nArg < 1 ){` |
|      - | 2009 | `		/* Missing argument,return NULL */` |
|      3 | 2010 | `		ph7_result_null(pCtx);` |
|      3 | 2011 | `		return PH7_OK;` |
|      - | 2012 | `	}` |
|      - | 2013 | `	/* Prepare the implode context */` |
|     11 | 2014 | `	imp_data.pCtx = pCtx;` |
|     11 | 2015 | `	imp_data.bRecursive = 1;` |
|     11 | 2016 | `	imp_data.bFirst = 1;` |
|     11 | 2017 | `	imp_data.nRecCount = 0;` |
|     11 | 2018 | `	imp_data.rc = SXRET_OK;` |
|     11 | 2019 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 2020 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 2021 | `	}else{` |
|    ! 0 | 2022 | `		imp_data.zSep = 0;` |
|    ! 0 | 2023 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 2024 | `		i = 0;` |
|      - | 2025 | `	}` |
|     11 | 2026 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2027 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2028 | `	}` |
|      - | 2029 | `	/* Start the 'join' process */` |
|     21 | 2030 | `	while( i < nArg ){` |
|     11 | 2031 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2032 | `			/* Iterate throw array entries */` |
|      3 | 2033 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2034 | `			/* Surface a callback allocation failure as a fatal */` |
|      3 | 2035 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2036 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2037 | `			}` |
|      2 | 2038 | `		}else{` |
|      - | 2039 | `			const char *zData;` |
|      - | 2040 | `			int nLen;` |
|      - | 2041 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 2042 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2043 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 2044 | `			if( imp_data.bFirst ){` |
|      9 | 2045 | `				imp_data.bFirst = 0;` |
|      4 | 2046 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2047 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2048 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2049 | `				}` |
|    ! 0 | 2050 | `			}` |
|      - | 2051 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 2052 | `			if( nLen > 0 ){` |
|      9 | 2053 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2054 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2055 | `				}` |
|      4 | 2056 | `			}` |
|      - | 2057 | `		}` |
|     11 | 2058 | `		i++;` |
|      1 | 2059 | `	}` |
|     11 | 2060 | `	return PH7_OK;` |
|      7 | 2061 | `}` |
|      - | 2062 | `/*` |
|      - | 2063 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 2064 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 2065 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 2066 | ` * Parameters` |
|      - | 2067 | ` *  $delimiter` |
|      - | 2068 | ` *   The boundary string.` |
|      - | 2069 | ` * $string` |
|      - | 2070 | ` *   The input string.` |
|      - | 2071 | ` * $limit` |
|      - | 2072 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 2073 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 2074 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 2075 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 2076 | ` * Returns` |
|      - | 2077 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 2078 | ` *  on boundaries formed by the delimiter.` |
|      - | 2079 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 2080 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 2081 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 2082 | ` *  will be returned.` |
|      - | 2083 | ` * NOTE:` |
|      - | 2084 | ` *  Negative limit is not supported.` |
|      - | 2085 | ` */` |
|   6868 | 2086 | `PH7_PRIVATE int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2087 | `{` |
|      - | 2088 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2089 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2090 | `	ph7_value *pArray;` |
|      - | 2091 | `	ph7_value *pValue;` |
|      - | 2092 | `	sxu32 nOfft;` |
|      - | 2093 | `	sxi32 rc;` |
|   6873 | 2094 | `	if( nArg < 2 ){` |
|      - | 2095 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2096 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2097 | `		return PH7_OK;` |
|      - | 2098 | `	}` |
|      - | 2099 | `	/* Extract the delimiter */` |
|   6873 | 2100 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   6873 | 2101 | `	if( nDelim < 1 ){` |
|      - | 2102 | `		/* Empty delimiter: PHP 8 throws a catchable ValueError. */` |
|      5 | 2103 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2104 | `			"explode(): Argument #1 ($separator) must not be empty");` |
|      - | 2105 | `	}` |
|      - | 2106 | `	/* Extract the string */` |
|   6869 | 2107 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   6869 | 2108 | `	if( nStrlen < 1 ){` |
|      - | 2109 | `		/* Empty string: normally an array with a single empty element (PHP behavior).` |
|      - | 2110 | `		 * A negative limit drops the last -limit components, so the sole empty` |
|      - | 2111 | `		 * component is dropped and the result is an empty array. */` |
|     13 | 2112 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|     13 | 2113 | `		if( pArrayTmp == 0 ){` |
|      - | 2114 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 2115 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2116 | `			return PH7_OK;` |
|      - | 2117 | `		}` |
|     13 | 2118 | `		if( !(nArg > 2 && ph7_value_to_int(apArg[2]) < 0) ){` |
|     11 | 2119 | `			ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|     11 | 2120 | `			if( pValueTmp == 0 ){` |
|      - | 2121 | `				/* Out of memory,return FALSE */` |
|    ! 0 | 2122 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 2123 | `				return PH7_OK;` |
|      - | 2124 | `			}` |
|     11 | 2125 | `			ph7_value_string(pValueTmp, "", 0);` |
|     11 | 2126 | `			if( ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp) != SXRET_OK ){` |
|    ! 0 | 2127 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2128 | `			}` |
|      5 | 2129 | `		}` |
|     13 | 2130 | `		ph7_result_value(pCtx, pArrayTmp);` |
|     13 | 2131 | `		return PH7_OK;` |
|      - | 2132 | `	}` |
|      - | 2133 | `	/* Point to the end of the string */` |
|   6857 | 2134 | `	zEnd = &zString[nStrlen];` |
|      - | 2135 | `	/* Create the array */` |
|   6857 | 2136 | `	pArray =  ph7_context_new_array(pCtx);` |
|   6857 | 2137 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   6857 | 2138 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2139 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2140 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2141 | `		return PH7_OK;` |
|      - | 2142 | `	}` |
|      - | 2143 | `	/* Set a defualt limit */` |
|   6857 | 2144 | `	iLimit = SXI32_HIGH;` |
|   6857 | 2145 | `	if( nArg > 2 ){` |
|     55 | 2146 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     55 | 2147 | `		if( iLimit < 0 ){` |
|      - | 2148 | `			/* Negative limit: keep all components except the last -iLimit (PHP).` |
|      - | 2149 | `			 * Pre-count the components (delimiters + 1), then emit only the first` |
|      - | 2150 | `			 * nKeep CLEAN components — no trailing-remainder merge (the difference` |
|      - | 2151 | `			 * from the positive path). nKeep <= 0 drops everything -> empty array. */` |
|     17 | 2152 | `			int nTotal = 1,nKeep;` |
|     17 | 2153 | `			const char *zScan = zString;` |
|      - | 2154 | `			sxu32 nScanOfft;` |
|     57 | 2155 | `			while( SyBlobSearch(zScan,(sxu32)(zEnd - zScan),zDelim,nDelim,&nScanOfft) == SXRET_OK ){` |
|     41 | 2156 | `				nTotal++;` |
|     41 | 2157 | `				zScan = &zScan[nScanOfft + nDelim];` |
|      1 | 2158 | `			}` |
|     17 | 2159 | `			nKeep = nTotal + iLimit; /* iLimit < 0, so this is nTotal - (-iLimit) */` |
|     49 | 2160 | `			while( nKeep > (int)ph7_array_count(pArray)` |
|     39 | 2161 | `				&& SyBlobSearch(zString,(sxu32)(zEnd - zString),zDelim,nDelim,&nOfft) == SXRET_OK ){` |
|      - | 2162 | `				/* Emit the next clean component */` |
|     23 | 2163 | `				zCur = &zString[nOfft];` |
|     23 | 2164 | `				ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|     23 | 2165 | `				if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2166 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2167 | `				}` |
|     23 | 2168 | `				zString = &zCur[nDelim];` |
|     23 | 2169 | `				ph7_value_reset_string_cursor(pValue);` |
|      1 | 2170 | `			}` |
|     17 | 2171 | `			ph7_result_value(pCtx,pArray);` |
|     17 | 2172 | `			return PH7_OK;` |
|      - | 2173 | `		}` |
|     39 | 2174 | `		if( iLimit == 0 ){` |
|      5 | 2175 | `			iLimit = 1;` |
|      2 | 2176 | `		}` |
|     39 | 2177 | `		iLimit--;` |
|     17 | 2178 | `	}` |
|      - | 2179 | `	/* Start exploding */` |
|  82506 | 2180 | `	for(;;){` |
| 165017 | 2181 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 165017 | 2182 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2183 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   6841 | 2184 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   6841 | 2185 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2186 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2187 | `			}` |
|   6841 | 2188 | `			break;` |
|      - | 2189 | `		}` |
|      - | 2190 | `		/* Point to the desired offset */` |
| 158181 | 2191 | `		zCur = &zString[nOfft];` |
|      - | 2192 | `		/* Perform the store operation (may be empty) */` |
| 158181 | 2193 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 158181 | 2194 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2195 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2196 | `		}` |
|      - | 2197 | `		/* Point beyond the delimiter */` |
| 158181 | 2198 | `		zString = &zCur[nDelim];` |
|      - | 2199 | `		/* Reset the cursor */` |
| 158181 | 2200 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 2201 | `	}` |
|      - | 2202 | `	/* Return the freshly created array */` |
|   6841 | 2203 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2204 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2205 | `	 * released as soon we return from this foregin function.` |
|      - | 2206 | `	 */` |
|   6841 | 2207 | `	return PH7_OK;` |
|   3439 | 2208 | `}` |
|      - | 2209 | `/*` |
|      - | 2210 | ` * string trim(string $str[,string $charlist ])` |
|      - | 2211 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2212 | ` * Parameters` |
|      - | 2213 | ` *  $str` |
|      - | 2214 | ` *   The string that will be trimmed.` |
|      - | 2215 | ` * $charlist` |
|      - | 2216 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2217 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2218 | ` *   With .. you can specify a range of characters.` |
|      - | 2219 | ` * Returns.` |
|      - | 2220 | ` *  Thr processed string.` |
|      - | 2221 | ` * NOTE:` |
|      - | 2222 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2223 | ` */` |
|  14264 | 2224 | `PH7_PRIVATE int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2225 | `{` |
|  14269 | 2226 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"trim",1,"$string"); }` |
|      - | 2227 | `	const char *zString;` |
|      - | 2228 | `	int nLen;` |
|  14269 | 2229 | `	if( nArg < 1 ){` |
|      - | 2230 | `		/* Missing arguments,return null */` |
|    ! 0 | 2231 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2232 | `		return PH7_OK;` |
|      - | 2233 | `	}` |
|      - | 2234 | `	/* Extract the target string */` |
|  14269 | 2235 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  14269 | 2236 | `	if( nLen < 1 ){` |
|      - | 2237 | `		/* Empty string,return */` |
|    551 | 2238 | `		ph7_result_string(pCtx,"",0);` |
|    551 | 2239 | `		return PH7_OK;` |
|      - | 2240 | `	}` |
|      - | 2241 | `	/* Start the trim process */` |
|  13723 | 2242 | `	if( nArg < 2 ){` |
|      - | 2243 | `		SyString sStr;` |
|      - | 2244 | `		/* Remove white spaces and NUL bytes */` |
|  13693 | 2245 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  34469 | 2246 | `		SyStringFullTrimSafe(&sStr);` |
|  13693 | 2247 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   6849 | 2248 | `	}else{` |
|      - | 2249 | `		/* Char list */` |
|      - | 2250 | `		const char *zList;` |
|      - | 2251 | `		int nListlen;` |
|     33 | 2252 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     33 | 2253 | `		if( nListlen < 1 ){` |
|      - | 2254 | `			/* Return the string unchanged */` |
|      6 | 2255 | `			ph7_result_string(pCtx,zString,nLen);` |
|      4 | 2256 | `		}else{` |
|      - | 2257 | `			char aMask[256];` |
|     29 | 2258 | `			const char *zEnd = &zString[nLen];` |
|     29 | 2259 | `			const char *zCur = zString;` |
|     29 | 2260 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2261 | `			/* Left trim */` |
|     79 | 2262 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     53 | 2263 | `				zCur++;` |
|      3 | 2264 | `			}` |
|      - | 2265 | `			/* Right trim */` |
|     79 | 2266 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     53 | 2267 | `				zEnd--;` |
|      3 | 2268 | `			}` |
|     29 | 2269 | `			if( zCur >= zEnd ){` |
|      - | 2270 | `				/* Return the empty string */` |
|    ! 0 | 2271 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2272 | `			}else{` |
|     29 | 2273 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2274 | `			}` |
|      - | 2275 | `		}` |
|      - | 2276 | `	}` |
|  13723 | 2277 | `	return PH7_OK;` |
|   7137 | 2278 | `}` |
|      - | 2279 | `/*` |
|      - | 2280 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 2281 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 2282 | ` * Parameters` |
|      - | 2283 | ` *  $str` |
|      - | 2284 | ` *   The string that will be trimmed.` |
|      - | 2285 | ` * $charlist` |
|      - | 2286 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2287 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2288 | ` *   With .. you can specify a range of characters.` |
|      - | 2289 | ` * Returns.` |
|      - | 2290 | ` *  Thr processed string.` |
|      - | 2291 | ` * NOTE:` |
|      - | 2292 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2293 | ` */` |
|    162 | 2294 | `PH7_PRIVATE int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2295 | `{` |
|    166 | 2296 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"rtrim",1,"$string"); }` |
|      - | 2297 | `	const char *zString;` |
|      - | 2298 | `	int nLen;` |
|    166 | 2299 | `	if( nArg < 1 ){` |
|      - | 2300 | `		/* Missing arguments,return null */` |
|    ! 0 | 2301 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2302 | `		return PH7_OK;` |
|      - | 2303 | `	}` |
|      - | 2304 | `	/* Extract the target string */` |
|    166 | 2305 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    166 | 2306 | `	if( nLen < 1 ){` |
|      - | 2307 | `		/* Empty string,return */` |
|      5 | 2308 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2309 | `		return PH7_OK;` |
|      - | 2310 | `	}` |
|      - | 2311 | `	/* Start the trim process */` |
|    162 | 2312 | `	if( nArg < 2 ){` |
|      - | 2313 | `		SyString sStr;` |
|      - | 2314 | `		/* Remove white spaces and NUL bytes*/` |
|     19 | 2315 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     48 | 2316 | `		SyStringRightTrimSafe(&sStr);` |
|     19 | 2317 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|     10 | 2318 | `	}else{` |
|      - | 2319 | `		/* Char list */` |
|      - | 2320 | `		const char *zList;` |
|      - | 2321 | `		int nListlen;` |
|    144 | 2322 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|    144 | 2323 | `		if( nListlen < 1 ){` |
|      - | 2324 | `			/* Return the string unchanged */` |
|    ! 0 | 2325 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 2326 | `		}else{` |
|      - | 2327 | `			char aMask[256];` |
|    144 | 2328 | `			const char *zEnd = &zString[nLen];` |
|    144 | 2329 | `			const char *zCur = zString;` |
|    144 | 2330 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2331 | `			/* Right trim */` |
|    162 | 2332 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     21 | 2333 | `				zEnd--;` |
|      3 | 2334 | `			}` |
|    144 | 2335 | `			if( zEnd <= zCur ){` |
|      - | 2336 | `				/* Return the empty string */` |
|    ! 0 | 2337 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2338 | `			}else{` |
|    144 | 2339 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2340 | `			}` |
|      - | 2341 | `		}` |
|      - | 2342 | `	}` |
|    162 | 2343 | `	return PH7_OK;` |
|     85 | 2344 | `}` |
|      - | 2345 | `/*` |
|      - | 2346 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 2347 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2348 | ` * Parameters` |
|      - | 2349 | ` *  $str` |
|      - | 2350 | ` *   The string that will be trimmed.` |
|      - | 2351 | ` * $charlist` |
|      - | 2352 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2353 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2354 | ` *   With .. you can specify a range of characters.` |
|      - | 2355 | ` * Returns.` |
|      - | 2356 | ` *  Thr processed string.` |
|      - | 2357 | ` * NOTE:` |
|      - | 2358 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2359 | ` */` |
|     52 | 2360 | `PH7_PRIVATE int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2361 | `{` |
|     57 | 2362 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"ltrim",1,"$string"); }` |
|      - | 2363 | `	const char *zString;` |
|      - | 2364 | `	int nLen;` |
|     57 | 2365 | `	if( nArg < 1 ){` |
|      - | 2366 | `		/* Missing arguments,return null */` |
|    ! 0 | 2367 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2368 | `		return PH7_OK;` |
|      - | 2369 | `	}` |
|      - | 2370 | `	/* Extract the target string */` |
|     57 | 2371 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     57 | 2372 | `	if( nLen < 1 ){` |
|      - | 2373 | `		/* Empty string,return */` |
|     21 | 2374 | `		ph7_result_string(pCtx,"",0);` |
|     21 | 2375 | `		return PH7_OK;` |
|      - | 2376 | `	}` |
|      - | 2377 | `	/* Start the trim process */` |
|     39 | 2378 | `	if( nArg < 2 ){` |
|      - | 2379 | `		SyString sStr;` |
|      - | 2380 | `		/* Remove white spaces and NUL byte */` |
|      5 | 2381 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     13 | 2382 | `		SyStringLeftTrimSafe(&sStr);` |
|      5 | 2383 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      3 | 2384 | `	}else{` |
|      - | 2385 | `		/* Char list */` |
|      - | 2386 | `		const char *zList;` |
|      - | 2387 | `		int nListlen;` |
|     35 | 2388 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     35 | 2389 | `		if( nListlen < 1 ){` |
|      - | 2390 | `			/* Return the string unchanged */` |
|      3 | 2391 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2392 | `		}else{` |
|      - | 2393 | `			char aMask[256];` |
|     33 | 2394 | `			const char *zEnd = &zString[nLen];` |
|     33 | 2395 | `			const char *zCur = zString;` |
|     33 | 2396 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2397 | `			/* Left trim */` |
|     87 | 2398 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     59 | 2399 | `				zCur++;` |
|      5 | 2400 | `			}` |
|     33 | 2401 | `			if( zCur >= zEnd ){` |
|      - | 2402 | `				/* Return the empty string */` |
|    ! 0 | 2403 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2404 | `			}else{` |
|     33 | 2405 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2406 | `			}` |
|      - | 2407 | `		}` |
|      - | 2408 | `	}` |
|     39 | 2409 | `	return PH7_OK;` |
|     31 | 2410 | `}` |
|      - | 2411 | `/*` |
|      - | 2412 | ` * string strtolower(string $str)` |
|      - | 2413 | ` *  Make a string lowercase.` |
|      - | 2414 | ` * Parameters` |
|      - | 2415 | ` *  $str` |
|      - | 2416 | ` *   The input string.` |
|      - | 2417 | ` * Returns.` |
|      - | 2418 | ` *  The lowercased string.` |
|      - | 2419 | ` */` |
|  33134 | 2420 | `PH7_PRIVATE int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2421 | `{` |
|  33139 | 2422 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtolower",1,"$string"); }` |
|      - | 2423 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2424 | `	int nLen;` |
|  33139 | 2425 | `	if( nArg < 1 ){` |
|      - | 2426 | `		/* Missing arguments,return null */` |
|    ! 0 | 2427 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2428 | `		return PH7_OK;` |
|      - | 2429 | `	}` |
|      - | 2430 | `	/* Extract the target string */` |
|  33139 | 2431 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  33139 | 2432 | `	if( nLen < 1 ){` |
|      - | 2433 | `		/* Empty string,return */` |
|      3 | 2434 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2435 | `		return PH7_OK;` |
|      - | 2436 | `	}` |
|      - | 2437 | `	/* Perform the requested operation */` |
|  33137 | 2438 | `	zEnd = &zString[nLen];` |
| 104474 | 2439 | `	for(;;){` |
| 208953 | 2440 | `		if( zString >= zEnd ){` |
|      - | 2441 | `			/* No more input,break immediately */` |
|  33137 | 2442 | `			break;` |
|      - | 2443 | `		}` |
| 175821 | 2444 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2445 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2446 | `			zCur = zString;` |
|    ! 0 | 2447 | `			zString++;` |
|    ! 0 | 2448 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2449 | `				zString++;` |
|    ! 0 | 2450 | `			}` |
|      - | 2451 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2452 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2453 | `		}else{` |
| 175821 | 2454 | `			int c = zString[0];` |
| 175821 | 2455 | `			if( SyisUpper(c) ){` |
| 172809 | 2456 | `				c = SyToLower(zString[0]);` |
|  86402 | 2457 | `			}` |
|      - | 2458 | `			/* Append character */` |
| 175821 | 2459 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2460 | `			/* Advance the cursor */` |
| 175821 | 2461 | `			zString++;` |
|      - | 2462 | `		}` |
|      5 | 2463 | `	}` |
|  33137 | 2464 | `	return PH7_OK;` |
|  16572 | 2465 | `}` |
|      - | 2466 | `/*` |
|      - | 2467 | ` * string strtolower(string $str)` |
|      - | 2468 | ` *  Make a string uppercase.` |
|      - | 2469 | ` * Parameters` |
|      - | 2470 | ` *  $str` |
|      - | 2471 | ` *   The input string.` |
|      - | 2472 | ` * Returns.` |
|      - | 2473 | ` *  The uppercased string.` |
|      - | 2474 | ` */` |
|     72 | 2475 | `PH7_PRIVATE int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2476 | `{` |
|     76 | 2477 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtoupper",1,"$string"); }` |
|      - | 2478 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2479 | `	int nLen;` |
|     76 | 2480 | `	if( nArg < 1 ){` |
|      - | 2481 | `		/* Missing arguments,return null */` |
|    ! 0 | 2482 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2483 | `		return PH7_OK;` |
|      - | 2484 | `	}` |
|      - | 2485 | `	/* Extract the target string */` |
|     76 | 2486 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     76 | 2487 | `	if( nLen < 1 ){` |
|      - | 2488 | `		/* Empty string,return */` |
|      3 | 2489 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2490 | `		return PH7_OK;` |
|      - | 2491 | `	}` |
|      - | 2492 | `	/* Perform the requested operation */` |
|     74 | 2493 | `	zEnd = &zString[nLen];` |
|    146 | 2494 | `	for(;;){` |
|    296 | 2495 | `		if( zString >= zEnd ){` |
|      - | 2496 | `			/* No more input,break immediately */` |
|     74 | 2497 | `			break;` |
|      - | 2498 | `		}` |
|    226 | 2499 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2500 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2501 | `			zCur = zString;` |
|    ! 0 | 2502 | `			zString++;` |
|    ! 0 | 2503 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2504 | `				zString++;` |
|    ! 0 | 2505 | `			}` |
|      - | 2506 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2507 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2508 | `		}else{` |
|    226 | 2509 | `			int c = zString[0];` |
|    226 | 2510 | `			if( SyisLower(c) ){` |
|    210 | 2511 | `				c = SyToUpper(zString[0]);` |
|    103 | 2512 | `			}` |
|      - | 2513 | `			/* Append character */` |
|    226 | 2514 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2515 | `			/* Advance the cursor */` |
|    226 | 2516 | `			zString++;` |
|      - | 2517 | `		}` |
|      4 | 2518 | `	}` |
|     74 | 2519 | `	return PH7_OK;` |
|     40 | 2520 | `}` |
|      - | 2521 | `/*` |
|      - | 2522 | ` * string ucfirst(string $str)` |
|      - | 2523 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2524 | ` *  character is alphabetic.` |
|      - | 2525 | ` * Parameters` |
|      - | 2526 | ` *  $str` |
|      - | 2527 | ` *   The input string.` |
|      - | 2528 | ` * Returns.` |
|      - | 2529 | ` *  The processed string.` |
|      - | 2530 | ` */` |
|      4 | 2531 | `PH7_PRIVATE int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2532 | `{` |
|      - | 2533 | `	const char *zString,*zEnd;` |
|      - | 2534 | `	int nLen,c;` |
|      5 | 2535 | `	if( nArg < 1 ){` |
|      - | 2536 | `		/* Missing arguments,return null */` |
|    ! 0 | 2537 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2538 | `		return PH7_OK;` |
|      - | 2539 | `	}` |
|      - | 2540 | `	/* Extract the target string */` |
|      5 | 2541 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2542 | `	if( nLen < 1 ){` |
|      - | 2543 | `		/* Empty string,return */` |
|      3 | 2544 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2545 | `		return PH7_OK;` |
|      - | 2546 | `	}` |
|      - | 2547 | `	/* Perform the requested operation */` |
|      3 | 2548 | `	zEnd = &zString[nLen];` |
|      3 | 2549 | `	c = zString[0];` |
|      3 | 2550 | `	if( SyisLower(c) ){` |
|      3 | 2551 | `		c = SyToUpper(c);` |
|      1 | 2552 | `	}` |
|      - | 2553 | `	/* Append the first character */` |
|      3 | 2554 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2555 | `	zString++;` |
|      3 | 2556 | `	if( zString < zEnd ){` |
|      - | 2557 | `		/* Append the rest of the input verbatim */` |
|      3 | 2558 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2559 | `	}` |
|      3 | 2560 | `	return PH7_OK;` |
|      3 | 2561 | `}` |
|      - | 2562 | `/*` |
|      - | 2563 | ` * string lcfirst(string $str)` |
|      - | 2564 | ` *  Make a string's first character lowercase.` |
|      - | 2565 | ` * Parameters` |
|      - | 2566 | ` *  $str` |
|      - | 2567 | ` *   The input string.` |
|      - | 2568 | ` * Returns.` |
|      - | 2569 | ` *  The processed string.` |
|      - | 2570 | ` */` |
|      4 | 2571 | `PH7_PRIVATE int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2572 | `{` |
|      - | 2573 | `	const char *zString,*zEnd;` |
|      - | 2574 | `	int nLen,c;` |
|      5 | 2575 | `	if( nArg < 1 ){` |
|      - | 2576 | `		/* Missing arguments,return null */` |
|    ! 0 | 2577 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2578 | `		return PH7_OK;` |
|      - | 2579 | `	}` |
|      - | 2580 | `	/* Extract the target string */` |
|      5 | 2581 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2582 | `	if( nLen < 1 ){` |
|      - | 2583 | `		/* Empty string,return */` |
|      3 | 2584 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2585 | `		return PH7_OK;` |
|      - | 2586 | `	}` |
|      - | 2587 | `	/* Perform the requested operation */` |
|      3 | 2588 | `	zEnd = &zString[nLen];` |
|      3 | 2589 | `	c = zString[0];` |
|      3 | 2590 | `	if( SyisUpper(c) ){` |
|      3 | 2591 | `		c = SyToLower(c);` |
|      1 | 2592 | `	}` |
|      - | 2593 | `	/* Append the first character */` |
|      3 | 2594 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2595 | `	zString++;` |
|      3 | 2596 | `	if( zString < zEnd ){` |
|      - | 2597 | `		/* Append the rest of the input verbatim */` |
|      3 | 2598 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2599 | `	}` |
|      3 | 2600 | `	return PH7_OK;` |
|      3 | 2601 | `}` |
|      - | 2602 | `/*` |
|      - | 2603 | ` * int ord(string $string)` |
|      - | 2604 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 2605 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 2606 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 2607 | ` * Parameters` |
|      - | 2608 | ` *  $string` |
|      - | 2609 | ` *   The input string.` |
|      - | 2610 | ` * Returns` |
|      - | 2611 | ` *  The ASCII value as an integer.` |
|      - | 2612 | ` */` |
|    214 | 2613 | `PH7_PRIVATE int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2614 | `{` |
|      - | 2615 | `	const char *zString;` |
|      - | 2616 | `	int nLen,c;` |
|      - | 2617 | `	/* PHP requires exactly one argument. */` |
|    217 | 2618 | `	if( nArg != 1 ){` |
|    ! 0 | 2619 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2620 | `			"ArgumentCountError",` |
|      - | 2621 | `			"ord() expects exactly 1 argument, %d given",` |
|    ! 0 | 2622 | `			nArg` |
|      - | 2623 | `			);` |
|      - | 2624 | `	}` |
|      - | 2625 | `	/* php only DEPRECATES null here; PHL rejects it. */` |
|    217 | 2626 | `	if( ph7_value_is_null(apArg[0]) ){` |
|    ! 0 | 2627 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 2628 | `			"ord(): Argument #1 ($character) must be of type string, null given"` |
|      - | 2629 | `			);` |
|      - | 2630 | `	}` |
|      - | 2631 | `	/* Extract the target string */` |
|    217 | 2632 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    217 | 2633 | `	if( nLen < 1 ){` |
|      - | 2634 | `		/* php only DEPRECATES an empty string here; PHL rejects it. */` |
|      3 | 2635 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2636 | `			"ord(): Argument #1 ($character) must not be empty"` |
|      - | 2637 | `			);` |
|      - | 2638 | `	}` |
|      - | 2639 | `	/* A string longer than one byte: php DEPRECATES it; PHL rejects it. */` |
|    215 | 2640 | `	if( nLen > 1 ){` |
|      3 | 2641 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2642 | `			"ord(): Argument #1 ($character) must be a single byte, use ord($str[0]) instead"` |
|      - | 2643 | `			);` |
|      - | 2644 | `	}` |
|      - | 2645 | `	/* Extract the ASCII value of the first character */` |
|    213 | 2646 | `	c = (unsigned char)zString[0];` |
|      - | 2647 | `	/* Return that value */` |
|    213 | 2648 | `	ph7_result_int(pCtx,c);` |
|    213 | 2649 | `	return PH7_OK;` |
|    110 | 2650 | `}` |
|      - | 2651 | `/*` |
|      - | 2652 | ` * string chr(int $codepoint)` |
|      - | 2653 | ` *  Returns a one-character string containing the character specified` |
|      - | 2654 | ` *  by the given codepoint, which must be in the [0, 255] range.` |
|      - | 2655 | ` * Parameters` |
|      - | 2656 | ` *  $codepoint` |
|      - | 2657 | ` *   An integer codepoint in [0, 255]. php merely deprecates values` |
|      - | 2658 | ` *   outside that range (constraining them with % 256); PHL rejects` |
|      - | 2659 | ` *   them with a ValueError (scope policy).` |
|      - | 2660 | ` * Returns` |
|      - | 2661 | ` *  A single-character string.` |
|      - | 2662 | ` */` |
|   7150 | 2663 | `PH7_PRIVATE int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2664 | `{` |
|      - | 2665 | `	int c;` |
|      - | 2666 | `	unsigned char ch;` |
|      - | 2667 | `	/* PHP requires exactly one argument. */` |
|   7154 | 2668 | `	if( nArg != 1 ){` |
|    ! 0 | 2669 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2670 | `			"ArgumentCountError",` |
|      - | 2671 | `			"chr() expects exactly 1 argument, %d given",` |
|    ! 0 | 2672 | `			nArg` |
|      - | 2673 | `			);` |
|      - | 2674 | `	}` |
|      - | 2675 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 2676 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 2677 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 2678 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|   7154 | 2679 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      3 | 2680 | `		double d = ph7_value_to_double(apArg[0]);` |
|      3 | 2681 | `		if( d != (double)(sxi64)d ){` |
|      - | 2682 | `			/* php only DEPRECATES a lossy float->int here; PHL rejects it. */` |
|      3 | 2683 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 2684 | `				"chr(): Argument #1 ($codepoint) must be of type int, float given");` |
|      - | 2685 | `		}` |
|    ! 0 | 2686 | `	}` |
|      - | 2687 | `	/* Extract the codepoint. */` |
|   7152 | 2688 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 2689 | `	/* php only DEPRECATES an out-of-range codepoint (constraining it with % 256);` |
|      - | 2690 | `	 * PHL targets php's non-deprecated surface and rejects it loudly, matching the` |
|      - | 2691 | `	 * lossy-float branch above. This was the last engine site still emitting` |
|      - | 2692 | `	 * E_DEPRECATED — the scope policy says none remain. */` |
|   7152 | 2693 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 2694 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2695 | `			"chr(): Argument #1 ($codepoint) must be between 0 and 255");` |
|      - | 2696 | `	}` |
|      - | 2697 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 2698 | `	 * when taking the address of a wider int. */` |
|   7148 | 2699 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 2700 | `	/* Return the specified character */` |
|   7148 | 2701 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|   7148 | 2702 | `	return PH7_OK;` |
|   3579 | 2703 | `}` |
|      - | 2704 | `/*` |
|      - | 2705 | ` * Binary to hex consumer callback.` |
|      - | 2706 | ` * This callback is the default consumer used by the hash functions` |
|      - | 2707 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 2708 | ` */` |
|   3170 | 2709 | `PH7_PRIVATE int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      3 | 2710 | `{` |
|      - | 2711 | `	/* Append hex chunk verbatim */` |
|   3173 | 2712 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   3173 | 2713 | `	return SXRET_OK;` |
|      3 | 2714 | `}` |
|      - | 2715 |  |
|      - | 2716 | `/*` |
|      - | 2717 | ` * string bin2hex(string $str)` |
|      - | 2718 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 2719 | ` * Parameters` |
|      - | 2720 | ` *  $str` |
|      - | 2721 | ` *   The input string.` |
|      - | 2722 | ` * Returns.` |
|      - | 2723 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 2724 | ` */` |
|    150 | 2725 | `PH7_PRIVATE int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2726 | `{` |
|      - | 2727 | `	const char *zString;` |
|      - | 2728 | `	int nLen;` |
|      - | 2729 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|    153 | 2730 | `	if( nArg != 1 ){` |
|    ! 0 | 2731 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2732 | `			"ArgumentCountError",` |
|      - | 2733 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|    ! 0 | 2734 | `			nArg` |
|      - | 2735 | `			);` |
|      - | 2736 | `	}` |
|      - | 2737 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 2738 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 2739 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 2740 | `	 */` |
|    228 | 2741 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|     75 | 2742 | `		( ph7_value_is_object(apArg[0]) &&` |
|    ! 0 | 2743 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|    ! 0 | 2744 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|    ! 0 | 2745 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 2746 | `		)` |
|      - | 2747 | `	){` |
|    ! 0 | 2748 | `		const char *zType = ph7_type_name(apArg[0]);` |
|    ! 0 | 2749 | `		if( ph7_value_is_object(apArg[0]) ){` |
|    ! 0 | 2750 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    ! 0 | 2751 | `			if( pInst && pInst->pClass ){` |
|    ! 0 | 2752 | `				zType = SyStringData(&pInst->pClass->sName);` |
|    ! 0 | 2753 | `			}` |
|    ! 0 | 2754 | `		}` |
|    ! 0 | 2755 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2756 | `			"TypeError",` |
|      - | 2757 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 2758 | `			zType` |
|      - | 2759 | `			);` |
|      - | 2760 | `	}` |
|      - | 2761 | `	/* Extract the target string */` |
|    153 | 2762 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    153 | 2763 | `	if( nLen < 1 ){` |
|      - | 2764 | `		/* Empty string,return */` |
|     13 | 2765 | `		ph7_result_string(pCtx,"",0);` |
|     13 | 2766 | `		return PH7_OK;` |
|      - | 2767 | `	}` |
|      - | 2768 | `	/* Perform the requested operation */` |
|    141 | 2769 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|    141 | 2770 | `	return PH7_OK;` |
|     78 | 2771 | `}` |
|      - | 2772 |  |
|      - | 2773 | `/* Search callback signature */` |
|      - | 2774 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 2775 | `/*` |
|      - | 2776 | ` * Case-insensitive pattern match.` |
|      - | 2777 | ` * Brute force is the default search method used here.` |
|      - | 2778 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 2779 | ` * well for short/medium texts on modern hardware.` |
|      - | 2780 | ` */` |
|    262 | 2781 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      2 | 2782 | `{` |
|    264 | 2783 | `	const char *zpIn = (const char *)pPattern;` |
|    264 | 2784 | `	const char *zIn = (const char *)pText;` |
|    264 | 2785 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    264 | 2786 | `	const char *zEnd = &zIn[nLen];` |
|      - | 2787 | `	const char *zPtr,*zPtr2;` |
|      - | 2788 | `	int c,d;` |
|    264 | 2789 | `	if( iPatLen > nLen ){` |
|      - | 2790 | `		/* Don't bother processing */` |
|     35 | 2791 | `		return SXERR_NOTFOUND;` |
|      - | 2792 | `	}` |
|    772 | 2793 | `	for(;;){` |
|   1546 | 2794 | `		if( zIn >= zEnd ){` |
|    184 | 2795 | `			break;` |
|      - | 2796 | `		}` |
|   1364 | 2797 | `		c = SyToLower(zIn[0]);` |
|   1364 | 2798 | `		d = SyToLower(zpIn[0]);` |
|   1364 | 2799 | `		if( c == d ){` |
|    188 | 2800 | `			zPtr   = &zIn[1];` |
|    188 | 2801 | `			zPtr2  = &zpIn[1];` |
|    144 | 2802 | `			for(;;){` |
|    290 | 2803 | `				if( zPtr2 >= zpEnd ){` |
|      - | 2804 | `					/* Pattern found */` |
|     47 | 2805 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     47 | 2806 | `					return SXRET_OK;` |
|      - | 2807 | `				}` |
|    244 | 2808 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 2809 | `					break;` |
|      - | 2810 | `				}` |
|    244 | 2811 | `				c = SyToLower(zPtr[0]);` |
|    244 | 2812 | `				d = SyToLower(zPtr2[0]);` |
|    244 | 2813 | `				if( c != d ){` |
|    142 | 2814 | `					break;` |
|      - | 2815 | `				}` |
|    103 | 2816 | `				zPtr++; zPtr2++;` |
|      1 | 2817 | `			}` |
|     70 | 2818 | `		}` |
|   1318 | 2819 | `		zIn++;` |
|      2 | 2820 | `	}` |
|      - | 2821 | `	/* Pattern not found */` |
|    184 | 2822 | `	return SXERR_NOTFOUND;` |
|    133 | 2823 | `}` |
|      - | 2824 | `/*` |
|      - | 2825 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2826 | ` *  Find the first occurrence of a string.` |
|      - | 2827 | ` * Parameters` |
|      - | 2828 | ` *  $haystack` |
|      - | 2829 | ` *   The input string.` |
|      - | 2830 | ` * $needle` |
|      - | 2831 | ` *   Search pattern (must be a string).` |
|      - | 2832 | ` * $before_needle` |
|      - | 2833 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2834 | ` *   of the needle (excluding the needle).` |
|      - | 2835 | ` * Return` |
|      - | 2836 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2837 | ` */` |
|     12 | 2838 | `PH7_PRIVATE int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2839 | `{` |
|     13 | 2840 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2841 | `	const char *zBlob,*zPattern;` |
|      - | 2842 | `	int nLen,nPatLen;` |
|      - | 2843 | `	sxu32 nOfft;` |
|      - | 2844 | `	sxi32 rc;` |
|     13 | 2845 | `	if( nArg < 2 ){` |
|      - | 2846 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2847 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2848 | `		return PH7_OK;` |
|      - | 2849 | `	}` |
|      - | 2850 | `	/* Extract the needle and the haystack */` |
|     13 | 2851 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 2852 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     13 | 2853 | `	nOfft = 0; /* cc warning */` |
|     13 | 2854 | `	if( nPatLen < 1 ){` |
|      - | 2855 | `		/* php 8: the empty needle matches at position 0, so the whole haystack` |
|      - | 2856 | `		 * is returned (and nothing at all when $before_needle is set). */` |
|      7 | 2857 | `		if( nArg > 2 && ph7_value_to_bool(apArg[2]) ){` |
|      3 | 2858 | `			ph7_result_string(pCtx,"",0);` |
|      2 | 2859 | `		}else{` |
|      5 | 2860 | `			ph7_result_string(pCtx,zBlob,nLen);` |
|      - | 2861 | `		}` |
|      7 | 2862 | `		return PH7_OK;` |
|      - | 2863 | `	}` |
|      7 | 2864 | `	if( nLen > 0 ){` |
|      7 | 2865 | `		int before = 0;` |
|      - | 2866 | `		/* Perform the lookup */` |
|      7 | 2867 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      7 | 2868 | `		if( rc != SXRET_OK ){` |
|      - | 2869 | `			/* Pattern not found,return FALSE */` |
|      3 | 2870 | `			ph7_result_bool(pCtx,0);` |
|      3 | 2871 | `			return PH7_OK;` |
|      - | 2872 | `		}` |
|      - | 2873 | `		/* Return the portion of the string */` |
|      5 | 2874 | `		if( nArg > 2 ){` |
|      3 | 2875 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2876 | `		}` |
|      5 | 2877 | `		if( before ){` |
|      3 | 2878 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2879 | `		}else{` |
|      3 | 2880 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2881 | `		}` |
|      3 | 2882 | `	}else{` |
|    ! 0 | 2883 | `		ph7_result_bool(pCtx,0);` |
|      - | 2884 | `	}` |
|      5 | 2885 | `	return PH7_OK;` |
|      7 | 2886 | `}` |
|      - | 2887 | `/*` |
|      - | 2888 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2889 | ` *  Case-insensitive strstr().` |
|      - | 2890 | ` * Parameters` |
|      - | 2891 | ` *  $haystack` |
|      - | 2892 | ` *   The input string.` |
|      - | 2893 | ` * $needle` |
|      - | 2894 | ` *   Search pattern (must be a string).` |
|      - | 2895 | ` * $before_needle` |
|      - | 2896 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2897 | ` *   of the needle (excluding the needle).` |
|      - | 2898 | ` * Return` |
|      - | 2899 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2900 | ` */` |
|      6 | 2901 | `PH7_PRIVATE int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2902 | `{` |
|      7 | 2903 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2904 | `	const char *zBlob,*zPattern;` |
|      - | 2905 | `	int nLen,nPatLen;` |
|      - | 2906 | `	sxu32 nOfft;` |
|      - | 2907 | `	sxi32 rc;` |
|      7 | 2908 | `	if( nArg < 2 ){` |
|      - | 2909 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2910 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2911 | `		return PH7_OK;` |
|      - | 2912 | `	}` |
|      - | 2913 | `	/* Extract the needle and the haystack */` |
|      7 | 2914 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 2915 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 2916 | `	nOfft = 0; /* cc warning */` |
|      7 | 2917 | `	if( nPatLen < 1 ){` |
|      - | 2918 | `		/* php 8: the empty needle matches at position 0, so the whole haystack` |
|      - | 2919 | `		 * is returned (and nothing at all when $before_needle is set). */` |
|      3 | 2920 | `		if( nArg > 2 && ph7_value_to_bool(apArg[2]) ){` |
|    ! 0 | 2921 | `			ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2922 | `		}else{` |
|      3 | 2923 | `			ph7_result_string(pCtx,zBlob,nLen);` |
|      - | 2924 | `		}` |
|      3 | 2925 | `		return PH7_OK;` |
|      - | 2926 | `	}` |
|      5 | 2927 | `	if( nLen > 0 ){` |
|      5 | 2928 | `		int before = 0;` |
|      - | 2929 | `		/* Perform the lookup */` |
|      5 | 2930 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2931 | `		if( rc != SXRET_OK ){` |
|      - | 2932 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2933 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2934 | `			return PH7_OK;` |
|      - | 2935 | `		}` |
|      - | 2936 | `		/* Return the portion of the string */` |
|      5 | 2937 | `		if( nArg > 2 ){` |
|      3 | 2938 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2939 | `		}` |
|      5 | 2940 | `		if( before ){` |
|      3 | 2941 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2942 | `		}else{` |
|      3 | 2943 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2944 | `		}` |
|      3 | 2945 | `	}else{` |
|    ! 0 | 2946 | `		ph7_result_bool(pCtx,0);` |
|      - | 2947 | `	}` |
|      5 | 2948 | `	return PH7_OK;` |
|      4 | 2949 | `}` |
|      - | 2950 | `/*` |
|      - | 2951 | ` * Resolve the $offset argument shared by strpos()/stripos().` |
|      - | 2952 | ` *` |
|      - | 2953 | ` * php requires -strlen($haystack) <= $offset <= strlen($haystack) and throws` |
|      - | 2954 | ` * ValueError otherwise; a negative offset counts back from the end. PHL used to` |
|      - | 2955 | ` * negate a negative offset and silently clamp an out-of-range one to zero, so` |
|      - | 2956 | ` * strpos("Hello","l",100) answered 2 where php raises — an argument error` |
|      - | 2957 | ` * turned into a wrong answer.` |
|      - | 2958 | ` *` |
|      - | 2959 | ` * On success *pnStart receives the resolved non-negative offset.` |
|      - | 2960 | ` */` |
|     24 | 2961 | `static sxi32 StrSearchOffset(` |
|      - | 2962 | `	ph7_context *pCtx,` |
|      - | 2963 | `	ph7_value *pArg,` |
|      - | 2964 | `	int nLen,` |
|      - | 2965 | `	const char *zFunc,` |
|      - | 2966 | `	int *pnStart` |
|      - | 2967 | `	)` |
|      2 | 2968 | `{` |
|     26 | 2969 | `	ph7_int64 iOfft = ph7_value_to_int64(pArg);` |
|      - | 2970 | `	/* Compare without negating iOfft: -INT64_MIN would overflow. */` |
|     26 | 2971 | `	if( iOfft < 0 ? (iOfft < -(ph7_int64)nLen) : (iOfft > (ph7_int64)nLen) ){` |
|      8 | 2972 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      4 | 2973 | `			"%s(): Argument #3 ($offset) must be contained in argument #1 ($haystack)",zFunc);` |
|      - | 2974 | `	}` |
|     26 | 2975 | `	*pnStart = (int)(iOfft < 0 ? (ph7_int64)nLen + iOfft : iOfft);` |
|     26 | 2976 | `	return PH7_OK;` |
|     18 | 2977 | `}` |
|      - | 2978 | `/*` |
|      - | 2979 | ` * Resolve the window of match START positions for strrpos()/strripos().` |
|      - | 2980 | ` *` |
|      - | 2981 | ` * php's rule is asymmetric in the sign of $offset: a non-negative offset is a` |
|      - | 2982 | ` * LOWER bound on where the match may start, while a negative one is an UPPER` |
|      - | 2983 | ` * bound counted back from the end of the haystack (zend_memnrstr). The range` |
|      - | 2984 | ` * check is the same as StrSearchOffset()'s.` |
|      - | 2985 | ` *` |
|      - | 2986 | ` * On success the closed interval [*pnMin,*pnMax] holds every position at which` |
|      - | 2987 | ` * a match is allowed to begin; it is empty (max < min) when the needle cannot` |
|      - | 2988 | ` * fit, which the caller reports as FALSE.` |
|      - | 2989 | ` */` |
|     98 | 2990 | `static sxi32 StrRSearchWindow(` |
|      - | 2991 | `	ph7_context *pCtx,` |
|      - | 2992 | `	ph7_value *pArg, /* The $offset argument, or NULL when it was omitted */` |
|      - | 2993 | `	int nLen,` |
|      - | 2994 | `	int nPatLen,` |
|      - | 2995 | `	const char *zFunc,` |
|      - | 2996 | `	int *pnMin,` |
|      - | 2997 | `	int *pnMax` |
|      - | 2998 | `	)` |
|      1 | 2999 | `{` |
|     99 | 3000 | `	int nMin = 0;` |
|     99 | 3001 | `	int nMax = nLen - nPatLen;` |
|     99 | 3002 | `	if( pArg ){` |
|     47 | 3003 | `		ph7_int64 iOfft = ph7_value_to_int64(pArg);` |
|     47 | 3004 | `		if( iOfft < 0 ? (iOfft < -(ph7_int64)nLen) : (iOfft > (ph7_int64)nLen) ){` |
|     33 | 3005 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|     14 | 3006 | `				"%s(): Argument #3 ($offset) must be contained in argument #1 ($haystack)",zFunc);` |
|      - | 3007 | `		}` |
|     29 | 3008 | `		if( iOfft < 0 ){` |
|     15 | 3009 | `			int nLimit = nLen + (int)iOfft;` |
|     15 | 3010 | `			if( nMax > nLimit ){` |
|     15 | 3011 | `				nMax = nLimit;` |
|      7 | 3012 | `			}` |
|      8 | 3013 | `		}else{` |
|     15 | 3014 | `			nMin = (int)iOfft;` |
|      - | 3015 | `		}` |
|     14 | 3016 | `	}` |
|     81 | 3017 | `	*pnMin = nMin;` |
|     81 | 3018 | `	*pnMax = nMax;` |
|     81 | 3019 | `	return PH7_OK;` |
|     45 | 3020 | `}` |
|      - | 3021 | `/*` |
|      - | 3022 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3023 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 3024 | ` * Parameters` |
|      - | 3025 | ` *  $haystack` |
|      - | 3026 | ` *   The input string.` |
|      - | 3027 | ` * $needle` |
|      - | 3028 | ` *   Search pattern (must be a string).` |
|      - | 3029 | ` * $offset` |
|      - | 3030 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3031 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3032 | ` *   of haystack.` |
|      - | 3033 | ` * Return` |
|      - | 3034 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3035 | ` */` |
|   1602 | 3036 | `PH7_PRIVATE int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3037 | `{` |
|   1607 | 3038 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strpos",1,"$haystack"); }` |
|   1607 | 3039 | `	if( nArg > 1 ){ StrNullArgNotice(pCtx,apArg[1],"strpos",2,"$needle"); }` |
|   1607 | 3040 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3041 | `	const char *zBlob,*zPattern;` |
|      - | 3042 | `	int nLen,nPatLen,nStart;` |
|      - | 3043 | `	sxu32 nOfft;` |
|      - | 3044 | `	sxi32 rc;` |
|   1607 | 3045 | `	if( nArg < 2 ){` |
|      - | 3046 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3047 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3048 | `		return PH7_OK;` |
|      - | 3049 | `	}` |
|      - | 3050 | `	/* Extract the needle and the haystack */` |
|   1607 | 3051 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|   1607 | 3052 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|   1607 | 3053 | `	nOfft = 0; /* cc warning */` |
|   1607 | 3054 | `	nStart = 0;` |
|      - | 3055 | `	/* Peek the starting offset if available */` |
|   1607 | 3056 | `	if( nArg > 2 ){` |
|     22 | 3057 | `		rc = StrSearchOffset(pCtx,apArg[2],nLen,"strpos",&nStart);` |
|     22 | 3058 | `		if( rc != PH7_OK ){` |
|    ! 0 | 3059 | `			return rc;` |
|      - | 3060 | `		}` |
|     10 | 3061 | `	}` |
|   1607 | 3062 | `	if( nPatLen < 1 ){` |
|      - | 3063 | `		/* php 8 treats the empty needle as matching at the search offset. */` |
|     11 | 3064 | `		ph7_result_int64(pCtx,(ph7_int64)nStart);` |
|     11 | 3065 | `		return PH7_OK;` |
|      - | 3066 | `	}` |
|   1597 | 3067 | `	zBlob += nStart;` |
|   1597 | 3068 | `	nLen -= nStart;` |
|   1597 | 3069 | `	if( nLen > 0 ){` |
|      - | 3070 | `		/* Perform the lookup */` |
|   1595 | 3071 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|   1595 | 3072 | `		if( rc != SXRET_OK ){` |
|      - | 3073 | `			/* Pattern not found,return FALSE */` |
|    799 | 3074 | `			ph7_result_bool(pCtx,0);` |
|    799 | 3075 | `			return PH7_OK;` |
|      - | 3076 | `		}` |
|      - | 3077 | `		/* Return the pattern position */` |
|    801 | 3078 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|    403 | 3079 | `	}else{` |
|      3 | 3080 | `		ph7_result_bool(pCtx,0);` |
|      - | 3081 | `	}` |
|    803 | 3082 | `	return PH7_OK;` |
|    806 | 3083 | `}` |
|      - | 3084 | `/*` |
|      - | 3085 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|      - | 3086 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|      - | 3087 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|      - | 3088 | ` * TypeError for arrays, resources, and objects without __toString.` |
|      - | 3089 | ` *` |
|      - | 3090 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|      - | 3091 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|      - | 3092 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|      - | 3093 | ` *` |
|      - | 3094 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|      - | 3095 | ` * is valid until pTmp is released or pArg is mutated.` |
|      - | 3096 | ` */` |
|    644 | 3097 | `static sxi32 StrPredicateResolveArg(` |
|      - | 3098 | `	ph7_context *pCtx,` |
|      - | 3099 | `	ph7_value *pArg,` |
|      - | 3100 | `	const char *zFunc,` |
|      - | 3101 | `	int iArgNum,` |
|      - | 3102 | `	const char *zParamName,` |
|      - | 3103 | `	const char *zTypeStr, /* Declared type in the TypeError, e.g. "string" / "?string" */` |
|      - | 3104 | `	const char *zNullMsg,` |
|      - | 3105 | `	ph7_value *pTmp,` |
|      - | 3106 | `	const char **pzOut,` |
|      - | 3107 | `	int *pnOut` |
|      2 | 3108 | `){` |
|    322 | 3109 | `	SXUNUSED(zNullMsg); /* php's deprecation text — PHL rejects null instead of coercing */` |
|    646 | 3110 | `	if( ph7_value_is_null(pArg) ){` |
|      - | 3111 | `		/* php only DEPRECATES null here; PHL rejects it with the TypeError php will` |
|      - | 3112 | `		 * eventually raise. */` |
|    ! 0 | 3113 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 3114 | `			"%s(): Argument #%d (%s) must be of type %s, null given",` |
|    ! 0 | 3115 | `			zFunc,iArgNum,zParamName,zTypeStr);` |
|      - | 3116 | `	}` |
|    992 | 3117 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    644 | 3118 | `	    ( ph7_value_is_object(pArg) &&` |
|     72 | 3119 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     48 | 3120 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     24 | 3121 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 3122 | `	    )` |
|      - | 3123 | `	){` |
|    ! 0 | 3124 | `		const char *zType = ph7_type_name(pArg);` |
|    ! 0 | 3125 | `		if( ph7_value_is_object(pArg) ){` |
|    ! 0 | 3126 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|    ! 0 | 3127 | `			if( pInst && pInst->pClass ){` |
|    ! 0 | 3128 | `				zType = SyStringData(&pInst->pClass->sName);` |
|    ! 0 | 3129 | `			}` |
|    ! 0 | 3130 | `		}` |
|    ! 0 | 3131 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3132 | `			"TypeError",` |
|      - | 3133 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|    ! 0 | 3134 | `			zFunc, iArgNum, zParamName, zTypeStr, zType` |
|      - | 3135 | `			);` |
|      - | 3136 | `	}` |
|    646 | 3137 | `	if( ph7_value_is_object(pArg) ){` |
|     49 | 3138 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     49 | 3139 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 3140 | `			"__toString",sizeof("__toString")-1);` |
|     49 | 3141 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|     49 | 3142 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|     49 | 3143 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|     49 | 3144 | `		return PH7_OK;` |
|      - | 3145 | `	}` |
|    598 | 3146 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|    598 | 3147 | `	return PH7_OK;` |
|    324 | 3148 | `}` |
|      - | 3149 | `/*` |
|      - | 3150 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 3151 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 3152 | ` * Return` |
|      - | 3153 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 3154 | ` */` |
|     84 | 3155 | `PH7_PRIVATE int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3156 | `{` |
|      - | 3157 | `	const char *zHaystack,*zNeedle;` |
|      - | 3158 | `	int nHayLen,nNeedleLen;` |
|      - | 3159 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3160 | `	sxi32 rc;` |
|     86 | 3161 | `	if( nArg != 2 ){` |
|    ! 0 | 3162 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3163 | `			"ArgumentCountError",` |
|      - | 3164 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|    ! 0 | 3165 | `			nArg` |
|      - | 3166 | `			);` |
|      - | 3167 | `	}` |
|     86 | 3168 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     86 | 3169 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     86 | 3170 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack","string",` |
|      - | 3171 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|      - | 3172 | `		"of type string is deprecated",` |
|      - | 3173 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     86 | 3174 | `	if( rc != PH7_OK ) goto out;` |
|     86 | 3175 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle","string",` |
|      - | 3176 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|      - | 3177 | `		"of type string is deprecated",` |
|      - | 3178 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     86 | 3179 | `	if( rc != PH7_OK ) goto out;` |
|     86 | 3180 | `	if( nNeedleLen < 1 ){` |
|     11 | 3181 | `		ph7_result_bool(pCtx,1);` |
|     81 | 3182 | `	}else if( nHayLen < nNeedleLen ){` |
|      7 | 3183 | `		ph7_result_bool(pCtx,0);` |
|      4 | 3184 | `	}else{` |
|    104 | 3185 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|     34 | 3186 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|     70 | 3187 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|      - | 3188 | `	}` |
|     86 | 3189 | `	rc = PH7_OK;` |
|     42 | 3190 | `out:` |
|     86 | 3191 | `	PH7_MemObjRelease(&sHayTmp);` |
|     86 | 3192 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     86 | 3193 | `	return rc;` |
|     44 | 3194 | `}` |
|      - | 3195 | `/*` |
|      - | 3196 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 3197 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 3198 | ` * Return` |
|      - | 3199 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 3200 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3201 | ` */` |
|     54 | 3202 | `PH7_PRIVATE int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3203 | `{` |
|      - | 3204 | `	const char *zHaystack,*zNeedle;` |
|      - | 3205 | `	int nHayLen,nNeedleLen;` |
|      - | 3206 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3207 | `	sxi32 rc;` |
|     55 | 3208 | `	if( nArg != 2 ){` |
|    ! 0 | 3209 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3210 | `			"ArgumentCountError",` |
|      - | 3211 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|    ! 0 | 3212 | `			nArg` |
|      - | 3213 | `			);` |
|      - | 3214 | `	}` |
|     55 | 3215 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     55 | 3216 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     55 | 3217 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack","string",` |
|      - | 3218 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3219 | `		"of type string is deprecated",` |
|      - | 3220 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     55 | 3221 | `	if( rc != PH7_OK ) goto out;` |
|     55 | 3222 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle","string",` |
|      - | 3223 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3224 | `		"of type string is deprecated",` |
|      - | 3225 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     55 | 3226 | `	if( rc != PH7_OK ) goto out;` |
|     55 | 3227 | `	if( nNeedleLen < 1 ){` |
|     11 | 3228 | `		ph7_result_bool(pCtx,1);` |
|     50 | 3229 | `	}else if( nHayLen < nNeedleLen ){` |
|      7 | 3230 | `		ph7_result_bool(pCtx,0);` |
|      4 | 3231 | `	}else{` |
|     58 | 3232 | `		ph7_result_bool(pCtx,` |
|     38 | 3233 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3234 | `	}` |
|     55 | 3235 | `	rc = PH7_OK;` |
|     27 | 3236 | `out:` |
|     55 | 3237 | `	PH7_MemObjRelease(&sHayTmp);` |
|     55 | 3238 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     55 | 3239 | `	return rc;` |
|     28 | 3240 | `}` |
|      - | 3241 | `/*` |
|      - | 3242 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 3243 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 3244 | ` * Return` |
|      - | 3245 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 3246 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3247 | ` */` |
|     54 | 3248 | `PH7_PRIVATE int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3249 | `{` |
|      - | 3250 | `	const char *zHaystack,*zNeedle;` |
|      - | 3251 | `	int nHayLen,nNeedleLen;` |
|      - | 3252 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3253 | `	sxi32 rc;` |
|     55 | 3254 | `	if( nArg != 2 ){` |
|    ! 0 | 3255 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3256 | `			"ArgumentCountError",` |
|      - | 3257 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|    ! 0 | 3258 | `			nArg` |
|      - | 3259 | `			);` |
|      - | 3260 | `	}` |
|     55 | 3261 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     55 | 3262 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     55 | 3263 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack","string",` |
|      - | 3264 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3265 | `		"of type string is deprecated",` |
|      - | 3266 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     55 | 3267 | `	if( rc != PH7_OK ) goto out;` |
|     55 | 3268 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle","string",` |
|      - | 3269 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3270 | `		"of type string is deprecated",` |
|      - | 3271 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     55 | 3272 | `	if( rc != PH7_OK ) goto out;` |
|     55 | 3273 | `	if( nNeedleLen < 1 ){` |
|     11 | 3274 | `		ph7_result_bool(pCtx,1);` |
|     50 | 3275 | `	}else if( nHayLen < nNeedleLen ){` |
|      7 | 3276 | `		ph7_result_bool(pCtx,0);` |
|      4 | 3277 | `	}else{` |
|     58 | 3278 | `		ph7_result_bool(pCtx,` |
|     38 | 3279 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3280 | `	}` |
|     55 | 3281 | `	rc = PH7_OK;` |
|     27 | 3282 | `out:` |
|     55 | 3283 | `	PH7_MemObjRelease(&sHayTmp);` |
|     55 | 3284 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     55 | 3285 | `	return rc;` |
|     28 | 3286 | `}` |
|      - | 3287 | `/*` |
|      - | 3288 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3289 | ` *  Case-insensitive strpos.` |
|      - | 3290 | ` * Parameters` |
|      - | 3291 | ` *  $haystack` |
|      - | 3292 | ` *   The input string.` |
|      - | 3293 | ` * $needle` |
|      - | 3294 | ` *   Search pattern (must be a string).` |
|      - | 3295 | ` * $offset` |
|      - | 3296 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3297 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3298 | ` *   of haystack.` |
|      - | 3299 | ` * Return` |
|      - | 3300 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3301 | ` */` |
|    198 | 3302 | `PH7_PRIVATE int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3303 | `{` |
|    200 | 3304 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3305 | `	const char *zBlob,*zPattern;` |
|      - | 3306 | `	int nLen,nPatLen,nStart;` |
|      - | 3307 | `	sxu32 nOfft;` |
|      - | 3308 | `	sxi32 rc;` |
|    200 | 3309 | `	if( nArg < 2 ){` |
|      - | 3310 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3311 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3312 | `		return PH7_OK;` |
|      - | 3313 | `	}` |
|      - | 3314 | `	/* Extract the needle and the haystack */` |
|    200 | 3315 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    200 | 3316 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    200 | 3317 | `	nOfft = 0; /* cc warning */` |
|    200 | 3318 | `	nStart = 0;` |
|      - | 3319 | `	/* Peek the starting offset if available */` |
|    200 | 3320 | `	if( nArg > 2 ){` |
|      5 | 3321 | `		rc = StrSearchOffset(pCtx,apArg[2],nLen,"stripos",&nStart);` |
|      5 | 3322 | `		if( rc != PH7_OK ){` |
|    ! 0 | 3323 | `			return rc;` |
|      - | 3324 | `		}` |
|      2 | 3325 | `	}` |
|    200 | 3326 | `	if( nPatLen < 1 ){` |
|      - | 3327 | `		/* php 8 treats the empty needle as matching at the search offset. */` |
|      3 | 3328 | `		ph7_result_int64(pCtx,(ph7_int64)nStart);` |
|      3 | 3329 | `		return PH7_OK;` |
|      - | 3330 | `	}` |
|    198 | 3331 | `	zBlob += nStart;` |
|    198 | 3332 | `	nLen -= nStart;` |
|    198 | 3333 | `	if( nLen > 0 ){` |
|      - | 3334 | `		/* Perform the lookup */` |
|    198 | 3335 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    198 | 3336 | `		if( rc != SXRET_OK ){` |
|      - | 3337 | `			/* Pattern not found,return FALSE */` |
|    184 | 3338 | `			ph7_result_bool(pCtx,0);` |
|    184 | 3339 | `			return PH7_OK;` |
|      - | 3340 | `		}` |
|      - | 3341 | `		/* Return the pattern position */` |
|     15 | 3342 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 3343 | `	}else{` |
|    ! 0 | 3344 | `		ph7_result_bool(pCtx,0);` |
|      - | 3345 | `	}` |
|     15 | 3346 | `	return PH7_OK;` |
|    101 | 3347 | `}` |
|      - | 3348 | `/*` |
|      - | 3349 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3350 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 3351 | ` * Parameters` |
|      - | 3352 | ` *  $haystack` |
|      - | 3353 | ` *   The input string.` |
|      - | 3354 | ` * $needle` |
|      - | 3355 | ` *   Search pattern (must be a string).` |
|      - | 3356 | ` * $offset` |
|      - | 3357 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3358 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3359 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3360 | ` * Return` |
|      - | 3361 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3362 | ` */` |
|     54 | 3363 | `PH7_PRIVATE int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3364 | `{` |
|      - | 3365 | `	const char *zBlob,*zPattern;` |
|     55 | 3366 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3367 | `	int nLen,nPatLen,i;` |
|     55 | 3368 | `	int nMin = 0,nMax = 0;` |
|      - | 3369 | `	sxu32 nOfft;` |
|      - | 3370 | `	sxi32 rc;` |
|     55 | 3371 | `	if( nArg < 2 ){` |
|      - | 3372 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3373 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3374 | `		return PH7_OK;` |
|      - | 3375 | `	}` |
|      - | 3376 | `	/* Extract the needle and the haystack */` |
|     55 | 3377 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     55 | 3378 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     55 | 3379 | `	nOfft = 0; /* cc warning */` |
|      - | 3380 | `	/* Resolve the range of positions the match may start at */` |
|     55 | 3381 | `	rc = StrRSearchWindow(pCtx,nArg > 2 ? apArg[2] : 0,nLen,nPatLen,"strrpos",&nMin,&nMax);` |
|     55 | 3382 | `	if( rc != PH7_OK ){` |
|      5 | 3383 | `		return rc;` |
|      - | 3384 | `	}` |
|     51 | 3385 | `	if( nPatLen < 1 ){` |
|      - | 3386 | `		/* php 8: the empty needle matches everywhere, so the LAST match is the` |
|      - | 3387 | `		 * highest position the window allows. */` |
|     11 | 3388 | `		ph7_result_int64(pCtx,(ph7_int64)nMax);` |
|     11 | 3389 | `		return PH7_OK;` |
|      - | 3390 | `	}` |
|      - | 3391 | `	/* Walk backwards, comparing at each candidate position. Searching a window` |
|      - | 3392 | `	 * exactly as long as the needle makes the match test an equality test while` |
|      - | 3393 | `	 * still going through xPatternMatch, which carries the case folding. */` |
|    217 | 3394 | `	for( i = nMax ; i >= nMin ; --i ){` |
|    203 | 3395 | `		rc = xPatternMatch((const void *)&zBlob[i],(sxu32)nPatLen,(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    203 | 3396 | `		if( rc == SXRET_OK ){` |
|      - | 3397 | `			/* Pattern found,return it's position */` |
|     27 | 3398 | `			ph7_result_int64(pCtx,(ph7_int64)i);` |
|     27 | 3399 | `			return PH7_OK;` |
|      - | 3400 | `		}` |
|     89 | 3401 | `	}` |
|      - | 3402 | `	/* Pattern not found,return FALSE */` |
|     15 | 3403 | `	ph7_result_bool(pCtx,0);` |
|     15 | 3404 | `	return PH7_OK;` |
|     28 | 3405 | `}` |
|      - | 3406 | `/*` |
|      - | 3407 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3408 | ` *  Case-insensitive strrpos.` |
|      - | 3409 | ` * Parameters` |
|      - | 3410 | ` *  $haystack` |
|      - | 3411 | ` *   The input string.` |
|      - | 3412 | ` * $needle` |
|      - | 3413 | ` *   Search pattern (must be a string).` |
|      - | 3414 | ` * $offset` |
|      - | 3415 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3416 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3417 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3418 | ` * Return` |
|      - | 3419 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3420 | ` */` |
|     34 | 3421 | `PH7_PRIVATE int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3422 | `{` |
|      - | 3423 | `	const char *zBlob,*zPattern;` |
|     35 | 3424 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3425 | `	int nLen,nPatLen,i;` |
|     35 | 3426 | `	int nMin = 0,nMax = 0;` |
|      - | 3427 | `	sxu32 nOfft;` |
|      - | 3428 | `	sxi32 rc;` |
|     35 | 3429 | `	if( nArg < 2 ){` |
|      - | 3430 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3431 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3432 | `		return PH7_OK;` |
|      - | 3433 | `	}` |
|      - | 3434 | `	/* Extract the needle and the haystack */` |
|     35 | 3435 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     35 | 3436 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     35 | 3437 | `	nOfft = 0; /* cc warning */` |
|      - | 3438 | `	/* Resolve the range of positions the match may start at */` |
|     35 | 3439 | `	rc = StrRSearchWindow(pCtx,nArg > 2 ? apArg[2] : 0,nLen,nPatLen,"strripos",&nMin,&nMax);` |
|     35 | 3440 | `	if( rc != PH7_OK ){` |
|      5 | 3441 | `		return rc;` |
|      - | 3442 | `	}` |
|     31 | 3443 | `	if( nPatLen < 1 ){` |
|      - | 3444 | `		/* php 8: the empty needle matches everywhere, so the LAST match is the` |
|      - | 3445 | `		 * highest position the window allows. */` |
|     11 | 3446 | `		ph7_result_int64(pCtx,(ph7_int64)nMax);` |
|     11 | 3447 | `		return PH7_OK;` |
|      - | 3448 | `	}` |
|      - | 3449 | `	/* Walk backwards, comparing at each candidate position (see strrpos). */` |
|     49 | 3450 | `	for( i = nMax ; i >= nMin ; --i ){` |
|     45 | 3451 | `		rc = xPatternMatch((const void *)&zBlob[i],(sxu32)nPatLen,(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     45 | 3452 | `		if( rc == SXRET_OK ){` |
|      - | 3453 | `			/* Pattern found,return it's position */` |
|     17 | 3454 | `			ph7_result_int64(pCtx,(ph7_int64)i);` |
|     17 | 3455 | `			return PH7_OK;` |
|      - | 3456 | `		}` |
|     15 | 3457 | `	}` |
|      - | 3458 | `	/* Pattern not found,return FALSE */` |
|      5 | 3459 | `	ph7_result_bool(pCtx,0);` |
|      5 | 3460 | `	return PH7_OK;` |
|     18 | 3461 | `}` |
|      - | 3462 | `/*` |
|      - | 3463 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3464 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3465 | ` * Parameters` |
|      - | 3466 | ` *  $haystack` |
|      - | 3467 | ` *   The input string.` |
|      - | 3468 | ` * $needle` |
|      - | 3469 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3470 | ` *  This behavior is different from that of strstr().` |
|      - | 3471 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3472 | ` *  as the ordinal value of a character.` |
|      - | 3473 | ` * Return` |
|      - | 3474 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3475 | ` */` |
|     24 | 3476 | `PH7_PRIVATE int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3477 | `{` |
|      - | 3478 | `	const char *zBlob;` |
|      - | 3479 | `	int nLen,c;` |
|     25 | 3480 | `	if( nArg < 2 ){` |
|      - | 3481 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3482 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3483 | `		return PH7_OK;` |
|      - | 3484 | `	}` |
|      - | 3485 | `	/* Extract the haystack */` |
|     25 | 3486 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 3487 | `	c = 0; /* cc warning */` |
|     25 | 3488 | `	if( nLen > 0 ){` |
|      - | 3489 | `		const char *zPattern;` |
|      - | 3490 | `		int nPatLen;` |
|      - | 3491 | `		sxu32 nOfft;` |
|      - | 3492 | `		sxi32 rc;` |
|      - | 3493 | `		/* php 8 casts the needle to string and uses only its first character.` |
|      - | 3494 | `		 * The old "if not a string, take it as an ordinal" reading was php 7` |
|      - | 3495 | `		 * behaviour, removed in php 8: strrchr("hello world",111) now looks for` |
|      - | 3496 | `		 * "1", not "o". An empty needle matches nothing. */` |
|     23 | 3497 | `		zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     23 | 3498 | `		if( nPatLen < 1 ){` |
|      3 | 3499 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3500 | `			return PH7_OK;` |
|      - | 3501 | `		}` |
|     21 | 3502 | `		c = zPattern[0];` |
|      - | 3503 | `		/* Perform the lookup */` |
|     21 | 3504 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3505 | `		if( rc != SXRET_OK ){` |
|      - | 3506 | `			/* No such entry,return FALSE */` |
|      9 | 3507 | `			ph7_result_bool(pCtx,0);` |
|      9 | 3508 | `			return PH7_OK;` |
|      - | 3509 | `		}` |
|      - | 3510 | `		/* Return the string portion */` |
|     13 | 3511 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      7 | 3512 | `	}else{` |
|      3 | 3513 | `		ph7_result_bool(pCtx,0);` |
|      - | 3514 | `	}` |
|     15 | 3515 | `	return PH7_OK;` |
|     13 | 3516 | `}` |
|      - | 3517 | `/*` |
|      - | 3518 | ` * string strrev(string $string)` |
|      - | 3519 | ` *  Reverse a string.` |
|      - | 3520 | ` * Parameters` |
|      - | 3521 | ` *  $string` |
|      - | 3522 | ` *   String to be reversed.` |
|      - | 3523 | ` * Return` |
|      - | 3524 | ` *  The reversed string.` |
|      - | 3525 | ` */` |
|      2 | 3526 | `PH7_PRIVATE int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3527 | `{` |
|      - | 3528 | `	const char *zIn,*zEnd;` |
|      - | 3529 | `	int nLen,c;` |
|      3 | 3530 | `	if( nArg < 1 ){` |
|      - | 3531 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3532 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3533 | `		return PH7_OK;` |
|      - | 3534 | `	}` |
|      - | 3535 | `	/* Extract the target string */` |
|      3 | 3536 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3537 | `	if( nLen < 1 ){` |
|      - | 3538 | `		/* Empty string Return null */` |
|    ! 0 | 3539 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3540 | `		return PH7_OK;` |
|      - | 3541 | `	}` |
|      - | 3542 | `	/* Perform the requested operation */` |
|      3 | 3543 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3544 | `	for(;;){` |
|      9 | 3545 | `		if( zEnd < zIn ){` |
|      - | 3546 | `			/* No more input to process */` |
|      3 | 3547 | `			break;` |
|      - | 3548 | `		}` |
|      - | 3549 | `		/* Append current character */` |
|      7 | 3550 | `		c = zEnd[0];` |
|      7 | 3551 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3552 | `		zEnd--;` |
|      1 | 3553 | `	}` |
|      3 | 3554 | `	return PH7_OK;` |
|      2 | 3555 | `}` |
|      - | 3556 | `/*` |
|      - | 3557 | ` * string ucwords(string $string [, string $separators = " \t\r\n\f\v"])` |
|      - | 3558 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3559 | ` *  A word begins at the start of the string and after any character present in` |
|      - | 3560 | ` *  $separators. The default separators are the whitespace characters (space,` |
|      - | 3561 | ` *  horizontal tab, carriage return, newline, form-feed and vertical tab); an` |
|      - | 3562 | ` *  explicit $separators argument REPLACES them (an empty string leaves only the` |
|      - | 3563 | ` *  very first character upper-cased). Like PHP, this is byte-based: only ASCII` |
|      - | 3564 | ` *  bytes are upper-cased and a byte is a separator only if it appears in the set.` |
|      - | 3565 | ` * Parameters` |
|      - | 3566 | ` *  $string` |
|      - | 3567 | ` *   The input string.` |
|      - | 3568 | ` *  $separators` |
|      - | 3569 | ` *   The optional word-boundary characters.` |
|      - | 3570 | ` * Return` |
|      - | 3571 | ` *  The modified string.` |
|      - | 3572 | ` */` |
|     22 | 3573 | `PH7_PRIVATE int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3574 | `{` |
|      - | 3575 | `	const char *zIn;` |
|      - | 3576 | `	int nLen,i,iStart;` |
|      - | 3577 | `	char aDelim[256];` |
|     23 | 3578 | `	if( nArg < 1 ){` |
|      - | 3579 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3580 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3581 | `		return PH7_OK;` |
|      - | 3582 | `	}` |
|      - | 3583 | `	/* Build the separator membership table: an explicit $separators argument` |
|      - | 3584 | `	 * replaces the default whitespace set (an empty string clears it). */` |
|     23 | 3585 | `	SyZero(aDelim,(sxu32)sizeof(aDelim));` |
|     23 | 3586 | `	if( nArg > 1 ){` |
|      - | 3587 | `		int nDelim;` |
|      9 | 3588 | `		const char *zDelim = ph7_value_to_string(apArg[1],&nDelim);` |
|     17 | 3589 | `		for( i = 0 ; i < nDelim ; i++ ){` |
|      9 | 3590 | `			aDelim[(unsigned char)zDelim[i]] = 1;` |
|      5 | 3591 | `		}` |
|      5 | 3592 | `	}else{` |
|     15 | 3593 | `		aDelim[(unsigned char)' ']  = 1;` |
|     15 | 3594 | `		aDelim[(unsigned char)'\t'] = 1;` |
|     15 | 3595 | `		aDelim[(unsigned char)'\r'] = 1;` |
|     15 | 3596 | `		aDelim[(unsigned char)'\n'] = 1;` |
|     15 | 3597 | `		aDelim[(unsigned char)'\f'] = 1;` |
|     15 | 3598 | `		aDelim[(unsigned char)'\v'] = 1;` |
|      - | 3599 | `	}` |
|      - | 3600 | `	/* Extract the target string */` |
|     23 | 3601 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3602 | `	if( nLen < 1 ){` |
|      - | 3603 | `		/* Empty string – match PHP semantics */` |
|      3 | 3604 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3605 | `		return PH7_OK;` |
|      - | 3606 | `	}` |
|      - | 3607 | `	/* Upper-case the first byte of each word (the leading byte, or any byte that` |
|      - | 3608 | `	 * follows a separator), appending the untouched runs in between verbatim. */` |
|     21 | 3609 | `	iStart = 0;` |
|    309 | 3610 | `	for( i = 0 ; i < nLen ; i++ ){` |
|    289 | 3611 | `		int c = (unsigned char)zIn[i];` |
|    289 | 3612 | `		if( (i == 0 \|\| aDelim[(unsigned char)zIn[i-1]]) && c < 0x80 && SyisLower(c) ){` |
|     53 | 3613 | `			char up = (char)SyToUpper(c);` |
|     53 | 3614 | `			if( i > iStart ){` |
|     35 | 3615 | `				ph7_result_string(pCtx,&zIn[iStart],i - iStart);` |
|     17 | 3616 | `			}` |
|     53 | 3617 | `			ph7_result_string(pCtx,&up,1);` |
|     53 | 3618 | `			iStart = i + 1;` |
|     26 | 3619 | `		}` |
|    145 | 3620 | `	}` |
|     21 | 3621 | `	if( nLen > iStart ){` |
|     21 | 3622 | `		ph7_result_string(pCtx,&zIn[iStart],nLen - iStart);` |
|     10 | 3623 | `	}` |
|     21 | 3624 | `	return PH7_OK;` |
|     12 | 3625 | `}` |
|      - | 3626 | `/*` |
|      - | 3627 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3628 | ` *  Returns input repeated multiplier times.` |
|      - | 3629 | ` * Parameters` |
|      - | 3630 | ` *  $string` |
|      - | 3631 | ` *   String to be repeated.` |
|      - | 3632 | ` * $multiplier` |
|      - | 3633 | ` *  Number of time the input string should be repeated.` |
|      - | 3634 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3635 | ` *  to 0, the function will return an empty string.` |
|      - | 3636 | ` * Return` |
|      - | 3637 | ` *  The repeated string.` |
|      - | 3638 | ` */` |
|  20444 | 3639 | `PH7_PRIVATE int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3640 | `{` |
|      - | 3641 | `	const char *zIn;` |
|      - | 3642 | `	int nLen;` |
|      - | 3643 | `	ph7_int64 nMul;` |
|      - | 3644 | `	int rc;` |
|  20447 | 3645 | `	if( nArg < 2 ){` |
|      - | 3646 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3647 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3648 | `		return PH7_OK;` |
|      - | 3649 | `	}` |
|      - | 3650 | `	/* Extract the target string */` |
|  20447 | 3651 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3652 | `	/* Resolve $times through the shared ZPP helper so a lossy float / float-string` |
|      - | 3653 | `	 * carries php's precision deprecation and NAN/INF/non-numeric fail with php's` |
|      - | 3654 | `	 * TypeError — a bare ph7_value_to_int64() coerced them silently. */` |
|      - | 3655 | `	{` |
|  20447 | 3656 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_repeat",2,"$times","int",&nMul);` |
|  20447 | 3657 | `		if( rcArg != PH7_OK ){` |
|      5 | 3658 | `			return rcArg;` |
|      - | 3659 | `		}` |
|      - | 3660 | `	}` |
|  20443 | 3661 | `	if( nMul < 0 ){` |
|      3 | 3662 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 3663 | `			"str_repeat(): Argument #2 ($times) must be greater than or equal to 0");` |
|      - | 3664 | `	}` |
|  20441 | 3665 | `	if( nLen < 1 \|\| nMul < 1 ){` |
|      - | 3666 | `		/* Empty input or a zero multiplier yields the empty string (PHP). */` |
|    ! 0 | 3667 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3668 | `		return PH7_OK;` |
|      - | 3669 | `	}` |
|      - | 3670 | `	/* Perform the requested operation */` |
| 223892 | 3671 | `	for(;;){` |
| 447787 | 3672 | `		if( !nMul ){` |
|  20441 | 3673 | `			break;` |
|      - | 3674 | `		}` |
|      - | 3675 | `		/* Append the copy */` |
| 427349 | 3676 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 427349 | 3677 | `		if( rc != PH7_OK ){` |
|      - | 3678 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 3679 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 3680 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 3681 | `		}` |
| 427349 | 3682 | `		nMul--;` |
|      3 | 3683 | `	}` |
|  20441 | 3684 | `	return PH7_OK;` |
|  10225 | 3685 | `}` |
|      - | 3686 | `/*` |
|      - | 3687 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3688 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3689 | ` * Parameters` |
|      - | 3690 | ` *  $string` |
|      - | 3691 | ` *   The input string.` |
|      - | 3692 | ` * $is_xhtml` |
|      - | 3693 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3694 | ` * Return` |
|      - | 3695 | ` *  The processed string.` |
|      - | 3696 | ` */` |
|      4 | 3697 | `PH7_PRIVATE int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3698 | `{` |
|      - | 3699 | `	const char *zIn,*zCur,*zEnd;` |
|      5 | 3700 | `	int is_xhtml = 1; /* Default to XHTML-style '<br />' like PHP */` |
|      - | 3701 | `	int nLen;` |
|      5 | 3702 | `	if( nArg < 1 ){` |
|      - | 3703 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 3704 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3705 | `		return PH7_OK;` |
|      - | 3706 | `	}` |
|      - | 3707 | `	/* Extract the target string */` |
|      5 | 3708 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3709 | `	if( nLen < 1 ){` |
|      - | 3710 | `		/* Empty string,return null */` |
|    ! 0 | 3711 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3712 | `		return PH7_OK;` |
|      - | 3713 | `	}` |
|      5 | 3714 | `	if( nArg > 1 ){` |
|      3 | 3715 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3716 | `	}` |
|      5 | 3717 | `	zEnd = &zIn[nLen];` |
|      - | 3718 | `	/* Perform the requested operation */` |
|      4 | 3719 | `	for(;;){` |
|      9 | 3720 | `		zCur = zIn;` |
|      - | 3721 | `		/* Delimit the string */` |
|     21 | 3722 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3723 | `			zIn++;` |
|      1 | 3724 | `		}` |
|      9 | 3725 | `		if( zCur < zIn ){` |
|      - | 3726 | `			/* Output chunk verbatim */` |
|      9 | 3727 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3728 | `		}` |
|      9 | 3729 | `		if( zIn >= zEnd ){` |
|      - | 3730 | `			/* No more input to process */` |
|      5 | 3731 | `			break;` |
|      - | 3732 | `		}` |
|      - | 3733 | `		/* Output the HTML line break */` |
|      - | 3734 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br />' (space before the slash), otherwise use '<br>' */` |
|      5 | 3735 | `		if( is_xhtml ){` |
|      3 | 3736 | `			ph7_result_string(pCtx,"<br />",(int)sizeof("<br />")-1);` |
|      2 | 3737 | `		}else{` |
|      3 | 3738 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3739 | `		}` |
|      5 | 3740 | `		zCur = zIn;` |
|      - | 3741 | `		/* Append trailing line */` |
|     11 | 3742 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3743 | `			zIn++;` |
|      1 | 3744 | `		}` |
|      5 | 3745 | `		if( zCur < zIn ){` |
|      - | 3746 | `			/* Output chunk verbatim */` |
|      5 | 3747 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3748 | `		}` |
|      1 | 3749 | `	}` |
|      5 | 3750 | `	return PH7_OK;` |
|      3 | 3751 | `}` |
|      - | 3752 | `/*` |
|      - | 3753 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3754 | ` *  According to the PHP reference manual.` |
|      - | 3755 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3756 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3757 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3758 | ` * This applies to both sprintf() and printf().` |
|      - | 3759 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3760 | ` * or more of these elements, in order:` |
|      - | 3761 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3762 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3763 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 3764 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 3765 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 3766 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 3767 | ` *   it with a single quote ('). See the examples below.` |
|      - | 3768 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 3769 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 3770 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 3771 | ` *   should result in.` |
|      - | 3772 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 3773 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 3774 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 3775 | ` *   limit to the string.` |
|      - | 3776 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 3777 | ` *       % - a literal percent character. No argument is required.` |
|      - | 3778 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 3779 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 3780 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 3781 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 3782 | ` * 	     for the number of digits after the decimal point.` |
|      - | 3783 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 3784 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 3785 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 3786 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 3787 | ` *       g - shorter of %e and %f.` |
|      - | 3788 | ` *       G - shorter of %E and %f.` |
|      - | 3789 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 3790 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 3791 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 3792 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 3793 | ` */` |
|      - | 3794 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3795 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 3796 | `/*` |
|      - | 3797 | ` * Symisc eXtension.` |
|      - | 3798 | ` * string size_format(int64 $size)` |
|      - | 3799 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 3800 | ` *  Example:` |
|      - | 3801 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 3802 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 3803 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 3804 | ` * Parameter` |
|      - | 3805 | ` *  $size` |
|      - | 3806 | ` *    Entity size in bytes.` |
|      - | 3807 | ` * Return` |
|      - | 3808 | ` *   Formatted string representation of the given size.` |
|      - | 3809 | ` */` |
|     24 | 3810 | `PH7_PRIVATE int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3811 | `{` |
|      - | 3812 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 3813 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 3814 | `	sxi32 nRest,i_32;` |
|      - | 3815 | `	ph7_int64 iSize;` |
|     25 | 3816 | `	int c = -1; /* index in zUnit[] */` |
|      - | 3817 |  |
|     25 | 3818 | `	if( nArg < 1 ){` |
|      - | 3819 | `		/* Missing argument,return the empty string */` |
|      3 | 3820 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3821 | `		return PH7_OK;` |
|      - | 3822 | `	}` |
|      - | 3823 | `	/* Extract the given size */` |
|     23 | 3824 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 3825 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 3826 | `		/* Don't bother formatting,return immediately */` |
|      5 | 3827 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 3828 | `		return PH7_OK;` |
|      - | 3829 | `	}` |
|     19 | 3830 | `	for(;;){` |
|     39 | 3831 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 3832 | `		iSize >>= 10;` |
|     39 | 3833 | `		c++;` |
|     39 | 3834 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 3835 | `			break;` |
|      - | 3836 | `		}` |
|      1 | 3837 | `	}` |
|     19 | 3838 | `	nRest /= 100;` |
|     19 | 3839 | `	if( nRest > 9 ){` |
|    ! 0 | 3840 | `		nRest = 9;` |
|    ! 0 | 3841 | `	}` |
|     19 | 3842 | `	if( iSize > 999 ){` |
|    ! 0 | 3843 | `		c++;` |
|    ! 0 | 3844 | `		nRest = 9;` |
|    ! 0 | 3845 | `		iSize = 0;` |
|    ! 0 | 3846 | `	}` |
|     19 | 3847 | `	i_32 = (sxi32)iSize;` |
|      - | 3848 | `	/* Format */` |
|     19 | 3849 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 3850 | `	return PH7_OK;` |
|     13 | 3851 | `}` |
|      - | 3852 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3853 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 3854 | `/*` |
|      - | 3855 | ` * string str_shuffle(string $str)` |
|      - | 3856 |  |
|      - | 3857 | ` *  Randomly shuffles a string.` |
|      - | 3858 | ` * Parameters` |
|      - | 3859 | ` *  $str` |
|      - | 3860 | ` *   The input string.` |
|      - | 3861 | ` * Return` |
|      - | 3862 | ` *  Returns the shuffled string.` |
|      - | 3863 | ` */` |
|     10 | 3864 | `PH7_PRIVATE int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3865 | `{` |
|      - | 3866 | `	const char *zString;` |
|      - | 3867 | `	int nLen,i,c;` |
|      - | 3868 | `	sxu32 iR;` |
|     11 | 3869 | `	if( nArg < 1 ){` |
|      - | 3870 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 3871 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3872 | `		return PH7_OK;` |
|      - | 3873 | `	}` |
|      - | 3874 | `	/* Extract the target string */` |
|     11 | 3875 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 3876 | `	if( nLen < 1 ){` |
|      - | 3877 | `		/* Nothing to shuffle */` |
|      3 | 3878 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3879 | `		return PH7_OK;` |
|      - | 3880 | `	}` |
|      - | 3881 | `	/* Shuffle the string */` |
|     43 | 3882 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 3883 | `		/* Generate a random number first */` |
|     35 | 3884 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 3885 | `		/* Extract a random offset */` |
|     35 | 3886 | `		c = zString[iR % nLen];` |
|      - | 3887 | `		/* Append it */` |
|     35 | 3888 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 3889 | `	}` |
|      9 | 3890 | `	return PH7_OK;` |
|      6 | 3891 | `}` |
|      - | 3892 | `/*` |
|      - | 3893 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 3894 | ` *  Convert a string to an array.` |
|      - | 3895 | ` * Parameters` |
|      - | 3896 | ` * $string` |
|      - | 3897 | ` *  The input string.` |
|      - | 3898 | ` * $split_length` |
|      - | 3899 | ` *  Maximum length of the chunk.` |
|      - | 3900 | ` * Return` |
|      - | 3901 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 3902 | ` *  except possibly the last one which may be shorter.` |
|      - | 3903 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 3904 | ` *  as the first (and only) array element.` |
|      - | 3905 | ` *  An empty string returns an empty array.` |
|      - | 3906 | ` * Errors` |
|      - | 3907 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 3908 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 3909 | ` *  ValueError if $split_length is less than 1.` |
|      - | 3910 | ` */` |
|     26 | 3911 | `PH7_PRIVATE int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3912 | `{` |
|      - | 3913 | `	const char *zString,*zEnd;` |
|      - | 3914 | `	ph7_value *pArray,*pValue;` |
|      - | 3915 | `	int split_len;` |
|      - | 3916 | `	int nLen;` |
|     29 | 3917 | `	if( nArg < 1 ){` |
|    ! 0 | 3918 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3919 | `			"ArgumentCountError",` |
|      - | 3920 | `			"str_split() expects at least 1 argument, %d given",` |
|    ! 0 | 3921 | `			nArg` |
|      - | 3922 | `			);` |
|      - | 3923 | `	}` |
|      - | 3924 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     39 | 3925 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     42 | 3926 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     26 | 3927 | `	    ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 3928 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3929 | `			"TypeError",` |
|      - | 3930 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 3931 | `			ph7_type_name(apArg[0])` |
|      - | 3932 | `			);` |
|      - | 3933 | `	}` |
|      - | 3934 | `	/* Point to the target string */` |
|     29 | 3935 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     29 | 3936 | `	split_len = (int)sizeof(char);` |
|     29 | 3937 | `	if( nArg > 1 ){` |
|      - | 3938 | `		/* Split length */` |
|     17 | 3939 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 3940 | `		if( split_len < 1 ){` |
|      6 | 3941 | `			return PH7_VmThrowException(pCtx,` |
|      - | 3942 | `				"ValueError",` |
|      - | 3943 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 3944 | `				);` |
|      - | 3945 | `		}` |
|     11 | 3946 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 3947 | `			split_len = nLen;` |
|      1 | 3948 | `		}` |
|      5 | 3949 | `	}` |
|      - | 3950 | `	/* Create the array and the scalar value */` |
|     23 | 3951 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 3952 | `	/*Chunk value */` |
|     23 | 3953 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     23 | 3954 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 3955 | `		/* Return FALSE */` |
|    ! 0 | 3956 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3957 | `		return PH7_OK;` |
|      - | 3958 | `	}` |
|      - | 3959 | `	/* Point to the end of the string */` |
|     23 | 3960 | `	zEnd = &zString[nLen];` |
|      - | 3961 | `	/* Perform the requested operation */` |
|    131 | 3962 | `	for(;;){` |
|      - | 3963 | `		int nMax;` |
|    143 | 3964 | `		if( zString >= zEnd ){` |
|      - | 3965 | `			/* No more input to process */` |
|     23 | 3966 | `			break;` |
|      - | 3967 | `		}` |
|    121 | 3968 | `		nMax = (int)(zEnd-zString);` |
|    121 | 3969 | `		if( nMax < split_len ){` |
|      3 | 3970 | `			split_len = nMax;` |
|      1 | 3971 | `		}` |
|      - | 3972 | `		/* Copy the current chunk */` |
|    121 | 3973 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 3974 | `		/* Insert it */` |
|    121 | 3975 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 3976 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 3977 | `		}` |
|      - | 3978 | `		/* reset the string cursor */` |
|    121 | 3979 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 3980 | `		/* Update position */` |
|    121 | 3981 | `		zString += split_len;` |
|      1 | 3982 | `	}` |
|      - | 3983 | `	/*` |
|      - | 3984 | `	 * Return the array.` |
|      - | 3985 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 3986 | `	 * upon we return from this function.` |
|      - | 3987 | `	 */` |
|     23 | 3988 | `	ph7_result_value(pCtx,pArray);` |
|     23 | 3989 | `	return PH7_OK;` |
|     16 | 3990 | `}` |
|      - | 3991 | `/*` |
|      - | 3992 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 3993 | ` * Refer to [strspn()].` |
|      - | 3994 | ` */` |
|     28 | 3995 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 3996 | `{` |
|     29 | 3997 | `	const char *zIn = *pzIn;` |
|      - | 3998 | `	const char *zPtr;` |
|      - | 3999 | `	/* Ignore leading white spaces */` |
|     29 | 4000 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 4001 | `		zIn++;` |
|    ! 0 | 4002 | `	}` |
|     29 | 4003 | `	if( zIn >= zEnd ){` |
|      - | 4004 | `		/* End of input */` |
|    ! 0 | 4005 | `		return SXERR_EOF;` |
|      - | 4006 | `	}` |
|     29 | 4007 | `	zPtr = zIn;` |
|      - | 4008 | `	/* Extract the token */` |
|    201 | 4009 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 4010 | `		zIn++;` |
|      1 | 4011 | `	}` |
|     29 | 4012 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 4013 | `	/* Synchronize pointers */` |
|     29 | 4014 | `	*pzIn = zIn;` |
|      - | 4015 | `	/* Return to the caller */` |
|     29 | 4016 | `	return SXRET_OK;` |
|     15 | 4017 | `}` |
|      - | 4018 | `/*` |
|      - | 4019 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 4020 | ` * return the longest match.` |
|      - | 4021 | ` * Refer to [strspn()].` |
|      - | 4022 | ` */` |
|     18 | 4023 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 4024 | `{` |
|     19 | 4025 | `	const char *zEnd = &zString[nLen];` |
|     19 | 4026 | `	const char *zIn = zString;` |
|      - | 4027 | `	int i,c;` |
|     45 | 4028 | `	for(;;){` |
|     91 | 4029 | `		if( zString >= zEnd ){` |
|      7 | 4030 | `			break;` |
|      - | 4031 | `		}` |
|      - | 4032 | `		/* Extract current character */` |
|     85 | 4033 | `		c = zString[0];` |
|      - | 4034 | `		/* Perform the lookup */` |
|    383 | 4035 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 4036 | `			if( c == zMask[i] ){` |
|      - | 4037 | `				/* Character found */` |
|     73 | 4038 | `				break;` |
|      - | 4039 | `			}` |
|    150 | 4040 | `		}` |
|     85 | 4041 | `		if( i >= nMaskLen ){` |
|      - | 4042 | `			/* Character not in the current mask,break immediately */` |
|     13 | 4043 | `			break;` |
|      - | 4044 | `		}` |
|      - | 4045 | `		/* Advance cursor */` |
|     73 | 4046 | `		zString++;` |
|      1 | 4047 | `	}` |
|      - | 4048 | `	/* Longest match */` |
|     19 | 4049 | `	return (int)(zString-zIn);` |
|      1 | 4050 | `}` |
|      - | 4051 | `/*` |
|      - | 4052 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 4053 | ` * Refer to [strcspn()].` |
|      - | 4054 | ` */` |
|     10 | 4055 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 4056 | `{` |
|     11 | 4057 | `	const char *zEnd = &zString[nLen];` |
|     11 | 4058 | `	const char *zIn = zString;` |
|      - | 4059 | `	int i,c;` |
|     12 | 4060 | `	for(;;){` |
|     25 | 4061 | `		if( zString >= zEnd ){` |
|      3 | 4062 | `			break;` |
|      - | 4063 | `		}` |
|      - | 4064 | `		/* Extract current character */` |
|     23 | 4065 | `		c = zString[0];` |
|      - | 4066 | `		/* Perform the lookup */` |
|     51 | 4067 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 4068 | `			if( c == zMask[i] ){` |
|      9 | 4069 | `				break;` |
|      - | 4070 | `			}` |
|     15 | 4071 | `		}` |
|     23 | 4072 | `		if( i < nMaskLen ){` |
|      - | 4073 | `			/* Character in the current mask,break immediately */` |
|      9 | 4074 | `			break;` |
|      - | 4075 | `		}` |
|      - | 4076 | `		/* Advance cursor */` |
|     15 | 4077 | `		zString++;` |
|      1 | 4078 | `	}` |
|      - | 4079 | `	/* Longest match */` |
|     11 | 4080 | `	return (int)(zString-zIn);` |
|      1 | 4081 | `}` |
|      - | 4082 | `/*` |
|      - | 4083 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 4084 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 4085 | ` *  of characters contained within a given mask.` |
|      - | 4086 | ` * Parameters` |
|      - | 4087 | ` * $str` |
|      - | 4088 | ` *  The input string.` |
|      - | 4089 | ` * $mask` |
|      - | 4090 | ` *  The list of allowable characters.` |
|      - | 4091 | ` * $start` |
|      - | 4092 | ` *  The position in subject to start searching.` |
|      - | 4093 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 4094 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 4095 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 4096 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 4097 | ` *  start'th position from the end of subject.` |
|      - | 4098 | ` * $length` |
|      - | 4099 | ` *  The length of the segment from subject to examine.` |
|      - | 4100 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 4101 | ` *  characters after the starting position.` |
|      - | 4102 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 4103 | ` *  position up to length characters from the end of subject.` |
|      - | 4104 | ` * Return` |
|      - | 4105 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 4106 | ` * in mask.` |
|      - | 4107 | ` */` |
|     24 | 4108 | `PH7_PRIVATE int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4109 | `{` |
|      - | 4110 | `	const char *zString,*zMask,*zEnd;` |
|      - | 4111 | `	int iMasklen,iLen;` |
|      - | 4112 | `	SyString sToken;` |
|     25 | 4113 | `	int iCount = 0;` |
|      - | 4114 | `	int rc;` |
|     25 | 4115 | `	if( nArg < 2 ){` |
|      - | 4116 | `		/* Missing agruments,return zero */` |
|    ! 0 | 4117 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4118 | `		return PH7_OK;` |
|      - | 4119 | `	}` |
|      - | 4120 | `	/* Extract the target string */` |
|     25 | 4121 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 4122 | `	/* Extract the mask */` |
|     25 | 4123 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 4124 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 4125 | `		/* Nothing to process,return zero */` |
|      7 | 4126 | `		ph7_result_int(pCtx,0);` |
|      7 | 4127 | `		return PH7_OK;` |
|      - | 4128 | `	}` |
|     19 | 4129 | `	if( nArg > 2 ){` |
|      - | 4130 | `		int nOfft;` |
|      - | 4131 | `		/* Extract the offset */` |
|      9 | 4132 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 4133 | `		if( nOfft < 0 ){` |
|    ! 0 | 4134 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 4135 | `			if( zBase > zString ){` |
|    ! 0 | 4136 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 4137 | `				zString = zBase;` |
|    ! 0 | 4138 | `			}else{` |
|      - | 4139 | `				/* Invalid offset */` |
|    ! 0 | 4140 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 4141 | `				return PH7_OK;` |
|      - | 4142 | `			}` |
|    ! 0 | 4143 | `		}else{` |
|      9 | 4144 | `			if( nOfft >= iLen ){` |
|      - | 4145 | `				/* Invalid offset */` |
|    ! 0 | 4146 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 4147 | `				return PH7_OK;` |
|    ! 0 | 4148 | `			}else{` |
|      - | 4149 | `				/* Update offset */` |
|      9 | 4150 | `				zString += nOfft;` |
|      9 | 4151 | `				iLen -= nOfft;` |
|      - | 4152 | `			}` |
|      - | 4153 | `		}` |
|      9 | 4154 | `		if( nArg > 3 ){` |
|      - | 4155 | `			int iUserlen;` |
|      - | 4156 | `			/* Extract the desired length */` |
|      9 | 4157 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 4158 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 4159 | `				iLen = iUserlen;` |
|      2 | 4160 | `			}` |
|      4 | 4161 | `		}` |
|      4 | 4162 | `	}` |
|      - | 4163 | `	/* Point to the end of the string */` |
|     19 | 4164 | `	zEnd = &zString[iLen];` |
|      - | 4165 | `	/* Extract the first non-space token */` |
|     19 | 4166 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 4167 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 4168 | `		/* Compare against the current mask */` |
|     19 | 4169 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 4170 | `	}` |
|      - | 4171 | `	/* Longest match */` |
|     19 | 4172 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 4173 | `	return PH7_OK;` |
|     13 | 4174 | `}` |
|      - | 4175 | `/*` |
|      - | 4176 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 4177 | ` *  Find length of initial segment not matching mask.` |
|      - | 4178 | ` * Parameters` |
|      - | 4179 | ` * $str` |
|      - | 4180 | ` *  The input string.` |
|      - | 4181 | ` * $mask` |
|      - | 4182 | ` *  The list of not allowed characters.` |
|      - | 4183 | ` * $start` |
|      - | 4184 | ` *  The position in subject to start searching.` |
|      - | 4185 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 4186 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 4187 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 4188 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 4189 | ` *  start'th position from the end of subject.` |
|      - | 4190 | ` * $length` |
|      - | 4191 | ` *  The length of the segment from subject to examine.` |
|      - | 4192 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 4193 | ` *  characters after the starting position.` |
|      - | 4194 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 4195 | ` *  position up to length characters from the end of subject.` |
|      - | 4196 | ` * Return` |
|      - | 4197 | ` *  Returns the length of the segment as an integer.` |
|      - | 4198 | ` */` |
|     14 | 4199 | `PH7_PRIVATE int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4200 | `{` |
|      - | 4201 | `	const char *zString,*zMask,*zEnd;` |
|      - | 4202 | `	int iMasklen,iLen;` |
|      - | 4203 | `	SyString sToken;` |
|     15 | 4204 | `	int iCount = 0;` |
|      - | 4205 | `	int rc;` |
|     15 | 4206 | `	if( nArg < 2 ){` |
|      - | 4207 | `		/* Missing agruments,return zero */` |
|    ! 0 | 4208 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4209 | `		return PH7_OK;` |
|      - | 4210 | `	}` |
|      - | 4211 | `	/* Extract the target string */` |
|     15 | 4212 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 4213 | `	/* Extract the mask */` |
|     15 | 4214 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 4215 | `	if( iLen < 1 ){` |
|      - | 4216 | `		/* Nothing to process,return zero */` |
|    ! 0 | 4217 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4218 | `		return PH7_OK;` |
|      - | 4219 | `	}` |
|     15 | 4220 | `	if( iMasklen < 1 ){` |
|      - | 4221 | `		/* No given mask,return the string length */` |
|      3 | 4222 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 4223 | `		return PH7_OK;` |
|      - | 4224 | `	}` |
|     13 | 4225 | `	if( nArg > 2 ){` |
|      - | 4226 | `		int nOfft;` |
|      - | 4227 | `		/* Extract the offset */` |
|     11 | 4228 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 4229 | `		if( nOfft < 0 ){` |
|    ! 0 | 4230 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 4231 | `			if( zBase > zString ){` |
|    ! 0 | 4232 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 4233 | `				zString = zBase;` |
|    ! 0 | 4234 | `			}else{` |
|      - | 4235 | `				/* Invalid offset */` |
|    ! 0 | 4236 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 4237 | `				return PH7_OK;` |
|      - | 4238 | `			}` |
|    ! 0 | 4239 | `		}else{` |
|     11 | 4240 | `			if( nOfft >= iLen ){` |
|      - | 4241 | `				/* Invalid offset */` |
|      3 | 4242 | `				ph7_result_int(pCtx,0);` |
|      3 | 4243 | `				return PH7_OK;` |
|    ! 0 | 4244 | `			}else{` |
|      - | 4245 | `				/* Update offset */` |
|      9 | 4246 | `				zString += nOfft;` |
|      9 | 4247 | `				iLen -= nOfft;` |
|      - | 4248 | `			}` |
|      - | 4249 | `		}` |
|      9 | 4250 | `		if( nArg > 3 ){` |
|      - | 4251 | `			int iUserlen;` |
|      - | 4252 | `			/* Extract the desired length */` |
|    ! 0 | 4253 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 4254 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 4255 | `				iLen = iUserlen;` |
|    ! 0 | 4256 | `			}` |
|    ! 0 | 4257 | `		}` |
|      4 | 4258 | `	}` |
|      - | 4259 | `	/* Point to the end of the string */` |
|     11 | 4260 | `	zEnd = &zString[iLen];` |
|      - | 4261 | `	/* Extract the first non-space token */` |
|     11 | 4262 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 4263 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 4264 | `		/* Compare against the current mask */` |
|     11 | 4265 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 4266 | `	}` |
|      - | 4267 | `	/* Longest match */` |
|     11 | 4268 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 4269 | `	return PH7_OK;` |
|      8 | 4270 | `}` |
|      - | 4271 | `/*` |
|      - | 4272 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 4273 | ` *  Search a string for any of a set of characters.` |
|      - | 4274 | ` * Parameters` |
|      - | 4275 | ` *  $haystack` |
|      - | 4276 | ` *   The string where char_list is looked for.` |
|      - | 4277 | ` *  $char_list` |
|      - | 4278 | ` *   This parameter is case sensitive.` |
|      - | 4279 | ` * Return` |
|      - | 4280 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 4281 | ` */` |
|      4 | 4282 | `PH7_PRIVATE int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4283 | `{` |
|      - | 4284 | `	const char *zString,*zList,*zEnd;` |
|      - | 4285 | `	int iLen,iListLen,i,c;` |
|      - | 4286 | `	sxu32 nOfft,nMax;` |
|      - | 4287 | `	sxi32 rc;` |
|      5 | 4288 | `	if( nArg < 2 ){` |
|      - | 4289 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 4290 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4291 | `		return PH7_OK;` |
|      - | 4292 | `	}` |
|      - | 4293 | `	/* Extract the haystack and the char list */` |
|      5 | 4294 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 4295 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 4296 | `	if( iLen < 1 ){` |
|      - | 4297 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 4298 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4299 | `		return PH7_OK;` |
|      - | 4300 | `	}` |
|      - | 4301 | `	/* Point to the end of the string */` |
|      5 | 4302 | `	zEnd = &zString[iLen];` |
|      5 | 4303 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 4304 | `	/* perform the requested operation */` |
|     15 | 4305 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 4306 | `		c = zList[i];` |
|     11 | 4307 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 4308 | `		if( rc == SXRET_OK ){` |
|      5 | 4309 | `			if( nMax < nOfft ){` |
|      3 | 4310 | `				nOfft = nMax;` |
|      1 | 4311 | `			}` |
|      2 | 4312 | `		}` |
|      6 | 4313 | `	}` |
|      5 | 4314 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 4315 | `		/* No such substring,return FALSE */` |
|      3 | 4316 | `		ph7_result_bool(pCtx,0);` |
|      2 | 4317 | `	}else{` |
|      - | 4318 | `		/* Return the substring */` |
|      3 | 4319 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 4320 | `	}` |
|      5 | 4321 | `	return PH7_OK;` |
|      3 | 4322 | `}` |
|      - | 4323 | `/* SPDX-SnippetBegin */` |
|      - | 4324 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 4325 | `/* SPDX-License-Identifier: blessing */` |
|      - | 4326 | `/*` |
|      - | 4327 | ` * string soundex(string $str)` |
|      - | 4328 | ` *  Calculate the soundex key of a string.` |
|      - | 4329 | ` * Parameters` |
|      - | 4330 | ` *  $str` |
|      - | 4331 | ` *   The input string.` |
|      - | 4332 | ` * Return` |
|      - | 4333 | ` *  Returns the soundex key as a string.` |
|      - | 4334 | ` * Note:` |
|      - | 4335 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 4336 | ` * source tree.` |
|      - | 4337 | ` */` |
|     22 | 4338 | `PH7_PRIVATE int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4339 | `{` |
|      - | 4340 | `	const unsigned char *zIn;` |
|      - | 4341 | `	char zResult[8];` |
|      - | 4342 | `	int i, j;` |
|      - | 4343 | `	static const unsigned char iCode[] = {` |
|      - | 4344 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 4345 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 4346 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 4347 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 4348 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 4349 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 4350 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 4351 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 4352 | `	};` |
|     23 | 4353 | `	if( nArg < 1 ){` |
|      - | 4354 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4355 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4356 | `		return PH7_OK;` |
|      - | 4357 | `	}` |
|     23 | 4358 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     35 | 4359 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     23 | 4360 | `	if( zIn[i] ){` |
|     17 | 4361 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 4362 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 4363 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 4364 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 4365 | `			if( code>0 ){` |
|     45 | 4366 | `				if( code!=prevcode ){` |
|     33 | 4367 | `					prevcode = (unsigned char)code;` |
|     33 | 4368 | `					zResult[j++] = (char)code + '0';` |
|     16 | 4369 | `				}` |
|     23 | 4370 | `			}else{` |
|     49 | 4371 | `				prevcode = 0;` |
|      - | 4372 | `			}` |
|     47 | 4373 | `		}` |
|     33 | 4374 | `		while( j<4 ){` |
|     17 | 4375 | `			zResult[j++] = '0';` |
|      1 | 4376 | `		}` |
|     17 | 4377 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 4378 | `	}else{` |
|      - | 4379 | `	  /* No alphabetic character: PHP returns "0000" (not the SQLite "?000"). */` |
|      7 | 4380 | `	  ph7_result_string(pCtx,"0000",4);` |
|      - | 4381 | `	}` |
|     23 | 4382 | `	return PH7_OK;` |
|     12 | 4383 | `}` |
|      - | 4384 | `/* SPDX-SnippetEnd */` |
|      - | 4385 | `/*` |
|      - | 4386 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 4387 | ` *  Wraps a string to a given number of characters.` |
|      - | 4388 | ` * Parameters` |
|      - | 4389 | ` *  $str` |
|      - | 4390 | ` *   The input string.` |
|      - | 4391 | ` * $width` |
|      - | 4392 | ` *  The column width.` |
|      - | 4393 | ` * $break` |
|      - | 4394 | ` *  The line is broken using the optional break parameter.` |
|      - | 4395 | ` * Return` |
|      - | 4396 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 4397 | ` */` |
|     26 | 4398 | `PH7_PRIVATE int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4399 | `{` |
|      - | 4400 | `	const char *zIn,*zBreak;` |
|      - | 4401 | `	SyBlob sWorker;` |
|      - | 4402 | `	int iLen,iBreaklen,iWidth,iCut,iStart,iSpace,iCur;` |
|      - | 4403 | `	sxi32 rc;` |
|     27 | 4404 | `	if( nArg < 1 ){` |
|      - | 4405 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4406 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4407 | `		return PH7_OK;` |
|      - | 4408 | `	}` |
|      - | 4409 | `	/* Extract the input string */` |
|     27 | 4410 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 4411 | `	/* Width (default 75; PHP allows 0/negative — break at every space). */` |
|     27 | 4412 | `	iWidth = 75;` |
|     27 | 4413 | `	if( nArg > 1 ){` |
|     27 | 4414 | `		iWidth = ph7_value_to_int(apArg[1]);` |
|     13 | 4415 | `	}` |
|      - | 4416 | `	/* Break string (default "\n"). */` |
|     27 | 4417 | `	zBreak = "\n";` |
|     27 | 4418 | `	iBreaklen = (int)sizeof(char);` |
|     27 | 4419 | `	if( nArg > 2 ){` |
|     13 | 4420 | `		zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      6 | 4421 | `	}` |
|      - | 4422 | `	/* Cut long words? (default false). */` |
|     27 | 4423 | `	iCut = 0;` |
|     27 | 4424 | `	if( nArg > 3 ){` |
|      7 | 4425 | `		iCut = ph7_value_to_bool(apArg[3]);` |
|      3 | 4426 | `	}` |
|     27 | 4427 | `	if( iLen < 1 ){` |
|      - | 4428 | `		/* PHP returns the empty string for empty input before validating the other args. */` |
|      5 | 4429 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 4430 | `		return PH7_OK;` |
|      - | 4431 | `	}` |
|      - | 4432 | `	/* PHP 8 domain errors (catchable ValueError). */` |
|     23 | 4433 | `	if( iBreaklen < 1 ){` |
|      3 | 4434 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4435 | `			"wordwrap(): Argument #3 ($break) must not be empty");` |
|      - | 4436 | `	}` |
|     21 | 4437 | `	if( iWidth == 0 && iCut ){` |
|      3 | 4438 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4439 | `			"wordwrap(): Argument #4 ($cut_long_words) cannot be true when argument #2 ($width) is 0");` |
|      - | 4440 | `	}` |
|      - | 4441 | `	/*` |
|      - | 4442 | `	 * PHP's algorithm: a single left-to-right pass tracking the start of the` |
|      - | 4443 | `	 * current line (iStart) and the position of the last space seen on it` |
|      - | 4444 | `	 * (iSpace). A break is emitted when the line reaches the width, at the last` |
|      - | 4445 | `	 * space if there was one, otherwise (only when cut is enabled) hard at the` |
|      - | 4446 | `	 * boundary. An existing break sequence in the input resets the line.` |
|      - | 4447 | `	 */` |
|     19 | 4448 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     19 | 4449 | `	iStart = iSpace = iCur = 0;` |
|     19 | 4450 | `	rc = SXRET_OK;` |
|    551 | 4451 | `	while( iCur < iLen ){` |
|    533 | 4452 | `		if( iBreaklen <= iLen - iCur && SyMemcmp(&zIn[iCur],zBreak,(sxu32)iBreaklen) == 0 ){` |
|      - | 4453 | `			/* Existing break sequence in the input: copy it verbatim and reset the line. */` |
|    ! 0 | 4454 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart + iBreaklen));` |
|    ! 0 | 4455 | `			if( rc != SXRET_OK ){ goto oom; }` |
|    ! 0 | 4456 | `			iCur += iBreaklen;` |
|    ! 0 | 4457 | `			iStart = iSpace = iCur;` |
|    ! 0 | 4458 | `			continue;` |
|    533 | 4459 | `		}else if( zIn[iCur] == ' ' ){` |
|     67 | 4460 | `			if( iCur - iStart >= iWidth ){` |
|      - | 4461 | `				/* The line already fills the width at this space: break here (the space is consumed). */` |
|     13 | 4462 | `				rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     13 | 4463 | `				if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     13 | 4464 | `				if( rc != SXRET_OK ){ goto oom; }` |
|     13 | 4465 | `				iStart = iCur + 1;` |
|      6 | 4466 | `			}` |
|     67 | 4467 | `			iSpace = iCur;` |
|    500 | 4468 | `		}else if( iCut && iCur - iStart >= iWidth && iStart >= iSpace ){` |
|      - | 4469 | `			/* A word longer than the width with no space to break at: hard-cut at the boundary. */` |
|      7 | 4470 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|      7 | 4471 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|      7 | 4472 | `			if( rc != SXRET_OK ){ goto oom; }` |
|      7 | 4473 | `			iStart = iSpace = iCur;` |
|    464 | 4474 | `		}else if( iCur - iStart >= iWidth && iStart < iSpace ){` |
|      - | 4475 | `			/* Past the width mid-word: wrap back to the last space (which is consumed). */` |
|     17 | 4476 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iSpace - iStart));` |
|     17 | 4477 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     17 | 4478 | `			if( rc != SXRET_OK ){ goto oom; }` |
|     17 | 4479 | `			iStart = iSpace = iSpace + 1;` |
|      8 | 4480 | `		}` |
|    533 | 4481 | `		iCur++;` |
|      1 | 4482 | `	}` |
|      - | 4483 | `	/* Emit the trailing chunk. */` |
|     19 | 4484 | `	if( iStart < iCur ){` |
|     19 | 4485 | `		rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     19 | 4486 | `		if( rc != SXRET_OK ){ goto oom; }` |
|      9 | 4487 | `	}` |
|     19 | 4488 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|     19 | 4489 | `	SyBlobRelease(&sWorker);` |
|     19 | 4490 | `	return PH7_OK;` |
|    ! 0 | 4491 | `oom:` |
|    ! 0 | 4492 | `	SyBlobRelease(&sWorker);` |
|    ! 0 | 4493 | `	return PH7_ContextMemoryError(pCtx);` |
|     14 | 4494 | `}` |
|      - | 4495 | `/*` |
|      - | 4496 | ` * Check if the given character is a member of the given mask.` |
|      - | 4497 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 4498 | ` * Refer to [strtok()].` |
|      - | 4499 | ` */` |
|     30 | 4500 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 4501 | `{` |
|      - | 4502 | `	int i;` |
|     57 | 4503 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 4504 | `		if( c == zMask[i] ){` |
|     13 | 4505 | `			if( pOfft ){` |
|      5 | 4506 | `				*pOfft = i;` |
|      2 | 4507 | `			}` |
|     13 | 4508 | `			return TRUE;` |
|      - | 4509 | `		}` |
|     14 | 4510 | `	}` |
|     19 | 4511 | `	return FALSE;` |
|     16 | 4512 | `}` |
|      - | 4513 | `/*` |
|      - | 4514 | ` * Extract a single token from the input stream.` |
|      - | 4515 | ` * Refer to [strtok()].` |
|      - | 4516 | ` */` |
|      6 | 4517 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 4518 | `{` |
|      7 | 4519 | `	const char *zIn = *pzIn;` |
|      - | 4520 | `	const char *zPtr;` |
|      - | 4521 | `	/* Ignore leading delimiter */` |
|     11 | 4522 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 4523 | `		zIn++;` |
|      1 | 4524 | `	}` |
|      7 | 4525 | `	if( zIn >= zEnd ){` |
|      - | 4526 | `		/* End of input */` |
|    ! 0 | 4527 | `		return SXERR_EOF;` |
|      - | 4528 | `	}` |
|      7 | 4529 | `	zPtr = zIn;` |
|      - | 4530 | `	/* Extract the token */` |
|     13 | 4531 | `	while( zIn < zEnd ){` |
|     11 | 4532 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 4533 | `			/* UTF-8 stream */` |
|    ! 0 | 4534 | `			zIn++;` |
|    ! 0 | 4535 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 4536 | `		}else{` |
|     11 | 4537 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 4538 | `				break;` |
|      - | 4539 | `			}` |
|      7 | 4540 | `			zIn++;` |
|      - | 4541 | `		}` |
|      1 | 4542 | `	}` |
|      7 | 4543 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 4544 | `	/* Update the cursor */` |
|      7 | 4545 | `	*pzIn = zIn;` |
|      - | 4546 | `	/* Return to the caller */` |
|      7 | 4547 | `	return SXRET_OK;` |
|      4 | 4548 | `}` |
|      - | 4549 | `/* strtok auxiliary private data */` |
|      - | 4550 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 4551 | `struct strtok_aux_data` |
|      - | 4552 | `{` |
|      - | 4553 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 4554 | `	const char *zIn;   /* Current input stream */` |
|      - | 4555 | `	const char *zEnd;  /* End of input */` |
|      - | 4556 | `};` |
|      - | 4557 | `/*` |
|      - | 4558 | ` * string strtok(string $str,string $token)` |
|      - | 4559 | ` * string strtok(string $token)` |
|      - | 4560 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 4561 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 4562 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 4563 | ` *  words by using the space character as the token.` |
|      - | 4564 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 4565 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 4566 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 4567 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 4568 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 4569 | ` *  the argument are found.` |
|      - | 4570 | ` * Parameters` |
|      - | 4571 | ` *  $str` |
|      - | 4572 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 4573 | ` * $token` |
|      - | 4574 | ` *  The delimiter used when splitting up str.` |
|      - | 4575 | ` * Return` |
|      - | 4576 | ` *   Current token or FALSE on EOF.` |
|      - | 4577 | ` */` |
|      6 | 4578 | `PH7_PRIVATE int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4579 | `{` |
|      - | 4580 | `	strtok_aux_data *pAux;` |
|      - | 4581 | `	const char *zMask;` |
|      - | 4582 | `	SyString sToken;` |
|      - | 4583 | `	int nMasklen;` |
|      - | 4584 | `	sxi32 rc;` |
|      7 | 4585 | `	if( nArg < 2 ){` |
|      - | 4586 | `		/* Extract top aux data */` |
|      5 | 4587 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      5 | 4588 | `		if( pAux == 0 ){` |
|      - | 4589 | `			/* No aux data,return FALSE */` |
|    ! 0 | 4590 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 4591 | `			return PH7_OK;` |
|      - | 4592 | `		}` |
|      5 | 4593 | `		nMasklen = 0;` |
|      5 | 4594 | `		zMask = ""; /* cc warning */` |
|      5 | 4595 | `		if( nArg > 0 ){` |
|      - | 4596 | `			/* Extract the mask */` |
|      5 | 4597 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 4598 | `		}` |
|      5 | 4599 | `		if( nMasklen < 1 ){` |
|      - | 4600 | `			/* Invalid mask,return FALSE */` |
|    ! 0 | 4601 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 4602 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 4603 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 4604 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 4605 | `			return PH7_OK;` |
|      - | 4606 | `		}` |
|      - | 4607 | `		/* Extract the token */` |
|      5 | 4608 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 4609 | `		if( rc != SXRET_OK ){` |
|      - | 4610 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 4611 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 4612 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 4613 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 4614 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 4615 | `		}else{` |
|      - | 4616 | `			/* Return the extracted token */` |
|      5 | 4617 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 4618 | `		}` |
|      3 | 4619 | `	}else{` |
|      - | 4620 | `		const char *zInput,*zCur;` |
|      - | 4621 | `		char *zDup;` |
|      - | 4622 | `		int nLen;` |
|      - | 4623 | `		/* Extract the raw input */` |
|      3 | 4624 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4625 | `		if( nLen < 1 ){` |
|      - | 4626 | `			/* Empty input,return FALSE */` |
|    ! 0 | 4627 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 4628 | `			return PH7_OK;` |
|      - | 4629 | `		}` |
|      - | 4630 | `		/* Extract the mask */` |
|      3 | 4631 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 4632 | `		if( nMasklen < 1 ){` |
|      - | 4633 | `			/* Set a default mask */` |
|      - | 4634 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 4635 | `			zMask = TOK_MASK;` |
|    ! 0 | 4636 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 4637 | `#undef TOK_MASK` |
|    ! 0 | 4638 | `		}` |
|      - | 4639 | `		/* Extract a single token */` |
|      3 | 4640 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 4641 | `		if( rc != SXRET_OK ){` |
|      - | 4642 | `			/* Empty input */` |
|    ! 0 | 4643 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 4644 | `			return PH7_OK;` |
|    ! 0 | 4645 | `		}else{` |
|      - | 4646 | `			/* Return the extracted token */` |
|      3 | 4647 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 4648 | `		}` |
|      - | 4649 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 4650 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 4651 | `		if( pAux ){` |
|      3 | 4652 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 4653 | `			if( nLen < 1 ){` |
|    ! 0 | 4654 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 4655 | `				return PH7_OK;` |
|      - | 4656 | `			}` |
|      - | 4657 | `			/* Duplicate input */` |
|      3 | 4658 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 4659 | `			if( zDup  ){` |
|      3 | 4660 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 4661 | `				/* Register the aux data */` |
|      3 | 4662 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 4663 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 4664 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 4665 | `			}` |
|      1 | 4666 | `		}` |
|      - | 4667 | `	}` |
|      7 | 4668 | `	return PH7_OK;` |
|      4 | 4669 | `}` |
|      - | 4670 | `/*` |
|      - | 4671 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 4672 | ` *  Pad a string to a certain length with another string` |
|      - | 4673 | ` * Parameters` |
|      - | 4674 | ` *  $input` |
|      - | 4675 | ` *   The input string.` |
|      - | 4676 | ` * $pad_length` |
|      - | 4677 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 4678 | ` *   string, no padding takes place.` |
|      - | 4679 | ` * $pad_string` |
|      - | 4680 | ` *   Note:` |
|      - | 4681 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 4682 | ` *    divided by the pad_string's length.` |
|      - | 4683 | ` * $pad_type` |
|      - | 4684 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 4685 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 4686 | ` * Return` |
|      - | 4687 | ` *  The padded string.` |
|      - | 4688 | ` */` |
|    122 | 4689 | `PH7_PRIVATE int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4690 | `{` |
|      - | 4691 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 4692 | `	const char *zIn,*zPad;` |
|    124 | 4693 | `	if( nArg < 2 ){` |
|      - | 4694 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4695 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4696 | `		return PH7_OK;` |
|      - | 4697 | `	}` |
|      - | 4698 | `	/* Extract the target string */` |
|    124 | 4699 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 4700 | `	/* Padding length */` |
|      - | 4701 | `	{` |
|    124 | 4702 | `		sxi64 iTmp = 0;` |
|    124 | 4703 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_pad",2,"$length","int",&iTmp);` |
|    124 | 4704 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 4705 | `			return rcArg;` |
|      - | 4706 | `		}` |
|    124 | 4707 | `		iRealPad = iPadlen = (int)iTmp;` |
|      - | 4708 | `	}` |
|    124 | 4709 | `	if( iPadlen > 0 ){` |
|    122 | 4710 | `		iPadlen -= iLen;` |
|     60 | 4711 | `	}` |
|    124 | 4712 | `	if( iPadlen < 1  ){` |
|      - | 4713 | `		/* Return the string verbatim */` |
|      5 | 4714 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      5 | 4715 | `		return PH7_OK;` |
|      - | 4716 | `	}` |
|    120 | 4717 | `	zPad = " "; /* Whitespace padding */` |
|    120 | 4718 | `	iStrpad = (int)sizeof(char);` |
|    120 | 4719 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|    120 | 4720 | `	if( nArg > 2 ){` |
|      - | 4721 | `		/* Padding string */` |
|      7 | 4722 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      7 | 4723 | `		if( iStrpad < 1 ){` |
|      - | 4724 | `			/* An empty pad string throws a catchable ValueError in PHP 8` |
|      - | 4725 | `			 * (only reached once padding is actually required). */` |
|      3 | 4726 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4727 | `				"str_pad(): Argument #3 ($pad_string) must not be empty");` |
|      - | 4728 | `		}` |
|      5 | 4729 | `		if( nArg > 3 ){` |
|      - | 4730 | `			/* Padd type */` |
|      5 | 4731 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 4732 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 4733 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 4734 | `			}` |
|      2 | 4735 | `		}` |
|      2 | 4736 | `	}` |
|    118 | 4737 | `	iDiv = 1;` |
|    118 | 4738 | `	if( iType == 2 ){` |
|    ! 0 | 4739 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 4740 | `	}` |
|      - | 4741 | `	/* Perform the requested operation */` |
|    118 | 4742 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 4743 | `		jPad = iStrpad;` |
|      5 | 4744 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 4745 | `			/* Padding */` |
|      5 | 4746 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 4747 | `				break;` |
|      - | 4748 | `			}` |
|      3 | 4749 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 4750 | `		}` |
|      3 | 4751 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 4752 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 4753 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 4754 | `				if( jPad > iStrpad ){` |
|    ! 0 | 4755 | `					jPad = iStrpad;` |
|    ! 0 | 4756 | `				}` |
|      3 | 4757 | `				if( jPad < 1){` |
|    ! 0 | 4758 | `					break;` |
|      - | 4759 | `				}` |
|      3 | 4760 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 4761 | `			}` |
|      1 | 4762 | `		}` |
|      1 | 4763 | `	}` |
|    118 | 4764 | `	if( iLen > 0 ){` |
|      - | 4765 | `		/* Append the input string */` |
|    118 | 4766 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|     58 | 4767 | `	}` |
|    118 | 4768 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|   1252 | 4769 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 4770 | `			/* Padding */` |
|   1252 | 4771 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|    116 | 4772 | `				break;` |
|      - | 4773 | `			}` |
|   1138 | 4774 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|    570 | 4775 | `		}` |
|    230 | 4776 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|    116 | 4777 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|    116 | 4778 | `			if( jPad > iStrpad ){` |
|    ! 0 | 4779 | `				jPad = iStrpad;` |
|    ! 0 | 4780 | `			}` |
|    116 | 4781 | `			if( jPad < 1){` |
|    ! 0 | 4782 | `				break;` |
|      - | 4783 | `			}` |
|    116 | 4784 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 4785 | `		}` |
|     57 | 4786 | `	}` |
|    118 | 4787 | `	return PH7_OK;` |
|     63 | 4788 | `}` |
|      - | 4789 | `/*` |
|      - | 4790 | ` * String replacement private data.` |
|      - | 4791 | ` */` |
|      - | 4792 | `typedef struct str_replace_data str_replace_data;` |
|      - | 4793 | `struct str_replace_data` |
|      - | 4794 | `{` |
|      - | 4795 | `	/* Used by the str_replace family to collect the search/replace arguments. */` |
|      - | 4796 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 4797 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 4798 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 4799 | `};` |
|      - | 4800 | `/*` |
|      - | 4801 | ` * Remove a substring.` |
|      - | 4802 | ` */` |
|      - | 4803 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 4804 | `	for(;;){\` |
|      - | 4805 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 4806 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 4807 | `		++OFFT;\` |
|      - | 4808 | `	}\` |
|      - | 4809 | `}` |
|      - | 4810 | `/*` |
|      - | 4811 | ` * Shift right and insert algorithm.` |
|      - | 4812 | ` */` |
|      - | 4813 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 4814 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 4815 | `		for(;;){\` |
|      - | 4816 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 4817 | `			if(INLEN < 1 ) { break; }\` |
|      - | 4818 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 4819 | `			--INLEN; \` |
|      - | 4820 | `		}\` |
|      - | 4821 | `		for(;;){\` |
|      - | 4822 | `				if(ELEN < 1) { break; }\` |
|      - | 4823 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 4824 | `				OFFT++;\` |
|      - | 4825 | `				ENTRY++;\` |
|      - | 4826 | `				--ELEN;\` |
|      - | 4827 | `		}\` |
|      - | 4828 | `}` |
|      - | 4829 | `/*` |
|      - | 4830 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 4831 | ` * replacement string [i.e: zReplace].` |
|      - | 4832 | ` */` |
|     88 | 4833 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      5 | 4834 | `{` |
|     93 | 4835 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 4836 | `	sxu32 n,m;` |
|     93 | 4837 | `	n = SyBlobLength(pWorker);` |
|     93 | 4838 | `	m = nOfft;` |
|      - | 4839 | `	/* Delete the old entry */` |
|   7427 | 4840 | `	STRDEL(zInput,n,m,nLen);` |
|     93 | 4841 | `	SyBlobLength(pWorker) -= nLen;` |
|     93 | 4842 | `	if( nReplen > 0 ){` |
|     87 | 4843 | `		sxi32 iRep = nReplen;` |
|      - | 4844 | `		sxi32 rc;` |
|      - | 4845 | `		/*` |
|      - | 4846 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 4847 | `		 * string.` |
|      - | 4848 | `		 */` |
|     87 | 4849 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     87 | 4850 | `		if( rc != SXRET_OK ){` |
|      - | 4851 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 4852 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 4853 | `			return rc;` |
|      - | 4854 | `		}` |
|      - | 4855 | `		/* Perform the insertion now */` |
|     87 | 4856 | `		zInput = (char *)SyBlobData(pWorker);` |
|     87 | 4857 | `		n = SyBlobLength(pWorker);` |
|   7253 | 4858 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     87 | 4859 | `		SyBlobLength(pWorker) += nReplen;` |
|     41 | 4860 | `	}` |
|     93 | 4861 | `	return SXRET_OK;` |
|     49 | 4862 | `}` |
|      - | 4863 | `/*` |
|      - | 4864 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 4865 | ` * to collect search/replace string.` |
|      - | 4866 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 4867 | ` */` |
|    194 | 4868 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      5 | 4869 | `{` |
|    199 | 4870 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 4871 | `	SyString sWorker;` |
|      - | 4872 | `	const char *zIn;` |
|      - | 4873 | `	int nByte;` |
|      - | 4874 | `	/* Extract a string representation of the given argument */` |
|    199 | 4875 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|    199 | 4876 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|    199 | 4877 | `	if( nByte > 0 ){` |
|      - | 4878 | `		char *zDup;` |
|      - | 4879 | `		/* Duplicate the chunk */` |
|    193 | 4880 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 4881 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 4882 | `			);` |
|    193 | 4883 | `		if( zDup == 0 ){` |
|      - | 4884 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 4885 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 4886 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 4887 | `			return SXERR_MEM;` |
|      - | 4888 | `		}` |
|    193 | 4889 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 4890 | `		/* Save the chunk */` |
|    193 | 4891 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     94 | 4892 | `	}` |
|      - | 4893 | `	/* Save for later processing */` |
|    199 | 4894 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 4895 | `	/* All done */` |
|     97 | 4896 | `	SXUNUSED(pKey); /* cc warning */` |
|    199 | 4897 | `	return PH7_OK;` |
|    102 | 4898 | `}` |
|      - | 4899 | `/*` |
|      - | 4900 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 4901 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 4902 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 4903 | ` * Parameters` |
|      - | 4904 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 4905 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 4906 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 4907 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 4908 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 4909 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 4910 | ` * $search` |
|      - | 4911 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 4912 | ` *  to designate multiple needles.` |
|      - | 4913 | ` * $replace` |
|      - | 4914 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 4915 | ` *  to designate multiple replacements.` |
|      - | 4916 | ` * $subject` |
|      - | 4917 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 4918 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 4919 | ` *  of subject, and the return value is an array as well.` |
|      - | 4920 | ` * $count (Not used)` |
|      - | 4921 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 4922 | ` * Return` |
|      - | 4923 | ` * This function returns a string or an array with the replaced values.` |
|      - | 4924 | ` */` |
|  30068 | 4925 | `PH7_PRIVATE int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 4926 | `{` |
|      - | 4927 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 4928 | `	ProcStringMatch xMatch;` |
|      - | 4929 | `	const char *zIn,*zFunc;` |
|      - | 4930 | `	str_replace_data sRep;` |
|      - | 4931 | `	SyBlob sWorker;` |
|      - | 4932 | `	SySet sReplace;` |
|      - | 4933 | `	SySet sSearch;` |
|      - | 4934 | `	int rep_str;` |
|      - | 4935 | `	int nByte;` |
|      - | 4936 | `	sxi32 rc;` |
|  30073 | 4937 | `	if( nArg < 3 ){` |
|      - | 4938 | `		/* Missing/Invalid arguments,return null */` |
|    ! 0 | 4939 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4940 | `		return PH7_OK;` |
|      - | 4941 | `	}` |
|      - | 4942 | `	/* Initialize fields */` |
|  30073 | 4943 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  30073 | 4944 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  30073 | 4945 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  30073 | 4946 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  30073 | 4947 | `	sRep.pCtx = pCtx;` |
|  30073 | 4948 | `	sRep.pCollector = &sSearch;` |
|  30073 | 4949 | `	rep_str = 0;` |
|      - | 4950 | `	/* Extract the subject */` |
|  30073 | 4951 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  30073 | 4952 | `	if( nByte < 1 ){` |
|      - | 4953 | `		/* Nothing to replace,return the empty string */` |
|     21 | 4954 | `		ph7_result_string(pCtx,"",0);` |
|     21 | 4955 | `		return PH7_OK;` |
|      - | 4956 | `	}` |
|      - | 4957 | `	/* Copy the subject */` |
|  30053 | 4958 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 4959 | `	/* Search string */` |
|  30053 | 4960 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 4961 | `		/* Collect search string */` |
|     93 | 4962 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|     49 | 4963 | `	}else{` |
|      - | 4964 | `		/* Single pattern */` |
|  29965 | 4965 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  29965 | 4966 | `		if( nByte < 1 ){` |
|      - | 4967 | `			/* Return the subject untouched since no search string is available */` |
|      7 | 4968 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      7 | 4969 | `			return PH7_OK;` |
|      - | 4970 | `		}` |
|  29959 | 4971 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 4972 | `		/* Save for later processing */` |
|  29959 | 4973 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 4974 | `	}` |
|      - | 4975 | `	/* Replace string */` |
|  30047 | 4976 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 4977 | `		/* Collect replace string */` |
|      9 | 4978 | `		sRep.pCollector = &sReplace;` |
|      9 | 4979 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      5 | 4980 | `	}else{` |
|      - | 4981 | `		/* Single needle */` |
|  30039 | 4982 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  30039 | 4983 | `		rep_str = 1;` |
|  30039 | 4984 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 4985 | `		/* Save for later processing */` |
|  30039 | 4986 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 4987 | `	}` |
|      - | 4988 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  30047 | 4989 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 4990 | `		SySetRelease(&sSearch);` |
|    ! 0 | 4991 | `		SySetRelease(&sReplace);` |
|    ! 0 | 4992 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 4993 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4994 | `	}` |
|      - | 4995 | `	/* Reset loop cursors */` |
|  30047 | 4996 | `	SySetResetCursor(&sSearch);` |
|  30047 | 4997 | `	SySetResetCursor(&sReplace);` |
|  30047 | 4998 | `	pReplace = pSearch = 0; /* cc warning */` |
|  30047 | 4999 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 5000 | `	/* Extract function name */` |
|  30047 | 5001 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 5002 | `	/* Set the default pattern match routine */` |
|  30047 | 5003 | `	xMatch = SyBlobSearch;` |
|  30047 | 5004 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 5005 | `		/* Case insensitive pattern match */` |
|     11 | 5006 | `		xMatch = iPatternMatch;` |
|      5 | 5007 | `	}` |
|      - | 5008 | `	/* Start the replace process */` |
|  60179 | 5009 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 5010 | `		sxu32 nCount,nOfft;` |
|      - | 5011 | `		/* Extract the replace string */` |
|  30137 | 5012 | `		if( rep_str ){` |
|  30119 | 5013 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  15062 | 5014 | `		}else{` |
|     19 | 5015 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 5016 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 5017 | `				 * An empty string is used for the rest of replacement values` |
|      - | 5018 | `				 */` |
|      3 | 5019 | `				pReplace = 0;` |
|      1 | 5020 | `			}` |
|      - | 5021 | `		}` |
|  30137 | 5022 | `		if( pReplace == 0 ){` |
|      - | 5023 | `			/* Use an empty string instead */` |
|      3 | 5024 | `			pReplace = &sTemp;` |
|      1 | 5025 | `		}` |
|  30137 | 5026 | `		if( pSearch->nByte <  1 ){` |
|      - | 5027 | `			/* php ignores an empty search string, but its replacement still` |
|      - | 5028 | `			 * CONSUMES a slot so the remaining pairs stay aligned. Skipping` |
|      - | 5029 | `			 * before the fetch above shifted every later replacement by one:` |
|      - | 5030 | `			 * str_replace(['','l'],['x','L'],'hello') answered "hexxo". */` |
|      7 | 5031 | `			continue;` |
|      - | 5032 | `		}` |
|  30131 | 5033 | `		nOfft = nCount = 0;` |
|  15107 | 5034 | `		for(;;){` |
|  30219 | 5035 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     15 | 5036 | `				break;` |
|      - | 5037 | `			}` |
|      - | 5038 | `			/* Perform a pattern lookup */` |
|  45305 | 5039 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  30200 | 5040 | `				pSearch->nByte,&nOfft);` |
|  30205 | 5041 | `			if( rc != SXRET_OK ){` |
|      - | 5042 | `				/* Pattern not found */` |
|  30117 | 5043 | `				break;` |
|      - | 5044 | `			}` |
|      - | 5045 | `			/* Perform the replace operation */` |
|     93 | 5046 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     93 | 5047 | `			if( rc != SXRET_OK ){` |
|      - | 5048 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 5049 | `				SySetRelease(&sSearch);` |
|    ! 0 | 5050 | `				SySetRelease(&sReplace);` |
|    ! 0 | 5051 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 5052 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 5053 | `			}` |
|      - | 5054 | `			/* Increment offset counter */` |
|     93 | 5055 | `			nCount += nOfft + pReplace->nByte;` |
|      5 | 5056 | `		}` |
|      5 | 5057 | `	}` |
|      - | 5058 | `	/* All done,clean-up the mess left behind */` |
|  30047 | 5059 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  30047 | 5060 | `	SySetRelease(&sSearch);` |
|  30047 | 5061 | `	SySetRelease(&sReplace);` |
|  30047 | 5062 | `	SyBlobRelease(&sWorker);` |
|  30047 | 5063 | `	if( rc != PH7_OK ){` |
|    ! 0 | 5064 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5065 | `	}` |
|  30047 | 5066 | `	return PH7_OK;` |
|  15039 | 5067 | `}` |
|      - | 5068 | `/*` |
|      - | 5069 | ` * strtr() array form: a single (key,value) pair copied out of the replace_pairs` |
|      - | 5070 | ` * array. The bytes are owned by a persistent pool (see strtr_collect) rather than` |
|      - | 5071 | ` * the transient walker values, which HashmapWalk releases after each callback, so` |
|      - | 5072 | ` * we store byte offsets into that pool instead of raw pointers.` |
|      - | 5073 | ` */` |
|      - | 5074 | `typedef struct strtr_entry strtr_entry;` |
|      - | 5075 | `struct strtr_entry` |
|      - | 5076 | `{` |
|      - | 5077 | `	sxu32 nKeyOfft; /* Offset of the search key inside the pool */` |
|      - | 5078 | `	sxu32 nKeyLen;  /* Length of the search key */` |
|      - | 5079 | `	sxu32 nValOfft; /* Offset of the replacement inside the pool */` |
|      - | 5080 | `	sxu32 nValLen;  /* Length of the replacement */` |
|      - | 5081 | `};` |
|      - | 5082 | `typedef struct strtr_collect strtr_collect;` |
|      - | 5083 | `struct strtr_collect` |
|      - | 5084 | `{` |
|      - | 5085 | `	SyBlob *pPool;  /* Byte pool holding copied key + value bytes */` |
|      - | 5086 | `	SySet  *pTable; /* Set of strtr_entry (parallel offsets into pPool) */` |
|      - | 5087 | `	sxi32   rc;     /* Carries an allocation failure (SXERR_MEM) out of the walker */` |
|      - | 5088 | `	ph7_context *pCtx; /* Needed to warn about an empty key */` |
|      - | 5089 | `};` |
|      - | 5090 | `/*` |
|      - | 5091 | ` * Collect one replace_pairs entry into the persistent pool/offset table.` |
|      - | 5092 | ` * PHP coerces both the key and the value to string (an integer key becomes its` |
|      - | 5093 | ` * decimal form) and ignores an empty-string key.` |
|      - | 5094 | ` */` |
|     20 | 5095 | `static int StrtrCollectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 5096 | `{` |
|     21 | 5097 | `	strtr_collect *pCol = (strtr_collect *)pUserData;` |
|      - | 5098 | `	const char *zKey,*zVal;` |
|      - | 5099 | `	strtr_entry sEnt;` |
|      - | 5100 | `	int nKey,nVal;` |
|     21 | 5101 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     21 | 5102 | `	if( nKey < 1 ){` |
|      - | 5103 | `		/* PHP ignores an empty-string key, and warns that it did so. */` |
|      3 | 5104 | `		ph7_context_throw_error_format(pCol->pCtx,PH7_CTX_WARNING,` |
|      - | 5105 | `			"Ignoring replacement of empty string");` |
|      3 | 5106 | `		return PH7_OK;` |
|      - | 5107 | `	}` |
|     19 | 5108 | `	zVal = ph7_value_to_string(pData,&nVal);` |
|     19 | 5109 | `	sEnt.nKeyOfft = SyBlobLength(pCol->pPool);` |
|     19 | 5110 | `	sEnt.nKeyLen  = (sxu32)nKey;` |
|     19 | 5111 | `	if( SyBlobAppend(pCol->pPool,(const void *)zKey,(sxu32)nKey) != SXRET_OK ){` |
|    ! 0 | 5112 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 5113 | `		return SXERR_ABORT;` |
|      - | 5114 | `	}` |
|     19 | 5115 | `	sEnt.nValOfft = SyBlobLength(pCol->pPool);` |
|     19 | 5116 | `	sEnt.nValLen  = (sxu32)nVal;` |
|     19 | 5117 | `	if( nVal > 0 && SyBlobAppend(pCol->pPool,(const void *)zVal,(sxu32)nVal) != SXRET_OK ){` |
|    ! 0 | 5118 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 5119 | `		return SXERR_ABORT;` |
|      - | 5120 | `	}` |
|     19 | 5121 | `	if( SySetPut(pCol->pTable,(const void *)&sEnt) != SXRET_OK ){` |
|    ! 0 | 5122 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 5123 | `		return SXERR_ABORT;` |
|      - | 5124 | `	}` |
|     19 | 5125 | `	return PH7_OK;` |
|     11 | 5126 | `}` |
|      - | 5127 | `/*` |
|      - | 5128 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 5129 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 5130 | ` *  Translate characters or replace substrings.` |
|      - | 5131 | ` * Parameters` |
|      - | 5132 | ` *  $str` |
|      - | 5133 | ` *  The string being translated.` |
|      - | 5134 | ` * $from` |
|      - | 5135 | ` *  The string being translated to to.` |
|      - | 5136 | ` * $to` |
|      - | 5137 | ` *  The string replacing from.` |
|      - | 5138 | ` * $replace_pairs` |
|      - | 5139 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 5140 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 5141 | ` * Return` |
|      - | 5142 | ` *  The translated string.` |
|      - | 5143 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 5144 | ` */` |
|     12 | 5145 | `PH7_PRIVATE int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5146 | `{` |
|      - | 5147 | `	const char *zIn;` |
|      - | 5148 | `	int nLen;` |
|     13 | 5149 | `	if( nArg < 1 ){` |
|      - | 5150 | `		/* Nothing to replace,return FALSE */` |
|    ! 0 | 5151 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5152 | `		return PH7_OK;` |
|      - | 5153 | `	}` |
|     13 | 5154 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 5155 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 5156 | `		/* Invalid arguments */` |
|    ! 0 | 5157 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5158 | `		return PH7_OK;` |
|      - | 5159 | `	}` |
|     18 | 5160 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 5161 | `		strtr_collect sCol;` |
|      - | 5162 | `		SyBlob sPool,sWorker;` |
|      - | 5163 | `		SySet sTable;` |
|      - | 5164 | `		const char *zPool;` |
|      - | 5165 | `		strtr_entry *pEnt;` |
|      - | 5166 | `		sxi32 rc;` |
|      - | 5167 | `		int i,iRun;` |
|      - | 5168 | `		/*` |
|      - | 5169 | `		 * PHP's array-form strtr is a single left-to-right pass over the subject:` |
|      - | 5170 | `		 * at every position it substitutes the LONGEST replace_pairs key that` |
|      - | 5171 | `		 * matches there, then advances past the key (replacements are never` |
|      - | 5172 | `		 * rescanned). It is not a sequential per-key global replace. First copy` |
|      - | 5173 | `		 * the pairs into a persistent pool, then run that scan.` |
|      - | 5174 | `		 */` |
|     11 | 5175 | `		SyBlobInit(&sPool,&pCtx->pVm->sAllocator);` |
|     11 | 5176 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     11 | 5177 | `		SySetInit(&sTable,&pCtx->pVm->sAllocator,sizeof(strtr_entry));` |
|     11 | 5178 | `		sCol.pPool  = &sPool;` |
|     11 | 5179 | `		sCol.pTable = &sTable;` |
|     11 | 5180 | `		sCol.rc     = SXRET_OK;` |
|     11 | 5181 | `		sCol.pCtx   = pCtx;` |
|     11 | 5182 | `		ph7_array_walk(apArg[1],StrtrCollectWalker,&sCol);` |
|     11 | 5183 | `		if( sCol.rc != SXRET_OK ){` |
|      - | 5184 | `			/* Allocation failure while collecting the pairs: surface a fatal */` |
|    ! 0 | 5185 | `			SyBlobRelease(&sPool);` |
|    ! 0 | 5186 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 5187 | `			SySetRelease(&sTable);` |
|    ! 0 | 5188 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 5189 | `		}` |
|      - | 5190 | `		/* The pool is now stable, so offsets can be resolved against its base. */` |
|     11 | 5191 | `		zPool = (const char *)SyBlobData(&sPool);` |
|     11 | 5192 | `		rc = SXRET_OK;` |
|     11 | 5193 | `		iRun = 0; /* Start of the pending run of unmatched bytes copied verbatim. */` |
|     43 | 5194 | `		for( i = 0 ; i < nLen ; ){` |
|     33 | 5195 | `			strtr_entry *pBest = 0;` |
|     33 | 5196 | `			sxu32 nBest = 0;` |
|      - | 5197 | `			/* Pick the longest key that matches at the current position. */` |
|     33 | 5198 | `			SySetResetCursor(&sTable);` |
|     87 | 5199 | `			while( SXRET_OK == SySetGetNextEntry(&sTable,(void **)&pEnt) ){` |
|     54 | 5200 | `				if( pEnt->nKeyLen > nBest` |
|     50 | 5201 | `					&& pEnt->nKeyLen <= (sxu32)(nLen - i)` |
|     46 | 5202 | `					&& SyMemcmp(zPool + pEnt->nKeyOfft,zIn + i,pEnt->nKeyLen) == 0 ){` |
|     29 | 5203 | `					nBest = pEnt->nKeyLen;` |
|     29 | 5204 | `					pBest = pEnt;` |
|     14 | 5205 | `				}` |
|      1 | 5206 | `			}` |
|     33 | 5207 | `			if( pBest == 0 ){` |
|      - | 5208 | `				/* No key here: extend the literal run and copy it in one shot later. */` |
|      9 | 5209 | `				i++;` |
|      9 | 5210 | `				continue;` |
|      - | 5211 | `			}` |
|      - | 5212 | `			/* Flush the pending literal run, then the replacement. */` |
|     25 | 5213 | `			if( i > iRun ){` |
|      5 | 5214 | `				rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(i - iRun));` |
|      2 | 5215 | `			}` |
|     25 | 5216 | `			if( rc == SXRET_OK && pBest->nValLen > 0 ){` |
|     25 | 5217 | `				rc = SyBlobAppend(&sWorker,zPool + pBest->nValOfft,pBest->nValLen);` |
|     12 | 5218 | `			}` |
|     25 | 5219 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 5220 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 5221 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 5222 | `				SySetRelease(&sTable);` |
|    ! 0 | 5223 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 5224 | `			}` |
|     25 | 5225 | `			i += (int)pBest->nKeyLen;` |
|     25 | 5226 | `			iRun = i;` |
|      1 | 5227 | `		}` |
|      - | 5228 | `		/* Flush the trailing literal run. */` |
|     11 | 5229 | `		if( nLen > iRun ){` |
|      3 | 5230 | `			rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(nLen - iRun));` |
|      3 | 5231 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 5232 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 5233 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 5234 | `				SySetRelease(&sTable);` |
|    ! 0 | 5235 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 5236 | `			}` |
|      1 | 5237 | `		}` |
|      - | 5238 | `		/* All done, return the result string */` |
|     16 | 5239 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|     10 | 5240 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 5241 | `		/* Clean-up */` |
|     11 | 5242 | `		SyBlobRelease(&sPool);` |
|     11 | 5243 | `		SyBlobRelease(&sWorker);` |
|     11 | 5244 | `		SySetRelease(&sTable);` |
|     11 | 5245 | `		if( rc != PH7_OK ){` |
|    ! 0 | 5246 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 5247 | `		}` |
|      6 | 5248 | `	}else{` |
|      - | 5249 | `		int i,flen,tlen,c,iOfft;` |
|      - | 5250 | `		const char *zFrom,*zTo;` |
|      3 | 5251 | `		if( nArg < 3 ){` |
|      - | 5252 | `			/* Nothing to replace */` |
|    ! 0 | 5253 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5254 | `			return PH7_OK;` |
|      - | 5255 | `		}` |
|      - | 5256 | `		/* Extract given arguments */` |
|      3 | 5257 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 5258 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 5259 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 5260 | `			/* Nothing to replace */` |
|    ! 0 | 5261 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5262 | `			return PH7_OK;` |
|      - | 5263 | `		}` |
|      - | 5264 | `		/* Start the replace process */` |
|     13 | 5265 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 5266 | `			c = zIn[i];` |
|     11 | 5267 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 5268 | `				if ( iOfft < tlen ){` |
|      5 | 5269 | `					c = zTo[iOfft];` |
|      2 | 5270 | `				}` |
|      2 | 5271 | `			}` |
|     11 | 5272 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 5273 |  |
|      6 | 5274 | `		}` |
|      - | 5275 | `	}` |
|     13 | 5276 | `	return PH7_OK;` |
|      7 | 5277 | `}` |
|      - | 5278 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 5279 |  |
