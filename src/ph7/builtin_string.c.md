# src/ph7/builtin_string.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2302/2693 lines (85.48%)

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
| 277419 |   60 | `PH7_PRIVATE int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |   61 | `{` |
|      - |   62 | `	const char *zSource;` |
|      - |   63 | `	int nSrcLen;` |
|      - |   64 | `	sxi64 iStart,iEnd;` |
| 277424 |   65 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"substr",1,"$string"); }` |
| 277424 |   66 | `	if( nArg < 2 ){` |
|      - |   67 | `		/* Arity is enforced at the call boundary; nothing sensible to return here. */` |
|    ! 0 |   68 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 |   69 | `		return PH7_OK;` |
|      - |   70 | `	}` |
|      - |   71 | `	/* Extract the target string */` |
| 277424 |   72 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|      - |   73 | `	/* Extract the offset */` |
|      - |   74 | `	{` |
| 277424 |   75 | `		sxi64 iTmp = 0;` |
| 277424 |   76 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"substr",2,"$offset","int",&iTmp);` |
| 277424 |   77 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |   78 | `			return rcArg;` |
|      - |   79 | `		}` |
| 277424 |   80 | `		iStart = iTmp;` |
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
| 277424 |   92 | `	if( iStart < 0 ){` |
|  32945 |   93 | `		iStart += nSrcLen;` |
|  32945 |   94 | `		if( iStart < 0 ){` |
|      5 |   95 | `			iStart = 0;` |
|      7 |   96 | `		}` |
| 260954 |   97 | `	}else if( iStart > nSrcLen ){` |
|      7 |   98 | `		iStart = nSrcLen;` |
|      3 |   99 | `	}` |
| 277424 |  100 | `	iEnd = nSrcLen;` |
| 277424 |  101 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
| 196583 |  102 | `		sxi64 iLen = 0;` |
| 196583 |  103 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],"substr",3,"$length","?int",&iLen);` |
| 196583 |  104 | `		if( rcArg != PH7_OK ){` |
|    ! 0 |  105 | `			return rcArg;` |
|      - |  106 | `		}` |
| 196583 |  107 | `		if( iLen < 0 ){` |
|  32577 |  108 | `			iEnd = (sxi64)nSrcLen + iLen;` |
| 180297 |  109 | `		}else if( iLen > (sxi64)nSrcLen - iStart ){` |
|  18391 |  110 | `			iEnd = nSrcLen;` |
|   9198 |  111 | `		}else{` |
| 145625 |  112 | `			iEnd = iStart + iLen;` |
|      - |  113 | `		}` |
|  98289 |  114 | `	}` |
| 277424 |  115 | `	if( iEnd < iStart ){` |
|      3 |  116 | `		iEnd = iStart;` |
|      1 |  117 | `	}` |
| 277424 |  118 | `	ph7_result_string(pCtx,&zSource[iStart],(int)(iEnd - iStart));` |
| 277424 |  119 | `	return PH7_OK;` |
| 138817 |  120 | `}` |
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
| 421097 |  319 | `static void StrNullArgNotice(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNum,const char *zParamName)` |
|      5 |  320 | `{` |
| 421102 |  321 | `	if( ph7_value_is_null(pArg) ){` |
|     13 |  322 | `		PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  323 | `			"%s(): Argument #%d (%s) must be of type string, null given",` |
|      4 |  324 | `			zFunc,iArgNum,zParamName);` |
|      4 |  325 | `	}` |
| 421102 |  326 | `}` |
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
|     18 | 1101 | `PH7_PRIVATE int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1102 | `{` |
|      - | 1103 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1104 | `	int nLen;` |
|      - | 1105 | `	/* PHP enforces exactly one argument. */` |
|     20 | 1106 | `	if( nArg != 1 ){` |
|    ! 0 | 1107 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1108 | `			"ArgumentCountError",` |
|      - | 1109 | `			"addslashes() expects exactly 1 argument, %d given",` |
|    ! 0 | 1110 | `			nArg` |
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
|     11 | 1164 | `}` |
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
|     30 | 1235 | `PH7_PRIVATE int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1236 | `{` |
|      - | 1237 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - | 1238 | `	char aMask[256];` |
|      - | 1239 | `	int nLen,nMask;` |
|      - | 1240 | `	/* PHP enforces exactly two arguments. */` |
|     34 | 1241 | `	if( nArg != 2 ){` |
|    ! 0 | 1242 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1243 | `			"ArgumentCountError",` |
|      - | 1244 | `			"addcslashes() expects exactly 2 arguments, %d given",` |
|    ! 0 | 1245 | `			nArg` |
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
|     19 | 1332 | `}` |
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
|      8 | 1396 | `PH7_PRIVATE int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1397 | `{` |
|      - | 1398 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1399 | `	int nLen;` |
|      9 | 1400 | `	if( nArg < 1 ){` |
|      - | 1401 | `		/* Nothing to process,retun NULL */` |
|    ! 0 | 1402 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1403 | `		return PH7_OK;` |
|      - | 1404 | `	}` |
|      - | 1405 | `	/* Extract the string to process */` |
|      9 | 1406 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 1407 | `	if( zIn == 0 ){` |
|    ! 0 | 1408 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1409 | `		return PH7_OK;` |
|      - | 1410 | `	}` |
|      9 | 1411 | `	zEnd = &zIn[nLen];` |
|      9 | 1412 | `	zCur = 0; /* cc warning */` |
|      - | 1413 | `	/* Seed an empty string result: the loop below only ever APPENDS, so without` |
|      - | 1414 | `	 * this an empty input would leave the return value untouched and answer` |
|      - | 1415 | `	 * NULL where php answers "". */` |
|      9 | 1416 | `	ph7_result_string(pCtx,"",0);` |
|      - | 1417 | `	/* Encode the string */` |
|      5 | 1418 | `	for(;;){` |
|     11 | 1419 | `		if( zIn >= zEnd ){` |
|      - | 1420 | `			/* No more input */` |
|      5 | 1421 | `			break;` |
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
|      9 | 1441 | `	return PH7_OK;` |
|      5 | 1442 | `}` |
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
|  92838 | 1610 | `PH7_PRIVATE int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1611 | `{` |
|  92843 | 1612 | `	int iLen = 0;` |
|  92843 | 1613 | `	if( nArg > 0 ){` |
|  92843 | 1614 | `		StrNullArgNotice(pCtx,apArg[0],"strlen",1,"$string");` |
|  92843 | 1615 | `		ph7_value_to_string(apArg[0],&iLen);` |
|  46624 | 1616 | `	}` |
|      - | 1617 | `	/* String length */` |
|  92843 | 1618 | `	ph7_result_int(pCtx,iLen);` |
|  92843 | 1619 | `	return PH7_OK;` |
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
|     46 | 1814 | `PH7_PRIVATE int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1815 | `{` |
|      - | 1816 | `	const char *z1,*z2;` |
|      - | 1817 | `	int res;` |
|      - | 1818 | `	int n;` |
|     51 | 1819 | `	if( nArg < 3 ){` |
|      - | 1820 | `		/* Perform a standard comparison */` |
|    ! 0 | 1821 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 1822 | `	}` |
|      - | 1823 | `	/* Desired comparison length */` |
|     51 | 1824 | `	n  = ph7_value_to_int(apArg[2]);` |
|     51 | 1825 | `	if( n < 0 ){` |
|      - | 1826 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 1827 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1828 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 1829 | `			ph7_function_name(pCtx));` |
|      - | 1830 | `	}` |
|      - | 1831 | `	/* Perform the comparison */` |
|     49 | 1832 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     49 | 1833 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     49 | 1834 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 1835 | `	/* Comparison result */` |
|     49 | 1836 | `	ph7_result_int(pCtx,res);` |
|     49 | 1837 | `	return PH7_OK;` |
|     28 | 1838 | `}` |
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
| 155326 | 1858 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 1859 | `{` |
|  77663 | 1860 | `	SXUNUSED(pKey);` |
| 155331 | 1861 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 1862 | `	const char *zData;` |
|      - | 1863 | `	int nLen;` |
| 155331 | 1864 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
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
| 155329 | 1888 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 1889 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 155329 | 1890 | `	if( pData->bFirst ){` |
|  33219 | 1891 | `		pData->bFirst = 0;` |
| 138722 | 1892 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 1893 | `		/* append the separator first */` |
| 122035 | 1894 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 1895 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1896 | `			return PH7_ABORT;` |
|      - | 1897 | `		}` |
|  61015 | 1898 | `	}` |
|      - | 1899 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 155329 | 1900 | `	if( nLen > 0 ){` |
| 143107 | 1901 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1902 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1903 | `			return PH7_ABORT;` |
|      - | 1904 | `		}` |
|  71551 | 1905 | `	}` |
| 155329 | 1906 | `	return PH7_OK;` |
|  77668 | 1907 | `}` |
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
|  33280 | 1921 | `PH7_PRIVATE int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1922 | `{` |
|      - | 1923 | `	struct implode_data imp_data;` |
|  33285 | 1924 | `	int i = 1;` |
|  33285 | 1925 | `	if( nArg < 1 ){` |
|      - | 1926 | `		/* Missing argument,return NULL */` |
|    ! 0 | 1927 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1928 | `		return PH7_OK;` |
|      - | 1929 | `	}` |
|      - | 1930 | `	/* Prepare the implode context */` |
|  33285 | 1931 | `	imp_data.pCtx = pCtx;` |
|  33285 | 1932 | `	imp_data.bRecursive = 0;` |
|  33285 | 1933 | `	imp_data.bFirst = 1;` |
|  33285 | 1934 | `	imp_data.nRecCount = 0;` |
|  33285 | 1935 | `	imp_data.rc = SXRET_OK;` |
|  33285 | 1936 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  33283 | 1937 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  33283 | 1938 | `		if( nArg > 1 && !ph7_value_is_array(apArg[1]) && !ph7_value_is_null(apArg[1]) ){` |
|      - | 1939 | `			/* php: implode($glue, $pieces) requires an ARRAY. PH7 stringified whatever it` |
|      - | 1940 | `			 * was handed, so implode(",", 5) quietly returned "5". */` |
|      - | 1941 | `			char zBuf[64];` |
|      4 | 1942 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 1943 | `				"implode(): Argument #2 ($array) must be of type ?array, %s given",` |
|      2 | 1944 | `				VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 1945 | `		}` |
|  16643 | 1946 | `	}else{` |
|      3 | 1947 | `		imp_data.zSep = 0;` |
|      3 | 1948 | `		imp_data.nSeplen = 0;` |
|      3 | 1949 | `		i = 0;` |
|      - | 1950 | `	}` |
|  33283 | 1951 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 1952 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1953 | `	}` |
|      - | 1954 | `	/* Start the 'join' process */` |
|  66561 | 1955 | `	while( i < nArg ){` |
|  33283 | 1956 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1957 | `			/* Iterate throw array entries */` |
|  33283 | 1958 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 1959 | `			/* Surface a callback allocation failure as a fatal */` |
|  33283 | 1960 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 1961 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1962 | `			}` |
|  16644 | 1963 | `		}else{` |
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
|  33283 | 1983 | `		i++;` |
|      5 | 1984 | `	}` |
|  33283 | 1985 | `	return PH7_OK;` |
|  16645 | 1986 | `}` |
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
|   6852 | 2086 | `PH7_PRIVATE int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2087 | `{` |
|      - | 2088 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2089 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2090 | `	ph7_value *pArray;` |
|      - | 2091 | `	ph7_value *pValue;` |
|      - | 2092 | `	sxu32 nOfft;` |
|      - | 2093 | `	sxi32 rc;` |
|   6857 | 2094 | `	if( nArg < 2 ){` |
|      - | 2095 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2096 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2097 | `		return PH7_OK;` |
|      - | 2098 | `	}` |
|      - | 2099 | `	/* Extract the delimiter */` |
|   6857 | 2100 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   6857 | 2101 | `	if( nDelim < 1 ){` |
|      - | 2102 | `		/* Empty delimiter: PHP 8 throws a catchable ValueError. */` |
|      5 | 2103 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2104 | `			"explode(): Argument #1 ($separator) must not be empty");` |
|      - | 2105 | `	}` |
|      - | 2106 | `	/* Extract the string */` |
|   6853 | 2107 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   6853 | 2108 | `	if( nStrlen < 1 ){` |
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
|   6841 | 2134 | `	zEnd = &zString[nStrlen];` |
|      - | 2135 | `	/* Create the array */` |
|   6841 | 2136 | `	pArray =  ph7_context_new_array(pCtx);` |
|   6841 | 2137 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   6841 | 2138 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2139 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2140 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2141 | `		return PH7_OK;` |
|      - | 2142 | `	}` |
|      - | 2143 | `	/* Set a defualt limit */` |
|   6841 | 2144 | `	iLimit = SXI32_HIGH;` |
|   6841 | 2145 | `	if( nArg > 2 ){` |
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
|  82209 | 2180 | `	for(;;){` |
| 164423 | 2181 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 164423 | 2182 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2183 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   6825 | 2184 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   6825 | 2185 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2186 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2187 | `			}` |
|   6825 | 2188 | `			break;` |
|      - | 2189 | `		}` |
|      - | 2190 | `		/* Point to the desired offset */` |
| 157603 | 2191 | `		zCur = &zString[nOfft];` |
|      - | 2192 | `		/* Perform the store operation (may be empty) */` |
| 157603 | 2193 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 157603 | 2194 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2195 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2196 | `		}` |
|      - | 2197 | `		/* Point beyond the delimiter */` |
| 157603 | 2198 | `		zString = &zCur[nDelim];` |
|      - | 2199 | `		/* Reset the cursor */` |
| 157603 | 2200 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 2201 | `	}` |
|      - | 2202 | `	/* Return the freshly created array */` |
|   6825 | 2203 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2204 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2205 | `	 * released as soon we return from this foregin function.` |
|      - | 2206 | `	 */` |
|   6825 | 2207 | `	return PH7_OK;` |
|   3431 | 2208 | `}` |
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
|  14286 | 2224 | `PH7_PRIVATE int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2225 | `{` |
|  14291 | 2226 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"trim",1,"$string"); }` |
|      - | 2227 | `	const char *zString;` |
|      - | 2228 | `	int nLen;` |
|  14291 | 2229 | `	if( nArg < 1 ){` |
|      - | 2230 | `		/* Missing arguments,return null */` |
|    ! 0 | 2231 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2232 | `		return PH7_OK;` |
|      - | 2233 | `	}` |
|      - | 2234 | `	/* Extract the target string */` |
|  14291 | 2235 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  14291 | 2236 | `	if( nLen < 1 ){` |
|      - | 2237 | `		/* Empty string,return */` |
|    601 | 2238 | `		ph7_result_string(pCtx,"",0);` |
|    601 | 2239 | `		return PH7_OK;` |
|      - | 2240 | `	}` |
|      - | 2241 | `	/* Start the trim process */` |
|  13695 | 2242 | `	if( nArg < 2 ){` |
|      - | 2243 | `		SyString sStr;` |
|      - | 2244 | `		/* Remove white spaces and NUL bytes */` |
|  13665 | 2245 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  34399 | 2246 | `		SyStringFullTrimSafe(&sStr);` |
|  13665 | 2247 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   6835 | 2248 | `	}else{` |
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
|  13695 | 2277 | `	return PH7_OK;` |
|   7148 | 2278 | `}` |
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
|     50 | 2360 | `PH7_PRIVATE int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2361 | `{` |
|     55 | 2362 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"ltrim",1,"$string"); }` |
|      - | 2363 | `	const char *zString;` |
|      - | 2364 | `	int nLen;` |
|     55 | 2365 | `	if( nArg < 1 ){` |
|      - | 2366 | `		/* Missing arguments,return null */` |
|    ! 0 | 2367 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2368 | `		return PH7_OK;` |
|      - | 2369 | `	}` |
|      - | 2370 | `	/* Extract the target string */` |
|     55 | 2371 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     55 | 2372 | `	if( nLen < 1 ){` |
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
|     30 | 2410 | `}` |
|      - | 2411 | `/*` |
|      - | 2412 | ` * string strtolower(string $str)` |
|      - | 2413 | ` *  Make a string lowercase.` |
|      - | 2414 | ` * Parameters` |
|      - | 2415 | ` *  $str` |
|      - | 2416 | ` *   The input string.` |
|      - | 2417 | ` * Returns.` |
|      - | 2418 | ` *  The lowercased string.` |
|      - | 2419 | ` */` |
|  33100 | 2420 | `PH7_PRIVATE int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2421 | `{` |
|  33105 | 2422 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtolower",1,"$string"); }` |
|      - | 2423 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2424 | `	int nLen;` |
|  33105 | 2425 | `	if( nArg < 1 ){` |
|      - | 2426 | `		/* Missing arguments,return null */` |
|    ! 0 | 2427 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2428 | `		return PH7_OK;` |
|      - | 2429 | `	}` |
|      - | 2430 | `	/* Extract the target string */` |
|  33105 | 2431 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  33105 | 2432 | `	if( nLen < 1 ){` |
|      - | 2433 | `		/* Empty string,return */` |
|      3 | 2434 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2435 | `		return PH7_OK;` |
|      - | 2436 | `	}` |
|      - | 2437 | `	/* Perform the requested operation */` |
|  33103 | 2438 | `	zEnd = &zString[nLen];` |
| 104384 | 2439 | `	for(;;){` |
| 208773 | 2440 | `		if( zString >= zEnd ){` |
|      - | 2441 | `			/* No more input,break immediately */` |
|  33103 | 2442 | `			break;` |
|      - | 2443 | `		}` |
| 175675 | 2444 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2445 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2446 | `			zCur = zString;` |
|    ! 0 | 2447 | `			zString++;` |
|    ! 0 | 2448 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2449 | `				zString++;` |
|    ! 0 | 2450 | `			}` |
|      - | 2451 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2452 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2453 | `		}else{` |
| 175675 | 2454 | `			int c = zString[0];` |
| 175675 | 2455 | `			if( SyisUpper(c) ){` |
| 172663 | 2456 | `				c = SyToLower(zString[0]);` |
|  86329 | 2457 | `			}` |
|      - | 2458 | `			/* Append character */` |
| 175675 | 2459 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2460 | `			/* Advance the cursor */` |
| 175675 | 2461 | `			zString++;` |
|      - | 2462 | `		}` |
|      5 | 2463 | `	}` |
|  33103 | 2464 | `	return PH7_OK;` |
|  16555 | 2465 | `}` |
|      - | 2466 | `/*` |
|      - | 2467 | ` * string strtolower(string $str)` |
|      - | 2468 | ` *  Make a string uppercase.` |
|      - | 2469 | ` * Parameters` |
|      - | 2470 | ` *  $str` |
|      - | 2471 | ` *   The input string.` |
|      - | 2472 | ` * Returns.` |
|      - | 2473 | ` *  The uppercased string.` |
|      - | 2474 | ` */` |
|     74 | 2475 | `PH7_PRIVATE int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2476 | `{` |
|     78 | 2477 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtoupper",1,"$string"); }` |
|      - | 2478 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2479 | `	int nLen;` |
|     78 | 2480 | `	if( nArg < 1 ){` |
|      - | 2481 | `		/* Missing arguments,return null */` |
|    ! 0 | 2482 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2483 | `		return PH7_OK;` |
|      - | 2484 | `	}` |
|      - | 2485 | `	/* Extract the target string */` |
|     78 | 2486 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     78 | 2487 | `	if( nLen < 1 ){` |
|      - | 2488 | `		/* Empty string,return */` |
|      6 | 2489 | `		ph7_result_string(pCtx,"",0);` |
|      6 | 2490 | `		return PH7_OK;` |
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
|     41 | 2520 | `}` |
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
|    220 | 2613 | `PH7_PRIVATE int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2614 | `{` |
|      - | 2615 | `	const char *zString;` |
|      - | 2616 | `	int nLen,c;` |
|      - | 2617 | `	/* PHP requires exactly one argument. */` |
|    223 | 2618 | `	if( nArg != 1 ){` |
|    ! 0 | 2619 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2620 | `			"ArgumentCountError",` |
|      - | 2621 | `			"ord() expects exactly 1 argument, %d given",` |
|    ! 0 | 2622 | `			nArg` |
|      - | 2623 | `			);` |
|      - | 2624 | `	}` |
|      - | 2625 | `	/* php only DEPRECATES null here; PHL rejects it. */` |
|    223 | 2626 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 2627 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 2628 | `			"ord(): Argument #1 ($character) must be of type string, null given"` |
|      - | 2629 | `			);` |
|      - | 2630 | `	}` |
|      - | 2631 | `	/* Extract the target string */` |
|    221 | 2632 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    221 | 2633 | `	if( nLen < 1 ){` |
|      - | 2634 | `		/* php only DEPRECATES an empty string here; PHL rejects it. */` |
|      3 | 2635 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2636 | `			"ord(): Argument #1 ($character) must not be empty"` |
|      - | 2637 | `			);` |
|      - | 2638 | `	}` |
|      - | 2639 | `	/* A string longer than one byte: php DEPRECATES it; PHL rejects it. */` |
|    219 | 2640 | `	if( nLen > 1 ){` |
|      3 | 2641 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2642 | `			"ord(): Argument #1 ($character) must be a single byte, use ord($str[0]) instead"` |
|      - | 2643 | `			);` |
|      - | 2644 | `	}` |
|      - | 2645 | `	/* Extract the ASCII value of the first character */` |
|    217 | 2646 | `	c = (unsigned char)zString[0];` |
|      - | 2647 | `	/* Return that value */` |
|    217 | 2648 | `	ph7_result_int(pCtx,c);` |
|    217 | 2649 | `	return PH7_OK;` |
|    113 | 2650 | `}` |
|      - | 2651 | `/*` |
|      - | 2652 | ` * string chr(int $codepoint)` |
|      - | 2653 | ` *  Returns a one-character string containing the character specified` |
|      - | 2654 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 2655 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 2656 | ` * Parameters` |
|      - | 2657 | ` *  $codepoint` |
|      - | 2658 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 2659 | ` *   will be constrained to a single byte.` |
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
|      - | 2689 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 2690 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 2691 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 2692 | `	 * name to avoid the API double-prefixing it. */` |
|   7152 | 2693 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 2694 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 2695 | `			E_DEPRECATED,` |
|      - | 2696 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 2697 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 2698 | `			"The value used will be constrained using % 256"` |
|      - | 2699 | `			);` |
|      2 | 2700 | `	}` |
|      - | 2701 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 2702 | `	 * when taking the address of a wider int. */` |
|   7152 | 2703 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 2704 | `	/* Return the specified character */` |
|   7152 | 2705 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|   7152 | 2706 | `	return PH7_OK;` |
|   3579 | 2707 | `}` |
|      - | 2708 | `/*` |
|      - | 2709 | ` * Binary to hex consumer callback.` |
|      - | 2710 | ` * This callback is the default consumer used by the hash functions` |
|      - | 2711 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 2712 | ` */` |
|   3170 | 2713 | `PH7_PRIVATE int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      3 | 2714 | `{` |
|      - | 2715 | `	/* Append hex chunk verbatim */` |
|   3173 | 2716 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   3173 | 2717 | `	return SXRET_OK;` |
|      3 | 2718 | `}` |
|      - | 2719 |  |
|      - | 2720 | `/*` |
|      - | 2721 | ` * string bin2hex(string $str)` |
|      - | 2722 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 2723 | ` * Parameters` |
|      - | 2724 | ` *  $str` |
|      - | 2725 | ` *   The input string.` |
|      - | 2726 | ` * Returns.` |
|      - | 2727 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 2728 | ` */` |
|    150 | 2729 | `PH7_PRIVATE int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2730 | `{` |
|      - | 2731 | `	const char *zString;` |
|      - | 2732 | `	int nLen;` |
|      - | 2733 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|    153 | 2734 | `	if( nArg != 1 ){` |
|    ! 0 | 2735 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2736 | `			"ArgumentCountError",` |
|      - | 2737 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|    ! 0 | 2738 | `			nArg` |
|      - | 2739 | `			);` |
|      - | 2740 | `	}` |
|      - | 2741 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 2742 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 2743 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 2744 | `	 */` |
|    228 | 2745 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|     75 | 2746 | `		( ph7_value_is_object(apArg[0]) &&` |
|    ! 0 | 2747 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|    ! 0 | 2748 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|    ! 0 | 2749 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 2750 | `		)` |
|      - | 2751 | `	){` |
|    ! 0 | 2752 | `		const char *zType = ph7_type_name(apArg[0]);` |
|    ! 0 | 2753 | `		if( ph7_value_is_object(apArg[0]) ){` |
|    ! 0 | 2754 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    ! 0 | 2755 | `			if( pInst && pInst->pClass ){` |
|    ! 0 | 2756 | `				zType = SyStringData(&pInst->pClass->sName);` |
|    ! 0 | 2757 | `			}` |
|    ! 0 | 2758 | `		}` |
|    ! 0 | 2759 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2760 | `			"TypeError",` |
|      - | 2761 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 2762 | `			zType` |
|      - | 2763 | `			);` |
|      - | 2764 | `	}` |
|      - | 2765 | `	/* Extract the target string */` |
|    153 | 2766 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    153 | 2767 | `	if( nLen < 1 ){` |
|      - | 2768 | `		/* Empty string,return */` |
|     13 | 2769 | `		ph7_result_string(pCtx,"",0);` |
|     13 | 2770 | `		return PH7_OK;` |
|      - | 2771 | `	}` |
|      - | 2772 | `	/* Perform the requested operation */` |
|    141 | 2773 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|    141 | 2774 | `	return PH7_OK;` |
|     78 | 2775 | `}` |
|      - | 2776 |  |
|      - | 2777 | `/* Search callback signature */` |
|      - | 2778 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 2779 | `/*` |
|      - | 2780 | ` * Case-insensitive pattern match.` |
|      - | 2781 | ` * Brute force is the default search method used here.` |
|      - | 2782 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 2783 | ` * well for short/medium texts on modern hardware.` |
|      - | 2784 | ` */` |
|    262 | 2785 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      2 | 2786 | `{` |
|    264 | 2787 | `	const char *zpIn = (const char *)pPattern;` |
|    264 | 2788 | `	const char *zIn = (const char *)pText;` |
|    264 | 2789 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    264 | 2790 | `	const char *zEnd = &zIn[nLen];` |
|      - | 2791 | `	const char *zPtr,*zPtr2;` |
|      - | 2792 | `	int c,d;` |
|    264 | 2793 | `	if( iPatLen > nLen ){` |
|      - | 2794 | `		/* Don't bother processing */` |
|     35 | 2795 | `		return SXERR_NOTFOUND;` |
|      - | 2796 | `	}` |
|    772 | 2797 | `	for(;;){` |
|   1546 | 2798 | `		if( zIn >= zEnd ){` |
|    184 | 2799 | `			break;` |
|      - | 2800 | `		}` |
|   1364 | 2801 | `		c = SyToLower(zIn[0]);` |
|   1364 | 2802 | `		d = SyToLower(zpIn[0]);` |
|   1364 | 2803 | `		if( c == d ){` |
|    188 | 2804 | `			zPtr   = &zIn[1];` |
|    188 | 2805 | `			zPtr2  = &zpIn[1];` |
|    144 | 2806 | `			for(;;){` |
|    290 | 2807 | `				if( zPtr2 >= zpEnd ){` |
|      - | 2808 | `					/* Pattern found */` |
|     47 | 2809 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     47 | 2810 | `					return SXRET_OK;` |
|      - | 2811 | `				}` |
|    244 | 2812 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 2813 | `					break;` |
|      - | 2814 | `				}` |
|    244 | 2815 | `				c = SyToLower(zPtr[0]);` |
|    244 | 2816 | `				d = SyToLower(zPtr2[0]);` |
|    244 | 2817 | `				if( c != d ){` |
|    142 | 2818 | `					break;` |
|      - | 2819 | `				}` |
|    103 | 2820 | `				zPtr++; zPtr2++;` |
|      1 | 2821 | `			}` |
|     70 | 2822 | `		}` |
|   1318 | 2823 | `		zIn++;` |
|      2 | 2824 | `	}` |
|      - | 2825 | `	/* Pattern not found */` |
|    184 | 2826 | `	return SXERR_NOTFOUND;` |
|    133 | 2827 | `}` |
|      - | 2828 | `/*` |
|      - | 2829 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2830 | ` *  Find the first occurrence of a string.` |
|      - | 2831 | ` * Parameters` |
|      - | 2832 | ` *  $haystack` |
|      - | 2833 | ` *   The input string.` |
|      - | 2834 | ` * $needle` |
|      - | 2835 | ` *   Search pattern (must be a string).` |
|      - | 2836 | ` * $before_needle` |
|      - | 2837 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2838 | ` *   of the needle (excluding the needle).` |
|      - | 2839 | ` * Return` |
|      - | 2840 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2841 | ` */` |
|     12 | 2842 | `PH7_PRIVATE int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2843 | `{` |
|     13 | 2844 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2845 | `	const char *zBlob,*zPattern;` |
|      - | 2846 | `	int nLen,nPatLen;` |
|      - | 2847 | `	sxu32 nOfft;` |
|      - | 2848 | `	sxi32 rc;` |
|     13 | 2849 | `	if( nArg < 2 ){` |
|      - | 2850 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2851 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2852 | `		return PH7_OK;` |
|      - | 2853 | `	}` |
|      - | 2854 | `	/* Extract the needle and the haystack */` |
|     13 | 2855 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 2856 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     13 | 2857 | `	nOfft = 0; /* cc warning */` |
|     13 | 2858 | `	if( nPatLen < 1 ){` |
|      - | 2859 | `		/* php 8: the empty needle matches at position 0, so the whole haystack` |
|      - | 2860 | `		 * is returned (and nothing at all when $before_needle is set). */` |
|      7 | 2861 | `		if( nArg > 2 && ph7_value_to_bool(apArg[2]) ){` |
|      3 | 2862 | `			ph7_result_string(pCtx,"",0);` |
|      2 | 2863 | `		}else{` |
|      5 | 2864 | `			ph7_result_string(pCtx,zBlob,nLen);` |
|      - | 2865 | `		}` |
|      7 | 2866 | `		return PH7_OK;` |
|      - | 2867 | `	}` |
|      7 | 2868 | `	if( nLen > 0 ){` |
|      7 | 2869 | `		int before = 0;` |
|      - | 2870 | `		/* Perform the lookup */` |
|      7 | 2871 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      7 | 2872 | `		if( rc != SXRET_OK ){` |
|      - | 2873 | `			/* Pattern not found,return FALSE */` |
|      3 | 2874 | `			ph7_result_bool(pCtx,0);` |
|      3 | 2875 | `			return PH7_OK;` |
|      - | 2876 | `		}` |
|      - | 2877 | `		/* Return the portion of the string */` |
|      5 | 2878 | `		if( nArg > 2 ){` |
|      3 | 2879 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2880 | `		}` |
|      5 | 2881 | `		if( before ){` |
|      3 | 2882 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2883 | `		}else{` |
|      3 | 2884 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2885 | `		}` |
|      3 | 2886 | `	}else{` |
|    ! 0 | 2887 | `		ph7_result_bool(pCtx,0);` |
|      - | 2888 | `	}` |
|      5 | 2889 | `	return PH7_OK;` |
|      7 | 2890 | `}` |
|      - | 2891 | `/*` |
|      - | 2892 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2893 | ` *  Case-insensitive strstr().` |
|      - | 2894 | ` * Parameters` |
|      - | 2895 | ` *  $haystack` |
|      - | 2896 | ` *   The input string.` |
|      - | 2897 | ` * $needle` |
|      - | 2898 | ` *   Search pattern (must be a string).` |
|      - | 2899 | ` * $before_needle` |
|      - | 2900 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2901 | ` *   of the needle (excluding the needle).` |
|      - | 2902 | ` * Return` |
|      - | 2903 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2904 | ` */` |
|      6 | 2905 | `PH7_PRIVATE int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2906 | `{` |
|      7 | 2907 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2908 | `	const char *zBlob,*zPattern;` |
|      - | 2909 | `	int nLen,nPatLen;` |
|      - | 2910 | `	sxu32 nOfft;` |
|      - | 2911 | `	sxi32 rc;` |
|      7 | 2912 | `	if( nArg < 2 ){` |
|      - | 2913 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2914 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2915 | `		return PH7_OK;` |
|      - | 2916 | `	}` |
|      - | 2917 | `	/* Extract the needle and the haystack */` |
|      7 | 2918 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 2919 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 2920 | `	nOfft = 0; /* cc warning */` |
|      7 | 2921 | `	if( nPatLen < 1 ){` |
|      - | 2922 | `		/* php 8: the empty needle matches at position 0, so the whole haystack` |
|      - | 2923 | `		 * is returned (and nothing at all when $before_needle is set). */` |
|      3 | 2924 | `		if( nArg > 2 && ph7_value_to_bool(apArg[2]) ){` |
|    ! 0 | 2925 | `			ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2926 | `		}else{` |
|      3 | 2927 | `			ph7_result_string(pCtx,zBlob,nLen);` |
|      - | 2928 | `		}` |
|      3 | 2929 | `		return PH7_OK;` |
|      - | 2930 | `	}` |
|      5 | 2931 | `	if( nLen > 0 ){` |
|      5 | 2932 | `		int before = 0;` |
|      - | 2933 | `		/* Perform the lookup */` |
|      5 | 2934 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2935 | `		if( rc != SXRET_OK ){` |
|      - | 2936 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2937 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2938 | `			return PH7_OK;` |
|      - | 2939 | `		}` |
|      - | 2940 | `		/* Return the portion of the string */` |
|      5 | 2941 | `		if( nArg > 2 ){` |
|      3 | 2942 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2943 | `		}` |
|      5 | 2944 | `		if( before ){` |
|      3 | 2945 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2946 | `		}else{` |
|      3 | 2947 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2948 | `		}` |
|      3 | 2949 | `	}else{` |
|    ! 0 | 2950 | `		ph7_result_bool(pCtx,0);` |
|      - | 2951 | `	}` |
|      5 | 2952 | `	return PH7_OK;` |
|      4 | 2953 | `}` |
|      - | 2954 | `/*` |
|      - | 2955 | ` * Resolve the $offset argument shared by strpos()/stripos().` |
|      - | 2956 | ` *` |
|      - | 2957 | ` * php requires -strlen($haystack) <= $offset <= strlen($haystack) and throws` |
|      - | 2958 | ` * ValueError otherwise; a negative offset counts back from the end. PHL used to` |
|      - | 2959 | ` * negate a negative offset and silently clamp an out-of-range one to zero, so` |
|      - | 2960 | ` * strpos("Hello","l",100) answered 2 where php raises — an argument error` |
|      - | 2961 | ` * turned into a wrong answer.` |
|      - | 2962 | ` *` |
|      - | 2963 | ` * On success *pnStart receives the resolved non-negative offset.` |
|      - | 2964 | ` */` |
|     24 | 2965 | `static sxi32 StrSearchOffset(` |
|      - | 2966 | `	ph7_context *pCtx,` |
|      - | 2967 | `	ph7_value *pArg,` |
|      - | 2968 | `	int nLen,` |
|      - | 2969 | `	const char *zFunc,` |
|      - | 2970 | `	int *pnStart` |
|      - | 2971 | `	)` |
|      2 | 2972 | `{` |
|     26 | 2973 | `	ph7_int64 iOfft = ph7_value_to_int64(pArg);` |
|      - | 2974 | `	/* Compare without negating iOfft: -INT64_MIN would overflow. */` |
|     26 | 2975 | `	if( iOfft < 0 ? (iOfft < -(ph7_int64)nLen) : (iOfft > (ph7_int64)nLen) ){` |
|      8 | 2976 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      4 | 2977 | `			"%s(): Argument #3 ($offset) must be contained in argument #1 ($haystack)",zFunc);` |
|      - | 2978 | `	}` |
|     26 | 2979 | `	*pnStart = (int)(iOfft < 0 ? (ph7_int64)nLen + iOfft : iOfft);` |
|     26 | 2980 | `	return PH7_OK;` |
|     18 | 2981 | `}` |
|      - | 2982 | `/*` |
|      - | 2983 | ` * Resolve the window of match START positions for strrpos()/strripos().` |
|      - | 2984 | ` *` |
|      - | 2985 | ` * php's rule is asymmetric in the sign of $offset: a non-negative offset is a` |
|      - | 2986 | ` * LOWER bound on where the match may start, while a negative one is an UPPER` |
|      - | 2987 | ` * bound counted back from the end of the haystack (zend_memnrstr). The range` |
|      - | 2988 | ` * check is the same as StrSearchOffset()'s.` |
|      - | 2989 | ` *` |
|      - | 2990 | ` * On success the closed interval [*pnMin,*pnMax] holds every position at which` |
|      - | 2991 | ` * a match is allowed to begin; it is empty (max < min) when the needle cannot` |
|      - | 2992 | ` * fit, which the caller reports as FALSE.` |
|      - | 2993 | ` */` |
|     98 | 2994 | `static sxi32 StrRSearchWindow(` |
|      - | 2995 | `	ph7_context *pCtx,` |
|      - | 2996 | `	ph7_value *pArg, /* The $offset argument, or NULL when it was omitted */` |
|      - | 2997 | `	int nLen,` |
|      - | 2998 | `	int nPatLen,` |
|      - | 2999 | `	const char *zFunc,` |
|      - | 3000 | `	int *pnMin,` |
|      - | 3001 | `	int *pnMax` |
|      - | 3002 | `	)` |
|      1 | 3003 | `{` |
|     99 | 3004 | `	int nMin = 0;` |
|     99 | 3005 | `	int nMax = nLen - nPatLen;` |
|     99 | 3006 | `	if( pArg ){` |
|     47 | 3007 | `		ph7_int64 iOfft = ph7_value_to_int64(pArg);` |
|     47 | 3008 | `		if( iOfft < 0 ? (iOfft < -(ph7_int64)nLen) : (iOfft > (ph7_int64)nLen) ){` |
|     33 | 3009 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|     14 | 3010 | `				"%s(): Argument #3 ($offset) must be contained in argument #1 ($haystack)",zFunc);` |
|      - | 3011 | `		}` |
|     29 | 3012 | `		if( iOfft < 0 ){` |
|     15 | 3013 | `			int nLimit = nLen + (int)iOfft;` |
|     15 | 3014 | `			if( nMax > nLimit ){` |
|     15 | 3015 | `				nMax = nLimit;` |
|      7 | 3016 | `			}` |
|      8 | 3017 | `		}else{` |
|     15 | 3018 | `			nMin = (int)iOfft;` |
|      - | 3019 | `		}` |
|     14 | 3020 | `	}` |
|     81 | 3021 | `	*pnMin = nMin;` |
|     81 | 3022 | `	*pnMax = nMax;` |
|     81 | 3023 | `	return PH7_OK;` |
|     45 | 3024 | `}` |
|      - | 3025 | `/*` |
|      - | 3026 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3027 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 3028 | ` * Parameters` |
|      - | 3029 | ` *  $haystack` |
|      - | 3030 | ` *   The input string.` |
|      - | 3031 | ` * $needle` |
|      - | 3032 | ` *   Search pattern (must be a string).` |
|      - | 3033 | ` * $offset` |
|      - | 3034 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3035 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3036 | ` *   of haystack.` |
|      - | 3037 | ` * Return` |
|      - | 3038 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3039 | ` */` |
|   1584 | 3040 | `PH7_PRIVATE int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3041 | `{` |
|   1589 | 3042 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strpos",1,"$haystack"); }` |
|   1589 | 3043 | `	if( nArg > 1 ){ StrNullArgNotice(pCtx,apArg[1],"strpos",2,"$needle"); }` |
|   1589 | 3044 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3045 | `	const char *zBlob,*zPattern;` |
|      - | 3046 | `	int nLen,nPatLen,nStart;` |
|      - | 3047 | `	sxu32 nOfft;` |
|      - | 3048 | `	sxi32 rc;` |
|   1589 | 3049 | `	if( nArg < 2 ){` |
|      - | 3050 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3051 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3052 | `		return PH7_OK;` |
|      - | 3053 | `	}` |
|      - | 3054 | `	/* Extract the needle and the haystack */` |
|   1589 | 3055 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|   1589 | 3056 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|   1589 | 3057 | `	nOfft = 0; /* cc warning */` |
|   1589 | 3058 | `	nStart = 0;` |
|      - | 3059 | `	/* Peek the starting offset if available */` |
|   1589 | 3060 | `	if( nArg > 2 ){` |
|     22 | 3061 | `		rc = StrSearchOffset(pCtx,apArg[2],nLen,"strpos",&nStart);` |
|     22 | 3062 | `		if( rc != PH7_OK ){` |
|    ! 0 | 3063 | `			return rc;` |
|      - | 3064 | `		}` |
|     10 | 3065 | `	}` |
|   1589 | 3066 | `	if( nPatLen < 1 ){` |
|      - | 3067 | `		/* php 8 treats the empty needle as matching at the search offset. */` |
|     11 | 3068 | `		ph7_result_int64(pCtx,(ph7_int64)nStart);` |
|     11 | 3069 | `		return PH7_OK;` |
|      - | 3070 | `	}` |
|   1579 | 3071 | `	zBlob += nStart;` |
|   1579 | 3072 | `	nLen -= nStart;` |
|   1579 | 3073 | `	if( nLen > 0 ){` |
|      - | 3074 | `		/* Perform the lookup */` |
|   1577 | 3075 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|   1577 | 3076 | `		if( rc != SXRET_OK ){` |
|      - | 3077 | `			/* Pattern not found,return FALSE */` |
|    789 | 3078 | `			ph7_result_bool(pCtx,0);` |
|    789 | 3079 | `			return PH7_OK;` |
|      - | 3080 | `		}` |
|      - | 3081 | `		/* Return the pattern position */` |
|    793 | 3082 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|    399 | 3083 | `	}else{` |
|      3 | 3084 | `		ph7_result_bool(pCtx,0);` |
|      - | 3085 | `	}` |
|    795 | 3086 | `	return PH7_OK;` |
|    797 | 3087 | `}` |
|      - | 3088 | `/*` |
|      - | 3089 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|      - | 3090 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|      - | 3091 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|      - | 3092 | ` * TypeError for arrays, resources, and objects without __toString.` |
|      - | 3093 | ` *` |
|      - | 3094 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|      - | 3095 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|      - | 3096 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|      - | 3097 | ` *` |
|      - | 3098 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|      - | 3099 | ` * is valid until pTmp is released or pArg is mutated.` |
|      - | 3100 | ` */` |
|    646 | 3101 | `static sxi32 StrPredicateResolveArg(` |
|      - | 3102 | `	ph7_context *pCtx,` |
|      - | 3103 | `	ph7_value *pArg,` |
|      - | 3104 | `	const char *zFunc,` |
|      - | 3105 | `	int iArgNum,` |
|      - | 3106 | `	const char *zParamName,` |
|      - | 3107 | `	const char *zTypeStr, /* Declared type in the TypeError, e.g. "string" / "?string" */` |
|      - | 3108 | `	const char *zNullMsg,` |
|      - | 3109 | `	ph7_value *pTmp,` |
|      - | 3110 | `	const char **pzOut,` |
|      - | 3111 | `	int *pnOut` |
|      3 | 3112 | `){` |
|    323 | 3113 | `	SXUNUSED(zNullMsg); /* php's deprecation text — PHL rejects null instead of coercing */` |
|    649 | 3114 | `	if( ph7_value_is_null(pArg) ){` |
|      - | 3115 | `		/* php only DEPRECATES null here; PHL rejects it with the TypeError php will` |
|      - | 3116 | `		 * eventually raise. */` |
|      4 | 3117 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 3118 | `			"%s(): Argument #%d (%s) must be of type %s, null given",` |
|      1 | 3119 | `			zFunc,iArgNum,zParamName,zTypeStr);` |
|      - | 3120 | `	}` |
|    992 | 3121 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    644 | 3122 | `	    ( ph7_value_is_object(pArg) &&` |
|     72 | 3123 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     48 | 3124 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     24 | 3125 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 3126 | `	    )` |
|      - | 3127 | `	){` |
|    ! 0 | 3128 | `		const char *zType = ph7_type_name(pArg);` |
|    ! 0 | 3129 | `		if( ph7_value_is_object(pArg) ){` |
|    ! 0 | 3130 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|    ! 0 | 3131 | `			if( pInst && pInst->pClass ){` |
|    ! 0 | 3132 | `				zType = SyStringData(&pInst->pClass->sName);` |
|    ! 0 | 3133 | `			}` |
|    ! 0 | 3134 | `		}` |
|    ! 0 | 3135 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3136 | `			"TypeError",` |
|      - | 3137 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|    ! 0 | 3138 | `			zFunc, iArgNum, zParamName, zTypeStr, zType` |
|      - | 3139 | `			);` |
|      - | 3140 | `	}` |
|    646 | 3141 | `	if( ph7_value_is_object(pArg) ){` |
|     49 | 3142 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     49 | 3143 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 3144 | `			"__toString",sizeof("__toString")-1);` |
|     49 | 3145 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|     49 | 3146 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|     49 | 3147 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|     49 | 3148 | `		return PH7_OK;` |
|      - | 3149 | `	}` |
|    598 | 3150 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|    598 | 3151 | `	return PH7_OK;` |
|    326 | 3152 | `}` |
|      - | 3153 | `/*` |
|      - | 3154 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 3155 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 3156 | ` * Return` |
|      - | 3157 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 3158 | ` */` |
|     86 | 3159 | `PH7_PRIVATE int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3160 | `{` |
|      - | 3161 | `	const char *zHaystack,*zNeedle;` |
|      - | 3162 | `	int nHayLen,nNeedleLen;` |
|      - | 3163 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3164 | `	sxi32 rc;` |
|     89 | 3165 | `	if( nArg != 2 ){` |
|    ! 0 | 3166 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3167 | `			"ArgumentCountError",` |
|      - | 3168 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|    ! 0 | 3169 | `			nArg` |
|      - | 3170 | `			);` |
|      - | 3171 | `	}` |
|     89 | 3172 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     89 | 3173 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     89 | 3174 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack","string",` |
|      - | 3175 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|      - | 3176 | `		"of type string is deprecated",` |
|      - | 3177 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     89 | 3178 | `	if( rc != PH7_OK ) goto out;` |
|     86 | 3179 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle","string",` |
|      - | 3180 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|      - | 3181 | `		"of type string is deprecated",` |
|      - | 3182 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     86 | 3183 | `	if( rc != PH7_OK ) goto out;` |
|     86 | 3184 | `	if( nNeedleLen < 1 ){` |
|     11 | 3185 | `		ph7_result_bool(pCtx,1);` |
|     81 | 3186 | `	}else if( nHayLen < nNeedleLen ){` |
|      7 | 3187 | `		ph7_result_bool(pCtx,0);` |
|      4 | 3188 | `	}else{` |
|    104 | 3189 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|     34 | 3190 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|     70 | 3191 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|      - | 3192 | `	}` |
|     86 | 3193 | `	rc = PH7_OK;` |
|     43 | 3194 | `out:` |
|     89 | 3195 | `	PH7_MemObjRelease(&sHayTmp);` |
|     89 | 3196 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     89 | 3197 | `	return rc;` |
|     46 | 3198 | `}` |
|      - | 3199 | `/*` |
|      - | 3200 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 3201 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 3202 | ` * Return` |
|      - | 3203 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 3204 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3205 | ` */` |
|     54 | 3206 | `PH7_PRIVATE int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3207 | `{` |
|      - | 3208 | `	const char *zHaystack,*zNeedle;` |
|      - | 3209 | `	int nHayLen,nNeedleLen;` |
|      - | 3210 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3211 | `	sxi32 rc;` |
|     55 | 3212 | `	if( nArg != 2 ){` |
|    ! 0 | 3213 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3214 | `			"ArgumentCountError",` |
|      - | 3215 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|    ! 0 | 3216 | `			nArg` |
|      - | 3217 | `			);` |
|      - | 3218 | `	}` |
|     55 | 3219 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     55 | 3220 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     55 | 3221 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack","string",` |
|      - | 3222 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3223 | `		"of type string is deprecated",` |
|      - | 3224 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     55 | 3225 | `	if( rc != PH7_OK ) goto out;` |
|     55 | 3226 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle","string",` |
|      - | 3227 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3228 | `		"of type string is deprecated",` |
|      - | 3229 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     55 | 3230 | `	if( rc != PH7_OK ) goto out;` |
|     55 | 3231 | `	if( nNeedleLen < 1 ){` |
|     11 | 3232 | `		ph7_result_bool(pCtx,1);` |
|     50 | 3233 | `	}else if( nHayLen < nNeedleLen ){` |
|      7 | 3234 | `		ph7_result_bool(pCtx,0);` |
|      4 | 3235 | `	}else{` |
|     58 | 3236 | `		ph7_result_bool(pCtx,` |
|     38 | 3237 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3238 | `	}` |
|     55 | 3239 | `	rc = PH7_OK;` |
|     27 | 3240 | `out:` |
|     55 | 3241 | `	PH7_MemObjRelease(&sHayTmp);` |
|     55 | 3242 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     55 | 3243 | `	return rc;` |
|     28 | 3244 | `}` |
|      - | 3245 | `/*` |
|      - | 3246 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 3247 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 3248 | ` * Return` |
|      - | 3249 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 3250 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3251 | ` */` |
|     54 | 3252 | `PH7_PRIVATE int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3253 | `{` |
|      - | 3254 | `	const char *zHaystack,*zNeedle;` |
|      - | 3255 | `	int nHayLen,nNeedleLen;` |
|      - | 3256 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3257 | `	sxi32 rc;` |
|     55 | 3258 | `	if( nArg != 2 ){` |
|    ! 0 | 3259 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3260 | `			"ArgumentCountError",` |
|      - | 3261 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|    ! 0 | 3262 | `			nArg` |
|      - | 3263 | `			);` |
|      - | 3264 | `	}` |
|     55 | 3265 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     55 | 3266 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     55 | 3267 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack","string",` |
|      - | 3268 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3269 | `		"of type string is deprecated",` |
|      - | 3270 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     55 | 3271 | `	if( rc != PH7_OK ) goto out;` |
|     55 | 3272 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle","string",` |
|      - | 3273 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3274 | `		"of type string is deprecated",` |
|      - | 3275 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     55 | 3276 | `	if( rc != PH7_OK ) goto out;` |
|     55 | 3277 | `	if( nNeedleLen < 1 ){` |
|     11 | 3278 | `		ph7_result_bool(pCtx,1);` |
|     50 | 3279 | `	}else if( nHayLen < nNeedleLen ){` |
|      7 | 3280 | `		ph7_result_bool(pCtx,0);` |
|      4 | 3281 | `	}else{` |
|     58 | 3282 | `		ph7_result_bool(pCtx,` |
|     38 | 3283 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3284 | `	}` |
|     55 | 3285 | `	rc = PH7_OK;` |
|     27 | 3286 | `out:` |
|     55 | 3287 | `	PH7_MemObjRelease(&sHayTmp);` |
|     55 | 3288 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     55 | 3289 | `	return rc;` |
|     28 | 3290 | `}` |
|      - | 3291 | `/*` |
|      - | 3292 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3293 | ` *  Case-insensitive strpos.` |
|      - | 3294 | ` * Parameters` |
|      - | 3295 | ` *  $haystack` |
|      - | 3296 | ` *   The input string.` |
|      - | 3297 | ` * $needle` |
|      - | 3298 | ` *   Search pattern (must be a string).` |
|      - | 3299 | ` * $offset` |
|      - | 3300 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3301 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3302 | ` *   of haystack.` |
|      - | 3303 | ` * Return` |
|      - | 3304 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3305 | ` */` |
|    198 | 3306 | `PH7_PRIVATE int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3307 | `{` |
|    200 | 3308 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3309 | `	const char *zBlob,*zPattern;` |
|      - | 3310 | `	int nLen,nPatLen,nStart;` |
|      - | 3311 | `	sxu32 nOfft;` |
|      - | 3312 | `	sxi32 rc;` |
|    200 | 3313 | `	if( nArg < 2 ){` |
|      - | 3314 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3315 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3316 | `		return PH7_OK;` |
|      - | 3317 | `	}` |
|      - | 3318 | `	/* Extract the needle and the haystack */` |
|    200 | 3319 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    200 | 3320 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    200 | 3321 | `	nOfft = 0; /* cc warning */` |
|    200 | 3322 | `	nStart = 0;` |
|      - | 3323 | `	/* Peek the starting offset if available */` |
|    200 | 3324 | `	if( nArg > 2 ){` |
|      5 | 3325 | `		rc = StrSearchOffset(pCtx,apArg[2],nLen,"stripos",&nStart);` |
|      5 | 3326 | `		if( rc != PH7_OK ){` |
|    ! 0 | 3327 | `			return rc;` |
|      - | 3328 | `		}` |
|      2 | 3329 | `	}` |
|    200 | 3330 | `	if( nPatLen < 1 ){` |
|      - | 3331 | `		/* php 8 treats the empty needle as matching at the search offset. */` |
|      3 | 3332 | `		ph7_result_int64(pCtx,(ph7_int64)nStart);` |
|      3 | 3333 | `		return PH7_OK;` |
|      - | 3334 | `	}` |
|    198 | 3335 | `	zBlob += nStart;` |
|    198 | 3336 | `	nLen -= nStart;` |
|    198 | 3337 | `	if( nLen > 0 ){` |
|      - | 3338 | `		/* Perform the lookup */` |
|    198 | 3339 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    198 | 3340 | `		if( rc != SXRET_OK ){` |
|      - | 3341 | `			/* Pattern not found,return FALSE */` |
|    184 | 3342 | `			ph7_result_bool(pCtx,0);` |
|    184 | 3343 | `			return PH7_OK;` |
|      - | 3344 | `		}` |
|      - | 3345 | `		/* Return the pattern position */` |
|     15 | 3346 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 3347 | `	}else{` |
|    ! 0 | 3348 | `		ph7_result_bool(pCtx,0);` |
|      - | 3349 | `	}` |
|     15 | 3350 | `	return PH7_OK;` |
|    101 | 3351 | `}` |
|      - | 3352 | `/*` |
|      - | 3353 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3354 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 3355 | ` * Parameters` |
|      - | 3356 | ` *  $haystack` |
|      - | 3357 | ` *   The input string.` |
|      - | 3358 | ` * $needle` |
|      - | 3359 | ` *   Search pattern (must be a string).` |
|      - | 3360 | ` * $offset` |
|      - | 3361 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3362 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3363 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3364 | ` * Return` |
|      - | 3365 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3366 | ` */` |
|     54 | 3367 | `PH7_PRIVATE int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3368 | `{` |
|      - | 3369 | `	const char *zBlob,*zPattern;` |
|     55 | 3370 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3371 | `	int nLen,nPatLen,i;` |
|     55 | 3372 | `	int nMin = 0,nMax = 0;` |
|      - | 3373 | `	sxu32 nOfft;` |
|      - | 3374 | `	sxi32 rc;` |
|     55 | 3375 | `	if( nArg < 2 ){` |
|      - | 3376 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3377 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3378 | `		return PH7_OK;` |
|      - | 3379 | `	}` |
|      - | 3380 | `	/* Extract the needle and the haystack */` |
|     55 | 3381 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     55 | 3382 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     55 | 3383 | `	nOfft = 0; /* cc warning */` |
|      - | 3384 | `	/* Resolve the range of positions the match may start at */` |
|     55 | 3385 | `	rc = StrRSearchWindow(pCtx,nArg > 2 ? apArg[2] : 0,nLen,nPatLen,"strrpos",&nMin,&nMax);` |
|     55 | 3386 | `	if( rc != PH7_OK ){` |
|      5 | 3387 | `		return rc;` |
|      - | 3388 | `	}` |
|     51 | 3389 | `	if( nPatLen < 1 ){` |
|      - | 3390 | `		/* php 8: the empty needle matches everywhere, so the LAST match is the` |
|      - | 3391 | `		 * highest position the window allows. */` |
|     11 | 3392 | `		ph7_result_int64(pCtx,(ph7_int64)nMax);` |
|     11 | 3393 | `		return PH7_OK;` |
|      - | 3394 | `	}` |
|      - | 3395 | `	/* Walk backwards, comparing at each candidate position. Searching a window` |
|      - | 3396 | `	 * exactly as long as the needle makes the match test an equality test while` |
|      - | 3397 | `	 * still going through xPatternMatch, which carries the case folding. */` |
|    217 | 3398 | `	for( i = nMax ; i >= nMin ; --i ){` |
|    203 | 3399 | `		rc = xPatternMatch((const void *)&zBlob[i],(sxu32)nPatLen,(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    203 | 3400 | `		if( rc == SXRET_OK ){` |
|      - | 3401 | `			/* Pattern found,return it's position */` |
|     27 | 3402 | `			ph7_result_int64(pCtx,(ph7_int64)i);` |
|     27 | 3403 | `			return PH7_OK;` |
|      - | 3404 | `		}` |
|     89 | 3405 | `	}` |
|      - | 3406 | `	/* Pattern not found,return FALSE */` |
|     15 | 3407 | `	ph7_result_bool(pCtx,0);` |
|     15 | 3408 | `	return PH7_OK;` |
|     28 | 3409 | `}` |
|      - | 3410 | `/*` |
|      - | 3411 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3412 | ` *  Case-insensitive strrpos.` |
|      - | 3413 | ` * Parameters` |
|      - | 3414 | ` *  $haystack` |
|      - | 3415 | ` *   The input string.` |
|      - | 3416 | ` * $needle` |
|      - | 3417 | ` *   Search pattern (must be a string).` |
|      - | 3418 | ` * $offset` |
|      - | 3419 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3420 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3421 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3422 | ` * Return` |
|      - | 3423 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3424 | ` */` |
|     34 | 3425 | `PH7_PRIVATE int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3426 | `{` |
|      - | 3427 | `	const char *zBlob,*zPattern;` |
|     35 | 3428 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3429 | `	int nLen,nPatLen,i;` |
|     35 | 3430 | `	int nMin = 0,nMax = 0;` |
|      - | 3431 | `	sxu32 nOfft;` |
|      - | 3432 | `	sxi32 rc;` |
|     35 | 3433 | `	if( nArg < 2 ){` |
|      - | 3434 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3435 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3436 | `		return PH7_OK;` |
|      - | 3437 | `	}` |
|      - | 3438 | `	/* Extract the needle and the haystack */` |
|     35 | 3439 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     35 | 3440 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     35 | 3441 | `	nOfft = 0; /* cc warning */` |
|      - | 3442 | `	/* Resolve the range of positions the match may start at */` |
|     35 | 3443 | `	rc = StrRSearchWindow(pCtx,nArg > 2 ? apArg[2] : 0,nLen,nPatLen,"strripos",&nMin,&nMax);` |
|     35 | 3444 | `	if( rc != PH7_OK ){` |
|      5 | 3445 | `		return rc;` |
|      - | 3446 | `	}` |
|     31 | 3447 | `	if( nPatLen < 1 ){` |
|      - | 3448 | `		/* php 8: the empty needle matches everywhere, so the LAST match is the` |
|      - | 3449 | `		 * highest position the window allows. */` |
|     11 | 3450 | `		ph7_result_int64(pCtx,(ph7_int64)nMax);` |
|     11 | 3451 | `		return PH7_OK;` |
|      - | 3452 | `	}` |
|      - | 3453 | `	/* Walk backwards, comparing at each candidate position (see strrpos). */` |
|     49 | 3454 | `	for( i = nMax ; i >= nMin ; --i ){` |
|     45 | 3455 | `		rc = xPatternMatch((const void *)&zBlob[i],(sxu32)nPatLen,(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     45 | 3456 | `		if( rc == SXRET_OK ){` |
|      - | 3457 | `			/* Pattern found,return it's position */` |
|     17 | 3458 | `			ph7_result_int64(pCtx,(ph7_int64)i);` |
|     17 | 3459 | `			return PH7_OK;` |
|      - | 3460 | `		}` |
|     15 | 3461 | `	}` |
|      - | 3462 | `	/* Pattern not found,return FALSE */` |
|      5 | 3463 | `	ph7_result_bool(pCtx,0);` |
|      5 | 3464 | `	return PH7_OK;` |
|     18 | 3465 | `}` |
|      - | 3466 | `/*` |
|      - | 3467 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3468 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3469 | ` * Parameters` |
|      - | 3470 | ` *  $haystack` |
|      - | 3471 | ` *   The input string.` |
|      - | 3472 | ` * $needle` |
|      - | 3473 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3474 | ` *  This behavior is different from that of strstr().` |
|      - | 3475 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3476 | ` *  as the ordinal value of a character.` |
|      - | 3477 | ` * Return` |
|      - | 3478 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3479 | ` */` |
|     24 | 3480 | `PH7_PRIVATE int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3481 | `{` |
|      - | 3482 | `	const char *zBlob;` |
|      - | 3483 | `	int nLen,c;` |
|     25 | 3484 | `	if( nArg < 2 ){` |
|      - | 3485 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3486 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3487 | `		return PH7_OK;` |
|      - | 3488 | `	}` |
|      - | 3489 | `	/* Extract the haystack */` |
|     25 | 3490 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     25 | 3491 | `	c = 0; /* cc warning */` |
|     25 | 3492 | `	if( nLen > 0 ){` |
|      - | 3493 | `		const char *zPattern;` |
|      - | 3494 | `		int nPatLen;` |
|      - | 3495 | `		sxu32 nOfft;` |
|      - | 3496 | `		sxi32 rc;` |
|      - | 3497 | `		/* php 8 casts the needle to string and uses only its first character.` |
|      - | 3498 | `		 * The old "if not a string, take it as an ordinal" reading was php 7` |
|      - | 3499 | `		 * behaviour, removed in php 8: strrchr("hello world",111) now looks for` |
|      - | 3500 | `		 * "1", not "o". An empty needle matches nothing. */` |
|     23 | 3501 | `		zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     23 | 3502 | `		if( nPatLen < 1 ){` |
|      3 | 3503 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3504 | `			return PH7_OK;` |
|      - | 3505 | `		}` |
|     21 | 3506 | `		c = zPattern[0];` |
|      - | 3507 | `		/* Perform the lookup */` |
|     21 | 3508 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3509 | `		if( rc != SXRET_OK ){` |
|      - | 3510 | `			/* No such entry,return FALSE */` |
|      9 | 3511 | `			ph7_result_bool(pCtx,0);` |
|      9 | 3512 | `			return PH7_OK;` |
|      - | 3513 | `		}` |
|      - | 3514 | `		/* Return the string portion */` |
|     13 | 3515 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      7 | 3516 | `	}else{` |
|      3 | 3517 | `		ph7_result_bool(pCtx,0);` |
|      - | 3518 | `	}` |
|     15 | 3519 | `	return PH7_OK;` |
|     13 | 3520 | `}` |
|      - | 3521 | `/*` |
|      - | 3522 | ` * string strrev(string $string)` |
|      - | 3523 | ` *  Reverse a string.` |
|      - | 3524 | ` * Parameters` |
|      - | 3525 | ` *  $string` |
|      - | 3526 | ` *   String to be reversed.` |
|      - | 3527 | ` * Return` |
|      - | 3528 | ` *  The reversed string.` |
|      - | 3529 | ` */` |
|      2 | 3530 | `PH7_PRIVATE int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3531 | `{` |
|      - | 3532 | `	const char *zIn,*zEnd;` |
|      - | 3533 | `	int nLen,c;` |
|      3 | 3534 | `	if( nArg < 1 ){` |
|      - | 3535 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3536 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3537 | `		return PH7_OK;` |
|      - | 3538 | `	}` |
|      - | 3539 | `	/* Extract the target string */` |
|      3 | 3540 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3541 | `	if( nLen < 1 ){` |
|      - | 3542 | `		/* Empty string Return null */` |
|    ! 0 | 3543 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3544 | `		return PH7_OK;` |
|      - | 3545 | `	}` |
|      - | 3546 | `	/* Perform the requested operation */` |
|      3 | 3547 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3548 | `	for(;;){` |
|      9 | 3549 | `		if( zEnd < zIn ){` |
|      - | 3550 | `			/* No more input to process */` |
|      3 | 3551 | `			break;` |
|      - | 3552 | `		}` |
|      - | 3553 | `		/* Append current character */` |
|      7 | 3554 | `		c = zEnd[0];` |
|      7 | 3555 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3556 | `		zEnd--;` |
|      1 | 3557 | `	}` |
|      3 | 3558 | `	return PH7_OK;` |
|      2 | 3559 | `}` |
|      - | 3560 | `/*` |
|      - | 3561 | ` * string ucwords(string $string [, string $separators = " \t\r\n\f\v"])` |
|      - | 3562 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3563 | ` *  A word begins at the start of the string and after any character present in` |
|      - | 3564 | ` *  $separators. The default separators are the whitespace characters (space,` |
|      - | 3565 | ` *  horizontal tab, carriage return, newline, form-feed and vertical tab); an` |
|      - | 3566 | ` *  explicit $separators argument REPLACES them (an empty string leaves only the` |
|      - | 3567 | ` *  very first character upper-cased). Like PHP, this is byte-based: only ASCII` |
|      - | 3568 | ` *  bytes are upper-cased and a byte is a separator only if it appears in the set.` |
|      - | 3569 | ` * Parameters` |
|      - | 3570 | ` *  $string` |
|      - | 3571 | ` *   The input string.` |
|      - | 3572 | ` *  $separators` |
|      - | 3573 | ` *   The optional word-boundary characters.` |
|      - | 3574 | ` * Return` |
|      - | 3575 | ` *  The modified string.` |
|      - | 3576 | ` */` |
|     22 | 3577 | `PH7_PRIVATE int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3578 | `{` |
|      - | 3579 | `	const char *zIn;` |
|      - | 3580 | `	int nLen,i,iStart;` |
|      - | 3581 | `	char aDelim[256];` |
|     23 | 3582 | `	if( nArg < 1 ){` |
|      - | 3583 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3584 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3585 | `		return PH7_OK;` |
|      - | 3586 | `	}` |
|      - | 3587 | `	/* Build the separator membership table: an explicit $separators argument` |
|      - | 3588 | `	 * replaces the default whitespace set (an empty string clears it). */` |
|     23 | 3589 | `	SyZero(aDelim,(sxu32)sizeof(aDelim));` |
|     23 | 3590 | `	if( nArg > 1 ){` |
|      - | 3591 | `		int nDelim;` |
|      9 | 3592 | `		const char *zDelim = ph7_value_to_string(apArg[1],&nDelim);` |
|     17 | 3593 | `		for( i = 0 ; i < nDelim ; i++ ){` |
|      9 | 3594 | `			aDelim[(unsigned char)zDelim[i]] = 1;` |
|      5 | 3595 | `		}` |
|      5 | 3596 | `	}else{` |
|     15 | 3597 | `		aDelim[(unsigned char)' ']  = 1;` |
|     15 | 3598 | `		aDelim[(unsigned char)'\t'] = 1;` |
|     15 | 3599 | `		aDelim[(unsigned char)'\r'] = 1;` |
|     15 | 3600 | `		aDelim[(unsigned char)'\n'] = 1;` |
|     15 | 3601 | `		aDelim[(unsigned char)'\f'] = 1;` |
|     15 | 3602 | `		aDelim[(unsigned char)'\v'] = 1;` |
|      - | 3603 | `	}` |
|      - | 3604 | `	/* Extract the target string */` |
|     23 | 3605 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3606 | `	if( nLen < 1 ){` |
|      - | 3607 | `		/* Empty string – match PHP semantics */` |
|      3 | 3608 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3609 | `		return PH7_OK;` |
|      - | 3610 | `	}` |
|      - | 3611 | `	/* Upper-case the first byte of each word (the leading byte, or any byte that` |
|      - | 3612 | `	 * follows a separator), appending the untouched runs in between verbatim. */` |
|     21 | 3613 | `	iStart = 0;` |
|    309 | 3614 | `	for( i = 0 ; i < nLen ; i++ ){` |
|    289 | 3615 | `		int c = (unsigned char)zIn[i];` |
|    289 | 3616 | `		if( (i == 0 \|\| aDelim[(unsigned char)zIn[i-1]]) && c < 0x80 && SyisLower(c) ){` |
|     53 | 3617 | `			char up = (char)SyToUpper(c);` |
|     53 | 3618 | `			if( i > iStart ){` |
|     35 | 3619 | `				ph7_result_string(pCtx,&zIn[iStart],i - iStart);` |
|     17 | 3620 | `			}` |
|     53 | 3621 | `			ph7_result_string(pCtx,&up,1);` |
|     53 | 3622 | `			iStart = i + 1;` |
|     26 | 3623 | `		}` |
|    145 | 3624 | `	}` |
|     21 | 3625 | `	if( nLen > iStart ){` |
|     21 | 3626 | `		ph7_result_string(pCtx,&zIn[iStart],nLen - iStart);` |
|     10 | 3627 | `	}` |
|     21 | 3628 | `	return PH7_OK;` |
|     12 | 3629 | `}` |
|      - | 3630 | `/*` |
|      - | 3631 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3632 | ` *  Returns input repeated multiplier times.` |
|      - | 3633 | ` * Parameters` |
|      - | 3634 | ` *  $string` |
|      - | 3635 | ` *   String to be repeated.` |
|      - | 3636 | ` * $multiplier` |
|      - | 3637 | ` *  Number of time the input string should be repeated.` |
|      - | 3638 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3639 | ` *  to 0, the function will return an empty string.` |
|      - | 3640 | ` * Return` |
|      - | 3641 | ` *  The repeated string.` |
|      - | 3642 | ` */` |
|  20444 | 3643 | `PH7_PRIVATE int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3644 | `{` |
|      - | 3645 | `	const char *zIn;` |
|      - | 3646 | `	int nLen;` |
|      - | 3647 | `	ph7_int64 nMul;` |
|      - | 3648 | `	int rc;` |
|  20447 | 3649 | `	if( nArg < 2 ){` |
|      - | 3650 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3651 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3652 | `		return PH7_OK;` |
|      - | 3653 | `	}` |
|      - | 3654 | `	/* Extract the target string */` |
|  20447 | 3655 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3656 | `	/* Resolve $times through the shared ZPP helper so a lossy float / float-string` |
|      - | 3657 | `	 * carries php's precision deprecation and NAN/INF/non-numeric fail with php's` |
|      - | 3658 | `	 * TypeError — a bare ph7_value_to_int64() coerced them silently. */` |
|      - | 3659 | `	{` |
|  20447 | 3660 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_repeat",2,"$times","int",&nMul);` |
|  20447 | 3661 | `		if( rcArg != PH7_OK ){` |
|      5 | 3662 | `			return rcArg;` |
|      - | 3663 | `		}` |
|      - | 3664 | `	}` |
|  20443 | 3665 | `	if( nMul < 0 ){` |
|      3 | 3666 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 3667 | `			"str_repeat(): Argument #2 ($times) must be greater than or equal to 0");` |
|      - | 3668 | `	}` |
|  20441 | 3669 | `	if( nLen < 1 \|\| nMul < 1 ){` |
|      - | 3670 | `		/* Empty input or a zero multiplier yields the empty string (PHP). */` |
|    ! 0 | 3671 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3672 | `		return PH7_OK;` |
|      - | 3673 | `	}` |
|      - | 3674 | `	/* Perform the requested operation */` |
| 223892 | 3675 | `	for(;;){` |
| 447787 | 3676 | `		if( !nMul ){` |
|  20441 | 3677 | `			break;` |
|      - | 3678 | `		}` |
|      - | 3679 | `		/* Append the copy */` |
| 427349 | 3680 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 427349 | 3681 | `		if( rc != PH7_OK ){` |
|      - | 3682 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 3683 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 3684 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 3685 | `		}` |
| 427349 | 3686 | `		nMul--;` |
|      3 | 3687 | `	}` |
|  20441 | 3688 | `	return PH7_OK;` |
|  10225 | 3689 | `}` |
|      - | 3690 | `/*` |
|      - | 3691 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3692 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3693 | ` * Parameters` |
|      - | 3694 | ` *  $string` |
|      - | 3695 | ` *   The input string.` |
|      - | 3696 | ` * $is_xhtml` |
|      - | 3697 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3698 | ` * Return` |
|      - | 3699 | ` *  The processed string.` |
|      - | 3700 | ` */` |
|      4 | 3701 | `PH7_PRIVATE int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3702 | `{` |
|      - | 3703 | `	const char *zIn,*zCur,*zEnd;` |
|      5 | 3704 | `	int is_xhtml = 1; /* Default to XHTML-style '<br />' like PHP */` |
|      - | 3705 | `	int nLen;` |
|      5 | 3706 | `	if( nArg < 1 ){` |
|      - | 3707 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 3708 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3709 | `		return PH7_OK;` |
|      - | 3710 | `	}` |
|      - | 3711 | `	/* Extract the target string */` |
|      5 | 3712 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3713 | `	if( nLen < 1 ){` |
|      - | 3714 | `		/* Empty string,return null */` |
|    ! 0 | 3715 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3716 | `		return PH7_OK;` |
|      - | 3717 | `	}` |
|      5 | 3718 | `	if( nArg > 1 ){` |
|      3 | 3719 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3720 | `	}` |
|      5 | 3721 | `	zEnd = &zIn[nLen];` |
|      - | 3722 | `	/* Perform the requested operation */` |
|      4 | 3723 | `	for(;;){` |
|      9 | 3724 | `		zCur = zIn;` |
|      - | 3725 | `		/* Delimit the string */` |
|     21 | 3726 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3727 | `			zIn++;` |
|      1 | 3728 | `		}` |
|      9 | 3729 | `		if( zCur < zIn ){` |
|      - | 3730 | `			/* Output chunk verbatim */` |
|      9 | 3731 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3732 | `		}` |
|      9 | 3733 | `		if( zIn >= zEnd ){` |
|      - | 3734 | `			/* No more input to process */` |
|      5 | 3735 | `			break;` |
|      - | 3736 | `		}` |
|      - | 3737 | `		/* Output the HTML line break */` |
|      - | 3738 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br />' (space before the slash), otherwise use '<br>' */` |
|      5 | 3739 | `		if( is_xhtml ){` |
|      3 | 3740 | `			ph7_result_string(pCtx,"<br />",(int)sizeof("<br />")-1);` |
|      2 | 3741 | `		}else{` |
|      3 | 3742 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3743 | `		}` |
|      5 | 3744 | `		zCur = zIn;` |
|      - | 3745 | `		/* Append trailing line */` |
|     11 | 3746 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3747 | `			zIn++;` |
|      1 | 3748 | `		}` |
|      5 | 3749 | `		if( zCur < zIn ){` |
|      - | 3750 | `			/* Output chunk verbatim */` |
|      5 | 3751 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3752 | `		}` |
|      1 | 3753 | `	}` |
|      5 | 3754 | `	return PH7_OK;` |
|      3 | 3755 | `}` |
|      - | 3756 | `/*` |
|      - | 3757 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3758 | ` *  According to the PHP reference manual.` |
|      - | 3759 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3760 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3761 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3762 | ` * This applies to both sprintf() and printf().` |
|      - | 3763 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3764 | ` * or more of these elements, in order:` |
|      - | 3765 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3766 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3767 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 3768 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 3769 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 3770 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 3771 | ` *   it with a single quote ('). See the examples below.` |
|      - | 3772 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 3773 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 3774 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 3775 | ` *   should result in.` |
|      - | 3776 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 3777 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 3778 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 3779 | ` *   limit to the string.` |
|      - | 3780 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 3781 | ` *       % - a literal percent character. No argument is required.` |
|      - | 3782 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 3783 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 3784 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 3785 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 3786 | ` * 	     for the number of digits after the decimal point.` |
|      - | 3787 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 3788 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 3789 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 3790 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 3791 | ` *       g - shorter of %e and %f.` |
|      - | 3792 | ` *       G - shorter of %E and %f.` |
|      - | 3793 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 3794 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 3795 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 3796 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 3797 | ` */` |
|      - | 3798 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3799 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 3800 | `/*` |
|      - | 3801 | ` * Symisc eXtension.` |
|      - | 3802 | ` * string size_format(int64 $size)` |
|      - | 3803 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 3804 | ` *  Example:` |
|      - | 3805 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 3806 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 3807 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 3808 | ` * Parameter` |
|      - | 3809 | ` *  $size` |
|      - | 3810 | ` *    Entity size in bytes.` |
|      - | 3811 | ` * Return` |
|      - | 3812 | ` *   Formatted string representation of the given size.` |
|      - | 3813 | ` */` |
|     24 | 3814 | `PH7_PRIVATE int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3815 | `{` |
|      - | 3816 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 3817 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 3818 | `	sxi32 nRest,i_32;` |
|      - | 3819 | `	ph7_int64 iSize;` |
|     25 | 3820 | `	int c = -1; /* index in zUnit[] */` |
|      - | 3821 |  |
|     25 | 3822 | `	if( nArg < 1 ){` |
|      - | 3823 | `		/* Missing argument,return the empty string */` |
|      3 | 3824 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3825 | `		return PH7_OK;` |
|      - | 3826 | `	}` |
|      - | 3827 | `	/* Extract the given size */` |
|     23 | 3828 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 3829 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 3830 | `		/* Don't bother formatting,return immediately */` |
|      5 | 3831 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 3832 | `		return PH7_OK;` |
|      - | 3833 | `	}` |
|     19 | 3834 | `	for(;;){` |
|     39 | 3835 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 3836 | `		iSize >>= 10;` |
|     39 | 3837 | `		c++;` |
|     39 | 3838 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 3839 | `			break;` |
|      - | 3840 | `		}` |
|      1 | 3841 | `	}` |
|     19 | 3842 | `	nRest /= 100;` |
|     19 | 3843 | `	if( nRest > 9 ){` |
|    ! 0 | 3844 | `		nRest = 9;` |
|    ! 0 | 3845 | `	}` |
|     19 | 3846 | `	if( iSize > 999 ){` |
|    ! 0 | 3847 | `		c++;` |
|    ! 0 | 3848 | `		nRest = 9;` |
|    ! 0 | 3849 | `		iSize = 0;` |
|    ! 0 | 3850 | `	}` |
|     19 | 3851 | `	i_32 = (sxi32)iSize;` |
|      - | 3852 | `	/* Format */` |
|     19 | 3853 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 3854 | `	return PH7_OK;` |
|     13 | 3855 | `}` |
|      - | 3856 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3857 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 3858 | `/*` |
|      - | 3859 | ` * string str_shuffle(string $str)` |
|      - | 3860 |  |
|      - | 3861 | ` *  Randomly shuffles a string.` |
|      - | 3862 | ` * Parameters` |
|      - | 3863 | ` *  $str` |
|      - | 3864 | ` *   The input string.` |
|      - | 3865 | ` * Return` |
|      - | 3866 | ` *  Returns the shuffled string.` |
|      - | 3867 | ` */` |
|     10 | 3868 | `PH7_PRIVATE int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3869 | `{` |
|      - | 3870 | `	const char *zString;` |
|      - | 3871 | `	int nLen,i,c;` |
|      - | 3872 | `	sxu32 iR;` |
|     11 | 3873 | `	if( nArg < 1 ){` |
|      - | 3874 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 3875 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3876 | `		return PH7_OK;` |
|      - | 3877 | `	}` |
|      - | 3878 | `	/* Extract the target string */` |
|     11 | 3879 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 3880 | `	if( nLen < 1 ){` |
|      - | 3881 | `		/* Nothing to shuffle */` |
|      3 | 3882 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3883 | `		return PH7_OK;` |
|      - | 3884 | `	}` |
|      - | 3885 | `	/* Shuffle the string */` |
|     43 | 3886 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 3887 | `		/* Generate a random number first */` |
|     35 | 3888 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 3889 | `		/* Extract a random offset */` |
|     35 | 3890 | `		c = zString[iR % nLen];` |
|      - | 3891 | `		/* Append it */` |
|     35 | 3892 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 3893 | `	}` |
|      9 | 3894 | `	return PH7_OK;` |
|      6 | 3895 | `}` |
|      - | 3896 | `/*` |
|      - | 3897 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 3898 | ` *  Convert a string to an array.` |
|      - | 3899 | ` * Parameters` |
|      - | 3900 | ` * $string` |
|      - | 3901 | ` *  The input string.` |
|      - | 3902 | ` * $split_length` |
|      - | 3903 | ` *  Maximum length of the chunk.` |
|      - | 3904 | ` * Return` |
|      - | 3905 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 3906 | ` *  except possibly the last one which may be shorter.` |
|      - | 3907 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 3908 | ` *  as the first (and only) array element.` |
|      - | 3909 | ` *  An empty string returns an empty array.` |
|      - | 3910 | ` * Errors` |
|      - | 3911 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 3912 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 3913 | ` *  ValueError if $split_length is less than 1.` |
|      - | 3914 | ` */` |
|     26 | 3915 | `PH7_PRIVATE int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 3916 | `{` |
|      - | 3917 | `	const char *zString,*zEnd;` |
|      - | 3918 | `	ph7_value *pArray,*pValue;` |
|      - | 3919 | `	int split_len;` |
|      - | 3920 | `	int nLen;` |
|     29 | 3921 | `	if( nArg < 1 ){` |
|    ! 0 | 3922 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3923 | `			"ArgumentCountError",` |
|      - | 3924 | `			"str_split() expects at least 1 argument, %d given",` |
|    ! 0 | 3925 | `			nArg` |
|      - | 3926 | `			);` |
|      - | 3927 | `	}` |
|      - | 3928 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     39 | 3929 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     42 | 3930 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     26 | 3931 | `	    ph7_value_is_resource(apArg[0]) ){` |
|    ! 0 | 3932 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3933 | `			"TypeError",` |
|      - | 3934 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|    ! 0 | 3935 | `			ph7_type_name(apArg[0])` |
|      - | 3936 | `			);` |
|      - | 3937 | `	}` |
|      - | 3938 | `	/* Point to the target string */` |
|     29 | 3939 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     29 | 3940 | `	split_len = (int)sizeof(char);` |
|     29 | 3941 | `	if( nArg > 1 ){` |
|      - | 3942 | `		/* Split length */` |
|     17 | 3943 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 3944 | `		if( split_len < 1 ){` |
|      6 | 3945 | `			return PH7_VmThrowException(pCtx,` |
|      - | 3946 | `				"ValueError",` |
|      - | 3947 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 3948 | `				);` |
|      - | 3949 | `		}` |
|     11 | 3950 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 3951 | `			split_len = nLen;` |
|      1 | 3952 | `		}` |
|      5 | 3953 | `	}` |
|      - | 3954 | `	/* Create the array and the scalar value */` |
|     23 | 3955 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 3956 | `	/*Chunk value */` |
|     23 | 3957 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     23 | 3958 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 3959 | `		/* Return FALSE */` |
|    ! 0 | 3960 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3961 | `		return PH7_OK;` |
|      - | 3962 | `	}` |
|      - | 3963 | `	/* Point to the end of the string */` |
|     23 | 3964 | `	zEnd = &zString[nLen];` |
|      - | 3965 | `	/* Perform the requested operation */` |
|    131 | 3966 | `	for(;;){` |
|      - | 3967 | `		int nMax;` |
|    143 | 3968 | `		if( zString >= zEnd ){` |
|      - | 3969 | `			/* No more input to process */` |
|     23 | 3970 | `			break;` |
|      - | 3971 | `		}` |
|    121 | 3972 | `		nMax = (int)(zEnd-zString);` |
|    121 | 3973 | `		if( nMax < split_len ){` |
|      3 | 3974 | `			split_len = nMax;` |
|      1 | 3975 | `		}` |
|      - | 3976 | `		/* Copy the current chunk */` |
|    121 | 3977 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 3978 | `		/* Insert it */` |
|    121 | 3979 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 3980 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 3981 | `		}` |
|      - | 3982 | `		/* reset the string cursor */` |
|    121 | 3983 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 3984 | `		/* Update position */` |
|    121 | 3985 | `		zString += split_len;` |
|      1 | 3986 | `	}` |
|      - | 3987 | `	/*` |
|      - | 3988 | `	 * Return the array.` |
|      - | 3989 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 3990 | `	 * upon we return from this function.` |
|      - | 3991 | `	 */` |
|     23 | 3992 | `	ph7_result_value(pCtx,pArray);` |
|     23 | 3993 | `	return PH7_OK;` |
|     16 | 3994 | `}` |
|      - | 3995 | `/*` |
|      - | 3996 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 3997 | ` * Refer to [strspn()].` |
|      - | 3998 | ` */` |
|     28 | 3999 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 4000 | `{` |
|     29 | 4001 | `	const char *zIn = *pzIn;` |
|      - | 4002 | `	const char *zPtr;` |
|      - | 4003 | `	/* Ignore leading white spaces */` |
|     29 | 4004 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 4005 | `		zIn++;` |
|    ! 0 | 4006 | `	}` |
|     29 | 4007 | `	if( zIn >= zEnd ){` |
|      - | 4008 | `		/* End of input */` |
|    ! 0 | 4009 | `		return SXERR_EOF;` |
|      - | 4010 | `	}` |
|     29 | 4011 | `	zPtr = zIn;` |
|      - | 4012 | `	/* Extract the token */` |
|    201 | 4013 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 4014 | `		zIn++;` |
|      1 | 4015 | `	}` |
|     29 | 4016 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 4017 | `	/* Synchronize pointers */` |
|     29 | 4018 | `	*pzIn = zIn;` |
|      - | 4019 | `	/* Return to the caller */` |
|     29 | 4020 | `	return SXRET_OK;` |
|     15 | 4021 | `}` |
|      - | 4022 | `/*` |
|      - | 4023 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 4024 | ` * return the longest match.` |
|      - | 4025 | ` * Refer to [strspn()].` |
|      - | 4026 | ` */` |
|     18 | 4027 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 4028 | `{` |
|     19 | 4029 | `	const char *zEnd = &zString[nLen];` |
|     19 | 4030 | `	const char *zIn = zString;` |
|      - | 4031 | `	int i,c;` |
|     45 | 4032 | `	for(;;){` |
|     91 | 4033 | `		if( zString >= zEnd ){` |
|      7 | 4034 | `			break;` |
|      - | 4035 | `		}` |
|      - | 4036 | `		/* Extract current character */` |
|     85 | 4037 | `		c = zString[0];` |
|      - | 4038 | `		/* Perform the lookup */` |
|    383 | 4039 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 4040 | `			if( c == zMask[i] ){` |
|      - | 4041 | `				/* Character found */` |
|     73 | 4042 | `				break;` |
|      - | 4043 | `			}` |
|    150 | 4044 | `		}` |
|     85 | 4045 | `		if( i >= nMaskLen ){` |
|      - | 4046 | `			/* Character not in the current mask,break immediately */` |
|     13 | 4047 | `			break;` |
|      - | 4048 | `		}` |
|      - | 4049 | `		/* Advance cursor */` |
|     73 | 4050 | `		zString++;` |
|      1 | 4051 | `	}` |
|      - | 4052 | `	/* Longest match */` |
|     19 | 4053 | `	return (int)(zString-zIn);` |
|      1 | 4054 | `}` |
|      - | 4055 | `/*` |
|      - | 4056 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 4057 | ` * Refer to [strcspn()].` |
|      - | 4058 | ` */` |
|     10 | 4059 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 4060 | `{` |
|     11 | 4061 | `	const char *zEnd = &zString[nLen];` |
|     11 | 4062 | `	const char *zIn = zString;` |
|      - | 4063 | `	int i,c;` |
|     12 | 4064 | `	for(;;){` |
|     25 | 4065 | `		if( zString >= zEnd ){` |
|      3 | 4066 | `			break;` |
|      - | 4067 | `		}` |
|      - | 4068 | `		/* Extract current character */` |
|     23 | 4069 | `		c = zString[0];` |
|      - | 4070 | `		/* Perform the lookup */` |
|     51 | 4071 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 4072 | `			if( c == zMask[i] ){` |
|      9 | 4073 | `				break;` |
|      - | 4074 | `			}` |
|     15 | 4075 | `		}` |
|     23 | 4076 | `		if( i < nMaskLen ){` |
|      - | 4077 | `			/* Character in the current mask,break immediately */` |
|      9 | 4078 | `			break;` |
|      - | 4079 | `		}` |
|      - | 4080 | `		/* Advance cursor */` |
|     15 | 4081 | `		zString++;` |
|      1 | 4082 | `	}` |
|      - | 4083 | `	/* Longest match */` |
|     11 | 4084 | `	return (int)(zString-zIn);` |
|      1 | 4085 | `}` |
|      - | 4086 | `/*` |
|      - | 4087 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 4088 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 4089 | ` *  of characters contained within a given mask.` |
|      - | 4090 | ` * Parameters` |
|      - | 4091 | ` * $str` |
|      - | 4092 | ` *  The input string.` |
|      - | 4093 | ` * $mask` |
|      - | 4094 | ` *  The list of allowable characters.` |
|      - | 4095 | ` * $start` |
|      - | 4096 | ` *  The position in subject to start searching.` |
|      - | 4097 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 4098 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 4099 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 4100 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 4101 | ` *  start'th position from the end of subject.` |
|      - | 4102 | ` * $length` |
|      - | 4103 | ` *  The length of the segment from subject to examine.` |
|      - | 4104 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 4105 | ` *  characters after the starting position.` |
|      - | 4106 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 4107 | ` *  position up to length characters from the end of subject.` |
|      - | 4108 | ` * Return` |
|      - | 4109 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 4110 | ` * in mask.` |
|      - | 4111 | ` */` |
|     24 | 4112 | `PH7_PRIVATE int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4113 | `{` |
|      - | 4114 | `	const char *zString,*zMask,*zEnd;` |
|      - | 4115 | `	int iMasklen,iLen;` |
|      - | 4116 | `	SyString sToken;` |
|     25 | 4117 | `	int iCount = 0;` |
|      - | 4118 | `	int rc;` |
|     25 | 4119 | `	if( nArg < 2 ){` |
|      - | 4120 | `		/* Missing agruments,return zero */` |
|    ! 0 | 4121 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4122 | `		return PH7_OK;` |
|      - | 4123 | `	}` |
|      - | 4124 | `	/* Extract the target string */` |
|     25 | 4125 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 4126 | `	/* Extract the mask */` |
|     25 | 4127 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 4128 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 4129 | `		/* Nothing to process,return zero */` |
|      7 | 4130 | `		ph7_result_int(pCtx,0);` |
|      7 | 4131 | `		return PH7_OK;` |
|      - | 4132 | `	}` |
|     19 | 4133 | `	if( nArg > 2 ){` |
|      - | 4134 | `		int nOfft;` |
|      - | 4135 | `		/* Extract the offset */` |
|      9 | 4136 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 4137 | `		if( nOfft < 0 ){` |
|    ! 0 | 4138 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 4139 | `			if( zBase > zString ){` |
|    ! 0 | 4140 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 4141 | `				zString = zBase;` |
|    ! 0 | 4142 | `			}else{` |
|      - | 4143 | `				/* Invalid offset */` |
|    ! 0 | 4144 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 4145 | `				return PH7_OK;` |
|      - | 4146 | `			}` |
|    ! 0 | 4147 | `		}else{` |
|      9 | 4148 | `			if( nOfft >= iLen ){` |
|      - | 4149 | `				/* Invalid offset */` |
|    ! 0 | 4150 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 4151 | `				return PH7_OK;` |
|    ! 0 | 4152 | `			}else{` |
|      - | 4153 | `				/* Update offset */` |
|      9 | 4154 | `				zString += nOfft;` |
|      9 | 4155 | `				iLen -= nOfft;` |
|      - | 4156 | `			}` |
|      - | 4157 | `		}` |
|      9 | 4158 | `		if( nArg > 3 ){` |
|      - | 4159 | `			int iUserlen;` |
|      - | 4160 | `			/* Extract the desired length */` |
|      9 | 4161 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 4162 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 4163 | `				iLen = iUserlen;` |
|      2 | 4164 | `			}` |
|      4 | 4165 | `		}` |
|      4 | 4166 | `	}` |
|      - | 4167 | `	/* Point to the end of the string */` |
|     19 | 4168 | `	zEnd = &zString[iLen];` |
|      - | 4169 | `	/* Extract the first non-space token */` |
|     19 | 4170 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 4171 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 4172 | `		/* Compare against the current mask */` |
|     19 | 4173 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 4174 | `	}` |
|      - | 4175 | `	/* Longest match */` |
|     19 | 4176 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 4177 | `	return PH7_OK;` |
|     13 | 4178 | `}` |
|      - | 4179 | `/*` |
|      - | 4180 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 4181 | ` *  Find length of initial segment not matching mask.` |
|      - | 4182 | ` * Parameters` |
|      - | 4183 | ` * $str` |
|      - | 4184 | ` *  The input string.` |
|      - | 4185 | ` * $mask` |
|      - | 4186 | ` *  The list of not allowed characters.` |
|      - | 4187 | ` * $start` |
|      - | 4188 | ` *  The position in subject to start searching.` |
|      - | 4189 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 4190 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 4191 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 4192 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 4193 | ` *  start'th position from the end of subject.` |
|      - | 4194 | ` * $length` |
|      - | 4195 | ` *  The length of the segment from subject to examine.` |
|      - | 4196 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 4197 | ` *  characters after the starting position.` |
|      - | 4198 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 4199 | ` *  position up to length characters from the end of subject.` |
|      - | 4200 | ` * Return` |
|      - | 4201 | ` *  Returns the length of the segment as an integer.` |
|      - | 4202 | ` */` |
|     14 | 4203 | `PH7_PRIVATE int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4204 | `{` |
|      - | 4205 | `	const char *zString,*zMask,*zEnd;` |
|      - | 4206 | `	int iMasklen,iLen;` |
|      - | 4207 | `	SyString sToken;` |
|     15 | 4208 | `	int iCount = 0;` |
|      - | 4209 | `	int rc;` |
|     15 | 4210 | `	if( nArg < 2 ){` |
|      - | 4211 | `		/* Missing agruments,return zero */` |
|    ! 0 | 4212 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4213 | `		return PH7_OK;` |
|      - | 4214 | `	}` |
|      - | 4215 | `	/* Extract the target string */` |
|     15 | 4216 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 4217 | `	/* Extract the mask */` |
|     15 | 4218 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 4219 | `	if( iLen < 1 ){` |
|      - | 4220 | `		/* Nothing to process,return zero */` |
|    ! 0 | 4221 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4222 | `		return PH7_OK;` |
|      - | 4223 | `	}` |
|     15 | 4224 | `	if( iMasklen < 1 ){` |
|      - | 4225 | `		/* No given mask,return the string length */` |
|      3 | 4226 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 4227 | `		return PH7_OK;` |
|      - | 4228 | `	}` |
|     13 | 4229 | `	if( nArg > 2 ){` |
|      - | 4230 | `		int nOfft;` |
|      - | 4231 | `		/* Extract the offset */` |
|     11 | 4232 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 4233 | `		if( nOfft < 0 ){` |
|    ! 0 | 4234 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 4235 | `			if( zBase > zString ){` |
|    ! 0 | 4236 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 4237 | `				zString = zBase;` |
|    ! 0 | 4238 | `			}else{` |
|      - | 4239 | `				/* Invalid offset */` |
|    ! 0 | 4240 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 4241 | `				return PH7_OK;` |
|      - | 4242 | `			}` |
|    ! 0 | 4243 | `		}else{` |
|     11 | 4244 | `			if( nOfft >= iLen ){` |
|      - | 4245 | `				/* Invalid offset */` |
|      3 | 4246 | `				ph7_result_int(pCtx,0);` |
|      3 | 4247 | `				return PH7_OK;` |
|    ! 0 | 4248 | `			}else{` |
|      - | 4249 | `				/* Update offset */` |
|      9 | 4250 | `				zString += nOfft;` |
|      9 | 4251 | `				iLen -= nOfft;` |
|      - | 4252 | `			}` |
|      - | 4253 | `		}` |
|      9 | 4254 | `		if( nArg > 3 ){` |
|      - | 4255 | `			int iUserlen;` |
|      - | 4256 | `			/* Extract the desired length */` |
|    ! 0 | 4257 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 4258 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 4259 | `				iLen = iUserlen;` |
|    ! 0 | 4260 | `			}` |
|    ! 0 | 4261 | `		}` |
|      4 | 4262 | `	}` |
|      - | 4263 | `	/* Point to the end of the string */` |
|     11 | 4264 | `	zEnd = &zString[iLen];` |
|      - | 4265 | `	/* Extract the first non-space token */` |
|     11 | 4266 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 4267 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 4268 | `		/* Compare against the current mask */` |
|     11 | 4269 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 4270 | `	}` |
|      - | 4271 | `	/* Longest match */` |
|     11 | 4272 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 4273 | `	return PH7_OK;` |
|      8 | 4274 | `}` |
|      - | 4275 | `/*` |
|      - | 4276 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 4277 | ` *  Search a string for any of a set of characters.` |
|      - | 4278 | ` * Parameters` |
|      - | 4279 | ` *  $haystack` |
|      - | 4280 | ` *   The string where char_list is looked for.` |
|      - | 4281 | ` *  $char_list` |
|      - | 4282 | ` *   This parameter is case sensitive.` |
|      - | 4283 | ` * Return` |
|      - | 4284 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 4285 | ` */` |
|      4 | 4286 | `PH7_PRIVATE int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4287 | `{` |
|      - | 4288 | `	const char *zString,*zList,*zEnd;` |
|      - | 4289 | `	int iLen,iListLen,i,c;` |
|      - | 4290 | `	sxu32 nOfft,nMax;` |
|      - | 4291 | `	sxi32 rc;` |
|      5 | 4292 | `	if( nArg < 2 ){` |
|      - | 4293 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 4294 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4295 | `		return PH7_OK;` |
|      - | 4296 | `	}` |
|      - | 4297 | `	/* Extract the haystack and the char list */` |
|      5 | 4298 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 4299 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 4300 | `	if( iLen < 1 ){` |
|      - | 4301 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 4302 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4303 | `		return PH7_OK;` |
|      - | 4304 | `	}` |
|      - | 4305 | `	/* Point to the end of the string */` |
|      5 | 4306 | `	zEnd = &zString[iLen];` |
|      5 | 4307 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 4308 | `	/* perform the requested operation */` |
|     15 | 4309 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 4310 | `		c = zList[i];` |
|     11 | 4311 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 4312 | `		if( rc == SXRET_OK ){` |
|      5 | 4313 | `			if( nMax < nOfft ){` |
|      3 | 4314 | `				nOfft = nMax;` |
|      1 | 4315 | `			}` |
|      2 | 4316 | `		}` |
|      6 | 4317 | `	}` |
|      5 | 4318 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 4319 | `		/* No such substring,return FALSE */` |
|      3 | 4320 | `		ph7_result_bool(pCtx,0);` |
|      2 | 4321 | `	}else{` |
|      - | 4322 | `		/* Return the substring */` |
|      3 | 4323 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 4324 | `	}` |
|      5 | 4325 | `	return PH7_OK;` |
|      3 | 4326 | `}` |
|      - | 4327 | `/* SPDX-SnippetBegin */` |
|      - | 4328 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 4329 | `/* SPDX-License-Identifier: blessing */` |
|      - | 4330 | `/*` |
|      - | 4331 | ` * string soundex(string $str)` |
|      - | 4332 | ` *  Calculate the soundex key of a string.` |
|      - | 4333 | ` * Parameters` |
|      - | 4334 | ` *  $str` |
|      - | 4335 | ` *   The input string.` |
|      - | 4336 | ` * Return` |
|      - | 4337 | ` *  Returns the soundex key as a string.` |
|      - | 4338 | ` * Note:` |
|      - | 4339 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 4340 | ` * source tree.` |
|      - | 4341 | ` */` |
|     22 | 4342 | `PH7_PRIVATE int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4343 | `{` |
|      - | 4344 | `	const unsigned char *zIn;` |
|      - | 4345 | `	char zResult[8];` |
|      - | 4346 | `	int i, j;` |
|      - | 4347 | `	static const unsigned char iCode[] = {` |
|      - | 4348 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 4349 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 4350 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 4351 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 4352 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 4353 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 4354 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 4355 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 4356 | `	};` |
|     23 | 4357 | `	if( nArg < 1 ){` |
|      - | 4358 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4359 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4360 | `		return PH7_OK;` |
|      - | 4361 | `	}` |
|     23 | 4362 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     35 | 4363 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     23 | 4364 | `	if( zIn[i] ){` |
|     17 | 4365 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 4366 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 4367 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 4368 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 4369 | `			if( code>0 ){` |
|     45 | 4370 | `				if( code!=prevcode ){` |
|     33 | 4371 | `					prevcode = (unsigned char)code;` |
|     33 | 4372 | `					zResult[j++] = (char)code + '0';` |
|     16 | 4373 | `				}` |
|     23 | 4374 | `			}else{` |
|     49 | 4375 | `				prevcode = 0;` |
|      - | 4376 | `			}` |
|     47 | 4377 | `		}` |
|     33 | 4378 | `		while( j<4 ){` |
|     17 | 4379 | `			zResult[j++] = '0';` |
|      1 | 4380 | `		}` |
|     17 | 4381 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 4382 | `	}else{` |
|      - | 4383 | `	  /* No alphabetic character: PHP returns "0000" (not the SQLite "?000"). */` |
|      7 | 4384 | `	  ph7_result_string(pCtx,"0000",4);` |
|      - | 4385 | `	}` |
|     23 | 4386 | `	return PH7_OK;` |
|     12 | 4387 | `}` |
|      - | 4388 | `/* SPDX-SnippetEnd */` |
|      - | 4389 | `/*` |
|      - | 4390 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 4391 | ` *  Wraps a string to a given number of characters.` |
|      - | 4392 | ` * Parameters` |
|      - | 4393 | ` *  $str` |
|      - | 4394 | ` *   The input string.` |
|      - | 4395 | ` * $width` |
|      - | 4396 | ` *  The column width.` |
|      - | 4397 | ` * $break` |
|      - | 4398 | ` *  The line is broken using the optional break parameter.` |
|      - | 4399 | ` * Return` |
|      - | 4400 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 4401 | ` */` |
|     26 | 4402 | `PH7_PRIVATE int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4403 | `{` |
|      - | 4404 | `	const char *zIn,*zBreak;` |
|      - | 4405 | `	SyBlob sWorker;` |
|      - | 4406 | `	int iLen,iBreaklen,iWidth,iCut,iStart,iSpace,iCur;` |
|      - | 4407 | `	sxi32 rc;` |
|     27 | 4408 | `	if( nArg < 1 ){` |
|      - | 4409 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4410 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4411 | `		return PH7_OK;` |
|      - | 4412 | `	}` |
|      - | 4413 | `	/* Extract the input string */` |
|     27 | 4414 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 4415 | `	/* Width (default 75; PHP allows 0/negative — break at every space). */` |
|     27 | 4416 | `	iWidth = 75;` |
|     27 | 4417 | `	if( nArg > 1 ){` |
|     27 | 4418 | `		iWidth = ph7_value_to_int(apArg[1]);` |
|     13 | 4419 | `	}` |
|      - | 4420 | `	/* Break string (default "\n"). */` |
|     27 | 4421 | `	zBreak = "\n";` |
|     27 | 4422 | `	iBreaklen = (int)sizeof(char);` |
|     27 | 4423 | `	if( nArg > 2 ){` |
|     13 | 4424 | `		zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      6 | 4425 | `	}` |
|      - | 4426 | `	/* Cut long words? (default false). */` |
|     27 | 4427 | `	iCut = 0;` |
|     27 | 4428 | `	if( nArg > 3 ){` |
|      7 | 4429 | `		iCut = ph7_value_to_bool(apArg[3]);` |
|      3 | 4430 | `	}` |
|     27 | 4431 | `	if( iLen < 1 ){` |
|      - | 4432 | `		/* PHP returns the empty string for empty input before validating the other args. */` |
|      5 | 4433 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 4434 | `		return PH7_OK;` |
|      - | 4435 | `	}` |
|      - | 4436 | `	/* PHP 8 domain errors (catchable ValueError). */` |
|     23 | 4437 | `	if( iBreaklen < 1 ){` |
|      3 | 4438 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4439 | `			"wordwrap(): Argument #3 ($break) must not be empty");` |
|      - | 4440 | `	}` |
|     21 | 4441 | `	if( iWidth == 0 && iCut ){` |
|      3 | 4442 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4443 | `			"wordwrap(): Argument #4 ($cut_long_words) cannot be true when argument #2 ($width) is 0");` |
|      - | 4444 | `	}` |
|      - | 4445 | `	/*` |
|      - | 4446 | `	 * PHP's algorithm: a single left-to-right pass tracking the start of the` |
|      - | 4447 | `	 * current line (iStart) and the position of the last space seen on it` |
|      - | 4448 | `	 * (iSpace). A break is emitted when the line reaches the width, at the last` |
|      - | 4449 | `	 * space if there was one, otherwise (only when cut is enabled) hard at the` |
|      - | 4450 | `	 * boundary. An existing break sequence in the input resets the line.` |
|      - | 4451 | `	 */` |
|     19 | 4452 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     19 | 4453 | `	iStart = iSpace = iCur = 0;` |
|     19 | 4454 | `	rc = SXRET_OK;` |
|    551 | 4455 | `	while( iCur < iLen ){` |
|    533 | 4456 | `		if( iBreaklen <= iLen - iCur && SyMemcmp(&zIn[iCur],zBreak,(sxu32)iBreaklen) == 0 ){` |
|      - | 4457 | `			/* Existing break sequence in the input: copy it verbatim and reset the line. */` |
|    ! 0 | 4458 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart + iBreaklen));` |
|    ! 0 | 4459 | `			if( rc != SXRET_OK ){ goto oom; }` |
|    ! 0 | 4460 | `			iCur += iBreaklen;` |
|    ! 0 | 4461 | `			iStart = iSpace = iCur;` |
|    ! 0 | 4462 | `			continue;` |
|    533 | 4463 | `		}else if( zIn[iCur] == ' ' ){` |
|     67 | 4464 | `			if( iCur - iStart >= iWidth ){` |
|      - | 4465 | `				/* The line already fills the width at this space: break here (the space is consumed). */` |
|     13 | 4466 | `				rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     13 | 4467 | `				if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     13 | 4468 | `				if( rc != SXRET_OK ){ goto oom; }` |
|     13 | 4469 | `				iStart = iCur + 1;` |
|      6 | 4470 | `			}` |
|     67 | 4471 | `			iSpace = iCur;` |
|    500 | 4472 | `		}else if( iCut && iCur - iStart >= iWidth && iStart >= iSpace ){` |
|      - | 4473 | `			/* A word longer than the width with no space to break at: hard-cut at the boundary. */` |
|      7 | 4474 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|      7 | 4475 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|      7 | 4476 | `			if( rc != SXRET_OK ){ goto oom; }` |
|      7 | 4477 | `			iStart = iSpace = iCur;` |
|    464 | 4478 | `		}else if( iCur - iStart >= iWidth && iStart < iSpace ){` |
|      - | 4479 | `			/* Past the width mid-word: wrap back to the last space (which is consumed). */` |
|     17 | 4480 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iSpace - iStart));` |
|     17 | 4481 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     17 | 4482 | `			if( rc != SXRET_OK ){ goto oom; }` |
|     17 | 4483 | `			iStart = iSpace = iSpace + 1;` |
|      8 | 4484 | `		}` |
|    533 | 4485 | `		iCur++;` |
|      1 | 4486 | `	}` |
|      - | 4487 | `	/* Emit the trailing chunk. */` |
|     19 | 4488 | `	if( iStart < iCur ){` |
|     19 | 4489 | `		rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     19 | 4490 | `		if( rc != SXRET_OK ){ goto oom; }` |
|      9 | 4491 | `	}` |
|     19 | 4492 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|     19 | 4493 | `	SyBlobRelease(&sWorker);` |
|     19 | 4494 | `	return PH7_OK;` |
|    ! 0 | 4495 | `oom:` |
|    ! 0 | 4496 | `	SyBlobRelease(&sWorker);` |
|    ! 0 | 4497 | `	return PH7_ContextMemoryError(pCtx);` |
|     14 | 4498 | `}` |
|      - | 4499 | `/*` |
|      - | 4500 | ` * Check if the given character is a member of the given mask.` |
|      - | 4501 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 4502 | ` * Refer to [strtok()].` |
|      - | 4503 | ` */` |
|     30 | 4504 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 4505 | `{` |
|      - | 4506 | `	int i;` |
|     57 | 4507 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 4508 | `		if( c == zMask[i] ){` |
|     13 | 4509 | `			if( pOfft ){` |
|      5 | 4510 | `				*pOfft = i;` |
|      2 | 4511 | `			}` |
|     13 | 4512 | `			return TRUE;` |
|      - | 4513 | `		}` |
|     14 | 4514 | `	}` |
|     19 | 4515 | `	return FALSE;` |
|     16 | 4516 | `}` |
|      - | 4517 | `/*` |
|      - | 4518 | ` * Extract a single token from the input stream.` |
|      - | 4519 | ` * Refer to [strtok()].` |
|      - | 4520 | ` */` |
|      6 | 4521 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 4522 | `{` |
|      7 | 4523 | `	const char *zIn = *pzIn;` |
|      - | 4524 | `	const char *zPtr;` |
|      - | 4525 | `	/* Ignore leading delimiter */` |
|     11 | 4526 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 4527 | `		zIn++;` |
|      1 | 4528 | `	}` |
|      7 | 4529 | `	if( zIn >= zEnd ){` |
|      - | 4530 | `		/* End of input */` |
|    ! 0 | 4531 | `		return SXERR_EOF;` |
|      - | 4532 | `	}` |
|      7 | 4533 | `	zPtr = zIn;` |
|      - | 4534 | `	/* Extract the token */` |
|     13 | 4535 | `	while( zIn < zEnd ){` |
|     11 | 4536 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 4537 | `			/* UTF-8 stream */` |
|    ! 0 | 4538 | `			zIn++;` |
|    ! 0 | 4539 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 4540 | `		}else{` |
|     11 | 4541 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 4542 | `				break;` |
|      - | 4543 | `			}` |
|      7 | 4544 | `			zIn++;` |
|      - | 4545 | `		}` |
|      1 | 4546 | `	}` |
|      7 | 4547 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 4548 | `	/* Update the cursor */` |
|      7 | 4549 | `	*pzIn = zIn;` |
|      - | 4550 | `	/* Return to the caller */` |
|      7 | 4551 | `	return SXRET_OK;` |
|      4 | 4552 | `}` |
|      - | 4553 | `/* strtok auxiliary private data */` |
|      - | 4554 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 4555 | `struct strtok_aux_data` |
|      - | 4556 | `{` |
|      - | 4557 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 4558 | `	const char *zIn;   /* Current input stream */` |
|      - | 4559 | `	const char *zEnd;  /* End of input */` |
|      - | 4560 | `};` |
|      - | 4561 | `/*` |
|      - | 4562 | ` * string strtok(string $str,string $token)` |
|      - | 4563 | ` * string strtok(string $token)` |
|      - | 4564 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 4565 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 4566 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 4567 | ` *  words by using the space character as the token.` |
|      - | 4568 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 4569 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 4570 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 4571 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 4572 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 4573 | ` *  the argument are found.` |
|      - | 4574 | ` * Parameters` |
|      - | 4575 | ` *  $str` |
|      - | 4576 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 4577 | ` * $token` |
|      - | 4578 | ` *  The delimiter used when splitting up str.` |
|      - | 4579 | ` * Return` |
|      - | 4580 | ` *   Current token or FALSE on EOF.` |
|      - | 4581 | ` */` |
|      6 | 4582 | `PH7_PRIVATE int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4583 | `{` |
|      - | 4584 | `	strtok_aux_data *pAux;` |
|      - | 4585 | `	const char *zMask;` |
|      - | 4586 | `	SyString sToken;` |
|      - | 4587 | `	int nMasklen;` |
|      - | 4588 | `	sxi32 rc;` |
|      7 | 4589 | `	if( nArg < 2 ){` |
|      - | 4590 | `		/* Extract top aux data */` |
|      5 | 4591 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      5 | 4592 | `		if( pAux == 0 ){` |
|      - | 4593 | `			/* No aux data,return FALSE */` |
|    ! 0 | 4594 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 4595 | `			return PH7_OK;` |
|      - | 4596 | `		}` |
|      5 | 4597 | `		nMasklen = 0;` |
|      5 | 4598 | `		zMask = ""; /* cc warning */` |
|      5 | 4599 | `		if( nArg > 0 ){` |
|      - | 4600 | `			/* Extract the mask */` |
|      5 | 4601 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 4602 | `		}` |
|      5 | 4603 | `		if( nMasklen < 1 ){` |
|      - | 4604 | `			/* Invalid mask,return FALSE */` |
|    ! 0 | 4605 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 4606 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 4607 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 4608 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 4609 | `			return PH7_OK;` |
|      - | 4610 | `		}` |
|      - | 4611 | `		/* Extract the token */` |
|      5 | 4612 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 4613 | `		if( rc != SXRET_OK ){` |
|      - | 4614 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 4615 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 4616 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 4617 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 4618 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 4619 | `		}else{` |
|      - | 4620 | `			/* Return the extracted token */` |
|      5 | 4621 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 4622 | `		}` |
|      3 | 4623 | `	}else{` |
|      - | 4624 | `		const char *zInput,*zCur;` |
|      - | 4625 | `		char *zDup;` |
|      - | 4626 | `		int nLen;` |
|      - | 4627 | `		/* Extract the raw input */` |
|      3 | 4628 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4629 | `		if( nLen < 1 ){` |
|      - | 4630 | `			/* Empty input,return FALSE */` |
|    ! 0 | 4631 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 4632 | `			return PH7_OK;` |
|      - | 4633 | `		}` |
|      - | 4634 | `		/* Extract the mask */` |
|      3 | 4635 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 4636 | `		if( nMasklen < 1 ){` |
|      - | 4637 | `			/* Set a default mask */` |
|      - | 4638 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 4639 | `			zMask = TOK_MASK;` |
|    ! 0 | 4640 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 4641 | `#undef TOK_MASK` |
|    ! 0 | 4642 | `		}` |
|      - | 4643 | `		/* Extract a single token */` |
|      3 | 4644 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 4645 | `		if( rc != SXRET_OK ){` |
|      - | 4646 | `			/* Empty input */` |
|    ! 0 | 4647 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 4648 | `			return PH7_OK;` |
|    ! 0 | 4649 | `		}else{` |
|      - | 4650 | `			/* Return the extracted token */` |
|      3 | 4651 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 4652 | `		}` |
|      - | 4653 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 4654 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 4655 | `		if( pAux ){` |
|      3 | 4656 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 4657 | `			if( nLen < 1 ){` |
|    ! 0 | 4658 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 4659 | `				return PH7_OK;` |
|      - | 4660 | `			}` |
|      - | 4661 | `			/* Duplicate input */` |
|      3 | 4662 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 4663 | `			if( zDup  ){` |
|      3 | 4664 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 4665 | `				/* Register the aux data */` |
|      3 | 4666 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 4667 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 4668 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 4669 | `			}` |
|      1 | 4670 | `		}` |
|      - | 4671 | `	}` |
|      7 | 4672 | `	return PH7_OK;` |
|      4 | 4673 | `}` |
|      - | 4674 | `/*` |
|      - | 4675 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 4676 | ` *  Pad a string to a certain length with another string` |
|      - | 4677 | ` * Parameters` |
|      - | 4678 | ` *  $input` |
|      - | 4679 | ` *   The input string.` |
|      - | 4680 | ` * $pad_length` |
|      - | 4681 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 4682 | ` *   string, no padding takes place.` |
|      - | 4683 | ` * $pad_string` |
|      - | 4684 | ` *   Note:` |
|      - | 4685 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 4686 | ` *    divided by the pad_string's length.` |
|      - | 4687 | ` * $pad_type` |
|      - | 4688 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 4689 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 4690 | ` * Return` |
|      - | 4691 | ` *  The padded string.` |
|      - | 4692 | ` */` |
|     20 | 4693 | `PH7_PRIVATE int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4694 | `{` |
|      - | 4695 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 4696 | `	const char *zIn,*zPad;` |
|     22 | 4697 | `	if( nArg < 2 ){` |
|      - | 4698 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4699 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4700 | `		return PH7_OK;` |
|      - | 4701 | `	}` |
|      - | 4702 | `	/* Extract the target string */` |
|     22 | 4703 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 4704 | `	/* Padding length */` |
|      - | 4705 | `	{` |
|     22 | 4706 | `		sxi64 iTmp = 0;` |
|     22 | 4707 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_pad",2,"$length","int",&iTmp);` |
|     22 | 4708 | `		if( rcArg != PH7_OK ){` |
|    ! 0 | 4709 | `			return rcArg;` |
|      - | 4710 | `		}` |
|     22 | 4711 | `		iRealPad = iPadlen = (int)iTmp;` |
|      - | 4712 | `	}` |
|     22 | 4713 | `	if( iPadlen > 0 ){` |
|     20 | 4714 | `		iPadlen -= iLen;` |
|      9 | 4715 | `	}` |
|     22 | 4716 | `	if( iPadlen < 1  ){` |
|      - | 4717 | `		/* Return the string verbatim */` |
|      5 | 4718 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      5 | 4719 | `		return PH7_OK;` |
|      - | 4720 | `	}` |
|     18 | 4721 | `	zPad = " "; /* Whitespace padding */` |
|     18 | 4722 | `	iStrpad = (int)sizeof(char);` |
|     18 | 4723 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|     18 | 4724 | `	if( nArg > 2 ){` |
|      - | 4725 | `		/* Padding string */` |
|      7 | 4726 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      7 | 4727 | `		if( iStrpad < 1 ){` |
|      - | 4728 | `			/* An empty pad string throws a catchable ValueError in PHP 8` |
|      - | 4729 | `			 * (only reached once padding is actually required). */` |
|      3 | 4730 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4731 | `				"str_pad(): Argument #3 ($pad_string) must not be empty");` |
|      - | 4732 | `		}` |
|      5 | 4733 | `		if( nArg > 3 ){` |
|      - | 4734 | `			/* Padd type */` |
|      5 | 4735 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 4736 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 4737 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 4738 | `			}` |
|      2 | 4739 | `		}` |
|      2 | 4740 | `	}` |
|     16 | 4741 | `	iDiv = 1;` |
|     16 | 4742 | `	if( iType == 2 ){` |
|    ! 0 | 4743 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 4744 | `	}` |
|      - | 4745 | `	/* Perform the requested operation */` |
|     16 | 4746 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 4747 | `		jPad = iStrpad;` |
|      5 | 4748 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 4749 | `			/* Padding */` |
|      5 | 4750 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 4751 | `				break;` |
|      - | 4752 | `			}` |
|      3 | 4753 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 4754 | `		}` |
|      3 | 4755 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 4756 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 4757 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 4758 | `				if( jPad > iStrpad ){` |
|    ! 0 | 4759 | `					jPad = iStrpad;` |
|    ! 0 | 4760 | `				}` |
|      3 | 4761 | `				if( jPad < 1){` |
|    ! 0 | 4762 | `					break;` |
|      - | 4763 | `				}` |
|      3 | 4764 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 4765 | `			}` |
|      1 | 4766 | `		}` |
|      1 | 4767 | `	}` |
|     16 | 4768 | `	if( iLen > 0 ){` |
|      - | 4769 | `		/* Append the input string */` |
|     16 | 4770 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      7 | 4771 | `	}` |
|     16 | 4772 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|     56 | 4773 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 4774 | `			/* Padding */` |
|     56 | 4775 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|     14 | 4776 | `				break;` |
|      - | 4777 | `			}` |
|     44 | 4778 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|     23 | 4779 | `		}` |
|     26 | 4780 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|     14 | 4781 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|     14 | 4782 | `			if( jPad > iStrpad ){` |
|    ! 0 | 4783 | `				jPad = iStrpad;` |
|    ! 0 | 4784 | `			}` |
|     14 | 4785 | `			if( jPad < 1){` |
|    ! 0 | 4786 | `				break;` |
|      - | 4787 | `			}` |
|     14 | 4788 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 4789 | `		}` |
|      6 | 4790 | `	}` |
|     16 | 4791 | `	return PH7_OK;` |
|     12 | 4792 | `}` |
|      - | 4793 | `/*` |
|      - | 4794 | ` * String replacement private data.` |
|      - | 4795 | ` */` |
|      - | 4796 | `typedef struct str_replace_data str_replace_data;` |
|      - | 4797 | `struct str_replace_data` |
|      - | 4798 | `{` |
|      - | 4799 | `	/* Used by the str_replace family to collect the search/replace arguments. */` |
|      - | 4800 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 4801 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 4802 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 4803 | `};` |
|      - | 4804 | `/*` |
|      - | 4805 | ` * Remove a substring.` |
|      - | 4806 | ` */` |
|      - | 4807 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 4808 | `	for(;;){\` |
|      - | 4809 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 4810 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 4811 | `		++OFFT;\` |
|      - | 4812 | `	}\` |
|      - | 4813 | `}` |
|      - | 4814 | `/*` |
|      - | 4815 | ` * Shift right and insert algorithm.` |
|      - | 4816 | ` */` |
|      - | 4817 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 4818 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 4819 | `		for(;;){\` |
|      - | 4820 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 4821 | `			if(INLEN < 1 ) { break; }\` |
|      - | 4822 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 4823 | `			--INLEN; \` |
|      - | 4824 | `		}\` |
|      - | 4825 | `		for(;;){\` |
|      - | 4826 | `				if(ELEN < 1) { break; }\` |
|      - | 4827 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 4828 | `				OFFT++;\` |
|      - | 4829 | `				ENTRY++;\` |
|      - | 4830 | `				--ELEN;\` |
|      - | 4831 | `		}\` |
|      - | 4832 | `}` |
|      - | 4833 | `/*` |
|      - | 4834 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 4835 | ` * replacement string [i.e: zReplace].` |
|      - | 4836 | ` */` |
|     88 | 4837 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      5 | 4838 | `{` |
|     93 | 4839 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 4840 | `	sxu32 n,m;` |
|     93 | 4841 | `	n = SyBlobLength(pWorker);` |
|     93 | 4842 | `	m = nOfft;` |
|      - | 4843 | `	/* Delete the old entry */` |
|   7427 | 4844 | `	STRDEL(zInput,n,m,nLen);` |
|     93 | 4845 | `	SyBlobLength(pWorker) -= nLen;` |
|     93 | 4846 | `	if( nReplen > 0 ){` |
|     87 | 4847 | `		sxi32 iRep = nReplen;` |
|      - | 4848 | `		sxi32 rc;` |
|      - | 4849 | `		/*` |
|      - | 4850 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 4851 | `		 * string.` |
|      - | 4852 | `		 */` |
|     87 | 4853 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     87 | 4854 | `		if( rc != SXRET_OK ){` |
|      - | 4855 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 4856 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 4857 | `			return rc;` |
|      - | 4858 | `		}` |
|      - | 4859 | `		/* Perform the insertion now */` |
|     87 | 4860 | `		zInput = (char *)SyBlobData(pWorker);` |
|     87 | 4861 | `		n = SyBlobLength(pWorker);` |
|   7253 | 4862 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     87 | 4863 | `		SyBlobLength(pWorker) += nReplen;` |
|     41 | 4864 | `	}` |
|     93 | 4865 | `	return SXRET_OK;` |
|     49 | 4866 | `}` |
|      - | 4867 | `/*` |
|      - | 4868 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 4869 | ` * to collect search/replace string.` |
|      - | 4870 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 4871 | ` */` |
|    190 | 4872 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      5 | 4873 | `{` |
|    195 | 4874 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 4875 | `	SyString sWorker;` |
|      - | 4876 | `	const char *zIn;` |
|      - | 4877 | `	int nByte;` |
|      - | 4878 | `	/* Extract a string representation of the given argument */` |
|    195 | 4879 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|    195 | 4880 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|    195 | 4881 | `	if( nByte > 0 ){` |
|      - | 4882 | `		char *zDup;` |
|      - | 4883 | `		/* Duplicate the chunk */` |
|    189 | 4884 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 4885 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 4886 | `			);` |
|    189 | 4887 | `		if( zDup == 0 ){` |
|      - | 4888 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 4889 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 4890 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 4891 | `			return SXERR_MEM;` |
|      - | 4892 | `		}` |
|    189 | 4893 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 4894 | `		/* Save the chunk */` |
|    189 | 4895 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     92 | 4896 | `	}` |
|      - | 4897 | `	/* Save for later processing */` |
|    195 | 4898 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 4899 | `	/* All done */` |
|     95 | 4900 | `	SXUNUSED(pKey); /* cc warning */` |
|    195 | 4901 | `	return PH7_OK;` |
|    100 | 4902 | `}` |
|      - | 4903 | `/*` |
|      - | 4904 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 4905 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 4906 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 4907 | ` * Parameters` |
|      - | 4908 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 4909 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 4910 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 4911 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 4912 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 4913 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 4914 | ` * $search` |
|      - | 4915 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 4916 | ` *  to designate multiple needles.` |
|      - | 4917 | ` * $replace` |
|      - | 4918 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 4919 | ` *  to designate multiple replacements.` |
|      - | 4920 | ` * $subject` |
|      - | 4921 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 4922 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 4923 | ` *  of subject, and the return value is an array as well.` |
|      - | 4924 | ` * $count (Not used)` |
|      - | 4925 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 4926 | ` * Return` |
|      - | 4927 | ` * This function returns a string or an array with the replaced values.` |
|      - | 4928 | ` */` |
|  29994 | 4929 | `PH7_PRIVATE int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 4930 | `{` |
|      - | 4931 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 4932 | `	ProcStringMatch xMatch;` |
|      - | 4933 | `	const char *zIn,*zFunc;` |
|      - | 4934 | `	str_replace_data sRep;` |
|      - | 4935 | `	SyBlob sWorker;` |
|      - | 4936 | `	SySet sReplace;` |
|      - | 4937 | `	SySet sSearch;` |
|      - | 4938 | `	int rep_str;` |
|      - | 4939 | `	int nByte;` |
|      - | 4940 | `	sxi32 rc;` |
|  29999 | 4941 | `	if( nArg < 3 ){` |
|      - | 4942 | `		/* Missing/Invalid arguments,return null */` |
|    ! 0 | 4943 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4944 | `		return PH7_OK;` |
|      - | 4945 | `	}` |
|      - | 4946 | `	/* Initialize fields */` |
|  29999 | 4947 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  29999 | 4948 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  29999 | 4949 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  29999 | 4950 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  29999 | 4951 | `	sRep.pCtx = pCtx;` |
|  29999 | 4952 | `	sRep.pCollector = &sSearch;` |
|  29999 | 4953 | `	rep_str = 0;` |
|      - | 4954 | `	/* Extract the subject */` |
|  29999 | 4955 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  29999 | 4956 | `	if( nByte < 1 ){` |
|      - | 4957 | `		/* Nothing to replace,return the empty string */` |
|     21 | 4958 | `		ph7_result_string(pCtx,"",0);` |
|     21 | 4959 | `		return PH7_OK;` |
|      - | 4960 | `	}` |
|      - | 4961 | `	/* Copy the subject */` |
|  29979 | 4962 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 4963 | `	/* Search string */` |
|  29979 | 4964 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 4965 | `		/* Collect search string */` |
|     91 | 4966 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|     48 | 4967 | `	}else{` |
|      - | 4968 | `		/* Single pattern */` |
|  29893 | 4969 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  29893 | 4970 | `		if( nByte < 1 ){` |
|      - | 4971 | `			/* Return the subject untouched since no search string is available */` |
|      7 | 4972 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      7 | 4973 | `			return PH7_OK;` |
|      - | 4974 | `		}` |
|  29887 | 4975 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 4976 | `		/* Save for later processing */` |
|  29887 | 4977 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 4978 | `	}` |
|      - | 4979 | `	/* Replace string */` |
|  29973 | 4980 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 4981 | `		/* Collect replace string */` |
|      9 | 4982 | `		sRep.pCollector = &sReplace;` |
|      9 | 4983 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      5 | 4984 | `	}else{` |
|      - | 4985 | `		/* Single needle */` |
|  29965 | 4986 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  29965 | 4987 | `		rep_str = 1;` |
|  29965 | 4988 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 4989 | `		/* Save for later processing */` |
|  29965 | 4990 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 4991 | `	}` |
|      - | 4992 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  29973 | 4993 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 4994 | `		SySetRelease(&sSearch);` |
|    ! 0 | 4995 | `		SySetRelease(&sReplace);` |
|    ! 0 | 4996 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 4997 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4998 | `	}` |
|      - | 4999 | `	/* Reset loop cursors */` |
|  29973 | 5000 | `	SySetResetCursor(&sSearch);` |
|  29973 | 5001 | `	SySetResetCursor(&sReplace);` |
|  29973 | 5002 | `	pReplace = pSearch = 0; /* cc warning */` |
|  29973 | 5003 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 5004 | `	/* Extract function name */` |
|  29973 | 5005 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 5006 | `	/* Set the default pattern match routine */` |
|  29973 | 5007 | `	xMatch = SyBlobSearch;` |
|  29973 | 5008 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 5009 | `		/* Case insensitive pattern match */` |
|     11 | 5010 | `		xMatch = iPatternMatch;` |
|      5 | 5011 | `	}` |
|      - | 5012 | `	/* Start the replace process */` |
|  60029 | 5013 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 5014 | `		sxu32 nCount,nOfft;` |
|      - | 5015 | `		/* Extract the replace string */` |
|  30061 | 5016 | `		if( rep_str ){` |
|  30043 | 5017 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  15024 | 5018 | `		}else{` |
|     19 | 5019 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 5020 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 5021 | `				 * An empty string is used for the rest of replacement values` |
|      - | 5022 | `				 */` |
|      3 | 5023 | `				pReplace = 0;` |
|      1 | 5024 | `			}` |
|      - | 5025 | `		}` |
|  30061 | 5026 | `		if( pReplace == 0 ){` |
|      - | 5027 | `			/* Use an empty string instead */` |
|      3 | 5028 | `			pReplace = &sTemp;` |
|      1 | 5029 | `		}` |
|  30061 | 5030 | `		if( pSearch->nByte <  1 ){` |
|      - | 5031 | `			/* php ignores an empty search string, but its replacement still` |
|      - | 5032 | `			 * CONSUMES a slot so the remaining pairs stay aligned. Skipping` |
|      - | 5033 | `			 * before the fetch above shifted every later replacement by one:` |
|      - | 5034 | `			 * str_replace(['','l'],['x','L'],'hello') answered "hexxo". */` |
|      7 | 5035 | `			continue;` |
|      - | 5036 | `		}` |
|  30055 | 5037 | `		nOfft = nCount = 0;` |
|  15069 | 5038 | `		for(;;){` |
|  30143 | 5039 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     15 | 5040 | `				break;` |
|      - | 5041 | `			}` |
|      - | 5042 | `			/* Perform a pattern lookup */` |
|  45191 | 5043 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  30124 | 5044 | `				pSearch->nByte,&nOfft);` |
|  30129 | 5045 | `			if( rc != SXRET_OK ){` |
|      - | 5046 | `				/* Pattern not found */` |
|  30041 | 5047 | `				break;` |
|      - | 5048 | `			}` |
|      - | 5049 | `			/* Perform the replace operation */` |
|     93 | 5050 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     93 | 5051 | `			if( rc != SXRET_OK ){` |
|      - | 5052 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 5053 | `				SySetRelease(&sSearch);` |
|    ! 0 | 5054 | `				SySetRelease(&sReplace);` |
|    ! 0 | 5055 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 5056 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 5057 | `			}` |
|      - | 5058 | `			/* Increment offset counter */` |
|     93 | 5059 | `			nCount += nOfft + pReplace->nByte;` |
|      5 | 5060 | `		}` |
|      5 | 5061 | `	}` |
|      - | 5062 | `	/* All done,clean-up the mess left behind */` |
|  29973 | 5063 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  29973 | 5064 | `	SySetRelease(&sSearch);` |
|  29973 | 5065 | `	SySetRelease(&sReplace);` |
|  29973 | 5066 | `	SyBlobRelease(&sWorker);` |
|  29973 | 5067 | `	if( rc != PH7_OK ){` |
|    ! 0 | 5068 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5069 | `	}` |
|  29973 | 5070 | `	return PH7_OK;` |
|  15002 | 5071 | `}` |
|      - | 5072 | `/*` |
|      - | 5073 | ` * strtr() array form: a single (key,value) pair copied out of the replace_pairs` |
|      - | 5074 | ` * array. The bytes are owned by a persistent pool (see strtr_collect) rather than` |
|      - | 5075 | ` * the transient walker values, which HashmapWalk releases after each callback, so` |
|      - | 5076 | ` * we store byte offsets into that pool instead of raw pointers.` |
|      - | 5077 | ` */` |
|      - | 5078 | `typedef struct strtr_entry strtr_entry;` |
|      - | 5079 | `struct strtr_entry` |
|      - | 5080 | `{` |
|      - | 5081 | `	sxu32 nKeyOfft; /* Offset of the search key inside the pool */` |
|      - | 5082 | `	sxu32 nKeyLen;  /* Length of the search key */` |
|      - | 5083 | `	sxu32 nValOfft; /* Offset of the replacement inside the pool */` |
|      - | 5084 | `	sxu32 nValLen;  /* Length of the replacement */` |
|      - | 5085 | `};` |
|      - | 5086 | `typedef struct strtr_collect strtr_collect;` |
|      - | 5087 | `struct strtr_collect` |
|      - | 5088 | `{` |
|      - | 5089 | `	SyBlob *pPool;  /* Byte pool holding copied key + value bytes */` |
|      - | 5090 | `	SySet  *pTable; /* Set of strtr_entry (parallel offsets into pPool) */` |
|      - | 5091 | `	sxi32   rc;     /* Carries an allocation failure (SXERR_MEM) out of the walker */` |
|      - | 5092 | `	ph7_context *pCtx; /* Needed to warn about an empty key */` |
|      - | 5093 | `};` |
|      - | 5094 | `/*` |
|      - | 5095 | ` * Collect one replace_pairs entry into the persistent pool/offset table.` |
|      - | 5096 | ` * PHP coerces both the key and the value to string (an integer key becomes its` |
|      - | 5097 | ` * decimal form) and ignores an empty-string key.` |
|      - | 5098 | ` */` |
|     20 | 5099 | `static int StrtrCollectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 5100 | `{` |
|     21 | 5101 | `	strtr_collect *pCol = (strtr_collect *)pUserData;` |
|      - | 5102 | `	const char *zKey,*zVal;` |
|      - | 5103 | `	strtr_entry sEnt;` |
|      - | 5104 | `	int nKey,nVal;` |
|     21 | 5105 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     21 | 5106 | `	if( nKey < 1 ){` |
|      - | 5107 | `		/* PHP ignores an empty-string key, and warns that it did so. */` |
|      3 | 5108 | `		ph7_context_throw_error_format(pCol->pCtx,PH7_CTX_WARNING,` |
|      - | 5109 | `			"Ignoring replacement of empty string");` |
|      3 | 5110 | `		return PH7_OK;` |
|      - | 5111 | `	}` |
|     19 | 5112 | `	zVal = ph7_value_to_string(pData,&nVal);` |
|     19 | 5113 | `	sEnt.nKeyOfft = SyBlobLength(pCol->pPool);` |
|     19 | 5114 | `	sEnt.nKeyLen  = (sxu32)nKey;` |
|     19 | 5115 | `	if( SyBlobAppend(pCol->pPool,(const void *)zKey,(sxu32)nKey) != SXRET_OK ){` |
|    ! 0 | 5116 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 5117 | `		return SXERR_ABORT;` |
|      - | 5118 | `	}` |
|     19 | 5119 | `	sEnt.nValOfft = SyBlobLength(pCol->pPool);` |
|     19 | 5120 | `	sEnt.nValLen  = (sxu32)nVal;` |
|     19 | 5121 | `	if( nVal > 0 && SyBlobAppend(pCol->pPool,(const void *)zVal,(sxu32)nVal) != SXRET_OK ){` |
|    ! 0 | 5122 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 5123 | `		return SXERR_ABORT;` |
|      - | 5124 | `	}` |
|     19 | 5125 | `	if( SySetPut(pCol->pTable,(const void *)&sEnt) != SXRET_OK ){` |
|    ! 0 | 5126 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 5127 | `		return SXERR_ABORT;` |
|      - | 5128 | `	}` |
|     19 | 5129 | `	return PH7_OK;` |
|     11 | 5130 | `}` |
|      - | 5131 | `/*` |
|      - | 5132 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 5133 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 5134 | ` *  Translate characters or replace substrings.` |
|      - | 5135 | ` * Parameters` |
|      - | 5136 | ` *  $str` |
|      - | 5137 | ` *  The string being translated.` |
|      - | 5138 | ` * $from` |
|      - | 5139 | ` *  The string being translated to to.` |
|      - | 5140 | ` * $to` |
|      - | 5141 | ` *  The string replacing from.` |
|      - | 5142 | ` * $replace_pairs` |
|      - | 5143 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 5144 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 5145 | ` * Return` |
|      - | 5146 | ` *  The translated string.` |
|      - | 5147 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 5148 | ` */` |
|     12 | 5149 | `PH7_PRIVATE int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5150 | `{` |
|      - | 5151 | `	const char *zIn;` |
|      - | 5152 | `	int nLen;` |
|     13 | 5153 | `	if( nArg < 1 ){` |
|      - | 5154 | `		/* Nothing to replace,return FALSE */` |
|    ! 0 | 5155 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5156 | `		return PH7_OK;` |
|      - | 5157 | `	}` |
|     13 | 5158 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 5159 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 5160 | `		/* Invalid arguments */` |
|    ! 0 | 5161 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5162 | `		return PH7_OK;` |
|      - | 5163 | `	}` |
|     18 | 5164 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 5165 | `		strtr_collect sCol;` |
|      - | 5166 | `		SyBlob sPool,sWorker;` |
|      - | 5167 | `		SySet sTable;` |
|      - | 5168 | `		const char *zPool;` |
|      - | 5169 | `		strtr_entry *pEnt;` |
|      - | 5170 | `		sxi32 rc;` |
|      - | 5171 | `		int i,iRun;` |
|      - | 5172 | `		/*` |
|      - | 5173 | `		 * PHP's array-form strtr is a single left-to-right pass over the subject:` |
|      - | 5174 | `		 * at every position it substitutes the LONGEST replace_pairs key that` |
|      - | 5175 | `		 * matches there, then advances past the key (replacements are never` |
|      - | 5176 | `		 * rescanned). It is not a sequential per-key global replace. First copy` |
|      - | 5177 | `		 * the pairs into a persistent pool, then run that scan.` |
|      - | 5178 | `		 */` |
|     11 | 5179 | `		SyBlobInit(&sPool,&pCtx->pVm->sAllocator);` |
|     11 | 5180 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     11 | 5181 | `		SySetInit(&sTable,&pCtx->pVm->sAllocator,sizeof(strtr_entry));` |
|     11 | 5182 | `		sCol.pPool  = &sPool;` |
|     11 | 5183 | `		sCol.pTable = &sTable;` |
|     11 | 5184 | `		sCol.rc     = SXRET_OK;` |
|     11 | 5185 | `		sCol.pCtx   = pCtx;` |
|     11 | 5186 | `		ph7_array_walk(apArg[1],StrtrCollectWalker,&sCol);` |
|     11 | 5187 | `		if( sCol.rc != SXRET_OK ){` |
|      - | 5188 | `			/* Allocation failure while collecting the pairs: surface a fatal */` |
|    ! 0 | 5189 | `			SyBlobRelease(&sPool);` |
|    ! 0 | 5190 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 5191 | `			SySetRelease(&sTable);` |
|    ! 0 | 5192 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 5193 | `		}` |
|      - | 5194 | `		/* The pool is now stable, so offsets can be resolved against its base. */` |
|     11 | 5195 | `		zPool = (const char *)SyBlobData(&sPool);` |
|     11 | 5196 | `		rc = SXRET_OK;` |
|     11 | 5197 | `		iRun = 0; /* Start of the pending run of unmatched bytes copied verbatim. */` |
|     43 | 5198 | `		for( i = 0 ; i < nLen ; ){` |
|     33 | 5199 | `			strtr_entry *pBest = 0;` |
|     33 | 5200 | `			sxu32 nBest = 0;` |
|      - | 5201 | `			/* Pick the longest key that matches at the current position. */` |
|     33 | 5202 | `			SySetResetCursor(&sTable);` |
|     87 | 5203 | `			while( SXRET_OK == SySetGetNextEntry(&sTable,(void **)&pEnt) ){` |
|     54 | 5204 | `				if( pEnt->nKeyLen > nBest` |
|     50 | 5205 | `					&& pEnt->nKeyLen <= (sxu32)(nLen - i)` |
|     46 | 5206 | `					&& SyMemcmp(zPool + pEnt->nKeyOfft,zIn + i,pEnt->nKeyLen) == 0 ){` |
|     29 | 5207 | `					nBest = pEnt->nKeyLen;` |
|     29 | 5208 | `					pBest = pEnt;` |
|     14 | 5209 | `				}` |
|      1 | 5210 | `			}` |
|     33 | 5211 | `			if( pBest == 0 ){` |
|      - | 5212 | `				/* No key here: extend the literal run and copy it in one shot later. */` |
|      9 | 5213 | `				i++;` |
|      9 | 5214 | `				continue;` |
|      - | 5215 | `			}` |
|      - | 5216 | `			/* Flush the pending literal run, then the replacement. */` |
|     25 | 5217 | `			if( i > iRun ){` |
|      5 | 5218 | `				rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(i - iRun));` |
|      2 | 5219 | `			}` |
|     25 | 5220 | `			if( rc == SXRET_OK && pBest->nValLen > 0 ){` |
|     25 | 5221 | `				rc = SyBlobAppend(&sWorker,zPool + pBest->nValOfft,pBest->nValLen);` |
|     12 | 5222 | `			}` |
|     25 | 5223 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 5224 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 5225 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 5226 | `				SySetRelease(&sTable);` |
|    ! 0 | 5227 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 5228 | `			}` |
|     25 | 5229 | `			i += (int)pBest->nKeyLen;` |
|     25 | 5230 | `			iRun = i;` |
|      1 | 5231 | `		}` |
|      - | 5232 | `		/* Flush the trailing literal run. */` |
|     11 | 5233 | `		if( nLen > iRun ){` |
|      3 | 5234 | `			rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(nLen - iRun));` |
|      3 | 5235 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 5236 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 5237 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 5238 | `				SySetRelease(&sTable);` |
|    ! 0 | 5239 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 5240 | `			}` |
|      1 | 5241 | `		}` |
|      - | 5242 | `		/* All done, return the result string */` |
|     16 | 5243 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|     10 | 5244 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 5245 | `		/* Clean-up */` |
|     11 | 5246 | `		SyBlobRelease(&sPool);` |
|     11 | 5247 | `		SyBlobRelease(&sWorker);` |
|     11 | 5248 | `		SySetRelease(&sTable);` |
|     11 | 5249 | `		if( rc != PH7_OK ){` |
|    ! 0 | 5250 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 5251 | `		}` |
|      6 | 5252 | `	}else{` |
|      - | 5253 | `		int i,flen,tlen,c,iOfft;` |
|      - | 5254 | `		const char *zFrom,*zTo;` |
|      3 | 5255 | `		if( nArg < 3 ){` |
|      - | 5256 | `			/* Nothing to replace */` |
|    ! 0 | 5257 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5258 | `			return PH7_OK;` |
|      - | 5259 | `		}` |
|      - | 5260 | `		/* Extract given arguments */` |
|      3 | 5261 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 5262 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 5263 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 5264 | `			/* Nothing to replace */` |
|    ! 0 | 5265 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 5266 | `			return PH7_OK;` |
|      - | 5267 | `		}` |
|      - | 5268 | `		/* Start the replace process */` |
|     13 | 5269 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 5270 | `			c = zIn[i];` |
|     11 | 5271 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 5272 | `				if ( iOfft < tlen ){` |
|      5 | 5273 | `					c = zTo[iOfft];` |
|      2 | 5274 | `				}` |
|      2 | 5275 | `			}` |
|     11 | 5276 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 5277 |  |
|      6 | 5278 | `		}` |
|      - | 5279 | `	}` |
|     13 | 5280 | `	return PH7_OK;` |
|      7 | 5281 | `}` |
|      - | 5282 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 5283 |  |
