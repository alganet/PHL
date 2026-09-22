# src/ph7/builtin_string.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2379/2743 lines (86.73%)

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
| 345970 |   60 | `PH7_PRIVATE int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |   61 | `{` |
|      - |   62 | `	const char *zSource;` |
|      - |   63 | `	int nSrcLen;` |
|      - |   64 | `	sxi64 iStart,iEnd;` |
| 345975 |   65 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"substr",1,"$string"); }` |
| 345975 |   66 | `	if( nArg < 2 ){` |
|      - |   67 | `		/* Arity is enforced at the call boundary; nothing sensible to return here. */` |
|    ! 0 |   68 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 |   69 | `		return PH7_OK;` |
|      - |   70 | `	}` |
|      - |   71 | `	/* Extract the target string */` |
| 345975 |   72 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|      - |   73 | `	/* Extract the offset */` |
|      - |   74 | `	{` |
| 345975 |   75 | `		sxi64 iTmp = 0;` |
| 345975 |   76 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"substr",2,"$offset","int",&iTmp);` |
| 345975 |   77 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |   78 | `			return rcArg;` |
|      - |   79 | `		}` |
| 345975 |   80 | `		iStart = iTmp;` |
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
| 345975 |   92 | `	if( iStart < 0 ){` |
|  36151 |   93 | `		iStart += nSrcLen;` |
|  36151 |   94 | `		if( iStart < 0 ){` |
|      5 |   95 | `			iStart = 0;` |
|      7 |   96 | `		}` |
| 327902 |   97 | `	}else if( iStart > nSrcLen ){` |
|      7 |   98 | `		iStart = nSrcLen;` |
|      3 |   99 | `	}` |
| 345975 |  100 | `	iEnd = nSrcLen;` |
| 345975 |  101 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
| 238401 |  102 | `		sxi64 iLen = 0;` |
| 238401 |  103 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],"substr",3,"$length","?int",&iLen);` |
| 238401 |  104 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  105 | `			return rcArg;` |
|      - |  106 | `		}` |
| 238401 |  107 | `		if( iLen < 0 ){` |
|  35759 |  108 | `			iEnd = (sxi64)nSrcLen + iLen;` |
| 220524 |  109 | `		}else if( iLen > (sxi64)nSrcLen - iStart ){` |
|  22149 |  110 | `			iEnd = nSrcLen;` |
|  11077 |  111 | `		}else{` |
| 180503 |  112 | `			iEnd = iStart + iLen;` |
|      - |  113 | `		}` |
| 119198 |  114 | `	}` |
| 345975 |  115 | `	if( iEnd < iStart ){` |
|      3 |  116 | `		iEnd = iStart;` |
|      1 |  117 | `	}` |
| 345975 |  118 | `	ph7_result_string(pCtx,&zSource[iStart],(int)(iEnd - iStart));` |
| 345975 |  119 | `	return PH7_OK;` |
| 173188 |  120 | `}` |
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
|     22 |  142 | `PH7_PRIVATE int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  143 | `{` |
|      - |  144 | `	const char *zSource,*zSub;` |
|      - |  145 | `	int nSrcLen,nSubLen;` |
|      - |  146 | `	sxi64 iOfft,iLen,l1,l2,nCmp;` |
|     23 |  147 | `	int iCase = 0;` |
|      - |  148 | `	int rc;` |
|     23 |  149 | `	if( nArg < 3 ){` |
|    ! 0 |  150 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  151 | `		return PH7_OK;` |
|      - |  152 | `	}` |
|     23 |  153 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|     23 |  154 | `	zSub    = ph7_value_to_string(apArg[1],&nSubLen);` |
|      - |  155 | `	{` |
|     23 |  156 | `		sxi64 iTmp = 0;` |
|     23 |  157 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],"substr_compare",3,"$offset","int",&iTmp);` |
|     23 |  158 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  159 | `			return rcArg;` |
|      - |  160 | `		}` |
|     23 |  161 | `		iOfft = iTmp;` |
|      - |  162 | `	}` |
|     23 |  163 | `	if( iOfft < 0 ){` |
|      5 |  164 | `		iOfft += nSrcLen;` |
|      5 |  165 | `		if( iOfft < 0 ){` |
|      3 |  166 | `			iOfft = 0;` |
|      1 |  167 | `		}` |
|      2 |  168 | `	}` |
|     23 |  169 | `	if( iOfft > nSrcLen ){` |
|      - |  170 | `		/* php rejects an offset past the end of the haystack outright */` |
|    ! 0 |  171 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  172 | `			"substr_compare(): Argument #3 ($offset) must be contained in argument #1 ($haystack)");` |
|      - |  173 | `	}` |
|      - |  174 | `	/* A NULL/absent length compares as far as the longer of the two operands reaches */` |
|     23 |  175 | `	iLen = (sxi64)nSrcLen - iOfft;` |
|     23 |  176 | `	if( iLen < nSubLen ){` |
|      7 |  177 | `		iLen = nSubLen;` |
|      3 |  178 | `	}` |
|     23 |  179 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|     13 |  180 | `		sxi64 iTmp = 0;` |
|     13 |  181 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[3],"substr_compare",4,"$length","?int",&iTmp);` |
|     13 |  182 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  183 | `			return rcArg;` |
|      - |  184 | `		}` |
|     13 |  185 | `		if( iTmp < 0 ){` |
|      3 |  186 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  187 | `				"substr_compare(): Argument #4 ($length) must be greater than or equal to 0");` |
|      - |  188 | `		}` |
|     11 |  189 | `		iLen = iTmp;` |
|      5 |  190 | `	}` |
|     21 |  191 | `	if( nArg > 4 ){` |
|      5 |  192 | `		iCase = ph7_value_to_bool(apArg[4]);` |
|      2 |  193 | `	}` |
|      - |  194 | `	/* Each side contributes at most what it actually has left */` |
|     21 |  195 | `	l1 = (sxi64)nSrcLen - iOfft;` |
|     21 |  196 | `	if( l1 > iLen ){ l1 = iLen; }` |
|     21 |  197 | `	l2 = nSubLen;` |
|     21 |  198 | `	if( l2 > iLen ){ l2 = iLen; }` |
|     21 |  199 | `	nCmp = (l1 < l2) ? l1 : l2;` |
|     21 |  200 | `	if( iCase ){` |
|      3 |  201 | `		rc = SyStrnicmp(&zSource[iOfft],zSub,(sxu32)nCmp);` |
|      2 |  202 | `	}else{` |
|     19 |  203 | `		rc = SyStrncmp(&zSource[iOfft],zSub,(sxu32)nCmp);` |
|      - |  204 | `	}` |
|     21 |  205 | `	if( rc == 0 ){` |
|      - |  206 | `		/* Prefixes equal: php falls back to a THREE-WAY compare of the lengths, so this` |
|      - |  207 | `		 * arm is normalized to -1/0/1 (substr_compare("abc","",0) is 1, not 3). */` |
|     15 |  208 | `		rc = (l1 == l2) ? 0 : (l1 < l2 ? -1 : 1);` |
|      7 |  209 | `	}` |
|      - |  210 | `	/* ...but when the prefixes differ php returns the RAW byte difference, not its sign:` |
|      - |  211 | `	 * substr_compare("abc","def",1,10) is -2 ('b' - 'd'), which is what SyMemcmp gives. */` |
|     21 |  212 | `	ph7_result_int(pCtx,rc);` |
|     21 |  213 | `	return PH7_OK;` |
|     12 |  214 | `}` |
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
|    100 |  231 | `PH7_PRIVATE int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  232 | `{` |
|      - |  233 | `	const char *zText,*zPattern,*zEnd;` |
|      - |  234 | `	int nTextlen,nPatlen;` |
|    102 |  235 | `	int iCount = 0;` |
|      - |  236 | `	sxu32 nOfft;` |
|      - |  237 | `	sxi32 rc;` |
|    102 |  238 | `	if( nArg < 2 ){` |
|      - |  239 | `		/* Missing arguments */` |
|    ! 0 |  240 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  241 | `		return PH7_OK;` |
|      - |  242 | `	}` |
|      - |  243 | `	/* Point to the haystack */` |
|    102 |  244 | `	zText = ph7_value_to_string(apArg[0],&nTextlen);` |
|      - |  245 | `	/* Point to the neddle */` |
|    102 |  246 | `	zPattern = ph7_value_to_string(apArg[1],&nPatlen);` |
|    102 |  247 | `	if( nPatlen < 1 ){` |
|      - |  248 | `		/* Empty needle: PHP 8 throws a catchable ValueError. */` |
|      3 |  249 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  250 | `			"substr_count(): Argument #2 ($needle) must not be empty");` |
|      - |  251 | `	}` |
|      - |  252 | `	/* Apply the optional $offset/$length window before searching. PHP 8 validates` |
|      - |  253 | `	 * both against the haystack (a negative value counts from the end) and throws a` |
|      - |  254 | `	 * catchable ValueError when the result falls outside it — this happens before the` |
|      - |  255 | `	 * needle-fits check, so it fires even when the needle is longer than the haystack. */` |
|    100 |  256 | `	if( nArg > 2 ){` |
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
|     98 |  269 | `	if( nArg > 3 ){` |
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
|     94 |  281 | `	if( nTextlen < 1 \|\| nPatlen > nTextlen ){` |
|      - |  282 | `		/* The windowed haystack can't contain the needle: zero matches */` |
|      3 |  283 | `		ph7_result_int(pCtx,0);` |
|      3 |  284 | `		return PH7_OK;` |
|      - |  285 | `	}` |
|      - |  286 | `	/* Point to the end of the windowed haystack */` |
|     92 |  287 | `	zEnd = &zText[nTextlen];` |
|      - |  288 | `	/* Perform the search */` |
|     82 |  289 | `	for(;;){` |
|    166 |  290 | `		rc = SyBlobSearch((const void *)zText,(sxu32)(zEnd-zText),(const void *)zPattern,nPatlen,&nOfft);` |
|    166 |  291 | `		if( rc != SXRET_OK ){` |
|      - |  292 | `			/* Pattern not found,break immediately */` |
|     67 |  293 | `			break;` |
|      - |  294 | `		}` |
|      - |  295 | `		/* Increment counter and update the offset */` |
|    100 |  296 | `		iCount++;` |
|    100 |  297 | `		zText += nOfft + nPatlen;` |
|    100 |  298 | `		if( zText >= zEnd ){` |
|     26 |  299 | `			break;` |
|      - |  300 | `		}` |
|      2 |  301 | `	}` |
|      - |  302 | `	/* Pattern count */` |
|     92 |  303 | `	ph7_result_int(pCtx,iCount);` |
|     92 |  304 | `	return PH7_OK;` |
|     52 |  305 | `}` |
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
|      - |  316 | ` * non-deprecated surface and rejects it with a TypeError. Every caller of this helper` |
|      - |  317 | `` * now also carries a `string $…` row in the vm_arg_check.c signature table, so`` |
|      - |  318 | ` * VmEnforceBuiltinArgTypes raises that TypeError BEFORE the routine runs and this is a` |
|      - |  319 | ` * backstop rather than the live path. It stays correct either way: the throw records` |
|      - |  320 | ` * its status on the call context and the OP_CALL boundary (VmHostFuncThrowRc) reports` |
|      - |  321 | ` * it in place of the routine's own, so the call ABORTS as php's would without the` |
|      - |  322 | ` * callers threading a status back. */` |
| 552880 |  323 | `static void StrNullArgNotice(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNum,const char *zParamName)` |
|      5 |  324 | `{` |
| 552885 |  325 | `	if( ph7_value_is_null(pArg) ){` |
|    ! 0 |  326 | `		PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  327 | `			"%s(): Argument #%d (%s) must be of type string, null given",` |
|    ! 0 |  328 | `			zFunc,iArgNum,zParamName);` |
|    ! 0 |  329 | `	}` |
| 552885 |  330 | `}` |
|      - |  331 | `static sxi32 StrPredicateResolveArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,` |
|      - |  332 | `	int iArgNum,const char *zParamName,const char *zTypeStr,const char *zNullMsg,` |
|      - |  333 | `	ph7_value *pTmp,const char **pzOut,int *pnOut);` |
|      - |  334 | `/*` |
|      - |  335 | ` * Validate and resolve an int-typed builtin parameter with php-8 ZPP weak-mode` |
|      - |  336 | ` * semantics: ints and bools pass through; null emits the 8.1 deprecation and` |
|      - |  337 | ` * resolves to 0; floats and float-strings convert, with the implicit-conversion` |
|      - |  338 | ` * E_DEPRECATED when lossy and a TypeError when NAN/INF/out of int range;` |
|      - |  339 | ` * integral numeric strings convert exactly; everything else (arrays, resources,` |
|      - |  340 | ` * objects, non-numeric strings) is a TypeError naming zTypeStr (e.g. "int",` |
|      - |  341 | ` * "array\|int"). Returns PH7_OK with *pOut set, or the throw status.` |
|      - |  342 | ` */` |
|      - |  343 | `/*` |
|      - |  344 | ` * Normalize a substr_replace() offset/length pair against a string of nStrLen` |
|      - |  345 | ` * bytes, exactly like PHP: a negative offset counts from the end (clamped to 0),` |
|      - |  346 | ` * an offset past the end clamps to the end; a negative length leaves that many` |
|      - |  347 | ` * bytes off the end of the remaining region (clamped to 0), and the length is` |
|      - |  348 | ` * finally clamped to the remaining region. Written without f+l additions so an` |
|      - |  349 | ` * INT64_MAX length cannot overflow.` |
|      - |  350 | ` */` |
|     60 |  351 | `static void SubstrReplaceWindow(sxi64 *pF,sxi64 *pL,int nStrLen)` |
|      1 |  352 | `{` |
|     61 |  353 | `	sxi64 f = *pF,l = *pL;` |
|     61 |  354 | `	if( f < 0 ){` |
|      9 |  355 | `		f += nStrLen;` |
|      9 |  356 | `		if( f < 0 ){` |
|      5 |  357 | `			f = 0;` |
|      3 |  358 | `		}` |
|     57 |  359 | `	}else if( f > nStrLen ){` |
|      5 |  360 | `		f = nStrLen;` |
|      2 |  361 | `	}` |
|     61 |  362 | `	if( l < 0 ){` |
|      7 |  363 | `		l += nStrLen - f;` |
|      7 |  364 | `		if( l < 0 ){` |
|      5 |  365 | `			l = 0;` |
|      2 |  366 | `		}` |
|      3 |  367 | `	}` |
|     61 |  368 | `	if( l > nStrLen - f ){` |
|     25 |  369 | `		l = nStrLen - f;` |
|     12 |  370 | `	}` |
|     61 |  371 | `	*pF = f;` |
|     61 |  372 | `	*pL = l;` |
|     61 |  373 | `}` |
|      - |  374 | `/* A replacement string collected out of substr_replace()'s $replace array.` |
|      - |  375 | ` * The bytes live in a shared pool blob (walker values are transient), so the` |
|      - |  376 | ` * item stores pool offsets, mirroring the strtr_entry technique. */` |
|      - |  377 | `typedef struct substr_repl_item substr_repl_item;` |
|      - |  378 | `struct substr_repl_item` |
|      - |  379 | `{` |
|      - |  380 | `	sxu32 nOfft; /* Offset of the string inside the pool */` |
|      - |  381 | `	sxu32 nLen;  /* Length of the string */` |
|      - |  382 | `};` |
|      - |  383 | `typedef struct substr_replace_collect substr_replace_collect;` |
|      - |  384 | `struct substr_replace_collect` |
|      - |  385 | `{` |
|      - |  386 | `	SyBlob *pPool;  /* Byte pool for string items (string walker only) */` |
|      - |  387 | `	SySet *pSet;    /* substr_repl_item set (string) or sxi64 set (int) */` |
|      - |  388 | `	sxi32 rc;       /* SXRET_OK or SXERR_MEM on collector failure */` |
|      - |  389 | `};` |
|      - |  390 | `/* ph7_array_walk() callback: append one $replace element to the pool. */` |
|      6 |  391 | `static int SubstrReplaceStrWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  392 | `{` |
|      7 |  393 | `	substr_replace_collect *pCol = (substr_replace_collect *)pUserData;` |
|      - |  394 | `	substr_repl_item sItem;` |
|      - |  395 | `	const char *zStr;` |
|      - |  396 | `	int nLen;` |
|      3 |  397 | `	SXUNUSED(pKey);` |
|      7 |  398 | `	zStr = ph7_value_to_string(pData,&nLen);` |
|      7 |  399 | `	sItem.nOfft = SyBlobLength(pCol->pPool);` |
|      7 |  400 | `	sItem.nLen = (sxu32)nLen;` |
|      7 |  401 | `	if( nLen > 0 && SXRET_OK != SyBlobAppend(pCol->pPool,(const void *)zStr,(sxu32)nLen) ){` |
|    ! 0 |  402 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  403 | `		return SXERR_ABORT;` |
|      - |  404 | `	}` |
|      7 |  405 | `	if( SXRET_OK != SySetPut(pCol->pSet,(const void *)&sItem) ){` |
|    ! 0 |  406 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  407 | `		return SXERR_ABORT;` |
|      - |  408 | `	}` |
|      7 |  409 | `	return PH7_OK;` |
|      4 |  410 | `}` |
|      - |  411 | `/* ph7_array_walk() callback: collect one $offset/$length element as an int. */` |
|     12 |  412 | `static int SubstrReplaceIntWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  413 | `{` |
|     13 |  414 | `	substr_replace_collect *pCol = (substr_replace_collect *)pUserData;` |
|     13 |  415 | `	sxi64 iVal = ph7_value_to_int64(pData);` |
|      6 |  416 | `	SXUNUSED(pKey);` |
|     13 |  417 | `	if( SXRET_OK != SySetPut(pCol->pSet,(const void *)&iVal) ){` |
|    ! 0 |  418 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  419 | `		return SXERR_ABORT;` |
|      - |  420 | `	}` |
|     13 |  421 | `	return PH7_OK;` |
|      7 |  422 | `}` |
|      - |  423 | `/* Per-element state while walking substr_replace()'s array $string. */` |
|      - |  424 | `typedef struct substr_replace_ctx substr_replace_ctx;` |
|      - |  425 | `struct substr_replace_ctx` |
|      - |  426 | `{` |
|      - |  427 | `	ph7_value *pResult;   /* Result array (keys preserved) */` |
|      - |  428 | `	ph7_value *pScratch;  /* Reusable string value for each element */` |
|      - |  429 | `	SyBlob *pReplPool;    /* Pool behind aRepl items */` |
|      - |  430 | `	SySet *pRepl;         /* substr_repl_item set or NULL when $replace is scalar */` |
|      - |  431 | `	SySet *pFrom;         /* sxi64 set or NULL when $offset is scalar */` |
|      - |  432 | `	SySet *pLen;          /* sxi64 set or NULL when $length is scalar/absent */` |
|      - |  433 | `	sxu32 iReplCur;       /* Next-position cursors into the three sets */` |
|      - |  434 | `	sxu32 iFromCur;` |
|      - |  435 | `	sxu32 iLenCur;` |
|      - |  436 | `	const char *zRepl;    /* Scalar $replace */` |
|      - |  437 | `	int nRepl;` |
|      - |  438 | `	sxi64 iFrom;          /* Scalar $offset */` |
|      - |  439 | `	sxi64 iLen;           /* Scalar $length */` |
|      - |  440 | `	int bLenGiven;        /* FALSE: $length absent/null -> element length */` |
|      - |  441 | `	sxi32 rc;             /* SXRET_OK or SXERR_MEM */` |
|      - |  442 | `};` |
|      - |  443 | `/*` |
|      - |  444 | ` * ph7_array_walk() callback over the array $string: replace the window of one` |
|      - |  445 | ` * element and insert the result under the element's original key. Array-form` |
|      - |  446 | ` * $replace/$offset/$length are consumed positionally; when a set runs out PHP` |
|      - |  447 | ` * falls back to ""/0/element-length respectively.` |
|      - |  448 | ` */` |
|     24 |  449 | `static int SubstrReplaceElemWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  450 | `{` |
|     25 |  451 | `	substr_replace_ctx *pRep = (substr_replace_ctx *)pUserData;` |
|      - |  452 | `	const char *zStr,*zRepl;` |
|      - |  453 | `	sxi64 f,l;` |
|      - |  454 | `	int nLen,nRepl;` |
|     25 |  455 | `	zStr = ph7_value_to_string(pData,&nLen);` |
|      - |  456 | `	/* Positional $replace element ("" when exhausted) */` |
|     25 |  457 | `	if( pRep->pRepl ){` |
|     11 |  458 | `		if( pRep->iReplCur < SySetUsed(pRep->pRepl) ){` |
|      7 |  459 | `			substr_repl_item *pItem = (substr_repl_item *)SySetAt(pRep->pRepl,pRep->iReplCur++);` |
|      7 |  460 | `			zRepl = (const char *)SyBlobDataAt(pRep->pReplPool,pItem->nOfft);` |
|      7 |  461 | `			nRepl = (int)pItem->nLen;` |
|      4 |  462 | `		}else{` |
|      5 |  463 | `			zRepl = "";` |
|      5 |  464 | `			nRepl = 0;` |
|      - |  465 | `		}` |
|      6 |  466 | `	}else{` |
|     15 |  467 | `		zRepl = pRep->zRepl;` |
|     15 |  468 | `		nRepl = pRep->nRepl;` |
|      - |  469 | `	}` |
|      - |  470 | `	/* Positional $offset element (0 when exhausted) */` |
|     25 |  471 | `	if( pRep->pFrom ){` |
|     13 |  472 | `		sxi64 *pVal = 0;` |
|     13 |  473 | `		if( pRep->iFromCur < SySetUsed(pRep->pFrom) ){` |
|      9 |  474 | `			pVal = (sxi64 *)SySetAt(pRep->pFrom,pRep->iFromCur++);` |
|      4 |  475 | `		}` |
|     13 |  476 | `		f = pVal ? *pVal : 0;` |
|      7 |  477 | `	}else{` |
|     13 |  478 | `		f = pRep->iFrom;` |
|      - |  479 | `	}` |
|      - |  480 | `	/* Positional $length element (element length when exhausted) */` |
|     25 |  481 | `	if( pRep->pLen ){` |
|      7 |  482 | `		sxi64 *pVal = 0;` |
|      7 |  483 | `		if( pRep->iLenCur < SySetUsed(pRep->pLen) ){` |
|      5 |  484 | `			pVal = (sxi64 *)SySetAt(pRep->pLen,pRep->iLenCur++);` |
|      2 |  485 | `		}` |
|      7 |  486 | `		l = pVal ? *pVal : nLen;` |
|      4 |  487 | `	}else{` |
|     19 |  488 | `		l = pRep->bLenGiven ? pRep->iLen : nLen;` |
|      - |  489 | `	}` |
|     25 |  490 | `	SubstrReplaceWindow(&f,&l,nLen);` |
|      - |  491 | `	/* Assemble prefix + replacement + suffix in the scratch value */` |
|     25 |  492 | `	ph7_value_reset_string_cursor(pRep->pScratch);` |
|     24 |  493 | `	if( (f > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,zStr,(int)f))` |
|     24 |  494 | `	 \|\| (nRepl > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,zRepl,nRepl))` |
|     40 |  495 | `	 \|\| (nLen - (int)(f+l) > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,&zStr[f+l],nLen - (int)(f+l))) ){` |
|     30 |  496 | `		pRep->rc = SXERR_MEM;` |
|     30 |  497 | `		return SXERR_ABORT;` |
|      - |  498 | `	}` |
|     25 |  499 | `	if( SXRET_OK != ph7_array_add_elem(pRep->pResult,pKey,pRep->pScratch) ){` |
|    ! 0 |  500 | `		pRep->rc = SXERR_MEM;` |
|    ! 0 |  501 | `		return SXERR_ABORT;` |
|      - |  502 | `	}` |
|     25 |  503 | `	return PH7_OK;` |
|     43 |  504 | `}` |
|      - |  505 | `/*` |
|      - |  506 | ` * mixed substr_replace(array\|string $string,array\|string $replace,array\|int $offset[,array\|int\|null $length = null])` |
|      - |  507 | ` *  Replace text within a portion of a string.` |
|      - |  508 | ` * Parameters` |
|      - |  509 | ` *  $string` |
|      - |  510 | ` *   The input string or an array of strings (each element is processed with` |
|      - |  511 | ` *   its own positional replace/offset/length when those are arrays too).` |
|      - |  512 | ` *  $replace` |
|      - |  513 | ` *   The replacement string. When $string is scalar and $replace is an array,` |
|      - |  514 | ` *   only its first element is used (PHP quirk).` |
|      - |  515 | ` *  $offset` |
|      - |  516 | ` *   Window start; negative counts from the end of the string.` |
|      - |  517 | ` *  $length` |
|      - |  518 | ` *   Window length; negative leaves that many bytes at the end; null/absent` |
|      - |  519 | ` *   means "to the end of the string".` |
|      - |  520 | ` * Return` |
|      - |  521 | ` *  The processed string, or an array of processed strings (keys preserved).` |
|      - |  522 | ` * Errors` |
|      - |  523 | ` *  ArgumentCountError on fewer than 3 arguments; TypeError when an array` |
|      - |  524 | ` *  $offset/$length is combined with a scalar $string.` |
|      - |  525 | ` */` |
|     58 |  526 | `PH7_PRIVATE int PH7_builtin_substr_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  527 | `{` |
|      - |  528 | `	ph7_value sStrTmp,sReplTmp;` |
|     59 |  529 | `	const char *zStr = 0,*zRepl = 0;` |
|     59 |  530 | `	int nLen = 0,nRepl = 0;` |
|      - |  531 | `	int bLenGiven;` |
|     59 |  532 | `	sxi64 f = 0,l = 0;` |
|      - |  533 | `	sxi32 rc;` |
|     59 |  534 | `	if( nArg < 3 ){` |
|    ! 0 |  535 | `		return PH7_VmThrowException(pCtx,` |
|      - |  536 | `			"ArgumentCountError",` |
|      - |  537 | `			"substr_replace() expects at least 3 arguments, %d given",` |
|    ! 0 |  538 | `			nArg` |
|      - |  539 | `			);` |
|      - |  540 | `	}` |
|      - |  541 | `	/* $length counts as given unless absent or null (php: ?null semantics) */` |
|     59 |  542 | `	bLenGiven = (nArg > 3 && !ph7_value_is_null(apArg[3]));` |
|      - |  543 | `	/* php ZPP validates all four args, in order, before the body runs: the` |
|      - |  544 | `	 * non-array forms resolve here (null deprecation, __toString objects,` |
|      - |  545 | `	 * numeric strings), arrays pass through to the per-mode handling. */` |
|     59 |  546 | `	PH7_MemObjInit(pCtx->pVm,&sStrTmp);` |
|     59 |  547 | `	PH7_MemObjInit(pCtx->pVm,&sReplTmp);` |
|     59 |  548 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     45 |  549 | `		rc = StrPredicateResolveArg(pCtx,apArg[0],"substr_replace",1,"$string","array\|string",` |
|      - |  550 | `			"substr_replace(): Passing null to parameter #1 ($string) "` |
|      - |  551 | `			"of type array\|string is deprecated",` |
|      - |  552 | `			&sStrTmp,&zStr,&nLen);` |
|     45 |  553 | `		if( rc != PH7_OK ) goto out;` |
|     22 |  554 | `	}` |
|     59 |  555 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|     51 |  556 | `		rc = StrPredicateResolveArg(pCtx,apArg[1],"substr_replace",2,"$replace","array\|string",` |
|      - |  557 | `			"substr_replace(): Passing null to parameter #2 ($replace) "` |
|      - |  558 | `			"of type array\|string is deprecated",` |
|      - |  559 | `			&sReplTmp,&zRepl,&nRepl);` |
|     51 |  560 | `		if( rc != PH7_OK ) goto out;` |
|     25 |  561 | `	}` |
|     59 |  562 | `	if( !ph7_value_is_array(apArg[2]) ){` |
|     51 |  563 | `		rc = PH7_IntArgResolve(pCtx,apArg[2],"substr_replace",3,"$offset","array\|int",&f);` |
|     51 |  564 | `		if( rc != PH7_OK ) goto out;` |
|     24 |  565 | `	}` |
|     57 |  566 | `	if( bLenGiven && !ph7_value_is_array(apArg[3]) ){` |
|     31 |  567 | `		rc = PH7_IntArgResolve(pCtx,apArg[3],"substr_replace",4,"$length","array\|int\|null",&l);` |
|     31 |  568 | `		if( rc != PH7_OK ) goto out;` |
|     14 |  569 | `	}` |
|     55 |  570 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - |  571 | `		/* Array form: process each element, preserving keys */` |
|      - |  572 | `		substr_replace_ctx sRep;` |
|      - |  573 | `		substr_replace_collect sCol;` |
|      - |  574 | `		SyBlob sReplPool;` |
|      - |  575 | `		SySet sRepl,sFrom,sLen;` |
|      - |  576 | `		ph7_value *pResult,*pScratch;` |
|     15 |  577 | `		sxi32 rcWalk = SXRET_OK;` |
|     15 |  578 | `		SyBlobInit(&sReplPool,&pCtx->pVm->sAllocator);` |
|     15 |  579 | `		SySetInit(&sRepl,&pCtx->pVm->sAllocator,sizeof(substr_repl_item));` |
|     15 |  580 | `		SySetInit(&sFrom,&pCtx->pVm->sAllocator,sizeof(sxi64));` |
|     15 |  581 | `		SySetInit(&sLen,&pCtx->pVm->sAllocator,sizeof(sxi64));` |
|     15 |  582 | `		SyZero(&sRep,sizeof(substr_replace_ctx));` |
|     15 |  583 | `		sRep.bLenGiven = bLenGiven;` |
|     15 |  584 | `		sCol.rc = SXRET_OK;` |
|      - |  585 | `		/* Collect array-form $replace/$offset/$length positionally; the` |
|      - |  586 | `		 * scalar forms were already resolved above. */` |
|     15 |  587 | `		if( ph7_value_is_array(apArg[1]) ){` |
|      5 |  588 | `			sCol.pPool = &sReplPool;` |
|      5 |  589 | `			sCol.pSet = &sRepl;` |
|      5 |  590 | `			ph7_array_walk(apArg[1],SubstrReplaceStrWalker,&sCol);` |
|      5 |  591 | `			sRep.pRepl = &sRepl;` |
|      5 |  592 | `			sRep.pReplPool = &sReplPool;` |
|      3 |  593 | `		}else{` |
|     11 |  594 | `			sRep.zRepl = zRepl;` |
|     11 |  595 | `			sRep.nRepl = nRepl;` |
|      - |  596 | `		}` |
|     15 |  597 | `		if( sCol.rc == SXRET_OK && ph7_value_is_array(apArg[2]) ){` |
|      7 |  598 | `			sCol.pSet = &sFrom;` |
|      7 |  599 | `			ph7_array_walk(apArg[2],SubstrReplaceIntWalker,&sCol);` |
|      7 |  600 | `			sRep.pFrom = &sFrom;` |
|      4 |  601 | `		}else{` |
|      9 |  602 | `			sRep.iFrom = f;` |
|      - |  603 | `		}` |
|     15 |  604 | `		if( sCol.rc == SXRET_OK && bLenGiven ){` |
|      9 |  605 | `			if( ph7_value_is_array(apArg[3]) ){` |
|      5 |  606 | `				sCol.pSet = &sLen;` |
|      5 |  607 | `				ph7_array_walk(apArg[3],SubstrReplaceIntWalker,&sCol);` |
|      5 |  608 | `				sRep.pLen = &sLen;` |
|      3 |  609 | `			}else{` |
|      5 |  610 | `				sRep.iLen = l;` |
|      - |  611 | `			}` |
|      4 |  612 | `		}` |
|     15 |  613 | `		pResult = ph7_context_new_array(pCtx);` |
|     15 |  614 | `		pScratch = ph7_context_new_scalar(pCtx);` |
|     15 |  615 | `		if( sCol.rc != SXRET_OK \|\| pResult == 0 \|\| pScratch == 0 ){` |
|    ! 0 |  616 | `			rcWalk = SXERR_MEM;` |
|    ! 0 |  617 | `		}else{` |
|     15 |  618 | `			sRep.pResult = pResult;` |
|     15 |  619 | `			sRep.pScratch = pScratch;` |
|     15 |  620 | `			ph7_value_string(pScratch,"",0); /* Force string representation */` |
|     15 |  621 | `			ph7_array_walk(apArg[0],SubstrReplaceElemWalker,&sRep);` |
|     15 |  622 | `			rcWalk = sRep.rc;` |
|      - |  623 | `		}` |
|     15 |  624 | `		SyBlobRelease(&sReplPool);` |
|     15 |  625 | `		SySetRelease(&sRepl);` |
|     15 |  626 | `		SySetRelease(&sFrom);` |
|     15 |  627 | `		SySetRelease(&sLen);` |
|     15 |  628 | `		if( rcWalk != SXRET_OK ){` |
|    ! 0 |  629 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 |  630 | `			goto out;` |
|      - |  631 | `		}` |
|     15 |  632 | `		ph7_result_value(pCtx,pResult);` |
|     15 |  633 | `		rc = PH7_OK;` |
|     15 |  634 | `		goto out;` |
|      - |  635 | `	}` |
|      - |  636 | `	/* Scalar form: array $offset/$length are a TypeError, array $replace` |
|      - |  637 | `	 * degrades to its first element (php quirk). */` |
|     41 |  638 | `	if( ph7_value_is_array(apArg[2]) ){` |
|      3 |  639 | `		rc = PH7_VmThrowException(pCtx,` |
|      - |  640 | `			"TypeError",` |
|      - |  641 | `			"substr_replace(): Argument #3 ($offset) cannot be an array when working on a single string"` |
|      - |  642 | `			);` |
|      3 |  643 | `		goto out;` |
|      - |  644 | `	}` |
|     39 |  645 | `	if( bLenGiven && ph7_value_is_array(apArg[3]) ){` |
|      3 |  646 | `		rc = PH7_VmThrowException(pCtx,` |
|      - |  647 | `			"TypeError",` |
|      - |  648 | `			"substr_replace(): Argument #4 ($length) cannot be an array when working on a single string"` |
|      - |  649 | `			);` |
|      3 |  650 | `		goto out;` |
|      - |  651 | `	}` |
|     37 |  652 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - |  653 | `		/* First element of the replace array, or "" when empty */` |
|      5 |  654 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      5 |  655 | `		zRepl = "";` |
|      5 |  656 | `		nRepl = 0;` |
|      5 |  657 | `		if( pMap->pFirst ){` |
|      3 |  658 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pMap->pFirst->nValIdx);` |
|      3 |  659 | `			if( pVal ){` |
|      3 |  660 | `				zRepl = ph7_value_to_string(pVal,&nRepl);` |
|      1 |  661 | `			}` |
|      1 |  662 | `		}` |
|      2 |  663 | `	}` |
|     37 |  664 | `	if( !bLenGiven ){` |
|     15 |  665 | `		l = nLen;` |
|      7 |  666 | `	}` |
|     37 |  667 | `	SubstrReplaceWindow(&f,&l,nLen);` |
|      - |  668 | `	/* Assemble prefix + replacement + suffix straight into the call result` |
|      - |  669 | `	 * (ph7_result_string appends), no scratch buffer needed. */` |
|     37 |  670 | `	rc = SXRET_OK;` |
|     37 |  671 | `	if( f > 0 ){` |
|     29 |  672 | `		rc = ph7_result_string(pCtx,zStr,(int)f);` |
|     14 |  673 | `	}` |
|     37 |  674 | `	if( rc == SXRET_OK && nRepl > 0 ){` |
|     33 |  675 | `		rc = ph7_result_string(pCtx,zRepl,nRepl);` |
|     16 |  676 | `	}` |
|     37 |  677 | `	if( rc == SXRET_OK && nLen - (int)(f+l) > 0 ){` |
|     17 |  678 | `		rc = ph7_result_string(pCtx,&zStr[f+l],nLen - (int)(f+l));` |
|      8 |  679 | `	}` |
|     37 |  680 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  681 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 |  682 | `		goto out;` |
|      - |  683 | `	}` |
|      - |  684 | `	/* Force a string result even when all three segments are empty */` |
|     37 |  685 | `	rc = ph7_result_string(pCtx,"",0);` |
|     37 |  686 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  687 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 |  688 | `		goto out;` |
|      - |  689 | `	}` |
|     37 |  690 | `	rc = PH7_OK;` |
|     29 |  691 | `out:` |
|     59 |  692 | `	PH7_MemObjRelease(&sStrTmp);` |
|     59 |  693 | `	PH7_MemObjRelease(&sReplTmp);` |
|     59 |  694 | `	return rc;` |
|     30 |  695 | `}` |
|      - |  696 | `/*` |
|      - |  697 | ` * int levenshtein(string $string1,string $string2[,int $insertion_cost = 1[,int $replacement_cost = 1[,int $deletion_cost = 1]]])` |
|      - |  698 | ` *  Calculate the Levenshtein distance between two strings, byte per byte` |
|      - |  699 | ` *  (case-sensitive), with optional per-operation costs. Mirrors PHP's` |
|      - |  700 | ` *  reference_levdist(): two rolling rows over string2.` |
|      - |  701 | ` * Return` |
|      - |  702 | ` *  The minimal number of weighted edit operations turning $string1 into` |
|      - |  703 | ` *  $string2.` |
|      - |  704 | ` */` |
|     34 |  705 | `PH7_PRIVATE int PH7_builtin_levenshtein(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  706 | `{` |
|      - |  707 | `	static const char *azParam[] = { "$insertion_cost","$replacement_cost","$deletion_cost" };` |
|      - |  708 | `	const char *zStr1,*zStr2;` |
|     35 |  709 | `	sxi64 iCostIns = 1,iCostRep = 1,iCostDel = 1;` |
|      - |  710 | `	sxi64 *p1,*p2,*pTmp;` |
|      - |  711 | `	sxi64 c0,c1,c2;` |
|      - |  712 | `	ph7_value sTmp1,sTmp2;` |
|      - |  713 | `	int nLen1,nLen2;` |
|      - |  714 | `	int i1,i2;` |
|      - |  715 | `	sxi32 rc;` |
|      - |  716 | `	int i;` |
|     35 |  717 | `	if( nArg < 2 ){` |
|    ! 0 |  718 | `		return PH7_VmThrowException(pCtx,` |
|      - |  719 | `			"ArgumentCountError",` |
|      - |  720 | `			"levenshtein() expects at least 2 arguments, %d given",` |
|    ! 0 |  721 | `			nArg` |
|      - |  722 | `			);` |
|      - |  723 | `	}` |
|      - |  724 | `	/* $string1/$string2: null deprecates to "", __toString objects resolve,` |
|      - |  725 | `	 * everything non-stringish is a TypeError (php ZPP weak mode). */` |
|     35 |  726 | `	PH7_MemObjInit(pCtx->pVm,&sTmp1);` |
|     35 |  727 | `	PH7_MemObjInit(pCtx->pVm,&sTmp2);` |
|     35 |  728 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"levenshtein",1,"$string1","string",` |
|      - |  729 | `		"levenshtein(): Passing null to parameter #1 ($string1) "` |
|      - |  730 | `		"of type string is deprecated",` |
|      - |  731 | `		&sTmp1,&zStr1,&nLen1);` |
|     35 |  732 | `	if( rc != PH7_OK ) goto out;` |
|     35 |  733 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"levenshtein",2,"$string2","string",` |
|      - |  734 | `		"levenshtein(): Passing null to parameter #2 ($string2) "` |
|      - |  735 | `		"of type string is deprecated",` |
|      - |  736 | `		&sTmp2,&zStr2,&nLen2);` |
|     35 |  737 | `	if( rc != PH7_OK ) goto out;` |
|      - |  738 | `	/* Optional integer costs */` |
|     57 |  739 | `	for( i = 2 ; i < nArg && i < 5 ; i++ ){` |
|      - |  740 | `		sxi64 iVal;` |
|     31 |  741 | `		rc = PH7_IntArgResolve(pCtx,apArg[i],"levenshtein",i+1,azParam[i-2],"int",&iVal);` |
|     31 |  742 | `		if( rc != PH7_OK ) goto out;` |
|     23 |  743 | `		if( i == 2 ){` |
|     11 |  744 | `			iCostIns = iVal;` |
|     18 |  745 | `		}else if( i == 3 ){` |
|      7 |  746 | `			iCostRep = iVal;` |
|      4 |  747 | `		}else{` |
|      7 |  748 | `			iCostDel = iVal;` |
|      - |  749 | `		}` |
|     12 |  750 | `	}` |
|     27 |  751 | `	if( nLen1 == 0 ){` |
|      3 |  752 | `		ph7_result_int64(pCtx,(sxi64)nLen2 * iCostIns);` |
|      3 |  753 | `		rc = PH7_OK;` |
|      3 |  754 | `		goto out;` |
|      - |  755 | `	}` |
|     25 |  756 | `	if( nLen2 == 0 ){` |
|      3 |  757 | `		ph7_result_int64(pCtx,(sxi64)nLen1 * iCostDel);` |
|      3 |  758 | `		rc = PH7_OK;` |
|      3 |  759 | `		goto out;` |
|      - |  760 | `	}` |
|      - |  761 | `	/* Two rolling DP rows over string2 (auto-released on return). Reject a` |
|      - |  762 | `	 * string2 long enough to overflow the 32-bit allocation size. */` |
|     23 |  763 | `	if( (sxu32)nLen2 >= (SXU32_HIGH / sizeof(sxi64)) - 1 ){` |
|    ! 0 |  764 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 |  765 | `		goto out;` |
|      - |  766 | `	}` |
|     23 |  767 | `	p1 = (sxi64 *)ph7_context_alloc_chunk(pCtx,(unsigned int)(sizeof(sxi64) * (sxu32)(nLen2 + 1)),FALSE,TRUE);` |
|     23 |  768 | `	p2 = (sxi64 *)ph7_context_alloc_chunk(pCtx,(unsigned int)(sizeof(sxi64) * (sxu32)(nLen2 + 1)),FALSE,TRUE);` |
|     23 |  769 | `	if( p1 == 0 \|\| p2 == 0 ){` |
|    ! 0 |  770 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 |  771 | `		goto out;` |
|      - |  772 | `	}` |
|    733 |  773 | `	for( i2 = 0 ; i2 <= nLen2 ; i2++ ){` |
|    711 |  774 | `		p1[i2] = (sxi64)i2 * iCostIns;` |
|    356 |  775 | `	}` |
|    707 |  776 | `	for( i1 = 0 ; i1 < nLen1 ; i1++ ){` |
|    685 |  777 | `		p2[0] = p1[0] + iCostDel;` |
| 181111 |  778 | `		for( i2 = 0 ; i2 < nLen2 ; i2++ ){` |
| 180427 |  779 | `			c0 = p1[i2] + ((zStr1[i1] == zStr2[i2]) ? 0 : iCostRep);` |
| 180427 |  780 | `			c1 = p1[i2 + 1] + iCostDel;` |
| 180427 |  781 | `			if( c1 < c0 ){` |
|  45393 |  782 | `				c0 = c1;` |
|  22696 |  783 | `			}` |
| 180427 |  784 | `			c2 = p2[i2] + iCostIns;` |
| 180427 |  785 | `			if( c2 < c0 ){` |
|  44809 |  786 | `				c0 = c2;` |
|  22404 |  787 | `			}` |
| 180427 |  788 | `			p2[i2 + 1] = c0;` |
|  90214 |  789 | `		}` |
|    685 |  790 | `		pTmp = p1;` |
|    685 |  791 | `		p1 = p2;` |
|    685 |  792 | `		p2 = pTmp;` |
|    343 |  793 | `	}` |
|     23 |  794 | `	ph7_result_int64(pCtx,p1[nLen2]);` |
|     23 |  795 | `	rc = PH7_OK;` |
|     17 |  796 | `out:` |
|     35 |  797 | `	PH7_MemObjRelease(&sTmp1);` |
|     35 |  798 | `	PH7_MemObjRelease(&sTmp2);` |
|     35 |  799 | `	return rc;` |
|     18 |  800 | `}` |
|      - |  801 | `/*` |
|      - |  802 | ` * Longest common substring scan behind similar_text() — a faithful port of` |
|      - |  803 | ` * PHP's php_similar_str(): O(n*m) scan recording the first longest run.` |
|      - |  804 | ` */` |
|     26 |  805 | `static void SimilarStr(const char *zTxt1,int nLen1,const char *zTxt2,int nLen2,` |
|      - |  806 | `	int *pPos1,int *pPos2,int *pMax,int *pCount)` |
|      1 |  807 | `{` |
|      - |  808 | `	const char *p,*q;` |
|     27 |  809 | `	const char *zEnd1 = &zTxt1[nLen1];` |
|     27 |  810 | `	const char *zEnd2 = &zTxt2[nLen2];` |
|      - |  811 | `	int l;` |
|     27 |  812 | `	*pMax = 0;` |
|     27 |  813 | `	*pCount = 0;` |
|    143 |  814 | `	for( p = zTxt1 ; p < zEnd1 ; p++ ){` |
|    843 |  815 | `		for( q = zTxt2 ; q < zEnd2 ; q++ ){` |
|    999 |  816 | `			for( l = 0 ; (p+l < zEnd1) && (q+l < zEnd2) && (p[l] == q[l]) ; l++ );` |
|    727 |  817 | `			if( l > *pMax ){` |
|     25 |  818 | `				*pMax = l;` |
|     25 |  819 | `				*pCount += 1;` |
|     25 |  820 | `				*pPos1 = (int)(p - zTxt1);` |
|     25 |  821 | `				*pPos2 = (int)(q - zTxt2);` |
|     12 |  822 | `			}` |
|    364 |  823 | `		}` |
|     59 |  824 | `	}` |
|     27 |  825 | `}` |
|      - |  826 | `/*` |
|      - |  827 | ` * Recursive divide-and-conquer behind similar_text() — a faithful port of` |
|      - |  828 | `` * PHP's php_similar_char(), including its quirky `count > 1` guard on the`` |
|      - |  829 | ` * left-side recursion.` |
|      - |  830 | ` */` |
|     26 |  831 | `static int SimilarChar(const char *zTxt1,int nLen1,const char *zTxt2,int nLen2)` |
|      1 |  832 | `{` |
|      - |  833 | `	int nSum;` |
|     27 |  834 | `	int nPos1 = 0,nPos2 = 0,nMax,nCount;` |
|     27 |  835 | `	SimilarStr(zTxt1,nLen1,zTxt2,nLen2,&nPos1,&nPos2,&nMax,&nCount);` |
|     27 |  836 | `	if( (nSum = nMax) != 0 ){` |
|     25 |  837 | `		if( nPos1 && nPos2 && nCount > 1 ){` |
|    ! 0 |  838 | `			nSum += SimilarChar(zTxt1,nPos1,zTxt2,nPos2);` |
|    ! 0 |  839 | `		}` |
|     25 |  840 | `		if( (nPos1 + nMax < nLen1) && (nPos2 + nMax < nLen2) ){` |
|     13 |  841 | `			nSum += SimilarChar(&zTxt1[nPos1 + nMax],nLen1 - nPos1 - nMax,` |
|      8 |  842 | `				&zTxt2[nPos2 + nMax],nLen2 - nPos2 - nMax);` |
|      4 |  843 | `		}` |
|     12 |  844 | `	}` |
|     27 |  845 | `	return nSum;` |
|      1 |  846 | `}` |
|      - |  847 | `/*` |
|      - |  848 | ` * int similar_text(string $string1,string $string2[,float &$percent])` |
|      - |  849 | ` *  Calculate the similarity between two strings, as the number of matching` |
|      - |  850 | ` *  characters found by PHP's greedy longest-common-substring recursion.` |
|      - |  851 | ` *  When $percent is given it receives the similarity in percent:` |
|      - |  852 | ` *  matching * 200 / (len1 + len2).` |
|      - |  853 | ` * Return` |
|      - |  854 | ` *  The number of matching characters in both strings.` |
|      - |  855 | ` */` |
|     22 |  856 | `PH7_PRIVATE int PH7_builtin_similar_text(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  857 | `{` |
|      - |  858 | `	const char *zStr1,*zStr2;` |
|      - |  859 | `	ph7_value sTmp1,sTmp2;` |
|      - |  860 | `	int nLen1,nLen2;` |
|      - |  861 | `	int nSim;` |
|      - |  862 | `	sxi32 rc;` |
|     23 |  863 | `	if( nArg < 2 ){` |
|    ! 0 |  864 | `		return PH7_VmThrowException(pCtx,` |
|      - |  865 | `			"ArgumentCountError",` |
|      - |  866 | `			"similar_text() expects at least 2 arguments, %d given",` |
|    ! 0 |  867 | `			nArg` |
|      - |  868 | `			);` |
|      - |  869 | `	}` |
|     23 |  870 | `	PH7_MemObjInit(pCtx->pVm,&sTmp1);` |
|     23 |  871 | `	PH7_MemObjInit(pCtx->pVm,&sTmp2);` |
|     23 |  872 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"similar_text",1,"$string1","string",` |
|      - |  873 | `		"similar_text(): Passing null to parameter #1 ($string1) "` |
|      - |  874 | `		"of type string is deprecated",` |
|      - |  875 | `		&sTmp1,&zStr1,&nLen1);` |
|     23 |  876 | `	if( rc != PH7_OK ) goto out;` |
|     23 |  877 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"similar_text",2,"$string2","string",` |
|      - |  878 | `		"similar_text(): Passing null to parameter #2 ($string2) "` |
|      - |  879 | `		"of type string is deprecated",` |
|      - |  880 | `		&sTmp2,&zStr2,&nLen2);` |
|     23 |  881 | `	if( rc != PH7_OK ) goto out;` |
|     23 |  882 | `	if( nLen1 + nLen2 == 0 ){` |
|      5 |  883 | `		nSim = 0;` |
|      3 |  884 | `	}else{` |
|     19 |  885 | `		nSim = SimilarChar(zStr1,nLen1,zStr2,nLen2);` |
|      - |  886 | `	}` |
|     23 |  887 | `	if( nArg > 2 ){` |
|      - |  888 | `		/* Write the percentage through the by-ref out-param */` |
|      7 |  889 | `		ph7_value *pPercent = ph7_context_new_scalar(pCtx);` |
|      7 |  890 | `		if( pPercent == 0 ){` |
|    ! 0 |  891 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 |  892 | `			goto out;` |
|    ! 0 |  893 | `		}else{` |
|      7 |  894 | `			double dPct = (nLen1 + nLen2 == 0) ? 0.0 : (double)nSim * 200.0 / (double)(nLen1 + nLen2);` |
|      7 |  895 | `			ph7_value_double(pPercent,dPct);` |
|      7 |  896 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],pPercent);` |
|      - |  897 | `		}` |
|      3 |  898 | `	}` |
|     23 |  899 | `	ph7_result_int(pCtx,nSim);` |
|     23 |  900 | `	rc = PH7_OK;` |
|     11 |  901 | `out:` |
|     23 |  902 | `	PH7_MemObjRelease(&sTmp1);` |
|     23 |  903 | `	PH7_MemObjRelease(&sTmp2);` |
|     23 |  904 | `	return rc;` |
|     12 |  905 | `}` |
|      - |  906 | `/*` |
|      - |  907 | ` * array\|int str_word_count(string $string[,int $format = 0[,?string $characters = null]])` |
|      - |  908 | ` *  Count (or return) the words inside a string. A word is a run of alphabetic` |
|      - |  909 | ` *  characters, which may contain (but not start the string with) "'" and "-";` |
|      - |  910 | ` *  $characters adds extra bytes to the word set ("a..z" ranges supported, as` |
|      - |  911 | ` *  in PHP's php_charmask).` |
|      - |  912 | ` *  $format: 0 -> word count, 1 -> array of words, 2 -> array of words keyed` |
|      - |  913 | ` *  by their byte position in $string.` |
|      - |  914 | ` * Errors` |
|      - |  915 | ` *  ValueError when $format is not 0, 1 or 2.` |
|      - |  916 | ` */` |
|     44 |  917 | `PH7_PRIVATE int PH7_builtin_str_word_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  918 | `{` |
|      - |  919 | `	const char *zIn,*zEnd,*zPtr;` |
|     45 |  920 | `	ph7_value *pArray = 0,*pValue = 0;` |
|      - |  921 | `	ph7_value sTmp,sListTmp;` |
|      - |  922 | `	char aMask[256];` |
|     45 |  923 | `	int bMask = 0;` |
|     45 |  924 | `	int iFormat = 0;` |
|     45 |  925 | `	int nCount = 0;` |
|      - |  926 | `	int nLen;` |
|      - |  927 | `	sxi32 rc;` |
|     45 |  928 | `	if( nArg < 1 ){` |
|    ! 0 |  929 | `		return PH7_VmThrowException(pCtx,` |
|      - |  930 | `			"ArgumentCountError",` |
|      - |  931 | `			"str_word_count() expects at least 1 argument, %d given",` |
|    ! 0 |  932 | `			nArg` |
|      - |  933 | `			);` |
|      - |  934 | `	}` |
|     45 |  935 | `	PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|     45 |  936 | `	PH7_MemObjInit(pCtx->pVm,&sListTmp);` |
|     45 |  937 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_word_count",1,"$string","string",` |
|      - |  938 | `		"str_word_count(): Passing null to parameter #1 ($string) "` |
|      - |  939 | `		"of type string is deprecated",` |
|      - |  940 | `		&sTmp,&zIn,&nLen);` |
|     45 |  941 | `	if( rc != PH7_OK ) goto out;` |
|     45 |  942 | `	if( nArg > 1 ){` |
|      - |  943 | `		sxi64 iVal;` |
|     31 |  944 | `		rc = PH7_IntArgResolve(pCtx,apArg[1],"str_word_count",2,"$format","int",&iVal);` |
|     33 |  945 | `		if( rc != PH7_OK ) goto out;` |
|     29 |  946 | `		if( iVal < 0 \|\| iVal > 2 ){` |
|      5 |  947 | `			rc = PH7_VmThrowException(pCtx,` |
|      - |  948 | `				"ValueError",` |
|      - |  949 | `				"str_word_count(): Argument #2 ($format) must be a valid format value"` |
|      - |  950 | `				);` |
|      5 |  951 | `			goto out;` |
|      - |  952 | `		}` |
|     25 |  953 | `		iFormat = (int)iVal;` |
|     12 |  954 | `	}` |
|     39 |  955 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      - |  956 | `		/* $characters is ?string: null (skipped above) simply keeps the` |
|      - |  957 | `		 * default word set, no deprecation. */` |
|      - |  958 | `		const char *zList;` |
|      - |  959 | `		int nList;` |
|     13 |  960 | `		rc = StrPredicateResolveArg(pCtx,apArg[2],"str_word_count",3,"$characters","?string",` |
|      - |  961 | `			"" /* unreachable: null never gets here */,` |
|      - |  962 | `			&sListTmp,&zList,&nList);` |
|     13 |  963 | `		if( rc != PH7_OK ) goto out;` |
|     13 |  964 | `		PH7_BuildCharMask(pCtx,zList,nList,aMask);` |
|     13 |  965 | `		bMask = 1;` |
|      6 |  966 | `	}` |
|     39 |  967 | `	if( iFormat != 0 ){` |
|     25 |  968 | `		pArray = ph7_context_new_array(pCtx);` |
|     25 |  969 | `		pValue = ph7_context_new_scalar(pCtx);` |
|     25 |  970 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 |  971 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 |  972 | `			goto out;` |
|      - |  973 | `		}` |
|     12 |  974 | `	}` |
|     39 |  975 | `	zPtr = zIn;` |
|     39 |  976 | `	zEnd = &zIn[nLen];` |
|     39 |  977 | `	if( nLen > 0 ){` |
|      - |  978 | `		/* php: the string's first byte cannot be ' or -, and its last byte` |
|      - |  979 | `		 * cannot be -, unless the charlist explicitly allows them. */` |
|     33 |  980 | `		if( (zPtr[0] == '\'' && (!bMask \|\| !aMask[(unsigned char)'\''])) \|\|` |
|     28 |  981 | `			(zPtr[0] == '-'  && (!bMask \|\| !aMask[(unsigned char)'-'])) ){` |
|      9 |  982 | `			zPtr++;` |
|      4 |  983 | `		}` |
|     33 |  984 | `		if( zEnd[-1] == '-' && (!bMask \|\| !aMask[(unsigned char)'-']) ){` |
|      9 |  985 | `			zEnd--;` |
|      4 |  986 | `		}` |
|     16 |  987 | `	}` |
|    135 |  988 | `	while( zPtr < zEnd ){` |
|     91 |  989 | `		const char *zStart = zPtr;` |
|    477 |  990 | `		while( zPtr < zEnd && ( SyisAlpha((unsigned char)zPtr[0])` |
|    253 |  991 | `			\|\| (bMask && aMask[(unsigned char)zPtr[0]])` |
|     98 |  992 | `			\|\| zPtr[0] == '\'' \|\| zPtr[0] == '-' ) ){` |
|    339 |  993 | `			zPtr++;` |
|      1 |  994 | `		}` |
|     97 |  995 | `		if( zPtr > zStart ){` |
|     91 |  996 | `			if( iFormat == 0 ){` |
|     19 |  997 | `				nCount++;` |
|     10 |  998 | `			}else{` |
|     73 |  999 | `				ph7_value_reset_string_cursor(pValue);` |
|     73 | 1000 | `				if( SXRET_OK != ph7_value_string(pValue,zStart,(int)(zPtr-zStart)) ){` |
|    ! 0 | 1001 | `					rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1002 | `					goto out;` |
|      - | 1003 | `				}` |
|     73 | 1004 | `				if( iFormat == 1 ){` |
|     59 | 1005 | `					if( SXRET_OK != ph7_array_add_elem(pArray,0,pValue) ){` |
|    ! 0 | 1006 | `						rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1007 | `						goto out;` |
|      - | 1008 | `					}` |
|     30 | 1009 | `				}else{` |
|     15 | 1010 | `					if( SXRET_OK != ph7_array_add_intkey_elem(pArray,(int)(zStart-zIn),pValue) ){` |
|    ! 0 | 1011 | `						rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1012 | `						goto out;` |
|      - | 1013 | `					}` |
|      - | 1014 | `				}` |
|      - | 1015 | `			}` |
|     45 | 1016 | `		}` |
|     97 | 1017 | `		zPtr++;` |
|      1 | 1018 | `	}` |
|     37 | 1019 | `	if( iFormat == 0 ){` |
|     13 | 1020 | `		ph7_result_int(pCtx,nCount);` |
|      7 | 1021 | `	}else{` |
|     25 | 1022 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1023 | `	}` |
|     37 | 1024 | `	rc = PH7_OK;` |
|     21 | 1025 | `out:` |
|     43 | 1026 | `	PH7_MemObjRelease(&sTmp);` |
|     43 | 1027 | `	PH7_MemObjRelease(&sListTmp);` |
|     43 | 1028 | `	return rc;` |
|     22 | 1029 | `}` |
|      - | 1030 | `/*` |
|      - | 1031 | ` * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])` |
|      - | 1032 | ` *   Split a string into smaller chunks.` |
|      - | 1033 | ` * Parameters` |
|      - | 1034 | ` *  $body` |
|      - | 1035 | ` *   The string to be chunked.` |
|      - | 1036 | ` * $chunklen` |
|      - | 1037 | ` *   The chunk length.` |
|      - | 1038 | ` * $end` |
|      - | 1039 | ` *   The line ending sequence.` |
|      - | 1040 | ` * Return` |
|      - | 1041 | ` *  The chunked string or NULL on failure.` |
|      - | 1042 | ` */` |
|     14 | 1043 | `PH7_PRIVATE int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1044 | `{` |
|     15 | 1045 | `	const char *zIn,*zEnd,*zSep = "\r\n";` |
|      - | 1046 | `	int nSepLen,nChunkLen,nLen;` |
|      - | 1047 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1048 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     15 | 1049 | `	if( nArg < 1 ){` |
|      - | 1050 | `		/* Nothing to split,return null */` |
|    ! 0 | 1051 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1052 | `		return PH7_OK;` |
|      - | 1053 | `	}` |
|      - | 1054 | `	/* initialize/Extract arguments */` |
|     15 | 1055 | `	nSepLen = (int)sizeof("\r\n") - 1;` |
|     15 | 1056 | `	nChunkLen = 76;` |
|     15 | 1057 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 1058 | `	zEnd = &zIn[nLen];` |
|     15 | 1059 | `	if( nArg > 1 ){` |
|      - | 1060 | `		/* Chunk length */` |
|     13 | 1061 | `		nChunkLen = ph7_value_to_int(apArg[1]);` |
|     13 | 1062 | `		if( nChunkLen < 1 ){` |
|      - | 1063 | `			/* PHP 8 throws a catchable ValueError for a non-positive length. */` |
|      3 | 1064 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1065 | `				"chunk_split(): Argument #2 ($length) must be greater than 0");` |
|      - | 1066 | `		}` |
|     11 | 1067 | `		if( nArg > 2 ){` |
|      - | 1068 | `			/* Separator */` |
|      9 | 1069 | `			zSep = ph7_value_to_string(apArg[2],&nSepLen);` |
|      9 | 1070 | `			if( nSepLen < 1 ){` |
|      - | 1071 | `				/* Switch back to the default separator */` |
|      3 | 1072 | `				zSep = "\r\n";` |
|      3 | 1073 | `				nSepLen = (int)sizeof("\r\n") - 1;` |
|      1 | 1074 | `			}` |
|      4 | 1075 | `		}` |
|      5 | 1076 | `	}` |
|      - | 1077 | `	/* Perform the requested operation */` |
|     13 | 1078 | `	if( nChunkLen > nLen ){` |
|      - | 1079 | `		/* Nothing to split,return the string and the separator */` |
|      9 | 1080 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);` |
|      9 | 1081 | `		return PH7_OK;` |
|      - | 1082 | `	}` |
|     17 | 1083 | `	while( zIn < zEnd ){` |
|     13 | 1084 | `		if( nChunkLen > (int)(zEnd-zIn) ){` |
|      3 | 1085 | `			nChunkLen = (int)(zEnd - zIn);` |
|      1 | 1086 | `		}` |
|      - | 1087 | `		/* Append the chunk and the separator */` |
|     13 | 1088 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);` |
|      - | 1089 | `		/* Point beyond the chunk */` |
|     13 | 1090 | `		zIn += nChunkLen;` |
|      1 | 1091 | `	}` |
|      5 | 1092 | `	return PH7_OK;` |
|      8 | 1093 | `}` |
|      - | 1094 | `/*` |
|      - | 1095 | ` * string addslashes(string $str)` |
|      - | 1096 | ` *  Quote string with slashes.` |
|      - | 1097 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1098 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1099 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1100 | ` * Parameter` |
|      - | 1101 | ` *  str: The string to be escaped.` |
|      - | 1102 | ` * Return` |
|      - | 1103 | ` *  Returns the escaped string` |
|      - | 1104 | ` */` |
|     18 | 1105 | `PH7_PRIVATE int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1106 | `{` |
|      - | 1107 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1108 | `	int nLen;` |
|      - | 1109 | `	/* PHP enforces exactly one argument. */` |
|     20 | 1110 | `	if( nArg != 1 ){` |
|    ! 0 | 1111 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1112 | `			"ArgumentCountError",` |
|      - | 1113 | `			"addslashes() expects exactly 1 argument, %d given",` |
|    ! 0 | 1114 | `			nArg` |
|      - | 1115 | `			);` |
|      - | 1116 | `	}` |
|      - | 1117 | `	/* php only DEPRECATES null here; PHL rejects it. */` |
|     20 | 1118 | `	if( ph7_value_is_null(apArg[0]) ){` |
|    ! 0 | 1119 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1120 | `			"addslashes(): Argument #1 ($string) must be of type string, null given"` |
|      - | 1121 | `			);` |
|      - | 1122 | `	}` |
|      - | 1123 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     27 | 1124 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     29 | 1125 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     18 | 1126 | `	    ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 1127 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1128 | `			"TypeError",` |
|      - | 1129 | `			"addslashes(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 1130 | `			ph7_type_name(apArg[0])` |
|      - | 1131 | `			);` |
|      - | 1132 | `	}` |
|      - | 1133 | `	/* Convert to string representation first and obtain length. */` |
|     20 | 1134 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     20 | 1135 | `	if( nLen < 1 ){` |
|      - | 1136 | `		/* Return the empty string */` |
|      6 | 1137 | `		ph7_result_string(pCtx,"",0);` |
|      6 | 1138 | `		return PH7_OK;` |
|      - | 1139 | `	}` |
|     15 | 1140 | `	zEnd = &zIn[nLen];` |
|     15 | 1141 | `	zCur = 0; /* cc warning */` |
|     20 | 1142 | `	for(;;){` |
|     41 | 1143 | `		if( zIn >= zEnd ){` |
|      - | 1144 | `			/* No more input */` |
|     15 | 1145 | `			break;` |
|      - | 1146 | `		}` |
|     27 | 1147 | `		zCur = zIn;` |
|      - | 1148 | `		/* scan until a character that needs escaping (', ", \\, or NUL) */` |
|     89 | 1149 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' && zIn[0] != '\0' ){` |
|     63 | 1150 | `			zIn++;` |
|      1 | 1151 | `		}` |
|     27 | 1152 | `		if( zIn > zCur ){` |
|      - | 1153 | `			/* Append raw contents */` |
|     23 | 1154 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 1155 | `		}` |
|     27 | 1156 | `		if( zIn < zEnd ){` |
|     17 | 1157 | `			int c = zIn[0];` |
|     17 | 1158 | `			if( c == '\0' ){` |
|      - | 1159 | `				/* PHP escapes NUL as "\\0" (two characters) */` |
|      3 | 1160 | `				ph7_result_string(pCtx,"\\0",2);` |
|      2 | 1161 | `			}else{` |
|     15 | 1162 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1163 | `			}` |
|      8 | 1164 | `		}` |
|     27 | 1165 | `		zIn++;` |
|      1 | 1166 | `	}` |
|     15 | 1167 | `	return PH7_OK;` |
|     11 | 1168 | `}` |
|      - | 1169 | `/*` |
|      - | 1170 | ``  * Build a 256-entry membership mask from a PHP charlist, expanding `a..z` `` |
|      - | 1171 | ` * byte ranges exactly like PHP's php_charmask(). On return aMask[c] != 0 iff` |
|      - | 1172 | ` * the byte c belongs to the set. Emits the PHP-exact warnings for the three` |
|      - | 1173 | ` * malformed-range shapes (ph7_context_throw_error_format prepends the active` |
|      - | 1174 | ` * function name, so the messages omit it); on a bad range the surrounding` |
|      - | 1175 | ` * bytes are still added and the scan never aborts. Reads only within` |
|      - | 1176 | ` * [zList, zList+nLen).` |
|      - | 1177 | ` *` |
|      - | 1178 | ` * Use ONLY for the builtins whose charlist expands ranges the way PHP's` |
|      - | 1179 | ` * php_charmask() does: trim/ltrim/rtrim/addcslashes (and quotemeta, whose set` |
|      - | 1180 | ` * is a fixed literal with no ".."). Do NOT route strspn/strcspn/strtok/strpbrk` |
|      - | 1181 | ` * through this — PHP treats their charlists literally, so expanding "a..z" here` |
|      - | 1182 | ` * would be a behavior regression plus spurious "Invalid '..'-range" warnings.` |
|      - | 1183 | ` */` |
|    288 | 1184 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256])` |
|      5 | 1185 | `{` |
|    293 | 1186 | `	const unsigned char *zIn  = (const unsigned char *)zList;` |
|    293 | 1187 | `	const unsigned char *zEnd = zIn + (nLen > 0 ? nLen : 0);` |
|    293 | 1188 | `	SyZero(aMask,256);` |
|    877 | 1189 | `	for( ; zIn < zEnd ; zIn++ ){` |
|    589 | 1190 | `		int c = zIn[0];` |
|    589 | 1191 | `		if( zIn + 3 < zEnd && zIn[1] == '.' && zIn[2] == '.' && zIn[3] >= c ){` |
|      - | 1192 | `			/* Valid incrementing range c..zIn[3] */` |
|     22 | 1193 | `			int hi = zIn[3],k;` |
|    386 | 1194 | `			for( k = c ; k <= hi ; k++ ){` |
|    366 | 1195 | `				aMask[k] = 1;` |
|    184 | 1196 | `			}` |
|     22 | 1197 | `			zIn += 3; /* the loop's ++ then steps past the range end */` |
|    591 | 1198 | `		}else if( zIn + 1 < zEnd && zIn[0] == '.' && zIn[1] == '.' ){` |
|      - | 1199 | `			/* Malformed range: mirror php_charmask's three diagnostics. */` |
|      - | 1200 | `			const char *zMsg;` |
|     26 | 1201 | `			if( (const unsigned char *)zList >= zIn ){` |
|      6 | 1202 | `				zMsg = "no character to the left of '..'";` |
|     24 | 1203 | `			}else if( zIn + 2 >= zEnd ){` |
|      6 | 1204 | `				zMsg = "no character to the right of '..'";` |
|     20 | 1205 | `			}else if( zIn[-1] > zIn[2] ){` |
|     18 | 1206 | `				zMsg = "'..'-range needs to be incrementing";` |
|     10 | 1207 | `			}else{` |
|    ! 0 | 1208 | `				zMsg = 0; /* catch-all (e.g. a..b..c) */` |
|      - | 1209 | `			}` |
|     26 | 1210 | `			if( zMsg ){` |
|     38 | 1211 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|     12 | 1212 | `					"Invalid '..'-range, %s",zMsg);` |
|     14 | 1213 | `			}else{` |
|    ! 0 | 1214 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1215 | `					"Invalid '..'-range");` |
|      - | 1216 | `			}` |
|      - | 1217 | `			/* Do not consume the dots: the loop's ++ steps one byte so the` |
|      - | 1218 | `			 * dots are re-scanned as literals, exactly like php_charmask. */` |
|     14 | 1219 | `		}else{` |
|    545 | 1220 | `			aMask[c] = 1;` |
|      - | 1221 | `		}` |
|    297 | 1222 | `	}` |
|    293 | 1223 | `}` |
|      - | 1224 | `/*` |
|      - | 1225 | ` * string addcslashes(string $str,string $charlist)` |
|      - | 1226 | ` *  Quote string with slashes in a C style.` |
|      - | 1227 | ` * Parameter` |
|      - | 1228 | ` *  $str:` |
|      - | 1229 | ` *    The string to be escaped.` |
|      - | 1230 | ` *  $charlist:` |
|      - | 1231 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|      - | 1232 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|      - | 1233 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|      - | 1234 | ` * Return` |
|      - | 1235 | ` *  Returns the escaped string.` |
|      - | 1236 | ` * Note:` |
|      - | 1237 | ` *  Character ranges [i.e: 'A..Z'] are supported (see PH7_BuildCharMask).` |
|      - | 1238 | ` */` |
|     28 | 1239 | `PH7_PRIVATE int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1240 | `{` |
|      - | 1241 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - | 1242 | `	char aMask[256];` |
|      - | 1243 | `	int nLen,nMask;` |
|      - | 1244 | `	/* PHP enforces exactly two arguments. */` |
|     31 | 1245 | `	if( nArg != 2 ){` |
|    ! 0 | 1246 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1247 | `			"ArgumentCountError",` |
|      - | 1248 | `			"addcslashes() expects exactly 2 arguments, %d given",` |
|    ! 0 | 1249 | `			nArg` |
|      - | 1250 | `			);` |
|      - | 1251 | `	}` |
|      - | 1252 | `	/* php only DEPRECATES null here; PHL rejects it. */` |
|     31 | 1253 | `	if( ph7_value_is_null(apArg[0]) ){` |
|    ! 0 | 1254 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1255 | `			"TypeError",` |
|      - | 1256 | `			"addcslashes(): Argument #1 ($string) must be of type string, null given"` |
|      - | 1257 | `			);` |
|     42 | 1258 | `	} else if( ph7_value_is_array(apArg[0]) \|\|` |
|     45 | 1259 | `	          ph7_value_is_object(apArg[0]) \|\|` |
|     28 | 1260 | `	          ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 1261 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1262 | `			"TypeError",` |
|      - | 1263 | `			"addcslashes(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 1264 | `			ph7_type_name(apArg[0])` |
|      - | 1265 | `			);` |
|      - | 1266 | `	}` |
|      - | 1267 | `	/* php only DEPRECATES null here; PHL rejects it. */` |
|     31 | 1268 | `	if( ph7_value_is_null(apArg[1]) ){` |
|    ! 0 | 1269 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1270 | `			"TypeError",` |
|      - | 1271 | `			"addcslashes(): Argument #2 ($characters) must be of type string, null given"` |
|      - | 1272 | `			);` |
|     42 | 1273 | `	} else if( ph7_value_is_array(apArg[1]) \|\|` |
|     45 | 1274 | `	          ph7_value_is_object(apArg[1]) \|\|` |
|     28 | 1275 | `	          ph7_value_is_resource(apArg[1]) ){` |
|    ! 0 | 1276 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1277 | `			"TypeError",` |
|      - | 1278 | `			"addcslashes(): Argument #2 ($characters) must be of type string, %s given",` |
|    ! 0 | 1279 | `			ph7_type_name(apArg[1])` |
|      - | 1280 | `			);` |
|      - | 1281 | `	}` |
|      - | 1282 | `	/* Extract the string to process */` |
|     31 | 1283 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1284 | `	/* NULL would never reach here due to the check above. */` |
|     31 | 1285 | `	if( nLen < 1 ){` |
|      - | 1286 | `		/* Empty string returns itself. */` |
|      3 | 1287 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      3 | 1288 | `		return PH7_OK;` |
|      - | 1289 | `	}` |
|      - | 1290 | ``	/* Extract the desired mask and expand any `a..z` ranges into a lookup. */`` |
|     29 | 1291 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|     29 | 1292 | `	PH7_BuildCharMask(pCtx,zMask,nMask,aMask);` |
|     29 | 1293 | `	zEnd = &zIn[nLen];` |
|     29 | 1294 | `	zCur = 0; /* cc warning */` |
|     35 | 1295 | `	for(;;){` |
|     73 | 1296 | `		if( zIn >= zEnd ){` |
|      - | 1297 | `			/* No more input */` |
|     29 | 1298 | `			break;` |
|      - | 1299 | `		}` |
|     47 | 1300 | `		zCur = zIn;` |
|    117 | 1301 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     73 | 1302 | `			zIn++;` |
|      3 | 1303 | `		}` |
|     47 | 1304 | `		if( zIn > zCur ){` |
|      - | 1305 | `			/* Append raw contents */` |
|     41 | 1306 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     19 | 1307 | `		}` |
|     47 | 1308 | `		if( zIn < zEnd ){` |
|      - | 1309 | `			/* Make sure we treat the byte as unsigned to avoid negative values` |
|      - | 1310 | `			 * on platforms where char is signed. */` |
|     29 | 1311 | `			int c = (unsigned char)zIn[0];` |
|      - | 1312 | `			/* Handle special C-like escapes for common control characters first.` |
|      - | 1313 | `			 * PHP outputs "\n" "\r" "\t" "\v" "\f" when those chars are` |
|      - | 1314 | `			 * in the mask. NUL is left to the octal conversion below. */` |
|     29 | 1315 | `			if( c == '\n' ){` |
|      3 | 1316 | `				ph7_result_string(pCtx,"\\n",2);` |
|     28 | 1317 | `			}else if( c == '\r' ){` |
|      3 | 1318 | `				ph7_result_string(pCtx,"\\r",2);` |
|     26 | 1319 | `			}else if( c == '\t' ){` |
|      3 | 1320 | `				ph7_result_string(pCtx,"\\t",2);` |
|     24 | 1321 | `			}else if( c == '\v' ){` |
|      3 | 1322 | `				ph7_result_string(pCtx,"\\v",2);` |
|     22 | 1323 | `			}else if( c == '\f' ){` |
|      3 | 1324 | `				ph7_result_string(pCtx,"\\f",2);` |
|     20 | 1325 | `			}else if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - | 1326 | `				/* Convert to octal.  PHP always emits three-digit zero-padded` |
|      - | 1327 | `				 * octal escapes (\001 not \1). */` |
|      7 | 1328 | `				ph7_result_string_format(pCtx,"\\%03o",c);` |
|      4 | 1329 | `			}else{` |
|     13 | 1330 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1331 | `			}` |
|     13 | 1332 | `		}` |
|     47 | 1333 | `		zIn++;` |
|      3 | 1334 | `	}` |
|     29 | 1335 | `	return PH7_OK;` |
|     17 | 1336 | `}` |
|      - | 1337 | `/*` |
|      - | 1338 | ` * string quotemeta(string $str)` |
|      - | 1339 | ` *  Quote meta characters.` |
|      - | 1340 | ` * Parameter` |
|      - | 1341 | ` *  $str:` |
|      - | 1342 | ` *    The string to be escaped.` |
|      - | 1343 | ` * Return` |
|      - | 1344 | ` *  Returns the escaped string.` |
|      - | 1345 | `*/` |
|     12 | 1346 | `PH7_PRIVATE int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1347 | `{` |
|      - | 1348 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1349 | `	char aMask[256];` |
|      - | 1350 | `	int nLen;` |
|     15 | 1351 | `	if( nArg < 1 ){` |
|      - | 1352 | `		/* Nothing to process,retun NULL */` |
|    ! 0 | 1353 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1354 | `		return PH7_OK;` |
|      - | 1355 | `	}` |
|      - | 1356 | `	/* Extract the string to process */` |
|     15 | 1357 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 1358 | `	if( nLen < 1 ){` |
|      - | 1359 | `		/* Return the empty string */` |
|      6 | 1360 | `		ph7_result_string(pCtx,"",0);` |
|      6 | 1361 | `		return PH7_OK;` |
|      - | 1362 | `	}` |
|      - | 1363 | `	/* Fixed meta-character set (no ranges); build the lookup once. */` |
|     10 | 1364 | `	PH7_BuildCharMask(pCtx,".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1,aMask);` |
|     10 | 1365 | `	zEnd = &zIn[nLen];` |
|     10 | 1366 | `	zCur = 0; /* cc warning */` |
|     22 | 1367 | `	for(;;){` |
|     46 | 1368 | `		if( zIn >= zEnd ){` |
|      - | 1369 | `			/* No more input */` |
|     10 | 1370 | `			break;` |
|      - | 1371 | `		}` |
|     38 | 1372 | `		zCur = zIn;` |
|     76 | 1373 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     40 | 1374 | `			zIn++;` |
|      2 | 1375 | `		}` |
|     38 | 1376 | `		if( zIn > zCur ){` |
|      - | 1377 | `			/* Append raw contents */` |
|     20 | 1378 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      9 | 1379 | `		}` |
|     38 | 1380 | `		if( zIn < zEnd ){` |
|     36 | 1381 | `			int c = zIn[0];` |
|     36 | 1382 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     17 | 1383 | `		}` |
|     38 | 1384 | `		zIn++;` |
|      2 | 1385 | `	}` |
|     10 | 1386 | `	return PH7_OK;` |
|      9 | 1387 | `}` |
|      - | 1388 | `/*` |
|      - | 1389 | ` * string stripslashes(string $str)` |
|      - | 1390 | ` *  Un-quotes a quoted string.` |
|      - | 1391 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1392 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1393 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1394 | ` * Parameter` |
|      - | 1395 | ` *  $str` |
|      - | 1396 | ` *   The input string.` |
|      - | 1397 | ` * Return` |
|      - | 1398 | ` *  Returns a string with backslashes stripped off.` |
|      - | 1399 | ` */` |
|      8 | 1400 | `PH7_PRIVATE int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1401 | `{` |
|      - | 1402 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1403 | `	int nLen;` |
|     10 | 1404 | `	if( nArg < 1 ){` |
|      - | 1405 | `		/* Nothing to process,retun NULL */` |
|    ! 0 | 1406 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1407 | `		return PH7_OK;` |
|      - | 1408 | `	}` |
|      - | 1409 | `	/* Extract the string to process */` |
|     10 | 1410 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     10 | 1411 | `	if( zIn == 0 ){` |
|    ! 0 | 1412 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1413 | `		return PH7_OK;` |
|      - | 1414 | `	}` |
|     10 | 1415 | `	zEnd = &zIn[nLen];` |
|     10 | 1416 | `	zCur = 0; /* cc warning */` |
|      - | 1417 | `	/* Seed an empty string result: the loop below only ever APPENDS, so without` |
|      - | 1418 | `	 * this an empty input would leave the return value untouched and answer` |
|      - | 1419 | `	 * NULL where php answers "". */` |
|     10 | 1420 | `	ph7_result_string(pCtx,"",0);` |
|      - | 1421 | `	/* Encode the string */` |
|      5 | 1422 | `	for(;;){` |
|     12 | 1423 | `		if( zIn >= zEnd ){` |
|      - | 1424 | `			/* No more input */` |
|      6 | 1425 | `			break;` |
|      - | 1426 | `		}` |
|      7 | 1427 | `		zCur = zIn;` |
|     19 | 1428 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 | 1429 | `			zIn++;` |
|      1 | 1430 | `		}` |
|      7 | 1431 | `		if( zIn > zCur ){` |
|      - | 1432 | `			/* Append raw contents */` |
|      5 | 1433 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 1434 | `		}` |
|      7 | 1435 | `		if( &zIn[1] < zEnd ){` |
|      3 | 1436 | `			int c = zIn[1];` |
|      3 | 1437 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - | 1438 | `				/* Ignore the backslash */` |
|      3 | 1439 | `				zIn++;` |
|      1 | 1440 | `			}` |
|      2 | 1441 | `		}else{` |
|      5 | 1442 | `			break;` |
|      - | 1443 | `		}` |
|      1 | 1444 | `	}` |
|     10 | 1445 | `	return PH7_OK;` |
|      6 | 1446 | `}` |
|      - | 1447 | `/*` |
|      - | 1448 | ` * UTF-8-aware HTML entity machinery, shared by htmlspecialchars/htmlentities/` |
|      - | 1449 | ` * htmlspecialchars_decode/html_entity_decode/get_html_translation_table.` |
|      - | 1450 | ` * The implementations live further down in this file, next to the filter_var` |
|      - | 1451 | ` * FULL_SPECIAL_CHARS machinery they reuse (aHtml401Ent[]/FvHtml401Lookup()/` |
|      - | 1452 | ` * FvUtf8Next()). Semantics are byte-exact vs php 8.5.7; PHL is UTF-8-only` |
|      - | 1453 | ` * so every charset argument other than a UTF-8 alias gets PHP's` |
|      - | 1454 | ` * unsupported-charset warning and is treated as UTF-8.` |
|      - | 1455 | ` *` |
|      - | 1456 | ` * Flag model (the PHP-exact ENT_* values, see constant.c): bit 1 = encode/` |
|      - | 1457 | ` * decode single quotes, bit 2 = double quotes (ENT_QUOTES=3, ENT_COMPAT=2,` |
|      - | 1458 | ` * ENT_NOQUOTES=0); bits 16\|32 select the doctype (0=HTML401, 16=XML1,` |
|      - | 1459 | ` * 32=XHTML, 48=HTML5); ENT_IGNORE=4 drops invalid UTF-8 bytes (wins over` |
|      - | 1460 | ` * ENT_SUBSTITUTE=8, which replaces each with U+FFFD; with neither set the` |
|      - | 1461 | ` * whole result collapses to ""); ENT_DISALLOWED=128 substitutes valid but` |
|      - | 1462 | ` * doctype-disallowed codepoints. The shared default is` |
|      - | 1463 | ` * ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 = 11.` |
|      - | 1464 | ` */` |
|      - | 1465 | `/*` |
|      - | 1466 | ` * string htmlspecialchars(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1467 | ` *                         [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1468 | ` *  Convert the special characters & < > " ' to HTML entities.` |
|      - | 1469 | ` * Return` |
|      - | 1470 | ` *  The escaped string or NULL on failure.` |
|      - | 1471 | ` */` |
|     42 | 1472 | `PH7_PRIVATE int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1473 | `{` |
|     43 | 1474 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1475 | `	const char *zIn;` |
|     43 | 1476 | `	int nLen,bDouble = 1;` |
|      - | 1477 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1478 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     43 | 1479 | `	if( nArg < 1 ){` |
|      - | 1480 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1481 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1482 | `		return PH7_OK;` |
|      - | 1483 | `	}` |
|      - | 1484 | `	/* Extract the target string */` |
|     43 | 1485 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     43 | 1486 | `	if( nArg > 1 ){` |
|     35 | 1487 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     17 | 1488 | `	}` |
|     43 | 1489 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     43 | 1490 | `	if( nArg > 3 ){` |
|      7 | 1491 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      3 | 1492 | `	}` |
|     43 | 1493 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,0,bDouble);` |
|     43 | 1494 | `	return PH7_OK;` |
|     22 | 1495 | `}` |
|      - | 1496 | `/*` |
|      - | 1497 | ` * string htmlspecialchars_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401])` |
|      - | 1498 | ` *  Convert the special HTML entities (&amp; &lt; &gt; &quot; and the` |
|      - | 1499 | ` *  numeric/doctype forms of the two quotes) back to characters.` |
|      - | 1500 | ` * Return` |
|      - | 1501 | ` *  The unescaped string or NULL on failure.` |
|      - | 1502 | ` */` |
|     22 | 1503 | `PH7_PRIVATE int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1504 | `{` |
|     23 | 1505 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1506 | `	const char *zIn;` |
|      - | 1507 | `	int nLen;` |
|      - | 1508 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1509 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     23 | 1510 | `	if( nArg < 1 ){` |
|      - | 1511 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1512 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1513 | `		return PH7_OK;` |
|      - | 1514 | `	}` |
|      - | 1515 | `	/* Extract the target string */` |
|     23 | 1516 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 1517 | `	if( nArg > 1 ){` |
|      9 | 1518 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1519 | `	}` |
|     23 | 1520 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,0);` |
|     23 | 1521 | `	return PH7_OK;` |
|     12 | 1522 | `}` |
|      - | 1523 | `/*` |
|      - | 1524 | ` * array get_html_translation_table(int $table = HTML_SPECIALCHARS` |
|      - | 1525 | ` *      [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 [, string $encoding = "UTF-8"]])` |
|      - | 1526 | ` *  Return the translation table used by htmlspecialchars() (HTML_SPECIALCHARS)` |
|      - | 1527 | ` *  or htmlentities() (HTML_ENTITIES) as character => entity pairs.` |
|      - | 1528 | ` * Return` |
|      - | 1529 | ` *  The translation table as an array or NULL on failure.` |
|      - | 1530 | ` */` |
|     12 | 1531 | `PH7_PRIVATE int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1532 | `{` |
|     13 | 1533 | `	int iTable = 0; /* HTML_SPECIALCHARS */` |
|     13 | 1534 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|     13 | 1535 | `	if( nArg > 0 ){` |
|     11 | 1536 | `		iTable = ph7_value_to_int(apArg[0]);` |
|      5 | 1537 | `	}` |
|     13 | 1538 | `	if( nArg > 1 ){` |
|      9 | 1539 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1540 | `	}` |
|     13 | 1541 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     13 | 1542 | `	HtmlTranslationTable(pCtx,iTable,iFlags);` |
|     13 | 1543 | `	return PH7_OK;` |
|      1 | 1544 | `}` |
|      - | 1545 | `/*` |
|      - | 1546 | ` * string htmlentities(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1547 | ` *                     [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1548 | ` *  Convert all applicable characters to HTML entities: the specials plus` |
|      - | 1549 | ` *  every codepoint with an HTML 4.01 named entity (aHtml401Ent[]).` |
|      - | 1550 | ` * Return` |
|      - | 1551 | ` *  The encoded string or NULL on failure.` |
|      - | 1552 | ` */` |
|     30 | 1553 | `PH7_PRIVATE int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1554 | `{` |
|     31 | 1555 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1556 | `	const char *zIn;` |
|     31 | 1557 | `	int nLen,bDouble = 1;` |
|      - | 1558 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1559 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     31 | 1560 | `	if( nArg < 1 ){` |
|      - | 1561 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1562 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1563 | `		return PH7_OK;` |
|      - | 1564 | `	}` |
|      - | 1565 | `	/* Extract the target string */` |
|     31 | 1566 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 1567 | `	if( nArg > 1 ){` |
|     19 | 1568 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1569 | `	}` |
|     31 | 1570 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     31 | 1571 | `	if( nArg > 3 ){` |
|      3 | 1572 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      1 | 1573 | `	}` |
|     31 | 1574 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,1,bDouble);` |
|     31 | 1575 | `	return PH7_OK;` |
|     16 | 1576 | `}` |
|      - | 1577 | `/*` |
|      - | 1578 | ` * string html_entity_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1579 | ` *                           [, string $encoding = "UTF-8"]])` |
|      - | 1580 | ` *  Convert HTML entities (named — case-sensitive — and numeric, decimal or` |
|      - | 1581 | ` *  hex) back to their UTF-8 characters. The reverse of htmlentities().` |
|      - | 1582 | ` * Return` |
|      - | 1583 | ` *  The decoded string or NULL on failure.` |
|      - | 1584 | ` */` |
|     58 | 1585 | `PH7_PRIVATE int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1586 | `{` |
|     59 | 1587 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1588 | `	const char *zIn;` |
|      - | 1589 | `	int nLen;` |
|      - | 1590 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|      - | 1591 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|     59 | 1592 | `	if( nArg < 1 ){` |
|      - | 1593 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1594 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1595 | `		return PH7_OK;` |
|      - | 1596 | `	}` |
|      - | 1597 | `	/* Extract the target string */` |
|     59 | 1598 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     59 | 1599 | `	if( nArg > 1 ){` |
|     27 | 1600 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     13 | 1601 | `	}` |
|     59 | 1602 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     59 | 1603 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,1);` |
|     59 | 1604 | `	return PH7_OK;` |
|     30 | 1605 | `}` |
|      - | 1606 | `/*` |
|      - | 1607 | ` * int strlen($string)` |
|      - | 1608 | ` *  return the length of the given string.` |
|      - | 1609 | ` * Parameter` |
|      - | 1610 | ` *  string: The string being measured for length.` |
|      - | 1611 | ` * Return` |
|      - | 1612 | ` *  length of the given string.` |
|      - | 1613 | ` */` |
| 142928 | 1614 | `PH7_PRIVATE int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1615 | `{` |
| 142933 | 1616 | `	int iLen = 0;` |
| 142933 | 1617 | `	if( nArg > 0 ){` |
| 142933 | 1618 | `		StrNullArgNotice(pCtx,apArg[0],"strlen",1,"$string");` |
| 142933 | 1619 | `		ph7_value_to_string(apArg[0],&iLen);` |
|  71860 | 1620 | `	}` |
|      - | 1621 | `	/* String length */` |
| 142933 | 1622 | `	ph7_result_int(pCtx,iLen);` |
| 142933 | 1623 | `	return PH7_OK;` |
|      5 | 1624 | `}` |
|      - | 1625 | `/*` |
|      - | 1626 | ` * int strcmp(string $str1,string $str2)` |
|      - | 1627 | ` *  Perform a binary safe string comparison.` |
|      - | 1628 | ` * Parameter` |
|      - | 1629 | ` *  str1: The first string` |
|      - | 1630 | ` *  str2: The second string` |
|      - | 1631 | ` * Return` |
|      - | 1632 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1633 | ` *  than str2, and 0 if they are equal.` |
|      - | 1634 | ` */` |
|     86 | 1635 | `PH7_PRIVATE int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1636 | `{` |
|      - | 1637 | `	const char *z1,*z2;` |
|      - | 1638 | `	int n1,n2;` |
|      - | 1639 | `	int res;` |
|     89 | 1640 | `	if( nArg < 2 ){` |
|    ! 0 | 1641 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 1642 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 1643 | `		return PH7_OK;` |
|      - | 1644 | `	}` |
|      - | 1645 | `	/* Perform the comparison */` |
|     89 | 1646 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     89 | 1647 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     89 | 1648 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1649 | `	/* Comparison result */` |
|     89 | 1650 | `	ph7_result_int(pCtx,res);` |
|     89 | 1651 | `	return PH7_OK;` |
|     46 | 1652 | `}` |
|      - | 1653 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - | 1654 | `/*` |
|      - | 1655 | ` * The natural-order comparison core lives OUTSIDE the PH7_DISABLE_BUILTIN_FUNC` |
|      - | 1656 | ` * guard: hashmap.c's SORT_NATURAL path (always compiled) calls PH7_StrNatCmp, so` |
|      - | 1657 | ` * it must exist in the tiny build too. [[tiny-build-disk-io-guard-fragility]]` |
|      - | 1658 | ` */` |
|      - | 1659 | `/*` |
|      - | 1660 | ` * Natural-order comparison core (Martin Pool's natcompare as adapted by php's` |
|      - | 1661 | ` * ext/standard/strnatcmp.c): digit runs compare numerically — the longer run` |
|      - | 1662 | ` * wins, a leading zero flips to fractional first-difference-wins semantics —` |
|      - | 1663 | ` * everything else compares bytewise with whitespace skipped.` |
|      - | 1664 | ` */` |
|    150 | 1665 | `static int StrNatCompareRight(const char **pa,const char *aEnd,const char **pb,const char *bEnd)` |
|      3 | 1666 | `{` |
|    153 | 1667 | `	int bias = 0;` |
|    245 | 1668 | `	for(;;){` |
|    323 | 1669 | `		int da = (*pa < aEnd) && SyisDigit(**pa);` |
|    323 | 1670 | `		int db = (*pb < bEnd) && SyisDigit(**pb);` |
|    323 | 1671 | `		if( !da && !db ){ return bias; }` |
|    259 | 1672 | `		if( !da ){ return -1; }` |
|    229 | 1673 | `		if( !db ){ return 1; }` |
|    173 | 1674 | `		if( **pa < **pb ){ if( !bias ){ bias = -1; } }` |
|    107 | 1675 | `		else if( **pa > **pb ){ if( !bias ){ bias = 1; } }` |
|    173 | 1676 | `		(*pa)++;` |
|    173 | 1677 | `		(*pb)++;` |
|      3 | 1678 | `	}` |
|     78 | 1679 | `}` |
|      4 | 1680 | `static int StrNatCompareLeft(const char **pa,const char *aEnd,const char **pb,const char *bEnd)` |
|      1 | 1681 | `{` |
|      2 | 1682 | `	for(;;){` |
|      5 | 1683 | `		int da = (*pa < aEnd) && SyisDigit(**pa);` |
|      5 | 1684 | `		int db = (*pb < bEnd) && SyisDigit(**pb);` |
|      5 | 1685 | `		if( !da && !db ){ return 0; }` |
|      5 | 1686 | `		if( !da ){ return -1; }` |
|      5 | 1687 | `		if( !db ){ return 1; }` |
|      5 | 1688 | `		if( **pa < **pb ){ return -1; }` |
|    ! 0 | 1689 | `		if( **pa > **pb ){ return 1; }` |
|    ! 0 | 1690 | `		(*pa)++;` |
|    ! 0 | 1691 | `		(*pb)++;` |
|    ! 0 | 1692 | `	}` |
|      3 | 1693 | `}` |
|    232 | 1694 | `PH7_PRIVATE int PH7_StrNatCmp(const char *zA,int nA,const char *zB,int nB,int bFold)` |
|      3 | 1695 | `{` |
|    235 | 1696 | `	const char *a = zA,*aEnd = &zA[nA];` |
|    235 | 1697 | `	const char *b = zB,*bEnd = &zB[nB];` |
|    586 | 1698 | `	for(;;){` |
|      - | 1699 | `		int ca,cb;` |
|    713 | 1700 | `		while( a < aEnd && SyisSpace(a[0]) ){ a++; }` |
|    711 | 1701 | `		while( b < bEnd && SyisSpace(b[0]) ){ b++; }` |
|    711 | 1702 | `		ca = (a < aEnd) ? (unsigned char)a[0] : 0;` |
|    711 | 1703 | `		cb = (b < bEnd) ? (unsigned char)b[0] : 0;` |
|    711 | 1704 | `		if( SyisDigit(ca) && SyisDigit(cb) ){` |
|    155 | 1705 | `			int r = (ca == '0' \|\| cb == '0')` |
|      4 | 1706 | `				? StrNatCompareLeft(&a,aEnd,&b,bEnd)` |
|    227 | 1707 | `				: StrNatCompareRight(&a,aEnd,&b,bEnd);` |
|    157 | 1708 | `			if( r ){ return r; }` |
|     14 | 1709 | `			continue;` |
|      - | 1710 | `		}` |
|    557 | 1711 | `		if( ca == 0 && cb == 0 ){ return 0; }` |
|    535 | 1712 | `		if( bFold ){` |
|    249 | 1713 | `			ca = SyToLower(ca);` |
|    249 | 1714 | `			cb = SyToLower(cb);` |
|    123 | 1715 | `		}` |
|    535 | 1716 | `		if( ca < cb ){ return -1; }` |
|    487 | 1717 | `		if( ca > cb ){ return 1; }` |
|    467 | 1718 | `		a++;` |
|    467 | 1719 | `		b++;` |
|      3 | 1720 | `	}` |
|    119 | 1721 | `}` |
|      - | 1722 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 1723 | `/*` |
|      - | 1724 | ` * int strnatcmp(string $string1, string $string2)` |
|      - | 1725 | ` * int strnatcasecmp(string $string1, string $string2)` |
|      - | 1726 | ` *  Natural-order string comparison ("img2" < "img10"), case folded for the` |
|      - | 1727 | ` *  latter. php 8.2+ normalizes the result to -1/0/1.` |
|      - | 1728 | ` */` |
|     62 | 1729 | `PH7_PRIVATE int PH7_builtin_strnatcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1730 | `{` |
|      - | 1731 | `	const char *z1,*z2,*zFunc;` |
|      - | 1732 | `	int n1,n2,bFold;` |
|     64 | 1733 | `	if( nArg < 2 ){` |
|    ! 0 | 1734 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 1735 | `		return PH7_OK;` |
|      - | 1736 | `	}` |
|     64 | 1737 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 1738 | `	/* Both names carry a 'c' at that offset -- "strnat\|c\|mp" as much as` |
|      - | 1739 | `	 * "strnat\|c\|asecmp" -- so testing it alone made strnatcmp() fold as well, and` |
|      - | 1740 | `	 * natsort()/ArrayObject::natsort() (prelude wrappers over strnatcmp) with it:` |
|      - | 1741 | `	 * 'Hello' and 'hello' compared EQUAL where php answers -1. */` |
|     64 | 1742 | `	bFold = SyStrnicmp(zFunc,"strnatcase",sizeof("strnatcase")-1) == 0;` |
|     64 | 1743 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     64 | 1744 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     64 | 1745 | `	ph7_result_int(pCtx,PH7_StrNatCmp(z1,n1,z2,n2,bFold));` |
|     64 | 1746 | `	return PH7_OK;` |
|     33 | 1747 | `}` |
|      - | 1748 | `/*` |
|      - | 1749 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 1750 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 1751 | ` * Parameter` |
|      - | 1752 | ` *  str1: The first string` |
|      - | 1753 | ` *  str2: The second string` |
|      - | 1754 | ` * Return` |
|      - | 1755 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1756 | ` *  than str2, and 0 if they are equal.` |
|      - | 1757 | ` */` |
|    388 | 1758 | `PH7_PRIVATE int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1759 | `{` |
|      - | 1760 | `	const char *z1,*z2;` |
|      - | 1761 | `	int res;` |
|      - | 1762 | `	int n;` |
|    391 | 1763 | `	if( nArg < 3 ){` |
|      - | 1764 | `		/* Perform a standard comparison */` |
|    ! 0 | 1765 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 1766 | `	}` |
|      - | 1767 | `	/* Desired comparison length */` |
|    391 | 1768 | `	n  = ph7_value_to_int(apArg[2]);` |
|    391 | 1769 | `	if( n < 0 ){` |
|      - | 1770 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 1771 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1772 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 1773 | `			ph7_function_name(pCtx));` |
|      - | 1774 | `	}` |
|      - | 1775 | `	/* Perform the comparison */` |
|    389 | 1776 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|    389 | 1777 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|    389 | 1778 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 1779 | `	/* Comparison result */` |
|    389 | 1780 | `	ph7_result_int(pCtx,res);` |
|    389 | 1781 | `	return PH7_OK;` |
|    197 | 1782 | `}` |
|      - | 1783 | `/*` |
|      - | 1784 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 1785 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 1786 | ` * Parameter` |
|      - | 1787 | ` *  str1: The first string` |
|      - | 1788 | ` *  str2: The second string` |
|      - | 1789 | ` * Return` |
|      - | 1790 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1791 | ` *  than str2, and 0 if they are equal.` |
|      - | 1792 | ` */` |
|    158 | 1793 | `PH7_PRIVATE int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1794 | `{` |
|      - | 1795 | `	const char *z1,*z2;` |
|      - | 1796 | `	int n1,n2;` |
|      - | 1797 | `	int res;` |
|    160 | 1798 | `	if( nArg < 2 ){` |
|    ! 0 | 1799 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 1800 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 1801 | `		return PH7_OK;` |
|      - | 1802 | `	}` |
|      - | 1803 | `	/* Perform the comparison */` |
|    160 | 1804 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|    160 | 1805 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|    160 | 1806 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1807 | `	/* Comparison result */` |
|    160 | 1808 | `	ph7_result_int(pCtx,res);` |
|    160 | 1809 | `	return PH7_OK;` |
|     81 | 1810 | `}` |
|      - | 1811 | `/*` |
|      - | 1812 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 1813 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 1814 | ` * Parameter` |
|      - | 1815 | ` *  $str1: The first string` |
|      - | 1816 | ` *  $str2: The second string` |
|      - | 1817 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 1818 | ` * Return` |
|      - | 1819 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1820 | ` *  than str2, and 0 if they are equal.` |
|      - | 1821 | ` */` |
|     76 | 1822 | `PH7_PRIVATE int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1823 | `{` |
|      - | 1824 | `	const char *z1,*z2;` |
|      - | 1825 | `	int res;` |
|      - | 1826 | `	int n;` |
|     81 | 1827 | `	if( nArg < 3 ){` |
|      - | 1828 | `		/* Perform a standard comparison */` |
|    ! 0 | 1829 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 1830 | `	}` |
|      - | 1831 | `	/* Desired comparison length */` |
|     81 | 1832 | `	n  = ph7_value_to_int(apArg[2]);` |
|     81 | 1833 | `	if( n < 0 ){` |
|      - | 1834 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 1835 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1836 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 1837 | `			ph7_function_name(pCtx));` |
|      - | 1838 | `	}` |
|      - | 1839 | `	/* Perform the comparison */` |
|     79 | 1840 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     79 | 1841 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     79 | 1842 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 1843 | `	/* Comparison result */` |
|     79 | 1844 | `	ph7_result_int(pCtx,res);` |
|     79 | 1845 | `	return PH7_OK;` |
|     43 | 1846 | `}` |
|      - | 1847 | `/*` |
|      - | 1848 | ` * Implode context [i.e: it's private data].` |
|      - | 1849 | ` * A pointer to the following structure is forwarded` |
|      - | 1850 | ` * verbatim to the array walker callback defined below.` |
|      - | 1851 | ` */` |
|      - | 1852 | `struct implode_data {` |
|      - | 1853 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 1854 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 1855 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 1856 | `	int nSeplen;          /* Separator length */` |
|      - | 1857 | `	int bFirst;           /* TRUE if first call */` |
|      - | 1858 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 1859 | `	sxi32 rc;             /* Captured allocation rc; SXERR_MEM => the builtin raises an OOM fatal */` |
|      - | 1860 | `	sxi32 rcThrow;        /* Captured coercion throw; the builtin propagates it instead of a result */` |
|      - | 1861 | `};` |
|      - | 1862 | `/*` |
|      - | 1863 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 1864 | ` * The following routine is invoked for each array entry passed` |
|      - | 1865 | ` * to the implode() function.` |
|      - | 1866 | ` */` |
| 191868 | 1867 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 1868 | `{` |
|  95934 | 1869 | `	SXUNUSED(pKey);` |
| 191873 | 1870 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 1871 | `	const char *zData;` |
|      - | 1872 | `	int nLen;` |
| 191873 | 1873 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 1874 | `		if( pData->nSeplen > 0 ){` |
|      3 | 1875 | `			if( !pData->bFirst ){` |
|      - | 1876 | `				/* append the separator first */` |
|      3 | 1877 | `				if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 1878 | `					pData->rc = SXERR_MEM;` |
|    ! 0 | 1879 | `					return PH7_ABORT;` |
|      - | 1880 | `				}` |
|      2 | 1881 | `			}else{` |
|    ! 0 | 1882 | `				pData->bFirst = 0;` |
|      - | 1883 | `			}` |
|      1 | 1884 | `		}` |
|      - | 1885 | `		/* Recurse */` |
|      3 | 1886 | `		pData->bFirst = 1;` |
|      3 | 1887 | `		pData->nRecCount++;` |
|      3 | 1888 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 1889 | `		pData->nRecCount--;` |
|      - | 1890 | `		/* Propagate an allocation failure surfaced deeper in the recursion. */` |
|      3 | 1891 | `		if( pData->rc != SXRET_OK ){` |
|    ! 0 | 1892 | `			return PH7_ABORT;` |
|      - | 1893 | `		}` |
|      3 | 1894 | `		return PH7_OK;` |
|      - | 1895 | `	}` |
|      - | 1896 | `	/* Extract the string representation of the entry value, USER-VISIBLY: an` |
|      - | 1897 | `	 * element that is itself an array renders as "Array" and warns, and one that` |
|      - | 1898 | `	 * is an object with no __toString() is php's catchable Error (it used to` |
|      - | 1899 | `	 * render as the literal "Object", §2). The walk cannot return a status, so` |
|      - | 1900 | `	 * park it on the context struct and abort. */` |
|      - | 1901 | `	{` |
| 191871 | 1902 | `		sxi32 rcSv = PH7_ValueToStringUV(pData->pCtx,pValue,&zData,&nLen);` |
| 191871 | 1903 | `		if( rcSv != SXRET_OK ){` |
|     18 | 1904 | `			pData->rcThrow = rcSv;` |
|     18 | 1905 | `			return PH7_ABORT;` |
|      - | 1906 | `		}` |
|      - | 1907 | `	}` |
|      - | 1908 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 191855 | 1909 | `	if( pData->bFirst ){` |
|  36707 | 1910 | `		pData->bFirst = 0;` |
| 173504 | 1911 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 1912 | `		/* append the separator first */` |
| 154549 | 1913 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 1914 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1915 | `			return PH7_ABORT;` |
|      - | 1916 | `		}` |
|  77272 | 1917 | `	}` |
|      - | 1918 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 191855 | 1919 | `	if( nLen > 0 ){` |
| 177277 | 1920 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1921 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1922 | `			return PH7_ABORT;` |
|      - | 1923 | `		}` |
|  88636 | 1924 | `	}` |
| 191855 | 1925 | `	return PH7_OK;` |
|  95939 | 1926 | `}` |
|      - | 1927 | `/*` |
|      - | 1928 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 1929 | ` * string implode(array $pieces,...)` |
|      - | 1930 | ` *  Join array elements with a string.` |
|      - | 1931 | ` * $glue` |
|      - | 1932 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 1933 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 1934 | ` * $pieces` |
|      - | 1935 | ` *   The array of strings to implode.` |
|      - | 1936 | ` * Return` |
|      - | 1937 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 1938 | ` *  order, with the glue string between each element.` |
|      - | 1939 | ` */` |
|  36836 | 1940 | `PH7_PRIVATE int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1941 | `{` |
|      - | 1942 | `	struct implode_data imp_data;` |
|      - | 1943 | `	/*` |
|      - | 1944 | `` 	 * php's contract for implode()/join(): one function, a `array\|string $separator` `` |
|      - | 1945 | ``	 * and a `?array $array` row, and a body that resolves the two ARITIES. The`` |
|      - | 1946 | `	 * messages report the overload as already resolved -- an $array argument in` |
|      - | 1947 | ``	 * position #2 means #1 is the SEPARATOR and must be a `string`, never the`` |
|      - | 1948 | `	 * union (which is only what the two arities accept BETWEEN them), and an ARRAY` |
|      - | 1949 | `	 * in position #1 with a second argument is that same #1 error. One aBuiltinSig` |
|      - | 1950 | `	 * row cannot say either (it drives both the accepted set and the message),` |
|      - | 1951 | `	 * which is why implode/join sit on azSelfChecked[] with strtr() and check` |
|      - | 1952 | `	 * their own rows here.` |
|      - | 1953 | `	 *` |
|      - | 1954 | ``	 * The messages name the INVOKED function: php reports `join(): ...` for`` |
|      - | 1955 | ``	 * join(), where this builtin used to hardcode `implode(): ...`.`` |
|      - | 1956 | `	 *` |
|      - | 1957 | `	 * One divergence, twin-paired in` |
|      - | 1958 | `	 * 002-integration/function/implode_separator_type{,_zend}.phpt. php 8.5 words` |
|      - | 1959 | `	 * the three cases below through a SPECIALIZED handler that only a DIRECT,` |
|      - | 1960 | ``	 * compile-time-resolved `implode(...)` call reaches; join(),`` |
|      - | 1961 | ``	 * `$f='implode'; $f(...)` and call_user_func('implode', ...) fall back to a`` |
|      - | 1962 | ``	 * generic path that answers differently (`array\|string` for the first, a #2`` |
|      - | 1963 | `	 * error for the second, and "ab" for the third). It is a call-FORM` |
|      - | 1964 | `	 * specialization, not a semantic rule -- it does not change with opcache off` |
|      - | 1965 | `	 * -- so PHL gives every call form the one contract php's direct calls use,` |
|      - | 1966 | `	 * which is the form real code writes and the form the corpus pins.` |
|      - | 1967 | `	 */` |
|  36841 | 1968 | `	const char *zName = ph7_function_name(pCtx);` |
|  36841 | 1969 | `	int i = 1;` |
|  36841 | 1970 | `	if( nArg < 1 ){` |
|      - | 1971 | `		/* Missing argument,return NULL */` |
|    ! 0 | 1972 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1973 | `		return PH7_OK;` |
|      - | 1974 | `	}` |
|      - | 1975 | `	/* Prepare the implode context */` |
|  36841 | 1976 | `	imp_data.pCtx = pCtx;` |
|  36841 | 1977 | `	imp_data.bRecursive = 0;` |
|  36841 | 1978 | `	imp_data.bFirst = 1;` |
|  36841 | 1979 | `	imp_data.nRecCount = 0;` |
|  36841 | 1980 | `	imp_data.rc = SXRET_OK;` |
|  36841 | 1981 | `	imp_data.rcThrow = SXRET_OK;` |
|  36841 | 1982 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  36819 | 1983 | `		if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|      - | 1984 | `			/* php only DEPRECATES null for the union parameter and coerces it to` |
|      - | 1985 | `			 * ""; PHL rejects it (§10 null-strictness), naming php's DECLARED type` |
|      - | 1986 | ``			 * -- the one case where `array\|string` is the right wording, because`` |
|      - | 1987 | `			 * php never narrows the union for a value it accepts. */` |
|    ! 0 | 1988 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    ! 0 | 1989 | `				"%s(): Argument #1 ($separator) must be of type array\|string, null given",zName);` |
|      - | 1990 | `		}` |
|  36819 | 1991 | `		if( nArg < 2 \|\| ph7_value_is_null(apArg[1]) ){` |
|      - | 1992 | ``			/* php: a string separator REQUIRES the array. `implode("x")` and`` |
|      - | 1993 | ``			 * `implode("x", null)` both answered "" -- the `?array` in php's`` |
|      - | 1994 | `			 * signature is the DEFAULT's type, not a value it accepts. */` |
|     17 | 1995 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1996 | `				"%s(): If argument #1 ($separator) is of type string, "` |
|      5 | 1997 | `				"argument #2 ($array) must be of type array, null given",zName);` |
|      - | 1998 | `		}` |
|  36809 | 1999 | `		if( !PH7_ArgSatisfiesString(apArg[0]) ){` |
|      - | 2000 | `			/* The overload is resolved, so #1 is the separator and must be a` |
|      - | 2001 | `			 * STRING. PHL used to fall through to the central screen here and` |
|      - | 2002 | ``			 * report the whole `array\|string` union instead. */`` |
|      - | 2003 | `			char zBuf[64];` |
|     10 | 2004 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 2005 | `				"%s(): Argument #1 ($separator) must be of type string, %s given",` |
|      3 | 2006 | `				zName,VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|      - | 2007 | `		}` |
|  36803 | 2008 | `		if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 2009 | `			/* php: implode($glue, $pieces) requires an ARRAY. PH7 stringified` |
|      - | 2010 | `			 * whatever it was handed, so implode(",", 5) quietly returned "5". */` |
|      - | 2011 | `			char zBuf[64];` |
|     12 | 2012 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 2013 | `				"%s(): Argument #2 ($array) must be of type ?array, %s given",` |
|      6 | 2014 | `				zName,VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 2015 | `		}` |
|  36797 | 2016 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  18401 | 2017 | `	}else{` |
|     25 | 2018 | `		if( nArg > 1 ){` |
|      - | 2019 | `			/* php 8 removed the legacy swapped order: implode($pieces, $glue) is a` |
|      - | 2020 | `			 * TypeError whatever $glue holds (PHL used to swap silently, a wrong` |
|      - | 2021 | `			 * ANSWER when the caller meant php's signature). One array argument` |
|      - | 2022 | `			 * alone stays the legal ""-glue form. */` |
|     26 | 2023 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      8 | 2024 | `				"%s(): Argument #1 ($separator) must be of type string, array given",zName);` |
|      - | 2025 | `		}` |
|      9 | 2026 | `		imp_data.zSep = 0;` |
|      9 | 2027 | `		imp_data.nSeplen = 0;` |
|      9 | 2028 | `		i = 0;` |
|      - | 2029 | `	}` |
|  36803 | 2030 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2031 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2032 | `	}` |
|      - | 2033 | `	/* Start the 'join' process */` |
|  73585 | 2034 | `	while( i < nArg ){` |
|  36803 | 2035 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2036 | `			/* Iterate throw array entries */` |
|  36803 | 2037 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2038 | `			/* An element whose coercion threw ends the join with that throw */` |
|  36803 | 2039 | `			if( imp_data.rcThrow != SXRET_OK ){` |
|     18 | 2040 | `				return imp_data.rcThrow;` |
|      - | 2041 | `			}` |
|      - | 2042 | `			/* Surface a callback allocation failure as a fatal */` |
|  36787 | 2043 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2044 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2045 | `			}` |
|  18396 | 2046 | `		}else{` |
|      - | 2047 | `			const char *zData;` |
|      - | 2048 | `			int nLen;` |
|      - | 2049 | `			/* Extract the string representation of the ph7 value (user-visible) */` |
|    ! 0 | 2050 | `			sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[i],&zData,&nLen);` |
|    ! 0 | 2051 | `			if( rcSv != SXRET_OK ){` |
|    ! 0 | 2052 | `				return rcSv;` |
|      - | 2053 | `			}` |
|      - | 2054 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 2055 | `			if( imp_data.bFirst ){` |
|    ! 0 | 2056 | `				imp_data.bFirst = 0;` |
|    ! 0 | 2057 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2058 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2059 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2060 | `				}` |
|    ! 0 | 2061 | `			}` |
|      - | 2062 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 2063 | `			if( nLen > 0 ){` |
|    ! 0 | 2064 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2065 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2066 | `				}` |
|    ! 0 | 2067 | `			}` |
|      - | 2068 | `		}` |
|  36787 | 2069 | `		i++;` |
|      5 | 2070 | `	}` |
|  36787 | 2071 | `	return PH7_OK;` |
|  18423 | 2072 | `}` |
|      - | 2073 | `/*` |
|      - | 2074 | ` * Symisc eXtension:` |
|      - | 2075 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 2076 | ` * Purpose` |
|      - | 2077 | ` *  Same as implode() but recurse on arrays.` |
|      - | 2078 | ` * Example:` |
|      - | 2079 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 2080 | ` *   echo implode_recursive("/",$a);` |
|      - | 2081 | ` *   Will output` |
|      - | 2082 | ` *     usr/home/dean.` |
|      - | 2083 | ` *   While the standard implode would produce.` |
|      - | 2084 | ` *    usr/Array.` |
|      - | 2085 | ` * Parameter` |
|      - | 2086 | ` *  Refer to implode().` |
|      - | 2087 | ` * Return` |
|      - | 2088 | ` *  Refer to implode().` |
|      - | 2089 | ` */` |
|     12 | 2090 | `PH7_PRIVATE int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2091 | `{` |
|      - | 2092 | `	struct implode_data imp_data;` |
|     13 | 2093 | `	int i = 1;` |
|     13 | 2094 | `	if( nArg < 1 ){` |
|      - | 2095 | `		/* Missing argument,return NULL */` |
|      3 | 2096 | `		ph7_result_null(pCtx);` |
|      3 | 2097 | `		return PH7_OK;` |
|      - | 2098 | `	}` |
|      - | 2099 | `	/* Prepare the implode context */` |
|     11 | 2100 | `	imp_data.pCtx = pCtx;` |
|     11 | 2101 | `	imp_data.bRecursive = 1;` |
|     11 | 2102 | `	imp_data.bFirst = 1;` |
|     11 | 2103 | `	imp_data.nRecCount = 0;` |
|     11 | 2104 | `	imp_data.rc = SXRET_OK;` |
|     11 | 2105 | `	imp_data.rcThrow = SXRET_OK;` |
|     11 | 2106 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 2107 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 2108 | `	}else{` |
|    ! 0 | 2109 | `		imp_data.zSep = 0;` |
|    ! 0 | 2110 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 2111 | `		i = 0;` |
|      - | 2112 | `	}` |
|     11 | 2113 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2114 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2115 | `	}` |
|      - | 2116 | `	/* Start the 'join' process */` |
|     21 | 2117 | `	while( i < nArg ){` |
|     11 | 2118 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2119 | `			/* Iterate throw array entries */` |
|      3 | 2120 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2121 | `			/* An element whose coercion threw ends the join with that throw */` |
|      3 | 2122 | `			if( imp_data.rcThrow != SXRET_OK ){` |
|    ! 0 | 2123 | `				return imp_data.rcThrow;` |
|      - | 2124 | `			}` |
|      - | 2125 | `			/* Surface a callback allocation failure as a fatal */` |
|      3 | 2126 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2127 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2128 | `			}` |
|      2 | 2129 | `		}else{` |
|      - | 2130 | `			const char *zData;` |
|      - | 2131 | `			int nLen;` |
|      - | 2132 | `			/* Extract the string representation of the ph7 value (user-visible) */` |
|      9 | 2133 | `			sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[i],&zData,&nLen);` |
|      9 | 2134 | `			if( rcSv != SXRET_OK ){` |
|    ! 0 | 2135 | `				return rcSv;` |
|      - | 2136 | `			}` |
|      - | 2137 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 2138 | `			if( imp_data.bFirst ){` |
|      9 | 2139 | `				imp_data.bFirst = 0;` |
|      4 | 2140 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2141 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2142 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2143 | `				}` |
|    ! 0 | 2144 | `			}` |
|      - | 2145 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 2146 | `			if( nLen > 0 ){` |
|      9 | 2147 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2148 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2149 | `				}` |
|      4 | 2150 | `			}` |
|      - | 2151 | `		}` |
|     11 | 2152 | `		i++;` |
|      1 | 2153 | `	}` |
|     11 | 2154 | `	return PH7_OK;` |
|      7 | 2155 | `}` |
|      - | 2156 | `/*` |
|      - | 2157 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 2158 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 2159 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 2160 | ` * Parameters` |
|      - | 2161 | ` *  $delimiter` |
|      - | 2162 | ` *   The boundary string.` |
|      - | 2163 | ` * $string` |
|      - | 2164 | ` *   The input string.` |
|      - | 2165 | ` * $limit` |
|      - | 2166 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 2167 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 2168 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 2169 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 2170 | ` * Returns` |
|      - | 2171 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 2172 | ` *  on boundaries formed by the delimiter.` |
|      - | 2173 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 2174 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 2175 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 2176 | ` *  will be returned.` |
|      - | 2177 | ` * NOTE:` |
|      - | 2178 | ` *  Negative limit is not supported.` |
|      - | 2179 | ` */` |
|   7642 | 2180 | `PH7_PRIVATE int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2181 | `{` |
|      - | 2182 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2183 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2184 | `	ph7_value *pArray;` |
|      - | 2185 | `	ph7_value *pValue;` |
|      - | 2186 | `	sxu32 nOfft;` |
|      - | 2187 | `	sxi32 rc;` |
|   7647 | 2188 | `	if( nArg < 2 ){` |
|      - | 2189 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2190 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2191 | `		return PH7_OK;` |
|      - | 2192 | `	}` |
|      - | 2193 | `	/* Extract the delimiter */` |
|   7647 | 2194 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   7647 | 2195 | `	if( nDelim < 1 ){` |
|      - | 2196 | `		/* Empty delimiter: PHP 8 throws a catchable ValueError. */` |
|      5 | 2197 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2198 | `			"explode(): Argument #1 ($separator) must not be empty");` |
|      - | 2199 | `	}` |
|      - | 2200 | `	/* Extract the string */` |
|   7643 | 2201 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   7643 | 2202 | `	if( nStrlen < 1 ){` |
|      - | 2203 | `		/* Empty string: normally an array with a single empty element (PHP behavior).` |
|      - | 2204 | `		 * A negative limit drops the last -limit components, so the sole empty` |
|      - | 2205 | `		 * component is dropped and the result is an empty array. */` |
|     13 | 2206 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|     13 | 2207 | `		if( pArrayTmp == 0 ){` |
|      - | 2208 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 2209 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2210 | `			return PH7_OK;` |
|      - | 2211 | `		}` |
|     13 | 2212 | `		if( !(nArg > 2 && ph7_value_to_int(apArg[2]) < 0) ){` |
|     11 | 2213 | `			ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|     11 | 2214 | `			if( pValueTmp == 0 ){` |
|      - | 2215 | `				/* Out of memory,return FALSE */` |
|    ! 0 | 2216 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 2217 | `				return PH7_OK;` |
|      - | 2218 | `			}` |
|     11 | 2219 | `			ph7_value_string(pValueTmp, "", 0);` |
|     11 | 2220 | `			if( ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp) != SXRET_OK ){` |
|    ! 0 | 2221 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2222 | `			}` |
|      5 | 2223 | `		}` |
|     13 | 2224 | `		ph7_result_value(pCtx, pArrayTmp);` |
|     13 | 2225 | `		return PH7_OK;` |
|      - | 2226 | `	}` |
|      - | 2227 | `	/* Point to the end of the string */` |
|   7631 | 2228 | `	zEnd = &zString[nStrlen];` |
|      - | 2229 | `	/* Create the array */` |
|   7631 | 2230 | `	pArray =  ph7_context_new_array(pCtx);` |
|   7631 | 2231 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   7631 | 2232 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2233 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2234 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2235 | `		return PH7_OK;` |
|      - | 2236 | `	}` |
|      - | 2237 | `	/* Set a defualt limit */` |
|   7631 | 2238 | `	iLimit = SXI32_HIGH;` |
|   7631 | 2239 | `	if( nArg > 2 ){` |
|     91 | 2240 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     91 | 2241 | `		if( iLimit < 0 ){` |
|      - | 2242 | `			/* Negative limit: keep all components except the last -iLimit (PHP).` |
|      - | 2243 | `			 * Pre-count the components (delimiters + 1), then emit only the first` |
|      - | 2244 | `			 * nKeep CLEAN components — no trailing-remainder merge (the difference` |
|      - | 2245 | `			 * from the positive path). nKeep <= 0 drops everything -> empty array. */` |
|     17 | 2246 | `			int nTotal = 1,nKeep;` |
|     17 | 2247 | `			const char *zScan = zString;` |
|      - | 2248 | `			sxu32 nScanOfft;` |
|     57 | 2249 | `			while( SyBlobSearch(zScan,(sxu32)(zEnd - zScan),zDelim,nDelim,&nScanOfft) == SXRET_OK ){` |
|     41 | 2250 | `				nTotal++;` |
|     41 | 2251 | `				zScan = &zScan[nScanOfft + nDelim];` |
|      1 | 2252 | `			}` |
|     17 | 2253 | `			nKeep = nTotal + iLimit; /* iLimit < 0, so this is nTotal - (-iLimit) */` |
|     49 | 2254 | `			while( nKeep > (int)ph7_array_count(pArray)` |
|     39 | 2255 | `				&& SyBlobSearch(zString,(sxu32)(zEnd - zString),zDelim,nDelim,&nOfft) == SXRET_OK ){` |
|      - | 2256 | `				/* Emit the next clean component */` |
|     23 | 2257 | `				zCur = &zString[nOfft];` |
|     23 | 2258 | `				ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|     23 | 2259 | `				if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2260 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2261 | `				}` |
|     23 | 2262 | `				zString = &zCur[nDelim];` |
|     23 | 2263 | `				ph7_value_reset_string_cursor(pValue);` |
|      1 | 2264 | `			}` |
|     17 | 2265 | `			ph7_result_value(pCtx,pArray);` |
|     17 | 2266 | `			return PH7_OK;` |
|      - | 2267 | `		}` |
|     75 | 2268 | `		if( iLimit == 0 ){` |
|      5 | 2269 | `			iLimit = 1;` |
|      2 | 2270 | `		}` |
|     75 | 2271 | `		iLimit--;` |
|     35 | 2272 | `	}` |
|      - | 2273 | `	/* Start exploding */` |
| 101520 | 2274 | `	for(;;){` |
| 203045 | 2275 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 203045 | 2276 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2277 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   7615 | 2278 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   7615 | 2279 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2280 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2281 | `			}` |
|   7615 | 2282 | `			break;` |
|      - | 2283 | `		}` |
|      - | 2284 | `		/* Point to the desired offset */` |
| 195435 | 2285 | `		zCur = &zString[nOfft];` |
|      - | 2286 | `		/* Perform the store operation (may be empty) */` |
| 195435 | 2287 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 195435 | 2288 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2289 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2290 | `		}` |
|      - | 2291 | `		/* Point beyond the delimiter */` |
| 195435 | 2292 | `		zString = &zCur[nDelim];` |
|      - | 2293 | `		/* Reset the cursor */` |
| 195435 | 2294 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 2295 | `	}` |
|      - | 2296 | `	/* Return the freshly created array */` |
|   7615 | 2297 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2298 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2299 | `	 * released as soon we return from this foregin function.` |
|      - | 2300 | `	 */` |
|   7615 | 2301 | `	return PH7_OK;` |
|   3826 | 2302 | `}` |
|      - | 2303 | `/*` |
|      - | 2304 | ` * string trim(string $str[,string $charlist ])` |
|      - | 2305 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2306 | ` * Parameters` |
|      - | 2307 | ` *  $str` |
|      - | 2308 | ` *   The string that will be trimmed.` |
|      - | 2309 | ` * $charlist` |
|      - | 2310 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2311 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2312 | ` *   With .. you can specify a range of characters.` |
|      - | 2313 | ` * Returns.` |
|      - | 2314 | ` *  Thr processed string.` |
|      - | 2315 | ` * NOTE:` |
|      - | 2316 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2317 | ` */` |
|  22682 | 2318 | `PH7_PRIVATE int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2319 | `{` |
|  22687 | 2320 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"trim",1,"$string"); }` |
|      - | 2321 | `	const char *zString;` |
|      - | 2322 | `	int nLen;` |
|  22687 | 2323 | `	if( nArg < 1 ){` |
|      - | 2324 | `		/* Missing arguments,return null */` |
|    ! 0 | 2325 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2326 | `		return PH7_OK;` |
|      - | 2327 | `	}` |
|      - | 2328 | `	/* Extract the target string */` |
|  22687 | 2329 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  22687 | 2330 | `	if( nLen < 1 ){` |
|      - | 2331 | `		/* Empty string,return */` |
|   7523 | 2332 | `		ph7_result_string(pCtx,"",0);` |
|   7523 | 2333 | `		return PH7_OK;` |
|      - | 2334 | `	}` |
|      - | 2335 | `	/* Start the trim process */` |
|  15169 | 2336 | `	if( nArg < 2 ){` |
|      - | 2337 | `		SyString sStr;` |
|      - | 2338 | `		/* Remove white spaces and NUL bytes */` |
|  15133 | 2339 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  38167 | 2340 | `		SyStringFullTrimSafe(&sStr);` |
|  15133 | 2341 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   7569 | 2342 | `	}else{` |
|      - | 2343 | `		/* Char list */` |
|      - | 2344 | `		const char *zList;` |
|      - | 2345 | `		int nListlen;` |
|     39 | 2346 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     39 | 2347 | `		if( nListlen < 1 ){` |
|      - | 2348 | `			/* Return the string unchanged */` |
|      6 | 2349 | `			ph7_result_string(pCtx,zString,nLen);` |
|      4 | 2350 | `		}else{` |
|      - | 2351 | `			char aMask[256];` |
|     35 | 2352 | `			const char *zEnd = &zString[nLen];` |
|     35 | 2353 | `			const char *zCur = zString;` |
|     35 | 2354 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2355 | `			/* Left trim */` |
|     91 | 2356 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     59 | 2357 | `				zCur++;` |
|      3 | 2358 | `			}` |
|      - | 2359 | `			/* Right trim */` |
|     85 | 2360 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     53 | 2361 | `				zEnd--;` |
|      3 | 2362 | `			}` |
|     35 | 2363 | `			if( zCur >= zEnd ){` |
|      - | 2364 | `				/* Return the empty string */` |
|    ! 0 | 2365 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2366 | `			}else{` |
|     35 | 2367 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2368 | `			}` |
|      - | 2369 | `		}` |
|      - | 2370 | `	}` |
|  15169 | 2371 | `	return PH7_OK;` |
|  11346 | 2372 | `}` |
|      - | 2373 | `/*` |
|      - | 2374 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 2375 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 2376 | ` * Parameters` |
|      - | 2377 | ` *  $str` |
|      - | 2378 | ` *   The string that will be trimmed.` |
|      - | 2379 | ` * $charlist` |
|      - | 2380 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2381 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2382 | ` *   With .. you can specify a range of characters.` |
|      - | 2383 | ` * Returns.` |
|      - | 2384 | ` *  Thr processed string.` |
|      - | 2385 | ` * NOTE:` |
|      - | 2386 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2387 | ` */` |
|    178 | 2388 | `PH7_PRIVATE int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2389 | `{` |
|    182 | 2390 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"rtrim",1,"$string"); }` |
|      - | 2391 | `	const char *zString;` |
|      - | 2392 | `	int nLen;` |
|    182 | 2393 | `	if( nArg < 1 ){` |
|      - | 2394 | `		/* Missing arguments,return null */` |
|    ! 0 | 2395 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2396 | `		return PH7_OK;` |
|      - | 2397 | `	}` |
|      - | 2398 | `	/* Extract the target string */` |
|    182 | 2399 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    182 | 2400 | `	if( nLen < 1 ){` |
|      - | 2401 | `		/* Empty string,return */` |
|      8 | 2402 | `		ph7_result_string(pCtx,"",0);` |
|      8 | 2403 | `		return PH7_OK;` |
|      - | 2404 | `	}` |
|      - | 2405 | `	/* Start the trim process */` |
|    176 | 2406 | `	if( nArg < 2 ){` |
|      - | 2407 | `		SyString sStr;` |
|      - | 2408 | `		/* Remove white spaces and NUL bytes*/` |
|     19 | 2409 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     48 | 2410 | `		SyStringRightTrimSafe(&sStr);` |
|     19 | 2411 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|     10 | 2412 | `	}else{` |
|      - | 2413 | `		/* Char list */` |
|      - | 2414 | `		const char *zList;` |
|      - | 2415 | `		int nListlen;` |
|    158 | 2416 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|    158 | 2417 | `		if( nListlen < 1 ){` |
|      - | 2418 | `			/* Return the string unchanged */` |
|    ! 0 | 2419 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 2420 | `		}else{` |
|      - | 2421 | `			char aMask[256];` |
|    158 | 2422 | `			const char *zEnd = &zString[nLen];` |
|    158 | 2423 | `			const char *zCur = zString;` |
|    158 | 2424 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2425 | `			/* Right trim */` |
|    176 | 2426 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     22 | 2427 | `				zEnd--;` |
|      4 | 2428 | `			}` |
|    158 | 2429 | `			if( zEnd <= zCur ){` |
|      - | 2430 | `				/* Return the empty string */` |
|    ! 0 | 2431 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2432 | `			}else{` |
|    158 | 2433 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2434 | `			}` |
|      - | 2435 | `		}` |
|      - | 2436 | `	}` |
|    176 | 2437 | `	return PH7_OK;` |
|     93 | 2438 | `}` |
|      - | 2439 | `/*` |
|      - | 2440 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 2441 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2442 | ` * Parameters` |
|      - | 2443 | ` *  $str` |
|      - | 2444 | ` *   The string that will be trimmed.` |
|      - | 2445 | ` * $charlist` |
|      - | 2446 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2447 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2448 | ` *   With .. you can specify a range of characters.` |
|      - | 2449 | ` * Returns.` |
|      - | 2450 | ` *  Thr processed string.` |
|      - | 2451 | ` * NOTE:` |
|      - | 2452 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2453 | ` */` |
|     86 | 2454 | `PH7_PRIVATE int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2455 | `{` |
|     91 | 2456 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"ltrim",1,"$string"); }` |
|      - | 2457 | `	const char *zString;` |
|      - | 2458 | `	int nLen;` |
|     91 | 2459 | `	if( nArg < 1 ){` |
|      - | 2460 | `		/* Missing arguments,return null */` |
|    ! 0 | 2461 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2462 | `		return PH7_OK;` |
|      - | 2463 | `	}` |
|      - | 2464 | `	/* Extract the target string */` |
|     91 | 2465 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     91 | 2466 | `	if( nLen < 1 ){` |
|      - | 2467 | `		/* Empty string,return */` |
|     28 | 2468 | `		ph7_result_string(pCtx,"",0);` |
|     28 | 2469 | `		return PH7_OK;` |
|      - | 2470 | `	}` |
|      - | 2471 | `	/* Start the trim process */` |
|     67 | 2472 | `	if( nArg < 2 ){` |
|      - | 2473 | `		SyString sStr;` |
|      - | 2474 | `		/* Remove white spaces and NUL byte */` |
|      5 | 2475 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     13 | 2476 | `		SyStringLeftTrimSafe(&sStr);` |
|      5 | 2477 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      3 | 2478 | `	}else{` |
|      - | 2479 | `		/* Char list */` |
|      - | 2480 | `		const char *zList;` |
|      - | 2481 | `		int nListlen;` |
|     63 | 2482 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     63 | 2483 | `		if( nListlen < 1 ){` |
|      - | 2484 | `			/* Return the string unchanged */` |
|      3 | 2485 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2486 | `		}else{` |
|      - | 2487 | `			char aMask[256];` |
|     61 | 2488 | `			const char *zEnd = &zString[nLen];` |
|     61 | 2489 | `			const char *zCur = zString;` |
|     61 | 2490 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2491 | `			/* Left trim */` |
|    149 | 2492 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     93 | 2493 | `				zCur++;` |
|      5 | 2494 | `			}` |
|     61 | 2495 | `			if( zCur >= zEnd ){` |
|      - | 2496 | `				/* Return the empty string */` |
|    ! 0 | 2497 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2498 | `			}else{` |
|     61 | 2499 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2500 | `			}` |
|      - | 2501 | `		}` |
|      - | 2502 | `	}` |
|     67 | 2503 | `	return PH7_OK;` |
|     48 | 2504 | `}` |
|      - | 2505 | `/*` |
|      - | 2506 | ` * string strtolower(string $str)` |
|      - | 2507 | ` *  Make a string lowercase.` |
|      - | 2508 | ` * Parameters` |
|      - | 2509 | ` *  $str` |
|      - | 2510 | ` *   The input string.` |
|      - | 2511 | ` * Returns.` |
|      - | 2512 | ` *  The lowercased string.` |
|      - | 2513 | ` */` |
|  36570 | 2514 | `PH7_PRIVATE int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2515 | `{` |
|  36575 | 2516 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtolower",1,"$string"); }` |
|      - | 2517 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2518 | `	int nLen;` |
|  36575 | 2519 | `	if( nArg < 1 ){` |
|      - | 2520 | `		/* Missing arguments,return null */` |
|    ! 0 | 2521 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2522 | `		return PH7_OK;` |
|      - | 2523 | `	}` |
|      - | 2524 | `	/* Extract the target string */` |
|  36575 | 2525 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  36575 | 2526 | `	if( nLen < 1 ){` |
|      - | 2527 | `		/* Empty string,return */` |
|      6 | 2528 | `		ph7_result_string(pCtx,"",0);` |
|      6 | 2529 | `		return PH7_OK;` |
|      - | 2530 | `	}` |
|      - | 2531 | `	/* Perform the requested operation */` |
|  36571 | 2532 | `	zEnd = &zString[nLen];` |
| 115750 | 2533 | `	for(;;){` |
| 231505 | 2534 | `		if( zString >= zEnd ){` |
|      - | 2535 | `			/* No more input,break immediately */` |
|  36571 | 2536 | `			break;` |
|      - | 2537 | `		}` |
| 194939 | 2538 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2539 | `			/* UTF-8 stream,output verbatim */` |
|      9 | 2540 | `			zCur = zString;` |
|      9 | 2541 | `			zString++;` |
|     13 | 2542 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|      5 | 2543 | `				zString++;` |
|      1 | 2544 | `			}` |
|      - | 2545 | `			/* Append UTF-8 stream */` |
|      9 | 2546 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|      5 | 2547 | `		}else{` |
| 194931 | 2548 | `			int c = zString[0];` |
| 194931 | 2549 | `			if( SyisUpper(c) ){` |
| 190609 | 2550 | `				c = SyToLower(zString[0]);` |
|  95302 | 2551 | `			}` |
|      - | 2552 | `			/* Append character */` |
| 194931 | 2553 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2554 | `			/* Advance the cursor */` |
| 194931 | 2555 | `			zString++;` |
|      - | 2556 | `		}` |
|      5 | 2557 | `	}` |
|  36571 | 2558 | `	return PH7_OK;` |
|  18290 | 2559 | `}` |
|      - | 2560 | `/*` |
|      - | 2561 | ` * string strtolower(string $str)` |
|      - | 2562 | ` *  Make a string uppercase.` |
|      - | 2563 | ` * Parameters` |
|      - | 2564 | ` *  $str` |
|      - | 2565 | ` *   The input string.` |
|      - | 2566 | ` * Returns.` |
|      - | 2567 | ` *  The uppercased string.` |
|      - | 2568 | ` */` |
|     94 | 2569 | `PH7_PRIVATE int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2570 | `{` |
|     99 | 2571 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtoupper",1,"$string"); }` |
|      - | 2572 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2573 | `	int nLen;` |
|     99 | 2574 | `	if( nArg < 1 ){` |
|      - | 2575 | `		/* Missing arguments,return null */` |
|    ! 0 | 2576 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2577 | `		return PH7_OK;` |
|      - | 2578 | `	}` |
|      - | 2579 | `	/* Extract the target string */` |
|     99 | 2580 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     99 | 2581 | `	if( nLen < 1 ){` |
|      - | 2582 | `		/* Empty string,return */` |
|      6 | 2583 | `		ph7_result_string(pCtx,"",0);` |
|      6 | 2584 | `		return PH7_OK;` |
|      - | 2585 | `	}` |
|      - | 2586 | `	/* Perform the requested operation */` |
|     95 | 2587 | `	zEnd = &zString[nLen];` |
|    185 | 2588 | `	for(;;){` |
|    375 | 2589 | `		if( zString >= zEnd ){` |
|      - | 2590 | `			/* No more input,break immediately */` |
|     95 | 2591 | `			break;` |
|      - | 2592 | `		}` |
|    285 | 2593 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2594 | `			/* UTF-8 stream,output verbatim */` |
|      9 | 2595 | `			zCur = zString;` |
|      9 | 2596 | `			zString++;` |
|     13 | 2597 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|      5 | 2598 | `				zString++;` |
|      1 | 2599 | `			}` |
|      - | 2600 | `			/* Append UTF-8 stream */` |
|      9 | 2601 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|      5 | 2602 | `		}else{` |
|    277 | 2603 | `			int c = zString[0];` |
|    277 | 2604 | `			if( SyisLower(c) ){` |
|    249 | 2605 | `				c = SyToUpper(zString[0]);` |
|    122 | 2606 | `			}` |
|      - | 2607 | `			/* Append character */` |
|    277 | 2608 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2609 | `			/* Advance the cursor */` |
|    277 | 2610 | `			zString++;` |
|      - | 2611 | `		}` |
|      5 | 2612 | `	}` |
|     95 | 2613 | `	return PH7_OK;` |
|     52 | 2614 | `}` |
|      - | 2615 | `/*` |
|      - | 2616 | ` * string ucfirst(string $str)` |
|      - | 2617 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2618 | ` *  character is alphabetic.` |
|      - | 2619 | ` * Parameters` |
|      - | 2620 | ` *  $str` |
|      - | 2621 | ` *   The input string.` |
|      - | 2622 | ` * Returns.` |
|      - | 2623 | ` *  The processed string.` |
|      - | 2624 | ` */` |
|      8 | 2625 | `PH7_PRIVATE int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2626 | `{` |
|      - | 2627 | `	const char *zString,*zEnd;` |
|      - | 2628 | `	int nLen,c;` |
|     10 | 2629 | `	if( nArg < 1 ){` |
|      - | 2630 | `		/* Missing arguments,return null */` |
|    ! 0 | 2631 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2632 | `		return PH7_OK;` |
|      - | 2633 | `	}` |
|      - | 2634 | `	/* Extract the target string */` |
|     10 | 2635 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     10 | 2636 | `	if( nLen < 1 ){` |
|      - | 2637 | `		/* Empty string,return */` |
|      6 | 2638 | `		ph7_result_string(pCtx,"",0);` |
|      6 | 2639 | `		return PH7_OK;` |
|      - | 2640 | `	}` |
|      - | 2641 | `	/* Perform the requested operation */` |
|      5 | 2642 | `	zEnd = &zString[nLen];` |
|      5 | 2643 | `	c = zString[0];` |
|      5 | 2644 | `	if( SyisLower(c) ){` |
|      3 | 2645 | `		c = SyToUpper(c);` |
|      1 | 2646 | `	}` |
|      - | 2647 | `	/* Append the first character */` |
|      5 | 2648 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      5 | 2649 | `	zString++;` |
|      5 | 2650 | `	if( zString < zEnd ){` |
|      - | 2651 | `		/* Append the rest of the input verbatim */` |
|      5 | 2652 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      2 | 2653 | `	}` |
|      5 | 2654 | `	return PH7_OK;` |
|      6 | 2655 | `}` |
|      - | 2656 | `/*` |
|      - | 2657 | ` * string lcfirst(string $str)` |
|      - | 2658 | ` *  Make a string's first character lowercase.` |
|      - | 2659 | ` * Parameters` |
|      - | 2660 | ` *  $str` |
|      - | 2661 | ` *   The input string.` |
|      - | 2662 | ` * Returns.` |
|      - | 2663 | ` *  The processed string.` |
|      - | 2664 | ` */` |
|      8 | 2665 | `PH7_PRIVATE int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2666 | `{` |
|      - | 2667 | `	const char *zString,*zEnd;` |
|      - | 2668 | `	int nLen,c;` |
|     10 | 2669 | `	if( nArg < 1 ){` |
|      - | 2670 | `		/* Missing arguments,return null */` |
|    ! 0 | 2671 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2672 | `		return PH7_OK;` |
|      - | 2673 | `	}` |
|      - | 2674 | `	/* Extract the target string */` |
|     10 | 2675 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     10 | 2676 | `	if( nLen < 1 ){` |
|      - | 2677 | `		/* Empty string,return */` |
|      6 | 2678 | `		ph7_result_string(pCtx,"",0);` |
|      6 | 2679 | `		return PH7_OK;` |
|      - | 2680 | `	}` |
|      - | 2681 | `	/* Perform the requested operation */` |
|      5 | 2682 | `	zEnd = &zString[nLen];` |
|      5 | 2683 | `	c = zString[0];` |
|      5 | 2684 | `	if( SyisUpper(c) ){` |
|      3 | 2685 | `		c = SyToLower(c);` |
|      1 | 2686 | `	}` |
|      - | 2687 | `	/* Append the first character */` |
|      5 | 2688 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      5 | 2689 | `	zString++;` |
|      5 | 2690 | `	if( zString < zEnd ){` |
|      - | 2691 | `		/* Append the rest of the input verbatim */` |
|      5 | 2692 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      2 | 2693 | `	}` |
|      5 | 2694 | `	return PH7_OK;` |
|      6 | 2695 | `}` |
|      - | 2696 | `/*` |
|      - | 2697 | ` * int ord(string $string)` |
|      - | 2698 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 2699 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 2700 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 2701 | ` * Parameters` |
|      - | 2702 | ` *  $string` |
|      - | 2703 | ` *   The input string.` |
|      - | 2704 | ` * Returns` |
|      - | 2705 | ` *  The ASCII value as an integer.` |
|      - | 2706 | ` */` |
|    324 | 2707 | `PH7_PRIVATE int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2708 | `{` |
|      - | 2709 | `	const char *zString;` |
|      - | 2710 | `	int nLen,c;` |
|      - | 2711 | `	/* PHP requires exactly one argument. */` |
|    327 | 2712 | `	if( nArg != 1 ){` |
|    ! 0 | 2713 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2714 | `			"ArgumentCountError",` |
|      - | 2715 | `			"ord() expects exactly 1 argument, %d given",` |
|    ! 0 | 2716 | `			nArg` |
|      - | 2717 | `			);` |
|      - | 2718 | `	}` |
|      - | 2719 | `	/* php only DEPRECATES null here; PHL rejects it. */` |
|    327 | 2720 | `	if( ph7_value_is_null(apArg[0]) ){` |
|    ! 0 | 2721 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 2722 | `			"ord(): Argument #1 ($character) must be of type string, null given"` |
|      - | 2723 | `			);` |
|      - | 2724 | `	}` |
|      - | 2725 | `	/* Extract the target string */` |
|    327 | 2726 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    327 | 2727 | `	if( nLen < 1 ){` |
|      - | 2728 | `		/* php only DEPRECATES an empty string here; PHL rejects it. */` |
|      3 | 2729 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2730 | `			"ord(): Argument #1 ($character) must not be empty"` |
|      - | 2731 | `			);` |
|      - | 2732 | `	}` |
|      - | 2733 | `	/* A string longer than one byte: php DEPRECATES it; PHL rejects it. */` |
|    325 | 2734 | `	if( nLen > 1 ){` |
|      3 | 2735 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2736 | `			"ord(): Argument #1 ($character) must be a single byte, use ord($str[0]) instead"` |
|      - | 2737 | `			);` |
|      - | 2738 | `	}` |
|      - | 2739 | `	/* Extract the ASCII value of the first character */` |
|    323 | 2740 | `	c = (unsigned char)zString[0];` |
|      - | 2741 | `	/* Return that value */` |
|    323 | 2742 | `	ph7_result_int(pCtx,c);` |
|    323 | 2743 | `	return PH7_OK;` |
|    165 | 2744 | `}` |
|      - | 2745 | `/*` |
|      - | 2746 | ` * string chr(int $codepoint)` |
|      - | 2747 | ` *  Returns a one-character string containing the character specified` |
|      - | 2748 | ` *  by the given codepoint, which must be in the [0, 255] range.` |
|      - | 2749 | ` * Parameters` |
|      - | 2750 | ` *  $codepoint` |
|      - | 2751 | ` *   An integer codepoint in [0, 255]. php merely deprecates values` |
|      - | 2752 | ` *   outside that range (constraining them with % 256); PHL rejects` |
|      - | 2753 | ` *   them with a ValueError (scope policy).` |
|      - | 2754 | ` * Returns` |
|      - | 2755 | ` *  A single-character string.` |
|      - | 2756 | ` */` |
|  10608 | 2757 | `PH7_PRIVATE int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2758 | `{` |
|      - | 2759 | `	int c;` |
|      - | 2760 | `	unsigned char ch;` |
|      - | 2761 | `	/* PHP requires exactly one argument. */` |
|  10612 | 2762 | `	if( nArg != 1 ){` |
|    ! 0 | 2763 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2764 | `			"ArgumentCountError",` |
|      - | 2765 | `			"chr() expects exactly 1 argument, %d given",` |
|    ! 0 | 2766 | `			nArg` |
|      - | 2767 | `			);` |
|      - | 2768 | `	}` |
|      - | 2769 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 2770 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 2771 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 2772 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|  10612 | 2773 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      3 | 2774 | `		double d = ph7_value_to_double(apArg[0]);` |
|      3 | 2775 | `		if( d != (double)(sxi64)d ){` |
|      - | 2776 | `			/* php only DEPRECATES a lossy float->int here; PHL rejects it. */` |
|      3 | 2777 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 2778 | `				"chr(): Argument #1 ($codepoint) must be of type int, float given");` |
|      - | 2779 | `		}` |
|    ! 0 | 2780 | `	}` |
|      - | 2781 | `	/* Extract the codepoint. */` |
|  10610 | 2782 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 2783 | `	/* php only DEPRECATES an out-of-range codepoint (constraining it with % 256);` |
|      - | 2784 | `	 * PHL targets php's non-deprecated surface and rejects it loudly, matching the` |
|      - | 2785 | `	 * lossy-float branch above. This was the last engine site still emitting` |
|      - | 2786 | `	 * E_DEPRECATED — the scope policy says none remain. */` |
|  10610 | 2787 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 2788 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2789 | `			"chr(): Argument #1 ($codepoint) must be between 0 and 255");` |
|      - | 2790 | `	}` |
|      - | 2791 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 2792 | `	 * when taking the address of a wider int. */` |
|  10606 | 2793 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 2794 | `	/* Return the specified character */` |
|  10606 | 2795 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|  10606 | 2796 | `	return PH7_OK;` |
|   5308 | 2797 | `}` |
|      - | 2798 | `/*` |
|      - | 2799 | ` * Binary to hex consumer callback.` |
|      - | 2800 | ` * This callback is the default consumer used by the hash functions` |
|      - | 2801 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 2802 | ` */` |
|   5262 | 2803 | `PH7_PRIVATE int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      4 | 2804 | `{` |
|      - | 2805 | `	/* Append hex chunk verbatim */` |
|   5266 | 2806 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   5266 | 2807 | `	return SXRET_OK;` |
|      4 | 2808 | `}` |
|      - | 2809 |  |
|      - | 2810 | `/*` |
|      - | 2811 | ` * string bin2hex(string $str)` |
|      - | 2812 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 2813 | ` * Parameters` |
|      - | 2814 | ` *  $str` |
|      - | 2815 | ` *   The input string.` |
|      - | 2816 | ` * Returns.` |
|      - | 2817 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 2818 | ` */` |
|    666 | 2819 | `PH7_PRIVATE int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2820 | `{` |
|      - | 2821 | `	const char *zString;` |
|      - | 2822 | `	int nLen;` |
|      - | 2823 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|    669 | 2824 | `	if( nArg != 1 ){` |
|    ! 0 | 2825 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2826 | `			"ArgumentCountError",` |
|      - | 2827 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|    ! 0 | 2828 | `			nArg` |
|      - | 2829 | `			);` |
|      - | 2830 | `	}` |
|      - | 2831 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 2832 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 2833 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 2834 | `	 */` |
|   1002 | 2835 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|    333 | 2836 | `		( ph7_value_is_object(apArg[0]) &&` |
|    ! 0 | 2837 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|    ! 0 | 2838 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|    ! 0 | 2839 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 2840 | `		)` |
|      - | 2841 | `	){` |
|    ! 0 | 2842 | `		const char *zType = ph7_type_name(apArg[0]);` |
|    ! 0 | 2843 | `		if( ph7_value_is_object(apArg[0]) ){` |
|    ! 0 | 2844 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    ! 0 | 2845 | `			if( pInst && pInst->pClass ){` |
|    ! 0 | 2846 | `				zType = SyStringData(&pInst->pClass->sName);` |
|    ! 0 | 2847 | `			}` |
|    ! 0 | 2848 | `		}` |
|    ! 0 | 2849 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2850 | `			"TypeError",` |
|      - | 2851 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 2852 | `			zType` |
|      - | 2853 | `			);` |
|      - | 2854 | `	}` |
|      - | 2855 | `	/* Extract the target string */` |
|    669 | 2856 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    669 | 2857 | `	if( nLen < 1 ){` |
|      - | 2858 | `		/* Empty string,return */` |
|     21 | 2859 | `		ph7_result_string(pCtx,"",0);` |
|     21 | 2860 | `		return PH7_OK;` |
|      - | 2861 | `	}` |
|      - | 2862 | `	/* Perform the requested operation */` |
|    649 | 2863 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|    649 | 2864 | `	return PH7_OK;` |
|    336 | 2865 | `}` |
|      - | 2866 |  |
|      - | 2867 | `/* Search callback signature */` |
|      - | 2868 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 2869 | `/*` |
|      - | 2870 | ` * Case-insensitive pattern match.` |
|      - | 2871 | ` * Brute force is the default search method used here.` |
|      - | 2872 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 2873 | ` * well for short/medium texts on modern hardware.` |
|      - | 2874 | ` */` |
|    274 | 2875 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      2 | 2876 | `{` |
|    276 | 2877 | `	const char *zpIn = (const char *)pPattern;` |
|    276 | 2878 | `	const char *zIn = (const char *)pText;` |
|    276 | 2879 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    276 | 2880 | `	const char *zEnd = &zIn[nLen];` |
|      - | 2881 | `	const char *zPtr,*zPtr2;` |
|      - | 2882 | `	int c,d;` |
|    276 | 2883 | `	if( iPatLen > nLen ){` |
|      - | 2884 | `		/* Don't bother processing */` |
|     35 | 2885 | `		return SXERR_NOTFOUND;` |
|      - | 2886 | `	}` |
|    776 | 2887 | `	for(;;){` |
|   1554 | 2888 | `		if( zIn >= zEnd ){` |
|    184 | 2889 | `			break;` |
|      - | 2890 | `		}` |
|   1372 | 2891 | `		c = SyToLower(zIn[0]);` |
|   1372 | 2892 | `		d = SyToLower(zpIn[0]);` |
|   1372 | 2893 | `		if( c == d ){` |
|    200 | 2894 | `			zPtr   = &zIn[1];` |
|    200 | 2895 | `			zPtr2  = &zpIn[1];` |
|    150 | 2896 | `			for(;;){` |
|    302 | 2897 | `				if( zPtr2 >= zpEnd ){` |
|      - | 2898 | `					/* Pattern found */` |
|     59 | 2899 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     59 | 2900 | `					return SXRET_OK;` |
|      - | 2901 | `				}` |
|    244 | 2902 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 2903 | `					break;` |
|      - | 2904 | `				}` |
|    244 | 2905 | `				c = SyToLower(zPtr[0]);` |
|    244 | 2906 | `				d = SyToLower(zPtr2[0]);` |
|    244 | 2907 | `				if( c != d ){` |
|    142 | 2908 | `					break;` |
|      - | 2909 | `				}` |
|    103 | 2910 | `				zPtr++; zPtr2++;` |
|      1 | 2911 | `			}` |
|     70 | 2912 | `		}` |
|   1314 | 2913 | `		zIn++;` |
|      2 | 2914 | `	}` |
|      - | 2915 | `	/* Pattern not found */` |
|    184 | 2916 | `	return SXERR_NOTFOUND;` |
|    139 | 2917 | `}` |
|      - | 2918 | `/*` |
|      - | 2919 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2920 | ` *  Find the first occurrence of a string.` |
|      - | 2921 | ` * Parameters` |
|      - | 2922 | ` *  $haystack` |
|      - | 2923 | ` *   The input string.` |
|      - | 2924 | ` * $needle` |
|      - | 2925 | ` *   Search pattern (must be a string).` |
|      - | 2926 | ` * $before_needle` |
|      - | 2927 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2928 | ` *   of the needle (excluding the needle).` |
|      - | 2929 | ` * Return` |
|      - | 2930 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2931 | ` */` |
|     38 | 2932 | `PH7_PRIVATE int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2933 | `{` |
|     40 | 2934 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2935 | `	const char *zBlob,*zPattern;` |
|      - | 2936 | `	int nLen,nPatLen;` |
|      - | 2937 | `	sxu32 nOfft;` |
|      - | 2938 | `	sxi32 rc;` |
|     40 | 2939 | `	if( nArg < 2 ){` |
|      - | 2940 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2941 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2942 | `		return PH7_OK;` |
|      - | 2943 | `	}` |
|      - | 2944 | `	/* Extract the needle and the haystack */` |
|     40 | 2945 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     40 | 2946 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     40 | 2947 | `	nOfft = 0; /* cc warning */` |
|     40 | 2948 | `	if( nPatLen < 1 ){` |
|      - | 2949 | `		/* php 8: the empty needle matches at position 0, so the whole haystack` |
|      - | 2950 | `		 * is returned (and nothing at all when $before_needle is set). */` |
|      7 | 2951 | `		if( nArg > 2 && ph7_value_to_bool(apArg[2]) ){` |
|      3 | 2952 | `			ph7_result_string(pCtx,"",0);` |
|      2 | 2953 | `		}else{` |
|      5 | 2954 | `			ph7_result_string(pCtx,zBlob,nLen);` |
|      - | 2955 | `		}` |
|      7 | 2956 | `		return PH7_OK;` |
|      - | 2957 | `	}` |
|     34 | 2958 | `	if( nLen > 0 ){` |
|     34 | 2959 | `		int before = 0;` |
|      - | 2960 | `		/* Perform the lookup */` |
|     34 | 2961 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     34 | 2962 | `		if( rc != SXRET_OK ){` |
|      - | 2963 | `			/* Pattern not found,return FALSE */` |
|      3 | 2964 | `			ph7_result_bool(pCtx,0);` |
|      3 | 2965 | `			return PH7_OK;` |
|      - | 2966 | `		}` |
|      - | 2967 | `		/* Return the portion of the string */` |
|     32 | 2968 | `		if( nArg > 2 ){` |
|     30 | 2969 | `			before = ph7_value_to_int(apArg[2]);` |
|     14 | 2970 | `		}` |
|     32 | 2971 | `		if( before ){` |
|     30 | 2972 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|     16 | 2973 | `		}else{` |
|      3 | 2974 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2975 | `		}` |
|     17 | 2976 | `	}else{` |
|    ! 0 | 2977 | `		ph7_result_bool(pCtx,0);` |
|      - | 2978 | `	}` |
|     32 | 2979 | `	return PH7_OK;` |
|     21 | 2980 | `}` |
|      - | 2981 | `/*` |
|      - | 2982 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2983 | ` *  Case-insensitive strstr().` |
|      - | 2984 | ` * Parameters` |
|      - | 2985 | ` *  $haystack` |
|      - | 2986 | ` *   The input string.` |
|      - | 2987 | ` * $needle` |
|      - | 2988 | ` *   Search pattern (must be a string).` |
|      - | 2989 | ` * $before_needle` |
|      - | 2990 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2991 | ` *   of the needle (excluding the needle).` |
|      - | 2992 | ` * Return` |
|      - | 2993 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2994 | ` */` |
|      6 | 2995 | `PH7_PRIVATE int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2996 | `{` |
|      7 | 2997 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2998 | `	const char *zBlob,*zPattern;` |
|      - | 2999 | `	int nLen,nPatLen;` |
|      - | 3000 | `	sxu32 nOfft;` |
|      - | 3001 | `	sxi32 rc;` |
|      7 | 3002 | `	if( nArg < 2 ){` |
|      - | 3003 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3004 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3005 | `		return PH7_OK;` |
|      - | 3006 | `	}` |
|      - | 3007 | `	/* Extract the needle and the haystack */` |
|      7 | 3008 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 3009 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 3010 | `	nOfft = 0; /* cc warning */` |
|      7 | 3011 | `	if( nPatLen < 1 ){` |
|      - | 3012 | `		/* php 8: the empty needle matches at position 0, so the whole haystack` |
|      - | 3013 | `		 * is returned (and nothing at all when $before_needle is set). */` |
|      3 | 3014 | `		if( nArg > 2 && ph7_value_to_bool(apArg[2]) ){` |
|    ! 0 | 3015 | `			ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3016 | `		}else{` |
|      3 | 3017 | `			ph7_result_string(pCtx,zBlob,nLen);` |
|      - | 3018 | `		}` |
|      3 | 3019 | `		return PH7_OK;` |
|      - | 3020 | `	}` |
|      5 | 3021 | `	if( nLen > 0 ){` |
|      5 | 3022 | `		int before = 0;` |
|      - | 3023 | `		/* Perform the lookup */` |
|      5 | 3024 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3025 | `		if( rc != SXRET_OK ){` |
|      - | 3026 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3027 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3028 | `			return PH7_OK;` |
|      - | 3029 | `		}` |
|      - | 3030 | `		/* Return the portion of the string */` |
|      5 | 3031 | `		if( nArg > 2 ){` |
|      3 | 3032 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3033 | `		}` |
|      5 | 3034 | `		if( before ){` |
|      3 | 3035 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3036 | `		}else{` |
|      3 | 3037 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3038 | `		}` |
|      3 | 3039 | `	}else{` |
|    ! 0 | 3040 | `		ph7_result_bool(pCtx,0);` |
|      - | 3041 | `	}` |
|      5 | 3042 | `	return PH7_OK;` |
|      4 | 3043 | `}` |
|      - | 3044 | `/*` |
|      - | 3045 | ` * Resolve the $offset argument shared by strpos()/stripos().` |
|      - | 3046 | ` *` |
|      - | 3047 | ` * php requires -strlen($haystack) <= $offset <= strlen($haystack) and throws` |
|      - | 3048 | ` * ValueError otherwise; a negative offset counts back from the end. PHL used to` |
|      - | 3049 | ` * negate a negative offset and silently clamp an out-of-range one to zero, so` |
|      - | 3050 | ` * strpos("Hello","l",100) answered 2 where php raises — an argument error` |
|      - | 3051 | ` * turned into a wrong answer.` |
|      - | 3052 | ` *` |
|      - | 3053 | ` * On success *pnStart receives the resolved non-negative offset.` |
|      - | 3054 | ` */` |
|     24 | 3055 | `static sxi32 StrSearchOffset(` |
|      - | 3056 | `	ph7_context *pCtx,` |
|      - | 3057 | `	ph7_value *pArg,` |
|      - | 3058 | `	int nLen,` |
|      - | 3059 | `	const char *zFunc,` |
|      - | 3060 | `	int *pnStart` |
|      - | 3061 | `	)` |
|      2 | 3062 | `{` |
|     26 | 3063 | `	ph7_int64 iOfft = ph7_value_to_int64(pArg);` |
|      - | 3064 | `	/* Compare without negating iOfft: -INT64_MIN would overflow. */` |
|     26 | 3065 | `	if( iOfft < 0 ? (iOfft < -(ph7_int64)nLen) : (iOfft > (ph7_int64)nLen) ){` |
|      8 | 3066 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      4 | 3067 | `			"%s(): Argument #3 ($offset) must be contained in argument #1 ($haystack)",zFunc);` |
|      - | 3068 | `	}` |
|     26 | 3069 | `	*pnStart = (int)(iOfft < 0 ? (ph7_int64)nLen + iOfft : iOfft);` |
|     26 | 3070 | `	return PH7_OK;` |
|     18 | 3071 | `}` |
|      - | 3072 | `/*` |
|      - | 3073 | ` * Resolve the window of match START positions for strrpos()/strripos().` |
|      - | 3074 | ` *` |
|      - | 3075 | ` * php's rule is asymmetric in the sign of $offset: a non-negative offset is a` |
|      - | 3076 | ` * LOWER bound on where the match may start, while a negative one is an UPPER` |
|      - | 3077 | ` * bound counted back from the end of the haystack (zend_memnrstr). The range` |
|      - | 3078 | ` * check is the same as StrSearchOffset()'s.` |
|      - | 3079 | ` *` |
|      - | 3080 | ` * On success the closed interval [*pnMin,*pnMax] holds every position at which` |
|      - | 3081 | ` * a match is allowed to begin; it is empty (max < min) when the needle cannot` |
|      - | 3082 | ` * fit, which the caller reports as FALSE.` |
|      - | 3083 | ` */` |
|    106 | 3084 | `static sxi32 StrRSearchWindow(` |
|      - | 3085 | `	ph7_context *pCtx,` |
|      - | 3086 | `	ph7_value *pArg, /* The $offset argument, or NULL when it was omitted */` |
|      - | 3087 | `	int nLen,` |
|      - | 3088 | `	int nPatLen,` |
|      - | 3089 | `	const char *zFunc,` |
|      - | 3090 | `	int *pnMin,` |
|      - | 3091 | `	int *pnMax` |
|      - | 3092 | `	)` |
|      1 | 3093 | `{` |
|    107 | 3094 | `	int nMin = 0;` |
|    107 | 3095 | `	int nMax = nLen - nPatLen;` |
|    107 | 3096 | `	if( pArg ){` |
|     47 | 3097 | `		ph7_int64 iOfft = ph7_value_to_int64(pArg);` |
|     47 | 3098 | `		if( iOfft < 0 ? (iOfft < -(ph7_int64)nLen) : (iOfft > (ph7_int64)nLen) ){` |
|     33 | 3099 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|     14 | 3100 | `				"%s(): Argument #3 ($offset) must be contained in argument #1 ($haystack)",zFunc);` |
|      - | 3101 | `		}` |
|     29 | 3102 | `		if( iOfft < 0 ){` |
|     15 | 3103 | `			int nLimit = nLen + (int)iOfft;` |
|     15 | 3104 | `			if( nMax > nLimit ){` |
|     15 | 3105 | `				nMax = nLimit;` |
|      7 | 3106 | `			}` |
|      8 | 3107 | `		}else{` |
|     15 | 3108 | `			nMin = (int)iOfft;` |
|      - | 3109 | `		}` |
|     14 | 3110 | `	}` |
|     89 | 3111 | `	*pnMin = nMin;` |
|     89 | 3112 | `	*pnMax = nMax;` |
|     89 | 3113 | `	return PH7_OK;` |
|     49 | 3114 | `}` |
|      - | 3115 | `/*` |
|      - | 3116 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3117 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 3118 | ` * Parameters` |
|      - | 3119 | ` *  $haystack` |
|      - | 3120 | ` *   The input string.` |
|      - | 3121 | ` * $needle` |
|      - | 3122 | ` *   Search pattern (must be a string).` |
|      - | 3123 | ` * $offset` |
|      - | 3124 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3125 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3126 | ` *   of haystack.` |
|      - | 3127 | ` * Return` |
|      - | 3128 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3129 | ` */` |
|   2186 | 3130 | `PH7_PRIVATE int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3131 | `{` |
|   2191 | 3132 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strpos",1,"$haystack"); }` |
|   2191 | 3133 | `	if( nArg > 1 ){ StrNullArgNotice(pCtx,apArg[1],"strpos",2,"$needle"); }` |
|   2191 | 3134 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3135 | `	const char *zBlob,*zPattern;` |
|      - | 3136 | `	int nLen,nPatLen,nStart;` |
|      - | 3137 | `	sxu32 nOfft;` |
|      - | 3138 | `	sxi32 rc;` |
|   2191 | 3139 | `	if( nArg < 2 ){` |
|      - | 3140 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3141 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3142 | `		return PH7_OK;` |
|      - | 3143 | `	}` |
|      - | 3144 | `	/* Extract the needle and the haystack */` |
|   2191 | 3145 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|   2191 | 3146 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|   2191 | 3147 | `	nOfft = 0; /* cc warning */` |
|   2191 | 3148 | `	nStart = 0;` |
|      - | 3149 | `	/* Peek the starting offset if available */` |
|   2191 | 3150 | `	if( nArg > 2 ){` |
|     22 | 3151 | `		rc = StrSearchOffset(pCtx,apArg[2],nLen,"strpos",&nStart);` |
|     22 | 3152 | `		if( rc != PH7_OK ){` |
|    ! 0 | 3153 | `			return rc;` |
|      - | 3154 | `		}` |
|     10 | 3155 | `	}` |
|   2191 | 3156 | `	if( nPatLen < 1 ){` |
|      - | 3157 | `		/* php 8 treats the empty needle as matching at the search offset. */` |
|     11 | 3158 | `		ph7_result_int64(pCtx,(ph7_int64)nStart);` |
|     11 | 3159 | `		return PH7_OK;` |
|      - | 3160 | `	}` |
|   2181 | 3161 | `	zBlob += nStart;` |
|   2181 | 3162 | `	nLen -= nStart;` |
|   2181 | 3163 | `	if( nLen > 0 ){` |
|      - | 3164 | `		/* Perform the lookup */` |
|   2179 | 3165 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|   2179 | 3166 | `		if( rc != SXRET_OK ){` |
|      - | 3167 | `			/* Pattern not found,return FALSE */` |
|   1151 | 3168 | `			ph7_result_bool(pCtx,0);` |
|   1151 | 3169 | `			return PH7_OK;` |
|      - | 3170 | `		}` |
|      - | 3171 | `		/* Return the pattern position */` |
|   1033 | 3172 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|    519 | 3173 | `	}else{` |
|      3 | 3174 | `		ph7_result_bool(pCtx,0);` |
|      - | 3175 | `	}` |
|   1035 | 3176 | `	return PH7_OK;` |
|   1098 | 3177 | `}` |
|      - | 3178 | `/*` |
|      - | 3179 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|      - | 3180 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|      - | 3181 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|      - | 3182 | ` * TypeError for arrays, resources, and objects without __toString.` |
|      - | 3183 | ` *` |
|      - | 3184 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|      - | 3185 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|      - | 3186 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|      - | 3187 | ` *` |
|      - | 3188 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|      - | 3189 | ` * is valid until pTmp is released or pArg is mutated.` |
|      - | 3190 | ` */` |
|    708 | 3191 | `static sxi32 StrPredicateResolveArg(` |
|      - | 3192 | `	ph7_context *pCtx,` |
|      - | 3193 | `	ph7_value *pArg,` |
|      - | 3194 | `	const char *zFunc,` |
|      - | 3195 | `	int iArgNum,` |
|      - | 3196 | `	const char *zParamName,` |
|      - | 3197 | `	const char *zTypeStr, /* Declared type in the TypeError, e.g. "string" / "?string" */` |
|      - | 3198 | `	const char *zNullMsg,` |
|      - | 3199 | `	ph7_value *pTmp,` |
|      - | 3200 | `	const char **pzOut,` |
|      - | 3201 | `	int *pnOut` |
|      4 | 3202 | `){` |
|    354 | 3203 | `	SXUNUSED(zNullMsg); /* php's deprecation text — PHL rejects null instead of coercing */` |
|    712 | 3204 | `	if( ph7_value_is_null(pArg) ){` |
|      - | 3205 | `		/* php only DEPRECATES null here; PHL rejects it with the TypeError php will` |
|      - | 3206 | `		 * eventually raise. */` |
|    ! 0 | 3207 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 3208 | `			"%s(): Argument #%d (%s) must be of type %s, null given",` |
|    ! 0 | 3209 | `			zFunc,iArgNum,zParamName,zTypeStr);` |
|      - | 3210 | `	}` |
|   1090 | 3211 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    708 | 3212 | `	    ( ph7_value_is_object(pArg) &&` |
|     72 | 3213 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     48 | 3214 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     24 | 3215 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 3216 | `	    )` |
|      - | 3217 | `	){` |
|    ! 0 | 3218 | `		const char *zType = ph7_type_name(pArg);` |
|    ! 0 | 3219 | `		if( ph7_value_is_object(pArg) ){` |
|    ! 0 | 3220 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|    ! 0 | 3221 | `			if( pInst && pInst->pClass ){` |
|    ! 0 | 3222 | `				zType = SyStringData(&pInst->pClass->sName);` |
|    ! 0 | 3223 | `			}` |
|    ! 0 | 3224 | `		}` |
|    ! 0 | 3225 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3226 | `			"TypeError",` |
|      - | 3227 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|    ! 0 | 3228 | `			zFunc, iArgNum, zParamName, zTypeStr, zType` |
|      - | 3229 | `			);` |
|      - | 3230 | `	}` |
|    712 | 3231 | `	if( ph7_value_is_object(pArg) ){` |
|     49 | 3232 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     49 | 3233 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 3234 | `			"__toString",sizeof("__toString")-1);` |
|     49 | 3235 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|     49 | 3236 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|     49 | 3237 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|     49 | 3238 | `		return PH7_OK;` |
|      - | 3239 | `	}` |
|    664 | 3240 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|    664 | 3241 | `	return PH7_OK;` |
|    358 | 3242 | `}` |
|      - | 3243 | `/*` |
|      - | 3244 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 3245 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 3246 | ` * Return` |
|      - | 3247 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 3248 | ` */` |
|     84 | 3249 | `PH7_PRIVATE int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3250 | `{` |
|      - | 3251 | `	const char *zHaystack,*zNeedle;` |
|      - | 3252 | `	int nHayLen,nNeedleLen;` |
|      - | 3253 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3254 | `	sxi32 rc;` |
|     86 | 3255 | `	if( nArg != 2 ){` |
|    ! 0 | 3256 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3257 | `			"ArgumentCountError",` |
|      - | 3258 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|    ! 0 | 3259 | `			nArg` |
|      - | 3260 | `			);` |
|      - | 3261 | `	}` |
|     86 | 3262 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     86 | 3263 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     86 | 3264 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack","string",` |
|      - | 3265 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|      - | 3266 | `		"of type string is deprecated",` |
|      - | 3267 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     86 | 3268 | `	if( rc != PH7_OK ) goto out;` |
|     86 | 3269 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle","string",` |
|      - | 3270 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|      - | 3271 | `		"of type string is deprecated",` |
|      - | 3272 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     86 | 3273 | `	if( rc != PH7_OK ) goto out;` |
|     86 | 3274 | `	if( nNeedleLen < 1 ){` |
|     11 | 3275 | `		ph7_result_bool(pCtx,1);` |
|     81 | 3276 | `	}else if( nHayLen < nNeedleLen ){` |
|      7 | 3277 | `		ph7_result_bool(pCtx,0);` |
|      4 | 3278 | `	}else{` |
|    104 | 3279 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|     34 | 3280 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|     70 | 3281 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|      - | 3282 | `	}` |
|     86 | 3283 | `	rc = PH7_OK;` |
|     42 | 3284 | `out:` |
|     86 | 3285 | `	PH7_MemObjRelease(&sHayTmp);` |
|     86 | 3286 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     86 | 3287 | `	return rc;` |
|     44 | 3288 | `}` |
|      - | 3289 | `/*` |
|      - | 3290 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 3291 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 3292 | ` * Return` |
|      - | 3293 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 3294 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3295 | ` */` |
|     86 | 3296 | `PH7_PRIVATE int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3297 | `{` |
|      - | 3298 | `	const char *zHaystack,*zNeedle;` |
|      - | 3299 | `	int nHayLen,nNeedleLen;` |
|      - | 3300 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3301 | `	sxi32 rc;` |
|     89 | 3302 | `	if( nArg != 2 ){` |
|    ! 0 | 3303 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3304 | `			"ArgumentCountError",` |
|      - | 3305 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|    ! 0 | 3306 | `			nArg` |
|      - | 3307 | `			);` |
|      - | 3308 | `	}` |
|     89 | 3309 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     89 | 3310 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     89 | 3311 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack","string",` |
|      - | 3312 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3313 | `		"of type string is deprecated",` |
|      - | 3314 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     89 | 3315 | `	if( rc != PH7_OK ) goto out;` |
|     89 | 3316 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle","string",` |
|      - | 3317 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3318 | `		"of type string is deprecated",` |
|      - | 3319 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     89 | 3320 | `	if( rc != PH7_OK ) goto out;` |
|     89 | 3321 | `	if( nNeedleLen < 1 ){` |
|     11 | 3322 | `		ph7_result_bool(pCtx,1);` |
|     84 | 3323 | `	}else if( nHayLen < nNeedleLen ){` |
|      7 | 3324 | `		ph7_result_bool(pCtx,0);` |
|      4 | 3325 | `	}else{` |
|    108 | 3326 | `		ph7_result_bool(pCtx,` |
|     70 | 3327 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3328 | `	}` |
|     89 | 3329 | `	rc = PH7_OK;` |
|     43 | 3330 | `out:` |
|     89 | 3331 | `	PH7_MemObjRelease(&sHayTmp);` |
|     89 | 3332 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     89 | 3333 | `	return rc;` |
|     46 | 3334 | `}` |
|      - | 3335 | `/*` |
|      - | 3336 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 3337 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 3338 | ` * Return` |
|      - | 3339 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 3340 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3341 | ` */` |
|     54 | 3342 | `PH7_PRIVATE int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3343 | `{` |
|      - | 3344 | `	const char *zHaystack,*zNeedle;` |
|      - | 3345 | `	int nHayLen,nNeedleLen;` |
|      - | 3346 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3347 | `	sxi32 rc;` |
|     55 | 3348 | `	if( nArg != 2 ){` |
|    ! 0 | 3349 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3350 | `			"ArgumentCountError",` |
|      - | 3351 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|    ! 0 | 3352 | `			nArg` |
|      - | 3353 | `			);` |
|      - | 3354 | `	}` |
|     55 | 3355 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     55 | 3356 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     55 | 3357 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack","string",` |
|      - | 3358 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3359 | `		"of type string is deprecated",` |
|      - | 3360 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     55 | 3361 | `	if( rc != PH7_OK ) goto out;` |
|     55 | 3362 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle","string",` |
|      - | 3363 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3364 | `		"of type string is deprecated",` |
|      - | 3365 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     55 | 3366 | `	if( rc != PH7_OK ) goto out;` |
|     55 | 3367 | `	if( nNeedleLen < 1 ){` |
|     11 | 3368 | `		ph7_result_bool(pCtx,1);` |
|     50 | 3369 | `	}else if( nHayLen < nNeedleLen ){` |
|      7 | 3370 | `		ph7_result_bool(pCtx,0);` |
|      4 | 3371 | `	}else{` |
|     58 | 3372 | `		ph7_result_bool(pCtx,` |
|     38 | 3373 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3374 | `	}` |
|     55 | 3375 | `	rc = PH7_OK;` |
|     27 | 3376 | `out:` |
|     55 | 3377 | `	PH7_MemObjRelease(&sHayTmp);` |
|     55 | 3378 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     55 | 3379 | `	return rc;` |
|     28 | 3380 | `}` |
|      - | 3381 | `/*` |
|      - | 3382 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3383 | ` *  Case-insensitive strpos.` |
|      - | 3384 | ` * Parameters` |
|      - | 3385 | ` *  $haystack` |
|      - | 3386 | ` *   The input string.` |
|      - | 3387 | ` * $needle` |
|      - | 3388 | ` *   Search pattern (must be a string).` |
|      - | 3389 | ` * $offset` |
|      - | 3390 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3391 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3392 | ` *   of haystack.` |
|      - | 3393 | ` * Return` |
|      - | 3394 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3395 | ` */` |
|    196 | 3396 | `PH7_PRIVATE int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3397 | `{` |
|    198 | 3398 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3399 | `	const char *zBlob,*zPattern;` |
|      - | 3400 | `	int nLen,nPatLen,nStart;` |
|      - | 3401 | `	sxu32 nOfft;` |
|      - | 3402 | `	sxi32 rc;` |
|    198 | 3403 | `	if( nArg < 2 ){` |
|      - | 3404 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3405 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3406 | `		return PH7_OK;` |
|      - | 3407 | `	}` |
|      - | 3408 | `	/* Extract the needle and the haystack */` |
|    198 | 3409 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    198 | 3410 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    198 | 3411 | `	nOfft = 0; /* cc warning */` |
|    198 | 3412 | `	nStart = 0;` |
|      - | 3413 | `	/* Peek the starting offset if available */` |
|    198 | 3414 | `	if( nArg > 2 ){` |
|      5 | 3415 | `		rc = StrSearchOffset(pCtx,apArg[2],nLen,"stripos",&nStart);` |
|      5 | 3416 | `		if( rc != PH7_OK ){` |
|    ! 0 | 3417 | `			return rc;` |
|      - | 3418 | `		}` |
|      2 | 3419 | `	}` |
|    198 | 3420 | `	if( nPatLen < 1 ){` |
|      - | 3421 | `		/* php 8 treats the empty needle as matching at the search offset. */` |
|      3 | 3422 | `		ph7_result_int64(pCtx,(ph7_int64)nStart);` |
|      3 | 3423 | `		return PH7_OK;` |
|      - | 3424 | `	}` |
|    196 | 3425 | `	zBlob += nStart;` |
|    196 | 3426 | `	nLen -= nStart;` |
|    196 | 3427 | `	if( nLen > 0 ){` |
|      - | 3428 | `		/* Perform the lookup */` |
|    196 | 3429 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    196 | 3430 | `		if( rc != SXRET_OK ){` |
|      - | 3431 | `			/* Pattern not found,return FALSE */` |
|    182 | 3432 | `			ph7_result_bool(pCtx,0);` |
|    182 | 3433 | `			return PH7_OK;` |
|      - | 3434 | `		}` |
|      - | 3435 | `		/* Return the pattern position */` |
|     15 | 3436 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 3437 | `	}else{` |
|    ! 0 | 3438 | `		ph7_result_bool(pCtx,0);` |
|      - | 3439 | `	}` |
|     15 | 3440 | `	return PH7_OK;` |
|    100 | 3441 | `}` |
|      - | 3442 | `/*` |
|      - | 3443 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3444 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 3445 | ` * Parameters` |
|      - | 3446 | ` *  $haystack` |
|      - | 3447 | ` *   The input string.` |
|      - | 3448 | ` * $needle` |
|      - | 3449 | ` *   Search pattern (must be a string).` |
|      - | 3450 | ` * $offset` |
|      - | 3451 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3452 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3453 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3454 | ` * Return` |
|      - | 3455 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3456 | ` */` |
|     62 | 3457 | `PH7_PRIVATE int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3458 | `{` |
|      - | 3459 | `	const char *zBlob,*zPattern;` |
|     63 | 3460 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3461 | `	int nLen,nPatLen,i;` |
|     63 | 3462 | `	int nMin = 0,nMax = 0;` |
|      - | 3463 | `	sxu32 nOfft;` |
|      - | 3464 | `	sxi32 rc;` |
|     63 | 3465 | `	if( nArg < 2 ){` |
|      - | 3466 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3467 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3468 | `		return PH7_OK;` |
|      - | 3469 | `	}` |
|      - | 3470 | `	/* Extract the needle and the haystack */` |
|     63 | 3471 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     63 | 3472 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     63 | 3473 | `	nOfft = 0; /* cc warning */` |
|      - | 3474 | `	/* Resolve the range of positions the match may start at */` |
|     63 | 3475 | `	rc = StrRSearchWindow(pCtx,nArg > 2 ? apArg[2] : 0,nLen,nPatLen,"strrpos",&nMin,&nMax);` |
|     63 | 3476 | `	if( rc != PH7_OK ){` |
|      5 | 3477 | `		return rc;` |
|      - | 3478 | `	}` |
|     59 | 3479 | `	if( nPatLen < 1 ){` |
|      - | 3480 | `		/* php 8: the empty needle matches everywhere, so the LAST match is the` |
|      - | 3481 | `		 * highest position the window allows. */` |
|     11 | 3482 | `		ph7_result_int64(pCtx,(ph7_int64)nMax);` |
|     11 | 3483 | `		return PH7_OK;` |
|      - | 3484 | `	}` |
|      - | 3485 | `	/* Walk backwards, comparing at each candidate position. Searching a window` |
|      - | 3486 | `	 * exactly as long as the needle makes the match test an equality test while` |
|      - | 3487 | `	 * still going through xPatternMatch, which carries the case folding. */` |
|    301 | 3488 | `	for( i = nMax ; i >= nMin ; --i ){` |
|    285 | 3489 | `		rc = xPatternMatch((const void *)&zBlob[i],(sxu32)nPatLen,(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    285 | 3490 | `		if( rc == SXRET_OK ){` |
|      - | 3491 | `			/* Pattern found,return it's position */` |
|     33 | 3492 | `			ph7_result_int64(pCtx,(ph7_int64)i);` |
|     33 | 3493 | `			return PH7_OK;` |
|      - | 3494 | `		}` |
|    127 | 3495 | `	}` |
|      - | 3496 | `	/* Pattern not found,return FALSE */` |
|     17 | 3497 | `	ph7_result_bool(pCtx,0);` |
|     17 | 3498 | `	return PH7_OK;` |
|     32 | 3499 | `}` |
|      - | 3500 | `/*` |
|      - | 3501 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3502 | ` *  Case-insensitive strrpos.` |
|      - | 3503 | ` * Parameters` |
|      - | 3504 | ` *  $haystack` |
|      - | 3505 | ` *   The input string.` |
|      - | 3506 | ` * $needle` |
|      - | 3507 | ` *   Search pattern (must be a string).` |
|      - | 3508 | ` * $offset` |
|      - | 3509 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3510 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3511 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3512 | ` * Return` |
|      - | 3513 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3514 | ` */` |
|     34 | 3515 | `PH7_PRIVATE int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3516 | `{` |
|      - | 3517 | `	const char *zBlob,*zPattern;` |
|     35 | 3518 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3519 | `	int nLen,nPatLen,i;` |
|     35 | 3520 | `	int nMin = 0,nMax = 0;` |
|      - | 3521 | `	sxu32 nOfft;` |
|      - | 3522 | `	sxi32 rc;` |
|     35 | 3523 | `	if( nArg < 2 ){` |
|      - | 3524 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3525 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3526 | `		return PH7_OK;` |
|      - | 3527 | `	}` |
|      - | 3528 | `	/* Extract the needle and the haystack */` |
|     35 | 3529 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     35 | 3530 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     35 | 3531 | `	nOfft = 0; /* cc warning */` |
|      - | 3532 | `	/* Resolve the range of positions the match may start at */` |
|     35 | 3533 | `	rc = StrRSearchWindow(pCtx,nArg > 2 ? apArg[2] : 0,nLen,nPatLen,"strripos",&nMin,&nMax);` |
|     35 | 3534 | `	if( rc != PH7_OK ){` |
|      5 | 3535 | `		return rc;` |
|      - | 3536 | `	}` |
|     31 | 3537 | `	if( nPatLen < 1 ){` |
|      - | 3538 | `		/* php 8: the empty needle matches everywhere, so the LAST match is the` |
|      - | 3539 | `		 * highest position the window allows. */` |
|     11 | 3540 | `		ph7_result_int64(pCtx,(ph7_int64)nMax);` |
|     11 | 3541 | `		return PH7_OK;` |
|      - | 3542 | `	}` |
|      - | 3543 | `	/* Walk backwards, comparing at each candidate position (see strrpos). */` |
|     49 | 3544 | `	for( i = nMax ; i >= nMin ; --i ){` |
|     45 | 3545 | `		rc = xPatternMatch((const void *)&zBlob[i],(sxu32)nPatLen,(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     45 | 3546 | `		if( rc == SXRET_OK ){` |
|      - | 3547 | `			/* Pattern found,return it's position */` |
|     17 | 3548 | `			ph7_result_int64(pCtx,(ph7_int64)i);` |
|     17 | 3549 | `			return PH7_OK;` |
|      - | 3550 | `		}` |
|     15 | 3551 | `	}` |
|      - | 3552 | `	/* Pattern not found,return FALSE */` |
|      5 | 3553 | `	ph7_result_bool(pCtx,0);` |
|      5 | 3554 | `	return PH7_OK;` |
|     18 | 3555 | `}` |
|      - | 3556 | `/*` |
|      - | 3557 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3558 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3559 | ` * Parameters` |
|      - | 3560 | ` *  $haystack` |
|      - | 3561 | ` *   The input string.` |
|      - | 3562 | ` * $needle` |
|      - | 3563 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3564 | ` *  This behavior is different from that of strstr().` |
|      - | 3565 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3566 | ` *  as the ordinal value of a character.` |
|      - | 3567 | ` * Return` |
|      - | 3568 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3569 | ` */` |
|     24 | 3570 | `PH7_PRIVATE int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3571 | `{` |
|      - | 3572 | `	const char *zBlob;` |
|      - | 3573 | `	int nLen,c;` |
|     25 | 3574 | `	if( nArg < 2 ){` |
|      - | 3575 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3576 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3577 | `		return PH7_OK;` |
|      - | 3578 | `	}` |
|      - | 3579 | `	/* Extract the haystack */` |
|     25 | 3580 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 3581 | `	c = 0; /* cc warning */` |
|     25 | 3582 | `	if( nLen > 0 ){` |
|      - | 3583 | `		const char *zPattern;` |
|      - | 3584 | `		int nPatLen;` |
|      - | 3585 | `		sxu32 nOfft;` |
|      - | 3586 | `		sxi32 rc;` |
|      - | 3587 | `		/* php 8 casts the needle to string and uses only its first character.` |
|      - | 3588 | `		 * The old "if not a string, take it as an ordinal" reading was php 7` |
|      - | 3589 | `		 * behaviour, removed in php 8: strrchr("hello world",111) now looks for` |
|      - | 3590 | `		 * "1", not "o". An empty needle matches nothing. */` |
|     23 | 3591 | `		zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     23 | 3592 | `		if( nPatLen < 1 ){` |
|      3 | 3593 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3594 | `			return PH7_OK;` |
|      - | 3595 | `		}` |
|     21 | 3596 | `		c = zPattern[0];` |
|      - | 3597 | `		/* Perform the lookup */` |
|     21 | 3598 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3599 | `		if( rc != SXRET_OK ){` |
|      - | 3600 | `			/* No such entry,return FALSE */` |
|      9 | 3601 | `			ph7_result_bool(pCtx,0);` |
|      9 | 3602 | `			return PH7_OK;` |
|      - | 3603 | `		}` |
|      - | 3604 | `		/* Return the string portion */` |
|     13 | 3605 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      7 | 3606 | `	}else{` |
|      3 | 3607 | `		ph7_result_bool(pCtx,0);` |
|      - | 3608 | `	}` |
|     15 | 3609 | `	return PH7_OK;` |
|     13 | 3610 | `}` |
|      - | 3611 | `/*` |
|      - | 3612 | ` * string strrev(string $string)` |
|      - | 3613 | ` *  Reverse a string.` |
|      - | 3614 | ` * Parameters` |
|      - | 3615 | ` *  $string` |
|      - | 3616 | ` *   String to be reversed.` |
|      - | 3617 | ` * Return` |
|      - | 3618 | ` *  The reversed string.` |
|      - | 3619 | ` */` |
|      6 | 3620 | `PH7_PRIVATE int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3621 | `{` |
|      - | 3622 | `	const char *zIn,*zEnd;` |
|      - | 3623 | `	int nLen,c;` |
|      8 | 3624 | `	if( nArg < 1 ){` |
|      - | 3625 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3626 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3627 | `		return PH7_OK;` |
|      - | 3628 | `	}` |
|      - | 3629 | `	/* Extract the target string */` |
|      8 | 3630 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      8 | 3631 | `	if( nLen < 1 ){` |
|      - | 3632 | `		/* php answers the empty STRING here, not null */` |
|      3 | 3633 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3634 | `		return PH7_OK;` |
|      - | 3635 | `	}` |
|      - | 3636 | `	/* Perform the requested operation */` |
|      6 | 3637 | `	zEnd = &zIn[nLen - 1];` |
|      8 | 3638 | `	for(;;){` |
|     18 | 3639 | `		if( zEnd < zIn ){` |
|      - | 3640 | `			/* No more input to process */` |
|      6 | 3641 | `			break;` |
|      - | 3642 | `		}` |
|      - | 3643 | `		/* Append current character */` |
|     14 | 3644 | `		c = zEnd[0];` |
|     14 | 3645 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     14 | 3646 | `		zEnd--;` |
|      2 | 3647 | `	}` |
|      6 | 3648 | `	return PH7_OK;` |
|      5 | 3649 | `}` |
|      - | 3650 | `/*` |
|      - | 3651 | ` * string ucwords(string $string [, string $separators = " \t\r\n\f\v"])` |
|      - | 3652 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3653 | ` *  A word begins at the start of the string and after any character present in` |
|      - | 3654 | ` *  $separators. The default separators are the whitespace characters (space,` |
|      - | 3655 | ` *  horizontal tab, carriage return, newline, form-feed and vertical tab); an` |
|      - | 3656 | ` *  explicit $separators argument REPLACES them (an empty string leaves only the` |
|      - | 3657 | ` *  very first character upper-cased). Like PHP, this is byte-based: only ASCII` |
|      - | 3658 | ` *  bytes are upper-cased and a byte is a separator only if it appears in the set.` |
|      - | 3659 | ` * Parameters` |
|      - | 3660 | ` *  $string` |
|      - | 3661 | ` *   The input string.` |
|      - | 3662 | ` *  $separators` |
|      - | 3663 | ` *   The optional word-boundary characters.` |
|      - | 3664 | ` * Return` |
|      - | 3665 | ` *  The modified string.` |
|      - | 3666 | ` */` |
|     26 | 3667 | `PH7_PRIVATE int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3668 | `{` |
|      - | 3669 | `	const char *zIn;` |
|      - | 3670 | `	int nLen,i,iStart;` |
|      - | 3671 | `	char aDelim[256];` |
|     28 | 3672 | `	if( nArg < 1 ){` |
|      - | 3673 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3674 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3675 | `		return PH7_OK;` |
|      - | 3676 | `	}` |
|      - | 3677 | `	/* Build the separator membership table: an explicit $separators argument` |
|      - | 3678 | `	 * replaces the default whitespace set (an empty string clears it). */` |
|     28 | 3679 | `	SyZero(aDelim,(sxu32)sizeof(aDelim));` |
|     28 | 3680 | `	if( nArg > 1 ){` |
|      - | 3681 | `		int nDelim;` |
|      9 | 3682 | `		const char *zDelim = ph7_value_to_string(apArg[1],&nDelim);` |
|     17 | 3683 | `		for( i = 0 ; i < nDelim ; i++ ){` |
|      9 | 3684 | `			aDelim[(unsigned char)zDelim[i]] = 1;` |
|      5 | 3685 | `		}` |
|      5 | 3686 | `	}else{` |
|     20 | 3687 | `		aDelim[(unsigned char)' ']  = 1;` |
|     20 | 3688 | `		aDelim[(unsigned char)'\t'] = 1;` |
|     20 | 3689 | `		aDelim[(unsigned char)'\r'] = 1;` |
|     20 | 3690 | `		aDelim[(unsigned char)'\n'] = 1;` |
|     20 | 3691 | `		aDelim[(unsigned char)'\f'] = 1;` |
|     20 | 3692 | `		aDelim[(unsigned char)'\v'] = 1;` |
|      - | 3693 | `	}` |
|      - | 3694 | `	/* Extract the target string */` |
|     28 | 3695 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     28 | 3696 | `	if( nLen < 1 ){` |
|      - | 3697 | `		/* Empty string – match PHP semantics */` |
|      6 | 3698 | `		ph7_result_string(pCtx,"",0);` |
|      6 | 3699 | `		return PH7_OK;` |
|      - | 3700 | `	}` |
|      - | 3701 | `	/* Upper-case the first byte of each word (the leading byte, or any byte that` |
|      - | 3702 | `	 * follows a separator), appending the untouched runs in between verbatim. */` |
|     23 | 3703 | `	iStart = 0;` |
|    325 | 3704 | `	for( i = 0 ; i < nLen ; i++ ){` |
|    303 | 3705 | `		int c = (unsigned char)zIn[i];` |
|    303 | 3706 | `		if( (i == 0 \|\| aDelim[(unsigned char)zIn[i-1]]) && c < 0x80 && SyisLower(c) ){` |
|     55 | 3707 | `			char up = (char)SyToUpper(c);` |
|     55 | 3708 | `			if( i > iStart ){` |
|     37 | 3709 | `				ph7_result_string(pCtx,&zIn[iStart],i - iStart);` |
|     18 | 3710 | `			}` |
|     55 | 3711 | `			ph7_result_string(pCtx,&up,1);` |
|     55 | 3712 | `			iStart = i + 1;` |
|     27 | 3713 | `		}` |
|    152 | 3714 | `	}` |
|     23 | 3715 | `	if( nLen > iStart ){` |
|     23 | 3716 | `		ph7_result_string(pCtx,&zIn[iStart],nLen - iStart);` |
|     11 | 3717 | `	}` |
|     23 | 3718 | `	return PH7_OK;` |
|     15 | 3719 | `}` |
|      - | 3720 | `/*` |
|      - | 3721 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3722 | ` *  Returns input repeated multiplier times.` |
|      - | 3723 | ` * Parameters` |
|      - | 3724 | ` *  $string` |
|      - | 3725 | ` *   String to be repeated.` |
|      - | 3726 | ` * $multiplier` |
|      - | 3727 | ` *  Number of time the input string should be repeated.` |
|      - | 3728 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3729 | ` *  to 0, the function will return an empty string.` |
|      - | 3730 | ` * Return` |
|      - | 3731 | ` *  The repeated string.` |
|      - | 3732 | ` */` |
|  20496 | 3733 | `PH7_PRIVATE int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3734 | `{` |
|      - | 3735 | `	const char *zIn;` |
|      - | 3736 | `	int nLen;` |
|      - | 3737 | `	ph7_int64 nMul;` |
|      - | 3738 | `	int rc;` |
|  20499 | 3739 | `	if( nArg < 2 ){` |
|      - | 3740 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3741 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3742 | `		return PH7_OK;` |
|      - | 3743 | `	}` |
|      - | 3744 | `	/* Extract the target string */` |
|  20499 | 3745 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3746 | `	/* Resolve $times through the shared ZPP helper so a lossy float / float-string` |
|      - | 3747 | `	 * carries php's precision deprecation and NAN/INF/non-numeric fail with php's` |
|      - | 3748 | `	 * TypeError — a bare ph7_value_to_int64() coerced them silently. */` |
|      - | 3749 | `	{` |
|  20499 | 3750 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_repeat",2,"$times","int",&nMul);` |
|  20499 | 3751 | `		if( rcArg != PH7_OK ){` |
|      5 | 3752 | `			return rcArg;` |
|      - | 3753 | `		}` |
|      - | 3754 | `	}` |
|  20495 | 3755 | `	if( nMul < 0 ){` |
|      3 | 3756 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 3757 | `			"str_repeat(): Argument #2 ($times) must be greater than or equal to 0");` |
|      - | 3758 | `	}` |
|  20493 | 3759 | `	if( nLen < 1 \|\| nMul < 1 ){` |
|      - | 3760 | `		/* Empty input or a zero multiplier yields the empty string (PHP). */` |
|      5 | 3761 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 3762 | `		return PH7_OK;` |
|      - | 3763 | `	}` |
|      - | 3764 | `	/* Perform the requested operation */` |
| 235601 | 3765 | `	for(;;){` |
| 471205 | 3766 | `		if( !nMul ){` |
|  20489 | 3767 | `			break;` |
|      - | 3768 | `		}` |
|      - | 3769 | `		/* Append the copy */` |
| 450719 | 3770 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 450719 | 3771 | `		if( rc != PH7_OK ){` |
|      - | 3772 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 3773 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 3774 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 3775 | `		}` |
| 450719 | 3776 | `		nMul--;` |
|      3 | 3777 | `	}` |
|  20489 | 3778 | `	return PH7_OK;` |
|  10251 | 3779 | `}` |
|      - | 3780 | `/*` |
|      - | 3781 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3782 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3783 | ` * Parameters` |
|      - | 3784 | ` *  $string` |
|      - | 3785 | ` *   The input string.` |
|      - | 3786 | ` * $is_xhtml` |
|      - | 3787 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3788 | ` * Return` |
|      - | 3789 | ` *  The processed string.` |
|      - | 3790 | ` */` |
|      8 | 3791 | `PH7_PRIVATE int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3792 | `{` |
|      - | 3793 | `	const char *zIn,*zCur,*zEnd;` |
|     10 | 3794 | `	int is_xhtml = 1; /* Default to XHTML-style '<br />' like PHP */` |
|      - | 3795 | `	int nLen;` |
|     10 | 3796 | `	if( nArg < 1 ){` |
|      - | 3797 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 3798 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3799 | `		return PH7_OK;` |
|      - | 3800 | `	}` |
|      - | 3801 | `	/* Extract the target string */` |
|     10 | 3802 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     10 | 3803 | `	if( nLen < 1 ){` |
|      - | 3804 | `		/* php answers the empty STRING here, not null */` |
|      3 | 3805 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3806 | `		return PH7_OK;` |
|      - | 3807 | `	}` |
|      8 | 3808 | `	if( nArg > 1 ){` |
|      3 | 3809 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3810 | `	}` |
|      8 | 3811 | `	zEnd = &zIn[nLen];` |
|      - | 3812 | `	/* Perform the requested operation */` |
|      6 | 3813 | `	for(;;){` |
|     14 | 3814 | `		zCur = zIn;` |
|      - | 3815 | `		/* Delimit the string */` |
|     32 | 3816 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|     14 | 3817 | `			zIn++;` |
|      2 | 3818 | `		}` |
|     14 | 3819 | `		if( zCur < zIn ){` |
|      - | 3820 | `			/* Output chunk verbatim */` |
|     14 | 3821 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      6 | 3822 | `		}` |
|     14 | 3823 | `		if( zIn >= zEnd ){` |
|      - | 3824 | `			/* No more input to process */` |
|      8 | 3825 | `			break;` |
|      - | 3826 | `		}` |
|      - | 3827 | `		/* Output the HTML line break */` |
|      - | 3828 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br />' (space before the slash), otherwise use '<br>' */` |
|      8 | 3829 | `		if( is_xhtml ){` |
|      6 | 3830 | `			ph7_result_string(pCtx,"<br />",(int)sizeof("<br />")-1);` |
|      4 | 3831 | `		}else{` |
|      3 | 3832 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3833 | `		}` |
|      8 | 3834 | `		zCur = zIn;` |
|      - | 3835 | `		/* Append trailing line */` |
|     17 | 3836 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      8 | 3837 | `			zIn++;` |
|      2 | 3838 | `		}` |
|      8 | 3839 | `		if( zCur < zIn ){` |
|      - | 3840 | `			/* Output chunk verbatim */` |
|      8 | 3841 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      3 | 3842 | `		}` |
|      2 | 3843 | `	}` |
|      8 | 3844 | `	return PH7_OK;` |
|      6 | 3845 | `}` |
|      - | 3846 | `/*` |
|      - | 3847 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3848 | ` *  According to the PHP reference manual.` |
|      - | 3849 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3850 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3851 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3852 | ` * This applies to both sprintf() and printf().` |
|      - | 3853 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3854 | ` * or more of these elements, in order:` |
|      - | 3855 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3856 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3857 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 3858 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 3859 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 3860 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 3861 | ` *   it with a single quote ('). See the examples below.` |
|      - | 3862 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 3863 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 3864 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 3865 | ` *   should result in.` |
|      - | 3866 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 3867 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 3868 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 3869 | ` *   limit to the string.` |
|      - | 3870 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 3871 | ` *       % - a literal percent character. No argument is required.` |
|      - | 3872 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 3873 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 3874 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 3875 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 3876 | ` * 	     for the number of digits after the decimal point.` |
|      - | 3877 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 3878 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 3879 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 3880 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 3881 | ` *       g - shorter of %e and %f.` |
|      - | 3882 | ` *       G - shorter of %E and %f.` |
|      - | 3883 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 3884 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 3885 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 3886 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 3887 | ` */` |
|      - | 3888 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3889 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 3890 | `/*` |
|      - | 3891 | ` * Symisc eXtension.` |
|      - | 3892 | ` * string size_format(int64 $size)` |
|      - | 3893 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 3894 | ` *  Example:` |
|      - | 3895 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 3896 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 3897 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 3898 | ` * Parameter` |
|      - | 3899 | ` *  $size` |
|      - | 3900 | ` *    Entity size in bytes.` |
|      - | 3901 | ` * Return` |
|      - | 3902 | ` *   Formatted string representation of the given size.` |
|      - | 3903 | ` */` |
|     24 | 3904 | `PH7_PRIVATE int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3905 | `{` |
|      - | 3906 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 3907 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 3908 | `	sxi32 nRest,i_32;` |
|      - | 3909 | `	ph7_int64 iSize;` |
|     25 | 3910 | `	int c = -1; /* index in zUnit[] */` |
|      - | 3911 |  |
|     25 | 3912 | `	if( nArg < 1 ){` |
|      - | 3913 | `		/* Missing argument,return the empty string */` |
|      3 | 3914 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3915 | `		return PH7_OK;` |
|      - | 3916 | `	}` |
|      - | 3917 | `	/* Extract the given size */` |
|     23 | 3918 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 3919 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 3920 | `		/* Don't bother formatting,return immediately */` |
|      5 | 3921 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 3922 | `		return PH7_OK;` |
|      - | 3923 | `	}` |
|     19 | 3924 | `	for(;;){` |
|     39 | 3925 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 3926 | `		iSize >>= 10;` |
|     39 | 3927 | `		c++;` |
|     39 | 3928 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 3929 | `			break;` |
|      - | 3930 | `		}` |
|      1 | 3931 | `	}` |
|     19 | 3932 | `	nRest /= 100;` |
|     19 | 3933 | `	if( nRest > 9 ){` |
|    ! 0 | 3934 | `		nRest = 9;` |
|    ! 0 | 3935 | `	}` |
|     19 | 3936 | `	if( iSize > 999 ){` |
|    ! 0 | 3937 | `		c++;` |
|    ! 0 | 3938 | `		nRest = 9;` |
|    ! 0 | 3939 | `		iSize = 0;` |
|    ! 0 | 3940 | `	}` |
|     19 | 3941 | `	i_32 = (sxi32)iSize;` |
|      - | 3942 | `	/* Format */` |
|     19 | 3943 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 3944 | `	return PH7_OK;` |
|     13 | 3945 | `}` |
|      - | 3946 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3947 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 3948 | `/*` |
|      - | 3949 | ` * string str_shuffle(string $str)` |
|      - | 3950 |  |
|      - | 3951 | ` *  Randomly shuffles a string.` |
|      - | 3952 | ` * Parameters` |
|      - | 3953 | ` *  $str` |
|      - | 3954 | ` *   The input string.` |
|      - | 3955 | ` * Return` |
|      - | 3956 | ` *  Returns the shuffled string.` |
|      - | 3957 | ` */` |
|     10 | 3958 | `PH7_PRIVATE int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3959 | `{` |
|      - | 3960 | `	const char *zString;` |
|      - | 3961 | `	int nLen,i,c;` |
|      - | 3962 | `	sxu32 iR;` |
|     11 | 3963 | `	if( nArg < 1 ){` |
|      - | 3964 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 3965 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3966 | `		return PH7_OK;` |
|      - | 3967 | `	}` |
|      - | 3968 | `	/* Extract the target string */` |
|     11 | 3969 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 3970 | `	if( nLen < 1 ){` |
|      - | 3971 | `		/* Nothing to shuffle */` |
|      3 | 3972 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3973 | `		return PH7_OK;` |
|      - | 3974 | `	}` |
|      - | 3975 | `	/* Shuffle the string. Draw through the MT19937 generator so str_shuffle()` |
|      - | 3976 | `	 * responds to srand()/mt_srand() (reproducible under a seed), like php; the` |
|      - | 3977 | `	 * sampling differs from php's Fisher-Yates so it is not value-parity. */` |
|     43 | 3978 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 3979 | `		/* Generate a random number first */` |
|     35 | 3980 | `		iR = PH7_VmMtRand(pCtx->pVm);` |
|      - | 3981 | `		/* Extract a random offset */` |
|     35 | 3982 | `		c = zString[iR % nLen];` |
|      - | 3983 | `		/* Append it */` |
|     35 | 3984 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 3985 | `	}` |
|      9 | 3986 | `	return PH7_OK;` |
|      6 | 3987 | `}` |
|      - | 3988 | `/*` |
|      - | 3989 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 3990 | ` *  Convert a string to an array.` |
|      - | 3991 | ` * Parameters` |
|      - | 3992 | ` * $string` |
|      - | 3993 | ` *  The input string.` |
|      - | 3994 | ` * $split_length` |
|      - | 3995 | ` *  Maximum length of the chunk.` |
|      - | 3996 | ` * Return` |
|      - | 3997 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 3998 | ` *  except possibly the last one which may be shorter.` |
|      - | 3999 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 4000 | ` *  as the first (and only) array element.` |
|      - | 4001 | ` *  An empty string returns an empty array.` |
|      - | 4002 | ` * Errors` |
|      - | 4003 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 4004 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 4005 | ` *  ValueError if $split_length is less than 1.` |
|      - | 4006 | ` */` |
|     26 | 4007 | `PH7_PRIVATE int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4008 | `{` |
|      - | 4009 | `	const char *zString,*zEnd;` |
|      - | 4010 | `	ph7_value *pArray,*pValue;` |
|      - | 4011 | `	int split_len;` |
|      - | 4012 | `	int nLen;` |
|     29 | 4013 | `	if( nArg < 1 ){` |
|    ! 0 | 4014 | `		return PH7_VmThrowException(pCtx,` |
|      - | 4015 | `			"ArgumentCountError",` |
|      - | 4016 | `			"str_split() expects at least 1 argument, %d given",` |
|    ! 0 | 4017 | `			nArg` |
|      - | 4018 | `			);` |
|      - | 4019 | `	}` |
|      - | 4020 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     39 | 4021 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     42 | 4022 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     26 | 4023 | `	    ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 4024 | `		return PH7_VmThrowException(pCtx,` |
|      - | 4025 | `			"TypeError",` |
|      - | 4026 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 4027 | `			ph7_type_name(apArg[0])` |
|      - | 4028 | `			);` |
|      - | 4029 | `	}` |
|      - | 4030 | `	/* Point to the target string */` |
|     29 | 4031 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     29 | 4032 | `	split_len = (int)sizeof(char);` |
|     29 | 4033 | `	if( nArg > 1 ){` |
|      - | 4034 | `		/* Split length */` |
|     17 | 4035 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 4036 | `		if( split_len < 1 ){` |
|      6 | 4037 | `			return PH7_VmThrowException(pCtx,` |
|      - | 4038 | `				"ValueError",` |
|      - | 4039 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 4040 | `				);` |
|      - | 4041 | `		}` |
|     11 | 4042 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 4043 | `			split_len = nLen;` |
|      1 | 4044 | `		}` |
|      5 | 4045 | `	}` |
|      - | 4046 | `	/* Create the array and the scalar value */` |
|     23 | 4047 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 4048 | `	/*Chunk value */` |
|     23 | 4049 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     23 | 4050 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 4051 | `		/* Return FALSE */` |
|    ! 0 | 4052 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4053 | `		return PH7_OK;` |
|      - | 4054 | `	}` |
|      - | 4055 | `	/* Point to the end of the string */` |
|     23 | 4056 | `	zEnd = &zString[nLen];` |
|      - | 4057 | `	/* Perform the requested operation */` |
|    131 | 4058 | `	for(;;){` |
|      - | 4059 | `		int nMax;` |
|    143 | 4060 | `		if( zString >= zEnd ){` |
|      - | 4061 | `			/* No more input to process */` |
|     23 | 4062 | `			break;` |
|      - | 4063 | `		}` |
|    121 | 4064 | `		nMax = (int)(zEnd-zString);` |
|    121 | 4065 | `		if( nMax < split_len ){` |
|      3 | 4066 | `			split_len = nMax;` |
|      1 | 4067 | `		}` |
|      - | 4068 | `		/* Copy the current chunk */` |
|    121 | 4069 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 4070 | `		/* Insert it */` |
|    121 | 4071 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 4072 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 4073 | `		}` |
|      - | 4074 | `		/* reset the string cursor */` |
|    121 | 4075 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 4076 | `		/* Update position */` |
|    121 | 4077 | `		zString += split_len;` |
|      1 | 4078 | `	}` |
|      - | 4079 | `	/*` |
|      - | 4080 | `	 * Return the array.` |
|      - | 4081 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 4082 | `	 * upon we return from this function.` |
|      - | 4083 | `	 */` |
|     23 | 4084 | `	ph7_result_value(pCtx,pArray);` |
|     23 | 4085 | `	return PH7_OK;` |
|     16 | 4086 | `}` |
|      - | 4087 | `/*` |
|      - | 4088 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 4089 | ` * return the longest match.` |
|      - | 4090 | ` * Refer to [strspn()].` |
|      - | 4091 | ` */` |
|     66 | 4092 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 4093 | `{` |
|     67 | 4094 | `	const char *zEnd = &zString[nLen];` |
|     67 | 4095 | `	const char *zIn = zString;` |
|      - | 4096 | `	int i,c;` |
|    110 | 4097 | `	for(;;){` |
|    221 | 4098 | `		if( zString >= zEnd ){` |
|     45 | 4099 | `			break;` |
|      - | 4100 | `		}` |
|      - | 4101 | `		/* Extract current character */` |
|    177 | 4102 | `		c = zString[0];` |
|      - | 4103 | `		/* Perform the lookup */` |
|    589 | 4104 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    567 | 4105 | `			if( c == zMask[i] ){` |
|      - | 4106 | `				/* Character found */` |
|    155 | 4107 | `				break;` |
|      - | 4108 | `			}` |
|    207 | 4109 | `		}` |
|    177 | 4110 | `		if( i >= nMaskLen ){` |
|      - | 4111 | `			/* Character not in the current mask,break immediately */` |
|     23 | 4112 | `			break;` |
|      - | 4113 | `		}` |
|      - | 4114 | `		/* Advance cursor */` |
|    155 | 4115 | `		zString++;` |
|      1 | 4116 | `	}` |
|      - | 4117 | `	/* Longest match */` |
|     67 | 4118 | `	return (int)(zString-zIn);` |
|      1 | 4119 | `}` |
|      - | 4120 | `/*` |
|      - | 4121 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 4122 | ` * Refer to [strcspn()].` |
|      - | 4123 | ` */` |
|     48 | 4124 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 4125 | `{` |
|     49 | 4126 | `	const char *zEnd = &zString[nLen];` |
|     49 | 4127 | `	const char *zIn = zString;` |
|      - | 4128 | `	int i,c;` |
|     81 | 4129 | `	for(;;){` |
|    163 | 4130 | `		if( zString >= zEnd ){` |
|     39 | 4131 | `			break;` |
|      - | 4132 | `		}` |
|      - | 4133 | `		/* Extract current character */` |
|    125 | 4134 | `		c = zString[0];` |
|      - | 4135 | `		/* Perform the lookup */` |
|    217 | 4136 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    103 | 4137 | `			if( c == zMask[i] ){` |
|     11 | 4138 | `				break;` |
|      - | 4139 | `			}` |
|     47 | 4140 | `		}` |
|    125 | 4141 | `		if( i < nMaskLen ){` |
|      - | 4142 | `			/* Character in the current mask,break immediately */` |
|     11 | 4143 | `			break;` |
|      - | 4144 | `		}` |
|      - | 4145 | `		/* Advance cursor */` |
|    115 | 4146 | `		zString++;` |
|      1 | 4147 | `	}` |
|      - | 4148 | `	/* Longest match */` |
|     49 | 4149 | `	return (int)(zString-zIn);` |
|      1 | 4150 | `}` |
|      - | 4151 | `/*` |
|      - | 4152 | ` * Shared body of strspn()/strcspn(): resolve php's ($offset,$length) window over` |
|      - | 4153 | ` * $string, then measure the span from the window's first byte.` |
|      - | 4154 | ` *` |
|      - | 4155 | ` * php's window rules (ext/standard/string.c, php_spn_common_handler) — a negative` |
|      - | 4156 | ` * $offset counts back from the end and CLAMPS to 0 (it is never "invalid"); an` |
|      - | 4157 | ` * $offset past the end clamps to the end, so the window is empty and the answer is` |
|      - | 4158 | ` * 0; a negative $length leaves that many bytes off the end of the remaining span` |
|      - | 4159 | ` * and clamps to 0; a zero-length window answers 0. PH7 answered 0 for a negative` |
|      - | 4160 | ` * offset that reached past the start, IGNORED a zero or negative $length entirely` |
|      - | 4161 | ` * (measuring the whole rest of the string instead), and truncated the offset to` |
|      - | 4162 | `` * `int`, so a 64-bit offset wrapped into a valid one.`` |
|      - | 4163 | ` *` |
|      - | 4164 | ` * PH7 also ran the scan over the first WHITESPACE-DELIMITED TOKEN rather than over` |
|      - | 4165 | ` * the raw window (leading spaces skipped, scan stopped at the next space), so` |
|      - | 4166 | ` * strspn("a b c","abc ") answered 1 where php answers 5 and strspn("  abc","abc")` |
|      - | 4167 | ` * answered 3 where php answers 0 — silent wrong answers on ordinary input. php` |
|      - | 4168 | ` * scans raw bytes; so does this.` |
|      - | 4169 | ` *` |
|      - | 4170 | ` * An empty $mask needs no special case: the mask lookup fails for every byte, so` |
|      - | 4171 | ` * strspn stops at once (0) and strcspn runs to the end of the window (its length),` |
|      - | 4172 | ` * which is exactly what php answers.` |
|      - | 4173 | ` */` |
|    120 | 4174 | `static int StrSpnCommonHandler(` |
|      - | 4175 | `	ph7_context *pCtx,    /* Call context */` |
|      - | 4176 | `	int nArg,             /* Argument count */` |
|      - | 4177 | `	ph7_value **apArg,    /* Arguments */` |
|      - | 4178 | `	int bComplement       /* TRUE for strcspn() */` |
|      - | 4179 | `	)` |
|      1 | 4180 | `{` |
|    121 | 4181 | `	const char *zFunc = bComplement ? "strcspn" : "strspn";` |
|      - | 4182 | `	const char *zString,*zMask;` |
|      - | 4183 | `	int iMasklen,iLen;` |
|      - | 4184 | `	sxi64 iStart,iSpan;` |
|    121 | 4185 | `	if( nArg < 2 ){` |
|      - | 4186 | `		/* Arity is enforced at the call boundary; nothing sensible to return here. */` |
|    ! 0 | 4187 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4188 | `		return PH7_OK;` |
|      - | 4189 | `	}` |
|      - | 4190 | `	/* Extract the target string and the mask */` |
|    121 | 4191 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|    121 | 4192 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|    121 | 4193 | `	if( iLen < 0 ){` |
|    ! 0 | 4194 | `		iLen = 0;` |
|    ! 0 | 4195 | `	}` |
|    121 | 4196 | `	if( iMasklen < 0 ){` |
|    ! 0 | 4197 | `		iMasklen = 0;` |
|    ! 0 | 4198 | `	}` |
|    121 | 4199 | `	iStart = 0;` |
|    121 | 4200 | `	if( nArg > 2 ){` |
|     79 | 4201 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],zFunc,3,"$offset","int",&iStart);` |
|     79 | 4202 | `		if( rcArg != PH7_OK ){` |
|      5 | 4203 | `			return rcArg;` |
|      - | 4204 | `		}` |
|     75 | 4205 | `		if( iStart < 0 ){` |
|      - | 4206 | `			/* Count back from the end, clamped to the start (guarded so an` |
|      - | 4207 | `			 * INT64_MIN offset cannot overflow the addition). */` |
|     13 | 4208 | `			iStart = ( iStart < -(sxi64)iLen ) ? 0 : iStart + iLen;` |
|     69 | 4209 | `		}else if( iStart > (sxi64)iLen ){` |
|      9 | 4210 | `			iStart = iLen;` |
|      4 | 4211 | `		}` |
|     37 | 4212 | `	}` |
|    117 | 4213 | `	iSpan = (sxi64)iLen - iStart;` |
|    117 | 4214 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|     41 | 4215 | `		sxi64 iUserlen = 0;` |
|     41 | 4216 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[3],zFunc,4,"$length","?int",&iUserlen);` |
|     41 | 4217 | `		if( rcArg != PH7_OK ){` |
|      3 | 4218 | `			return rcArg;` |
|      - | 4219 | `		}` |
|     39 | 4220 | `		if( iUserlen < 0 ){` |
|      - | 4221 | `			/* Leave \|$length\| bytes off the end of the remaining span (guarded` |
|      - | 4222 | `			 * against an INT64_MIN underflow the same way). */` |
|     21 | 4223 | `			iSpan = ( iUserlen < -iSpan ) ? 0 : iSpan + iUserlen;` |
|     29 | 4224 | `		}else if( iUserlen < iSpan ){` |
|     11 | 4225 | `			iSpan = iUserlen;` |
|      5 | 4226 | `		}` |
|     19 | 4227 | `	}` |
|    172 | 4228 | `	ph7_result_int(pCtx,bComplement` |
|     48 | 4229 | `		? LongestStringMask2(&zString[iStart],(int)iSpan,zMask,iMasklen)` |
|     66 | 4230 | `		: LongestStringMask(&zString[iStart],(int)iSpan,zMask,iMasklen));` |
|    115 | 4231 | `	return PH7_OK;` |
|     61 | 4232 | `}` |
|      - | 4233 | `/*` |
|      - | 4234 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 4235 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 4236 | ` *  of characters contained within a given mask.` |
|      - | 4237 | ` * Parameters` |
|      - | 4238 | ` * $str` |
|      - | 4239 | ` *  The input string.` |
|      - | 4240 | ` * $mask` |
|      - | 4241 | ` *  The list of allowable characters.` |
|      - | 4242 | ` * $start` |
|      - | 4243 | ` *  The position in subject to start searching.` |
|      - | 4244 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 4245 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 4246 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 4247 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 4248 | ` *  start'th position from the end of subject.` |
|      - | 4249 | ` * $length` |
|      - | 4250 | ` *  The length of the segment from subject to examine.` |
|      - | 4251 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 4252 | ` *  characters after the starting position.` |
|      - | 4253 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 4254 | ` *  position up to length characters from the end of subject.` |
|      - | 4255 | ` * Return` |
|      - | 4256 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 4257 | ` * in mask.` |
|      - | 4258 | ` */` |
|     70 | 4259 | `PH7_PRIVATE int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4260 | `{` |
|     71 | 4261 | `	return StrSpnCommonHandler(pCtx,nArg,apArg,0);` |
|      1 | 4262 | `}` |
|      - | 4263 | `/*` |
|      - | 4264 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 4265 | ` *  Find length of initial segment not matching mask.` |
|      - | 4266 | ` * Parameters` |
|      - | 4267 | ` * $str` |
|      - | 4268 | ` *  The input string.` |
|      - | 4269 | ` * $mask` |
|      - | 4270 | ` *  The list of not allowed characters.` |
|      - | 4271 | ` * $start` |
|      - | 4272 | ` *  The position in subject to start searching.` |
|      - | 4273 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 4274 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 4275 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 4276 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 4277 | ` *  start'th position from the end of subject.` |
|      - | 4278 | ` * $length` |
|      - | 4279 | ` *  The length of the segment from subject to examine.` |
|      - | 4280 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 4281 | ` *  characters after the starting position.` |
|      - | 4282 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 4283 | ` *  position up to length characters from the end of subject.` |
|      - | 4284 | ` * Return` |
|      - | 4285 | ` *  Returns the length of the segment as an integer.` |
|      - | 4286 | ` */` |
|     50 | 4287 | `PH7_PRIVATE int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4288 | `{` |
|     51 | 4289 | `	return StrSpnCommonHandler(pCtx,nArg,apArg,1);` |
|      1 | 4290 | `}` |
|      - | 4291 | `/*` |
|      - | 4292 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 4293 | ` *  Search a string for any of a set of characters.` |
|      - | 4294 | ` * Parameters` |
|      - | 4295 | ` *  $haystack` |
|      - | 4296 | ` *   The string where char_list is looked for.` |
|      - | 4297 | ` *  $char_list` |
|      - | 4298 | ` *   This parameter is case sensitive.` |
|      - | 4299 | ` * Return` |
|      - | 4300 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 4301 | ` */` |
|     14 | 4302 | `PH7_PRIVATE int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4303 | `{` |
|      - | 4304 | `	const char *zString,*zList,*zEnd;` |
|      - | 4305 | `	int iLen,iListLen,i,c;` |
|      - | 4306 | `	sxu32 nOfft,nMax;` |
|      - | 4307 | `	sxi32 rc;` |
|     15 | 4308 | `	if( nArg < 2 ){` |
|      - | 4309 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 4310 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4311 | `		return PH7_OK;` |
|      - | 4312 | `	}` |
|      - | 4313 | `	/* Extract the haystack and the char list */` |
|     15 | 4314 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|     15 | 4315 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|     15 | 4316 | `	if( iListLen < 1 ){` |
|      - | 4317 | `		/* An empty set can never match, so php rejects it rather than answering` |
|      - | 4318 | `		 * a FALSE indistinguishable from "not found" (checked BEFORE the haystack,` |
|      - | 4319 | `		 * so strpbrk("","") throws too). */` |
|      5 | 4320 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4321 | `			"strpbrk(): Argument #2 ($characters) must be a non-empty string");` |
|      - | 4322 | `	}` |
|     11 | 4323 | `	if( iLen < 1 ){` |
|      - | 4324 | `		/* Nothing to process,return FALSE */` |
|      3 | 4325 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4326 | `		return PH7_OK;` |
|      - | 4327 | `	}` |
|      - | 4328 | `	/* Point to the end of the string */` |
|      9 | 4329 | `	zEnd = &zString[iLen];` |
|      9 | 4330 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 4331 | `	/* perform the requested operation */` |
|     25 | 4332 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     17 | 4333 | `		c = zList[i];` |
|     17 | 4334 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     17 | 4335 | `		if( rc == SXRET_OK ){` |
|      9 | 4336 | `			if( nMax < nOfft ){` |
|      5 | 4337 | `				nOfft = nMax;` |
|      2 | 4338 | `			}` |
|      4 | 4339 | `		}` |
|      9 | 4340 | `	}` |
|      9 | 4341 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 4342 | `		/* No such substring,return FALSE */` |
|      5 | 4343 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4344 | `	}else{` |
|      - | 4345 | `		/* Return the substring */` |
|      5 | 4346 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 4347 | `	}` |
|      9 | 4348 | `	return PH7_OK;` |
|      8 | 4349 | `}` |
|      - | 4350 | `/*` |
|      - | 4351 | ` * string soundex(string $str)` |
|      - | 4352 | ` *  Calculate the soundex key of a string.` |
|      - | 4353 | ` * Parameters` |
|      - | 4354 | ` *  $str` |
|      - | 4355 | ` *   The input string.` |
|      - | 4356 | ` * Return` |
|      - | 4357 | ` *  Returns the soundex key as a string.` |
|      - | 4358 | ` * Note:` |
|      - | 4359 | ` *  Knuth's algorithm as php implements it (ext/standard/soundex.c). The` |
|      - | 4360 | ` *  previous implementation came from SQLite and diverged from php on three` |
|      - | 4361 | ` *  counts, each of them a silent wrong answer:` |
|      - | 4362 | ` *` |
|      - | 4363 | ` *   - a NON-LETTER inside the word RESET the "same code in a row" state, so` |
|      - | 4364 | ` *     soundex("S s") answered S200 where php answers S000 and soundex("b1b")` |
|      - | 4365 | ` *     answered B100 where php answers B000. php simply skips anything that is` |
|      - | 4366 | ` *     not a letter; only a VOWEL separates two consonants sharing a code.` |
|      - | 4367 | ` *   - the scan stopped at the first byte >= 0xC0, taking every UTF-8 lead byte` |
|      - | 4368 | ` *     for a letter and copying it raw into the key: soundex("\xff\xfe") answered` |
|      - | 4369 | ` *     "\xff000" where php answers "0000", and a leading accent HID the letters` |
|      - | 4370 | ` *     behind it (soundex("éa") answered "\xc3000" for php's A000). Inside the` |
|      - | 4371 | `` *     loop the table was indexed by `byte & 0x7f`, which folds high bytes onto`` |
|      - | 4372 | ` *     ASCII letters and invents codes for them.` |
|      - | 4373 | ` *   - the input was walked as a NUL-terminated C string, so soundex("a\0b")` |
|      - | 4374 | ` *     stopped at the NUL (A000) where php walks the whole php string (A100).` |
|      - | 4375 | ` *` |
|      - | 4376 | ` *  Classification is ASCII-only, matching php's own A-Z table (§7's locale` |
|      - | 4377 | ` *  dependence family: the old code asked libc's isalpha() through SyisAlpha,` |
|      - | 4378 | ` *  which answers differently under a non-C LC_CTYPE).` |
|      - | 4379 | ` */` |
|     52 | 4380 | `PH7_PRIVATE int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4381 | `{` |
|      - | 4382 | `	/* Code per letter A-Z; 0 means "no code" (a vowel, plus H/W/Y) */` |
|      - | 4383 | `	static const char zCode[] = "01230120022455012623010202";` |
|      - | 4384 | `	const unsigned char *zIn;` |
|      - | 4385 | `	char zResult[4];` |
|     53 | 4386 | `	int nByte,i,nOut = 0,iLast = -1;` |
|     53 | 4387 | `	if( nArg < 1 ){` |
|      - | 4388 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4389 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4390 | `		return PH7_OK;` |
|      - | 4391 | `	}` |
|     53 | 4392 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nByte);` |
|    265 | 4393 | `	for( i = 0 ; i < nByte && nOut < 4 ; ++i ){` |
|    213 | 4394 | `		int c = zIn[i];` |
|      - | 4395 | `		int code;` |
|    213 | 4396 | `		if( c >= 'a' && c <= 'z' ){` |
|    139 | 4397 | `			c -= 'a' - 'A';` |
|     69 | 4398 | `		}` |
|    213 | 4399 | `		if( c < 'A' \|\| c > 'Z' ){` |
|     35 | 4400 | `			continue; /* not a letter: skipped outright, state untouched */` |
|      - | 4401 | `		}` |
|    179 | 4402 | `		code = zCode[c - 'A'] - '0';` |
|    179 | 4403 | `		if( nOut == 0 ){` |
|      - | 4404 | `			/* The key opens with the first letter itself */` |
|     43 | 4405 | `			zResult[nOut++] = (char)c;` |
|    158 | 4406 | `		}else if( code != iLast && code != 0 ){` |
|     69 | 4407 | `			zResult[nOut++] = (char)(code + '0');` |
|     34 | 4408 | `		}` |
|    179 | 4409 | `		iLast = code;` |
|     90 | 4410 | `	}` |
|      - | 4411 | `	/* Pad to four characters. A string with no letter at all pads from nothing,` |
|      - | 4412 | `	 * which is php's "0000" (an empty input included). */` |
|    151 | 4413 | `	while( nOut < 4 ){` |
|     99 | 4414 | `		zResult[nOut++] = '0';` |
|      1 | 4415 | `	}` |
|     53 | 4416 | `	ph7_result_string(pCtx,zResult,4);` |
|     53 | 4417 | `	return PH7_OK;` |
|     27 | 4418 | `}` |
|      - | 4419 | `/*` |
|      - | 4420 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 4421 | ` *  Wraps a string to a given number of characters.` |
|      - | 4422 | ` * Parameters` |
|      - | 4423 | ` *  $str` |
|      - | 4424 | ` *   The input string.` |
|      - | 4425 | ` * $width` |
|      - | 4426 | ` *  The column width.` |
|      - | 4427 | ` * $break` |
|      - | 4428 | ` *  The line is broken using the optional break parameter.` |
|      - | 4429 | ` * Return` |
|      - | 4430 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 4431 | ` */` |
|     28 | 4432 | `PH7_PRIVATE int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4433 | `{` |
|      - | 4434 | `	const char *zIn,*zBreak;` |
|      - | 4435 | `	SyBlob sWorker;` |
|      - | 4436 | `	int iLen,iBreaklen,iWidth,iCut,iStart,iSpace,iCur;` |
|      - | 4437 | `	sxi32 rc;` |
|     30 | 4438 | `	if( nArg < 1 ){` |
|      - | 4439 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4440 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4441 | `		return PH7_OK;` |
|      - | 4442 | `	}` |
|      - | 4443 | `	/* Extract the input string */` |
|     30 | 4444 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 4445 | `	/* Width (default 75; PHP allows 0/negative — break at every space). */` |
|     30 | 4446 | `	iWidth = 75;` |
|     30 | 4447 | `	if( nArg > 1 ){` |
|     27 | 4448 | `		iWidth = ph7_value_to_int(apArg[1]);` |
|     13 | 4449 | `	}` |
|      - | 4450 | `	/* Break string (default "\n"). */` |
|     30 | 4451 | `	zBreak = "\n";` |
|     30 | 4452 | `	iBreaklen = (int)sizeof(char);` |
|     30 | 4453 | `	if( nArg > 2 ){` |
|     13 | 4454 | `		zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      6 | 4455 | `	}` |
|      - | 4456 | `	/* Cut long words? (default false). */` |
|     30 | 4457 | `	iCut = 0;` |
|     30 | 4458 | `	if( nArg > 3 ){` |
|      7 | 4459 | `		iCut = ph7_value_to_bool(apArg[3]);` |
|      3 | 4460 | `	}` |
|     30 | 4461 | `	if( iLen < 1 ){` |
|      - | 4462 | `		/* PHP returns the empty string for empty input before validating the other args. */` |
|      8 | 4463 | `		ph7_result_string(pCtx,"",0);` |
|      8 | 4464 | `		return PH7_OK;` |
|      - | 4465 | `	}` |
|      - | 4466 | `	/* PHP 8 domain errors (catchable ValueError). */` |
|     23 | 4467 | `	if( iBreaklen < 1 ){` |
|      3 | 4468 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4469 | `			"wordwrap(): Argument #3 ($break) must not be empty");` |
|      - | 4470 | `	}` |
|     21 | 4471 | `	if( iWidth == 0 && iCut ){` |
|      3 | 4472 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4473 | `			"wordwrap(): Argument #4 ($cut_long_words) cannot be true when argument #2 ($width) is 0");` |
|      - | 4474 | `	}` |
|      - | 4475 | `	/*` |
|      - | 4476 | `	 * PHP's algorithm: a single left-to-right pass tracking the start of the` |
|      - | 4477 | `	 * current line (iStart) and the position of the last space seen on it` |
|      - | 4478 | `	 * (iSpace). A break is emitted when the line reaches the width, at the last` |
|      - | 4479 | `	 * space if there was one, otherwise (only when cut is enabled) hard at the` |
|      - | 4480 | `	 * boundary. An existing break sequence in the input resets the line.` |
|      - | 4481 | `	 */` |
|     19 | 4482 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     19 | 4483 | `	iStart = iSpace = iCur = 0;` |
|     19 | 4484 | `	rc = SXRET_OK;` |
|    551 | 4485 | `	while( iCur < iLen ){` |
|    533 | 4486 | `		if( iBreaklen <= iLen - iCur && SyMemcmp(&zIn[iCur],zBreak,(sxu32)iBreaklen) == 0 ){` |
|      - | 4487 | `			/* Existing break sequence in the input: copy it verbatim and reset the line. */` |
|    ! 0 | 4488 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart + iBreaklen));` |
|    ! 0 | 4489 | `			if( rc != SXRET_OK ){ goto oom; }` |
|    ! 0 | 4490 | `			iCur += iBreaklen;` |
|    ! 0 | 4491 | `			iStart = iSpace = iCur;` |
|    ! 0 | 4492 | `			continue;` |
|    533 | 4493 | `		}else if( zIn[iCur] == ' ' ){` |
|     67 | 4494 | `			if( iCur - iStart >= iWidth ){` |
|      - | 4495 | `				/* The line already fills the width at this space: break here (the space is consumed). */` |
|     13 | 4496 | `				rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     13 | 4497 | `				if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     13 | 4498 | `				if( rc != SXRET_OK ){ goto oom; }` |
|     13 | 4499 | `				iStart = iCur + 1;` |
|      6 | 4500 | `			}` |
|     67 | 4501 | `			iSpace = iCur;` |
|    500 | 4502 | `		}else if( iCut && iCur - iStart >= iWidth && iStart >= iSpace ){` |
|      - | 4503 | `			/* A word longer than the width with no space to break at: hard-cut at the boundary. */` |
|      7 | 4504 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|      7 | 4505 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|      7 | 4506 | `			if( rc != SXRET_OK ){ goto oom; }` |
|      7 | 4507 | `			iStart = iSpace = iCur;` |
|    464 | 4508 | `		}else if( iCur - iStart >= iWidth && iStart < iSpace ){` |
|      - | 4509 | `			/* Past the width mid-word: wrap back to the last space (which is consumed). */` |
|     17 | 4510 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iSpace - iStart));` |
|     17 | 4511 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     17 | 4512 | `			if( rc != SXRET_OK ){ goto oom; }` |
|     17 | 4513 | `			iStart = iSpace = iSpace + 1;` |
|      8 | 4514 | `		}` |
|    533 | 4515 | `		iCur++;` |
|      1 | 4516 | `	}` |
|      - | 4517 | `	/* Emit the trailing chunk. */` |
|     19 | 4518 | `	if( iStart < iCur ){` |
|     19 | 4519 | `		rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     19 | 4520 | `		if( rc != SXRET_OK ){ goto oom; }` |
|      9 | 4521 | `	}` |
|     19 | 4522 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|     19 | 4523 | `	SyBlobRelease(&sWorker);` |
|     19 | 4524 | `	return PH7_OK;` |
|    ! 0 | 4525 | `oom:` |
|    ! 0 | 4526 | `	SyBlobRelease(&sWorker);` |
|    ! 0 | 4527 | `	return PH7_ContextMemoryError(pCtx);` |
|     16 | 4528 | `}` |
|      - | 4529 | `/*` |
|      - | 4530 | ` * Check if the given character is a member of the given mask.` |
|      - | 4531 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 4532 | ` * Refer to [strtok()].` |
|      - | 4533 | ` */` |
|    132 | 4534 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 4535 | `{` |
|      - | 4536 | `	int i;` |
|    263 | 4537 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|    157 | 4538 | `		if( c == zMask[i] ){` |
|     27 | 4539 | `			if( pOfft ){` |
|     19 | 4540 | `				*pOfft = i;` |
|      9 | 4541 | `			}` |
|     27 | 4542 | `			return TRUE;` |
|      - | 4543 | `		}` |
|     66 | 4544 | `	}` |
|    107 | 4545 | `	return FALSE;` |
|     67 | 4546 | `}` |
|      - | 4547 | `/*` |
|      - | 4548 | ` * Extract a single token from the input stream.` |
|      - | 4549 | ` * Refer to [strtok()].` |
|      - | 4550 | ` */` |
|      6 | 4551 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 4552 | `{` |
|      7 | 4553 | `	const char *zIn = *pzIn;` |
|      - | 4554 | `	const char *zPtr;` |
|      - | 4555 | `	/* Ignore leading delimiter */` |
|     11 | 4556 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 4557 | `		zIn++;` |
|      1 | 4558 | `	}` |
|      7 | 4559 | `	if( zIn >= zEnd ){` |
|      - | 4560 | `		/* End of input */` |
|    ! 0 | 4561 | `		return SXERR_EOF;` |
|      - | 4562 | `	}` |
|      7 | 4563 | `	zPtr = zIn;` |
|      - | 4564 | `	/* Extract the token */` |
|     13 | 4565 | `	while( zIn < zEnd ){` |
|     11 | 4566 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 4567 | `			/* UTF-8 stream */` |
|    ! 0 | 4568 | `			zIn++;` |
|    ! 0 | 4569 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 4570 | `		}else{` |
|     11 | 4571 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 4572 | `				break;` |
|      - | 4573 | `			}` |
|      7 | 4574 | `			zIn++;` |
|      - | 4575 | `		}` |
|      1 | 4576 | `	}` |
|      7 | 4577 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 4578 | `	/* Update the cursor */` |
|      7 | 4579 | `	*pzIn = zIn;` |
|      - | 4580 | `	/* Return to the caller */` |
|      7 | 4581 | `	return SXRET_OK;` |
|      4 | 4582 | `}` |
|      - | 4583 | `/* strtok auxiliary private data */` |
|      - | 4584 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 4585 | `struct strtok_aux_data` |
|      - | 4586 | `{` |
|      - | 4587 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 4588 | `	const char *zIn;   /* Current input stream */` |
|      - | 4589 | `	const char *zEnd;  /* End of input */` |
|      - | 4590 | `};` |
|      - | 4591 | `/*` |
|      - | 4592 | ` * string strtok(string $str,string $token)` |
|      - | 4593 | ` * string strtok(string $token)` |
|      - | 4594 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 4595 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 4596 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 4597 | ` *  words by using the space character as the token.` |
|      - | 4598 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 4599 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 4600 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 4601 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 4602 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 4603 | ` *  the argument are found.` |
|      - | 4604 | ` * Parameters` |
|      - | 4605 | ` *  $str` |
|      - | 4606 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 4607 | ` * $token` |
|      - | 4608 | ` *  The delimiter used when splitting up str.` |
|      - | 4609 | ` * Return` |
|      - | 4610 | ` *   Current token or FALSE on EOF.` |
|      - | 4611 | ` */` |
|      6 | 4612 | `PH7_PRIVATE int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4613 | `{` |
|      - | 4614 | `	strtok_aux_data *pAux;` |
|      - | 4615 | `	const char *zMask;` |
|      - | 4616 | `	SyString sToken;` |
|      - | 4617 | `	int nMasklen;` |
|      - | 4618 | `	sxi32 rc;` |
|      7 | 4619 | `	if( nArg < 2 ){` |
|      - | 4620 | `		/* Extract top aux data */` |
|      5 | 4621 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      5 | 4622 | `		if( pAux == 0 ){` |
|      - | 4623 | `			/* No aux data,return FALSE */` |
|    ! 0 | 4624 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 4625 | `			return PH7_OK;` |
|      - | 4626 | `		}` |
|      5 | 4627 | `		nMasklen = 0;` |
|      5 | 4628 | `		zMask = ""; /* cc warning */` |
|      5 | 4629 | `		if( nArg > 0 ){` |
|      - | 4630 | `			/* Extract the mask */` |
|      5 | 4631 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 4632 | `		}` |
|      5 | 4633 | `		if( nMasklen < 1 ){` |
|      - | 4634 | `			/* Invalid mask,return FALSE */` |
|    ! 0 | 4635 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 4636 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 4637 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 4638 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 4639 | `			return PH7_OK;` |
|      - | 4640 | `		}` |
|      - | 4641 | `		/* Extract the token */` |
|      5 | 4642 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 4643 | `		if( rc != SXRET_OK ){` |
|      - | 4644 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 4645 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 4646 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 4647 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 4648 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 4649 | `		}else{` |
|      - | 4650 | `			/* Return the extracted token */` |
|      5 | 4651 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 4652 | `		}` |
|      3 | 4653 | `	}else{` |
|      - | 4654 | `		const char *zInput,*zCur;` |
|      - | 4655 | `		char *zDup;` |
|      - | 4656 | `		int nLen;` |
|      - | 4657 | `		/* Extract the raw input */` |
|      3 | 4658 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4659 | `		if( nLen < 1 ){` |
|      - | 4660 | `			/* Empty input,return FALSE */` |
|    ! 0 | 4661 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 4662 | `			return PH7_OK;` |
|      - | 4663 | `		}` |
|      - | 4664 | `		/* Extract the mask */` |
|      3 | 4665 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 4666 | `		if( nMasklen < 1 ){` |
|      - | 4667 | `			/* Set a default mask */` |
|      - | 4668 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 4669 | `			zMask = TOK_MASK;` |
|    ! 0 | 4670 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 4671 | `#undef TOK_MASK` |
|    ! 0 | 4672 | `		}` |
|      - | 4673 | `		/* Extract a single token */` |
|      3 | 4674 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 4675 | `		if( rc != SXRET_OK ){` |
|      - | 4676 | `			/* Empty input */` |
|    ! 0 | 4677 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 4678 | `			return PH7_OK;` |
|    ! 0 | 4679 | `		}else{` |
|      - | 4680 | `			/* Return the extracted token */` |
|      3 | 4681 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 4682 | `		}` |
|      - | 4683 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 4684 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 4685 | `		if( pAux ){` |
|      3 | 4686 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 4687 | `			if( nLen < 1 ){` |
|    ! 0 | 4688 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 4689 | `				return PH7_OK;` |
|      - | 4690 | `			}` |
|      - | 4691 | `			/* Duplicate input */` |
|      3 | 4692 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 4693 | `			if( zDup  ){` |
|      3 | 4694 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 4695 | `				/* Register the aux data */` |
|      3 | 4696 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 4697 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 4698 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 4699 | `			}` |
|      1 | 4700 | `		}` |
|      - | 4701 | `	}` |
|      7 | 4702 | `	return PH7_OK;` |
|      4 | 4703 | `}` |
|      - | 4704 | `/*` |
|      - | 4705 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 4706 | ` *  Pad a string to a certain length with another string` |
|      - | 4707 | ` * Parameters` |
|      - | 4708 | ` *  $input` |
|      - | 4709 | ` *   The input string.` |
|      - | 4710 | ` * $pad_length` |
|      - | 4711 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 4712 | ` *   string, no padding takes place.` |
|      - | 4713 | ` * $pad_string` |
|      - | 4714 | ` *   Note:` |
|      - | 4715 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 4716 | ` *    divided by the pad_string's length.` |
|      - | 4717 | ` * $pad_type` |
|      - | 4718 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 4719 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 4720 | ` * Return` |
|      - | 4721 | ` *  The padded string.` |
|      - | 4722 | ` */` |
|    456 | 4723 | `PH7_PRIVATE int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 4724 | `{` |
|      - | 4725 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 4726 | `	const char *zIn,*zPad;` |
|    460 | 4727 | `	if( nArg < 2 ){` |
|      - | 4728 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4729 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4730 | `		return PH7_OK;` |
|      - | 4731 | `	}` |
|      - | 4732 | `	/* Extract the target string */` |
|    460 | 4733 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 4734 | `	/* Padding length */` |
|      - | 4735 | `	{` |
|    460 | 4736 | `		sxi64 iTmp = 0;` |
|    460 | 4737 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_pad",2,"$length","int",&iTmp);` |
|    460 | 4738 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 4739 | `			return rcArg;` |
|      - | 4740 | `		}` |
|    460 | 4741 | `		iRealPad = iPadlen = (int)iTmp;` |
|      - | 4742 | `	}` |
|    460 | 4743 | `	if( iPadlen > 0 ){` |
|    458 | 4744 | `		iPadlen -= iLen;` |
|    227 | 4745 | `	}` |
|    460 | 4746 | `	if( iPadlen < 1  ){` |
|      - | 4747 | `		/* Return the string verbatim */` |
|      7 | 4748 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      7 | 4749 | `		return PH7_OK;` |
|      - | 4750 | `	}` |
|    454 | 4751 | `	zPad = " "; /* Whitespace padding */` |
|    454 | 4752 | `	iStrpad = (int)sizeof(char);` |
|    454 | 4753 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|    454 | 4754 | `	if( nArg > 2 ){` |
|      - | 4755 | `		/* Padding string */` |
|     17 | 4756 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|     17 | 4757 | `		if( iStrpad < 1 ){` |
|      - | 4758 | `			/* An empty pad string throws a catchable ValueError in PHP 8` |
|      - | 4759 | `			 * (only reached once padding is actually required). */` |
|      3 | 4760 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4761 | `				"str_pad(): Argument #3 ($pad_string) must not be empty");` |
|      - | 4762 | `		}` |
|     15 | 4763 | `		if( nArg > 3 ){` |
|      - | 4764 | `			/* Padd type. php 8: anything outside LEFT(0)/RIGHT(1)/BOTH(2) is a` |
|      - | 4765 | `			 * catchable ValueError (PHL used to fall back to RIGHT silently);` |
|      - | 4766 | `			 * like the empty-pad check above, php only reaches it once padding` |
|      - | 4767 | `			 * is actually required (probed: str_pad("abc",2," ",9) is "abc"). */` |
|     15 | 4768 | `			iType = ph7_value_to_int(apArg[3]);` |
|     15 | 4769 | `			if( iType < 0 \|\| iType > 2 ){` |
|      5 | 4770 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4771 | `					"str_pad(): Argument #4 ($pad_type) must be STR_PAD_LEFT, STR_PAD_RIGHT, or STR_PAD_BOTH");` |
|      - | 4772 | `			}` |
|      5 | 4773 | `		}` |
|      5 | 4774 | `	}` |
|    448 | 4775 | `	iDiv = 1;` |
|    448 | 4776 | `	if( iType == 2 ){` |
|      3 | 4777 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|      1 | 4778 | `	}` |
|      - | 4779 | `	/* Perform the requested operation */` |
|    448 | 4780 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      7 | 4781 | `		jPad = iStrpad;` |
|     13 | 4782 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 4783 | `			/* Padding */` |
|     11 | 4784 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      5 | 4785 | `				break;` |
|      - | 4786 | `			}` |
|      7 | 4787 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      4 | 4788 | `		}` |
|      7 | 4789 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      9 | 4790 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      5 | 4791 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      5 | 4792 | `				if( jPad > iStrpad ){` |
|    ! 0 | 4793 | `					jPad = iStrpad;` |
|    ! 0 | 4794 | `				}` |
|      5 | 4795 | `				if( jPad < 1){` |
|    ! 0 | 4796 | `					break;` |
|      - | 4797 | `				}` |
|      5 | 4798 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 4799 | `			}` |
|      2 | 4800 | `		}` |
|      3 | 4801 | `	}` |
|    448 | 4802 | `	if( iLen > 0 ){` |
|      - | 4803 | `		/* Append the input string */` |
|    448 | 4804 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|    222 | 4805 | `	}` |
|    448 | 4806 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|   5062 | 4807 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 4808 | `			/* Padding */` |
|   5060 | 4809 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|    442 | 4810 | `				break;` |
|      - | 4811 | `			}` |
|   4622 | 4812 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|   2313 | 4813 | `		}` |
|    884 | 4814 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|    444 | 4815 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|    444 | 4816 | `			if( jPad > iStrpad ){` |
|    ! 0 | 4817 | `				jPad = iStrpad;` |
|    ! 0 | 4818 | `			}` |
|    444 | 4819 | `			if( jPad < 1){` |
|    ! 0 | 4820 | `				break;` |
|      - | 4821 | `			}` |
|    444 | 4822 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      4 | 4823 | `		}` |
|    220 | 4824 | `	}` |
|    448 | 4825 | `	return PH7_OK;` |
|    232 | 4826 | `}` |
|      - | 4827 | `/*` |
|      - | 4828 | ` * String replacement private data.` |
|      - | 4829 | ` */` |
|      - | 4830 | `typedef struct str_replace_data str_replace_data;` |
|      - | 4831 | `struct str_replace_data` |
|      - | 4832 | `{` |
|      - | 4833 | `	/* Used by the str_replace family to collect the search/replace arguments. */` |
|      - | 4834 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 4835 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 4836 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 4837 | `};` |
|      - | 4838 | `/*` |
|      - | 4839 | ` * Remove a substring.` |
|      - | 4840 | ` */` |
|      - | 4841 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 4842 | `	for(;;){\` |
|      - | 4843 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 4844 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 4845 | `		++OFFT;\` |
|      - | 4846 | `	}\` |
|      - | 4847 | `}` |
|      - | 4848 | `/*` |
|      - | 4849 | ` * Shift right and insert algorithm.` |
|      - | 4850 | ` */` |
|      - | 4851 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 4852 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 4853 | `		for(;;){\` |
|      - | 4854 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 4855 | `			if(INLEN < 1 ) { break; }\` |
|      - | 4856 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 4857 | `			--INLEN; \` |
|      - | 4858 | `		}\` |
|      - | 4859 | `		for(;;){\` |
|      - | 4860 | `				if(ELEN < 1) { break; }\` |
|      - | 4861 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 4862 | `				OFFT++;\` |
|      - | 4863 | `				ENTRY++;\` |
|      - | 4864 | `				--ELEN;\` |
|      - | 4865 | `		}\` |
|      - | 4866 | `}` |
|      - | 4867 | `/*` |
|      - | 4868 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 4869 | ` * replacement string [i.e: zReplace].` |
|      - | 4870 | ` */` |
|    234 | 4871 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      5 | 4872 | `{` |
|    239 | 4873 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 4874 | `	sxu32 n,m;` |
|    239 | 4875 | `	n = SyBlobLength(pWorker);` |
|    239 | 4876 | `	m = nOfft;` |
|      - | 4877 | `	/* Delete the old entry */` |
|   8725 | 4878 | `	STRDEL(zInput,n,m,nLen);` |
|    239 | 4879 | `	SyBlobLength(pWorker) -= nLen;` |
|    239 | 4880 | `	if( nReplen > 0 ){` |
|    167 | 4881 | `		sxi32 iRep = nReplen;` |
|      - | 4882 | `		sxi32 rc;` |
|      - | 4883 | `		/*` |
|      - | 4884 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 4885 | `		 * string.` |
|      - | 4886 | `		 */` |
|    167 | 4887 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|    167 | 4888 | `		if( rc != SXRET_OK ){` |
|      - | 4889 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 4890 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 4891 | `			return rc;` |
|      - | 4892 | `		}` |
|      - | 4893 | `		/* Perform the insertion now */` |
|    167 | 4894 | `		zInput = (char *)SyBlobData(pWorker);` |
|    167 | 4895 | `		n = SyBlobLength(pWorker);` |
|   7531 | 4896 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|    167 | 4897 | `		SyBlobLength(pWorker) += nReplen;` |
|     81 | 4898 | `	}` |
|    239 | 4899 | `	return SXRET_OK;` |
|    122 | 4900 | `}` |
|      - | 4901 | `/*` |
|      - | 4902 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 4903 | ` * to collect search/replace string.` |
|      - | 4904 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 4905 | ` */` |
|    284 | 4906 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      5 | 4907 | `{` |
|    289 | 4908 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 4909 | `	SyString sWorker;` |
|      - | 4910 | `	const char *zIn;` |
|      - | 4911 | `	int nByte;` |
|      - | 4912 | `	/* Extract a string representation of the given argument */` |
|    289 | 4913 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|    289 | 4914 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|    289 | 4915 | `	if( nByte > 0 ){` |
|      - | 4916 | `		char *zDup;` |
|      - | 4917 | `		/* Duplicate the chunk */` |
|    283 | 4918 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 4919 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 4920 | `			);` |
|    283 | 4921 | `		if( zDup == 0 ){` |
|      - | 4922 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 4923 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 4924 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 4925 | `			return SXERR_MEM;` |
|      - | 4926 | `		}` |
|    283 | 4927 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 4928 | `		/* Save the chunk */` |
|    283 | 4929 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|    139 | 4930 | `	}` |
|      - | 4931 | `	/* Save for later processing */` |
|    289 | 4932 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 4933 | `	/* All done */` |
|    142 | 4934 | `	SXUNUSED(pKey); /* cc warning */` |
|    289 | 4935 | `	return PH7_OK;` |
|    147 | 4936 | `}` |
|      - | 4937 | `/*` |
|      - | 4938 | ` * Run the collected search/replace pairs over a single subject string, writing` |
|      - | 4939 | ` * the transformed bytes into pOut (reset here). Shared by the scalar-subject and` |
|      - | 4940 | ` * the array-subject (element-wise) paths. The search/replace SySets are walked` |
|      - | 4941 | ` * fresh on every call — cursors are reset here — so each array element is` |
|      - | 4942 | ` * transformed independently, exactly like php. Returns SXRET_OK, or SXERR_MEM` |
|      - | 4943 | ` * on an allocation failure inside StringReplace.` |
|      - | 4944 | ` *` |
|      - | 4945 | ` * *pnCount is INCREMENTED (never reset) by the number of replacements performed,` |
|      - | 4946 | ` * so an array subject accumulates across its elements exactly like php's &$count.` |
|      - | 4947 | ` */` |
|  47572 | 4948 | `static sxi32 StrReplaceOneSubject(` |
|      - | 4949 | `	SyBlob *pOut,             /* Output buffer (reset then filled here) */` |
|      - | 4950 | `	const char *zSubject,     /* Subject bytes */` |
|      - | 4951 | `	sxu32 nSubject,           /* Subject length */` |
|      - | 4952 | `	SySet *pSearch,           /* Collected search terms */` |
|      - | 4953 | `	SySet *pReplace,          /* Collected replacement terms */` |
|      - | 4954 | `	int rep_str,              /* TRUE: a single replacement reused for every search */` |
|      - | 4955 | `	ProcStringMatch xMatch,   /* SyBlobSearch (str_replace) / iPatternMatch (str_ireplace) */` |
|      - | 4956 | `	sxi64 *pnCount            /* Running replacement count (incremented here) */` |
|      - | 4957 | `	)` |
|      5 | 4958 | `{` |
|      - | 4959 | `	SyString *pSearch_,*pReplace_,sEmpty;` |
|      - | 4960 | `	sxi32 rc;` |
|  47577 | 4961 | `	SyBlobReset(pOut);` |
|  47577 | 4962 | `	if( nSubject > 0 ){` |
|  33369 | 4963 | `		rc = SyBlobAppend(pOut,(const void *)zSubject,nSubject);` |
|  33369 | 4964 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4965 | `			return rc;` |
|      - | 4966 | `		}` |
|  16682 | 4967 | `	}` |
|  47577 | 4968 | `	SyStringInitFromBuf(&sEmpty,"",0);` |
|  47577 | 4969 | `	SySetResetCursor(pSearch);` |
|  47577 | 4970 | `	SySetResetCursor(pReplace);` |
|  47577 | 4971 | `	pSearch_ = pReplace_ = 0; /* cc warning */` |
|  95285 | 4972 | `	while( SXRET_OK == SySetGetNextEntry(pSearch,(void **)&pSearch_) ){` |
|      - | 4973 | `		sxu32 nCount,nOfft;` |
|  47713 | 4974 | `		if( rep_str ){` |
|      - | 4975 | `			/* Single replacement string reused for every search term */` |
|  47679 | 4976 | `			pReplace_ = (SyString *)SySetPeek(pReplace);` |
|  23872 | 4977 | `		}else if( SXRET_OK != SySetGetNextEntry(pReplace,(void **)&pReplace_) ){` |
|      - | 4978 | `			/* 'replace set' has fewer values than the search set: an empty` |
|      - | 4979 | `			 * string is used for the rest of the replacement values. */` |
|      5 | 4980 | `			pReplace_ = 0;` |
|      2 | 4981 | `		}` |
|  47713 | 4982 | `		if( pReplace_ == 0 ){` |
|      5 | 4983 | `			pReplace_ = &sEmpty;` |
|      2 | 4984 | `		}` |
|  47713 | 4985 | `		if( pSearch_->nByte < 1 ){` |
|      - | 4986 | `			/* php ignores an empty search string, but it still CONSUMED a replace` |
|      - | 4987 | `			 * slot above so the remaining pairs stay aligned. */` |
|     15 | 4988 | `			continue;` |
|      - | 4989 | `		}` |
|  47699 | 4990 | `		nOfft = nCount = 0;` |
|  23964 | 4991 | `		for(;;){` |
|  47933 | 4992 | `			if( nCount >= SyBlobLength(pOut) ){` |
|  14259 | 4993 | `				break;` |
|      - | 4994 | `			}` |
|      - | 4995 | `			/* Perform a pattern lookup */` |
|  50516 | 4996 | `			rc = xMatch(SyBlobDataAt(pOut,nCount),SyBlobLength(pOut) - nCount,` |
|  33674 | 4997 | `				(const void *)pSearch_->zString,pSearch_->nByte,&nOfft);` |
|  33679 | 4998 | `			if( rc != SXRET_OK ){` |
|      - | 4999 | `				/* Pattern not found */` |
|  33445 | 5000 | `				break;` |
|      - | 5001 | `			}` |
|      - | 5002 | `			/* Perform the replace operation */` |
|    356 | 5003 | `			rc = StringReplace(pOut,nCount+nOfft,(int)pSearch_->nByte,` |
|    234 | 5004 | `				pReplace_->zString,(int)pReplace_->nByte);` |
|    239 | 5005 | `			if( rc != SXRET_OK ){` |
|      - | 5006 | `				/* Propagate an allocation failure so the caller raises a fatal` |
|      - | 5007 | `				 * instead of returning a partially-replaced result. */` |
|    ! 0 | 5008 | `				return rc;` |
|      - | 5009 | `			}` |
|    239 | 5010 | `			*pnCount += 1;` |
|      - | 5011 | `			/* Increment offset counter */` |
|    239 | 5012 | `			nCount += nOfft + pReplace_->nByte;` |
|      5 | 5013 | `		}` |
|      5 | 5014 | `	}` |
|  47577 | 5015 | `	return SXRET_OK;` |
|  23791 | 5016 | `}` |
|      - | 5017 | `/* Per-call state for the array-subject form of str_replace()/str_ireplace(). */` |
|      - | 5018 | `typedef struct str_replace_subject str_replace_subject;` |
|      - | 5019 | `struct str_replace_subject` |
|      - | 5020 | `{` |
|      - | 5021 | `	ph7_value *pResult;    /* Result array (keys preserved) */` |
|      - | 5022 | `	ph7_value *pScratch;   /* Reusable string value for each element */` |
|      - | 5023 | `	SyBlob *pWorker;       /* Scratch output buffer for one element */` |
|      - | 5024 | `	SySet *pSearch;        /* Collected search terms */` |
|      - | 5025 | `	SySet *pReplace;       /* Collected replacement terms */` |
|      - | 5026 | `	ProcStringMatch xMatch;/* Match routine (case-sensitive or not) */` |
|      - | 5027 | `	int rep_str;           /* TRUE: scalar $replace */` |
|      - | 5028 | `	sxi64 nReplaced;       /* Replacements performed so far (&$count) */` |
|      - | 5029 | `	sxi32 rc;              /* SXRET_OK or SXERR_MEM */` |
|      - | 5030 | `};` |
|      - | 5031 | `/*` |
|      - | 5032 | ` * ph7_array_walk() callback over an array $subject: string-cast one element, run` |
|      - | 5033 | ` * the search/replace over it, and insert the result under the element's original` |
|      - | 5034 | ` * key. A non-string element is coerced exactly like php (int/float/bool/null via` |
|      - | 5035 | ` * their string form). A nested-array element becomes "Array" — the value matches` |
|      - | 5036 | ` * php, but PHL does not emit php's "Array to string conversion" warning here (the` |
|      - | 5037 | ` * engine raises it at echo/interpolation sites, not this C-level cast; a` |
|      - | 5038 | ` * recorded divergence).` |
|      - | 5039 | ` */` |
|     34 | 5040 | `static int StrReplaceSubjectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 5041 | `{` |
|     35 | 5042 | `	str_replace_subject *pS = (str_replace_subject *)pUserData;` |
|      - | 5043 | `	const char *zSub;` |
|      - | 5044 | `	int nSub;` |
|      - | 5045 | `	/* php coerces every element to string (same cast used everywhere). */` |
|     35 | 5046 | `	zSub = ph7_value_to_string(pData,&nSub);` |
|     34 | 5047 | `	if( StrReplaceOneSubject(pS->pWorker,zSub,(sxu32)(nSub > 0 ? nSub : 0),` |
|     35 | 5048 | `			pS->pSearch,pS->pReplace,pS->rep_str,pS->xMatch,&pS->nReplaced) != SXRET_OK ){` |
|    ! 0 | 5049 | `		pS->rc = SXERR_MEM;` |
|    ! 0 | 5050 | `		return SXERR_ABORT;` |
|      - | 5051 | `	}` |
|      - | 5052 | `	/* Publish the transformed bytes as a string under the original key. */` |
|     35 | 5053 | `	ph7_value_reset_string_cursor(pS->pScratch);` |
|     34 | 5054 | `	if( SyBlobLength(pS->pWorker) > 0` |
|     33 | 5055 | `	 && ph7_value_string(pS->pScratch,(const char *)SyBlobData(pS->pWorker),` |
|     45 | 5056 | `			(int)SyBlobLength(pS->pWorker)) != SXRET_OK ){` |
|    ! 0 | 5057 | `		pS->rc = SXERR_MEM;` |
|    ! 0 | 5058 | `		return SXERR_ABORT;` |
|      - | 5059 | `	}` |
|     35 | 5060 | `	if( ph7_array_add_elem(pS->pResult,pKey,pS->pScratch) != SXRET_OK ){` |
|    ! 0 | 5061 | `		pS->rc = SXERR_MEM;` |
|    ! 0 | 5062 | `		return SXERR_ABORT;` |
|      - | 5063 | `	}` |
|     35 | 5064 | `	return PH7_OK;` |
|     18 | 5065 | `}` |
|      - | 5066 | `/*` |
|      - | 5067 | ` * Write str_replace()/str_ireplace()'s optional by-reference &$count out-param.` |
|      - | 5068 | ` * The call compiler auto-vivifies argument #4 for these two names` |
|      - | 5069 | ` * (GenStateByRefBuiltinMask in compile.c), so an undefined variable, an array` |
|      - | 5070 | ` * element and a property all arrive with a real slot to write through.` |
|      - | 5071 | ` */` |
|  47550 | 5072 | `static void StrReplaceStoreCount(ph7_context *pCtx,int nArg,ph7_value **apArg,sxi64 nReplaced)` |
|      5 | 5073 | `{` |
|      - | 5074 | `	ph7_value sCount;` |
|  47555 | 5075 | `	if( nArg < 4 ){` |
|  47535 | 5076 | `		return;` |
|      - | 5077 | `	}` |
|     21 | 5078 | `	PH7_MemObjInitFromInt(pCtx->pVm,&sCount,nReplaced);` |
|     21 | 5079 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[3],&sCount);` |
|     21 | 5080 | `	PH7_MemObjRelease(&sCount);` |
|  23780 | 5081 | `}` |
|      - | 5082 | `/*` |
|      - | 5083 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 5084 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 5085 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 5086 | ` * Parameters` |
|      - | 5087 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 5088 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 5089 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 5090 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 5091 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 5092 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 5093 | ` * $search` |
|      - | 5094 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 5095 | ` *  to designate multiple needles.` |
|      - | 5096 | ` * $replace` |
|      - | 5097 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 5098 | ` *  to designate multiple replacements.` |
|      - | 5099 | ` * $subject` |
|      - | 5100 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 5101 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 5102 | ` *  of subject, and the return value is an array as well.` |
|      - | 5103 | ` * &$count` |
|      - | 5104 | ` *  If passed, this is set to the number of replacements performed — accumulated` |
|      - | 5105 | ` *  over every search term AND, for an array subject, over every element.` |
|      - | 5106 | ` * Return` |
|      - | 5107 | ` * This function returns a string or an array with the replaced values.` |
|      - | 5108 | ` */` |
|  47550 | 5109 | `PH7_PRIVATE int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 5110 | `{` |
|      - | 5111 | `	SyString sTemp;` |
|      - | 5112 | `	ProcStringMatch xMatch;` |
|      - | 5113 | `	const char *zIn,*zFunc;` |
|      - | 5114 | `	str_replace_data sRep;` |
|      - | 5115 | `	SyBlob sWorker;` |
|      - | 5116 | `	SySet sReplace;` |
|      - | 5117 | `	SySet sSearch;` |
|      - | 5118 | `	sxi64 nReplaced;` |
|      - | 5119 | `	int rep_str;` |
|      - | 5120 | `	int nByte;` |
|      - | 5121 | `	sxi32 rc;` |
|  47555 | 5122 | `	if( nArg < 3 ){` |
|      - | 5123 | `		/* Missing/Invalid arguments,return null */` |
|    ! 0 | 5124 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5125 | `		return PH7_OK;` |
|      - | 5126 | `	}` |
|      - | 5127 | `	/* Initialize fields */` |
|  47555 | 5128 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  47555 | 5129 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  47555 | 5130 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  47555 | 5131 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  47555 | 5132 | `	sRep.pCtx = pCtx;` |
|  47555 | 5133 | `	sRep.pCollector = &sSearch;` |
|  47555 | 5134 | `	rep_str = 0;` |
|  47555 | 5135 | `	nReplaced = 0;` |
|      - | 5136 | `	/* Collect the search term(s) — independent of the subject. */` |
|  47555 | 5137 | `	if( ph7_value_is_array(apArg[0]) ){` |
|    135 | 5138 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|     70 | 5139 | `	}else{` |
|  47425 | 5140 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  47425 | 5141 | `		SyStringInitFromBuf(&sTemp,zIn,nByte > 0 ? nByte : 0);` |
|  47425 | 5142 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 5143 | `	}` |
|      - | 5144 | `	/* Collect the replacement term(s). */` |
|  47555 | 5145 | `	if( ph7_value_is_array(apArg[1]) ){` |
|     13 | 5146 | `		sRep.pCollector = &sReplace;` |
|     13 | 5147 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      7 | 5148 | `	}else{` |
|  47543 | 5149 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  47543 | 5150 | `		rep_str = 1;` |
|  47543 | 5151 | `		SyStringInitFromBuf(&sTemp,zIn,nByte > 0 ? nByte : 0);` |
|  47543 | 5152 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 5153 | `	}` |
|      - | 5154 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  47555 | 5155 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 5156 | `		SySetRelease(&sSearch);` |
|    ! 0 | 5157 | `		SySetRelease(&sReplace);` |
|    ! 0 | 5158 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 5159 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5160 | `	}` |
|      - | 5161 | `	/* Pick the match routine by function name */` |
|  47555 | 5162 | `	zFunc = ph7_function_name(pCtx);` |
|  47555 | 5163 | `	xMatch = SyBlobSearch;` |
|  47555 | 5164 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 5165 | `		/* Case insensitive pattern match */` |
|     17 | 5166 | `		xMatch = iPatternMatch;` |
|      8 | 5167 | `	}` |
|  47555 | 5168 | `	if( ph7_value_is_array(apArg[2]) ){` |
|      - | 5169 | `		/* Array subject: replace element-wise and RETURN AN ARRAY whose keys` |
|      - | 5170 | `		 * mirror the subject's (php semantics). */` |
|      - | 5171 | `		str_replace_subject sSub;` |
|      - | 5172 | `		ph7_value *pResult,*pScratch;` |
|     13 | 5173 | `		pResult = ph7_context_new_array(pCtx);` |
|     13 | 5174 | `		pScratch = ph7_context_new_scalar(pCtx);` |
|     13 | 5175 | `		if( pResult == 0 \|\| pScratch == 0 ){` |
|    ! 0 | 5176 | `			SySetRelease(&sSearch);` |
|    ! 0 | 5177 | `			SySetRelease(&sReplace);` |
|    ! 0 | 5178 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 5179 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 5180 | `		}` |
|     13 | 5181 | `		ph7_value_string(pScratch,"",0); /* force string representation */` |
|     13 | 5182 | `		SyZero(&sSub,sizeof(sSub));` |
|     13 | 5183 | `		sSub.pResult  = pResult;` |
|     13 | 5184 | `		sSub.pScratch = pScratch;` |
|     13 | 5185 | `		sSub.pWorker  = &sWorker;` |
|     13 | 5186 | `		sSub.pSearch  = &sSearch;` |
|     13 | 5187 | `		sSub.pReplace = &sReplace;` |
|     13 | 5188 | `		sSub.xMatch   = xMatch;` |
|     13 | 5189 | `		sSub.rep_str  = rep_str;` |
|     13 | 5190 | `		ph7_array_walk(apArg[2],StrReplaceSubjectWalker,&sSub);` |
|     13 | 5191 | `		SySetRelease(&sSearch);` |
|     13 | 5192 | `		SySetRelease(&sReplace);` |
|     13 | 5193 | `		SyBlobRelease(&sWorker);` |
|     13 | 5194 | `		if( sSub.rc != SXRET_OK ){` |
|    ! 0 | 5195 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 5196 | `		}` |
|     13 | 5197 | `		ph7_result_value(pCtx,pResult);` |
|     13 | 5198 | `		StrReplaceStoreCount(pCtx,nArg,apArg,sSub.nReplaced);` |
|     13 | 5199 | `		return PH7_OK;` |
|      - | 5200 | `	}` |
|      - | 5201 | `	/* Scalar subject: run once and return a string. An empty subject yields the` |
|      - | 5202 | `	 * empty string, and a lone empty search term leaves the subject untouched —` |
|      - | 5203 | `	 * both fall out of StrReplaceOneSubject's empty-term skip. */` |
|  47543 | 5204 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  47543 | 5205 | `	rc = StrReplaceOneSubject(&sWorker,zIn,(sxu32)(nByte > 0 ? nByte : 0),` |
|  23769 | 5206 | `		&sSearch,&sReplace,rep_str,xMatch,&nReplaced);` |
|  47543 | 5207 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 5208 | `		SySetRelease(&sSearch);` |
|    ! 0 | 5209 | `		SySetRelease(&sReplace);` |
|    ! 0 | 5210 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 5211 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5212 | `	}` |
|  47543 | 5213 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  47543 | 5214 | `	SySetRelease(&sSearch);` |
|  47543 | 5215 | `	SySetRelease(&sReplace);` |
|  47543 | 5216 | `	SyBlobRelease(&sWorker);` |
|  47543 | 5217 | `	if( rc != PH7_OK ){` |
|    ! 0 | 5218 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5219 | `	}` |
|  47543 | 5220 | `	StrReplaceStoreCount(pCtx,nArg,apArg,nReplaced);` |
|  47543 | 5221 | `	return PH7_OK;` |
|  23780 | 5222 | `}` |
|      - | 5223 | `/*` |
|      - | 5224 | ` * strtr() array form: a single (key,value) pair copied out of the replace_pairs` |
|      - | 5225 | ` * array. The bytes are owned by a persistent pool (see strtr_collect) rather than` |
|      - | 5226 | ` * the transient walker values, which HashmapWalk releases after each callback, so` |
|      - | 5227 | ` * we store byte offsets into that pool instead of raw pointers.` |
|      - | 5228 | ` */` |
|      - | 5229 | `typedef struct strtr_entry strtr_entry;` |
|      - | 5230 | `struct strtr_entry` |
|      - | 5231 | `{` |
|      - | 5232 | `	sxu32 nKeyOfft; /* Offset of the search key inside the pool */` |
|      - | 5233 | `	sxu32 nKeyLen;  /* Length of the search key */` |
|      - | 5234 | `	sxu32 nValOfft; /* Offset of the replacement inside the pool */` |
|      - | 5235 | `	sxu32 nValLen;  /* Length of the replacement */` |
|      - | 5236 | `};` |
|      - | 5237 | `typedef struct strtr_collect strtr_collect;` |
|      - | 5238 | `struct strtr_collect` |
|      - | 5239 | `{` |
|      - | 5240 | `	SyBlob *pPool;  /* Byte pool holding copied key + value bytes */` |
|      - | 5241 | `	SySet  *pTable; /* Set of strtr_entry (parallel offsets into pPool) */` |
|      - | 5242 | `	sxi32   rc;     /* Carries an allocation failure (SXERR_MEM) out of the walker */` |
|      - | 5243 | `	ph7_context *pCtx; /* Needed to warn about an empty key */` |
|      - | 5244 | `};` |
|      - | 5245 | `/*` |
|      - | 5246 | ` * Collect one replace_pairs entry into the persistent pool/offset table.` |
|      - | 5247 | ` * PHP coerces both the key and the value to string (an integer key becomes its` |
|      - | 5248 | ` * decimal form) and ignores an empty-string key.` |
|      - | 5249 | ` */` |
|     22 | 5250 | `static int StrtrCollectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 5251 | `{` |
|     23 | 5252 | `	strtr_collect *pCol = (strtr_collect *)pUserData;` |
|      - | 5253 | `	const char *zKey,*zVal;` |
|      - | 5254 | `	strtr_entry sEnt;` |
|      - | 5255 | `	int nKey,nVal;` |
|     23 | 5256 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     23 | 5257 | `	if( nKey < 1 ){` |
|      - | 5258 | `		/* PHP ignores an empty-string key, and warns that it did so. */` |
|      3 | 5259 | `		ph7_context_throw_error_format(pCol->pCtx,PH7_CTX_WARNING,` |
|      - | 5260 | `			"Ignoring replacement of empty string");` |
|      3 | 5261 | `		return PH7_OK;` |
|      - | 5262 | `	}` |
|     21 | 5263 | `	zVal = ph7_value_to_string(pData,&nVal);` |
|     21 | 5264 | `	sEnt.nKeyOfft = SyBlobLength(pCol->pPool);` |
|     21 | 5265 | `	sEnt.nKeyLen  = (sxu32)nKey;` |
|     21 | 5266 | `	if( SyBlobAppend(pCol->pPool,(const void *)zKey,(sxu32)nKey) != SXRET_OK ){` |
|    ! 0 | 5267 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 5268 | `		return SXERR_ABORT;` |
|      - | 5269 | `	}` |
|     21 | 5270 | `	sEnt.nValOfft = SyBlobLength(pCol->pPool);` |
|     21 | 5271 | `	sEnt.nValLen  = (sxu32)nVal;` |
|     21 | 5272 | `	if( nVal > 0 && SyBlobAppend(pCol->pPool,(const void *)zVal,(sxu32)nVal) != SXRET_OK ){` |
|    ! 0 | 5273 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 5274 | `		return SXERR_ABORT;` |
|      - | 5275 | `	}` |
|     21 | 5276 | `	if( SySetPut(pCol->pTable,(const void *)&sEnt) != SXRET_OK ){` |
|    ! 0 | 5277 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 5278 | `		return SXERR_ABORT;` |
|      - | 5279 | `	}` |
|     21 | 5280 | `	return PH7_OK;` |
|     12 | 5281 | `}` |
|      - | 5282 | `/*` |
|      - | 5283 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 5284 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 5285 | ` *  Translate characters or replace substrings.` |
|      - | 5286 | ` * Parameters` |
|      - | 5287 | ` *  $str` |
|      - | 5288 | ` *  The string being translated.` |
|      - | 5289 | ` * $from` |
|      - | 5290 | ` *  The string being translated to to.` |
|      - | 5291 | ` * $to` |
|      - | 5292 | ` *  The string replacing from.` |
|      - | 5293 | ` * $replace_pairs` |
|      - | 5294 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 5295 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 5296 | ` * Return` |
|      - | 5297 | ` *  The translated string.` |
|      - | 5298 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 5299 | ` */` |
|     94 | 5300 | `PH7_PRIVATE int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5301 | `{` |
|      - | 5302 | `	const char *zIn;` |
|      - | 5303 | `	char zGiven[64];` |
|      - | 5304 | `	int nLen;` |
|     95 | 5305 | `	if( nArg < 1 ){` |
|      - | 5306 | `		/* Nothing to replace,return FALSE */` |
|    ! 0 | 5307 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5308 | `		return PH7_OK;` |
|      - | 5309 | `	}` |
|      - | 5310 | `	/*` |
|      - | 5311 | `	 * php dispatches strtr() on ARITY between two overloads — strtr(string, array)` |
|      - | 5312 | ``	 * and strtr(string, string, string) — so $from's expected type is `array` with`` |
|      - | 5313 | ``	 * two arguments and `string` with three, and the stub's `array\|string` union is`` |
|      - | 5314 | `	 * a wording php itself never emits. One signature cannot express that, so the` |
|      - | 5315 | `	 * shared ZPP screen skips this builtin (azSelfChecked[] in vm_arg_check.c) and` |
|      - | 5316 | `	 * the dispatch happens here, in php's left-to-right argument order.` |
|      - | 5317 | `	 *` |
|      - | 5318 | `	 * Both directions used to pass silently: a 2-argument string $from` |
|      - | 5319 | `	 * (strtr("abc","ab")) returned the subject UNCHANGED, and a 3-argument array` |
|      - | 5320 | `	 * $from was likewise ignored — the caller got its input back as if it had been` |
|      - | 5321 | `	 * translated.` |
|      - | 5322 | `	 */` |
|     95 | 5323 | `	if( !PH7_ArgSatisfiesString(apArg[0]) ){` |
|      4 | 5324 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5325 | `			"strtr(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 5326 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|      - | 5327 | `	}` |
|     93 | 5328 | `	if( nArg == 2 ){` |
|     31 | 5329 | `		if( !ph7_value_is_array(apArg[1]) ){` |
|     22 | 5330 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5331 | `				"strtr(): Argument #2 ($from) must be of type array, %s given",` |
|     14 | 5332 | `				VmValueGivenName(apArg[1],zGiven,sizeof(zGiven)));` |
|      1 | 5333 | `		}` |
|     71 | 5334 | `	}else if( nArg > 2 ){` |
|     63 | 5335 | `		if( !PH7_ArgSatisfiesString(apArg[1]) ){` |
|     10 | 5336 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5337 | `				"strtr(): Argument #2 ($from) must be of type string, %s given",` |
|      6 | 5338 | `				VmValueGivenName(apArg[1],zGiven,sizeof(zGiven)));` |
|      - | 5339 | `		}` |
|      - | 5340 | ``		/* $to is php's `string`, but a null one stays accepted (php coerces it to`` |
|      - | 5341 | `		 * "" with a deprecation, and both engines answer the subject unchanged). */` |
|     57 | 5342 | `		if( !ph7_value_is_null(apArg[2]) && !PH7_ArgSatisfiesString(apArg[2]) ){` |
|      4 | 5343 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5344 | `				"strtr(): Argument #3 ($to) must be of type string, %s given",` |
|      2 | 5345 | `				VmValueGivenName(apArg[2],zGiven,sizeof(zGiven)));` |
|      - | 5346 | `		}` |
|     27 | 5347 | `	}` |
|     71 | 5348 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     71 | 5349 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 5350 | `		/* Invalid arguments */` |
|      3 | 5351 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      3 | 5352 | `		return PH7_OK;` |
|      - | 5353 | `	}` |
|     76 | 5354 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 5355 | `		strtr_collect sCol;` |
|      - | 5356 | `		SyBlob sPool,sWorker;` |
|      - | 5357 | `		SySet sTable;` |
|      - | 5358 | `		const char *zPool;` |
|      - | 5359 | `		strtr_entry *pEnt;` |
|      - | 5360 | `		sxi32 rc;` |
|      - | 5361 | `		int i,iRun;` |
|      - | 5362 | `		/*` |
|      - | 5363 | `		 * PHP's array-form strtr is a single left-to-right pass over the subject:` |
|      - | 5364 | `		 * at every position it substitutes the LONGEST replace_pairs key that` |
|      - | 5365 | `		 * matches there, then advances past the key (replacements are never` |
|      - | 5366 | `		 * rescanned). It is not a sequential per-key global replace. First copy` |
|      - | 5367 | `		 * the pairs into a persistent pool, then run that scan.` |
|      - | 5368 | `		 */` |
|     15 | 5369 | `		SyBlobInit(&sPool,&pCtx->pVm->sAllocator);` |
|     15 | 5370 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     15 | 5371 | `		SySetInit(&sTable,&pCtx->pVm->sAllocator,sizeof(strtr_entry));` |
|     15 | 5372 | `		sCol.pPool  = &sPool;` |
|     15 | 5373 | `		sCol.pTable = &sTable;` |
|     15 | 5374 | `		sCol.rc     = SXRET_OK;` |
|     15 | 5375 | `		sCol.pCtx   = pCtx;` |
|     15 | 5376 | `		ph7_array_walk(apArg[1],StrtrCollectWalker,&sCol);` |
|     15 | 5377 | `		if( sCol.rc != SXRET_OK ){` |
|      - | 5378 | `			/* Allocation failure while collecting the pairs: surface a fatal */` |
|    ! 0 | 5379 | `			SyBlobRelease(&sPool);` |
|    ! 0 | 5380 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 5381 | `			SySetRelease(&sTable);` |
|    ! 0 | 5382 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 5383 | `		}` |
|      - | 5384 | `		/* The pool is now stable, so offsets can be resolved against its base. */` |
|     15 | 5385 | `		zPool = (const char *)SyBlobData(&sPool);` |
|     15 | 5386 | `		rc = SXRET_OK;` |
|     15 | 5387 | `		iRun = 0; /* Start of the pending run of unmatched bytes copied verbatim. */` |
|     59 | 5388 | `		for( i = 0 ; i < nLen ; ){` |
|     45 | 5389 | `			strtr_entry *pBest = 0;` |
|     45 | 5390 | `			sxu32 nBest = 0;` |
|      - | 5391 | `			/* Pick the longest key that matches at the current position. */` |
|     45 | 5392 | `			SySetResetCursor(&sTable);` |
|    105 | 5393 | `			while( SXRET_OK == SySetGetNextEntry(&sTable,(void **)&pEnt) ){` |
|     60 | 5394 | `				if( pEnt->nKeyLen > nBest` |
|     56 | 5395 | `					&& pEnt->nKeyLen <= (sxu32)(nLen - i)` |
|     52 | 5396 | `					&& SyMemcmp(zPool + pEnt->nKeyOfft,zIn + i,pEnt->nKeyLen) == 0 ){` |
|     31 | 5397 | `					nBest = pEnt->nKeyLen;` |
|     31 | 5398 | `					pBest = pEnt;` |
|     15 | 5399 | `				}` |
|      1 | 5400 | `			}` |
|     45 | 5401 | `			if( pBest == 0 ){` |
|      - | 5402 | `				/* No key here: extend the literal run and copy it in one shot later. */` |
|     19 | 5403 | `				i++;` |
|     19 | 5404 | `				continue;` |
|      - | 5405 | `			}` |
|      - | 5406 | `			/* Flush the pending literal run, then the replacement. */` |
|     27 | 5407 | `			if( i > iRun ){` |
|      5 | 5408 | `				rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(i - iRun));` |
|      2 | 5409 | `			}` |
|     27 | 5410 | `			if( rc == SXRET_OK && pBest->nValLen > 0 ){` |
|     27 | 5411 | `				rc = SyBlobAppend(&sWorker,zPool + pBest->nValOfft,pBest->nValLen);` |
|     13 | 5412 | `			}` |
|     27 | 5413 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 5414 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 5415 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 5416 | `				SySetRelease(&sTable);` |
|    ! 0 | 5417 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 5418 | `			}` |
|     27 | 5419 | `			i += (int)pBest->nKeyLen;` |
|     27 | 5420 | `			iRun = i;` |
|      1 | 5421 | `		}` |
|      - | 5422 | `		/* Flush the trailing literal run. */` |
|     15 | 5423 | `		if( nLen > iRun ){` |
|      7 | 5424 | `			rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(nLen - iRun));` |
|      7 | 5425 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 5426 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 5427 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 5428 | `				SySetRelease(&sTable);` |
|    ! 0 | 5429 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 5430 | `			}` |
|      3 | 5431 | `		}` |
|      - | 5432 | `		/* All done, return the result string */` |
|     22 | 5433 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|     14 | 5434 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 5435 | `		/* Clean-up */` |
|     15 | 5436 | `		SyBlobRelease(&sPool);` |
|     15 | 5437 | `		SyBlobRelease(&sWorker);` |
|     15 | 5438 | `		SySetRelease(&sTable);` |
|     15 | 5439 | `		if( rc != PH7_OK ){` |
|    ! 0 | 5440 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 5441 | `		}` |
|      8 | 5442 | `	}else{` |
|      - | 5443 | `		int i,flen,tlen,c,iOfft;` |
|      - | 5444 | `		const char *zFrom,*zTo;` |
|     55 | 5445 | `		if( nArg < 3 ){` |
|      - | 5446 | `			/* Nothing to replace */` |
|    ! 0 | 5447 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5448 | `			return PH7_OK;` |
|      - | 5449 | `		}` |
|      - | 5450 | `		/* Extract given arguments */` |
|     55 | 5451 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|     55 | 5452 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|     55 | 5453 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 5454 | `			/* Nothing to replace */` |
|    ! 0 | 5455 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5456 | `			return PH7_OK;` |
|      - | 5457 | `		}` |
|      - | 5458 | `		/* Start the replace process */` |
|    167 | 5459 | `		for( i = 0 ; i < nLen ; ++i ){` |
|    113 | 5460 | `			c = zIn[i];` |
|    113 | 5461 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|     19 | 5462 | `				if ( iOfft < tlen ){` |
|     19 | 5463 | `					c = zTo[iOfft];` |
|      9 | 5464 | `				}` |
|      9 | 5465 | `			}` |
|    113 | 5466 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 5467 |  |
|     57 | 5468 | `		}` |
|      - | 5469 | `	}` |
|     69 | 5470 | `	return PH7_OK;` |
|     48 | 5471 | `}` |
|      - | 5472 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 5473 |  |
