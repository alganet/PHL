# src/ph7/builtin_string.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2730/3145 lines (86.80%)

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
| 394410 |   60 | `PH7_PRIVATE int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |   61 | `{` |
|      - |   62 | `	const char *zSource;` |
|      - |   63 | `	int nSrcLen;` |
|      - |   64 | `	sxi64 iStart,iEnd;` |
| 394415 |   65 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"substr",1,"$string"); }` |
| 394415 |   66 | `	if( nArg < 2 ){` |
|      - |   67 | `		/* Arity is enforced at the call boundary; nothing sensible to return here. */` |
|    ! 0 |   68 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 |   69 | `		return PH7_OK;` |
|      - |   70 | `	}` |
|      - |   71 | `	/* Extract the target string */` |
| 394415 |   72 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|      - |   73 | `	/* Extract the offset */` |
|      - |   74 | `	{` |
| 394415 |   75 | `		sxi64 iTmp = 0;` |
| 394415 |   76 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"substr",2,"$offset","int",&iTmp);` |
| 394415 |   77 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |   78 | `			return rcArg;` |
|      - |   79 | `		}` |
| 394415 |   80 | `		iStart = iTmp;` |
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
| 394415 |   92 | `	if( iStart < 0 ){` |
|  38625 |   93 | `		iStart += nSrcLen;` |
|  38625 |   94 | `		if( iStart < 0 ){` |
|      5 |   95 | `			iStart = 0;` |
|      7 |   96 | `		}` |
| 375105 |   97 | `	}else if( iStart > nSrcLen ){` |
|      7 |   98 | `		iStart = nSrcLen;` |
|      3 |   99 | `	}` |
| 394415 |  100 | `	iEnd = nSrcLen;` |
| 394415 |  101 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
| 279969 |  102 | `		sxi64 iLen = 0;` |
| 279969 |  103 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],"substr",3,"$length","?int",&iLen);` |
| 279969 |  104 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  105 | `			return rcArg;` |
|      - |  106 | `		}` |
| 279969 |  107 | `		if( iLen < 0 ){` |
|  38195 |  108 | `			iEnd = (sxi64)nSrcLen + iLen;` |
| 260874 |  109 | `		}else if( iLen > (sxi64)nSrcLen - iStart ){` |
|  25973 |  110 | `			iEnd = nSrcLen;` |
|  12989 |  111 | `		}else{` |
| 215811 |  112 | `			iEnd = iStart + iLen;` |
|      - |  113 | `		}` |
| 139982 |  114 | `	}` |
| 394415 |  115 | `	if( iEnd < iStart ){` |
|      3 |  116 | `		iEnd = iStart;` |
|      1 |  117 | `	}` |
| 394415 |  118 | `	ph7_result_string(pCtx,&zSource[iStart],(int)(iEnd - iStart));` |
| 394415 |  119 | `	return PH7_OK;` |
| 197422 |  120 | `}` |
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
| 613246 |  323 | `static void StrNullArgNotice(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNum,const char *zParamName)` |
|      5 |  324 | `{` |
| 613251 |  325 | `	if( ph7_value_is_null(pArg) ){` |
|    ! 0 |  326 | `		PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  327 | `			"%s(): Argument #%d (%s) must be of type string, null given",` |
|    ! 0 |  328 | `			zFunc,iArgNum,zParamName);` |
|    ! 0 |  329 | `	}` |
| 613251 |  330 | `}` |
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
|     26 |  705 | `PH7_PRIVATE int PH7_builtin_levenshtein(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  706 | `{` |
|      - |  707 | `	static const char *azParam[] = { "$insertion_cost","$replacement_cost","$deletion_cost" };` |
|      - |  708 | `	const char *zStr1,*zStr2;` |
|     27 |  709 | `	sxi64 iCostIns = 1,iCostRep = 1,iCostDel = 1;` |
|      - |  710 | `	sxi64 *p1,*p2,*pTmp;` |
|      - |  711 | `	sxi64 c0,c1,c2;` |
|      - |  712 | `	ph7_value sTmp1,sTmp2;` |
|      - |  713 | `	int nLen1,nLen2;` |
|      - |  714 | `	int i1,i2;` |
|      - |  715 | `	sxi32 rc;` |
|      - |  716 | `	int i;` |
|     27 |  717 | `	if( nArg < 2 ){` |
|    ! 0 |  718 | `		return PH7_VmThrowException(pCtx,` |
|      - |  719 | `			"ArgumentCountError",` |
|      - |  720 | `			"levenshtein() expects at least 2 arguments, %d given",` |
|    ! 0 |  721 | `			nArg` |
|      - |  722 | `			);` |
|      - |  723 | `	}` |
|      - |  724 | `	/* $string1/$string2: null deprecates to "", __toString objects resolve,` |
|      - |  725 | `	 * everything non-stringish is a TypeError (php ZPP weak mode). */` |
|     27 |  726 | `	PH7_MemObjInit(pCtx->pVm,&sTmp1);` |
|     27 |  727 | `	PH7_MemObjInit(pCtx->pVm,&sTmp2);` |
|     27 |  728 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"levenshtein",1,"$string1","string",` |
|      - |  729 | `		"levenshtein(): Passing null to parameter #1 ($string1) "` |
|      - |  730 | `		"of type string is deprecated",` |
|      - |  731 | `		&sTmp1,&zStr1,&nLen1);` |
|     27 |  732 | `	if( rc != PH7_OK ) goto out;` |
|     27 |  733 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"levenshtein",2,"$string2","string",` |
|      - |  734 | `		"levenshtein(): Passing null to parameter #2 ($string2) "` |
|      - |  735 | `		"of type string is deprecated",` |
|      - |  736 | `		&sTmp2,&zStr2,&nLen2);` |
|     27 |  737 | `	if( rc != PH7_OK ) goto out;` |
|      - |  738 | `	/* Optional integer costs */` |
|     49 |  739 | `	for( i = 2 ; i < nArg && i < 5 ; i++ ){` |
|      - |  740 | `		sxi64 iVal;` |
|     23 |  741 | `		rc = PH7_IntArgResolve(pCtx,apArg[i],"levenshtein",i+1,azParam[i-2],"int",&iVal);` |
|     23 |  742 | `		if( rc != PH7_OK ) goto out;` |
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
|     13 |  796 | `out:` |
|     27 |  797 | `	PH7_MemObjRelease(&sTmp1);` |
|     27 |  798 | `	PH7_MemObjRelease(&sTmp2);` |
|     27 |  799 | `	return rc;` |
|     14 |  800 | `}` |
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
|     42 |  917 | `PH7_PRIVATE int PH7_builtin_str_word_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  918 | `{` |
|      - |  919 | `	const char *zIn,*zEnd,*zPtr;` |
|     43 |  920 | `	ph7_value *pArray = 0,*pValue = 0;` |
|      - |  921 | `	ph7_value sTmp,sListTmp;` |
|      - |  922 | `	char aMask[256];` |
|     43 |  923 | `	int bMask = 0;` |
|     43 |  924 | `	int iFormat = 0;` |
|     43 |  925 | `	int nCount = 0;` |
|      - |  926 | `	int nLen;` |
|      - |  927 | `	sxi32 rc;` |
|     43 |  928 | `	if( nArg < 1 ){` |
|    ! 0 |  929 | `		return PH7_VmThrowException(pCtx,` |
|      - |  930 | `			"ArgumentCountError",` |
|      - |  931 | `			"str_word_count() expects at least 1 argument, %d given",` |
|    ! 0 |  932 | `			nArg` |
|      - |  933 | `			);` |
|      - |  934 | `	}` |
|     43 |  935 | `	PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|     43 |  936 | `	PH7_MemObjInit(pCtx->pVm,&sListTmp);` |
|     43 |  937 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_word_count",1,"$string","string",` |
|      - |  938 | `		"str_word_count(): Passing null to parameter #1 ($string) "` |
|      - |  939 | `		"of type string is deprecated",` |
|      - |  940 | `		&sTmp,&zIn,&nLen);` |
|     43 |  941 | `	if( rc != PH7_OK ) goto out;` |
|     43 |  942 | `	if( nArg > 1 ){` |
|      - |  943 | `		sxi64 iVal;` |
|     29 |  944 | `		rc = PH7_IntArgResolve(pCtx,apArg[1],"str_word_count",2,"$format","int",&iVal);` |
|     31 |  945 | `		if( rc != PH7_OK ) goto out;` |
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
|     20 | 1025 | `out:` |
|     41 | 1026 | `	PH7_MemObjRelease(&sTmp);` |
|     41 | 1027 | `	PH7_MemObjRelease(&sListTmp);` |
|     41 | 1028 | `	return rc;` |
|     21 | 1029 | `}` |
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
|    326 | 1184 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256])` |
|      5 | 1185 | `{` |
|    331 | 1186 | `	const unsigned char *zIn  = (const unsigned char *)zList;` |
|    331 | 1187 | `	const unsigned char *zEnd = zIn + (nLen > 0 ? nLen : 0);` |
|    331 | 1188 | `	SyZero(aMask,256);` |
|   1033 | 1189 | `	for( ; zIn < zEnd ; zIn++ ){` |
|    707 | 1190 | `		int c = zIn[0];` |
|    707 | 1191 | `		if( zIn + 3 < zEnd && zIn[1] == '.' && zIn[2] == '.' && zIn[3] >= c ){` |
|      - | 1192 | `			/* Valid incrementing range c..zIn[3] */` |
|     22 | 1193 | `			int hi = zIn[3],k;` |
|    386 | 1194 | `			for( k = c ; k <= hi ; k++ ){` |
|    366 | 1195 | `				aMask[k] = 1;` |
|    184 | 1196 | `			}` |
|     22 | 1197 | `			zIn += 3; /* the loop's ++ then steps past the range end */` |
|    709 | 1198 | `		}else if( zIn + 1 < zEnd && zIn[0] == '.' && zIn[1] == '.' ){` |
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
|    663 | 1220 | `			aMask[c] = 1;` |
|      - | 1221 | `		}` |
|    356 | 1222 | `	}` |
|    331 | 1223 | `}` |
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
| 151504 | 1614 | `PH7_PRIVATE int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1615 | `{` |
| 151509 | 1616 | `	int iLen = 0;` |
| 151509 | 1617 | `	if( nArg > 0 ){` |
| 151509 | 1618 | `		StrNullArgNotice(pCtx,apArg[0],"strlen",1,"$string");` |
| 151509 | 1619 | `		ph7_value_to_string(apArg[0],&iLen);` |
|  76176 | 1620 | `	}` |
|      - | 1621 | `	/* String length */` |
| 151509 | 1622 | `	ph7_result_int(pCtx,iLen);` |
| 151509 | 1623 | `	return PH7_OK;` |
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
|     90 | 1635 | `PH7_PRIVATE int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1636 | `{` |
|      - | 1637 | `	const char *z1,*z2;` |
|      - | 1638 | `	int n1,n2;` |
|      - | 1639 | `	int res;` |
|     92 | 1640 | `	if( nArg < 2 ){` |
|    ! 0 | 1641 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 1642 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 1643 | `		return PH7_OK;` |
|      - | 1644 | `	}` |
|      - | 1645 | `	/* Perform the comparison */` |
|     92 | 1646 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     92 | 1647 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     92 | 1648 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1649 | `	/* Comparison result */` |
|     92 | 1650 | `	ph7_result_int(pCtx,res);` |
|     92 | 1651 | `	return PH7_OK;` |
|     47 | 1652 | `}` |
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
|    166 | 1665 | `static int StrNatCompareRight(const char **pa,const char *aEnd,const char **pb,const char *bEnd)` |
|      3 | 1666 | `{` |
|    169 | 1667 | `	int bias = 0;` |
|    275 | 1668 | `	for(;;){` |
|    361 | 1669 | `		int da = (*pa < aEnd) && SyisDigit(**pa);` |
|    361 | 1670 | `		int db = (*pb < bEnd) && SyisDigit(**pb);` |
|    361 | 1671 | `		if( !da && !db ){ return bias; }` |
|    289 | 1672 | `		if( !da ){ return -1; }` |
|    259 | 1673 | `		if( !db ){ return 1; }` |
|    195 | 1674 | `		if( **pa < **pb ){ if( !bias ){ bias = -1; } }` |
|    123 | 1675 | `		else if( **pa > **pb ){ if( !bias ){ bias = 1; } }` |
|    195 | 1676 | `		(*pa)++;` |
|    195 | 1677 | `		(*pb)++;` |
|      3 | 1678 | `	}` |
|     86 | 1679 | `}` |
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
|    248 | 1694 | `PH7_PRIVATE int PH7_StrNatCmp(const char *zA,int nA,const char *zB,int nB,int bFold)` |
|      3 | 1695 | `{` |
|    251 | 1696 | `	const char *a = zA,*aEnd = &zA[nA];` |
|    251 | 1697 | `	const char *b = zB,*bEnd = &zB[nB];` |
|    642 | 1698 | `	for(;;){` |
|      - | 1699 | `		int ca,cb;` |
|    777 | 1700 | `		while( a < aEnd && SyisSpace(a[0]) ){ a++; }` |
|    775 | 1701 | `		while( b < bEnd && SyisSpace(b[0]) ){ b++; }` |
|    775 | 1702 | `		ca = (a < aEnd) ? (unsigned char)a[0] : 0;` |
|    775 | 1703 | `		cb = (b < bEnd) ? (unsigned char)b[0] : 0;` |
|    775 | 1704 | `		if( SyisDigit(ca) && SyisDigit(cb) ){` |
|    171 | 1705 | `			int r = (ca == '0' \|\| cb == '0')` |
|      4 | 1706 | `				? StrNatCompareLeft(&a,aEnd,&b,bEnd)` |
|    251 | 1707 | `				: StrNatCompareRight(&a,aEnd,&b,bEnd);` |
|    173 | 1708 | `			if( r ){ return r; }` |
|     14 | 1709 | `			continue;` |
|      - | 1710 | `		}` |
|    605 | 1711 | `		if( ca == 0 && cb == 0 ){ return 0; }` |
|    583 | 1712 | `		if( bFold ){` |
|    285 | 1713 | `			ca = SyToLower(ca);` |
|    285 | 1714 | `			cb = SyToLower(cb);` |
|    141 | 1715 | `		}` |
|    583 | 1716 | `		if( ca < cb ){ return -1; }` |
|    535 | 1717 | `		if( ca > cb ){ return 1; }` |
|    515 | 1718 | `		a++;` |
|    515 | 1719 | `		b++;` |
|      3 | 1720 | `	}` |
|    127 | 1721 | `}` |
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
|      - | 1749 | ` * php's special version forms and their ordering` |
|      - | 1750 | ` * (compare_special_version_forms(), ext/standard/versioning.c). A form matches` |
|      - | 1751 | ` * a component by PREFIX -- "alpha3" is an alpha, "RC1" an RC -- and "#" is the` |
|      - | 1752 | ` * marker php compares a NUMERIC component as, spelled "#N#" at the call sites.` |
|      - | 1753 | ` * An unrecognized component ranks -1, BELOW dev.` |
|      - | 1754 | ` */` |
|      - | 1755 | `static const struct VersionForm {` |
|      - | 1756 | `	const char *zName;` |
|      - | 1757 | `	int nLen;` |
|      - | 1758 | `	int iOrder;` |
|      - | 1759 | `} aVersionForm[] = {` |
|      - | 1760 | `	{ "dev", 3, 0 }, { "alpha", 5, 1 }, { "a",  1, 1 }, { "beta", 4, 2 },` |
|      - | 1761 | `	{ "b",   1, 2 }, { "RC",    2, 3 }, { "rc", 2, 3 }, { "#",    1, 4 },` |
|      - | 1762 | `	{ "pl",  2, 5 }, { "p",     1, 5 },` |
|      - | 1763 | `};` |
|    144 | 1764 | `static int VersionFormOrder(const char *zPart)` |
|      1 | 1765 | `{` |
|      - | 1766 | `	sxu32 n;` |
|    977 | 1767 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVersionForm) ; ++n ){` |
|    955 | 1768 | `		if( SyStrncmp(zPart,aVersionForm[n].zName,(sxu32)aVersionForm[n].nLen) == 0 ){` |
|    123 | 1769 | `			return aVersionForm[n].iOrder;` |
|      - | 1770 | `		}` |
|    417 | 1771 | `	}` |
|     23 | 1772 | `	return -1;` |
|     73 | 1773 | `}` |
|     72 | 1774 | `static int VersionSpecialCmp(const char *zPart1,const char *zPart2)` |
|      1 | 1775 | `{` |
|     73 | 1776 | `	int iOrd1 = VersionFormOrder(zPart1);` |
|     73 | 1777 | `	int iOrd2 = VersionFormOrder(zPart2);` |
|     73 | 1778 | `	return iOrd1 < iOrd2 ? -1 : (iOrd1 > iOrd2 ? 1 : 0);` |
|      1 | 1779 | `}` |
|      - | 1780 | `/*` |
|      - | 1781 | ` * php_canonicalize_version(): '-', '_', '+' and every other non-alphanumeric` |
|      - | 1782 | ` * byte become '.', and a '.' is inserted at each digit<->non-digit boundary.` |
|      - | 1783 | ` * The FIRST byte is copied verbatim -- even a separator -- so "-1"` |
|      - | 1784 | ` * canonicalizes to "-.1" and not to "1", and its leading "-" then compares as` |
|      - | 1785 | ` * an unrecognized form. A trailing '.' is dropped rather than left as an empty` |
|      - | 1786 | ` * last component. zOut must hold 2*nLen + 2 bytes.` |
|      - | 1787 | ` */` |
|    518 | 1788 | `static void VersionCanonicalize(const char *zIn,sxu32 nLen,char *zOut)` |
|      1 | 1789 | `{` |
|      - | 1790 | `	const char *p,*pEnd;` |
|    519 | 1791 | `	char *q = zOut;` |
|      - | 1792 | `	int lp;` |
|    519 | 1793 | `	if( nLen < 1 ){` |
|    ! 0 | 1794 | `		zOut[0] = 0;` |
|    ! 0 | 1795 | `		return;` |
|      - | 1796 | `	}` |
|    519 | 1797 | `	lp = (unsigned char)zIn[0];` |
|    519 | 1798 | `	*q++ = (char)lp;` |
|    519 | 1799 | `	pEnd = &zIn[nLen];` |
|   1433 | 1800 | `	for( p = &zIn[1] ; p < pEnd ; lp = (unsigned char)*p++ ){` |
|    977 | 1801 | `		int c  = (unsigned char)p[0];` |
|    977 | 1802 | `		int lq = (unsigned char)q[-1];` |
|    977 | 1803 | `		if( c == '-' \|\| c == '_' \|\| c == '+' ){` |
|    181 | 1804 | `			if( lq != '.' ){ *q++ = '.'; }` |
|   1165 | 1805 | `		}else if( (!SyisDigit(lp) && lp != '.' && SyisDigit(c)) \|\|` |
|    846 | 1806 | `		          (SyisDigit(lp) && !SyisDigit(c) && c != '.') ){` |
|    199 | 1807 | `			if( lq != '.' ){ *q++ = '.'; }` |
|    109 | 1808 | `			*q++ = (char)c;` |
|    831 | 1809 | `		}else if( !SyisAlphaNum(c) ){` |
|    323 | 1810 | `			if( lq != '.' ){ *q++ = '.'; }` |
|    162 | 1811 | `		}else{` |
|    455 | 1812 | `			*q++ = (char)c;` |
|      - | 1813 | `		}` |
|    458 | 1814 | `	}` |
|    457 | 1815 | `	if( q[-1] == '.' ){` |
|     13 | 1816 | `		q[-1] = 0;` |
|      7 | 1817 | `	}else{` |
|    445 | 1818 | `		q[0] = 0;` |
|      - | 1819 | `	}` |
|    229 | 1820 | `}` |
|      - | 1821 | `/* strtol() over a canonical numeric component, saturating like the C library. */` |
|    680 | 1822 | `static sxi64 VersionPartToInt(const char *zPart)` |
|      1 | 1823 | `{` |
|    681 | 1824 | `	sxi64 iVal = 0;` |
|   1367 | 1825 | `	while( SyisDigit((unsigned char)zPart[0]) ){` |
|    687 | 1826 | `		if( iVal > (SXI64_HIGH - 9) / 10 ){` |
|    ! 0 | 1827 | `			return SXI64_HIGH;` |
|      - | 1828 | `		}` |
|    687 | 1829 | `		iVal = iVal * 10 + (zPart[0] - '0');` |
|    687 | 1830 | `		zPart++;` |
|      1 | 1831 | `	}` |
|    681 | 1832 | `	return iVal;` |
|    341 | 1833 | `}` |
|    824 | 1834 | `static char * VersionNextDot(char *zPart)` |
|      1 | 1835 | `{` |
|   1813 | 1836 | `	while( zPart[0] && zPart[0] != '.' ){ zPart++; }` |
|    825 | 1837 | `	return zPart[0] ? zPart : 0;` |
|      1 | 1838 | `}` |
|      - | 1839 | `/*` |
|      - | 1840 | ` * php_version_compare() over two CANONICAL buffers -- the caller has already` |
|      - | 1841 | ` * applied php's empty-operand shortcut to the ORIGINAL strings. Both buffers` |
|      - | 1842 | ` * are written in place (the walk NUL-terminates each component where php's` |
|      - | 1843 | ` * strchr does), so they must be writable copies.` |
|      - | 1844 | ` *` |
|      - | 1845 | ` * Where php recurses on the leftover of the longer version, this loops: the` |
|      - | 1846 | ` * recursion is a tail call, and its depth would otherwise grow with the` |
|      - | 1847 | ` * component count of an attacker-supplied string.` |
|      - | 1848 | ` */` |
|    232 | 1849 | `static int VersionCompareCanon(char *zV1,char *zV2)` |
|      1 | 1850 | `{` |
|    233 | 1851 | `	char zMark[] = "#N#";   /* php's "this component is a number" marker */` |
|    140 | 1852 | `	for(;;){` |
|      - | 1853 | `		char *p1,*p2,*n1,*n2;` |
|    257 | 1854 | `		int cmp = 0;` |
|    257 | 1855 | `		p1 = n1 = zV1;` |
|    257 | 1856 | `		p2 = n2 = zV2;` |
|    525 | 1857 | `		while( p1[0] && p2[0] && n1 && n2 ){` |
|    413 | 1858 | `			if( (n1 = VersionNextDot(p1)) != 0 ){ n1[0] = 0; }` |
|    413 | 1859 | `			if( (n2 = VersionNextDot(p2)) != 0 ){ n2[0] = 0; }` |
|    413 | 1860 | `			if( SyisDigit((unsigned char)p1[0]) && SyisDigit((unsigned char)p2[0]) ){` |
|    341 | 1861 | `				sxi64 l1 = VersionPartToInt(p1);` |
|    341 | 1862 | `				sxi64 l2 = VersionPartToInt(p2);` |
|    341 | 1863 | `				cmp = l1 < l2 ? -1 : (l1 > l2 ? 1 : 0);` |
|    243 | 1864 | `			}else if( !SyisDigit((unsigned char)p1[0]) && !SyisDigit((unsigned char)p2[0]) ){` |
|     59 | 1865 | `				cmp = VersionSpecialCmp(p1,p2);` |
|     44 | 1866 | `			}else if( SyisDigit((unsigned char)p1[0]) ){` |
|      5 | 1867 | `				cmp = VersionSpecialCmp(zMark,p2);` |
|      3 | 1868 | `			}else{` |
|     11 | 1869 | `				cmp = VersionSpecialCmp(p1,zMark);` |
|      - | 1870 | `			}` |
|    413 | 1871 | `			if( cmp != 0 ){ break; }` |
|    269 | 1872 | `			if( n1 ){ p1 = &n1[1]; }` |
|    269 | 1873 | `			if( n2 ){ p2 = &n2[1]; }` |
|      1 | 1874 | `		}` |
|    257 | 1875 | `		if( cmp != 0 ){` |
|    145 | 1876 | `			return cmp;` |
|      - | 1877 | `		}` |
|      - | 1878 | `		/*` |
|      - | 1879 | `		 * Equal so far and one side has components left, so php asks whether the` |
|      - | 1880 | `		 * next one outranks a plain number: "1.2.3" > "1.2" but "1.2.dev" < "1.2".` |
|      - | 1881 | `		 * The leftover can still hold dots ("1" vs "1#2" leaves "#.2"), which is` |
|      - | 1882 | `		 * why this is a whole comparison and not one special-form lookup.` |
|      - | 1883 | `		 */` |
|    113 | 1884 | `		if( n1 ){` |
|     25 | 1885 | `			if( SyisDigit((unsigned char)p1[0]) ){ return 1; }` |
|     23 | 1886 | `			if( p1[0] == 0 ){ return -1; }   /* php's empty-operand shortcut */` |
|     19 | 1887 | `			zV1 = p1;` |
|     19 | 1888 | `			zV2 = zMark;` |
|     98 | 1889 | `		}else if( n2 ){` |
|     29 | 1890 | `			if( SyisDigit((unsigned char)p2[0]) ){ return -1; }` |
|      7 | 1891 | `			if( p2[0] == 0 ){ return 1; }` |
|      7 | 1892 | `			zV1 = zMark;` |
|      7 | 1893 | `			zV2 = p2;` |
|      4 | 1894 | `		}else{` |
|     61 | 1895 | `			return 0;` |
|      - | 1896 | `		}` |
|      1 | 1897 | `	}` |
|    117 | 1898 | `}` |
|      - | 1899 | `/*` |
|      - | 1900 | ` * int\|bool version_compare(string $version1,string $version2,?string $operator = null)` |
|      - | 1901 | ` *  Compare two "PHP-standardized" version number strings: -1/0/1 without an` |
|      - | 1902 | ` *  $operator, the operator's verdict with one. An operator php does not know` |
|      - | 1903 | ` *  is a ValueError (php 8 stopped answering NULL for it).` |
|      - | 1904 | ` */` |
|    244 | 1905 | `PH7_PRIVATE int PH7_builtin_version_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1906 | `{` |
|      - | 1907 | `	static const struct VersionOp {` |
|      - | 1908 | `		const char *zName;` |
|      - | 1909 | `		int nLen;` |
|      - | 1910 | `		int bLt,bEq,bGt;   /* answer for cmp < 0, cmp == 0, cmp > 0 */` |
|      - | 1911 | `	} aVersionOp[] = {` |
|      - | 1912 | `		{ "<", 1, 1,0,0 }, { "lt", 2, 1,0,0 }, { "<=",2, 1,1,0 }, { "le",2, 1,1,0 },` |
|      - | 1913 | `		{ ">", 1, 0,0,1 }, { "gt", 2, 0,0,1 }, { ">=",2, 0,1,1 }, { "ge",2, 0,1,1 },` |
|      - | 1914 | `		{ "==",2, 0,1,0 }, { "=",  1, 0,1,0 }, { "eq",2, 0,1,0 },` |
|      - | 1915 | `		{ "!=",2, 1,0,1 }, { "<>", 2, 1,0,1 }, { "ne",2, 1,0,1 },` |
|      - | 1916 | `	};` |
|      - | 1917 | `	const char *zV1,*zV2,*zOp;` |
|      - | 1918 | `	sxu32 n1,n2,n;` |
|      - | 1919 | `	int cmp,nOp;` |
|    245 | 1920 | `	if( nArg < 2 ){` |
|      - | 1921 | `		/* Arity is enforced from aBuiltinSig[] before the call. */` |
|    ! 0 | 1922 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1923 | `		return PH7_OK;` |
|      - | 1924 | `	}` |
|      - | 1925 | `	/* php reads both operands as NUL-terminated C strings, so an embedded NUL` |
|      - | 1926 | `	 * ends the version there. ph7_value_to_string() null-appends. */` |
|    245 | 1927 | `	zV1 = ph7_value_to_string(apArg[0],0);` |
|    245 | 1928 | `	zV2 = ph7_value_to_string(apArg[1],0);` |
|    245 | 1929 | `	n1 = SyStrlen(zV1);` |
|    245 | 1930 | `	n2 = SyStrlen(zV2);` |
|    245 | 1931 | `	if( n1 < 1 \|\| n2 < 1 ){` |
|     13 | 1932 | `		cmp = (n1 == n2) ? 0 : (n1 > 0 ? 1 : -1);` |
|      7 | 1933 | `	}else{` |
|      - | 1934 | `		char *zC1,*zC2;` |
|    233 | 1935 | `		zC1 = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(2 * n1 + 2),FALSE,TRUE);` |
|    233 | 1936 | `		zC2 = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(2 * n2 + 2),FALSE,TRUE);` |
|    233 | 1937 | `		if( zC1 == 0 \|\| zC2 == 0 ){` |
|    ! 0 | 1938 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 1939 | `		}` |
|      - | 1940 | `		/* A version already starting with '#' is php's own marker: it bypasses` |
|      - | 1941 | `		 * canonicalization so that "#N#" survives as one component. */` |
|    233 | 1942 | `		if( zV1[0] == '#' ){ SyMemcpy(zV1,zC1,n1 + 1); }else{ VersionCanonicalize(zV1,n1,zC1); }` |
|    233 | 1943 | `		if( zV2[0] == '#' ){ SyMemcpy(zV2,zC2,n2 + 1); }else{ VersionCanonicalize(zV2,n2,zC2); }` |
|    233 | 1944 | `		cmp = VersionCompareCanon(zC1,zC2);` |
|      - | 1945 | `	}` |
|    245 | 1946 | `	if( nArg < 3 \|\| ph7_value_is_null(apArg[2]) ){` |
|     85 | 1947 | `		ph7_result_int(pCtx,cmp);` |
|     85 | 1948 | `		return PH7_OK;` |
|      - | 1949 | `	}` |
|    161 | 1950 | `	zOp = ph7_value_to_string(apArg[2],&nOp);` |
|   1141 | 1951 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVersionOp) ; ++n ){` |
|   1125 | 1952 | `		if( nOp == aVersionOp[n].nLen && SyMemcmp(zOp,aVersionOp[n].zName,(sxu32)nOp) == 0 ){` |
|    213 | 1953 | `			ph7_result_bool(pCtx,cmp < 0 ? aVersionOp[n].bLt` |
|     68 | 1954 | `				: (cmp > 0 ? aVersionOp[n].bGt : aVersionOp[n].bEq));` |
|    145 | 1955 | `			return PH7_OK;` |
|      - | 1956 | `		}` |
|    491 | 1957 | `	}` |
|     17 | 1958 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1959 | `		"version_compare(): Argument #3 ($operator) must be a valid comparison operator");` |
|    123 | 1960 | `}` |
|      - | 1961 | `/*` |
|      - | 1962 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 1963 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 1964 | ` * Parameter` |
|      - | 1965 | ` *  str1: The first string` |
|      - | 1966 | ` *  str2: The second string` |
|      - | 1967 | ` * Return` |
|      - | 1968 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1969 | ` *  than str2, and 0 if they are equal.` |
|      - | 1970 | ` */` |
|    600 | 1971 | `PH7_PRIVATE int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1972 | `{` |
|      - | 1973 | `	const char *z1,*z2;` |
|      - | 1974 | `	int res;` |
|      - | 1975 | `	int n;` |
|    601 | 1976 | `	if( nArg < 3 ){` |
|      - | 1977 | `		/* Perform a standard comparison */` |
|    ! 0 | 1978 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 1979 | `	}` |
|      - | 1980 | `	/* Desired comparison length */` |
|    601 | 1981 | `	n  = ph7_value_to_int(apArg[2]);` |
|    601 | 1982 | `	if( n < 0 ){` |
|      - | 1983 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 1984 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1985 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 1986 | `			ph7_function_name(pCtx));` |
|      - | 1987 | `	}` |
|      - | 1988 | `	/* Perform the comparison */` |
|    599 | 1989 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|    599 | 1990 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|    599 | 1991 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 1992 | `	/* Comparison result */` |
|    599 | 1993 | `	ph7_result_int(pCtx,res);` |
|    599 | 1994 | `	return PH7_OK;` |
|    301 | 1995 | `}` |
|      - | 1996 | `/*` |
|      - | 1997 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 1998 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 1999 | ` * Parameter` |
|      - | 2000 | ` *  str1: The first string` |
|      - | 2001 | ` *  str2: The second string` |
|      - | 2002 | ` * Return` |
|      - | 2003 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2004 | ` *  than str2, and 0 if they are equal.` |
|      - | 2005 | ` */` |
|    112 | 2006 | `PH7_PRIVATE int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2007 | `{` |
|      - | 2008 | `	const char *z1,*z2;` |
|      - | 2009 | `	int n1,n2;` |
|      - | 2010 | `	int res;` |
|    115 | 2011 | `	if( nArg < 2 ){` |
|    ! 0 | 2012 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 2013 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 2014 | `		return PH7_OK;` |
|      - | 2015 | `	}` |
|      - | 2016 | `	/* Perform the comparison */` |
|    115 | 2017 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|    115 | 2018 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|    115 | 2019 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2020 | `	/* Comparison result */` |
|    115 | 2021 | `	ph7_result_int(pCtx,res);` |
|    115 | 2022 | `	return PH7_OK;` |
|     59 | 2023 | `}` |
|      - | 2024 | `/*` |
|      - | 2025 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 2026 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 2027 | ` * Parameter` |
|      - | 2028 | ` *  $str1: The first string` |
|      - | 2029 | ` *  $str2: The second string` |
|      - | 2030 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 2031 | ` * Return` |
|      - | 2032 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2033 | ` *  than str2, and 0 if they are equal.` |
|      - | 2034 | ` */` |
|    106 | 2035 | `PH7_PRIVATE int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2036 | `{` |
|      - | 2037 | `	const char *z1,*z2;` |
|      - | 2038 | `	int res;` |
|      - | 2039 | `	int n;` |
|    111 | 2040 | `	if( nArg < 3 ){` |
|      - | 2041 | `		/* Perform a standard comparison */` |
|    ! 0 | 2042 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 2043 | `	}` |
|      - | 2044 | `	/* Desired comparison length */` |
|    111 | 2045 | `	n  = ph7_value_to_int(apArg[2]);` |
|    111 | 2046 | `	if( n < 0 ){` |
|      - | 2047 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 2048 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2049 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 2050 | `			ph7_function_name(pCtx));` |
|      - | 2051 | `	}` |
|      - | 2052 | `	/* Perform the comparison */` |
|    109 | 2053 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|    109 | 2054 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|    109 | 2055 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 2056 | `	/* Comparison result */` |
|    109 | 2057 | `	ph7_result_int(pCtx,res);` |
|    109 | 2058 | `	return PH7_OK;` |
|     58 | 2059 | `}` |
|      - | 2060 | `/*` |
|      - | 2061 | ` * Implode context [i.e: it's private data].` |
|      - | 2062 | ` * A pointer to the following structure is forwarded` |
|      - | 2063 | ` * verbatim to the array walker callback defined below.` |
|      - | 2064 | ` */` |
|      - | 2065 | `struct implode_data {` |
|      - | 2066 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 2067 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 2068 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 2069 | `	int nSeplen;          /* Separator length */` |
|      - | 2070 | `	int bFirst;           /* TRUE if first call */` |
|      - | 2071 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 2072 | `	sxi32 rc;             /* Captured allocation rc; SXERR_MEM => the builtin raises an OOM fatal */` |
|      - | 2073 | `	sxi32 rcThrow;        /* Captured coercion throw; the builtin propagates it instead of a result */` |
|      - | 2074 | `};` |
|      - | 2075 | `/*` |
|      - | 2076 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 2077 | ` * The following routine is invoked for each array entry passed` |
|      - | 2078 | ` * to the implode() function.` |
|      - | 2079 | ` */` |
| 231564 | 2080 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 2081 | `{` |
| 115782 | 2082 | `	SXUNUSED(pKey);` |
| 231569 | 2083 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 2084 | `	const char *zData;` |
|      - | 2085 | `	int nLen;` |
| 231569 | 2086 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 2087 | `		if( pData->nSeplen > 0 ){` |
|      3 | 2088 | `			if( !pData->bFirst ){` |
|      - | 2089 | `				/* append the separator first */` |
|      3 | 2090 | `				if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 2091 | `					pData->rc = SXERR_MEM;` |
|    ! 0 | 2092 | `					return PH7_ABORT;` |
|      - | 2093 | `				}` |
|      2 | 2094 | `			}else{` |
|    ! 0 | 2095 | `				pData->bFirst = 0;` |
|      - | 2096 | `			}` |
|      1 | 2097 | `		}` |
|      - | 2098 | `		/* Recurse */` |
|      3 | 2099 | `		pData->bFirst = 1;` |
|      3 | 2100 | `		pData->nRecCount++;` |
|      3 | 2101 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 2102 | `		pData->nRecCount--;` |
|      - | 2103 | `		/* Propagate an allocation failure surfaced deeper in the recursion. */` |
|      3 | 2104 | `		if( pData->rc != SXRET_OK ){` |
|    ! 0 | 2105 | `			return PH7_ABORT;` |
|      - | 2106 | `		}` |
|      3 | 2107 | `		return PH7_OK;` |
|      - | 2108 | `	}` |
|      - | 2109 | `	/* Extract the string representation of the entry value, USER-VISIBLY: an` |
|      - | 2110 | `	 * element that is itself an array renders as "Array" and warns, and one that` |
|      - | 2111 | `	 * is an object with no __toString() is php's catchable Error (it used to` |
|      - | 2112 | `	 * render as the literal "Object", §2). The walk cannot return a status, so` |
|      - | 2113 | `	 * park it on the context struct and abort. */` |
|      - | 2114 | `	{` |
| 231567 | 2115 | `		sxi32 rcSv = PH7_ValueToStringUV(pData->pCtx,pValue,&zData,&nLen);` |
| 231567 | 2116 | `		if( rcSv != SXRET_OK ){` |
|     18 | 2117 | `			pData->rcThrow = rcSv;` |
|     18 | 2118 | `			return PH7_ABORT;` |
|      - | 2119 | `		}` |
|      - | 2120 | `	}` |
|      - | 2121 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 231551 | 2122 | `	if( pData->bFirst ){` |
|  39377 | 2123 | `		pData->bFirst = 0;` |
| 211865 | 2124 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 2125 | `		/* append the separator first */` |
| 191625 | 2126 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 2127 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 2128 | `			return PH7_ABORT;` |
|      - | 2129 | `		}` |
|  95810 | 2130 | `	}` |
|      - | 2131 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 231551 | 2132 | `	if( nLen > 0 ){` |
| 214669 | 2133 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2134 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 2135 | `			return PH7_ABORT;` |
|      - | 2136 | `		}` |
| 107332 | 2137 | `	}` |
| 231551 | 2138 | `	return PH7_OK;` |
| 115787 | 2139 | `}` |
|      - | 2140 | `/*` |
|      - | 2141 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 2142 | ` * string implode(array $pieces,...)` |
|      - | 2143 | ` *  Join array elements with a string.` |
|      - | 2144 | ` * $glue` |
|      - | 2145 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 2146 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 2147 | ` * $pieces` |
|      - | 2148 | ` *   The array of strings to implode.` |
|      - | 2149 | ` * Return` |
|      - | 2150 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 2151 | ` *  order, with the glue string between each element.` |
|      - | 2152 | ` */` |
|  39514 | 2153 | `PH7_PRIVATE int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2154 | `{` |
|      - | 2155 | `	struct implode_data imp_data;` |
|      - | 2156 | `	/*` |
|      - | 2157 | `` 	 * php's contract for implode()/join(): one function, a `array\|string $separator` `` |
|      - | 2158 | ``	 * and a `?array $array` row, and a body that resolves the two ARITIES. The`` |
|      - | 2159 | `	 * messages report the overload as already resolved -- an $array argument in` |
|      - | 2160 | ``	 * position #2 means #1 is the SEPARATOR and must be a `string`, never the`` |
|      - | 2161 | `	 * union (which is only what the two arities accept BETWEEN them), and an ARRAY` |
|      - | 2162 | `	 * in position #1 with a second argument is that same #1 error. One aBuiltinSig` |
|      - | 2163 | `	 * row cannot say either (it drives both the accepted set and the message),` |
|      - | 2164 | `	 * which is why implode/join sit on azSelfChecked[] with strtr() and check` |
|      - | 2165 | `	 * their own rows here.` |
|      - | 2166 | `	 *` |
|      - | 2167 | ``	 * The messages name the INVOKED function: php reports `join(): ...` for`` |
|      - | 2168 | ``	 * join(), where this builtin used to hardcode `implode(): ...`.`` |
|      - | 2169 | `	 *` |
|      - | 2170 | `	 * One divergence, twin-paired in` |
|      - | 2171 | `	 * 002-integration/function/implode_separator_type{,_zend}.phpt. php 8.5 words` |
|      - | 2172 | `	 * the three cases below through a SPECIALIZED handler that only a DIRECT,` |
|      - | 2173 | ``	 * compile-time-resolved `implode(...)` call reaches; join(),`` |
|      - | 2174 | ``	 * `$f='implode'; $f(...)` and call_user_func('implode', ...) fall back to a`` |
|      - | 2175 | ``	 * generic path that answers differently (`array\|string` for the first, a #2`` |
|      - | 2176 | `	 * error for the second, and "ab" for the third). It is a call-FORM` |
|      - | 2177 | `	 * specialization, not a semantic rule -- it does not change with opcache off` |
|      - | 2178 | `	 * -- so PHL gives every call form the one contract php's direct calls use,` |
|      - | 2179 | `	 * which is the form real code writes and the form the corpus pins.` |
|      - | 2180 | `	 */` |
|  39519 | 2181 | `	const char *zName = ph7_function_name(pCtx);` |
|  39519 | 2182 | `	int i = 1;` |
|  39519 | 2183 | `	if( nArg < 1 ){` |
|      - | 2184 | `		/* Missing argument,return NULL */` |
|    ! 0 | 2185 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2186 | `		return PH7_OK;` |
|      - | 2187 | `	}` |
|      - | 2188 | `	/* Prepare the implode context */` |
|  39519 | 2189 | `	imp_data.pCtx = pCtx;` |
|  39519 | 2190 | `	imp_data.bRecursive = 0;` |
|  39519 | 2191 | `	imp_data.bFirst = 1;` |
|  39519 | 2192 | `	imp_data.nRecCount = 0;` |
|  39519 | 2193 | `	imp_data.rc = SXRET_OK;` |
|  39519 | 2194 | `	imp_data.rcThrow = SXRET_OK;` |
|  39519 | 2195 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  39497 | 2196 | `		if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|      - | 2197 | `			/* php only DEPRECATES null for the union parameter and coerces it to` |
|      - | 2198 | `			 * ""; PHL rejects it (§10 null-strictness), naming php's DECLARED type` |
|      - | 2199 | ``			 * -- the one case where `array\|string` is the right wording, because`` |
|      - | 2200 | `			 * php never narrows the union for a value it accepts. */` |
|    ! 0 | 2201 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|    ! 0 | 2202 | `				"%s(): Argument #1 ($separator) must be of type array\|string, null given",zName);` |
|      - | 2203 | `		}` |
|  39497 | 2204 | `		if( nArg < 2 \|\| ph7_value_is_null(apArg[1]) ){` |
|      - | 2205 | ``			/* php: a string separator REQUIRES the array. `implode("x")` and`` |
|      - | 2206 | ``			 * `implode("x", null)` both answered "" -- the `?array` in php's`` |
|      - | 2207 | `			 * signature is the DEFAULT's type, not a value it accepts. */` |
|     17 | 2208 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 2209 | `				"%s(): If argument #1 ($separator) is of type string, "` |
|      5 | 2210 | `				"argument #2 ($array) must be of type array, null given",zName);` |
|      - | 2211 | `		}` |
|  39487 | 2212 | `		if( !PH7_ArgSatisfiesString(apArg[0]) ){` |
|      - | 2213 | `			/* The overload is resolved, so #1 is the separator and must be a` |
|      - | 2214 | `			 * STRING. PHL used to fall through to the central screen here and` |
|      - | 2215 | ``			 * report the whole `array\|string` union instead. */`` |
|      - | 2216 | `			char zBuf[64];` |
|     10 | 2217 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 2218 | `				"%s(): Argument #1 ($separator) must be of type string, %s given",` |
|      3 | 2219 | `				zName,VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|      - | 2220 | `		}` |
|  39481 | 2221 | `		if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 2222 | `			/* php: implode($glue, $pieces) requires an ARRAY. PH7 stringified` |
|      - | 2223 | `			 * whatever it was handed, so implode(",", 5) quietly returned "5". */` |
|      - | 2224 | `			char zBuf[64];` |
|     12 | 2225 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 2226 | `				"%s(): Argument #2 ($array) must be of type ?array, %s given",` |
|      6 | 2227 | `				zName,VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 2228 | `		}` |
|  39475 | 2229 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  19740 | 2230 | `	}else{` |
|     25 | 2231 | `		if( nArg > 1 ){` |
|      - | 2232 | `			/* php 8 removed the legacy swapped order: implode($pieces, $glue) is a` |
|      - | 2233 | `			 * TypeError whatever $glue holds (PHL used to swap silently, a wrong` |
|      - | 2234 | `			 * ANSWER when the caller meant php's signature). One array argument` |
|      - | 2235 | `			 * alone stays the legal ""-glue form. */` |
|     26 | 2236 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      8 | 2237 | `				"%s(): Argument #1 ($separator) must be of type string, array given",zName);` |
|      - | 2238 | `		}` |
|      9 | 2239 | `		imp_data.zSep = 0;` |
|      9 | 2240 | `		imp_data.nSeplen = 0;` |
|      9 | 2241 | `		i = 0;` |
|      - | 2242 | `	}` |
|  39481 | 2243 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2244 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2245 | `	}` |
|      - | 2246 | `	/* Start the 'join' process */` |
|  78941 | 2247 | `	while( i < nArg ){` |
|  39481 | 2248 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2249 | `			/* Iterate throw array entries */` |
|  39481 | 2250 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2251 | `			/* An element whose coercion threw ends the join with that throw */` |
|  39481 | 2252 | `			if( imp_data.rcThrow != SXRET_OK ){` |
|     18 | 2253 | `				return imp_data.rcThrow;` |
|      - | 2254 | `			}` |
|      - | 2255 | `			/* Surface a callback allocation failure as a fatal */` |
|  39465 | 2256 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2257 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2258 | `			}` |
|  19735 | 2259 | `		}else{` |
|      - | 2260 | `			const char *zData;` |
|      - | 2261 | `			int nLen;` |
|      - | 2262 | `			/* Extract the string representation of the ph7 value (user-visible) */` |
|    ! 0 | 2263 | `			sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[i],&zData,&nLen);` |
|    ! 0 | 2264 | `			if( rcSv != SXRET_OK ){` |
|    ! 0 | 2265 | `				return rcSv;` |
|      - | 2266 | `			}` |
|      - | 2267 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 2268 | `			if( imp_data.bFirst ){` |
|    ! 0 | 2269 | `				imp_data.bFirst = 0;` |
|    ! 0 | 2270 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2271 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2272 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2273 | `				}` |
|    ! 0 | 2274 | `			}` |
|      - | 2275 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 2276 | `			if( nLen > 0 ){` |
|    ! 0 | 2277 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2278 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2279 | `				}` |
|    ! 0 | 2280 | `			}` |
|      - | 2281 | `		}` |
|  39465 | 2282 | `		i++;` |
|      5 | 2283 | `	}` |
|  39465 | 2284 | `	return PH7_OK;` |
|  19762 | 2285 | `}` |
|      - | 2286 | `/*` |
|      - | 2287 | ` * Symisc eXtension:` |
|      - | 2288 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 2289 | ` * Purpose` |
|      - | 2290 | ` *  Same as implode() but recurse on arrays.` |
|      - | 2291 | ` * Example:` |
|      - | 2292 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 2293 | ` *   echo implode_recursive("/",$a);` |
|      - | 2294 | ` *   Will output` |
|      - | 2295 | ` *     usr/home/dean.` |
|      - | 2296 | ` *   While the standard implode would produce.` |
|      - | 2297 | ` *    usr/Array.` |
|      - | 2298 | ` * Parameter` |
|      - | 2299 | ` *  Refer to implode().` |
|      - | 2300 | ` * Return` |
|      - | 2301 | ` *  Refer to implode().` |
|      - | 2302 | ` */` |
|     12 | 2303 | `PH7_PRIVATE int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2304 | `{` |
|      - | 2305 | `	struct implode_data imp_data;` |
|     13 | 2306 | `	int i = 1;` |
|     13 | 2307 | `	if( nArg < 1 ){` |
|      - | 2308 | `		/* Missing argument,return NULL */` |
|      3 | 2309 | `		ph7_result_null(pCtx);` |
|      3 | 2310 | `		return PH7_OK;` |
|      - | 2311 | `	}` |
|      - | 2312 | `	/* Prepare the implode context */` |
|     11 | 2313 | `	imp_data.pCtx = pCtx;` |
|     11 | 2314 | `	imp_data.bRecursive = 1;` |
|     11 | 2315 | `	imp_data.bFirst = 1;` |
|     11 | 2316 | `	imp_data.nRecCount = 0;` |
|     11 | 2317 | `	imp_data.rc = SXRET_OK;` |
|     11 | 2318 | `	imp_data.rcThrow = SXRET_OK;` |
|     11 | 2319 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 2320 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 2321 | `	}else{` |
|    ! 0 | 2322 | `		imp_data.zSep = 0;` |
|    ! 0 | 2323 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 2324 | `		i = 0;` |
|      - | 2325 | `	}` |
|     11 | 2326 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2327 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2328 | `	}` |
|      - | 2329 | `	/* Start the 'join' process */` |
|     21 | 2330 | `	while( i < nArg ){` |
|     11 | 2331 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2332 | `			/* Iterate throw array entries */` |
|      3 | 2333 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2334 | `			/* An element whose coercion threw ends the join with that throw */` |
|      3 | 2335 | `			if( imp_data.rcThrow != SXRET_OK ){` |
|    ! 0 | 2336 | `				return imp_data.rcThrow;` |
|      - | 2337 | `			}` |
|      - | 2338 | `			/* Surface a callback allocation failure as a fatal */` |
|      3 | 2339 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2340 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2341 | `			}` |
|      2 | 2342 | `		}else{` |
|      - | 2343 | `			const char *zData;` |
|      - | 2344 | `			int nLen;` |
|      - | 2345 | `			/* Extract the string representation of the ph7 value (user-visible) */` |
|      9 | 2346 | `			sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[i],&zData,&nLen);` |
|      9 | 2347 | `			if( rcSv != SXRET_OK ){` |
|    ! 0 | 2348 | `				return rcSv;` |
|      - | 2349 | `			}` |
|      - | 2350 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 2351 | `			if( imp_data.bFirst ){` |
|      9 | 2352 | `				imp_data.bFirst = 0;` |
|      4 | 2353 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2354 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2355 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2356 | `				}` |
|    ! 0 | 2357 | `			}` |
|      - | 2358 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 2359 | `			if( nLen > 0 ){` |
|      9 | 2360 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2361 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2362 | `				}` |
|      4 | 2363 | `			}` |
|      - | 2364 | `		}` |
|     11 | 2365 | `		i++;` |
|      1 | 2366 | `	}` |
|     11 | 2367 | `	return PH7_OK;` |
|      7 | 2368 | `}` |
|      - | 2369 | `/*` |
|      - | 2370 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 2371 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 2372 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 2373 | ` * Parameters` |
|      - | 2374 | ` *  $delimiter` |
|      - | 2375 | ` *   The boundary string.` |
|      - | 2376 | ` * $string` |
|      - | 2377 | ` *   The input string.` |
|      - | 2378 | ` * $limit` |
|      - | 2379 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 2380 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 2381 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 2382 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 2383 | ` * Returns` |
|      - | 2384 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 2385 | ` *  on boundaries formed by the delimiter.` |
|      - | 2386 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 2387 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 2388 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 2389 | ` *  will be returned.` |
|      - | 2390 | ` * NOTE:` |
|      - | 2391 | ` *  Negative limit is not supported.` |
|      - | 2392 | ` */` |
|   7900 | 2393 | `PH7_PRIVATE int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2394 | `{` |
|      - | 2395 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2396 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2397 | `	ph7_value *pArray;` |
|      - | 2398 | `	ph7_value *pValue;` |
|      - | 2399 | `	sxu32 nOfft;` |
|      - | 2400 | `	sxi32 rc;` |
|   7905 | 2401 | `	if( nArg < 2 ){` |
|      - | 2402 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2403 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2404 | `		return PH7_OK;` |
|      - | 2405 | `	}` |
|      - | 2406 | `	/* Extract the delimiter */` |
|   7905 | 2407 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   7905 | 2408 | `	if( nDelim < 1 ){` |
|      - | 2409 | `		/* Empty delimiter: PHP 8 throws a catchable ValueError. */` |
|      5 | 2410 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2411 | `			"explode(): Argument #1 ($separator) must not be empty");` |
|      - | 2412 | `	}` |
|      - | 2413 | `	/* Extract the string */` |
|   7901 | 2414 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   7901 | 2415 | `	if( nStrlen < 1 ){` |
|      - | 2416 | `		/* Empty string: normally an array with a single empty element (PHP behavior).` |
|      - | 2417 | `		 * A negative limit drops the last -limit components, so the sole empty` |
|      - | 2418 | `		 * component is dropped and the result is an empty array. */` |
|      7 | 2419 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      7 | 2420 | `		if( pArrayTmp == 0 ){` |
|      - | 2421 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 2422 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2423 | `			return PH7_OK;` |
|      - | 2424 | `		}` |
|      7 | 2425 | `		if( !(nArg > 2 && ph7_value_to_int(apArg[2]) < 0) ){` |
|      5 | 2426 | `			ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      5 | 2427 | `			if( pValueTmp == 0 ){` |
|      - | 2428 | `				/* Out of memory,return FALSE */` |
|    ! 0 | 2429 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 2430 | `				return PH7_OK;` |
|      - | 2431 | `			}` |
|      5 | 2432 | `			ph7_value_string(pValueTmp, "", 0);` |
|      5 | 2433 | `			if( ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp) != SXRET_OK ){` |
|    ! 0 | 2434 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2435 | `			}` |
|      2 | 2436 | `		}` |
|      7 | 2437 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      7 | 2438 | `		return PH7_OK;` |
|      - | 2439 | `	}` |
|      - | 2440 | `	/* Point to the end of the string */` |
|   7895 | 2441 | `	zEnd = &zString[nStrlen];` |
|      - | 2442 | `	/* Create the array */` |
|   7895 | 2443 | `	pArray =  ph7_context_new_array(pCtx);` |
|   7895 | 2444 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   7895 | 2445 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2446 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2447 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2448 | `		return PH7_OK;` |
|      - | 2449 | `	}` |
|      - | 2450 | `	/* Set a defualt limit */` |
|   7895 | 2451 | `	iLimit = SXI32_HIGH;` |
|   7895 | 2452 | `	if( nArg > 2 ){` |
|     93 | 2453 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     93 | 2454 | `		if( iLimit < 0 ){` |
|      - | 2455 | `			/* Negative limit: keep all components except the last -iLimit (PHP).` |
|      - | 2456 | `			 * Pre-count the components (delimiters + 1), then emit only the first` |
|      - | 2457 | `			 * nKeep CLEAN components — no trailing-remainder merge (the difference` |
|      - | 2458 | `			 * from the positive path). nKeep <= 0 drops everything -> empty array. */` |
|     17 | 2459 | `			int nTotal = 1,nKeep;` |
|     17 | 2460 | `			const char *zScan = zString;` |
|      - | 2461 | `			sxu32 nScanOfft;` |
|     57 | 2462 | `			while( SyBlobSearch(zScan,(sxu32)(zEnd - zScan),zDelim,nDelim,&nScanOfft) == SXRET_OK ){` |
|     41 | 2463 | `				nTotal++;` |
|     41 | 2464 | `				zScan = &zScan[nScanOfft + nDelim];` |
|      1 | 2465 | `			}` |
|     17 | 2466 | `			nKeep = nTotal + iLimit; /* iLimit < 0, so this is nTotal - (-iLimit) */` |
|     49 | 2467 | `			while( nKeep > (int)ph7_array_count(pArray)` |
|     39 | 2468 | `				&& SyBlobSearch(zString,(sxu32)(zEnd - zString),zDelim,nDelim,&nOfft) == SXRET_OK ){` |
|      - | 2469 | `				/* Emit the next clean component */` |
|     23 | 2470 | `				zCur = &zString[nOfft];` |
|     23 | 2471 | `				ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|     23 | 2472 | `				if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2473 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2474 | `				}` |
|     23 | 2475 | `				zString = &zCur[nDelim];` |
|     23 | 2476 | `				ph7_value_reset_string_cursor(pValue);` |
|      1 | 2477 | `			}` |
|     17 | 2478 | `			ph7_result_value(pCtx,pArray);` |
|     17 | 2479 | `			return PH7_OK;` |
|      - | 2480 | `		}` |
|     77 | 2481 | `		if( iLimit == 0 ){` |
|      5 | 2482 | `			iLimit = 1;` |
|      2 | 2483 | `		}` |
|     77 | 2484 | `		iLimit--;` |
|     36 | 2485 | `	}` |
|      - | 2486 | `	/* Start exploding */` |
| 121021 | 2487 | `	for(;;){` |
| 242047 | 2488 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 242047 | 2489 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2490 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   7879 | 2491 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   7879 | 2492 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2493 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2494 | `			}` |
|   7879 | 2495 | `			break;` |
|      - | 2496 | `		}` |
|      - | 2497 | `		/* Point to the desired offset */` |
| 234173 | 2498 | `		zCur = &zString[nOfft];` |
|      - | 2499 | `		/* Perform the store operation (may be empty) */` |
| 234173 | 2500 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 234173 | 2501 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2502 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2503 | `		}` |
|      - | 2504 | `		/* Point beyond the delimiter */` |
| 234173 | 2505 | `		zString = &zCur[nDelim];` |
|      - | 2506 | `		/* Reset the cursor */` |
| 234173 | 2507 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 2508 | `	}` |
|      - | 2509 | `	/* Return the freshly created array */` |
|   7879 | 2510 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2511 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2512 | `	 * released as soon we return from this foregin function.` |
|      - | 2513 | `	 */` |
|   7879 | 2514 | `	return PH7_OK;` |
|   3955 | 2515 | `}` |
|      - | 2516 | `/*` |
|      - | 2517 | ` * string trim(string $str[,string $charlist ])` |
|      - | 2518 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2519 | ` * Parameters` |
|      - | 2520 | ` *  $str` |
|      - | 2521 | ` *   The string that will be trimmed.` |
|      - | 2522 | ` * $charlist` |
|      - | 2523 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2524 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2525 | ` *   With .. you can specify a range of characters.` |
|      - | 2526 | ` * Returns.` |
|      - | 2527 | ` *  Thr processed string.` |
|      - | 2528 | ` * NOTE:` |
|      - | 2529 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2530 | ` */` |
|  24088 | 2531 | `PH7_PRIVATE int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2532 | `{` |
|  24093 | 2533 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"trim",1,"$string"); }` |
|      - | 2534 | `	const char *zString;` |
|      - | 2535 | `	int nLen;` |
|  24093 | 2536 | `	if( nArg < 1 ){` |
|      - | 2537 | `		/* Missing arguments,return null */` |
|    ! 0 | 2538 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2539 | `		return PH7_OK;` |
|      - | 2540 | `	}` |
|      - | 2541 | `	/* Extract the target string */` |
|  24093 | 2542 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  24093 | 2543 | `	if( nLen < 1 ){` |
|      - | 2544 | `		/* Empty string,return */` |
|   8087 | 2545 | `		ph7_result_string(pCtx,"",0);` |
|   8087 | 2546 | `		return PH7_OK;` |
|      - | 2547 | `	}` |
|      - | 2548 | `	/* Start the trim process */` |
|  16011 | 2549 | `	if( nArg < 2 ){` |
|      - | 2550 | `		SyString sStr;` |
|      - | 2551 | `		/* Remove white spaces and NUL bytes */` |
|  15975 | 2552 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  40811 | 2553 | `		SyStringFullTrimSafe(&sStr);` |
|  15975 | 2554 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   7990 | 2555 | `	}else{` |
|      - | 2556 | `		/* Char list */` |
|      - | 2557 | `		const char *zList;` |
|      - | 2558 | `		int nListlen;` |
|     39 | 2559 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     39 | 2560 | `		if( nListlen < 1 ){` |
|      - | 2561 | `			/* Return the string unchanged */` |
|      6 | 2562 | `			ph7_result_string(pCtx,zString,nLen);` |
|      4 | 2563 | `		}else{` |
|      - | 2564 | `			char aMask[256];` |
|     35 | 2565 | `			const char *zEnd = &zString[nLen];` |
|     35 | 2566 | `			const char *zCur = zString;` |
|     35 | 2567 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2568 | `			/* Left trim */` |
|     91 | 2569 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     59 | 2570 | `				zCur++;` |
|      3 | 2571 | `			}` |
|      - | 2572 | `			/* Right trim */` |
|     85 | 2573 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     53 | 2574 | `				zEnd--;` |
|      3 | 2575 | `			}` |
|     35 | 2576 | `			if( zCur >= zEnd ){` |
|      - | 2577 | `				/* Return the empty string */` |
|    ! 0 | 2578 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2579 | `			}else{` |
|     35 | 2580 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2581 | `			}` |
|      - | 2582 | `		}` |
|      - | 2583 | `	}` |
|  16011 | 2584 | `	return PH7_OK;` |
|  12049 | 2585 | `}` |
|      - | 2586 | `/*` |
|      - | 2587 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 2588 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 2589 | ` * Parameters` |
|      - | 2590 | ` *  $str` |
|      - | 2591 | ` *   The string that will be trimmed.` |
|      - | 2592 | ` * $charlist` |
|      - | 2593 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2594 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2595 | ` *   With .. you can specify a range of characters.` |
|      - | 2596 | ` * Returns.` |
|      - | 2597 | ` *  Thr processed string.` |
|      - | 2598 | ` * NOTE:` |
|      - | 2599 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2600 | ` */` |
|    190 | 2601 | `PH7_PRIVATE int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2602 | `{` |
|    193 | 2603 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"rtrim",1,"$string"); }` |
|      - | 2604 | `	const char *zString;` |
|      - | 2605 | `	int nLen;` |
|    193 | 2606 | `	if( nArg < 1 ){` |
|      - | 2607 | `		/* Missing arguments,return null */` |
|    ! 0 | 2608 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2609 | `		return PH7_OK;` |
|      - | 2610 | `	}` |
|      - | 2611 | `	/* Extract the target string */` |
|    193 | 2612 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    193 | 2613 | `	if( nLen < 1 ){` |
|      - | 2614 | `		/* Empty string,return */` |
|      8 | 2615 | `		ph7_result_string(pCtx,"",0);` |
|      8 | 2616 | `		return PH7_OK;` |
|      - | 2617 | `	}` |
|      - | 2618 | `	/* Start the trim process */` |
|    187 | 2619 | `	if( nArg < 2 ){` |
|      - | 2620 | `		SyString sStr;` |
|      - | 2621 | `		/* Remove white spaces and NUL bytes*/` |
|     19 | 2622 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     48 | 2623 | `		SyStringRightTrimSafe(&sStr);` |
|     19 | 2624 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|     10 | 2625 | `	}else{` |
|      - | 2626 | `		/* Char list */` |
|      - | 2627 | `		const char *zList;` |
|      - | 2628 | `		int nListlen;` |
|    169 | 2629 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|    169 | 2630 | `		if( nListlen < 1 ){` |
|      - | 2631 | `			/* Return the string unchanged */` |
|    ! 0 | 2632 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 2633 | `		}else{` |
|      - | 2634 | `			char aMask[256];` |
|    169 | 2635 | `			const char *zEnd = &zString[nLen];` |
|    169 | 2636 | `			const char *zCur = zString;` |
|    169 | 2637 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2638 | `			/* Right trim */` |
|    187 | 2639 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     20 | 2640 | `				zEnd--;` |
|      2 | 2641 | `			}` |
|    169 | 2642 | `			if( zEnd <= zCur ){` |
|      - | 2643 | `				/* Return the empty string */` |
|    ! 0 | 2644 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2645 | `			}else{` |
|    169 | 2646 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2647 | `			}` |
|      - | 2648 | `		}` |
|      - | 2649 | `	}` |
|    187 | 2650 | `	return PH7_OK;` |
|     98 | 2651 | `}` |
|      - | 2652 | `/*` |
|      - | 2653 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 2654 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2655 | ` * Parameters` |
|      - | 2656 | ` *  $str` |
|      - | 2657 | ` *   The string that will be trimmed.` |
|      - | 2658 | ` * $charlist` |
|      - | 2659 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2660 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2661 | ` *   With .. you can specify a range of characters.` |
|      - | 2662 | ` * Returns.` |
|      - | 2663 | ` *  Thr processed string.` |
|      - | 2664 | ` * NOTE:` |
|      - | 2665 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2666 | ` */` |
|    108 | 2667 | `PH7_PRIVATE int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2668 | `{` |
|    113 | 2669 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"ltrim",1,"$string"); }` |
|      - | 2670 | `	const char *zString;` |
|      - | 2671 | `	int nLen;` |
|    113 | 2672 | `	if( nArg < 1 ){` |
|      - | 2673 | `		/* Missing arguments,return null */` |
|    ! 0 | 2674 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2675 | `		return PH7_OK;` |
|      - | 2676 | `	}` |
|      - | 2677 | `	/* Extract the target string */` |
|    113 | 2678 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    113 | 2679 | `	if( nLen < 1 ){` |
|      - | 2680 | `		/* Empty string,return */` |
|     26 | 2681 | `		ph7_result_string(pCtx,"",0);` |
|     26 | 2682 | `		return PH7_OK;` |
|      - | 2683 | `	}` |
|      - | 2684 | `	/* Start the trim process */` |
|     91 | 2685 | `	if( nArg < 2 ){` |
|      - | 2686 | `		SyString sStr;` |
|      - | 2687 | `		/* Remove white spaces and NUL byte */` |
|      3 | 2688 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|      8 | 2689 | `		SyStringLeftTrimSafe(&sStr);` |
|      3 | 2690 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      2 | 2691 | `	}else{` |
|      - | 2692 | `		/* Char list */` |
|      - | 2693 | `		const char *zList;` |
|      - | 2694 | `		int nListlen;` |
|     89 | 2695 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     89 | 2696 | `		if( nListlen < 1 ){` |
|      - | 2697 | `			/* Return the string unchanged */` |
|      3 | 2698 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2699 | `		}else{` |
|      - | 2700 | `			char aMask[256];` |
|     87 | 2701 | `			const char *zEnd = &zString[nLen];` |
|     87 | 2702 | `			const char *zCur = zString;` |
|     87 | 2703 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2704 | `			/* Left trim */` |
|    221 | 2705 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|    139 | 2706 | `				zCur++;` |
|      5 | 2707 | `			}` |
|     87 | 2708 | `			if( zCur >= zEnd ){` |
|      - | 2709 | `				/* Return the empty string */` |
|    ! 0 | 2710 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2711 | `			}else{` |
|     87 | 2712 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2713 | `			}` |
|      - | 2714 | `		}` |
|      - | 2715 | `	}` |
|     91 | 2716 | `	return PH7_OK;` |
|     59 | 2717 | `}` |
|      - | 2718 | `/*` |
|      - | 2719 | ` * string strtolower(string $str)` |
|      - | 2720 | ` *  Make a string lowercase.` |
|      - | 2721 | ` * Parameters` |
|      - | 2722 | ` *  $str` |
|      - | 2723 | ` *   The input string.` |
|      - | 2724 | ` * Returns.` |
|      - | 2725 | ` *  The lowercased string.` |
|      - | 2726 | ` */` |
|  38206 | 2727 | `PH7_PRIVATE int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2728 | `{` |
|  38211 | 2729 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtolower",1,"$string"); }` |
|      - | 2730 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2731 | `	int nLen;` |
|  38211 | 2732 | `	if( nArg < 1 ){` |
|      - | 2733 | `		/* Missing arguments,return null */` |
|    ! 0 | 2734 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2735 | `		return PH7_OK;` |
|      - | 2736 | `	}` |
|      - | 2737 | `	/* Extract the target string */` |
|  38211 | 2738 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  38211 | 2739 | `	if( nLen < 1 ){` |
|      - | 2740 | `		/* Empty string,return */` |
|      6 | 2741 | `		ph7_result_string(pCtx,"",0);` |
|      6 | 2742 | `		return PH7_OK;` |
|      - | 2743 | `	}` |
|      - | 2744 | `	/* Perform the requested operation */` |
|  38207 | 2745 | `	zEnd = &zString[nLen];` |
| 120953 | 2746 | `	for(;;){` |
| 241911 | 2747 | `		if( zString >= zEnd ){` |
|      - | 2748 | `			/* No more input,break immediately */` |
|  38207 | 2749 | `			break;` |
|      - | 2750 | `		}` |
| 203709 | 2751 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2752 | `			/* UTF-8 stream,output verbatim */` |
|      9 | 2753 | `			zCur = zString;` |
|      9 | 2754 | `			zString++;` |
|     13 | 2755 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|      5 | 2756 | `				zString++;` |
|      1 | 2757 | `			}` |
|      - | 2758 | `			/* Append UTF-8 stream */` |
|      9 | 2759 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|      5 | 2760 | `		}else{` |
| 203701 | 2761 | `			int c = zString[0];` |
| 203701 | 2762 | `			if( SyisUpper(c) ){` |
| 203537 | 2763 | `				c = SyToLower(zString[0]);` |
| 101766 | 2764 | `			}` |
|      - | 2765 | `			/* Append character */` |
| 203701 | 2766 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2767 | `			/* Advance the cursor */` |
| 203701 | 2768 | `			zString++;` |
|      - | 2769 | `		}` |
|      5 | 2770 | `	}` |
|  38207 | 2771 | `	return PH7_OK;` |
|  19108 | 2772 | `}` |
|      - | 2773 | `/*` |
|      - | 2774 | ` * string strtolower(string $str)` |
|      - | 2775 | ` *  Make a string uppercase.` |
|      - | 2776 | ` * Parameters` |
|      - | 2777 | ` *  $str` |
|      - | 2778 | ` *   The input string.` |
|      - | 2779 | ` * Returns.` |
|      - | 2780 | ` *  The uppercased string.` |
|      - | 2781 | ` */` |
|    140 | 2782 | `PH7_PRIVATE int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2783 | `{` |
|    145 | 2784 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtoupper",1,"$string"); }` |
|      - | 2785 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2786 | `	int nLen;` |
|    145 | 2787 | `	if( nArg < 1 ){` |
|      - | 2788 | `		/* Missing arguments,return null */` |
|    ! 0 | 2789 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2790 | `		return PH7_OK;` |
|      - | 2791 | `	}` |
|      - | 2792 | `	/* Extract the target string */` |
|    145 | 2793 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    145 | 2794 | `	if( nLen < 1 ){` |
|      - | 2795 | `		/* Empty string,return */` |
|      6 | 2796 | `		ph7_result_string(pCtx,"",0);` |
|      6 | 2797 | `		return PH7_OK;` |
|      - | 2798 | `	}` |
|      - | 2799 | `	/* Perform the requested operation */` |
|    141 | 2800 | `	zEnd = &zString[nLen];` |
|    290 | 2801 | `	for(;;){` |
|    585 | 2802 | `		if( zString >= zEnd ){` |
|      - | 2803 | `			/* No more input,break immediately */` |
|    141 | 2804 | `			break;` |
|      - | 2805 | `		}` |
|    449 | 2806 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2807 | `			/* UTF-8 stream,output verbatim */` |
|      9 | 2808 | `			zCur = zString;` |
|      9 | 2809 | `			zString++;` |
|     13 | 2810 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|      5 | 2811 | `				zString++;` |
|      1 | 2812 | `			}` |
|      - | 2813 | `			/* Append UTF-8 stream */` |
|      9 | 2814 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|      5 | 2815 | `		}else{` |
|    441 | 2816 | `			int c = zString[0];` |
|    441 | 2817 | `			if( SyisLower(c) ){` |
|    401 | 2818 | `				c = SyToUpper(zString[0]);` |
|    198 | 2819 | `			}` |
|      - | 2820 | `			/* Append character */` |
|    441 | 2821 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2822 | `			/* Advance the cursor */` |
|    441 | 2823 | `			zString++;` |
|      - | 2824 | `		}` |
|      5 | 2825 | `	}` |
|    141 | 2826 | `	return PH7_OK;` |
|     75 | 2827 | `}` |
|      - | 2828 | `/*` |
|      - | 2829 | ` * string ucfirst(string $str)` |
|      - | 2830 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2831 | ` *  character is alphabetic.` |
|      - | 2832 | ` * Parameters` |
|      - | 2833 | ` *  $str` |
|      - | 2834 | ` *   The input string.` |
|      - | 2835 | ` * Returns.` |
|      - | 2836 | ` *  The processed string.` |
|      - | 2837 | ` */` |
|      8 | 2838 | `PH7_PRIVATE int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2839 | `{` |
|      - | 2840 | `	const char *zString,*zEnd;` |
|      - | 2841 | `	int nLen,c;` |
|     10 | 2842 | `	if( nArg < 1 ){` |
|      - | 2843 | `		/* Missing arguments,return null */` |
|    ! 0 | 2844 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2845 | `		return PH7_OK;` |
|      - | 2846 | `	}` |
|      - | 2847 | `	/* Extract the target string */` |
|     10 | 2848 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     10 | 2849 | `	if( nLen < 1 ){` |
|      - | 2850 | `		/* Empty string,return */` |
|      6 | 2851 | `		ph7_result_string(pCtx,"",0);` |
|      6 | 2852 | `		return PH7_OK;` |
|      - | 2853 | `	}` |
|      - | 2854 | `	/* Perform the requested operation */` |
|      5 | 2855 | `	zEnd = &zString[nLen];` |
|      5 | 2856 | `	c = zString[0];` |
|      5 | 2857 | `	if( SyisLower(c) ){` |
|      3 | 2858 | `		c = SyToUpper(c);` |
|      1 | 2859 | `	}` |
|      - | 2860 | `	/* Append the first character */` |
|      5 | 2861 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      5 | 2862 | `	zString++;` |
|      5 | 2863 | `	if( zString < zEnd ){` |
|      - | 2864 | `		/* Append the rest of the input verbatim */` |
|      5 | 2865 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      2 | 2866 | `	}` |
|      5 | 2867 | `	return PH7_OK;` |
|      6 | 2868 | `}` |
|      - | 2869 | `/*` |
|      - | 2870 | ` * string lcfirst(string $str)` |
|      - | 2871 | ` *  Make a string's first character lowercase.` |
|      - | 2872 | ` * Parameters` |
|      - | 2873 | ` *  $str` |
|      - | 2874 | ` *   The input string.` |
|      - | 2875 | ` * Returns.` |
|      - | 2876 | ` *  The processed string.` |
|      - | 2877 | ` */` |
|      8 | 2878 | `PH7_PRIVATE int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2879 | `{` |
|      - | 2880 | `	const char *zString,*zEnd;` |
|      - | 2881 | `	int nLen,c;` |
|     10 | 2882 | `	if( nArg < 1 ){` |
|      - | 2883 | `		/* Missing arguments,return null */` |
|    ! 0 | 2884 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2885 | `		return PH7_OK;` |
|      - | 2886 | `	}` |
|      - | 2887 | `	/* Extract the target string */` |
|     10 | 2888 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     10 | 2889 | `	if( nLen < 1 ){` |
|      - | 2890 | `		/* Empty string,return */` |
|      6 | 2891 | `		ph7_result_string(pCtx,"",0);` |
|      6 | 2892 | `		return PH7_OK;` |
|      - | 2893 | `	}` |
|      - | 2894 | `	/* Perform the requested operation */` |
|      5 | 2895 | `	zEnd = &zString[nLen];` |
|      5 | 2896 | `	c = zString[0];` |
|      5 | 2897 | `	if( SyisUpper(c) ){` |
|      3 | 2898 | `		c = SyToLower(c);` |
|      1 | 2899 | `	}` |
|      - | 2900 | `	/* Append the first character */` |
|      5 | 2901 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      5 | 2902 | `	zString++;` |
|      5 | 2903 | `	if( zString < zEnd ){` |
|      - | 2904 | `		/* Append the rest of the input verbatim */` |
|      5 | 2905 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      2 | 2906 | `	}` |
|      5 | 2907 | `	return PH7_OK;` |
|      6 | 2908 | `}` |
|      - | 2909 | `/*` |
|      - | 2910 | ` * int ord(string $string)` |
|      - | 2911 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 2912 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 2913 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 2914 | ` * Parameters` |
|      - | 2915 | ` *  $string` |
|      - | 2916 | ` *   The input string.` |
|      - | 2917 | ` * Returns` |
|      - | 2918 | ` *  The ASCII value as an integer.` |
|      - | 2919 | ` */` |
|     74 | 2920 | `PH7_PRIVATE int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2921 | `{` |
|      - | 2922 | `	const char *zString;` |
|      - | 2923 | `	int nLen,c;` |
|      - | 2924 | `	/* PHP requires exactly one argument. */` |
|     76 | 2925 | `	if( nArg != 1 ){` |
|    ! 0 | 2926 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2927 | `			"ArgumentCountError",` |
|      - | 2928 | `			"ord() expects exactly 1 argument, %d given",` |
|    ! 0 | 2929 | `			nArg` |
|      - | 2930 | `			);` |
|      - | 2931 | `	}` |
|      - | 2932 | `	/* php only DEPRECATES null here; PHL rejects it. */` |
|     76 | 2933 | `	if( ph7_value_is_null(apArg[0]) ){` |
|    ! 0 | 2934 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 2935 | `			"ord(): Argument #1 ($character) must be of type string, null given"` |
|      - | 2936 | `			);` |
|      - | 2937 | `	}` |
|      - | 2938 | `	/* Extract the target string */` |
|     76 | 2939 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     76 | 2940 | `	if( nLen < 1 ){` |
|      - | 2941 | `		/* php only DEPRECATES an empty string here; PHL rejects it. */` |
|      3 | 2942 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2943 | `			"ord(): Argument #1 ($character) must not be empty"` |
|      - | 2944 | `			);` |
|      - | 2945 | `	}` |
|      - | 2946 | `	/* A string longer than one byte: php DEPRECATES it; PHL rejects it. */` |
|     74 | 2947 | `	if( nLen > 1 ){` |
|      3 | 2948 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2949 | `			"ord(): Argument #1 ($character) must be a single byte, use ord($str[0]) instead"` |
|      - | 2950 | `			);` |
|      - | 2951 | `	}` |
|      - | 2952 | `	/* Extract the ASCII value of the first character */` |
|     72 | 2953 | `	c = (unsigned char)zString[0];` |
|      - | 2954 | `	/* Return that value */` |
|     72 | 2955 | `	ph7_result_int(pCtx,c);` |
|     72 | 2956 | `	return PH7_OK;` |
|     39 | 2957 | `}` |
|      - | 2958 | `/*` |
|      - | 2959 | ` * string chr(int $codepoint)` |
|      - | 2960 | ` *  Returns a one-character string containing the character specified` |
|      - | 2961 | ` *  by the given codepoint, which must be in the [0, 255] range.` |
|      - | 2962 | ` * Parameters` |
|      - | 2963 | ` *  $codepoint` |
|      - | 2964 | ` *   An integer codepoint in [0, 255]. php merely deprecates values` |
|      - | 2965 | ` *   outside that range (constraining them with % 256); PHL rejects` |
|      - | 2966 | ` *   them with a ValueError (scope policy).` |
|      - | 2967 | ` * Returns` |
|      - | 2968 | ` *  A single-character string.` |
|      - | 2969 | ` */` |
|   3670 | 2970 | `PH7_PRIVATE int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2971 | `{` |
|      - | 2972 | `	int c;` |
|      - | 2973 | `	unsigned char ch;` |
|      - | 2974 | `	/* PHP requires exactly one argument. */` |
|   3675 | 2975 | `	if( nArg != 1 ){` |
|    ! 0 | 2976 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2977 | `			"ArgumentCountError",` |
|      - | 2978 | `			"chr() expects exactly 1 argument, %d given",` |
|    ! 0 | 2979 | `			nArg` |
|      - | 2980 | `			);` |
|      - | 2981 | `	}` |
|      - | 2982 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 2983 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 2984 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 2985 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|   3675 | 2986 | `	if( ph7_value_is_float(apArg[0]) ){` |
|    ! 0 | 2987 | `		double d = ph7_value_to_double(apArg[0]);` |
|    ! 0 | 2988 | `		if( d != (double)(sxi64)d ){` |
|      - | 2989 | `			/* php only DEPRECATES a lossy float->int here; PHL rejects it. */` |
|    ! 0 | 2990 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 2991 | `				"chr(): Argument #1 ($codepoint) must be of type int, float given");` |
|      - | 2992 | `		}` |
|    ! 0 | 2993 | `	}` |
|      - | 2994 | `	/* Extract the codepoint. */` |
|   3675 | 2995 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 2996 | `	/* php only DEPRECATES an out-of-range codepoint (constraining it with % 256);` |
|      - | 2997 | `	 * PHL targets php's non-deprecated surface and rejects it loudly, matching the` |
|      - | 2998 | `	 * lossy-float branch above. This was the last engine site still emitting` |
|      - | 2999 | `	 * E_DEPRECATED — the scope policy says none remain. */` |
|   3675 | 3000 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 3001 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 3002 | `			"chr(): Argument #1 ($codepoint) must be between 0 and 255");` |
|      - | 3003 | `	}` |
|      - | 3004 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 3005 | `	 * when taking the address of a wider int. */` |
|   3671 | 3006 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 3007 | `	/* Return the specified character */` |
|   3671 | 3008 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|   3671 | 3009 | `	return PH7_OK;` |
|   1840 | 3010 | `}` |
|      - | 3011 | `/*` |
|      - | 3012 | ` * Binary to hex consumer callback.` |
|      - | 3013 | ` * This callback is the default consumer used by the hash functions` |
|      - | 3014 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 3015 | ` */` |
|   5712 | 3016 | `PH7_PRIVATE int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      4 | 3017 | `{` |
|      - | 3018 | `	/* Append hex chunk verbatim */` |
|   5716 | 3019 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   5716 | 3020 | `	return SXRET_OK;` |
|      4 | 3021 | `}` |
|      - | 3022 |  |
|      - | 3023 | `/*` |
|      - | 3024 | ` * string bin2hex(string $str)` |
|      - | 3025 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 3026 | ` * Parameters` |
|      - | 3027 | ` *  $str` |
|      - | 3028 | ` *   The input string.` |
|      - | 3029 | ` * Returns.` |
|      - | 3030 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 3031 | ` */` |
|    826 | 3032 | `PH7_PRIVATE int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 3033 | `{` |
|      - | 3034 | `	const char *zString;` |
|      - | 3035 | `	int nLen;` |
|      - | 3036 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|    830 | 3037 | `	if( nArg != 1 ){` |
|    ! 0 | 3038 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3039 | `			"ArgumentCountError",` |
|      - | 3040 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|    ! 0 | 3041 | `			nArg` |
|      - | 3042 | `			);` |
|      - | 3043 | `	}` |
|      - | 3044 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 3045 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 3046 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 3047 | `	 */` |
|   1243 | 3048 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|    413 | 3049 | `		( ph7_value_is_object(apArg[0]) &&` |
|    ! 0 | 3050 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|    ! 0 | 3051 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|    ! 0 | 3052 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 3053 | `		)` |
|      - | 3054 | `	){` |
|    ! 0 | 3055 | `		const char *zType = ph7_type_name(apArg[0]);` |
|    ! 0 | 3056 | `		if( ph7_value_is_object(apArg[0]) ){` |
|    ! 0 | 3057 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    ! 0 | 3058 | `			if( pInst && pInst->pClass ){` |
|    ! 0 | 3059 | `				zType = SyStringData(&pInst->pClass->sName);` |
|    ! 0 | 3060 | `			}` |
|    ! 0 | 3061 | `		}` |
|    ! 0 | 3062 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3063 | `			"TypeError",` |
|      - | 3064 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 3065 | `			zType` |
|      - | 3066 | `			);` |
|      - | 3067 | `	}` |
|      - | 3068 | `	/* Extract the target string */` |
|    830 | 3069 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    830 | 3070 | `	if( nLen < 1 ){` |
|      - | 3071 | `		/* Empty string,return */` |
|     45 | 3072 | `		ph7_result_string(pCtx,"",0);` |
|     45 | 3073 | `		return PH7_OK;` |
|      - | 3074 | `	}` |
|      - | 3075 | `	/* Perform the requested operation */` |
|    786 | 3076 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|    786 | 3077 | `	return PH7_OK;` |
|    417 | 3078 | `}` |
|      - | 3079 |  |
|      - | 3080 | `/* Search callback signature */` |
|      - | 3081 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 3082 | `/*` |
|      - | 3083 | ` * Case-insensitive pattern match.` |
|      - | 3084 | ` * Brute force is the default search method used here.` |
|      - | 3085 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 3086 | ` * well for short/medium texts on modern hardware.` |
|      - | 3087 | ` */` |
|     98 | 3088 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      1 | 3089 | `{` |
|     99 | 3090 | `	const char *zpIn = (const char *)pPattern;` |
|     99 | 3091 | `	const char *zIn = (const char *)pText;` |
|     99 | 3092 | `	const char *zpEnd = &zpIn[iPatLen];` |
|     99 | 3093 | `	const char *zEnd = &zIn[nLen];` |
|      - | 3094 | `	const char *zPtr,*zPtr2;` |
|      - | 3095 | `	int c,d;` |
|     99 | 3096 | `	if( iPatLen > nLen ){` |
|      - | 3097 | `		/* Don't bother processing */` |
|    ! 0 | 3098 | `		return SXERR_NOTFOUND;` |
|      - | 3099 | `	}` |
|    170 | 3100 | `	for(;;){` |
|    341 | 3101 | `		if( zIn >= zEnd ){` |
|     41 | 3102 | `			break;` |
|      - | 3103 | `		}` |
|    301 | 3104 | `		c = SyToLower(zIn[0]);` |
|    301 | 3105 | `		d = SyToLower(zpIn[0]);` |
|    301 | 3106 | `		if( c == d ){` |
|     59 | 3107 | `			zPtr   = &zIn[1];` |
|     59 | 3108 | `			zPtr2  = &zpIn[1];` |
|     80 | 3109 | `			for(;;){` |
|    161 | 3110 | `				if( zPtr2 >= zpEnd ){` |
|      - | 3111 | `					/* Pattern found */` |
|     59 | 3112 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     59 | 3113 | `					return SXRET_OK;` |
|      - | 3114 | `				}` |
|    103 | 3115 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 3116 | `					break;` |
|      - | 3117 | `				}` |
|    103 | 3118 | `				c = SyToLower(zPtr[0]);` |
|    103 | 3119 | `				d = SyToLower(zPtr2[0]);` |
|    103 | 3120 | `				if( c != d ){` |
|    ! 0 | 3121 | `					break;` |
|      - | 3122 | `				}` |
|    103 | 3123 | `				zPtr++; zPtr2++;` |
|      1 | 3124 | `			}` |
|    ! 0 | 3125 | `		}` |
|    243 | 3126 | `		zIn++;` |
|      1 | 3127 | `	}` |
|      - | 3128 | `	/* Pattern not found */` |
|     41 | 3129 | `	return SXERR_NOTFOUND;` |
|     50 | 3130 | `}` |
|      - | 3131 | `/*` |
|      - | 3132 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3133 | ` *  Find the first occurrence of a string.` |
|      - | 3134 | ` * Parameters` |
|      - | 3135 | ` *  $haystack` |
|      - | 3136 | ` *   The input string.` |
|      - | 3137 | ` * $needle` |
|      - | 3138 | ` *   Search pattern (must be a string).` |
|      - | 3139 | ` * $before_needle` |
|      - | 3140 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3141 | ` *   of the needle (excluding the needle).` |
|      - | 3142 | ` * Return` |
|      - | 3143 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3144 | ` */` |
|     38 | 3145 | `PH7_PRIVATE int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3146 | `{` |
|     40 | 3147 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3148 | `	const char *zBlob,*zPattern;` |
|      - | 3149 | `	int nLen,nPatLen;` |
|      - | 3150 | `	sxu32 nOfft;` |
|      - | 3151 | `	sxi32 rc;` |
|     40 | 3152 | `	if( nArg < 2 ){` |
|      - | 3153 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3154 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3155 | `		return PH7_OK;` |
|      - | 3156 | `	}` |
|      - | 3157 | `	/* Extract the needle and the haystack */` |
|     40 | 3158 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     40 | 3159 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     40 | 3160 | `	nOfft = 0; /* cc warning */` |
|     40 | 3161 | `	if( nPatLen < 1 ){` |
|      - | 3162 | `		/* php 8: the empty needle matches at position 0, so the whole haystack` |
|      - | 3163 | `		 * is returned (and nothing at all when $before_needle is set). */` |
|      7 | 3164 | `		if( nArg > 2 && ph7_value_to_bool(apArg[2]) ){` |
|      3 | 3165 | `			ph7_result_string(pCtx,"",0);` |
|      2 | 3166 | `		}else{` |
|      5 | 3167 | `			ph7_result_string(pCtx,zBlob,nLen);` |
|      - | 3168 | `		}` |
|      7 | 3169 | `		return PH7_OK;` |
|      - | 3170 | `	}` |
|     34 | 3171 | `	if( nLen > 0 ){` |
|     34 | 3172 | `		int before = 0;` |
|      - | 3173 | `		/* Perform the lookup */` |
|     34 | 3174 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     34 | 3175 | `		if( rc != SXRET_OK ){` |
|      - | 3176 | `			/* Pattern not found,return FALSE */` |
|      3 | 3177 | `			ph7_result_bool(pCtx,0);` |
|      3 | 3178 | `			return PH7_OK;` |
|      - | 3179 | `		}` |
|      - | 3180 | `		/* Return the portion of the string */` |
|     32 | 3181 | `		if( nArg > 2 ){` |
|     30 | 3182 | `			before = ph7_value_to_int(apArg[2]);` |
|     14 | 3183 | `		}` |
|     32 | 3184 | `		if( before ){` |
|     30 | 3185 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|     16 | 3186 | `		}else{` |
|      3 | 3187 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3188 | `		}` |
|     17 | 3189 | `	}else{` |
|    ! 0 | 3190 | `		ph7_result_bool(pCtx,0);` |
|      - | 3191 | `	}` |
|     32 | 3192 | `	return PH7_OK;` |
|     21 | 3193 | `}` |
|      - | 3194 | `/*` |
|      - | 3195 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3196 | ` *  Case-insensitive strstr().` |
|      - | 3197 | ` * Parameters` |
|      - | 3198 | ` *  $haystack` |
|      - | 3199 | ` *   The input string.` |
|      - | 3200 | ` * $needle` |
|      - | 3201 | ` *   Search pattern (must be a string).` |
|      - | 3202 | ` * $before_needle` |
|      - | 3203 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3204 | ` *   of the needle (excluding the needle).` |
|      - | 3205 | ` * Return` |
|      - | 3206 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3207 | ` */` |
|      6 | 3208 | `PH7_PRIVATE int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3209 | `{` |
|      7 | 3210 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3211 | `	const char *zBlob,*zPattern;` |
|      - | 3212 | `	int nLen,nPatLen;` |
|      - | 3213 | `	sxu32 nOfft;` |
|      - | 3214 | `	sxi32 rc;` |
|      7 | 3215 | `	if( nArg < 2 ){` |
|      - | 3216 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3217 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3218 | `		return PH7_OK;` |
|      - | 3219 | `	}` |
|      - | 3220 | `	/* Extract the needle and the haystack */` |
|      7 | 3221 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 3222 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 3223 | `	nOfft = 0; /* cc warning */` |
|      7 | 3224 | `	if( nPatLen < 1 ){` |
|      - | 3225 | `		/* php 8: the empty needle matches at position 0, so the whole haystack` |
|      - | 3226 | `		 * is returned (and nothing at all when $before_needle is set). */` |
|      3 | 3227 | `		if( nArg > 2 && ph7_value_to_bool(apArg[2]) ){` |
|    ! 0 | 3228 | `			ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3229 | `		}else{` |
|      3 | 3230 | `			ph7_result_string(pCtx,zBlob,nLen);` |
|      - | 3231 | `		}` |
|      3 | 3232 | `		return PH7_OK;` |
|      - | 3233 | `	}` |
|      5 | 3234 | `	if( nLen > 0 ){` |
|      5 | 3235 | `		int before = 0;` |
|      - | 3236 | `		/* Perform the lookup */` |
|      5 | 3237 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3238 | `		if( rc != SXRET_OK ){` |
|      - | 3239 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3240 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3241 | `			return PH7_OK;` |
|      - | 3242 | `		}` |
|      - | 3243 | `		/* Return the portion of the string */` |
|      5 | 3244 | `		if( nArg > 2 ){` |
|      3 | 3245 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3246 | `		}` |
|      5 | 3247 | `		if( before ){` |
|      3 | 3248 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3249 | `		}else{` |
|      3 | 3250 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3251 | `		}` |
|      3 | 3252 | `	}else{` |
|    ! 0 | 3253 | `		ph7_result_bool(pCtx,0);` |
|      - | 3254 | `	}` |
|      5 | 3255 | `	return PH7_OK;` |
|      4 | 3256 | `}` |
|      - | 3257 | `/*` |
|      - | 3258 | ` * Resolve the $offset argument shared by strpos()/stripos().` |
|      - | 3259 | ` *` |
|      - | 3260 | ` * php requires -strlen($haystack) <= $offset <= strlen($haystack) and throws` |
|      - | 3261 | ` * ValueError otherwise; a negative offset counts back from the end. PHL used to` |
|      - | 3262 | ` * negate a negative offset and silently clamp an out-of-range one to zero, so` |
|      - | 3263 | ` * strpos("Hello","l",100) answered 2 where php raises — an argument error` |
|      - | 3264 | ` * turned into a wrong answer.` |
|      - | 3265 | ` *` |
|      - | 3266 | ` * On success *pnStart receives the resolved non-negative offset.` |
|      - | 3267 | ` */` |
|     10 | 3268 | `static sxi32 StrSearchOffset(` |
|      - | 3269 | `	ph7_context *pCtx,` |
|      - | 3270 | `	ph7_value *pArg,` |
|      - | 3271 | `	int nLen,` |
|      - | 3272 | `	const char *zFunc,` |
|      - | 3273 | `	int *pnStart` |
|      - | 3274 | `	)` |
|      1 | 3275 | `{` |
|     11 | 3276 | `	ph7_int64 iOfft = ph7_value_to_int64(pArg);` |
|      - | 3277 | `	/* Compare without negating iOfft: -INT64_MIN would overflow. */` |
|     11 | 3278 | `	if( iOfft < 0 ? (iOfft < -(ph7_int64)nLen) : (iOfft > (ph7_int64)nLen) ){` |
|      8 | 3279 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      4 | 3280 | `			"%s(): Argument #3 ($offset) must be contained in argument #1 ($haystack)",zFunc);` |
|      - | 3281 | `	}` |
|     11 | 3282 | `	*pnStart = (int)(iOfft < 0 ? (ph7_int64)nLen + iOfft : iOfft);` |
|     11 | 3283 | `	return PH7_OK;` |
|     10 | 3284 | `}` |
|      - | 3285 | `/*` |
|      - | 3286 | ` * Resolve the window of match START positions for strrpos()/strripos().` |
|      - | 3287 | ` *` |
|      - | 3288 | ` * php's rule is asymmetric in the sign of $offset: a non-negative offset is a` |
|      - | 3289 | ` * LOWER bound on where the match may start, while a negative one is an UPPER` |
|      - | 3290 | ` * bound counted back from the end of the haystack (zend_memnrstr). The range` |
|      - | 3291 | ` * check is the same as StrSearchOffset()'s.` |
|      - | 3292 | ` *` |
|      - | 3293 | ` * On success the closed interval [*pnMin,*pnMax] holds every position at which` |
|      - | 3294 | ` * a match is allowed to begin; it is empty (max < min) when the needle cannot` |
|      - | 3295 | ` * fit, which the caller reports as FALSE.` |
|      - | 3296 | ` */` |
|    124 | 3297 | `static sxi32 StrRSearchWindow(` |
|      - | 3298 | `	ph7_context *pCtx,` |
|      - | 3299 | `	ph7_value *pArg, /* The $offset argument, or NULL when it was omitted */` |
|      - | 3300 | `	int nLen,` |
|      - | 3301 | `	int nPatLen,` |
|      - | 3302 | `	const char *zFunc,` |
|      - | 3303 | `	int *pnMin,` |
|      - | 3304 | `	int *pnMax` |
|      - | 3305 | `	)` |
|      1 | 3306 | `{` |
|    125 | 3307 | `	int nMin = 0;` |
|    125 | 3308 | `	int nMax = nLen - nPatLen;` |
|    125 | 3309 | `	if( pArg ){` |
|     47 | 3310 | `		ph7_int64 iOfft = ph7_value_to_int64(pArg);` |
|     47 | 3311 | `		if( iOfft < 0 ? (iOfft < -(ph7_int64)nLen) : (iOfft > (ph7_int64)nLen) ){` |
|     33 | 3312 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|     14 | 3313 | `				"%s(): Argument #3 ($offset) must be contained in argument #1 ($haystack)",zFunc);` |
|      - | 3314 | `		}` |
|     29 | 3315 | `		if( iOfft < 0 ){` |
|     15 | 3316 | `			int nLimit = nLen + (int)iOfft;` |
|     15 | 3317 | `			if( nMax > nLimit ){` |
|     15 | 3318 | `				nMax = nLimit;` |
|      7 | 3319 | `			}` |
|      8 | 3320 | `		}else{` |
|     15 | 3321 | `			nMin = (int)iOfft;` |
|      - | 3322 | `		}` |
|     14 | 3323 | `	}` |
|    107 | 3324 | `	*pnMin = nMin;` |
|    107 | 3325 | `	*pnMax = nMax;` |
|    107 | 3326 | `	return PH7_OK;` |
|     58 | 3327 | `}` |
|      - | 3328 | `/*` |
|      - | 3329 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3330 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 3331 | ` * Parameters` |
|      - | 3332 | ` *  $haystack` |
|      - | 3333 | ` *   The input string.` |
|      - | 3334 | ` * $needle` |
|      - | 3335 | ` *   Search pattern (must be a string).` |
|      - | 3336 | ` * $offset` |
|      - | 3337 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3338 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3339 | ` *   of haystack.` |
|      - | 3340 | ` * Return` |
|      - | 3341 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3342 | ` */` |
|   2300 | 3343 | `PH7_PRIVATE int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3344 | `{` |
|   2305 | 3345 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strpos",1,"$haystack"); }` |
|   2305 | 3346 | `	if( nArg > 1 ){ StrNullArgNotice(pCtx,apArg[1],"strpos",2,"$needle"); }` |
|   2305 | 3347 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3348 | `	const char *zBlob,*zPattern;` |
|      - | 3349 | `	int nLen,nPatLen,nStart;` |
|      - | 3350 | `	sxu32 nOfft;` |
|      - | 3351 | `	sxi32 rc;` |
|   2305 | 3352 | `	if( nArg < 2 ){` |
|      - | 3353 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3354 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3355 | `		return PH7_OK;` |
|      - | 3356 | `	}` |
|      - | 3357 | `	/* Extract the needle and the haystack */` |
|   2305 | 3358 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|   2305 | 3359 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|   2305 | 3360 | `	nOfft = 0; /* cc warning */` |
|   2305 | 3361 | `	nStart = 0;` |
|      - | 3362 | `	/* Peek the starting offset if available */` |
|   2305 | 3363 | `	if( nArg > 2 ){` |
|      7 | 3364 | `		rc = StrSearchOffset(pCtx,apArg[2],nLen,"strpos",&nStart);` |
|      7 | 3365 | `		if( rc != PH7_OK ){` |
|    ! 0 | 3366 | `			return rc;` |
|      - | 3367 | `		}` |
|      3 | 3368 | `	}` |
|   2305 | 3369 | `	if( nPatLen < 1 ){` |
|      - | 3370 | `		/* php 8 treats the empty needle as matching at the search offset. */` |
|     11 | 3371 | `		ph7_result_int64(pCtx,(ph7_int64)nStart);` |
|     11 | 3372 | `		return PH7_OK;` |
|      - | 3373 | `	}` |
|   2295 | 3374 | `	zBlob += nStart;` |
|   2295 | 3375 | `	nLen -= nStart;` |
|   2295 | 3376 | `	if( nLen > 0 ){` |
|      - | 3377 | `		/* Perform the lookup */` |
|   2285 | 3378 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|   2285 | 3379 | `		if( rc != SXRET_OK ){` |
|      - | 3380 | `			/* Pattern not found,return FALSE */` |
|   1773 | 3381 | `			ph7_result_bool(pCtx,0);` |
|   1773 | 3382 | `			return PH7_OK;` |
|      - | 3383 | `		}` |
|      - | 3384 | `		/* Return the pattern position */` |
|    517 | 3385 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|    261 | 3386 | `	}else{` |
|     11 | 3387 | `		ph7_result_bool(pCtx,0);` |
|      - | 3388 | `	}` |
|    527 | 3389 | `	return PH7_OK;` |
|   1155 | 3390 | `}` |
|      - | 3391 | `/*` |
|      - | 3392 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|      - | 3393 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|      - | 3394 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|      - | 3395 | ` * TypeError for arrays, resources, and objects without __toString.` |
|      - | 3396 | ` *` |
|      - | 3397 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|      - | 3398 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|      - | 3399 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|      - | 3400 | ` *` |
|      - | 3401 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|      - | 3402 | ` * is valid until pTmp is released or pArg is mutated.` |
|      - | 3403 | ` */` |
|    814 | 3404 | `static sxi32 StrPredicateResolveArg(` |
|      - | 3405 | `	ph7_context *pCtx,` |
|      - | 3406 | `	ph7_value *pArg,` |
|      - | 3407 | `	const char *zFunc,` |
|      - | 3408 | `	int iArgNum,` |
|      - | 3409 | `	const char *zParamName,` |
|      - | 3410 | `	const char *zTypeStr, /* Declared type in the TypeError, e.g. "string" / "?string" */` |
|      - | 3411 | `	const char *zNullMsg,` |
|      - | 3412 | `	ph7_value *pTmp,` |
|      - | 3413 | `	const char **pzOut,` |
|      - | 3414 | `	int *pnOut` |
|      4 | 3415 | `){` |
|    407 | 3416 | `	SXUNUSED(zNullMsg); /* php's deprecation text — PHL rejects null instead of coercing */` |
|    818 | 3417 | `	if( ph7_value_is_null(pArg) ){` |
|      - | 3418 | `		/* php only DEPRECATES null here; PHL rejects it with the TypeError php will` |
|      - | 3419 | `		 * eventually raise. */` |
|    ! 0 | 3420 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 3421 | `			"%s(): Argument #%d (%s) must be of type %s, null given",` |
|    ! 0 | 3422 | `			zFunc,iArgNum,zParamName,zTypeStr);` |
|      - | 3423 | `	}` |
|   1249 | 3424 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    814 | 3425 | `	    ( ph7_value_is_object(pArg) &&` |
|     72 | 3426 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     48 | 3427 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     24 | 3428 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 3429 | `	    )` |
|      - | 3430 | `	){` |
|    ! 0 | 3431 | `		const char *zType = ph7_type_name(pArg);` |
|    ! 0 | 3432 | `		if( ph7_value_is_object(pArg) ){` |
|    ! 0 | 3433 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|    ! 0 | 3434 | `			if( pInst && pInst->pClass ){` |
|    ! 0 | 3435 | `				zType = SyStringData(&pInst->pClass->sName);` |
|    ! 0 | 3436 | `			}` |
|    ! 0 | 3437 | `		}` |
|    ! 0 | 3438 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3439 | `			"TypeError",` |
|      - | 3440 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|    ! 0 | 3441 | `			zFunc, iArgNum, zParamName, zTypeStr, zType` |
|      - | 3442 | `			);` |
|      - | 3443 | `	}` |
|    818 | 3444 | `	if( ph7_value_is_object(pArg) ){` |
|     49 | 3445 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     49 | 3446 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 3447 | `			"__toString",sizeof("__toString")-1);` |
|     49 | 3448 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|     49 | 3449 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|     49 | 3450 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|     49 | 3451 | `		return PH7_OK;` |
|      - | 3452 | `	}` |
|    770 | 3453 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|    770 | 3454 | `	return PH7_OK;` |
|    411 | 3455 | `}` |
|      - | 3456 | `/*` |
|      - | 3457 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 3458 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 3459 | ` * Return` |
|      - | 3460 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 3461 | ` */` |
|    136 | 3462 | `PH7_PRIVATE int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3463 | `{` |
|      - | 3464 | `	const char *zHaystack,*zNeedle;` |
|      - | 3465 | `	int nHayLen,nNeedleLen;` |
|      - | 3466 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3467 | `	sxi32 rc;` |
|    138 | 3468 | `	if( nArg != 2 ){` |
|    ! 0 | 3469 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3470 | `			"ArgumentCountError",` |
|      - | 3471 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|    ! 0 | 3472 | `			nArg` |
|      - | 3473 | `			);` |
|      - | 3474 | `	}` |
|    138 | 3475 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|    138 | 3476 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|    138 | 3477 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack","string",` |
|      - | 3478 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|      - | 3479 | `		"of type string is deprecated",` |
|      - | 3480 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|    138 | 3481 | `	if( rc != PH7_OK ) goto out;` |
|    138 | 3482 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle","string",` |
|      - | 3483 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|      - | 3484 | `		"of type string is deprecated",` |
|      - | 3485 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|    138 | 3486 | `	if( rc != PH7_OK ) goto out;` |
|    138 | 3487 | `	if( nNeedleLen < 1 ){` |
|     11 | 3488 | `		ph7_result_bool(pCtx,1);` |
|    133 | 3489 | `	}else if( nHayLen < nNeedleLen ){` |
|     13 | 3490 | `		ph7_result_bool(pCtx,0);` |
|      7 | 3491 | `	}else{` |
|    173 | 3492 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|     57 | 3493 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|    116 | 3494 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|      - | 3495 | `	}` |
|    138 | 3496 | `	rc = PH7_OK;` |
|     68 | 3497 | `out:` |
|    138 | 3498 | `	PH7_MemObjRelease(&sHayTmp);` |
|    138 | 3499 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|    138 | 3500 | `	return rc;` |
|     70 | 3501 | `}` |
|      - | 3502 | `/*` |
|      - | 3503 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 3504 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 3505 | ` * Return` |
|      - | 3506 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 3507 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3508 | ` */` |
|     96 | 3509 | `PH7_PRIVATE int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 3510 | `{` |
|      - | 3511 | `	const char *zHaystack,*zNeedle;` |
|      - | 3512 | `	int nHayLen,nNeedleLen;` |
|      - | 3513 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3514 | `	sxi32 rc;` |
|    100 | 3515 | `	if( nArg != 2 ){` |
|    ! 0 | 3516 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3517 | `			"ArgumentCountError",` |
|      - | 3518 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|    ! 0 | 3519 | `			nArg` |
|      - | 3520 | `			);` |
|      - | 3521 | `	}` |
|    100 | 3522 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|    100 | 3523 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|    100 | 3524 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack","string",` |
|      - | 3525 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3526 | `		"of type string is deprecated",` |
|      - | 3527 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|    100 | 3528 | `	if( rc != PH7_OK ) goto out;` |
|    100 | 3529 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle","string",` |
|      - | 3530 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3531 | `		"of type string is deprecated",` |
|      - | 3532 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|    100 | 3533 | `	if( rc != PH7_OK ) goto out;` |
|    100 | 3534 | `	if( nNeedleLen < 1 ){` |
|     11 | 3535 | `		ph7_result_bool(pCtx,1);` |
|     95 | 3536 | `	}else if( nHayLen < nNeedleLen ){` |
|      7 | 3537 | `		ph7_result_bool(pCtx,0);` |
|      4 | 3538 | `	}else{` |
|    124 | 3539 | `		ph7_result_bool(pCtx,` |
|     80 | 3540 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3541 | `	}` |
|    100 | 3542 | `	rc = PH7_OK;` |
|     48 | 3543 | `out:` |
|    100 | 3544 | `	PH7_MemObjRelease(&sHayTmp);` |
|    100 | 3545 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|    100 | 3546 | `	return rc;` |
|     52 | 3547 | `}` |
|      - | 3548 | `/*` |
|      - | 3549 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 3550 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 3551 | ` * Return` |
|      - | 3552 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 3553 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3554 | ` */` |
|     54 | 3555 | `PH7_PRIVATE int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3556 | `{` |
|      - | 3557 | `	const char *zHaystack,*zNeedle;` |
|      - | 3558 | `	int nHayLen,nNeedleLen;` |
|      - | 3559 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3560 | `	sxi32 rc;` |
|     55 | 3561 | `	if( nArg != 2 ){` |
|    ! 0 | 3562 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3563 | `			"ArgumentCountError",` |
|      - | 3564 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|    ! 0 | 3565 | `			nArg` |
|      - | 3566 | `			);` |
|      - | 3567 | `	}` |
|     55 | 3568 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     55 | 3569 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     55 | 3570 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack","string",` |
|      - | 3571 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3572 | `		"of type string is deprecated",` |
|      - | 3573 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     55 | 3574 | `	if( rc != PH7_OK ) goto out;` |
|     55 | 3575 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle","string",` |
|      - | 3576 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3577 | `		"of type string is deprecated",` |
|      - | 3578 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     55 | 3579 | `	if( rc != PH7_OK ) goto out;` |
|     55 | 3580 | `	if( nNeedleLen < 1 ){` |
|     11 | 3581 | `		ph7_result_bool(pCtx,1);` |
|     50 | 3582 | `	}else if( nHayLen < nNeedleLen ){` |
|      7 | 3583 | `		ph7_result_bool(pCtx,0);` |
|      4 | 3584 | `	}else{` |
|     58 | 3585 | `		ph7_result_bool(pCtx,` |
|     38 | 3586 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3587 | `	}` |
|     55 | 3588 | `	rc = PH7_OK;` |
|     27 | 3589 | `out:` |
|     55 | 3590 | `	PH7_MemObjRelease(&sHayTmp);` |
|     55 | 3591 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     55 | 3592 | `	return rc;` |
|     28 | 3593 | `}` |
|      - | 3594 | `/*` |
|      - | 3595 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3596 | ` *  Case-insensitive strpos.` |
|      - | 3597 | ` * Parameters` |
|      - | 3598 | ` *  $haystack` |
|      - | 3599 | ` *   The input string.` |
|      - | 3600 | ` * $needle` |
|      - | 3601 | ` *   Search pattern (must be a string).` |
|      - | 3602 | ` * $offset` |
|      - | 3603 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3604 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3605 | ` *   of haystack.` |
|      - | 3606 | ` * Return` |
|      - | 3607 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3608 | ` */` |
|     20 | 3609 | `PH7_PRIVATE int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3610 | `{` |
|     21 | 3611 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3612 | `	const char *zBlob,*zPattern;` |
|      - | 3613 | `	int nLen,nPatLen,nStart;` |
|      - | 3614 | `	sxu32 nOfft;` |
|      - | 3615 | `	sxi32 rc;` |
|     21 | 3616 | `	if( nArg < 2 ){` |
|      - | 3617 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3618 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3619 | `		return PH7_OK;` |
|      - | 3620 | `	}` |
|      - | 3621 | `	/* Extract the needle and the haystack */` |
|     21 | 3622 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     21 | 3623 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     21 | 3624 | `	nOfft = 0; /* cc warning */` |
|     21 | 3625 | `	nStart = 0;` |
|      - | 3626 | `	/* Peek the starting offset if available */` |
|     21 | 3627 | `	if( nArg > 2 ){` |
|      5 | 3628 | `		rc = StrSearchOffset(pCtx,apArg[2],nLen,"stripos",&nStart);` |
|      5 | 3629 | `		if( rc != PH7_OK ){` |
|    ! 0 | 3630 | `			return rc;` |
|      - | 3631 | `		}` |
|      2 | 3632 | `	}` |
|     21 | 3633 | `	if( nPatLen < 1 ){` |
|      - | 3634 | `		/* php 8 treats the empty needle as matching at the search offset. */` |
|      3 | 3635 | `		ph7_result_int64(pCtx,(ph7_int64)nStart);` |
|      3 | 3636 | `		return PH7_OK;` |
|      - | 3637 | `	}` |
|     19 | 3638 | `	zBlob += nStart;` |
|     19 | 3639 | `	nLen -= nStart;` |
|     19 | 3640 | `	if( nLen > 0 ){` |
|      - | 3641 | `		/* Perform the lookup */` |
|     19 | 3642 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     19 | 3643 | `		if( rc != SXRET_OK ){` |
|      - | 3644 | `			/* Pattern not found,return FALSE */` |
|      5 | 3645 | `			ph7_result_bool(pCtx,0);` |
|      5 | 3646 | `			return PH7_OK;` |
|      - | 3647 | `		}` |
|      - | 3648 | `		/* Return the pattern position */` |
|     15 | 3649 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 3650 | `	}else{` |
|    ! 0 | 3651 | `		ph7_result_bool(pCtx,0);` |
|      - | 3652 | `	}` |
|     15 | 3653 | `	return PH7_OK;` |
|     11 | 3654 | `}` |
|      - | 3655 | `/*` |
|      - | 3656 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3657 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 3658 | ` * Parameters` |
|      - | 3659 | ` *  $haystack` |
|      - | 3660 | ` *   The input string.` |
|      - | 3661 | ` * $needle` |
|      - | 3662 | ` *   Search pattern (must be a string).` |
|      - | 3663 | ` * $offset` |
|      - | 3664 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3665 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3666 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3667 | ` * Return` |
|      - | 3668 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3669 | ` */` |
|     80 | 3670 | `PH7_PRIVATE int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3671 | `{` |
|      - | 3672 | `	const char *zBlob,*zPattern;` |
|     81 | 3673 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3674 | `	int nLen,nPatLen,i;` |
|     81 | 3675 | `	int nMin = 0,nMax = 0;` |
|      - | 3676 | `	sxu32 nOfft;` |
|      - | 3677 | `	sxi32 rc;` |
|     81 | 3678 | `	if( nArg < 2 ){` |
|      - | 3679 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3680 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3681 | `		return PH7_OK;` |
|      - | 3682 | `	}` |
|      - | 3683 | `	/* Extract the needle and the haystack */` |
|     81 | 3684 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     81 | 3685 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     81 | 3686 | `	nOfft = 0; /* cc warning */` |
|      - | 3687 | `	/* Resolve the range of positions the match may start at */` |
|     81 | 3688 | `	rc = StrRSearchWindow(pCtx,nArg > 2 ? apArg[2] : 0,nLen,nPatLen,"strrpos",&nMin,&nMax);` |
|     81 | 3689 | `	if( rc != PH7_OK ){` |
|      5 | 3690 | `		return rc;` |
|      - | 3691 | `	}` |
|     77 | 3692 | `	if( nPatLen < 1 ){` |
|      - | 3693 | `		/* php 8: the empty needle matches everywhere, so the LAST match is the` |
|      - | 3694 | `		 * highest position the window allows. */` |
|     11 | 3695 | `		ph7_result_int64(pCtx,(ph7_int64)nMax);` |
|     11 | 3696 | `		return PH7_OK;` |
|      - | 3697 | `	}` |
|      - | 3698 | `	/* Walk backwards, comparing at each candidate position. Searching a window` |
|      - | 3699 | `	 * exactly as long as the needle makes the match test an equality test while` |
|      - | 3700 | `	 * still going through xPatternMatch, which carries the case folding. */` |
|    295 | 3701 | `	for( i = nMax ; i >= nMin ; --i ){` |
|    283 | 3702 | `		rc = xPatternMatch((const void *)&zBlob[i],(sxu32)nPatLen,(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    283 | 3703 | `		if( rc == SXRET_OK ){` |
|      - | 3704 | `			/* Pattern found,return it's position */` |
|     55 | 3705 | `			ph7_result_int64(pCtx,(ph7_int64)i);` |
|     55 | 3706 | `			return PH7_OK;` |
|      - | 3707 | `		}` |
|    115 | 3708 | `	}` |
|      - | 3709 | `	/* Pattern not found,return FALSE */` |
|     13 | 3710 | `	ph7_result_bool(pCtx,0);` |
|     13 | 3711 | `	return PH7_OK;` |
|     41 | 3712 | `}` |
|      - | 3713 | `/*` |
|      - | 3714 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3715 | ` *  Case-insensitive strrpos.` |
|      - | 3716 | ` * Parameters` |
|      - | 3717 | ` *  $haystack` |
|      - | 3718 | ` *   The input string.` |
|      - | 3719 | ` * $needle` |
|      - | 3720 | ` *   Search pattern (must be a string).` |
|      - | 3721 | ` * $offset` |
|      - | 3722 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3723 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3724 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3725 | ` * Return` |
|      - | 3726 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3727 | ` */` |
|     34 | 3728 | `PH7_PRIVATE int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3729 | `{` |
|      - | 3730 | `	const char *zBlob,*zPattern;` |
|     35 | 3731 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3732 | `	int nLen,nPatLen,i;` |
|     35 | 3733 | `	int nMin = 0,nMax = 0;` |
|      - | 3734 | `	sxu32 nOfft;` |
|      - | 3735 | `	sxi32 rc;` |
|     35 | 3736 | `	if( nArg < 2 ){` |
|      - | 3737 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3738 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3739 | `		return PH7_OK;` |
|      - | 3740 | `	}` |
|      - | 3741 | `	/* Extract the needle and the haystack */` |
|     35 | 3742 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     35 | 3743 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     35 | 3744 | `	nOfft = 0; /* cc warning */` |
|      - | 3745 | `	/* Resolve the range of positions the match may start at */` |
|     35 | 3746 | `	rc = StrRSearchWindow(pCtx,nArg > 2 ? apArg[2] : 0,nLen,nPatLen,"strripos",&nMin,&nMax);` |
|     35 | 3747 | `	if( rc != PH7_OK ){` |
|      5 | 3748 | `		return rc;` |
|      - | 3749 | `	}` |
|     31 | 3750 | `	if( nPatLen < 1 ){` |
|      - | 3751 | `		/* php 8: the empty needle matches everywhere, so the LAST match is the` |
|      - | 3752 | `		 * highest position the window allows. */` |
|     11 | 3753 | `		ph7_result_int64(pCtx,(ph7_int64)nMax);` |
|     11 | 3754 | `		return PH7_OK;` |
|      - | 3755 | `	}` |
|      - | 3756 | `	/* Walk backwards, comparing at each candidate position (see strrpos). */` |
|     49 | 3757 | `	for( i = nMax ; i >= nMin ; --i ){` |
|     45 | 3758 | `		rc = xPatternMatch((const void *)&zBlob[i],(sxu32)nPatLen,(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     45 | 3759 | `		if( rc == SXRET_OK ){` |
|      - | 3760 | `			/* Pattern found,return it's position */` |
|     17 | 3761 | `			ph7_result_int64(pCtx,(ph7_int64)i);` |
|     17 | 3762 | `			return PH7_OK;` |
|      - | 3763 | `		}` |
|     15 | 3764 | `	}` |
|      - | 3765 | `	/* Pattern not found,return FALSE */` |
|      5 | 3766 | `	ph7_result_bool(pCtx,0);` |
|      5 | 3767 | `	return PH7_OK;` |
|     18 | 3768 | `}` |
|      - | 3769 | `/*` |
|      - | 3770 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3771 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3772 | ` * Parameters` |
|      - | 3773 | ` *  $haystack` |
|      - | 3774 | ` *   The input string.` |
|      - | 3775 | ` * $needle` |
|      - | 3776 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3777 | ` *  This behavior is different from that of strstr().` |
|      - | 3778 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3779 | ` *  as the ordinal value of a character.` |
|      - | 3780 | ` * Return` |
|      - | 3781 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3782 | ` */` |
|     24 | 3783 | `PH7_PRIVATE int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3784 | `{` |
|      - | 3785 | `	const char *zBlob;` |
|      - | 3786 | `	int nLen,c;` |
|     25 | 3787 | `	if( nArg < 2 ){` |
|      - | 3788 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3789 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3790 | `		return PH7_OK;` |
|      - | 3791 | `	}` |
|      - | 3792 | `	/* Extract the haystack */` |
|     25 | 3793 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 3794 | `	c = 0; /* cc warning */` |
|     25 | 3795 | `	if( nLen > 0 ){` |
|      - | 3796 | `		const char *zPattern;` |
|      - | 3797 | `		int nPatLen;` |
|      - | 3798 | `		sxu32 nOfft;` |
|      - | 3799 | `		sxi32 rc;` |
|      - | 3800 | `		/* php 8 casts the needle to string and uses only its first character.` |
|      - | 3801 | `		 * The old "if not a string, take it as an ordinal" reading was php 7` |
|      - | 3802 | `		 * behaviour, removed in php 8: strrchr("hello world",111) now looks for` |
|      - | 3803 | `		 * "1", not "o". An empty needle matches nothing. */` |
|     23 | 3804 | `		zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     23 | 3805 | `		if( nPatLen < 1 ){` |
|      3 | 3806 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3807 | `			return PH7_OK;` |
|      - | 3808 | `		}` |
|     21 | 3809 | `		c = zPattern[0];` |
|      - | 3810 | `		/* Perform the lookup */` |
|     21 | 3811 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3812 | `		if( rc != SXRET_OK ){` |
|      - | 3813 | `			/* No such entry,return FALSE */` |
|      9 | 3814 | `			ph7_result_bool(pCtx,0);` |
|      9 | 3815 | `			return PH7_OK;` |
|      - | 3816 | `		}` |
|      - | 3817 | `		/* Return the string portion */` |
|     13 | 3818 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      7 | 3819 | `	}else{` |
|      3 | 3820 | `		ph7_result_bool(pCtx,0);` |
|      - | 3821 | `	}` |
|     15 | 3822 | `	return PH7_OK;` |
|     13 | 3823 | `}` |
|      - | 3824 | `/*` |
|      - | 3825 | ` * string strrev(string $string)` |
|      - | 3826 | ` *  Reverse a string.` |
|      - | 3827 | ` * Parameters` |
|      - | 3828 | ` *  $string` |
|      - | 3829 | ` *   String to be reversed.` |
|      - | 3830 | ` * Return` |
|      - | 3831 | ` *  The reversed string.` |
|      - | 3832 | ` */` |
|     10 | 3833 | `PH7_PRIVATE int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3834 | `{` |
|      - | 3835 | `	const char *zIn,*zEnd;` |
|      - | 3836 | `	int nLen,c;` |
|     12 | 3837 | `	if( nArg < 1 ){` |
|      - | 3838 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3839 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3840 | `		return PH7_OK;` |
|      - | 3841 | `	}` |
|      - | 3842 | `	/* Extract the target string */` |
|     12 | 3843 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     12 | 3844 | `	if( nLen < 1 ){` |
|      - | 3845 | `		/* php answers the empty STRING here, not null */` |
|      3 | 3846 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3847 | `		return PH7_OK;` |
|      - | 3848 | `	}` |
|      - | 3849 | `	/* Perform the requested operation */` |
|     10 | 3850 | `	zEnd = &zIn[nLen - 1];` |
|     15 | 3851 | `	for(;;){` |
|     32 | 3852 | `		if( zEnd < zIn ){` |
|      - | 3853 | `			/* No more input to process */` |
|     10 | 3854 | `			break;` |
|      - | 3855 | `		}` |
|      - | 3856 | `		/* Append current character */` |
|     24 | 3857 | `		c = zEnd[0];` |
|     24 | 3858 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     24 | 3859 | `		zEnd--;` |
|      2 | 3860 | `	}` |
|     10 | 3861 | `	return PH7_OK;` |
|      7 | 3862 | `}` |
|      - | 3863 | `/*` |
|      - | 3864 | ` * string ucwords(string $string [, string $separators = " \t\r\n\f\v"])` |
|      - | 3865 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3866 | ` *  A word begins at the start of the string and after any character present in` |
|      - | 3867 | ` *  $separators. The default separators are the whitespace characters (space,` |
|      - | 3868 | ` *  horizontal tab, carriage return, newline, form-feed and vertical tab); an` |
|      - | 3869 | ` *  explicit $separators argument REPLACES them (an empty string leaves only the` |
|      - | 3870 | ` *  very first character upper-cased). Like PHP, this is byte-based: only ASCII` |
|      - | 3871 | ` *  bytes are upper-cased and a byte is a separator only if it appears in the set.` |
|      - | 3872 | ` * Parameters` |
|      - | 3873 | ` *  $string` |
|      - | 3874 | ` *   The input string.` |
|      - | 3875 | ` *  $separators` |
|      - | 3876 | ` *   The optional word-boundary characters.` |
|      - | 3877 | ` * Return` |
|      - | 3878 | ` *  The modified string.` |
|      - | 3879 | ` */` |
|     26 | 3880 | `PH7_PRIVATE int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3881 | `{` |
|      - | 3882 | `	const char *zIn;` |
|      - | 3883 | `	int nLen,i,iStart;` |
|      - | 3884 | `	char aDelim[256];` |
|     28 | 3885 | `	if( nArg < 1 ){` |
|      - | 3886 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3887 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3888 | `		return PH7_OK;` |
|      - | 3889 | `	}` |
|      - | 3890 | `	/* Build the separator membership table: an explicit $separators argument` |
|      - | 3891 | `	 * replaces the default whitespace set (an empty string clears it). */` |
|     28 | 3892 | `	SyZero(aDelim,(sxu32)sizeof(aDelim));` |
|     28 | 3893 | `	if( nArg > 1 ){` |
|      - | 3894 | `		int nDelim;` |
|      9 | 3895 | `		const char *zDelim = ph7_value_to_string(apArg[1],&nDelim);` |
|     17 | 3896 | `		for( i = 0 ; i < nDelim ; i++ ){` |
|      9 | 3897 | `			aDelim[(unsigned char)zDelim[i]] = 1;` |
|      5 | 3898 | `		}` |
|      5 | 3899 | `	}else{` |
|     20 | 3900 | `		aDelim[(unsigned char)' ']  = 1;` |
|     20 | 3901 | `		aDelim[(unsigned char)'\t'] = 1;` |
|     20 | 3902 | `		aDelim[(unsigned char)'\r'] = 1;` |
|     20 | 3903 | `		aDelim[(unsigned char)'\n'] = 1;` |
|     20 | 3904 | `		aDelim[(unsigned char)'\f'] = 1;` |
|     20 | 3905 | `		aDelim[(unsigned char)'\v'] = 1;` |
|      - | 3906 | `	}` |
|      - | 3907 | `	/* Extract the target string */` |
|     28 | 3908 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     28 | 3909 | `	if( nLen < 1 ){` |
|      - | 3910 | `		/* Empty string – match PHP semantics */` |
|      6 | 3911 | `		ph7_result_string(pCtx,"",0);` |
|      6 | 3912 | `		return PH7_OK;` |
|      - | 3913 | `	}` |
|      - | 3914 | `	/* Upper-case the first byte of each word (the leading byte, or any byte that` |
|      - | 3915 | `	 * follows a separator), appending the untouched runs in between verbatim. */` |
|     23 | 3916 | `	iStart = 0;` |
|    325 | 3917 | `	for( i = 0 ; i < nLen ; i++ ){` |
|    303 | 3918 | `		int c = (unsigned char)zIn[i];` |
|    303 | 3919 | `		if( (i == 0 \|\| aDelim[(unsigned char)zIn[i-1]]) && c < 0x80 && SyisLower(c) ){` |
|     55 | 3920 | `			char up = (char)SyToUpper(c);` |
|     55 | 3921 | `			if( i > iStart ){` |
|     37 | 3922 | `				ph7_result_string(pCtx,&zIn[iStart],i - iStart);` |
|     18 | 3923 | `			}` |
|     55 | 3924 | `			ph7_result_string(pCtx,&up,1);` |
|     55 | 3925 | `			iStart = i + 1;` |
|     27 | 3926 | `		}` |
|    152 | 3927 | `	}` |
|     23 | 3928 | `	if( nLen > iStart ){` |
|     23 | 3929 | `		ph7_result_string(pCtx,&zIn[iStart],nLen - iStart);` |
|     11 | 3930 | `	}` |
|     23 | 3931 | `	return PH7_OK;` |
|     15 | 3932 | `}` |
|      - | 3933 | `/*` |
|      - | 3934 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3935 | ` *  Returns input repeated multiplier times.` |
|      - | 3936 | ` * Parameters` |
|      - | 3937 | ` *  $string` |
|      - | 3938 | ` *   String to be repeated.` |
|      - | 3939 | ` * $multiplier` |
|      - | 3940 | ` *  Number of time the input string should be repeated.` |
|      - | 3941 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3942 | ` *  to 0, the function will return an empty string.` |
|      - | 3943 | ` * Return` |
|      - | 3944 | ` *  The repeated string.` |
|      - | 3945 | ` */` |
|  20522 | 3946 | `PH7_PRIVATE int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 3947 | `{` |
|      - | 3948 | `	const char *zIn;` |
|      - | 3949 | `	int nLen;` |
|      - | 3950 | `	ph7_int64 nMul;` |
|      - | 3951 | `	int rc;` |
|  20526 | 3952 | `	if( nArg < 2 ){` |
|      - | 3953 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3954 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3955 | `		return PH7_OK;` |
|      - | 3956 | `	}` |
|      - | 3957 | `	/* Extract the target string */` |
|  20526 | 3958 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3959 | `	/* Resolve $times through the shared ZPP helper so a lossy float / float-string` |
|      - | 3960 | `	 * carries php's precision deprecation and NAN/INF/non-numeric fail with php's` |
|      - | 3961 | `	 * TypeError — a bare ph7_value_to_int64() coerced them silently. */` |
|      - | 3962 | `	{` |
|  20526 | 3963 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_repeat",2,"$times","int",&nMul);` |
|  20526 | 3964 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 3965 | `			return rcArg;` |
|      - | 3966 | `		}` |
|      - | 3967 | `	}` |
|  20526 | 3968 | `	if( nMul < 0 ){` |
|      3 | 3969 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 3970 | `			"str_repeat(): Argument #2 ($times) must be greater than or equal to 0");` |
|      - | 3971 | `	}` |
|  20524 | 3972 | `	if( nLen < 1 \|\| nMul < 1 ){` |
|      - | 3973 | `		/* Empty input or a zero multiplier yields the empty string (PHP). */` |
|      5 | 3974 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 3975 | `		return PH7_OK;` |
|      - | 3976 | `	}` |
|      - | 3977 | `	/* Perform the requested operation */` |
| 264086 | 3978 | `	for(;;){` |
| 528176 | 3979 | `		if( !nMul ){` |
|  20520 | 3980 | `			break;` |
|      - | 3981 | `		}` |
|      - | 3982 | `		/* Append the copy */` |
| 507660 | 3983 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 507660 | 3984 | `		if( rc != PH7_OK ){` |
|      - | 3985 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 3986 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 3987 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 3988 | `		}` |
| 507660 | 3989 | `		nMul--;` |
|      4 | 3990 | `	}` |
|  20520 | 3991 | `	return PH7_OK;` |
|  10265 | 3992 | `}` |
|      - | 3993 | `/*` |
|      - | 3994 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3995 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3996 | ` * Parameters` |
|      - | 3997 | ` *  $string` |
|      - | 3998 | ` *   The input string.` |
|      - | 3999 | ` * $is_xhtml` |
|      - | 4000 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 4001 | ` * Return` |
|      - | 4002 | ` *  The processed string.` |
|      - | 4003 | ` */` |
|      8 | 4004 | `PH7_PRIVATE int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4005 | `{` |
|      - | 4006 | `	const char *zIn,*zCur,*zEnd;` |
|     10 | 4007 | `	int is_xhtml = 1; /* Default to XHTML-style '<br />' like PHP */` |
|      - | 4008 | `	int nLen;` |
|     10 | 4009 | `	if( nArg < 1 ){` |
|      - | 4010 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4011 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4012 | `		return PH7_OK;` |
|      - | 4013 | `	}` |
|      - | 4014 | `	/* Extract the target string */` |
|     10 | 4015 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     10 | 4016 | `	if( nLen < 1 ){` |
|      - | 4017 | `		/* php answers the empty STRING here, not null */` |
|      3 | 4018 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4019 | `		return PH7_OK;` |
|      - | 4020 | `	}` |
|      8 | 4021 | `	if( nArg > 1 ){` |
|      3 | 4022 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 4023 | `	}` |
|      8 | 4024 | `	zEnd = &zIn[nLen];` |
|      - | 4025 | `	/* Perform the requested operation */` |
|      6 | 4026 | `	for(;;){` |
|     14 | 4027 | `		zCur = zIn;` |
|      - | 4028 | `		/* Delimit the string */` |
|     32 | 4029 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|     14 | 4030 | `			zIn++;` |
|      2 | 4031 | `		}` |
|     14 | 4032 | `		if( zCur < zIn ){` |
|      - | 4033 | `			/* Output chunk verbatim */` |
|     14 | 4034 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      6 | 4035 | `		}` |
|     14 | 4036 | `		if( zIn >= zEnd ){` |
|      - | 4037 | `			/* No more input to process */` |
|      8 | 4038 | `			break;` |
|      - | 4039 | `		}` |
|      - | 4040 | `		/* Output the HTML line break */` |
|      - | 4041 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br />' (space before the slash), otherwise use '<br>' */` |
|      8 | 4042 | `		if( is_xhtml ){` |
|      6 | 4043 | `			ph7_result_string(pCtx,"<br />",(int)sizeof("<br />")-1);` |
|      4 | 4044 | `		}else{` |
|      3 | 4045 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 4046 | `		}` |
|      8 | 4047 | `		zCur = zIn;` |
|      - | 4048 | `		/* Append trailing line */` |
|     17 | 4049 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      8 | 4050 | `			zIn++;` |
|      2 | 4051 | `		}` |
|      8 | 4052 | `		if( zCur < zIn ){` |
|      - | 4053 | `			/* Output chunk verbatim */` |
|      8 | 4054 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      3 | 4055 | `		}` |
|      2 | 4056 | `	}` |
|      8 | 4057 | `	return PH7_OK;` |
|      6 | 4058 | `}` |
|      - | 4059 | `/*` |
|      - | 4060 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 4061 | ` *  According to the PHP reference manual.` |
|      - | 4062 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 4063 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 4064 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 4065 | ` * This applies to both sprintf() and printf().` |
|      - | 4066 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 4067 | ` * or more of these elements, in order:` |
|      - | 4068 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 4069 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 4070 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 4071 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 4072 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 4073 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 4074 | ` *   it with a single quote ('). See the examples below.` |
|      - | 4075 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 4076 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 4077 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 4078 | ` *   should result in.` |
|      - | 4079 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 4080 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 4081 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 4082 | ` *   limit to the string.` |
|      - | 4083 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 4084 | ` *       % - a literal percent character. No argument is required.` |
|      - | 4085 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 4086 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 4087 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 4088 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 4089 | ` * 	     for the number of digits after the decimal point.` |
|      - | 4090 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 4091 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 4092 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 4093 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 4094 | ` *       g - shorter of %e and %f.` |
|      - | 4095 | ` *       G - shorter of %E and %f.` |
|      - | 4096 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 4097 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 4098 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 4099 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 4100 | ` */` |
|      - | 4101 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4102 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4103 | `/*` |
|      - | 4104 | ` * Symisc eXtension.` |
|      - | 4105 | ` * string size_format(int64 $size)` |
|      - | 4106 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 4107 | ` *  Example:` |
|      - | 4108 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 4109 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 4110 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 4111 | ` * Parameter` |
|      - | 4112 | ` *  $size` |
|      - | 4113 | ` *    Entity size in bytes.` |
|      - | 4114 | ` * Return` |
|      - | 4115 | ` *   Formatted string representation of the given size.` |
|      - | 4116 | ` */` |
|     24 | 4117 | `PH7_PRIVATE int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4118 | `{` |
|      - | 4119 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 4120 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 4121 | `	sxi32 nRest,i_32;` |
|      - | 4122 | `	ph7_int64 iSize;` |
|     25 | 4123 | `	int c = -1; /* index in zUnit[] */` |
|      - | 4124 |  |
|     25 | 4125 | `	if( nArg < 1 ){` |
|      - | 4126 | `		/* Missing argument,return the empty string */` |
|      3 | 4127 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4128 | `		return PH7_OK;` |
|      - | 4129 | `	}` |
|      - | 4130 | `	/* Extract the given size */` |
|     23 | 4131 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 4132 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 4133 | `		/* Don't bother formatting,return immediately */` |
|      5 | 4134 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 4135 | `		return PH7_OK;` |
|      - | 4136 | `	}` |
|     19 | 4137 | `	for(;;){` |
|     39 | 4138 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 4139 | `		iSize >>= 10;` |
|     39 | 4140 | `		c++;` |
|     39 | 4141 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 4142 | `			break;` |
|      - | 4143 | `		}` |
|      1 | 4144 | `	}` |
|     19 | 4145 | `	nRest /= 100;` |
|     19 | 4146 | `	if( nRest > 9 ){` |
|    ! 0 | 4147 | `		nRest = 9;` |
|    ! 0 | 4148 | `	}` |
|     19 | 4149 | `	if( iSize > 999 ){` |
|    ! 0 | 4150 | `		c++;` |
|    ! 0 | 4151 | `		nRest = 9;` |
|    ! 0 | 4152 | `		iSize = 0;` |
|    ! 0 | 4153 | `	}` |
|     19 | 4154 | `	i_32 = (sxi32)iSize;` |
|      - | 4155 | `	/* Format */` |
|     19 | 4156 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 4157 | `	return PH7_OK;` |
|     13 | 4158 | `}` |
|      - | 4159 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4160 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4161 | `/*` |
|      - | 4162 | ` * string str_shuffle(string $str)` |
|      - | 4163 |  |
|      - | 4164 | ` *  Randomly shuffles a string.` |
|      - | 4165 | ` * Parameters` |
|      - | 4166 | ` *  $str` |
|      - | 4167 | ` *   The input string.` |
|      - | 4168 | ` * Return` |
|      - | 4169 | ` *  Returns the shuffled string.` |
|      - | 4170 | ` */` |
|     10 | 4171 | `PH7_PRIVATE int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4172 | `{` |
|      - | 4173 | `	const char *zString;` |
|      - | 4174 | `	int nLen,i,c;` |
|      - | 4175 | `	sxu32 iR;` |
|     11 | 4176 | `	if( nArg < 1 ){` |
|      - | 4177 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4178 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4179 | `		return PH7_OK;` |
|      - | 4180 | `	}` |
|      - | 4181 | `	/* Extract the target string */` |
|     11 | 4182 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 4183 | `	if( nLen < 1 ){` |
|      - | 4184 | `		/* Nothing to shuffle */` |
|      3 | 4185 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4186 | `		return PH7_OK;` |
|      - | 4187 | `	}` |
|      - | 4188 | `	/* Shuffle the string. Draw through the MT19937 generator so str_shuffle()` |
|      - | 4189 | `	 * responds to srand()/mt_srand() (reproducible under a seed), like php; the` |
|      - | 4190 | `	 * sampling differs from php's Fisher-Yates so it is not value-parity. */` |
|     43 | 4191 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 4192 | `		/* Generate a random number first */` |
|     35 | 4193 | `		iR = PH7_VmMtRand(pCtx->pVm);` |
|      - | 4194 | `		/* Extract a random offset */` |
|     35 | 4195 | `		c = zString[iR % nLen];` |
|      - | 4196 | `		/* Append it */` |
|     35 | 4197 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 4198 | `	}` |
|      9 | 4199 | `	return PH7_OK;` |
|      6 | 4200 | `}` |
|      - | 4201 | `/*` |
|      - | 4202 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 4203 | ` *  Convert a string to an array.` |
|      - | 4204 | ` * Parameters` |
|      - | 4205 | ` * $string` |
|      - | 4206 | ` *  The input string.` |
|      - | 4207 | ` * $split_length` |
|      - | 4208 | ` *  Maximum length of the chunk.` |
|      - | 4209 | ` * Return` |
|      - | 4210 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 4211 | ` *  except possibly the last one which may be shorter.` |
|      - | 4212 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 4213 | ` *  as the first (and only) array element.` |
|      - | 4214 | ` *  An empty string returns an empty array.` |
|      - | 4215 | ` * Errors` |
|      - | 4216 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 4217 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 4218 | ` *  ValueError if $split_length is less than 1.` |
|      - | 4219 | ` */` |
|     26 | 4220 | `PH7_PRIVATE int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4221 | `{` |
|      - | 4222 | `	const char *zString,*zEnd;` |
|      - | 4223 | `	ph7_value *pArray,*pValue;` |
|      - | 4224 | `	int split_len;` |
|      - | 4225 | `	int nLen;` |
|     29 | 4226 | `	if( nArg < 1 ){` |
|    ! 0 | 4227 | `		return PH7_VmThrowException(pCtx,` |
|      - | 4228 | `			"ArgumentCountError",` |
|      - | 4229 | `			"str_split() expects at least 1 argument, %d given",` |
|    ! 0 | 4230 | `			nArg` |
|      - | 4231 | `			);` |
|      - | 4232 | `	}` |
|      - | 4233 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     39 | 4234 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     42 | 4235 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     26 | 4236 | `	    ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 4237 | `		return PH7_VmThrowException(pCtx,` |
|      - | 4238 | `			"TypeError",` |
|      - | 4239 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 4240 | `			ph7_type_name(apArg[0])` |
|      - | 4241 | `			);` |
|      - | 4242 | `	}` |
|      - | 4243 | `	/* Point to the target string */` |
|     29 | 4244 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     29 | 4245 | `	split_len = (int)sizeof(char);` |
|     29 | 4246 | `	if( nArg > 1 ){` |
|      - | 4247 | `		/* Split length */` |
|     17 | 4248 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 4249 | `		if( split_len < 1 ){` |
|      6 | 4250 | `			return PH7_VmThrowException(pCtx,` |
|      - | 4251 | `				"ValueError",` |
|      - | 4252 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 4253 | `				);` |
|      - | 4254 | `		}` |
|     11 | 4255 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 4256 | `			split_len = nLen;` |
|      1 | 4257 | `		}` |
|      5 | 4258 | `	}` |
|      - | 4259 | `	/* Create the array and the scalar value */` |
|     23 | 4260 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 4261 | `	/*Chunk value */` |
|     23 | 4262 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     23 | 4263 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 4264 | `		/* Return FALSE */` |
|    ! 0 | 4265 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4266 | `		return PH7_OK;` |
|      - | 4267 | `	}` |
|      - | 4268 | `	/* Point to the end of the string */` |
|     23 | 4269 | `	zEnd = &zString[nLen];` |
|      - | 4270 | `	/* Perform the requested operation */` |
|    131 | 4271 | `	for(;;){` |
|      - | 4272 | `		int nMax;` |
|    143 | 4273 | `		if( zString >= zEnd ){` |
|      - | 4274 | `			/* No more input to process */` |
|     23 | 4275 | `			break;` |
|      - | 4276 | `		}` |
|    121 | 4277 | `		nMax = (int)(zEnd-zString);` |
|    121 | 4278 | `		if( nMax < split_len ){` |
|      3 | 4279 | `			split_len = nMax;` |
|      1 | 4280 | `		}` |
|      - | 4281 | `		/* Copy the current chunk */` |
|    121 | 4282 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 4283 | `		/* Insert it */` |
|    121 | 4284 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 4285 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 4286 | `		}` |
|      - | 4287 | `		/* reset the string cursor */` |
|    121 | 4288 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 4289 | `		/* Update position */` |
|    121 | 4290 | `		zString += split_len;` |
|      1 | 4291 | `	}` |
|      - | 4292 | `	/*` |
|      - | 4293 | `	 * Return the array.` |
|      - | 4294 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 4295 | `	 * upon we return from this function.` |
|      - | 4296 | `	 */` |
|     23 | 4297 | `	ph7_result_value(pCtx,pArray);` |
|     23 | 4298 | `	return PH7_OK;` |
|     16 | 4299 | `}` |
|      - | 4300 | `/*` |
|      - | 4301 | ` * array\|string count_chars(string $string[,int $mode = 0 ])` |
|      - | 4302 | ` *  How many times each byte value occurs in a string.` |
|      - | 4303 | ` * Parameters` |
|      - | 4304 | ` *  $string` |
|      - | 4305 | ` *   The examined string.` |
|      - | 4306 | ` *  $mode` |
|      - | 4307 | ` *   0: an array of all 256 byte values -> frequency.` |
|      - | 4308 | ` *   1: only the byte values that occur.` |
|      - | 4309 | ` *   2: only the byte values that do not.` |
|      - | 4310 | ` *   3: a string of the byte values that occur.` |
|      - | 4311 | ` *   4: a string of the byte values that do not.` |
|      - | 4312 | ` * Return` |
|      - | 4313 | ` *  An array for modes 0-2, a string for modes 3-4.` |
|      - | 4314 | ` */` |
|     74 | 4315 | `PH7_PRIVATE int PH7_builtin_count_chars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4316 | `{` |
|      - | 4317 | `	sxu32 aCount[256];` |
|      - | 4318 | `	const unsigned char *zIn;` |
|      - | 4319 | `	ph7_value *pArray,*pValue;` |
|      - | 4320 | `	char zOut[256];` |
|     76 | 4321 | `	int nOut = 0;` |
|     76 | 4322 | `	int nLen = 0;` |
|     76 | 4323 | `	int iMode = 0;` |
|      - | 4324 | `	int i;` |
|     76 | 4325 | `	if( nArg < 1 ){` |
|      - | 4326 | `		/* Arity is enforced from aBuiltinSig[] before the call. */` |
|    ! 0 | 4327 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4328 | `		return PH7_OK;` |
|      - | 4329 | `	}` |
|     76 | 4330 | `	if( nArg > 1 ){` |
|      - | 4331 | ``		/* php declares `int $mode`; the shared screen refuses the values no`` |
|      - | 4332 | `		 * coercion can reach (array, object, resource, null), leaving the string` |
|      - | 4333 | `		 * and float narrowing to the builtin -- both of which php only DEPRECATES` |
|      - | 4334 | `		 * and PHL rejects (§10). */` |
|     76 | 4335 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 4336 | `			/* php wants the WHOLE string to be numeric (surrounding whitespace` |
|      - | 4337 | `			 * aside): "2abc" and "0x2" are TypeErrors, not 2. A float-shaped one` |
|      - | 4338 | `			 * that would LOSE something is §10's refusal of a deprecation. */` |
|      - | 4339 | `			double d;` |
|      7 | 4340 | `			if( !PH7_MemObjStringIsNumeric(apArg[1]) ){` |
|    ! 0 | 4341 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4342 | `					"count_chars(): Argument #2 ($mode) must be of type int, string given");` |
|      - | 4343 | `			}` |
|      7 | 4344 | `			d = ph7_value_to_double(apArg[1]);` |
|      7 | 4345 | `			if( d != (double)(sxi64)d ){` |
|    ! 0 | 4346 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4347 | `					"count_chars(): Argument #2 ($mode) must be of type int, string given");` |
|      1 | 4348 | `			}` |
|     73 | 4349 | `		}else if( ph7_value_is_float(apArg[1]) ){` |
|      3 | 4350 | `			double d = ph7_value_to_double(apArg[1]);` |
|      3 | 4351 | `			if( d != (double)(sxi64)d ){` |
|    ! 0 | 4352 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4353 | `					"count_chars(): Argument #2 ($mode) must be of type int, float given");` |
|      - | 4354 | `			}` |
|      1 | 4355 | `		}` |
|     76 | 4356 | `		iMode = ph7_value_to_int(apArg[1]);` |
|     76 | 4357 | `		if( iMode < 0 \|\| iMode > 4 ){` |
|      7 | 4358 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4359 | `				"count_chars(): Argument #2 ($mode) must be between 0 and 4 (inclusive)");` |
|      - | 4360 | `		}` |
|     34 | 4361 | `	}` |
|      - | 4362 | `	/* Binary safe: the string is counted by LENGTH, so an embedded NUL is a byte` |
|      - | 4363 | `	 * value like any other. */` |
|     70 | 4364 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  17478 | 4365 | `	for( i = 0 ; i < 256 ; ++i ){` |
|  17410 | 4366 | `		aCount[i] = 0;` |
|   8706 | 4367 | `	}` |
|    280 | 4368 | `	for( i = 0 ; i < nLen ; ++i ){` |
|    212 | 4369 | `		aCount[zIn[i]]++;` |
|    107 | 4370 | `	}` |
|     70 | 4371 | `	if( iMode >= 3 ){` |
|      - | 4372 | `		/* 3 = the bytes that occur, 4 = the bytes that do not. */` |
|   6684 | 4373 | `		for( i = 0 ; i < 256 ; ++i ){` |
|   6658 | 4374 | `			if( (aCount[i] != 0) == (iMode == 3) ){` |
|   1568 | 4375 | `				zOut[nOut++] = (char)i;` |
|    783 | 4376 | `			}` |
|   3330 | 4377 | `		}` |
|     28 | 4378 | `		ph7_result_string(pCtx,zOut,nOut);` |
|     28 | 4379 | `		return PH7_OK;` |
|      - | 4380 | `	}` |
|     44 | 4381 | `	pArray = ph7_context_new_array(pCtx);` |
|     44 | 4382 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     44 | 4383 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 4384 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4385 | `	}` |
|  10796 | 4386 | `	for( i = 0 ; i < 256 ; ++i ){` |
|      - | 4387 | `		/* 0 = every byte value, 1 = the ones that occur, 2 = the ones that do not` |
|      - | 4388 | `		 * (php stores their count, which is always 0). */` |
|  10754 | 4389 | `		if( iMode != 0 && (aCount[i] != 0) != (iMode == 1) ){` |
|   7630 | 4390 | `			continue;` |
|      - | 4391 | `		}` |
|   3126 | 4392 | `		ph7_value_int64(pValue,(ph7_int64)aCount[i]);` |
|   3126 | 4393 | `		if( ph7_array_add_intkey_elem(pArray,i,pValue) != SXRET_OK ){` |
|    ! 0 | 4394 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 4395 | `		}` |
|   1564 | 4396 | `	}` |
|     44 | 4397 | `	ph7_result_value(pCtx,pArray);` |
|     44 | 4398 | `	return PH7_OK;` |
|     39 | 4399 | `}` |
|      - | 4400 | `/*` |
|      - | 4401 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 4402 | ` * return the longest match.` |
|      - | 4403 | ` * Refer to [strspn()].` |
|      - | 4404 | ` */` |
|     66 | 4405 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 4406 | `{` |
|     67 | 4407 | `	const char *zEnd = &zString[nLen];` |
|     67 | 4408 | `	const char *zIn = zString;` |
|      - | 4409 | `	int i,c;` |
|    110 | 4410 | `	for(;;){` |
|    221 | 4411 | `		if( zString >= zEnd ){` |
|     45 | 4412 | `			break;` |
|      - | 4413 | `		}` |
|      - | 4414 | `		/* Extract current character */` |
|    177 | 4415 | `		c = zString[0];` |
|      - | 4416 | `		/* Perform the lookup */` |
|    589 | 4417 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    567 | 4418 | `			if( c == zMask[i] ){` |
|      - | 4419 | `				/* Character found */` |
|    155 | 4420 | `				break;` |
|      - | 4421 | `			}` |
|    207 | 4422 | `		}` |
|    177 | 4423 | `		if( i >= nMaskLen ){` |
|      - | 4424 | `			/* Character not in the current mask,break immediately */` |
|     23 | 4425 | `			break;` |
|      - | 4426 | `		}` |
|      - | 4427 | `		/* Advance cursor */` |
|    155 | 4428 | `		zString++;` |
|      1 | 4429 | `	}` |
|      - | 4430 | `	/* Longest match */` |
|     67 | 4431 | `	return (int)(zString-zIn);` |
|      1 | 4432 | `}` |
|      - | 4433 | `/*` |
|      - | 4434 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 4435 | ` * Refer to [strcspn()].` |
|      - | 4436 | ` */` |
|     48 | 4437 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 4438 | `{` |
|     49 | 4439 | `	const char *zEnd = &zString[nLen];` |
|     49 | 4440 | `	const char *zIn = zString;` |
|      - | 4441 | `	int i,c;` |
|     81 | 4442 | `	for(;;){` |
|    163 | 4443 | `		if( zString >= zEnd ){` |
|     39 | 4444 | `			break;` |
|      - | 4445 | `		}` |
|      - | 4446 | `		/* Extract current character */` |
|    125 | 4447 | `		c = zString[0];` |
|      - | 4448 | `		/* Perform the lookup */` |
|    217 | 4449 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    103 | 4450 | `			if( c == zMask[i] ){` |
|     11 | 4451 | `				break;` |
|      - | 4452 | `			}` |
|     47 | 4453 | `		}` |
|    125 | 4454 | `		if( i < nMaskLen ){` |
|      - | 4455 | `			/* Character in the current mask,break immediately */` |
|     11 | 4456 | `			break;` |
|      - | 4457 | `		}` |
|      - | 4458 | `		/* Advance cursor */` |
|    115 | 4459 | `		zString++;` |
|      1 | 4460 | `	}` |
|      - | 4461 | `	/* Longest match */` |
|     49 | 4462 | `	return (int)(zString-zIn);` |
|      1 | 4463 | `}` |
|      - | 4464 | `/*` |
|      - | 4465 | ` * Shared body of strspn()/strcspn(): resolve php's ($offset,$length) window over` |
|      - | 4466 | ` * $string, then measure the span from the window's first byte.` |
|      - | 4467 | ` *` |
|      - | 4468 | ` * php's window rules (ext/standard/string.c, php_spn_common_handler) — a negative` |
|      - | 4469 | ` * $offset counts back from the end and CLAMPS to 0 (it is never "invalid"); an` |
|      - | 4470 | ` * $offset past the end clamps to the end, so the window is empty and the answer is` |
|      - | 4471 | ` * 0; a negative $length leaves that many bytes off the end of the remaining span` |
|      - | 4472 | ` * and clamps to 0; a zero-length window answers 0. PH7 answered 0 for a negative` |
|      - | 4473 | ` * offset that reached past the start, IGNORED a zero or negative $length entirely` |
|      - | 4474 | ` * (measuring the whole rest of the string instead), and truncated the offset to` |
|      - | 4475 | `` * `int`, so a 64-bit offset wrapped into a valid one.`` |
|      - | 4476 | ` *` |
|      - | 4477 | ` * PH7 also ran the scan over the first WHITESPACE-DELIMITED TOKEN rather than over` |
|      - | 4478 | ` * the raw window (leading spaces skipped, scan stopped at the next space), so` |
|      - | 4479 | ` * strspn("a b c","abc ") answered 1 where php answers 5 and strspn("  abc","abc")` |
|      - | 4480 | ` * answered 3 where php answers 0 — silent wrong answers on ordinary input. php` |
|      - | 4481 | ` * scans raw bytes; so does this.` |
|      - | 4482 | ` *` |
|      - | 4483 | ` * An empty $mask needs no special case: the mask lookup fails for every byte, so` |
|      - | 4484 | ` * strspn stops at once (0) and strcspn runs to the end of the window (its length),` |
|      - | 4485 | ` * which is exactly what php answers.` |
|      - | 4486 | ` */` |
|    114 | 4487 | `static int StrSpnCommonHandler(` |
|      - | 4488 | `	ph7_context *pCtx,    /* Call context */` |
|      - | 4489 | `	int nArg,             /* Argument count */` |
|      - | 4490 | `	ph7_value **apArg,    /* Arguments */` |
|      - | 4491 | `	int bComplement       /* TRUE for strcspn() */` |
|      - | 4492 | `	)` |
|      1 | 4493 | `{` |
|    115 | 4494 | `	const char *zFunc = bComplement ? "strcspn" : "strspn";` |
|      - | 4495 | `	const char *zString,*zMask;` |
|      - | 4496 | `	int iMasklen,iLen;` |
|      - | 4497 | `	sxi64 iStart,iSpan;` |
|    115 | 4498 | `	if( nArg < 2 ){` |
|      - | 4499 | `		/* Arity is enforced at the call boundary; nothing sensible to return here. */` |
|    ! 0 | 4500 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4501 | `		return PH7_OK;` |
|      - | 4502 | `	}` |
|      - | 4503 | `	/* Extract the target string and the mask */` |
|    115 | 4504 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|    115 | 4505 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|    115 | 4506 | `	if( iLen < 0 ){` |
|    ! 0 | 4507 | `		iLen = 0;` |
|    ! 0 | 4508 | `	}` |
|    115 | 4509 | `	if( iMasklen < 0 ){` |
|    ! 0 | 4510 | `		iMasklen = 0;` |
|    ! 0 | 4511 | `	}` |
|    115 | 4512 | `	iStart = 0;` |
|    115 | 4513 | `	if( nArg > 2 ){` |
|     73 | 4514 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],zFunc,3,"$offset","int",&iStart);` |
|     73 | 4515 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 4516 | `			return rcArg;` |
|      - | 4517 | `		}` |
|     73 | 4518 | `		if( iStart < 0 ){` |
|      - | 4519 | `			/* Count back from the end, clamped to the start (guarded so an` |
|      - | 4520 | `			 * INT64_MIN offset cannot overflow the addition). */` |
|     13 | 4521 | `			iStart = ( iStart < -(sxi64)iLen ) ? 0 : iStart + iLen;` |
|     67 | 4522 | `		}else if( iStart > (sxi64)iLen ){` |
|      9 | 4523 | `			iStart = iLen;` |
|      4 | 4524 | `		}` |
|     36 | 4525 | `	}` |
|    115 | 4526 | `	iSpan = (sxi64)iLen - iStart;` |
|    115 | 4527 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|     39 | 4528 | `		sxi64 iUserlen = 0;` |
|     39 | 4529 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[3],zFunc,4,"$length","?int",&iUserlen);` |
|     39 | 4530 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 4531 | `			return rcArg;` |
|      - | 4532 | `		}` |
|     39 | 4533 | `		if( iUserlen < 0 ){` |
|      - | 4534 | `			/* Leave \|$length\| bytes off the end of the remaining span (guarded` |
|      - | 4535 | `			 * against an INT64_MIN underflow the same way). */` |
|     21 | 4536 | `			iSpan = ( iUserlen < -iSpan ) ? 0 : iSpan + iUserlen;` |
|     29 | 4537 | `		}else if( iUserlen < iSpan ){` |
|     11 | 4538 | `			iSpan = iUserlen;` |
|      5 | 4539 | `		}` |
|     19 | 4540 | `	}` |
|    172 | 4541 | `	ph7_result_int(pCtx,bComplement` |
|     48 | 4542 | `		? LongestStringMask2(&zString[iStart],(int)iSpan,zMask,iMasklen)` |
|     66 | 4543 | `		: LongestStringMask(&zString[iStart],(int)iSpan,zMask,iMasklen));` |
|    115 | 4544 | `	return PH7_OK;` |
|     58 | 4545 | `}` |
|      - | 4546 | `/*` |
|      - | 4547 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 4548 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 4549 | ` *  of characters contained within a given mask.` |
|      - | 4550 | ` * Parameters` |
|      - | 4551 | ` * $str` |
|      - | 4552 | ` *  The input string.` |
|      - | 4553 | ` * $mask` |
|      - | 4554 | ` *  The list of allowable characters.` |
|      - | 4555 | ` * $start` |
|      - | 4556 | ` *  The position in subject to start searching.` |
|      - | 4557 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 4558 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 4559 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 4560 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 4561 | ` *  start'th position from the end of subject.` |
|      - | 4562 | ` * $length` |
|      - | 4563 | ` *  The length of the segment from subject to examine.` |
|      - | 4564 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 4565 | ` *  characters after the starting position.` |
|      - | 4566 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 4567 | ` *  position up to length characters from the end of subject.` |
|      - | 4568 | ` * Return` |
|      - | 4569 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 4570 | ` * in mask.` |
|      - | 4571 | ` */` |
|     66 | 4572 | `PH7_PRIVATE int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4573 | `{` |
|     67 | 4574 | `	return StrSpnCommonHandler(pCtx,nArg,apArg,0);` |
|      1 | 4575 | `}` |
|      - | 4576 | `/*` |
|      - | 4577 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 4578 | ` *  Find length of initial segment not matching mask.` |
|      - | 4579 | ` * Parameters` |
|      - | 4580 | ` * $str` |
|      - | 4581 | ` *  The input string.` |
|      - | 4582 | ` * $mask` |
|      - | 4583 | ` *  The list of not allowed characters.` |
|      - | 4584 | ` * $start` |
|      - | 4585 | ` *  The position in subject to start searching.` |
|      - | 4586 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 4587 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 4588 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 4589 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 4590 | ` *  start'th position from the end of subject.` |
|      - | 4591 | ` * $length` |
|      - | 4592 | ` *  The length of the segment from subject to examine.` |
|      - | 4593 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 4594 | ` *  characters after the starting position.` |
|      - | 4595 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 4596 | ` *  position up to length characters from the end of subject.` |
|      - | 4597 | ` * Return` |
|      - | 4598 | ` *  Returns the length of the segment as an integer.` |
|      - | 4599 | ` */` |
|     48 | 4600 | `PH7_PRIVATE int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4601 | `{` |
|     49 | 4602 | `	return StrSpnCommonHandler(pCtx,nArg,apArg,1);` |
|      1 | 4603 | `}` |
|      - | 4604 | `/*` |
|      - | 4605 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 4606 | ` *  Search a string for any of a set of characters.` |
|      - | 4607 | ` * Parameters` |
|      - | 4608 | ` *  $haystack` |
|      - | 4609 | ` *   The string where char_list is looked for.` |
|      - | 4610 | ` *  $char_list` |
|      - | 4611 | ` *   This parameter is case sensitive.` |
|      - | 4612 | ` * Return` |
|      - | 4613 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 4614 | ` */` |
|     14 | 4615 | `PH7_PRIVATE int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4616 | `{` |
|      - | 4617 | `	const char *zString,*zList,*zEnd;` |
|      - | 4618 | `	int iLen,iListLen,i,c;` |
|      - | 4619 | `	sxu32 nOfft,nMax;` |
|      - | 4620 | `	sxi32 rc;` |
|     15 | 4621 | `	if( nArg < 2 ){` |
|      - | 4622 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 4623 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4624 | `		return PH7_OK;` |
|      - | 4625 | `	}` |
|      - | 4626 | `	/* Extract the haystack and the char list */` |
|     15 | 4627 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|     15 | 4628 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|     15 | 4629 | `	if( iListLen < 1 ){` |
|      - | 4630 | `		/* An empty set can never match, so php rejects it rather than answering` |
|      - | 4631 | `		 * a FALSE indistinguishable from "not found" (checked BEFORE the haystack,` |
|      - | 4632 | `		 * so strpbrk("","") throws too). */` |
|      5 | 4633 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4634 | `			"strpbrk(): Argument #2 ($characters) must be a non-empty string");` |
|      - | 4635 | `	}` |
|     11 | 4636 | `	if( iLen < 1 ){` |
|      - | 4637 | `		/* Nothing to process,return FALSE */` |
|      3 | 4638 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4639 | `		return PH7_OK;` |
|      - | 4640 | `	}` |
|      - | 4641 | `	/* Point to the end of the string */` |
|      9 | 4642 | `	zEnd = &zString[iLen];` |
|      9 | 4643 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 4644 | `	/* perform the requested operation */` |
|     25 | 4645 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     17 | 4646 | `		c = zList[i];` |
|     17 | 4647 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     17 | 4648 | `		if( rc == SXRET_OK ){` |
|      9 | 4649 | `			if( nMax < nOfft ){` |
|      5 | 4650 | `				nOfft = nMax;` |
|      2 | 4651 | `			}` |
|      4 | 4652 | `		}` |
|      9 | 4653 | `	}` |
|      9 | 4654 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 4655 | `		/* No such substring,return FALSE */` |
|      5 | 4656 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4657 | `	}else{` |
|      - | 4658 | `		/* Return the substring */` |
|      5 | 4659 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 4660 | `	}` |
|      9 | 4661 | `	return PH7_OK;` |
|      8 | 4662 | `}` |
|      - | 4663 | `/*` |
|      - | 4664 | ` * string soundex(string $str)` |
|      - | 4665 | ` *  Calculate the soundex key of a string.` |
|      - | 4666 | ` * Parameters` |
|      - | 4667 | ` *  $str` |
|      - | 4668 | ` *   The input string.` |
|      - | 4669 | ` * Return` |
|      - | 4670 | ` *  Returns the soundex key as a string.` |
|      - | 4671 | ` * Note:` |
|      - | 4672 | ` *  Knuth's algorithm as php implements it (ext/standard/soundex.c). The` |
|      - | 4673 | ` *  previous implementation came from SQLite and diverged from php on three` |
|      - | 4674 | ` *  counts, each of them a silent wrong answer:` |
|      - | 4675 | ` *` |
|      - | 4676 | ` *   - a NON-LETTER inside the word RESET the "same code in a row" state, so` |
|      - | 4677 | ` *     soundex("S s") answered S200 where php answers S000 and soundex("b1b")` |
|      - | 4678 | ` *     answered B100 where php answers B000. php simply skips anything that is` |
|      - | 4679 | ` *     not a letter; only a VOWEL separates two consonants sharing a code.` |
|      - | 4680 | ` *   - the scan stopped at the first byte >= 0xC0, taking every UTF-8 lead byte` |
|      - | 4681 | ` *     for a letter and copying it raw into the key: soundex("\xff\xfe") answered` |
|      - | 4682 | ` *     "\xff000" where php answers "0000", and a leading accent HID the letters` |
|      - | 4683 | ` *     behind it (soundex("éa") answered "\xc3000" for php's A000). Inside the` |
|      - | 4684 | `` *     loop the table was indexed by `byte & 0x7f`, which folds high bytes onto`` |
|      - | 4685 | ` *     ASCII letters and invents codes for them.` |
|      - | 4686 | ` *   - the input was walked as a NUL-terminated C string, so soundex("a\0b")` |
|      - | 4687 | ` *     stopped at the NUL (A000) where php walks the whole php string (A100).` |
|      - | 4688 | ` *` |
|      - | 4689 | ` *  Classification is ASCII-only, matching php's own A-Z table (§7's locale` |
|      - | 4690 | ` *  dependence family: the old code asked libc's isalpha() through SyisAlpha,` |
|      - | 4691 | ` *  which answers differently under a non-C LC_CTYPE).` |
|      - | 4692 | ` */` |
|     52 | 4693 | `PH7_PRIVATE int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4694 | `{` |
|      - | 4695 | `	/* Code per letter A-Z; 0 means "no code" (a vowel, plus H/W/Y) */` |
|      - | 4696 | `	static const char zCode[] = "01230120022455012623010202";` |
|      - | 4697 | `	const unsigned char *zIn;` |
|      - | 4698 | `	char zResult[4];` |
|     53 | 4699 | `	int nByte,i,nOut = 0,iLast = -1;` |
|     53 | 4700 | `	if( nArg < 1 ){` |
|      - | 4701 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4702 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4703 | `		return PH7_OK;` |
|      - | 4704 | `	}` |
|     53 | 4705 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nByte);` |
|    265 | 4706 | `	for( i = 0 ; i < nByte && nOut < 4 ; ++i ){` |
|    213 | 4707 | `		int c = zIn[i];` |
|      - | 4708 | `		int code;` |
|    213 | 4709 | `		if( c >= 'a' && c <= 'z' ){` |
|    139 | 4710 | `			c -= 'a' - 'A';` |
|     69 | 4711 | `		}` |
|    213 | 4712 | `		if( c < 'A' \|\| c > 'Z' ){` |
|     35 | 4713 | `			continue; /* not a letter: skipped outright, state untouched */` |
|      - | 4714 | `		}` |
|    179 | 4715 | `		code = zCode[c - 'A'] - '0';` |
|    179 | 4716 | `		if( nOut == 0 ){` |
|      - | 4717 | `			/* The key opens with the first letter itself */` |
|     43 | 4718 | `			zResult[nOut++] = (char)c;` |
|    158 | 4719 | `		}else if( code != iLast && code != 0 ){` |
|     69 | 4720 | `			zResult[nOut++] = (char)(code + '0');` |
|     34 | 4721 | `		}` |
|    179 | 4722 | `		iLast = code;` |
|     90 | 4723 | `	}` |
|      - | 4724 | `	/* Pad to four characters. A string with no letter at all pads from nothing,` |
|      - | 4725 | `	 * which is php's "0000" (an empty input included). */` |
|    151 | 4726 | `	while( nOut < 4 ){` |
|     99 | 4727 | `		zResult[nOut++] = '0';` |
|      1 | 4728 | `	}` |
|     53 | 4729 | `	ph7_result_string(pCtx,zResult,4);` |
|     53 | 4730 | `	return PH7_OK;` |
|     27 | 4731 | `}` |
|      - | 4732 | `/*` |
|      - | 4733 | ` * string str_rot13(string $string)` |
|      - | 4734 | ` *  Perform the ROT13 transform: each ASCII letter is rotated 13 places through` |
|      - | 4735 | ` *  its own alphabet, everything else (digits, punctuation, high bytes, NULs)` |
|      - | 4736 | ` *  passes through untouched. ROT13 is its own inverse.` |
|      - | 4737 | ` */` |
|     20 | 4738 | `PH7_PRIVATE int PH7_builtin_str_rot13(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4739 | `{` |
|      - | 4740 | `	const char *zIn;` |
|      - | 4741 | `	char *zOut;` |
|      - | 4742 | `	int nLen,i;` |
|     22 | 4743 | `	if( nArg < 1 ){` |
|      - | 4744 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4745 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4746 | `		return PH7_OK;` |
|      - | 4747 | `	}` |
|     22 | 4748 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     22 | 4749 | `	if( nLen < 1 ){` |
|      3 | 4750 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4751 | `		return PH7_OK;` |
|      - | 4752 | `	}` |
|     20 | 4753 | `	zOut = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,TRUE);` |
|     20 | 4754 | `	if( zOut == 0 ){` |
|    ! 0 | 4755 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4756 | `	}` |
|    204 | 4757 | `	for( i = 0 ; i < nLen ; i++ ){` |
|    190 | 4758 | `		int c = (unsigned char)zIn[i];` |
|    190 | 4759 | `		if( (c >= 'a' && c <= 'm') \|\| (c >= 'A' && c <= 'M') ){` |
|     75 | 4760 | `			c += 13;` |
|    155 | 4761 | `		}else if( (c >= 'n' && c <= 'z') \|\| (c >= 'N' && c <= 'Z') ){` |
|     69 | 4762 | `			c -= 13;` |
|     34 | 4763 | `		}` |
|    186 | 4764 | `		zOut[i] = (char)c;` |
|     94 | 4765 | `	}` |
|     16 | 4766 | `	ph7_result_string(pCtx,zOut,nLen);` |
|     16 | 4767 | `	return PH7_OK;` |
|     10 | 4768 | `}` |
|      - | 4769 | `/*` |
|      - | 4770 | ` * Character-class table for metaphone(), php's _codes[] (ext/standard/` |
|      - | 4771 | ` * metaphone.c, itself from CPAN Text-Metaphone), indexed by 'A'..'Z':` |
|      - | 4772 | ` * bit 1 vowel (AEIOU) · bit 2 passes through unchanged (FJMNR) · bit 4 forms a` |
|      - | 4773 | ` * diphthong before H (CGPST) · bit 8 makes C and G soft (EIY) · bit 16 keeps a` |
|      - | 4774 | ` * GH from becoming F (BDH).` |
|      - | 4775 | ` */` |
|      - | 4776 | `static const char aMetaCode[26] = {` |
|      - | 4777 | `	1,16,4,16,9,2,4,16,9,2,0,2,2,2,1,4,0,2,4,4,1,0,0,0,8,0` |
|      - | 4778 | `};` |
|      - | 4779 | `/* Classification is ASCII-only, php's own table (§7 locale-dependence family:` |
|      - | 4780 | ` * libc's isalpha()/toupper() answer differently under a non-C LC_CTYPE). */` |
|      - | 4781 | `#define META_IS_ALPHA(c) (((c) >= 'A' && (c) <= 'Z') \|\| ((c) >= 'a' && (c) <= 'z'))` |
|      - | 4782 | `#define META_UP(c)       (((c) >= 'a' && (c) <= 'z') ? (char)((c) - ('a' - 'A')) : (char)(c))` |
|      - | 4783 | `#define META_ENCODE(c)   (((c) >= 'A' && (c) <= 'Z') ? aMetaCode[(c) - 'A'] : 0)` |
|      - | 4784 | `#define META_ISVOWEL(c)  (META_ENCODE(c) & 1)  /* AEIOU */` |
|      - | 4785 | `#define META_AFFECTH(c)  (META_ENCODE(c) & 4)  /* CGPST */` |
|      - | 4786 | `#define META_MAKESOFT(c) (META_ENCODE(c) & 8)  /* EIY */` |
|      - | 4787 | `#define META_NOGHTOF(c)  (META_ENCODE(c) & 16) /* BDH */` |
|      - | 4788 | `/* php's special phoneme encodings: 'sh' and 'th' */` |
|      - | 4789 | `#define META_SH '\x58' /* 'X' */` |
|      - | 4790 | `#define META_TH '\x30' /* '0' */` |
|      - | 4791 | `/*` |
|      - | 4792 | ` * php's Lookahead(): step up to nHow bytes forward from iFrom, stopping early` |
|      - | 4793 | ` * at a NUL, and answer the byte at the stop position. The php original walks a` |
|      - | 4794 | ` * NUL-terminated buffer; this walks the same way over a bounded one, treating` |
|      - | 4795 | ` * the end of the buffer as the NUL.` |
|      - | 4796 | ` */` |
|    ! 0 | 4797 | `static char MetaLookahead(const char *zIn,sxu32 nLen,sxu32 iFrom,sxu32 nHow)` |
|    ! 0 | 4798 | `{` |
|      - | 4799 | `	sxu32 idx;` |
|    ! 0 | 4800 | `	for( idx = 0 ; idx < nHow ; idx++ ){` |
|    ! 0 | 4801 | `		if( iFrom + idx >= nLen \|\| zIn[iFrom + idx] == '\0' ){` |
|    ! 0 | 4802 | `			break;` |
|      - | 4803 | `		}` |
|    ! 0 | 4804 | `	}` |
|    ! 0 | 4805 | `	return (iFrom + idx < nLen) ? zIn[iFrom + idx] : '\0';` |
|    ! 0 | 4806 | `}` |
|      - | 4807 | `/*` |
|      - | 4808 | ` * string metaphone(string $string, int $max_phonemes = 0)` |
|      - | 4809 | ` *  Break an english phrase down into its phonemes. Faithful port of php-src` |
|      - | 4810 | `` *  PHP-8.5 ext/standard/metaphone.c (the `traditional` flavour, which is the`` |
|      - | 4811 | ` *  only one php's own function invokes — the non-traditional Christ/School/` |
|      - | 4812 | ` *  SCHW branches are compiled out there and are not ported). Like php's, the` |
|      - | 4813 | ` *  scan stops at an embedded NUL: the original walks a NUL-terminated buffer.` |
|      - | 4814 | ` * Return` |
|      - | 4815 | ` *  The phonemes as a string of A-Z plus php's two special encodings` |
|      - | 4816 | ` *  ('X' for "sh", '0' for "th"); "" when no letter is reached.` |
|      - | 4817 | ` */` |
|     80 | 4818 | `PH7_PRIVATE int PH7_builtin_metaphone(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4819 | `{` |
|      - | 4820 | `	const char *zIn;` |
|      - | 4821 | `	char *zOut;` |
|     82 | 4822 | `	sxi64 iMaxPhonemes = 0;` |
|     82 | 4823 | `	sxu32 w_idx = 0,nLen;` |
|     82 | 4824 | `	int nOut = 0;` |
|      - | 4825 | `	int nByte;` |
|      - | 4826 | `	char cCurr;` |
|      - | 4827 | `	/* Bounded reads standing in for the php original's NUL-terminated ones. */` |
|      - | 4828 | `#define META_BYTE(i)   ((sxu32)(i) < nLen ? zIn[(i)] : '\0')` |
|      - | 4829 | `#define META_NEXT      (META_UP(META_BYTE(w_idx + 1)))` |
|      - | 4830 | `#define META_PREV      (w_idx >= 1 ? META_UP(META_BYTE(w_idx - 1)) : '\0')` |
|      - | 4831 | `#define META_BACK(n)   (w_idx >= (sxu32)(n) ? META_UP(META_BYTE(w_idx - (sxu32)(n))) : '\0')` |
|      - | 4832 | `#define META_AFTERNEXT (META_BYTE(w_idx + 1) != '\0' ? META_UP(META_BYTE(w_idx + 2)) : '\0')` |
|      - | 4833 | `#define META_PHONIZE(c) do { zOut[nOut++] = (c); } while(0)` |
|     82 | 4834 | `	if( nArg < 1 ){` |
|      - | 4835 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4836 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4837 | `		return PH7_OK;` |
|      - | 4838 | `	}` |
|     82 | 4839 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|     82 | 4840 | `	if( nArg > 1 ){` |
|     13 | 4841 | `		iMaxPhonemes = ph7_value_to_int64(apArg[1]);` |
|     13 | 4842 | `		if( iMaxPhonemes < 0 ){` |
|      3 | 4843 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4844 | `				"metaphone(): Argument #2 ($max_phonemes) must be greater than or equal to 0");` |
|      - | 4845 | `		}` |
|      5 | 4846 | `	}` |
|     80 | 4847 | `	nLen = (sxu32)(nByte > 0 ? nByte : 0);` |
|      - | 4848 | `	/* Two output bytes per input letter ('X' phonizes "KS") is the ceiling, so` |
|      - | 4849 | `	 * one allocation covers the whole run — php grows its buffer instead. */` |
|     80 | 4850 | `	zOut = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(2 * nLen + 4),FALSE,TRUE);` |
|     80 | 4851 | `	if( zOut == 0 ){` |
|    ! 0 | 4852 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4853 | `	}` |
|      - | 4854 | `	/* Find the first letter; nothing but non-letters answers "". */` |
|     98 | 4855 | `	for( ; !META_IS_ALPHA(cCurr = META_BYTE(w_idx)) ; w_idx++ ){` |
|     23 | 4856 | `		if( cCurr == '\0' ){` |
|      5 | 4857 | `			ph7_result_string(pCtx,"",0);` |
|      5 | 4858 | `			return PH7_OK;` |
|      - | 4859 | `		}` |
|     10 | 4860 | `	}` |
|      - | 4861 | `	/* The first phoneme is processed specially. A case that neither phonizes` |
|      - | 4862 | `	 * nor advances leaves the letter for the main loop to read as an ordinary` |
|      - | 4863 | `	 * one (php's own structure). */` |
|     76 | 4864 | `	cCurr = META_UP(cCurr);` |
|     76 | 4865 | `	switch( cCurr ){` |
|      3 | 4866 | `	case 'A':` |
|      - | 4867 | `		/* AE becomes E; a vowel at the beginning is preserved */` |
|      8 | 4868 | `		if( META_NEXT == 'E' ){` |
|    ! 0 | 4869 | `			META_PHONIZE('E');` |
|    ! 0 | 4870 | `			w_idx += 2;` |
|    ! 0 | 4871 | `		}else{` |
|      8 | 4872 | `			META_PHONIZE('A');` |
|      8 | 4873 | `			w_idx++;` |
|      - | 4874 | `		}` |
|      8 | 4875 | `		break;` |
|      5 | 4876 | `	case 'G':` |
|      - | 4877 | `	case 'K':` |
|      - | 4878 | `	case 'P':` |
|      - | 4879 | `		/* [GKP]N becomes N */` |
|     11 | 4880 | `		if( META_NEXT == 'N' ){` |
|      5 | 4881 | `			META_PHONIZE('N');` |
|      5 | 4882 | `			w_idx += 2;` |
|      2 | 4883 | `		}` |
|     11 | 4884 | `		break;` |
|      3 | 4885 | `	case 'W':{` |
|      - | 4886 | `		/* WR becomes R; WH and W before a vowel keep the W; else dropped */` |
|      7 | 4887 | `		char cNext = META_NEXT;` |
|      7 | 4888 | `		if( cNext == 'R' ){` |
|      3 | 4889 | `			META_PHONIZE('R');` |
|      3 | 4890 | `			w_idx += 2;` |
|      6 | 4891 | `		}else if( cNext == 'H' \|\| META_ISVOWEL(cNext) ){` |
|      5 | 4892 | `			META_PHONIZE('W');` |
|      5 | 4893 | `			w_idx += 2;` |
|      2 | 4894 | `		}` |
|      7 | 4895 | `		break;` |
|      - | 4896 | `	}` |
|      1 | 4897 | `	case 'X':` |
|      - | 4898 | `		/* X becomes S */` |
|      3 | 4899 | `		META_PHONIZE('S');` |
|      3 | 4900 | `		w_idx++;` |
|      3 | 4901 | `		break;` |
|      2 | 4902 | `	case 'E':` |
|      - | 4903 | `	case 'I':` |
|      - | 4904 | `	case 'O':` |
|      - | 4905 | `	case 'U':` |
|      - | 4906 | `		/* Vowels are kept (A handled above) */` |
|      5 | 4907 | `		META_PHONIZE(cCurr);` |
|      5 | 4908 | `		w_idx++;` |
|      4 | 4909 | `		break;` |
|     23 | 4910 | `	default:` |
|     46 | 4911 | `		break;` |
|      - | 4912 | `	}` |
|      - | 4913 | `	/* On to the metaphoning */` |
|    658 | 4914 | `	for( ; (cCurr = META_BYTE(w_idx)) != '\0' &&` |
|    604 | 4915 | `	       (iMaxPhonemes == 0 \|\| (sxi64)nOut < iMaxPhonemes) ; w_idx++ ){` |
|      - | 4916 | `		/* Letters an encoding below consumed along with this one */` |
|    388 | 4917 | `		sxu32 nSkip = 0;` |
|      - | 4918 | `		char cPrev;` |
|    388 | 4919 | `		if( !META_IS_ALPHA(cCurr) ){` |
|      5 | 4920 | `			continue;` |
|      - | 4921 | `		}` |
|    384 | 4922 | `		cCurr = META_UP(cCurr);` |
|    384 | 4923 | `		cPrev = META_PREV;` |
|      - | 4924 | `		/* Drop duplicates, except CC */` |
|    384 | 4925 | `		if( cCurr == cPrev && cCurr != 'C' ){` |
|      7 | 4926 | `			continue;` |
|      - | 4927 | `		}` |
|    378 | 4928 | `		switch( cCurr ){` |
|      3 | 4929 | `		case 'B':` |
|      - | 4930 | `			/* B unless in MB */` |
|      8 | 4931 | `			if( cPrev != 'M' ){` |
|      3 | 4932 | `				META_PHONIZE('B');` |
|      1 | 4933 | `			}` |
|      8 | 4934 | `			break;` |
|     17 | 4935 | `		case 'C':{` |
|      - | 4936 | `			/* 'sh' in -CIA- and -CH-; S in -CI-, -CE-, -CY-;` |
|      - | 4937 | `			 * dropped in -SCI-, -SCE-, -SCY-; else K */` |
|     36 | 4938 | `			char cNext = META_NEXT;` |
|     36 | 4939 | `			if( META_MAKESOFT(cNext) ){ /* C[IEY] */` |
|      7 | 4940 | `				if( cNext == 'I' && META_AFTERNEXT == 'A' ){ /* CIA */` |
|    ! 0 | 4941 | `					META_PHONIZE(META_SH);` |
|      6 | 4942 | `				}else if( cPrev == 'S' ){` |
|      - | 4943 | `					/* dropped */` |
|      2 | 4944 | `				}else{` |
|      5 | 4945 | `					META_PHONIZE('S');` |
|      1 | 4946 | `				}` |
|     33 | 4947 | `			}else if( cNext == 'H' ){` |
|     15 | 4948 | `				META_PHONIZE(META_SH);` |
|     15 | 4949 | `				nSkip++;` |
|      8 | 4950 | `			}else{` |
|     16 | 4951 | `				META_PHONIZE('K');` |
|      - | 4952 | `			}` |
|     36 | 4953 | `			break;` |
|      - | 4954 | `		}` |
|      5 | 4955 | `		case 'D':` |
|      - | 4956 | `			/* J in -DGE-, -DGI-, -DGY-; else T */` |
|     14 | 4957 | `			if( META_NEXT == 'G' && META_MAKESOFT(META_AFTERNEXT) ){` |
|      3 | 4958 | `				META_PHONIZE('J');` |
|      3 | 4959 | `				nSkip++;` |
|      2 | 4960 | `			}else{` |
|      9 | 4961 | `				META_PHONIZE('T');` |
|      - | 4962 | `			}` |
|     11 | 4963 | `			break;` |
|      7 | 4964 | `		case 'G':{` |
|      - | 4965 | `			/* F in -GH unless B--GH, D--GH, -H--GH, -H---GH (silent there);` |
|      - | 4966 | `			 * dropped in -GN, -GNED (and -DG[EIY]-, handled in D);` |
|      - | 4967 | `			 * J in -GE-, -GI-, -GY- when not GG; else K */` |
|     15 | 4968 | `			char cNext = META_NEXT;` |
|     15 | 4969 | `			if( cNext == 'H' ){` |
|     17 | 4970 | `				if( !(META_NOGHTOF(META_BACK(3)) \|\| META_BACK(4) == 'H') ){` |
|      9 | 4971 | `					META_PHONIZE('F');` |
|      9 | 4972 | `					nSkip++;` |
|      5 | 4973 | `				}` |
|     11 | 4974 | `			}else if( cNext == 'N' ){` |
|    ! 0 | 4975 | `				char cAfterNext = META_AFTERNEXT;` |
|    ! 0 | 4976 | `				if( !META_IS_ALPHA(cAfterNext) \|\|` |
|    ! 0 | 4977 | `				    (cAfterNext == 'E' && META_UP(MetaLookahead(zIn,nLen,w_idx,3)) == 'D') ){` |
|      - | 4978 | `					/* dropped */` |
|    ! 0 | 4979 | `				}else{` |
|    ! 0 | 4980 | `					META_PHONIZE('K');` |
|    ! 0 | 4981 | `				}` |
|      7 | 4982 | `			}else if( META_MAKESOFT(cNext) && cPrev != 'G' ){` |
|      3 | 4983 | `				META_PHONIZE('J');` |
|      2 | 4984 | `			}else{` |
|      5 | 4985 | `				META_PHONIZE('K');` |
|      - | 4986 | `			}` |
|     15 | 4987 | `			break;` |
|      - | 4988 | `		}` |
|      3 | 4989 | `		case 'H':` |
|      - | 4990 | `			/* H before a vowel and not after C, G, P, S, T */` |
|      7 | 4991 | `			if( META_ISVOWEL(META_NEXT) && !META_AFFECTH(cPrev) ){` |
|      3 | 4992 | `				META_PHONIZE('H');` |
|      1 | 4993 | `			}` |
|      7 | 4994 | `			break;` |
|      2 | 4995 | `		case 'K':` |
|      - | 4996 | `			/* dropped after C; else K */` |
|      5 | 4997 | `			if( cPrev != 'C' ){` |
|      3 | 4998 | `				META_PHONIZE('K');` |
|      1 | 4999 | `			}` |
|      5 | 5000 | `			break;` |
|      6 | 5001 | `		case 'P':` |
|      - | 5002 | `			/* F before H; else P */` |
|     14 | 5003 | `			if( META_NEXT == 'H' ){` |
|      3 | 5004 | `				META_PHONIZE('F');` |
|      2 | 5005 | `			}else{` |
|     12 | 5006 | `				META_PHONIZE('P');` |
|      - | 5007 | `			}` |
|     14 | 5008 | `			break;` |
|      2 | 5009 | `		case 'Q':` |
|      5 | 5010 | `			META_PHONIZE('K');` |
|      5 | 5011 | `			break;` |
|     14 | 5012 | `		case 'S':{` |
|      - | 5013 | `			/* 'sh' in -SH-, -SIO-, -SIA-; else S */` |
|     30 | 5014 | `			char cNext = META_NEXT;` |
|      - | 5015 | `			char cAfterNext;` |
|     30 | 5016 | `			if( cNext == 'I' &&` |
|      2 | 5017 | `			    ((cAfterNext = META_AFTERNEXT) == 'O' \|\| cAfterNext == 'A') ){` |
|      3 | 5018 | `				META_PHONIZE(META_SH);` |
|     28 | 5019 | `			}else if( cNext == 'H' ){` |
|    ! 0 | 5020 | `				META_PHONIZE(META_SH);` |
|    ! 0 | 5021 | `				nSkip++;` |
|    ! 0 | 5022 | `			}else{` |
|     28 | 5023 | `				META_PHONIZE('S');` |
|      - | 5024 | `			}` |
|     30 | 5025 | `			break;` |
|      - | 5026 | `		}` |
|     17 | 5027 | `		case 'T':{` |
|      - | 5028 | `			/* 'sh' in -TIA-, -TIO-; 'th' before H; dropped in -TCH-; else T */` |
|     36 | 5029 | `			char cNext = META_NEXT;` |
|      - | 5030 | `			char cAfterNext;` |
|     36 | 5031 | `			if( cNext == 'I' &&` |
|      4 | 5032 | `			    ((cAfterNext = META_AFTERNEXT) == 'O' \|\| cAfterNext == 'A') ){` |
|      5 | 5033 | `				META_PHONIZE(META_SH);` |
|     33 | 5034 | `			}else if( cNext == 'H' ){` |
|     12 | 5035 | `				META_PHONIZE(META_TH);` |
|     12 | 5036 | `				nSkip++;` |
|     26 | 5037 | `			}else if( !(cNext == 'C' && META_AFTERNEXT == 'H') ){` |
|     17 | 5038 | `				META_PHONIZE('T');` |
|      8 | 5039 | `			}` |
|     36 | 5040 | `			break;` |
|      - | 5041 | `		}` |
|      2 | 5042 | `		case 'V':` |
|      5 | 5043 | `			META_PHONIZE('F');` |
|      5 | 5044 | `			break;` |
|      1 | 5045 | `		case 'W':` |
|      - | 5046 | `			/* W before a vowel, else dropped */` |
|      3 | 5047 | `			if( META_ISVOWEL(META_NEXT) ){` |
|    ! 0 | 5048 | `				META_PHONIZE('W');` |
|    ! 0 | 5049 | `			}` |
|      3 | 5050 | `			break;` |
|      2 | 5051 | `		case 'X':` |
|      6 | 5052 | `			META_PHONIZE('K');` |
|      6 | 5053 | `			META_PHONIZE('S');` |
|      6 | 5054 | `			break;` |
|      7 | 5055 | `		case 'Y':` |
|      - | 5056 | `			/* Y before a vowel, else dropped */` |
|     15 | 5057 | `			if( META_ISVOWEL(META_NEXT) ){` |
|      3 | 5058 | `				META_PHONIZE('Y');` |
|      1 | 5059 | `			}` |
|     15 | 5060 | `			break;` |
|      1 | 5061 | `		case 'Z':` |
|      3 | 5062 | `			META_PHONIZE('S');` |
|      3 | 5063 | `			break;` |
|     33 | 5064 | `		case 'F':` |
|      - | 5065 | `		case 'J':` |
|      - | 5066 | `		case 'L':` |
|      - | 5067 | `		case 'M':` |
|      - | 5068 | `		case 'N':` |
|      - | 5069 | `		case 'R':` |
|      - | 5070 | `			/* passed through unchanged */` |
|     68 | 5071 | `			META_PHONIZE(cCurr);` |
|     66 | 5072 | `			break;` |
|     66 | 5073 | `		default:` |
|    132 | 5074 | `			break;` |
|      - | 5075 | `		}` |
|    378 | 5076 | `		w_idx += nSkip;` |
|    190 | 5077 | `	}` |
|     76 | 5078 | `	ph7_result_string(pCtx,zOut,nOut);` |
|     76 | 5079 | `	return PH7_OK;` |
|      - | 5080 | `#undef META_BYTE` |
|      - | 5081 | `#undef META_NEXT` |
|      - | 5082 | `#undef META_PREV` |
|      - | 5083 | `#undef META_BACK` |
|      - | 5084 | `#undef META_AFTERNEXT` |
|      - | 5085 | `#undef META_PHONIZE` |
|     42 | 5086 | `}` |
|      - | 5087 | `/*` |
|      - | 5088 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 5089 | ` *  Wraps a string to a given number of characters.` |
|      - | 5090 | ` * Parameters` |
|      - | 5091 | ` *  $str` |
|      - | 5092 | ` *   The input string.` |
|      - | 5093 | ` * $width` |
|      - | 5094 | ` *  The column width.` |
|      - | 5095 | ` * $break` |
|      - | 5096 | ` *  The line is broken using the optional break parameter.` |
|      - | 5097 | ` * Return` |
|      - | 5098 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 5099 | ` */` |
|     28 | 5100 | `PH7_PRIVATE int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5101 | `{` |
|      - | 5102 | `	const char *zIn,*zBreak;` |
|      - | 5103 | `	SyBlob sWorker;` |
|      - | 5104 | `	int iLen,iBreaklen,iWidth,iCut,iStart,iSpace,iCur;` |
|      - | 5105 | `	sxi32 rc;` |
|     30 | 5106 | `	if( nArg < 1 ){` |
|      - | 5107 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5108 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5109 | `		return PH7_OK;` |
|      - | 5110 | `	}` |
|      - | 5111 | `	/* Extract the input string */` |
|     30 | 5112 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5113 | `	/* Width (default 75; PHP allows 0/negative — break at every space). */` |
|     30 | 5114 | `	iWidth = 75;` |
|     30 | 5115 | `	if( nArg > 1 ){` |
|     27 | 5116 | `		iWidth = ph7_value_to_int(apArg[1]);` |
|     13 | 5117 | `	}` |
|      - | 5118 | `	/* Break string (default "\n"). */` |
|     30 | 5119 | `	zBreak = "\n";` |
|     30 | 5120 | `	iBreaklen = (int)sizeof(char);` |
|     30 | 5121 | `	if( nArg > 2 ){` |
|     13 | 5122 | `		zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      6 | 5123 | `	}` |
|      - | 5124 | `	/* Cut long words? (default false). */` |
|     30 | 5125 | `	iCut = 0;` |
|     30 | 5126 | `	if( nArg > 3 ){` |
|      7 | 5127 | `		iCut = ph7_value_to_bool(apArg[3]);` |
|      3 | 5128 | `	}` |
|     30 | 5129 | `	if( iLen < 1 ){` |
|      - | 5130 | `		/* PHP returns the empty string for empty input before validating the other args. */` |
|      8 | 5131 | `		ph7_result_string(pCtx,"",0);` |
|      8 | 5132 | `		return PH7_OK;` |
|      - | 5133 | `	}` |
|      - | 5134 | `	/* PHP 8 domain errors (catchable ValueError). */` |
|     23 | 5135 | `	if( iBreaklen < 1 ){` |
|      3 | 5136 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5137 | `			"wordwrap(): Argument #3 ($break) must not be empty");` |
|      - | 5138 | `	}` |
|     21 | 5139 | `	if( iWidth == 0 && iCut ){` |
|      3 | 5140 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5141 | `			"wordwrap(): Argument #4 ($cut_long_words) cannot be true when argument #2 ($width) is 0");` |
|      - | 5142 | `	}` |
|      - | 5143 | `	/*` |
|      - | 5144 | `	 * PHP's algorithm: a single left-to-right pass tracking the start of the` |
|      - | 5145 | `	 * current line (iStart) and the position of the last space seen on it` |
|      - | 5146 | `	 * (iSpace). A break is emitted when the line reaches the width, at the last` |
|      - | 5147 | `	 * space if there was one, otherwise (only when cut is enabled) hard at the` |
|      - | 5148 | `	 * boundary. An existing break sequence in the input resets the line.` |
|      - | 5149 | `	 */` |
|     19 | 5150 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     19 | 5151 | `	iStart = iSpace = iCur = 0;` |
|     19 | 5152 | `	rc = SXRET_OK;` |
|    551 | 5153 | `	while( iCur < iLen ){` |
|    533 | 5154 | `		if( iBreaklen <= iLen - iCur && SyMemcmp(&zIn[iCur],zBreak,(sxu32)iBreaklen) == 0 ){` |
|      - | 5155 | `			/* Existing break sequence in the input: copy it verbatim and reset the line. */` |
|    ! 0 | 5156 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart + iBreaklen));` |
|    ! 0 | 5157 | `			if( rc != SXRET_OK ){ goto oom; }` |
|    ! 0 | 5158 | `			iCur += iBreaklen;` |
|    ! 0 | 5159 | `			iStart = iSpace = iCur;` |
|    ! 0 | 5160 | `			continue;` |
|    533 | 5161 | `		}else if( zIn[iCur] == ' ' ){` |
|     67 | 5162 | `			if( iCur - iStart >= iWidth ){` |
|      - | 5163 | `				/* The line already fills the width at this space: break here (the space is consumed). */` |
|     13 | 5164 | `				rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     13 | 5165 | `				if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     13 | 5166 | `				if( rc != SXRET_OK ){ goto oom; }` |
|     13 | 5167 | `				iStart = iCur + 1;` |
|      6 | 5168 | `			}` |
|     67 | 5169 | `			iSpace = iCur;` |
|    500 | 5170 | `		}else if( iCut && iCur - iStart >= iWidth && iStart >= iSpace ){` |
|      - | 5171 | `			/* A word longer than the width with no space to break at: hard-cut at the boundary. */` |
|      7 | 5172 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|      7 | 5173 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|      7 | 5174 | `			if( rc != SXRET_OK ){ goto oom; }` |
|      7 | 5175 | `			iStart = iSpace = iCur;` |
|    464 | 5176 | `		}else if( iCur - iStart >= iWidth && iStart < iSpace ){` |
|      - | 5177 | `			/* Past the width mid-word: wrap back to the last space (which is consumed). */` |
|     17 | 5178 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iSpace - iStart));` |
|     17 | 5179 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     17 | 5180 | `			if( rc != SXRET_OK ){ goto oom; }` |
|     17 | 5181 | `			iStart = iSpace = iSpace + 1;` |
|      8 | 5182 | `		}` |
|    533 | 5183 | `		iCur++;` |
|      1 | 5184 | `	}` |
|      - | 5185 | `	/* Emit the trailing chunk. */` |
|     19 | 5186 | `	if( iStart < iCur ){` |
|     19 | 5187 | `		rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     19 | 5188 | `		if( rc != SXRET_OK ){ goto oom; }` |
|      9 | 5189 | `	}` |
|     19 | 5190 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|     19 | 5191 | `	SyBlobRelease(&sWorker);` |
|     19 | 5192 | `	return PH7_OK;` |
|    ! 0 | 5193 | `oom:` |
|    ! 0 | 5194 | `	SyBlobRelease(&sWorker);` |
|    ! 0 | 5195 | `	return PH7_ContextMemoryError(pCtx);` |
|     16 | 5196 | `}` |
|      - | 5197 | `/*` |
|      - | 5198 | ` * Check if the given character is a member of the given mask.` |
|      - | 5199 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 5200 | ` * Refer to [strtok()].` |
|      - | 5201 | ` */` |
|    790 | 5202 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 5203 | `{` |
|      - | 5204 | `	int i;` |
|   1571 | 5205 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|    815 | 5206 | `		if( c == zMask[i] ){` |
|     35 | 5207 | `			if( pOfft ){` |
|     19 | 5208 | `				*pOfft = i;` |
|      9 | 5209 | `			}` |
|     35 | 5210 | `			return TRUE;` |
|      - | 5211 | `		}` |
|    391 | 5212 | `	}` |
|    757 | 5213 | `	return FALSE;` |
|    396 | 5214 | `}` |
|      - | 5215 | `/*` |
|      - | 5216 | ` * Extract a single token from the input stream.` |
|      - | 5217 | ` * Refer to [strtok()].` |
|      - | 5218 | ` */` |
|     14 | 5219 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 5220 | `{` |
|     15 | 5221 | `	const char *zIn = *pzIn;` |
|      - | 5222 | `	const char *zPtr;` |
|      - | 5223 | `	/* Ignore leading delimiter */` |
|     19 | 5224 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 5225 | `		zIn++;` |
|      1 | 5226 | `	}` |
|     15 | 5227 | `	if( zIn >= zEnd ){` |
|      - | 5228 | `		/* End of input */` |
|    ! 0 | 5229 | `		return SXERR_EOF;` |
|      - | 5230 | `	}` |
|     15 | 5231 | `	zPtr = zIn;` |
|      - | 5232 | `	/* Extract the token */` |
|    633 | 5233 | `	while( zIn < zEnd ){` |
|    631 | 5234 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 5235 | `			/* UTF-8 stream */` |
|    ! 0 | 5236 | `			zIn++;` |
|    ! 0 | 5237 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 5238 | `		}else{` |
|    631 | 5239 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|     13 | 5240 | `				break;` |
|      - | 5241 | `			}` |
|    619 | 5242 | `			zIn++;` |
|      - | 5243 | `		}` |
|      1 | 5244 | `	}` |
|     15 | 5245 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 5246 | `	/* Update the cursor */` |
|     15 | 5247 | `	*pzIn = zIn;` |
|      - | 5248 | `	/* Return to the caller */` |
|     15 | 5249 | `	return SXRET_OK;` |
|      8 | 5250 | `}` |
|      - | 5251 | `/* strtok auxiliary private data */` |
|      - | 5252 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 5253 | `struct strtok_aux_data` |
|      - | 5254 | `{` |
|      - | 5255 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 5256 | `	const char *zIn;   /* Current input stream */` |
|      - | 5257 | `	const char *zEnd;  /* End of input */` |
|      - | 5258 | `};` |
|      - | 5259 | `/*` |
|      - | 5260 | ` * string strtok(string $str,string $token)` |
|      - | 5261 | ` * string strtok(string $token)` |
|      - | 5262 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 5263 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 5264 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 5265 | ` *  words by using the space character as the token.` |
|      - | 5266 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 5267 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 5268 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 5269 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 5270 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 5271 | ` *  the argument are found.` |
|      - | 5272 | ` * Parameters` |
|      - | 5273 | ` *  $str` |
|      - | 5274 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 5275 | ` * $token` |
|      - | 5276 | ` *  The delimiter used when splitting up str.` |
|      - | 5277 | ` * Return` |
|      - | 5278 | ` *   Current token or FALSE on EOF.` |
|      - | 5279 | ` */` |
|     14 | 5280 | `PH7_PRIVATE int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5281 | `{` |
|      - | 5282 | `	strtok_aux_data *pAux;` |
|      - | 5283 | `	const char *zMask;` |
|      - | 5284 | `	SyString sToken;` |
|      - | 5285 | `	int nMasklen;` |
|      - | 5286 | `	sxi32 rc;` |
|     15 | 5287 | `	if( nArg < 2 ){` |
|      - | 5288 | `		/* Extract top aux data */` |
|      5 | 5289 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      5 | 5290 | `		if( pAux == 0 ){` |
|      - | 5291 | `			/* No aux data,return FALSE */` |
|    ! 0 | 5292 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5293 | `			return PH7_OK;` |
|      - | 5294 | `		}` |
|      5 | 5295 | `		nMasklen = 0;` |
|      5 | 5296 | `		zMask = ""; /* cc warning */` |
|      5 | 5297 | `		if( nArg > 0 ){` |
|      - | 5298 | `			/* Extract the mask */` |
|      5 | 5299 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 5300 | `		}` |
|      5 | 5301 | `		if( nMasklen < 1 ){` |
|      - | 5302 | `			/* Invalid mask,return FALSE */` |
|    ! 0 | 5303 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 5304 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5305 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 5306 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5307 | `			return PH7_OK;` |
|      - | 5308 | `		}` |
|      - | 5309 | `		/* Extract the token */` |
|      5 | 5310 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 5311 | `		if( rc != SXRET_OK ){` |
|      - | 5312 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 5313 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 5314 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5315 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 5316 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5317 | `		}else{` |
|      - | 5318 | `			/* Return the extracted token */` |
|      5 | 5319 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5320 | `		}` |
|      3 | 5321 | `	}else{` |
|      - | 5322 | `		const char *zInput,*zCur;` |
|      - | 5323 | `		char *zDup;` |
|      - | 5324 | `		int nLen;` |
|      - | 5325 | `		/* Extract the raw input */` |
|     11 | 5326 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5327 | `		if( nLen < 1 ){` |
|      - | 5328 | `			/* Empty input,return FALSE */` |
|    ! 0 | 5329 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5330 | `			return PH7_OK;` |
|      - | 5331 | `		}` |
|      - | 5332 | `		/* Extract the mask */` |
|     11 | 5333 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|     11 | 5334 | `		if( nMasklen < 1 ){` |
|      - | 5335 | `			/* Set a default mask */` |
|      - | 5336 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 5337 | `			zMask = TOK_MASK;` |
|    ! 0 | 5338 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 5339 | `#undef TOK_MASK` |
|    ! 0 | 5340 | `		}` |
|      - | 5341 | `		/* Extract a single token */` |
|     11 | 5342 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|     11 | 5343 | `		if( rc != SXRET_OK ){` |
|      - | 5344 | `			/* Empty input */` |
|    ! 0 | 5345 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 5346 | `			return PH7_OK;` |
|    ! 0 | 5347 | `		}else{` |
|      - | 5348 | `			/* Return the extracted token */` |
|     11 | 5349 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 5350 | `		}` |
|      - | 5351 | `		/* Create our auxilliary data and copy the input */` |
|     11 | 5352 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|     11 | 5353 | `		if( pAux ){` |
|     11 | 5354 | `			nLen -= (int)(zInput-zCur);` |
|     11 | 5355 | `			if( nLen < 1 ){` |
|    ! 0 | 5356 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 5357 | `				return PH7_OK;` |
|      - | 5358 | `			}` |
|      - | 5359 | `			/* Duplicate input */` |
|     11 | 5360 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|     11 | 5361 | `			if( zDup  ){` |
|     11 | 5362 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 5363 | `				/* Register the aux data */` |
|     11 | 5364 | `				pAux->zDup = pAux->zIn = zDup;` |
|     11 | 5365 | `				pAux->zEnd = &zDup[nLen];` |
|     11 | 5366 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      5 | 5367 | `			}` |
|      5 | 5368 | `		}` |
|      - | 5369 | `	}` |
|     15 | 5370 | `	return PH7_OK;` |
|      8 | 5371 | `}` |
|      - | 5372 | `/*` |
|      - | 5373 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 5374 | ` *  Pad a string to a certain length with another string` |
|      - | 5375 | ` * Parameters` |
|      - | 5376 | ` *  $input` |
|      - | 5377 | ` *   The input string.` |
|      - | 5378 | ` * $pad_length` |
|      - | 5379 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 5380 | ` *   string, no padding takes place.` |
|      - | 5381 | ` * $pad_string` |
|      - | 5382 | ` *   Note:` |
|      - | 5383 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 5384 | ` *    divided by the pad_string's length.` |
|      - | 5385 | ` * $pad_type` |
|      - | 5386 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 5387 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 5388 | ` * Return` |
|      - | 5389 | ` *  The padded string.` |
|      - | 5390 | ` */` |
|    874 | 5391 | `PH7_PRIVATE int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 5392 | `{` |
|      - | 5393 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 5394 | `	const char *zIn,*zPad;` |
|    878 | 5395 | `	if( nArg < 2 ){` |
|      - | 5396 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5397 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5398 | `		return PH7_OK;` |
|      - | 5399 | `	}` |
|      - | 5400 | `	/* Extract the target string */` |
|    878 | 5401 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 5402 | `	/* Padding length */` |
|      - | 5403 | `	{` |
|    878 | 5404 | `		sxi64 iTmp = 0;` |
|    878 | 5405 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_pad",2,"$length","int",&iTmp);` |
|    878 | 5406 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 5407 | `			return rcArg;` |
|      - | 5408 | `		}` |
|    878 | 5409 | `		iRealPad = iPadlen = (int)iTmp;` |
|      - | 5410 | `	}` |
|    878 | 5411 | `	if( iPadlen > 0 ){` |
|    876 | 5412 | `		iPadlen -= iLen;` |
|    436 | 5413 | `	}` |
|    878 | 5414 | `	if( iPadlen < 1  ){` |
|      - | 5415 | `		/* Return the string verbatim */` |
|      7 | 5416 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      7 | 5417 | `		return PH7_OK;` |
|      - | 5418 | `	}` |
|    872 | 5419 | `	zPad = " "; /* Whitespace padding */` |
|    872 | 5420 | `	iStrpad = (int)sizeof(char);` |
|    872 | 5421 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|    872 | 5422 | `	if( nArg > 2 ){` |
|      - | 5423 | `		/* Padding string */` |
|     25 | 5424 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|     25 | 5425 | `		if( iStrpad < 1 ){` |
|      - | 5426 | `			/* An empty pad string throws a catchable ValueError in PHP 8` |
|      - | 5427 | `			 * (only reached once padding is actually required). */` |
|      3 | 5428 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5429 | `				"str_pad(): Argument #3 ($pad_string) must not be empty");` |
|      - | 5430 | `		}` |
|     23 | 5431 | `		if( nArg > 3 ){` |
|      - | 5432 | `			/* Padd type. php 8: anything outside LEFT(0)/RIGHT(1)/BOTH(2) is a` |
|      - | 5433 | `			 * catchable ValueError (PHL used to fall back to RIGHT silently);` |
|      - | 5434 | `			 * like the empty-pad check above, php only reaches it once padding` |
|      - | 5435 | `			 * is actually required (probed: str_pad("abc",2," ",9) is "abc"). */` |
|     19 | 5436 | `			iType = ph7_value_to_int(apArg[3]);` |
|     19 | 5437 | `			if( iType < 0 \|\| iType > 2 ){` |
|      5 | 5438 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5439 | `					"str_pad(): Argument #4 ($pad_type) must be STR_PAD_LEFT, STR_PAD_RIGHT, or STR_PAD_BOTH");` |
|      - | 5440 | `			}` |
|      7 | 5441 | `		}` |
|      9 | 5442 | `	}` |
|    866 | 5443 | `	iDiv = 1;` |
|    866 | 5444 | `	if( iType == 2 ){` |
|      3 | 5445 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|      1 | 5446 | `	}` |
|      - | 5447 | `	/* Perform the requested operation */` |
|    866 | 5448 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|     11 | 5449 | `		jPad = iStrpad;` |
|     29 | 5450 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 5451 | `			/* Padding */` |
|     27 | 5452 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      9 | 5453 | `				break;` |
|      - | 5454 | `			}` |
|     19 | 5455 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|     10 | 5456 | `		}` |
|     11 | 5457 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|     17 | 5458 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      9 | 5459 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      9 | 5460 | `				if( jPad > iStrpad ){` |
|    ! 0 | 5461 | `					jPad = iStrpad;` |
|    ! 0 | 5462 | `				}` |
|      9 | 5463 | `				if( jPad < 1){` |
|    ! 0 | 5464 | `					break;` |
|      - | 5465 | `				}` |
|      9 | 5466 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 5467 | `			}` |
|      4 | 5468 | `		}` |
|      5 | 5469 | `	}` |
|    866 | 5470 | `	if( iLen > 0 ){` |
|      - | 5471 | `		/* Append the input string */` |
|    866 | 5472 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|    431 | 5473 | `	}` |
|    866 | 5474 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|   8122 | 5475 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 5476 | `			/* Padding */` |
|   8120 | 5477 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|    856 | 5478 | `				break;` |
|      - | 5479 | `			}` |
|   7268 | 5480 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|   3636 | 5481 | `		}` |
|   1712 | 5482 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|    858 | 5483 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|    858 | 5484 | `			if( jPad > iStrpad ){` |
|    ! 0 | 5485 | `				jPad = iStrpad;` |
|    ! 0 | 5486 | `			}` |
|    858 | 5487 | `			if( jPad < 1){` |
|    ! 0 | 5488 | `				break;` |
|      - | 5489 | `			}` |
|    858 | 5490 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      4 | 5491 | `		}` |
|    427 | 5492 | `	}` |
|    866 | 5493 | `	return PH7_OK;` |
|    441 | 5494 | `}` |
|      - | 5495 | `/*` |
|      - | 5496 | ` * String replacement private data.` |
|      - | 5497 | ` */` |
|      - | 5498 | `typedef struct str_replace_data str_replace_data;` |
|      - | 5499 | `struct str_replace_data` |
|      - | 5500 | `{` |
|      - | 5501 | `	/* Used by the str_replace family to collect the search/replace arguments. */` |
|      - | 5502 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 5503 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 5504 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 5505 | `};` |
|      - | 5506 | `/*` |
|      - | 5507 | ` * Remove a substring.` |
|      - | 5508 | ` */` |
|      - | 5509 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 5510 | `	for(;;){\` |
|      - | 5511 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 5512 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 5513 | `		++OFFT;\` |
|      - | 5514 | `	}\` |
|      - | 5515 | `}` |
|      - | 5516 | `/*` |
|      - | 5517 | ` * Shift right and insert algorithm.` |
|      - | 5518 | ` */` |
|      - | 5519 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 5520 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 5521 | `		for(;;){\` |
|      - | 5522 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 5523 | `			if(INLEN < 1 ) { break; }\` |
|      - | 5524 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 5525 | `			--INLEN; \` |
|      - | 5526 | `		}\` |
|      - | 5527 | `		for(;;){\` |
|      - | 5528 | `				if(ELEN < 1) { break; }\` |
|      - | 5529 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 5530 | `				OFFT++;\` |
|      - | 5531 | `				ENTRY++;\` |
|      - | 5532 | `				--ELEN;\` |
|      - | 5533 | `		}\` |
|      - | 5534 | `}` |
|      - | 5535 | `/*` |
|      - | 5536 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 5537 | ` * replacement string [i.e: zReplace].` |
|      - | 5538 | ` */` |
|   3142 | 5539 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      5 | 5540 | `{` |
|   3147 | 5541 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 5542 | `	sxu32 n,m;` |
|   3147 | 5543 | `	n = SyBlobLength(pWorker);` |
|   3147 | 5544 | `	m = nOfft;` |
|      - | 5545 | `	/* Delete the old entry */` |
| 134114 | 5546 | `	STRDEL(zInput,n,m,nLen);` |
|   3147 | 5547 | `	SyBlobLength(pWorker) -= nLen;` |
|   3147 | 5548 | `	if( nReplen > 0 ){` |
|    735 | 5549 | `		sxi32 iRep = nReplen;` |
|      - | 5550 | `		sxi32 rc;` |
|      - | 5551 | `		/*` |
|      - | 5552 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 5553 | `		 * string.` |
|      - | 5554 | `		 */` |
|    735 | 5555 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|    735 | 5556 | `		if( rc != SXRET_OK ){` |
|      - | 5557 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 5558 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 5559 | `			return rc;` |
|      - | 5560 | `		}` |
|      - | 5561 | `		/* Perform the insertion now */` |
|    735 | 5562 | `		zInput = (char *)SyBlobData(pWorker);` |
|    735 | 5563 | `		n = SyBlobLength(pWorker);` |
|  35944 | 5564 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|    735 | 5565 | `		SyBlobLength(pWorker) += nReplen;` |
|    365 | 5566 | `	}` |
|   3147 | 5567 | `	return SXRET_OK;` |
|   1576 | 5568 | `}` |
|      - | 5569 | `/*` |
|      - | 5570 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 5571 | ` * to collect search/replace string.` |
|      - | 5572 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 5573 | ` */` |
|    976 | 5574 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      5 | 5575 | `{` |
|    981 | 5576 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 5577 | `	SyString sWorker;` |
|      - | 5578 | `	const char *zIn;` |
|      - | 5579 | `	int nByte;` |
|      - | 5580 | `	/* Extract a string representation of the given argument */` |
|    981 | 5581 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|    981 | 5582 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|    981 | 5583 | `	if( nByte > 0 ){` |
|      - | 5584 | `		char *zDup;` |
|      - | 5585 | `		/* Duplicate the chunk */` |
|    839 | 5586 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 5587 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 5588 | `			);` |
|    839 | 5589 | `		if( zDup == 0 ){` |
|      - | 5590 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 5591 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 5592 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 5593 | `			return SXERR_MEM;` |
|      - | 5594 | `		}` |
|    839 | 5595 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 5596 | `		/* Save the chunk */` |
|    839 | 5597 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|    417 | 5598 | `	}` |
|      - | 5599 | `	/* Save for later processing */` |
|    981 | 5600 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 5601 | `	/* All done */` |
|    488 | 5602 | `	SXUNUSED(pKey); /* cc warning */` |
|    981 | 5603 | `	return PH7_OK;` |
|    493 | 5604 | `}` |
|      - | 5605 | `/*` |
|      - | 5606 | ` * Run the collected search/replace pairs over a single subject string, writing` |
|      - | 5607 | ` * the transformed bytes into pOut (reset here). Shared by the scalar-subject and` |
|      - | 5608 | ` * the array-subject (element-wise) paths. The search/replace SySets are walked` |
|      - | 5609 | ` * fresh on every call — cursors are reset here — so each array element is` |
|      - | 5610 | ` * transformed independently, exactly like php. Returns SXRET_OK, or SXERR_MEM` |
|      - | 5611 | ` * on an allocation failure inside StringReplace.` |
|      - | 5612 | ` *` |
|      - | 5613 | ` * *pnCount is INCREMENTED (never reset) by the number of replacements performed,` |
|      - | 5614 | ` * so an array subject accumulates across its elements exactly like php's &$count.` |
|      - | 5615 | ` */` |
|  52662 | 5616 | `static sxi32 StrReplaceOneSubject(` |
|      - | 5617 | `	SyBlob *pOut,             /* Output buffer (reset then filled here) */` |
|      - | 5618 | `	const char *zSubject,     /* Subject bytes */` |
|      - | 5619 | `	sxu32 nSubject,           /* Subject length */` |
|      - | 5620 | `	SySet *pSearch,           /* Collected search terms */` |
|      - | 5621 | `	SySet *pReplace,          /* Collected replacement terms */` |
|      - | 5622 | `	int rep_str,              /* TRUE: a single replacement reused for every search */` |
|      - | 5623 | `	ProcStringMatch xMatch,   /* SyBlobSearch (str_replace) / iPatternMatch (str_ireplace) */` |
|      - | 5624 | `	sxi64 *pnCount            /* Running replacement count (incremented here) */` |
|      - | 5625 | `	)` |
|      5 | 5626 | `{` |
|      - | 5627 | `	SyString *pSearch_,*pReplace_,sEmpty;` |
|      - | 5628 | `	sxi32 rc;` |
|  52667 | 5629 | `	SyBlobReset(pOut);` |
|  52667 | 5630 | `	if( nSubject > 0 ){` |
|  37515 | 5631 | `		rc = SyBlobAppend(pOut,(const void *)zSubject,nSubject);` |
|  37515 | 5632 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 5633 | `			return rc;` |
|      - | 5634 | `		}` |
|  18755 | 5635 | `	}` |
|  52667 | 5636 | `	SyStringInitFromBuf(&sEmpty,"",0);` |
|  52667 | 5637 | `	SySetResetCursor(pSearch);` |
|  52667 | 5638 | `	SySetResetCursor(pReplace);` |
|  52667 | 5639 | `	pSearch_ = pReplace_ = 0; /* cc warning */` |
| 105629 | 5640 | `	while( SXRET_OK == SySetGetNextEntry(pSearch,(void **)&pSearch_) ){` |
|      - | 5641 | `		sxu32 nCount,nOfft;` |
|  52967 | 5642 | `		if( rep_str ){` |
|      - | 5643 | `			/* Single replacement string reused for every search term */` |
|  52569 | 5644 | `			pReplace_ = (SyString *)SySetPeek(pReplace);` |
|  26681 | 5645 | `		}else if( SXRET_OK != SySetGetNextEntry(pReplace,(void **)&pReplace_) ){` |
|      - | 5646 | `			/* 'replace set' has fewer values than the search set: an empty` |
|      - | 5647 | `			 * string is used for the rest of the replacement values. */` |
|      5 | 5648 | `			pReplace_ = 0;` |
|      2 | 5649 | `		}` |
|  52967 | 5650 | `		if( pReplace_ == 0 ){` |
|      5 | 5651 | `			pReplace_ = &sEmpty;` |
|      2 | 5652 | `		}` |
|  52967 | 5653 | `		if( pSearch_->nByte < 1 ){` |
|      - | 5654 | `			/* php ignores an empty search string, but it still CONSUMED a replace` |
|      - | 5655 | `			 * slot above so the remaining pairs stay aligned. */` |
|     15 | 5656 | `			continue;` |
|      - | 5657 | `		}` |
|  52953 | 5658 | `		nOfft = nCount = 0;` |
|  28045 | 5659 | `		for(;;){` |
|  56095 | 5660 | `			if( nCount >= SyBlobLength(pOut) ){` |
|  15239 | 5661 | `				break;` |
|      - | 5662 | `			}` |
|      - | 5663 | `			/* Perform a pattern lookup */` |
|  61289 | 5664 | `			rc = xMatch(SyBlobDataAt(pOut,nCount),SyBlobLength(pOut) - nCount,` |
|  40856 | 5665 | `				(const void *)pSearch_->zString,pSearch_->nByte,&nOfft);` |
|  40861 | 5666 | `			if( rc != SXRET_OK ){` |
|      - | 5667 | `				/* Pattern not found */` |
|  37719 | 5668 | `				break;` |
|      - | 5669 | `			}` |
|      - | 5670 | `			/* Perform the replace operation */` |
|   4718 | 5671 | `			rc = StringReplace(pOut,nCount+nOfft,(int)pSearch_->nByte,` |
|   3142 | 5672 | `				pReplace_->zString,(int)pReplace_->nByte);` |
|   3147 | 5673 | `			if( rc != SXRET_OK ){` |
|      - | 5674 | `				/* Propagate an allocation failure so the caller raises a fatal` |
|      - | 5675 | `				 * instead of returning a partially-replaced result. */` |
|    ! 0 | 5676 | `				return rc;` |
|      - | 5677 | `			}` |
|   3147 | 5678 | `			*pnCount += 1;` |
|      - | 5679 | `			/* Increment offset counter */` |
|   3147 | 5680 | `			nCount += nOfft + pReplace_->nByte;` |
|      5 | 5681 | `		}` |
|      5 | 5682 | `	}` |
|  52667 | 5683 | `	return SXRET_OK;` |
|  26336 | 5684 | `}` |
|      - | 5685 | `/* Per-call state for the array-subject form of str_replace()/str_ireplace(). */` |
|      - | 5686 | `typedef struct str_replace_subject str_replace_subject;` |
|      - | 5687 | `struct str_replace_subject` |
|      - | 5688 | `{` |
|      - | 5689 | `	ph7_value *pResult;    /* Result array (keys preserved) */` |
|      - | 5690 | `	ph7_value *pScratch;   /* Reusable string value for each element */` |
|      - | 5691 | `	SyBlob *pWorker;       /* Scratch output buffer for one element */` |
|      - | 5692 | `	SySet *pSearch;        /* Collected search terms */` |
|      - | 5693 | `	SySet *pReplace;       /* Collected replacement terms */` |
|      - | 5694 | `	ProcStringMatch xMatch;/* Match routine (case-sensitive or not) */` |
|      - | 5695 | `	int rep_str;           /* TRUE: scalar $replace */` |
|      - | 5696 | `	sxi64 nReplaced;       /* Replacements performed so far (&$count) */` |
|      - | 5697 | `	sxi32 rc;              /* SXRET_OK or SXERR_MEM */` |
|      - | 5698 | `};` |
|      - | 5699 | `/*` |
|      - | 5700 | ` * ph7_array_walk() callback over an array $subject: string-cast one element, run` |
|      - | 5701 | ` * the search/replace over it, and insert the result under the element's original` |
|      - | 5702 | ` * key. A non-string element is coerced exactly like php (int/float/bool/null via` |
|      - | 5703 | ` * their string form). A nested-array element becomes "Array" — the value matches` |
|      - | 5704 | ` * php, but PHL does not emit php's "Array to string conversion" warning here (the` |
|      - | 5705 | ` * engine raises it at echo/interpolation sites, not this C-level cast; a` |
|      - | 5706 | ` * recorded divergence).` |
|      - | 5707 | ` */` |
|     34 | 5708 | `static int StrReplaceSubjectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 5709 | `{` |
|     35 | 5710 | `	str_replace_subject *pS = (str_replace_subject *)pUserData;` |
|      - | 5711 | `	const char *zSub;` |
|      - | 5712 | `	int nSub;` |
|      - | 5713 | `	/* php coerces every element to string (same cast used everywhere). */` |
|     35 | 5714 | `	zSub = ph7_value_to_string(pData,&nSub);` |
|     34 | 5715 | `	if( StrReplaceOneSubject(pS->pWorker,zSub,(sxu32)(nSub > 0 ? nSub : 0),` |
|     35 | 5716 | `			pS->pSearch,pS->pReplace,pS->rep_str,pS->xMatch,&pS->nReplaced) != SXRET_OK ){` |
|    ! 0 | 5717 | `		pS->rc = SXERR_MEM;` |
|    ! 0 | 5718 | `		return SXERR_ABORT;` |
|      - | 5719 | `	}` |
|      - | 5720 | `	/* Publish the transformed bytes as a string under the original key. */` |
|     35 | 5721 | `	ph7_value_reset_string_cursor(pS->pScratch);` |
|     34 | 5722 | `	if( SyBlobLength(pS->pWorker) > 0` |
|     33 | 5723 | `	 && ph7_value_string(pS->pScratch,(const char *)SyBlobData(pS->pWorker),` |
|     45 | 5724 | `			(int)SyBlobLength(pS->pWorker)) != SXRET_OK ){` |
|    ! 0 | 5725 | `		pS->rc = SXERR_MEM;` |
|    ! 0 | 5726 | `		return SXERR_ABORT;` |
|      - | 5727 | `	}` |
|     35 | 5728 | `	if( ph7_array_add_elem(pS->pResult,pKey,pS->pScratch) != SXRET_OK ){` |
|    ! 0 | 5729 | `		pS->rc = SXERR_MEM;` |
|    ! 0 | 5730 | `		return SXERR_ABORT;` |
|      - | 5731 | `	}` |
|     35 | 5732 | `	return PH7_OK;` |
|     18 | 5733 | `}` |
|      - | 5734 | `/*` |
|      - | 5735 | ` * Write str_replace()/str_ireplace()'s optional by-reference &$count out-param.` |
|      - | 5736 | ` * The call compiler auto-vivifies argument #4 for these two names` |
|      - | 5737 | ` * (GenStateByRefBuiltinMask in compile.c), so an undefined variable, an array` |
|      - | 5738 | ` * element and a property all arrive with a real slot to write through.` |
|      - | 5739 | ` */` |
|  52640 | 5740 | `static void StrReplaceStoreCount(ph7_context *pCtx,int nArg,ph7_value **apArg,sxi64 nReplaced)` |
|      5 | 5741 | `{` |
|      - | 5742 | `	ph7_value sCount;` |
|  52645 | 5743 | `	if( nArg < 4 ){` |
|  52623 | 5744 | `		return;` |
|      - | 5745 | `	}` |
|     23 | 5746 | `	PH7_MemObjInitFromInt(pCtx->pVm,&sCount,nReplaced);` |
|     23 | 5747 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[3],&sCount);` |
|     23 | 5748 | `	PH7_MemObjRelease(&sCount);` |
|  26325 | 5749 | `}` |
|      - | 5750 | `/*` |
|      - | 5751 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 5752 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 5753 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 5754 | ` * Parameters` |
|      - | 5755 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 5756 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 5757 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 5758 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 5759 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 5760 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 5761 | ` * $search` |
|      - | 5762 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 5763 | ` *  to designate multiple needles.` |
|      - | 5764 | ` * $replace` |
|      - | 5765 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 5766 | ` *  to designate multiple replacements.` |
|      - | 5767 | ` * $subject` |
|      - | 5768 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 5769 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 5770 | ` *  of subject, and the return value is an array as well.` |
|      - | 5771 | ` * &$count` |
|      - | 5772 | ` *  If passed, this is set to the number of replacements performed — accumulated` |
|      - | 5773 | ` *  over every search term AND, for an array subject, over every element.` |
|      - | 5774 | ` * Return` |
|      - | 5775 | ` * This function returns a string or an array with the replaced values.` |
|      - | 5776 | ` */` |
|  52640 | 5777 | `PH7_PRIVATE int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 5778 | `{` |
|      - | 5779 | `	SyString sTemp;` |
|      - | 5780 | `	ProcStringMatch xMatch;` |
|      - | 5781 | `	const char *zIn,*zFunc;` |
|      - | 5782 | `	str_replace_data sRep;` |
|      - | 5783 | `	SyBlob sWorker;` |
|      - | 5784 | `	SySet sReplace;` |
|      - | 5785 | `	SySet sSearch;` |
|      - | 5786 | `	sxi64 nReplaced;` |
|      - | 5787 | `	int rep_str;` |
|      - | 5788 | `	int nByte;` |
|      - | 5789 | `	sxi32 rc;` |
|  52645 | 5790 | `	if( nArg < 3 ){` |
|      - | 5791 | `		/* Missing/Invalid arguments,return null */` |
|    ! 0 | 5792 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5793 | `		return PH7_OK;` |
|      - | 5794 | `	}` |
|      - | 5795 | `	/* Initialize fields */` |
|  52645 | 5796 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  52645 | 5797 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  52645 | 5798 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  52645 | 5799 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  52645 | 5800 | `	sRep.pCtx = pCtx;` |
|  52645 | 5801 | `	sRep.pCollector = &sSearch;` |
|  52645 | 5802 | `	rep_str = 0;` |
|  52645 | 5803 | `	nReplaced = 0;` |
|      - | 5804 | `	/* Collect the search term(s) — independent of the subject. */` |
|  52645 | 5805 | `	if( ph7_value_is_array(apArg[0]) ){` |
|    299 | 5806 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|    152 | 5807 | `	}else{` |
|  52351 | 5808 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  52351 | 5809 | `		SyStringInitFromBuf(&sTemp,zIn,nByte > 0 ? nByte : 0);` |
|  52351 | 5810 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 5811 | `	}` |
|      - | 5812 | `	/* Collect the replacement term(s). */` |
|  52645 | 5813 | `	if( ph7_value_is_array(apArg[1]) ){` |
|    195 | 5814 | `		sRep.pCollector = &sReplace;` |
|    195 | 5815 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|     98 | 5816 | `	}else{` |
|  52451 | 5817 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  52451 | 5818 | `		rep_str = 1;` |
|  52451 | 5819 | `		SyStringInitFromBuf(&sTemp,zIn,nByte > 0 ? nByte : 0);` |
|  52451 | 5820 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 5821 | `	}` |
|      - | 5822 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  52645 | 5823 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 5824 | `		SySetRelease(&sSearch);` |
|    ! 0 | 5825 | `		SySetRelease(&sReplace);` |
|    ! 0 | 5826 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 5827 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5828 | `	}` |
|      - | 5829 | `	/* Pick the match routine by function name */` |
|  52645 | 5830 | `	zFunc = ph7_function_name(pCtx);` |
|  52645 | 5831 | `	xMatch = SyBlobSearch;` |
|  52645 | 5832 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 5833 | `		/* Case insensitive pattern match */` |
|     17 | 5834 | `		xMatch = iPatternMatch;` |
|      8 | 5835 | `	}` |
|  52645 | 5836 | `	if( ph7_value_is_array(apArg[2]) ){` |
|      - | 5837 | `		/* Array subject: replace element-wise and RETURN AN ARRAY whose keys` |
|      - | 5838 | `		 * mirror the subject's (php semantics). */` |
|      - | 5839 | `		str_replace_subject sSub;` |
|      - | 5840 | `		ph7_value *pResult,*pScratch;` |
|     13 | 5841 | `		pResult = ph7_context_new_array(pCtx);` |
|     13 | 5842 | `		pScratch = ph7_context_new_scalar(pCtx);` |
|     13 | 5843 | `		if( pResult == 0 \|\| pScratch == 0 ){` |
|    ! 0 | 5844 | `			SySetRelease(&sSearch);` |
|    ! 0 | 5845 | `			SySetRelease(&sReplace);` |
|    ! 0 | 5846 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 5847 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 5848 | `		}` |
|     13 | 5849 | `		ph7_value_string(pScratch,"",0); /* force string representation */` |
|     13 | 5850 | `		SyZero(&sSub,sizeof(sSub));` |
|     13 | 5851 | `		sSub.pResult  = pResult;` |
|     13 | 5852 | `		sSub.pScratch = pScratch;` |
|     13 | 5853 | `		sSub.pWorker  = &sWorker;` |
|     13 | 5854 | `		sSub.pSearch  = &sSearch;` |
|     13 | 5855 | `		sSub.pReplace = &sReplace;` |
|     13 | 5856 | `		sSub.xMatch   = xMatch;` |
|     13 | 5857 | `		sSub.rep_str  = rep_str;` |
|     13 | 5858 | `		ph7_array_walk(apArg[2],StrReplaceSubjectWalker,&sSub);` |
|     13 | 5859 | `		SySetRelease(&sSearch);` |
|     13 | 5860 | `		SySetRelease(&sReplace);` |
|     13 | 5861 | `		SyBlobRelease(&sWorker);` |
|     13 | 5862 | `		if( sSub.rc != SXRET_OK ){` |
|    ! 0 | 5863 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 5864 | `		}` |
|     13 | 5865 | `		ph7_result_value(pCtx,pResult);` |
|     13 | 5866 | `		StrReplaceStoreCount(pCtx,nArg,apArg,sSub.nReplaced);` |
|     13 | 5867 | `		return PH7_OK;` |
|      - | 5868 | `	}` |
|      - | 5869 | `	/* Scalar subject: run once and return a string. An empty subject yields the` |
|      - | 5870 | `	 * empty string, and a lone empty search term leaves the subject untouched —` |
|      - | 5871 | `	 * both fall out of StrReplaceOneSubject's empty-term skip. */` |
|  52633 | 5872 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  52633 | 5873 | `	rc = StrReplaceOneSubject(&sWorker,zIn,(sxu32)(nByte > 0 ? nByte : 0),` |
|  26314 | 5874 | `		&sSearch,&sReplace,rep_str,xMatch,&nReplaced);` |
|  52633 | 5875 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 5876 | `		SySetRelease(&sSearch);` |
|    ! 0 | 5877 | `		SySetRelease(&sReplace);` |
|    ! 0 | 5878 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 5879 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5880 | `	}` |
|  52633 | 5881 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  52633 | 5882 | `	SySetRelease(&sSearch);` |
|  52633 | 5883 | `	SySetRelease(&sReplace);` |
|  52633 | 5884 | `	SyBlobRelease(&sWorker);` |
|  52633 | 5885 | `	if( rc != PH7_OK ){` |
|    ! 0 | 5886 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5887 | `	}` |
|  52633 | 5888 | `	StrReplaceStoreCount(pCtx,nArg,apArg,nReplaced);` |
|  52633 | 5889 | `	return PH7_OK;` |
|  26325 | 5890 | `}` |
|      - | 5891 | `/*` |
|      - | 5892 | ` * strtr() array form: a single (key,value) pair copied out of the replace_pairs` |
|      - | 5893 | ` * array. The bytes are owned by a persistent pool (see strtr_collect) rather than` |
|      - | 5894 | ` * the transient walker values, which HashmapWalk releases after each callback, so` |
|      - | 5895 | ` * we store byte offsets into that pool instead of raw pointers.` |
|      - | 5896 | ` */` |
|      - | 5897 | `typedef struct strtr_entry strtr_entry;` |
|      - | 5898 | `struct strtr_entry` |
|      - | 5899 | `{` |
|      - | 5900 | `	sxu32 nKeyOfft; /* Offset of the search key inside the pool */` |
|      - | 5901 | `	sxu32 nKeyLen;  /* Length of the search key */` |
|      - | 5902 | `	sxu32 nValOfft; /* Offset of the replacement inside the pool */` |
|      - | 5903 | `	sxu32 nValLen;  /* Length of the replacement */` |
|      - | 5904 | `};` |
|      - | 5905 | `typedef struct strtr_collect strtr_collect;` |
|      - | 5906 | `struct strtr_collect` |
|      - | 5907 | `{` |
|      - | 5908 | `	SyBlob *pPool;  /* Byte pool holding copied key + value bytes */` |
|      - | 5909 | `	SySet  *pTable; /* Set of strtr_entry (parallel offsets into pPool) */` |
|      - | 5910 | `	sxi32   rc;     /* Carries an allocation failure (SXERR_MEM) out of the walker */` |
|      - | 5911 | `	ph7_context *pCtx; /* Needed to warn about an empty key */` |
|      - | 5912 | `};` |
|      - | 5913 | `/*` |
|      - | 5914 | ` * Collect one replace_pairs entry into the persistent pool/offset table.` |
|      - | 5915 | ` * PHP coerces both the key and the value to string (an integer key becomes its` |
|      - | 5916 | ` * decimal form) and ignores an empty-string key.` |
|      - | 5917 | ` */` |
|     22 | 5918 | `static int StrtrCollectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 5919 | `{` |
|     23 | 5920 | `	strtr_collect *pCol = (strtr_collect *)pUserData;` |
|      - | 5921 | `	const char *zKey,*zVal;` |
|      - | 5922 | `	strtr_entry sEnt;` |
|      - | 5923 | `	int nKey,nVal;` |
|     23 | 5924 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     23 | 5925 | `	if( nKey < 1 ){` |
|      - | 5926 | `		/* PHP ignores an empty-string key, and warns that it did so. */` |
|      3 | 5927 | `		ph7_context_throw_error_format(pCol->pCtx,PH7_CTX_WARNING,` |
|      - | 5928 | `			"Ignoring replacement of empty string");` |
|      3 | 5929 | `		return PH7_OK;` |
|      - | 5930 | `	}` |
|     21 | 5931 | `	zVal = ph7_value_to_string(pData,&nVal);` |
|     21 | 5932 | `	sEnt.nKeyOfft = SyBlobLength(pCol->pPool);` |
|     21 | 5933 | `	sEnt.nKeyLen  = (sxu32)nKey;` |
|     21 | 5934 | `	if( SyBlobAppend(pCol->pPool,(const void *)zKey,(sxu32)nKey) != SXRET_OK ){` |
|    ! 0 | 5935 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 5936 | `		return SXERR_ABORT;` |
|      - | 5937 | `	}` |
|     21 | 5938 | `	sEnt.nValOfft = SyBlobLength(pCol->pPool);` |
|     21 | 5939 | `	sEnt.nValLen  = (sxu32)nVal;` |
|     21 | 5940 | `	if( nVal > 0 && SyBlobAppend(pCol->pPool,(const void *)zVal,(sxu32)nVal) != SXRET_OK ){` |
|    ! 0 | 5941 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 5942 | `		return SXERR_ABORT;` |
|      - | 5943 | `	}` |
|     21 | 5944 | `	if( SySetPut(pCol->pTable,(const void *)&sEnt) != SXRET_OK ){` |
|    ! 0 | 5945 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 5946 | `		return SXERR_ABORT;` |
|      - | 5947 | `	}` |
|     21 | 5948 | `	return PH7_OK;` |
|     12 | 5949 | `}` |
|      - | 5950 | `/*` |
|      - | 5951 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 5952 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 5953 | ` *  Translate characters or replace substrings.` |
|      - | 5954 | ` * Parameters` |
|      - | 5955 | ` *  $str` |
|      - | 5956 | ` *  The string being translated.` |
|      - | 5957 | ` * $from` |
|      - | 5958 | ` *  The string being translated to to.` |
|      - | 5959 | ` * $to` |
|      - | 5960 | ` *  The string replacing from.` |
|      - | 5961 | ` * $replace_pairs` |
|      - | 5962 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 5963 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 5964 | ` * Return` |
|      - | 5965 | ` *  The translated string.` |
|      - | 5966 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 5967 | ` */` |
|    102 | 5968 | `PH7_PRIVATE int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5969 | `{` |
|      - | 5970 | `	const char *zIn;` |
|      - | 5971 | `	char zGiven[64];` |
|      - | 5972 | `	int nLen;` |
|    103 | 5973 | `	if( nArg < 1 ){` |
|      - | 5974 | `		/* Nothing to replace,return FALSE */` |
|    ! 0 | 5975 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5976 | `		return PH7_OK;` |
|      - | 5977 | `	}` |
|      - | 5978 | `	/*` |
|      - | 5979 | `	 * php dispatches strtr() on ARITY between two overloads — strtr(string, array)` |
|      - | 5980 | ``	 * and strtr(string, string, string) — so $from's expected type is `array` with`` |
|      - | 5981 | ``	 * two arguments and `string` with three, and the stub's `array\|string` union is`` |
|      - | 5982 | `	 * a wording php itself never emits. One signature cannot express that, so the` |
|      - | 5983 | `	 * shared ZPP screen skips this builtin (azSelfChecked[] in vm_arg_check.c) and` |
|      - | 5984 | `	 * the dispatch happens here, in php's left-to-right argument order.` |
|      - | 5985 | `	 *` |
|      - | 5986 | `	 * Both directions used to pass silently: a 2-argument string $from` |
|      - | 5987 | `	 * (strtr("abc","ab")) returned the subject UNCHANGED, and a 3-argument array` |
|      - | 5988 | `	 * $from was likewise ignored — the caller got its input back as if it had been` |
|      - | 5989 | `	 * translated.` |
|      - | 5990 | `	 */` |
|    103 | 5991 | `	if( !PH7_ArgSatisfiesString(apArg[0]) ){` |
|      4 | 5992 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5993 | `			"strtr(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 5994 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|      - | 5995 | `	}` |
|    101 | 5996 | `	if( nArg == 2 ){` |
|     31 | 5997 | `		if( !ph7_value_is_array(apArg[1]) ){` |
|     22 | 5998 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5999 | `				"strtr(): Argument #2 ($from) must be of type array, %s given",` |
|     14 | 6000 | `				VmValueGivenName(apArg[1],zGiven,sizeof(zGiven)));` |
|      1 | 6001 | `		}` |
|     79 | 6002 | `	}else if( nArg > 2 ){` |
|     71 | 6003 | `		if( !PH7_ArgSatisfiesString(apArg[1]) ){` |
|     10 | 6004 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 6005 | `				"strtr(): Argument #2 ($from) must be of type string, %s given",` |
|      6 | 6006 | `				VmValueGivenName(apArg[1],zGiven,sizeof(zGiven)));` |
|      - | 6007 | `		}` |
|      - | 6008 | ``		/* $to is php's `string`, but a null one stays accepted (php coerces it to`` |
|      - | 6009 | `		 * "" with a deprecation, and both engines answer the subject unchanged). */` |
|     65 | 6010 | `		if( !ph7_value_is_null(apArg[2]) && !PH7_ArgSatisfiesString(apArg[2]) ){` |
|      4 | 6011 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 6012 | `				"strtr(): Argument #3 ($to) must be of type string, %s given",` |
|      2 | 6013 | `				VmValueGivenName(apArg[2],zGiven,sizeof(zGiven)));` |
|      - | 6014 | `		}` |
|     31 | 6015 | `	}` |
|     79 | 6016 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     79 | 6017 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 6018 | `		/* Invalid arguments */` |
|      5 | 6019 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      5 | 6020 | `		return PH7_OK;` |
|      - | 6021 | `	}` |
|     82 | 6022 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 6023 | `		strtr_collect sCol;` |
|      - | 6024 | `		SyBlob sPool,sWorker;` |
|      - | 6025 | `		SySet sTable;` |
|      - | 6026 | `		const char *zPool;` |
|      - | 6027 | `		strtr_entry *pEnt;` |
|      - | 6028 | `		sxi32 rc;` |
|      - | 6029 | `		int i,iRun;` |
|      - | 6030 | `		/*` |
|      - | 6031 | `		 * PHP's array-form strtr is a single left-to-right pass over the subject:` |
|      - | 6032 | `		 * at every position it substitutes the LONGEST replace_pairs key that` |
|      - | 6033 | `		 * matches there, then advances past the key (replacements are never` |
|      - | 6034 | `		 * rescanned). It is not a sequential per-key global replace. First copy` |
|      - | 6035 | `		 * the pairs into a persistent pool, then run that scan.` |
|      - | 6036 | `		 */` |
|     15 | 6037 | `		SyBlobInit(&sPool,&pCtx->pVm->sAllocator);` |
|     15 | 6038 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     15 | 6039 | `		SySetInit(&sTable,&pCtx->pVm->sAllocator,sizeof(strtr_entry));` |
|     15 | 6040 | `		sCol.pPool  = &sPool;` |
|     15 | 6041 | `		sCol.pTable = &sTable;` |
|     15 | 6042 | `		sCol.rc     = SXRET_OK;` |
|     15 | 6043 | `		sCol.pCtx   = pCtx;` |
|     15 | 6044 | `		ph7_array_walk(apArg[1],StrtrCollectWalker,&sCol);` |
|     15 | 6045 | `		if( sCol.rc != SXRET_OK ){` |
|      - | 6046 | `			/* Allocation failure while collecting the pairs: surface a fatal */` |
|    ! 0 | 6047 | `			SyBlobRelease(&sPool);` |
|    ! 0 | 6048 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 6049 | `			SySetRelease(&sTable);` |
|    ! 0 | 6050 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6051 | `		}` |
|      - | 6052 | `		/* The pool is now stable, so offsets can be resolved against its base. */` |
|     15 | 6053 | `		zPool = (const char *)SyBlobData(&sPool);` |
|     15 | 6054 | `		rc = SXRET_OK;` |
|     15 | 6055 | `		iRun = 0; /* Start of the pending run of unmatched bytes copied verbatim. */` |
|     59 | 6056 | `		for( i = 0 ; i < nLen ; ){` |
|     45 | 6057 | `			strtr_entry *pBest = 0;` |
|     45 | 6058 | `			sxu32 nBest = 0;` |
|      - | 6059 | `			/* Pick the longest key that matches at the current position. */` |
|     45 | 6060 | `			SySetResetCursor(&sTable);` |
|    105 | 6061 | `			while( SXRET_OK == SySetGetNextEntry(&sTable,(void **)&pEnt) ){` |
|     60 | 6062 | `				if( pEnt->nKeyLen > nBest` |
|     56 | 6063 | `					&& pEnt->nKeyLen <= (sxu32)(nLen - i)` |
|     52 | 6064 | `					&& SyMemcmp(zPool + pEnt->nKeyOfft,zIn + i,pEnt->nKeyLen) == 0 ){` |
|     31 | 6065 | `					nBest = pEnt->nKeyLen;` |
|     31 | 6066 | `					pBest = pEnt;` |
|     15 | 6067 | `				}` |
|      1 | 6068 | `			}` |
|     45 | 6069 | `			if( pBest == 0 ){` |
|      - | 6070 | `				/* No key here: extend the literal run and copy it in one shot later. */` |
|     19 | 6071 | `				i++;` |
|     19 | 6072 | `				continue;` |
|      - | 6073 | `			}` |
|      - | 6074 | `			/* Flush the pending literal run, then the replacement. */` |
|     27 | 6075 | `			if( i > iRun ){` |
|      5 | 6076 | `				rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(i - iRun));` |
|      2 | 6077 | `			}` |
|     27 | 6078 | `			if( rc == SXRET_OK && pBest->nValLen > 0 ){` |
|     27 | 6079 | `				rc = SyBlobAppend(&sWorker,zPool + pBest->nValOfft,pBest->nValLen);` |
|     13 | 6080 | `			}` |
|     27 | 6081 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 6082 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 6083 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 6084 | `				SySetRelease(&sTable);` |
|    ! 0 | 6085 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 6086 | `			}` |
|     27 | 6087 | `			i += (int)pBest->nKeyLen;` |
|     27 | 6088 | `			iRun = i;` |
|      1 | 6089 | `		}` |
|      - | 6090 | `		/* Flush the trailing literal run. */` |
|     15 | 6091 | `		if( nLen > iRun ){` |
|      7 | 6092 | `			rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(nLen - iRun));` |
|      7 | 6093 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 6094 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 6095 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 6096 | `				SySetRelease(&sTable);` |
|    ! 0 | 6097 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 6098 | `			}` |
|      3 | 6099 | `		}` |
|      - | 6100 | `		/* All done, return the result string */` |
|     22 | 6101 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|     14 | 6102 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 6103 | `		/* Clean-up */` |
|     15 | 6104 | `		SyBlobRelease(&sPool);` |
|     15 | 6105 | `		SyBlobRelease(&sWorker);` |
|     15 | 6106 | `		SySetRelease(&sTable);` |
|     15 | 6107 | `		if( rc != PH7_OK ){` |
|    ! 0 | 6108 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6109 | `		}` |
|      8 | 6110 | `	}else{` |
|      - | 6111 | `		int i,flen,tlen,c,iOfft;` |
|      - | 6112 | `		const char *zFrom,*zTo;` |
|     61 | 6113 | `		if( nArg < 3 ){` |
|      - | 6114 | `			/* Nothing to replace */` |
|    ! 0 | 6115 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6116 | `			return PH7_OK;` |
|      - | 6117 | `		}` |
|      - | 6118 | `		/* Extract given arguments */` |
|     61 | 6119 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|     61 | 6120 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|     61 | 6121 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 6122 | `			/* Nothing to replace */` |
|    ! 0 | 6123 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 6124 | `			return PH7_OK;` |
|      - | 6125 | `		}` |
|      - | 6126 | `		/* Start the replace process */` |
|    203 | 6127 | `		for( i = 0 ; i < nLen ; ++i ){` |
|    143 | 6128 | `			c = zIn[i];` |
|    143 | 6129 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|     19 | 6130 | `				if ( iOfft < tlen ){` |
|     19 | 6131 | `					c = zTo[iOfft];` |
|      9 | 6132 | `				}` |
|      9 | 6133 | `			}` |
|    143 | 6134 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 6135 |  |
|     72 | 6136 | `		}` |
|      - | 6137 | `	}` |
|     75 | 6138 | `	return PH7_OK;` |
|     52 | 6139 | `}` |
|      - | 6140 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6141 |  |
