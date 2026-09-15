# src/ph7/builtin_string.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2310/2692 lines (85.81%)

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
| 277577 |   60 | `PH7_PRIVATE int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |   61 | `{` |
|      - |   62 | `	const char *zSource;` |
|      - |   63 | `	int nSrcLen;` |
|      - |   64 | `	sxi64 iStart,iEnd;` |
| 277582 |   65 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"substr",1,"$string"); }` |
| 277582 |   66 | `	if( nArg < 2 ){` |
|      - |   67 | `		/* Arity is enforced at the call boundary; nothing sensible to return here. */` |
|    ! 0 |   68 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 |   69 | `		return PH7_OK;` |
|      - |   70 | `	}` |
|      - |   71 | `	/* Extract the target string */` |
| 277582 |   72 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|      - |   73 | `	/* Extract the offset */` |
|      - |   74 | `	{` |
| 277582 |   75 | `		sxi64 iTmp = 0;` |
| 277582 |   76 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"substr",2,"$offset","int",&iTmp);` |
| 277582 |   77 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |   78 | `			return rcArg;` |
|      - |   79 | `		}` |
| 277582 |   80 | `		iStart = iTmp;` |
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
| 277582 |   92 | `	if( iStart < 0 ){` |
|  32975 |   93 | `		iStart += nSrcLen;` |
|  32975 |   94 | `		if( iStart < 0 ){` |
|      5 |   95 | `			iStart = 0;` |
|      7 |   96 | `		}` |
| 261097 |   97 | `	}else if( iStart > nSrcLen ){` |
|      7 |   98 | `		iStart = nSrcLen;` |
|      3 |   99 | `	}` |
| 277582 |  100 | `	iEnd = nSrcLen;` |
| 277582 |  101 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
| 196703 |  102 | `		sxi64 iLen = 0;` |
| 196703 |  103 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],"substr",3,"$length","?int",&iLen);` |
| 196703 |  104 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  105 | `			return rcArg;` |
|      - |  106 | `		}` |
| 196703 |  107 | `		if( iLen < 0 ){` |
|  32607 |  108 | `			iEnd = (sxi64)nSrcLen + iLen;` |
| 180402 |  109 | `		}else if( iLen > (sxi64)nSrcLen - iStart ){` |
|  18439 |  110 | `			iEnd = nSrcLen;` |
|   9222 |  111 | `		}else{` |
| 145667 |  112 | `			iEnd = iStart + iLen;` |
|      - |  113 | `		}` |
|  98349 |  114 | `	}` |
| 277582 |  115 | `	if( iEnd < iStart ){` |
|      3 |  116 | `		iEnd = iStart;` |
|      1 |  117 | `	}` |
| 277582 |  118 | `	ph7_result_string(pCtx,&zSource[iStart],(int)(iEnd - iStart));` |
| 277582 |  119 | `	return PH7_OK;` |
| 138897 |  120 | `}` |
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
| 421325 |  319 | `static void StrNullArgNotice(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNum,const char *zParamName)` |
|      5 |  320 | `{` |
| 421330 |  321 | `	if( ph7_value_is_null(pArg) ){` |
|     13 |  322 | `		PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  323 | `			"%s(): Argument #%d (%s) must be of type string, null given",` |
|      4 |  324 | `			zFunc,iArgNum,zParamName);` |
|      4 |  325 | `	}` |
| 421330 |  326 | `}` |
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
|     20 | 1101 | `PH7_PRIVATE int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1102 | `{` |
|      - | 1103 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1104 | `	int nLen;` |
|      - | 1105 | `	/* PHP enforces exactly one argument. */` |
|     23 | 1106 | `	if( nArg != 1 ){` |
|      4 | 1107 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1108 | `			"ArgumentCountError",` |
|      - | 1109 | `			"addslashes() expects exactly 1 argument, %d given",` |
|      1 | 1110 | `			nArg` |
|      - | 1111 | `			);` |
|      - | 1112 | `	}` |
|      - | 1113 | `	/* php only DEPRECATES null here; PHL rejects it. */` |
|     20 | 1114 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 1115 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
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
|     13 | 1164 | `}` |
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
|     32 | 1235 | `PH7_PRIVATE int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1236 | `{` |
|      - | 1237 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - | 1238 | `	char aMask[256];` |
|      - | 1239 | `	int nLen,nMask;` |
|      - | 1240 | `	/* PHP enforces exactly two arguments. */` |
|     36 | 1241 | `	if( nArg != 2 ){` |
|      4 | 1242 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1243 | `			"ArgumentCountError",` |
|      - | 1244 | `			"addcslashes() expects exactly 2 arguments, %d given",` |
|      1 | 1245 | `			nArg` |
|      - | 1246 | `			);` |
|      - | 1247 | `	}` |
|      - | 1248 | `	/* php only DEPRECATES null here; PHL rejects it. */` |
|     34 | 1249 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 1250 | `		return PH7_VmThrowException(pCtx,` |
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
|     20 | 1332 | `}` |
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
|      - | 1413 | `	/* Encode the string */` |
|      4 | 1414 | `	for(;;){` |
|      9 | 1415 | `		if( zIn >= zEnd ){` |
|      - | 1416 | `			/* No more input */` |
|      5 | 1417 | `			break;` |
|      - | 1418 | `		}` |
|      5 | 1419 | `		zCur = zIn;` |
|     17 | 1420 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 | 1421 | `			zIn++;` |
|      1 | 1422 | `		}` |
|      5 | 1423 | `		if( zIn > zCur ){` |
|      - | 1424 | `			/* Append raw contents */` |
|      5 | 1425 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 1426 | `		}` |
|      5 | 1427 | `		if( &zIn[1] < zEnd ){` |
|      3 | 1428 | `			int c = zIn[1];` |
|      3 | 1429 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - | 1430 | `				/* Ignore the backslash */` |
|      3 | 1431 | `				zIn++;` |
|      1 | 1432 | `			}` |
|      2 | 1433 | `		}else{` |
|      3 | 1434 | `			break;` |
|      - | 1435 | `		}` |
|      1 | 1436 | `	}` |
|      7 | 1437 | `	return PH7_OK;` |
|      4 | 1438 | `}` |
|      - | 1439 | `/*` |
|      - | 1440 | ` * UTF-8-aware HTML entity machinery, shared by htmlspecialchars/htmlentities/` |
|      - | 1441 | ` * htmlspecialchars_decode/html_entity_decode/get_html_translation_table.` |
|      - | 1442 | ` * The implementations live further down in this file, next to the filter_var` |
|      - | 1443 | ` * FULL_SPECIAL_CHARS machinery they reuse (aHtml401Ent[]/FvHtml401Lookup()/` |
|      - | 1444 | ` * FvUtf8Next()). Semantics are byte-exact vs php 8.5.7; PHL is UTF-8-only` |
|      - | 1445 | ` * so every charset argument other than a UTF-8 alias gets PHP's` |
|      - | 1446 | ` * unsupported-charset warning and is treated as UTF-8.` |
|      - | 1447 | ` *` |
|      - | 1448 | ` * Flag model (the PHP-exact ENT_* values, see constant.c): bit 1 = encode/` |
|      - | 1449 | ` * decode single quotes, bit 2 = double quotes (ENT_QUOTES=3, ENT_COMPAT=2,` |
|      - | 1450 | ` * ENT_NOQUOTES=0); bits 16\|32 select the doctype (0=HTML401, 16=XML1,` |
|      - | 1451 | ` * 32=XHTML, 48=HTML5); ENT_IGNORE=4 drops invalid UTF-8 bytes (wins over` |
|      - | 1452 | ` * ENT_SUBSTITUTE=8, which replaces each with U+FFFD; with neither set the` |
|      - | 1453 | ` * whole result collapses to ""); ENT_DISALLOWED=128 substitutes valid but` |
|      - | 1454 | ` * doctype-disallowed codepoints. The shared default is` |
|      - | 1455 | ` * ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 = 11.` |
|      - | 1456 | ` */` |
|      - | 1457 | `/*` |
|      - | 1458 | ` * string htmlspecialchars(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1459 | ` *                         [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1460 | ` *  Convert the special characters & < > " ' to HTML entities.` |
|      - | 1461 | ` * Return` |
|      - | 1462 | ` *  The escaped string or NULL on failure.` |
|      - | 1463 | ` */` |
|     42 | 1464 | `PH7_PRIVATE int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1465 | `{` |
|     43 | 1466 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1467 | `	const char *zIn;` |
|     43 | 1468 | `	int nLen,bDouble = 1;` |
|      - | 1469 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1470 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     43 | 1471 | `	if( nArg < 1 ){` |
|      - | 1472 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1473 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1474 | `		return PH7_OK;` |
|      - | 1475 | `	}` |
|      - | 1476 | `	/* Extract the target string */` |
|     43 | 1477 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     43 | 1478 | `	if( nArg > 1 ){` |
|     35 | 1479 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     17 | 1480 | `	}` |
|     43 | 1481 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     43 | 1482 | `	if( nArg > 3 ){` |
|      7 | 1483 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      3 | 1484 | `	}` |
|     43 | 1485 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,0,bDouble);` |
|     43 | 1486 | `	return PH7_OK;` |
|     22 | 1487 | `}` |
|      - | 1488 | `/*` |
|      - | 1489 | ` * string htmlspecialchars_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401])` |
|      - | 1490 | ` *  Convert the special HTML entities (&amp; &lt; &gt; &quot; and the` |
|      - | 1491 | ` *  numeric/doctype forms of the two quotes) back to characters.` |
|      - | 1492 | ` * Return` |
|      - | 1493 | ` *  The unescaped string or NULL on failure.` |
|      - | 1494 | ` */` |
|     22 | 1495 | `PH7_PRIVATE int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1496 | `{` |
|     23 | 1497 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1498 | `	const char *zIn;` |
|      - | 1499 | `	int nLen;` |
|      - | 1500 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1501 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     23 | 1502 | `	if( nArg < 1 ){` |
|      - | 1503 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1504 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1505 | `		return PH7_OK;` |
|      - | 1506 | `	}` |
|      - | 1507 | `	/* Extract the target string */` |
|     23 | 1508 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 1509 | `	if( nArg > 1 ){` |
|      9 | 1510 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1511 | `	}` |
|     23 | 1512 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,0);` |
|     23 | 1513 | `	return PH7_OK;` |
|     12 | 1514 | `}` |
|      - | 1515 | `/*` |
|      - | 1516 | ` * array get_html_translation_table(int $table = HTML_SPECIALCHARS` |
|      - | 1517 | ` *      [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 [, string $encoding = "UTF-8"]])` |
|      - | 1518 | ` *  Return the translation table used by htmlspecialchars() (HTML_SPECIALCHARS)` |
|      - | 1519 | ` *  or htmlentities() (HTML_ENTITIES) as character => entity pairs.` |
|      - | 1520 | ` * Return` |
|      - | 1521 | ` *  The translation table as an array or NULL on failure.` |
|      - | 1522 | ` */` |
|     12 | 1523 | `PH7_PRIVATE int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1524 | `{` |
|     13 | 1525 | `	int iTable = 0; /* HTML_SPECIALCHARS */` |
|     13 | 1526 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|     13 | 1527 | `	if( nArg > 0 ){` |
|     11 | 1528 | `		iTable = ph7_value_to_int(apArg[0]);` |
|      5 | 1529 | `	}` |
|     13 | 1530 | `	if( nArg > 1 ){` |
|      9 | 1531 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1532 | `	}` |
|     13 | 1533 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     13 | 1534 | `	HtmlTranslationTable(pCtx,iTable,iFlags);` |
|     13 | 1535 | `	return PH7_OK;` |
|      1 | 1536 | `}` |
|      - | 1537 | `/*` |
|      - | 1538 | ` * string htmlentities(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1539 | ` *                     [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1540 | ` *  Convert all applicable characters to HTML entities: the specials plus` |
|      - | 1541 | ` *  every codepoint with an HTML 4.01 named entity (aHtml401Ent[]).` |
|      - | 1542 | ` * Return` |
|      - | 1543 | ` *  The encoded string or NULL on failure.` |
|      - | 1544 | ` */` |
|     30 | 1545 | `PH7_PRIVATE int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1546 | `{` |
|     31 | 1547 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1548 | `	const char *zIn;` |
|     31 | 1549 | `	int nLen,bDouble = 1;` |
|      - | 1550 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1551 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     31 | 1552 | `	if( nArg < 1 ){` |
|      - | 1553 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1554 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1555 | `		return PH7_OK;` |
|      - | 1556 | `	}` |
|      - | 1557 | `	/* Extract the target string */` |
|     31 | 1558 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 1559 | `	if( nArg > 1 ){` |
|     19 | 1560 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1561 | `	}` |
|     31 | 1562 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     31 | 1563 | `	if( nArg > 3 ){` |
|      3 | 1564 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      1 | 1565 | `	}` |
|     31 | 1566 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,1,bDouble);` |
|     31 | 1567 | `	return PH7_OK;` |
|     16 | 1568 | `}` |
|      - | 1569 | `/*` |
|      - | 1570 | ` * string html_entity_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1571 | ` *                           [, string $encoding = "UTF-8"]])` |
|      - | 1572 | ` *  Convert HTML entities (named — case-sensitive — and numeric, decimal or` |
|      - | 1573 | ` *  hex) back to their UTF-8 characters. The reverse of htmlentities().` |
|      - | 1574 | ` * Return` |
|      - | 1575 | ` *  The decoded string or NULL on failure.` |
|      - | 1576 | ` */` |
|     58 | 1577 | `PH7_PRIVATE int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1578 | `{` |
|     59 | 1579 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1580 | `	const char *zIn;` |
|      - | 1581 | `	int nLen;` |
|      - | 1582 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1583 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     59 | 1584 | `	if( nArg < 1 ){` |
|      - | 1585 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1586 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1587 | `		return PH7_OK;` |
|      - | 1588 | `	}` |
|      - | 1589 | `	/* Extract the target string */` |
|     59 | 1590 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     59 | 1591 | `	if( nArg > 1 ){` |
|     27 | 1592 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     13 | 1593 | `	}` |
|     59 | 1594 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     59 | 1595 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,1);` |
|     59 | 1596 | `	return PH7_OK;` |
|     30 | 1597 | `}` |
|      - | 1598 | `/*` |
|      - | 1599 | ` * int strlen($string)` |
|      - | 1600 | ` *  return the length of the given string.` |
|      - | 1601 | ` * Parameter` |
|      - | 1602 | ` *  string: The string being measured for length.` |
|      - | 1603 | ` * Return` |
|      - | 1604 | ` *  length of the given string.` |
|      - | 1605 | ` */` |
|  92862 | 1606 | `PH7_PRIVATE int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1607 | `{` |
|  92867 | 1608 | `	int iLen = 0;` |
|  92867 | 1609 | `	if( nArg > 0 ){` |
|  92867 | 1610 | `		StrNullArgNotice(pCtx,apArg[0],"strlen",1,"$string");` |
|  92867 | 1611 | `		ph7_value_to_string(apArg[0],&iLen);` |
|  46638 | 1612 | `	}` |
|      - | 1613 | `	/* String length */` |
|  92867 | 1614 | `	ph7_result_int(pCtx,iLen);` |
|  92867 | 1615 | `	return PH7_OK;` |
|      5 | 1616 | `}` |
|      - | 1617 | `/*` |
|      - | 1618 | ` * int strcmp(string $str1,string $str2)` |
|      - | 1619 | ` *  Perform a binary safe string comparison.` |
|      - | 1620 | ` * Parameter` |
|      - | 1621 | ` *  str1: The first string` |
|      - | 1622 | ` *  str2: The second string` |
|      - | 1623 | ` * Return` |
|      - | 1624 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1625 | ` *  than str2, and 0 if they are equal.` |
|      - | 1626 | ` */` |
|     72 | 1627 | `PH7_PRIVATE int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1628 | `{` |
|      - | 1629 | `	const char *z1,*z2;` |
|      - | 1630 | `	int n1,n2;` |
|      - | 1631 | `	int res;` |
|     73 | 1632 | `	if( nArg < 2 ){` |
|    ! 0 | 1633 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 1634 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 1635 | `		return PH7_OK;` |
|      - | 1636 | `	}` |
|      - | 1637 | `	/* Perform the comparison */` |
|     73 | 1638 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     73 | 1639 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     73 | 1640 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1641 | `	/* Comparison result */` |
|     73 | 1642 | `	ph7_result_int(pCtx,res);` |
|     73 | 1643 | `	return PH7_OK;` |
|     37 | 1644 | `}` |
|      - | 1645 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 1646 | `/*` |
|      - | 1647 | ` * The natural-order comparison core lives OUTSIDE the PH7_DISABLE_BUILTIN_FUNC` |
|      - | 1648 | ` * guard: hashmap.c's SORT_NATURAL path (always compiled) calls PH7_StrNatCmp, so` |
|      - | 1649 | ` * it must exist in the tiny build too. [[tiny-build-disk-io-guard-fragility]]` |
|      - | 1650 | ` */` |
|      - | 1651 | `/*` |
|      - | 1652 | ` * Natural-order comparison core (Martin Pool's natcompare as adapted by php's` |
|      - | 1653 | ` * ext/standard/strnatcmp.c): digit runs compare numerically — the longer run` |
|      - | 1654 | ` * wins, a leading zero flips to fractional first-difference-wins semantics —` |
|      - | 1655 | ` * everything else compares bytewise with whitespace skipped.` |
|      - | 1656 | ` */` |
|     42 | 1657 | `static int StrNatCompareRight(const char **pa,const char *aEnd,const char **pb,const char *bEnd)` |
|      1 | 1658 | `{` |
|     43 | 1659 | `	int bias = 0;` |
|     71 | 1660 | `	for(;;){` |
|     93 | 1661 | `		int da = (*pa < aEnd) && SyisDigit(**pa);` |
|     93 | 1662 | `		int db = (*pb < bEnd) && SyisDigit(**pb);` |
|     93 | 1663 | `		if( !da && !db ){ return bias; }` |
|     73 | 1664 | `		if( !da ){ return -1; }` |
|     63 | 1665 | `		if( !db ){ return 1; }` |
|     51 | 1666 | `		if( **pa < **pb ){ if( !bias ){ bias = -1; } }` |
|     37 | 1667 | `		else if( **pa > **pb ){ if( !bias ){ bias = 1; } }` |
|     51 | 1668 | `		(*pa)++;` |
|     51 | 1669 | `		(*pb)++;` |
|      1 | 1670 | `	}` |
|     22 | 1671 | `}` |
|      4 | 1672 | `static int StrNatCompareLeft(const char **pa,const char *aEnd,const char **pb,const char *bEnd)` |
|      1 | 1673 | `{` |
|      2 | 1674 | `	for(;;){` |
|      5 | 1675 | `		int da = (*pa < aEnd) && SyisDigit(**pa);` |
|      5 | 1676 | `		int db = (*pb < bEnd) && SyisDigit(**pb);` |
|      5 | 1677 | `		if( !da && !db ){ return 0; }` |
|      5 | 1678 | `		if( !da ){ return -1; }` |
|      5 | 1679 | `		if( !db ){ return 1; }` |
|      5 | 1680 | `		if( **pa < **pb ){ return -1; }` |
|    ! 0 | 1681 | `		if( **pa > **pb ){ return 1; }` |
|    ! 0 | 1682 | `		(*pa)++;` |
|    ! 0 | 1683 | `		(*pb)++;` |
|    ! 0 | 1684 | `	}` |
|      3 | 1685 | `}` |
|     48 | 1686 | `PH7_PRIVATE int PH7_StrNatCmp(const char *zA,int nA,const char *zB,int nB,int bFold)` |
|      1 | 1687 | `{` |
|     49 | 1688 | `	const char *a = zA,*aEnd = &zA[nA];` |
|     49 | 1689 | `	const char *b = zB,*bEnd = &zB[nB];` |
|    146 | 1690 | `	for(;;){` |
|      - | 1691 | `		int ca,cb;` |
|    175 | 1692 | `		while( a < aEnd && SyisSpace(a[0]) ){ a++; }` |
|    173 | 1693 | `		while( b < bEnd && SyisSpace(b[0]) ){ b++; }` |
|    173 | 1694 | `		ca = (a < aEnd) ? (unsigned char)a[0] : 0;` |
|    173 | 1695 | `		cb = (b < bEnd) ? (unsigned char)b[0] : 0;` |
|    173 | 1696 | `		if( SyisDigit(ca) && SyisDigit(cb) ){` |
|     45 | 1697 | `			int r = (ca == '0' \|\| cb == '0')` |
|      4 | 1698 | `				? StrNatCompareLeft(&a,aEnd,&b,bEnd)` |
|     65 | 1699 | `				: StrNatCompareRight(&a,aEnd,&b,bEnd);` |
|     47 | 1700 | `			if( r ){ return r; }` |
|      5 | 1701 | `			continue;` |
|      - | 1702 | `		}` |
|    127 | 1703 | `		if( ca == 0 && cb == 0 ){ return 0; }` |
|    121 | 1704 | `		if( bFold ){` |
|     67 | 1705 | `			ca = SyToLower(ca);` |
|     67 | 1706 | `			cb = SyToLower(cb);` |
|     33 | 1707 | `		}` |
|    121 | 1708 | `		if( ca < cb ){ return -1; }` |
|    121 | 1709 | `		if( ca > cb ){ return 1; }` |
|    121 | 1710 | `		a++;` |
|    121 | 1711 | `		b++;` |
|      1 | 1712 | `	}` |
|     25 | 1713 | `}` |
|      - | 1714 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 1715 | `/*` |
|      - | 1716 | ` * int strnatcmp(string $string1, string $string2)` |
|      - | 1717 | ` * int strnatcasecmp(string $string1, string $string2)` |
|      - | 1718 | ` *  Natural-order string comparison ("img2" < "img10"), case folded for the` |
|      - | 1719 | ` *  latter. php 8.2+ normalizes the result to -1/0/1.` |
|      - | 1720 | ` */` |
|     20 | 1721 | `PH7_PRIVATE int PH7_builtin_strnatcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1722 | `{` |
|      - | 1723 | `	const char *z1,*z2,*zFunc;` |
|      - | 1724 | `	int n1,n2,bFold;` |
|     21 | 1725 | `	if( nArg < 2 ){` |
|    ! 0 | 1726 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 1727 | `		return PH7_OK;` |
|      - | 1728 | `	}` |
|     21 | 1729 | `	zFunc = ph7_function_name(pCtx);` |
|     21 | 1730 | `	bFold = zFunc[sizeof("strnat")-1] == 'c'; /* strnatCasecmp */` |
|     21 | 1731 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     21 | 1732 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     21 | 1733 | `	ph7_result_int(pCtx,PH7_StrNatCmp(z1,n1,z2,n2,bFold));` |
|     21 | 1734 | `	return PH7_OK;` |
|     11 | 1735 | `}` |
|      - | 1736 | `/*` |
|      - | 1737 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 1738 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 1739 | ` * Parameter` |
|      - | 1740 | ` *  str1: The first string` |
|      - | 1741 | ` *  str2: The second string` |
|      - | 1742 | ` * Return` |
|      - | 1743 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1744 | ` *  than str2, and 0 if they are equal.` |
|      - | 1745 | ` */` |
|    380 | 1746 | `PH7_PRIVATE int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1747 | `{` |
|      - | 1748 | `	const char *z1,*z2;` |
|      - | 1749 | `	int res;` |
|      - | 1750 | `	int n;` |
|    382 | 1751 | `	if( nArg < 3 ){` |
|      - | 1752 | `		/* Perform a standard comparison */` |
|    ! 0 | 1753 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 1754 | `	}` |
|      - | 1755 | `	/* Desired comparison length */` |
|    382 | 1756 | `	n  = ph7_value_to_int(apArg[2]);` |
|    382 | 1757 | `	if( n < 0 ){` |
|      - | 1758 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 1759 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1760 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 1761 | `			ph7_function_name(pCtx));` |
|      - | 1762 | `	}` |
|      - | 1763 | `	/* Perform the comparison */` |
|    380 | 1764 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|    380 | 1765 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|    380 | 1766 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 1767 | `	/* Comparison result */` |
|    380 | 1768 | `	ph7_result_int(pCtx,res);` |
|    380 | 1769 | `	return PH7_OK;` |
|    192 | 1770 | `}` |
|      - | 1771 | `/*` |
|      - | 1772 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 1773 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 1774 | ` * Parameter` |
|      - | 1775 | ` *  str1: The first string` |
|      - | 1776 | ` *  str2: The second string` |
|      - | 1777 | ` * Return` |
|      - | 1778 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1779 | ` *  than str2, and 0 if they are equal.` |
|      - | 1780 | ` */` |
|    152 | 1781 | `PH7_PRIVATE int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1782 | `{` |
|      - | 1783 | `	const char *z1,*z2;` |
|      - | 1784 | `	int n1,n2;` |
|      - | 1785 | `	int res;` |
|    153 | 1786 | `	if( nArg < 2 ){` |
|    ! 0 | 1787 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 1788 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 1789 | `		return PH7_OK;` |
|      - | 1790 | `	}` |
|      - | 1791 | `	/* Perform the comparison */` |
|    153 | 1792 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|    153 | 1793 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|    153 | 1794 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1795 | `	/* Comparison result */` |
|    153 | 1796 | `	ph7_result_int(pCtx,res);` |
|    153 | 1797 | `	return PH7_OK;` |
|     77 | 1798 | `}` |
|      - | 1799 | `/*` |
|      - | 1800 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 1801 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 1802 | ` * Parameter` |
|      - | 1803 | ` *  $str1: The first string` |
|      - | 1804 | ` *  $str2: The second string` |
|      - | 1805 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 1806 | ` * Return` |
|      - | 1807 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1808 | ` *  than str2, and 0 if they are equal.` |
|      - | 1809 | ` */` |
|     46 | 1810 | `PH7_PRIVATE int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1811 | `{` |
|      - | 1812 | `	const char *z1,*z2;` |
|      - | 1813 | `	int res;` |
|      - | 1814 | `	int n;` |
|     51 | 1815 | `	if( nArg < 3 ){` |
|      - | 1816 | `		/* Perform a standard comparison */` |
|    ! 0 | 1817 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 1818 | `	}` |
|      - | 1819 | `	/* Desired comparison length */` |
|     51 | 1820 | `	n  = ph7_value_to_int(apArg[2]);` |
|     51 | 1821 | `	if( n < 0 ){` |
|      - | 1822 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 1823 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1824 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 1825 | `			ph7_function_name(pCtx));` |
|      - | 1826 | `	}` |
|      - | 1827 | `	/* Perform the comparison */` |
|     49 | 1828 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     49 | 1829 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     49 | 1830 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 1831 | `	/* Comparison result */` |
|     49 | 1832 | `	ph7_result_int(pCtx,res);` |
|     49 | 1833 | `	return PH7_OK;` |
|     28 | 1834 | `}` |
|      - | 1835 | `/*` |
|      - | 1836 | ` * Implode context [i.e: it's private data].` |
|      - | 1837 | ` * A pointer to the following structure is forwarded` |
|      - | 1838 | ` * verbatim to the array walker callback defined below.` |
|      - | 1839 | ` */` |
|      - | 1840 | `struct implode_data {` |
|      - | 1841 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 1842 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 1843 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 1844 | `	int nSeplen;          /* Separator length */` |
|      - | 1845 | `	int bFirst;           /* TRUE if first call */` |
|      - | 1846 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 1847 | `	sxi32 rc;             /* Captured allocation rc; SXERR_MEM => the builtin raises an OOM fatal */` |
|      - | 1848 | `};` |
|      - | 1849 | `/*` |
|      - | 1850 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 1851 | ` * The following routine is invoked for each array entry passed` |
|      - | 1852 | ` * to the implode() function.` |
|      - | 1853 | ` */` |
| 155382 | 1854 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 1855 | `{` |
|  77691 | 1856 | `	SXUNUSED(pKey);` |
| 155387 | 1857 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 1858 | `	const char *zData;` |
|      - | 1859 | `	int nLen;` |
| 155387 | 1860 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 1861 | `		if( pData->nSeplen > 0 ){` |
|      3 | 1862 | `			if( !pData->bFirst ){` |
|      - | 1863 | `				/* append the separator first */` |
|      3 | 1864 | `				if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 1865 | `					pData->rc = SXERR_MEM;` |
|    ! 0 | 1866 | `					return PH7_ABORT;` |
|      - | 1867 | `				}` |
|      2 | 1868 | `			}else{` |
|    ! 0 | 1869 | `				pData->bFirst = 0;` |
|      - | 1870 | `			}` |
|      1 | 1871 | `		}` |
|      - | 1872 | `		/* Recurse */` |
|      3 | 1873 | `		pData->bFirst = 1;` |
|      3 | 1874 | `		pData->nRecCount++;` |
|      3 | 1875 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 1876 | `		pData->nRecCount--;` |
|      - | 1877 | `		/* Propagate an allocation failure surfaced deeper in the recursion. */` |
|      3 | 1878 | `		if( pData->rc != SXRET_OK ){` |
|    ! 0 | 1879 | `			return PH7_ABORT;` |
|      - | 1880 | `		}` |
|      3 | 1881 | `		return PH7_OK;` |
|      - | 1882 | `	}` |
|      - | 1883 | `	/* Extract the string representation of the entry value */` |
| 155385 | 1884 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 1885 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 155385 | 1886 | `	if( pData->bFirst ){` |
|  33247 | 1887 | `		pData->bFirst = 0;` |
| 138764 | 1888 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 1889 | `		/* append the separator first */` |
| 122063 | 1890 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 1891 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1892 | `			return PH7_ABORT;` |
|      - | 1893 | `		}` |
|  61029 | 1894 | `	}` |
|      - | 1895 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 155385 | 1896 | `	if( nLen > 0 ){` |
| 143111 | 1897 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1898 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1899 | `			return PH7_ABORT;` |
|      - | 1900 | `		}` |
|  71553 | 1901 | `	}` |
| 155385 | 1902 | `	return PH7_OK;` |
|  77696 | 1903 | `}` |
|      - | 1904 | `/*` |
|      - | 1905 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 1906 | ` * string implode(array $pieces,...)` |
|      - | 1907 | ` *  Join array elements with a string.` |
|      - | 1908 | ` * $glue` |
|      - | 1909 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 1910 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 1911 | ` * $pieces` |
|      - | 1912 | ` *   The array of strings to implode.` |
|      - | 1913 | ` * Return` |
|      - | 1914 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 1915 | ` *  order, with the glue string between each element.` |
|      - | 1916 | ` */` |
|  33308 | 1917 | `PH7_PRIVATE int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1918 | `{` |
|      - | 1919 | `	struct implode_data imp_data;` |
|  33313 | 1920 | `	int i = 1;` |
|  33313 | 1921 | `	if( nArg < 1 ){` |
|      - | 1922 | `		/* Missing argument,return NULL */` |
|    ! 0 | 1923 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1924 | `		return PH7_OK;` |
|      - | 1925 | `	}` |
|      - | 1926 | `	/* Prepare the implode context */` |
|  33313 | 1927 | `	imp_data.pCtx = pCtx;` |
|  33313 | 1928 | `	imp_data.bRecursive = 0;` |
|  33313 | 1929 | `	imp_data.bFirst = 1;` |
|  33313 | 1930 | `	imp_data.nRecCount = 0;` |
|  33313 | 1931 | `	imp_data.rc = SXRET_OK;` |
|  33313 | 1932 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  33311 | 1933 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  33311 | 1934 | `		if( nArg > 1 && !ph7_value_is_array(apArg[1]) && !ph7_value_is_null(apArg[1]) ){` |
|      - | 1935 | `			/* php: implode($glue, $pieces) requires an ARRAY. PH7 stringified whatever it` |
|      - | 1936 | `			 * was handed, so implode(",", 5) quietly returned "5". */` |
|      - | 1937 | `			char zBuf[64];` |
|      4 | 1938 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1939 | `				"implode(): Argument #2 ($array) must be of type ?array, %s given",` |
|      2 | 1940 | `				VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 1941 | `		}` |
|  16657 | 1942 | `	}else{` |
|      3 | 1943 | `		imp_data.zSep = 0;` |
|      3 | 1944 | `		imp_data.nSeplen = 0;` |
|      3 | 1945 | `		i = 0;` |
|      - | 1946 | `	}` |
|  33311 | 1947 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 1948 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1949 | `	}` |
|      - | 1950 | `	/* Start the 'join' process */` |
|  66617 | 1951 | `	while( i < nArg ){` |
|  33311 | 1952 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1953 | `			/* Iterate throw array entries */` |
|  33311 | 1954 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 1955 | `			/* Surface a callback allocation failure as a fatal */` |
|  33311 | 1956 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 1957 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1958 | `			}` |
|  16658 | 1959 | `		}else{` |
|      - | 1960 | `			const char *zData;` |
|      - | 1961 | `			int nLen;` |
|      - | 1962 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 1963 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1964 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 1965 | `			if( imp_data.bFirst ){` |
|    ! 0 | 1966 | `				imp_data.bFirst = 0;` |
|    ! 0 | 1967 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1968 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 1969 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1970 | `				}` |
|    ! 0 | 1971 | `			}` |
|      - | 1972 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 1973 | `			if( nLen > 0 ){` |
|    ! 0 | 1974 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1975 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1976 | `				}` |
|    ! 0 | 1977 | `			}` |
|      - | 1978 | `		}` |
|  33311 | 1979 | `		i++;` |
|      5 | 1980 | `	}` |
|  33311 | 1981 | `	return PH7_OK;` |
|  16659 | 1982 | `}` |
|      - | 1983 | `/*` |
|      - | 1984 | ` * Symisc eXtension:` |
|      - | 1985 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 1986 | ` * Purpose` |
|      - | 1987 | ` *  Same as implode() but recurse on arrays.` |
|      - | 1988 | ` * Example:` |
|      - | 1989 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 1990 | ` *   echo implode_recursive("/",$a);` |
|      - | 1991 | ` *   Will output` |
|      - | 1992 | ` *     usr/home/dean.` |
|      - | 1993 | ` *   While the standard implode would produce.` |
|      - | 1994 | ` *    usr/Array.` |
|      - | 1995 | ` * Parameter` |
|      - | 1996 | ` *  Refer to implode().` |
|      - | 1997 | ` * Return` |
|      - | 1998 | ` *  Refer to implode().` |
|      - | 1999 | ` */` |
|     12 | 2000 | `PH7_PRIVATE int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2001 | `{` |
|      - | 2002 | `	struct implode_data imp_data;` |
|     13 | 2003 | `	int i = 1;` |
|     13 | 2004 | `	if( nArg < 1 ){` |
|      - | 2005 | `		/* Missing argument,return NULL */` |
|      3 | 2006 | `		ph7_result_null(pCtx);` |
|      3 | 2007 | `		return PH7_OK;` |
|      - | 2008 | `	}` |
|      - | 2009 | `	/* Prepare the implode context */` |
|     11 | 2010 | `	imp_data.pCtx = pCtx;` |
|     11 | 2011 | `	imp_data.bRecursive = 1;` |
|     11 | 2012 | `	imp_data.bFirst = 1;` |
|     11 | 2013 | `	imp_data.nRecCount = 0;` |
|     11 | 2014 | `	imp_data.rc = SXRET_OK;` |
|     11 | 2015 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 2016 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 2017 | `	}else{` |
|    ! 0 | 2018 | `		imp_data.zSep = 0;` |
|    ! 0 | 2019 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 2020 | `		i = 0;` |
|      - | 2021 | `	}` |
|     11 | 2022 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2023 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2024 | `	}` |
|      - | 2025 | `	/* Start the 'join' process */` |
|     21 | 2026 | `	while( i < nArg ){` |
|     11 | 2027 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2028 | `			/* Iterate throw array entries */` |
|      3 | 2029 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2030 | `			/* Surface a callback allocation failure as a fatal */` |
|      3 | 2031 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2032 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2033 | `			}` |
|      2 | 2034 | `		}else{` |
|      - | 2035 | `			const char *zData;` |
|      - | 2036 | `			int nLen;` |
|      - | 2037 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 2038 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2039 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 2040 | `			if( imp_data.bFirst ){` |
|      9 | 2041 | `				imp_data.bFirst = 0;` |
|      4 | 2042 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2043 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2044 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2045 | `				}` |
|    ! 0 | 2046 | `			}` |
|      - | 2047 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 2048 | `			if( nLen > 0 ){` |
|      9 | 2049 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2050 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2051 | `				}` |
|      4 | 2052 | `			}` |
|      - | 2053 | `		}` |
|     11 | 2054 | `		i++;` |
|      1 | 2055 | `	}` |
|     11 | 2056 | `	return PH7_OK;` |
|      7 | 2057 | `}` |
|      - | 2058 | `/*` |
|      - | 2059 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 2060 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 2061 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 2062 | ` * Parameters` |
|      - | 2063 | ` *  $delimiter` |
|      - | 2064 | ` *   The boundary string.` |
|      - | 2065 | ` * $string` |
|      - | 2066 | ` *   The input string.` |
|      - | 2067 | ` * $limit` |
|      - | 2068 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 2069 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 2070 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 2071 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 2072 | ` * Returns` |
|      - | 2073 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 2074 | ` *  on boundaries formed by the delimiter.` |
|      - | 2075 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 2076 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 2077 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 2078 | ` *  will be returned.` |
|      - | 2079 | ` * NOTE:` |
|      - | 2080 | ` *  Negative limit is not supported.` |
|      - | 2081 | ` */` |
|   6852 | 2082 | `PH7_PRIVATE int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2083 | `{` |
|      - | 2084 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2085 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2086 | `	ph7_value *pArray;` |
|      - | 2087 | `	ph7_value *pValue;` |
|      - | 2088 | `	sxu32 nOfft;` |
|      - | 2089 | `	sxi32 rc;` |
|   6857 | 2090 | `	if( nArg < 2 ){` |
|      - | 2091 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2092 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2093 | `		return PH7_OK;` |
|      - | 2094 | `	}` |
|      - | 2095 | `	/* Extract the delimiter */` |
|   6857 | 2096 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   6857 | 2097 | `	if( nDelim < 1 ){` |
|      - | 2098 | `		/* Empty delimiter: PHP 8 throws a catchable ValueError. */` |
|      5 | 2099 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2100 | `			"explode(): Argument #1 ($separator) must not be empty");` |
|      - | 2101 | `	}` |
|      - | 2102 | `	/* Extract the string */` |
|   6853 | 2103 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   6853 | 2104 | `	if( nStrlen < 1 ){` |
|      - | 2105 | `		/* Empty string: normally an array with a single empty element (PHP behavior).` |
|      - | 2106 | `		 * A negative limit drops the last -limit components, so the sole empty` |
|      - | 2107 | `		 * component is dropped and the result is an empty array. */` |
|     13 | 2108 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|     13 | 2109 | `		if( pArrayTmp == 0 ){` |
|      - | 2110 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 2111 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2112 | `			return PH7_OK;` |
|      - | 2113 | `		}` |
|     13 | 2114 | `		if( !(nArg > 2 && ph7_value_to_int(apArg[2]) < 0) ){` |
|     11 | 2115 | `			ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|     11 | 2116 | `			if( pValueTmp == 0 ){` |
|      - | 2117 | `				/* Out of memory,return FALSE */` |
|    ! 0 | 2118 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 2119 | `				return PH7_OK;` |
|      - | 2120 | `			}` |
|     11 | 2121 | `			ph7_value_string(pValueTmp, "", 0);` |
|     11 | 2122 | `			if( ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp) != SXRET_OK ){` |
|    ! 0 | 2123 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2124 | `			}` |
|      5 | 2125 | `		}` |
|     13 | 2126 | `		ph7_result_value(pCtx, pArrayTmp);` |
|     13 | 2127 | `		return PH7_OK;` |
|      - | 2128 | `	}` |
|      - | 2129 | `	/* Point to the end of the string */` |
|   6841 | 2130 | `	zEnd = &zString[nStrlen];` |
|      - | 2131 | `	/* Create the array */` |
|   6841 | 2132 | `	pArray =  ph7_context_new_array(pCtx);` |
|   6841 | 2133 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   6841 | 2134 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2135 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2136 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2137 | `		return PH7_OK;` |
|      - | 2138 | `	}` |
|      - | 2139 | `	/* Set a defualt limit */` |
|   6841 | 2140 | `	iLimit = SXI32_HIGH;` |
|   6841 | 2141 | `	if( nArg > 2 ){` |
|     55 | 2142 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     55 | 2143 | `		if( iLimit < 0 ){` |
|      - | 2144 | `			/* Negative limit: keep all components except the last -iLimit (PHP).` |
|      - | 2145 | `			 * Pre-count the components (delimiters + 1), then emit only the first` |
|      - | 2146 | `			 * nKeep CLEAN components — no trailing-remainder merge (the difference` |
|      - | 2147 | `			 * from the positive path). nKeep <= 0 drops everything -> empty array. */` |
|     17 | 2148 | `			int nTotal = 1,nKeep;` |
|     17 | 2149 | `			const char *zScan = zString;` |
|      - | 2150 | `			sxu32 nScanOfft;` |
|     57 | 2151 | `			while( SyBlobSearch(zScan,(sxu32)(zEnd - zScan),zDelim,nDelim,&nScanOfft) == SXRET_OK ){` |
|     41 | 2152 | `				nTotal++;` |
|     41 | 2153 | `				zScan = &zScan[nScanOfft + nDelim];` |
|      1 | 2154 | `			}` |
|     17 | 2155 | `			nKeep = nTotal + iLimit; /* iLimit < 0, so this is nTotal - (-iLimit) */` |
|     49 | 2156 | `			while( nKeep > (int)ph7_array_count(pArray)` |
|     39 | 2157 | `				&& SyBlobSearch(zString,(sxu32)(zEnd - zString),zDelim,nDelim,&nOfft) == SXRET_OK ){` |
|      - | 2158 | `				/* Emit the next clean component */` |
|     23 | 2159 | `				zCur = &zString[nOfft];` |
|     23 | 2160 | `				ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|     23 | 2161 | `				if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2162 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2163 | `				}` |
|     23 | 2164 | `				zString = &zCur[nDelim];` |
|     23 | 2165 | `				ph7_value_reset_string_cursor(pValue);` |
|      1 | 2166 | `			}` |
|     17 | 2167 | `			ph7_result_value(pCtx,pArray);` |
|     17 | 2168 | `			return PH7_OK;` |
|      - | 2169 | `		}` |
|     39 | 2170 | `		if( iLimit == 0 ){` |
|      5 | 2171 | `			iLimit = 1;` |
|      2 | 2172 | `		}` |
|     39 | 2173 | `		iLimit--;` |
|     17 | 2174 | `	}` |
|      - | 2175 | `	/* Start exploding */` |
|  82254 | 2176 | `	for(;;){` |
| 164513 | 2177 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 164513 | 2178 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2179 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   6825 | 2180 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   6825 | 2181 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2182 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2183 | `			}` |
|   6825 | 2184 | `			break;` |
|      - | 2185 | `		}` |
|      - | 2186 | `		/* Point to the desired offset */` |
| 157693 | 2187 | `		zCur = &zString[nOfft];` |
|      - | 2188 | `		/* Perform the store operation (may be empty) */` |
| 157693 | 2189 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 157693 | 2190 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2191 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2192 | `		}` |
|      - | 2193 | `		/* Point beyond the delimiter */` |
| 157693 | 2194 | `		zString = &zCur[nDelim];` |
|      - | 2195 | `		/* Reset the cursor */` |
| 157693 | 2196 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 2197 | `	}` |
|      - | 2198 | `	/* Return the freshly created array */` |
|   6825 | 2199 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2200 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2201 | `	 * released as soon we return from this foregin function.` |
|      - | 2202 | `	 */` |
|   6825 | 2203 | `	return PH7_OK;` |
|   3431 | 2204 | `}` |
|      - | 2205 | `/*` |
|      - | 2206 | ` * string trim(string $str[,string $charlist ])` |
|      - | 2207 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2208 | ` * Parameters` |
|      - | 2209 | ` *  $str` |
|      - | 2210 | ` *   The string that will be trimmed.` |
|      - | 2211 | ` * $charlist` |
|      - | 2212 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2213 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2214 | ` *   With .. you can specify a range of characters.` |
|      - | 2215 | ` * Returns.` |
|      - | 2216 | ` *  Thr processed string.` |
|      - | 2217 | ` * NOTE:` |
|      - | 2218 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2219 | ` */` |
|  14318 | 2220 | `PH7_PRIVATE int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2221 | `{` |
|  14323 | 2222 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"trim",1,"$string"); }` |
|      - | 2223 | `	const char *zString;` |
|      - | 2224 | `	int nLen;` |
|  14323 | 2225 | `	if( nArg < 1 ){` |
|      - | 2226 | `		/* Missing arguments,return null */` |
|    ! 0 | 2227 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2228 | `		return PH7_OK;` |
|      - | 2229 | `	}` |
|      - | 2230 | `	/* Extract the target string */` |
|  14323 | 2231 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  14323 | 2232 | `	if( nLen < 1 ){` |
|      - | 2233 | `		/* Empty string,return */` |
|    633 | 2234 | `		ph7_result_string(pCtx,"",0);` |
|    633 | 2235 | `		return PH7_OK;` |
|      - | 2236 | `	}` |
|      - | 2237 | `	/* Start the trim process */` |
|  13695 | 2238 | `	if( nArg < 2 ){` |
|      - | 2239 | `		SyString sStr;` |
|      - | 2240 | `		/* Remove white spaces and NUL bytes */` |
|  13665 | 2241 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  34389 | 2242 | `		SyStringFullTrimSafe(&sStr);` |
|  13665 | 2243 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   6835 | 2244 | `	}else{` |
|      - | 2245 | `		/* Char list */` |
|      - | 2246 | `		const char *zList;` |
|      - | 2247 | `		int nListlen;` |
|     33 | 2248 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     33 | 2249 | `		if( nListlen < 1 ){` |
|      - | 2250 | `			/* Return the string unchanged */` |
|      6 | 2251 | `			ph7_result_string(pCtx,zString,nLen);` |
|      4 | 2252 | `		}else{` |
|      - | 2253 | `			char aMask[256];` |
|     29 | 2254 | `			const char *zEnd = &zString[nLen];` |
|     29 | 2255 | `			const char *zCur = zString;` |
|     29 | 2256 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2257 | `			/* Left trim */` |
|     79 | 2258 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     53 | 2259 | `				zCur++;` |
|      3 | 2260 | `			}` |
|      - | 2261 | `			/* Right trim */` |
|     79 | 2262 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     53 | 2263 | `				zEnd--;` |
|      3 | 2264 | `			}` |
|     29 | 2265 | `			if( zCur >= zEnd ){` |
|      - | 2266 | `				/* Return the empty string */` |
|    ! 0 | 2267 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2268 | `			}else{` |
|     29 | 2269 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2270 | `			}` |
|      - | 2271 | `		}` |
|      - | 2272 | `	}` |
|  13695 | 2273 | `	return PH7_OK;` |
|   7164 | 2274 | `}` |
|      - | 2275 | `/*` |
|      - | 2276 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 2277 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 2278 | ` * Parameters` |
|      - | 2279 | ` *  $str` |
|      - | 2280 | ` *   The string that will be trimmed.` |
|      - | 2281 | ` * $charlist` |
|      - | 2282 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2283 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2284 | ` *   With .. you can specify a range of characters.` |
|      - | 2285 | ` * Returns.` |
|      - | 2286 | ` *  Thr processed string.` |
|      - | 2287 | ` * NOTE:` |
|      - | 2288 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2289 | ` */` |
|    162 | 2290 | `PH7_PRIVATE int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2291 | `{` |
|    166 | 2292 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"rtrim",1,"$string"); }` |
|      - | 2293 | `	const char *zString;` |
|      - | 2294 | `	int nLen;` |
|    166 | 2295 | `	if( nArg < 1 ){` |
|      - | 2296 | `		/* Missing arguments,return null */` |
|    ! 0 | 2297 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2298 | `		return PH7_OK;` |
|      - | 2299 | `	}` |
|      - | 2300 | `	/* Extract the target string */` |
|    166 | 2301 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    166 | 2302 | `	if( nLen < 1 ){` |
|      - | 2303 | `		/* Empty string,return */` |
|      5 | 2304 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2305 | `		return PH7_OK;` |
|      - | 2306 | `	}` |
|      - | 2307 | `	/* Start the trim process */` |
|    162 | 2308 | `	if( nArg < 2 ){` |
|      - | 2309 | `		SyString sStr;` |
|      - | 2310 | `		/* Remove white spaces and NUL bytes*/` |
|     19 | 2311 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     48 | 2312 | `		SyStringRightTrimSafe(&sStr);` |
|     19 | 2313 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|     10 | 2314 | `	}else{` |
|      - | 2315 | `		/* Char list */` |
|      - | 2316 | `		const char *zList;` |
|      - | 2317 | `		int nListlen;` |
|    144 | 2318 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|    144 | 2319 | `		if( nListlen < 1 ){` |
|      - | 2320 | `			/* Return the string unchanged */` |
|    ! 0 | 2321 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 2322 | `		}else{` |
|      - | 2323 | `			char aMask[256];` |
|    144 | 2324 | `			const char *zEnd = &zString[nLen];` |
|    144 | 2325 | `			const char *zCur = zString;` |
|    144 | 2326 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2327 | `			/* Right trim */` |
|    162 | 2328 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     21 | 2329 | `				zEnd--;` |
|      3 | 2330 | `			}` |
|    144 | 2331 | `			if( zEnd <= zCur ){` |
|      - | 2332 | `				/* Return the empty string */` |
|    ! 0 | 2333 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2334 | `			}else{` |
|    144 | 2335 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2336 | `			}` |
|      - | 2337 | `		}` |
|      - | 2338 | `	}` |
|    162 | 2339 | `	return PH7_OK;` |
|     85 | 2340 | `}` |
|      - | 2341 | `/*` |
|      - | 2342 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 2343 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2344 | ` * Parameters` |
|      - | 2345 | ` *  $str` |
|      - | 2346 | ` *   The string that will be trimmed.` |
|      - | 2347 | ` * $charlist` |
|      - | 2348 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2349 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2350 | ` *   With .. you can specify a range of characters.` |
|      - | 2351 | ` * Returns.` |
|      - | 2352 | ` *  Thr processed string.` |
|      - | 2353 | ` * NOTE:` |
|      - | 2354 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2355 | ` */` |
|     50 | 2356 | `PH7_PRIVATE int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2357 | `{` |
|     55 | 2358 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"ltrim",1,"$string"); }` |
|      - | 2359 | `	const char *zString;` |
|      - | 2360 | `	int nLen;` |
|     55 | 2361 | `	if( nArg < 1 ){` |
|      - | 2362 | `		/* Missing arguments,return null */` |
|    ! 0 | 2363 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2364 | `		return PH7_OK;` |
|      - | 2365 | `	}` |
|      - | 2366 | `	/* Extract the target string */` |
|     55 | 2367 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     55 | 2368 | `	if( nLen < 1 ){` |
|      - | 2369 | `		/* Empty string,return */` |
|     21 | 2370 | `		ph7_result_string(pCtx,"",0);` |
|     21 | 2371 | `		return PH7_OK;` |
|      - | 2372 | `	}` |
|      - | 2373 | `	/* Start the trim process */` |
|     39 | 2374 | `	if( nArg < 2 ){` |
|      - | 2375 | `		SyString sStr;` |
|      - | 2376 | `		/* Remove white spaces and NUL byte */` |
|      5 | 2377 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     13 | 2378 | `		SyStringLeftTrimSafe(&sStr);` |
|      5 | 2379 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      3 | 2380 | `	}else{` |
|      - | 2381 | `		/* Char list */` |
|      - | 2382 | `		const char *zList;` |
|      - | 2383 | `		int nListlen;` |
|     35 | 2384 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     35 | 2385 | `		if( nListlen < 1 ){` |
|      - | 2386 | `			/* Return the string unchanged */` |
|      3 | 2387 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2388 | `		}else{` |
|      - | 2389 | `			char aMask[256];` |
|     33 | 2390 | `			const char *zEnd = &zString[nLen];` |
|     33 | 2391 | `			const char *zCur = zString;` |
|     33 | 2392 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2393 | `			/* Left trim */` |
|     87 | 2394 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     59 | 2395 | `				zCur++;` |
|      5 | 2396 | `			}` |
|     33 | 2397 | `			if( zCur >= zEnd ){` |
|      - | 2398 | `				/* Return the empty string */` |
|    ! 0 | 2399 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2400 | `			}else{` |
|     33 | 2401 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2402 | `			}` |
|      - | 2403 | `		}` |
|      - | 2404 | `	}` |
|     39 | 2405 | `	return PH7_OK;` |
|     30 | 2406 | `}` |
|      - | 2407 | `/*` |
|      - | 2408 | ` * string strtolower(string $str)` |
|      - | 2409 | ` *  Make a string lowercase.` |
|      - | 2410 | ` * Parameters` |
|      - | 2411 | ` *  $str` |
|      - | 2412 | ` *   The input string.` |
|      - | 2413 | ` * Returns.` |
|      - | 2414 | ` *  The lowercased string.` |
|      - | 2415 | ` */` |
|  33130 | 2416 | `PH7_PRIVATE int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2417 | `{` |
|  33135 | 2418 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtolower",1,"$string"); }` |
|      - | 2419 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2420 | `	int nLen;` |
|  33135 | 2421 | `	if( nArg < 1 ){` |
|      - | 2422 | `		/* Missing arguments,return null */` |
|    ! 0 | 2423 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2424 | `		return PH7_OK;` |
|      - | 2425 | `	}` |
|      - | 2426 | `	/* Extract the target string */` |
|  33135 | 2427 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  33135 | 2428 | `	if( nLen < 1 ){` |
|      - | 2429 | `		/* Empty string,return */` |
|      3 | 2430 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2431 | `		return PH7_OK;` |
|      - | 2432 | `	}` |
|      - | 2433 | `	/* Perform the requested operation */` |
|  33133 | 2434 | `	zEnd = &zString[nLen];` |
| 104455 | 2435 | `	for(;;){` |
| 208915 | 2436 | `		if( zString >= zEnd ){` |
|      - | 2437 | `			/* No more input,break immediately */` |
|  33133 | 2438 | `			break;` |
|      - | 2439 | `		}` |
| 175787 | 2440 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2441 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2442 | `			zCur = zString;` |
|    ! 0 | 2443 | `			zString++;` |
|    ! 0 | 2444 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2445 | `				zString++;` |
|    ! 0 | 2446 | `			}` |
|      - | 2447 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2448 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2449 | `		}else{` |
| 175787 | 2450 | `			int c = zString[0];` |
| 175787 | 2451 | `			if( SyisUpper(c) ){` |
| 172775 | 2452 | `				c = SyToLower(zString[0]);` |
|  86385 | 2453 | `			}` |
|      - | 2454 | `			/* Append character */` |
| 175787 | 2455 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2456 | `			/* Advance the cursor */` |
| 175787 | 2457 | `			zString++;` |
|      - | 2458 | `		}` |
|      5 | 2459 | `	}` |
|  33133 | 2460 | `	return PH7_OK;` |
|  16570 | 2461 | `}` |
|      - | 2462 | `/*` |
|      - | 2463 | ` * string strtolower(string $str)` |
|      - | 2464 | ` *  Make a string uppercase.` |
|      - | 2465 | ` * Parameters` |
|      - | 2466 | ` *  $str` |
|      - | 2467 | ` *   The input string.` |
|      - | 2468 | ` * Returns.` |
|      - | 2469 | ` *  The uppercased string.` |
|      - | 2470 | ` */` |
|     74 | 2471 | `PH7_PRIVATE int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2472 | `{` |
|     78 | 2473 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtoupper",1,"$string"); }` |
|      - | 2474 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2475 | `	int nLen;` |
|     78 | 2476 | `	if( nArg < 1 ){` |
|      - | 2477 | `		/* Missing arguments,return null */` |
|    ! 0 | 2478 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2479 | `		return PH7_OK;` |
|      - | 2480 | `	}` |
|      - | 2481 | `	/* Extract the target string */` |
|     78 | 2482 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     78 | 2483 | `	if( nLen < 1 ){` |
|      - | 2484 | `		/* Empty string,return */` |
|      6 | 2485 | `		ph7_result_string(pCtx,"",0);` |
|      6 | 2486 | `		return PH7_OK;` |
|      - | 2487 | `	}` |
|      - | 2488 | `	/* Perform the requested operation */` |
|     74 | 2489 | `	zEnd = &zString[nLen];` |
|    146 | 2490 | `	for(;;){` |
|    296 | 2491 | `		if( zString >= zEnd ){` |
|      - | 2492 | `			/* No more input,break immediately */` |
|     74 | 2493 | `			break;` |
|      - | 2494 | `		}` |
|    226 | 2495 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2496 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2497 | `			zCur = zString;` |
|    ! 0 | 2498 | `			zString++;` |
|    ! 0 | 2499 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2500 | `				zString++;` |
|    ! 0 | 2501 | `			}` |
|      - | 2502 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2503 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2504 | `		}else{` |
|    226 | 2505 | `			int c = zString[0];` |
|    226 | 2506 | `			if( SyisLower(c) ){` |
|    210 | 2507 | `				c = SyToUpper(zString[0]);` |
|    103 | 2508 | `			}` |
|      - | 2509 | `			/* Append character */` |
|    226 | 2510 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2511 | `			/* Advance the cursor */` |
|    226 | 2512 | `			zString++;` |
|      - | 2513 | `		}` |
|      4 | 2514 | `	}` |
|     74 | 2515 | `	return PH7_OK;` |
|     41 | 2516 | `}` |
|      - | 2517 | `/*` |
|      - | 2518 | ` * string ucfirst(string $str)` |
|      - | 2519 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2520 | ` *  character is alphabetic.` |
|      - | 2521 | ` * Parameters` |
|      - | 2522 | ` *  $str` |
|      - | 2523 | ` *   The input string.` |
|      - | 2524 | ` * Returns.` |
|      - | 2525 | ` *  The processed string.` |
|      - | 2526 | ` */` |
|      4 | 2527 | `PH7_PRIVATE int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2528 | `{` |
|      - | 2529 | `	const char *zString,*zEnd;` |
|      - | 2530 | `	int nLen,c;` |
|      5 | 2531 | `	if( nArg < 1 ){` |
|      - | 2532 | `		/* Missing arguments,return null */` |
|    ! 0 | 2533 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2534 | `		return PH7_OK;` |
|      - | 2535 | `	}` |
|      - | 2536 | `	/* Extract the target string */` |
|      5 | 2537 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2538 | `	if( nLen < 1 ){` |
|      - | 2539 | `		/* Empty string,return */` |
|      3 | 2540 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2541 | `		return PH7_OK;` |
|      - | 2542 | `	}` |
|      - | 2543 | `	/* Perform the requested operation */` |
|      3 | 2544 | `	zEnd = &zString[nLen];` |
|      3 | 2545 | `	c = zString[0];` |
|      3 | 2546 | `	if( SyisLower(c) ){` |
|      3 | 2547 | `		c = SyToUpper(c);` |
|      1 | 2548 | `	}` |
|      - | 2549 | `	/* Append the first character */` |
|      3 | 2550 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2551 | `	zString++;` |
|      3 | 2552 | `	if( zString < zEnd ){` |
|      - | 2553 | `		/* Append the rest of the input verbatim */` |
|      3 | 2554 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2555 | `	}` |
|      3 | 2556 | `	return PH7_OK;` |
|      3 | 2557 | `}` |
|      - | 2558 | `/*` |
|      - | 2559 | ` * string lcfirst(string $str)` |
|      - | 2560 | ` *  Make a string's first character lowercase.` |
|      - | 2561 | ` * Parameters` |
|      - | 2562 | ` *  $str` |
|      - | 2563 | ` *   The input string.` |
|      - | 2564 | ` * Returns.` |
|      - | 2565 | ` *  The processed string.` |
|      - | 2566 | ` */` |
|      4 | 2567 | `PH7_PRIVATE int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2568 | `{` |
|      - | 2569 | `	const char *zString,*zEnd;` |
|      - | 2570 | `	int nLen,c;` |
|      5 | 2571 | `	if( nArg < 1 ){` |
|      - | 2572 | `		/* Missing arguments,return null */` |
|    ! 0 | 2573 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2574 | `		return PH7_OK;` |
|      - | 2575 | `	}` |
|      - | 2576 | `	/* Extract the target string */` |
|      5 | 2577 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2578 | `	if( nLen < 1 ){` |
|      - | 2579 | `		/* Empty string,return */` |
|      3 | 2580 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2581 | `		return PH7_OK;` |
|      - | 2582 | `	}` |
|      - | 2583 | `	/* Perform the requested operation */` |
|      3 | 2584 | `	zEnd = &zString[nLen];` |
|      3 | 2585 | `	c = zString[0];` |
|      3 | 2586 | `	if( SyisUpper(c) ){` |
|      3 | 2587 | `		c = SyToLower(c);` |
|      1 | 2588 | `	}` |
|      - | 2589 | `	/* Append the first character */` |
|      3 | 2590 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2591 | `	zString++;` |
|      3 | 2592 | `	if( zString < zEnd ){` |
|      - | 2593 | `		/* Append the rest of the input verbatim */` |
|      3 | 2594 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2595 | `	}` |
|      3 | 2596 | `	return PH7_OK;` |
|      3 | 2597 | `}` |
|      - | 2598 | `/*` |
|      - | 2599 | ` * int ord(string $string)` |
|      - | 2600 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 2601 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 2602 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 2603 | ` * Parameters` |
|      - | 2604 | ` *  $string` |
|      - | 2605 | ` *   The input string.` |
|      - | 2606 | ` * Returns` |
|      - | 2607 | ` *  The ASCII value as an integer.` |
|      - | 2608 | ` */` |
|    222 | 2609 | `PH7_PRIVATE int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2610 | `{` |
|      - | 2611 | `	const char *zString;` |
|      - | 2612 | `	int nLen,c;` |
|      - | 2613 | `	/* PHP requires exactly one argument. */` |
|    225 | 2614 | `	if( nArg != 1 ){` |
|      4 | 2615 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2616 | `			"ArgumentCountError",` |
|      - | 2617 | `			"ord() expects exactly 1 argument, %d given",` |
|      1 | 2618 | `			nArg` |
|      - | 2619 | `			);` |
|      - | 2620 | `	}` |
|      - | 2621 | `	/* php only DEPRECATES null here; PHL rejects it. */` |
|    223 | 2622 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 2623 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 2624 | `			"ord(): Argument #1 ($character) must be of type string, null given"` |
|      - | 2625 | `			);` |
|      - | 2626 | `	}` |
|      - | 2627 | `	/* Extract the target string */` |
|    221 | 2628 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    221 | 2629 | `	if( nLen < 1 ){` |
|      - | 2630 | `		/* php only DEPRECATES an empty string here; PHL rejects it. */` |
|      3 | 2631 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2632 | `			"ord(): Argument #1 ($character) must not be empty"` |
|      - | 2633 | `			);` |
|      - | 2634 | `	}` |
|      - | 2635 | `	/* A string longer than one byte: php DEPRECATES it; PHL rejects it. */` |
|    219 | 2636 | `	if( nLen > 1 ){` |
|      3 | 2637 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2638 | `			"ord(): Argument #1 ($character) must be a single byte, use ord($str[0]) instead"` |
|      - | 2639 | `			);` |
|      - | 2640 | `	}` |
|      - | 2641 | `	/* Extract the ASCII value of the first character */` |
|    217 | 2642 | `	c = (unsigned char)zString[0];` |
|      - | 2643 | `	/* Return that value */` |
|    217 | 2644 | `	ph7_result_int(pCtx,c);` |
|    217 | 2645 | `	return PH7_OK;` |
|    114 | 2646 | `}` |
|      - | 2647 | `/*` |
|      - | 2648 | ` * string chr(int $codepoint)` |
|      - | 2649 | ` *  Returns a one-character string containing the character specified` |
|      - | 2650 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 2651 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 2652 | ` * Parameters` |
|      - | 2653 | ` *  $codepoint` |
|      - | 2654 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 2655 | ` *   will be constrained to a single byte.` |
|      - | 2656 | ` * Returns` |
|      - | 2657 | ` *  A single-character string.` |
|      - | 2658 | ` */` |
|   7152 | 2659 | `PH7_PRIVATE int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2660 | `{` |
|      - | 2661 | `	int c;` |
|      - | 2662 | `	unsigned char ch;` |
|      - | 2663 | `	/* PHP requires exactly one argument. */` |
|   7156 | 2664 | `	if( nArg != 1 ){` |
|      4 | 2665 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2666 | `			"ArgumentCountError",` |
|      - | 2667 | `			"chr() expects exactly 1 argument, %d given",` |
|      1 | 2668 | `			nArg` |
|      - | 2669 | `			);` |
|      - | 2670 | `	}` |
|      - | 2671 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 2672 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 2673 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 2674 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|   7154 | 2675 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      3 | 2676 | `		double d = ph7_value_to_double(apArg[0]);` |
|      3 | 2677 | `		if( d != (double)(sxi64)d ){` |
|      - | 2678 | `			/* php only DEPRECATES a lossy float->int here; PHL rejects it. */` |
|      3 | 2679 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 2680 | `				"chr(): Argument #1 ($codepoint) must be of type int, float given");` |
|      - | 2681 | `		}` |
|    ! 0 | 2682 | `	}` |
|      - | 2683 | `	/* Extract the codepoint. */` |
|   7152 | 2684 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 2685 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 2686 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 2687 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 2688 | `	 * name to avoid the API double-prefixing it. */` |
|   7152 | 2689 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 2690 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 2691 | `			E_DEPRECATED,` |
|      - | 2692 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 2693 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 2694 | `			"The value used will be constrained using % 256"` |
|      - | 2695 | `			);` |
|      2 | 2696 | `	}` |
|      - | 2697 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 2698 | `	 * when taking the address of a wider int. */` |
|   7152 | 2699 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 2700 | `	/* Return the specified character */` |
|   7152 | 2701 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|   7152 | 2702 | `	return PH7_OK;` |
|   3580 | 2703 | `}` |
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
|    152 | 2725 | `PH7_PRIVATE int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2726 | `{` |
|      - | 2727 | `	const char *zString;` |
|      - | 2728 | `	int nLen;` |
|      - | 2729 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|    156 | 2730 | `	if( nArg != 1 ){` |
|      4 | 2731 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2732 | `			"ArgumentCountError",` |
|      - | 2733 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|      1 | 2734 | `			nArg` |
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
|     80 | 2771 | `}` |
|      - | 2772 |  |
|      - | 2773 | `/* Search callback signature */` |
|      - | 2774 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 2775 | `/*` |
|      - | 2776 | ` * Case-insensitive pattern match.` |
|      - | 2777 | ` * Brute force is the default search method used here.` |
|      - | 2778 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 2779 | ` * well for short/medium texts on modern hardware.` |
|      - | 2780 | ` */` |
|    298 | 2781 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      2 | 2782 | `{` |
|    300 | 2783 | `	const char *zpIn = (const char *)pPattern;` |
|    300 | 2784 | `	const char *zIn = (const char *)pText;` |
|    300 | 2785 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    300 | 2786 | `	const char *zEnd = &zIn[nLen];` |
|      - | 2787 | `	const char *zPtr,*zPtr2;` |
|      - | 2788 | `	int c,d;` |
|    300 | 2789 | `	if( iPatLen > nLen ){` |
|      - | 2790 | `		/* Don't bother processing */` |
|     67 | 2791 | `		return SXERR_NOTFOUND;` |
|      - | 2792 | `	}` |
|    860 | 2793 | `	for(;;){` |
|   1722 | 2794 | `		if( zIn >= zEnd ){` |
|    194 | 2795 | `			break;` |
|      - | 2796 | `		}` |
|   1530 | 2797 | `		c = SyToLower(zIn[0]);` |
|   1530 | 2798 | `		d = SyToLower(zpIn[0]);` |
|   1530 | 2799 | `		if( c == d ){` |
|    182 | 2800 | `			zPtr   = &zIn[1];` |
|    182 | 2801 | `			zPtr2  = &zpIn[1];` |
|    141 | 2802 | `			for(;;){` |
|    284 | 2803 | `				if( zPtr2 >= zpEnd ){` |
|      - | 2804 | `					/* Pattern found */` |
|     41 | 2805 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 2806 | `					return SXRET_OK;` |
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
|   1490 | 2819 | `		zIn++;` |
|      2 | 2820 | `	}` |
|      - | 2821 | `	/* Pattern not found */` |
|    194 | 2822 | `	return SXERR_NOTFOUND;` |
|    151 | 2823 | `}` |
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
|      6 | 2838 | `PH7_PRIVATE int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2839 | `{` |
|      7 | 2840 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2841 | `	const char *zBlob,*zPattern;` |
|      - | 2842 | `	int nLen,nPatLen;` |
|      - | 2843 | `	sxu32 nOfft;` |
|      - | 2844 | `	sxi32 rc;` |
|      7 | 2845 | `	if( nArg < 2 ){` |
|      - | 2846 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2847 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2848 | `		return PH7_OK;` |
|      - | 2849 | `	}` |
|      - | 2850 | `	/* Extract the needle and the haystack */` |
|      7 | 2851 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 2852 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 2853 | `	nOfft = 0; /* cc warning */` |
|      9 | 2854 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2855 | `		int before = 0;` |
|      - | 2856 | `		/* Perform the lookup */` |
|      5 | 2857 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2858 | `		if( rc != SXRET_OK ){` |
|      - | 2859 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2860 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2861 | `			return PH7_OK;` |
|      - | 2862 | `		}` |
|      - | 2863 | `		/* Return the portion of the string */` |
|      5 | 2864 | `		if( nArg > 2 ){` |
|      3 | 2865 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2866 | `		}` |
|      5 | 2867 | `		if( before ){` |
|      3 | 2868 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2869 | `		}else{` |
|      3 | 2870 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2871 | `		}` |
|      3 | 2872 | `	}else{` |
|      3 | 2873 | `		ph7_result_bool(pCtx,0);` |
|      - | 2874 | `	}` |
|      7 | 2875 | `	return PH7_OK;` |
|      4 | 2876 | `}` |
|      - | 2877 | `/*` |
|      - | 2878 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2879 | ` *  Case-insensitive strstr().` |
|      - | 2880 | ` * Parameters` |
|      - | 2881 | ` *  $haystack` |
|      - | 2882 | ` *   The input string.` |
|      - | 2883 | ` * $needle` |
|      - | 2884 | ` *   Search pattern (must be a string).` |
|      - | 2885 | ` * $before_needle` |
|      - | 2886 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2887 | ` *   of the needle (excluding the needle).` |
|      - | 2888 | ` * Return` |
|      - | 2889 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2890 | ` */` |
|      4 | 2891 | `PH7_PRIVATE int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2892 | `{` |
|      5 | 2893 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2894 | `	const char *zBlob,*zPattern;` |
|      - | 2895 | `	int nLen,nPatLen;` |
|      - | 2896 | `	sxu32 nOfft;` |
|      - | 2897 | `	sxi32 rc;` |
|      5 | 2898 | `	if( nArg < 2 ){` |
|      - | 2899 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2900 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2901 | `		return PH7_OK;` |
|      - | 2902 | `	}` |
|      - | 2903 | `	/* Extract the needle and the haystack */` |
|      5 | 2904 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2905 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 2906 | `	nOfft = 0; /* cc warning */` |
|      7 | 2907 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2908 | `		int before = 0;` |
|      - | 2909 | `		/* Perform the lookup */` |
|      5 | 2910 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2911 | `		if( rc != SXRET_OK ){` |
|      - | 2912 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2913 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2914 | `			return PH7_OK;` |
|      - | 2915 | `		}` |
|      - | 2916 | `		/* Return the portion of the string */` |
|      5 | 2917 | `		if( nArg > 2 ){` |
|      3 | 2918 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2919 | `		}` |
|      5 | 2920 | `		if( before ){` |
|      3 | 2921 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2922 | `		}else{` |
|      3 | 2923 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2924 | `		}` |
|      3 | 2925 | `	}else{` |
|    ! 0 | 2926 | `		ph7_result_bool(pCtx,0);` |
|      - | 2927 | `	}` |
|      5 | 2928 | `	return PH7_OK;` |
|      3 | 2929 | `}` |
|      - | 2930 | `/*` |
|      - | 2931 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2932 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 2933 | ` * Parameters` |
|      - | 2934 | ` *  $haystack` |
|      - | 2935 | ` *   The input string.` |
|      - | 2936 | ` * $needle` |
|      - | 2937 | ` *   Search pattern (must be a string).` |
|      - | 2938 | ` * $offset` |
|      - | 2939 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 2940 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 2941 | ` *   of haystack.` |
|      - | 2942 | ` * Return` |
|      - | 2943 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 2944 | ` */` |
|   1576 | 2945 | `PH7_PRIVATE int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2946 | `{` |
|   1581 | 2947 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strpos",1,"$haystack"); }` |
|   1581 | 2948 | `	if( nArg > 1 ){ StrNullArgNotice(pCtx,apArg[1],"strpos",2,"$needle"); }` |
|   1581 | 2949 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2950 | `	const char *zBlob,*zPattern;` |
|      - | 2951 | `	int nLen,nPatLen,nStart;` |
|      - | 2952 | `	sxu32 nOfft;` |
|      - | 2953 | `	sxi32 rc;` |
|   1581 | 2954 | `	if( nArg < 2 ){` |
|      - | 2955 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2956 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2957 | `		return PH7_OK;` |
|      - | 2958 | `	}` |
|      - | 2959 | `	/* Extract the needle and the haystack */` |
|   1581 | 2960 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|   1581 | 2961 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|   1581 | 2962 | `	nOfft = 0; /* cc warning */` |
|   1581 | 2963 | `	nStart = 0;` |
|      - | 2964 | `	/* Peek the starting offset if available */` |
|   1581 | 2965 | `	if( nArg > 2 ){` |
|     15 | 2966 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 2967 | `		if( nStart < 0 ){` |
|    ! 0 | 2968 | `			nStart = -nStart;` |
|    ! 0 | 2969 | `		}` |
|     15 | 2970 | `		if( nStart >= nLen ){` |
|      - | 2971 | `			/* Invalid offset */` |
|    ! 0 | 2972 | `			nStart = 0;` |
|    ! 0 | 2973 | `		}else{` |
|     15 | 2974 | `			zBlob += nStart;` |
|     15 | 2975 | `			nLen -= nStart;` |
|      - | 2976 | `		}` |
|      7 | 2977 | `	}` |
|   1581 | 2978 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2979 | `		/* Perform the lookup */` |
|   1577 | 2980 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|   1577 | 2981 | `		if( rc != SXRET_OK ){` |
|      - | 2982 | `			/* Pattern not found,return FALSE */` |
|    789 | 2983 | `			ph7_result_bool(pCtx,0);` |
|    789 | 2984 | `			return PH7_OK;` |
|      - | 2985 | `		}` |
|      - | 2986 | `		/* Return the pattern position */` |
|    793 | 2987 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|    399 | 2988 | `	}else{` |
|      5 | 2989 | `		ph7_result_bool(pCtx,0);` |
|      - | 2990 | `	}` |
|    797 | 2991 | `	return PH7_OK;` |
|    793 | 2992 | `}` |
|      - | 2993 | `/*` |
|      - | 2994 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|      - | 2995 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|      - | 2996 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|      - | 2997 | ` * TypeError for arrays, resources, and objects without __toString.` |
|      - | 2998 | ` *` |
|      - | 2999 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|      - | 3000 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|      - | 3001 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|      - | 3002 | ` *` |
|      - | 3003 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|      - | 3004 | ` * is valid until pTmp is released or pArg is mutated.` |
|      - | 3005 | ` */` |
|    646 | 3006 | `static sxi32 StrPredicateResolveArg(` |
|      - | 3007 | `	ph7_context *pCtx,` |
|      - | 3008 | `	ph7_value *pArg,` |
|      - | 3009 | `	const char *zFunc,` |
|      - | 3010 | `	int iArgNum,` |
|      - | 3011 | `	const char *zParamName,` |
|      - | 3012 | `	const char *zTypeStr, /* Declared type in the TypeError, e.g. "string" / "?string" */` |
|      - | 3013 | `	const char *zNullMsg,` |
|      - | 3014 | `	ph7_value *pTmp,` |
|      - | 3015 | `	const char **pzOut,` |
|      - | 3016 | `	int *pnOut` |
|      3 | 3017 | `){` |
|    323 | 3018 | `	SXUNUSED(zNullMsg); /* php's deprecation text — PHL rejects null instead of coercing */` |
|    649 | 3019 | `	if( ph7_value_is_null(pArg) ){` |
|      - | 3020 | `		/* php only DEPRECATES null here; PHL rejects it with the TypeError php will` |
|      - | 3021 | `		 * eventually raise. */` |
|      4 | 3022 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 3023 | `			"%s(): Argument #%d (%s) must be of type %s, null given",` |
|      1 | 3024 | `			zFunc,iArgNum,zParamName,zTypeStr);` |
|      - | 3025 | `	}` |
|    992 | 3026 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    644 | 3027 | `	    ( ph7_value_is_object(pArg) &&` |
|     72 | 3028 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     48 | 3029 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     24 | 3030 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 3031 | `	    )` |
|      - | 3032 | `	){` |
|    ! 0 | 3033 | `		const char *zType = ph7_type_name(pArg);` |
|    ! 0 | 3034 | `		if( ph7_value_is_object(pArg) ){` |
|    ! 0 | 3035 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|    ! 0 | 3036 | `			if( pInst && pInst->pClass ){` |
|    ! 0 | 3037 | `				zType = SyStringData(&pInst->pClass->sName);` |
|    ! 0 | 3038 | `			}` |
|    ! 0 | 3039 | `		}` |
|    ! 0 | 3040 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3041 | `			"TypeError",` |
|      - | 3042 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|    ! 0 | 3043 | `			zFunc, iArgNum, zParamName, zTypeStr, zType` |
|      - | 3044 | `			);` |
|      - | 3045 | `	}` |
|    646 | 3046 | `	if( ph7_value_is_object(pArg) ){` |
|     49 | 3047 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     49 | 3048 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 3049 | `			"__toString",sizeof("__toString")-1);` |
|     49 | 3050 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|     49 | 3051 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|     49 | 3052 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|     49 | 3053 | `		return PH7_OK;` |
|      - | 3054 | `	}` |
|    598 | 3055 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|    598 | 3056 | `	return PH7_OK;` |
|    326 | 3057 | `}` |
|      - | 3058 | `/*` |
|      - | 3059 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 3060 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 3061 | ` * Return` |
|      - | 3062 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 3063 | ` */` |
|     90 | 3064 | `PH7_PRIVATE int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 3065 | `{` |
|      - | 3066 | `	const char *zHaystack,*zNeedle;` |
|      - | 3067 | `	int nHayLen,nNeedleLen;` |
|      - | 3068 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3069 | `	sxi32 rc;` |
|     94 | 3070 | `	if( nArg != 2 ){` |
|      8 | 3071 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3072 | `			"ArgumentCountError",` |
|      - | 3073 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|      2 | 3074 | `			nArg` |
|      - | 3075 | `			);` |
|      - | 3076 | `	}` |
|     89 | 3077 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     89 | 3078 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     89 | 3079 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack","string",` |
|      - | 3080 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|      - | 3081 | `		"of type string is deprecated",` |
|      - | 3082 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     89 | 3083 | `	if( rc != PH7_OK ) goto out;` |
|     86 | 3084 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle","string",` |
|      - | 3085 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|      - | 3086 | `		"of type string is deprecated",` |
|      - | 3087 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     86 | 3088 | `	if( rc != PH7_OK ) goto out;` |
|     86 | 3089 | `	if( nNeedleLen < 1 ){` |
|     11 | 3090 | `		ph7_result_bool(pCtx,1);` |
|     81 | 3091 | `	}else if( nHayLen < nNeedleLen ){` |
|      7 | 3092 | `		ph7_result_bool(pCtx,0);` |
|      4 | 3093 | `	}else{` |
|    104 | 3094 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|     34 | 3095 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|     70 | 3096 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|      - | 3097 | `	}` |
|     86 | 3098 | `	rc = PH7_OK;` |
|     43 | 3099 | `out:` |
|     89 | 3100 | `	PH7_MemObjRelease(&sHayTmp);` |
|     89 | 3101 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     89 | 3102 | `	return rc;` |
|     49 | 3103 | `}` |
|      - | 3104 | `/*` |
|      - | 3105 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 3106 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 3107 | ` * Return` |
|      - | 3108 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 3109 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3110 | ` */` |
|     58 | 3111 | `PH7_PRIVATE int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3112 | `{` |
|      - | 3113 | `	const char *zHaystack,*zNeedle;` |
|      - | 3114 | `	int nHayLen,nNeedleLen;` |
|      - | 3115 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3116 | `	sxi32 rc;` |
|     60 | 3117 | `	if( nArg != 2 ){` |
|      8 | 3118 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3119 | `			"ArgumentCountError",` |
|      - | 3120 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|      2 | 3121 | `			nArg` |
|      - | 3122 | `			);` |
|      - | 3123 | `	}` |
|     55 | 3124 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     55 | 3125 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     55 | 3126 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack","string",` |
|      - | 3127 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3128 | `		"of type string is deprecated",` |
|      - | 3129 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     55 | 3130 | `	if( rc != PH7_OK ) goto out;` |
|     55 | 3131 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle","string",` |
|      - | 3132 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3133 | `		"of type string is deprecated",` |
|      - | 3134 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     55 | 3135 | `	if( rc != PH7_OK ) goto out;` |
|     55 | 3136 | `	if( nNeedleLen < 1 ){` |
|     11 | 3137 | `		ph7_result_bool(pCtx,1);` |
|     50 | 3138 | `	}else if( nHayLen < nNeedleLen ){` |
|      7 | 3139 | `		ph7_result_bool(pCtx,0);` |
|      4 | 3140 | `	}else{` |
|     58 | 3141 | `		ph7_result_bool(pCtx,` |
|     38 | 3142 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3143 | `	}` |
|     55 | 3144 | `	rc = PH7_OK;` |
|     27 | 3145 | `out:` |
|     55 | 3146 | `	PH7_MemObjRelease(&sHayTmp);` |
|     55 | 3147 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     55 | 3148 | `	return rc;` |
|     31 | 3149 | `}` |
|      - | 3150 | `/*` |
|      - | 3151 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 3152 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 3153 | ` * Return` |
|      - | 3154 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 3155 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3156 | ` */` |
|     58 | 3157 | `PH7_PRIVATE int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3158 | `{` |
|      - | 3159 | `	const char *zHaystack,*zNeedle;` |
|      - | 3160 | `	int nHayLen,nNeedleLen;` |
|      - | 3161 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3162 | `	sxi32 rc;` |
|     60 | 3163 | `	if( nArg != 2 ){` |
|      8 | 3164 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3165 | `			"ArgumentCountError",` |
|      - | 3166 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|      2 | 3167 | `			nArg` |
|      - | 3168 | `			);` |
|      - | 3169 | `	}` |
|     55 | 3170 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     55 | 3171 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     55 | 3172 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack","string",` |
|      - | 3173 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3174 | `		"of type string is deprecated",` |
|      - | 3175 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     55 | 3176 | `	if( rc != PH7_OK ) goto out;` |
|     55 | 3177 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle","string",` |
|      - | 3178 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3179 | `		"of type string is deprecated",` |
|      - | 3180 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     55 | 3181 | `	if( rc != PH7_OK ) goto out;` |
|     55 | 3182 | `	if( nNeedleLen < 1 ){` |
|     11 | 3183 | `		ph7_result_bool(pCtx,1);` |
|     50 | 3184 | `	}else if( nHayLen < nNeedleLen ){` |
|      7 | 3185 | `		ph7_result_bool(pCtx,0);` |
|      4 | 3186 | `	}else{` |
|     58 | 3187 | `		ph7_result_bool(pCtx,` |
|     38 | 3188 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3189 | `	}` |
|     55 | 3190 | `	rc = PH7_OK;` |
|     27 | 3191 | `out:` |
|     55 | 3192 | `	PH7_MemObjRelease(&sHayTmp);` |
|     55 | 3193 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     55 | 3194 | `	return rc;` |
|     31 | 3195 | `}` |
|      - | 3196 | `/*` |
|      - | 3197 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3198 | ` *  Case-insensitive strpos.` |
|      - | 3199 | ` * Parameters` |
|      - | 3200 | ` *  $haystack` |
|      - | 3201 | ` *   The input string.` |
|      - | 3202 | ` * $needle` |
|      - | 3203 | ` *   Search pattern (must be a string).` |
|      - | 3204 | ` * $offset` |
|      - | 3205 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3206 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3207 | ` *   of haystack.` |
|      - | 3208 | ` * Return` |
|      - | 3209 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3210 | ` */` |
|    196 | 3211 | `PH7_PRIVATE int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3212 | `{` |
|    198 | 3213 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3214 | `	const char *zBlob,*zPattern;` |
|      - | 3215 | `	int nLen,nPatLen,nStart;` |
|      - | 3216 | `	sxu32 nOfft;` |
|      - | 3217 | `	sxi32 rc;` |
|    198 | 3218 | `	if( nArg < 2 ){` |
|      - | 3219 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3220 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3221 | `		return PH7_OK;` |
|      - | 3222 | `	}` |
|      - | 3223 | `	/* Extract the needle and the haystack */` |
|    198 | 3224 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    198 | 3225 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    198 | 3226 | `	nOfft = 0; /* cc warning */` |
|    198 | 3227 | `	nStart = 0;` |
|      - | 3228 | `	/* Peek the starting offset if available */` |
|    198 | 3229 | `	if( nArg > 2 ){` |
|      5 | 3230 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 3231 | `		if( nStart < 0 ){` |
|      3 | 3232 | `			nStart = -nStart;` |
|      1 | 3233 | `		}` |
|      5 | 3234 | `		if( nStart >= nLen ){` |
|      - | 3235 | `			/* Invalid offset */` |
|    ! 0 | 3236 | `			nStart = 0;` |
|    ! 0 | 3237 | `		}else{` |
|      5 | 3238 | `			zBlob += nStart;` |
|      5 | 3239 | `			nLen -= nStart;` |
|      - | 3240 | `		}` |
|      2 | 3241 | `	}` |
|    198 | 3242 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3243 | `		/* Perform the lookup */` |
|    198 | 3244 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    198 | 3245 | `		if( rc != SXRET_OK ){` |
|      - | 3246 | `			/* Pattern not found,return FALSE */` |
|    184 | 3247 | `			ph7_result_bool(pCtx,0);` |
|    184 | 3248 | `			return PH7_OK;` |
|      - | 3249 | `		}` |
|      - | 3250 | `		/* Return the pattern position */` |
|     15 | 3251 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 3252 | `	}else{` |
|    ! 0 | 3253 | `		ph7_result_bool(pCtx,0);` |
|      - | 3254 | `	}` |
|     15 | 3255 | `	return PH7_OK;` |
|    100 | 3256 | `}` |
|      - | 3257 | `/*` |
|      - | 3258 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3259 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 3260 | ` * Parameters` |
|      - | 3261 | ` *  $haystack` |
|      - | 3262 | ` *   The input string.` |
|      - | 3263 | ` * $needle` |
|      - | 3264 | ` *   Search pattern (must be a string).` |
|      - | 3265 | ` * $offset` |
|      - | 3266 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3267 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3268 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3269 | ` * Return` |
|      - | 3270 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3271 | ` */` |
|     48 | 3272 | `PH7_PRIVATE int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3273 | `{` |
|      - | 3274 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     49 | 3275 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3276 | `	int nLen,nPatLen;` |
|      - | 3277 | `	sxu32 nOfft;` |
|      - | 3278 | `	sxi32 rc;` |
|     49 | 3279 | `	if( nArg < 2 ){` |
|      - | 3280 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3281 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3282 | `		return PH7_OK;` |
|      - | 3283 | `	}` |
|      - | 3284 | `	/* Extract the needle and the haystack */` |
|     49 | 3285 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     49 | 3286 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3287 | `	/* Point to the end of the pattern */` |
|     49 | 3288 | `	zPtr = &zBlob[nLen - 1];` |
|     49 | 3289 | `	zEnd = &zBlob[nLen];` |
|      - | 3290 | `	/* Save the starting posistion */` |
|     49 | 3291 | `	zStart = zBlob;` |
|     49 | 3292 | `	nOfft = 0; /* cc warning */` |
|      - | 3293 | `	/* Peek the starting offset if available */` |
|     49 | 3294 | `	if( nArg > 2 ){` |
|      - | 3295 | `		int nStart;` |
|     21 | 3296 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 3297 | `		if( nStart < 0 ){` |
|     11 | 3298 | `			nStart = -nStart;` |
|     11 | 3299 | `			if( nStart >= nLen ){` |
|      - | 3300 | `				/* Invalid offset */` |
|      3 | 3301 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3302 | `				return PH7_OK;` |
|    ! 0 | 3303 | `			}else{` |
|      9 | 3304 | `				nLen -= nStart;` |
|      9 | 3305 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 3306 | `				zEnd = &zBlob[nLen];` |
|      - | 3307 | `			}` |
|      5 | 3308 | `		}else{` |
|     11 | 3309 | `			if( nStart >= nLen ){` |
|      - | 3310 | `				/* Invalid offset */` |
|      5 | 3311 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3312 | `				return PH7_OK;` |
|    ! 0 | 3313 | `			}else{` |
|      7 | 3314 | `				zBlob += nStart;` |
|      7 | 3315 | `				nLen -= nStart;` |
|      - | 3316 | `			}` |
|      - | 3317 | `		}` |
|      7 | 3318 | `	}` |
|     43 | 3319 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3320 | `		/* Perform the lookup */` |
|    130 | 3321 | `		for(;;){` |
|    261 | 3322 | `			if( zBlob >= zPtr ){` |
|     21 | 3323 | `				break;` |
|      - | 3324 | `			}` |
|    241 | 3325 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    241 | 3326 | `			if( rc == SXRET_OK ){` |
|      - | 3327 | `				/* Pattern found,return it's position */` |
|     21 | 3328 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     21 | 3329 | `				return PH7_OK;` |
|      - | 3330 | `			}` |
|    221 | 3331 | `			zPtr--;` |
|      1 | 3332 | `		}` |
|      - | 3333 | `		/* Pattern not found,return FALSE */` |
|     21 | 3334 | `		ph7_result_bool(pCtx,0);` |
|     11 | 3335 | `	}else{` |
|      3 | 3336 | `		ph7_result_bool(pCtx,0);` |
|      - | 3337 | `	}` |
|     23 | 3338 | `	return PH7_OK;` |
|     25 | 3339 | `}` |
|      - | 3340 | `/*` |
|      - | 3341 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3342 | ` *  Case-insensitive strrpos.` |
|      - | 3343 | ` * Parameters` |
|      - | 3344 | ` *  $haystack` |
|      - | 3345 | ` *   The input string.` |
|      - | 3346 | ` * $needle` |
|      - | 3347 | ` *   Search pattern (must be a string).` |
|      - | 3348 | ` * $offset` |
|      - | 3349 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3350 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3351 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3352 | ` * Return` |
|      - | 3353 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3354 | ` */` |
|     26 | 3355 | `PH7_PRIVATE int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3356 | `{` |
|      - | 3357 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     27 | 3358 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3359 | `	int nLen,nPatLen;` |
|      - | 3360 | `	sxu32 nOfft;` |
|      - | 3361 | `	sxi32 rc;` |
|     27 | 3362 | `	if( nArg < 2 ){` |
|      - | 3363 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3364 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3365 | `		return PH7_OK;` |
|      - | 3366 | `	}` |
|      - | 3367 | `	/* Extract the needle and the haystack */` |
|     27 | 3368 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 3369 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3370 | `	/* Point to the end of the pattern */` |
|     27 | 3371 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3372 | `	zEnd = &zBlob[nLen];` |
|      - | 3373 | `	/* Save the starting posistion */` |
|     27 | 3374 | `	zStart = zBlob;` |
|     27 | 3375 | `	nOfft = 0; /* cc warning */` |
|      - | 3376 | `	/* Peek the starting offset if available */` |
|     27 | 3377 | `	if( nArg > 2 ){` |
|      - | 3378 | `		int nStart;` |
|     15 | 3379 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3380 | `		if( nStart < 0 ){` |
|      7 | 3381 | `			nStart = -nStart;` |
|      7 | 3382 | `			if( nStart >= nLen ){` |
|      - | 3383 | `				/* Invalid offset */` |
|      3 | 3384 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3385 | `				return PH7_OK;` |
|    ! 0 | 3386 | `			}else{` |
|      5 | 3387 | `				nLen -= nStart;` |
|      5 | 3388 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3389 | `				zEnd = &zBlob[nLen];` |
|      - | 3390 | `			}` |
|      3 | 3391 | `		}else{` |
|      9 | 3392 | `			if( nStart >= nLen ){` |
|      - | 3393 | `				/* Invalid offset */` |
|      5 | 3394 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3395 | `				return PH7_OK;` |
|    ! 0 | 3396 | `			}else{` |
|      5 | 3397 | `				zBlob += nStart;` |
|      5 | 3398 | `				nLen -= nStart;` |
|      - | 3399 | `			}` |
|      - | 3400 | `		}` |
|      4 | 3401 | `	}` |
|     21 | 3402 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3403 | `		/* Perform the lookup */` |
|     44 | 3404 | `		for(;;){` |
|     89 | 3405 | `			if( zBlob >= zPtr ){` |
|      9 | 3406 | `				break;` |
|      - | 3407 | `			}` |
|     81 | 3408 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3409 | `			if( rc == SXRET_OK ){` |
|      - | 3410 | `				/* Pattern found,return it's position */` |
|     11 | 3411 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3412 | `				return PH7_OK;` |
|      - | 3413 | `			}` |
|     71 | 3414 | `			zPtr--;` |
|      1 | 3415 | `		}` |
|      - | 3416 | `		/* Pattern not found,return FALSE */` |
|      9 | 3417 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3418 | `	}else{` |
|      3 | 3419 | `		ph7_result_bool(pCtx,0);` |
|      - | 3420 | `	}` |
|     11 | 3421 | `	return PH7_OK;` |
|     14 | 3422 | `}` |
|      - | 3423 | `/*` |
|      - | 3424 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3425 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3426 | ` * Parameters` |
|      - | 3427 | ` *  $haystack` |
|      - | 3428 | ` *   The input string.` |
|      - | 3429 | ` * $needle` |
|      - | 3430 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3431 | ` *  This behavior is different from that of strstr().` |
|      - | 3432 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3433 | ` *  as the ordinal value of a character.` |
|      - | 3434 | ` * Return` |
|      - | 3435 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3436 | ` */` |
|     22 | 3437 | `PH7_PRIVATE int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3438 | `{` |
|      - | 3439 | `	const char *zBlob;` |
|      - | 3440 | `	int nLen,c;` |
|     23 | 3441 | `	if( nArg < 2 ){` |
|      - | 3442 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3443 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3444 | `		return PH7_OK;` |
|      - | 3445 | `	}` |
|      - | 3446 | `	/* Extract the haystack */` |
|     23 | 3447 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3448 | `	c = 0; /* cc warning */` |
|     23 | 3449 | `	if( nLen > 0 ){` |
|      - | 3450 | `		sxu32 nOfft;` |
|      - | 3451 | `		sxi32 rc;` |
|     21 | 3452 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3453 | `			const char *zPattern;` |
|     11 | 3454 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3455 | `														 * for NULL pointer.` |
|      - | 3456 | `														 */` |
|     11 | 3457 | `			c = zPattern[0];` |
|      6 | 3458 | `		}else{` |
|      - | 3459 | `			/* Int cast */` |
|     11 | 3460 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3461 | `		}` |
|      - | 3462 | `		/* Perform the lookup */` |
|     21 | 3463 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3464 | `		if( rc != SXRET_OK ){` |
|      - | 3465 | `			/* No such entry,return FALSE */` |
|      7 | 3466 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3467 | `			return PH7_OK;` |
|      - | 3468 | `		}` |
|      - | 3469 | `		/* Return the string portion */` |
|     15 | 3470 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3471 | `	}else{` |
|      3 | 3472 | `		ph7_result_bool(pCtx,0);` |
|      - | 3473 | `	}` |
|     17 | 3474 | `	return PH7_OK;` |
|     12 | 3475 | `}` |
|      - | 3476 | `/*` |
|      - | 3477 | ` * string strrev(string $string)` |
|      - | 3478 | ` *  Reverse a string.` |
|      - | 3479 | ` * Parameters` |
|      - | 3480 | ` *  $string` |
|      - | 3481 | ` *   String to be reversed.` |
|      - | 3482 | ` * Return` |
|      - | 3483 | ` *  The reversed string.` |
|      - | 3484 | ` */` |
|      2 | 3485 | `PH7_PRIVATE int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3486 | `{` |
|      - | 3487 | `	const char *zIn,*zEnd;` |
|      - | 3488 | `	int nLen,c;` |
|      3 | 3489 | `	if( nArg < 1 ){` |
|      - | 3490 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3491 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3492 | `		return PH7_OK;` |
|      - | 3493 | `	}` |
|      - | 3494 | `	/* Extract the target string */` |
|      3 | 3495 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3496 | `	if( nLen < 1 ){` |
|      - | 3497 | `		/* Empty string Return null */` |
|    ! 0 | 3498 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3499 | `		return PH7_OK;` |
|      - | 3500 | `	}` |
|      - | 3501 | `	/* Perform the requested operation */` |
|      3 | 3502 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3503 | `	for(;;){` |
|      9 | 3504 | `		if( zEnd < zIn ){` |
|      - | 3505 | `			/* No more input to process */` |
|      3 | 3506 | `			break;` |
|      - | 3507 | `		}` |
|      - | 3508 | `		/* Append current character */` |
|      7 | 3509 | `		c = zEnd[0];` |
|      7 | 3510 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3511 | `		zEnd--;` |
|      1 | 3512 | `	}` |
|      3 | 3513 | `	return PH7_OK;` |
|      2 | 3514 | `}` |
|      - | 3515 | `/*` |
|      - | 3516 | ` * string ucwords(string $string [, string $separators = " \t\r\n\f\v"])` |
|      - | 3517 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3518 | ` *  A word begins at the start of the string and after any character present in` |
|      - | 3519 | ` *  $separators. The default separators are the whitespace characters (space,` |
|      - | 3520 | ` *  horizontal tab, carriage return, newline, form-feed and vertical tab); an` |
|      - | 3521 | ` *  explicit $separators argument REPLACES them (an empty string leaves only the` |
|      - | 3522 | ` *  very first character upper-cased). Like PHP, this is byte-based: only ASCII` |
|      - | 3523 | ` *  bytes are upper-cased and a byte is a separator only if it appears in the set.` |
|      - | 3524 | ` * Parameters` |
|      - | 3525 | ` *  $string` |
|      - | 3526 | ` *   The input string.` |
|      - | 3527 | ` *  $separators` |
|      - | 3528 | ` *   The optional word-boundary characters.` |
|      - | 3529 | ` * Return` |
|      - | 3530 | ` *  The modified string.` |
|      - | 3531 | ` */` |
|     22 | 3532 | `PH7_PRIVATE int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3533 | `{` |
|      - | 3534 | `	const char *zIn;` |
|      - | 3535 | `	int nLen,i,iStart;` |
|      - | 3536 | `	char aDelim[256];` |
|     23 | 3537 | `	if( nArg < 1 ){` |
|      - | 3538 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3539 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3540 | `		return PH7_OK;` |
|      - | 3541 | `	}` |
|      - | 3542 | `	/* Build the separator membership table: an explicit $separators argument` |
|      - | 3543 | `	 * replaces the default whitespace set (an empty string clears it). */` |
|     23 | 3544 | `	SyZero(aDelim,(sxu32)sizeof(aDelim));` |
|     23 | 3545 | `	if( nArg > 1 ){` |
|      - | 3546 | `		int nDelim;` |
|      9 | 3547 | `		const char *zDelim = ph7_value_to_string(apArg[1],&nDelim);` |
|     17 | 3548 | `		for( i = 0 ; i < nDelim ; i++ ){` |
|      9 | 3549 | `			aDelim[(unsigned char)zDelim[i]] = 1;` |
|      5 | 3550 | `		}` |
|      5 | 3551 | `	}else{` |
|     15 | 3552 | `		aDelim[(unsigned char)' ']  = 1;` |
|     15 | 3553 | `		aDelim[(unsigned char)'\t'] = 1;` |
|     15 | 3554 | `		aDelim[(unsigned char)'\r'] = 1;` |
|     15 | 3555 | `		aDelim[(unsigned char)'\n'] = 1;` |
|     15 | 3556 | `		aDelim[(unsigned char)'\f'] = 1;` |
|     15 | 3557 | `		aDelim[(unsigned char)'\v'] = 1;` |
|      - | 3558 | `	}` |
|      - | 3559 | `	/* Extract the target string */` |
|     23 | 3560 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3561 | `	if( nLen < 1 ){` |
|      - | 3562 | `		/* Empty string – match PHP semantics */` |
|      3 | 3563 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3564 | `		return PH7_OK;` |
|      - | 3565 | `	}` |
|      - | 3566 | `	/* Upper-case the first byte of each word (the leading byte, or any byte that` |
|      - | 3567 | `	 * follows a separator), appending the untouched runs in between verbatim. */` |
|     21 | 3568 | `	iStart = 0;` |
|    309 | 3569 | `	for( i = 0 ; i < nLen ; i++ ){` |
|    289 | 3570 | `		int c = (unsigned char)zIn[i];` |
|    289 | 3571 | `		if( (i == 0 \|\| aDelim[(unsigned char)zIn[i-1]]) && c < 0x80 && SyisLower(c) ){` |
|     53 | 3572 | `			char up = (char)SyToUpper(c);` |
|     53 | 3573 | `			if( i > iStart ){` |
|     35 | 3574 | `				ph7_result_string(pCtx,&zIn[iStart],i - iStart);` |
|     17 | 3575 | `			}` |
|     53 | 3576 | `			ph7_result_string(pCtx,&up,1);` |
|     53 | 3577 | `			iStart = i + 1;` |
|     26 | 3578 | `		}` |
|    145 | 3579 | `	}` |
|     21 | 3580 | `	if( nLen > iStart ){` |
|     21 | 3581 | `		ph7_result_string(pCtx,&zIn[iStart],nLen - iStart);` |
|     10 | 3582 | `	}` |
|     21 | 3583 | `	return PH7_OK;` |
|     12 | 3584 | `}` |
|      - | 3585 | `/*` |
|      - | 3586 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3587 | ` *  Returns input repeated multiplier times.` |
|      - | 3588 | ` * Parameters` |
|      - | 3589 | ` *  $string` |
|      - | 3590 | ` *   String to be repeated.` |
|      - | 3591 | ` * $multiplier` |
|      - | 3592 | ` *  Number of time the input string should be repeated.` |
|      - | 3593 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3594 | ` *  to 0, the function will return an empty string.` |
|      - | 3595 | ` * Return` |
|      - | 3596 | ` *  The repeated string.` |
|      - | 3597 | ` */` |
|  20444 | 3598 | `PH7_PRIVATE int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3599 | `{` |
|      - | 3600 | `	const char *zIn;` |
|      - | 3601 | `	int nLen;` |
|      - | 3602 | `	ph7_int64 nMul;` |
|      - | 3603 | `	int rc;` |
|  20447 | 3604 | `	if( nArg < 2 ){` |
|      - | 3605 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3606 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3607 | `		return PH7_OK;` |
|      - | 3608 | `	}` |
|      - | 3609 | `	/* Extract the target string */` |
|  20447 | 3610 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3611 | `	/* Resolve $times through the shared ZPP helper so a lossy float / float-string` |
|      - | 3612 | `	 * carries php's precision deprecation and NAN/INF/non-numeric fail with php's` |
|      - | 3613 | `	 * TypeError — a bare ph7_value_to_int64() coerced them silently. */` |
|      - | 3614 | `	{` |
|  20447 | 3615 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_repeat",2,"$times","int",&nMul);` |
|  20447 | 3616 | `		if( rcArg != PH7_OK ){` |
|      5 | 3617 | `			return rcArg;` |
|      - | 3618 | `		}` |
|      - | 3619 | `	}` |
|  20443 | 3620 | `	if( nMul < 0 ){` |
|      3 | 3621 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 3622 | `			"str_repeat(): Argument #2 ($times) must be greater than or equal to 0");` |
|      - | 3623 | `	}` |
|  20441 | 3624 | `	if( nLen < 1 \|\| nMul < 1 ){` |
|      - | 3625 | `		/* Empty input or a zero multiplier yields the empty string (PHP). */` |
|    ! 0 | 3626 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3627 | `		return PH7_OK;` |
|      - | 3628 | `	}` |
|      - | 3629 | `	/* Perform the requested operation */` |
| 223892 | 3630 | `	for(;;){` |
| 447787 | 3631 | `		if( !nMul ){` |
|  20441 | 3632 | `			break;` |
|      - | 3633 | `		}` |
|      - | 3634 | `		/* Append the copy */` |
| 427349 | 3635 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 427349 | 3636 | `		if( rc != PH7_OK ){` |
|      - | 3637 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 3638 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 3639 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 3640 | `		}` |
| 427349 | 3641 | `		nMul--;` |
|      3 | 3642 | `	}` |
|  20441 | 3643 | `	return PH7_OK;` |
|  10225 | 3644 | `}` |
|      - | 3645 | `/*` |
|      - | 3646 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3647 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3648 | ` * Parameters` |
|      - | 3649 | ` *  $string` |
|      - | 3650 | ` *   The input string.` |
|      - | 3651 | ` * $is_xhtml` |
|      - | 3652 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3653 | ` * Return` |
|      - | 3654 | ` *  The processed string.` |
|      - | 3655 | ` */` |
|      4 | 3656 | `PH7_PRIVATE int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3657 | `{` |
|      - | 3658 | `	const char *zIn,*zCur,*zEnd;` |
|      5 | 3659 | `	int is_xhtml = 1; /* Default to XHTML-style '<br />' like PHP */` |
|      - | 3660 | `	int nLen;` |
|      5 | 3661 | `	if( nArg < 1 ){` |
|      - | 3662 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 3663 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3664 | `		return PH7_OK;` |
|      - | 3665 | `	}` |
|      - | 3666 | `	/* Extract the target string */` |
|      5 | 3667 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3668 | `	if( nLen < 1 ){` |
|      - | 3669 | `		/* Empty string,return null */` |
|    ! 0 | 3670 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3671 | `		return PH7_OK;` |
|      - | 3672 | `	}` |
|      5 | 3673 | `	if( nArg > 1 ){` |
|      3 | 3674 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3675 | `	}` |
|      5 | 3676 | `	zEnd = &zIn[nLen];` |
|      - | 3677 | `	/* Perform the requested operation */` |
|      4 | 3678 | `	for(;;){` |
|      9 | 3679 | `		zCur = zIn;` |
|      - | 3680 | `		/* Delimit the string */` |
|     21 | 3681 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3682 | `			zIn++;` |
|      1 | 3683 | `		}` |
|      9 | 3684 | `		if( zCur < zIn ){` |
|      - | 3685 | `			/* Output chunk verbatim */` |
|      9 | 3686 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3687 | `		}` |
|      9 | 3688 | `		if( zIn >= zEnd ){` |
|      - | 3689 | `			/* No more input to process */` |
|      5 | 3690 | `			break;` |
|      - | 3691 | `		}` |
|      - | 3692 | `		/* Output the HTML line break */` |
|      - | 3693 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br />' (space before the slash), otherwise use '<br>' */` |
|      5 | 3694 | `		if( is_xhtml ){` |
|      3 | 3695 | `			ph7_result_string(pCtx,"<br />",(int)sizeof("<br />")-1);` |
|      2 | 3696 | `		}else{` |
|      3 | 3697 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3698 | `		}` |
|      5 | 3699 | `		zCur = zIn;` |
|      - | 3700 | `		/* Append trailing line */` |
|     11 | 3701 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3702 | `			zIn++;` |
|      1 | 3703 | `		}` |
|      5 | 3704 | `		if( zCur < zIn ){` |
|      - | 3705 | `			/* Output chunk verbatim */` |
|      5 | 3706 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3707 | `		}` |
|      1 | 3708 | `	}` |
|      5 | 3709 | `	return PH7_OK;` |
|      3 | 3710 | `}` |
|      - | 3711 | `/*` |
|      - | 3712 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3713 | ` *  According to the PHP reference manual.` |
|      - | 3714 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3715 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3716 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3717 | ` * This applies to both sprintf() and printf().` |
|      - | 3718 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3719 | ` * or more of these elements, in order:` |
|      - | 3720 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3721 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3722 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 3723 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 3724 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 3725 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 3726 | ` *   it with a single quote ('). See the examples below.` |
|      - | 3727 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 3728 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 3729 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 3730 | ` *   should result in.` |
|      - | 3731 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 3732 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 3733 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 3734 | ` *   limit to the string.` |
|      - | 3735 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 3736 | ` *       % - a literal percent character. No argument is required.` |
|      - | 3737 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 3738 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 3739 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 3740 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 3741 | ` * 	     for the number of digits after the decimal point.` |
|      - | 3742 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 3743 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 3744 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 3745 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 3746 | ` *       g - shorter of %e and %f.` |
|      - | 3747 | ` *       G - shorter of %E and %f.` |
|      - | 3748 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 3749 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 3750 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 3751 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 3752 | ` */` |
|      - | 3753 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3754 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 3755 | `/*` |
|      - | 3756 | ` * Symisc eXtension.` |
|      - | 3757 | ` * string size_format(int64 $size)` |
|      - | 3758 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 3759 | ` *  Example:` |
|      - | 3760 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 3761 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 3762 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 3763 | ` * Parameter` |
|      - | 3764 | ` *  $size` |
|      - | 3765 | ` *    Entity size in bytes.` |
|      - | 3766 | ` * Return` |
|      - | 3767 | ` *   Formatted string representation of the given size.` |
|      - | 3768 | ` */` |
|     24 | 3769 | `PH7_PRIVATE int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3770 | `{` |
|      - | 3771 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 3772 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 3773 | `	sxi32 nRest,i_32;` |
|      - | 3774 | `	ph7_int64 iSize;` |
|     25 | 3775 | `	int c = -1; /* index in zUnit[] */` |
|      - | 3776 |  |
|     25 | 3777 | `	if( nArg < 1 ){` |
|      - | 3778 | `		/* Missing argument,return the empty string */` |
|      3 | 3779 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3780 | `		return PH7_OK;` |
|      - | 3781 | `	}` |
|      - | 3782 | `	/* Extract the given size */` |
|     23 | 3783 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 3784 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 3785 | `		/* Don't bother formatting,return immediately */` |
|      5 | 3786 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 3787 | `		return PH7_OK;` |
|      - | 3788 | `	}` |
|     19 | 3789 | `	for(;;){` |
|     39 | 3790 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 3791 | `		iSize >>= 10;` |
|     39 | 3792 | `		c++;` |
|     39 | 3793 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 3794 | `			break;` |
|      - | 3795 | `		}` |
|      1 | 3796 | `	}` |
|     19 | 3797 | `	nRest /= 100;` |
|     19 | 3798 | `	if( nRest > 9 ){` |
|    ! 0 | 3799 | `		nRest = 9;` |
|    ! 0 | 3800 | `	}` |
|     19 | 3801 | `	if( iSize > 999 ){` |
|    ! 0 | 3802 | `		c++;` |
|    ! 0 | 3803 | `		nRest = 9;` |
|    ! 0 | 3804 | `		iSize = 0;` |
|    ! 0 | 3805 | `	}` |
|     19 | 3806 | `	i_32 = (sxi32)iSize;` |
|      - | 3807 | `	/* Format */` |
|     19 | 3808 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 3809 | `	return PH7_OK;` |
|     13 | 3810 | `}` |
|      - | 3811 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3812 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 3813 | `/*` |
|      - | 3814 | ` * string str_shuffle(string $str)` |
|      - | 3815 |  |
|      - | 3816 | ` *  Randomly shuffles a string.` |
|      - | 3817 | ` * Parameters` |
|      - | 3818 | ` *  $str` |
|      - | 3819 | ` *   The input string.` |
|      - | 3820 | ` * Return` |
|      - | 3821 | ` *  Returns the shuffled string.` |
|      - | 3822 | ` */` |
|     10 | 3823 | `PH7_PRIVATE int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3824 | `{` |
|      - | 3825 | `	const char *zString;` |
|      - | 3826 | `	int nLen,i,c;` |
|      - | 3827 | `	sxu32 iR;` |
|     11 | 3828 | `	if( nArg < 1 ){` |
|      - | 3829 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 3830 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3831 | `		return PH7_OK;` |
|      - | 3832 | `	}` |
|      - | 3833 | `	/* Extract the target string */` |
|     11 | 3834 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 3835 | `	if( nLen < 1 ){` |
|      - | 3836 | `		/* Nothing to shuffle */` |
|      3 | 3837 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3838 | `		return PH7_OK;` |
|      - | 3839 | `	}` |
|      - | 3840 | `	/* Shuffle the string */` |
|     43 | 3841 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 3842 | `		/* Generate a random number first */` |
|     35 | 3843 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 3844 | `		/* Extract a random offset */` |
|     35 | 3845 | `		c = zString[iR % nLen];` |
|      - | 3846 | `		/* Append it */` |
|     35 | 3847 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 3848 | `	}` |
|      9 | 3849 | `	return PH7_OK;` |
|      6 | 3850 | `}` |
|      - | 3851 | `/*` |
|      - | 3852 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 3853 | ` *  Convert a string to an array.` |
|      - | 3854 | ` * Parameters` |
|      - | 3855 | ` * $string` |
|      - | 3856 | ` *  The input string.` |
|      - | 3857 | ` * $split_length` |
|      - | 3858 | ` *  Maximum length of the chunk.` |
|      - | 3859 | ` * Return` |
|      - | 3860 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 3861 | ` *  except possibly the last one which may be shorter.` |
|      - | 3862 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 3863 | ` *  as the first (and only) array element.` |
|      - | 3864 | ` *  An empty string returns an empty array.` |
|      - | 3865 | ` * Errors` |
|      - | 3866 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 3867 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 3868 | ` *  ValueError if $split_length is less than 1.` |
|      - | 3869 | ` */` |
|     26 | 3870 | `PH7_PRIVATE int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3871 | `{` |
|      - | 3872 | `	const char *zString,*zEnd;` |
|      - | 3873 | `	ph7_value *pArray,*pValue;` |
|      - | 3874 | `	int split_len;` |
|      - | 3875 | `	int nLen;` |
|     29 | 3876 | `	if( nArg < 1 ){` |
|    ! 0 | 3877 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3878 | `			"ArgumentCountError",` |
|      - | 3879 | `			"str_split() expects at least 1 argument, %d given",` |
|    ! 0 | 3880 | `			nArg` |
|      - | 3881 | `			);` |
|      - | 3882 | `	}` |
|      - | 3883 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     39 | 3884 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     42 | 3885 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     26 | 3886 | `	    ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 3887 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3888 | `			"TypeError",` |
|      - | 3889 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 3890 | `			ph7_type_name(apArg[0])` |
|      - | 3891 | `			);` |
|      - | 3892 | `	}` |
|      - | 3893 | `	/* Point to the target string */` |
|     29 | 3894 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     29 | 3895 | `	split_len = (int)sizeof(char);` |
|     29 | 3896 | `	if( nArg > 1 ){` |
|      - | 3897 | `		/* Split length */` |
|     17 | 3898 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 3899 | `		if( split_len < 1 ){` |
|      6 | 3900 | `			return PH7_VmThrowException(pCtx,` |
|      - | 3901 | `				"ValueError",` |
|      - | 3902 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 3903 | `				);` |
|      - | 3904 | `		}` |
|     11 | 3905 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 3906 | `			split_len = nLen;` |
|      1 | 3907 | `		}` |
|      5 | 3908 | `	}` |
|      - | 3909 | `	/* Create the array and the scalar value */` |
|     23 | 3910 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 3911 | `	/*Chunk value */` |
|     23 | 3912 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     23 | 3913 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 3914 | `		/* Return FALSE */` |
|    ! 0 | 3915 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3916 | `		return PH7_OK;` |
|      - | 3917 | `	}` |
|      - | 3918 | `	/* Point to the end of the string */` |
|     23 | 3919 | `	zEnd = &zString[nLen];` |
|      - | 3920 | `	/* Perform the requested operation */` |
|    131 | 3921 | `	for(;;){` |
|      - | 3922 | `		int nMax;` |
|    143 | 3923 | `		if( zString >= zEnd ){` |
|      - | 3924 | `			/* No more input to process */` |
|     23 | 3925 | `			break;` |
|      - | 3926 | `		}` |
|    121 | 3927 | `		nMax = (int)(zEnd-zString);` |
|    121 | 3928 | `		if( nMax < split_len ){` |
|      3 | 3929 | `			split_len = nMax;` |
|      1 | 3930 | `		}` |
|      - | 3931 | `		/* Copy the current chunk */` |
|    121 | 3932 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 3933 | `		/* Insert it */` |
|    121 | 3934 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 3935 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 3936 | `		}` |
|      - | 3937 | `		/* reset the string cursor */` |
|    121 | 3938 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 3939 | `		/* Update position */` |
|    121 | 3940 | `		zString += split_len;` |
|      1 | 3941 | `	}` |
|      - | 3942 | `	/*` |
|      - | 3943 | `	 * Return the array.` |
|      - | 3944 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 3945 | `	 * upon we return from this function.` |
|      - | 3946 | `	 */` |
|     23 | 3947 | `	ph7_result_value(pCtx,pArray);` |
|     23 | 3948 | `	return PH7_OK;` |
|     16 | 3949 | `}` |
|      - | 3950 | `/*` |
|      - | 3951 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 3952 | ` * Refer to [strspn()].` |
|      - | 3953 | ` */` |
|     28 | 3954 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 3955 | `{` |
|     29 | 3956 | `	const char *zIn = *pzIn;` |
|      - | 3957 | `	const char *zPtr;` |
|      - | 3958 | `	/* Ignore leading white spaces */` |
|     29 | 3959 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 3960 | `		zIn++;` |
|    ! 0 | 3961 | `	}` |
|     29 | 3962 | `	if( zIn >= zEnd ){` |
|      - | 3963 | `		/* End of input */` |
|    ! 0 | 3964 | `		return SXERR_EOF;` |
|      - | 3965 | `	}` |
|     29 | 3966 | `	zPtr = zIn;` |
|      - | 3967 | `	/* Extract the token */` |
|    201 | 3968 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 3969 | `		zIn++;` |
|      1 | 3970 | `	}` |
|     29 | 3971 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 3972 | `	/* Synchronize pointers */` |
|     29 | 3973 | `	*pzIn = zIn;` |
|      - | 3974 | `	/* Return to the caller */` |
|     29 | 3975 | `	return SXRET_OK;` |
|     15 | 3976 | `}` |
|      - | 3977 | `/*` |
|      - | 3978 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 3979 | ` * return the longest match.` |
|      - | 3980 | ` * Refer to [strspn()].` |
|      - | 3981 | ` */` |
|     18 | 3982 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 3983 | `{` |
|     19 | 3984 | `	const char *zEnd = &zString[nLen];` |
|     19 | 3985 | `	const char *zIn = zString;` |
|      - | 3986 | `	int i,c;` |
|     45 | 3987 | `	for(;;){` |
|     91 | 3988 | `		if( zString >= zEnd ){` |
|      7 | 3989 | `			break;` |
|      - | 3990 | `		}` |
|      - | 3991 | `		/* Extract current character */` |
|     85 | 3992 | `		c = zString[0];` |
|      - | 3993 | `		/* Perform the lookup */` |
|    383 | 3994 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 3995 | `			if( c == zMask[i] ){` |
|      - | 3996 | `				/* Character found */` |
|     73 | 3997 | `				break;` |
|      - | 3998 | `			}` |
|    150 | 3999 | `		}` |
|     85 | 4000 | `		if( i >= nMaskLen ){` |
|      - | 4001 | `			/* Character not in the current mask,break immediately */` |
|     13 | 4002 | `			break;` |
|      - | 4003 | `		}` |
|      - | 4004 | `		/* Advance cursor */` |
|     73 | 4005 | `		zString++;` |
|      1 | 4006 | `	}` |
|      - | 4007 | `	/* Longest match */` |
|     19 | 4008 | `	return (int)(zString-zIn);` |
|      1 | 4009 | `}` |
|      - | 4010 | `/*` |
|      - | 4011 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 4012 | ` * Refer to [strcspn()].` |
|      - | 4013 | ` */` |
|     10 | 4014 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 4015 | `{` |
|     11 | 4016 | `	const char *zEnd = &zString[nLen];` |
|     11 | 4017 | `	const char *zIn = zString;` |
|      - | 4018 | `	int i,c;` |
|     12 | 4019 | `	for(;;){` |
|     25 | 4020 | `		if( zString >= zEnd ){` |
|      3 | 4021 | `			break;` |
|      - | 4022 | `		}` |
|      - | 4023 | `		/* Extract current character */` |
|     23 | 4024 | `		c = zString[0];` |
|      - | 4025 | `		/* Perform the lookup */` |
|     51 | 4026 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 4027 | `			if( c == zMask[i] ){` |
|      9 | 4028 | `				break;` |
|      - | 4029 | `			}` |
|     15 | 4030 | `		}` |
|     23 | 4031 | `		if( i < nMaskLen ){` |
|      - | 4032 | `			/* Character in the current mask,break immediately */` |
|      9 | 4033 | `			break;` |
|      - | 4034 | `		}` |
|      - | 4035 | `		/* Advance cursor */` |
|     15 | 4036 | `		zString++;` |
|      1 | 4037 | `	}` |
|      - | 4038 | `	/* Longest match */` |
|     11 | 4039 | `	return (int)(zString-zIn);` |
|      1 | 4040 | `}` |
|      - | 4041 | `/*` |
|      - | 4042 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 4043 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 4044 | ` *  of characters contained within a given mask.` |
|      - | 4045 | ` * Parameters` |
|      - | 4046 | ` * $str` |
|      - | 4047 | ` *  The input string.` |
|      - | 4048 | ` * $mask` |
|      - | 4049 | ` *  The list of allowable characters.` |
|      - | 4050 | ` * $start` |
|      - | 4051 | ` *  The position in subject to start searching.` |
|      - | 4052 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 4053 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 4054 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 4055 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 4056 | ` *  start'th position from the end of subject.` |
|      - | 4057 | ` * $length` |
|      - | 4058 | ` *  The length of the segment from subject to examine.` |
|      - | 4059 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 4060 | ` *  characters after the starting position.` |
|      - | 4061 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 4062 | ` *  position up to length characters from the end of subject.` |
|      - | 4063 | ` * Return` |
|      - | 4064 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 4065 | ` * in mask.` |
|      - | 4066 | ` */` |
|     24 | 4067 | `PH7_PRIVATE int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4068 | `{` |
|      - | 4069 | `	const char *zString,*zMask,*zEnd;` |
|      - | 4070 | `	int iMasklen,iLen;` |
|      - | 4071 | `	SyString sToken;` |
|     25 | 4072 | `	int iCount = 0;` |
|      - | 4073 | `	int rc;` |
|     25 | 4074 | `	if( nArg < 2 ){` |
|      - | 4075 | `		/* Missing agruments,return zero */` |
|    ! 0 | 4076 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4077 | `		return PH7_OK;` |
|      - | 4078 | `	}` |
|      - | 4079 | `	/* Extract the target string */` |
|     25 | 4080 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 4081 | `	/* Extract the mask */` |
|     25 | 4082 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 4083 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 4084 | `		/* Nothing to process,return zero */` |
|      7 | 4085 | `		ph7_result_int(pCtx,0);` |
|      7 | 4086 | `		return PH7_OK;` |
|      - | 4087 | `	}` |
|     19 | 4088 | `	if( nArg > 2 ){` |
|      - | 4089 | `		int nOfft;` |
|      - | 4090 | `		/* Extract the offset */` |
|      9 | 4091 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 4092 | `		if( nOfft < 0 ){` |
|    ! 0 | 4093 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 4094 | `			if( zBase > zString ){` |
|    ! 0 | 4095 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 4096 | `				zString = zBase;` |
|    ! 0 | 4097 | `			}else{` |
|      - | 4098 | `				/* Invalid offset */` |
|    ! 0 | 4099 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 4100 | `				return PH7_OK;` |
|      - | 4101 | `			}` |
|    ! 0 | 4102 | `		}else{` |
|      9 | 4103 | `			if( nOfft >= iLen ){` |
|      - | 4104 | `				/* Invalid offset */` |
|    ! 0 | 4105 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 4106 | `				return PH7_OK;` |
|    ! 0 | 4107 | `			}else{` |
|      - | 4108 | `				/* Update offset */` |
|      9 | 4109 | `				zString += nOfft;` |
|      9 | 4110 | `				iLen -= nOfft;` |
|      - | 4111 | `			}` |
|      - | 4112 | `		}` |
|      9 | 4113 | `		if( nArg > 3 ){` |
|      - | 4114 | `			int iUserlen;` |
|      - | 4115 | `			/* Extract the desired length */` |
|      9 | 4116 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 4117 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 4118 | `				iLen = iUserlen;` |
|      2 | 4119 | `			}` |
|      4 | 4120 | `		}` |
|      4 | 4121 | `	}` |
|      - | 4122 | `	/* Point to the end of the string */` |
|     19 | 4123 | `	zEnd = &zString[iLen];` |
|      - | 4124 | `	/* Extract the first non-space token */` |
|     19 | 4125 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 4126 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 4127 | `		/* Compare against the current mask */` |
|     19 | 4128 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 4129 | `	}` |
|      - | 4130 | `	/* Longest match */` |
|     19 | 4131 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 4132 | `	return PH7_OK;` |
|     13 | 4133 | `}` |
|      - | 4134 | `/*` |
|      - | 4135 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 4136 | ` *  Find length of initial segment not matching mask.` |
|      - | 4137 | ` * Parameters` |
|      - | 4138 | ` * $str` |
|      - | 4139 | ` *  The input string.` |
|      - | 4140 | ` * $mask` |
|      - | 4141 | ` *  The list of not allowed characters.` |
|      - | 4142 | ` * $start` |
|      - | 4143 | ` *  The position in subject to start searching.` |
|      - | 4144 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 4145 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 4146 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 4147 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 4148 | ` *  start'th position from the end of subject.` |
|      - | 4149 | ` * $length` |
|      - | 4150 | ` *  The length of the segment from subject to examine.` |
|      - | 4151 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 4152 | ` *  characters after the starting position.` |
|      - | 4153 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 4154 | ` *  position up to length characters from the end of subject.` |
|      - | 4155 | ` * Return` |
|      - | 4156 | ` *  Returns the length of the segment as an integer.` |
|      - | 4157 | ` */` |
|     14 | 4158 | `PH7_PRIVATE int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4159 | `{` |
|      - | 4160 | `	const char *zString,*zMask,*zEnd;` |
|      - | 4161 | `	int iMasklen,iLen;` |
|      - | 4162 | `	SyString sToken;` |
|     15 | 4163 | `	int iCount = 0;` |
|      - | 4164 | `	int rc;` |
|     15 | 4165 | `	if( nArg < 2 ){` |
|      - | 4166 | `		/* Missing agruments,return zero */` |
|    ! 0 | 4167 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4168 | `		return PH7_OK;` |
|      - | 4169 | `	}` |
|      - | 4170 | `	/* Extract the target string */` |
|     15 | 4171 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 4172 | `	/* Extract the mask */` |
|     15 | 4173 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 4174 | `	if( iLen < 1 ){` |
|      - | 4175 | `		/* Nothing to process,return zero */` |
|    ! 0 | 4176 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4177 | `		return PH7_OK;` |
|      - | 4178 | `	}` |
|     15 | 4179 | `	if( iMasklen < 1 ){` |
|      - | 4180 | `		/* No given mask,return the string length */` |
|      3 | 4181 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 4182 | `		return PH7_OK;` |
|      - | 4183 | `	}` |
|     13 | 4184 | `	if( nArg > 2 ){` |
|      - | 4185 | `		int nOfft;` |
|      - | 4186 | `		/* Extract the offset */` |
|     11 | 4187 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 4188 | `		if( nOfft < 0 ){` |
|    ! 0 | 4189 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 4190 | `			if( zBase > zString ){` |
|    ! 0 | 4191 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 4192 | `				zString = zBase;` |
|    ! 0 | 4193 | `			}else{` |
|      - | 4194 | `				/* Invalid offset */` |
|    ! 0 | 4195 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 4196 | `				return PH7_OK;` |
|      - | 4197 | `			}` |
|    ! 0 | 4198 | `		}else{` |
|     11 | 4199 | `			if( nOfft >= iLen ){` |
|      - | 4200 | `				/* Invalid offset */` |
|      3 | 4201 | `				ph7_result_int(pCtx,0);` |
|      3 | 4202 | `				return PH7_OK;` |
|    ! 0 | 4203 | `			}else{` |
|      - | 4204 | `				/* Update offset */` |
|      9 | 4205 | `				zString += nOfft;` |
|      9 | 4206 | `				iLen -= nOfft;` |
|      - | 4207 | `			}` |
|      - | 4208 | `		}` |
|      9 | 4209 | `		if( nArg > 3 ){` |
|      - | 4210 | `			int iUserlen;` |
|      - | 4211 | `			/* Extract the desired length */` |
|    ! 0 | 4212 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 4213 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 4214 | `				iLen = iUserlen;` |
|    ! 0 | 4215 | `			}` |
|    ! 0 | 4216 | `		}` |
|      4 | 4217 | `	}` |
|      - | 4218 | `	/* Point to the end of the string */` |
|     11 | 4219 | `	zEnd = &zString[iLen];` |
|      - | 4220 | `	/* Extract the first non-space token */` |
|     11 | 4221 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 4222 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 4223 | `		/* Compare against the current mask */` |
|     11 | 4224 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 4225 | `	}` |
|      - | 4226 | `	/* Longest match */` |
|     11 | 4227 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 4228 | `	return PH7_OK;` |
|      8 | 4229 | `}` |
|      - | 4230 | `/*` |
|      - | 4231 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 4232 | ` *  Search a string for any of a set of characters.` |
|      - | 4233 | ` * Parameters` |
|      - | 4234 | ` *  $haystack` |
|      - | 4235 | ` *   The string where char_list is looked for.` |
|      - | 4236 | ` *  $char_list` |
|      - | 4237 | ` *   This parameter is case sensitive.` |
|      - | 4238 | ` * Return` |
|      - | 4239 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 4240 | ` */` |
|      4 | 4241 | `PH7_PRIVATE int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4242 | `{` |
|      - | 4243 | `	const char *zString,*zList,*zEnd;` |
|      - | 4244 | `	int iLen,iListLen,i,c;` |
|      - | 4245 | `	sxu32 nOfft,nMax;` |
|      - | 4246 | `	sxi32 rc;` |
|      5 | 4247 | `	if( nArg < 2 ){` |
|      - | 4248 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 4249 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4250 | `		return PH7_OK;` |
|      - | 4251 | `	}` |
|      - | 4252 | `	/* Extract the haystack and the char list */` |
|      5 | 4253 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 4254 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 4255 | `	if( iLen < 1 ){` |
|      - | 4256 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 4257 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4258 | `		return PH7_OK;` |
|      - | 4259 | `	}` |
|      - | 4260 | `	/* Point to the end of the string */` |
|      5 | 4261 | `	zEnd = &zString[iLen];` |
|      5 | 4262 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 4263 | `	/* perform the requested operation */` |
|     15 | 4264 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 4265 | `		c = zList[i];` |
|     11 | 4266 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 4267 | `		if( rc == SXRET_OK ){` |
|      5 | 4268 | `			if( nMax < nOfft ){` |
|      3 | 4269 | `				nOfft = nMax;` |
|      1 | 4270 | `			}` |
|      2 | 4271 | `		}` |
|      6 | 4272 | `	}` |
|      5 | 4273 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 4274 | `		/* No such substring,return FALSE */` |
|      3 | 4275 | `		ph7_result_bool(pCtx,0);` |
|      2 | 4276 | `	}else{` |
|      - | 4277 | `		/* Return the substring */` |
|      3 | 4278 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 4279 | `	}` |
|      5 | 4280 | `	return PH7_OK;` |
|      3 | 4281 | `}` |
|      - | 4282 | `/* SPDX-SnippetBegin */` |
|      - | 4283 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 4284 | `/* SPDX-License-Identifier: blessing */` |
|      - | 4285 | `/*` |
|      - | 4286 | ` * string soundex(string $str)` |
|      - | 4287 | ` *  Calculate the soundex key of a string.` |
|      - | 4288 | ` * Parameters` |
|      - | 4289 | ` *  $str` |
|      - | 4290 | ` *   The input string.` |
|      - | 4291 | ` * Return` |
|      - | 4292 | ` *  Returns the soundex key as a string.` |
|      - | 4293 | ` * Note:` |
|      - | 4294 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 4295 | ` * source tree.` |
|      - | 4296 | ` */` |
|     22 | 4297 | `PH7_PRIVATE int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4298 | `{` |
|      - | 4299 | `	const unsigned char *zIn;` |
|      - | 4300 | `	char zResult[8];` |
|      - | 4301 | `	int i, j;` |
|      - | 4302 | `	static const unsigned char iCode[] = {` |
|      - | 4303 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 4304 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 4305 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 4306 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 4307 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 4308 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 4309 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 4310 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 4311 | `	};` |
|     23 | 4312 | `	if( nArg < 1 ){` |
|      - | 4313 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4314 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4315 | `		return PH7_OK;` |
|      - | 4316 | `	}` |
|     23 | 4317 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     35 | 4318 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     23 | 4319 | `	if( zIn[i] ){` |
|     17 | 4320 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 4321 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 4322 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 4323 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 4324 | `			if( code>0 ){` |
|     45 | 4325 | `				if( code!=prevcode ){` |
|     33 | 4326 | `					prevcode = (unsigned char)code;` |
|     33 | 4327 | `					zResult[j++] = (char)code + '0';` |
|     16 | 4328 | `				}` |
|     23 | 4329 | `			}else{` |
|     49 | 4330 | `				prevcode = 0;` |
|      - | 4331 | `			}` |
|     47 | 4332 | `		}` |
|     33 | 4333 | `		while( j<4 ){` |
|     17 | 4334 | `			zResult[j++] = '0';` |
|      1 | 4335 | `		}` |
|     17 | 4336 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 4337 | `	}else{` |
|      - | 4338 | `	  /* No alphabetic character: PHP returns "0000" (not the SQLite "?000"). */` |
|      7 | 4339 | `	  ph7_result_string(pCtx,"0000",4);` |
|      - | 4340 | `	}` |
|     23 | 4341 | `	return PH7_OK;` |
|     12 | 4342 | `}` |
|      - | 4343 | `/* SPDX-SnippetEnd */` |
|      - | 4344 | `/*` |
|      - | 4345 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 4346 | ` *  Wraps a string to a given number of characters.` |
|      - | 4347 | ` * Parameters` |
|      - | 4348 | ` *  $str` |
|      - | 4349 | ` *   The input string.` |
|      - | 4350 | ` * $width` |
|      - | 4351 | ` *  The column width.` |
|      - | 4352 | ` * $break` |
|      - | 4353 | ` *  The line is broken using the optional break parameter.` |
|      - | 4354 | ` * Return` |
|      - | 4355 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 4356 | ` */` |
|     26 | 4357 | `PH7_PRIVATE int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4358 | `{` |
|      - | 4359 | `	const char *zIn,*zBreak;` |
|      - | 4360 | `	SyBlob sWorker;` |
|      - | 4361 | `	int iLen,iBreaklen,iWidth,iCut,iStart,iSpace,iCur;` |
|      - | 4362 | `	sxi32 rc;` |
|     27 | 4363 | `	if( nArg < 1 ){` |
|      - | 4364 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4365 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4366 | `		return PH7_OK;` |
|      - | 4367 | `	}` |
|      - | 4368 | `	/* Extract the input string */` |
|     27 | 4369 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 4370 | `	/* Width (default 75; PHP allows 0/negative — break at every space). */` |
|     27 | 4371 | `	iWidth = 75;` |
|     27 | 4372 | `	if( nArg > 1 ){` |
|     27 | 4373 | `		iWidth = ph7_value_to_int(apArg[1]);` |
|     13 | 4374 | `	}` |
|      - | 4375 | `	/* Break string (default "\n"). */` |
|     27 | 4376 | `	zBreak = "\n";` |
|     27 | 4377 | `	iBreaklen = (int)sizeof(char);` |
|     27 | 4378 | `	if( nArg > 2 ){` |
|     13 | 4379 | `		zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      6 | 4380 | `	}` |
|      - | 4381 | `	/* Cut long words? (default false). */` |
|     27 | 4382 | `	iCut = 0;` |
|     27 | 4383 | `	if( nArg > 3 ){` |
|      7 | 4384 | `		iCut = ph7_value_to_bool(apArg[3]);` |
|      3 | 4385 | `	}` |
|     27 | 4386 | `	if( iLen < 1 ){` |
|      - | 4387 | `		/* PHP returns the empty string for empty input before validating the other args. */` |
|      5 | 4388 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 4389 | `		return PH7_OK;` |
|      - | 4390 | `	}` |
|      - | 4391 | `	/* PHP 8 domain errors (catchable ValueError). */` |
|     23 | 4392 | `	if( iBreaklen < 1 ){` |
|      3 | 4393 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4394 | `			"wordwrap(): Argument #3 ($break) must not be empty");` |
|      - | 4395 | `	}` |
|     21 | 4396 | `	if( iWidth == 0 && iCut ){` |
|      3 | 4397 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4398 | `			"wordwrap(): Argument #4 ($cut_long_words) cannot be true when argument #2 ($width) is 0");` |
|      - | 4399 | `	}` |
|      - | 4400 | `	/*` |
|      - | 4401 | `	 * PHP's algorithm: a single left-to-right pass tracking the start of the` |
|      - | 4402 | `	 * current line (iStart) and the position of the last space seen on it` |
|      - | 4403 | `	 * (iSpace). A break is emitted when the line reaches the width, at the last` |
|      - | 4404 | `	 * space if there was one, otherwise (only when cut is enabled) hard at the` |
|      - | 4405 | `	 * boundary. An existing break sequence in the input resets the line.` |
|      - | 4406 | `	 */` |
|     19 | 4407 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     19 | 4408 | `	iStart = iSpace = iCur = 0;` |
|     19 | 4409 | `	rc = SXRET_OK;` |
|    551 | 4410 | `	while( iCur < iLen ){` |
|    533 | 4411 | `		if( iBreaklen <= iLen - iCur && SyMemcmp(&zIn[iCur],zBreak,(sxu32)iBreaklen) == 0 ){` |
|      - | 4412 | `			/* Existing break sequence in the input: copy it verbatim and reset the line. */` |
|    ! 0 | 4413 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart + iBreaklen));` |
|    ! 0 | 4414 | `			if( rc != SXRET_OK ){ goto oom; }` |
|    ! 0 | 4415 | `			iCur += iBreaklen;` |
|    ! 0 | 4416 | `			iStart = iSpace = iCur;` |
|    ! 0 | 4417 | `			continue;` |
|    533 | 4418 | `		}else if( zIn[iCur] == ' ' ){` |
|     67 | 4419 | `			if( iCur - iStart >= iWidth ){` |
|      - | 4420 | `				/* The line already fills the width at this space: break here (the space is consumed). */` |
|     13 | 4421 | `				rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     13 | 4422 | `				if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     13 | 4423 | `				if( rc != SXRET_OK ){ goto oom; }` |
|     13 | 4424 | `				iStart = iCur + 1;` |
|      6 | 4425 | `			}` |
|     67 | 4426 | `			iSpace = iCur;` |
|    500 | 4427 | `		}else if( iCut && iCur - iStart >= iWidth && iStart >= iSpace ){` |
|      - | 4428 | `			/* A word longer than the width with no space to break at: hard-cut at the boundary. */` |
|      7 | 4429 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|      7 | 4430 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|      7 | 4431 | `			if( rc != SXRET_OK ){ goto oom; }` |
|      7 | 4432 | `			iStart = iSpace = iCur;` |
|    464 | 4433 | `		}else if( iCur - iStart >= iWidth && iStart < iSpace ){` |
|      - | 4434 | `			/* Past the width mid-word: wrap back to the last space (which is consumed). */` |
|     17 | 4435 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iSpace - iStart));` |
|     17 | 4436 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     17 | 4437 | `			if( rc != SXRET_OK ){ goto oom; }` |
|     17 | 4438 | `			iStart = iSpace = iSpace + 1;` |
|      8 | 4439 | `		}` |
|    533 | 4440 | `		iCur++;` |
|      1 | 4441 | `	}` |
|      - | 4442 | `	/* Emit the trailing chunk. */` |
|     19 | 4443 | `	if( iStart < iCur ){` |
|     19 | 4444 | `		rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     19 | 4445 | `		if( rc != SXRET_OK ){ goto oom; }` |
|      9 | 4446 | `	}` |
|     19 | 4447 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|     19 | 4448 | `	SyBlobRelease(&sWorker);` |
|     19 | 4449 | `	return PH7_OK;` |
|    ! 0 | 4450 | `oom:` |
|    ! 0 | 4451 | `	SyBlobRelease(&sWorker);` |
|    ! 0 | 4452 | `	return PH7_ContextMemoryError(pCtx);` |
|     14 | 4453 | `}` |
|      - | 4454 | `/*` |
|      - | 4455 | ` * Check if the given character is a member of the given mask.` |
|      - | 4456 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 4457 | ` * Refer to [strtok()].` |
|      - | 4458 | ` */` |
|     30 | 4459 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 4460 | `{` |
|      - | 4461 | `	int i;` |
|     57 | 4462 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 4463 | `		if( c == zMask[i] ){` |
|     13 | 4464 | `			if( pOfft ){` |
|      5 | 4465 | `				*pOfft = i;` |
|      2 | 4466 | `			}` |
|     13 | 4467 | `			return TRUE;` |
|      - | 4468 | `		}` |
|     14 | 4469 | `	}` |
|     19 | 4470 | `	return FALSE;` |
|     16 | 4471 | `}` |
|      - | 4472 | `/*` |
|      - | 4473 | ` * Extract a single token from the input stream.` |
|      - | 4474 | ` * Refer to [strtok()].` |
|      - | 4475 | ` */` |
|      6 | 4476 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 4477 | `{` |
|      7 | 4478 | `	const char *zIn = *pzIn;` |
|      - | 4479 | `	const char *zPtr;` |
|      - | 4480 | `	/* Ignore leading delimiter */` |
|     11 | 4481 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 4482 | `		zIn++;` |
|      1 | 4483 | `	}` |
|      7 | 4484 | `	if( zIn >= zEnd ){` |
|      - | 4485 | `		/* End of input */` |
|    ! 0 | 4486 | `		return SXERR_EOF;` |
|      - | 4487 | `	}` |
|      7 | 4488 | `	zPtr = zIn;` |
|      - | 4489 | `	/* Extract the token */` |
|     13 | 4490 | `	while( zIn < zEnd ){` |
|     11 | 4491 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 4492 | `			/* UTF-8 stream */` |
|    ! 0 | 4493 | `			zIn++;` |
|    ! 0 | 4494 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 4495 | `		}else{` |
|     11 | 4496 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 4497 | `				break;` |
|      - | 4498 | `			}` |
|      7 | 4499 | `			zIn++;` |
|      - | 4500 | `		}` |
|      1 | 4501 | `	}` |
|      7 | 4502 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 4503 | `	/* Update the cursor */` |
|      7 | 4504 | `	*pzIn = zIn;` |
|      - | 4505 | `	/* Return to the caller */` |
|      7 | 4506 | `	return SXRET_OK;` |
|      4 | 4507 | `}` |
|      - | 4508 | `/* strtok auxiliary private data */` |
|      - | 4509 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 4510 | `struct strtok_aux_data` |
|      - | 4511 | `{` |
|      - | 4512 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 4513 | `	const char *zIn;   /* Current input stream */` |
|      - | 4514 | `	const char *zEnd;  /* End of input */` |
|      - | 4515 | `};` |
|      - | 4516 | `/*` |
|      - | 4517 | ` * string strtok(string $str,string $token)` |
|      - | 4518 | ` * string strtok(string $token)` |
|      - | 4519 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 4520 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 4521 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 4522 | ` *  words by using the space character as the token.` |
|      - | 4523 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 4524 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 4525 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 4526 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 4527 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 4528 | ` *  the argument are found.` |
|      - | 4529 | ` * Parameters` |
|      - | 4530 | ` *  $str` |
|      - | 4531 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 4532 | ` * $token` |
|      - | 4533 | ` *  The delimiter used when splitting up str.` |
|      - | 4534 | ` * Return` |
|      - | 4535 | ` *   Current token or FALSE on EOF.` |
|      - | 4536 | ` */` |
|      6 | 4537 | `PH7_PRIVATE int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4538 | `{` |
|      - | 4539 | `	strtok_aux_data *pAux;` |
|      - | 4540 | `	const char *zMask;` |
|      - | 4541 | `	SyString sToken;` |
|      - | 4542 | `	int nMasklen;` |
|      - | 4543 | `	sxi32 rc;` |
|      7 | 4544 | `	if( nArg < 2 ){` |
|      - | 4545 | `		/* Extract top aux data */` |
|      5 | 4546 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      5 | 4547 | `		if( pAux == 0 ){` |
|      - | 4548 | `			/* No aux data,return FALSE */` |
|    ! 0 | 4549 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 4550 | `			return PH7_OK;` |
|      - | 4551 | `		}` |
|      5 | 4552 | `		nMasklen = 0;` |
|      5 | 4553 | `		zMask = ""; /* cc warning */` |
|      5 | 4554 | `		if( nArg > 0 ){` |
|      - | 4555 | `			/* Extract the mask */` |
|      5 | 4556 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 4557 | `		}` |
|      5 | 4558 | `		if( nMasklen < 1 ){` |
|      - | 4559 | `			/* Invalid mask,return FALSE */` |
|    ! 0 | 4560 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 4561 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 4562 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 4563 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 4564 | `			return PH7_OK;` |
|      - | 4565 | `		}` |
|      - | 4566 | `		/* Extract the token */` |
|      5 | 4567 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 4568 | `		if( rc != SXRET_OK ){` |
|      - | 4569 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 4570 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 4571 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 4572 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 4573 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 4574 | `		}else{` |
|      - | 4575 | `			/* Return the extracted token */` |
|      5 | 4576 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 4577 | `		}` |
|      3 | 4578 | `	}else{` |
|      - | 4579 | `		const char *zInput,*zCur;` |
|      - | 4580 | `		char *zDup;` |
|      - | 4581 | `		int nLen;` |
|      - | 4582 | `		/* Extract the raw input */` |
|      3 | 4583 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4584 | `		if( nLen < 1 ){` |
|      - | 4585 | `			/* Empty input,return FALSE */` |
|    ! 0 | 4586 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 4587 | `			return PH7_OK;` |
|      - | 4588 | `		}` |
|      - | 4589 | `		/* Extract the mask */` |
|      3 | 4590 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 4591 | `		if( nMasklen < 1 ){` |
|      - | 4592 | `			/* Set a default mask */` |
|      - | 4593 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 4594 | `			zMask = TOK_MASK;` |
|    ! 0 | 4595 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 4596 | `#undef TOK_MASK` |
|    ! 0 | 4597 | `		}` |
|      - | 4598 | `		/* Extract a single token */` |
|      3 | 4599 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 4600 | `		if( rc != SXRET_OK ){` |
|      - | 4601 | `			/* Empty input */` |
|    ! 0 | 4602 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 4603 | `			return PH7_OK;` |
|    ! 0 | 4604 | `		}else{` |
|      - | 4605 | `			/* Return the extracted token */` |
|      3 | 4606 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 4607 | `		}` |
|      - | 4608 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 4609 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 4610 | `		if( pAux ){` |
|      3 | 4611 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 4612 | `			if( nLen < 1 ){` |
|    ! 0 | 4613 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 4614 | `				return PH7_OK;` |
|      - | 4615 | `			}` |
|      - | 4616 | `			/* Duplicate input */` |
|      3 | 4617 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 4618 | `			if( zDup  ){` |
|      3 | 4619 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 4620 | `				/* Register the aux data */` |
|      3 | 4621 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 4622 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 4623 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 4624 | `			}` |
|      1 | 4625 | `		}` |
|      - | 4626 | `	}` |
|      7 | 4627 | `	return PH7_OK;` |
|      4 | 4628 | `}` |
|      - | 4629 | `/*` |
|      - | 4630 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 4631 | ` *  Pad a string to a certain length with another string` |
|      - | 4632 | ` * Parameters` |
|      - | 4633 | ` *  $input` |
|      - | 4634 | ` *   The input string.` |
|      - | 4635 | ` * $pad_length` |
|      - | 4636 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 4637 | ` *   string, no padding takes place.` |
|      - | 4638 | ` * $pad_string` |
|      - | 4639 | ` *   Note:` |
|      - | 4640 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 4641 | ` *    divided by the pad_string's length.` |
|      - | 4642 | ` * $pad_type` |
|      - | 4643 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 4644 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 4645 | ` * Return` |
|      - | 4646 | ` *  The padded string.` |
|      - | 4647 | ` */` |
|     20 | 4648 | `PH7_PRIVATE int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4649 | `{` |
|      - | 4650 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 4651 | `	const char *zIn,*zPad;` |
|     22 | 4652 | `	if( nArg < 2 ){` |
|      - | 4653 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4654 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4655 | `		return PH7_OK;` |
|      - | 4656 | `	}` |
|      - | 4657 | `	/* Extract the target string */` |
|     22 | 4658 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 4659 | `	/* Padding length */` |
|      - | 4660 | `	{` |
|     22 | 4661 | `		sxi64 iTmp = 0;` |
|     22 | 4662 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_pad",2,"$length","int",&iTmp);` |
|     22 | 4663 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 4664 | `			return rcArg;` |
|      - | 4665 | `		}` |
|     22 | 4666 | `		iRealPad = iPadlen = (int)iTmp;` |
|      - | 4667 | `	}` |
|     22 | 4668 | `	if( iPadlen > 0 ){` |
|     20 | 4669 | `		iPadlen -= iLen;` |
|      9 | 4670 | `	}` |
|     22 | 4671 | `	if( iPadlen < 1  ){` |
|      - | 4672 | `		/* Return the string verbatim */` |
|      5 | 4673 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      5 | 4674 | `		return PH7_OK;` |
|      - | 4675 | `	}` |
|     18 | 4676 | `	zPad = " "; /* Whitespace padding */` |
|     18 | 4677 | `	iStrpad = (int)sizeof(char);` |
|     18 | 4678 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|     18 | 4679 | `	if( nArg > 2 ){` |
|      - | 4680 | `		/* Padding string */` |
|      7 | 4681 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      7 | 4682 | `		if( iStrpad < 1 ){` |
|      - | 4683 | `			/* An empty pad string throws a catchable ValueError in PHP 8` |
|      - | 4684 | `			 * (only reached once padding is actually required). */` |
|      3 | 4685 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4686 | `				"str_pad(): Argument #3 ($pad_string) must not be empty");` |
|      - | 4687 | `		}` |
|      5 | 4688 | `		if( nArg > 3 ){` |
|      - | 4689 | `			/* Padd type */` |
|      5 | 4690 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 4691 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 4692 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 4693 | `			}` |
|      2 | 4694 | `		}` |
|      2 | 4695 | `	}` |
|     16 | 4696 | `	iDiv = 1;` |
|     16 | 4697 | `	if( iType == 2 ){` |
|    ! 0 | 4698 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 4699 | `	}` |
|      - | 4700 | `	/* Perform the requested operation */` |
|     16 | 4701 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 4702 | `		jPad = iStrpad;` |
|      5 | 4703 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 4704 | `			/* Padding */` |
|      5 | 4705 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 4706 | `				break;` |
|      - | 4707 | `			}` |
|      3 | 4708 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 4709 | `		}` |
|      3 | 4710 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 4711 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 4712 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 4713 | `				if( jPad > iStrpad ){` |
|    ! 0 | 4714 | `					jPad = iStrpad;` |
|    ! 0 | 4715 | `				}` |
|      3 | 4716 | `				if( jPad < 1){` |
|    ! 0 | 4717 | `					break;` |
|      - | 4718 | `				}` |
|      3 | 4719 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 4720 | `			}` |
|      1 | 4721 | `		}` |
|      1 | 4722 | `	}` |
|     16 | 4723 | `	if( iLen > 0 ){` |
|      - | 4724 | `		/* Append the input string */` |
|     16 | 4725 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      7 | 4726 | `	}` |
|     16 | 4727 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|     56 | 4728 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 4729 | `			/* Padding */` |
|     56 | 4730 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|     14 | 4731 | `				break;` |
|      - | 4732 | `			}` |
|     44 | 4733 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|     23 | 4734 | `		}` |
|     26 | 4735 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|     14 | 4736 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|     14 | 4737 | `			if( jPad > iStrpad ){` |
|    ! 0 | 4738 | `				jPad = iStrpad;` |
|    ! 0 | 4739 | `			}` |
|     14 | 4740 | `			if( jPad < 1){` |
|    ! 0 | 4741 | `				break;` |
|      - | 4742 | `			}` |
|     14 | 4743 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 4744 | `		}` |
|      6 | 4745 | `	}` |
|     16 | 4746 | `	return PH7_OK;` |
|     12 | 4747 | `}` |
|      - | 4748 | `/*` |
|      - | 4749 | ` * String replacement private data.` |
|      - | 4750 | ` */` |
|      - | 4751 | `typedef struct str_replace_data str_replace_data;` |
|      - | 4752 | `struct str_replace_data` |
|      - | 4753 | `{` |
|      - | 4754 | `	/* Used by the str_replace family to collect the search/replace arguments. */` |
|      - | 4755 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 4756 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 4757 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 4758 | `};` |
|      - | 4759 | `/*` |
|      - | 4760 | ` * Remove a substring.` |
|      - | 4761 | ` */` |
|      - | 4762 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 4763 | `	for(;;){\` |
|      - | 4764 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 4765 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 4766 | `		++OFFT;\` |
|      - | 4767 | `	}\` |
|      - | 4768 | `}` |
|      - | 4769 | `/*` |
|      - | 4770 | ` * Shift right and insert algorithm.` |
|      - | 4771 | ` */` |
|      - | 4772 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 4773 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 4774 | `		for(;;){\` |
|      - | 4775 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 4776 | `			if(INLEN < 1 ) { break; }\` |
|      - | 4777 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 4778 | `			--INLEN; \` |
|      - | 4779 | `		}\` |
|      - | 4780 | `		for(;;){\` |
|      - | 4781 | `				if(ELEN < 1) { break; }\` |
|      - | 4782 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 4783 | `				OFFT++;\` |
|      - | 4784 | `				ENTRY++;\` |
|      - | 4785 | `				--ELEN;\` |
|      - | 4786 | `		}\` |
|      - | 4787 | `}` |
|      - | 4788 | `/*` |
|      - | 4789 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 4790 | ` * replacement string [i.e: zReplace].` |
|      - | 4791 | ` */` |
|     80 | 4792 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      5 | 4793 | `{` |
|     85 | 4794 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 4795 | `	sxu32 n,m;` |
|     85 | 4796 | `	n = SyBlobLength(pWorker);` |
|     85 | 4797 | `	m = nOfft;` |
|      - | 4798 | `	/* Delete the old entry */` |
|   7411 | 4799 | `	STRDEL(zInput,n,m,nLen);` |
|     85 | 4800 | `	SyBlobLength(pWorker) -= nLen;` |
|     85 | 4801 | `	if( nReplen > 0 ){` |
|     79 | 4802 | `		sxi32 iRep = nReplen;` |
|      - | 4803 | `		sxi32 rc;` |
|      - | 4804 | `		/*` |
|      - | 4805 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 4806 | `		 * string.` |
|      - | 4807 | `		 */` |
|     79 | 4808 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     79 | 4809 | `		if( rc != SXRET_OK ){` |
|      - | 4810 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 4811 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 4812 | `			return rc;` |
|      - | 4813 | `		}` |
|      - | 4814 | `		/* Perform the insertion now */` |
|     79 | 4815 | `		zInput = (char *)SyBlobData(pWorker);` |
|     79 | 4816 | `		n = SyBlobLength(pWorker);` |
|   7229 | 4817 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     79 | 4818 | `		SyBlobLength(pWorker) += nReplen;` |
|     37 | 4819 | `	}` |
|     85 | 4820 | `	return SXRET_OK;` |
|     45 | 4821 | `}` |
|      - | 4822 | `/*` |
|      - | 4823 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 4824 | ` * to collect search/replace string.` |
|      - | 4825 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 4826 | ` */` |
|    174 | 4827 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      5 | 4828 | `{` |
|    179 | 4829 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 4830 | `	SyString sWorker;` |
|      - | 4831 | `	const char *zIn;` |
|      - | 4832 | `	int nByte;` |
|      - | 4833 | `	/* Extract a string representation of the given argument */` |
|    179 | 4834 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|    179 | 4835 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|    179 | 4836 | `	if( nByte > 0 ){` |
|      - | 4837 | `		char *zDup;` |
|      - | 4838 | `		/* Duplicate the chunk */` |
|    177 | 4839 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 4840 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 4841 | `			);` |
|    177 | 4842 | `		if( zDup == 0 ){` |
|      - | 4843 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 4844 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 4845 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 4846 | `			return SXERR_MEM;` |
|      - | 4847 | `		}` |
|    177 | 4848 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 4849 | `		/* Save the chunk */` |
|    177 | 4850 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     86 | 4851 | `	}` |
|      - | 4852 | `	/* Save for later processing */` |
|    179 | 4853 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 4854 | `	/* All done */` |
|     87 | 4855 | `	SXUNUSED(pKey); /* cc warning */` |
|    179 | 4856 | `	return PH7_OK;` |
|     92 | 4857 | `}` |
|      - | 4858 | `/*` |
|      - | 4859 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 4860 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 4861 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 4862 | ` * Parameters` |
|      - | 4863 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 4864 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 4865 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 4866 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 4867 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 4868 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 4869 | ` * $search` |
|      - | 4870 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 4871 | ` *  to designate multiple needles.` |
|      - | 4872 | ` * $replace` |
|      - | 4873 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 4874 | ` *  to designate multiple replacements.` |
|      - | 4875 | ` * $subject` |
|      - | 4876 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 4877 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 4878 | ` *  of subject, and the return value is an array as well.` |
|      - | 4879 | ` * $count (Not used)` |
|      - | 4880 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 4881 | ` * Return` |
|      - | 4882 | ` * This function returns a string or an array with the replaced values.` |
|      - | 4883 | ` */` |
|  29992 | 4884 | `PH7_PRIVATE int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 4885 | `{` |
|      - | 4886 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 4887 | `	ProcStringMatch xMatch;` |
|      - | 4888 | `	const char *zIn,*zFunc;` |
|      - | 4889 | `	str_replace_data sRep;` |
|      - | 4890 | `	SyBlob sWorker;` |
|      - | 4891 | `	SySet sReplace;` |
|      - | 4892 | `	SySet sSearch;` |
|      - | 4893 | `	int rep_str;` |
|      - | 4894 | `	int nByte;` |
|      - | 4895 | `	sxi32 rc;` |
|  29997 | 4896 | `	if( nArg < 3 ){` |
|      - | 4897 | `		/* Missing/Invalid arguments,return null */` |
|    ! 0 | 4898 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4899 | `		return PH7_OK;` |
|      - | 4900 | `	}` |
|      - | 4901 | `	/* Initialize fields */` |
|  29997 | 4902 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  29997 | 4903 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  29997 | 4904 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  29997 | 4905 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  29997 | 4906 | `	sRep.pCtx = pCtx;` |
|  29997 | 4907 | `	sRep.pCollector = &sSearch;` |
|  29997 | 4908 | `	rep_str = 0;` |
|      - | 4909 | `	/* Extract the subject */` |
|  29997 | 4910 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  29997 | 4911 | `	if( nByte < 1 ){` |
|      - | 4912 | `		/* Nothing to replace,return the empty string */` |
|     21 | 4913 | `		ph7_result_string(pCtx,"",0);` |
|     21 | 4914 | `		return PH7_OK;` |
|      - | 4915 | `	}` |
|      - | 4916 | `	/* Copy the subject */` |
|  29977 | 4917 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 4918 | `	/* Search string */` |
|  29977 | 4919 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 4920 | `		/* Collect search string */` |
|     87 | 4921 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|     46 | 4922 | `	}else{` |
|      - | 4923 | `		/* Single pattern */` |
|  29895 | 4924 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  29895 | 4925 | `		if( nByte < 1 ){` |
|      - | 4926 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 4927 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 4928 | `			return PH7_OK;` |
|      - | 4929 | `		}` |
|  29891 | 4930 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 4931 | `		/* Save for later processing */` |
|  29891 | 4932 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 4933 | `	}` |
|      - | 4934 | `	/* Replace string */` |
|  29973 | 4935 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 4936 | `		/* Collect replace string */` |
|      7 | 4937 | `		sRep.pCollector = &sReplace;` |
|      7 | 4938 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 4939 | `	}else{` |
|      - | 4940 | `		/* Single needle */` |
|  29967 | 4941 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  29967 | 4942 | `		rep_str = 1;` |
|  29967 | 4943 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 4944 | `		/* Save for later processing */` |
|  29967 | 4945 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 4946 | `	}` |
|      - | 4947 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  29973 | 4948 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 4949 | `		SySetRelease(&sSearch);` |
|    ! 0 | 4950 | `		SySetRelease(&sReplace);` |
|    ! 0 | 4951 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 4952 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4953 | `	}` |
|      - | 4954 | `	/* Reset loop cursors */` |
|  29973 | 4955 | `	SySetResetCursor(&sSearch);` |
|  29973 | 4956 | `	SySetResetCursor(&sReplace);` |
|  29973 | 4957 | `	pReplace = pSearch = 0; /* cc warning */` |
|  29973 | 4958 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 4959 | `	/* Extract function name */` |
|  29973 | 4960 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 4961 | `	/* Set the default pattern match routine */` |
|  29973 | 4962 | `	xMatch = SyBlobSearch;` |
|  29973 | 4963 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 4964 | `		/* Case insensitive pattern match */` |
|     11 | 4965 | `		xMatch = iPatternMatch;` |
|      5 | 4966 | `	}` |
|      - | 4967 | `	/* Start the replace process */` |
|  60023 | 4968 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 4969 | `		sxu32 nCount,nOfft;` |
|  30055 | 4970 | `		if( pSearch->nByte <  1 ){` |
|      - | 4971 | `			/* Empty string,ignore */` |
|      3 | 4972 | `			continue;` |
|      - | 4973 | `		}` |
|      - | 4974 | `		/* Extract the replace string */` |
|  30053 | 4975 | `		if( rep_str ){` |
|  30043 | 4976 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  15024 | 4977 | `		}else{` |
|     11 | 4978 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 4979 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 4980 | `				 * An empty string is used for the rest of replacement values` |
|      - | 4981 | `				 */` |
|      3 | 4982 | `				pReplace = 0;` |
|      1 | 4983 | `			}` |
|      - | 4984 | `		}` |
|  30053 | 4985 | `		if( pReplace == 0 ){` |
|      - | 4986 | `			/* Use an empty string instead */` |
|      3 | 4987 | `			pReplace = &sTemp;` |
|      1 | 4988 | `		}` |
|  30053 | 4989 | `		nOfft = nCount = 0;` |
|  15064 | 4990 | `		for(;;){` |
|  30133 | 4991 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 4992 | `				break;` |
|      - | 4993 | `			}` |
|      - | 4994 | `			/* Perform a pattern lookup */` |
|  45179 | 4995 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  30116 | 4996 | `				pSearch->nByte,&nOfft);` |
|  30121 | 4997 | `			if( rc != SXRET_OK ){` |
|      - | 4998 | `				/* Pattern not found */` |
|  30041 | 4999 | `				break;` |
|      - | 5000 | `			}` |
|      - | 5001 | `			/* Perform the replace operation */` |
|     85 | 5002 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     85 | 5003 | `			if( rc != SXRET_OK ){` |
|      - | 5004 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 5005 | `				SySetRelease(&sSearch);` |
|    ! 0 | 5006 | `				SySetRelease(&sReplace);` |
|    ! 0 | 5007 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 5008 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 5009 | `			}` |
|      - | 5010 | `			/* Increment offset counter */` |
|     85 | 5011 | `			nCount += nOfft + pReplace->nByte;` |
|      5 | 5012 | `		}` |
|      5 | 5013 | `	}` |
|      - | 5014 | `	/* All done,clean-up the mess left behind */` |
|  29973 | 5015 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  29973 | 5016 | `	SySetRelease(&sSearch);` |
|  29973 | 5017 | `	SySetRelease(&sReplace);` |
|  29973 | 5018 | `	SyBlobRelease(&sWorker);` |
|  29973 | 5019 | `	if( rc != PH7_OK ){` |
|    ! 0 | 5020 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5021 | `	}` |
|  29973 | 5022 | `	return PH7_OK;` |
|  15001 | 5023 | `}` |
|      - | 5024 | `/*` |
|      - | 5025 | ` * strtr() array form: a single (key,value) pair copied out of the replace_pairs` |
|      - | 5026 | ` * array. The bytes are owned by a persistent pool (see strtr_collect) rather than` |
|      - | 5027 | ` * the transient walker values, which HashmapWalk releases after each callback, so` |
|      - | 5028 | ` * we store byte offsets into that pool instead of raw pointers.` |
|      - | 5029 | ` */` |
|      - | 5030 | `typedef struct strtr_entry strtr_entry;` |
|      - | 5031 | `struct strtr_entry` |
|      - | 5032 | `{` |
|      - | 5033 | `	sxu32 nKeyOfft; /* Offset of the search key inside the pool */` |
|      - | 5034 | `	sxu32 nKeyLen;  /* Length of the search key */` |
|      - | 5035 | `	sxu32 nValOfft; /* Offset of the replacement inside the pool */` |
|      - | 5036 | `	sxu32 nValLen;  /* Length of the replacement */` |
|      - | 5037 | `};` |
|      - | 5038 | `typedef struct strtr_collect strtr_collect;` |
|      - | 5039 | `struct strtr_collect` |
|      - | 5040 | `{` |
|      - | 5041 | `	SyBlob *pPool;  /* Byte pool holding copied key + value bytes */` |
|      - | 5042 | `	SySet  *pTable; /* Set of strtr_entry (parallel offsets into pPool) */` |
|      - | 5043 | `	sxi32   rc;     /* Carries an allocation failure (SXERR_MEM) out of the walker */` |
|      - | 5044 | `};` |
|      - | 5045 | `/*` |
|      - | 5046 | ` * Collect one replace_pairs entry into the persistent pool/offset table.` |
|      - | 5047 | ` * PHP coerces both the key and the value to string (an integer key becomes its` |
|      - | 5048 | ` * decimal form) and ignores an empty-string key.` |
|      - | 5049 | ` */` |
|     20 | 5050 | `static int StrtrCollectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 5051 | `{` |
|     21 | 5052 | `	strtr_collect *pCol = (strtr_collect *)pUserData;` |
|      - | 5053 | `	const char *zKey,*zVal;` |
|      - | 5054 | `	strtr_entry sEnt;` |
|      - | 5055 | `	int nKey,nVal;` |
|     21 | 5056 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     21 | 5057 | `	if( nKey < 1 ){` |
|      - | 5058 | `		/* PHP ignores an empty-string key (it also emits a warning we do not replicate). */` |
|      3 | 5059 | `		return PH7_OK;` |
|      - | 5060 | `	}` |
|     19 | 5061 | `	zVal = ph7_value_to_string(pData,&nVal);` |
|     19 | 5062 | `	sEnt.nKeyOfft = SyBlobLength(pCol->pPool);` |
|     19 | 5063 | `	sEnt.nKeyLen  = (sxu32)nKey;` |
|     19 | 5064 | `	if( SyBlobAppend(pCol->pPool,(const void *)zKey,(sxu32)nKey) != SXRET_OK ){` |
|    ! 0 | 5065 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 5066 | `		return SXERR_ABORT;` |
|      - | 5067 | `	}` |
|     19 | 5068 | `	sEnt.nValOfft = SyBlobLength(pCol->pPool);` |
|     19 | 5069 | `	sEnt.nValLen  = (sxu32)nVal;` |
|     19 | 5070 | `	if( nVal > 0 && SyBlobAppend(pCol->pPool,(const void *)zVal,(sxu32)nVal) != SXRET_OK ){` |
|    ! 0 | 5071 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 5072 | `		return SXERR_ABORT;` |
|      - | 5073 | `	}` |
|     19 | 5074 | `	if( SySetPut(pCol->pTable,(const void *)&sEnt) != SXRET_OK ){` |
|    ! 0 | 5075 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 5076 | `		return SXERR_ABORT;` |
|      - | 5077 | `	}` |
|     19 | 5078 | `	return PH7_OK;` |
|     11 | 5079 | `}` |
|      - | 5080 | `/*` |
|      - | 5081 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 5082 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 5083 | ` *  Translate characters or replace substrings.` |
|      - | 5084 | ` * Parameters` |
|      - | 5085 | ` *  $str` |
|      - | 5086 | ` *  The string being translated.` |
|      - | 5087 | ` * $from` |
|      - | 5088 | ` *  The string being translated to to.` |
|      - | 5089 | ` * $to` |
|      - | 5090 | ` *  The string replacing from.` |
|      - | 5091 | ` * $replace_pairs` |
|      - | 5092 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 5093 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 5094 | ` * Return` |
|      - | 5095 | ` *  The translated string.` |
|      - | 5096 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 5097 | ` */` |
|     12 | 5098 | `PH7_PRIVATE int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5099 | `{` |
|      - | 5100 | `	const char *zIn;` |
|      - | 5101 | `	int nLen;` |
|     13 | 5102 | `	if( nArg < 1 ){` |
|      - | 5103 | `		/* Nothing to replace,return FALSE */` |
|    ! 0 | 5104 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5105 | `		return PH7_OK;` |
|      - | 5106 | `	}` |
|     13 | 5107 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 5108 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 5109 | `		/* Invalid arguments */` |
|    ! 0 | 5110 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5111 | `		return PH7_OK;` |
|      - | 5112 | `	}` |
|     18 | 5113 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 5114 | `		strtr_collect sCol;` |
|      - | 5115 | `		SyBlob sPool,sWorker;` |
|      - | 5116 | `		SySet sTable;` |
|      - | 5117 | `		const char *zPool;` |
|      - | 5118 | `		strtr_entry *pEnt;` |
|      - | 5119 | `		sxi32 rc;` |
|      - | 5120 | `		int i,iRun;` |
|      - | 5121 | `		/*` |
|      - | 5122 | `		 * PHP's array-form strtr is a single left-to-right pass over the subject:` |
|      - | 5123 | `		 * at every position it substitutes the LONGEST replace_pairs key that` |
|      - | 5124 | `		 * matches there, then advances past the key (replacements are never` |
|      - | 5125 | `		 * rescanned). It is not a sequential per-key global replace. First copy` |
|      - | 5126 | `		 * the pairs into a persistent pool, then run that scan.` |
|      - | 5127 | `		 */` |
|     11 | 5128 | `		SyBlobInit(&sPool,&pCtx->pVm->sAllocator);` |
|     11 | 5129 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     11 | 5130 | `		SySetInit(&sTable,&pCtx->pVm->sAllocator,sizeof(strtr_entry));` |
|     11 | 5131 | `		sCol.pPool  = &sPool;` |
|     11 | 5132 | `		sCol.pTable = &sTable;` |
|     11 | 5133 | `		sCol.rc     = SXRET_OK;` |
|     11 | 5134 | `		ph7_array_walk(apArg[1],StrtrCollectWalker,&sCol);` |
|     11 | 5135 | `		if( sCol.rc != SXRET_OK ){` |
|      - | 5136 | `			/* Allocation failure while collecting the pairs: surface a fatal */` |
|    ! 0 | 5137 | `			SyBlobRelease(&sPool);` |
|    ! 0 | 5138 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 5139 | `			SySetRelease(&sTable);` |
|    ! 0 | 5140 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 5141 | `		}` |
|      - | 5142 | `		/* The pool is now stable, so offsets can be resolved against its base. */` |
|     11 | 5143 | `		zPool = (const char *)SyBlobData(&sPool);` |
|     11 | 5144 | `		rc = SXRET_OK;` |
|     11 | 5145 | `		iRun = 0; /* Start of the pending run of unmatched bytes copied verbatim. */` |
|     43 | 5146 | `		for( i = 0 ; i < nLen ; ){` |
|     33 | 5147 | `			strtr_entry *pBest = 0;` |
|     33 | 5148 | `			sxu32 nBest = 0;` |
|      - | 5149 | `			/* Pick the longest key that matches at the current position. */` |
|     33 | 5150 | `			SySetResetCursor(&sTable);` |
|     87 | 5151 | `			while( SXRET_OK == SySetGetNextEntry(&sTable,(void **)&pEnt) ){` |
|     54 | 5152 | `				if( pEnt->nKeyLen > nBest` |
|     50 | 5153 | `					&& pEnt->nKeyLen <= (sxu32)(nLen - i)` |
|     46 | 5154 | `					&& SyMemcmp(zPool + pEnt->nKeyOfft,zIn + i,pEnt->nKeyLen) == 0 ){` |
|     29 | 5155 | `					nBest = pEnt->nKeyLen;` |
|     29 | 5156 | `					pBest = pEnt;` |
|     14 | 5157 | `				}` |
|      1 | 5158 | `			}` |
|     33 | 5159 | `			if( pBest == 0 ){` |
|      - | 5160 | `				/* No key here: extend the literal run and copy it in one shot later. */` |
|      9 | 5161 | `				i++;` |
|      9 | 5162 | `				continue;` |
|      - | 5163 | `			}` |
|      - | 5164 | `			/* Flush the pending literal run, then the replacement. */` |
|     25 | 5165 | `			if( i > iRun ){` |
|      5 | 5166 | `				rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(i - iRun));` |
|      2 | 5167 | `			}` |
|     25 | 5168 | `			if( rc == SXRET_OK && pBest->nValLen > 0 ){` |
|     25 | 5169 | `				rc = SyBlobAppend(&sWorker,zPool + pBest->nValOfft,pBest->nValLen);` |
|     12 | 5170 | `			}` |
|     25 | 5171 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 5172 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 5173 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 5174 | `				SySetRelease(&sTable);` |
|    ! 0 | 5175 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 5176 | `			}` |
|     25 | 5177 | `			i += (int)pBest->nKeyLen;` |
|     25 | 5178 | `			iRun = i;` |
|      1 | 5179 | `		}` |
|      - | 5180 | `		/* Flush the trailing literal run. */` |
|     11 | 5181 | `		if( nLen > iRun ){` |
|      3 | 5182 | `			rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(nLen - iRun));` |
|      3 | 5183 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 5184 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 5185 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 5186 | `				SySetRelease(&sTable);` |
|    ! 0 | 5187 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 5188 | `			}` |
|      1 | 5189 | `		}` |
|      - | 5190 | `		/* All done, return the result string */` |
|     16 | 5191 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|     10 | 5192 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 5193 | `		/* Clean-up */` |
|     11 | 5194 | `		SyBlobRelease(&sPool);` |
|     11 | 5195 | `		SyBlobRelease(&sWorker);` |
|     11 | 5196 | `		SySetRelease(&sTable);` |
|     11 | 5197 | `		if( rc != PH7_OK ){` |
|    ! 0 | 5198 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 5199 | `		}` |
|      6 | 5200 | `	}else{` |
|      - | 5201 | `		int i,flen,tlen,c,iOfft;` |
|      - | 5202 | `		const char *zFrom,*zTo;` |
|      3 | 5203 | `		if( nArg < 3 ){` |
|      - | 5204 | `			/* Nothing to replace */` |
|    ! 0 | 5205 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5206 | `			return PH7_OK;` |
|      - | 5207 | `		}` |
|      - | 5208 | `		/* Extract given arguments */` |
|      3 | 5209 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 5210 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 5211 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 5212 | `			/* Nothing to replace */` |
|    ! 0 | 5213 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5214 | `			return PH7_OK;` |
|      - | 5215 | `		}` |
|      - | 5216 | `		/* Start the replace process */` |
|     13 | 5217 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 5218 | `			c = zIn[i];` |
|     11 | 5219 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 5220 | `				if ( iOfft < tlen ){` |
|      5 | 5221 | `					c = zTo[iOfft];` |
|      2 | 5222 | `				}` |
|      2 | 5223 | `			}` |
|     11 | 5224 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 5225 |  |
|      6 | 5226 | `		}` |
|      - | 5227 | `	}` |
|     13 | 5228 | `	return PH7_OK;` |
|      7 | 5229 | `}` |
|      - | 5230 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 5231 |  |
