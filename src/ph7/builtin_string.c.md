# src/ph7/builtin_string.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2343/2722 lines (86.08%)

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
| 303933 |   60 | `PH7_PRIVATE int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |   61 | `{` |
|      - |   62 | `	const char *zSource;` |
|      - |   63 | `	int nSrcLen;` |
|      - |   64 | `	sxi64 iStart,iEnd;` |
| 303938 |   65 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"substr",1,"$string"); }` |
| 303938 |   66 | `	if( nArg < 2 ){` |
|      - |   67 | `		/* Arity is enforced at the call boundary; nothing sensible to return here. */` |
|    ! 0 |   68 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 |   69 | `		return PH7_OK;` |
|      - |   70 | `	}` |
|      - |   71 | `	/* Extract the target string */` |
| 303938 |   72 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|      - |   73 | `	/* Extract the offset */` |
|      - |   74 | `	{` |
| 303938 |   75 | `		sxi64 iTmp = 0;` |
| 303938 |   76 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"substr",2,"$offset","int",&iTmp);` |
| 303938 |   77 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |   78 | `			return rcArg;` |
|      - |   79 | `		}` |
| 303938 |   80 | `		iStart = iTmp;` |
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
| 303938 |   92 | `	if( iStart < 0 ){` |
|  34445 |   93 | `		iStart += nSrcLen;` |
|  34445 |   94 | `		if( iStart < 0 ){` |
|      5 |   95 | `			iStart = 0;` |
|      7 |   96 | `		}` |
| 286718 |   97 | `	}else if( iStart > nSrcLen ){` |
|      7 |   98 | `		iStart = nSrcLen;` |
|      3 |   99 | `	}` |
| 303938 |  100 | `	iEnd = nSrcLen;` |
| 303938 |  101 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
| 213871 |  102 | `		sxi64 iLen = 0;` |
| 213871 |  103 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],"substr",3,"$length","?int",&iLen);` |
| 213871 |  104 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  105 | `			return rcArg;` |
|      - |  106 | `		}` |
| 213871 |  107 | `		if( iLen < 0 ){` |
|  34069 |  108 | `			iEnd = (sxi64)nSrcLen + iLen;` |
| 196839 |  109 | `		}else if( iLen > (sxi64)nSrcLen - iStart ){` |
|  19957 |  110 | `			iEnd = nSrcLen;` |
|   9981 |  111 | `		}else{` |
| 159855 |  112 | `			iEnd = iStart + iLen;` |
|      - |  113 | `		}` |
| 106933 |  114 | `	}` |
| 303938 |  115 | `	if( iEnd < iStart ){` |
|      3 |  116 | `		iEnd = iStart;` |
|      1 |  117 | `	}` |
| 303938 |  118 | `	ph7_result_string(pCtx,&zSource[iStart],(int)(iEnd - iStart));` |
| 303938 |  119 | `	return PH7_OK;` |
| 152134 |  120 | `}` |
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
|     15 |  208 | `		rc = (l1 == l2) ? 0 : (l1 < l2 ? -1 : 1);` |
|      7 |  209 | `	}` |
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
|      - |  316 | ` * non-deprecated surface and rejects it with a TypeError. Every caller of this helper` |
|      - |  317 | `` * now also carries a `string $…` row in the vm_arg_check.c signature table, so`` |
|      - |  318 | ` * VmEnforceBuiltinArgTypes raises that TypeError BEFORE the routine runs and this is a` |
|      - |  319 | ` * backstop rather than the live path. It stays correct either way: the throw records` |
|      - |  320 | ` * its status on the call context and the OP_CALL boundary (VmHostFuncThrowRc) reports` |
|      - |  321 | ` * it in place of the routine's own, so the call ABORTS as php's would without the` |
|      - |  322 | ` * callers threading a status back. */` |
| 475885 |  323 | `static void StrNullArgNotice(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNum,const char *zParamName)` |
|      5 |  324 | `{` |
| 475890 |  325 | `	if( ph7_value_is_null(pArg) ){` |
|    ! 0 |  326 | `		PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  327 | `			"%s(): Argument #%d (%s) must be of type string, null given",` |
|    ! 0 |  328 | `			zFunc,iArgNum,zParamName);` |
|    ! 0 |  329 | `	}` |
| 475890 |  330 | `}` |
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
|     16 | 1105 | `PH7_PRIVATE int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1106 | `{` |
|      - | 1107 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1108 | `	int nLen;` |
|      - | 1109 | `	/* PHP enforces exactly one argument. */` |
|     17 | 1110 | `	if( nArg != 1 ){` |
|    ! 0 | 1111 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1112 | `			"ArgumentCountError",` |
|      - | 1113 | `			"addslashes() expects exactly 1 argument, %d given",` |
|    ! 0 | 1114 | `			nArg` |
|      - | 1115 | `			);` |
|      - | 1116 | `	}` |
|      - | 1117 | `	/* php only DEPRECATES null here; PHL rejects it. */` |
|     17 | 1118 | `	if( ph7_value_is_null(apArg[0]) ){` |
|    ! 0 | 1119 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1120 | `			"addslashes(): Argument #1 ($string) must be of type string, null given"` |
|      - | 1121 | `			);` |
|      - | 1122 | `	}` |
|      - | 1123 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     24 | 1124 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     25 | 1125 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     16 | 1126 | `	    ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 1127 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1128 | `			"TypeError",` |
|      - | 1129 | `			"addslashes(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 1130 | `			ph7_type_name(apArg[0])` |
|      - | 1131 | `			);` |
|      - | 1132 | `	}` |
|      - | 1133 | `	/* Convert to string representation first and obtain length. */` |
|     17 | 1134 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 1135 | `	if( nLen < 1 ){` |
|      - | 1136 | `		/* Return the empty string */` |
|      3 | 1137 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1138 | `		return PH7_OK;` |
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
|      9 | 1168 | `}` |
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
|    274 | 1184 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256])` |
|      5 | 1185 | `{` |
|    279 | 1186 | `	const unsigned char *zIn  = (const unsigned char *)zList;` |
|    279 | 1187 | `	const unsigned char *zEnd = zIn + (nLen > 0 ? nLen : 0);` |
|    279 | 1188 | `	SyZero(aMask,256);` |
|    807 | 1189 | `	for( ; zIn < zEnd ; zIn++ ){` |
|    533 | 1190 | `		int c = zIn[0];` |
|    533 | 1191 | `		if( zIn + 3 < zEnd && zIn[1] == '.' && zIn[2] == '.' && zIn[3] >= c ){` |
|      - | 1192 | `			/* Valid incrementing range c..zIn[3] */` |
|     22 | 1193 | `			int hi = zIn[3],k;` |
|    386 | 1194 | `			for( k = c ; k <= hi ; k++ ){` |
|    366 | 1195 | `				aMask[k] = 1;` |
|    184 | 1196 | `			}` |
|     22 | 1197 | `			zIn += 3; /* the loop's ++ then steps past the range end */` |
|    535 | 1198 | `		}else if( zIn + 1 < zEnd && zIn[0] == '.' && zIn[1] == '.' ){` |
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
|    489 | 1220 | `			aMask[c] = 1;` |
|      - | 1221 | `		}` |
|    269 | 1222 | `	}` |
|    279 | 1223 | `}` |
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
|     10 | 1346 | `PH7_PRIVATE int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1347 | `{` |
|      - | 1348 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1349 | `	char aMask[256];` |
|      - | 1350 | `	int nLen;` |
|     12 | 1351 | `	if( nArg < 1 ){` |
|      - | 1352 | `		/* Nothing to process,retun NULL */` |
|    ! 0 | 1353 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1354 | `		return PH7_OK;` |
|      - | 1355 | `	}` |
|      - | 1356 | `	/* Extract the string to process */` |
|     12 | 1357 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     12 | 1358 | `	if( nLen < 1 ){` |
|      - | 1359 | `		/* Return the empty string */` |
|      3 | 1360 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1361 | `		return PH7_OK;` |
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
|      7 | 1387 | `}` |
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
|      6 | 1400 | `PH7_PRIVATE int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1401 | `{` |
|      - | 1402 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1403 | `	int nLen;` |
|      7 | 1404 | `	if( nArg < 1 ){` |
|      - | 1405 | `		/* Nothing to process,retun NULL */` |
|    ! 0 | 1406 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1407 | `		return PH7_OK;` |
|      - | 1408 | `	}` |
|      - | 1409 | `	/* Extract the string to process */` |
|      7 | 1410 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 1411 | `	if( zIn == 0 ){` |
|    ! 0 | 1412 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1413 | `		return PH7_OK;` |
|      - | 1414 | `	}` |
|      7 | 1415 | `	zEnd = &zIn[nLen];` |
|      7 | 1416 | `	zCur = 0; /* cc warning */` |
|      - | 1417 | `	/* Seed an empty string result: the loop below only ever APPENDS, so without` |
|      - | 1418 | `	 * this an empty input would leave the return value untouched and answer` |
|      - | 1419 | `	 * NULL where php answers "". */` |
|      7 | 1420 | `	ph7_result_string(pCtx,"",0);` |
|      - | 1421 | `	/* Encode the string */` |
|      4 | 1422 | `	for(;;){` |
|      9 | 1423 | `		if( zIn >= zEnd ){` |
|      - | 1424 | `			/* No more input */` |
|      3 | 1425 | `			break;` |
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
|      7 | 1445 | `	return PH7_OK;` |
|      4 | 1446 | `}` |
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
| 111802 | 1614 | `PH7_PRIVATE int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1615 | `{` |
| 111807 | 1616 | `	int iLen = 0;` |
| 111807 | 1617 | `	if( nArg > 0 ){` |
| 111807 | 1618 | `		StrNullArgNotice(pCtx,apArg[0],"strlen",1,"$string");` |
| 111807 | 1619 | `		ph7_value_to_string(apArg[0],&iLen);` |
|  56226 | 1620 | `	}` |
|      - | 1621 | `	/* String length */` |
| 111807 | 1622 | `	ph7_result_int(pCtx,iLen);` |
| 111807 | 1623 | `	return PH7_OK;` |
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
|     82 | 1635 | `PH7_PRIVATE int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1636 | `{` |
|      - | 1637 | `	const char *z1,*z2;` |
|      - | 1638 | `	int n1,n2;` |
|      - | 1639 | `	int res;` |
|     84 | 1640 | `	if( nArg < 2 ){` |
|    ! 0 | 1641 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 1642 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 1643 | `		return PH7_OK;` |
|      - | 1644 | `	}` |
|      - | 1645 | `	/* Perform the comparison */` |
|     84 | 1646 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     84 | 1647 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     84 | 1648 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1649 | `	/* Comparison result */` |
|     84 | 1650 | `	ph7_result_int(pCtx,res);` |
|     84 | 1651 | `	return PH7_OK;` |
|     43 | 1652 | `}` |
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
|     42 | 1665 | `static int StrNatCompareRight(const char **pa,const char *aEnd,const char **pb,const char *bEnd)` |
|      1 | 1666 | `{` |
|     43 | 1667 | `	int bias = 0;` |
|     71 | 1668 | `	for(;;){` |
|     93 | 1669 | `		int da = (*pa < aEnd) && SyisDigit(**pa);` |
|     93 | 1670 | `		int db = (*pb < bEnd) && SyisDigit(**pb);` |
|     93 | 1671 | `		if( !da && !db ){ return bias; }` |
|     73 | 1672 | `		if( !da ){ return -1; }` |
|     63 | 1673 | `		if( !db ){ return 1; }` |
|     51 | 1674 | `		if( **pa < **pb ){ if( !bias ){ bias = -1; } }` |
|     37 | 1675 | `		else if( **pa > **pb ){ if( !bias ){ bias = 1; } }` |
|     51 | 1676 | `		(*pa)++;` |
|     51 | 1677 | `		(*pb)++;` |
|      1 | 1678 | `	}` |
|     22 | 1679 | `}` |
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
|     48 | 1694 | `PH7_PRIVATE int PH7_StrNatCmp(const char *zA,int nA,const char *zB,int nB,int bFold)` |
|      1 | 1695 | `{` |
|     49 | 1696 | `	const char *a = zA,*aEnd = &zA[nA];` |
|     49 | 1697 | `	const char *b = zB,*bEnd = &zB[nB];` |
|    146 | 1698 | `	for(;;){` |
|      - | 1699 | `		int ca,cb;` |
|    175 | 1700 | `		while( a < aEnd && SyisSpace(a[0]) ){ a++; }` |
|    173 | 1701 | `		while( b < bEnd && SyisSpace(b[0]) ){ b++; }` |
|    173 | 1702 | `		ca = (a < aEnd) ? (unsigned char)a[0] : 0;` |
|    173 | 1703 | `		cb = (b < bEnd) ? (unsigned char)b[0] : 0;` |
|    173 | 1704 | `		if( SyisDigit(ca) && SyisDigit(cb) ){` |
|     45 | 1705 | `			int r = (ca == '0' \|\| cb == '0')` |
|      4 | 1706 | `				? StrNatCompareLeft(&a,aEnd,&b,bEnd)` |
|     65 | 1707 | `				: StrNatCompareRight(&a,aEnd,&b,bEnd);` |
|     47 | 1708 | `			if( r ){ return r; }` |
|      5 | 1709 | `			continue;` |
|      - | 1710 | `		}` |
|    127 | 1711 | `		if( ca == 0 && cb == 0 ){ return 0; }` |
|    121 | 1712 | `		if( bFold ){` |
|     67 | 1713 | `			ca = SyToLower(ca);` |
|     67 | 1714 | `			cb = SyToLower(cb);` |
|     33 | 1715 | `		}` |
|    121 | 1716 | `		if( ca < cb ){ return -1; }` |
|    121 | 1717 | `		if( ca > cb ){ return 1; }` |
|    121 | 1718 | `		a++;` |
|    121 | 1719 | `		b++;` |
|      1 | 1720 | `	}` |
|     25 | 1721 | `}` |
|      - | 1722 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - | 1723 | `/*` |
|      - | 1724 | ` * int strnatcmp(string $string1, string $string2)` |
|      - | 1725 | ` * int strnatcasecmp(string $string1, string $string2)` |
|      - | 1726 | ` *  Natural-order string comparison ("img2" < "img10"), case folded for the` |
|      - | 1727 | ` *  latter. php 8.2+ normalizes the result to -1/0/1.` |
|      - | 1728 | ` */` |
|     20 | 1729 | `PH7_PRIVATE int PH7_builtin_strnatcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1730 | `{` |
|      - | 1731 | `	const char *z1,*z2,*zFunc;` |
|      - | 1732 | `	int n1,n2,bFold;` |
|     21 | 1733 | `	if( nArg < 2 ){` |
|    ! 0 | 1734 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 1735 | `		return PH7_OK;` |
|      - | 1736 | `	}` |
|     21 | 1737 | `	zFunc = ph7_function_name(pCtx);` |
|     21 | 1738 | `	bFold = zFunc[sizeof("strnat")-1] == 'c'; /* strnatCasecmp */` |
|     21 | 1739 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     21 | 1740 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     21 | 1741 | `	ph7_result_int(pCtx,PH7_StrNatCmp(z1,n1,z2,n2,bFold));` |
|     21 | 1742 | `	return PH7_OK;` |
|     11 | 1743 | `}` |
|      - | 1744 | `/*` |
|      - | 1745 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 1746 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 1747 | ` * Parameter` |
|      - | 1748 | ` *  str1: The first string` |
|      - | 1749 | ` *  str2: The second string` |
|      - | 1750 | ` * Return` |
|      - | 1751 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1752 | ` *  than str2, and 0 if they are equal.` |
|      - | 1753 | ` */` |
|    388 | 1754 | `PH7_PRIVATE int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1755 | `{` |
|      - | 1756 | `	const char *z1,*z2;` |
|      - | 1757 | `	int res;` |
|      - | 1758 | `	int n;` |
|    391 | 1759 | `	if( nArg < 3 ){` |
|      - | 1760 | `		/* Perform a standard comparison */` |
|    ! 0 | 1761 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 1762 | `	}` |
|      - | 1763 | `	/* Desired comparison length */` |
|    391 | 1764 | `	n  = ph7_value_to_int(apArg[2]);` |
|    391 | 1765 | `	if( n < 0 ){` |
|      - | 1766 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 1767 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1768 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 1769 | `			ph7_function_name(pCtx));` |
|      - | 1770 | `	}` |
|      - | 1771 | `	/* Perform the comparison */` |
|    389 | 1772 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|    389 | 1773 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|    389 | 1774 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 1775 | `	/* Comparison result */` |
|    389 | 1776 | `	ph7_result_int(pCtx,res);` |
|    389 | 1777 | `	return PH7_OK;` |
|    197 | 1778 | `}` |
|      - | 1779 | `/*` |
|      - | 1780 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 1781 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 1782 | ` * Parameter` |
|      - | 1783 | ` *  str1: The first string` |
|      - | 1784 | ` *  str2: The second string` |
|      - | 1785 | ` * Return` |
|      - | 1786 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1787 | ` *  than str2, and 0 if they are equal.` |
|      - | 1788 | ` */` |
|    152 | 1789 | `PH7_PRIVATE int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1790 | `{` |
|      - | 1791 | `	const char *z1,*z2;` |
|      - | 1792 | `	int n1,n2;` |
|      - | 1793 | `	int res;` |
|    153 | 1794 | `	if( nArg < 2 ){` |
|    ! 0 | 1795 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 1796 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 1797 | `		return PH7_OK;` |
|      - | 1798 | `	}` |
|      - | 1799 | `	/* Perform the comparison */` |
|    153 | 1800 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|    153 | 1801 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|    153 | 1802 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1803 | `	/* Comparison result */` |
|    153 | 1804 | `	ph7_result_int(pCtx,res);` |
|    153 | 1805 | `	return PH7_OK;` |
|     77 | 1806 | `}` |
|      - | 1807 | `/*` |
|      - | 1808 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 1809 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 1810 | ` * Parameter` |
|      - | 1811 | ` *  $str1: The first string` |
|      - | 1812 | ` *  $str2: The second string` |
|      - | 1813 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 1814 | ` * Return` |
|      - | 1815 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1816 | ` *  than str2, and 0 if they are equal.` |
|      - | 1817 | ` */` |
|     60 | 1818 | `PH7_PRIVATE int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1819 | `{` |
|      - | 1820 | `	const char *z1,*z2;` |
|      - | 1821 | `	int res;` |
|      - | 1822 | `	int n;` |
|     65 | 1823 | `	if( nArg < 3 ){` |
|      - | 1824 | `		/* Perform a standard comparison */` |
|    ! 0 | 1825 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 1826 | `	}` |
|      - | 1827 | `	/* Desired comparison length */` |
|     65 | 1828 | `	n  = ph7_value_to_int(apArg[2]);` |
|     65 | 1829 | `	if( n < 0 ){` |
|      - | 1830 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 1831 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1832 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 1833 | `			ph7_function_name(pCtx));` |
|      - | 1834 | `	}` |
|      - | 1835 | `	/* Perform the comparison */` |
|     63 | 1836 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     63 | 1837 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     63 | 1838 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 1839 | `	/* Comparison result */` |
|     63 | 1840 | `	ph7_result_int(pCtx,res);` |
|     63 | 1841 | `	return PH7_OK;` |
|     35 | 1842 | `}` |
|      - | 1843 | `/*` |
|      - | 1844 | ` * Implode context [i.e: it's private data].` |
|      - | 1845 | ` * A pointer to the following structure is forwarded` |
|      - | 1846 | ` * verbatim to the array walker callback defined below.` |
|      - | 1847 | ` */` |
|      - | 1848 | `struct implode_data {` |
|      - | 1849 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 1850 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 1851 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 1852 | `	int nSeplen;          /* Separator length */` |
|      - | 1853 | `	int bFirst;           /* TRUE if first call */` |
|      - | 1854 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 1855 | `	sxi32 rc;             /* Captured allocation rc; SXERR_MEM => the builtin raises an OOM fatal */` |
|      - | 1856 | `};` |
|      - | 1857 | `/*` |
|      - | 1858 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 1859 | ` * The following routine is invoked for each array entry passed` |
|      - | 1860 | ` * to the implode() function.` |
|      - | 1861 | ` */` |
| 170282 | 1862 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 1863 | `{` |
|  85141 | 1864 | `	SXUNUSED(pKey);` |
| 170287 | 1865 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 1866 | `	const char *zData;` |
|      - | 1867 | `	int nLen;` |
| 170287 | 1868 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 1869 | `		if( pData->nSeplen > 0 ){` |
|      3 | 1870 | `			if( !pData->bFirst ){` |
|      - | 1871 | `				/* append the separator first */` |
|      3 | 1872 | `				if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 1873 | `					pData->rc = SXERR_MEM;` |
|    ! 0 | 1874 | `					return PH7_ABORT;` |
|      - | 1875 | `				}` |
|      2 | 1876 | `			}else{` |
|    ! 0 | 1877 | `				pData->bFirst = 0;` |
|      - | 1878 | `			}` |
|      1 | 1879 | `		}` |
|      - | 1880 | `		/* Recurse */` |
|      3 | 1881 | `		pData->bFirst = 1;` |
|      3 | 1882 | `		pData->nRecCount++;` |
|      3 | 1883 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 1884 | `		pData->nRecCount--;` |
|      - | 1885 | `		/* Propagate an allocation failure surfaced deeper in the recursion. */` |
|      3 | 1886 | `		if( pData->rc != SXRET_OK ){` |
|    ! 0 | 1887 | `			return PH7_ABORT;` |
|      - | 1888 | `		}` |
|      3 | 1889 | `		return PH7_OK;` |
|      - | 1890 | `	}` |
|      - | 1891 | `	/* php's user-visible array->string warning: implode of an array element` |
|      - | 1892 | `	 * that is itself an array renders it as "Array" and warns (§2). */` |
| 170285 | 1893 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|      5 | 1894 | `		PH7_VmThrowError(pData->pCtx->pVm,0,PH7_CTX_WARNING,"Array to string conversion");` |
|      2 | 1895 | `	}` |
|      - | 1896 | `	/* Extract the string representation of the entry value */` |
| 170285 | 1897 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 1898 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 170285 | 1899 | `	if( pData->bFirst ){` |
|  34807 | 1900 | `		pData->bFirst = 0;` |
| 152884 | 1901 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 1902 | `		/* append the separator first */` |
| 134901 | 1903 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 1904 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1905 | `			return PH7_ABORT;` |
|      - | 1906 | `		}` |
|  67448 | 1907 | `	}` |
|      - | 1908 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 170285 | 1909 | `	if( nLen > 0 ){` |
| 157145 | 1910 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1911 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1912 | `			return PH7_ABORT;` |
|      - | 1913 | `		}` |
|  78570 | 1914 | `	}` |
| 170285 | 1915 | `	return PH7_OK;` |
|  85146 | 1916 | `}` |
|      - | 1917 | `/*` |
|      - | 1918 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 1919 | ` * string implode(array $pieces,...)` |
|      - | 1920 | ` *  Join array elements with a string.` |
|      - | 1921 | ` * $glue` |
|      - | 1922 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 1923 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 1924 | ` * $pieces` |
|      - | 1925 | ` *   The array of strings to implode.` |
|      - | 1926 | ` * Return` |
|      - | 1927 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 1928 | ` *  order, with the glue string between each element.` |
|      - | 1929 | ` */` |
|  34886 | 1930 | `PH7_PRIVATE int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1931 | `{` |
|      - | 1932 | `	struct implode_data imp_data;` |
|  34891 | 1933 | `	int i = 1;` |
|  34891 | 1934 | `	if( nArg < 1 ){` |
|      - | 1935 | `		/* Missing argument,return NULL */` |
|    ! 0 | 1936 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1937 | `		return PH7_OK;` |
|      - | 1938 | `	}` |
|      - | 1939 | `	/* Prepare the implode context */` |
|  34891 | 1940 | `	imp_data.pCtx = pCtx;` |
|  34891 | 1941 | `	imp_data.bRecursive = 0;` |
|  34891 | 1942 | `	imp_data.bFirst = 1;` |
|  34891 | 1943 | `	imp_data.nRecCount = 0;` |
|  34891 | 1944 | `	imp_data.rc = SXRET_OK;` |
|  34891 | 1945 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  34889 | 1946 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  34889 | 1947 | `		if( nArg > 1 && !ph7_value_is_array(apArg[1]) && !ph7_value_is_null(apArg[1]) ){` |
|      - | 1948 | `			/* php: implode($glue, $pieces) requires an ARRAY. PH7 stringified whatever it` |
|      - | 1949 | `			 * was handed, so implode(",", 5) quietly returned "5". */` |
|      - | 1950 | `			char zBuf[64];` |
|      4 | 1951 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1952 | `				"implode(): Argument #2 ($array) must be of type ?array, %s given",` |
|      2 | 1953 | `				VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 1954 | `		}` |
|  17446 | 1955 | `	}else{` |
|      3 | 1956 | `		if( nArg > 1 ){` |
|      - | 1957 | `			/* php 8 removed the legacy swapped order: implode($pieces, $glue)` |
|      - | 1958 | `			 * is a TypeError (PHL used to swap silently, a wrong ANSWER when` |
|      - | 1959 | `			 * the caller meant php's signature). One array argument alone` |
|      - | 1960 | `			 * stays the legal ""-glue form. */` |
|    ! 0 | 1961 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1962 | `				"implode(): Argument #1 ($separator) must be of type string, array given");` |
|      - | 1963 | `		}` |
|      3 | 1964 | `		imp_data.zSep = 0;` |
|      3 | 1965 | `		imp_data.nSeplen = 0;` |
|      3 | 1966 | `		i = 0;` |
|      - | 1967 | `	}` |
|  34889 | 1968 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 1969 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1970 | `	}` |
|      - | 1971 | `	/* Start the 'join' process */` |
|  69773 | 1972 | `	while( i < nArg ){` |
|  34889 | 1973 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1974 | `			/* Iterate throw array entries */` |
|  34889 | 1975 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 1976 | `			/* Surface a callback allocation failure as a fatal */` |
|  34889 | 1977 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 1978 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1979 | `			}` |
|  17447 | 1980 | `		}else{` |
|      - | 1981 | `			const char *zData;` |
|      - | 1982 | `			int nLen;` |
|      - | 1983 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 1984 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1985 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 1986 | `			if( imp_data.bFirst ){` |
|    ! 0 | 1987 | `				imp_data.bFirst = 0;` |
|    ! 0 | 1988 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1989 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 1990 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1991 | `				}` |
|    ! 0 | 1992 | `			}` |
|      - | 1993 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 1994 | `			if( nLen > 0 ){` |
|    ! 0 | 1995 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1996 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1997 | `				}` |
|    ! 0 | 1998 | `			}` |
|      - | 1999 | `		}` |
|  34889 | 2000 | `		i++;` |
|      5 | 2001 | `	}` |
|  34889 | 2002 | `	return PH7_OK;` |
|  17448 | 2003 | `}` |
|      - | 2004 | `/*` |
|      - | 2005 | ` * Symisc eXtension:` |
|      - | 2006 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 2007 | ` * Purpose` |
|      - | 2008 | ` *  Same as implode() but recurse on arrays.` |
|      - | 2009 | ` * Example:` |
|      - | 2010 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 2011 | ` *   echo implode_recursive("/",$a);` |
|      - | 2012 | ` *   Will output` |
|      - | 2013 | ` *     usr/home/dean.` |
|      - | 2014 | ` *   While the standard implode would produce.` |
|      - | 2015 | ` *    usr/Array.` |
|      - | 2016 | ` * Parameter` |
|      - | 2017 | ` *  Refer to implode().` |
|      - | 2018 | ` * Return` |
|      - | 2019 | ` *  Refer to implode().` |
|      - | 2020 | ` */` |
|     12 | 2021 | `PH7_PRIVATE int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2022 | `{` |
|      - | 2023 | `	struct implode_data imp_data;` |
|     13 | 2024 | `	int i = 1;` |
|     13 | 2025 | `	if( nArg < 1 ){` |
|      - | 2026 | `		/* Missing argument,return NULL */` |
|      3 | 2027 | `		ph7_result_null(pCtx);` |
|      3 | 2028 | `		return PH7_OK;` |
|      - | 2029 | `	}` |
|      - | 2030 | `	/* Prepare the implode context */` |
|     11 | 2031 | `	imp_data.pCtx = pCtx;` |
|     11 | 2032 | `	imp_data.bRecursive = 1;` |
|     11 | 2033 | `	imp_data.bFirst = 1;` |
|     11 | 2034 | `	imp_data.nRecCount = 0;` |
|     11 | 2035 | `	imp_data.rc = SXRET_OK;` |
|     11 | 2036 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 2037 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 2038 | `	}else{` |
|    ! 0 | 2039 | `		imp_data.zSep = 0;` |
|    ! 0 | 2040 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 2041 | `		i = 0;` |
|      - | 2042 | `	}` |
|     11 | 2043 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2044 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2045 | `	}` |
|      - | 2046 | `	/* Start the 'join' process */` |
|     21 | 2047 | `	while( i < nArg ){` |
|     11 | 2048 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2049 | `			/* Iterate throw array entries */` |
|      3 | 2050 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2051 | `			/* Surface a callback allocation failure as a fatal */` |
|      3 | 2052 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2053 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2054 | `			}` |
|      2 | 2055 | `		}else{` |
|      - | 2056 | `			const char *zData;` |
|      - | 2057 | `			int nLen;` |
|      - | 2058 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 2059 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2060 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 2061 | `			if( imp_data.bFirst ){` |
|      9 | 2062 | `				imp_data.bFirst = 0;` |
|      4 | 2063 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2064 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2065 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2066 | `				}` |
|    ! 0 | 2067 | `			}` |
|      - | 2068 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 2069 | `			if( nLen > 0 ){` |
|      9 | 2070 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2071 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2072 | `				}` |
|      4 | 2073 | `			}` |
|      - | 2074 | `		}` |
|     11 | 2075 | `		i++;` |
|      1 | 2076 | `	}` |
|     11 | 2077 | `	return PH7_OK;` |
|      7 | 2078 | `}` |
|      - | 2079 | `/*` |
|      - | 2080 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 2081 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 2082 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 2083 | ` * Parameters` |
|      - | 2084 | ` *  $delimiter` |
|      - | 2085 | ` *   The boundary string.` |
|      - | 2086 | ` * $string` |
|      - | 2087 | ` *   The input string.` |
|      - | 2088 | ` * $limit` |
|      - | 2089 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 2090 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 2091 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 2092 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 2093 | ` * Returns` |
|      - | 2094 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 2095 | ` *  on boundaries formed by the delimiter.` |
|      - | 2096 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 2097 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 2098 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 2099 | ` *  will be returned.` |
|      - | 2100 | ` * NOTE:` |
|      - | 2101 | ` *  Negative limit is not supported.` |
|      - | 2102 | ` */` |
|   7256 | 2103 | `PH7_PRIVATE int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2104 | `{` |
|      - | 2105 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2106 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2107 | `	ph7_value *pArray;` |
|      - | 2108 | `	ph7_value *pValue;` |
|      - | 2109 | `	sxu32 nOfft;` |
|      - | 2110 | `	sxi32 rc;` |
|   7261 | 2111 | `	if( nArg < 2 ){` |
|      - | 2112 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2113 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2114 | `		return PH7_OK;` |
|      - | 2115 | `	}` |
|      - | 2116 | `	/* Extract the delimiter */` |
|   7261 | 2117 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   7261 | 2118 | `	if( nDelim < 1 ){` |
|      - | 2119 | `		/* Empty delimiter: PHP 8 throws a catchable ValueError. */` |
|      5 | 2120 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2121 | `			"explode(): Argument #1 ($separator) must not be empty");` |
|      - | 2122 | `	}` |
|      - | 2123 | `	/* Extract the string */` |
|   7257 | 2124 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   7257 | 2125 | `	if( nStrlen < 1 ){` |
|      - | 2126 | `		/* Empty string: normally an array with a single empty element (PHP behavior).` |
|      - | 2127 | `		 * A negative limit drops the last -limit components, so the sole empty` |
|      - | 2128 | `		 * component is dropped and the result is an empty array. */` |
|     13 | 2129 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|     13 | 2130 | `		if( pArrayTmp == 0 ){` |
|      - | 2131 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 2132 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2133 | `			return PH7_OK;` |
|      - | 2134 | `		}` |
|     13 | 2135 | `		if( !(nArg > 2 && ph7_value_to_int(apArg[2]) < 0) ){` |
|     11 | 2136 | `			ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|     11 | 2137 | `			if( pValueTmp == 0 ){` |
|      - | 2138 | `				/* Out of memory,return FALSE */` |
|    ! 0 | 2139 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 2140 | `				return PH7_OK;` |
|      - | 2141 | `			}` |
|     11 | 2142 | `			ph7_value_string(pValueTmp, "", 0);` |
|     11 | 2143 | `			if( ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp) != SXRET_OK ){` |
|    ! 0 | 2144 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2145 | `			}` |
|      5 | 2146 | `		}` |
|     13 | 2147 | `		ph7_result_value(pCtx, pArrayTmp);` |
|     13 | 2148 | `		return PH7_OK;` |
|      - | 2149 | `	}` |
|      - | 2150 | `	/* Point to the end of the string */` |
|   7245 | 2151 | `	zEnd = &zString[nStrlen];` |
|      - | 2152 | `	/* Create the array */` |
|   7245 | 2153 | `	pArray =  ph7_context_new_array(pCtx);` |
|   7245 | 2154 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   7245 | 2155 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2156 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2157 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2158 | `		return PH7_OK;` |
|      - | 2159 | `	}` |
|      - | 2160 | `	/* Set a defualt limit */` |
|   7245 | 2161 | `	iLimit = SXI32_HIGH;` |
|   7245 | 2162 | `	if( nArg > 2 ){` |
|     91 | 2163 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     91 | 2164 | `		if( iLimit < 0 ){` |
|      - | 2165 | `			/* Negative limit: keep all components except the last -iLimit (PHP).` |
|      - | 2166 | `			 * Pre-count the components (delimiters + 1), then emit only the first` |
|      - | 2167 | `			 * nKeep CLEAN components — no trailing-remainder merge (the difference` |
|      - | 2168 | `			 * from the positive path). nKeep <= 0 drops everything -> empty array. */` |
|     17 | 2169 | `			int nTotal = 1,nKeep;` |
|     17 | 2170 | `			const char *zScan = zString;` |
|      - | 2171 | `			sxu32 nScanOfft;` |
|     57 | 2172 | `			while( SyBlobSearch(zScan,(sxu32)(zEnd - zScan),zDelim,nDelim,&nScanOfft) == SXRET_OK ){` |
|     41 | 2173 | `				nTotal++;` |
|     41 | 2174 | `				zScan = &zScan[nScanOfft + nDelim];` |
|      1 | 2175 | `			}` |
|     17 | 2176 | `			nKeep = nTotal + iLimit; /* iLimit < 0, so this is nTotal - (-iLimit) */` |
|     49 | 2177 | `			while( nKeep > (int)ph7_array_count(pArray)` |
|     39 | 2178 | `				&& SyBlobSearch(zString,(sxu32)(zEnd - zString),zDelim,nDelim,&nOfft) == SXRET_OK ){` |
|      - | 2179 | `				/* Emit the next clean component */` |
|     23 | 2180 | `				zCur = &zString[nOfft];` |
|     23 | 2181 | `				ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|     23 | 2182 | `				if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2183 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2184 | `				}` |
|     23 | 2185 | `				zString = &zCur[nDelim];` |
|     23 | 2186 | `				ph7_value_reset_string_cursor(pValue);` |
|      1 | 2187 | `			}` |
|     17 | 2188 | `			ph7_result_value(pCtx,pArray);` |
|     17 | 2189 | `			return PH7_OK;` |
|      - | 2190 | `		}` |
|     75 | 2191 | `		if( iLimit == 0 ){` |
|      5 | 2192 | `			iLimit = 1;` |
|      2 | 2193 | `		}` |
|     75 | 2194 | `		iLimit--;` |
|     35 | 2195 | `	}` |
|      - | 2196 | `	/* Start exploding */` |
|  90106 | 2197 | `	for(;;){` |
| 180217 | 2198 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 180217 | 2199 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2200 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   7229 | 2201 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   7229 | 2202 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2203 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2204 | `			}` |
|   7229 | 2205 | `			break;` |
|      - | 2206 | `		}` |
|      - | 2207 | `		/* Point to the desired offset */` |
| 172993 | 2208 | `		zCur = &zString[nOfft];` |
|      - | 2209 | `		/* Perform the store operation (may be empty) */` |
| 172993 | 2210 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 172993 | 2211 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2212 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2213 | `		}` |
|      - | 2214 | `		/* Point beyond the delimiter */` |
| 172993 | 2215 | `		zString = &zCur[nDelim];` |
|      - | 2216 | `		/* Reset the cursor */` |
| 172993 | 2217 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 2218 | `	}` |
|      - | 2219 | `	/* Return the freshly created array */` |
|   7229 | 2220 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2221 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2222 | `	 * released as soon we return from this foregin function.` |
|      - | 2223 | `	 */` |
|   7229 | 2224 | `	return PH7_OK;` |
|   3633 | 2225 | `}` |
|      - | 2226 | `/*` |
|      - | 2227 | ` * string trim(string $str[,string $charlist ])` |
|      - | 2228 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2229 | ` * Parameters` |
|      - | 2230 | ` *  $str` |
|      - | 2231 | ` *   The string that will be trimmed.` |
|      - | 2232 | ` * $charlist` |
|      - | 2233 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2234 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2235 | ` *   With .. you can specify a range of characters.` |
|      - | 2236 | ` * Returns.` |
|      - | 2237 | ` *  Thr processed string.` |
|      - | 2238 | ` * NOTE:` |
|      - | 2239 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2240 | ` */` |
|  21672 | 2241 | `PH7_PRIVATE int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2242 | `{` |
|  21677 | 2243 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"trim",1,"$string"); }` |
|      - | 2244 | `	const char *zString;` |
|      - | 2245 | `	int nLen;` |
|  21677 | 2246 | `	if( nArg < 1 ){` |
|      - | 2247 | `		/* Missing arguments,return null */` |
|    ! 0 | 2248 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2249 | `		return PH7_OK;` |
|      - | 2250 | `	}` |
|      - | 2251 | `	/* Extract the target string */` |
|  21677 | 2252 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  21677 | 2253 | `	if( nLen < 1 ){` |
|      - | 2254 | `		/* Empty string,return */` |
|   7185 | 2255 | `		ph7_result_string(pCtx,"",0);` |
|   7185 | 2256 | `		return PH7_OK;` |
|      - | 2257 | `	}` |
|      - | 2258 | `	/* Start the trim process */` |
|  14497 | 2259 | `	if( nArg < 2 ){` |
|      - | 2260 | `		SyString sStr;` |
|      - | 2261 | `		/* Remove white spaces and NUL bytes */` |
|  14461 | 2262 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  36391 | 2263 | `		SyStringFullTrimSafe(&sStr);` |
|  14461 | 2264 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   7233 | 2265 | `	}else{` |
|      - | 2266 | `		/* Char list */` |
|      - | 2267 | `		const char *zList;` |
|      - | 2268 | `		int nListlen;` |
|     39 | 2269 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     39 | 2270 | `		if( nListlen < 1 ){` |
|      - | 2271 | `			/* Return the string unchanged */` |
|      6 | 2272 | `			ph7_result_string(pCtx,zString,nLen);` |
|      4 | 2273 | `		}else{` |
|      - | 2274 | `			char aMask[256];` |
|     35 | 2275 | `			const char *zEnd = &zString[nLen];` |
|     35 | 2276 | `			const char *zCur = zString;` |
|     35 | 2277 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2278 | `			/* Left trim */` |
|     91 | 2279 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     59 | 2280 | `				zCur++;` |
|      3 | 2281 | `			}` |
|      - | 2282 | `			/* Right trim */` |
|     85 | 2283 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     53 | 2284 | `				zEnd--;` |
|      3 | 2285 | `			}` |
|     35 | 2286 | `			if( zCur >= zEnd ){` |
|      - | 2287 | `				/* Return the empty string */` |
|    ! 0 | 2288 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2289 | `			}else{` |
|     35 | 2290 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2291 | `			}` |
|      - | 2292 | `		}` |
|      - | 2293 | `	}` |
|  14497 | 2294 | `	return PH7_OK;` |
|  10841 | 2295 | `}` |
|      - | 2296 | `/*` |
|      - | 2297 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 2298 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 2299 | ` * Parameters` |
|      - | 2300 | ` *  $str` |
|      - | 2301 | ` *   The string that will be trimmed.` |
|      - | 2302 | ` * $charlist` |
|      - | 2303 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2304 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2305 | ` *   With .. you can specify a range of characters.` |
|      - | 2306 | ` * Returns.` |
|      - | 2307 | ` *  Thr processed string.` |
|      - | 2308 | ` * NOTE:` |
|      - | 2309 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2310 | ` */` |
|    176 | 2311 | `PH7_PRIVATE int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2312 | `{` |
|    179 | 2313 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"rtrim",1,"$string"); }` |
|      - | 2314 | `	const char *zString;` |
|      - | 2315 | `	int nLen;` |
|    179 | 2316 | `	if( nArg < 1 ){` |
|      - | 2317 | `		/* Missing arguments,return null */` |
|    ! 0 | 2318 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2319 | `		return PH7_OK;` |
|      - | 2320 | `	}` |
|      - | 2321 | `	/* Extract the target string */` |
|    179 | 2322 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    179 | 2323 | `	if( nLen < 1 ){` |
|      - | 2324 | `		/* Empty string,return */` |
|      5 | 2325 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2326 | `		return PH7_OK;` |
|      - | 2327 | `	}` |
|      - | 2328 | `	/* Start the trim process */` |
|    175 | 2329 | `	if( nArg < 2 ){` |
|      - | 2330 | `		SyString sStr;` |
|      - | 2331 | `		/* Remove white spaces and NUL bytes*/` |
|     19 | 2332 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     48 | 2333 | `		SyStringRightTrimSafe(&sStr);` |
|     19 | 2334 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|     10 | 2335 | `	}else{` |
|      - | 2336 | `		/* Char list */` |
|      - | 2337 | `		const char *zList;` |
|      - | 2338 | `		int nListlen;` |
|    157 | 2339 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|    157 | 2340 | `		if( nListlen < 1 ){` |
|      - | 2341 | `			/* Return the string unchanged */` |
|    ! 0 | 2342 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 2343 | `		}else{` |
|      - | 2344 | `			char aMask[256];` |
|    157 | 2345 | `			const char *zEnd = &zString[nLen];` |
|    157 | 2346 | `			const char *zCur = zString;` |
|    157 | 2347 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2348 | `			/* Right trim */` |
|    175 | 2349 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     20 | 2350 | `				zEnd--;` |
|      2 | 2351 | `			}` |
|    157 | 2352 | `			if( zEnd <= zCur ){` |
|      - | 2353 | `				/* Return the empty string */` |
|    ! 0 | 2354 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2355 | `			}else{` |
|    157 | 2356 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2357 | `			}` |
|      - | 2358 | `		}` |
|      - | 2359 | `	}` |
|    175 | 2360 | `	return PH7_OK;` |
|     91 | 2361 | `}` |
|      - | 2362 | `/*` |
|      - | 2363 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 2364 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2365 | ` * Parameters` |
|      - | 2366 | ` *  $str` |
|      - | 2367 | ` *   The string that will be trimmed.` |
|      - | 2368 | ` * $charlist` |
|      - | 2369 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2370 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2371 | ` *   With .. you can specify a range of characters.` |
|      - | 2372 | ` * Returns.` |
|      - | 2373 | ` *  Thr processed string.` |
|      - | 2374 | ` * NOTE:` |
|      - | 2375 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2376 | ` */` |
|     68 | 2377 | `PH7_PRIVATE int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2378 | `{` |
|     73 | 2379 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"ltrim",1,"$string"); }` |
|      - | 2380 | `	const char *zString;` |
|      - | 2381 | `	int nLen;` |
|     73 | 2382 | `	if( nArg < 1 ){` |
|      - | 2383 | `		/* Missing arguments,return null */` |
|    ! 0 | 2384 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2385 | `		return PH7_OK;` |
|      - | 2386 | `	}` |
|      - | 2387 | `	/* Extract the target string */` |
|     73 | 2388 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     73 | 2389 | `	if( nLen < 1 ){` |
|      - | 2390 | `		/* Empty string,return */` |
|     23 | 2391 | `		ph7_result_string(pCtx,"",0);` |
|     23 | 2392 | `		return PH7_OK;` |
|      - | 2393 | `	}` |
|      - | 2394 | `	/* Start the trim process */` |
|     53 | 2395 | `	if( nArg < 2 ){` |
|      - | 2396 | `		SyString sStr;` |
|      - | 2397 | `		/* Remove white spaces and NUL byte */` |
|      5 | 2398 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     13 | 2399 | `		SyStringLeftTrimSafe(&sStr);` |
|      5 | 2400 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      3 | 2401 | `	}else{` |
|      - | 2402 | `		/* Char list */` |
|      - | 2403 | `		const char *zList;` |
|      - | 2404 | `		int nListlen;` |
|     49 | 2405 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     49 | 2406 | `		if( nListlen < 1 ){` |
|      - | 2407 | `			/* Return the string unchanged */` |
|      3 | 2408 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2409 | `		}else{` |
|      - | 2410 | `			char aMask[256];` |
|     47 | 2411 | `			const char *zEnd = &zString[nLen];` |
|     47 | 2412 | `			const char *zCur = zString;` |
|     47 | 2413 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2414 | `			/* Left trim */` |
|    119 | 2415 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     77 | 2416 | `				zCur++;` |
|      5 | 2417 | `			}` |
|     47 | 2418 | `			if( zCur >= zEnd ){` |
|      - | 2419 | `				/* Return the empty string */` |
|    ! 0 | 2420 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2421 | `			}else{` |
|     47 | 2422 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2423 | `			}` |
|      - | 2424 | `		}` |
|      - | 2425 | `	}` |
|     53 | 2426 | `	return PH7_OK;` |
|     39 | 2427 | `}` |
|      - | 2428 | `/*` |
|      - | 2429 | ` * string strtolower(string $str)` |
|      - | 2430 | ` *  Make a string lowercase.` |
|      - | 2431 | ` * Parameters` |
|      - | 2432 | ` *  $str` |
|      - | 2433 | ` *   The input string.` |
|      - | 2434 | ` * Returns.` |
|      - | 2435 | ` *  The lowercased string.` |
|      - | 2436 | ` */` |
|  34604 | 2437 | `PH7_PRIVATE int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2438 | `{` |
|  34609 | 2439 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtolower",1,"$string"); }` |
|      - | 2440 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2441 | `	int nLen;` |
|  34609 | 2442 | `	if( nArg < 1 ){` |
|      - | 2443 | `		/* Missing arguments,return null */` |
|    ! 0 | 2444 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2445 | `		return PH7_OK;` |
|      - | 2446 | `	}` |
|      - | 2447 | `	/* Extract the target string */` |
|  34609 | 2448 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  34609 | 2449 | `	if( nLen < 1 ){` |
|      - | 2450 | `		/* Empty string,return */` |
|      3 | 2451 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2452 | `		return PH7_OK;` |
|      - | 2453 | `	}` |
|      - | 2454 | `	/* Perform the requested operation */` |
|  34607 | 2455 | `	zEnd = &zString[nLen];` |
| 109239 | 2456 | `	for(;;){` |
| 218483 | 2457 | `		if( zString >= zEnd ){` |
|      - | 2458 | `			/* No more input,break immediately */` |
|  34607 | 2459 | `			break;` |
|      - | 2460 | `		}` |
| 183881 | 2461 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2462 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2463 | `			zCur = zString;` |
|    ! 0 | 2464 | `			zString++;` |
|    ! 0 | 2465 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2466 | `				zString++;` |
|    ! 0 | 2467 | `			}` |
|      - | 2468 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2469 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2470 | `		}else{` |
| 183881 | 2471 | `			int c = zString[0];` |
| 183881 | 2472 | `			if( SyisUpper(c) ){` |
| 180815 | 2473 | `				c = SyToLower(zString[0]);` |
|  90405 | 2474 | `			}` |
|      - | 2475 | `			/* Append character */` |
| 183881 | 2476 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2477 | `			/* Advance the cursor */` |
| 183881 | 2478 | `			zString++;` |
|      - | 2479 | `		}` |
|      5 | 2480 | `	}` |
|  34607 | 2481 | `	return PH7_OK;` |
|  17307 | 2482 | `}` |
|      - | 2483 | `/*` |
|      - | 2484 | ` * string strtolower(string $str)` |
|      - | 2485 | ` *  Make a string uppercase.` |
|      - | 2486 | ` * Parameters` |
|      - | 2487 | ` *  $str` |
|      - | 2488 | ` *   The input string.` |
|      - | 2489 | ` * Returns.` |
|      - | 2490 | ` *  The uppercased string.` |
|      - | 2491 | ` */` |
|     82 | 2492 | `PH7_PRIVATE int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2493 | `{` |
|     87 | 2494 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtoupper",1,"$string"); }` |
|      - | 2495 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2496 | `	int nLen;` |
|     87 | 2497 | `	if( nArg < 1 ){` |
|      - | 2498 | `		/* Missing arguments,return null */` |
|    ! 0 | 2499 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2500 | `		return PH7_OK;` |
|      - | 2501 | `	}` |
|      - | 2502 | `	/* Extract the target string */` |
|     87 | 2503 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     87 | 2504 | `	if( nLen < 1 ){` |
|      - | 2505 | `		/* Empty string,return */` |
|      3 | 2506 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2507 | `		return PH7_OK;` |
|      - | 2508 | `	}` |
|      - | 2509 | `	/* Perform the requested operation */` |
|     85 | 2510 | `	zEnd = &zString[nLen];` |
|    158 | 2511 | `	for(;;){` |
|    321 | 2512 | `		if( zString >= zEnd ){` |
|      - | 2513 | `			/* No more input,break immediately */` |
|     85 | 2514 | `			break;` |
|      - | 2515 | `		}` |
|    241 | 2516 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2517 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2518 | `			zCur = zString;` |
|    ! 0 | 2519 | `			zString++;` |
|    ! 0 | 2520 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2521 | `				zString++;` |
|    ! 0 | 2522 | `			}` |
|      - | 2523 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2524 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2525 | `		}else{` |
|    241 | 2526 | `			int c = zString[0];` |
|    241 | 2527 | `			if( SyisLower(c) ){` |
|    224 | 2528 | `				c = SyToUpper(zString[0]);` |
|    110 | 2529 | `			}` |
|      - | 2530 | `			/* Append character */` |
|    241 | 2531 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2532 | `			/* Advance the cursor */` |
|    241 | 2533 | `			zString++;` |
|      - | 2534 | `		}` |
|      5 | 2535 | `	}` |
|     85 | 2536 | `	return PH7_OK;` |
|     46 | 2537 | `}` |
|      - | 2538 | `/*` |
|      - | 2539 | ` * string ucfirst(string $str)` |
|      - | 2540 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2541 | ` *  character is alphabetic.` |
|      - | 2542 | ` * Parameters` |
|      - | 2543 | ` *  $str` |
|      - | 2544 | ` *   The input string.` |
|      - | 2545 | ` * Returns.` |
|      - | 2546 | ` *  The processed string.` |
|      - | 2547 | ` */` |
|      4 | 2548 | `PH7_PRIVATE int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2549 | `{` |
|      - | 2550 | `	const char *zString,*zEnd;` |
|      - | 2551 | `	int nLen,c;` |
|      5 | 2552 | `	if( nArg < 1 ){` |
|      - | 2553 | `		/* Missing arguments,return null */` |
|    ! 0 | 2554 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2555 | `		return PH7_OK;` |
|      - | 2556 | `	}` |
|      - | 2557 | `	/* Extract the target string */` |
|      5 | 2558 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2559 | `	if( nLen < 1 ){` |
|      - | 2560 | `		/* Empty string,return */` |
|      3 | 2561 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2562 | `		return PH7_OK;` |
|      - | 2563 | `	}` |
|      - | 2564 | `	/* Perform the requested operation */` |
|      3 | 2565 | `	zEnd = &zString[nLen];` |
|      3 | 2566 | `	c = zString[0];` |
|      3 | 2567 | `	if( SyisLower(c) ){` |
|      3 | 2568 | `		c = SyToUpper(c);` |
|      1 | 2569 | `	}` |
|      - | 2570 | `	/* Append the first character */` |
|      3 | 2571 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2572 | `	zString++;` |
|      3 | 2573 | `	if( zString < zEnd ){` |
|      - | 2574 | `		/* Append the rest of the input verbatim */` |
|      3 | 2575 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2576 | `	}` |
|      3 | 2577 | `	return PH7_OK;` |
|      3 | 2578 | `}` |
|      - | 2579 | `/*` |
|      - | 2580 | ` * string lcfirst(string $str)` |
|      - | 2581 | ` *  Make a string's first character lowercase.` |
|      - | 2582 | ` * Parameters` |
|      - | 2583 | ` *  $str` |
|      - | 2584 | ` *   The input string.` |
|      - | 2585 | ` * Returns.` |
|      - | 2586 | ` *  The processed string.` |
|      - | 2587 | ` */` |
|      4 | 2588 | `PH7_PRIVATE int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2589 | `{` |
|      - | 2590 | `	const char *zString,*zEnd;` |
|      - | 2591 | `	int nLen,c;` |
|      5 | 2592 | `	if( nArg < 1 ){` |
|      - | 2593 | `		/* Missing arguments,return null */` |
|    ! 0 | 2594 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2595 | `		return PH7_OK;` |
|      - | 2596 | `	}` |
|      - | 2597 | `	/* Extract the target string */` |
|      5 | 2598 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2599 | `	if( nLen < 1 ){` |
|      - | 2600 | `		/* Empty string,return */` |
|      3 | 2601 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2602 | `		return PH7_OK;` |
|      - | 2603 | `	}` |
|      - | 2604 | `	/* Perform the requested operation */` |
|      3 | 2605 | `	zEnd = &zString[nLen];` |
|      3 | 2606 | `	c = zString[0];` |
|      3 | 2607 | `	if( SyisUpper(c) ){` |
|      3 | 2608 | `		c = SyToLower(c);` |
|      1 | 2609 | `	}` |
|      - | 2610 | `	/* Append the first character */` |
|      3 | 2611 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2612 | `	zString++;` |
|      3 | 2613 | `	if( zString < zEnd ){` |
|      - | 2614 | `		/* Append the rest of the input verbatim */` |
|      3 | 2615 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2616 | `	}` |
|      3 | 2617 | `	return PH7_OK;` |
|      3 | 2618 | `}` |
|      - | 2619 | `/*` |
|      - | 2620 | ` * int ord(string $string)` |
|      - | 2621 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 2622 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 2623 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 2624 | ` * Parameters` |
|      - | 2625 | ` *  $string` |
|      - | 2626 | ` *   The input string.` |
|      - | 2627 | ` * Returns` |
|      - | 2628 | ` *  The ASCII value as an integer.` |
|      - | 2629 | ` */` |
|    308 | 2630 | `PH7_PRIVATE int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2631 | `{` |
|      - | 2632 | `	const char *zString;` |
|      - | 2633 | `	int nLen,c;` |
|      - | 2634 | `	/* PHP requires exactly one argument. */` |
|    312 | 2635 | `	if( nArg != 1 ){` |
|    ! 0 | 2636 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2637 | `			"ArgumentCountError",` |
|      - | 2638 | `			"ord() expects exactly 1 argument, %d given",` |
|    ! 0 | 2639 | `			nArg` |
|      - | 2640 | `			);` |
|      - | 2641 | `	}` |
|      - | 2642 | `	/* php only DEPRECATES null here; PHL rejects it. */` |
|    312 | 2643 | `	if( ph7_value_is_null(apArg[0]) ){` |
|    ! 0 | 2644 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 2645 | `			"ord(): Argument #1 ($character) must be of type string, null given"` |
|      - | 2646 | `			);` |
|      - | 2647 | `	}` |
|      - | 2648 | `	/* Extract the target string */` |
|    312 | 2649 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    312 | 2650 | `	if( nLen < 1 ){` |
|      - | 2651 | `		/* php only DEPRECATES an empty string here; PHL rejects it. */` |
|      3 | 2652 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2653 | `			"ord(): Argument #1 ($character) must not be empty"` |
|      - | 2654 | `			);` |
|      - | 2655 | `	}` |
|      - | 2656 | `	/* A string longer than one byte: php DEPRECATES it; PHL rejects it. */` |
|    310 | 2657 | `	if( nLen > 1 ){` |
|      3 | 2658 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2659 | `			"ord(): Argument #1 ($character) must be a single byte, use ord($str[0]) instead"` |
|      - | 2660 | `			);` |
|      - | 2661 | `	}` |
|      - | 2662 | `	/* Extract the ASCII value of the first character */` |
|    308 | 2663 | `	c = (unsigned char)zString[0];` |
|      - | 2664 | `	/* Return that value */` |
|    308 | 2665 | `	ph7_result_int(pCtx,c);` |
|    308 | 2666 | `	return PH7_OK;` |
|    158 | 2667 | `}` |
|      - | 2668 | `/*` |
|      - | 2669 | ` * string chr(int $codepoint)` |
|      - | 2670 | ` *  Returns a one-character string containing the character specified` |
|      - | 2671 | ` *  by the given codepoint, which must be in the [0, 255] range.` |
|      - | 2672 | ` * Parameters` |
|      - | 2673 | ` *  $codepoint` |
|      - | 2674 | ` *   An integer codepoint in [0, 255]. php merely deprecates values` |
|      - | 2675 | ` *   outside that range (constraining them with % 256); PHL rejects` |
|      - | 2676 | ` *   them with a ValueError (scope policy).` |
|      - | 2677 | ` * Returns` |
|      - | 2678 | ` *  A single-character string.` |
|      - | 2679 | ` */` |
|   9182 | 2680 | `PH7_PRIVATE int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2681 | `{` |
|      - | 2682 | `	int c;` |
|      - | 2683 | `	unsigned char ch;` |
|      - | 2684 | `	/* PHP requires exactly one argument. */` |
|   9186 | 2685 | `	if( nArg != 1 ){` |
|    ! 0 | 2686 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2687 | `			"ArgumentCountError",` |
|      - | 2688 | `			"chr() expects exactly 1 argument, %d given",` |
|    ! 0 | 2689 | `			nArg` |
|      - | 2690 | `			);` |
|      - | 2691 | `	}` |
|      - | 2692 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 2693 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 2694 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 2695 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|   9186 | 2696 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      3 | 2697 | `		double d = ph7_value_to_double(apArg[0]);` |
|      3 | 2698 | `		if( d != (double)(sxi64)d ){` |
|      - | 2699 | `			/* php only DEPRECATES a lossy float->int here; PHL rejects it. */` |
|      3 | 2700 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 2701 | `				"chr(): Argument #1 ($codepoint) must be of type int, float given");` |
|      - | 2702 | `		}` |
|    ! 0 | 2703 | `	}` |
|      - | 2704 | `	/* Extract the codepoint. */` |
|   9184 | 2705 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 2706 | `	/* php only DEPRECATES an out-of-range codepoint (constraining it with % 256);` |
|      - | 2707 | `	 * PHL targets php's non-deprecated surface and rejects it loudly, matching the` |
|      - | 2708 | `	 * lossy-float branch above. This was the last engine site still emitting` |
|      - | 2709 | `	 * E_DEPRECATED — the scope policy says none remain. */` |
|   9184 | 2710 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 2711 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2712 | `			"chr(): Argument #1 ($codepoint) must be between 0 and 255");` |
|      - | 2713 | `	}` |
|      - | 2714 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 2715 | `	 * when taking the address of a wider int. */` |
|   9180 | 2716 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 2717 | `	/* Return the specified character */` |
|   9180 | 2718 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|   9180 | 2719 | `	return PH7_OK;` |
|   4595 | 2720 | `}` |
|      - | 2721 | `/*` |
|      - | 2722 | ` * Binary to hex consumer callback.` |
|      - | 2723 | ` * This callback is the default consumer used by the hash functions` |
|      - | 2724 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 2725 | ` */` |
|   3276 | 2726 | `PH7_PRIVATE int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      3 | 2727 | `{` |
|      - | 2728 | `	/* Append hex chunk verbatim */` |
|   3279 | 2729 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   3279 | 2730 | `	return SXRET_OK;` |
|      3 | 2731 | `}` |
|      - | 2732 |  |
|      - | 2733 | `/*` |
|      - | 2734 | ` * string bin2hex(string $str)` |
|      - | 2735 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 2736 | ` * Parameters` |
|      - | 2737 | ` *  $str` |
|      - | 2738 | ` *   The input string.` |
|      - | 2739 | ` * Returns.` |
|      - | 2740 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 2741 | ` */` |
|    192 | 2742 | `PH7_PRIVATE int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2743 | `{` |
|      - | 2744 | `	const char *zString;` |
|      - | 2745 | `	int nLen;` |
|      - | 2746 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|    195 | 2747 | `	if( nArg != 1 ){` |
|    ! 0 | 2748 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2749 | `			"ArgumentCountError",` |
|      - | 2750 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|    ! 0 | 2751 | `			nArg` |
|      - | 2752 | `			);` |
|      - | 2753 | `	}` |
|      - | 2754 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 2755 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 2756 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 2757 | `	 */` |
|    291 | 2758 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|     96 | 2759 | `		( ph7_value_is_object(apArg[0]) &&` |
|    ! 0 | 2760 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|    ! 0 | 2761 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|    ! 0 | 2762 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 2763 | `		)` |
|      - | 2764 | `	){` |
|    ! 0 | 2765 | `		const char *zType = ph7_type_name(apArg[0]);` |
|    ! 0 | 2766 | `		if( ph7_value_is_object(apArg[0]) ){` |
|    ! 0 | 2767 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    ! 0 | 2768 | `			if( pInst && pInst->pClass ){` |
|    ! 0 | 2769 | `				zType = SyStringData(&pInst->pClass->sName);` |
|    ! 0 | 2770 | `			}` |
|    ! 0 | 2771 | `		}` |
|    ! 0 | 2772 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2773 | `			"TypeError",` |
|      - | 2774 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 2775 | `			zType` |
|      - | 2776 | `			);` |
|      - | 2777 | `	}` |
|      - | 2778 | `	/* Extract the target string */` |
|    195 | 2779 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    195 | 2780 | `	if( nLen < 1 ){` |
|      - | 2781 | `		/* Empty string,return */` |
|     15 | 2782 | `		ph7_result_string(pCtx,"",0);` |
|     15 | 2783 | `		return PH7_OK;` |
|      - | 2784 | `	}` |
|      - | 2785 | `	/* Perform the requested operation */` |
|    181 | 2786 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|    181 | 2787 | `	return PH7_OK;` |
|     99 | 2788 | `}` |
|      - | 2789 |  |
|      - | 2790 | `/* Search callback signature */` |
|      - | 2791 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 2792 | `/*` |
|      - | 2793 | ` * Case-insensitive pattern match.` |
|      - | 2794 | ` * Brute force is the default search method used here.` |
|      - | 2795 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 2796 | ` * well for short/medium texts on modern hardware.` |
|      - | 2797 | ` */` |
|    276 | 2798 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      2 | 2799 | `{` |
|    278 | 2800 | `	const char *zpIn = (const char *)pPattern;` |
|    278 | 2801 | `	const char *zIn = (const char *)pText;` |
|    278 | 2802 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    278 | 2803 | `	const char *zEnd = &zIn[nLen];` |
|      - | 2804 | `	const char *zPtr,*zPtr2;` |
|      - | 2805 | `	int c,d;` |
|    278 | 2806 | `	if( iPatLen > nLen ){` |
|      - | 2807 | `		/* Don't bother processing */` |
|     35 | 2808 | `		return SXERR_NOTFOUND;` |
|      - | 2809 | `	}` |
|    784 | 2810 | `	for(;;){` |
|   1570 | 2811 | `		if( zIn >= zEnd ){` |
|    186 | 2812 | `			break;` |
|      - | 2813 | `		}` |
|   1386 | 2814 | `		c = SyToLower(zIn[0]);` |
|   1386 | 2815 | `		d = SyToLower(zpIn[0]);` |
|   1386 | 2816 | `		if( c == d ){` |
|    200 | 2817 | `			zPtr   = &zIn[1];` |
|    200 | 2818 | `			zPtr2  = &zpIn[1];` |
|    150 | 2819 | `			for(;;){` |
|    302 | 2820 | `				if( zPtr2 >= zpEnd ){` |
|      - | 2821 | `					/* Pattern found */` |
|     59 | 2822 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     59 | 2823 | `					return SXRET_OK;` |
|      - | 2824 | `				}` |
|    244 | 2825 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 2826 | `					break;` |
|      - | 2827 | `				}` |
|    244 | 2828 | `				c = SyToLower(zPtr[0]);` |
|    244 | 2829 | `				d = SyToLower(zPtr2[0]);` |
|    244 | 2830 | `				if( c != d ){` |
|    142 | 2831 | `					break;` |
|      - | 2832 | `				}` |
|    103 | 2833 | `				zPtr++; zPtr2++;` |
|      1 | 2834 | `			}` |
|     70 | 2835 | `		}` |
|   1328 | 2836 | `		zIn++;` |
|      2 | 2837 | `	}` |
|      - | 2838 | `	/* Pattern not found */` |
|    186 | 2839 | `	return SXERR_NOTFOUND;` |
|    140 | 2840 | `}` |
|      - | 2841 | `/*` |
|      - | 2842 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2843 | ` *  Find the first occurrence of a string.` |
|      - | 2844 | ` * Parameters` |
|      - | 2845 | ` *  $haystack` |
|      - | 2846 | ` *   The input string.` |
|      - | 2847 | ` * $needle` |
|      - | 2848 | ` *   Search pattern (must be a string).` |
|      - | 2849 | ` * $before_needle` |
|      - | 2850 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2851 | ` *   of the needle (excluding the needle).` |
|      - | 2852 | ` * Return` |
|      - | 2853 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2854 | ` */` |
|     12 | 2855 | `PH7_PRIVATE int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2856 | `{` |
|     13 | 2857 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2858 | `	const char *zBlob,*zPattern;` |
|      - | 2859 | `	int nLen,nPatLen;` |
|      - | 2860 | `	sxu32 nOfft;` |
|      - | 2861 | `	sxi32 rc;` |
|     13 | 2862 | `	if( nArg < 2 ){` |
|      - | 2863 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2864 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2865 | `		return PH7_OK;` |
|      - | 2866 | `	}` |
|      - | 2867 | `	/* Extract the needle and the haystack */` |
|     13 | 2868 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 2869 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     13 | 2870 | `	nOfft = 0; /* cc warning */` |
|     13 | 2871 | `	if( nPatLen < 1 ){` |
|      - | 2872 | `		/* php 8: the empty needle matches at position 0, so the whole haystack` |
|      - | 2873 | `		 * is returned (and nothing at all when $before_needle is set). */` |
|      7 | 2874 | `		if( nArg > 2 && ph7_value_to_bool(apArg[2]) ){` |
|      3 | 2875 | `			ph7_result_string(pCtx,"",0);` |
|      2 | 2876 | `		}else{` |
|      5 | 2877 | `			ph7_result_string(pCtx,zBlob,nLen);` |
|      - | 2878 | `		}` |
|      7 | 2879 | `		return PH7_OK;` |
|      - | 2880 | `	}` |
|      7 | 2881 | `	if( nLen > 0 ){` |
|      7 | 2882 | `		int before = 0;` |
|      - | 2883 | `		/* Perform the lookup */` |
|      7 | 2884 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      7 | 2885 | `		if( rc != SXRET_OK ){` |
|      - | 2886 | `			/* Pattern not found,return FALSE */` |
|      3 | 2887 | `			ph7_result_bool(pCtx,0);` |
|      3 | 2888 | `			return PH7_OK;` |
|      - | 2889 | `		}` |
|      - | 2890 | `		/* Return the portion of the string */` |
|      5 | 2891 | `		if( nArg > 2 ){` |
|      3 | 2892 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2893 | `		}` |
|      5 | 2894 | `		if( before ){` |
|      3 | 2895 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2896 | `		}else{` |
|      3 | 2897 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2898 | `		}` |
|      3 | 2899 | `	}else{` |
|    ! 0 | 2900 | `		ph7_result_bool(pCtx,0);` |
|      - | 2901 | `	}` |
|      5 | 2902 | `	return PH7_OK;` |
|      7 | 2903 | `}` |
|      - | 2904 | `/*` |
|      - | 2905 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2906 | ` *  Case-insensitive strstr().` |
|      - | 2907 | ` * Parameters` |
|      - | 2908 | ` *  $haystack` |
|      - | 2909 | ` *   The input string.` |
|      - | 2910 | ` * $needle` |
|      - | 2911 | ` *   Search pattern (must be a string).` |
|      - | 2912 | ` * $before_needle` |
|      - | 2913 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2914 | ` *   of the needle (excluding the needle).` |
|      - | 2915 | ` * Return` |
|      - | 2916 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2917 | ` */` |
|      6 | 2918 | `PH7_PRIVATE int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2919 | `{` |
|      7 | 2920 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2921 | `	const char *zBlob,*zPattern;` |
|      - | 2922 | `	int nLen,nPatLen;` |
|      - | 2923 | `	sxu32 nOfft;` |
|      - | 2924 | `	sxi32 rc;` |
|      7 | 2925 | `	if( nArg < 2 ){` |
|      - | 2926 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2927 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2928 | `		return PH7_OK;` |
|      - | 2929 | `	}` |
|      - | 2930 | `	/* Extract the needle and the haystack */` |
|      7 | 2931 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 2932 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 2933 | `	nOfft = 0; /* cc warning */` |
|      7 | 2934 | `	if( nPatLen < 1 ){` |
|      - | 2935 | `		/* php 8: the empty needle matches at position 0, so the whole haystack` |
|      - | 2936 | `		 * is returned (and nothing at all when $before_needle is set). */` |
|      3 | 2937 | `		if( nArg > 2 && ph7_value_to_bool(apArg[2]) ){` |
|    ! 0 | 2938 | `			ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2939 | `		}else{` |
|      3 | 2940 | `			ph7_result_string(pCtx,zBlob,nLen);` |
|      - | 2941 | `		}` |
|      3 | 2942 | `		return PH7_OK;` |
|      - | 2943 | `	}` |
|      5 | 2944 | `	if( nLen > 0 ){` |
|      5 | 2945 | `		int before = 0;` |
|      - | 2946 | `		/* Perform the lookup */` |
|      5 | 2947 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2948 | `		if( rc != SXRET_OK ){` |
|      - | 2949 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2950 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2951 | `			return PH7_OK;` |
|      - | 2952 | `		}` |
|      - | 2953 | `		/* Return the portion of the string */` |
|      5 | 2954 | `		if( nArg > 2 ){` |
|      3 | 2955 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2956 | `		}` |
|      5 | 2957 | `		if( before ){` |
|      3 | 2958 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2959 | `		}else{` |
|      3 | 2960 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2961 | `		}` |
|      3 | 2962 | `	}else{` |
|    ! 0 | 2963 | `		ph7_result_bool(pCtx,0);` |
|      - | 2964 | `	}` |
|      5 | 2965 | `	return PH7_OK;` |
|      4 | 2966 | `}` |
|      - | 2967 | `/*` |
|      - | 2968 | ` * Resolve the $offset argument shared by strpos()/stripos().` |
|      - | 2969 | ` *` |
|      - | 2970 | ` * php requires -strlen($haystack) <= $offset <= strlen($haystack) and throws` |
|      - | 2971 | ` * ValueError otherwise; a negative offset counts back from the end. PHL used to` |
|      - | 2972 | ` * negate a negative offset and silently clamp an out-of-range one to zero, so` |
|      - | 2973 | ` * strpos("Hello","l",100) answered 2 where php raises — an argument error` |
|      - | 2974 | ` * turned into a wrong answer.` |
|      - | 2975 | ` *` |
|      - | 2976 | ` * On success *pnStart receives the resolved non-negative offset.` |
|      - | 2977 | ` */` |
|     24 | 2978 | `static sxi32 StrSearchOffset(` |
|      - | 2979 | `	ph7_context *pCtx,` |
|      - | 2980 | `	ph7_value *pArg,` |
|      - | 2981 | `	int nLen,` |
|      - | 2982 | `	const char *zFunc,` |
|      - | 2983 | `	int *pnStart` |
|      - | 2984 | `	)` |
|      2 | 2985 | `{` |
|     26 | 2986 | `	ph7_int64 iOfft = ph7_value_to_int64(pArg);` |
|      - | 2987 | `	/* Compare without negating iOfft: -INT64_MIN would overflow. */` |
|     26 | 2988 | `	if( iOfft < 0 ? (iOfft < -(ph7_int64)nLen) : (iOfft > (ph7_int64)nLen) ){` |
|      8 | 2989 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      4 | 2990 | `			"%s(): Argument #3 ($offset) must be contained in argument #1 ($haystack)",zFunc);` |
|      - | 2991 | `	}` |
|     26 | 2992 | `	*pnStart = (int)(iOfft < 0 ? (ph7_int64)nLen + iOfft : iOfft);` |
|     26 | 2993 | `	return PH7_OK;` |
|     18 | 2994 | `}` |
|      - | 2995 | `/*` |
|      - | 2996 | ` * Resolve the window of match START positions for strrpos()/strripos().` |
|      - | 2997 | ` *` |
|      - | 2998 | ` * php's rule is asymmetric in the sign of $offset: a non-negative offset is a` |
|      - | 2999 | ` * LOWER bound on where the match may start, while a negative one is an UPPER` |
|      - | 3000 | ` * bound counted back from the end of the haystack (zend_memnrstr). The range` |
|      - | 3001 | ` * check is the same as StrSearchOffset()'s.` |
|      - | 3002 | ` *` |
|      - | 3003 | ` * On success the closed interval [*pnMin,*pnMax] holds every position at which` |
|      - | 3004 | ` * a match is allowed to begin; it is empty (max < min) when the needle cannot` |
|      - | 3005 | ` * fit, which the caller reports as FALSE.` |
|      - | 3006 | ` */` |
|    104 | 3007 | `static sxi32 StrRSearchWindow(` |
|      - | 3008 | `	ph7_context *pCtx,` |
|      - | 3009 | `	ph7_value *pArg, /* The $offset argument, or NULL when it was omitted */` |
|      - | 3010 | `	int nLen,` |
|      - | 3011 | `	int nPatLen,` |
|      - | 3012 | `	const char *zFunc,` |
|      - | 3013 | `	int *pnMin,` |
|      - | 3014 | `	int *pnMax` |
|      - | 3015 | `	)` |
|      1 | 3016 | `{` |
|    105 | 3017 | `	int nMin = 0;` |
|    105 | 3018 | `	int nMax = nLen - nPatLen;` |
|    105 | 3019 | `	if( pArg ){` |
|     47 | 3020 | `		ph7_int64 iOfft = ph7_value_to_int64(pArg);` |
|     47 | 3021 | `		if( iOfft < 0 ? (iOfft < -(ph7_int64)nLen) : (iOfft > (ph7_int64)nLen) ){` |
|     33 | 3022 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|     14 | 3023 | `				"%s(): Argument #3 ($offset) must be contained in argument #1 ($haystack)",zFunc);` |
|      - | 3024 | `		}` |
|     29 | 3025 | `		if( iOfft < 0 ){` |
|     15 | 3026 | `			int nLimit = nLen + (int)iOfft;` |
|     15 | 3027 | `			if( nMax > nLimit ){` |
|     15 | 3028 | `				nMax = nLimit;` |
|      7 | 3029 | `			}` |
|      8 | 3030 | `		}else{` |
|     15 | 3031 | `			nMin = (int)iOfft;` |
|      - | 3032 | `		}` |
|     14 | 3033 | `	}` |
|     87 | 3034 | `	*pnMin = nMin;` |
|     87 | 3035 | `	*pnMax = nMax;` |
|     87 | 3036 | `	return PH7_OK;` |
|     48 | 3037 | `}` |
|      - | 3038 | `/*` |
|      - | 3039 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3040 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 3041 | ` * Parameters` |
|      - | 3042 | ` *  $haystack` |
|      - | 3043 | ` *   The input string.` |
|      - | 3044 | ` * $needle` |
|      - | 3045 | ` *   Search pattern (must be a string).` |
|      - | 3046 | ` * $offset` |
|      - | 3047 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3048 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3049 | ` *   of haystack.` |
|      - | 3050 | ` * Return` |
|      - | 3051 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3052 | ` */` |
|   1774 | 3053 | `PH7_PRIVATE int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3054 | `{` |
|   1779 | 3055 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strpos",1,"$haystack"); }` |
|   1779 | 3056 | `	if( nArg > 1 ){ StrNullArgNotice(pCtx,apArg[1],"strpos",2,"$needle"); }` |
|   1779 | 3057 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3058 | `	const char *zBlob,*zPattern;` |
|      - | 3059 | `	int nLen,nPatLen,nStart;` |
|      - | 3060 | `	sxu32 nOfft;` |
|      - | 3061 | `	sxi32 rc;` |
|   1779 | 3062 | `	if( nArg < 2 ){` |
|      - | 3063 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3064 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3065 | `		return PH7_OK;` |
|      - | 3066 | `	}` |
|      - | 3067 | `	/* Extract the needle and the haystack */` |
|   1779 | 3068 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|   1779 | 3069 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|   1779 | 3070 | `	nOfft = 0; /* cc warning */` |
|   1779 | 3071 | `	nStart = 0;` |
|      - | 3072 | `	/* Peek the starting offset if available */` |
|   1779 | 3073 | `	if( nArg > 2 ){` |
|     22 | 3074 | `		rc = StrSearchOffset(pCtx,apArg[2],nLen,"strpos",&nStart);` |
|     22 | 3075 | `		if( rc != PH7_OK ){` |
|    ! 0 | 3076 | `			return rc;` |
|      - | 3077 | `		}` |
|     10 | 3078 | `	}` |
|   1779 | 3079 | `	if( nPatLen < 1 ){` |
|      - | 3080 | `		/* php 8 treats the empty needle as matching at the search offset. */` |
|     11 | 3081 | `		ph7_result_int64(pCtx,(ph7_int64)nStart);` |
|     11 | 3082 | `		return PH7_OK;` |
|      - | 3083 | `	}` |
|   1769 | 3084 | `	zBlob += nStart;` |
|   1769 | 3085 | `	nLen -= nStart;` |
|   1769 | 3086 | `	if( nLen > 0 ){` |
|      - | 3087 | `		/* Perform the lookup */` |
|   1767 | 3088 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|   1767 | 3089 | `		if( rc != SXRET_OK ){` |
|      - | 3090 | `			/* Pattern not found,return FALSE */` |
|    823 | 3091 | `			ph7_result_bool(pCtx,0);` |
|    823 | 3092 | `			return PH7_OK;` |
|      - | 3093 | `		}` |
|      - | 3094 | `		/* Return the pattern position */` |
|    949 | 3095 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|    477 | 3096 | `	}else{` |
|      3 | 3097 | `		ph7_result_bool(pCtx,0);` |
|      - | 3098 | `	}` |
|    951 | 3099 | `	return PH7_OK;` |
|    892 | 3100 | `}` |
|      - | 3101 | `/*` |
|      - | 3102 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|      - | 3103 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|      - | 3104 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|      - | 3105 | ` * TypeError for arrays, resources, and objects without __toString.` |
|      - | 3106 | ` *` |
|      - | 3107 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|      - | 3108 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|      - | 3109 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|      - | 3110 | ` *` |
|      - | 3111 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|      - | 3112 | ` * is valid until pTmp is released or pArg is mutated.` |
|      - | 3113 | ` */` |
|    648 | 3114 | `static sxi32 StrPredicateResolveArg(` |
|      - | 3115 | `	ph7_context *pCtx,` |
|      - | 3116 | `	ph7_value *pArg,` |
|      - | 3117 | `	const char *zFunc,` |
|      - | 3118 | `	int iArgNum,` |
|      - | 3119 | `	const char *zParamName,` |
|      - | 3120 | `	const char *zTypeStr, /* Declared type in the TypeError, e.g. "string" / "?string" */` |
|      - | 3121 | `	const char *zNullMsg,` |
|      - | 3122 | `	ph7_value *pTmp,` |
|      - | 3123 | `	const char **pzOut,` |
|      - | 3124 | `	int *pnOut` |
|      3 | 3125 | `){` |
|    324 | 3126 | `	SXUNUSED(zNullMsg); /* php's deprecation text — PHL rejects null instead of coercing */` |
|    651 | 3127 | `	if( ph7_value_is_null(pArg) ){` |
|      - | 3128 | `		/* php only DEPRECATES null here; PHL rejects it with the TypeError php will` |
|      - | 3129 | `		 * eventually raise. */` |
|    ! 0 | 3130 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 3131 | `			"%s(): Argument #%d (%s) must be of type %s, null given",` |
|    ! 0 | 3132 | `			zFunc,iArgNum,zParamName,zTypeStr);` |
|      - | 3133 | `	}` |
|    999 | 3134 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    648 | 3135 | `	    ( ph7_value_is_object(pArg) &&` |
|     72 | 3136 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     48 | 3137 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     24 | 3138 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 3139 | `	    )` |
|      - | 3140 | `	){` |
|    ! 0 | 3141 | `		const char *zType = ph7_type_name(pArg);` |
|    ! 0 | 3142 | `		if( ph7_value_is_object(pArg) ){` |
|    ! 0 | 3143 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|    ! 0 | 3144 | `			if( pInst && pInst->pClass ){` |
|    ! 0 | 3145 | `				zType = SyStringData(&pInst->pClass->sName);` |
|    ! 0 | 3146 | `			}` |
|    ! 0 | 3147 | `		}` |
|    ! 0 | 3148 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3149 | `			"TypeError",` |
|      - | 3150 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|    ! 0 | 3151 | `			zFunc, iArgNum, zParamName, zTypeStr, zType` |
|      - | 3152 | `			);` |
|      - | 3153 | `	}` |
|    651 | 3154 | `	if( ph7_value_is_object(pArg) ){` |
|     49 | 3155 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     49 | 3156 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 3157 | `			"__toString",sizeof("__toString")-1);` |
|     49 | 3158 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|     49 | 3159 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|     49 | 3160 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|     49 | 3161 | `		return PH7_OK;` |
|      - | 3162 | `	}` |
|    603 | 3163 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|    603 | 3164 | `	return PH7_OK;` |
|    327 | 3165 | `}` |
|      - | 3166 | `/*` |
|      - | 3167 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 3168 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 3169 | ` * Return` |
|      - | 3170 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 3171 | ` */` |
|     84 | 3172 | `PH7_PRIVATE int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3173 | `{` |
|      - | 3174 | `	const char *zHaystack,*zNeedle;` |
|      - | 3175 | `	int nHayLen,nNeedleLen;` |
|      - | 3176 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3177 | `	sxi32 rc;` |
|     86 | 3178 | `	if( nArg != 2 ){` |
|    ! 0 | 3179 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3180 | `			"ArgumentCountError",` |
|      - | 3181 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|    ! 0 | 3182 | `			nArg` |
|      - | 3183 | `			);` |
|      - | 3184 | `	}` |
|     86 | 3185 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     86 | 3186 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     86 | 3187 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack","string",` |
|      - | 3188 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|      - | 3189 | `		"of type string is deprecated",` |
|      - | 3190 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     86 | 3191 | `	if( rc != PH7_OK ) goto out;` |
|     86 | 3192 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle","string",` |
|      - | 3193 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|      - | 3194 | `		"of type string is deprecated",` |
|      - | 3195 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     86 | 3196 | `	if( rc != PH7_OK ) goto out;` |
|     86 | 3197 | `	if( nNeedleLen < 1 ){` |
|     11 | 3198 | `		ph7_result_bool(pCtx,1);` |
|     81 | 3199 | `	}else if( nHayLen < nNeedleLen ){` |
|      7 | 3200 | `		ph7_result_bool(pCtx,0);` |
|      4 | 3201 | `	}else{` |
|    104 | 3202 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|     34 | 3203 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|     70 | 3204 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|      - | 3205 | `	}` |
|     86 | 3206 | `	rc = PH7_OK;` |
|     42 | 3207 | `out:` |
|     86 | 3208 | `	PH7_MemObjRelease(&sHayTmp);` |
|     86 | 3209 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     86 | 3210 | `	return rc;` |
|     44 | 3211 | `}` |
|      - | 3212 | `/*` |
|      - | 3213 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 3214 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 3215 | ` * Return` |
|      - | 3216 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 3217 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3218 | ` */` |
|     56 | 3219 | `PH7_PRIVATE int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3220 | `{` |
|      - | 3221 | `	const char *zHaystack,*zNeedle;` |
|      - | 3222 | `	int nHayLen,nNeedleLen;` |
|      - | 3223 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3224 | `	sxi32 rc;` |
|     58 | 3225 | `	if( nArg != 2 ){` |
|    ! 0 | 3226 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3227 | `			"ArgumentCountError",` |
|      - | 3228 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|    ! 0 | 3229 | `			nArg` |
|      - | 3230 | `			);` |
|      - | 3231 | `	}` |
|     58 | 3232 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     58 | 3233 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     58 | 3234 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack","string",` |
|      - | 3235 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3236 | `		"of type string is deprecated",` |
|      - | 3237 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     58 | 3238 | `	if( rc != PH7_OK ) goto out;` |
|     58 | 3239 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle","string",` |
|      - | 3240 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3241 | `		"of type string is deprecated",` |
|      - | 3242 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     58 | 3243 | `	if( rc != PH7_OK ) goto out;` |
|     58 | 3244 | `	if( nNeedleLen < 1 ){` |
|     11 | 3245 | `		ph7_result_bool(pCtx,1);` |
|     53 | 3246 | `	}else if( nHayLen < nNeedleLen ){` |
|      7 | 3247 | `		ph7_result_bool(pCtx,0);` |
|      4 | 3248 | `	}else{` |
|     62 | 3249 | `		ph7_result_bool(pCtx,` |
|     40 | 3250 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3251 | `	}` |
|     58 | 3252 | `	rc = PH7_OK;` |
|     28 | 3253 | `out:` |
|     58 | 3254 | `	PH7_MemObjRelease(&sHayTmp);` |
|     58 | 3255 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     58 | 3256 | `	return rc;` |
|     30 | 3257 | `}` |
|      - | 3258 | `/*` |
|      - | 3259 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 3260 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 3261 | ` * Return` |
|      - | 3262 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 3263 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3264 | ` */` |
|     54 | 3265 | `PH7_PRIVATE int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3266 | `{` |
|      - | 3267 | `	const char *zHaystack,*zNeedle;` |
|      - | 3268 | `	int nHayLen,nNeedleLen;` |
|      - | 3269 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3270 | `	sxi32 rc;` |
|     55 | 3271 | `	if( nArg != 2 ){` |
|    ! 0 | 3272 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3273 | `			"ArgumentCountError",` |
|      - | 3274 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|    ! 0 | 3275 | `			nArg` |
|      - | 3276 | `			);` |
|      - | 3277 | `	}` |
|     55 | 3278 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     55 | 3279 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     55 | 3280 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack","string",` |
|      - | 3281 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3282 | `		"of type string is deprecated",` |
|      - | 3283 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     55 | 3284 | `	if( rc != PH7_OK ) goto out;` |
|     55 | 3285 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle","string",` |
|      - | 3286 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3287 | `		"of type string is deprecated",` |
|      - | 3288 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     55 | 3289 | `	if( rc != PH7_OK ) goto out;` |
|     55 | 3290 | `	if( nNeedleLen < 1 ){` |
|     11 | 3291 | `		ph7_result_bool(pCtx,1);` |
|     50 | 3292 | `	}else if( nHayLen < nNeedleLen ){` |
|      7 | 3293 | `		ph7_result_bool(pCtx,0);` |
|      4 | 3294 | `	}else{` |
|     58 | 3295 | `		ph7_result_bool(pCtx,` |
|     38 | 3296 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3297 | `	}` |
|     55 | 3298 | `	rc = PH7_OK;` |
|     27 | 3299 | `out:` |
|     55 | 3300 | `	PH7_MemObjRelease(&sHayTmp);` |
|     55 | 3301 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     55 | 3302 | `	return rc;` |
|     28 | 3303 | `}` |
|      - | 3304 | `/*` |
|      - | 3305 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3306 | ` *  Case-insensitive strpos.` |
|      - | 3307 | ` * Parameters` |
|      - | 3308 | ` *  $haystack` |
|      - | 3309 | ` *   The input string.` |
|      - | 3310 | ` * $needle` |
|      - | 3311 | ` *   Search pattern (must be a string).` |
|      - | 3312 | ` * $offset` |
|      - | 3313 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3314 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3315 | ` *   of haystack.` |
|      - | 3316 | ` * Return` |
|      - | 3317 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3318 | ` */` |
|    198 | 3319 | `PH7_PRIVATE int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3320 | `{` |
|    200 | 3321 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3322 | `	const char *zBlob,*zPattern;` |
|      - | 3323 | `	int nLen,nPatLen,nStart;` |
|      - | 3324 | `	sxu32 nOfft;` |
|      - | 3325 | `	sxi32 rc;` |
|    200 | 3326 | `	if( nArg < 2 ){` |
|      - | 3327 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3328 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3329 | `		return PH7_OK;` |
|      - | 3330 | `	}` |
|      - | 3331 | `	/* Extract the needle and the haystack */` |
|    200 | 3332 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    200 | 3333 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    200 | 3334 | `	nOfft = 0; /* cc warning */` |
|    200 | 3335 | `	nStart = 0;` |
|      - | 3336 | `	/* Peek the starting offset if available */` |
|    200 | 3337 | `	if( nArg > 2 ){` |
|      5 | 3338 | `		rc = StrSearchOffset(pCtx,apArg[2],nLen,"stripos",&nStart);` |
|      5 | 3339 | `		if( rc != PH7_OK ){` |
|    ! 0 | 3340 | `			return rc;` |
|      - | 3341 | `		}` |
|      2 | 3342 | `	}` |
|    200 | 3343 | `	if( nPatLen < 1 ){` |
|      - | 3344 | `		/* php 8 treats the empty needle as matching at the search offset. */` |
|      3 | 3345 | `		ph7_result_int64(pCtx,(ph7_int64)nStart);` |
|      3 | 3346 | `		return PH7_OK;` |
|      - | 3347 | `	}` |
|    198 | 3348 | `	zBlob += nStart;` |
|    198 | 3349 | `	nLen -= nStart;` |
|    198 | 3350 | `	if( nLen > 0 ){` |
|      - | 3351 | `		/* Perform the lookup */` |
|    198 | 3352 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    198 | 3353 | `		if( rc != SXRET_OK ){` |
|      - | 3354 | `			/* Pattern not found,return FALSE */` |
|    184 | 3355 | `			ph7_result_bool(pCtx,0);` |
|    184 | 3356 | `			return PH7_OK;` |
|      - | 3357 | `		}` |
|      - | 3358 | `		/* Return the pattern position */` |
|     15 | 3359 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 3360 | `	}else{` |
|    ! 0 | 3361 | `		ph7_result_bool(pCtx,0);` |
|      - | 3362 | `	}` |
|     15 | 3363 | `	return PH7_OK;` |
|    101 | 3364 | `}` |
|      - | 3365 | `/*` |
|      - | 3366 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3367 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 3368 | ` * Parameters` |
|      - | 3369 | ` *  $haystack` |
|      - | 3370 | ` *   The input string.` |
|      - | 3371 | ` * $needle` |
|      - | 3372 | ` *   Search pattern (must be a string).` |
|      - | 3373 | ` * $offset` |
|      - | 3374 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3375 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3376 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3377 | ` * Return` |
|      - | 3378 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3379 | ` */` |
|     60 | 3380 | `PH7_PRIVATE int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3381 | `{` |
|      - | 3382 | `	const char *zBlob,*zPattern;` |
|     61 | 3383 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3384 | `	int nLen,nPatLen,i;` |
|     61 | 3385 | `	int nMin = 0,nMax = 0;` |
|      - | 3386 | `	sxu32 nOfft;` |
|      - | 3387 | `	sxi32 rc;` |
|     61 | 3388 | `	if( nArg < 2 ){` |
|      - | 3389 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3390 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3391 | `		return PH7_OK;` |
|      - | 3392 | `	}` |
|      - | 3393 | `	/* Extract the needle and the haystack */` |
|     61 | 3394 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     61 | 3395 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     61 | 3396 | `	nOfft = 0; /* cc warning */` |
|      - | 3397 | `	/* Resolve the range of positions the match may start at */` |
|     61 | 3398 | `	rc = StrRSearchWindow(pCtx,nArg > 2 ? apArg[2] : 0,nLen,nPatLen,"strrpos",&nMin,&nMax);` |
|     61 | 3399 | `	if( rc != PH7_OK ){` |
|      5 | 3400 | `		return rc;` |
|      - | 3401 | `	}` |
|     57 | 3402 | `	if( nPatLen < 1 ){` |
|      - | 3403 | `		/* php 8: the empty needle matches everywhere, so the LAST match is the` |
|      - | 3404 | `		 * highest position the window allows. */` |
|     11 | 3405 | `		ph7_result_int64(pCtx,(ph7_int64)nMax);` |
|     11 | 3406 | `		return PH7_OK;` |
|      - | 3407 | `	}` |
|      - | 3408 | `	/* Walk backwards, comparing at each candidate position. Searching a window` |
|      - | 3409 | `	 * exactly as long as the needle makes the match test an equality test while` |
|      - | 3410 | `	 * still going through xPatternMatch, which carries the case folding. */` |
|    243 | 3411 | `	for( i = nMax ; i >= nMin ; --i ){` |
|    229 | 3412 | `		rc = xPatternMatch((const void *)&zBlob[i],(sxu32)nPatLen,(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    229 | 3413 | `		if( rc == SXRET_OK ){` |
|      - | 3414 | `			/* Pattern found,return it's position */` |
|     33 | 3415 | `			ph7_result_int64(pCtx,(ph7_int64)i);` |
|     33 | 3416 | `			return PH7_OK;` |
|      - | 3417 | `		}` |
|     99 | 3418 | `	}` |
|      - | 3419 | `	/* Pattern not found,return FALSE */` |
|     15 | 3420 | `	ph7_result_bool(pCtx,0);` |
|     15 | 3421 | `	return PH7_OK;` |
|     31 | 3422 | `}` |
|      - | 3423 | `/*` |
|      - | 3424 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3425 | ` *  Case-insensitive strrpos.` |
|      - | 3426 | ` * Parameters` |
|      - | 3427 | ` *  $haystack` |
|      - | 3428 | ` *   The input string.` |
|      - | 3429 | ` * $needle` |
|      - | 3430 | ` *   Search pattern (must be a string).` |
|      - | 3431 | ` * $offset` |
|      - | 3432 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3433 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3434 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3435 | ` * Return` |
|      - | 3436 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3437 | ` */` |
|     34 | 3438 | `PH7_PRIVATE int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3439 | `{` |
|      - | 3440 | `	const char *zBlob,*zPattern;` |
|     35 | 3441 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3442 | `	int nLen,nPatLen,i;` |
|     35 | 3443 | `	int nMin = 0,nMax = 0;` |
|      - | 3444 | `	sxu32 nOfft;` |
|      - | 3445 | `	sxi32 rc;` |
|     35 | 3446 | `	if( nArg < 2 ){` |
|      - | 3447 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3448 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3449 | `		return PH7_OK;` |
|      - | 3450 | `	}` |
|      - | 3451 | `	/* Extract the needle and the haystack */` |
|     35 | 3452 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     35 | 3453 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     35 | 3454 | `	nOfft = 0; /* cc warning */` |
|      - | 3455 | `	/* Resolve the range of positions the match may start at */` |
|     35 | 3456 | `	rc = StrRSearchWindow(pCtx,nArg > 2 ? apArg[2] : 0,nLen,nPatLen,"strripos",&nMin,&nMax);` |
|     35 | 3457 | `	if( rc != PH7_OK ){` |
|      5 | 3458 | `		return rc;` |
|      - | 3459 | `	}` |
|     31 | 3460 | `	if( nPatLen < 1 ){` |
|      - | 3461 | `		/* php 8: the empty needle matches everywhere, so the LAST match is the` |
|      - | 3462 | `		 * highest position the window allows. */` |
|     11 | 3463 | `		ph7_result_int64(pCtx,(ph7_int64)nMax);` |
|     11 | 3464 | `		return PH7_OK;` |
|      - | 3465 | `	}` |
|      - | 3466 | `	/* Walk backwards, comparing at each candidate position (see strrpos). */` |
|     49 | 3467 | `	for( i = nMax ; i >= nMin ; --i ){` |
|     45 | 3468 | `		rc = xPatternMatch((const void *)&zBlob[i],(sxu32)nPatLen,(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     45 | 3469 | `		if( rc == SXRET_OK ){` |
|      - | 3470 | `			/* Pattern found,return it's position */` |
|     17 | 3471 | `			ph7_result_int64(pCtx,(ph7_int64)i);` |
|     17 | 3472 | `			return PH7_OK;` |
|      - | 3473 | `		}` |
|     15 | 3474 | `	}` |
|      - | 3475 | `	/* Pattern not found,return FALSE */` |
|      5 | 3476 | `	ph7_result_bool(pCtx,0);` |
|      5 | 3477 | `	return PH7_OK;` |
|     18 | 3478 | `}` |
|      - | 3479 | `/*` |
|      - | 3480 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3481 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3482 | ` * Parameters` |
|      - | 3483 | ` *  $haystack` |
|      - | 3484 | ` *   The input string.` |
|      - | 3485 | ` * $needle` |
|      - | 3486 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3487 | ` *  This behavior is different from that of strstr().` |
|      - | 3488 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3489 | ` *  as the ordinal value of a character.` |
|      - | 3490 | ` * Return` |
|      - | 3491 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3492 | ` */` |
|     24 | 3493 | `PH7_PRIVATE int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3494 | `{` |
|      - | 3495 | `	const char *zBlob;` |
|      - | 3496 | `	int nLen,c;` |
|     25 | 3497 | `	if( nArg < 2 ){` |
|      - | 3498 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3499 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3500 | `		return PH7_OK;` |
|      - | 3501 | `	}` |
|      - | 3502 | `	/* Extract the haystack */` |
|     25 | 3503 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 3504 | `	c = 0; /* cc warning */` |
|     25 | 3505 | `	if( nLen > 0 ){` |
|      - | 3506 | `		const char *zPattern;` |
|      - | 3507 | `		int nPatLen;` |
|      - | 3508 | `		sxu32 nOfft;` |
|      - | 3509 | `		sxi32 rc;` |
|      - | 3510 | `		/* php 8 casts the needle to string and uses only its first character.` |
|      - | 3511 | `		 * The old "if not a string, take it as an ordinal" reading was php 7` |
|      - | 3512 | `		 * behaviour, removed in php 8: strrchr("hello world",111) now looks for` |
|      - | 3513 | `		 * "1", not "o". An empty needle matches nothing. */` |
|     23 | 3514 | `		zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     23 | 3515 | `		if( nPatLen < 1 ){` |
|      3 | 3516 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3517 | `			return PH7_OK;` |
|      - | 3518 | `		}` |
|     21 | 3519 | `		c = zPattern[0];` |
|      - | 3520 | `		/* Perform the lookup */` |
|     21 | 3521 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3522 | `		if( rc != SXRET_OK ){` |
|      - | 3523 | `			/* No such entry,return FALSE */` |
|      9 | 3524 | `			ph7_result_bool(pCtx,0);` |
|      9 | 3525 | `			return PH7_OK;` |
|      - | 3526 | `		}` |
|      - | 3527 | `		/* Return the string portion */` |
|     13 | 3528 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      7 | 3529 | `	}else{` |
|      3 | 3530 | `		ph7_result_bool(pCtx,0);` |
|      - | 3531 | `	}` |
|     15 | 3532 | `	return PH7_OK;` |
|     13 | 3533 | `}` |
|      - | 3534 | `/*` |
|      - | 3535 | ` * string strrev(string $string)` |
|      - | 3536 | ` *  Reverse a string.` |
|      - | 3537 | ` * Parameters` |
|      - | 3538 | ` *  $string` |
|      - | 3539 | ` *   String to be reversed.` |
|      - | 3540 | ` * Return` |
|      - | 3541 | ` *  The reversed string.` |
|      - | 3542 | ` */` |
|      2 | 3543 | `PH7_PRIVATE int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3544 | `{` |
|      - | 3545 | `	const char *zIn,*zEnd;` |
|      - | 3546 | `	int nLen,c;` |
|      3 | 3547 | `	if( nArg < 1 ){` |
|      - | 3548 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3549 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3550 | `		return PH7_OK;` |
|      - | 3551 | `	}` |
|      - | 3552 | `	/* Extract the target string */` |
|      3 | 3553 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3554 | `	if( nLen < 1 ){` |
|      - | 3555 | `		/* Empty string Return null */` |
|    ! 0 | 3556 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3557 | `		return PH7_OK;` |
|      - | 3558 | `	}` |
|      - | 3559 | `	/* Perform the requested operation */` |
|      3 | 3560 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3561 | `	for(;;){` |
|      9 | 3562 | `		if( zEnd < zIn ){` |
|      - | 3563 | `			/* No more input to process */` |
|      3 | 3564 | `			break;` |
|      - | 3565 | `		}` |
|      - | 3566 | `		/* Append current character */` |
|      7 | 3567 | `		c = zEnd[0];` |
|      7 | 3568 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3569 | `		zEnd--;` |
|      1 | 3570 | `	}` |
|      3 | 3571 | `	return PH7_OK;` |
|      2 | 3572 | `}` |
|      - | 3573 | `/*` |
|      - | 3574 | ` * string ucwords(string $string [, string $separators = " \t\r\n\f\v"])` |
|      - | 3575 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3576 | ` *  A word begins at the start of the string and after any character present in` |
|      - | 3577 | ` *  $separators. The default separators are the whitespace characters (space,` |
|      - | 3578 | ` *  horizontal tab, carriage return, newline, form-feed and vertical tab); an` |
|      - | 3579 | ` *  explicit $separators argument REPLACES them (an empty string leaves only the` |
|      - | 3580 | ` *  very first character upper-cased). Like PHP, this is byte-based: only ASCII` |
|      - | 3581 | ` *  bytes are upper-cased and a byte is a separator only if it appears in the set.` |
|      - | 3582 | ` * Parameters` |
|      - | 3583 | ` *  $string` |
|      - | 3584 | ` *   The input string.` |
|      - | 3585 | ` *  $separators` |
|      - | 3586 | ` *   The optional word-boundary characters.` |
|      - | 3587 | ` * Return` |
|      - | 3588 | ` *  The modified string.` |
|      - | 3589 | ` */` |
|     22 | 3590 | `PH7_PRIVATE int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3591 | `{` |
|      - | 3592 | `	const char *zIn;` |
|      - | 3593 | `	int nLen,i,iStart;` |
|      - | 3594 | `	char aDelim[256];` |
|     23 | 3595 | `	if( nArg < 1 ){` |
|      - | 3596 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3597 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3598 | `		return PH7_OK;` |
|      - | 3599 | `	}` |
|      - | 3600 | `	/* Build the separator membership table: an explicit $separators argument` |
|      - | 3601 | `	 * replaces the default whitespace set (an empty string clears it). */` |
|     23 | 3602 | `	SyZero(aDelim,(sxu32)sizeof(aDelim));` |
|     23 | 3603 | `	if( nArg > 1 ){` |
|      - | 3604 | `		int nDelim;` |
|      9 | 3605 | `		const char *zDelim = ph7_value_to_string(apArg[1],&nDelim);` |
|     17 | 3606 | `		for( i = 0 ; i < nDelim ; i++ ){` |
|      9 | 3607 | `			aDelim[(unsigned char)zDelim[i]] = 1;` |
|      5 | 3608 | `		}` |
|      5 | 3609 | `	}else{` |
|     15 | 3610 | `		aDelim[(unsigned char)' ']  = 1;` |
|     15 | 3611 | `		aDelim[(unsigned char)'\t'] = 1;` |
|     15 | 3612 | `		aDelim[(unsigned char)'\r'] = 1;` |
|     15 | 3613 | `		aDelim[(unsigned char)'\n'] = 1;` |
|     15 | 3614 | `		aDelim[(unsigned char)'\f'] = 1;` |
|     15 | 3615 | `		aDelim[(unsigned char)'\v'] = 1;` |
|      - | 3616 | `	}` |
|      - | 3617 | `	/* Extract the target string */` |
|     23 | 3618 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3619 | `	if( nLen < 1 ){` |
|      - | 3620 | `		/* Empty string – match PHP semantics */` |
|      3 | 3621 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3622 | `		return PH7_OK;` |
|      - | 3623 | `	}` |
|      - | 3624 | `	/* Upper-case the first byte of each word (the leading byte, or any byte that` |
|      - | 3625 | `	 * follows a separator), appending the untouched runs in between verbatim. */` |
|     21 | 3626 | `	iStart = 0;` |
|    309 | 3627 | `	for( i = 0 ; i < nLen ; i++ ){` |
|    289 | 3628 | `		int c = (unsigned char)zIn[i];` |
|    289 | 3629 | `		if( (i == 0 \|\| aDelim[(unsigned char)zIn[i-1]]) && c < 0x80 && SyisLower(c) ){` |
|     53 | 3630 | `			char up = (char)SyToUpper(c);` |
|     53 | 3631 | `			if( i > iStart ){` |
|     35 | 3632 | `				ph7_result_string(pCtx,&zIn[iStart],i - iStart);` |
|     17 | 3633 | `			}` |
|     53 | 3634 | `			ph7_result_string(pCtx,&up,1);` |
|     53 | 3635 | `			iStart = i + 1;` |
|     26 | 3636 | `		}` |
|    145 | 3637 | `	}` |
|     21 | 3638 | `	if( nLen > iStart ){` |
|     21 | 3639 | `		ph7_result_string(pCtx,&zIn[iStart],nLen - iStart);` |
|     10 | 3640 | `	}` |
|     21 | 3641 | `	return PH7_OK;` |
|     12 | 3642 | `}` |
|      - | 3643 | `/*` |
|      - | 3644 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3645 | ` *  Returns input repeated multiplier times.` |
|      - | 3646 | ` * Parameters` |
|      - | 3647 | ` *  $string` |
|      - | 3648 | ` *   String to be repeated.` |
|      - | 3649 | ` * $multiplier` |
|      - | 3650 | ` *  Number of time the input string should be repeated.` |
|      - | 3651 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3652 | ` *  to 0, the function will return an empty string.` |
|      - | 3653 | ` * Return` |
|      - | 3654 | ` *  The repeated string.` |
|      - | 3655 | ` */` |
|  20446 | 3656 | `PH7_PRIVATE int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3657 | `{` |
|      - | 3658 | `	const char *zIn;` |
|      - | 3659 | `	int nLen;` |
|      - | 3660 | `	ph7_int64 nMul;` |
|      - | 3661 | `	int rc;` |
|  20448 | 3662 | `	if( nArg < 2 ){` |
|      - | 3663 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3664 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3665 | `		return PH7_OK;` |
|      - | 3666 | `	}` |
|      - | 3667 | `	/* Extract the target string */` |
|  20448 | 3668 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3669 | `	/* Resolve $times through the shared ZPP helper so a lossy float / float-string` |
|      - | 3670 | `	 * carries php's precision deprecation and NAN/INF/non-numeric fail with php's` |
|      - | 3671 | `	 * TypeError — a bare ph7_value_to_int64() coerced them silently. */` |
|      - | 3672 | `	{` |
|  20448 | 3673 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_repeat",2,"$times","int",&nMul);` |
|  20448 | 3674 | `		if( rcArg != PH7_OK ){` |
|      5 | 3675 | `			return rcArg;` |
|      - | 3676 | `		}` |
|      - | 3677 | `	}` |
|  20444 | 3678 | `	if( nMul < 0 ){` |
|      3 | 3679 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 3680 | `			"str_repeat(): Argument #2 ($times) must be greater than or equal to 0");` |
|      - | 3681 | `	}` |
|  20442 | 3682 | `	if( nLen < 1 \|\| nMul < 1 ){` |
|      - | 3683 | `		/* Empty input or a zero multiplier yields the empty string (PHP). */` |
|    ! 0 | 3684 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3685 | `		return PH7_OK;` |
|      - | 3686 | `	}` |
|      - | 3687 | `	/* Perform the requested operation */` |
| 233893 | 3688 | `	for(;;){` |
| 467788 | 3689 | `		if( !nMul ){` |
|  20442 | 3690 | `			break;` |
|      - | 3691 | `		}` |
|      - | 3692 | `		/* Append the copy */` |
| 447348 | 3693 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 447348 | 3694 | `		if( rc != PH7_OK ){` |
|      - | 3695 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 3696 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 3697 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 3698 | `		}` |
| 447348 | 3699 | `		nMul--;` |
|      2 | 3700 | `	}` |
|  20442 | 3701 | `	return PH7_OK;` |
|  10225 | 3702 | `}` |
|      - | 3703 | `/*` |
|      - | 3704 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3705 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3706 | ` * Parameters` |
|      - | 3707 | ` *  $string` |
|      - | 3708 | ` *   The input string.` |
|      - | 3709 | ` * $is_xhtml` |
|      - | 3710 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3711 | ` * Return` |
|      - | 3712 | ` *  The processed string.` |
|      - | 3713 | ` */` |
|      4 | 3714 | `PH7_PRIVATE int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3715 | `{` |
|      - | 3716 | `	const char *zIn,*zCur,*zEnd;` |
|      5 | 3717 | `	int is_xhtml = 1; /* Default to XHTML-style '<br />' like PHP */` |
|      - | 3718 | `	int nLen;` |
|      5 | 3719 | `	if( nArg < 1 ){` |
|      - | 3720 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 3721 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3722 | `		return PH7_OK;` |
|      - | 3723 | `	}` |
|      - | 3724 | `	/* Extract the target string */` |
|      5 | 3725 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3726 | `	if( nLen < 1 ){` |
|      - | 3727 | `		/* Empty string,return null */` |
|    ! 0 | 3728 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3729 | `		return PH7_OK;` |
|      - | 3730 | `	}` |
|      5 | 3731 | `	if( nArg > 1 ){` |
|      3 | 3732 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3733 | `	}` |
|      5 | 3734 | `	zEnd = &zIn[nLen];` |
|      - | 3735 | `	/* Perform the requested operation */` |
|      4 | 3736 | `	for(;;){` |
|      9 | 3737 | `		zCur = zIn;` |
|      - | 3738 | `		/* Delimit the string */` |
|     21 | 3739 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3740 | `			zIn++;` |
|      1 | 3741 | `		}` |
|      9 | 3742 | `		if( zCur < zIn ){` |
|      - | 3743 | `			/* Output chunk verbatim */` |
|      9 | 3744 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3745 | `		}` |
|      9 | 3746 | `		if( zIn >= zEnd ){` |
|      - | 3747 | `			/* No more input to process */` |
|      5 | 3748 | `			break;` |
|      - | 3749 | `		}` |
|      - | 3750 | `		/* Output the HTML line break */` |
|      - | 3751 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br />' (space before the slash), otherwise use '<br>' */` |
|      5 | 3752 | `		if( is_xhtml ){` |
|      3 | 3753 | `			ph7_result_string(pCtx,"<br />",(int)sizeof("<br />")-1);` |
|      2 | 3754 | `		}else{` |
|      3 | 3755 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3756 | `		}` |
|      5 | 3757 | `		zCur = zIn;` |
|      - | 3758 | `		/* Append trailing line */` |
|     11 | 3759 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3760 | `			zIn++;` |
|      1 | 3761 | `		}` |
|      5 | 3762 | `		if( zCur < zIn ){` |
|      - | 3763 | `			/* Output chunk verbatim */` |
|      5 | 3764 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3765 | `		}` |
|      1 | 3766 | `	}` |
|      5 | 3767 | `	return PH7_OK;` |
|      3 | 3768 | `}` |
|      - | 3769 | `/*` |
|      - | 3770 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3771 | ` *  According to the PHP reference manual.` |
|      - | 3772 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3773 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3774 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3775 | ` * This applies to both sprintf() and printf().` |
|      - | 3776 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3777 | ` * or more of these elements, in order:` |
|      - | 3778 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3779 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3780 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 3781 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 3782 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 3783 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 3784 | ` *   it with a single quote ('). See the examples below.` |
|      - | 3785 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 3786 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 3787 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 3788 | ` *   should result in.` |
|      - | 3789 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 3790 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 3791 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 3792 | ` *   limit to the string.` |
|      - | 3793 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 3794 | ` *       % - a literal percent character. No argument is required.` |
|      - | 3795 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 3796 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 3797 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 3798 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 3799 | ` * 	     for the number of digits after the decimal point.` |
|      - | 3800 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 3801 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 3802 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 3803 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 3804 | ` *       g - shorter of %e and %f.` |
|      - | 3805 | ` *       G - shorter of %E and %f.` |
|      - | 3806 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 3807 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 3808 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 3809 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 3810 | ` */` |
|      - | 3811 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3812 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 3813 | `/*` |
|      - | 3814 | ` * Symisc eXtension.` |
|      - | 3815 | ` * string size_format(int64 $size)` |
|      - | 3816 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 3817 | ` *  Example:` |
|      - | 3818 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 3819 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 3820 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 3821 | ` * Parameter` |
|      - | 3822 | ` *  $size` |
|      - | 3823 | ` *    Entity size in bytes.` |
|      - | 3824 | ` * Return` |
|      - | 3825 | ` *   Formatted string representation of the given size.` |
|      - | 3826 | ` */` |
|     24 | 3827 | `PH7_PRIVATE int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3828 | `{` |
|      - | 3829 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 3830 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 3831 | `	sxi32 nRest,i_32;` |
|      - | 3832 | `	ph7_int64 iSize;` |
|     25 | 3833 | `	int c = -1; /* index in zUnit[] */` |
|      - | 3834 |  |
|     25 | 3835 | `	if( nArg < 1 ){` |
|      - | 3836 | `		/* Missing argument,return the empty string */` |
|      3 | 3837 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3838 | `		return PH7_OK;` |
|      - | 3839 | `	}` |
|      - | 3840 | `	/* Extract the given size */` |
|     23 | 3841 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 3842 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 3843 | `		/* Don't bother formatting,return immediately */` |
|      5 | 3844 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 3845 | `		return PH7_OK;` |
|      - | 3846 | `	}` |
|     19 | 3847 | `	for(;;){` |
|     39 | 3848 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 3849 | `		iSize >>= 10;` |
|     39 | 3850 | `		c++;` |
|     39 | 3851 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 3852 | `			break;` |
|      - | 3853 | `		}` |
|      1 | 3854 | `	}` |
|     19 | 3855 | `	nRest /= 100;` |
|     19 | 3856 | `	if( nRest > 9 ){` |
|    ! 0 | 3857 | `		nRest = 9;` |
|    ! 0 | 3858 | `	}` |
|     19 | 3859 | `	if( iSize > 999 ){` |
|    ! 0 | 3860 | `		c++;` |
|    ! 0 | 3861 | `		nRest = 9;` |
|    ! 0 | 3862 | `		iSize = 0;` |
|    ! 0 | 3863 | `	}` |
|     19 | 3864 | `	i_32 = (sxi32)iSize;` |
|      - | 3865 | `	/* Format */` |
|     19 | 3866 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 3867 | `	return PH7_OK;` |
|     13 | 3868 | `}` |
|      - | 3869 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3870 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 3871 | `/*` |
|      - | 3872 | ` * string str_shuffle(string $str)` |
|      - | 3873 |  |
|      - | 3874 | ` *  Randomly shuffles a string.` |
|      - | 3875 | ` * Parameters` |
|      - | 3876 | ` *  $str` |
|      - | 3877 | ` *   The input string.` |
|      - | 3878 | ` * Return` |
|      - | 3879 | ` *  Returns the shuffled string.` |
|      - | 3880 | ` */` |
|     10 | 3881 | `PH7_PRIVATE int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3882 | `{` |
|      - | 3883 | `	const char *zString;` |
|      - | 3884 | `	int nLen,i,c;` |
|      - | 3885 | `	sxu32 iR;` |
|     11 | 3886 | `	if( nArg < 1 ){` |
|      - | 3887 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 3888 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3889 | `		return PH7_OK;` |
|      - | 3890 | `	}` |
|      - | 3891 | `	/* Extract the target string */` |
|     11 | 3892 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 3893 | `	if( nLen < 1 ){` |
|      - | 3894 | `		/* Nothing to shuffle */` |
|      3 | 3895 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3896 | `		return PH7_OK;` |
|      - | 3897 | `	}` |
|      - | 3898 | `	/* Shuffle the string. Draw through the MT19937 generator so str_shuffle()` |
|      - | 3899 | `	 * responds to srand()/mt_srand() (reproducible under a seed), like php; the` |
|      - | 3900 | `	 * sampling differs from php's Fisher-Yates so it is not value-parity. */` |
|     43 | 3901 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 3902 | `		/* Generate a random number first */` |
|     35 | 3903 | `		iR = PH7_VmMtRand(pCtx->pVm);` |
|      - | 3904 | `		/* Extract a random offset */` |
|     35 | 3905 | `		c = zString[iR % nLen];` |
|      - | 3906 | `		/* Append it */` |
|     35 | 3907 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 3908 | `	}` |
|      9 | 3909 | `	return PH7_OK;` |
|      6 | 3910 | `}` |
|      - | 3911 | `/*` |
|      - | 3912 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 3913 | ` *  Convert a string to an array.` |
|      - | 3914 | ` * Parameters` |
|      - | 3915 | ` * $string` |
|      - | 3916 | ` *  The input string.` |
|      - | 3917 | ` * $split_length` |
|      - | 3918 | ` *  Maximum length of the chunk.` |
|      - | 3919 | ` * Return` |
|      - | 3920 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 3921 | ` *  except possibly the last one which may be shorter.` |
|      - | 3922 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 3923 | ` *  as the first (and only) array element.` |
|      - | 3924 | ` *  An empty string returns an empty array.` |
|      - | 3925 | ` * Errors` |
|      - | 3926 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 3927 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 3928 | ` *  ValueError if $split_length is less than 1.` |
|      - | 3929 | ` */` |
|     26 | 3930 | `PH7_PRIVATE int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3931 | `{` |
|      - | 3932 | `	const char *zString,*zEnd;` |
|      - | 3933 | `	ph7_value *pArray,*pValue;` |
|      - | 3934 | `	int split_len;` |
|      - | 3935 | `	int nLen;` |
|     29 | 3936 | `	if( nArg < 1 ){` |
|    ! 0 | 3937 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3938 | `			"ArgumentCountError",` |
|      - | 3939 | `			"str_split() expects at least 1 argument, %d given",` |
|    ! 0 | 3940 | `			nArg` |
|      - | 3941 | `			);` |
|      - | 3942 | `	}` |
|      - | 3943 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     39 | 3944 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     42 | 3945 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     26 | 3946 | `	    ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 3947 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3948 | `			"TypeError",` |
|      - | 3949 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 3950 | `			ph7_type_name(apArg[0])` |
|      - | 3951 | `			);` |
|      - | 3952 | `	}` |
|      - | 3953 | `	/* Point to the target string */` |
|     29 | 3954 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     29 | 3955 | `	split_len = (int)sizeof(char);` |
|     29 | 3956 | `	if( nArg > 1 ){` |
|      - | 3957 | `		/* Split length */` |
|     17 | 3958 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 3959 | `		if( split_len < 1 ){` |
|      6 | 3960 | `			return PH7_VmThrowException(pCtx,` |
|      - | 3961 | `				"ValueError",` |
|      - | 3962 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 3963 | `				);` |
|      - | 3964 | `		}` |
|     11 | 3965 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 3966 | `			split_len = nLen;` |
|      1 | 3967 | `		}` |
|      5 | 3968 | `	}` |
|      - | 3969 | `	/* Create the array and the scalar value */` |
|     23 | 3970 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 3971 | `	/*Chunk value */` |
|     23 | 3972 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     23 | 3973 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 3974 | `		/* Return FALSE */` |
|    ! 0 | 3975 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3976 | `		return PH7_OK;` |
|      - | 3977 | `	}` |
|      - | 3978 | `	/* Point to the end of the string */` |
|     23 | 3979 | `	zEnd = &zString[nLen];` |
|      - | 3980 | `	/* Perform the requested operation */` |
|    131 | 3981 | `	for(;;){` |
|      - | 3982 | `		int nMax;` |
|    143 | 3983 | `		if( zString >= zEnd ){` |
|      - | 3984 | `			/* No more input to process */` |
|     23 | 3985 | `			break;` |
|      - | 3986 | `		}` |
|    121 | 3987 | `		nMax = (int)(zEnd-zString);` |
|    121 | 3988 | `		if( nMax < split_len ){` |
|      3 | 3989 | `			split_len = nMax;` |
|      1 | 3990 | `		}` |
|      - | 3991 | `		/* Copy the current chunk */` |
|    121 | 3992 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 3993 | `		/* Insert it */` |
|    121 | 3994 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 3995 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 3996 | `		}` |
|      - | 3997 | `		/* reset the string cursor */` |
|    121 | 3998 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 3999 | `		/* Update position */` |
|    121 | 4000 | `		zString += split_len;` |
|      1 | 4001 | `	}` |
|      - | 4002 | `	/*` |
|      - | 4003 | `	 * Return the array.` |
|      - | 4004 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 4005 | `	 * upon we return from this function.` |
|      - | 4006 | `	 */` |
|     23 | 4007 | `	ph7_result_value(pCtx,pArray);` |
|     23 | 4008 | `	return PH7_OK;` |
|     16 | 4009 | `}` |
|      - | 4010 | `/*` |
|      - | 4011 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 4012 | ` * return the longest match.` |
|      - | 4013 | ` * Refer to [strspn()].` |
|      - | 4014 | ` */` |
|     66 | 4015 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 4016 | `{` |
|     67 | 4017 | `	const char *zEnd = &zString[nLen];` |
|     67 | 4018 | `	const char *zIn = zString;` |
|      - | 4019 | `	int i,c;` |
|    110 | 4020 | `	for(;;){` |
|    221 | 4021 | `		if( zString >= zEnd ){` |
|     45 | 4022 | `			break;` |
|      - | 4023 | `		}` |
|      - | 4024 | `		/* Extract current character */` |
|    177 | 4025 | `		c = zString[0];` |
|      - | 4026 | `		/* Perform the lookup */` |
|    589 | 4027 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    567 | 4028 | `			if( c == zMask[i] ){` |
|      - | 4029 | `				/* Character found */` |
|    155 | 4030 | `				break;` |
|      - | 4031 | `			}` |
|    207 | 4032 | `		}` |
|    177 | 4033 | `		if( i >= nMaskLen ){` |
|      - | 4034 | `			/* Character not in the current mask,break immediately */` |
|     23 | 4035 | `			break;` |
|      - | 4036 | `		}` |
|      - | 4037 | `		/* Advance cursor */` |
|    155 | 4038 | `		zString++;` |
|      1 | 4039 | `	}` |
|      - | 4040 | `	/* Longest match */` |
|     67 | 4041 | `	return (int)(zString-zIn);` |
|      1 | 4042 | `}` |
|      - | 4043 | `/*` |
|      - | 4044 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 4045 | ` * Refer to [strcspn()].` |
|      - | 4046 | ` */` |
|     48 | 4047 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 4048 | `{` |
|     49 | 4049 | `	const char *zEnd = &zString[nLen];` |
|     49 | 4050 | `	const char *zIn = zString;` |
|      - | 4051 | `	int i,c;` |
|     81 | 4052 | `	for(;;){` |
|    163 | 4053 | `		if( zString >= zEnd ){` |
|     39 | 4054 | `			break;` |
|      - | 4055 | `		}` |
|      - | 4056 | `		/* Extract current character */` |
|    125 | 4057 | `		c = zString[0];` |
|      - | 4058 | `		/* Perform the lookup */` |
|    217 | 4059 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    103 | 4060 | `			if( c == zMask[i] ){` |
|     11 | 4061 | `				break;` |
|      - | 4062 | `			}` |
|     47 | 4063 | `		}` |
|    125 | 4064 | `		if( i < nMaskLen ){` |
|      - | 4065 | `			/* Character in the current mask,break immediately */` |
|     11 | 4066 | `			break;` |
|      - | 4067 | `		}` |
|      - | 4068 | `		/* Advance cursor */` |
|    115 | 4069 | `		zString++;` |
|      1 | 4070 | `	}` |
|      - | 4071 | `	/* Longest match */` |
|     49 | 4072 | `	return (int)(zString-zIn);` |
|      1 | 4073 | `}` |
|      - | 4074 | `/*` |
|      - | 4075 | ` * Shared body of strspn()/strcspn(): resolve php's ($offset,$length) window over` |
|      - | 4076 | ` * $string, then measure the span from the window's first byte.` |
|      - | 4077 | ` *` |
|      - | 4078 | ` * php's window rules (ext/standard/string.c, php_spn_common_handler) — a negative` |
|      - | 4079 | ` * $offset counts back from the end and CLAMPS to 0 (it is never "invalid"); an` |
|      - | 4080 | ` * $offset past the end clamps to the end, so the window is empty and the answer is` |
|      - | 4081 | ` * 0; a negative $length leaves that many bytes off the end of the remaining span` |
|      - | 4082 | ` * and clamps to 0; a zero-length window answers 0. PH7 answered 0 for a negative` |
|      - | 4083 | ` * offset that reached past the start, IGNORED a zero or negative $length entirely` |
|      - | 4084 | ` * (measuring the whole rest of the string instead), and truncated the offset to` |
|      - | 4085 | `` * `int`, so a 64-bit offset wrapped into a valid one.`` |
|      - | 4086 | ` *` |
|      - | 4087 | ` * PH7 also ran the scan over the first WHITESPACE-DELIMITED TOKEN rather than over` |
|      - | 4088 | ` * the raw window (leading spaces skipped, scan stopped at the next space), so` |
|      - | 4089 | ` * strspn("a b c","abc ") answered 1 where php answers 5 and strspn("  abc","abc")` |
|      - | 4090 | ` * answered 3 where php answers 0 — silent wrong answers on ordinary input. php` |
|      - | 4091 | ` * scans raw bytes; so does this.` |
|      - | 4092 | ` *` |
|      - | 4093 | ` * An empty $mask needs no special case: the mask lookup fails for every byte, so` |
|      - | 4094 | ` * strspn stops at once (0) and strcspn runs to the end of the window (its length),` |
|      - | 4095 | ` * which is exactly what php answers.` |
|      - | 4096 | ` */` |
|    120 | 4097 | `static int StrSpnCommonHandler(` |
|      - | 4098 | `	ph7_context *pCtx,    /* Call context */` |
|      - | 4099 | `	int nArg,             /* Argument count */` |
|      - | 4100 | `	ph7_value **apArg,    /* Arguments */` |
|      - | 4101 | `	int bComplement       /* TRUE for strcspn() */` |
|      - | 4102 | `	)` |
|      1 | 4103 | `{` |
|    121 | 4104 | `	const char *zFunc = bComplement ? "strcspn" : "strspn";` |
|      - | 4105 | `	const char *zString,*zMask;` |
|      - | 4106 | `	int iMasklen,iLen;` |
|      - | 4107 | `	sxi64 iStart,iSpan;` |
|    121 | 4108 | `	if( nArg < 2 ){` |
|      - | 4109 | `		/* Arity is enforced at the call boundary; nothing sensible to return here. */` |
|    ! 0 | 4110 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4111 | `		return PH7_OK;` |
|      - | 4112 | `	}` |
|      - | 4113 | `	/* Extract the target string and the mask */` |
|    121 | 4114 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|    121 | 4115 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|    121 | 4116 | `	if( iLen < 0 ){` |
|    ! 0 | 4117 | `		iLen = 0;` |
|    ! 0 | 4118 | `	}` |
|    121 | 4119 | `	if( iMasklen < 0 ){` |
|    ! 0 | 4120 | `		iMasklen = 0;` |
|    ! 0 | 4121 | `	}` |
|    121 | 4122 | `	iStart = 0;` |
|    121 | 4123 | `	if( nArg > 2 ){` |
|     79 | 4124 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],zFunc,3,"$offset","int",&iStart);` |
|     79 | 4125 | `		if( rcArg != PH7_OK ){` |
|      5 | 4126 | `			return rcArg;` |
|      - | 4127 | `		}` |
|     75 | 4128 | `		if( iStart < 0 ){` |
|      - | 4129 | `			/* Count back from the end, clamped to the start (guarded so an` |
|      - | 4130 | `			 * INT64_MIN offset cannot overflow the addition). */` |
|     13 | 4131 | `			iStart = ( iStart < -(sxi64)iLen ) ? 0 : iStart + iLen;` |
|     69 | 4132 | `		}else if( iStart > (sxi64)iLen ){` |
|      9 | 4133 | `			iStart = iLen;` |
|      4 | 4134 | `		}` |
|     37 | 4135 | `	}` |
|    117 | 4136 | `	iSpan = (sxi64)iLen - iStart;` |
|    117 | 4137 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|     41 | 4138 | `		sxi64 iUserlen = 0;` |
|     41 | 4139 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[3],zFunc,4,"$length","?int",&iUserlen);` |
|     41 | 4140 | `		if( rcArg != PH7_OK ){` |
|      3 | 4141 | `			return rcArg;` |
|      - | 4142 | `		}` |
|     39 | 4143 | `		if( iUserlen < 0 ){` |
|      - | 4144 | `			/* Leave \|$length\| bytes off the end of the remaining span (guarded` |
|      - | 4145 | `			 * against an INT64_MIN underflow the same way). */` |
|     21 | 4146 | `			iSpan = ( iUserlen < -iSpan ) ? 0 : iSpan + iUserlen;` |
|     29 | 4147 | `		}else if( iUserlen < iSpan ){` |
|     11 | 4148 | `			iSpan = iUserlen;` |
|      5 | 4149 | `		}` |
|     19 | 4150 | `	}` |
|    172 | 4151 | `	ph7_result_int(pCtx,bComplement` |
|     48 | 4152 | `		? LongestStringMask2(&zString[iStart],(int)iSpan,zMask,iMasklen)` |
|     66 | 4153 | `		: LongestStringMask(&zString[iStart],(int)iSpan,zMask,iMasklen));` |
|    115 | 4154 | `	return PH7_OK;` |
|     61 | 4155 | `}` |
|      - | 4156 | `/*` |
|      - | 4157 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 4158 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 4159 | ` *  of characters contained within a given mask.` |
|      - | 4160 | ` * Parameters` |
|      - | 4161 | ` * $str` |
|      - | 4162 | ` *  The input string.` |
|      - | 4163 | ` * $mask` |
|      - | 4164 | ` *  The list of allowable characters.` |
|      - | 4165 | ` * $start` |
|      - | 4166 | ` *  The position in subject to start searching.` |
|      - | 4167 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 4168 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 4169 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 4170 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 4171 | ` *  start'th position from the end of subject.` |
|      - | 4172 | ` * $length` |
|      - | 4173 | ` *  The length of the segment from subject to examine.` |
|      - | 4174 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 4175 | ` *  characters after the starting position.` |
|      - | 4176 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 4177 | ` *  position up to length characters from the end of subject.` |
|      - | 4178 | ` * Return` |
|      - | 4179 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 4180 | ` * in mask.` |
|      - | 4181 | ` */` |
|     70 | 4182 | `PH7_PRIVATE int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4183 | `{` |
|     71 | 4184 | `	return StrSpnCommonHandler(pCtx,nArg,apArg,0);` |
|      1 | 4185 | `}` |
|      - | 4186 | `/*` |
|      - | 4187 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 4188 | ` *  Find length of initial segment not matching mask.` |
|      - | 4189 | ` * Parameters` |
|      - | 4190 | ` * $str` |
|      - | 4191 | ` *  The input string.` |
|      - | 4192 | ` * $mask` |
|      - | 4193 | ` *  The list of not allowed characters.` |
|      - | 4194 | ` * $start` |
|      - | 4195 | ` *  The position in subject to start searching.` |
|      - | 4196 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 4197 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 4198 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 4199 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 4200 | ` *  start'th position from the end of subject.` |
|      - | 4201 | ` * $length` |
|      - | 4202 | ` *  The length of the segment from subject to examine.` |
|      - | 4203 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 4204 | ` *  characters after the starting position.` |
|      - | 4205 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 4206 | ` *  position up to length characters from the end of subject.` |
|      - | 4207 | ` * Return` |
|      - | 4208 | ` *  Returns the length of the segment as an integer.` |
|      - | 4209 | ` */` |
|     50 | 4210 | `PH7_PRIVATE int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4211 | `{` |
|     51 | 4212 | `	return StrSpnCommonHandler(pCtx,nArg,apArg,1);` |
|      1 | 4213 | `}` |
|      - | 4214 | `/*` |
|      - | 4215 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 4216 | ` *  Search a string for any of a set of characters.` |
|      - | 4217 | ` * Parameters` |
|      - | 4218 | ` *  $haystack` |
|      - | 4219 | ` *   The string where char_list is looked for.` |
|      - | 4220 | ` *  $char_list` |
|      - | 4221 | ` *   This parameter is case sensitive.` |
|      - | 4222 | ` * Return` |
|      - | 4223 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 4224 | ` */` |
|     14 | 4225 | `PH7_PRIVATE int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4226 | `{` |
|      - | 4227 | `	const char *zString,*zList,*zEnd;` |
|      - | 4228 | `	int iLen,iListLen,i,c;` |
|      - | 4229 | `	sxu32 nOfft,nMax;` |
|      - | 4230 | `	sxi32 rc;` |
|     15 | 4231 | `	if( nArg < 2 ){` |
|      - | 4232 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 4233 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4234 | `		return PH7_OK;` |
|      - | 4235 | `	}` |
|      - | 4236 | `	/* Extract the haystack and the char list */` |
|     15 | 4237 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|     15 | 4238 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|     15 | 4239 | `	if( iListLen < 1 ){` |
|      - | 4240 | `		/* An empty set can never match, so php rejects it rather than answering` |
|      - | 4241 | `		 * a FALSE indistinguishable from "not found" (checked BEFORE the haystack,` |
|      - | 4242 | `		 * so strpbrk("","") throws too). */` |
|      5 | 4243 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4244 | `			"strpbrk(): Argument #2 ($characters) must be a non-empty string");` |
|      - | 4245 | `	}` |
|     11 | 4246 | `	if( iLen < 1 ){` |
|      - | 4247 | `		/* Nothing to process,return FALSE */` |
|      3 | 4248 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4249 | `		return PH7_OK;` |
|      - | 4250 | `	}` |
|      - | 4251 | `	/* Point to the end of the string */` |
|      9 | 4252 | `	zEnd = &zString[iLen];` |
|      9 | 4253 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 4254 | `	/* perform the requested operation */` |
|     25 | 4255 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     17 | 4256 | `		c = zList[i];` |
|     17 | 4257 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     17 | 4258 | `		if( rc == SXRET_OK ){` |
|      9 | 4259 | `			if( nMax < nOfft ){` |
|      5 | 4260 | `				nOfft = nMax;` |
|      2 | 4261 | `			}` |
|      4 | 4262 | `		}` |
|      9 | 4263 | `	}` |
|      9 | 4264 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 4265 | `		/* No such substring,return FALSE */` |
|      5 | 4266 | `		ph7_result_bool(pCtx,0);` |
|      3 | 4267 | `	}else{` |
|      - | 4268 | `		/* Return the substring */` |
|      5 | 4269 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 4270 | `	}` |
|      9 | 4271 | `	return PH7_OK;` |
|      8 | 4272 | `}` |
|      - | 4273 | `/* SPDX-SnippetBegin */` |
|      - | 4274 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 4275 | `/* SPDX-License-Identifier: blessing */` |
|      - | 4276 | `/*` |
|      - | 4277 | ` * string soundex(string $str)` |
|      - | 4278 | ` *  Calculate the soundex key of a string.` |
|      - | 4279 | ` * Parameters` |
|      - | 4280 | ` *  $str` |
|      - | 4281 | ` *   The input string.` |
|      - | 4282 | ` * Return` |
|      - | 4283 | ` *  Returns the soundex key as a string.` |
|      - | 4284 | ` * Note:` |
|      - | 4285 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 4286 | ` * source tree.` |
|      - | 4287 | ` */` |
|     22 | 4288 | `PH7_PRIVATE int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4289 | `{` |
|      - | 4290 | `	const unsigned char *zIn;` |
|      - | 4291 | `	char zResult[8];` |
|      - | 4292 | `	int i, j;` |
|      - | 4293 | `	static const unsigned char iCode[] = {` |
|      - | 4294 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 4295 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 4296 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 4297 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 4298 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 4299 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 4300 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 4301 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 4302 | `	};` |
|     23 | 4303 | `	if( nArg < 1 ){` |
|      - | 4304 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4305 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4306 | `		return PH7_OK;` |
|      - | 4307 | `	}` |
|     23 | 4308 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     35 | 4309 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     23 | 4310 | `	if( zIn[i] ){` |
|     17 | 4311 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 4312 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 4313 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 4314 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 4315 | `			if( code>0 ){` |
|     45 | 4316 | `				if( code!=prevcode ){` |
|     33 | 4317 | `					prevcode = (unsigned char)code;` |
|     33 | 4318 | `					zResult[j++] = (char)code + '0';` |
|     16 | 4319 | `				}` |
|     23 | 4320 | `			}else{` |
|     49 | 4321 | `				prevcode = 0;` |
|      - | 4322 | `			}` |
|     47 | 4323 | `		}` |
|     33 | 4324 | `		while( j<4 ){` |
|     17 | 4325 | `			zResult[j++] = '0';` |
|      1 | 4326 | `		}` |
|     17 | 4327 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 4328 | `	}else{` |
|      - | 4329 | `	  /* No alphabetic character: PHP returns "0000" (not the SQLite "?000"). */` |
|      7 | 4330 | `	  ph7_result_string(pCtx,"0000",4);` |
|      - | 4331 | `	}` |
|     23 | 4332 | `	return PH7_OK;` |
|     12 | 4333 | `}` |
|      - | 4334 | `/* SPDX-SnippetEnd */` |
|      - | 4335 | `/*` |
|      - | 4336 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 4337 | ` *  Wraps a string to a given number of characters.` |
|      - | 4338 | ` * Parameters` |
|      - | 4339 | ` *  $str` |
|      - | 4340 | ` *   The input string.` |
|      - | 4341 | ` * $width` |
|      - | 4342 | ` *  The column width.` |
|      - | 4343 | ` * $break` |
|      - | 4344 | ` *  The line is broken using the optional break parameter.` |
|      - | 4345 | ` * Return` |
|      - | 4346 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 4347 | ` */` |
|     26 | 4348 | `PH7_PRIVATE int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4349 | `{` |
|      - | 4350 | `	const char *zIn,*zBreak;` |
|      - | 4351 | `	SyBlob sWorker;` |
|      - | 4352 | `	int iLen,iBreaklen,iWidth,iCut,iStart,iSpace,iCur;` |
|      - | 4353 | `	sxi32 rc;` |
|     27 | 4354 | `	if( nArg < 1 ){` |
|      - | 4355 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4356 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4357 | `		return PH7_OK;` |
|      - | 4358 | `	}` |
|      - | 4359 | `	/* Extract the input string */` |
|     27 | 4360 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 4361 | `	/* Width (default 75; PHP allows 0/negative — break at every space). */` |
|     27 | 4362 | `	iWidth = 75;` |
|     27 | 4363 | `	if( nArg > 1 ){` |
|     27 | 4364 | `		iWidth = ph7_value_to_int(apArg[1]);` |
|     13 | 4365 | `	}` |
|      - | 4366 | `	/* Break string (default "\n"). */` |
|     27 | 4367 | `	zBreak = "\n";` |
|     27 | 4368 | `	iBreaklen = (int)sizeof(char);` |
|     27 | 4369 | `	if( nArg > 2 ){` |
|     13 | 4370 | `		zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      6 | 4371 | `	}` |
|      - | 4372 | `	/* Cut long words? (default false). */` |
|     27 | 4373 | `	iCut = 0;` |
|     27 | 4374 | `	if( nArg > 3 ){` |
|      7 | 4375 | `		iCut = ph7_value_to_bool(apArg[3]);` |
|      3 | 4376 | `	}` |
|     27 | 4377 | `	if( iLen < 1 ){` |
|      - | 4378 | `		/* PHP returns the empty string for empty input before validating the other args. */` |
|      5 | 4379 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 4380 | `		return PH7_OK;` |
|      - | 4381 | `	}` |
|      - | 4382 | `	/* PHP 8 domain errors (catchable ValueError). */` |
|     23 | 4383 | `	if( iBreaklen < 1 ){` |
|      3 | 4384 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4385 | `			"wordwrap(): Argument #3 ($break) must not be empty");` |
|      - | 4386 | `	}` |
|     21 | 4387 | `	if( iWidth == 0 && iCut ){` |
|      3 | 4388 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4389 | `			"wordwrap(): Argument #4 ($cut_long_words) cannot be true when argument #2 ($width) is 0");` |
|      - | 4390 | `	}` |
|      - | 4391 | `	/*` |
|      - | 4392 | `	 * PHP's algorithm: a single left-to-right pass tracking the start of the` |
|      - | 4393 | `	 * current line (iStart) and the position of the last space seen on it` |
|      - | 4394 | `	 * (iSpace). A break is emitted when the line reaches the width, at the last` |
|      - | 4395 | `	 * space if there was one, otherwise (only when cut is enabled) hard at the` |
|      - | 4396 | `	 * boundary. An existing break sequence in the input resets the line.` |
|      - | 4397 | `	 */` |
|     19 | 4398 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     19 | 4399 | `	iStart = iSpace = iCur = 0;` |
|     19 | 4400 | `	rc = SXRET_OK;` |
|    551 | 4401 | `	while( iCur < iLen ){` |
|    533 | 4402 | `		if( iBreaklen <= iLen - iCur && SyMemcmp(&zIn[iCur],zBreak,(sxu32)iBreaklen) == 0 ){` |
|      - | 4403 | `			/* Existing break sequence in the input: copy it verbatim and reset the line. */` |
|    ! 0 | 4404 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart + iBreaklen));` |
|    ! 0 | 4405 | `			if( rc != SXRET_OK ){ goto oom; }` |
|    ! 0 | 4406 | `			iCur += iBreaklen;` |
|    ! 0 | 4407 | `			iStart = iSpace = iCur;` |
|    ! 0 | 4408 | `			continue;` |
|    533 | 4409 | `		}else if( zIn[iCur] == ' ' ){` |
|     67 | 4410 | `			if( iCur - iStart >= iWidth ){` |
|      - | 4411 | `				/* The line already fills the width at this space: break here (the space is consumed). */` |
|     13 | 4412 | `				rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     13 | 4413 | `				if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     13 | 4414 | `				if( rc != SXRET_OK ){ goto oom; }` |
|     13 | 4415 | `				iStart = iCur + 1;` |
|      6 | 4416 | `			}` |
|     67 | 4417 | `			iSpace = iCur;` |
|    500 | 4418 | `		}else if( iCut && iCur - iStart >= iWidth && iStart >= iSpace ){` |
|      - | 4419 | `			/* A word longer than the width with no space to break at: hard-cut at the boundary. */` |
|      7 | 4420 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|      7 | 4421 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|      7 | 4422 | `			if( rc != SXRET_OK ){ goto oom; }` |
|      7 | 4423 | `			iStart = iSpace = iCur;` |
|    464 | 4424 | `		}else if( iCur - iStart >= iWidth && iStart < iSpace ){` |
|      - | 4425 | `			/* Past the width mid-word: wrap back to the last space (which is consumed). */` |
|     17 | 4426 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iSpace - iStart));` |
|     17 | 4427 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     17 | 4428 | `			if( rc != SXRET_OK ){ goto oom; }` |
|     17 | 4429 | `			iStart = iSpace = iSpace + 1;` |
|      8 | 4430 | `		}` |
|    533 | 4431 | `		iCur++;` |
|      1 | 4432 | `	}` |
|      - | 4433 | `	/* Emit the trailing chunk. */` |
|     19 | 4434 | `	if( iStart < iCur ){` |
|     19 | 4435 | `		rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     19 | 4436 | `		if( rc != SXRET_OK ){ goto oom; }` |
|      9 | 4437 | `	}` |
|     19 | 4438 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|     19 | 4439 | `	SyBlobRelease(&sWorker);` |
|     19 | 4440 | `	return PH7_OK;` |
|    ! 0 | 4441 | `oom:` |
|    ! 0 | 4442 | `	SyBlobRelease(&sWorker);` |
|    ! 0 | 4443 | `	return PH7_ContextMemoryError(pCtx);` |
|     14 | 4444 | `}` |
|      - | 4445 | `/*` |
|      - | 4446 | ` * Check if the given character is a member of the given mask.` |
|      - | 4447 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 4448 | ` * Refer to [strtok()].` |
|      - | 4449 | ` */` |
|    132 | 4450 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 4451 | `{` |
|      - | 4452 | `	int i;` |
|    263 | 4453 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|    157 | 4454 | `		if( c == zMask[i] ){` |
|     27 | 4455 | `			if( pOfft ){` |
|     19 | 4456 | `				*pOfft = i;` |
|      9 | 4457 | `			}` |
|     27 | 4458 | `			return TRUE;` |
|      - | 4459 | `		}` |
|     66 | 4460 | `	}` |
|    107 | 4461 | `	return FALSE;` |
|     67 | 4462 | `}` |
|      - | 4463 | `/*` |
|      - | 4464 | ` * Extract a single token from the input stream.` |
|      - | 4465 | ` * Refer to [strtok()].` |
|      - | 4466 | ` */` |
|      6 | 4467 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 4468 | `{` |
|      7 | 4469 | `	const char *zIn = *pzIn;` |
|      - | 4470 | `	const char *zPtr;` |
|      - | 4471 | `	/* Ignore leading delimiter */` |
|     11 | 4472 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 4473 | `		zIn++;` |
|      1 | 4474 | `	}` |
|      7 | 4475 | `	if( zIn >= zEnd ){` |
|      - | 4476 | `		/* End of input */` |
|    ! 0 | 4477 | `		return SXERR_EOF;` |
|      - | 4478 | `	}` |
|      7 | 4479 | `	zPtr = zIn;` |
|      - | 4480 | `	/* Extract the token */` |
|     13 | 4481 | `	while( zIn < zEnd ){` |
|     11 | 4482 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 4483 | `			/* UTF-8 stream */` |
|    ! 0 | 4484 | `			zIn++;` |
|    ! 0 | 4485 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 4486 | `		}else{` |
|     11 | 4487 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 4488 | `				break;` |
|      - | 4489 | `			}` |
|      7 | 4490 | `			zIn++;` |
|      - | 4491 | `		}` |
|      1 | 4492 | `	}` |
|      7 | 4493 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 4494 | `	/* Update the cursor */` |
|      7 | 4495 | `	*pzIn = zIn;` |
|      - | 4496 | `	/* Return to the caller */` |
|      7 | 4497 | `	return SXRET_OK;` |
|      4 | 4498 | `}` |
|      - | 4499 | `/* strtok auxiliary private data */` |
|      - | 4500 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 4501 | `struct strtok_aux_data` |
|      - | 4502 | `{` |
|      - | 4503 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 4504 | `	const char *zIn;   /* Current input stream */` |
|      - | 4505 | `	const char *zEnd;  /* End of input */` |
|      - | 4506 | `};` |
|      - | 4507 | `/*` |
|      - | 4508 | ` * string strtok(string $str,string $token)` |
|      - | 4509 | ` * string strtok(string $token)` |
|      - | 4510 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 4511 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 4512 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 4513 | ` *  words by using the space character as the token.` |
|      - | 4514 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 4515 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 4516 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 4517 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 4518 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 4519 | ` *  the argument are found.` |
|      - | 4520 | ` * Parameters` |
|      - | 4521 | ` *  $str` |
|      - | 4522 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 4523 | ` * $token` |
|      - | 4524 | ` *  The delimiter used when splitting up str.` |
|      - | 4525 | ` * Return` |
|      - | 4526 | ` *   Current token or FALSE on EOF.` |
|      - | 4527 | ` */` |
|      6 | 4528 | `PH7_PRIVATE int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4529 | `{` |
|      - | 4530 | `	strtok_aux_data *pAux;` |
|      - | 4531 | `	const char *zMask;` |
|      - | 4532 | `	SyString sToken;` |
|      - | 4533 | `	int nMasklen;` |
|      - | 4534 | `	sxi32 rc;` |
|      7 | 4535 | `	if( nArg < 2 ){` |
|      - | 4536 | `		/* Extract top aux data */` |
|      5 | 4537 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      5 | 4538 | `		if( pAux == 0 ){` |
|      - | 4539 | `			/* No aux data,return FALSE */` |
|    ! 0 | 4540 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 4541 | `			return PH7_OK;` |
|      - | 4542 | `		}` |
|      5 | 4543 | `		nMasklen = 0;` |
|      5 | 4544 | `		zMask = ""; /* cc warning */` |
|      5 | 4545 | `		if( nArg > 0 ){` |
|      - | 4546 | `			/* Extract the mask */` |
|      5 | 4547 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 4548 | `		}` |
|      5 | 4549 | `		if( nMasklen < 1 ){` |
|      - | 4550 | `			/* Invalid mask,return FALSE */` |
|    ! 0 | 4551 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 4552 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 4553 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 4554 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 4555 | `			return PH7_OK;` |
|      - | 4556 | `		}` |
|      - | 4557 | `		/* Extract the token */` |
|      5 | 4558 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 4559 | `		if( rc != SXRET_OK ){` |
|      - | 4560 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 4561 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 4562 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 4563 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 4564 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 4565 | `		}else{` |
|      - | 4566 | `			/* Return the extracted token */` |
|      5 | 4567 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 4568 | `		}` |
|      3 | 4569 | `	}else{` |
|      - | 4570 | `		const char *zInput,*zCur;` |
|      - | 4571 | `		char *zDup;` |
|      - | 4572 | `		int nLen;` |
|      - | 4573 | `		/* Extract the raw input */` |
|      3 | 4574 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4575 | `		if( nLen < 1 ){` |
|      - | 4576 | `			/* Empty input,return FALSE */` |
|    ! 0 | 4577 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 4578 | `			return PH7_OK;` |
|      - | 4579 | `		}` |
|      - | 4580 | `		/* Extract the mask */` |
|      3 | 4581 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 4582 | `		if( nMasklen < 1 ){` |
|      - | 4583 | `			/* Set a default mask */` |
|      - | 4584 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 4585 | `			zMask = TOK_MASK;` |
|    ! 0 | 4586 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 4587 | `#undef TOK_MASK` |
|    ! 0 | 4588 | `		}` |
|      - | 4589 | `		/* Extract a single token */` |
|      3 | 4590 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 4591 | `		if( rc != SXRET_OK ){` |
|      - | 4592 | `			/* Empty input */` |
|    ! 0 | 4593 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 4594 | `			return PH7_OK;` |
|    ! 0 | 4595 | `		}else{` |
|      - | 4596 | `			/* Return the extracted token */` |
|      3 | 4597 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 4598 | `		}` |
|      - | 4599 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 4600 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 4601 | `		if( pAux ){` |
|      3 | 4602 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 4603 | `			if( nLen < 1 ){` |
|    ! 0 | 4604 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 4605 | `				return PH7_OK;` |
|      - | 4606 | `			}` |
|      - | 4607 | `			/* Duplicate input */` |
|      3 | 4608 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 4609 | `			if( zDup  ){` |
|      3 | 4610 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 4611 | `				/* Register the aux data */` |
|      3 | 4612 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 4613 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 4614 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 4615 | `			}` |
|      1 | 4616 | `		}` |
|      - | 4617 | `	}` |
|      7 | 4618 | `	return PH7_OK;` |
|      4 | 4619 | `}` |
|      - | 4620 | `/*` |
|      - | 4621 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 4622 | ` *  Pad a string to a certain length with another string` |
|      - | 4623 | ` * Parameters` |
|      - | 4624 | ` *  $input` |
|      - | 4625 | ` *   The input string.` |
|      - | 4626 | ` * $pad_length` |
|      - | 4627 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 4628 | ` *   string, no padding takes place.` |
|      - | 4629 | ` * $pad_string` |
|      - | 4630 | ` *   Note:` |
|      - | 4631 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 4632 | ` *    divided by the pad_string's length.` |
|      - | 4633 | ` * $pad_type` |
|      - | 4634 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 4635 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 4636 | ` * Return` |
|      - | 4637 | ` *  The padded string.` |
|      - | 4638 | ` */` |
|    190 | 4639 | `PH7_PRIVATE int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4640 | `{` |
|      - | 4641 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 4642 | `	const char *zIn,*zPad;` |
|    193 | 4643 | `	if( nArg < 2 ){` |
|      - | 4644 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4645 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4646 | `		return PH7_OK;` |
|      - | 4647 | `	}` |
|      - | 4648 | `	/* Extract the target string */` |
|    193 | 4649 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 4650 | `	/* Padding length */` |
|      - | 4651 | `	{` |
|    193 | 4652 | `		sxi64 iTmp = 0;` |
|    193 | 4653 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_pad",2,"$length","int",&iTmp);` |
|    193 | 4654 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 4655 | `			return rcArg;` |
|      - | 4656 | `		}` |
|    193 | 4657 | `		iRealPad = iPadlen = (int)iTmp;` |
|      - | 4658 | `	}` |
|    193 | 4659 | `	if( iPadlen > 0 ){` |
|    191 | 4660 | `		iPadlen -= iLen;` |
|     94 | 4661 | `	}` |
|    193 | 4662 | `	if( iPadlen < 1  ){` |
|      - | 4663 | `		/* Return the string verbatim */` |
|      7 | 4664 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      7 | 4665 | `		return PH7_OK;` |
|      - | 4666 | `	}` |
|    187 | 4667 | `	zPad = " "; /* Whitespace padding */` |
|    187 | 4668 | `	iStrpad = (int)sizeof(char);` |
|    187 | 4669 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|    187 | 4670 | `	if( nArg > 2 ){` |
|      - | 4671 | `		/* Padding string */` |
|     17 | 4672 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|     17 | 4673 | `		if( iStrpad < 1 ){` |
|      - | 4674 | `			/* An empty pad string throws a catchable ValueError in PHP 8` |
|      - | 4675 | `			 * (only reached once padding is actually required). */` |
|      3 | 4676 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4677 | `				"str_pad(): Argument #3 ($pad_string) must not be empty");` |
|      - | 4678 | `		}` |
|     15 | 4679 | `		if( nArg > 3 ){` |
|      - | 4680 | `			/* Padd type. php 8: anything outside LEFT(0)/RIGHT(1)/BOTH(2) is a` |
|      - | 4681 | `			 * catchable ValueError (PHL used to fall back to RIGHT silently);` |
|      - | 4682 | `			 * like the empty-pad check above, php only reaches it once padding` |
|      - | 4683 | `			 * is actually required (probed: str_pad("abc",2," ",9) is "abc"). */` |
|     15 | 4684 | `			iType = ph7_value_to_int(apArg[3]);` |
|     15 | 4685 | `			if( iType < 0 \|\| iType > 2 ){` |
|      5 | 4686 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4687 | `					"str_pad(): Argument #4 ($pad_type) must be STR_PAD_LEFT, STR_PAD_RIGHT, or STR_PAD_BOTH");` |
|      - | 4688 | `			}` |
|      5 | 4689 | `		}` |
|      5 | 4690 | `	}` |
|    181 | 4691 | `	iDiv = 1;` |
|    181 | 4692 | `	if( iType == 2 ){` |
|      3 | 4693 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|      1 | 4694 | `	}` |
|      - | 4695 | `	/* Perform the requested operation */` |
|    181 | 4696 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      7 | 4697 | `		jPad = iStrpad;` |
|     13 | 4698 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 4699 | `			/* Padding */` |
|     11 | 4700 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      5 | 4701 | `				break;` |
|      - | 4702 | `			}` |
|      7 | 4703 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      4 | 4704 | `		}` |
|      7 | 4705 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      9 | 4706 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      5 | 4707 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      5 | 4708 | `				if( jPad > iStrpad ){` |
|    ! 0 | 4709 | `					jPad = iStrpad;` |
|    ! 0 | 4710 | `				}` |
|      5 | 4711 | `				if( jPad < 1){` |
|    ! 0 | 4712 | `					break;` |
|      - | 4713 | `				}` |
|      5 | 4714 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 4715 | `			}` |
|      2 | 4716 | `		}` |
|      3 | 4717 | `	}` |
|    181 | 4718 | `	if( iLen > 0 ){` |
|      - | 4719 | `		/* Append the input string */` |
|    181 | 4720 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|     89 | 4721 | `	}` |
|    181 | 4722 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|   1655 | 4723 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 4724 | `			/* Padding */` |
|   1653 | 4725 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|    175 | 4726 | `				break;` |
|      - | 4727 | `			}` |
|   1481 | 4728 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|    742 | 4729 | `		}` |
|    351 | 4730 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|    177 | 4731 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|    177 | 4732 | `			if( jPad > iStrpad ){` |
|    ! 0 | 4733 | `				jPad = iStrpad;` |
|    ! 0 | 4734 | `			}` |
|    177 | 4735 | `			if( jPad < 1){` |
|    ! 0 | 4736 | `				break;` |
|      - | 4737 | `			}` |
|    177 | 4738 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      3 | 4739 | `		}` |
|     87 | 4740 | `	}` |
|    181 | 4741 | `	return PH7_OK;` |
|     98 | 4742 | `}` |
|      - | 4743 | `/*` |
|      - | 4744 | ` * String replacement private data.` |
|      - | 4745 | ` */` |
|      - | 4746 | `typedef struct str_replace_data str_replace_data;` |
|      - | 4747 | `struct str_replace_data` |
|      - | 4748 | `{` |
|      - | 4749 | `	/* Used by the str_replace family to collect the search/replace arguments. */` |
|      - | 4750 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 4751 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 4752 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 4753 | `};` |
|      - | 4754 | `/*` |
|      - | 4755 | ` * Remove a substring.` |
|      - | 4756 | ` */` |
|      - | 4757 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 4758 | `	for(;;){\` |
|      - | 4759 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 4760 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 4761 | `		++OFFT;\` |
|      - | 4762 | `	}\` |
|      - | 4763 | `}` |
|      - | 4764 | `/*` |
|      - | 4765 | ` * Shift right and insert algorithm.` |
|      - | 4766 | ` */` |
|      - | 4767 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 4768 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 4769 | `		for(;;){\` |
|      - | 4770 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 4771 | `			if(INLEN < 1 ) { break; }\` |
|      - | 4772 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 4773 | `			--INLEN; \` |
|      - | 4774 | `		}\` |
|      - | 4775 | `		for(;;){\` |
|      - | 4776 | `				if(ELEN < 1) { break; }\` |
|      - | 4777 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 4778 | `				OFFT++;\` |
|      - | 4779 | `				ENTRY++;\` |
|      - | 4780 | `				--ELEN;\` |
|      - | 4781 | `		}\` |
|      - | 4782 | `}` |
|      - | 4783 | `/*` |
|      - | 4784 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 4785 | ` * replacement string [i.e: zReplace].` |
|      - | 4786 | ` */` |
|    170 | 4787 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      5 | 4788 | `{` |
|    175 | 4789 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 4790 | `	sxu32 n,m;` |
|    175 | 4791 | `	n = SyBlobLength(pWorker);` |
|    175 | 4792 | `	m = nOfft;` |
|      - | 4793 | `	/* Delete the old entry */` |
|   7623 | 4794 | `	STRDEL(zInput,n,m,nLen);` |
|    175 | 4795 | `	SyBlobLength(pWorker) -= nLen;` |
|    175 | 4796 | `	if( nReplen > 0 ){` |
|    165 | 4797 | `		sxi32 iRep = nReplen;` |
|      - | 4798 | `		sxi32 rc;` |
|      - | 4799 | `		/*` |
|      - | 4800 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 4801 | `		 * string.` |
|      - | 4802 | `		 */` |
|    165 | 4803 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|    165 | 4804 | `		if( rc != SXRET_OK ){` |
|      - | 4805 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 4806 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 4807 | `			return rc;` |
|      - | 4808 | `		}` |
|      - | 4809 | `		/* Perform the insertion now */` |
|    165 | 4810 | `		zInput = (char *)SyBlobData(pWorker);` |
|    165 | 4811 | `		n = SyBlobLength(pWorker);` |
|   7525 | 4812 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|    165 | 4813 | `		SyBlobLength(pWorker) += nReplen;` |
|     80 | 4814 | `	}` |
|    175 | 4815 | `	return SXRET_OK;` |
|     90 | 4816 | `}` |
|      - | 4817 | `/*` |
|      - | 4818 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 4819 | ` * to collect search/replace string.` |
|      - | 4820 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 4821 | ` */` |
|    232 | 4822 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      5 | 4823 | `{` |
|    237 | 4824 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 4825 | `	SyString sWorker;` |
|      - | 4826 | `	const char *zIn;` |
|      - | 4827 | `	int nByte;` |
|      - | 4828 | `	/* Extract a string representation of the given argument */` |
|    237 | 4829 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|    237 | 4830 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|    237 | 4831 | `	if( nByte > 0 ){` |
|      - | 4832 | `		char *zDup;` |
|      - | 4833 | `		/* Duplicate the chunk */` |
|    231 | 4834 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 4835 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 4836 | `			);` |
|    231 | 4837 | `		if( zDup == 0 ){` |
|      - | 4838 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 4839 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 4840 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 4841 | `			return SXERR_MEM;` |
|      - | 4842 | `		}` |
|    231 | 4843 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 4844 | `		/* Save the chunk */` |
|    231 | 4845 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|    113 | 4846 | `	}` |
|      - | 4847 | `	/* Save for later processing */` |
|    237 | 4848 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 4849 | `	/* All done */` |
|    116 | 4850 | `	SXUNUSED(pKey); /* cc warning */` |
|    237 | 4851 | `	return PH7_OK;` |
|    121 | 4852 | `}` |
|      - | 4853 | `/*` |
|      - | 4854 | ` * Run the collected search/replace pairs over a single subject string, writing` |
|      - | 4855 | ` * the transformed bytes into pOut (reset here). Shared by the scalar-subject and` |
|      - | 4856 | ` * the array-subject (element-wise) paths. The search/replace SySets are walked` |
|      - | 4857 | ` * fresh on every call — cursors are reset here — so each array element is` |
|      - | 4858 | ` * transformed independently, exactly like php. Returns SXRET_OK, or SXERR_MEM` |
|      - | 4859 | ` * on an allocation failure inside StringReplace.` |
|      - | 4860 | ` *` |
|      - | 4861 | ` * *pnCount is INCREMENTED (never reset) by the number of replacements performed,` |
|      - | 4862 | ` * so an array subject accumulates across its elements exactly like php's &$count.` |
|      - | 4863 | ` */` |
|  45228 | 4864 | `static sxi32 StrReplaceOneSubject(` |
|      - | 4865 | `	SyBlob *pOut,             /* Output buffer (reset then filled here) */` |
|      - | 4866 | `	const char *zSubject,     /* Subject bytes */` |
|      - | 4867 | `	sxu32 nSubject,           /* Subject length */` |
|      - | 4868 | `	SySet *pSearch,           /* Collected search terms */` |
|      - | 4869 | `	SySet *pReplace,          /* Collected replacement terms */` |
|      - | 4870 | `	int rep_str,              /* TRUE: a single replacement reused for every search */` |
|      - | 4871 | `	ProcStringMatch xMatch,   /* SyBlobSearch (str_replace) / iPatternMatch (str_ireplace) */` |
|      - | 4872 | `	sxi64 *pnCount            /* Running replacement count (incremented here) */` |
|      - | 4873 | `	)` |
|      5 | 4874 | `{` |
|      - | 4875 | `	SyString *pSearch_,*pReplace_,sEmpty;` |
|      - | 4876 | `	sxi32 rc;` |
|  45233 | 4877 | `	SyBlobReset(pOut);` |
|  45233 | 4878 | `	if( nSubject > 0 ){` |
|  31657 | 4879 | `		rc = SyBlobAppend(pOut,(const void *)zSubject,nSubject);` |
|  31657 | 4880 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4881 | `			return rc;` |
|      - | 4882 | `		}` |
|  15826 | 4883 | `	}` |
|  45233 | 4884 | `	SyStringInitFromBuf(&sEmpty,"",0);` |
|  45233 | 4885 | `	SySetResetCursor(pSearch);` |
|  45233 | 4886 | `	SySetResetCursor(pReplace);` |
|  45233 | 4887 | `	pSearch_ = pReplace_ = 0; /* cc warning */` |
|  90571 | 4888 | `	while( SXRET_OK == SySetGetNextEntry(pSearch,(void **)&pSearch_) ){` |
|      - | 4889 | `		sxu32 nCount,nOfft;` |
|  45343 | 4890 | `		if( rep_str ){` |
|      - | 4891 | `			/* Single replacement string reused for every search term */` |
|  45309 | 4892 | `			pReplace_ = (SyString *)SySetPeek(pReplace);` |
|  22687 | 4893 | `		}else if( SXRET_OK != SySetGetNextEntry(pReplace,(void **)&pReplace_) ){` |
|      - | 4894 | `			/* 'replace set' has fewer values than the search set: an empty` |
|      - | 4895 | `			 * string is used for the rest of the replacement values. */` |
|      5 | 4896 | `			pReplace_ = 0;` |
|      2 | 4897 | `		}` |
|  45343 | 4898 | `		if( pReplace_ == 0 ){` |
|      5 | 4899 | `			pReplace_ = &sEmpty;` |
|      2 | 4900 | `		}` |
|  45343 | 4901 | `		if( pSearch_->nByte < 1 ){` |
|      - | 4902 | `			/* php ignores an empty search string, but it still CONSUMED a replace` |
|      - | 4903 | `			 * slot above so the remaining pairs stay aligned. */` |
|     15 | 4904 | `			continue;` |
|      - | 4905 | `		}` |
|  45329 | 4906 | `		nOfft = nCount = 0;` |
|  22747 | 4907 | `		for(;;){` |
|  45499 | 4908 | `			if( nCount >= SyBlobLength(pOut) ){` |
|  13627 | 4909 | `				break;` |
|      - | 4910 | `			}` |
|      - | 4911 | `			/* Perform a pattern lookup */` |
|  47813 | 4912 | `			rc = xMatch(SyBlobDataAt(pOut,nCount),SyBlobLength(pOut) - nCount,` |
|  31872 | 4913 | `				(const void *)pSearch_->zString,pSearch_->nByte,&nOfft);` |
|  31877 | 4914 | `			if( rc != SXRET_OK ){` |
|      - | 4915 | `				/* Pattern not found */` |
|  31707 | 4916 | `				break;` |
|      - | 4917 | `			}` |
|      - | 4918 | `			/* Perform the replace operation */` |
|    260 | 4919 | `			rc = StringReplace(pOut,nCount+nOfft,(int)pSearch_->nByte,` |
|    170 | 4920 | `				pReplace_->zString,(int)pReplace_->nByte);` |
|    175 | 4921 | `			if( rc != SXRET_OK ){` |
|      - | 4922 | `				/* Propagate an allocation failure so the caller raises a fatal` |
|      - | 4923 | `				 * instead of returning a partially-replaced result. */` |
|    ! 0 | 4924 | `				return rc;` |
|      - | 4925 | `			}` |
|    175 | 4926 | `			*pnCount += 1;` |
|      - | 4927 | `			/* Increment offset counter */` |
|    175 | 4928 | `			nCount += nOfft + pReplace_->nByte;` |
|      5 | 4929 | `		}` |
|      5 | 4930 | `	}` |
|  45233 | 4931 | `	return SXRET_OK;` |
|  22619 | 4932 | `}` |
|      - | 4933 | `/* Per-call state for the array-subject form of str_replace()/str_ireplace(). */` |
|      - | 4934 | `typedef struct str_replace_subject str_replace_subject;` |
|      - | 4935 | `struct str_replace_subject` |
|      - | 4936 | `{` |
|      - | 4937 | `	ph7_value *pResult;    /* Result array (keys preserved) */` |
|      - | 4938 | `	ph7_value *pScratch;   /* Reusable string value for each element */` |
|      - | 4939 | `	SyBlob *pWorker;       /* Scratch output buffer for one element */` |
|      - | 4940 | `	SySet *pSearch;        /* Collected search terms */` |
|      - | 4941 | `	SySet *pReplace;       /* Collected replacement terms */` |
|      - | 4942 | `	ProcStringMatch xMatch;/* Match routine (case-sensitive or not) */` |
|      - | 4943 | `	int rep_str;           /* TRUE: scalar $replace */` |
|      - | 4944 | `	sxi64 nReplaced;       /* Replacements performed so far (&$count) */` |
|      - | 4945 | `	sxi32 rc;              /* SXRET_OK or SXERR_MEM */` |
|      - | 4946 | `};` |
|      - | 4947 | `/*` |
|      - | 4948 | ` * ph7_array_walk() callback over an array $subject: string-cast one element, run` |
|      - | 4949 | ` * the search/replace over it, and insert the result under the element's original` |
|      - | 4950 | ` * key. A non-string element is coerced exactly like php (int/float/bool/null via` |
|      - | 4951 | ` * their string form). A nested-array element becomes "Array" — the value matches` |
|      - | 4952 | ` * php, but PHL does not emit php's "Array to string conversion" warning here (the` |
|      - | 4953 | ` * engine raises it at echo/interpolation sites, not this C-level cast; a` |
|      - | 4954 | ` * recorded divergence).` |
|      - | 4955 | ` */` |
|     34 | 4956 | `static int StrReplaceSubjectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 4957 | `{` |
|     35 | 4958 | `	str_replace_subject *pS = (str_replace_subject *)pUserData;` |
|      - | 4959 | `	const char *zSub;` |
|      - | 4960 | `	int nSub;` |
|      - | 4961 | `	/* php coerces every element to string (same cast used everywhere). */` |
|     35 | 4962 | `	zSub = ph7_value_to_string(pData,&nSub);` |
|     34 | 4963 | `	if( StrReplaceOneSubject(pS->pWorker,zSub,(sxu32)(nSub > 0 ? nSub : 0),` |
|     35 | 4964 | `			pS->pSearch,pS->pReplace,pS->rep_str,pS->xMatch,&pS->nReplaced) != SXRET_OK ){` |
|    ! 0 | 4965 | `		pS->rc = SXERR_MEM;` |
|    ! 0 | 4966 | `		return SXERR_ABORT;` |
|      - | 4967 | `	}` |
|      - | 4968 | `	/* Publish the transformed bytes as a string under the original key. */` |
|     35 | 4969 | `	ph7_value_reset_string_cursor(pS->pScratch);` |
|     34 | 4970 | `	if( SyBlobLength(pS->pWorker) > 0` |
|     33 | 4971 | `	 && ph7_value_string(pS->pScratch,(const char *)SyBlobData(pS->pWorker),` |
|     45 | 4972 | `			(int)SyBlobLength(pS->pWorker)) != SXRET_OK ){` |
|    ! 0 | 4973 | `		pS->rc = SXERR_MEM;` |
|    ! 0 | 4974 | `		return SXERR_ABORT;` |
|      - | 4975 | `	}` |
|     35 | 4976 | `	if( ph7_array_add_elem(pS->pResult,pKey,pS->pScratch) != SXRET_OK ){` |
|    ! 0 | 4977 | `		pS->rc = SXERR_MEM;` |
|    ! 0 | 4978 | `		return SXERR_ABORT;` |
|      - | 4979 | `	}` |
|     35 | 4980 | `	return PH7_OK;` |
|     18 | 4981 | `}` |
|      - | 4982 | `/*` |
|      - | 4983 | ` * Write str_replace()/str_ireplace()'s optional by-reference &$count out-param.` |
|      - | 4984 | ` * The call compiler auto-vivifies argument #4 for these two names` |
|      - | 4985 | ` * (GenStateByRefBuiltinMask in compile.c), so an undefined variable, an array` |
|      - | 4986 | ` * element and a property all arrive with a real slot to write through.` |
|      - | 4987 | ` */` |
|  45206 | 4988 | `static void StrReplaceStoreCount(ph7_context *pCtx,int nArg,ph7_value **apArg,sxi64 nReplaced)` |
|      5 | 4989 | `{` |
|      - | 4990 | `	ph7_value sCount;` |
|  45211 | 4991 | `	if( nArg < 4 ){` |
|  45191 | 4992 | `		return;` |
|      - | 4993 | `	}` |
|     21 | 4994 | `	PH7_MemObjInitFromInt(pCtx->pVm,&sCount,nReplaced);` |
|     21 | 4995 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[3],&sCount);` |
|     21 | 4996 | `	PH7_MemObjRelease(&sCount);` |
|  22608 | 4997 | `}` |
|      - | 4998 | `/*` |
|      - | 4999 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 5000 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 5001 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 5002 | ` * Parameters` |
|      - | 5003 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 5004 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 5005 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 5006 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 5007 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 5008 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 5009 | ` * $search` |
|      - | 5010 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 5011 | ` *  to designate multiple needles.` |
|      - | 5012 | ` * $replace` |
|      - | 5013 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 5014 | ` *  to designate multiple replacements.` |
|      - | 5015 | ` * $subject` |
|      - | 5016 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 5017 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 5018 | ` *  of subject, and the return value is an array as well.` |
|      - | 5019 | ` * &$count` |
|      - | 5020 | ` *  If passed, this is set to the number of replacements performed — accumulated` |
|      - | 5021 | ` *  over every search term AND, for an array subject, over every element.` |
|      - | 5022 | ` * Return` |
|      - | 5023 | ` * This function returns a string or an array with the replaced values.` |
|      - | 5024 | ` */` |
|  45206 | 5025 | `PH7_PRIVATE int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 5026 | `{` |
|      - | 5027 | `	SyString sTemp;` |
|      - | 5028 | `	ProcStringMatch xMatch;` |
|      - | 5029 | `	const char *zIn,*zFunc;` |
|      - | 5030 | `	str_replace_data sRep;` |
|      - | 5031 | `	SyBlob sWorker;` |
|      - | 5032 | `	SySet sReplace;` |
|      - | 5033 | `	SySet sSearch;` |
|      - | 5034 | `	sxi64 nReplaced;` |
|      - | 5035 | `	int rep_str;` |
|      - | 5036 | `	int nByte;` |
|      - | 5037 | `	sxi32 rc;` |
|  45211 | 5038 | `	if( nArg < 3 ){` |
|      - | 5039 | `		/* Missing/Invalid arguments,return null */` |
|    ! 0 | 5040 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5041 | `		return PH7_OK;` |
|      - | 5042 | `	}` |
|      - | 5043 | `	/* Initialize fields */` |
|  45211 | 5044 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  45211 | 5045 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  45211 | 5046 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  45211 | 5047 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  45211 | 5048 | `	sRep.pCtx = pCtx;` |
|  45211 | 5049 | `	sRep.pCollector = &sSearch;` |
|  45211 | 5050 | `	rep_str = 0;` |
|  45211 | 5051 | `	nReplaced = 0;` |
|      - | 5052 | `	/* Collect the search term(s) — independent of the subject. */` |
|  45211 | 5053 | `	if( ph7_value_is_array(apArg[0]) ){` |
|    109 | 5054 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|     57 | 5055 | `	}else{` |
|  45107 | 5056 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  45107 | 5057 | `		SyStringInitFromBuf(&sTemp,zIn,nByte > 0 ? nByte : 0);` |
|  45107 | 5058 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 5059 | `	}` |
|      - | 5060 | `	/* Collect the replacement term(s). */` |
|  45211 | 5061 | `	if( ph7_value_is_array(apArg[1]) ){` |
|     13 | 5062 | `		sRep.pCollector = &sReplace;` |
|     13 | 5063 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      7 | 5064 | `	}else{` |
|  45199 | 5065 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  45199 | 5066 | `		rep_str = 1;` |
|  45199 | 5067 | `		SyStringInitFromBuf(&sTemp,zIn,nByte > 0 ? nByte : 0);` |
|  45199 | 5068 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 5069 | `	}` |
|      - | 5070 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  45211 | 5071 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 5072 | `		SySetRelease(&sSearch);` |
|    ! 0 | 5073 | `		SySetRelease(&sReplace);` |
|    ! 0 | 5074 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 5075 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5076 | `	}` |
|      - | 5077 | `	/* Pick the match routine by function name */` |
|  45211 | 5078 | `	zFunc = ph7_function_name(pCtx);` |
|  45211 | 5079 | `	xMatch = SyBlobSearch;` |
|  45211 | 5080 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 5081 | `		/* Case insensitive pattern match */` |
|     17 | 5082 | `		xMatch = iPatternMatch;` |
|      8 | 5083 | `	}` |
|  45211 | 5084 | `	if( ph7_value_is_array(apArg[2]) ){` |
|      - | 5085 | `		/* Array subject: replace element-wise and RETURN AN ARRAY whose keys` |
|      - | 5086 | `		 * mirror the subject's (php semantics). */` |
|      - | 5087 | `		str_replace_subject sSub;` |
|      - | 5088 | `		ph7_value *pResult,*pScratch;` |
|     13 | 5089 | `		pResult = ph7_context_new_array(pCtx);` |
|     13 | 5090 | `		pScratch = ph7_context_new_scalar(pCtx);` |
|     13 | 5091 | `		if( pResult == 0 \|\| pScratch == 0 ){` |
|    ! 0 | 5092 | `			SySetRelease(&sSearch);` |
|    ! 0 | 5093 | `			SySetRelease(&sReplace);` |
|    ! 0 | 5094 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 5095 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 5096 | `		}` |
|     13 | 5097 | `		ph7_value_string(pScratch,"",0); /* force string representation */` |
|     13 | 5098 | `		SyZero(&sSub,sizeof(sSub));` |
|     13 | 5099 | `		sSub.pResult  = pResult;` |
|     13 | 5100 | `		sSub.pScratch = pScratch;` |
|     13 | 5101 | `		sSub.pWorker  = &sWorker;` |
|     13 | 5102 | `		sSub.pSearch  = &sSearch;` |
|     13 | 5103 | `		sSub.pReplace = &sReplace;` |
|     13 | 5104 | `		sSub.xMatch   = xMatch;` |
|     13 | 5105 | `		sSub.rep_str  = rep_str;` |
|     13 | 5106 | `		ph7_array_walk(apArg[2],StrReplaceSubjectWalker,&sSub);` |
|     13 | 5107 | `		SySetRelease(&sSearch);` |
|     13 | 5108 | `		SySetRelease(&sReplace);` |
|     13 | 5109 | `		SyBlobRelease(&sWorker);` |
|     13 | 5110 | `		if( sSub.rc != SXRET_OK ){` |
|    ! 0 | 5111 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 5112 | `		}` |
|     13 | 5113 | `		ph7_result_value(pCtx,pResult);` |
|     13 | 5114 | `		StrReplaceStoreCount(pCtx,nArg,apArg,sSub.nReplaced);` |
|     13 | 5115 | `		return PH7_OK;` |
|      - | 5116 | `	}` |
|      - | 5117 | `	/* Scalar subject: run once and return a string. An empty subject yields the` |
|      - | 5118 | `	 * empty string, and a lone empty search term leaves the subject untouched —` |
|      - | 5119 | `	 * both fall out of StrReplaceOneSubject's empty-term skip. */` |
|  45199 | 5120 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  45199 | 5121 | `	rc = StrReplaceOneSubject(&sWorker,zIn,(sxu32)(nByte > 0 ? nByte : 0),` |
|  22597 | 5122 | `		&sSearch,&sReplace,rep_str,xMatch,&nReplaced);` |
|  45199 | 5123 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 5124 | `		SySetRelease(&sSearch);` |
|    ! 0 | 5125 | `		SySetRelease(&sReplace);` |
|    ! 0 | 5126 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 5127 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5128 | `	}` |
|  45199 | 5129 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  45199 | 5130 | `	SySetRelease(&sSearch);` |
|  45199 | 5131 | `	SySetRelease(&sReplace);` |
|  45199 | 5132 | `	SyBlobRelease(&sWorker);` |
|  45199 | 5133 | `	if( rc != PH7_OK ){` |
|    ! 0 | 5134 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5135 | `	}` |
|  45199 | 5136 | `	StrReplaceStoreCount(pCtx,nArg,apArg,nReplaced);` |
|  45199 | 5137 | `	return PH7_OK;` |
|  22608 | 5138 | `}` |
|      - | 5139 | `/*` |
|      - | 5140 | ` * strtr() array form: a single (key,value) pair copied out of the replace_pairs` |
|      - | 5141 | ` * array. The bytes are owned by a persistent pool (see strtr_collect) rather than` |
|      - | 5142 | ` * the transient walker values, which HashmapWalk releases after each callback, so` |
|      - | 5143 | ` * we store byte offsets into that pool instead of raw pointers.` |
|      - | 5144 | ` */` |
|      - | 5145 | `typedef struct strtr_entry strtr_entry;` |
|      - | 5146 | `struct strtr_entry` |
|      - | 5147 | `{` |
|      - | 5148 | `	sxu32 nKeyOfft; /* Offset of the search key inside the pool */` |
|      - | 5149 | `	sxu32 nKeyLen;  /* Length of the search key */` |
|      - | 5150 | `	sxu32 nValOfft; /* Offset of the replacement inside the pool */` |
|      - | 5151 | `	sxu32 nValLen;  /* Length of the replacement */` |
|      - | 5152 | `};` |
|      - | 5153 | `typedef struct strtr_collect strtr_collect;` |
|      - | 5154 | `struct strtr_collect` |
|      - | 5155 | `{` |
|      - | 5156 | `	SyBlob *pPool;  /* Byte pool holding copied key + value bytes */` |
|      - | 5157 | `	SySet  *pTable; /* Set of strtr_entry (parallel offsets into pPool) */` |
|      - | 5158 | `	sxi32   rc;     /* Carries an allocation failure (SXERR_MEM) out of the walker */` |
|      - | 5159 | `	ph7_context *pCtx; /* Needed to warn about an empty key */` |
|      - | 5160 | `};` |
|      - | 5161 | `/*` |
|      - | 5162 | ` * Collect one replace_pairs entry into the persistent pool/offset table.` |
|      - | 5163 | ` * PHP coerces both the key and the value to string (an integer key becomes its` |
|      - | 5164 | ` * decimal form) and ignores an empty-string key.` |
|      - | 5165 | ` */` |
|     22 | 5166 | `static int StrtrCollectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 5167 | `{` |
|     23 | 5168 | `	strtr_collect *pCol = (strtr_collect *)pUserData;` |
|      - | 5169 | `	const char *zKey,*zVal;` |
|      - | 5170 | `	strtr_entry sEnt;` |
|      - | 5171 | `	int nKey,nVal;` |
|     23 | 5172 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     23 | 5173 | `	if( nKey < 1 ){` |
|      - | 5174 | `		/* PHP ignores an empty-string key, and warns that it did so. */` |
|      3 | 5175 | `		ph7_context_throw_error_format(pCol->pCtx,PH7_CTX_WARNING,` |
|      - | 5176 | `			"Ignoring replacement of empty string");` |
|      3 | 5177 | `		return PH7_OK;` |
|      - | 5178 | `	}` |
|     21 | 5179 | `	zVal = ph7_value_to_string(pData,&nVal);` |
|     21 | 5180 | `	sEnt.nKeyOfft = SyBlobLength(pCol->pPool);` |
|     21 | 5181 | `	sEnt.nKeyLen  = (sxu32)nKey;` |
|     21 | 5182 | `	if( SyBlobAppend(pCol->pPool,(const void *)zKey,(sxu32)nKey) != SXRET_OK ){` |
|    ! 0 | 5183 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 5184 | `		return SXERR_ABORT;` |
|      - | 5185 | `	}` |
|     21 | 5186 | `	sEnt.nValOfft = SyBlobLength(pCol->pPool);` |
|     21 | 5187 | `	sEnt.nValLen  = (sxu32)nVal;` |
|     21 | 5188 | `	if( nVal > 0 && SyBlobAppend(pCol->pPool,(const void *)zVal,(sxu32)nVal) != SXRET_OK ){` |
|    ! 0 | 5189 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 5190 | `		return SXERR_ABORT;` |
|      - | 5191 | `	}` |
|     21 | 5192 | `	if( SySetPut(pCol->pTable,(const void *)&sEnt) != SXRET_OK ){` |
|    ! 0 | 5193 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 5194 | `		return SXERR_ABORT;` |
|      - | 5195 | `	}` |
|     21 | 5196 | `	return PH7_OK;` |
|     12 | 5197 | `}` |
|      - | 5198 | `/*` |
|      - | 5199 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 5200 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 5201 | ` *  Translate characters or replace substrings.` |
|      - | 5202 | ` * Parameters` |
|      - | 5203 | ` *  $str` |
|      - | 5204 | ` *  The string being translated.` |
|      - | 5205 | ` * $from` |
|      - | 5206 | ` *  The string being translated to to.` |
|      - | 5207 | ` * $to` |
|      - | 5208 | ` *  The string replacing from.` |
|      - | 5209 | ` * $replace_pairs` |
|      - | 5210 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 5211 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 5212 | ` * Return` |
|      - | 5213 | ` *  The translated string.` |
|      - | 5214 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 5215 | ` */` |
|     94 | 5216 | `PH7_PRIVATE int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5217 | `{` |
|      - | 5218 | `	const char *zIn;` |
|      - | 5219 | `	char zGiven[64];` |
|      - | 5220 | `	int nLen;` |
|     95 | 5221 | `	if( nArg < 1 ){` |
|      - | 5222 | `		/* Nothing to replace,return FALSE */` |
|    ! 0 | 5223 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5224 | `		return PH7_OK;` |
|      - | 5225 | `	}` |
|      - | 5226 | `	/*` |
|      - | 5227 | `	 * php dispatches strtr() on ARITY between two overloads — strtr(string, array)` |
|      - | 5228 | ``	 * and strtr(string, string, string) — so $from's expected type is `array` with`` |
|      - | 5229 | ``	 * two arguments and `string` with three, and the stub's `array\|string` union is`` |
|      - | 5230 | `	 * a wording php itself never emits. One signature cannot express that, so the` |
|      - | 5231 | `	 * shared ZPP screen skips this builtin (azSelfChecked[] in vm_arg_check.c) and` |
|      - | 5232 | `	 * the dispatch happens here, in php's left-to-right argument order.` |
|      - | 5233 | `	 *` |
|      - | 5234 | `	 * Both directions used to pass silently: a 2-argument string $from` |
|      - | 5235 | `	 * (strtr("abc","ab")) returned the subject UNCHANGED, and a 3-argument array` |
|      - | 5236 | `	 * $from was likewise ignored — the caller got its input back as if it had been` |
|      - | 5237 | `	 * translated.` |
|      - | 5238 | `	 */` |
|     95 | 5239 | `	if( !PH7_ArgSatisfiesString(apArg[0]) ){` |
|      4 | 5240 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5241 | `			"strtr(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 5242 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|      - | 5243 | `	}` |
|     93 | 5244 | `	if( nArg == 2 ){` |
|     31 | 5245 | `		if( !ph7_value_is_array(apArg[1]) ){` |
|     22 | 5246 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5247 | `				"strtr(): Argument #2 ($from) must be of type array, %s given",` |
|     14 | 5248 | `				VmValueGivenName(apArg[1],zGiven,sizeof(zGiven)));` |
|      1 | 5249 | `		}` |
|     71 | 5250 | `	}else if( nArg > 2 ){` |
|     63 | 5251 | `		if( !PH7_ArgSatisfiesString(apArg[1]) ){` |
|     10 | 5252 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5253 | `				"strtr(): Argument #2 ($from) must be of type string, %s given",` |
|      6 | 5254 | `				VmValueGivenName(apArg[1],zGiven,sizeof(zGiven)));` |
|      - | 5255 | `		}` |
|      - | 5256 | ``		/* $to is php's `string`, but a null one stays accepted (php coerces it to`` |
|      - | 5257 | `		 * "" with a deprecation, and both engines answer the subject unchanged). */` |
|     57 | 5258 | `		if( !ph7_value_is_null(apArg[2]) && !PH7_ArgSatisfiesString(apArg[2]) ){` |
|      4 | 5259 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5260 | `				"strtr(): Argument #3 ($to) must be of type string, %s given",` |
|      2 | 5261 | `				VmValueGivenName(apArg[2],zGiven,sizeof(zGiven)));` |
|      - | 5262 | `		}` |
|     27 | 5263 | `	}` |
|     71 | 5264 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     71 | 5265 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 5266 | `		/* Invalid arguments */` |
|      3 | 5267 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      3 | 5268 | `		return PH7_OK;` |
|      - | 5269 | `	}` |
|     76 | 5270 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 5271 | `		strtr_collect sCol;` |
|      - | 5272 | `		SyBlob sPool,sWorker;` |
|      - | 5273 | `		SySet sTable;` |
|      - | 5274 | `		const char *zPool;` |
|      - | 5275 | `		strtr_entry *pEnt;` |
|      - | 5276 | `		sxi32 rc;` |
|      - | 5277 | `		int i,iRun;` |
|      - | 5278 | `		/*` |
|      - | 5279 | `		 * PHP's array-form strtr is a single left-to-right pass over the subject:` |
|      - | 5280 | `		 * at every position it substitutes the LONGEST replace_pairs key that` |
|      - | 5281 | `		 * matches there, then advances past the key (replacements are never` |
|      - | 5282 | `		 * rescanned). It is not a sequential per-key global replace. First copy` |
|      - | 5283 | `		 * the pairs into a persistent pool, then run that scan.` |
|      - | 5284 | `		 */` |
|     15 | 5285 | `		SyBlobInit(&sPool,&pCtx->pVm->sAllocator);` |
|     15 | 5286 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     15 | 5287 | `		SySetInit(&sTable,&pCtx->pVm->sAllocator,sizeof(strtr_entry));` |
|     15 | 5288 | `		sCol.pPool  = &sPool;` |
|     15 | 5289 | `		sCol.pTable = &sTable;` |
|     15 | 5290 | `		sCol.rc     = SXRET_OK;` |
|     15 | 5291 | `		sCol.pCtx   = pCtx;` |
|     15 | 5292 | `		ph7_array_walk(apArg[1],StrtrCollectWalker,&sCol);` |
|     15 | 5293 | `		if( sCol.rc != SXRET_OK ){` |
|      - | 5294 | `			/* Allocation failure while collecting the pairs: surface a fatal */` |
|    ! 0 | 5295 | `			SyBlobRelease(&sPool);` |
|    ! 0 | 5296 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 5297 | `			SySetRelease(&sTable);` |
|    ! 0 | 5298 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 5299 | `		}` |
|      - | 5300 | `		/* The pool is now stable, so offsets can be resolved against its base. */` |
|     15 | 5301 | `		zPool = (const char *)SyBlobData(&sPool);` |
|     15 | 5302 | `		rc = SXRET_OK;` |
|     15 | 5303 | `		iRun = 0; /* Start of the pending run of unmatched bytes copied verbatim. */` |
|     59 | 5304 | `		for( i = 0 ; i < nLen ; ){` |
|     45 | 5305 | `			strtr_entry *pBest = 0;` |
|     45 | 5306 | `			sxu32 nBest = 0;` |
|      - | 5307 | `			/* Pick the longest key that matches at the current position. */` |
|     45 | 5308 | `			SySetResetCursor(&sTable);` |
|    105 | 5309 | `			while( SXRET_OK == SySetGetNextEntry(&sTable,(void **)&pEnt) ){` |
|     60 | 5310 | `				if( pEnt->nKeyLen > nBest` |
|     56 | 5311 | `					&& pEnt->nKeyLen <= (sxu32)(nLen - i)` |
|     52 | 5312 | `					&& SyMemcmp(zPool + pEnt->nKeyOfft,zIn + i,pEnt->nKeyLen) == 0 ){` |
|     31 | 5313 | `					nBest = pEnt->nKeyLen;` |
|     31 | 5314 | `					pBest = pEnt;` |
|     15 | 5315 | `				}` |
|      1 | 5316 | `			}` |
|     45 | 5317 | `			if( pBest == 0 ){` |
|      - | 5318 | `				/* No key here: extend the literal run and copy it in one shot later. */` |
|     19 | 5319 | `				i++;` |
|     19 | 5320 | `				continue;` |
|      - | 5321 | `			}` |
|      - | 5322 | `			/* Flush the pending literal run, then the replacement. */` |
|     27 | 5323 | `			if( i > iRun ){` |
|      5 | 5324 | `				rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(i - iRun));` |
|      2 | 5325 | `			}` |
|     27 | 5326 | `			if( rc == SXRET_OK && pBest->nValLen > 0 ){` |
|     27 | 5327 | `				rc = SyBlobAppend(&sWorker,zPool + pBest->nValOfft,pBest->nValLen);` |
|     13 | 5328 | `			}` |
|     27 | 5329 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 5330 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 5331 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 5332 | `				SySetRelease(&sTable);` |
|    ! 0 | 5333 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 5334 | `			}` |
|     27 | 5335 | `			i += (int)pBest->nKeyLen;` |
|     27 | 5336 | `			iRun = i;` |
|      1 | 5337 | `		}` |
|      - | 5338 | `		/* Flush the trailing literal run. */` |
|     15 | 5339 | `		if( nLen > iRun ){` |
|      7 | 5340 | `			rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(nLen - iRun));` |
|      7 | 5341 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 5342 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 5343 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 5344 | `				SySetRelease(&sTable);` |
|    ! 0 | 5345 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 5346 | `			}` |
|      3 | 5347 | `		}` |
|      - | 5348 | `		/* All done, return the result string */` |
|     22 | 5349 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|     14 | 5350 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 5351 | `		/* Clean-up */` |
|     15 | 5352 | `		SyBlobRelease(&sPool);` |
|     15 | 5353 | `		SyBlobRelease(&sWorker);` |
|     15 | 5354 | `		SySetRelease(&sTable);` |
|     15 | 5355 | `		if( rc != PH7_OK ){` |
|    ! 0 | 5356 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 5357 | `		}` |
|      8 | 5358 | `	}else{` |
|      - | 5359 | `		int i,flen,tlen,c,iOfft;` |
|      - | 5360 | `		const char *zFrom,*zTo;` |
|     55 | 5361 | `		if( nArg < 3 ){` |
|      - | 5362 | `			/* Nothing to replace */` |
|    ! 0 | 5363 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5364 | `			return PH7_OK;` |
|      - | 5365 | `		}` |
|      - | 5366 | `		/* Extract given arguments */` |
|     55 | 5367 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|     55 | 5368 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|     55 | 5369 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 5370 | `			/* Nothing to replace */` |
|    ! 0 | 5371 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5372 | `			return PH7_OK;` |
|      - | 5373 | `		}` |
|      - | 5374 | `		/* Start the replace process */` |
|    167 | 5375 | `		for( i = 0 ; i < nLen ; ++i ){` |
|    113 | 5376 | `			c = zIn[i];` |
|    113 | 5377 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|     19 | 5378 | `				if ( iOfft < tlen ){` |
|     19 | 5379 | `					c = zTo[iOfft];` |
|      9 | 5380 | `				}` |
|      9 | 5381 | `			}` |
|    113 | 5382 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 5383 |  |
|     57 | 5384 | `		}` |
|      - | 5385 | `	}` |
|     69 | 5386 | `	return PH7_OK;` |
|     48 | 5387 | `}` |
|      - | 5388 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 5389 |  |
