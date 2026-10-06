# src/ph7/builtin_string.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2920/3336 lines (87.53%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `#include <stdlib.h>  /* strtod */` |
|       - |    8 | `#include <math.h>    /* HUGE_VAL */` |
|       - |    9 | `#include <errno.h>   /* ERANGE (strtod range-error signal) */` |
|       - |   10 | `#include <stdio.h>   /* snprintf (printf-family float conversions — correctly` |
|       - |   11 | `                      * rounded shortest-representation output) */` |
|       - |   12 | `/*` |
|       - |   13 | ` * Section:` |
|       - |   14 | ` *    String handling functions.` |
|       - |   15 | ` * Status:` |
|       - |   16 | ` *    Stable.` |
|       - |   17 | ` */` |
|       - |   18 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|       - |   19 | `#define PH7_NEED_BUILTIN_REG 1` |
|       - |   20 | `#endif` |
|       - |   21 | `#ifndef PH7_DISABLE_DISK_IO` |
|       - |   22 | `#define PH7_NEED_FMT_AND_INI 1` |
|       - |   23 | `#endif` |
|       - |   24 | `#ifdef PH7_NEED_BUILTIN_REG` |
|       - |   25 | `/* Forward decl: null-to-string ZPP deprecation notice (defined near the ZPP` |
|       - |   26 | ` * helpers; both live inside the same DISABLE_BUILTIN_FUNC region as every` |
|       - |   27 | ` * caller — the tiny build compiles none of them). */` |
|       - |   28 | `static void StrNullArgNotice(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNum,const char *zParamName);` |
|       - |   29 | `/*` |
|       - |   30 | ` * Section:` |
|       - |   31 | ` *    String handling Functions.` |
|       - |   32 | ` * Status:` |
|       - |   33 | ` *    Stable.` |
|       - |   34 | ` */` |
|       - |   35 | `/*` |
|       - |   36 | ` * string substr(string $string,int $start[, int $length ])` |
|       - |   37 | ` *  Return part of a string.` |
|       - |   38 | ` * Parameters` |
|       - |   39 | ` *  $string` |
|       - |   40 | ` *   The input string. Must be one character or longer.` |
|       - |   41 | ` * $start` |
|       - |   42 | ` *   If start is non-negative, the returned string will start at the start'th position` |
|       - |   43 | ` *   in string, counting from zero. For instance, in the string 'abcdef', the character` |
|       - |   44 | ` *   at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|       - |   45 | ` *   If start is negative, the returned string will start at the start'th character` |
|       - |   46 | ` *   from the end of string.` |
|       - |   47 | ` *   If string is less than or equal to start characters long, FALSE will be returned.` |
|       - |   48 | ` * $length` |
|       - |   49 | ` *   If length is given and is positive, the string returned will contain at most length` |
|       - |   50 | ` *   characters beginning from start (depending on the length of string).` |
|       - |   51 | ` *   If length is given and is negative, then that many characters will be omitted from` |
|       - |   52 | ` *   the end of string (after the start position has been calculated when a start is negative).` |
|       - |   53 | ` *   If start denotes the position of this truncation or beyond, false will be returned.` |
|       - |   54 | ` *   If length is given and is 0, FALSE or NULL an empty string will be returned.` |
|       - |   55 | ` *   If length is omitted, the substring starting from start until the end of the string` |
|       - |   56 | ` *   will be returned.` |
|       - |   57 | ` * Return` |
|       - |   58 | ` *  Returns the extracted part of string, or FALSE on failure or an empty string.` |
|       - |   59 | ` */` |
|  776464 |   60 | `PH7_PRIVATE int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |   61 | `{` |
|       - |   62 | `	const char *zSource;` |
|       - |   63 | `	int nSrcLen;` |
|       - |   64 | `	sxi64 iStart,iEnd;` |
|  776469 |   65 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"substr",1,"$string"); }` |
|  776469 |   66 | `	if( nArg < 2 ){` |
|       - |   67 | `		/* Arity is enforced at the call boundary; nothing sensible to return here. */` |
|     ! 0 |   68 | `		ph7_result_string(pCtx,"",0);` |
|     ! 0 |   69 | `		return PH7_OK;` |
|       - |   70 | `	}` |
|       - |   71 | `	/* Extract the target string */` |
|  776469 |   72 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|       - |   73 | `	/* Extract the offset */` |
|       - |   74 | `	{` |
|  776469 |   75 | `		sxi64 iTmp = 0;` |
|  776469 |   76 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"substr",2,"$offset","int",&iTmp);` |
|  776469 |   77 | `		if( rcArg != PH7_OK ){` |
|     ! 0 |   78 | `			return rcArg;` |
|       - |   79 | `		}` |
|  776469 |   80 | `		iStart = iTmp;` |
|       - |   81 | `	}` |
|       - |   82 | `	/*` |
|       - |   83 | `	 * php 8 never answers substr() with FALSE — every out-of-range window simply` |
|       - |   84 | `	 * clamps to the empty string (substr("",0), substr("abc",5) and` |
|       - |   85 | `	 * substr("abc",1,-5) are all ""). PH7 returned FALSE for each of those, which` |
|       - |   86 | `	 * then flowed on as a bool into string context.` |
|       - |   87 | `	 *` |
|       - |   88 | `	 * A negative offset counts back from the end (clamped to 0); a negative length` |
|       - |   89 | `	 * leaves that many bytes off the end. Computed in sxi64 so an INT64 offset or` |
|       - |   90 | `	 * length cannot overflow the window arithmetic.` |
|       - |   91 | `	 */` |
|  776469 |   92 | `	if( iStart < 0 ){` |
|   53996 |   93 | `		iStart += nSrcLen;` |
|   53996 |   94 | `		if( iStart < 0 ){` |
|       5 |   95 | `			iStart = 0;` |
|       7 |   96 | `		}` |
|  749473 |   97 | `	}else if( iStart > nSrcLen ){` |
|       7 |   98 | `		iStart = nSrcLen;` |
|       3 |   99 | `	}` |
|  776469 |  100 | `	iEnd = nSrcLen;` |
|  776469 |  101 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|  612015 |  102 | `		sxi64 iLen = 0;` |
|  612015 |  103 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],"substr",3,"$length","?int",&iLen);` |
|  612015 |  104 | `		if( rcArg != PH7_OK ){` |
|     ! 0 |  105 | `			return rcArg;` |
|       - |  106 | `		}` |
|  612015 |  107 | `		if( iLen < 0 ){` |
|   51455 |  108 | `			iEnd = (sxi64)nSrcLen + iLen;` |
|  586290 |  109 | `		}else if( iLen > (sxi64)nSrcLen - iStart ){` |
|   46137 |  110 | `			iEnd = nSrcLen;` |
|   23071 |  111 | `		}else{` |
|  514433 |  112 | `			iEnd = iStart + iLen;` |
|       - |  113 | `		}` |
|  306177 |  114 | `	}` |
|  776469 |  115 | `	if( iEnd < iStart ){` |
|      10 |  116 | `		iEnd = iStart;` |
|       4 |  117 | `	}` |
|  776469 |  118 | `	ph7_result_string(pCtx,&zSource[iStart],(int)(iEnd - iStart));` |
|  776469 |  119 | `	return PH7_OK;` |
|  389097 |  120 | `}` |
|       - |  121 | `/*` |
|       - |  122 | ` * int substr_compare(string $main_str,string $str ,int $offset[,int $length[,bool $case_insensitivity = false ]])` |
|       - |  123 | ` *  Binary safe comparison of two strings from an offset, up to length characters.` |
|       - |  124 | ` * Parameters` |
|       - |  125 | ` *  $main_str` |
|       - |  126 | ` *  The main string being compared.` |
|       - |  127 | ` *  $str` |
|       - |  128 | ` *   The secondary string being compared.` |
|       - |  129 | ` * $offset` |
|       - |  130 | ` *  The start position for the comparison. If negative, it starts counting from` |
|       - |  131 | ` *  the end of the string.` |
|       - |  132 | ` * $length` |
|       - |  133 | ` *  The length of the comparison. The default value is the largest of the length` |
|       - |  134 | ` *  of the str compared to the length of main_str less the offset.` |
|       - |  135 | ` * $case_insensitivity` |
|       - |  136 | ` *  If case_insensitivity is TRUE, comparison is case insensitive.` |
|       - |  137 | ` * Return` |
|       - |  138 | ` *  Returns < 0 if main_str from position offset is less than str, > 0 if it is greater than` |
|       - |  139 | ` *  str, and 0 if they are equal. If offset is equal to or greater than the length of main_str` |
|       - |  140 | ` *  or length is set and is less than 1, substr_compare() prints a warning and returns FALSE.` |
|       - |  141 | ` */` |
|      22 |  142 | `PH7_PRIVATE int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  143 | `{` |
|       - |  144 | `	const char *zSource,*zSub;` |
|       - |  145 | `	int nSrcLen,nSubLen;` |
|       - |  146 | `	sxi64 iOfft,iLen,l1,l2,nCmp;` |
|      23 |  147 | `	int iCase = 0;` |
|       - |  148 | `	int rc;` |
|      23 |  149 | `	if( nArg < 3 ){` |
|     ! 0 |  150 | `		ph7_result_int(pCtx,0);` |
|     ! 0 |  151 | `		return PH7_OK;` |
|       - |  152 | `	}` |
|      23 |  153 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|      23 |  154 | `	zSub    = ph7_value_to_string(apArg[1],&nSubLen);` |
|       - |  155 | `	{` |
|      23 |  156 | `		sxi64 iTmp = 0;` |
|      23 |  157 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],"substr_compare",3,"$offset","int",&iTmp);` |
|      23 |  158 | `		if( rcArg != PH7_OK ){` |
|     ! 0 |  159 | `			return rcArg;` |
|       - |  160 | `		}` |
|      23 |  161 | `		iOfft = iTmp;` |
|       - |  162 | `	}` |
|      23 |  163 | `	if( iOfft < 0 ){` |
|       5 |  164 | `		iOfft += nSrcLen;` |
|       5 |  165 | `		if( iOfft < 0 ){` |
|       3 |  166 | `			iOfft = 0;` |
|       1 |  167 | `		}` |
|       2 |  168 | `	}` |
|      23 |  169 | `	if( iOfft > nSrcLen ){` |
|       - |  170 | `		/* php rejects an offset past the end of the haystack outright */` |
|     ! 0 |  171 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  172 | `			"substr_compare(): Argument #3 ($offset) must be contained in argument #1 ($haystack)");` |
|       - |  173 | `	}` |
|       - |  174 | `	/* A NULL/absent length compares as far as the longer of the two operands reaches */` |
|      23 |  175 | `	iLen = (sxi64)nSrcLen - iOfft;` |
|      23 |  176 | `	if( iLen < nSubLen ){` |
|       7 |  177 | `		iLen = nSubLen;` |
|       3 |  178 | `	}` |
|      23 |  179 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|      13 |  180 | `		sxi64 iTmp = 0;` |
|      13 |  181 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[3],"substr_compare",4,"$length","?int",&iTmp);` |
|      13 |  182 | `		if( rcArg != PH7_OK ){` |
|     ! 0 |  183 | `			return rcArg;` |
|       - |  184 | `		}` |
|      13 |  185 | `		if( iTmp < 0 ){` |
|       3 |  186 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  187 | `				"substr_compare(): Argument #4 ($length) must be greater than or equal to 0");` |
|       - |  188 | `		}` |
|      11 |  189 | `		iLen = iTmp;` |
|       5 |  190 | `	}` |
|      21 |  191 | `	if( nArg > 4 ){` |
|       5 |  192 | `		iCase = ph7_value_to_bool(apArg[4]);` |
|       2 |  193 | `	}` |
|       - |  194 | `	/* Each side contributes at most what it actually has left */` |
|      21 |  195 | `	l1 = (sxi64)nSrcLen - iOfft;` |
|      21 |  196 | `	if( l1 > iLen ){ l1 = iLen; }` |
|      21 |  197 | `	l2 = nSubLen;` |
|      21 |  198 | `	if( l2 > iLen ){ l2 = iLen; }` |
|      21 |  199 | `	nCmp = (l1 < l2) ? l1 : l2;` |
|      21 |  200 | `	if( iCase ){` |
|       3 |  201 | `		rc = SyStrnicmp(&zSource[iOfft],zSub,(sxu32)nCmp);` |
|       2 |  202 | `	}else{` |
|      19 |  203 | `		rc = SyStrncmp(&zSource[iOfft],zSub,(sxu32)nCmp);` |
|       - |  204 | `	}` |
|      21 |  205 | `	if( rc == 0 ){` |
|       - |  206 | `		/* Prefixes equal: php falls back to a THREE-WAY compare of the lengths, so this` |
|       - |  207 | `		 * arm is normalized to -1/0/1 (substr_compare("abc","",0) is 1, not 3). */` |
|      15 |  208 | `		rc = (l1 == l2) ? 0 : (l1 < l2 ? -1 : 1);` |
|       7 |  209 | `	}` |
|       - |  210 | `	/* ...but when the prefixes differ php returns the RAW byte difference, not its sign:` |
|       - |  211 | `	 * substr_compare("abc","def",1,10) is -2 ('b' - 'd'), which is what SyMemcmp gives. */` |
|      21 |  212 | `	ph7_result_int(pCtx,rc);` |
|      21 |  213 | `	return PH7_OK;` |
|      12 |  214 | `}` |
|       - |  215 | `/*` |
|       - |  216 | ` * int substr_count(string $haystack,string $needle[,int $offset = 0 [,int $length ]])` |
|       - |  217 | ` *  Count the number of substring occurrences.` |
|       - |  218 | ` * Parameters` |
|       - |  219 | ` * $haystack` |
|       - |  220 | ` *   The string to search in` |
|       - |  221 | ` * $needle` |
|       - |  222 | ` *   The substring to search for` |
|       - |  223 | ` * $offset` |
|       - |  224 | ` *  The offset where to start counting` |
|       - |  225 | ` * $length (NOT USED)` |
|       - |  226 | ` *  The maximum length after the specified offset to search for the substring.` |
|       - |  227 | ` *  It outputs a warning if the offset plus the length is greater than the haystack length.` |
|       - |  228 | ` * Return` |
|       - |  229 | ` *  Toral number of substring occurrences.` |
|       - |  230 | ` */` |
|     154 |  231 | `PH7_PRIVATE int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 |  232 | `{` |
|       - |  233 | `	const char *zText,*zPattern,*zEnd;` |
|       - |  234 | `	int nTextlen,nPatlen;` |
|     157 |  235 | `	int iCount = 0;` |
|       - |  236 | `	sxu32 nOfft;` |
|       - |  237 | `	sxi32 rc;` |
|     157 |  238 | `	if( nArg < 2 ){` |
|       - |  239 | `		/* Missing arguments */` |
|     ! 0 |  240 | `		ph7_result_int(pCtx,0);` |
|     ! 0 |  241 | `		return PH7_OK;` |
|       - |  242 | `	}` |
|       - |  243 | `	/* Point to the haystack */` |
|     157 |  244 | `	zText = ph7_value_to_string(apArg[0],&nTextlen);` |
|       - |  245 | `	/* Point to the neddle */` |
|     157 |  246 | `	zPattern = ph7_value_to_string(apArg[1],&nPatlen);` |
|     157 |  247 | `	if( nPatlen < 1 ){` |
|       - |  248 | `		/* Empty needle: PHP 8 throws a catchable ValueError. */` |
|       3 |  249 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  250 | `			"substr_count(): Argument #2 ($needle) must not be empty");` |
|       - |  251 | `	}` |
|       - |  252 | `	/* Apply the optional $offset/$length window before searching. PHP 8 validates` |
|       - |  253 | `	 * both against the haystack (a negative value counts from the end) and throws a` |
|       - |  254 | `	 * catchable ValueError when the result falls outside it — this happens before the` |
|       - |  255 | `	 * needle-fits check, so it fires even when the needle is longer than the haystack. */` |
|     155 |  256 | `	if( nArg > 2 ){` |
|      19 |  257 | `		ph7_int64 iOfft = ph7_value_to_int64(apArg[2]);` |
|      19 |  258 | `		if( iOfft < 0 ){` |
|       5 |  259 | `			iOfft += nTextlen;` |
|       2 |  260 | `		}` |
|      19 |  261 | `		if( iOfft < 0 \|\| iOfft > nTextlen ){` |
|       3 |  262 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  263 | `				"substr_count(): Argument #3 ($offset) must be contained in argument #1 ($haystack)");` |
|       - |  264 | `		}` |
|       - |  265 | `		/* Point to the desired offset and shrink the remaining region */` |
|      17 |  266 | `		zText = &zText[iOfft];` |
|      17 |  267 | `		nTextlen -= (int)iOfft;` |
|       8 |  268 | `	}` |
|     153 |  269 | `	if( nArg > 3 ){` |
|      15 |  270 | `		ph7_int64 nLen = ph7_value_to_int64(apArg[3]);` |
|      15 |  271 | `		if( nLen < 0 ){` |
|       - |  272 | `			/* Negative length is relative to the end of the (offset) haystack */` |
|       5 |  273 | `			nLen += nTextlen;` |
|       2 |  274 | `		}` |
|      15 |  275 | `		if( nLen < 0 \|\| nLen > nTextlen ){` |
|       5 |  276 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  277 | `				"substr_count(): Argument #4 ($length) must be contained in argument #1 ($haystack)");` |
|       - |  278 | `		}` |
|      11 |  279 | `		nTextlen = (int)nLen;` |
|       5 |  280 | `	}` |
|     149 |  281 | `	if( nTextlen < 1 \|\| nPatlen > nTextlen ){` |
|       - |  282 | `		/* The windowed haystack can't contain the needle: zero matches */` |
|       3 |  283 | `		ph7_result_int(pCtx,0);` |
|       3 |  284 | `		return PH7_OK;` |
|       - |  285 | `	}` |
|       - |  286 | `	/* Point to the end of the windowed haystack */` |
|     147 |  287 | `	zEnd = &zText[nTextlen];` |
|       - |  288 | `	/* Perform the search */` |
|     238 |  289 | `	for(;;){` |
|     479 |  290 | `		rc = SyBlobSearch((const void *)zText,(sxu32)(zEnd-zText),(const void *)zPattern,nPatlen,&nOfft);` |
|     479 |  291 | `		if( rc != SXRET_OK ){` |
|       - |  292 | `			/* Pattern not found,break immediately */` |
|     118 |  293 | `			break;` |
|       - |  294 | `		}` |
|       - |  295 | `		/* Increment counter and update the offset */` |
|     363 |  296 | `		iCount++;` |
|     363 |  297 | `		zText += nOfft + nPatlen;` |
|     363 |  298 | `		if( zText >= zEnd ){` |
|      31 |  299 | `			break;` |
|       - |  300 | `		}` |
|       3 |  301 | `	}` |
|       - |  302 | `	/* Pattern count */` |
|     147 |  303 | `	ph7_result_int(pCtx,iCount);` |
|     147 |  304 | `	return PH7_OK;` |
|      80 |  305 | `}` |
|       - |  306 | `/* Forward declarations: defined with the trim/addcslashes and str_contains` |
|       - |  307 | ` * families below. */` |
|       - |  308 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256]);` |
|       - |  309 | `/*` |
|       - |  310 | ` * php 8.1 null-to-non-nullable ZPP deprecation, notice-only form for the` |
|       - |  311 | ` * legacy string builtins that still coerce null to "" themselves: emit` |
|       - |  312 | ``  * `f(): Passing null to parameter #N ($name) of type string is deprecated` `` |
|       - |  313 | ` * when the arg is an actual null, leaving the resolution unchanged.` |
|       - |  314 | ` */` |
|       - |  315 | `/* php only DEPRECATES passing null to a non-nullable string param; PHL targets php's` |
|       - |  316 | ` * non-deprecated surface and rejects it with a TypeError. Every caller of this helper` |
|       - |  317 | `` * now also carries a `string $…` row in the vm_arg_check.c signature table, so`` |
|       - |  318 | ` * VmEnforceBuiltinArgTypes raises that TypeError BEFORE the routine runs and this is a` |
|       - |  319 | ` * backstop rather than the live path. It stays correct either way: the throw records` |
|       - |  320 | ` * its status on the call context and the OP_CALL boundary (VmHostFuncThrowRc) reports` |
|       - |  321 | ` * it in place of the routine's own, so the call ABORTS as php's would without the` |
|       - |  322 | ` * callers threading a status back. */` |
| 5596952 |  323 | `static void StrNullArgNotice(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNum,const char *zParamName)` |
|       5 |  324 | `{` |
| 5596957 |  325 | `	if( ph7_value_is_null(pArg) ){` |
|     ! 0 |  326 | `		PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  327 | `			"%s(): Argument #%d (%s) must be of type string, null given",` |
|     ! 0 |  328 | `			zFunc,iArgNum,zParamName);` |
|     ! 0 |  329 | `	}` |
| 5596957 |  330 | `}` |
|       - |  331 | `static sxi32 StrPredicateResolveArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,` |
|       - |  332 | `	int iArgNum,const char *zParamName,const char *zTypeStr,const char *zNullMsg,` |
|       - |  333 | `	ph7_value *pTmp,const char **pzOut,int *pnOut);` |
|       - |  334 | `/*` |
|       - |  335 | ` * Validate and resolve an int-typed builtin parameter with php-8 ZPP weak-mode` |
|       - |  336 | ` * semantics: ints and bools pass through; null emits the 8.1 deprecation and` |
|       - |  337 | ` * resolves to 0; floats and float-strings convert, with the implicit-conversion` |
|       - |  338 | ` * E_DEPRECATED when lossy and a TypeError when NAN/INF/out of int range;` |
|       - |  339 | ` * integral numeric strings convert exactly; everything else (arrays, resources,` |
|       - |  340 | ` * objects, non-numeric strings) is a TypeError naming zTypeStr (e.g. "int",` |
|       - |  341 | ` * "array\|int"). Returns PH7_OK with *pOut set, or the throw status.` |
|       - |  342 | ` */` |
|       - |  343 | `/*` |
|       - |  344 | ` * Normalize a substr_replace() offset/length pair against a string of nStrLen` |
|       - |  345 | ` * bytes, exactly like PHP: a negative offset counts from the end (clamped to 0),` |
|       - |  346 | ` * an offset past the end clamps to the end; a negative length leaves that many` |
|       - |  347 | ` * bytes off the end of the remaining region (clamped to 0), and the length is` |
|       - |  348 | ` * finally clamped to the remaining region. Written without f+l additions so an` |
|       - |  349 | ` * INT64_MAX length cannot overflow.` |
|       - |  350 | ` */` |
|      60 |  351 | `static void SubstrReplaceWindow(sxi64 *pF,sxi64 *pL,int nStrLen)` |
|       1 |  352 | `{` |
|      61 |  353 | `	sxi64 f = *pF,l = *pL;` |
|      61 |  354 | `	if( f < 0 ){` |
|       9 |  355 | `		f += nStrLen;` |
|       9 |  356 | `		if( f < 0 ){` |
|       5 |  357 | `			f = 0;` |
|       3 |  358 | `		}` |
|      57 |  359 | `	}else if( f > nStrLen ){` |
|       5 |  360 | `		f = nStrLen;` |
|       2 |  361 | `	}` |
|      61 |  362 | `	if( l < 0 ){` |
|       7 |  363 | `		l += nStrLen - f;` |
|       7 |  364 | `		if( l < 0 ){` |
|       5 |  365 | `			l = 0;` |
|       2 |  366 | `		}` |
|       3 |  367 | `	}` |
|      61 |  368 | `	if( l > nStrLen - f ){` |
|      25 |  369 | `		l = nStrLen - f;` |
|      12 |  370 | `	}` |
|      61 |  371 | `	*pF = f;` |
|      61 |  372 | `	*pL = l;` |
|      61 |  373 | `}` |
|       - |  374 | `/* A replacement string collected out of substr_replace()'s $replace array.` |
|       - |  375 | ` * The bytes live in a shared pool blob (walker values are transient), so the` |
|       - |  376 | ` * item stores pool offsets, mirroring the strtr_entry technique. */` |
|       - |  377 | `typedef struct substr_repl_item substr_repl_item;` |
|       - |  378 | `struct substr_repl_item` |
|       - |  379 | `{` |
|       - |  380 | `	sxu32 nOfft; /* Offset of the string inside the pool */` |
|       - |  381 | `	sxu32 nLen;  /* Length of the string */` |
|       - |  382 | `};` |
|       - |  383 | `typedef struct substr_replace_collect substr_replace_collect;` |
|       - |  384 | `struct substr_replace_collect` |
|       - |  385 | `{` |
|       - |  386 | `	SyBlob *pPool;  /* Byte pool for string items (string walker only) */` |
|       - |  387 | `	SySet *pSet;    /* substr_repl_item set (string) or sxi64 set (int) */` |
|       - |  388 | `	sxi32 rc;       /* SXRET_OK or SXERR_MEM on collector failure */` |
|       - |  389 | `};` |
|       - |  390 | `/* ph7_array_walk() callback: append one $replace element to the pool. */` |
|       6 |  391 | `static int SubstrReplaceStrWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|       1 |  392 | `{` |
|       7 |  393 | `	substr_replace_collect *pCol = (substr_replace_collect *)pUserData;` |
|       - |  394 | `	substr_repl_item sItem;` |
|       - |  395 | `	const char *zStr;` |
|       - |  396 | `	int nLen;` |
|       3 |  397 | `	SXUNUSED(pKey);` |
|       7 |  398 | `	zStr = ph7_value_to_string(pData,&nLen);` |
|       7 |  399 | `	sItem.nOfft = SyBlobLength(pCol->pPool);` |
|       7 |  400 | `	sItem.nLen = (sxu32)nLen;` |
|       7 |  401 | `	if( nLen > 0 && SXRET_OK != SyBlobAppend(pCol->pPool,(const void *)zStr,(sxu32)nLen) ){` |
|     ! 0 |  402 | `		pCol->rc = SXERR_MEM;` |
|     ! 0 |  403 | `		return SXERR_ABORT;` |
|       - |  404 | `	}` |
|       7 |  405 | `	if( SXRET_OK != SySetPut(pCol->pSet,(const void *)&sItem) ){` |
|     ! 0 |  406 | `		pCol->rc = SXERR_MEM;` |
|     ! 0 |  407 | `		return SXERR_ABORT;` |
|       - |  408 | `	}` |
|       7 |  409 | `	return PH7_OK;` |
|       4 |  410 | `}` |
|       - |  411 | `/* ph7_array_walk() callback: collect one $offset/$length element as an int. */` |
|      12 |  412 | `static int SubstrReplaceIntWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|       1 |  413 | `{` |
|      13 |  414 | `	substr_replace_collect *pCol = (substr_replace_collect *)pUserData;` |
|      13 |  415 | `	sxi64 iVal = ph7_value_to_int64(pData);` |
|       6 |  416 | `	SXUNUSED(pKey);` |
|      13 |  417 | `	if( SXRET_OK != SySetPut(pCol->pSet,(const void *)&iVal) ){` |
|     ! 0 |  418 | `		pCol->rc = SXERR_MEM;` |
|     ! 0 |  419 | `		return SXERR_ABORT;` |
|       - |  420 | `	}` |
|      13 |  421 | `	return PH7_OK;` |
|       7 |  422 | `}` |
|       - |  423 | `/* Per-element state while walking substr_replace()'s array $string. */` |
|       - |  424 | `typedef struct substr_replace_ctx substr_replace_ctx;` |
|       - |  425 | `struct substr_replace_ctx` |
|       - |  426 | `{` |
|       - |  427 | `	ph7_value *pResult;   /* Result array (keys preserved) */` |
|       - |  428 | `	ph7_value *pScratch;  /* Reusable string value for each element */` |
|       - |  429 | `	SyBlob *pReplPool;    /* Pool behind aRepl items */` |
|       - |  430 | `	SySet *pRepl;         /* substr_repl_item set or NULL when $replace is scalar */` |
|       - |  431 | `	SySet *pFrom;         /* sxi64 set or NULL when $offset is scalar */` |
|       - |  432 | `	SySet *pLen;          /* sxi64 set or NULL when $length is scalar/absent */` |
|       - |  433 | `	sxu32 iReplCur;       /* Next-position cursors into the three sets */` |
|       - |  434 | `	sxu32 iFromCur;` |
|       - |  435 | `	sxu32 iLenCur;` |
|       - |  436 | `	const char *zRepl;    /* Scalar $replace */` |
|       - |  437 | `	int nRepl;` |
|       - |  438 | `	sxi64 iFrom;          /* Scalar $offset */` |
|       - |  439 | `	sxi64 iLen;           /* Scalar $length */` |
|       - |  440 | `	int bLenGiven;        /* FALSE: $length absent/null -> element length */` |
|       - |  441 | `	sxi32 rc;             /* SXRET_OK or SXERR_MEM */` |
|       - |  442 | `};` |
|       - |  443 | `/*` |
|       - |  444 | ` * ph7_array_walk() callback over the array $string: replace the window of one` |
|       - |  445 | ` * element and insert the result under the element's original key. Array-form` |
|       - |  446 | ` * $replace/$offset/$length are consumed positionally; when a set runs out PHP` |
|       - |  447 | ` * falls back to ""/0/element-length respectively.` |
|       - |  448 | ` */` |
|      24 |  449 | `static int SubstrReplaceElemWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|       1 |  450 | `{` |
|      25 |  451 | `	substr_replace_ctx *pRep = (substr_replace_ctx *)pUserData;` |
|       - |  452 | `	const char *zStr,*zRepl;` |
|       - |  453 | `	sxi64 f,l;` |
|       - |  454 | `	int nLen,nRepl;` |
|      25 |  455 | `	zStr = ph7_value_to_string(pData,&nLen);` |
|       - |  456 | `	/* Positional $replace element ("" when exhausted) */` |
|      25 |  457 | `	if( pRep->pRepl ){` |
|      11 |  458 | `		if( pRep->iReplCur < SySetUsed(pRep->pRepl) ){` |
|       7 |  459 | `			substr_repl_item *pItem = (substr_repl_item *)SySetAt(pRep->pRepl,pRep->iReplCur++);` |
|       7 |  460 | `			zRepl = (const char *)SyBlobDataAt(pRep->pReplPool,pItem->nOfft);` |
|       7 |  461 | `			nRepl = (int)pItem->nLen;` |
|       4 |  462 | `		}else{` |
|       5 |  463 | `			zRepl = "";` |
|       5 |  464 | `			nRepl = 0;` |
|       - |  465 | `		}` |
|       6 |  466 | `	}else{` |
|      15 |  467 | `		zRepl = pRep->zRepl;` |
|      15 |  468 | `		nRepl = pRep->nRepl;` |
|       - |  469 | `	}` |
|       - |  470 | `	/* Positional $offset element (0 when exhausted) */` |
|      25 |  471 | `	if( pRep->pFrom ){` |
|      13 |  472 | `		sxi64 *pVal = 0;` |
|      13 |  473 | `		if( pRep->iFromCur < SySetUsed(pRep->pFrom) ){` |
|       9 |  474 | `			pVal = (sxi64 *)SySetAt(pRep->pFrom,pRep->iFromCur++);` |
|       4 |  475 | `		}` |
|      13 |  476 | `		f = pVal ? *pVal : 0;` |
|       7 |  477 | `	}else{` |
|      13 |  478 | `		f = pRep->iFrom;` |
|       - |  479 | `	}` |
|       - |  480 | `	/* Positional $length element (element length when exhausted) */` |
|      25 |  481 | `	if( pRep->pLen ){` |
|       7 |  482 | `		sxi64 *pVal = 0;` |
|       7 |  483 | `		if( pRep->iLenCur < SySetUsed(pRep->pLen) ){` |
|       5 |  484 | `			pVal = (sxi64 *)SySetAt(pRep->pLen,pRep->iLenCur++);` |
|       2 |  485 | `		}` |
|       7 |  486 | `		l = pVal ? *pVal : nLen;` |
|       4 |  487 | `	}else{` |
|      19 |  488 | `		l = pRep->bLenGiven ? pRep->iLen : nLen;` |
|       - |  489 | `	}` |
|      25 |  490 | `	SubstrReplaceWindow(&f,&l,nLen);` |
|       - |  491 | `	/* Assemble prefix + replacement + suffix in the scratch value */` |
|      25 |  492 | `	ph7_value_reset_string_cursor(pRep->pScratch);` |
|      24 |  493 | `	if( (f > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,zStr,(int)f))` |
|      24 |  494 | `	 \|\| (nRepl > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,zRepl,nRepl))` |
|      40 |  495 | `	 \|\| (nLen - (int)(f+l) > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,&zStr[f+l],nLen - (int)(f+l))) ){` |
|      30 |  496 | `		pRep->rc = SXERR_MEM;` |
|      30 |  497 | `		return SXERR_ABORT;` |
|       - |  498 | `	}` |
|      25 |  499 | `	if( SXRET_OK != ph7_array_add_elem(pRep->pResult,pKey,pRep->pScratch) ){` |
|     ! 0 |  500 | `		pRep->rc = SXERR_MEM;` |
|     ! 0 |  501 | `		return SXERR_ABORT;` |
|       - |  502 | `	}` |
|      25 |  503 | `	return PH7_OK;` |
|      43 |  504 | `}` |
|       - |  505 | `/*` |
|       - |  506 | ` * mixed substr_replace(array\|string $string,array\|string $replace,array\|int $offset[,array\|int\|null $length = null])` |
|       - |  507 | ` *  Replace text within a portion of a string.` |
|       - |  508 | ` * Parameters` |
|       - |  509 | ` *  $string` |
|       - |  510 | ` *   The input string or an array of strings (each element is processed with` |
|       - |  511 | ` *   its own positional replace/offset/length when those are arrays too).` |
|       - |  512 | ` *  $replace` |
|       - |  513 | ` *   The replacement string. When $string is scalar and $replace is an array,` |
|       - |  514 | ` *   only its first element is used (PHP quirk).` |
|       - |  515 | ` *  $offset` |
|       - |  516 | ` *   Window start; negative counts from the end of the string.` |
|       - |  517 | ` *  $length` |
|       - |  518 | ` *   Window length; negative leaves that many bytes at the end; null/absent` |
|       - |  519 | ` *   means "to the end of the string".` |
|       - |  520 | ` * Return` |
|       - |  521 | ` *  The processed string, or an array of processed strings (keys preserved).` |
|       - |  522 | ` * Errors` |
|       - |  523 | ` *  ArgumentCountError on fewer than 3 arguments; TypeError when an array` |
|       - |  524 | ` *  $offset/$length is combined with a scalar $string.` |
|       - |  525 | ` */` |
|      58 |  526 | `PH7_PRIVATE int PH7_builtin_substr_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  527 | `{` |
|       - |  528 | `	ph7_value sStrTmp,sReplTmp;` |
|      59 |  529 | `	const char *zStr = 0,*zRepl = 0;` |
|      59 |  530 | `	int nLen = 0,nRepl = 0;` |
|       - |  531 | `	int bLenGiven;` |
|      59 |  532 | `	sxi64 f = 0,l = 0;` |
|       - |  533 | `	sxi32 rc;` |
|      59 |  534 | `	if( nArg < 3 ){` |
|     ! 0 |  535 | `		return PH7_VmThrowException(pCtx,` |
|       - |  536 | `			"ArgumentCountError",` |
|       - |  537 | `			"substr_replace() expects at least 3 arguments, %d given",` |
|     ! 0 |  538 | `			nArg` |
|       - |  539 | `			);` |
|       - |  540 | `	}` |
|       - |  541 | `	/* $length counts as given unless absent or null (php: ?null semantics) */` |
|      59 |  542 | `	bLenGiven = (nArg > 3 && !ph7_value_is_null(apArg[3]));` |
|       - |  543 | `	/* php ZPP validates all four args, in order, before the body runs: the` |
|       - |  544 | `	 * non-array forms resolve here (null deprecation, __toString objects,` |
|       - |  545 | `	 * numeric strings), arrays pass through to the per-mode handling. */` |
|      59 |  546 | `	PH7_MemObjInit(pCtx->pVm,&sStrTmp);` |
|      59 |  547 | `	PH7_MemObjInit(pCtx->pVm,&sReplTmp);` |
|      59 |  548 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      45 |  549 | `		rc = StrPredicateResolveArg(pCtx,apArg[0],"substr_replace",1,"$string","array\|string",` |
|       - |  550 | `			"substr_replace(): Passing null to parameter #1 ($string) "` |
|       - |  551 | `			"of type array\|string is deprecated",` |
|       - |  552 | `			&sStrTmp,&zStr,&nLen);` |
|      45 |  553 | `		if( rc != PH7_OK ) goto out;` |
|      22 |  554 | `	}` |
|      59 |  555 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      51 |  556 | `		rc = StrPredicateResolveArg(pCtx,apArg[1],"substr_replace",2,"$replace","array\|string",` |
|       - |  557 | `			"substr_replace(): Passing null to parameter #2 ($replace) "` |
|       - |  558 | `			"of type array\|string is deprecated",` |
|       - |  559 | `			&sReplTmp,&zRepl,&nRepl);` |
|      51 |  560 | `		if( rc != PH7_OK ) goto out;` |
|      25 |  561 | `	}` |
|      59 |  562 | `	if( !ph7_value_is_array(apArg[2]) ){` |
|      51 |  563 | `		rc = PH7_IntArgResolve(pCtx,apArg[2],"substr_replace",3,"$offset","array\|int",&f);` |
|      51 |  564 | `		if( rc != PH7_OK ) goto out;` |
|      24 |  565 | `	}` |
|      57 |  566 | `	if( bLenGiven && !ph7_value_is_array(apArg[3]) ){` |
|      31 |  567 | `		rc = PH7_IntArgResolve(pCtx,apArg[3],"substr_replace",4,"$length","array\|int\|null",&l);` |
|      31 |  568 | `		if( rc != PH7_OK ) goto out;` |
|      14 |  569 | `	}` |
|      55 |  570 | `	if( ph7_value_is_array(apArg[0]) ){` |
|       - |  571 | `		/* Array form: process each element, preserving keys */` |
|       - |  572 | `		substr_replace_ctx sRep;` |
|       - |  573 | `		substr_replace_collect sCol;` |
|       - |  574 | `		SyBlob sReplPool;` |
|       - |  575 | `		SySet sRepl,sFrom,sLen;` |
|       - |  576 | `		ph7_value *pResult,*pScratch;` |
|      15 |  577 | `		sxi32 rcWalk = SXRET_OK;` |
|      15 |  578 | `		SyBlobInit(&sReplPool,&pCtx->pVm->sAllocator);` |
|      15 |  579 | `		SySetInit(&sRepl,&pCtx->pVm->sAllocator,sizeof(substr_repl_item));` |
|      15 |  580 | `		SySetInit(&sFrom,&pCtx->pVm->sAllocator,sizeof(sxi64));` |
|      15 |  581 | `		SySetInit(&sLen,&pCtx->pVm->sAllocator,sizeof(sxi64));` |
|      15 |  582 | `		SyZero(&sRep,sizeof(substr_replace_ctx));` |
|      15 |  583 | `		sRep.bLenGiven = bLenGiven;` |
|      15 |  584 | `		sCol.rc = SXRET_OK;` |
|       - |  585 | `		/* Collect array-form $replace/$offset/$length positionally; the` |
|       - |  586 | `		 * scalar forms were already resolved above. */` |
|      15 |  587 | `		if( ph7_value_is_array(apArg[1]) ){` |
|       5 |  588 | `			sCol.pPool = &sReplPool;` |
|       5 |  589 | `			sCol.pSet = &sRepl;` |
|       5 |  590 | `			ph7_array_walk(apArg[1],SubstrReplaceStrWalker,&sCol);` |
|       5 |  591 | `			sRep.pRepl = &sRepl;` |
|       5 |  592 | `			sRep.pReplPool = &sReplPool;` |
|       3 |  593 | `		}else{` |
|      11 |  594 | `			sRep.zRepl = zRepl;` |
|      11 |  595 | `			sRep.nRepl = nRepl;` |
|       - |  596 | `		}` |
|      15 |  597 | `		if( sCol.rc == SXRET_OK && ph7_value_is_array(apArg[2]) ){` |
|       7 |  598 | `			sCol.pSet = &sFrom;` |
|       7 |  599 | `			ph7_array_walk(apArg[2],SubstrReplaceIntWalker,&sCol);` |
|       7 |  600 | `			sRep.pFrom = &sFrom;` |
|       4 |  601 | `		}else{` |
|       9 |  602 | `			sRep.iFrom = f;` |
|       - |  603 | `		}` |
|      15 |  604 | `		if( sCol.rc == SXRET_OK && bLenGiven ){` |
|       9 |  605 | `			if( ph7_value_is_array(apArg[3]) ){` |
|       5 |  606 | `				sCol.pSet = &sLen;` |
|       5 |  607 | `				ph7_array_walk(apArg[3],SubstrReplaceIntWalker,&sCol);` |
|       5 |  608 | `				sRep.pLen = &sLen;` |
|       3 |  609 | `			}else{` |
|       5 |  610 | `				sRep.iLen = l;` |
|       - |  611 | `			}` |
|       4 |  612 | `		}` |
|      15 |  613 | `		pResult = ph7_context_new_array(pCtx);` |
|      15 |  614 | `		pScratch = ph7_context_new_scalar(pCtx);` |
|      15 |  615 | `		if( sCol.rc != SXRET_OK \|\| pResult == 0 \|\| pScratch == 0 ){` |
|     ! 0 |  616 | `			rcWalk = SXERR_MEM;` |
|     ! 0 |  617 | `		}else{` |
|      15 |  618 | `			sRep.pResult = pResult;` |
|      15 |  619 | `			sRep.pScratch = pScratch;` |
|      15 |  620 | `			ph7_value_string(pScratch,"",0); /* Force string representation */` |
|      15 |  621 | `			ph7_array_walk(apArg[0],SubstrReplaceElemWalker,&sRep);` |
|      15 |  622 | `			rcWalk = sRep.rc;` |
|       - |  623 | `		}` |
|      15 |  624 | `		SyBlobRelease(&sReplPool);` |
|      15 |  625 | `		SySetRelease(&sRepl);` |
|      15 |  626 | `		SySetRelease(&sFrom);` |
|      15 |  627 | `		SySetRelease(&sLen);` |
|      15 |  628 | `		if( rcWalk != SXRET_OK ){` |
|     ! 0 |  629 | `			rc = PH7_ContextMemoryError(pCtx);` |
|     ! 0 |  630 | `			goto out;` |
|       - |  631 | `		}` |
|      15 |  632 | `		ph7_result_value(pCtx,pResult);` |
|      15 |  633 | `		rc = PH7_OK;` |
|      15 |  634 | `		goto out;` |
|       - |  635 | `	}` |
|       - |  636 | `	/* Scalar form: array $offset/$length are a TypeError, array $replace` |
|       - |  637 | `	 * degrades to its first element (php quirk). */` |
|      41 |  638 | `	if( ph7_value_is_array(apArg[2]) ){` |
|       3 |  639 | `		rc = PH7_VmThrowException(pCtx,` |
|       - |  640 | `			"TypeError",` |
|       - |  641 | `			"substr_replace(): Argument #3 ($offset) cannot be an array when working on a single string"` |
|       - |  642 | `			);` |
|       3 |  643 | `		goto out;` |
|       - |  644 | `	}` |
|      39 |  645 | `	if( bLenGiven && ph7_value_is_array(apArg[3]) ){` |
|       3 |  646 | `		rc = PH7_VmThrowException(pCtx,` |
|       - |  647 | `			"TypeError",` |
|       - |  648 | `			"substr_replace(): Argument #4 ($length) cannot be an array when working on a single string"` |
|       - |  649 | `			);` |
|       3 |  650 | `		goto out;` |
|       - |  651 | `	}` |
|      37 |  652 | `	if( ph7_value_is_array(apArg[1]) ){` |
|       - |  653 | `		/* First element of the replace array, or "" when empty */` |
|       5 |  654 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|       5 |  655 | `		zRepl = "";` |
|       5 |  656 | `		nRepl = 0;` |
|       5 |  657 | `		if( pMap->pFirst ){` |
|       3 |  658 | `			ph7_value *pVal = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj,pMap->pFirst->nValIdx);` |
|       3 |  659 | `			if( pVal ){` |
|       3 |  660 | `				zRepl = ph7_value_to_string(pVal,&nRepl);` |
|       1 |  661 | `			}` |
|       1 |  662 | `		}` |
|       2 |  663 | `	}` |
|      37 |  664 | `	if( !bLenGiven ){` |
|      15 |  665 | `		l = nLen;` |
|       7 |  666 | `	}` |
|      37 |  667 | `	SubstrReplaceWindow(&f,&l,nLen);` |
|       - |  668 | `	/* Assemble prefix + replacement + suffix straight into the call result` |
|       - |  669 | `	 * (ph7_result_string appends), no scratch buffer needed. */` |
|      37 |  670 | `	rc = SXRET_OK;` |
|      37 |  671 | `	if( f > 0 ){` |
|      29 |  672 | `		rc = ph7_result_string(pCtx,zStr,(int)f);` |
|      14 |  673 | `	}` |
|      37 |  674 | `	if( rc == SXRET_OK && nRepl > 0 ){` |
|      33 |  675 | `		rc = ph7_result_string(pCtx,zRepl,nRepl);` |
|      16 |  676 | `	}` |
|      37 |  677 | `	if( rc == SXRET_OK && nLen - (int)(f+l) > 0 ){` |
|      17 |  678 | `		rc = ph7_result_string(pCtx,&zStr[f+l],nLen - (int)(f+l));` |
|       8 |  679 | `	}` |
|      37 |  680 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  681 | `		rc = PH7_ContextMemoryError(pCtx);` |
|     ! 0 |  682 | `		goto out;` |
|       - |  683 | `	}` |
|       - |  684 | `	/* Force a string result even when all three segments are empty */` |
|      37 |  685 | `	rc = ph7_result_string(pCtx,"",0);` |
|      37 |  686 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  687 | `		rc = PH7_ContextMemoryError(pCtx);` |
|     ! 0 |  688 | `		goto out;` |
|       - |  689 | `	}` |
|      37 |  690 | `	rc = PH7_OK;` |
|      29 |  691 | `out:` |
|      59 |  692 | `	PH7_MemObjRelease(&sStrTmp);` |
|      59 |  693 | `	PH7_MemObjRelease(&sReplTmp);` |
|      59 |  694 | `	return rc;` |
|      30 |  695 | `}` |
|       - |  696 | `/*` |
|       - |  697 | ` * int levenshtein(string $string1,string $string2[,int $insertion_cost = 1[,int $replacement_cost = 1[,int $deletion_cost = 1]]])` |
|       - |  698 | ` *  Calculate the Levenshtein distance between two strings, byte per byte` |
|       - |  699 | ` *  (case-sensitive), with optional per-operation costs. Mirrors PHP's` |
|       - |  700 | ` *  reference_levdist(): two rolling rows over string2.` |
|       - |  701 | ` * Return` |
|       - |  702 | ` *  The minimal number of weighted edit operations turning $string1 into` |
|       - |  703 | ` *  $string2.` |
|       - |  704 | ` */` |
|      26 |  705 | `PH7_PRIVATE int PH7_builtin_levenshtein(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  706 | `{` |
|       - |  707 | `	static const char *azParam[] = { "$insertion_cost","$replacement_cost","$deletion_cost" };` |
|       - |  708 | `	const char *zStr1,*zStr2;` |
|      27 |  709 | `	sxi64 iCostIns = 1,iCostRep = 1,iCostDel = 1;` |
|       - |  710 | `	sxi64 *p1,*p2,*pTmp;` |
|       - |  711 | `	sxi64 c0,c1,c2;` |
|       - |  712 | `	ph7_value sTmp1,sTmp2;` |
|       - |  713 | `	int nLen1,nLen2;` |
|       - |  714 | `	int i1,i2;` |
|       - |  715 | `	sxi32 rc;` |
|       - |  716 | `	int i;` |
|      27 |  717 | `	if( nArg < 2 ){` |
|     ! 0 |  718 | `		return PH7_VmThrowException(pCtx,` |
|       - |  719 | `			"ArgumentCountError",` |
|       - |  720 | `			"levenshtein() expects at least 2 arguments, %d given",` |
|     ! 0 |  721 | `			nArg` |
|       - |  722 | `			);` |
|       - |  723 | `	}` |
|       - |  724 | `	/* $string1/$string2: null deprecates to "", __toString objects resolve,` |
|       - |  725 | `	 * everything non-stringish is a TypeError (php ZPP weak mode). */` |
|      27 |  726 | `	PH7_MemObjInit(pCtx->pVm,&sTmp1);` |
|      27 |  727 | `	PH7_MemObjInit(pCtx->pVm,&sTmp2);` |
|      27 |  728 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"levenshtein",1,"$string1","string",` |
|       - |  729 | `		"levenshtein(): Passing null to parameter #1 ($string1) "` |
|       - |  730 | `		"of type string is deprecated",` |
|       - |  731 | `		&sTmp1,&zStr1,&nLen1);` |
|      27 |  732 | `	if( rc != PH7_OK ) goto out;` |
|      27 |  733 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"levenshtein",2,"$string2","string",` |
|       - |  734 | `		"levenshtein(): Passing null to parameter #2 ($string2) "` |
|       - |  735 | `		"of type string is deprecated",` |
|       - |  736 | `		&sTmp2,&zStr2,&nLen2);` |
|      27 |  737 | `	if( rc != PH7_OK ) goto out;` |
|       - |  738 | `	/* Optional integer costs */` |
|      49 |  739 | `	for( i = 2 ; i < nArg && i < 5 ; i++ ){` |
|       - |  740 | `		sxi64 iVal;` |
|      23 |  741 | `		rc = PH7_IntArgResolve(pCtx,apArg[i],"levenshtein",i+1,azParam[i-2],"int",&iVal);` |
|      23 |  742 | `		if( rc != PH7_OK ) goto out;` |
|      23 |  743 | `		if( i == 2 ){` |
|      11 |  744 | `			iCostIns = iVal;` |
|      18 |  745 | `		}else if( i == 3 ){` |
|       7 |  746 | `			iCostRep = iVal;` |
|       4 |  747 | `		}else{` |
|       7 |  748 | `			iCostDel = iVal;` |
|       - |  749 | `		}` |
|      12 |  750 | `	}` |
|      27 |  751 | `	if( nLen1 == 0 ){` |
|       3 |  752 | `		ph7_result_int64(pCtx,(sxi64)nLen2 * iCostIns);` |
|       3 |  753 | `		rc = PH7_OK;` |
|       3 |  754 | `		goto out;` |
|       - |  755 | `	}` |
|      25 |  756 | `	if( nLen2 == 0 ){` |
|       3 |  757 | `		ph7_result_int64(pCtx,(sxi64)nLen1 * iCostDel);` |
|       3 |  758 | `		rc = PH7_OK;` |
|       3 |  759 | `		goto out;` |
|       - |  760 | `	}` |
|       - |  761 | `	/* Two rolling DP rows over string2 (auto-released on return). Reject a` |
|       - |  762 | `	 * string2 long enough to overflow the 32-bit allocation size. */` |
|      23 |  763 | `	if( (sxu32)nLen2 >= (SXU32_HIGH / sizeof(sxi64)) - 1 ){` |
|     ! 0 |  764 | `		rc = PH7_ContextMemoryError(pCtx);` |
|     ! 0 |  765 | `		goto out;` |
|       - |  766 | `	}` |
|      23 |  767 | `	p1 = (sxi64 *)ph7_context_alloc_chunk(pCtx,(unsigned int)(sizeof(sxi64) * (sxu32)(nLen2 + 1)),FALSE,TRUE);` |
|      23 |  768 | `	p2 = (sxi64 *)ph7_context_alloc_chunk(pCtx,(unsigned int)(sizeof(sxi64) * (sxu32)(nLen2 + 1)),FALSE,TRUE);` |
|      23 |  769 | `	if( p1 == 0 \|\| p2 == 0 ){` |
|     ! 0 |  770 | `		rc = PH7_ContextMemoryError(pCtx);` |
|     ! 0 |  771 | `		goto out;` |
|       - |  772 | `	}` |
|     733 |  773 | `	for( i2 = 0 ; i2 <= nLen2 ; i2++ ){` |
|     711 |  774 | `		p1[i2] = (sxi64)i2 * iCostIns;` |
|     356 |  775 | `	}` |
|     707 |  776 | `	for( i1 = 0 ; i1 < nLen1 ; i1++ ){` |
|     685 |  777 | `		p2[0] = p1[0] + iCostDel;` |
|  181111 |  778 | `		for( i2 = 0 ; i2 < nLen2 ; i2++ ){` |
|  180427 |  779 | `			c0 = p1[i2] + ((zStr1[i1] == zStr2[i2]) ? 0 : iCostRep);` |
|  180427 |  780 | `			c1 = p1[i2 + 1] + iCostDel;` |
|  180427 |  781 | `			if( c1 < c0 ){` |
|   45393 |  782 | `				c0 = c1;` |
|   22696 |  783 | `			}` |
|  180427 |  784 | `			c2 = p2[i2] + iCostIns;` |
|  180427 |  785 | `			if( c2 < c0 ){` |
|   44809 |  786 | `				c0 = c2;` |
|   22404 |  787 | `			}` |
|  180427 |  788 | `			p2[i2 + 1] = c0;` |
|   90214 |  789 | `		}` |
|     685 |  790 | `		pTmp = p1;` |
|     685 |  791 | `		p1 = p2;` |
|     685 |  792 | `		p2 = pTmp;` |
|     343 |  793 | `	}` |
|      23 |  794 | `	ph7_result_int64(pCtx,p1[nLen2]);` |
|      23 |  795 | `	rc = PH7_OK;` |
|      13 |  796 | `out:` |
|      27 |  797 | `	PH7_MemObjRelease(&sTmp1);` |
|      27 |  798 | `	PH7_MemObjRelease(&sTmp2);` |
|      27 |  799 | `	return rc;` |
|      14 |  800 | `}` |
|       - |  801 | `/*` |
|       - |  802 | ` * Longest common substring scan behind similar_text() — a faithful port of` |
|       - |  803 | ` * PHP's php_similar_str(): O(n*m) scan recording the first longest run.` |
|       - |  804 | ` */` |
|      30 |  805 | `static void SimilarStr(const char *zTxt1,int nLen1,const char *zTxt2,int nLen2,` |
|       - |  806 | `	int *pPos1,int *pPos2,int *pMax,int *pCount)` |
|       1 |  807 | `{` |
|       - |  808 | `	const char *p,*q;` |
|      31 |  809 | `	const char *zEnd1 = &zTxt1[nLen1];` |
|      31 |  810 | `	const char *zEnd2 = &zTxt2[nLen2];` |
|       - |  811 | `	int l;` |
|      31 |  812 | `	*pMax = 0;` |
|      31 |  813 | `	*pCount = 0;` |
|     155 |  814 | `	for( p = zTxt1 ; p < zEnd1 ; p++ ){` |
|     871 |  815 | `		for( q = zTxt2 ; q < zEnd2 ; q++ ){` |
|    1025 |  816 | `			for( l = 0 ; (p+l < zEnd1) && (q+l < zEnd2) && (p[l] == q[l]) ; l++ );` |
|     747 |  817 | `			if( l > *pMax ){` |
|      27 |  818 | `				*pMax = l;` |
|      27 |  819 | `				*pCount += 1;` |
|      27 |  820 | `				*pPos1 = (int)(p - zTxt1);` |
|      27 |  821 | `				*pPos2 = (int)(q - zTxt2);` |
|      13 |  822 | `			}` |
|     374 |  823 | `		}` |
|      63 |  824 | `	}` |
|      31 |  825 | `}` |
|       - |  826 | `/*` |
|       - |  827 | ` * Recursive divide-and-conquer behind similar_text() — a faithful port of` |
|       - |  828 | `` * PHP's php_similar_char(), including its quirky `count > 1` guard on the`` |
|       - |  829 | ` * left-side recursion.` |
|       - |  830 | ` */` |
|      30 |  831 | `static int SimilarChar(const char *zTxt1,int nLen1,const char *zTxt2,int nLen2)` |
|       1 |  832 | `{` |
|       - |  833 | `	int nSum;` |
|      31 |  834 | `	int nPos1 = 0,nPos2 = 0,nMax,nCount;` |
|      31 |  835 | `	SimilarStr(zTxt1,nLen1,zTxt2,nLen2,&nPos1,&nPos2,&nMax,&nCount);` |
|      31 |  836 | `	if( (nSum = nMax) != 0 ){` |
|      27 |  837 | `		if( nPos1 && nPos2 && nCount > 1 ){` |
|     ! 0 |  838 | `			nSum += SimilarChar(zTxt1,nPos1,zTxt2,nPos2);` |
|     ! 0 |  839 | `		}` |
|      27 |  840 | `		if( (nPos1 + nMax < nLen1) && (nPos2 + nMax < nLen2) ){` |
|      16 |  841 | `			nSum += SimilarChar(&zTxt1[nPos1 + nMax],nLen1 - nPos1 - nMax,` |
|      10 |  842 | `				&zTxt2[nPos2 + nMax],nLen2 - nPos2 - nMax);` |
|       5 |  843 | `		}` |
|      13 |  844 | `	}` |
|      31 |  845 | `	return nSum;` |
|       1 |  846 | `}` |
|       - |  847 | `/*` |
|       - |  848 | ` * int similar_text(string $string1,string $string2[,float &$percent])` |
|       - |  849 | ` *  Calculate the similarity between two strings, as the number of matching` |
|       - |  850 | ` *  characters found by PHP's greedy longest-common-substring recursion.` |
|       - |  851 | ` *  When $percent is given it receives the similarity in percent:` |
|       - |  852 | ` *  matching * 200 / (len1 + len2).` |
|       - |  853 | ` * Return` |
|       - |  854 | ` *  The number of matching characters in both strings.` |
|       - |  855 | ` */` |
|      24 |  856 | `PH7_PRIVATE int PH7_builtin_similar_text(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  857 | `{` |
|       - |  858 | `	/* Initialized only to satisfy -Wmaybe-uninitialized: StrPredicateResolveArg` |
|       - |  859 | ``	 * writes both out-params on every PH7_OK return and the `goto out` covers every`` |
|       - |  860 | `	 * other one, but the inlined PH7_MemObjRelease made gcc inline enough of the` |
|       - |  861 | `	 * call chain to start guessing otherwise. */` |
|      25 |  862 | `	const char *zStr1 = 0,*zStr2 = 0;` |
|       - |  863 | `	ph7_value sTmp1,sTmp2;` |
|      25 |  864 | `	int nLen1 = 0,nLen2 = 0;` |
|       - |  865 | `	int nSim;` |
|       - |  866 | `	sxi32 rc;` |
|      25 |  867 | `	if( nArg < 2 ){` |
|     ! 0 |  868 | `		return PH7_VmThrowException(pCtx,` |
|       - |  869 | `			"ArgumentCountError",` |
|       - |  870 | `			"similar_text() expects at least 2 arguments, %d given",` |
|     ! 0 |  871 | `			nArg` |
|       - |  872 | `			);` |
|       - |  873 | `	}` |
|      25 |  874 | `	PH7_MemObjInit(pCtx->pVm,&sTmp1);` |
|      25 |  875 | `	PH7_MemObjInit(pCtx->pVm,&sTmp2);` |
|      25 |  876 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"similar_text",1,"$string1","string",` |
|       - |  877 | `		"similar_text(): Passing null to parameter #1 ($string1) "` |
|       - |  878 | `		"of type string is deprecated",` |
|       - |  879 | `		&sTmp1,&zStr1,&nLen1);` |
|      25 |  880 | `	if( rc != PH7_OK ) goto out;` |
|      25 |  881 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"similar_text",2,"$string2","string",` |
|       - |  882 | `		"similar_text(): Passing null to parameter #2 ($string2) "` |
|       - |  883 | `		"of type string is deprecated",` |
|       - |  884 | `		&sTmp2,&zStr2,&nLen2);` |
|      25 |  885 | `	if( rc != PH7_OK ) goto out;` |
|      25 |  886 | `	if( nLen1 + nLen2 == 0 ){` |
|       5 |  887 | `		nSim = 0;` |
|       3 |  888 | `	}else{` |
|      21 |  889 | `		nSim = SimilarChar(zStr1,nLen1,zStr2,nLen2);` |
|       - |  890 | `	}` |
|      25 |  891 | `	if( nArg > 2 ){` |
|       - |  892 | `		/* Write the percentage through the by-ref out-param */` |
|       9 |  893 | `		ph7_value *pPercent = ph7_context_new_scalar(pCtx);` |
|       9 |  894 | `		if( pPercent == 0 ){` |
|     ! 0 |  895 | `			rc = PH7_ContextMemoryError(pCtx);` |
|     ! 0 |  896 | `			goto out;` |
|     ! 0 |  897 | `		}else{` |
|       9 |  898 | `			double dPct = (nLen1 + nLen2 == 0) ? 0.0 : (double)nSim * 200.0 / (double)(nLen1 + nLen2);` |
|       9 |  899 | `			ph7_value_double(pPercent,dPct);` |
|       9 |  900 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],pPercent);` |
|       - |  901 | `		}` |
|       4 |  902 | `	}` |
|      25 |  903 | `	ph7_result_int(pCtx,nSim);` |
|      25 |  904 | `	rc = PH7_OK;` |
|      12 |  905 | `out:` |
|      25 |  906 | `	PH7_MemObjRelease(&sTmp1);` |
|      25 |  907 | `	PH7_MemObjRelease(&sTmp2);` |
|      25 |  908 | `	return rc;` |
|      13 |  909 | `}` |
|       - |  910 | `/*` |
|       - |  911 | ` * array\|int str_word_count(string $string[,int $format = 0[,?string $characters = null]])` |
|       - |  912 | ` *  Count (or return) the words inside a string. A word is a run of alphabetic` |
|       - |  913 | ` *  characters, which may contain (but not start the string with) "'" and "-";` |
|       - |  914 | ` *  $characters adds extra bytes to the word set ("a..z" ranges supported, as` |
|       - |  915 | ` *  in PHP's php_charmask).` |
|       - |  916 | ` *  $format: 0 -> word count, 1 -> array of words, 2 -> array of words keyed` |
|       - |  917 | ` *  by their byte position in $string.` |
|       - |  918 | ` * Errors` |
|       - |  919 | ` *  ValueError when $format is not 0, 1 or 2.` |
|       - |  920 | ` */` |
|      42 |  921 | `PH7_PRIVATE int PH7_builtin_str_word_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  922 | `{` |
|       - |  923 | `	const char *zIn,*zEnd,*zPtr;` |
|      43 |  924 | `	ph7_value *pArray = 0,*pValue = 0;` |
|       - |  925 | `	ph7_value sTmp,sListTmp;` |
|       - |  926 | `	char aMask[256];` |
|      43 |  927 | `	int bMask = 0;` |
|      43 |  928 | `	int iFormat = 0;` |
|      43 |  929 | `	int nCount = 0;` |
|       - |  930 | `	int nLen;` |
|       - |  931 | `	sxi32 rc;` |
|      43 |  932 | `	if( nArg < 1 ){` |
|     ! 0 |  933 | `		return PH7_VmThrowException(pCtx,` |
|       - |  934 | `			"ArgumentCountError",` |
|       - |  935 | `			"str_word_count() expects at least 1 argument, %d given",` |
|     ! 0 |  936 | `			nArg` |
|       - |  937 | `			);` |
|       - |  938 | `	}` |
|      43 |  939 | `	PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|      43 |  940 | `	PH7_MemObjInit(pCtx->pVm,&sListTmp);` |
|      43 |  941 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_word_count",1,"$string","string",` |
|       - |  942 | `		"str_word_count(): Passing null to parameter #1 ($string) "` |
|       - |  943 | `		"of type string is deprecated",` |
|       - |  944 | `		&sTmp,&zIn,&nLen);` |
|      43 |  945 | `	if( rc != PH7_OK ) goto out;` |
|      43 |  946 | `	if( nArg > 1 ){` |
|       - |  947 | `		sxi64 iVal;` |
|      29 |  948 | `		rc = PH7_IntArgResolve(pCtx,apArg[1],"str_word_count",2,"$format","int",&iVal);` |
|      31 |  949 | `		if( rc != PH7_OK ) goto out;` |
|      29 |  950 | `		if( iVal < 0 \|\| iVal > 2 ){` |
|       5 |  951 | `			rc = PH7_VmThrowException(pCtx,` |
|       - |  952 | `				"ValueError",` |
|       - |  953 | `				"str_word_count(): Argument #2 ($format) must be a valid format value"` |
|       - |  954 | `				);` |
|       5 |  955 | `			goto out;` |
|       - |  956 | `		}` |
|      25 |  957 | `		iFormat = (int)iVal;` |
|      12 |  958 | `	}` |
|      39 |  959 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|       - |  960 | `		/* $characters is ?string: null (skipped above) simply keeps the` |
|       - |  961 | `		 * default word set, no deprecation. */` |
|       - |  962 | `		const char *zList;` |
|       - |  963 | `		int nList;` |
|      13 |  964 | `		rc = StrPredicateResolveArg(pCtx,apArg[2],"str_word_count",3,"$characters","?string",` |
|       - |  965 | `			"" /* unreachable: null never gets here */,` |
|       - |  966 | `			&sListTmp,&zList,&nList);` |
|      13 |  967 | `		if( rc != PH7_OK ) goto out;` |
|      13 |  968 | `		PH7_BuildCharMask(pCtx,zList,nList,aMask);` |
|      13 |  969 | `		bMask = 1;` |
|       6 |  970 | `	}` |
|      39 |  971 | `	if( iFormat != 0 ){` |
|      25 |  972 | `		pArray = ph7_context_new_array(pCtx);` |
|      25 |  973 | `		pValue = ph7_context_new_scalar(pCtx);` |
|      25 |  974 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|     ! 0 |  975 | `			rc = PH7_ContextMemoryError(pCtx);` |
|     ! 0 |  976 | `			goto out;` |
|       - |  977 | `		}` |
|      12 |  978 | `	}` |
|      39 |  979 | `	zPtr = zIn;` |
|      39 |  980 | `	zEnd = &zIn[nLen];` |
|      39 |  981 | `	if( nLen > 0 ){` |
|       - |  982 | `		/* php: the string's first byte cannot be ' or -, and its last byte` |
|       - |  983 | `		 * cannot be -, unless the charlist explicitly allows them. */` |
|      33 |  984 | `		if( (zPtr[0] == '\'' && (!bMask \|\| !aMask[(unsigned char)'\''])) \|\|` |
|      28 |  985 | `			(zPtr[0] == '-'  && (!bMask \|\| !aMask[(unsigned char)'-'])) ){` |
|       9 |  986 | `			zPtr++;` |
|       4 |  987 | `		}` |
|      33 |  988 | `		if( zEnd[-1] == '-' && (!bMask \|\| !aMask[(unsigned char)'-']) ){` |
|       9 |  989 | `			zEnd--;` |
|       4 |  990 | `		}` |
|      16 |  991 | `	}` |
|     135 |  992 | `	while( zPtr < zEnd ){` |
|      91 |  993 | `		const char *zStart = zPtr;` |
|     477 |  994 | `		while( zPtr < zEnd && ( SyisAlpha((unsigned char)zPtr[0])` |
|     253 |  995 | `			\|\| (bMask && aMask[(unsigned char)zPtr[0]])` |
|      98 |  996 | `			\|\| zPtr[0] == '\'' \|\| zPtr[0] == '-' ) ){` |
|     339 |  997 | `			zPtr++;` |
|       1 |  998 | `		}` |
|      97 |  999 | `		if( zPtr > zStart ){` |
|      91 | 1000 | `			if( iFormat == 0 ){` |
|      19 | 1001 | `				nCount++;` |
|      10 | 1002 | `			}else{` |
|      73 | 1003 | `				ph7_value_reset_string_cursor(pValue);` |
|      73 | 1004 | `				if( SXRET_OK != ph7_value_string(pValue,zStart,(int)(zPtr-zStart)) ){` |
|     ! 0 | 1005 | `					rc = PH7_ContextMemoryError(pCtx);` |
|     ! 0 | 1006 | `					goto out;` |
|       - | 1007 | `				}` |
|      73 | 1008 | `				if( iFormat == 1 ){` |
|      59 | 1009 | `					if( SXRET_OK != ph7_array_add_elem(pArray,0,pValue) ){` |
|     ! 0 | 1010 | `						rc = PH7_ContextMemoryError(pCtx);` |
|     ! 0 | 1011 | `						goto out;` |
|       - | 1012 | `					}` |
|      30 | 1013 | `				}else{` |
|      15 | 1014 | `					if( SXRET_OK != ph7_array_add_intkey_elem(pArray,(int)(zStart-zIn),pValue) ){` |
|     ! 0 | 1015 | `						rc = PH7_ContextMemoryError(pCtx);` |
|     ! 0 | 1016 | `						goto out;` |
|       - | 1017 | `					}` |
|       - | 1018 | `				}` |
|       - | 1019 | `			}` |
|      45 | 1020 | `		}` |
|      97 | 1021 | `		zPtr++;` |
|       1 | 1022 | `	}` |
|      37 | 1023 | `	if( iFormat == 0 ){` |
|      13 | 1024 | `		ph7_result_int(pCtx,nCount);` |
|       7 | 1025 | `	}else{` |
|      25 | 1026 | `		ph7_result_value(pCtx,pArray);` |
|       - | 1027 | `	}` |
|      37 | 1028 | `	rc = PH7_OK;` |
|      20 | 1029 | `out:` |
|      41 | 1030 | `	PH7_MemObjRelease(&sTmp);` |
|      41 | 1031 | `	PH7_MemObjRelease(&sListTmp);` |
|      41 | 1032 | `	return rc;` |
|      21 | 1033 | `}` |
|       - | 1034 | `/*` |
|       - | 1035 | ` * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])` |
|       - | 1036 | ` *   Split a string into smaller chunks.` |
|       - | 1037 | ` * Parameters` |
|       - | 1038 | ` *  $body` |
|       - | 1039 | ` *   The string to be chunked.` |
|       - | 1040 | ` * $chunklen` |
|       - | 1041 | ` *   The chunk length.` |
|       - | 1042 | ` * $end` |
|       - | 1043 | ` *   The line ending sequence.` |
|       - | 1044 | ` * Return` |
|       - | 1045 | ` *  The chunked string or NULL on failure.` |
|       - | 1046 | ` */` |
|      24 | 1047 | `PH7_PRIVATE int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1048 | `{` |
|      27 | 1049 | `	const char *zIn,*zEnd,*zSep = "\r\n";` |
|       - | 1050 | `	int nSepLen,nChunkLen,nLen;` |
|       - | 1051 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|       - | 1052 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|      27 | 1053 | `	if( nArg < 1 ){` |
|       - | 1054 | `		/* Nothing to split,return null */` |
|     ! 0 | 1055 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1056 | `		return PH7_OK;` |
|       - | 1057 | `	}` |
|       - | 1058 | `	/* initialize/Extract arguments */` |
|      27 | 1059 | `	nSepLen = (int)sizeof("\r\n") - 1;` |
|      27 | 1060 | `	nChunkLen = 76;` |
|      27 | 1061 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      27 | 1062 | `	zEnd = &zIn[nLen];` |
|      27 | 1063 | `	if( nArg > 1 ){` |
|       - | 1064 | `		/* Chunk length */` |
|      25 | 1065 | `		nChunkLen = ph7_value_to_int(apArg[1]);` |
|      25 | 1066 | `		if( nChunkLen < 1 ){` |
|       - | 1067 | `			/* PHP 8 throws a catchable ValueError for a non-positive length. */` |
|       3 | 1068 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1069 | `				"chunk_split(): Argument #2 ($length) must be greater than 0");` |
|       - | 1070 | `		}` |
|      23 | 1071 | `		if( nArg > 2 ){` |
|       - | 1072 | `			/* Separator */` |
|      21 | 1073 | `			zSep = ph7_value_to_string(apArg[2],&nSepLen);` |
|      21 | 1074 | `			if( nSepLen < 1 ){` |
|       - | 1075 | `				/* Switch back to the default separator */` |
|       3 | 1076 | `				zSep = "\r\n";` |
|       3 | 1077 | `				nSepLen = (int)sizeof("\r\n") - 1;` |
|       1 | 1078 | `			}` |
|       9 | 1079 | `		}` |
|      10 | 1080 | `	}` |
|       - | 1081 | `	/* Perform the requested operation */` |
|      25 | 1082 | `	if( nChunkLen > nLen ){` |
|       - | 1083 | `		/* Nothing to split,return the string and the separator */` |
|       9 | 1084 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);` |
|       9 | 1085 | `		return PH7_OK;` |
|       - | 1086 | `	}` |
|     127 | 1087 | `	while( zIn < zEnd ){` |
|     113 | 1088 | `		if( nChunkLen > (int)(zEnd-zIn) ){` |
|      15 | 1089 | `			nChunkLen = (int)(zEnd - zIn);` |
|       6 | 1090 | `		}` |
|       - | 1091 | `		/* Append the chunk and the separator */` |
|     113 | 1092 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);` |
|       - | 1093 | `		/* Point beyond the chunk */` |
|     113 | 1094 | `		zIn += nChunkLen;` |
|       3 | 1095 | `	}` |
|      17 | 1096 | `	return PH7_OK;` |
|      15 | 1097 | `}` |
|       - | 1098 | `/*` |
|       - | 1099 | ` * string addslashes(string $str)` |
|       - | 1100 | ` *  Quote string with slashes.` |
|       - | 1101 | ` *  Returns a string with backslashes before characters that need` |
|       - | 1102 | ` *  to be quoted in database queries etc. These characters are single` |
|       - | 1103 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|       - | 1104 | ` * Parameter` |
|       - | 1105 | ` *  str: The string to be escaped.` |
|       - | 1106 | ` * Return` |
|       - | 1107 | ` *  Returns the escaped string` |
|       - | 1108 | ` */` |
|      20 | 1109 | `PH7_PRIVATE int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1110 | `{` |
|       - | 1111 | `	char zGiven[64];` |
|       - | 1112 | `	const char *zCur,*zIn,*zEnd;` |
|       - | 1113 | `	int nLen;` |
|       - | 1114 | `	/* PHP enforces exactly one argument. */` |
|      23 | 1115 | `	if( nArg != 1 ){` |
|     ! 0 | 1116 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1117 | `			"ArgumentCountError",` |
|       - | 1118 | `			"addslashes() expects exactly 1 argument, %d given",` |
|     ! 0 | 1119 | `			nArg` |
|       - | 1120 | `			);` |
|       - | 1121 | `	}` |
|       - | 1122 | `	/* php only DEPRECATES null here; PHL rejects it. */` |
|      23 | 1123 | `	if( ph7_value_is_null(apArg[0]) ){` |
|     ! 0 | 1124 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1125 | `			"addslashes(): Argument #1 ($string) must be of type string, null given"` |
|       - | 1126 | `			);` |
|       - | 1127 | `	}` |
|       - | 1128 | `	/* Arrays, resources and objects with no __toString() raise a TypeError like PHP */` |
|      30 | 1129 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|      33 | 1130 | `	    ph7_value_is_resource(apArg[0]) \|\|` |
|      20 | 1131 | `	    PH7_ArgIsUnstringableObject(apArg[0]) ){` |
|     ! 0 | 1132 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1133 | `			"TypeError",` |
|       - | 1134 | `			"addslashes(): Argument #1 ($string) must be of type string, %s given",` |
|     ! 0 | 1135 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 1136 | `			);` |
|       - | 1137 | `	}` |
|       - | 1138 | `	/* Convert to string representation first and obtain length. */` |
|      23 | 1139 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      23 | 1140 | `	if( nLen < 1 ){` |
|       - | 1141 | `		/* Return the empty string */` |
|       6 | 1142 | `		ph7_result_string(pCtx,"",0);` |
|       6 | 1143 | `		return PH7_OK;` |
|       - | 1144 | `	}` |
|      18 | 1145 | `	zEnd = &zIn[nLen];` |
|      18 | 1146 | `	zCur = 0; /* cc warning */` |
|      23 | 1147 | `	for(;;){` |
|      48 | 1148 | `		if( zIn >= zEnd ){` |
|       - | 1149 | `			/* No more input */` |
|      18 | 1150 | `			break;` |
|       - | 1151 | `		}` |
|      32 | 1152 | `		zCur = zIn;` |
|       - | 1153 | `		/* scan until a character that needs escaping (', ", \\, or NUL) */` |
|      98 | 1154 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' && zIn[0] != '\0' ){` |
|      68 | 1155 | `			zIn++;` |
|       2 | 1156 | `		}` |
|      32 | 1157 | `		if( zIn > zCur ){` |
|       - | 1158 | `			/* Append raw contents */` |
|      28 | 1159 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      13 | 1160 | `		}` |
|      32 | 1161 | `		if( zIn < zEnd ){` |
|      20 | 1162 | `			int c = zIn[0];` |
|      20 | 1163 | `			if( c == '\0' ){` |
|       - | 1164 | `				/* PHP escapes NUL as "\\0" (two characters) */` |
|       3 | 1165 | `				ph7_result_string(pCtx,"\\0",2);` |
|       2 | 1166 | `			}else{` |
|      18 | 1167 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|       - | 1168 | `			}` |
|       9 | 1169 | `		}` |
|      32 | 1170 | `		zIn++;` |
|       2 | 1171 | `	}` |
|      18 | 1172 | `	return PH7_OK;` |
|      13 | 1173 | `}` |
|       - | 1174 | `/*` |
|       - | 1175 | ``  * Build a 256-entry membership mask from a PHP charlist, expanding `a..z` `` |
|       - | 1176 | ` * byte ranges exactly like PHP's php_charmask(). On return aMask[c] != 0 iff` |
|       - | 1177 | ` * the byte c belongs to the set. Emits the PHP-exact warnings for the three` |
|       - | 1178 | ` * malformed-range shapes (ph7_context_throw_error_format prepends the active` |
|       - | 1179 | ` * function name, so the messages omit it); on a bad range the surrounding` |
|       - | 1180 | ` * bytes are still added and the scan never aborts. Reads only within` |
|       - | 1181 | ` * [zList, zList+nLen).` |
|       - | 1182 | ` *` |
|       - | 1183 | ` * Use ONLY for the builtins whose charlist expands ranges the way PHP's` |
|       - | 1184 | ` * php_charmask() does: trim/ltrim/rtrim/addcslashes (and quotemeta, whose set` |
|       - | 1185 | ` * is a fixed literal with no ".."). Do NOT route strspn/strcspn/strtok/strpbrk` |
|       - | 1186 | ` * through this — PHP treats their charlists literally, so expanding "a..z" here` |
|       - | 1187 | ` * would be a behavior regression plus spurious "Invalid '..'-range" warnings.` |
|       - | 1188 | ` */` |
|    2232 | 1189 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256])` |
|       5 | 1190 | `{` |
|    2237 | 1191 | `	const unsigned char *zIn  = (const unsigned char *)zList;` |
|    2237 | 1192 | `	const unsigned char *zEnd = zIn + (nLen > 0 ? nLen : 0);` |
|    2237 | 1193 | `	SyZero(aMask,256);` |
|    5275 | 1194 | `	for( ; zIn < zEnd ; zIn++ ){` |
|    3043 | 1195 | `		int c = zIn[0];` |
|    3043 | 1196 | `		if( zIn + 3 < zEnd && zIn[1] == '.' && zIn[2] == '.' && zIn[3] >= c ){` |
|       - | 1197 | `			/* Valid incrementing range c..zIn[3] */` |
|     286 | 1198 | `			int hi = zIn[3],k;` |
|   12064 | 1199 | `			for( k = c ; k <= hi ; k++ ){` |
|   11782 | 1200 | `				aMask[k] = 1;` |
|    5893 | 1201 | `			}` |
|     286 | 1202 | `			zIn += 3; /* the loop's ++ then steps past the range end */` |
|    2914 | 1203 | `		}else if( zIn + 1 < zEnd && zIn[0] == '.' && zIn[1] == '.' ){` |
|       - | 1204 | `			/* Malformed range: mirror php_charmask's three diagnostics. */` |
|       - | 1205 | `			const char *zMsg;` |
|      27 | 1206 | `			if( (const unsigned char *)zList >= zIn ){` |
|       6 | 1207 | `				zMsg = "no character to the left of '..'";` |
|      25 | 1208 | `			}else if( zIn + 2 >= zEnd ){` |
|       6 | 1209 | `				zMsg = "no character to the right of '..'";` |
|      21 | 1210 | `			}else if( zIn[-1] > zIn[2] ){` |
|      19 | 1211 | `				zMsg = "'..'-range needs to be incrementing";` |
|      11 | 1212 | `			}else{` |
|     ! 0 | 1213 | `				zMsg = 0; /* catch-all (e.g. a..b..c) */` |
|       - | 1214 | `			}` |
|      27 | 1215 | `			if( zMsg ){` |
|      39 | 1216 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      12 | 1217 | `					"Invalid '..'-range, %s",zMsg);` |
|      15 | 1218 | `			}else{` |
|     ! 0 | 1219 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - | 1220 | `					"Invalid '..'-range");` |
|       - | 1221 | `			}` |
|       - | 1222 | `			/* Do not consume the dots: the loop's ++ steps one byte so the` |
|       - | 1223 | `			 * dots are re-scanned as literals, exactly like php_charmask. */` |
|      15 | 1224 | `		}else{` |
|    2737 | 1225 | `			aMask[c] = 1;` |
|       - | 1226 | `		}` |
|    1539 | 1227 | `	}` |
|    2237 | 1228 | `}` |
|       - | 1229 | `/*` |
|       - | 1230 | ` * string addcslashes(string $str,string $charlist)` |
|       - | 1231 | ` *  Quote string with slashes in a C style.` |
|       - | 1232 | ` * Parameter` |
|       - | 1233 | ` *  $str:` |
|       - | 1234 | ` *    The string to be escaped.` |
|       - | 1235 | ` *  $charlist:` |
|       - | 1236 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|       - | 1237 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|       - | 1238 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|       - | 1239 | ` * Return` |
|       - | 1240 | ` *  Returns the escaped string.` |
|       - | 1241 | ` * Note:` |
|       - | 1242 | ` *  Character ranges [i.e: 'A..Z'] are supported (see PH7_BuildCharMask).` |
|       - | 1243 | ` */` |
|     348 | 1244 | `PH7_PRIVATE int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 1245 | `{` |
|       - | 1246 | `	char zGiven[64];` |
|       - | 1247 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|       - | 1248 | `	char aMask[256];` |
|       - | 1249 | `	int nLen,nMask;` |
|       - | 1250 | `	/* PHP enforces exactly two arguments. */` |
|     352 | 1251 | `	if( nArg != 2 ){` |
|     ! 0 | 1252 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1253 | `			"ArgumentCountError",` |
|       - | 1254 | `			"addcslashes() expects exactly 2 arguments, %d given",` |
|     ! 0 | 1255 | `			nArg` |
|       - | 1256 | `			);` |
|       - | 1257 | `	}` |
|       - | 1258 | `	/* php only DEPRECATES null here; PHL rejects it. */` |
|     352 | 1259 | `	if( ph7_value_is_null(apArg[0]) ){` |
|     ! 0 | 1260 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1261 | `			"TypeError",` |
|       - | 1262 | `			"addcslashes(): Argument #1 ($string) must be of type string, null given"` |
|       - | 1263 | `			);` |
|     522 | 1264 | `	} else if( ph7_value_is_array(apArg[0]) \|\|` |
|     526 | 1265 | `	          ph7_value_is_resource(apArg[0]) \|\|` |
|     348 | 1266 | `	          PH7_ArgIsUnstringableObject(apArg[0]) ){` |
|     ! 0 | 1267 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1268 | `			"TypeError",` |
|       - | 1269 | `			"addcslashes(): Argument #1 ($string) must be of type string, %s given",` |
|     ! 0 | 1270 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 1271 | `			);` |
|       - | 1272 | `	}` |
|       - | 1273 | `	/* php only DEPRECATES null here; PHL rejects it. */` |
|     352 | 1274 | `	if( ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 1275 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1276 | `			"TypeError",` |
|       - | 1277 | `			"addcslashes(): Argument #2 ($characters) must be of type string, null given"` |
|       - | 1278 | `			);` |
|     522 | 1279 | `	} else if( ph7_value_is_array(apArg[1]) \|\|` |
|     526 | 1280 | `	          ph7_value_is_resource(apArg[1]) \|\|` |
|     348 | 1281 | `	          PH7_ArgIsUnstringableObject(apArg[1]) ){` |
|     ! 0 | 1282 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1283 | `			"TypeError",` |
|       - | 1284 | `			"addcslashes(): Argument #2 ($characters) must be of type string, %s given",` |
|     ! 0 | 1285 | `			VmValueGivenName(apArg[1],zGiven,sizeof(zGiven))` |
|       - | 1286 | `			);` |
|       - | 1287 | `	}` |
|       - | 1288 | `	/* Extract the string to process */` |
|     352 | 1289 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|       - | 1290 | `	/* NULL would never reach here due to the check above. */` |
|     352 | 1291 | `	if( nLen < 1 ){` |
|       - | 1292 | `		/* Empty string returns itself. */` |
|      16 | 1293 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      16 | 1294 | `		return PH7_OK;` |
|       - | 1295 | `	}` |
|       - | 1296 | ``	/* Extract the desired mask and expand any `a..z` ranges into a lookup. */`` |
|     338 | 1297 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|     338 | 1298 | `	PH7_BuildCharMask(pCtx,zMask,nMask,aMask);` |
|     338 | 1299 | `	zEnd = &zIn[nLen];` |
|     338 | 1300 | `	zCur = 0; /* cc warning */` |
|     899 | 1301 | `	for(;;){` |
|    1802 | 1302 | `		if( zIn >= zEnd ){` |
|       - | 1303 | `			/* No more input */` |
|     338 | 1304 | `			break;` |
|       - | 1305 | `		}` |
|    1468 | 1306 | `		zCur = zIn;` |
|    7622 | 1307 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|    6158 | 1308 | `			zIn++;` |
|       4 | 1309 | `		}` |
|    1468 | 1310 | `		if( zIn > zCur ){` |
|       - | 1311 | `			/* Append raw contents */` |
|     416 | 1312 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     206 | 1313 | `		}` |
|    1468 | 1314 | `		if( zIn < zEnd ){` |
|       - | 1315 | `			/* Make sure we treat the byte as unsigned to avoid negative values` |
|       - | 1316 | `			 * on platforms where char is signed. */` |
|    1172 | 1317 | `			int c = (unsigned char)zIn[0];` |
|       - | 1318 | `			/* php escapes a byte OUTSIDE the printable range with a named C` |
|       - | 1319 | `			 * escape when it has one and with three-digit octal otherwise; a` |
|       - | 1320 | `			 * printable byte in the mask simply gets a backslash in front of` |
|       - | 1321 | `			 * it. The named set is php's own seven -- \n \t \r \a \v \b \f --` |
|       - | 1322 | `			 * and the two this engine used to leave out (\a and \b) were` |
|       - | 1323 | `			 * written as \007 and \010, a string php never produces, which` |
|       - | 1324 | `			 * stripcslashes() reads back to the same byte and every other` |
|       - | 1325 | `			 * reader does not. */` |
|    1172 | 1326 | `			if( c < 32 \|\| c > 126 ){` |
|     798 | 1327 | `				switch( c ){` |
|      86 | 1328 | `					case '\n': ph7_result_string(pCtx,"\\n",2); break;` |
|       9 | 1329 | `					case '\t': ph7_result_string(pCtx,"\\t",2); break;` |
|      23 | 1330 | `					case '\r': ph7_result_string(pCtx,"\\r",2); break;` |
|      11 | 1331 | `					case '\a': ph7_result_string(pCtx,"\\a",2); break;` |
|       9 | 1332 | `					case '\v': ph7_result_string(pCtx,"\\v",2); break;` |
|      11 | 1333 | `					case '\b': ph7_result_string(pCtx,"\\b",2); break;` |
|       9 | 1334 | `					case '\f': ph7_result_string(pCtx,"\\f",2); break;` |
|     322 | 1335 | `					default:` |
|       - | 1336 | `						/* php always emits three zero-padded octal digits` |
|       - | 1337 | `						 * (\001 and not \1). */` |
|     647 | 1338 | `						ph7_result_string_format(pCtx,"\\%03o",c);` |
|     644 | 1339 | `						break;` |
|       - | 1340 | `				}` |
|     401 | 1341 | `			}else{` |
|     378 | 1342 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|       - | 1343 | `			}` |
|     584 | 1344 | `		}` |
|    1468 | 1345 | `		zIn++;` |
|       4 | 1346 | `	}` |
|     338 | 1347 | `	return PH7_OK;` |
|     178 | 1348 | `}` |
|       - | 1349 | `/*` |
|       - | 1350 | ` * string quotemeta(string $str)` |
|       - | 1351 | ` *  Quote meta characters.` |
|       - | 1352 | ` * Parameter` |
|       - | 1353 | ` *  $str:` |
|       - | 1354 | ` *    The string to be escaped.` |
|       - | 1355 | ` * Return` |
|       - | 1356 | ` *  Returns the escaped string.` |
|       - | 1357 | `*/` |
|      12 | 1358 | `PH7_PRIVATE int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1359 | `{` |
|       - | 1360 | `	const char *zCur,*zIn,*zEnd;` |
|       - | 1361 | `	char aMask[256];` |
|       - | 1362 | `	int nLen;` |
|      15 | 1363 | `	if( nArg < 1 ){` |
|       - | 1364 | `		/* Nothing to process,retun NULL */` |
|     ! 0 | 1365 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1366 | `		return PH7_OK;` |
|       - | 1367 | `	}` |
|       - | 1368 | `	/* Extract the string to process */` |
|      15 | 1369 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      15 | 1370 | `	if( nLen < 1 ){` |
|       - | 1371 | `		/* Return the empty string */` |
|       6 | 1372 | `		ph7_result_string(pCtx,"",0);` |
|       6 | 1373 | `		return PH7_OK;` |
|       - | 1374 | `	}` |
|       - | 1375 | `	/* Fixed meta-character set (no ranges); build the lookup once. */` |
|      10 | 1376 | `	PH7_BuildCharMask(pCtx,".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1,aMask);` |
|      10 | 1377 | `	zEnd = &zIn[nLen];` |
|      10 | 1378 | `	zCur = 0; /* cc warning */` |
|      22 | 1379 | `	for(;;){` |
|      46 | 1380 | `		if( zIn >= zEnd ){` |
|       - | 1381 | `			/* No more input */` |
|      10 | 1382 | `			break;` |
|       - | 1383 | `		}` |
|      38 | 1384 | `		zCur = zIn;` |
|      76 | 1385 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|      40 | 1386 | `			zIn++;` |
|       2 | 1387 | `		}` |
|      38 | 1388 | `		if( zIn > zCur ){` |
|       - | 1389 | `			/* Append raw contents */` |
|      20 | 1390 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|       9 | 1391 | `		}` |
|      38 | 1392 | `		if( zIn < zEnd ){` |
|      36 | 1393 | `			int c = zIn[0];` |
|      36 | 1394 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|      17 | 1395 | `		}` |
|      38 | 1396 | `		zIn++;` |
|       2 | 1397 | `	}` |
|      10 | 1398 | `	return PH7_OK;` |
|       9 | 1399 | `}` |
|       - | 1400 | `/*` |
|       - | 1401 | ` * string stripslashes(string $str)` |
|       - | 1402 | ` *  Un-quotes a quoted string.` |
|       - | 1403 | ` *  Returns a string with backslashes before characters that need` |
|       - | 1404 | ` *  to be quoted in database queries etc. These characters are single` |
|       - | 1405 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|       - | 1406 | ` * Parameter` |
|       - | 1407 | ` *  $str` |
|       - | 1408 | ` *   The input string.` |
|       - | 1409 | ` * Return` |
|       - | 1410 | ` *  Returns a string with backslashes stripped off.` |
|       - | 1411 | ` */` |
|       8 | 1412 | `PH7_PRIVATE int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1413 | `{` |
|       - | 1414 | `	const char *zCur,*zIn,*zEnd;` |
|       - | 1415 | `	int nLen;` |
|      10 | 1416 | `	if( nArg < 1 ){` |
|       - | 1417 | `		/* Nothing to process,retun NULL */` |
|     ! 0 | 1418 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1419 | `		return PH7_OK;` |
|       - | 1420 | `	}` |
|       - | 1421 | `	/* Extract the string to process */` |
|      10 | 1422 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      10 | 1423 | `	if( zIn == 0 ){` |
|     ! 0 | 1424 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1425 | `		return PH7_OK;` |
|       - | 1426 | `	}` |
|      10 | 1427 | `	zEnd = &zIn[nLen];` |
|      10 | 1428 | `	zCur = 0; /* cc warning */` |
|       - | 1429 | `	/* Seed an empty string result: the loop below only ever APPENDS, so without` |
|       - | 1430 | `	 * this an empty input would leave the return value untouched and answer` |
|       - | 1431 | `	 * NULL where php answers "". */` |
|      10 | 1432 | `	ph7_result_string(pCtx,"",0);` |
|       - | 1433 | `	/* Encode the string */` |
|       6 | 1434 | `	for(;;){` |
|      14 | 1435 | `		if( zIn >= zEnd ){` |
|       - | 1436 | `			/* No more input */` |
|       8 | 1437 | `			break;` |
|       - | 1438 | `		}` |
|       7 | 1439 | `		zCur = zIn;` |
|      17 | 1440 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|      11 | 1441 | `			zIn++;` |
|       1 | 1442 | `		}` |
|       7 | 1443 | `		if( zIn > zCur ){` |
|       - | 1444 | `			/* Append raw contents */` |
|       5 | 1445 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|       2 | 1446 | `		}` |
|       7 | 1447 | `		if( zIn >= zEnd ){` |
|       - | 1448 | `			/* No backslash in the tail: the raw append above took all of it. */` |
|       3 | 1449 | `			break;` |
|       - | 1450 | `		}` |
|       - | 1451 | `		/* zIn is ON a backslash. php drops it and takes the NEXT byte LITERALLY,` |
|       - | 1452 | ``		 * whatever that byte is -- `\t` is a `t`, `\\` is one `\`, `\'` is a `'` --`` |
|       - | 1453 | `		 * and a lone trailing backslash is dropped with nothing after it.` |
|       - | 1454 | `		 *` |
|       - | 1455 | `		 * The old shape only skipped the backslash when the next byte was one of` |
|       - | 1456 | `` 		 * `'`, `"` or `\`, and otherwise advanced NOTHING: `stripslashes(" some\thing ")` `` |
|       - | 1457 | ``		 * spun forever on the `\t`. It is reachable from any input a user types, and`` |
|       - | 1458 | ``		 * it is what hung Respect\Validation's `v::after('stripslashes', ...)`. */`` |
|       5 | 1459 | `		zIn++;` |
|       5 | 1460 | `		if( zIn < zEnd ){` |
|       3 | 1461 | `			if( zIn[0] == '0' ){` |
|       - | 1462 | ``				/* php's ONE special case: `\0` is a NUL byte, not the digit zero`` |
|       - | 1463 | ``				 * (php_stripslashes' `case '0'`). */`` |
|     ! 0 | 1464 | `				ph7_result_string(pCtx,"\0",1);` |
|     ! 0 | 1465 | `			}else{` |
|       3 | 1466 | `				ph7_result_string(pCtx,zIn,1);` |
|       - | 1467 | `			}` |
|       3 | 1468 | `			zIn++;` |
|       1 | 1469 | `		}` |
|       1 | 1470 | `	}` |
|      10 | 1471 | `	return PH7_OK;` |
|       6 | 1472 | `}` |
|       - | 1473 | `/*` |
|       - | 1474 | ` * A hex digit, decided by VALUE rather than by the C library: isxdigit() (which` |
|       - | 1475 | ` * SyisHex() is) reads the current locale and is undefined for the negative` |
|       - | 1476 | `` * `char` a high byte becomes on a signed-char platform. The two decoders below`` |
|       - | 1477 | ` * walk arbitrary BYTES, so both matter.` |
|       - | 1478 | ` */` |
|      82 | 1479 | `static int StrIsHexDigit(int c)` |
|       1 | 1480 | `{` |
|      83 | 1481 | `	c &= 0xFF;` |
|      83 | 1482 | `	return (c >= '0' && c <= '9') \|\| (c >= 'a' && c <= 'f') \|\| (c >= 'A' && c <= 'F');` |
|       1 | 1483 | `}` |
|      48 | 1484 | `static int StrHexDigitVal(int c)` |
|       1 | 1485 | `{` |
|      49 | 1486 | `	c &= 0xFF;` |
|      49 | 1487 | `	if( c >= '0' && c <= '9' ){` |
|      31 | 1488 | `		return c - '0';` |
|       - | 1489 | `	}` |
|      19 | 1490 | `	return (c \| 0x20) - 'a' + 10;` |
|      25 | 1491 | `}` |
|       - | 1492 | `/*` |
|       - | 1493 | ` * string stripcslashes(string $string)` |
|       - | 1494 | ` *  Un-quote a string quoted with addcslashes().` |
|       - | 1495 | ` * Return` |
|       - | 1496 | ` *  The un-escaped string.` |
|       - | 1497 | ` *` |
|       - | 1498 | ` * php's php_stripcslashes, byte for byte. Three readings share the escape:` |
|       - | 1499 | `` * the named C escapes, `\xHH` with ONE or TWO hex digits, and an octal run of`` |
|       - | 1500 | ` * at most THREE digits — and a backslash before anything else simply drops,` |
|       - | 1501 | `` * which is what makes `\8` an 8 and `\e` an e. Both numeric readings take the`` |
|       - | 1502 | `` * low byte of what they add up to, so `\400` is a NUL and `\777` a 0xFF; a`` |
|       - | 1503 | ` * trailing backslash with nothing behind it is kept.` |
|       - | 1504 | ` */` |
|      58 | 1505 | `PH7_PRIVATE int PH7_builtin_stripcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1506 | `{` |
|       - | 1507 | `	const char *zIn,*zEnd,*zCur;` |
|       - | 1508 | `	int nLen;` |
|      59 | 1509 | `	if( nArg < 1 ){` |
|     ! 0 | 1510 | `		ph7_result_string(pCtx,"",0);` |
|     ! 0 | 1511 | `		return PH7_OK;` |
|       - | 1512 | `	}` |
|      59 | 1513 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|       - | 1514 | `	/* Seed an empty result: the loop only ever appends. */` |
|      59 | 1515 | `	ph7_result_string(pCtx,"",0);` |
|      59 | 1516 | `	if( zIn == 0 \|\| nLen < 1 ){` |
|       3 | 1517 | `		return PH7_OK;` |
|       - | 1518 | `	}` |
|      57 | 1519 | `	zEnd = &zIn[nLen];` |
|    1121 | 1520 | `	while( zIn < zEnd ){` |
|       - | 1521 | `		int c;` |
|    1065 | 1522 | `		if( zIn[0] != '\\' \|\| &zIn[1] >= zEnd ){` |
|       - | 1523 | `			/* A run with no escape in it (a trailing backslash included) */` |
|      17 | 1524 | `			zCur = zIn;` |
|      49 | 1525 | `			while( zIn < zEnd && (zIn[0] != '\\' \|\| &zIn[1] >= zEnd) ){` |
|      25 | 1526 | `				zIn++;` |
|       1 | 1527 | `			}` |
|      17 | 1528 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      17 | 1529 | `			continue;` |
|       - | 1530 | `		}` |
|    1049 | 1531 | `		zIn++; /* Step over the backslash */` |
|    1049 | 1532 | `		c = (unsigned char)zIn[0];` |
|    1049 | 1533 | `		switch( c ){` |
|      11 | 1534 | `			case 'n': c = '\n'; break;` |
|      11 | 1535 | `			case 'r': c = '\r'; break;` |
|      11 | 1536 | `			case 'a': c = '\a'; break;` |
|      11 | 1537 | `			case 't': c = '\t'; break;` |
|      11 | 1538 | `			case 'v': c = '\v'; break;` |
|      11 | 1539 | `			case 'b': c = '\b'; break;` |
|      11 | 1540 | `			case 'f': c = '\f'; break;` |
|       7 | 1541 | `			case '\\': c = '\\'; break;` |
|       8 | 1542 | `			case 'x':` |
|      17 | 1543 | `				if( &zIn[1] < zEnd && StrIsHexDigit(zIn[1]) ){` |
|       9 | 1544 | `					int iVal = StrHexDigitVal(zIn[1]);` |
|       9 | 1545 | `					zIn++;` |
|       9 | 1546 | `					if( &zIn[1] < zEnd && StrIsHexDigit(zIn[1]) ){` |
|       5 | 1547 | `						iVal = (iVal<<4) \| StrHexDigitVal(zIn[1]);` |
|       5 | 1548 | `						zIn++;` |
|       2 | 1549 | `					}` |
|       9 | 1550 | `					c = iVal & 0xFF;` |
|       9 | 1551 | `					break;` |
|       - | 1552 | `				}` |
|       - | 1553 | ``				/* No hex digit behind the `x`: fall through to the octal`` |
|       - | 1554 | `				 * reading, which finds no octal digit either and keeps the` |
|       - | 1555 | ``				 * `x` itself. */`` |
|       - | 1556 | `				/* fall through */` |
|       - | 1557 | `			default: {` |
|     965 | 1558 | `				int i = 0, iVal = 0;` |
|    2855 | 1559 | `				while( zIn < zEnd && zIn[0] >= '0' && zIn[0] <= '7' && i < 3 ){` |
|    1891 | 1560 | `					iVal = (iVal<<3) \| (zIn[0] - '0');` |
|    1891 | 1561 | `					zIn++;` |
|    1891 | 1562 | `					i++;` |
|       1 | 1563 | `				}` |
|     965 | 1564 | `				if( i > 0 ){` |
|     643 | 1565 | `					c = iVal & 0xFF;` |
|     643 | 1566 | `					zIn--; /* The loop below steps over the last digit */` |
|     322 | 1567 | `				}else{` |
|     323 | 1568 | `					c = (unsigned char)zIn[0];` |
|       - | 1569 | `				}` |
|     964 | 1570 | `				break;` |
|       - | 1571 | `			}` |
|       - | 1572 | `		}` |
|       - | 1573 | `		{` |
|    1049 | 1574 | `			char zByte = (char)c;` |
|    1049 | 1575 | `			ph7_result_string(pCtx,&zByte,1);` |
|       - | 1576 | `		}` |
|    1049 | 1577 | `		zIn++;` |
|       1 | 1578 | `	}` |
|      57 | 1579 | `	return PH7_OK;` |
|      30 | 1580 | `}` |
|       - | 1581 | `/*` |
|       - | 1582 | ` * php's quoted-printable line limit (PHP_QPRINT_MAXL): the length a line may` |
|       - | 1583 | ` * reach before the encoder breaks it with a soft line break.` |
|       - | 1584 | ` */` |
|       - | 1585 | `#define PH7_QPRINT_MAXL 75` |
|       - | 1586 | `/*` |
|       - | 1587 | ` * string quoted_printable_encode(string $string)` |
|       - | 1588 | ` *  Convert an 8 bit string to a quoted-printable string.` |
|       - | 1589 | ` * Return` |
|       - | 1590 | ` *  The encoded string.` |
|       - | 1591 | ` *` |
|       - | 1592 | ` * php's php_quot_print_encode. A CRLF PAIR passes through untouched and resets` |
|       - | 1593 | `` * the line; everything else is either encoded as `=HH` (a control byte, DEL,`` |
|       - | 1594 | `` * any high byte, `=` itself, and a SPACE that stands before a CR) or copied.`` |
|       - | 1595 | ` * The line break is decided BEFORE the byte is written, and the room the` |
|       - | 1596 | ` * encoder demands for a high byte is its UTF-8 sequence's whole width rather` |
|       - | 1597 | ` * than the three characters it is about to write -- which is why a run of` |
|       - | 1598 | ` * two-byte characters breaks at 72 and not at 75.` |
|       - | 1599 | ` */` |
|      42 | 1600 | `PH7_PRIVATE int PH7_builtin_quoted_printable_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1601 | `{` |
|       - | 1602 | `	static const char zHex[] = "0123456789ABCDEF";` |
|       - | 1603 | `	const char *zIn,*zEnd;` |
|      43 | 1604 | `	sxu32 nLine = 0;` |
|       - | 1605 | `	int nLen;` |
|      43 | 1606 | `	if( nArg < 1 ){` |
|     ! 0 | 1607 | `		ph7_result_string(pCtx,"",0);` |
|     ! 0 | 1608 | `		return PH7_OK;` |
|       - | 1609 | `	}` |
|      43 | 1610 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      43 | 1611 | `	ph7_result_string(pCtx,"",0);` |
|      43 | 1612 | `	if( zIn == 0 \|\| nLen < 1 ){` |
|       3 | 1613 | `		return PH7_OK;` |
|       - | 1614 | `	}` |
|      41 | 1615 | `	zEnd = &zIn[nLen];` |
|     825 | 1616 | `	while( zIn < zEnd ){` |
|     785 | 1617 | `		int c = (unsigned char)zIn[0];` |
|       - | 1618 | `		/* The byte behind this one; php reads its NUL terminator past the end. */` |
|     785 | 1619 | `		int cNext = (&zIn[1] < zEnd) ? (unsigned char)zIn[1] : 0;` |
|     785 | 1620 | `		zIn++;` |
|     785 | 1621 | `		if( c == '\r' && cNext == '\n' ){` |
|       5 | 1622 | `			ph7_result_string(pCtx,"\r\n",2);` |
|       5 | 1623 | `			zIn++;` |
|       5 | 1624 | `			nLine = 0;` |
|       5 | 1625 | `			continue;` |
|       - | 1626 | `		}` |
|     822 | 1627 | `		if( c < 32 \|\| c == 127 \|\| c > 127 \|\| c == '=' \|\| (c == ' ' && cNext == '\r') ){` |
|       - | 1628 | `			char zEsc[3];` |
|      83 | 1629 | `			nLine += 3;` |
|      82 | 1630 | `			if( (nLine > PH7_QPRINT_MAXL && c <= 127)` |
|      82 | 1631 | `			 \|\| (c > 127 && c <= 0xDF && nLine + 3 > PH7_QPRINT_MAXL)` |
|     108 | 1632 | `			 \|\| (c > 0xDF && c <= 0xEF && nLine + 6 > PH7_QPRINT_MAXL)` |
|      81 | 1633 | `			 \|\| (c > 0xEF && c <= 0xF4 && nLine + 9 > PH7_QPRINT_MAXL) ){` |
|      57 | 1634 | `				ph7_result_string(pCtx,"=\r\n",3);` |
|      57 | 1635 | `				nLine = 3;` |
|      55 | 1636 | `			}` |
|      83 | 1637 | `			zEsc[0] = '=';` |
|      83 | 1638 | `			zEsc[1] = zHex[c>>4];` |
|      83 | 1639 | `			zEsc[2] = zHex[c&0x0F];` |
|      83 | 1640 | `			ph7_result_string(pCtx,zEsc,3);` |
|      42 | 1641 | `		}else{` |
|     699 | 1642 | `			char zByte = (char)c;` |
|     699 | 1643 | `			if( ++nLine > PH7_QPRINT_MAXL ){` |
|       5 | 1644 | `				ph7_result_string(pCtx,"=\r\n",3);` |
|       5 | 1645 | `				nLine = 1;` |
|       2 | 1646 | `			}` |
|     699 | 1647 | `			ph7_result_string(pCtx,&zByte,1);` |
|       - | 1648 | `		}` |
|       1 | 1649 | `	}` |
|      41 | 1650 | `	return PH7_OK;` |
|      22 | 1651 | `}` |
|       - | 1652 | `/*` |
|       - | 1653 | ` * string quoted_printable_decode(string $string)` |
|       - | 1654 | ` *  Convert a quoted-printable string to an 8 bit string.` |
|       - | 1655 | ` * Return` |
|       - | 1656 | ` *  The decoded string.` |
|       - | 1657 | ` *` |
|       - | 1658 | `` * php's php_quot_print_decode, with the `_`-to-space replacement left off (it`` |
|       - | 1659 | `` * belongs to the MIME header decoder, not to this name). An `=` reads three`` |
|       - | 1660 | ` * ways: two hex digits are a byte, an end-of-line -- optionally behind a run of` |
|       - | 1661 | ` * spaces and tabs -- is a soft break that vanishes, and anything else leaves` |
|       - | 1662 | `` * the `=` standing as itself with the bytes behind it read normally. php walks`` |
|       - | 1663 | ` * a C string here, so a NUL byte ENDS the answer.` |
|       - | 1664 | ` */` |
|      46 | 1665 | `PH7_PRIVATE int PH7_builtin_quoted_printable_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1666 | `{` |
|       - | 1667 | `	const char *zIn,*zEnd,*zCur;` |
|       - | 1668 | `	int nLen;` |
|      47 | 1669 | `	if( nArg < 1 ){` |
|     ! 0 | 1670 | `		ph7_result_string(pCtx,"",0);` |
|     ! 0 | 1671 | `		return PH7_OK;` |
|       - | 1672 | `	}` |
|      47 | 1673 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      47 | 1674 | `	ph7_result_string(pCtx,"",0);` |
|      47 | 1675 | `	if( zIn == 0 \|\| nLen < 1 ){` |
|     ! 0 | 1676 | `		return PH7_OK;` |
|       - | 1677 | `	}` |
|      47 | 1678 | `	zEnd = &zIn[nLen];` |
|     131 | 1679 | `	while( zIn < zEnd && zIn[0] != 0 ){` |
|       - | 1680 | `		char zByte;` |
|      91 | 1681 | `		if( zIn[0] != '=' ){` |
|       - | 1682 | `			/* A run with no escape and no NUL in it */` |
|      43 | 1683 | `			zCur = zIn;` |
|     103 | 1684 | `			while( zIn < zEnd && zIn[0] != '=' && zIn[0] != 0 ){` |
|      61 | 1685 | `				zIn++;` |
|       1 | 1686 | `			}` |
|      43 | 1687 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      62 | 1688 | `			continue;` |
|       - | 1689 | `		}` |
|      49 | 1690 | `		zIn++; /* Step over the '=' */` |
|      49 | 1691 | `		if( zIn >= zEnd \|\| zIn[0] == 0 ){` |
|       - | 1692 | `			/* A trailing '=' is dropped */` |
|       3 | 1693 | `			break;` |
|       - | 1694 | `		}` |
|      45 | 1695 | `		if( StrIsHexDigit(zIn[0]) ){` |
|      23 | 1696 | `			if( &zIn[1] < zEnd && StrIsHexDigit(zIn[1]) ){` |
|      19 | 1697 | `				zByte = (char)((StrHexDigitVal(zIn[0])<<4) \| StrHexDigitVal(zIn[1]));` |
|      19 | 1698 | `				ph7_result_string(pCtx,&zByte,1);` |
|      19 | 1699 | `				zIn += 2;` |
|      10 | 1700 | `			}else{` |
|       - | 1701 | `				/* One hex digit alone: the '=' stands as itself */` |
|       5 | 1702 | `				ph7_result_string(pCtx,"=",1);` |
|       - | 1703 | `			}` |
|      23 | 1704 | `			continue;` |
|       - | 1705 | `		}` |
|      23 | 1706 | `		if( zIn[0] == ' ' \|\| zIn[0] == '\t' ){` |
|       - | 1707 | `			/* A soft break may stand behind a run of spaces and tabs; when no` |
|       - | 1708 | `			 * end-of-line follows the run, the '=' is an ordinary byte and the` |
|       - | 1709 | `			 * run is read normally. */` |
|      11 | 1710 | `			zCur = zIn;` |
|      26 | 1711 | `			while( zCur < zEnd && (zCur[0] == ' ' \|\| zCur[0] == '\t') ){` |
|      11 | 1712 | `				zCur++;` |
|       1 | 1713 | `			}` |
|      11 | 1714 | `			if( zCur < zEnd && zCur[0] != 0 && zCur[0] != '\r' && zCur[0] != '\n' ){` |
|       5 | 1715 | `				ph7_result_string(pCtx,"=",1);` |
|       5 | 1716 | `				continue;` |
|       - | 1717 | `			}` |
|       7 | 1718 | `			zIn = zCur;` |
|       7 | 1719 | `			if( zIn >= zEnd \|\| zIn[0] == 0 ){` |
|       2 | 1720 | `				break;` |
|       - | 1721 | `			}` |
|       2 | 1722 | `		}` |
|      17 | 1723 | `		if( zIn[0] == '\r' ){` |
|      11 | 1724 | `			zIn++;` |
|      11 | 1725 | `			if( zIn < zEnd && zIn[0] == '\n' ){` |
|       9 | 1726 | `				zIn++;` |
|       4 | 1727 | `			}` |
|      11 | 1728 | `			continue;` |
|       - | 1729 | `		}` |
|       7 | 1730 | `		if( zIn[0] == '\n' ){` |
|       3 | 1731 | `			zIn++;` |
|       3 | 1732 | `			continue;` |
|       - | 1733 | `		}` |
|       - | 1734 | `		/* Anything else: the '=' stands as itself */` |
|       5 | 1735 | `		ph7_result_string(pCtx,"=",1);` |
|       1 | 1736 | `	}` |
|      47 | 1737 | `	return PH7_OK;` |
|      24 | 1738 | `}` |
|       - | 1739 | `/*` |
|       - | 1740 | ` * UTF-8-aware HTML entity machinery, shared by htmlspecialchars/htmlentities/` |
|       - | 1741 | ` * htmlspecialchars_decode/html_entity_decode/get_html_translation_table.` |
|       - | 1742 | ` * The implementations live further down in this file, next to the filter_var` |
|       - | 1743 | ` * FULL_SPECIAL_CHARS machinery they reuse (aHtml401Ent[]/FvHtml401Lookup()/` |
|       - | 1744 | ` * FvUtf8Next()). Semantics are byte-exact vs php 8.5.7; PHL is UTF-8-only` |
|       - | 1745 | ` * so every charset argument other than a UTF-8 alias gets PHP's` |
|       - | 1746 | ` * unsupported-charset warning and is treated as UTF-8.` |
|       - | 1747 | ` *` |
|       - | 1748 | ` * Flag model (the PHP-exact ENT_* values, see constant.c): bit 1 = encode/` |
|       - | 1749 | ` * decode single quotes, bit 2 = double quotes (ENT_QUOTES=3, ENT_COMPAT=2,` |
|       - | 1750 | ` * ENT_NOQUOTES=0); bits 16\|32 select the doctype (0=HTML401, 16=XML1,` |
|       - | 1751 | ` * 32=XHTML, 48=HTML5); ENT_IGNORE=4 drops invalid UTF-8 bytes (wins over` |
|       - | 1752 | ` * ENT_SUBSTITUTE=8, which replaces each with U+FFFD; with neither set the` |
|       - | 1753 | ` * whole result collapses to ""); ENT_DISALLOWED=128 substitutes valid but` |
|       - | 1754 | ` * doctype-disallowed codepoints. The shared default is` |
|       - | 1755 | ` * ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 = 11.` |
|       - | 1756 | ` */` |
|       - | 1757 | `/*` |
|       - | 1758 | ` * string htmlspecialchars(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|       - | 1759 | ` *                         [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|       - | 1760 | ` *  Convert the special characters & < > " ' to HTML entities.` |
|       - | 1761 | ` * Return` |
|       - | 1762 | ` *  The escaped string or NULL on failure.` |
|       - | 1763 | ` */` |
|      80 | 1764 | `PH7_PRIVATE int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1765 | `{` |
|      83 | 1766 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|       - | 1767 | `	const char *zIn;` |
|      83 | 1768 | `	int nLen,bDouble = 1,iCs;` |
|       - | 1769 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|       - | 1770 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|      83 | 1771 | `	if( nArg < 1 ){` |
|       - | 1772 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 1773 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1774 | `		return PH7_OK;` |
|       - | 1775 | `	}` |
|       - | 1776 | `	/* Extract the target string */` |
|      83 | 1777 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      83 | 1778 | `	if( nArg > 1 ){` |
|      73 | 1779 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      35 | 1780 | `	}` |
|      83 | 1781 | `	iCs = HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|      83 | 1782 | `	if( nArg > 3 ){` |
|      10 | 1783 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|       4 | 1784 | `	}` |
|      83 | 1785 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,0,bDouble,iCs);` |
|      83 | 1786 | `	return PH7_OK;` |
|      43 | 1787 | `}` |
|       - | 1788 | `/*` |
|       - | 1789 | ` * string htmlspecialchars_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401])` |
|       - | 1790 | ` *  Convert the special HTML entities (&amp; &lt; &gt; &quot; and the` |
|       - | 1791 | ` *  numeric/doctype forms of the two quotes) back to characters.` |
|       - | 1792 | ` * Return` |
|       - | 1793 | ` *  The unescaped string or NULL on failure.` |
|       - | 1794 | ` */` |
|      22 | 1795 | `PH7_PRIVATE int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1796 | `{` |
|      23 | 1797 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|       - | 1798 | `	const char *zIn;` |
|       - | 1799 | `	int nLen;` |
|       - | 1800 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|       - | 1801 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|      23 | 1802 | `	if( nArg < 1 ){` |
|       - | 1803 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 1804 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1805 | `		return PH7_OK;` |
|       - | 1806 | `	}` |
|       - | 1807 | `	/* Extract the target string */` |
|      23 | 1808 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      23 | 1809 | `	if( nArg > 1 ){` |
|       9 | 1810 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|       4 | 1811 | `	}` |
|       - | 1812 | `	/* htmlspecialchars_decode() takes no charset: the five specials are ASCII in` |
|       - | 1813 | `	 * every charset php models here. */` |
|      23 | 1814 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,0,PH7_HTML_CS_UTF8);` |
|      23 | 1815 | `	return PH7_OK;` |
|      12 | 1816 | `}` |
|       - | 1817 | `/*` |
|       - | 1818 | ` * array get_html_translation_table(int $table = HTML_SPECIALCHARS` |
|       - | 1819 | ` *      [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 [, string $encoding = "UTF-8"]])` |
|       - | 1820 | ` *  Return the translation table used by htmlspecialchars() (HTML_SPECIALCHARS)` |
|       - | 1821 | ` *  or htmlentities() (HTML_ENTITIES) as character => entity pairs.` |
|       - | 1822 | ` * Return` |
|       - | 1823 | ` *  The translation table as an array or NULL on failure.` |
|       - | 1824 | ` */` |
|      44 | 1825 | `PH7_PRIVATE int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1826 | `{` |
|      47 | 1827 | `	int iTable = 0; /* HTML_SPECIALCHARS */` |
|      47 | 1828 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      47 | 1829 | `	if( nArg > 0 ){` |
|      45 | 1830 | `		iTable = ph7_value_to_int(apArg[0]);` |
|      21 | 1831 | `	}` |
|      47 | 1832 | `	if( nArg > 1 ){` |
|      43 | 1833 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      20 | 1834 | `	}` |
|      47 | 1835 | `	HtmlTranslationTable(pCtx,iTable,iFlags,HtmlCheckCharset(pCtx,nArg,apArg,2));` |
|      47 | 1836 | `	return PH7_OK;` |
|       3 | 1837 | `}` |
|       - | 1838 | `/*` |
|       - | 1839 | ` * string htmlentities(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|       - | 1840 | ` *                     [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|       - | 1841 | ` *  Convert all applicable characters to HTML entities: the specials plus` |
|       - | 1842 | ` *  every codepoint with an HTML 4.01 named entity (aHtml401Ent[]).` |
|       - | 1843 | ` * Return` |
|       - | 1844 | ` *  The encoded string or NULL on failure.` |
|       - | 1845 | ` */` |
|      62 | 1846 | `PH7_PRIVATE int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1847 | `{` |
|      64 | 1848 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|       - | 1849 | `	const char *zIn;` |
|      64 | 1850 | `	int nLen,bDouble = 1,iCs;` |
|       - | 1851 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|       - | 1852 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|      64 | 1853 | `	if( nArg < 1 ){` |
|       - | 1854 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 1855 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1856 | `		return PH7_OK;` |
|       - | 1857 | `	}` |
|       - | 1858 | `	/* Extract the target string */` |
|      64 | 1859 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      64 | 1860 | `	if( nArg > 1 ){` |
|      52 | 1861 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      25 | 1862 | `	}` |
|      64 | 1863 | `	iCs = HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|      64 | 1864 | `	if( nArg > 3 ){` |
|       6 | 1865 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|       2 | 1866 | `	}` |
|      64 | 1867 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,1,bDouble,iCs);` |
|      64 | 1868 | `	return PH7_OK;` |
|      33 | 1869 | `}` |
|       - | 1870 | `/*` |
|       - | 1871 | ` * string html_entity_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|       - | 1872 | ` *                           [, string $encoding = "UTF-8"]])` |
|       - | 1873 | ` *  Convert HTML entities (named — case-sensitive — and numeric, decimal or` |
|       - | 1874 | ` *  hex) back to their UTF-8 characters. The reverse of htmlentities().` |
|       - | 1875 | ` * Return` |
|       - | 1876 | ` *  The decoded string or NULL on failure.` |
|       - | 1877 | ` */` |
|      66 | 1878 | `PH7_PRIVATE int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1879 | `{` |
|      69 | 1880 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|       - | 1881 | `	const char *zIn;` |
|       - | 1882 | `	int nLen;` |
|       - | 1883 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|       - | 1884 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|      69 | 1885 | `	if( nArg < 1 ){` |
|       - | 1886 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 1887 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1888 | `		return PH7_OK;` |
|       - | 1889 | `	}` |
|       - | 1890 | `	/* Extract the target string */` |
|      69 | 1891 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      69 | 1892 | `	if( nArg > 1 ){` |
|      37 | 1893 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      17 | 1894 | `	}` |
|      69 | 1895 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,1,HtmlCheckCharset(pCtx,nArg,apArg,2));` |
|      69 | 1896 | `	return PH7_OK;` |
|      36 | 1897 | `}` |
|       - | 1898 | `/*` |
|       - | 1899 | ` * int strlen($string)` |
|       - | 1900 | ` *  return the length of the given string.` |
|       - | 1901 | ` * Parameter` |
|       - | 1902 | ` *  string: The string being measured for length.` |
|       - | 1903 | ` * Return` |
|       - | 1904 | ` *  length of the given string.` |
|       - | 1905 | ` */` |
| 4709379 | 1906 | `PH7_PRIVATE int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1907 | `{` |
| 4709384 | 1908 | `	int iLen = 0;` |
| 4709384 | 1909 | `	if( nArg > 0 ){` |
| 4709384 | 1910 | `		StrNullArgNotice(pCtx,apArg[0],"strlen",1,"$string");` |
| 4709384 | 1911 | `		ph7_value_to_string(apArg[0],&iLen);` |
| 2355962 | 1912 | `	}` |
|       - | 1913 | `	/* String length */` |
| 4709384 | 1914 | `	ph7_result_int(pCtx,iLen);` |
| 4709384 | 1915 | `	return PH7_OK;` |
|       5 | 1916 | `}` |
|       - | 1917 | `/*` |
|       - | 1918 | ` * int strcmp(string $str1,string $str2)` |
|       - | 1919 | ` *  Perform a binary safe string comparison.` |
|       - | 1920 | ` * Parameter` |
|       - | 1921 | ` *  str1: The first string` |
|       - | 1922 | ` *  str2: The second string` |
|       - | 1923 | ` * Return` |
|       - | 1924 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|       - | 1925 | ` *  than str2, and 0 if they are equal.` |
|       - | 1926 | ` */` |
|     224 | 1927 | `PH7_PRIVATE int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 1928 | `{` |
|       - | 1929 | `	const char *z1,*z2;` |
|       - | 1930 | `	int n1,n2;` |
|       - | 1931 | `	int res;` |
|     228 | 1932 | `	if( nArg < 2 ){` |
|     ! 0 | 1933 | `		res = nArg == 0 ? 0 : 1;` |
|     ! 0 | 1934 | `		ph7_result_int(pCtx,res);` |
|     ! 0 | 1935 | `		return PH7_OK;` |
|       - | 1936 | `	}` |
|       - | 1937 | `	/* Perform the comparison */` |
|     228 | 1938 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     228 | 1939 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     228 | 1940 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|       - | 1941 | `	/* Comparison result */` |
|     228 | 1942 | `	ph7_result_int(pCtx,res);` |
|     228 | 1943 | `	return PH7_OK;` |
|      91 | 1944 | `}` |
|       - | 1945 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|       - | 1946 | `/*` |
|       - | 1947 | ` * The natural-order comparison core lives OUTSIDE the PH7_DISABLE_BUILTIN_FUNC` |
|       - | 1948 | ` * guard: hashmap.c's SORT_NATURAL path (always compiled) calls PH7_StrNatCmp, so` |
|       - | 1949 | ` * it must exist in the tiny build too. [[tiny-build-disk-io-guard-fragility]]` |
|       - | 1950 | ` */` |
|       - | 1951 | `/*` |
|       - | 1952 | ` * Natural-order comparison core (Martin Pool's natcompare as adapted by php's` |
|       - | 1953 | ` * ext/standard/strnatcmp.c): digit runs compare numerically — the longer run` |
|       - | 1954 | ` * wins, a leading zero flips to fractional first-difference-wins semantics —` |
|       - | 1955 | ` * everything else compares bytewise with whitespace skipped.` |
|       - | 1956 | ` */` |
|     162 | 1957 | `static int StrNatCompareRight(const char **pa,const char *aEnd,const char **pb,const char *bEnd)` |
|       3 | 1958 | `{` |
|     165 | 1959 | `	int bias = 0;` |
|     269 | 1960 | `	for(;;){` |
|     353 | 1961 | `		int da = (*pa < aEnd) && SyisDigit(**pa);` |
|     353 | 1962 | `		int db = (*pb < bEnd) && SyisDigit(**pb);` |
|     353 | 1963 | `		if( !da && !db ){ return bias; }` |
|     285 | 1964 | `		if( !da ){ return -1; }` |
|     261 | 1965 | `		if( !db ){ return 1; }` |
|     191 | 1966 | `		if( **pa < **pb ){ if( !bias ){ bias = -1; } }` |
|     121 | 1967 | `		else if( **pa > **pb ){ if( !bias ){ bias = 1; } }` |
|     191 | 1968 | `		(*pa)++;` |
|     191 | 1969 | `		(*pb)++;` |
|       3 | 1970 | `	}` |
|      84 | 1971 | `}` |
|       4 | 1972 | `static int StrNatCompareLeft(const char **pa,const char *aEnd,const char **pb,const char *bEnd)` |
|       1 | 1973 | `{` |
|       2 | 1974 | `	for(;;){` |
|       5 | 1975 | `		int da = (*pa < aEnd) && SyisDigit(**pa);` |
|       5 | 1976 | `		int db = (*pb < bEnd) && SyisDigit(**pb);` |
|       5 | 1977 | `		if( !da && !db ){ return 0; }` |
|       5 | 1978 | `		if( !da ){ return -1; }` |
|       5 | 1979 | `		if( !db ){ return 1; }` |
|       5 | 1980 | `		if( **pa < **pb ){ return -1; }` |
|     ! 0 | 1981 | `		if( **pa > **pb ){ return 1; }` |
|     ! 0 | 1982 | `		(*pa)++;` |
|     ! 0 | 1983 | `		(*pb)++;` |
|     ! 0 | 1984 | `	}` |
|       3 | 1985 | `}` |
|     256 | 1986 | `PH7_PRIVATE int PH7_StrNatCmp(const char *zA,int nA,const char *zB,int nB,int bFold)` |
|       4 | 1987 | `{` |
|     260 | 1988 | `	const char *a = zA,*aEnd = &zA[nA];` |
|     260 | 1989 | `	const char *b = zB,*bEnd = &zB[nB];` |
|     622 | 1990 | `	for(;;){` |
|       - | 1991 | `		int ca,cb;` |
|     762 | 1992 | `		while( a < aEnd && SyisSpace(a[0]) ){ a++; }` |
|     760 | 1993 | `		while( b < bEnd && SyisSpace(b[0]) ){ b++; }` |
|     760 | 1994 | `		ca = (a < aEnd) ? (unsigned char)a[0] : 0;` |
|     760 | 1995 | `		cb = (b < bEnd) ? (unsigned char)b[0] : 0;` |
|     760 | 1996 | `		if( SyisDigit(ca) && SyisDigit(cb) ){` |
|     167 | 1997 | `			int r = (ca == '0' \|\| cb == '0')` |
|       4 | 1998 | `				? StrNatCompareLeft(&a,aEnd,&b,bEnd)` |
|     245 | 1999 | `				: StrNatCompareRight(&a,aEnd,&b,bEnd);` |
|     169 | 2000 | `			if( r ){ return r; }` |
|      14 | 2001 | `			continue;` |
|       - | 2002 | `		}` |
|     594 | 2003 | `		if( ca == 0 && cb == 0 ){ return 0; }` |
|     572 | 2004 | `		if( bFold ){` |
|     256 | 2005 | `			ca = SyToLower(ca);` |
|     256 | 2006 | `			cb = SyToLower(cb);` |
|     126 | 2007 | `		}` |
|     572 | 2008 | `		if( ca < cb ){ return -1; }` |
|     548 | 2009 | `		if( ca > cb ){ return 1; }` |
|     492 | 2010 | `		a++;` |
|     492 | 2011 | `		b++;` |
|       4 | 2012 | `	}` |
|     132 | 2013 | `}` |
|       - | 2014 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|       - | 2015 | `/*` |
|       - | 2016 | ` * int strnatcmp(string $string1, string $string2)` |
|       - | 2017 | ` * int strnatcasecmp(string $string1, string $string2)` |
|       - | 2018 | ` *  Natural-order string comparison ("img2" < "img10"), case folded for the` |
|       - | 2019 | ` *  latter. php 8.2+ normalizes the result to -1/0/1.` |
|       - | 2020 | ` */` |
|      64 | 2021 | `PH7_PRIVATE int PH7_builtin_strnatcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2022 | `{` |
|       - | 2023 | `	const char *z1,*z2,*zFunc;` |
|       - | 2024 | `	int n1,n2,bFold;` |
|      66 | 2025 | `	if( nArg < 2 ){` |
|     ! 0 | 2026 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 2027 | `		return PH7_OK;` |
|       - | 2028 | `	}` |
|      66 | 2029 | `	zFunc = ph7_function_name(pCtx);` |
|       - | 2030 | `	/* Both names carry a 'c' at that offset -- "strnat\|c\|mp" as much as` |
|       - | 2031 | `	 * "strnat\|c\|asecmp" -- so testing it alone made strnatcmp() fold as well, and` |
|       - | 2032 | `	 * natsort()/ArrayObject::natsort() (prelude wrappers over strnatcmp) with it:` |
|       - | 2033 | `	 * 'Hello' and 'hello' compared EQUAL where php answers -1. */` |
|      66 | 2034 | `	bFold = SyStrnicmp(zFunc,"strnatcase",sizeof("strnatcase")-1) == 0;` |
|      66 | 2035 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|      66 | 2036 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|      66 | 2037 | `	ph7_result_int(pCtx,PH7_StrNatCmp(z1,n1,z2,n2,bFold));` |
|      66 | 2038 | `	return PH7_OK;` |
|      34 | 2039 | `}` |
|       - | 2040 | `/*` |
|       - | 2041 | ` * php's special version forms and their ordering` |
|       - | 2042 | ` * (compare_special_version_forms(), ext/standard/versioning.c). A form matches` |
|       - | 2043 | ` * a component by PREFIX -- "alpha3" is an alpha, "RC1" an RC -- and "#" is the` |
|       - | 2044 | ` * marker php compares a NUMERIC component as, spelled "#N#" at the call sites.` |
|       - | 2045 | ` * An unrecognized component ranks -1, BELOW dev.` |
|       - | 2046 | ` */` |
|       - | 2047 | `static const struct VersionForm {` |
|       - | 2048 | `	const char *zName;` |
|       - | 2049 | `	int nLen;` |
|       - | 2050 | `	int iOrder;` |
|       - | 2051 | `} aVersionForm[] = {` |
|       - | 2052 | `	{ "dev", 3, 0 }, { "alpha", 5, 1 }, { "a",  1, 1 }, { "beta", 4, 2 },` |
|       - | 2053 | `	{ "b",   1, 2 }, { "RC",    2, 3 }, { "rc", 2, 3 }, { "#",    1, 4 },` |
|       - | 2054 | `	{ "pl",  2, 5 }, { "p",     1, 5 },` |
|       - | 2055 | `};` |
|     144 | 2056 | `static int VersionFormOrder(const char *zPart)` |
|       1 | 2057 | `{` |
|       - | 2058 | `	sxu32 n;` |
|     977 | 2059 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVersionForm) ; ++n ){` |
|     955 | 2060 | `		if( SyStrncmp(zPart,aVersionForm[n].zName,(sxu32)aVersionForm[n].nLen) == 0 ){` |
|     123 | 2061 | `			return aVersionForm[n].iOrder;` |
|       - | 2062 | `		}` |
|     417 | 2063 | `	}` |
|      23 | 2064 | `	return -1;` |
|      73 | 2065 | `}` |
|      72 | 2066 | `static int VersionSpecialCmp(const char *zPart1,const char *zPart2)` |
|       1 | 2067 | `{` |
|      73 | 2068 | `	int iOrd1 = VersionFormOrder(zPart1);` |
|      73 | 2069 | `	int iOrd2 = VersionFormOrder(zPart2);` |
|      73 | 2070 | `	return iOrd1 < iOrd2 ? -1 : (iOrd1 > iOrd2 ? 1 : 0);` |
|       1 | 2071 | `}` |
|       - | 2072 | `/*` |
|       - | 2073 | ` * php_canonicalize_version(): '-', '_', '+' and every other non-alphanumeric` |
|       - | 2074 | ` * byte become '.', and a '.' is inserted at each digit<->non-digit boundary.` |
|       - | 2075 | ` * The FIRST byte is copied verbatim -- even a separator -- so "-1"` |
|       - | 2076 | ` * canonicalizes to "-.1" and not to "1", and its leading "-" then compares as` |
|       - | 2077 | ` * an unrecognized form. A trailing '.' is dropped rather than left as an empty` |
|       - | 2078 | ` * last component. zOut must hold 2*nLen + 2 bytes.` |
|       - | 2079 | ` */` |
|     518 | 2080 | `static void VersionCanonicalize(const char *zIn,sxu32 nLen,char *zOut)` |
|       1 | 2081 | `{` |
|       - | 2082 | `	const char *p,*pEnd;` |
|     519 | 2083 | `	char *q = zOut;` |
|       - | 2084 | `	int lp;` |
|     519 | 2085 | `	if( nLen < 1 ){` |
|     ! 0 | 2086 | `		zOut[0] = 0;` |
|     ! 0 | 2087 | `		return;` |
|       - | 2088 | `	}` |
|     519 | 2089 | `	lp = (unsigned char)zIn[0];` |
|     519 | 2090 | `	*q++ = (char)lp;` |
|     519 | 2091 | `	pEnd = &zIn[nLen];` |
|    1433 | 2092 | `	for( p = &zIn[1] ; p < pEnd ; lp = (unsigned char)*p++ ){` |
|     977 | 2093 | `		int c  = (unsigned char)p[0];` |
|     977 | 2094 | `		int lq = (unsigned char)q[-1];` |
|     977 | 2095 | `		if( c == '-' \|\| c == '_' \|\| c == '+' ){` |
|     181 | 2096 | `			if( lq != '.' ){ *q++ = '.'; }` |
|    1165 | 2097 | `		}else if( (!SyisDigit(lp) && lp != '.' && SyisDigit(c)) \|\|` |
|     846 | 2098 | `		          (SyisDigit(lp) && !SyisDigit(c) && c != '.') ){` |
|     199 | 2099 | `			if( lq != '.' ){ *q++ = '.'; }` |
|     109 | 2100 | `			*q++ = (char)c;` |
|     831 | 2101 | `		}else if( !SyisAlphaNum(c) ){` |
|     323 | 2102 | `			if( lq != '.' ){ *q++ = '.'; }` |
|     162 | 2103 | `		}else{` |
|     455 | 2104 | `			*q++ = (char)c;` |
|       - | 2105 | `		}` |
|     458 | 2106 | `	}` |
|     457 | 2107 | `	if( q[-1] == '.' ){` |
|      13 | 2108 | `		q[-1] = 0;` |
|       7 | 2109 | `	}else{` |
|     445 | 2110 | `		q[0] = 0;` |
|       - | 2111 | `	}` |
|     229 | 2112 | `}` |
|       - | 2113 | `/* strtol() over a canonical numeric component, saturating like the C library. */` |
|     680 | 2114 | `static sxi64 VersionPartToInt(const char *zPart)` |
|       1 | 2115 | `{` |
|     681 | 2116 | `	sxi64 iVal = 0;` |
|    1367 | 2117 | `	while( SyisDigit((unsigned char)zPart[0]) ){` |
|     687 | 2118 | `		if( iVal > (SXI64_HIGH - 9) / 10 ){` |
|     ! 0 | 2119 | `			return SXI64_HIGH;` |
|       - | 2120 | `		}` |
|     687 | 2121 | `		iVal = iVal * 10 + (zPart[0] - '0');` |
|     687 | 2122 | `		zPart++;` |
|       1 | 2123 | `	}` |
|     681 | 2124 | `	return iVal;` |
|     341 | 2125 | `}` |
|     824 | 2126 | `static char * VersionNextDot(char *zPart)` |
|       1 | 2127 | `{` |
|    1813 | 2128 | `	while( zPart[0] && zPart[0] != '.' ){ zPart++; }` |
|     825 | 2129 | `	return zPart[0] ? zPart : 0;` |
|       1 | 2130 | `}` |
|       - | 2131 | `/*` |
|       - | 2132 | ` * php_version_compare() over two CANONICAL buffers -- the caller has already` |
|       - | 2133 | ` * applied php's empty-operand shortcut to the ORIGINAL strings. Both buffers` |
|       - | 2134 | ` * are written in place (the walk NUL-terminates each component where php's` |
|       - | 2135 | ` * strchr does), so they must be writable copies.` |
|       - | 2136 | ` *` |
|       - | 2137 | ` * Where php recurses on the leftover of the longer version, this loops: the` |
|       - | 2138 | ` * recursion is a tail call, and its depth would otherwise grow with the` |
|       - | 2139 | ` * component count of an attacker-supplied string.` |
|       - | 2140 | ` */` |
|     232 | 2141 | `static int VersionCompareCanon(char *zV1,char *zV2)` |
|       1 | 2142 | `{` |
|     233 | 2143 | `	char zMark[] = "#N#";   /* php's "this component is a number" marker */` |
|     140 | 2144 | `	for(;;){` |
|       - | 2145 | `		char *p1,*p2,*n1,*n2;` |
|     257 | 2146 | `		int cmp = 0;` |
|     257 | 2147 | `		p1 = n1 = zV1;` |
|     257 | 2148 | `		p2 = n2 = zV2;` |
|     525 | 2149 | `		while( p1[0] && p2[0] && n1 && n2 ){` |
|     413 | 2150 | `			if( (n1 = VersionNextDot(p1)) != 0 ){ n1[0] = 0; }` |
|     413 | 2151 | `			if( (n2 = VersionNextDot(p2)) != 0 ){ n2[0] = 0; }` |
|     413 | 2152 | `			if( SyisDigit((unsigned char)p1[0]) && SyisDigit((unsigned char)p2[0]) ){` |
|     341 | 2153 | `				sxi64 l1 = VersionPartToInt(p1);` |
|     341 | 2154 | `				sxi64 l2 = VersionPartToInt(p2);` |
|     341 | 2155 | `				cmp = l1 < l2 ? -1 : (l1 > l2 ? 1 : 0);` |
|     243 | 2156 | `			}else if( !SyisDigit((unsigned char)p1[0]) && !SyisDigit((unsigned char)p2[0]) ){` |
|      59 | 2157 | `				cmp = VersionSpecialCmp(p1,p2);` |
|      44 | 2158 | `			}else if( SyisDigit((unsigned char)p1[0]) ){` |
|       5 | 2159 | `				cmp = VersionSpecialCmp(zMark,p2);` |
|       3 | 2160 | `			}else{` |
|      11 | 2161 | `				cmp = VersionSpecialCmp(p1,zMark);` |
|       - | 2162 | `			}` |
|     413 | 2163 | `			if( cmp != 0 ){ break; }` |
|     269 | 2164 | `			if( n1 ){ p1 = &n1[1]; }` |
|     269 | 2165 | `			if( n2 ){ p2 = &n2[1]; }` |
|       1 | 2166 | `		}` |
|     257 | 2167 | `		if( cmp != 0 ){` |
|     145 | 2168 | `			return cmp;` |
|       - | 2169 | `		}` |
|       - | 2170 | `		/*` |
|       - | 2171 | `		 * Equal so far and one side has components left, so php asks whether the` |
|       - | 2172 | `		 * next one outranks a plain number: "1.2.3" > "1.2" but "1.2.dev" < "1.2".` |
|       - | 2173 | `		 * The leftover can still hold dots ("1" vs "1#2" leaves "#.2"), which is` |
|       - | 2174 | `		 * why this is a whole comparison and not one special-form lookup.` |
|       - | 2175 | `		 */` |
|     113 | 2176 | `		if( n1 ){` |
|      25 | 2177 | `			if( SyisDigit((unsigned char)p1[0]) ){ return 1; }` |
|      23 | 2178 | `			if( p1[0] == 0 ){ return -1; }   /* php's empty-operand shortcut */` |
|      19 | 2179 | `			zV1 = p1;` |
|      19 | 2180 | `			zV2 = zMark;` |
|      98 | 2181 | `		}else if( n2 ){` |
|      29 | 2182 | `			if( SyisDigit((unsigned char)p2[0]) ){ return -1; }` |
|       7 | 2183 | `			if( p2[0] == 0 ){ return 1; }` |
|       7 | 2184 | `			zV1 = zMark;` |
|       7 | 2185 | `			zV2 = p2;` |
|       4 | 2186 | `		}else{` |
|      61 | 2187 | `			return 0;` |
|       - | 2188 | `		}` |
|       1 | 2189 | `	}` |
|     117 | 2190 | `}` |
|       - | 2191 | `/*` |
|       - | 2192 | ` * int\|bool version_compare(string $version1,string $version2,?string $operator = null)` |
|       - | 2193 | ` *  Compare two "PHP-standardized" version number strings: -1/0/1 without an` |
|       - | 2194 | ` *  $operator, the operator's verdict with one. An operator php does not know` |
|       - | 2195 | ` *  is a ValueError (php 8 stopped answering NULL for it).` |
|       - | 2196 | ` */` |
|     244 | 2197 | `PH7_PRIVATE int PH7_builtin_version_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2198 | `{` |
|       - | 2199 | `	static const struct VersionOp {` |
|       - | 2200 | `		const char *zName;` |
|       - | 2201 | `		int nLen;` |
|       - | 2202 | `		int bLt,bEq,bGt;   /* answer for cmp < 0, cmp == 0, cmp > 0 */` |
|       - | 2203 | `	} aVersionOp[] = {` |
|       - | 2204 | `		{ "<", 1, 1,0,0 }, { "lt", 2, 1,0,0 }, { "<=",2, 1,1,0 }, { "le",2, 1,1,0 },` |
|       - | 2205 | `		{ ">", 1, 0,0,1 }, { "gt", 2, 0,0,1 }, { ">=",2, 0,1,1 }, { "ge",2, 0,1,1 },` |
|       - | 2206 | `		{ "==",2, 0,1,0 }, { "=",  1, 0,1,0 }, { "eq",2, 0,1,0 },` |
|       - | 2207 | `		{ "!=",2, 1,0,1 }, { "<>", 2, 1,0,1 }, { "ne",2, 1,0,1 },` |
|       - | 2208 | `	};` |
|       - | 2209 | `	const char *zV1,*zV2,*zOp;` |
|       - | 2210 | `	sxu32 n1,n2,n;` |
|       - | 2211 | `	int cmp,nOp;` |
|     245 | 2212 | `	if( nArg < 2 ){` |
|       - | 2213 | `		/* Arity is enforced from aBuiltinSig[] before the call. */` |
|     ! 0 | 2214 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2215 | `		return PH7_OK;` |
|       - | 2216 | `	}` |
|       - | 2217 | `	/* php reads both operands as NUL-terminated C strings, so an embedded NUL` |
|       - | 2218 | `	 * ends the version there. ph7_value_to_string() null-appends. */` |
|     245 | 2219 | `	zV1 = ph7_value_to_string(apArg[0],0);` |
|     245 | 2220 | `	zV2 = ph7_value_to_string(apArg[1],0);` |
|     245 | 2221 | `	n1 = SyStrlen(zV1);` |
|     245 | 2222 | `	n2 = SyStrlen(zV2);` |
|     245 | 2223 | `	if( n1 < 1 \|\| n2 < 1 ){` |
|      13 | 2224 | `		cmp = (n1 == n2) ? 0 : (n1 > 0 ? 1 : -1);` |
|       7 | 2225 | `	}else{` |
|       - | 2226 | `		char *zC1,*zC2;` |
|     233 | 2227 | `		zC1 = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(2 * n1 + 2),FALSE,TRUE);` |
|     233 | 2228 | `		zC2 = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(2 * n2 + 2),FALSE,TRUE);` |
|     233 | 2229 | `		if( zC1 == 0 \|\| zC2 == 0 ){` |
|     ! 0 | 2230 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 2231 | `		}` |
|       - | 2232 | `		/* A version already starting with '#' is php's own marker: it bypasses` |
|       - | 2233 | `		 * canonicalization so that "#N#" survives as one component. */` |
|     233 | 2234 | `		if( zV1[0] == '#' ){ SyMemcpy(zV1,zC1,n1 + 1); }else{ VersionCanonicalize(zV1,n1,zC1); }` |
|     233 | 2235 | `		if( zV2[0] == '#' ){ SyMemcpy(zV2,zC2,n2 + 1); }else{ VersionCanonicalize(zV2,n2,zC2); }` |
|     233 | 2236 | `		cmp = VersionCompareCanon(zC1,zC2);` |
|       - | 2237 | `	}` |
|     245 | 2238 | `	if( nArg < 3 \|\| ph7_value_is_null(apArg[2]) ){` |
|      85 | 2239 | `		ph7_result_int(pCtx,cmp);` |
|      85 | 2240 | `		return PH7_OK;` |
|       - | 2241 | `	}` |
|     161 | 2242 | `	zOp = ph7_value_to_string(apArg[2],&nOp);` |
|    1141 | 2243 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVersionOp) ; ++n ){` |
|    1125 | 2244 | `		if( nOp == aVersionOp[n].nLen && SyMemcmp(zOp,aVersionOp[n].zName,(sxu32)nOp) == 0 ){` |
|     213 | 2245 | `			ph7_result_bool(pCtx,cmp < 0 ? aVersionOp[n].bLt` |
|      68 | 2246 | `				: (cmp > 0 ? aVersionOp[n].bGt : aVersionOp[n].bEq));` |
|     145 | 2247 | `			return PH7_OK;` |
|       - | 2248 | `		}` |
|     491 | 2249 | `	}` |
|      17 | 2250 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 2251 | `		"version_compare(): Argument #3 ($operator) must be a valid comparison operator");` |
|     123 | 2252 | `}` |
|       - | 2253 | `/*` |
|       - | 2254 | ` * int strncmp(string $str1,string $str2,int n)` |
|       - | 2255 | ` *  Perform a binary safe string comparison of the first n characters.` |
|       - | 2256 | ` * Parameter` |
|       - | 2257 | ` *  str1: The first string` |
|       - | 2258 | ` *  str2: The second string` |
|       - | 2259 | ` * Return` |
|       - | 2260 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|       - | 2261 | ` *  than str2, and 0 if they are equal.` |
|       - | 2262 | ` */` |
|    3808 | 2263 | `PH7_PRIVATE int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2264 | `{` |
|       - | 2265 | `	const char *z1,*z2;` |
|       - | 2266 | `	int res;` |
|       - | 2267 | `	int n;` |
|    3809 | 2268 | `	if( nArg < 3 ){` |
|       - | 2269 | `		/* Perform a standard comparison */` |
|     ! 0 | 2270 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|       - | 2271 | `	}` |
|       - | 2272 | `	/* Desired comparison length */` |
|    3809 | 2273 | `	n  = ph7_value_to_int(apArg[2]);` |
|    3809 | 2274 | `	if( n < 0 ){` |
|       - | 2275 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|       4 | 2276 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 2277 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|       1 | 2278 | `			ph7_function_name(pCtx));` |
|       - | 2279 | `	}` |
|       - | 2280 | `	/* Perform the comparison */` |
|    3807 | 2281 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|    3807 | 2282 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|    3807 | 2283 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|       - | 2284 | `	/* Comparison result */` |
|    3807 | 2285 | `	ph7_result_int(pCtx,res);` |
|    3807 | 2286 | `	return PH7_OK;` |
|    1902 | 2287 | `}` |
|       - | 2288 | `/*` |
|       - | 2289 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|       - | 2290 | ` *  Perform a binary safe case-insensitive string comparison.` |
|       - | 2291 | ` * Parameter` |
|       - | 2292 | ` *  str1: The first string` |
|       - | 2293 | ` *  str2: The second string` |
|       - | 2294 | ` * Return` |
|       - | 2295 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|       - | 2296 | ` *  than str2, and 0 if they are equal.` |
|       - | 2297 | ` */` |
|    1102 | 2298 | `PH7_PRIVATE int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 2299 | `{` |
|       - | 2300 | `	const char *z1,*z2;` |
|       - | 2301 | `	int n1,n2;` |
|       - | 2302 | `	int res;` |
|    1106 | 2303 | `	if( nArg < 2 ){` |
|     ! 0 | 2304 | `		res = nArg == 0 ? 0 : 1;` |
|     ! 0 | 2305 | `		ph7_result_int(pCtx,res);` |
|     ! 0 | 2306 | `		return PH7_OK;` |
|       - | 2307 | `	}` |
|       - | 2308 | `	/* Perform the comparison */` |
|    1106 | 2309 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|    1106 | 2310 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|    1106 | 2311 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|       - | 2312 | `	/* Comparison result */` |
|    1106 | 2313 | `	ph7_result_int(pCtx,res);` |
|    1106 | 2314 | `	return PH7_OK;` |
|     555 | 2315 | `}` |
|       - | 2316 | `/*` |
|       - | 2317 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|       - | 2318 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|       - | 2319 | ` * Parameter` |
|       - | 2320 | ` *  $str1: The first string` |
|       - | 2321 | ` *  $str2: The second string` |
|       - | 2322 | ` *  $len:  The length of strings to be used in the comparison.` |
|       - | 2323 | ` * Return` |
|       - | 2324 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|       - | 2325 | ` *  than str2, and 0 if they are equal.` |
|       - | 2326 | ` */` |
|     194 | 2327 | `PH7_PRIVATE int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2328 | `{` |
|       - | 2329 | `	const char *z1,*z2;` |
|       - | 2330 | `	int res;` |
|       - | 2331 | `	int n;` |
|     199 | 2332 | `	if( nArg < 3 ){` |
|       - | 2333 | `		/* Perform a standard comparison */` |
|     ! 0 | 2334 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|       - | 2335 | `	}` |
|       - | 2336 | `	/* Desired comparison length */` |
|     199 | 2337 | `	n  = ph7_value_to_int(apArg[2]);` |
|     199 | 2338 | `	if( n < 0 ){` |
|       - | 2339 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|       4 | 2340 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 2341 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|       1 | 2342 | `			ph7_function_name(pCtx));` |
|       - | 2343 | `	}` |
|       - | 2344 | `	/* Perform the comparison */` |
|     197 | 2345 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     197 | 2346 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     197 | 2347 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|       - | 2348 | `	/* Comparison result */` |
|     197 | 2349 | `	ph7_result_int(pCtx,res);` |
|     197 | 2350 | `	return PH7_OK;` |
|     106 | 2351 | `}` |
|       - | 2352 | `/*` |
|       - | 2353 | ` * Implode context [i.e: it's private data].` |
|       - | 2354 | ` * A pointer to the following structure is forwarded` |
|       - | 2355 | ` * verbatim to the array walker callback defined below.` |
|       - | 2356 | ` */` |
|       - | 2357 | `struct implode_data {` |
|       - | 2358 | `	ph7_context *pCtx;    /* Call context */` |
|       - | 2359 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|       - | 2360 | `	const char *zSep;     /* Arguments separator if any */` |
|       - | 2361 | `	int nSeplen;          /* Separator length */` |
|       - | 2362 | `	int bFirst;           /* TRUE if first call */` |
|       - | 2363 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|       - | 2364 | `	sxi32 rc;             /* Captured allocation rc; SXERR_MEM => the builtin raises an OOM fatal */` |
|       - | 2365 | `	sxi32 rcThrow;        /* Captured coercion throw; the builtin propagates it instead of a result */` |
|       - | 2366 | `};` |
|       - | 2367 | `/*` |
|       - | 2368 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|       - | 2369 | ` * The following routine is invoked for each array entry passed` |
|       - | 2370 | ` * to the implode() function.` |
|       - | 2371 | ` */` |
|  495950 | 2372 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|       5 | 2373 | `{` |
|  247914 | 2374 | `	SXUNUSED(pKey);` |
|  495955 | 2375 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|       - | 2376 | `	const char *zData;` |
|       - | 2377 | `	int nLen;` |
|  495955 | 2378 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|       3 | 2379 | `		if( pData->nSeplen > 0 ){` |
|       3 | 2380 | `			if( !pData->bFirst ){` |
|       - | 2381 | `				/* append the separator first */` |
|       3 | 2382 | `				if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|     ! 0 | 2383 | `					pData->rc = SXERR_MEM;` |
|     ! 0 | 2384 | `					return PH7_ABORT;` |
|       - | 2385 | `				}` |
|       2 | 2386 | `			}else{` |
|     ! 0 | 2387 | `				pData->bFirst = 0;` |
|       - | 2388 | `			}` |
|       1 | 2389 | `		}` |
|       - | 2390 | `		/* Recurse */` |
|       3 | 2391 | `		pData->bFirst = 1;` |
|       3 | 2392 | `		pData->nRecCount++;` |
|       3 | 2393 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|       3 | 2394 | `		pData->nRecCount--;` |
|       - | 2395 | `		/* Propagate an allocation failure surfaced deeper in the recursion. */` |
|       3 | 2396 | `		if( pData->rc != SXRET_OK ){` |
|     ! 0 | 2397 | `			return PH7_ABORT;` |
|       - | 2398 | `		}` |
|       3 | 2399 | `		return PH7_OK;` |
|       - | 2400 | `	}` |
|       - | 2401 | `	/* Extract the string representation of the entry value, USER-VISIBLY: an` |
|       - | 2402 | `	 * element that is itself an array renders as "Array" and warns, and one that` |
|       - | 2403 | `	 * is an object with no __toString() is php's catchable Error (it used to` |
|       - | 2404 | `	 * render as the literal "Object"). The walk cannot return a status, so` |
|       - | 2405 | `	 * park it on the context struct and abort. */` |
|       - | 2406 | `	{` |
|  495953 | 2407 | `		sxi32 rcSv = PH7_ValueToStringUV(pData->pCtx,pValue,&zData,&nLen);` |
|  495953 | 2408 | `		if( rcSv != SXRET_OK ){` |
|      20 | 2409 | `			pData->rcThrow = rcSv;` |
|      20 | 2410 | `			return PH7_ABORT;` |
|       - | 2411 | `		}` |
|       - | 2412 | `	}` |
|       - | 2413 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
|  495935 | 2414 | `	if( pData->bFirst ){` |
|   55863 | 2415 | `		pData->bFirst = 0;` |
|  468003 | 2416 | `	}else if( pData->nSeplen > 0 ){` |
|       - | 2417 | `		/* append the separator first */` |
|  431513 | 2418 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|     ! 0 | 2419 | `			pData->rc = SXERR_MEM;` |
|     ! 0 | 2420 | `			return PH7_ABORT;` |
|       - | 2421 | `		}` |
|  215696 | 2422 | `	}` |
|       - | 2423 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
|  495935 | 2424 | `	if( nLen > 0 ){` |
|  467331 | 2425 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|     ! 0 | 2426 | `			pData->rc = SXERR_MEM;` |
|     ! 0 | 2427 | `			return PH7_ABORT;` |
|       - | 2428 | `		}` |
|  233602 | 2429 | `	}` |
|  495935 | 2430 | `	return PH7_OK;` |
|  247919 | 2431 | `}` |
|       - | 2432 | `/*` |
|       - | 2433 | ` * string implode(string $glue,array $pieces,...)` |
|       - | 2434 | ` * string implode(array $pieces,...)` |
|       - | 2435 | ` *  Join array elements with a string.` |
|       - | 2436 | ` * $glue` |
|       - | 2437 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|       - | 2438 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|       - | 2439 | ` * $pieces` |
|       - | 2440 | ` *   The array of strings to implode.` |
|       - | 2441 | ` * Return` |
|       - | 2442 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|       - | 2443 | ` *  order, with the glue string between each element.` |
|       - | 2444 | ` */` |
|   56438 | 2445 | `PH7_PRIVATE int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2446 | `{` |
|       - | 2447 | `	struct implode_data imp_data;` |
|       - | 2448 | `	/*` |
|       - | 2449 | `` 	 * php's contract for implode()/join(): one function, a `array\|string $separator` `` |
|       - | 2450 | ``	 * and a `?array $array` row, and a body that resolves the two ARITIES. The`` |
|       - | 2451 | `	 * messages report the overload as already resolved -- an $array argument in` |
|       - | 2452 | ``	 * position #2 means #1 is the SEPARATOR and must be a `string`, never the`` |
|       - | 2453 | `	 * union (which is only what the two arities accept BETWEEN them), and an ARRAY` |
|       - | 2454 | `	 * in position #1 with a second argument is that same #1 error. One aBuiltinSig` |
|       - | 2455 | `	 * row cannot say either (it drives both the accepted set and the message),` |
|       - | 2456 | `	 * which is why implode/join sit on azSelfChecked[] with strtr() and check` |
|       - | 2457 | `	 * their own rows here.` |
|       - | 2458 | `	 *` |
|       - | 2459 | ``	 * The messages name the INVOKED function: php reports `join(): ...` for`` |
|       - | 2460 | ``	 * join(), where this builtin used to hardcode `implode(): ...`.`` |
|       - | 2461 | `	 *` |
|       - | 2462 | `	 * One divergence, twin-paired in` |
|       - | 2463 | `	 * 002-integration/function/implode_separator_type{,_zend}.phpt. php 8.5 words` |
|       - | 2464 | `	 * the three cases below through a SPECIALIZED handler that only a DIRECT,` |
|       - | 2465 | ``	 * compile-time-resolved `implode(...)` call reaches; join(),`` |
|       - | 2466 | ``	 * `$f='implode'; $f(...)` and call_user_func('implode', ...) fall back to a`` |
|       - | 2467 | ``	 * generic path that answers differently (`array\|string` for the first, a #2`` |
|       - | 2468 | `	 * error for the second, and "ab" for the third). It is a call-FORM` |
|       - | 2469 | `	 * specialization, not a semantic rule -- it does not change with opcache off` |
|       - | 2470 | `	 * -- so PHL gives every call form the one contract php's direct calls use,` |
|       - | 2471 | `	 * which is the form real code writes and the form the corpus pins.` |
|       - | 2472 | `	 */` |
|   56443 | 2473 | `	const char *zName = ph7_function_name(pCtx);` |
|   56443 | 2474 | `	int i = 1;` |
|   56443 | 2475 | `	if( nArg < 1 ){` |
|       - | 2476 | `		/* Missing argument,return NULL */` |
|     ! 0 | 2477 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2478 | `		return PH7_OK;` |
|       - | 2479 | `	}` |
|       - | 2480 | `	/* Prepare the implode context */` |
|   56443 | 2481 | `	imp_data.pCtx = pCtx;` |
|   56443 | 2482 | `	imp_data.bRecursive = 0;` |
|   56443 | 2483 | `	imp_data.bFirst = 1;` |
|   56443 | 2484 | `	imp_data.nRecCount = 0;` |
|   56443 | 2485 | `	imp_data.rc = SXRET_OK;` |
|   56443 | 2486 | `	imp_data.rcThrow = SXRET_OK;` |
|   56443 | 2487 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|   56417 | 2488 | `		if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|       - | 2489 | `			/* php only DEPRECATES null for the union parameter and coerces it to` |
|       - | 2490 | `			 * ""; PHL rejects it (the null-strictness policy), naming php's DECLARED type` |
|       - | 2491 | ``			 * -- the one case where `array\|string` is the right wording, because`` |
|       - | 2492 | `			 * php never narrows the union for a value it accepts. */` |
|     ! 0 | 2493 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     ! 0 | 2494 | `				"%s(): Argument #1 ($separator) must be of type array\|string, null given",zName);` |
|       - | 2495 | `		}` |
|   56417 | 2496 | `		if( nArg < 2 \|\| ph7_value_is_null(apArg[1]) ){` |
|       - | 2497 | ``			/* php: a string separator REQUIRES the array. `implode("x")` and`` |
|       - | 2498 | ``			 * `implode("x", null)` both answered "" -- the `?array` in php's`` |
|       - | 2499 | `			 * signature is the DEFAULT's type, not a value it accepts. */` |
|      17 | 2500 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 2501 | `				"%s(): If argument #1 ($separator) is of type string, "` |
|       5 | 2502 | `				"argument #2 ($array) must be of type array, null given",zName);` |
|       - | 2503 | `		}` |
|   56407 | 2504 | `		if( !PH7_ArgSatisfiesString(apArg[0]) ){` |
|       - | 2505 | `			/* The overload is resolved, so #1 is the separator and must be a` |
|       - | 2506 | `			 * STRING. PHL used to fall through to the central screen here and` |
|       - | 2507 | ``			 * report the whole `array\|string` union instead. */`` |
|       - | 2508 | `			char zBuf[64];` |
|      10 | 2509 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 2510 | `				"%s(): Argument #1 ($separator) must be of type string, %s given",` |
|       3 | 2511 | `				zName,VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|       - | 2512 | `		}` |
|   56401 | 2513 | `		if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2514 | `			/* php: implode($glue, $pieces) requires an ARRAY. PH7 stringified` |
|       - | 2515 | `			 * whatever it was handed, so implode(",", 5) quietly returned "5". */` |
|       - | 2516 | `			char zBuf[64];` |
|      12 | 2517 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 2518 | `				"%s(): Argument #2 ($array) must be of type ?array, %s given",` |
|       6 | 2519 | `				zName,VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|       - | 2520 | `		}` |
|   56395 | 2521 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|   28197 | 2522 | `	}else{` |
|      29 | 2523 | `		if( nArg > 1 ){` |
|       - | 2524 | `			/* php 8 removed the legacy swapped order: implode($pieces, $glue) is a` |
|       - | 2525 | `			 * TypeError whatever $glue holds (PHL used to swap silently, a wrong` |
|       - | 2526 | `			 * ANSWER when the caller meant php's signature). One array argument` |
|       - | 2527 | `			 * alone stays the legal ""-glue form. */` |
|      26 | 2528 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       8 | 2529 | `				"%s(): Argument #1 ($separator) must be of type string, array given",zName);` |
|       - | 2530 | `		}` |
|      13 | 2531 | `		imp_data.zSep = 0;` |
|      13 | 2532 | `		imp_data.nSeplen = 0;` |
|      13 | 2533 | `		i = 0;` |
|       - | 2534 | `	}` |
|   56405 | 2535 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|     ! 0 | 2536 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 2537 | `	}` |
|       - | 2538 | `	/* Start the 'join' process */` |
|  112787 | 2539 | `	while( i < nArg ){` |
|   56405 | 2540 | `		if( ph7_value_is_array(apArg[i]) ){` |
|       - | 2541 | `			/* Iterate throw array entries */` |
|   56405 | 2542 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|       - | 2543 | `			/* An element whose coercion threw ends the join with that throw */` |
|   56405 | 2544 | `			if( imp_data.rcThrow != SXRET_OK ){` |
|      20 | 2545 | `				return imp_data.rcThrow;` |
|       - | 2546 | `			}` |
|       - | 2547 | `			/* Surface a callback allocation failure as a fatal */` |
|   56387 | 2548 | `			if( imp_data.rc != SXRET_OK ){` |
|     ! 0 | 2549 | `				return PH7_ContextMemoryError(pCtx);` |
|       - | 2550 | `			}` |
|   28193 | 2551 | `		}else{` |
|       - | 2552 | `			const char *zData;` |
|       - | 2553 | `			int nLen;` |
|       - | 2554 | `			/* Extract the string representation of the ph7 value (user-visible) */` |
|     ! 0 | 2555 | `			sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[i],&zData,&nLen);` |
|     ! 0 | 2556 | `			if( rcSv != SXRET_OK ){` |
|     ! 0 | 2557 | `				return rcSv;` |
|       - | 2558 | `			}` |
|       - | 2559 | `			/* Manage separator insertion regardless of string length */` |
|     ! 0 | 2560 | `			if( imp_data.bFirst ){` |
|     ! 0 | 2561 | `				imp_data.bFirst = 0;` |
|     ! 0 | 2562 | `			}else if( imp_data.nSeplen > 0 ){` |
|     ! 0 | 2563 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|     ! 0 | 2564 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 2565 | `				}` |
|     ! 0 | 2566 | `			}` |
|       - | 2567 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|     ! 0 | 2568 | `			if( nLen > 0 ){` |
|     ! 0 | 2569 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|     ! 0 | 2570 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 2571 | `				}` |
|     ! 0 | 2572 | `			}` |
|       - | 2573 | `		}` |
|   56387 | 2574 | `		i++;` |
|       5 | 2575 | `	}` |
|   56387 | 2576 | `	return PH7_OK;` |
|   28221 | 2577 | `}` |
|       - | 2578 | `/*` |
|       - | 2579 | ` * Symisc eXtension:` |
|       - | 2580 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|       - | 2581 | ` * Purpose` |
|       - | 2582 | ` *  Same as implode() but recurse on arrays.` |
|       - | 2583 | ` * Example:` |
|       - | 2584 | ` *   $a = array('usr',array('home','dean'));` |
|       - | 2585 | ` *   echo implode_recursive("/",$a);` |
|       - | 2586 | ` *   Will output` |
|       - | 2587 | ` *     usr/home/dean.` |
|       - | 2588 | ` *   While the standard implode would produce.` |
|       - | 2589 | ` *    usr/Array.` |
|       - | 2590 | ` * Parameter` |
|       - | 2591 | ` *  Refer to implode().` |
|       - | 2592 | ` * Return` |
|       - | 2593 | ` *  Refer to implode().` |
|       - | 2594 | ` */` |
|      12 | 2595 | `PH7_PRIVATE int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2596 | `{` |
|       - | 2597 | `	struct implode_data imp_data;` |
|      13 | 2598 | `	int i = 1;` |
|      13 | 2599 | `	if( nArg < 1 ){` |
|       - | 2600 | `		/* Missing argument,return NULL */` |
|       3 | 2601 | `		ph7_result_null(pCtx);` |
|       3 | 2602 | `		return PH7_OK;` |
|       - | 2603 | `	}` |
|       - | 2604 | `	/* Prepare the implode context */` |
|      11 | 2605 | `	imp_data.pCtx = pCtx;` |
|      11 | 2606 | `	imp_data.bRecursive = 1;` |
|      11 | 2607 | `	imp_data.bFirst = 1;` |
|      11 | 2608 | `	imp_data.nRecCount = 0;` |
|      11 | 2609 | `	imp_data.rc = SXRET_OK;` |
|      11 | 2610 | `	imp_data.rcThrow = SXRET_OK;` |
|      11 | 2611 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      11 | 2612 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|       6 | 2613 | `	}else{` |
|     ! 0 | 2614 | `		imp_data.zSep = 0;` |
|     ! 0 | 2615 | `		imp_data.nSeplen = 0;` |
|     ! 0 | 2616 | `		i = 0;` |
|       - | 2617 | `	}` |
|      11 | 2618 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|     ! 0 | 2619 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 2620 | `	}` |
|       - | 2621 | `	/* Start the 'join' process */` |
|      21 | 2622 | `	while( i < nArg ){` |
|      11 | 2623 | `		if( ph7_value_is_array(apArg[i]) ){` |
|       - | 2624 | `			/* Iterate throw array entries */` |
|       3 | 2625 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|       - | 2626 | `			/* An element whose coercion threw ends the join with that throw */` |
|       3 | 2627 | `			if( imp_data.rcThrow != SXRET_OK ){` |
|     ! 0 | 2628 | `				return imp_data.rcThrow;` |
|       - | 2629 | `			}` |
|       - | 2630 | `			/* Surface a callback allocation failure as a fatal */` |
|       3 | 2631 | `			if( imp_data.rc != SXRET_OK ){` |
|     ! 0 | 2632 | `				return PH7_ContextMemoryError(pCtx);` |
|       - | 2633 | `			}` |
|       2 | 2634 | `		}else{` |
|       - | 2635 | `			const char *zData;` |
|       - | 2636 | `			int nLen;` |
|       - | 2637 | `			/* Extract the string representation of the ph7 value (user-visible) */` |
|       9 | 2638 | `			sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[i],&zData,&nLen);` |
|       9 | 2639 | `			if( rcSv != SXRET_OK ){` |
|     ! 0 | 2640 | `				return rcSv;` |
|       - | 2641 | `			}` |
|       - | 2642 | `			/* Manage separator insertion regardless of string length */` |
|       9 | 2643 | `			if( imp_data.bFirst ){` |
|       9 | 2644 | `				imp_data.bFirst = 0;` |
|       4 | 2645 | `			}else if( imp_data.nSeplen > 0 ){` |
|     ! 0 | 2646 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|     ! 0 | 2647 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 2648 | `				}` |
|     ! 0 | 2649 | `			}` |
|       - | 2650 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|       9 | 2651 | `			if( nLen > 0 ){` |
|       9 | 2652 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|     ! 0 | 2653 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 2654 | `				}` |
|       4 | 2655 | `			}` |
|       - | 2656 | `		}` |
|      11 | 2657 | `		i++;` |
|       1 | 2658 | `	}` |
|      11 | 2659 | `	return PH7_OK;` |
|       7 | 2660 | `}` |
|       - | 2661 | `/*` |
|       - | 2662 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|       - | 2663 | ` *  Returns an array of strings, each of which is a substring of string` |
|       - | 2664 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|       - | 2665 | ` * Parameters` |
|       - | 2666 | ` *  $delimiter` |
|       - | 2667 | ` *   The boundary string.` |
|       - | 2668 | ` * $string` |
|       - | 2669 | ` *   The input string.` |
|       - | 2670 | ` * $limit` |
|       - | 2671 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|       - | 2672 | ` *   of limit elements with the last element containing the rest of string.` |
|       - | 2673 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|       - | 2674 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|       - | 2675 | ` * Returns` |
|       - | 2676 | ` *  Returns an array of strings created by splitting the string parameter` |
|       - | 2677 | ` *  on boundaries formed by the delimiter.` |
|       - | 2678 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|       - | 2679 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|       - | 2680 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|       - | 2681 | ` *  will be returned.` |
|       - | 2682 | ` * NOTE:` |
|       - | 2683 | ` *  Negative limit is not supported.` |
|       - | 2684 | ` */` |
|  401414 | 2685 | `PH7_PRIVATE int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2686 | `{` |
|       - | 2687 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|       - | 2688 | `	int nDelim,nStrlen,iLimit;` |
|       - | 2689 | `	ph7_value *pArray;` |
|       - | 2690 | `	ph7_value *pValue;` |
|       - | 2691 | `	sxu32 nOfft;` |
|       - | 2692 | `	sxi32 rc;` |
|  401419 | 2693 | `	if( nArg < 2 ){` |
|       - | 2694 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2695 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2696 | `		return PH7_OK;` |
|       - | 2697 | `	}` |
|       - | 2698 | `	/* Extract the delimiter */` |
|  401419 | 2699 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|  401419 | 2700 | `	if( nDelim < 1 ){` |
|       - | 2701 | `		/* Empty delimiter: PHP 8 throws a catchable ValueError. */` |
|       5 | 2702 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 2703 | `			"explode(): Argument #1 ($separator) must not be empty");` |
|       - | 2704 | `	}` |
|       - | 2705 | `	/* Extract the string */` |
|  401415 | 2706 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|  401415 | 2707 | `	if( nStrlen < 1 ){` |
|       - | 2708 | `		/* Empty string: normally an array with a single empty element (PHP behavior).` |
|       - | 2709 | `		 * A negative limit drops the last -limit components, so the sole empty` |
|       - | 2710 | `		 * component is dropped and the result is an empty array. */` |
|       7 | 2711 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|       7 | 2712 | `		if( pArrayTmp == 0 ){` |
|       - | 2713 | `			/* Out of memory,return FALSE */` |
|     ! 0 | 2714 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 2715 | `			return PH7_OK;` |
|       - | 2716 | `		}` |
|       7 | 2717 | `		if( !(nArg > 2 && ph7_value_to_int(apArg[2]) < 0) ){` |
|       5 | 2718 | `			ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|       5 | 2719 | `			if( pValueTmp == 0 ){` |
|       - | 2720 | `				/* Out of memory,return FALSE */` |
|     ! 0 | 2721 | `				ph7_result_bool(pCtx,0);` |
|     ! 0 | 2722 | `				return PH7_OK;` |
|       - | 2723 | `			}` |
|       5 | 2724 | `			ph7_value_string(pValueTmp, "", 0);` |
|       5 | 2725 | `			if( ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp) != SXRET_OK ){` |
|     ! 0 | 2726 | `				return PH7_ContextMemoryError(pCtx);` |
|       - | 2727 | `			}` |
|       2 | 2728 | `		}` |
|       7 | 2729 | `		ph7_result_value(pCtx, pArrayTmp);` |
|       7 | 2730 | `		return PH7_OK;` |
|       - | 2731 | `	}` |
|       - | 2732 | `	/* Point to the end of the string */` |
|  401409 | 2733 | `	zEnd = &zString[nStrlen];` |
|       - | 2734 | `	/* Create the array */` |
|  401409 | 2735 | `	pArray =  ph7_context_new_array(pCtx);` |
|  401409 | 2736 | `	pValue = ph7_context_new_scalar(pCtx);` |
|  401409 | 2737 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|       - | 2738 | `		/* Out of memory,return FALSE */` |
|     ! 0 | 2739 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2740 | `		return PH7_OK;` |
|       - | 2741 | `	}` |
|       - | 2742 | `	/* Set a defualt limit */` |
|  401409 | 2743 | `	iLimit = SXI32_HIGH;` |
|  401409 | 2744 | `	if( nArg > 2 ){` |
|     267 | 2745 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     267 | 2746 | `		if( iLimit < 0 ){` |
|       - | 2747 | `			/* Negative limit: keep all components except the last -iLimit (PHP).` |
|       - | 2748 | `			 * Pre-count the components (delimiters + 1), then emit only the first` |
|       - | 2749 | `			 * nKeep CLEAN components — no trailing-remainder merge (the difference` |
|       - | 2750 | `			 * from the positive path). nKeep <= 0 drops everything -> empty array. */` |
|      17 | 2751 | `			int nTotal = 1,nKeep;` |
|      17 | 2752 | `			const char *zScan = zString;` |
|       - | 2753 | `			sxu32 nScanOfft;` |
|      57 | 2754 | `			while( SyBlobSearch(zScan,(sxu32)(zEnd - zScan),zDelim,nDelim,&nScanOfft) == SXRET_OK ){` |
|      41 | 2755 | `				nTotal++;` |
|      41 | 2756 | `				zScan = &zScan[nScanOfft + nDelim];` |
|       1 | 2757 | `			}` |
|      17 | 2758 | `			nKeep = nTotal + iLimit; /* iLimit < 0, so this is nTotal - (-iLimit) */` |
|      49 | 2759 | `			while( nKeep > (int)ph7_array_count(pArray)` |
|      39 | 2760 | `				&& SyBlobSearch(zString,(sxu32)(zEnd - zString),zDelim,nDelim,&nOfft) == SXRET_OK ){` |
|       - | 2761 | `				/* Emit the next clean component */` |
|      23 | 2762 | `				zCur = &zString[nOfft];` |
|      23 | 2763 | `				ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|      23 | 2764 | `				if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|     ! 0 | 2765 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 2766 | `				}` |
|      23 | 2767 | `				zString = &zCur[nDelim];` |
|      23 | 2768 | `				ph7_value_reset_string_cursor(pValue);` |
|       1 | 2769 | `			}` |
|      17 | 2770 | `			ph7_result_value(pCtx,pArray);` |
|      17 | 2771 | `			return PH7_OK;` |
|       - | 2772 | `		}` |
|     251 | 2773 | `		if( iLimit == 0 ){` |
|       5 | 2774 | `			iLimit = 1;` |
|       2 | 2775 | `		}` |
|     251 | 2776 | `		iLimit--;` |
|     123 | 2777 | `	}` |
|       - | 2778 | `	/* Start exploding */` |
| 3071110 | 2779 | `	for(;;){` |
| 6142225 | 2780 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 6142225 | 2781 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|       - | 2782 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|  401393 | 2783 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|  401393 | 2784 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|     ! 0 | 2785 | `				return PH7_ContextMemoryError(pCtx);` |
|       - | 2786 | `			}` |
|  401393 | 2787 | `			break;` |
|       - | 2788 | `		}` |
|       - | 2789 | `		/* Point to the desired offset */` |
| 5740837 | 2790 | `		zCur = &zString[nOfft];` |
|       - | 2791 | `		/* Perform the store operation (may be empty) */` |
| 5740837 | 2792 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 5740837 | 2793 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|     ! 0 | 2794 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 2795 | `		}` |
|       - | 2796 | `		/* Point beyond the delimiter */` |
| 5740837 | 2797 | `		zString = &zCur[nDelim];` |
|       - | 2798 | `		/* Reset the cursor */` |
| 5740837 | 2799 | `		ph7_value_reset_string_cursor(pValue);` |
|       5 | 2800 | `	}` |
|       - | 2801 | `	/* Return the freshly created array */` |
|  401393 | 2802 | `	ph7_result_value(pCtx,pArray);` |
|       - | 2803 | `	/* NOTE that every allocated ph7_value will be automatically` |
|       - | 2804 | `	 * released as soon we return from this foregin function.` |
|       - | 2805 | `	 */` |
|  401393 | 2806 | `	return PH7_OK;` |
|  200712 | 2807 | `}` |
|       - | 2808 | `/*` |
|       - | 2809 | ` * string trim(string $str[,string $charlist ])` |
|       - | 2810 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|       - | 2811 | ` * Parameters` |
|       - | 2812 | ` *  $str` |
|       - | 2813 | ` *   The string that will be trimmed.` |
|       - | 2814 | ` * $charlist` |
|       - | 2815 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|       - | 2816 | ` *   Simply list all characters that you want to be stripped.` |
|       - | 2817 | ` *   With .. you can specify a range of characters.` |
|       - | 2818 | ` * Returns.` |
|       - | 2819 | ` *  Thr processed string.` |
|       - | 2820 | ` * NOTE:` |
|       - | 2821 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|       - | 2822 | ` */` |
|   35348 | 2823 | `PH7_PRIVATE int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2824 | `{` |
|   35353 | 2825 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"trim",1,"$string"); }` |
|       - | 2826 | `	const char *zString;` |
|       - | 2827 | `	int nLen;` |
|   35353 | 2828 | `	if( nArg < 1 ){` |
|       - | 2829 | `		/* Missing arguments,return null */` |
|     ! 0 | 2830 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2831 | `		return PH7_OK;` |
|       - | 2832 | `	}` |
|       - | 2833 | `	/* Extract the target string */` |
|   35353 | 2834 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|   35353 | 2835 | `	if( nLen < 1 ){` |
|       - | 2836 | `		/* Empty string,return */` |
|   11303 | 2837 | `		ph7_result_string(pCtx,"",0);` |
|   11303 | 2838 | `		return PH7_OK;` |
|       - | 2839 | `	}` |
|       - | 2840 | `	/* Start the trim process */` |
|   24055 | 2841 | `	if( nArg < 2 ){` |
|       - | 2842 | `		SyString sStr;` |
|       - | 2843 | `		/* Remove white spaces and NUL bytes */` |
|   24005 | 2844 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|   62873 | 2845 | `		SyStringFullTrimSafe(&sStr);` |
|   24005 | 2846 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   12005 | 2847 | `	}else{` |
|       - | 2848 | `		/* Char list */` |
|       - | 2849 | `		const char *zList;` |
|       - | 2850 | `		int nListlen;` |
|      54 | 2851 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      54 | 2852 | `		if( nListlen < 1 ){` |
|       - | 2853 | `			/* Return the string unchanged */` |
|       6 | 2854 | `			ph7_result_string(pCtx,zString,nLen);` |
|       4 | 2855 | `		}else{` |
|       - | 2856 | `			char aMask[256];` |
|      50 | 2857 | `			const char *zEnd = &zString[nLen];` |
|      50 | 2858 | `			const char *zCur = zString;` |
|      50 | 2859 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|       - | 2860 | `			/* Left trim */` |
|     120 | 2861 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|      74 | 2862 | `				zCur++;` |
|       4 | 2863 | `			}` |
|       - | 2864 | `			/* Right trim */` |
|     114 | 2865 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|      67 | 2866 | `				zEnd--;` |
|       3 | 2867 | `			}` |
|      50 | 2868 | `			if( zCur >= zEnd ){` |
|       - | 2869 | `				/* Return the empty string */` |
|     ! 0 | 2870 | `				ph7_result_string(pCtx,"",0);` |
|     ! 0 | 2871 | `			}else{` |
|      50 | 2872 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|       - | 2873 | `			}` |
|       - | 2874 | `		}` |
|       - | 2875 | `	}` |
|   24055 | 2876 | `	return PH7_OK;` |
|   17671 | 2877 | `}` |
|       - | 2878 | `/*` |
|       - | 2879 | ` * string rtrim(string $str[,string $charlist ])` |
|       - | 2880 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|       - | 2881 | ` * Parameters` |
|       - | 2882 | ` *  $str` |
|       - | 2883 | ` *   The string that will be trimmed.` |
|       - | 2884 | ` * $charlist` |
|       - | 2885 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|       - | 2886 | ` *   Simply list all characters that you want to be stripped.` |
|       - | 2887 | ` *   With .. you can specify a range of characters.` |
|       - | 2888 | ` * Returns.` |
|       - | 2889 | ` *  Thr processed string.` |
|       - | 2890 | ` * NOTE:` |
|       - | 2891 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|       - | 2892 | ` */` |
|    1930 | 2893 | `PH7_PRIVATE int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2894 | `{` |
|    1935 | 2895 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"rtrim",1,"$string"); }` |
|       - | 2896 | `	const char *zString;` |
|       - | 2897 | `	int nLen;` |
|    1935 | 2898 | `	if( nArg < 1 ){` |
|       - | 2899 | `		/* Missing arguments,return null */` |
|     ! 0 | 2900 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2901 | `		return PH7_OK;` |
|       - | 2902 | `	}` |
|       - | 2903 | `	/* Extract the target string */` |
|    1935 | 2904 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    1935 | 2905 | `	if( nLen < 1 ){` |
|       - | 2906 | `		/* Empty string,return */` |
|      22 | 2907 | `		ph7_result_string(pCtx,"",0);` |
|      22 | 2908 | `		return PH7_OK;` |
|       - | 2909 | `	}` |
|       - | 2910 | `	/* Start the trim process */` |
|    1915 | 2911 | `	if( nArg < 2 ){` |
|       - | 2912 | `		SyString sStr;` |
|       - | 2913 | `		/* Remove white spaces and NUL bytes*/` |
|     283 | 2914 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     696 | 2915 | `		SyStringRightTrimSafe(&sStr);` |
|     283 | 2916 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|     144 | 2917 | `	}else{` |
|       - | 2918 | `		/* Char list */` |
|       - | 2919 | `		const char *zList;` |
|       - | 2920 | `		int nListlen;` |
|    1637 | 2921 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|    1637 | 2922 | `		if( nListlen < 1 ){` |
|       - | 2923 | `			/* Return the string unchanged */` |
|     ! 0 | 2924 | `			ph7_result_string(pCtx,zString,nLen);` |
|     ! 0 | 2925 | `		}else{` |
|       - | 2926 | `			char aMask[256];` |
|    1637 | 2927 | `			const char *zEnd = &zString[nLen];` |
|    1637 | 2928 | `			const char *zCur = zString;` |
|    1637 | 2929 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|       - | 2930 | `			/* Right trim */` |
|    1797 | 2931 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     163 | 2932 | `				zEnd--;` |
|       3 | 2933 | `			}` |
|    1637 | 2934 | `			if( zEnd <= zCur ){` |
|       - | 2935 | `				/* Return the empty string */` |
|      14 | 2936 | `				ph7_result_string(pCtx,"",0);` |
|       7 | 2937 | `			}else{` |
|    1623 | 2938 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|       - | 2939 | `			}` |
|       - | 2940 | `		}` |
|       - | 2941 | `	}` |
|    1915 | 2942 | `	return PH7_OK;` |
|     969 | 2943 | `}` |
|       - | 2944 | `/*` |
|       - | 2945 | ` * string ltrim(string $str[,string $charlist ])` |
|       - | 2946 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|       - | 2947 | ` * Parameters` |
|       - | 2948 | ` *  $str` |
|       - | 2949 | ` *   The string that will be trimmed.` |
|       - | 2950 | ` * $charlist` |
|       - | 2951 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|       - | 2952 | ` *   Simply list all characters that you want to be stripped.` |
|       - | 2953 | ` *   With .. you can specify a range of characters.` |
|       - | 2954 | ` * Returns.` |
|       - | 2955 | ` *  Thr processed string.` |
|       - | 2956 | ` * NOTE:` |
|       - | 2957 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|       - | 2958 | ` */` |
|     224 | 2959 | `PH7_PRIVATE int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2960 | `{` |
|     229 | 2961 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"ltrim",1,"$string"); }` |
|       - | 2962 | `	const char *zString;` |
|       - | 2963 | `	int nLen;` |
|     229 | 2964 | `	if( nArg < 1 ){` |
|       - | 2965 | `		/* Missing arguments,return null */` |
|     ! 0 | 2966 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2967 | `		return PH7_OK;` |
|       - | 2968 | `	}` |
|       - | 2969 | `	/* Extract the target string */` |
|     229 | 2970 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     229 | 2971 | `	if( nLen < 1 ){` |
|       - | 2972 | `		/* Empty string,return */` |
|      23 | 2973 | `		ph7_result_string(pCtx,"",0);` |
|      23 | 2974 | `		return PH7_OK;` |
|       - | 2975 | `	}` |
|       - | 2976 | `	/* Start the trim process */` |
|     209 | 2977 | `	if( nArg < 2 ){` |
|       - | 2978 | `		SyString sStr;` |
|       - | 2979 | `		/* Remove white spaces and NUL byte */` |
|       3 | 2980 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|       8 | 2981 | `		SyStringLeftTrimSafe(&sStr);` |
|       3 | 2982 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|       2 | 2983 | `	}else{` |
|       - | 2984 | `		/* Char list */` |
|       - | 2985 | `		const char *zList;` |
|       - | 2986 | `		int nListlen;` |
|     207 | 2987 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     207 | 2988 | `		if( nListlen < 1 ){` |
|       - | 2989 | `			/* Return the string unchanged */` |
|       3 | 2990 | `			ph7_result_string(pCtx,zString,nLen);` |
|       2 | 2991 | `		}else{` |
|       - | 2992 | `			char aMask[256];` |
|     205 | 2993 | `			const char *zEnd = &zString[nLen];` |
|     205 | 2994 | `			const char *zCur = zString;` |
|     205 | 2995 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|       - | 2996 | `			/* Left trim */` |
|     457 | 2997 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     257 | 2998 | `				zCur++;` |
|       5 | 2999 | `			}` |
|     205 | 3000 | `			if( zCur >= zEnd ){` |
|       - | 3001 | `				/* Return the empty string */` |
|     ! 0 | 3002 | `				ph7_result_string(pCtx,"",0);` |
|     ! 0 | 3003 | `			}else{` |
|     205 | 3004 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|       - | 3005 | `			}` |
|       - | 3006 | `		}` |
|       - | 3007 | `	}` |
|     209 | 3008 | `	return PH7_OK;` |
|     121 | 3009 | `}` |
|       - | 3010 | `/*` |
|       - | 3011 | ` * string strtolower(string $str)` |
|       - | 3012 | ` *  Make a string lowercase.` |
|       - | 3013 | ` * Parameters` |
|       - | 3014 | ` *  $str` |
|       - | 3015 | ` *   The input string.` |
|       - | 3016 | ` * Returns.` |
|       - | 3017 | ` *  The lowercased string.` |
|       - | 3018 | ` */` |
|   55251 | 3019 | `PH7_PRIVATE int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3020 | `{` |
|   55256 | 3021 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtolower",1,"$string"); }` |
|       - | 3022 | `	const char *zString,*zCur,*zEnd;` |
|       - | 3023 | `	int nLen;` |
|   55256 | 3024 | `	if( nArg < 1 ){` |
|       - | 3025 | `		/* Missing arguments,return null */` |
|     ! 0 | 3026 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3027 | `		return PH7_OK;` |
|       - | 3028 | `	}` |
|       - | 3029 | `	/* Extract the target string */` |
|   55256 | 3030 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|   55256 | 3031 | `	if( nLen < 1 ){` |
|       - | 3032 | `		/* Empty string,return */` |
|      13 | 3033 | `		ph7_result_string(pCtx,"",0);` |
|      13 | 3034 | `		return PH7_OK;` |
|       - | 3035 | `	}` |
|       - | 3036 | `	/* Perform the requested operation */` |
|   55246 | 3037 | `	zEnd = &zString[nLen];` |
|  192980 | 3038 | `	for(;;){` |
|  385845 | 3039 | `		if( zString >= zEnd ){` |
|       - | 3040 | `			/* No more input,break immediately */` |
|   55246 | 3041 | `			break;` |
|       - | 3042 | `		}` |
|  330604 | 3043 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|       - | 3044 | `			/* UTF-8 stream,output verbatim */` |
|       9 | 3045 | `			zCur = zString;` |
|       9 | 3046 | `			zString++;` |
|      13 | 3047 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|       5 | 3048 | `				zString++;` |
|       1 | 3049 | `			}` |
|       - | 3050 | `			/* Append UTF-8 stream */` |
|       9 | 3051 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|       5 | 3052 | `		}else{` |
|  330596 | 3053 | `			int c = zString[0];` |
|  330596 | 3054 | `			if( SyisUpper(c) ){` |
|  280533 | 3055 | `				c = SyToLower(zString[0]);` |
|  140264 | 3056 | `			}` |
|       - | 3057 | `			/* Append character */` |
|  330596 | 3058 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|       - | 3059 | `			/* Advance the cursor */` |
|  330596 | 3060 | `			zString++;` |
|       - | 3061 | `		}` |
|       5 | 3062 | `	}` |
|   55246 | 3063 | `	return PH7_OK;` |
|   27633 | 3064 | `}` |
|       - | 3065 | `/*` |
|       - | 3066 | ` * string strtolower(string $str)` |
|       - | 3067 | ` *  Make a string uppercase.` |
|       - | 3068 | ` * Parameters` |
|       - | 3069 | ` *  $str` |
|       - | 3070 | ` *   The input string.` |
|       - | 3071 | ` * Returns.` |
|       - | 3072 | ` *  The uppercased string.` |
|       - | 3073 | ` */` |
|     256 | 3074 | `PH7_PRIVATE int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3075 | `{` |
|     261 | 3076 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtoupper",1,"$string"); }` |
|       - | 3077 | `	const char *zString,*zCur,*zEnd;` |
|       - | 3078 | `	int nLen;` |
|     261 | 3079 | `	if( nArg < 1 ){` |
|       - | 3080 | `		/* Missing arguments,return null */` |
|     ! 0 | 3081 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3082 | `		return PH7_OK;` |
|       - | 3083 | `	}` |
|       - | 3084 | `	/* Extract the target string */` |
|     261 | 3085 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     261 | 3086 | `	if( nLen < 1 ){` |
|       - | 3087 | `		/* Empty string,return */` |
|       9 | 3088 | `		ph7_result_string(pCtx,"",0);` |
|       9 | 3089 | `		return PH7_OK;` |
|       - | 3090 | `	}` |
|       - | 3091 | `	/* Perform the requested operation */` |
|     255 | 3092 | `	zEnd = &zString[nLen];` |
|     741 | 3093 | `	for(;;){` |
|    1532 | 3094 | `		if( zString >= zEnd ){` |
|       - | 3095 | `			/* No more input,break immediately */` |
|     255 | 3096 | `			break;` |
|       - | 3097 | `		}` |
|    1282 | 3098 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|       - | 3099 | `			/* UTF-8 stream,output verbatim */` |
|       9 | 3100 | `			zCur = zString;` |
|       9 | 3101 | `			zString++;` |
|      13 | 3102 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|       5 | 3103 | `				zString++;` |
|       1 | 3104 | `			}` |
|       - | 3105 | `			/* Append UTF-8 stream */` |
|       9 | 3106 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|       5 | 3107 | `		}else{` |
|    1274 | 3108 | `			int c = zString[0];` |
|    1274 | 3109 | `			if( SyisLower(c) ){` |
|    1046 | 3110 | `				c = SyToUpper(zString[0]);` |
|     502 | 3111 | `			}` |
|       - | 3112 | `			/* Append character */` |
|    1274 | 3113 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|       - | 3114 | `			/* Advance the cursor */` |
|    1274 | 3115 | `			zString++;` |
|       - | 3116 | `		}` |
|       5 | 3117 | `	}` |
|     255 | 3118 | `	return PH7_OK;` |
|     133 | 3119 | `}` |
|       - | 3120 | `/*` |
|       - | 3121 | ` * string ucfirst(string $str)` |
|       - | 3122 | ` *  Returns a string with the first character of str capitalized, if that` |
|       - | 3123 | ` *  character is alphabetic.` |
|       - | 3124 | ` * Parameters` |
|       - | 3125 | ` *  $str` |
|       - | 3126 | ` *   The input string.` |
|       - | 3127 | ` * Returns.` |
|       - | 3128 | ` *  The processed string.` |
|       - | 3129 | ` */` |
|      10 | 3130 | `PH7_PRIVATE int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3131 | `{` |
|       - | 3132 | `	const char *zString,*zEnd;` |
|       - | 3133 | `	int nLen,c;` |
|      12 | 3134 | `	if( nArg < 1 ){` |
|       - | 3135 | `		/* Missing arguments,return null */` |
|     ! 0 | 3136 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3137 | `		return PH7_OK;` |
|       - | 3138 | `	}` |
|       - | 3139 | `	/* Extract the target string */` |
|      12 | 3140 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      12 | 3141 | `	if( nLen < 1 ){` |
|       - | 3142 | `		/* Empty string,return */` |
|       6 | 3143 | `		ph7_result_string(pCtx,"",0);` |
|       6 | 3144 | `		return PH7_OK;` |
|       - | 3145 | `	}` |
|       - | 3146 | `	/* Perform the requested operation */` |
|       7 | 3147 | `	zEnd = &zString[nLen];` |
|       7 | 3148 | `	c = zString[0];` |
|       7 | 3149 | `	if( SyisLower(c) ){` |
|       5 | 3150 | `		c = SyToUpper(c);` |
|       2 | 3151 | `	}` |
|       - | 3152 | `	/* Append the first character */` |
|       7 | 3153 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|       7 | 3154 | `	zString++;` |
|       7 | 3155 | `	if( zString < zEnd ){` |
|       - | 3156 | `		/* Append the rest of the input verbatim */` |
|       7 | 3157 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|       3 | 3158 | `	}` |
|       7 | 3159 | `	return PH7_OK;` |
|       7 | 3160 | `}` |
|       - | 3161 | `/*` |
|       - | 3162 | ` * string lcfirst(string $str)` |
|       - | 3163 | ` *  Make a string's first character lowercase.` |
|       - | 3164 | ` * Parameters` |
|       - | 3165 | ` *  $str` |
|       - | 3166 | ` *   The input string.` |
|       - | 3167 | ` * Returns.` |
|       - | 3168 | ` *  The processed string.` |
|       - | 3169 | ` */` |
|       8 | 3170 | `PH7_PRIVATE int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3171 | `{` |
|       - | 3172 | `	const char *zString,*zEnd;` |
|       - | 3173 | `	int nLen,c;` |
|      10 | 3174 | `	if( nArg < 1 ){` |
|       - | 3175 | `		/* Missing arguments,return null */` |
|     ! 0 | 3176 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3177 | `		return PH7_OK;` |
|       - | 3178 | `	}` |
|       - | 3179 | `	/* Extract the target string */` |
|      10 | 3180 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      10 | 3181 | `	if( nLen < 1 ){` |
|       - | 3182 | `		/* Empty string,return */` |
|       6 | 3183 | `		ph7_result_string(pCtx,"",0);` |
|       6 | 3184 | `		return PH7_OK;` |
|       - | 3185 | `	}` |
|       - | 3186 | `	/* Perform the requested operation */` |
|       5 | 3187 | `	zEnd = &zString[nLen];` |
|       5 | 3188 | `	c = zString[0];` |
|       5 | 3189 | `	if( SyisUpper(c) ){` |
|       3 | 3190 | `		c = SyToLower(c);` |
|       1 | 3191 | `	}` |
|       - | 3192 | `	/* Append the first character */` |
|       5 | 3193 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|       5 | 3194 | `	zString++;` |
|       5 | 3195 | `	if( zString < zEnd ){` |
|       - | 3196 | `		/* Append the rest of the input verbatim */` |
|       5 | 3197 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|       2 | 3198 | `	}` |
|       5 | 3199 | `	return PH7_OK;` |
|       6 | 3200 | `}` |
|       - | 3201 | `/*` |
|       - | 3202 | ` * int ord(string $string)` |
|       - | 3203 | ` *  Returns the ASCII value of the first character of string.` |
|       - | 3204 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|       - | 3205 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|       - | 3206 | ` * Parameters` |
|       - | 3207 | ` *  $string` |
|       - | 3208 | ` *   The input string.` |
|       - | 3209 | ` * Returns` |
|       - | 3210 | ` *  The ASCII value as an integer.` |
|       - | 3211 | ` */` |
|     236 | 3212 | `PH7_PRIVATE int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 3213 | `{` |
|       - | 3214 | `	const char *zString;` |
|       - | 3215 | `	int nLen,c;` |
|       - | 3216 | `	/* PHP requires exactly one argument. */` |
|     239 | 3217 | `	if( nArg != 1 ){` |
|     ! 0 | 3218 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3219 | `			"ArgumentCountError",` |
|       - | 3220 | `			"ord() expects exactly 1 argument, %d given",` |
|     ! 0 | 3221 | `			nArg` |
|       - | 3222 | `			);` |
|       - | 3223 | `	}` |
|       - | 3224 | `	/* php only DEPRECATES null here; PHL rejects it. */` |
|     239 | 3225 | `	if( ph7_value_is_null(apArg[0]) ){` |
|     ! 0 | 3226 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 3227 | `			"ord(): Argument #1 ($character) must be of type string, null given"` |
|       - | 3228 | `			);` |
|       - | 3229 | `	}` |
|       - | 3230 | `	/* Extract the target string */` |
|     239 | 3231 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     239 | 3232 | `	if( nLen < 1 ){` |
|       - | 3233 | `		/* php only DEPRECATES an empty string here; PHL rejects it. */` |
|       3 | 3234 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 3235 | `			"ord(): Argument #1 ($character) must not be empty"` |
|       - | 3236 | `			);` |
|       - | 3237 | `	}` |
|       - | 3238 | `	/* A string longer than one byte: php DEPRECATES it; PHL rejects it. */` |
|     237 | 3239 | `	if( nLen > 1 ){` |
|       3 | 3240 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 3241 | `			"ord(): Argument #1 ($character) must be a single byte, use ord($str[0]) instead"` |
|       - | 3242 | `			);` |
|       - | 3243 | `	}` |
|       - | 3244 | `	/* Extract the ASCII value of the first character */` |
|     235 | 3245 | `	c = (unsigned char)zString[0];` |
|       - | 3246 | `	/* Return that value */` |
|     235 | 3247 | `	ph7_result_int(pCtx,c);` |
|     235 | 3248 | `	return PH7_OK;` |
|     121 | 3249 | `}` |
|       - | 3250 | `/*` |
|       - | 3251 | ` * string chr(int $codepoint)` |
|       - | 3252 | ` *  Returns a one-character string containing the character specified` |
|       - | 3253 | ` *  by the given codepoint, which must be in the [0, 255] range.` |
|       - | 3254 | ` * Parameters` |
|       - | 3255 | ` *  $codepoint` |
|       - | 3256 | ` *   An integer codepoint in [0, 255]. php merely deprecates values` |
|       - | 3257 | ` *   outside that range (constraining them with % 256); PHL rejects` |
|       - | 3258 | ` *   them with a ValueError (scope policy).` |
|       - | 3259 | ` * Returns` |
|       - | 3260 | ` *  A single-character string.` |
|       - | 3261 | ` */` |
|  726609 | 3262 | `PH7_PRIVATE int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3263 | `{` |
|       - | 3264 | `	int c;` |
|       - | 3265 | `	unsigned char ch;` |
|       - | 3266 | `	/* PHP requires exactly one argument. */` |
|  726614 | 3267 | `	if( nArg != 1 ){` |
|     ! 0 | 3268 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3269 | `			"ArgumentCountError",` |
|       - | 3270 | `			"chr() expects exactly 1 argument, %d given",` |
|     ! 0 | 3271 | `			nArg` |
|       - | 3272 | `			);` |
|       - | 3273 | `	}` |
|       - | 3274 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|       - | 3275 | `	 * PHP does not prefix this message with "chr():", so we call` |
|       - | 3276 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|       - | 3277 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|  726614 | 3278 | `	if( ph7_value_is_float(apArg[0]) ){` |
|     ! 0 | 3279 | `		double d = ph7_value_to_double(apArg[0]);` |
|       - | 3280 | ``		/* The range test comes FIRST: `(sxi64)d` is undefined outside it, and a`` |
|       - | 3281 | `		 * test of an undefined cast is one an optimiser may delete. NaN and both` |
|       - | 3282 | `		 * infinities fail it, which is the answer wanted anyway. */` |
|     ! 0 | 3283 | `		if( !PH7_RealFitsInt64(d) \|\| d != (double)(sxi64)d ){` |
|       - | 3284 | `			/* php only DEPRECATES a lossy float->int here; PHL rejects it. */` |
|     ! 0 | 3285 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 3286 | `				"chr(): Argument #1 ($codepoint) must be of type int, float given");` |
|       - | 3287 | `		}` |
|     ! 0 | 3288 | `	}` |
|       - | 3289 | `	/* Extract the codepoint. */` |
|  726614 | 3290 | `	c = ph7_value_to_int(apArg[0]);` |
|       - | 3291 | `	/* php only DEPRECATES an out-of-range codepoint (constraining it with % 256);` |
|       - | 3292 | `	 * PHL targets php's non-deprecated surface and rejects it loudly, matching the` |
|       - | 3293 | `	 * lossy-float branch above. This was the last engine site still emitting` |
|       - | 3294 | `	 * E_DEPRECATED — the scope policy says none remain. */` |
|  726614 | 3295 | `	if( c < 0 \|\| c > 255 ){` |
|       5 | 3296 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 3297 | `			"chr(): Argument #1 ($codepoint) must be between 0 and 255");` |
|       - | 3298 | `	}` |
|       - | 3299 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|       - | 3300 | `	 * when taking the address of a wider int. */` |
|  726610 | 3301 | `	ch = (unsigned char)(c & 0xFF);` |
|       - | 3302 | `	/* Return the specified character */` |
|  726610 | 3303 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|  726610 | 3304 | `	return PH7_OK;` |
|  363482 | 3305 | `}` |
|       - | 3306 | `/*` |
|       - | 3307 | ` * Binary to hex consumer callback.` |
|       - | 3308 | ` * This callback is the default consumer used by the hash functions` |
|       - | 3309 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|       - | 3310 | ` */` |
|  656761 | 3311 | `PH7_PRIVATE int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|       5 | 3312 | `{` |
|       - | 3313 | `	/* Append hex chunk verbatim */` |
|  656766 | 3314 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|  656766 | 3315 | `	return SXRET_OK;` |
|       5 | 3316 | `}` |
|       - | 3317 |  |
|       - | 3318 | `/*` |
|       - | 3319 | ` * string bin2hex(string $str)` |
|       - | 3320 | ` *  Convert binary data into hexadecimal representation.` |
|       - | 3321 | ` * Parameters` |
|       - | 3322 | ` *  $str` |
|       - | 3323 | ` *   The input string.` |
|       - | 3324 | ` * Returns.` |
|       - | 3325 | ` *  Returns the hexadecimal representation of the given string.` |
|       - | 3326 | ` */` |
|   13579 | 3327 | `PH7_PRIVATE int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3328 | `{` |
|       - | 3329 | `	const char *zString;` |
|       - | 3330 | `	int nLen;` |
|       - | 3331 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|   13584 | 3332 | `	if( nArg != 1 ){` |
|     ! 0 | 3333 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3334 | `			"ArgumentCountError",` |
|       - | 3335 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|     ! 0 | 3336 | `			nArg` |
|       - | 3337 | `			);` |
|       - | 3338 | `	}` |
|       - | 3339 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|       - | 3340 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|       - | 3341 | `	 * Objects without __toString() must also raise a TypeError.` |
|       - | 3342 | `	 */` |
|   20375 | 3343 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|    6788 | 3344 | `		( ph7_value_is_object(apArg[0]) &&` |
|     ! 0 | 3345 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|     ! 0 | 3346 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|     ! 0 | 3347 | `			"__toString",sizeof("__toString")-1) == 0` |
|       - | 3348 | `		)` |
|       - | 3349 | `	){` |
|     ! 0 | 3350 | `		const char *zType = ph7_type_name(apArg[0]);` |
|     ! 0 | 3351 | `		if( ph7_value_is_object(apArg[0]) ){` |
|     ! 0 | 3352 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     ! 0 | 3353 | `			if( pInst && pInst->pClass ){` |
|     ! 0 | 3354 | `				zType = SyStringData(&pInst->pClass->sName);` |
|     ! 0 | 3355 | `			}` |
|     ! 0 | 3356 | `		}` |
|     ! 0 | 3357 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3358 | `			"TypeError",` |
|       - | 3359 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|     ! 0 | 3360 | `			zType` |
|       - | 3361 | `			);` |
|       - | 3362 | `	}` |
|       - | 3363 | `	/* Extract the target string */` |
|   13584 | 3364 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|   13584 | 3365 | `	if( nLen < 1 ){` |
|       - | 3366 | `		/* Empty string,return */` |
|     156 | 3367 | `		ph7_result_string(pCtx,"",0);` |
|     156 | 3368 | `		return PH7_OK;` |
|       - | 3369 | `	}` |
|       - | 3370 | `	/* Perform the requested operation */` |
|   13432 | 3371 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|   13432 | 3372 | `	return PH7_OK;` |
|    6793 | 3373 | `}` |
|       - | 3374 |  |
|       - | 3375 | `/* Search callback signature */` |
|       - | 3376 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|       - | 3377 | `/*` |
|       - | 3378 | ` * Case-insensitive pattern match.` |
|       - | 3379 | ` * Brute force is the default search method used here.` |
|       - | 3380 | ` * This is due to the fact that brute-forcing works quite` |
|       - | 3381 | ` * well for short/medium texts on modern hardware.` |
|       - | 3382 | ` */` |
|    5141 | 3383 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|       5 | 3384 | `{` |
|    5146 | 3385 | `	const char *zpIn = (const char *)pPattern;` |
|    5146 | 3386 | `	const char *zIn = (const char *)pText;` |
|    5146 | 3387 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    5146 | 3388 | `	const char *zEnd = &zIn[nLen];` |
|       - | 3389 | `	const char *zPtr,*zPtr2;` |
|       - | 3390 | `	int c,d;` |
|    5146 | 3391 | `	if( iPatLen > nLen ){` |
|       - | 3392 | `		/* Don't bother processing */` |
|      36 | 3393 | `		return SXERR_NOTFOUND;` |
|       - | 3394 | `	}` |
|   43004 | 3395 | `	for(;;){` |
|   87850 | 3396 | `		if( zIn >= zEnd ){` |
|    4808 | 3397 | `			break;` |
|       - | 3398 | `		}` |
|   83045 | 3399 | `		c = SyToLower(zIn[0]);` |
|   83045 | 3400 | `		d = SyToLower(zpIn[0]);` |
|   83045 | 3401 | `		if( c == d ){` |
|    1788 | 3402 | `			zPtr   = &zIn[1];` |
|    1788 | 3403 | `			zPtr2  = &zpIn[1];` |
|    1883 | 3404 | `			for(;;){` |
|    3383 | 3405 | `				if( zPtr2 >= zpEnd ){` |
|       - | 3406 | `					/* Pattern found */` |
|     308 | 3407 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     308 | 3408 | `					return SXRET_OK;` |
|       - | 3409 | `				}` |
|    3080 | 3410 | `				if( zPtr >= zEnd ){` |
|     173 | 3411 | `					break;` |
|       - | 3412 | `				}` |
|    2909 | 3413 | `				c = SyToLower(zPtr[0]);` |
|    2909 | 3414 | `				d = SyToLower(zPtr2[0]);` |
|    2909 | 3415 | `				if( c != d ){` |
|    1312 | 3416 | `					break;` |
|       - | 3417 | `				}` |
|    1600 | 3418 | `				zPtr++; zPtr2++;` |
|       5 | 3419 | `			}` |
|     724 | 3420 | `		}` |
|   82740 | 3421 | `		zIn++;` |
|       3 | 3422 | `	}` |
|       - | 3423 | `	/* Pattern not found */` |
|    4808 | 3424 | `	return SXERR_NOTFOUND;` |
|    2519 | 3425 | `}` |
|       - | 3426 | `/*` |
|       - | 3427 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|       - | 3428 | ` *  Find the first occurrence of a string.` |
|       - | 3429 | ` * Parameters` |
|       - | 3430 | ` *  $haystack` |
|       - | 3431 | ` *   The input string.` |
|       - | 3432 | ` * $needle` |
|       - | 3433 | ` *   Search pattern (must be a string).` |
|       - | 3434 | ` * $before_needle` |
|       - | 3435 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|       - | 3436 | ` *   of the needle (excluding the needle).` |
|       - | 3437 | ` * Return` |
|       - | 3438 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|       - | 3439 | ` */` |
|     116 | 3440 | `PH7_PRIVATE int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 3441 | `{` |
|     120 | 3442 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|       - | 3443 | `	const char *zBlob,*zPattern;` |
|       - | 3444 | `	int nLen,nPatLen;` |
|       - | 3445 | `	sxu32 nOfft;` |
|       - | 3446 | `	sxi32 rc;` |
|     120 | 3447 | `	if( nArg < 2 ){` |
|       - | 3448 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3449 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3450 | `		return PH7_OK;` |
|       - | 3451 | `	}` |
|       - | 3452 | `	/* Extract the needle and the haystack */` |
|     120 | 3453 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     120 | 3454 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     120 | 3455 | `	nOfft = 0; /* cc warning */` |
|     120 | 3456 | `	if( nPatLen < 1 ){` |
|       - | 3457 | `		/* php 8: the empty needle matches at position 0, so the whole haystack` |
|       - | 3458 | `		 * is returned (and nothing at all when $before_needle is set). */` |
|       7 | 3459 | `		if( nArg > 2 && ph7_value_to_bool(apArg[2]) ){` |
|       3 | 3460 | `			ph7_result_string(pCtx,"",0);` |
|       2 | 3461 | `		}else{` |
|       5 | 3462 | `			ph7_result_string(pCtx,zBlob,nLen);` |
|       - | 3463 | `		}` |
|       7 | 3464 | `		return PH7_OK;` |
|       - | 3465 | `	}` |
|     114 | 3466 | `	if( nLen > 0 ){` |
|     114 | 3467 | `		int before = 0;` |
|       - | 3468 | `		/* Perform the lookup */` |
|     114 | 3469 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     114 | 3470 | `		if( rc != SXRET_OK ){` |
|       - | 3471 | `			/* Pattern not found,return FALSE */` |
|       8 | 3472 | `			ph7_result_bool(pCtx,0);` |
|       8 | 3473 | `			return PH7_OK;` |
|       - | 3474 | `		}` |
|       - | 3475 | `		/* Return the portion of the string */` |
|     108 | 3476 | `		if( nArg > 2 ){` |
|      72 | 3477 | `			before = ph7_value_to_int(apArg[2]);` |
|      34 | 3478 | `		}` |
|     108 | 3479 | `		if( before ){` |
|      72 | 3480 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      38 | 3481 | `		}else{` |
|      37 | 3482 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|       - | 3483 | `		}` |
|      56 | 3484 | `	}else{` |
|     ! 0 | 3485 | `		ph7_result_bool(pCtx,0);` |
|       - | 3486 | `	}` |
|     108 | 3487 | `	return PH7_OK;` |
|      62 | 3488 | `}` |
|       - | 3489 | `/*` |
|       - | 3490 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|       - | 3491 | ` *  Case-insensitive strstr().` |
|       - | 3492 | ` * Parameters` |
|       - | 3493 | ` *  $haystack` |
|       - | 3494 | ` *   The input string.` |
|       - | 3495 | ` * $needle` |
|       - | 3496 | ` *   Search pattern (must be a string).` |
|       - | 3497 | ` * $before_needle` |
|       - | 3498 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|       - | 3499 | ` *   of the needle (excluding the needle).` |
|       - | 3500 | ` * Return` |
|       - | 3501 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|       - | 3502 | ` */` |
|       6 | 3503 | `PH7_PRIVATE int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3504 | `{` |
|       7 | 3505 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|       - | 3506 | `	const char *zBlob,*zPattern;` |
|       - | 3507 | `	int nLen,nPatLen;` |
|       - | 3508 | `	sxu32 nOfft;` |
|       - | 3509 | `	sxi32 rc;` |
|       7 | 3510 | `	if( nArg < 2 ){` |
|       - | 3511 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3512 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3513 | `		return PH7_OK;` |
|       - | 3514 | `	}` |
|       - | 3515 | `	/* Extract the needle and the haystack */` |
|       7 | 3516 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|       7 | 3517 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|       7 | 3518 | `	nOfft = 0; /* cc warning */` |
|       7 | 3519 | `	if( nPatLen < 1 ){` |
|       - | 3520 | `		/* php 8: the empty needle matches at position 0, so the whole haystack` |
|       - | 3521 | `		 * is returned (and nothing at all when $before_needle is set). */` |
|       3 | 3522 | `		if( nArg > 2 && ph7_value_to_bool(apArg[2]) ){` |
|     ! 0 | 3523 | `			ph7_result_string(pCtx,"",0);` |
|     ! 0 | 3524 | `		}else{` |
|       3 | 3525 | `			ph7_result_string(pCtx,zBlob,nLen);` |
|       - | 3526 | `		}` |
|       3 | 3527 | `		return PH7_OK;` |
|       - | 3528 | `	}` |
|       5 | 3529 | `	if( nLen > 0 ){` |
|       5 | 3530 | `		int before = 0;` |
|       - | 3531 | `		/* Perform the lookup */` |
|       5 | 3532 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|       5 | 3533 | `		if( rc != SXRET_OK ){` |
|       - | 3534 | `			/* Pattern not found,return FALSE */` |
|     ! 0 | 3535 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 3536 | `			return PH7_OK;` |
|       - | 3537 | `		}` |
|       - | 3538 | `		/* Return the portion of the string */` |
|       5 | 3539 | `		if( nArg > 2 ){` |
|       3 | 3540 | `			before = ph7_value_to_int(apArg[2]);` |
|       1 | 3541 | `		}` |
|       5 | 3542 | `		if( before ){` |
|       3 | 3543 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|       2 | 3544 | `		}else{` |
|       3 | 3545 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|       - | 3546 | `		}` |
|       3 | 3547 | `	}else{` |
|     ! 0 | 3548 | `		ph7_result_bool(pCtx,0);` |
|       - | 3549 | `	}` |
|       5 | 3550 | `	return PH7_OK;` |
|       4 | 3551 | `}` |
|       - | 3552 | `/*` |
|       - | 3553 | ` * Resolve the $offset argument shared by strpos()/stripos().` |
|       - | 3554 | ` *` |
|       - | 3555 | ` * php requires -strlen($haystack) <= $offset <= strlen($haystack) and throws` |
|       - | 3556 | ` * ValueError otherwise; a negative offset counts back from the end. PHL used to` |
|       - | 3557 | ` * negate a negative offset and silently clamp an out-of-range one to zero, so` |
|       - | 3558 | ` * strpos("Hello","l",100) answered 2 where php raises — an argument error` |
|       - | 3559 | ` * turned into a wrong answer.` |
|       - | 3560 | ` *` |
|       - | 3561 | ` * On success *pnStart receives the resolved non-negative offset.` |
|       - | 3562 | ` */` |
|      10 | 3563 | `static sxi32 StrSearchOffset(` |
|       - | 3564 | `	ph7_context *pCtx,` |
|       - | 3565 | `	ph7_value *pArg,` |
|       - | 3566 | `	int nLen,` |
|       - | 3567 | `	const char *zFunc,` |
|       - | 3568 | `	int *pnStart` |
|       - | 3569 | `	)` |
|       1 | 3570 | `{` |
|      11 | 3571 | `	ph7_int64 iOfft = ph7_value_to_int64(pArg);` |
|       - | 3572 | `	/* Compare without negating iOfft: -INT64_MIN would overflow. */` |
|      11 | 3573 | `	if( iOfft < 0 ? (iOfft < -(ph7_int64)nLen) : (iOfft > (ph7_int64)nLen) ){` |
|       8 | 3574 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       4 | 3575 | `			"%s(): Argument #3 ($offset) must be contained in argument #1 ($haystack)",zFunc);` |
|       - | 3576 | `	}` |
|      11 | 3577 | `	*pnStart = (int)(iOfft < 0 ? (ph7_int64)nLen + iOfft : iOfft);` |
|      11 | 3578 | `	return PH7_OK;` |
|      10 | 3579 | `}` |
|       - | 3580 | `/*` |
|       - | 3581 | ` * Resolve the window of match START positions for strrpos()/strripos().` |
|       - | 3582 | ` *` |
|       - | 3583 | ` * php's rule is asymmetric in the sign of $offset: a non-negative offset is a` |
|       - | 3584 | ` * LOWER bound on where the match may start, while a negative one is an UPPER` |
|       - | 3585 | ` * bound counted back from the end of the haystack (zend_memnrstr). The range` |
|       - | 3586 | ` * check is the same as StrSearchOffset()'s.` |
|       - | 3587 | ` *` |
|       - | 3588 | ` * On success the closed interval [*pnMin,*pnMax] holds every position at which` |
|       - | 3589 | ` * a match is allowed to begin; it is empty (max < min) when the needle cannot` |
|       - | 3590 | ` * fit, which the caller reports as FALSE.` |
|       - | 3591 | ` */` |
|     988 | 3592 | `static sxi32 StrRSearchWindow(` |
|       - | 3593 | `	ph7_context *pCtx,` |
|       - | 3594 | `	ph7_value *pArg, /* The $offset argument, or NULL when it was omitted */` |
|       - | 3595 | `	int nLen,` |
|       - | 3596 | `	int nPatLen,` |
|       - | 3597 | `	const char *zFunc,` |
|       - | 3598 | `	int *pnMin,` |
|       - | 3599 | `	int *pnMax` |
|       - | 3600 | `	)` |
|       5 | 3601 | `{` |
|     993 | 3602 | `	int nMin = 0;` |
|     993 | 3603 | `	int nMax = nLen - nPatLen;` |
|     993 | 3604 | `	if( pArg ){` |
|      47 | 3605 | `		ph7_int64 iOfft = ph7_value_to_int64(pArg);` |
|      47 | 3606 | `		if( iOfft < 0 ? (iOfft < -(ph7_int64)nLen) : (iOfft > (ph7_int64)nLen) ){` |
|      33 | 3607 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      14 | 3608 | `				"%s(): Argument #3 ($offset) must be contained in argument #1 ($haystack)",zFunc);` |
|       - | 3609 | `		}` |
|      29 | 3610 | `		if( iOfft < 0 ){` |
|      15 | 3611 | `			int nLimit = nLen + (int)iOfft;` |
|      15 | 3612 | `			if( nMax > nLimit ){` |
|      15 | 3613 | `				nMax = nLimit;` |
|       7 | 3614 | `			}` |
|       8 | 3615 | `		}else{` |
|      15 | 3616 | `			nMin = (int)iOfft;` |
|       - | 3617 | `		}` |
|      14 | 3618 | `	}` |
|     975 | 3619 | `	*pnMin = nMin;` |
|     975 | 3620 | `	*pnMax = nMax;` |
|     975 | 3621 | `	return PH7_OK;` |
|     493 | 3622 | `}` |
|       - | 3623 | `/*` |
|       - | 3624 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|       - | 3625 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|       - | 3626 | ` * Parameters` |
|       - | 3627 | ` *  $haystack` |
|       - | 3628 | ` *   The input string.` |
|       - | 3629 | ` * $needle` |
|       - | 3630 | ` *   Search pattern (must be a string).` |
|       - | 3631 | ` * $offset` |
|       - | 3632 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|       - | 3633 | ` *   to start searching. The position returned is still relative to the beginning` |
|       - | 3634 | ` *   of haystack.` |
|       - | 3635 | ` * Return` |
|       - | 3636 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|       - | 3637 | ` */` |
|    9050 | 3638 | `PH7_PRIVATE int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3639 | `{` |
|    9055 | 3640 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strpos",1,"$haystack"); }` |
|    9055 | 3641 | `	if( nArg > 1 ){ StrNullArgNotice(pCtx,apArg[1],"strpos",2,"$needle"); }` |
|    9055 | 3642 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|       - | 3643 | `	const char *zBlob,*zPattern;` |
|       - | 3644 | `	int nLen,nPatLen,nStart;` |
|       - | 3645 | `	sxu32 nOfft;` |
|       - | 3646 | `	sxi32 rc;` |
|    9055 | 3647 | `	if( nArg < 2 ){` |
|       - | 3648 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3649 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3650 | `		return PH7_OK;` |
|       - | 3651 | `	}` |
|       - | 3652 | `	/* Extract the needle and the haystack */` |
|    9055 | 3653 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    9055 | 3654 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    9055 | 3655 | `	nOfft = 0; /* cc warning */` |
|    9055 | 3656 | `	nStart = 0;` |
|       - | 3657 | `	/* Peek the starting offset if available */` |
|    9055 | 3658 | `	if( nArg > 2 ){` |
|       7 | 3659 | `		rc = StrSearchOffset(pCtx,apArg[2],nLen,"strpos",&nStart);` |
|       7 | 3660 | `		if( rc != PH7_OK ){` |
|     ! 0 | 3661 | `			return rc;` |
|       - | 3662 | `		}` |
|       3 | 3663 | `	}` |
|    9055 | 3664 | `	if( nPatLen < 1 ){` |
|       - | 3665 | `		/* php 8 treats the empty needle as matching at the search offset. */` |
|      11 | 3666 | `		ph7_result_int64(pCtx,(ph7_int64)nStart);` |
|      11 | 3667 | `		return PH7_OK;` |
|       - | 3668 | `	}` |
|    9045 | 3669 | `	zBlob += nStart;` |
|    9045 | 3670 | `	nLen -= nStart;` |
|    9045 | 3671 | `	if( nLen > 0 ){` |
|       - | 3672 | `		/* Perform the lookup */` |
|    8848 | 3673 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    8848 | 3674 | `		if( rc != SXRET_OK ){` |
|       - | 3675 | `			/* Pattern not found,return FALSE */` |
|    7767 | 3676 | `			ph7_result_bool(pCtx,0);` |
|    7767 | 3677 | `			return PH7_OK;` |
|       - | 3678 | `		}` |
|       - | 3679 | `		/* Return the pattern position */` |
|    1086 | 3680 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|     543 | 3681 | `	}else{` |
|     202 | 3682 | `		ph7_result_bool(pCtx,0);` |
|       - | 3683 | `	}` |
|    1283 | 3684 | `	return PH7_OK;` |
|    4420 | 3685 | `}` |
|       - | 3686 | `/*` |
|       - | 3687 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|       - | 3688 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|       - | 3689 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|       - | 3690 | ` * TypeError for arrays, resources, and objects without __toString.` |
|       - | 3691 | ` *` |
|       - | 3692 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|       - | 3693 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|       - | 3694 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|       - | 3695 | ` *` |
|       - | 3696 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|       - | 3697 | ` * is valid until pTmp is released or pArg is mutated.` |
|       - | 3698 | ` */` |
|   28720 | 3699 | `static sxi32 StrPredicateResolveArg(` |
|       - | 3700 | `	ph7_context *pCtx,` |
|       - | 3701 | `	ph7_value *pArg,` |
|       - | 3702 | `	const char *zFunc,` |
|       - | 3703 | `	int iArgNum,` |
|       - | 3704 | `	const char *zParamName,` |
|       - | 3705 | `	const char *zTypeStr, /* Declared type in the TypeError, e.g. "string" / "?string" */` |
|       - | 3706 | `	const char *zNullMsg,` |
|       - | 3707 | `	ph7_value *pTmp,` |
|       - | 3708 | `	const char **pzOut,` |
|       - | 3709 | `	int *pnOut` |
|       5 | 3710 | `){` |
|   14079 | 3711 | `	SXUNUSED(zNullMsg); /* php's deprecation text — PHL rejects null instead of coercing */` |
|   28725 | 3712 | `	if( ph7_value_is_null(pArg) ){` |
|       - | 3713 | `		/* php only DEPRECATES null here; PHL rejects it with the TypeError php will` |
|       - | 3714 | `		 * eventually raise. */` |
|     ! 0 | 3715 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 3716 | `			"%s(): Argument #%d (%s) must be of type %s, null given",` |
|     ! 0 | 3717 | `			zFunc,iArgNum,zParamName,zTypeStr);` |
|       - | 3718 | `	}` |
|   43390 | 3719 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|   28720 | 3720 | `	    ( ph7_value_is_object(pArg) &&` |
|      72 | 3721 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|      48 | 3722 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|      24 | 3723 | `	        "__toString",sizeof("__toString")-1) == 0` |
|       - | 3724 | `	    )` |
|       - | 3725 | `	){` |
|     ! 0 | 3726 | `		const char *zType = ph7_type_name(pArg);` |
|     ! 0 | 3727 | `		if( ph7_value_is_object(pArg) ){` |
|     ! 0 | 3728 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     ! 0 | 3729 | `			if( pInst && pInst->pClass ){` |
|     ! 0 | 3730 | `				zType = SyStringData(&pInst->pClass->sName);` |
|     ! 0 | 3731 | `			}` |
|     ! 0 | 3732 | `		}` |
|     ! 0 | 3733 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3734 | `			"TypeError",` |
|       - | 3735 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|     ! 0 | 3736 | `			zFunc, iArgNum, zParamName, zTypeStr, zType` |
|       - | 3737 | `			);` |
|       - | 3738 | `	}` |
|   28725 | 3739 | `	if( ph7_value_is_object(pArg) ){` |
|      49 | 3740 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|      49 | 3741 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 3742 | `			"__toString",sizeof("__toString")-1);` |
|      49 | 3743 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|      49 | 3744 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|      49 | 3745 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|      49 | 3746 | `		return PH7_OK;` |
|       - | 3747 | `	}` |
|   28677 | 3748 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|   28677 | 3749 | `	return PH7_OK;` |
|   14084 | 3750 | `}` |
|       - | 3751 | `/*` |
|       - | 3752 | ` * bool str_contains(string $haystack, string $needle)` |
|       - | 3753 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|       - | 3754 | ` * Return` |
|       - | 3755 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|       - | 3756 | ` */` |
|   11940 | 3757 | `PH7_PRIVATE int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3758 | `{` |
|       - | 3759 | `	const char *zHaystack,*zNeedle;` |
|       - | 3760 | `	int nHayLen,nNeedleLen;` |
|       - | 3761 | `	ph7_value sHayTmp,sNeedleTmp;` |
|       - | 3762 | `	sxi32 rc;` |
|   11945 | 3763 | `	if( nArg != 2 ){` |
|     ! 0 | 3764 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3765 | `			"ArgumentCountError",` |
|       - | 3766 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|     ! 0 | 3767 | `			nArg` |
|       - | 3768 | `			);` |
|       - | 3769 | `	}` |
|   11945 | 3770 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|   11945 | 3771 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|   11945 | 3772 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack","string",` |
|       - | 3773 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|       - | 3774 | `		"of type string is deprecated",` |
|       - | 3775 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|   11945 | 3776 | `	if( rc != PH7_OK ) goto out;` |
|   11945 | 3777 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle","string",` |
|       - | 3778 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|       - | 3779 | `		"of type string is deprecated",` |
|       - | 3780 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|   11945 | 3781 | `	if( rc != PH7_OK ) goto out;` |
|   11945 | 3782 | `	if( nNeedleLen < 1 ){` |
|      11 | 3783 | `		ph7_result_bool(pCtx,1);` |
|   11940 | 3784 | `	}else if( nHayLen < nNeedleLen ){` |
|     565 | 3785 | `		ph7_result_bool(pCtx,0);` |
|     290 | 3786 | `	}else{` |
|   17036 | 3787 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|    5664 | 3788 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|   11372 | 3789 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|       - | 3790 | `	}` |
|   11945 | 3791 | `	rc = PH7_OK;` |
|    5983 | 3792 | `out:` |
|   11945 | 3793 | `	PH7_MemObjRelease(&sHayTmp);` |
|   11945 | 3794 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|   11945 | 3795 | `	return rc;` |
|    5962 | 3796 | `}` |
|       - | 3797 | `/*` |
|       - | 3798 | ` * bool str_starts_with(string $haystack, string $needle)` |
|       - | 3799 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|       - | 3800 | ` * Return` |
|       - | 3801 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|       - | 3802 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|       - | 3803 | ` */` |
|    2237 | 3804 | `PH7_PRIVATE int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3805 | `{` |
|       - | 3806 | `	const char *zHaystack,*zNeedle;` |
|       - | 3807 | `	int nHayLen,nNeedleLen;` |
|       - | 3808 | `	ph7_value sHayTmp,sNeedleTmp;` |
|       - | 3809 | `	sxi32 rc;` |
|    2242 | 3810 | `	if( nArg != 2 ){` |
|     ! 0 | 3811 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3812 | `			"ArgumentCountError",` |
|       - | 3813 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|     ! 0 | 3814 | `			nArg` |
|       - | 3815 | `			);` |
|       - | 3816 | `	}` |
|    2242 | 3817 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|    2242 | 3818 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|    2242 | 3819 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack","string",` |
|       - | 3820 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|       - | 3821 | `		"of type string is deprecated",` |
|       - | 3822 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|    2242 | 3823 | `	if( rc != PH7_OK ) goto out;` |
|    2242 | 3824 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle","string",` |
|       - | 3825 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|       - | 3826 | `		"of type string is deprecated",` |
|       - | 3827 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|    2242 | 3828 | `	if( rc != PH7_OK ) goto out;` |
|    2242 | 3829 | `	if( nNeedleLen < 1 ){` |
|      14 | 3830 | `		ph7_result_bool(pCtx,1);` |
|    2236 | 3831 | `	}else if( nHayLen < nNeedleLen ){` |
|     232 | 3832 | `		ph7_result_bool(pCtx,0);` |
|     117 | 3833 | `	}else{` |
|    2876 | 3834 | `		ph7_result_bool(pCtx,` |
|    1998 | 3835 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|       - | 3836 | `	}` |
|    2242 | 3837 | `	rc = PH7_OK;` |
|    1246 | 3838 | `out:` |
|    2242 | 3839 | `	PH7_MemObjRelease(&sHayTmp);` |
|    2242 | 3840 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|    2242 | 3841 | `	return rc;` |
|     996 | 3842 | `}` |
|       - | 3843 | `/*` |
|       - | 3844 | ` * bool str_ends_with(string $haystack, string $needle)` |
|       - | 3845 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|       - | 3846 | ` * Return` |
|       - | 3847 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|       - | 3848 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|       - | 3849 | ` */` |
|      60 | 3850 | `PH7_PRIVATE int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3851 | `{` |
|       - | 3852 | `	const char *zHaystack,*zNeedle;` |
|       - | 3853 | `	int nHayLen,nNeedleLen;` |
|       - | 3854 | `	ph7_value sHayTmp,sNeedleTmp;` |
|       - | 3855 | `	sxi32 rc;` |
|      61 | 3856 | `	if( nArg != 2 ){` |
|     ! 0 | 3857 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3858 | `			"ArgumentCountError",` |
|       - | 3859 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|     ! 0 | 3860 | `			nArg` |
|       - | 3861 | `			);` |
|       - | 3862 | `	}` |
|      61 | 3863 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|      61 | 3864 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|      61 | 3865 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack","string",` |
|       - | 3866 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|       - | 3867 | `		"of type string is deprecated",` |
|       - | 3868 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|      61 | 3869 | `	if( rc != PH7_OK ) goto out;` |
|      61 | 3870 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle","string",` |
|       - | 3871 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|       - | 3872 | `		"of type string is deprecated",` |
|       - | 3873 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|      61 | 3874 | `	if( rc != PH7_OK ) goto out;` |
|      61 | 3875 | `	if( nNeedleLen < 1 ){` |
|      11 | 3876 | `		ph7_result_bool(pCtx,1);` |
|      56 | 3877 | `	}else if( nHayLen < nNeedleLen ){` |
|       7 | 3878 | `		ph7_result_bool(pCtx,0);` |
|       4 | 3879 | `	}else{` |
|      67 | 3880 | `		ph7_result_bool(pCtx,` |
|      44 | 3881 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|       - | 3882 | `	}` |
|      61 | 3883 | `	rc = PH7_OK;` |
|      30 | 3884 | `out:` |
|      61 | 3885 | `	PH7_MemObjRelease(&sHayTmp);` |
|      61 | 3886 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|      61 | 3887 | `	return rc;` |
|      31 | 3888 | `}` |
|       - | 3889 | `/*` |
|       - | 3890 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|       - | 3891 | ` *  Case-insensitive strpos.` |
|       - | 3892 | ` * Parameters` |
|       - | 3893 | ` *  $haystack` |
|       - | 3894 | ` *   The input string.` |
|       - | 3895 | ` * $needle` |
|       - | 3896 | ` *   Search pattern (must be a string).` |
|       - | 3897 | ` * $offset` |
|       - | 3898 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|       - | 3899 | ` *   to start searching. The position returned is still relative to the beginning` |
|       - | 3900 | ` *   of haystack.` |
|       - | 3901 | ` * Return` |
|       - | 3902 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|       - | 3903 | ` */` |
|    5007 | 3904 | `PH7_PRIVATE int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3905 | `{` |
|    5012 | 3906 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|       - | 3907 | `	const char *zBlob,*zPattern;` |
|       - | 3908 | `	int nLen,nPatLen,nStart;` |
|       - | 3909 | `	sxu32 nOfft;` |
|       - | 3910 | `	sxi32 rc;` |
|    5012 | 3911 | `	if( nArg < 2 ){` |
|       - | 3912 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3913 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3914 | `		return PH7_OK;` |
|       - | 3915 | `	}` |
|       - | 3916 | `	/* Extract the needle and the haystack */` |
|    5012 | 3917 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    5012 | 3918 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    5012 | 3919 | `	nOfft = 0; /* cc warning */` |
|    5012 | 3920 | `	nStart = 0;` |
|       - | 3921 | `	/* Peek the starting offset if available */` |
|    5012 | 3922 | `	if( nArg > 2 ){` |
|       5 | 3923 | `		rc = StrSearchOffset(pCtx,apArg[2],nLen,"stripos",&nStart);` |
|       5 | 3924 | `		if( rc != PH7_OK ){` |
|     ! 0 | 3925 | `			return rc;` |
|       - | 3926 | `		}` |
|       2 | 3927 | `	}` |
|    5012 | 3928 | `	if( nPatLen < 1 ){` |
|       - | 3929 | `		/* php 8 treats the empty needle as matching at the search offset. */` |
|       3 | 3930 | `		ph7_result_int64(pCtx,(ph7_int64)nStart);` |
|       3 | 3931 | `		return PH7_OK;` |
|       - | 3932 | `	}` |
|    5010 | 3933 | `	zBlob += nStart;` |
|    5010 | 3934 | `	nLen -= nStart;` |
|    5010 | 3935 | `	if( nLen > 0 ){` |
|       - | 3936 | `		/* Perform the lookup */` |
|    5002 | 3937 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    5002 | 3938 | `		if( rc != SXRET_OK ){` |
|       - | 3939 | `			/* Pattern not found,return FALSE */` |
|    4759 | 3940 | `			ph7_result_bool(pCtx,0);` |
|    4759 | 3941 | `			return PH7_OK;` |
|       - | 3942 | `		}` |
|       - | 3943 | `		/* Return the pattern position */` |
|     246 | 3944 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|     141 | 3945 | `	}else{` |
|       8 | 3946 | `		ph7_result_bool(pCtx,0);` |
|       - | 3947 | `	}` |
|     254 | 3948 | `	return PH7_OK;` |
|    2452 | 3949 | `}` |
|       - | 3950 | `/*` |
|       - | 3951 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|       - | 3952 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|       - | 3953 | ` * Parameters` |
|       - | 3954 | ` *  $haystack` |
|       - | 3955 | ` *   The input string.` |
|       - | 3956 | ` * $needle` |
|       - | 3957 | ` *   Search pattern (must be a string).` |
|       - | 3958 | ` * $offset` |
|       - | 3959 | ` *   If specified, search will start this number of characters counted from the beginning` |
|       - | 3960 | ` *   of the string. If the value is negative, search will instead start from that many` |
|       - | 3961 | ` *   characters from the end of the string, searching backwards.` |
|       - | 3962 | ` * Return` |
|       - | 3963 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|       - | 3964 | ` */` |
|     944 | 3965 | `PH7_PRIVATE int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3966 | `{` |
|       - | 3967 | `	const char *zBlob,*zPattern;` |
|     949 | 3968 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|       - | 3969 | `	int nLen,nPatLen,i;` |
|     949 | 3970 | `	int nMin = 0,nMax = 0;` |
|       - | 3971 | `	sxu32 nOfft;` |
|       - | 3972 | `	sxi32 rc;` |
|     949 | 3973 | `	if( nArg < 2 ){` |
|       - | 3974 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3975 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3976 | `		return PH7_OK;` |
|       - | 3977 | `	}` |
|       - | 3978 | `	/* Extract the needle and the haystack */` |
|     949 | 3979 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     949 | 3980 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     949 | 3981 | `	nOfft = 0; /* cc warning */` |
|       - | 3982 | `	/* Resolve the range of positions the match may start at */` |
|     949 | 3983 | `	rc = StrRSearchWindow(pCtx,nArg > 2 ? apArg[2] : 0,nLen,nPatLen,"strrpos",&nMin,&nMax);` |
|     949 | 3984 | `	if( rc != PH7_OK ){` |
|       5 | 3985 | `		return rc;` |
|       - | 3986 | `	}` |
|     945 | 3987 | `	if( nPatLen < 1 ){` |
|       - | 3988 | `		/* php 8: the empty needle matches everywhere, so the LAST match is the` |
|       - | 3989 | `		 * highest position the window allows. */` |
|      11 | 3990 | `		ph7_result_int64(pCtx,(ph7_int64)nMax);` |
|      11 | 3991 | `		return PH7_OK;` |
|       - | 3992 | `	}` |
|       - | 3993 | `	/* Walk backwards, comparing at each candidate position. Searching a window` |
|       - | 3994 | `	 * exactly as long as the needle makes the match test an equality test while` |
|       - | 3995 | `	 * still going through xPatternMatch, which carries the case folding. */` |
|    3851 | 3996 | `	for( i = nMax ; i >= nMin ; --i ){` |
|    3827 | 3997 | `		rc = xPatternMatch((const void *)&zBlob[i],(sxu32)nPatLen,(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    3827 | 3998 | `		if( rc == SXRET_OK ){` |
|       - | 3999 | `			/* Pattern found,return it's position */` |
|     911 | 4000 | `			ph7_result_int64(pCtx,(ph7_int64)i);` |
|     911 | 4001 | `			return PH7_OK;` |
|       - | 4002 | `		}` |
|    1462 | 4003 | `	}` |
|       - | 4004 | `	/* Pattern not found,return FALSE */` |
|      25 | 4005 | `	ph7_result_bool(pCtx,0);` |
|      25 | 4006 | `	return PH7_OK;` |
|     476 | 4007 | `}` |
|       - | 4008 | `/*` |
|       - | 4009 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|       - | 4010 | ` *  Case-insensitive strrpos.` |
|       - | 4011 | ` * Parameters` |
|       - | 4012 | ` *  $haystack` |
|       - | 4013 | ` *   The input string.` |
|       - | 4014 | ` * $needle` |
|       - | 4015 | ` *   Search pattern (must be a string).` |
|       - | 4016 | ` * $offset` |
|       - | 4017 | ` *   If specified, search will start this number of characters counted from the beginning` |
|       - | 4018 | ` *   of the string. If the value is negative, search will instead start from that many` |
|       - | 4019 | ` *   characters from the end of the string, searching backwards.` |
|       - | 4020 | ` * Return` |
|       - | 4021 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|       - | 4022 | ` */` |
|      34 | 4023 | `PH7_PRIVATE int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4024 | `{` |
|       - | 4025 | `	const char *zBlob,*zPattern;` |
|      35 | 4026 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|       - | 4027 | `	int nLen,nPatLen,i;` |
|      35 | 4028 | `	int nMin = 0,nMax = 0;` |
|       - | 4029 | `	sxu32 nOfft;` |
|       - | 4030 | `	sxi32 rc;` |
|      35 | 4031 | `	if( nArg < 2 ){` |
|       - | 4032 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 4033 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4034 | `		return PH7_OK;` |
|       - | 4035 | `	}` |
|       - | 4036 | `	/* Extract the needle and the haystack */` |
|      35 | 4037 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      35 | 4038 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      35 | 4039 | `	nOfft = 0; /* cc warning */` |
|       - | 4040 | `	/* Resolve the range of positions the match may start at */` |
|      35 | 4041 | `	rc = StrRSearchWindow(pCtx,nArg > 2 ? apArg[2] : 0,nLen,nPatLen,"strripos",&nMin,&nMax);` |
|      35 | 4042 | `	if( rc != PH7_OK ){` |
|       5 | 4043 | `		return rc;` |
|       - | 4044 | `	}` |
|      31 | 4045 | `	if( nPatLen < 1 ){` |
|       - | 4046 | `		/* php 8: the empty needle matches everywhere, so the LAST match is the` |
|       - | 4047 | `		 * highest position the window allows. */` |
|      11 | 4048 | `		ph7_result_int64(pCtx,(ph7_int64)nMax);` |
|      11 | 4049 | `		return PH7_OK;` |
|       - | 4050 | `	}` |
|       - | 4051 | `	/* Walk backwards, comparing at each candidate position (see strrpos). */` |
|      49 | 4052 | `	for( i = nMax ; i >= nMin ; --i ){` |
|      45 | 4053 | `		rc = xPatternMatch((const void *)&zBlob[i],(sxu32)nPatLen,(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|      45 | 4054 | `		if( rc == SXRET_OK ){` |
|       - | 4055 | `			/* Pattern found,return it's position */` |
|      17 | 4056 | `			ph7_result_int64(pCtx,(ph7_int64)i);` |
|      17 | 4057 | `			return PH7_OK;` |
|       - | 4058 | `		}` |
|      15 | 4059 | `	}` |
|       - | 4060 | `	/* Pattern not found,return FALSE */` |
|       5 | 4061 | `	ph7_result_bool(pCtx,0);` |
|       5 | 4062 | `	return PH7_OK;` |
|      18 | 4063 | `}` |
|       - | 4064 | `/*` |
|       - | 4065 | ` * int strrchr(string $haystack,mixed $needle)` |
|       - | 4066 | ` *  Find the last occurrence of a character in a string.` |
|       - | 4067 | ` * Parameters` |
|       - | 4068 | ` *  $haystack` |
|       - | 4069 | ` *   The input string.` |
|       - | 4070 | ` * $needle` |
|       - | 4071 | ` *  If needle contains more than one character, only the first is used.` |
|       - | 4072 | ` *  This behavior is different from that of strstr().` |
|       - | 4073 | ` *  If needle is not a string, it is converted to an integer and applied` |
|       - | 4074 | ` *  as the ordinal value of a character.` |
|       - | 4075 | ` * Return` |
|       - | 4076 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|       - | 4077 | ` */` |
|      68 | 4078 | `PH7_PRIVATE int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4079 | `{` |
|       - | 4080 | `	const char *zBlob;` |
|       - | 4081 | `	int nLen,c;` |
|      69 | 4082 | `	if( nArg < 2 ){` |
|       - | 4083 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 4084 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4085 | `		return PH7_OK;` |
|       - | 4086 | `	}` |
|       - | 4087 | `	/* Extract the haystack */` |
|      69 | 4088 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      69 | 4089 | `	c = 0; /* cc warning */` |
|      69 | 4090 | `	if( nLen > 0 ){` |
|       - | 4091 | `		const char *zPattern;` |
|       - | 4092 | `		int nPatLen;` |
|       - | 4093 | `		sxu32 nOfft;` |
|       - | 4094 | `		sxi32 rc;` |
|       - | 4095 | `		/* php 8 casts the needle to string and uses only its first character.` |
|       - | 4096 | `		 * The old "if not a string, take it as an ordinal" reading was php 7` |
|       - | 4097 | `		 * behaviour, removed in php 8: strrchr("hello world",111) now looks for` |
|       - | 4098 | `		 * "1", not "o". An empty needle matches nothing. */` |
|      65 | 4099 | `		zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      65 | 4100 | `		if( nPatLen < 1 ){` |
|       5 | 4101 | `			ph7_result_bool(pCtx,0);` |
|      22 | 4102 | `			return PH7_OK;` |
|       - | 4103 | `		}` |
|      61 | 4104 | `		c = zPattern[0];` |
|       - | 4105 | `		/* Perform the lookup */` |
|      61 | 4106 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|      61 | 4107 | `		if( rc != SXRET_OK ){` |
|       - | 4108 | `			/* No such entry,return FALSE */` |
|      11 | 4109 | `			ph7_result_bool(pCtx,0);` |
|      11 | 4110 | `			return PH7_OK;` |
|       - | 4111 | `		}` |
|       - | 4112 | `		/* php 8.3's $before_needle: TRUE answers everything in FRONT of the last` |
|       - | 4113 | `		 * occurrence instead of the occurrence and everything after it. It was` |
|       - | 4114 | `		 * declared in aBuiltinSig[], screened as a bool and then never read, so` |
|       - | 4115 | ``		 * `strrchr($path, '/', true)` -- the ordinary way to take a dirname off a`` |
|       - | 4116 | `		 * delimiter -- answered the BASENAME, with the delimiter still on it. */` |
|      51 | 4117 | `		if( nArg > 2 && ph7_value_to_bool(apArg[2]) ){` |
|      25 | 4118 | `			ph7_result_string(pCtx,zBlob,(int)nOfft);` |
|      25 | 4119 | `			return PH7_OK;` |
|       - | 4120 | `		}` |
|       - | 4121 | `		/* Return the string portion */` |
|      27 | 4122 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      14 | 4123 | `	}else{` |
|       5 | 4124 | `		ph7_result_bool(pCtx,0);` |
|       - | 4125 | `	}` |
|      31 | 4126 | `	return PH7_OK;` |
|      35 | 4127 | `}` |
|       - | 4128 | `/*` |
|       - | 4129 | ` * string strrev(string $string)` |
|       - | 4130 | ` *  Reverse a string.` |
|       - | 4131 | ` * Parameters` |
|       - | 4132 | ` *  $string` |
|       - | 4133 | ` *   String to be reversed.` |
|       - | 4134 | ` * Return` |
|       - | 4135 | ` *  The reversed string.` |
|       - | 4136 | ` */` |
|      42 | 4137 | `PH7_PRIVATE int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4138 | `{` |
|       - | 4139 | `	const char *zIn,*zEnd;` |
|       - | 4140 | `	int nLen,c;` |
|      44 | 4141 | `	if( nArg < 1 ){` |
|       - | 4142 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 4143 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4144 | `		return PH7_OK;` |
|       - | 4145 | `	}` |
|       - | 4146 | `	/* Extract the target string */` |
|      44 | 4147 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      44 | 4148 | `	if( nLen < 1 ){` |
|       - | 4149 | `		/* php answers the empty STRING here, not null */` |
|       3 | 4150 | `		ph7_result_string(pCtx,"",0);` |
|       3 | 4151 | `		return PH7_OK;` |
|       - | 4152 | `	}` |
|       - | 4153 | `	/* Perform the requested operation */` |
|      42 | 4154 | `	zEnd = &zIn[nLen - 1];` |
|      67 | 4155 | `	for(;;){` |
|     136 | 4156 | `		if( zEnd < zIn ){` |
|       - | 4157 | `			/* No more input to process */` |
|      42 | 4158 | `			break;` |
|       - | 4159 | `		}` |
|       - | 4160 | `		/* Append current character */` |
|      96 | 4161 | `		c = zEnd[0];` |
|      96 | 4162 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      96 | 4163 | `		zEnd--;` |
|       2 | 4164 | `	}` |
|      42 | 4165 | `	return PH7_OK;` |
|      23 | 4166 | `}` |
|       - | 4167 | `/*` |
|       - | 4168 | ` * string ucwords(string $string [, string $separators = " \t\r\n\f\v"])` |
|       - | 4169 | ` *  Uppercase the first character of each word in a string.` |
|       - | 4170 | ` *  A word begins at the start of the string and after any character present in` |
|       - | 4171 | ` *  $separators. The default separators are the whitespace characters (space,` |
|       - | 4172 | ` *  horizontal tab, carriage return, newline, form-feed and vertical tab); an` |
|       - | 4173 | ` *  explicit $separators argument REPLACES them (an empty string leaves only the` |
|       - | 4174 | ` *  very first character upper-cased). Like PHP, this is byte-based: only ASCII` |
|       - | 4175 | ` *  bytes are upper-cased and a byte is a separator only if it appears in the set.` |
|       - | 4176 | ` * Parameters` |
|       - | 4177 | ` *  $string` |
|       - | 4178 | ` *   The input string.` |
|       - | 4179 | ` *  $separators` |
|       - | 4180 | ` *   The optional word-boundary characters.` |
|       - | 4181 | ` * Return` |
|       - | 4182 | ` *  The modified string.` |
|       - | 4183 | ` */` |
|      26 | 4184 | `PH7_PRIVATE int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4185 | `{` |
|       - | 4186 | `	const char *zIn;` |
|       - | 4187 | `	int nLen,i,iStart;` |
|       - | 4188 | `	char aDelim[256];` |
|      28 | 4189 | `	if( nArg < 1 ){` |
|       - | 4190 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 4191 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4192 | `		return PH7_OK;` |
|       - | 4193 | `	}` |
|       - | 4194 | `	/* Build the separator membership table: an explicit $separators argument` |
|       - | 4195 | `	 * replaces the default whitespace set (an empty string clears it). */` |
|      28 | 4196 | `	SyZero(aDelim,(sxu32)sizeof(aDelim));` |
|      28 | 4197 | `	if( nArg > 1 ){` |
|       - | 4198 | `		int nDelim;` |
|       9 | 4199 | `		const char *zDelim = ph7_value_to_string(apArg[1],&nDelim);` |
|      17 | 4200 | `		for( i = 0 ; i < nDelim ; i++ ){` |
|       9 | 4201 | `			aDelim[(unsigned char)zDelim[i]] = 1;` |
|       5 | 4202 | `		}` |
|       5 | 4203 | `	}else{` |
|      20 | 4204 | `		aDelim[(unsigned char)' ']  = 1;` |
|      20 | 4205 | `		aDelim[(unsigned char)'\t'] = 1;` |
|      20 | 4206 | `		aDelim[(unsigned char)'\r'] = 1;` |
|      20 | 4207 | `		aDelim[(unsigned char)'\n'] = 1;` |
|      20 | 4208 | `		aDelim[(unsigned char)'\f'] = 1;` |
|      20 | 4209 | `		aDelim[(unsigned char)'\v'] = 1;` |
|       - | 4210 | `	}` |
|       - | 4211 | `	/* Extract the target string */` |
|      28 | 4212 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      28 | 4213 | `	if( nLen < 1 ){` |
|       - | 4214 | `		/* Empty string – match PHP semantics */` |
|       6 | 4215 | `		ph7_result_string(pCtx,"",0);` |
|       6 | 4216 | `		return PH7_OK;` |
|       - | 4217 | `	}` |
|       - | 4218 | `	/* Upper-case the first byte of each word (the leading byte, or any byte that` |
|       - | 4219 | `	 * follows a separator), appending the untouched runs in between verbatim. */` |
|      23 | 4220 | `	iStart = 0;` |
|     325 | 4221 | `	for( i = 0 ; i < nLen ; i++ ){` |
|     303 | 4222 | `		int c = (unsigned char)zIn[i];` |
|     303 | 4223 | `		if( (i == 0 \|\| aDelim[(unsigned char)zIn[i-1]]) && c < 0x80 && SyisLower(c) ){` |
|      55 | 4224 | `			char up = (char)SyToUpper(c);` |
|      55 | 4225 | `			if( i > iStart ){` |
|      37 | 4226 | `				ph7_result_string(pCtx,&zIn[iStart],i - iStart);` |
|      18 | 4227 | `			}` |
|      55 | 4228 | `			ph7_result_string(pCtx,&up,1);` |
|      55 | 4229 | `			iStart = i + 1;` |
|      27 | 4230 | `		}` |
|     152 | 4231 | `	}` |
|      23 | 4232 | `	if( nLen > iStart ){` |
|      23 | 4233 | `		ph7_result_string(pCtx,&zIn[iStart],nLen - iStart);` |
|      11 | 4234 | `	}` |
|      23 | 4235 | `	return PH7_OK;` |
|      15 | 4236 | `}` |
|       - | 4237 | `/*` |
|       - | 4238 | ` * string str_repeat(string $input,int $multiplier)` |
|       - | 4239 | ` *  Returns input repeated multiplier times.` |
|       - | 4240 | ` * Parameters` |
|       - | 4241 | ` *  $string` |
|       - | 4242 | ` *   String to be repeated.` |
|       - | 4243 | ` * $multiplier` |
|       - | 4244 | ` *  Number of time the input string should be repeated.` |
|       - | 4245 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|       - | 4246 | ` *  to 0, the function will return an empty string.` |
|       - | 4247 | ` * Return` |
|       - | 4248 | ` *  The repeated string.` |
|       - | 4249 | ` */` |
|   22282 | 4250 | `PH7_PRIVATE int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4251 | `{` |
|       - | 4252 | `	const char *zIn;` |
|       - | 4253 | `	int nLen;` |
|       - | 4254 | `	ph7_int64 nMul;` |
|       - | 4255 | `	int rc;` |
|   22287 | 4256 | `	if( nArg < 2 ){` |
|       - | 4257 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 4258 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4259 | `		return PH7_OK;` |
|       - | 4260 | `	}` |
|       - | 4261 | `	/* Extract the target string */` |
|   22287 | 4262 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|       - | 4263 | `	/* Resolve $times through the shared ZPP helper so a lossy float / float-string` |
|       - | 4264 | `	 * carries php's precision deprecation and NAN/INF/non-numeric fail with php's` |
|       - | 4265 | `	 * TypeError — a bare ph7_value_to_int64() coerced them silently. */` |
|       - | 4266 | `	{` |
|   22287 | 4267 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_repeat",2,"$times","int",&nMul);` |
|   22287 | 4268 | `		if( rcArg != PH7_OK ){` |
|     ! 0 | 4269 | `			return rcArg;` |
|       - | 4270 | `		}` |
|       - | 4271 | `	}` |
|   22287 | 4272 | `	if( nMul < 0 ){` |
|      20 | 4273 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 4274 | `			"str_repeat(): Argument #2 ($times) must be greater than or equal to 0");` |
|       - | 4275 | `	}` |
|   22271 | 4276 | `	if( nLen < 1 \|\| nMul < 1 ){` |
|       - | 4277 | `		/* Empty input or a zero multiplier yields the empty string (PHP). */` |
|      96 | 4278 | `		ph7_result_string(pCtx,"",0);` |
|      96 | 4279 | `		return PH7_OK;` |
|       - | 4280 | `	}` |
|       - | 4281 | `	/* Perform the requested operation */` |
|  990579 | 4282 | `	for(;;){` |
| 1999605 | 4283 | `		if( !nMul ){` |
|   22177 | 4284 | `			break;` |
|       - | 4285 | `		}` |
|       - | 4286 | `		/* Append the copy */` |
| 1977433 | 4287 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 1977433 | 4288 | `		if( rc != PH7_OK ){` |
|       - | 4289 | `			/* Allocation failed: surface a fatal instead of returning a` |
|       - | 4290 | `			 * silently-truncated string with a success status. */` |
|     ! 0 | 4291 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 4292 | `		}` |
| 1977433 | 4293 | `		nMul--;` |
|       5 | 4294 | `	}` |
|   22177 | 4295 | `	return PH7_OK;` |
|   11143 | 4296 | `}` |
|       - | 4297 | `/*` |
|       - | 4298 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|       - | 4299 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|       - | 4300 | ` * Parameters` |
|       - | 4301 | ` *  $string` |
|       - | 4302 | ` *   The input string.` |
|       - | 4303 | ` * $is_xhtml` |
|       - | 4304 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|       - | 4305 | ` * Return` |
|       - | 4306 | ` *  The processed string.` |
|       - | 4307 | ` */` |
|       8 | 4308 | `PH7_PRIVATE int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4309 | `{` |
|       - | 4310 | `	const char *zIn,*zCur,*zEnd;` |
|      10 | 4311 | `	int is_xhtml = 1; /* Default to XHTML-style '<br />' like PHP */` |
|       - | 4312 | `	int nLen;` |
|      10 | 4313 | `	if( nArg < 1 ){` |
|       - | 4314 | `		/* Missing arguments,return the empty string */` |
|     ! 0 | 4315 | `		ph7_result_string(pCtx,"",0);` |
|     ! 0 | 4316 | `		return PH7_OK;` |
|       - | 4317 | `	}` |
|       - | 4318 | `	/* Extract the target string */` |
|      10 | 4319 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      10 | 4320 | `	if( nLen < 1 ){` |
|       - | 4321 | `		/* php answers the empty STRING here, not null */` |
|       3 | 4322 | `		ph7_result_string(pCtx,"",0);` |
|       3 | 4323 | `		return PH7_OK;` |
|       - | 4324 | `	}` |
|       8 | 4325 | `	if( nArg > 1 ){` |
|       3 | 4326 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|       1 | 4327 | `	}` |
|       8 | 4328 | `	zEnd = &zIn[nLen];` |
|       - | 4329 | `	/* Perform the requested operation */` |
|       6 | 4330 | `	for(;;){` |
|      14 | 4331 | `		zCur = zIn;` |
|       - | 4332 | `		/* Delimit the string */` |
|      32 | 4333 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      14 | 4334 | `			zIn++;` |
|       2 | 4335 | `		}` |
|      14 | 4336 | `		if( zCur < zIn ){` |
|       - | 4337 | `			/* Output chunk verbatim */` |
|      14 | 4338 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|       6 | 4339 | `		}` |
|      14 | 4340 | `		if( zIn >= zEnd ){` |
|       - | 4341 | `			/* No more input to process */` |
|       8 | 4342 | `			break;` |
|       - | 4343 | `		}` |
|       - | 4344 | `		/* Output the HTML line break */` |
|       - | 4345 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br />' (space before the slash), otherwise use '<br>' */` |
|       8 | 4346 | `		if( is_xhtml ){` |
|       6 | 4347 | `			ph7_result_string(pCtx,"<br />",(int)sizeof("<br />")-1);` |
|       4 | 4348 | `		}else{` |
|       3 | 4349 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|       - | 4350 | `		}` |
|       8 | 4351 | `		zCur = zIn;` |
|       - | 4352 | `		/* Append trailing line */` |
|      17 | 4353 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|       8 | 4354 | `			zIn++;` |
|       2 | 4355 | `		}` |
|       8 | 4356 | `		if( zCur < zIn ){` |
|       - | 4357 | `			/* Output chunk verbatim */` |
|       8 | 4358 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|       3 | 4359 | `		}` |
|       2 | 4360 | `	}` |
|       8 | 4361 | `	return PH7_OK;` |
|       6 | 4362 | `}` |
|       - | 4363 | `/*` |
|       - | 4364 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|       - | 4365 | ` *  According to the PHP reference manual.` |
|       - | 4366 | ` * The format string is composed of zero or more directives: ordinary characters` |
|       - | 4367 | ` * (excluding %) that are copied directly to the result, and conversion` |
|       - | 4368 | ` * specifications, each of which results in fetching its own parameter.` |
|       - | 4369 | ` * This applies to both sprintf() and printf().` |
|       - | 4370 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|       - | 4371 | ` * or more of these elements, in order:` |
|       - | 4372 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|       - | 4373 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|       - | 4374 | ` *   positive numbers to have the + sign attached as well.` |
|       - | 4375 | ` *   An optional padding specifier that says what character will be used for padding` |
|       - | 4376 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|       - | 4377 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|       - | 4378 | ` *   it with a single quote ('). See the examples below.` |
|       - | 4379 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|       - | 4380 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|       - | 4381 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|       - | 4382 | ` *   should result in.` |
|       - | 4383 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|       - | 4384 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|       - | 4385 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|       - | 4386 | ` *   limit to the string.` |
|       - | 4387 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|       - | 4388 | ` *       % - a literal percent character. No argument is required.` |
|       - | 4389 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|       - | 4390 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|       - | 4391 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|       - | 4392 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|       - | 4393 | ` * 	     for the number of digits after the decimal point.` |
|       - | 4394 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|       - | 4395 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|       - | 4396 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|       - | 4397 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|       - | 4398 | ` *       g - shorter of %e and %f.` |
|       - | 4399 | ` *       G - shorter of %E and %f.` |
|       - | 4400 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|       - | 4401 | ` *       s - the argument is treated as and presented as a string.` |
|       - | 4402 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|       - | 4403 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|       - | 4404 | ` */` |
|       - | 4405 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|       - | 4406 | `#ifdef PH7_NEED_BUILTIN_REG` |
|       - | 4407 | `/*` |
|       - | 4408 | ` * Symisc eXtension.` |
|       - | 4409 | ` * string size_format(int64 $size)` |
|       - | 4410 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|       - | 4411 | ` *  Example:` |
|       - | 4412 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|       - | 4413 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|       - | 4414 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|       - | 4415 | ` * Parameter` |
|       - | 4416 | ` *  $size` |
|       - | 4417 | ` *    Entity size in bytes.` |
|       - | 4418 | ` * Return` |
|       - | 4419 | ` *   Formatted string representation of the given size.` |
|       - | 4420 | ` */` |
|      24 | 4421 | `PH7_PRIVATE int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4422 | `{` |
|       - | 4423 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|       - | 4424 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|       - | 4425 | `	sxi32 nRest,i_32;` |
|       - | 4426 | `	ph7_int64 iSize;` |
|      25 | 4427 | `	int c = -1; /* index in zUnit[] */` |
|       - | 4428 |  |
|      25 | 4429 | `	if( nArg < 1 ){` |
|       - | 4430 | `		/* Missing argument,return the empty string */` |
|       3 | 4431 | `		ph7_result_string(pCtx,"",0);` |
|       3 | 4432 | `		return PH7_OK;` |
|       - | 4433 | `	}` |
|       - | 4434 | `	/* Extract the given size */` |
|      23 | 4435 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|      23 | 4436 | `	if( iSize < 100 /* Bytes */ ){` |
|       - | 4437 | `		/* Don't bother formatting,return immediately */` |
|       5 | 4438 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|       5 | 4439 | `		return PH7_OK;` |
|       - | 4440 | `	}` |
|      19 | 4441 | `	for(;;){` |
|      39 | 4442 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|      39 | 4443 | `		iSize >>= 10;` |
|      39 | 4444 | `		c++;` |
|      39 | 4445 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|      19 | 4446 | `			break;` |
|       - | 4447 | `		}` |
|       1 | 4448 | `	}` |
|      19 | 4449 | `	nRest /= 100;` |
|      19 | 4450 | `	if( nRest > 9 ){` |
|     ! 0 | 4451 | `		nRest = 9;` |
|     ! 0 | 4452 | `	}` |
|      19 | 4453 | `	if( iSize > 999 ){` |
|     ! 0 | 4454 | `		c++;` |
|     ! 0 | 4455 | `		nRest = 9;` |
|     ! 0 | 4456 | `		iSize = 0;` |
|     ! 0 | 4457 | `	}` |
|      19 | 4458 | `	i_32 = (sxi32)iSize;` |
|       - | 4459 | `	/* Format */` |
|      19 | 4460 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|      19 | 4461 | `	return PH7_OK;` |
|      13 | 4462 | `}` |
|       - | 4463 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|       - | 4464 | `#ifdef PH7_NEED_BUILTIN_REG` |
|       - | 4465 | `/*` |
|       - | 4466 | ` * string str_shuffle(string $str)` |
|       - | 4467 |  |
|       - | 4468 | ` *  Randomly shuffles a string.` |
|       - | 4469 | ` * Parameters` |
|       - | 4470 | ` *  $str` |
|       - | 4471 | ` *   The input string.` |
|       - | 4472 | ` * Return` |
|       - | 4473 | ` *  Returns the shuffled string.` |
|       - | 4474 | ` */` |
|      20 | 4475 | `PH7_PRIVATE int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4476 | `{` |
|       - | 4477 | `	const char *zString;` |
|       - | 4478 | `	int nLen,i,c;` |
|       - | 4479 | `	sxu32 iR;` |
|      22 | 4480 | `	if( nArg < 1 ){` |
|       - | 4481 | `		/* Missing arguments,return the empty string */` |
|     ! 0 | 4482 | `		ph7_result_string(pCtx,"",0);` |
|     ! 0 | 4483 | `		return PH7_OK;` |
|       - | 4484 | `	}` |
|       - | 4485 | `	/* Extract the target string */` |
|      22 | 4486 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      22 | 4487 | `	if( nLen < 1 ){` |
|       - | 4488 | `		/* Nothing to shuffle */` |
|       6 | 4489 | `		ph7_result_string(pCtx,"",0);` |
|       6 | 4490 | `		return PH7_OK;` |
|       - | 4491 | `	}` |
|       - | 4492 | `	/* php's Fisher-Yates, drawn from the same generator in the same order, so a` |
|       - | 4493 | `	 * seeded shuffle answers php's string.` |
|       - | 4494 | `	 *` |
|       - | 4495 | `	 * What was here picked each output byte independently — WITH replacement — so` |
|       - | 4496 | `` 	 * the answer was not a permutation of the input at all: `str_shuffle($alpha)` `` |
|       - | 4497 | `	 * came back with letters repeated and letters missing, which is the one thing` |
|       - | 4498 | `	 * the documented "randomly shuffles a string" cannot do. The idiom it breaks` |
|       - | 4499 | `	 * is the common one: shuffling an alphabet and slicing a token out of it. */` |
|       - | 4500 | `	{` |
|      18 | 4501 | `		char *zOut = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)nLen);` |
|      18 | 4502 | `		if( zOut == 0 ){` |
|     ! 0 | 4503 | `			return PH7_VmMemoryError(pCtx->pVm);` |
|       - | 4504 | `		}` |
|      18 | 4505 | `		SyMemcpy(zString,zOut,(sxu32)nLen);` |
|     254 | 4506 | `		for( i = nLen - 1 ; i > 0 ; --i ){` |
|     238 | 4507 | `			iR = (sxu32)PH7_VmMtRandRange(pCtx->pVm,0,(sxi64)i);` |
|     238 | 4508 | `			if( (int)iR != i ){` |
|     204 | 4509 | `				c = zOut[i];` |
|     204 | 4510 | `				zOut[i] = zOut[iR];` |
|     204 | 4511 | `				zOut[iR] = (char)c;` |
|     101 | 4512 | `			}` |
|     120 | 4513 | `		}` |
|      18 | 4514 | `		ph7_result_string(pCtx,zOut,nLen);` |
|      18 | 4515 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);` |
|       - | 4516 | `	}` |
|      18 | 4517 | `	return PH7_OK;` |
|      12 | 4518 | `}` |
|       - | 4519 | `/*` |
|       - | 4520 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|       - | 4521 | ` *  Convert a string to an array.` |
|       - | 4522 | ` * Parameters` |
|       - | 4523 | ` * $string` |
|       - | 4524 | ` *  The input string.` |
|       - | 4525 | ` * $split_length` |
|       - | 4526 | ` *  Maximum length of the chunk.` |
|       - | 4527 | ` * Return` |
|       - | 4528 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|       - | 4529 | ` *  except possibly the last one which may be shorter.` |
|       - | 4530 | ` *  If split_length exceeds the string length, the entire string is returned` |
|       - | 4531 | ` *  as the first (and only) array element.` |
|       - | 4532 | ` *  An empty string returns an empty array.` |
|       - | 4533 | ` * Errors` |
|       - | 4534 | ` *  ArgumentCountError if no arguments are given.` |
|       - | 4535 | ` *  TypeError if $string is an array, object or resource.` |
|       - | 4536 | ` *  ValueError if $split_length is less than 1.` |
|       - | 4537 | ` */` |
|      42 | 4538 | `PH7_PRIVATE int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 4539 | `{` |
|       - | 4540 | `	char zGiven[64];` |
|       - | 4541 | `	const char *zString,*zEnd;` |
|       - | 4542 | `	ph7_value *pArray,*pValue;` |
|       - | 4543 | `	int split_len;` |
|       - | 4544 | `	int nLen;` |
|      46 | 4545 | `	if( nArg < 1 ){` |
|     ! 0 | 4546 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4547 | `			"ArgumentCountError",` |
|       - | 4548 | `			"str_split() expects at least 1 argument, %d given",` |
|     ! 0 | 4549 | `			nArg` |
|       - | 4550 | `			);` |
|       - | 4551 | `	}` |
|       - | 4552 | `	/* Arrays, resources and objects with no __toString() raise a TypeError like PHP */` |
|      63 | 4553 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|      67 | 4554 | `	    ph7_value_is_resource(apArg[0]) \|\|` |
|      42 | 4555 | `	    PH7_ArgIsUnstringableObject(apArg[0]) ){` |
|     ! 0 | 4556 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4557 | `			"TypeError",` |
|       - | 4558 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|     ! 0 | 4559 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven))` |
|       - | 4560 | `			);` |
|       - | 4561 | `	}` |
|       - | 4562 | `	/* Point to the target string */` |
|      46 | 4563 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      46 | 4564 | `	split_len = (int)sizeof(char);` |
|      46 | 4565 | `	if( nArg > 1 ){` |
|       - | 4566 | `		/* Split length */` |
|      23 | 4567 | `		split_len = ph7_value_to_int(apArg[1]);` |
|      23 | 4568 | `		if( split_len < 1 ){` |
|       6 | 4569 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4570 | `				"ValueError",` |
|       - | 4571 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|       - | 4572 | `				);` |
|       - | 4573 | `		}` |
|      17 | 4574 | `		if( split_len > nLen && nLen > 0 ){` |
|       3 | 4575 | `			split_len = nLen;` |
|       1 | 4576 | `		}` |
|       8 | 4577 | `	}` |
|       - | 4578 | `	/* Create the array and the scalar value */` |
|      40 | 4579 | `	pArray = ph7_context_new_array(pCtx);` |
|       - | 4580 | `	/*Chunk value */` |
|      40 | 4581 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      40 | 4582 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|       - | 4583 | `		/* Return FALSE */` |
|     ! 0 | 4584 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4585 | `		return PH7_OK;` |
|       - | 4586 | `	}` |
|       - | 4587 | `	/* Point to the end of the string */` |
|      40 | 4588 | `	zEnd = &zString[nLen];` |
|       - | 4589 | `	/* Perform the requested operation */` |
|     243 | 4590 | `	for(;;){` |
|       - | 4591 | `		int nMax;` |
|     264 | 4592 | `		if( zString >= zEnd ){` |
|       - | 4593 | `			/* No more input to process */` |
|      40 | 4594 | `			break;` |
|       - | 4595 | `		}` |
|     226 | 4596 | `		nMax = (int)(zEnd-zString);` |
|     226 | 4597 | `		if( nMax < split_len ){` |
|       5 | 4598 | `			split_len = nMax;` |
|       2 | 4599 | `		}` |
|       - | 4600 | `		/* Copy the current chunk */` |
|     226 | 4601 | `		ph7_value_string(pValue,zString,split_len);` |
|       - | 4602 | `		/* Insert it */` |
|     226 | 4603 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|     ! 0 | 4604 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 4605 | `		}` |
|       - | 4606 | `		/* reset the string cursor */` |
|     226 | 4607 | `		ph7_value_reset_string_cursor(pValue);` |
|       - | 4608 | `		/* Update position */` |
|     226 | 4609 | `		zString += split_len;` |
|       2 | 4610 | `	}` |
|       - | 4611 | `	/*` |
|       - | 4612 | `	 * Return the array.` |
|       - | 4613 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|       - | 4614 | `	 * upon we return from this function.` |
|       - | 4615 | `	 */` |
|      40 | 4616 | `	ph7_result_value(pCtx,pArray);` |
|      40 | 4617 | `	return PH7_OK;` |
|      25 | 4618 | `}` |
|       - | 4619 | `/*` |
|       - | 4620 | ` * array\|string count_chars(string $string[,int $mode = 0 ])` |
|       - | 4621 | ` *  How many times each byte value occurs in a string.` |
|       - | 4622 | ` * Parameters` |
|       - | 4623 | ` *  $string` |
|       - | 4624 | ` *   The examined string.` |
|       - | 4625 | ` *  $mode` |
|       - | 4626 | ` *   0: an array of all 256 byte values -> frequency.` |
|       - | 4627 | ` *   1: only the byte values that occur.` |
|       - | 4628 | ` *   2: only the byte values that do not.` |
|       - | 4629 | ` *   3: a string of the byte values that occur.` |
|       - | 4630 | ` *   4: a string of the byte values that do not.` |
|       - | 4631 | ` * Return` |
|       - | 4632 | ` *  An array for modes 0-2, a string for modes 3-4.` |
|       - | 4633 | ` */` |
|      86 | 4634 | `PH7_PRIVATE int PH7_builtin_count_chars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4635 | `{` |
|       - | 4636 | `	sxu32 aCount[256];` |
|       - | 4637 | `	const unsigned char *zIn;` |
|       - | 4638 | `	ph7_value *pArray,*pValue;` |
|       - | 4639 | `	char zOut[256];` |
|      88 | 4640 | `	int nOut = 0;` |
|      88 | 4641 | `	int nLen = 0;` |
|      88 | 4642 | `	int iMode = 0;` |
|       - | 4643 | `	int i;` |
|      88 | 4644 | `	if( nArg < 1 ){` |
|       - | 4645 | `		/* Arity is enforced from aBuiltinSig[] before the call. */` |
|     ! 0 | 4646 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4647 | `		return PH7_OK;` |
|       - | 4648 | `	}` |
|      88 | 4649 | `	if( nArg > 1 ){` |
|       - | 4650 | ``		/* php declares `int $mode`; the shared screen refuses the values no`` |
|       - | 4651 | `		 * coercion can reach (array, object, resource, null), leaving the string` |
|       - | 4652 | `		 * and float narrowing to the builtin -- both of which php only DEPRECATES` |
|       - | 4653 | `		 * and PHL rejects (the scope policy). */` |
|      88 | 4654 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       - | 4655 | `			/* php wants the WHOLE string to be numeric (surrounding whitespace` |
|       - | 4656 | `			 * aside): "2abc" and "0x2" are TypeErrors, not 2. A float-shaped one` |
|       - | 4657 | `			 * that would LOSE something is the scope policy's refusal of a deprecation. */` |
|       - | 4658 | `			double d;` |
|       7 | 4659 | `			if( !PH7_MemObjStringIsNumeric(apArg[1]) ){` |
|     ! 0 | 4660 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 4661 | `					"count_chars(): Argument #2 ($mode) must be of type int, string given");` |
|       - | 4662 | `			}` |
|       7 | 4663 | `			d = ph7_value_to_double(apArg[1]);` |
|       7 | 4664 | `			if( !PH7_RealFitsInt64(d) \|\| d != (double)(sxi64)d ){` |
|     ! 0 | 4665 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 4666 | `					"count_chars(): Argument #2 ($mode) must be of type int, string given");` |
|       1 | 4667 | `			}` |
|      85 | 4668 | `		}else if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 4669 | `			double d = ph7_value_to_double(apArg[1]);` |
|       - | 4670 | `			/* Range first: see chr() above -- an out-of-range cast is undefined. */` |
|       3 | 4671 | `			if( !PH7_RealFitsInt64(d) \|\| d != (double)(sxi64)d ){` |
|     ! 0 | 4672 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 4673 | `					"count_chars(): Argument #2 ($mode) must be of type int, float given");` |
|       - | 4674 | `			}` |
|       1 | 4675 | `		}` |
|      88 | 4676 | `		iMode = ph7_value_to_int(apArg[1]);` |
|      88 | 4677 | `		if( iMode < 0 \|\| iMode > 4 ){` |
|       7 | 4678 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 4679 | `				"count_chars(): Argument #2 ($mode) must be between 0 and 4 (inclusive)");` |
|       - | 4680 | `		}` |
|      40 | 4681 | `	}` |
|       - | 4682 | `	/* Binary safe: the string is counted by LENGTH, so an embedded NUL is a byte` |
|       - | 4683 | `	 * value like any other. */` |
|      82 | 4684 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   20562 | 4685 | `	for( i = 0 ; i < 256 ; ++i ){` |
|   20482 | 4686 | `		aCount[i] = 0;` |
|   10242 | 4687 | `	}` |
|     724 | 4688 | `	for( i = 0 ; i < nLen ; ++i ){` |
|     644 | 4689 | `		aCount[zIn[i]]++;` |
|     323 | 4690 | `	}` |
|      82 | 4691 | `	if( iMode >= 3 ){` |
|       - | 4692 | `		/* 3 = the bytes that occur, 4 = the bytes that do not. */` |
|    6684 | 4693 | `		for( i = 0 ; i < 256 ; ++i ){` |
|    6658 | 4694 | `			if( (aCount[i] != 0) == (iMode == 3) ){` |
|    1568 | 4695 | `				zOut[nOut++] = (char)i;` |
|     783 | 4696 | `			}` |
|    3330 | 4697 | `		}` |
|      28 | 4698 | `		ph7_result_string(pCtx,zOut,nOut);` |
|      28 | 4699 | `		return PH7_OK;` |
|       - | 4700 | `	}` |
|      56 | 4701 | `	pArray = ph7_context_new_array(pCtx);` |
|      56 | 4702 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      56 | 4703 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|     ! 0 | 4704 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 4705 | `	}` |
|   13880 | 4706 | `	for( i = 0 ; i < 256 ; ++i ){` |
|       - | 4707 | `		/* 0 = every byte value, 1 = the ones that occur, 2 = the ones that do not` |
|       - | 4708 | `		 * (php stores their count, which is always 0). */` |
|   13826 | 4709 | `		if( iMode != 0 && (aCount[i] != 0) != (iMode == 1) ){` |
|   10270 | 4710 | `			continue;` |
|       - | 4711 | `		}` |
|    3558 | 4712 | `		ph7_value_int64(pValue,(ph7_int64)aCount[i]);` |
|    3558 | 4713 | `		if( ph7_array_add_intkey_elem(pArray,i,pValue) != SXRET_OK ){` |
|     ! 0 | 4714 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 4715 | `		}` |
|    1780 | 4716 | `	}` |
|      56 | 4717 | `	ph7_result_value(pCtx,pArray);` |
|      56 | 4718 | `	return PH7_OK;` |
|      45 | 4719 | `}` |
|       - | 4720 | `/*` |
|       - | 4721 | ` * Check if the given string contains only characters from the given mask.` |
|       - | 4722 | ` * return the longest match.` |
|       - | 4723 | ` * Refer to [strspn()].` |
|       - | 4724 | ` */` |
|      66 | 4725 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|       1 | 4726 | `{` |
|      67 | 4727 | `	const char *zEnd = &zString[nLen];` |
|      67 | 4728 | `	const char *zIn = zString;` |
|       - | 4729 | `	int i,c;` |
|     110 | 4730 | `	for(;;){` |
|     221 | 4731 | `		if( zString >= zEnd ){` |
|      45 | 4732 | `			break;` |
|       - | 4733 | `		}` |
|       - | 4734 | `		/* Extract current character */` |
|     177 | 4735 | `		c = zString[0];` |
|       - | 4736 | `		/* Perform the lookup */` |
|     589 | 4737 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     567 | 4738 | `			if( c == zMask[i] ){` |
|       - | 4739 | `				/* Character found */` |
|     155 | 4740 | `				break;` |
|       - | 4741 | `			}` |
|     207 | 4742 | `		}` |
|     177 | 4743 | `		if( i >= nMaskLen ){` |
|       - | 4744 | `			/* Character not in the current mask,break immediately */` |
|      23 | 4745 | `			break;` |
|       - | 4746 | `		}` |
|       - | 4747 | `		/* Advance cursor */` |
|     155 | 4748 | `		zString++;` |
|       1 | 4749 | `	}` |
|       - | 4750 | `	/* Longest match */` |
|      67 | 4751 | `	return (int)(zString-zIn);` |
|       1 | 4752 | `}` |
|       - | 4753 | `/*` |
|       - | 4754 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|       - | 4755 | ` * Refer to [strcspn()].` |
|       - | 4756 | ` */` |
|     455 | 4757 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|       5 | 4758 | `{` |
|     460 | 4759 | `	const char *zEnd = &zString[nLen];` |
|     460 | 4760 | `	const char *zIn = zString;` |
|       - | 4761 | `	int i,c;` |
|   11863 | 4762 | `	for(;;){` |
|   15919 | 4763 | `		if( zString >= zEnd ){` |
|     410 | 4764 | `			break;` |
|       - | 4765 | `		}` |
|       - | 4766 | `		/* Extract current character */` |
|   15514 | 4767 | `		c = zString[0];` |
|       - | 4768 | `		/* Perform the lookup */` |
|   61653 | 4769 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|   46194 | 4770 | `			if( c == zMask[i] ){` |
|      51 | 4771 | `				break;` |
|       - | 4772 | `			}` |
|   34794 | 4773 | `		}` |
|   15514 | 4774 | `		if( i < nMaskLen ){` |
|       - | 4775 | `			/* Character in the current mask,break immediately */` |
|      51 | 4776 | `			break;` |
|       - | 4777 | `		}` |
|       - | 4778 | `		/* Advance cursor */` |
|   15464 | 4779 | `		zString++;` |
|       5 | 4780 | `	}` |
|       - | 4781 | `	/* Longest match */` |
|     460 | 4782 | `	return (int)(zString-zIn);` |
|       5 | 4783 | `}` |
|       - | 4784 | `/*` |
|       - | 4785 | ` * Shared body of strspn()/strcspn(): resolve php's ($offset,$length) window over` |
|       - | 4786 | ` * $string, then measure the span from the window's first byte.` |
|       - | 4787 | ` *` |
|       - | 4788 | ` * php's window rules (ext/standard/string.c, php_spn_common_handler) — a negative` |
|       - | 4789 | ` * $offset counts back from the end and CLAMPS to 0 (it is never "invalid"); an` |
|       - | 4790 | ` * $offset past the end clamps to the end, so the window is empty and the answer is` |
|       - | 4791 | ` * 0; a negative $length leaves that many bytes off the end of the remaining span` |
|       - | 4792 | ` * and clamps to 0; a zero-length window answers 0. PH7 answered 0 for a negative` |
|       - | 4793 | ` * offset that reached past the start, IGNORED a zero or negative $length entirely` |
|       - | 4794 | ` * (measuring the whole rest of the string instead), and truncated the offset to` |
|       - | 4795 | `` * `int`, so a 64-bit offset wrapped into a valid one.`` |
|       - | 4796 | ` *` |
|       - | 4797 | ` * PH7 also ran the scan over the first WHITESPACE-DELIMITED TOKEN rather than over` |
|       - | 4798 | ` * the raw window (leading spaces skipped, scan stopped at the next space), so` |
|       - | 4799 | ` * strspn("a b c","abc ") answered 1 where php answers 5 and strspn("  abc","abc")` |
|       - | 4800 | ` * answered 3 where php answers 0 — silent wrong answers on ordinary input. php` |
|       - | 4801 | ` * scans raw bytes; so does this.` |
|       - | 4802 | ` *` |
|       - | 4803 | ` * An empty $mask needs no special case: the mask lookup fails for every byte, so` |
|       - | 4804 | ` * strspn stops at once (0) and strcspn runs to the end of the window (its length),` |
|       - | 4805 | ` * which is exactly what php answers.` |
|       - | 4806 | ` */` |
|     521 | 4807 | `static int StrSpnCommonHandler(` |
|       - | 4808 | `	ph7_context *pCtx,    /* Call context */` |
|       - | 4809 | `	int nArg,             /* Argument count */` |
|       - | 4810 | `	ph7_value **apArg,    /* Arguments */` |
|       - | 4811 | `	int bComplement       /* TRUE for strcspn() */` |
|       - | 4812 | `	)` |
|       5 | 4813 | `{` |
|     526 | 4814 | `	const char *zFunc = bComplement ? "strcspn" : "strspn";` |
|       - | 4815 | `	const char *zString,*zMask;` |
|       - | 4816 | `	int iMasklen,iLen;` |
|       - | 4817 | `	sxi64 iStart,iSpan;` |
|     526 | 4818 | `	if( nArg < 2 ){` |
|       - | 4819 | `		/* Arity is enforced at the call boundary; nothing sensible to return here. */` |
|     ! 0 | 4820 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 4821 | `		return PH7_OK;` |
|       - | 4822 | `	}` |
|       - | 4823 | `	/* Extract the target string and the mask */` |
|     526 | 4824 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|     526 | 4825 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     526 | 4826 | `	if( iLen < 0 ){` |
|     ! 0 | 4827 | `		iLen = 0;` |
|     ! 0 | 4828 | `	}` |
|     526 | 4829 | `	if( iMasklen < 0 ){` |
|     ! 0 | 4830 | `		iMasklen = 0;` |
|     ! 0 | 4831 | `	}` |
|     526 | 4832 | `	iStart = 0;` |
|     526 | 4833 | `	if( nArg > 2 ){` |
|      73 | 4834 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],zFunc,3,"$offset","int",&iStart);` |
|      73 | 4835 | `		if( rcArg != PH7_OK ){` |
|     ! 0 | 4836 | `			return rcArg;` |
|       - | 4837 | `		}` |
|      73 | 4838 | `		if( iStart < 0 ){` |
|       - | 4839 | `			/* Count back from the end, clamped to the start (guarded so an` |
|       - | 4840 | `			 * INT64_MIN offset cannot overflow the addition). */` |
|      13 | 4841 | `			iStart = ( iStart < -(sxi64)iLen ) ? 0 : iStart + iLen;` |
|      67 | 4842 | `		}else if( iStart > (sxi64)iLen ){` |
|       9 | 4843 | `			iStart = iLen;` |
|       4 | 4844 | `		}` |
|      36 | 4845 | `	}` |
|     526 | 4846 | `	iSpan = (sxi64)iLen - iStart;` |
|     526 | 4847 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|      39 | 4848 | `		sxi64 iUserlen = 0;` |
|      39 | 4849 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[3],zFunc,4,"$length","?int",&iUserlen);` |
|      39 | 4850 | `		if( rcArg != PH7_OK ){` |
|     ! 0 | 4851 | `			return rcArg;` |
|       - | 4852 | `		}` |
|      39 | 4853 | `		if( iUserlen < 0 ){` |
|       - | 4854 | `			/* Leave \|$length\| bytes off the end of the remaining span (guarded` |
|       - | 4855 | `			 * against an INT64_MIN underflow the same way). */` |
|      21 | 4856 | `			iSpan = ( iUserlen < -iSpan ) ? 0 : iSpan + iUserlen;` |
|      29 | 4857 | `		}else if( iUserlen < iSpan ){` |
|      11 | 4858 | `			iSpan = iUserlen;` |
|       5 | 4859 | `		}` |
|      19 | 4860 | `	}` |
|     787 | 4861 | `	ph7_result_int(pCtx,bComplement` |
|     455 | 4862 | `		? LongestStringMask2(&zString[iStart],(int)iSpan,zMask,iMasklen)` |
|      66 | 4863 | `		: LongestStringMask(&zString[iStart],(int)iSpan,zMask,iMasklen));` |
|     526 | 4864 | `	return PH7_OK;` |
|     265 | 4865 | `}` |
|       - | 4866 | `/*` |
|       - | 4867 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|       - | 4868 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|       - | 4869 | ` *  of characters contained within a given mask.` |
|       - | 4870 | ` * Parameters` |
|       - | 4871 | ` * $str` |
|       - | 4872 | ` *  The input string.` |
|       - | 4873 | ` * $mask` |
|       - | 4874 | ` *  The list of allowable characters.` |
|       - | 4875 | ` * $start` |
|       - | 4876 | ` *  The position in subject to start searching.` |
|       - | 4877 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|       - | 4878 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|       - | 4879 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|       - | 4880 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|       - | 4881 | ` *  start'th position from the end of subject.` |
|       - | 4882 | ` * $length` |
|       - | 4883 | ` *  The length of the segment from subject to examine.` |
|       - | 4884 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|       - | 4885 | ` *  characters after the starting position.` |
|       - | 4886 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|       - | 4887 | ` *  position up to length characters from the end of subject.` |
|       - | 4888 | ` * Return` |
|       - | 4889 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|       - | 4890 | ` * in mask.` |
|       - | 4891 | ` */` |
|      66 | 4892 | `PH7_PRIVATE int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4893 | `{` |
|      67 | 4894 | `	return StrSpnCommonHandler(pCtx,nArg,apArg,0);` |
|       1 | 4895 | `}` |
|       - | 4896 | `/*` |
|       - | 4897 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|       - | 4898 | ` *  Find length of initial segment not matching mask.` |
|       - | 4899 | ` * Parameters` |
|       - | 4900 | ` * $str` |
|       - | 4901 | ` *  The input string.` |
|       - | 4902 | ` * $mask` |
|       - | 4903 | ` *  The list of not allowed characters.` |
|       - | 4904 | ` * $start` |
|       - | 4905 | ` *  The position in subject to start searching.` |
|       - | 4906 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|       - | 4907 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|       - | 4908 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|       - | 4909 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|       - | 4910 | ` *  start'th position from the end of subject.` |
|       - | 4911 | ` * $length` |
|       - | 4912 | ` *  The length of the segment from subject to examine.` |
|       - | 4913 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|       - | 4914 | ` *  characters after the starting position.` |
|       - | 4915 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|       - | 4916 | ` *  position up to length characters from the end of subject.` |
|       - | 4917 | ` * Return` |
|       - | 4918 | ` *  Returns the length of the segment as an integer.` |
|       - | 4919 | ` */` |
|     455 | 4920 | `PH7_PRIVATE int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4921 | `{` |
|     460 | 4922 | `	return StrSpnCommonHandler(pCtx,nArg,apArg,1);` |
|       5 | 4923 | `}` |
|       - | 4924 | `/*` |
|       - | 4925 | ` * string strpbrk(string $haystack,string $char_list)` |
|       - | 4926 | ` *  Search a string for any of a set of characters.` |
|       - | 4927 | ` * Parameters` |
|       - | 4928 | ` *  $haystack` |
|       - | 4929 | ` *   The string where char_list is looked for.` |
|       - | 4930 | ` *  $char_list` |
|       - | 4931 | ` *   This parameter is case sensitive.` |
|       - | 4932 | ` * Return` |
|       - | 4933 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|       - | 4934 | ` */` |
|      14 | 4935 | `PH7_PRIVATE int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4936 | `{` |
|       - | 4937 | `	const char *zString,*zList,*zEnd;` |
|       - | 4938 | `	int iLen,iListLen,i,c;` |
|       - | 4939 | `	sxu32 nOfft,nMax;` |
|       - | 4940 | `	sxi32 rc;` |
|      15 | 4941 | `	if( nArg < 2 ){` |
|       - | 4942 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 4943 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4944 | `		return PH7_OK;` |
|       - | 4945 | `	}` |
|       - | 4946 | `	/* Extract the haystack and the char list */` |
|      15 | 4947 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      15 | 4948 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      15 | 4949 | `	if( iListLen < 1 ){` |
|       - | 4950 | `		/* An empty set can never match, so php rejects it rather than answering` |
|       - | 4951 | `		 * a FALSE indistinguishable from "not found" (checked BEFORE the haystack,` |
|       - | 4952 | `		 * so strpbrk("","") throws too). */` |
|       5 | 4953 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 4954 | `			"strpbrk(): Argument #2 ($characters) must be a non-empty string");` |
|       - | 4955 | `	}` |
|      11 | 4956 | `	if( iLen < 1 ){` |
|       - | 4957 | `		/* Nothing to process,return FALSE */` |
|       3 | 4958 | `		ph7_result_bool(pCtx,0);` |
|       3 | 4959 | `		return PH7_OK;` |
|       - | 4960 | `	}` |
|       - | 4961 | `	/* Point to the end of the string */` |
|       9 | 4962 | `	zEnd = &zString[iLen];` |
|       9 | 4963 | `	nOfft = nMax = SXU32_HIGH;` |
|       - | 4964 | `	/* perform the requested operation */` |
|      25 | 4965 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|      17 | 4966 | `		c = zList[i];` |
|      17 | 4967 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|      17 | 4968 | `		if( rc == SXRET_OK ){` |
|       9 | 4969 | `			if( nMax < nOfft ){` |
|       5 | 4970 | `				nOfft = nMax;` |
|       2 | 4971 | `			}` |
|       4 | 4972 | `		}` |
|       9 | 4973 | `	}` |
|       9 | 4974 | `	if( nOfft == SXU32_HIGH ){` |
|       - | 4975 | `		/* No such substring,return FALSE */` |
|       5 | 4976 | `		ph7_result_bool(pCtx,0);` |
|       3 | 4977 | `	}else{` |
|       - | 4978 | `		/* Return the substring */` |
|       5 | 4979 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|       - | 4980 | `	}` |
|       9 | 4981 | `	return PH7_OK;` |
|       8 | 4982 | `}` |
|       - | 4983 | `/*` |
|       - | 4984 | ` * string soundex(string $str)` |
|       - | 4985 | ` *  Calculate the soundex key of a string.` |
|       - | 4986 | ` * Parameters` |
|       - | 4987 | ` *  $str` |
|       - | 4988 | ` *   The input string.` |
|       - | 4989 | ` * Return` |
|       - | 4990 | ` *  Returns the soundex key as a string.` |
|       - | 4991 | ` * Note:` |
|       - | 4992 | ` *  Knuth's algorithm as php implements it (ext/standard/soundex.c). The` |
|       - | 4993 | ` *  previous implementation came from SQLite and diverged from php on three` |
|       - | 4994 | ` *  counts, each of them a silent wrong answer:` |
|       - | 4995 | ` *` |
|       - | 4996 | ` *   - a NON-LETTER inside the word RESET the "same code in a row" state, so` |
|       - | 4997 | ` *     soundex("S s") answered S200 where php answers S000 and soundex("b1b")` |
|       - | 4998 | ` *     answered B100 where php answers B000. php simply skips anything that is` |
|       - | 4999 | ` *     not a letter; only a VOWEL separates two consonants sharing a code.` |
|       - | 5000 | ` *   - the scan stopped at the first byte >= 0xC0, taking every UTF-8 lead byte` |
|       - | 5001 | ` *     for a letter and copying it raw into the key: soundex("\xff\xfe") answered` |
|       - | 5002 | ` *     "\xff000" where php answers "0000", and a leading accent HID the letters` |
|       - | 5003 | ` *     behind it (soundex("éa") answered "\xc3000" for php's A000). Inside the` |
|       - | 5004 | `` *     loop the table was indexed by `byte & 0x7f`, which folds high bytes onto`` |
|       - | 5005 | ` *     ASCII letters and invents codes for them.` |
|       - | 5006 | ` *   - the input was walked as a NUL-terminated C string, so soundex("a\0b")` |
|       - | 5007 | ` *     stopped at the NUL (A000) where php walks the whole php string (A100).` |
|       - | 5008 | ` *` |
|       - | 5009 | ` *  Classification is ASCII-only, matching php's own A-Z table (the locale` |
|       - | 5010 | ` *  dependence family: the old code asked libc's isalpha() through SyisAlpha,` |
|       - | 5011 | ` *  which answers differently under a non-C LC_CTYPE).` |
|       - | 5012 | ` */` |
|      52 | 5013 | `PH7_PRIVATE int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5014 | `{` |
|       - | 5015 | `	/* Code per letter A-Z; 0 means "no code" (a vowel, plus H/W/Y) */` |
|       - | 5016 | `	static const char zCode[] = "01230120022455012623010202";` |
|       - | 5017 | `	const unsigned char *zIn;` |
|       - | 5018 | `	char zResult[4];` |
|      53 | 5019 | `	int nByte,i,nOut = 0,iLast = -1;` |
|      53 | 5020 | `	if( nArg < 1 ){` |
|       - | 5021 | `		/* Missing arguments,return the empty string */` |
|     ! 0 | 5022 | `		ph7_result_string(pCtx,"",0);` |
|     ! 0 | 5023 | `		return PH7_OK;` |
|       - | 5024 | `	}` |
|      53 | 5025 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nByte);` |
|     265 | 5026 | `	for( i = 0 ; i < nByte && nOut < 4 ; ++i ){` |
|     213 | 5027 | `		int c = zIn[i];` |
|       - | 5028 | `		int code;` |
|     213 | 5029 | `		if( c >= 'a' && c <= 'z' ){` |
|     139 | 5030 | `			c -= 'a' - 'A';` |
|      69 | 5031 | `		}` |
|     213 | 5032 | `		if( c < 'A' \|\| c > 'Z' ){` |
|      35 | 5033 | `			continue; /* not a letter: skipped outright, state untouched */` |
|       - | 5034 | `		}` |
|     179 | 5035 | `		code = zCode[c - 'A'] - '0';` |
|     179 | 5036 | `		if( nOut == 0 ){` |
|       - | 5037 | `			/* The key opens with the first letter itself */` |
|      43 | 5038 | `			zResult[nOut++] = (char)c;` |
|     158 | 5039 | `		}else if( code != iLast && code != 0 ){` |
|      69 | 5040 | `			zResult[nOut++] = (char)(code + '0');` |
|      34 | 5041 | `		}` |
|     179 | 5042 | `		iLast = code;` |
|      90 | 5043 | `	}` |
|       - | 5044 | `	/* Pad to four characters. A string with no letter at all pads from nothing,` |
|       - | 5045 | `	 * which is php's "0000" (an empty input included). */` |
|     151 | 5046 | `	while( nOut < 4 ){` |
|      99 | 5047 | `		zResult[nOut++] = '0';` |
|       1 | 5048 | `	}` |
|      53 | 5049 | `	ph7_result_string(pCtx,zResult,4);` |
|      53 | 5050 | `	return PH7_OK;` |
|      27 | 5051 | `}` |
|       - | 5052 | `/*` |
|       - | 5053 | ` * string str_rot13(string $string)` |
|       - | 5054 | ` *  Perform the ROT13 transform: each ASCII letter is rotated 13 places through` |
|       - | 5055 | ` *  its own alphabet, everything else (digits, punctuation, high bytes, NULs)` |
|       - | 5056 | ` *  passes through untouched. ROT13 is its own inverse.` |
|       - | 5057 | ` */` |
|      20 | 5058 | `PH7_PRIVATE int PH7_builtin_str_rot13(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5059 | `{` |
|       - | 5060 | `	const char *zIn;` |
|       - | 5061 | `	char *zOut;` |
|       - | 5062 | `	int nLen,i;` |
|      22 | 5063 | `	if( nArg < 1 ){` |
|       - | 5064 | `		/* Missing arguments,return the empty string */` |
|     ! 0 | 5065 | `		ph7_result_string(pCtx,"",0);` |
|     ! 0 | 5066 | `		return PH7_OK;` |
|       - | 5067 | `	}` |
|      22 | 5068 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      22 | 5069 | `	if( nLen < 1 ){` |
|       3 | 5070 | `		ph7_result_string(pCtx,"",0);` |
|       3 | 5071 | `		return PH7_OK;` |
|       - | 5072 | `	}` |
|      20 | 5073 | `	zOut = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,TRUE);` |
|      20 | 5074 | `	if( zOut == 0 ){` |
|     ! 0 | 5075 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 5076 | `	}` |
|     204 | 5077 | `	for( i = 0 ; i < nLen ; i++ ){` |
|     190 | 5078 | `		int c = (unsigned char)zIn[i];` |
|     190 | 5079 | `		if( (c >= 'a' && c <= 'm') \|\| (c >= 'A' && c <= 'M') ){` |
|      75 | 5080 | `			c += 13;` |
|     155 | 5081 | `		}else if( (c >= 'n' && c <= 'z') \|\| (c >= 'N' && c <= 'Z') ){` |
|      69 | 5082 | `			c -= 13;` |
|      34 | 5083 | `		}` |
|     186 | 5084 | `		zOut[i] = (char)c;` |
|      94 | 5085 | `	}` |
|      16 | 5086 | `	ph7_result_string(pCtx,zOut,nLen);` |
|      16 | 5087 | `	return PH7_OK;` |
|      10 | 5088 | `}` |
|       - | 5089 | `/*` |
|       - | 5090 | ` * Character-class table for metaphone(), php's _codes[] (ext/standard/` |
|       - | 5091 | ` * metaphone.c, itself from CPAN Text-Metaphone), indexed by 'A'..'Z':` |
|       - | 5092 | ` * bit 1 vowel (AEIOU) · bit 2 passes through unchanged (FJMNR) · bit 4 forms a` |
|       - | 5093 | ` * diphthong before H (CGPST) · bit 8 makes C and G soft (EIY) · bit 16 keeps a` |
|       - | 5094 | ` * GH from becoming F (BDH).` |
|       - | 5095 | ` */` |
|       - | 5096 | `static const char aMetaCode[26] = {` |
|       - | 5097 | `	1,16,4,16,9,2,4,16,9,2,0,2,2,2,1,4,0,2,4,4,1,0,0,0,8,0` |
|       - | 5098 | `};` |
|       - | 5099 | `/* Classification is ASCII-only, php's own table (the locale-dependence family:` |
|       - | 5100 | ` * libc's isalpha()/toupper() answer differently under a non-C LC_CTYPE). */` |
|       - | 5101 | `#define META_IS_ALPHA(c) (((c) >= 'A' && (c) <= 'Z') \|\| ((c) >= 'a' && (c) <= 'z'))` |
|       - | 5102 | `#define META_UP(c)       (((c) >= 'a' && (c) <= 'z') ? (char)((c) - ('a' - 'A')) : (char)(c))` |
|       - | 5103 | `#define META_ENCODE(c)   (((c) >= 'A' && (c) <= 'Z') ? aMetaCode[(c) - 'A'] : 0)` |
|       - | 5104 | `#define META_ISVOWEL(c)  (META_ENCODE(c) & 1)  /* AEIOU */` |
|       - | 5105 | `#define META_AFFECTH(c)  (META_ENCODE(c) & 4)  /* CGPST */` |
|       - | 5106 | `#define META_MAKESOFT(c) (META_ENCODE(c) & 8)  /* EIY */` |
|       - | 5107 | `#define META_NOGHTOF(c)  (META_ENCODE(c) & 16) /* BDH */` |
|       - | 5108 | `/* php's special phoneme encodings: 'sh' and 'th' */` |
|       - | 5109 | `#define META_SH '\x58' /* 'X' */` |
|       - | 5110 | `#define META_TH '\x30' /* '0' */` |
|       - | 5111 | `/*` |
|       - | 5112 | ` * php's Lookahead(): step up to nHow bytes forward from iFrom, stopping early` |
|       - | 5113 | ` * at a NUL, and answer the byte at the stop position. The php original walks a` |
|       - | 5114 | ` * NUL-terminated buffer; this walks the same way over a bounded one, treating` |
|       - | 5115 | ` * the end of the buffer as the NUL.` |
|       - | 5116 | ` */` |
|     ! 0 | 5117 | `static char MetaLookahead(const char *zIn,sxu32 nLen,sxu32 iFrom,sxu32 nHow)` |
|     ! 0 | 5118 | `{` |
|       - | 5119 | `	sxu32 idx;` |
|     ! 0 | 5120 | `	for( idx = 0 ; idx < nHow ; idx++ ){` |
|     ! 0 | 5121 | `		if( iFrom + idx >= nLen \|\| zIn[iFrom + idx] == '\0' ){` |
|     ! 0 | 5122 | `			break;` |
|       - | 5123 | `		}` |
|     ! 0 | 5124 | `	}` |
|     ! 0 | 5125 | `	return (iFrom + idx < nLen) ? zIn[iFrom + idx] : '\0';` |
|     ! 0 | 5126 | `}` |
|       - | 5127 | `/*` |
|       - | 5128 | ` * string metaphone(string $string, int $max_phonemes = 0)` |
|       - | 5129 | ` *  Break an english phrase down into its phonemes. Faithful port of php-src` |
|       - | 5130 | `` *  PHP-8.5 ext/standard/metaphone.c (the `traditional` flavour, which is the`` |
|       - | 5131 | ` *  only one php's own function invokes — the non-traditional Christ/School/` |
|       - | 5132 | ` *  SCHW branches are compiled out there and are not ported). Like php's, the` |
|       - | 5133 | ` *  scan stops at an embedded NUL: the original walks a NUL-terminated buffer.` |
|       - | 5134 | ` * Return` |
|       - | 5135 | ` *  The phonemes as a string of A-Z plus php's two special encodings` |
|       - | 5136 | ` *  ('X' for "sh", '0' for "th"); "" when no letter is reached.` |
|       - | 5137 | ` */` |
|      80 | 5138 | `PH7_PRIVATE int PH7_builtin_metaphone(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5139 | `{` |
|       - | 5140 | `	const char *zIn;` |
|       - | 5141 | `	char *zOut;` |
|      82 | 5142 | `	sxi64 iMaxPhonemes = 0;` |
|      82 | 5143 | `	sxu32 w_idx = 0,nLen;` |
|      82 | 5144 | `	int nOut = 0;` |
|       - | 5145 | `	int nByte;` |
|       - | 5146 | `	char cCurr;` |
|       - | 5147 | `	/* Bounded reads standing in for the php original's NUL-terminated ones. */` |
|       - | 5148 | `#define META_BYTE(i)   ((sxu32)(i) < nLen ? zIn[(i)] : '\0')` |
|       - | 5149 | `#define META_NEXT      (META_UP(META_BYTE(w_idx + 1)))` |
|       - | 5150 | `#define META_PREV      (w_idx >= 1 ? META_UP(META_BYTE(w_idx - 1)) : '\0')` |
|       - | 5151 | `#define META_BACK(n)   (w_idx >= (sxu32)(n) ? META_UP(META_BYTE(w_idx - (sxu32)(n))) : '\0')` |
|       - | 5152 | `#define META_AFTERNEXT (META_BYTE(w_idx + 1) != '\0' ? META_UP(META_BYTE(w_idx + 2)) : '\0')` |
|       - | 5153 | `#define META_PHONIZE(c) do { zOut[nOut++] = (c); } while(0)` |
|      82 | 5154 | `	if( nArg < 1 ){` |
|       - | 5155 | `		/* Missing arguments,return the empty string */` |
|     ! 0 | 5156 | `		ph7_result_string(pCtx,"",0);` |
|     ! 0 | 5157 | `		return PH7_OK;` |
|       - | 5158 | `	}` |
|      82 | 5159 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|      82 | 5160 | `	if( nArg > 1 ){` |
|      13 | 5161 | `		iMaxPhonemes = ph7_value_to_int64(apArg[1]);` |
|      13 | 5162 | `		if( iMaxPhonemes < 0 ){` |
|       3 | 5163 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 5164 | `				"metaphone(): Argument #2 ($max_phonemes) must be greater than or equal to 0");` |
|       - | 5165 | `		}` |
|       5 | 5166 | `	}` |
|      80 | 5167 | `	nLen = (sxu32)(nByte > 0 ? nByte : 0);` |
|       - | 5168 | `	/* Two output bytes per input letter ('X' phonizes "KS") is the ceiling, so` |
|       - | 5169 | `	 * one allocation covers the whole run — php grows its buffer instead. */` |
|      80 | 5170 | `	zOut = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(2 * nLen + 4),FALSE,TRUE);` |
|      80 | 5171 | `	if( zOut == 0 ){` |
|     ! 0 | 5172 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 5173 | `	}` |
|       - | 5174 | `	/* Find the first letter; nothing but non-letters answers "". */` |
|      98 | 5175 | `	for( ; !META_IS_ALPHA(cCurr = META_BYTE(w_idx)) ; w_idx++ ){` |
|      23 | 5176 | `		if( cCurr == '\0' ){` |
|       5 | 5177 | `			ph7_result_string(pCtx,"",0);` |
|       5 | 5178 | `			return PH7_OK;` |
|       - | 5179 | `		}` |
|      10 | 5180 | `	}` |
|       - | 5181 | `	/* The first phoneme is processed specially. A case that neither phonizes` |
|       - | 5182 | `	 * nor advances leaves the letter for the main loop to read as an ordinary` |
|       - | 5183 | `	 * one (php's own structure). */` |
|      76 | 5184 | `	cCurr = META_UP(cCurr);` |
|      76 | 5185 | `	switch( cCurr ){` |
|       3 | 5186 | `	case 'A':` |
|       - | 5187 | `		/* AE becomes E; a vowel at the beginning is preserved */` |
|       8 | 5188 | `		if( META_NEXT == 'E' ){` |
|     ! 0 | 5189 | `			META_PHONIZE('E');` |
|     ! 0 | 5190 | `			w_idx += 2;` |
|     ! 0 | 5191 | `		}else{` |
|       8 | 5192 | `			META_PHONIZE('A');` |
|       8 | 5193 | `			w_idx++;` |
|       - | 5194 | `		}` |
|       8 | 5195 | `		break;` |
|       5 | 5196 | `	case 'G':` |
|       - | 5197 | `	case 'K':` |
|       - | 5198 | `	case 'P':` |
|       - | 5199 | `		/* [GKP]N becomes N */` |
|      11 | 5200 | `		if( META_NEXT == 'N' ){` |
|       5 | 5201 | `			META_PHONIZE('N');` |
|       5 | 5202 | `			w_idx += 2;` |
|       2 | 5203 | `		}` |
|      11 | 5204 | `		break;` |
|       3 | 5205 | `	case 'W':{` |
|       - | 5206 | `		/* WR becomes R; WH and W before a vowel keep the W; else dropped */` |
|       7 | 5207 | `		char cNext = META_NEXT;` |
|       7 | 5208 | `		if( cNext == 'R' ){` |
|       3 | 5209 | `			META_PHONIZE('R');` |
|       3 | 5210 | `			w_idx += 2;` |
|       6 | 5211 | `		}else if( cNext == 'H' \|\| META_ISVOWEL(cNext) ){` |
|       5 | 5212 | `			META_PHONIZE('W');` |
|       5 | 5213 | `			w_idx += 2;` |
|       2 | 5214 | `		}` |
|       7 | 5215 | `		break;` |
|       - | 5216 | `	}` |
|       1 | 5217 | `	case 'X':` |
|       - | 5218 | `		/* X becomes S */` |
|       3 | 5219 | `		META_PHONIZE('S');` |
|       3 | 5220 | `		w_idx++;` |
|       3 | 5221 | `		break;` |
|       2 | 5222 | `	case 'E':` |
|       - | 5223 | `	case 'I':` |
|       - | 5224 | `	case 'O':` |
|       - | 5225 | `	case 'U':` |
|       - | 5226 | `		/* Vowels are kept (A handled above) */` |
|       5 | 5227 | `		META_PHONIZE(cCurr);` |
|       5 | 5228 | `		w_idx++;` |
|       4 | 5229 | `		break;` |
|      23 | 5230 | `	default:` |
|      46 | 5231 | `		break;` |
|       - | 5232 | `	}` |
|       - | 5233 | `	/* On to the metaphoning */` |
|     658 | 5234 | `	for( ; (cCurr = META_BYTE(w_idx)) != '\0' &&` |
|     604 | 5235 | `	       (iMaxPhonemes == 0 \|\| (sxi64)nOut < iMaxPhonemes) ; w_idx++ ){` |
|       - | 5236 | `		/* Letters an encoding below consumed along with this one */` |
|     388 | 5237 | `		sxu32 nSkip = 0;` |
|       - | 5238 | `		char cPrev;` |
|     388 | 5239 | `		if( !META_IS_ALPHA(cCurr) ){` |
|       5 | 5240 | `			continue;` |
|       - | 5241 | `		}` |
|     384 | 5242 | `		cCurr = META_UP(cCurr);` |
|     384 | 5243 | `		cPrev = META_PREV;` |
|       - | 5244 | `		/* Drop duplicates, except CC */` |
|     384 | 5245 | `		if( cCurr == cPrev && cCurr != 'C' ){` |
|       7 | 5246 | `			continue;` |
|       - | 5247 | `		}` |
|     378 | 5248 | `		switch( cCurr ){` |
|       3 | 5249 | `		case 'B':` |
|       - | 5250 | `			/* B unless in MB */` |
|       8 | 5251 | `			if( cPrev != 'M' ){` |
|       3 | 5252 | `				META_PHONIZE('B');` |
|       1 | 5253 | `			}` |
|       8 | 5254 | `			break;` |
|      17 | 5255 | `		case 'C':{` |
|       - | 5256 | `			/* 'sh' in -CIA- and -CH-; S in -CI-, -CE-, -CY-;` |
|       - | 5257 | `			 * dropped in -SCI-, -SCE-, -SCY-; else K */` |
|      36 | 5258 | `			char cNext = META_NEXT;` |
|      36 | 5259 | `			if( META_MAKESOFT(cNext) ){ /* C[IEY] */` |
|       7 | 5260 | `				if( cNext == 'I' && META_AFTERNEXT == 'A' ){ /* CIA */` |
|     ! 0 | 5261 | `					META_PHONIZE(META_SH);` |
|       6 | 5262 | `				}else if( cPrev == 'S' ){` |
|       - | 5263 | `					/* dropped */` |
|       2 | 5264 | `				}else{` |
|       5 | 5265 | `					META_PHONIZE('S');` |
|       1 | 5266 | `				}` |
|      33 | 5267 | `			}else if( cNext == 'H' ){` |
|      15 | 5268 | `				META_PHONIZE(META_SH);` |
|      15 | 5269 | `				nSkip++;` |
|       8 | 5270 | `			}else{` |
|      16 | 5271 | `				META_PHONIZE('K');` |
|       - | 5272 | `			}` |
|      36 | 5273 | `			break;` |
|       - | 5274 | `		}` |
|       5 | 5275 | `		case 'D':` |
|       - | 5276 | `			/* J in -DGE-, -DGI-, -DGY-; else T */` |
|      14 | 5277 | `			if( META_NEXT == 'G' && META_MAKESOFT(META_AFTERNEXT) ){` |
|       3 | 5278 | `				META_PHONIZE('J');` |
|       3 | 5279 | `				nSkip++;` |
|       2 | 5280 | `			}else{` |
|       9 | 5281 | `				META_PHONIZE('T');` |
|       - | 5282 | `			}` |
|      11 | 5283 | `			break;` |
|       7 | 5284 | `		case 'G':{` |
|       - | 5285 | `			/* F in -GH unless B--GH, D--GH, -H--GH, -H---GH (silent there);` |
|       - | 5286 | `			 * dropped in -GN, -GNED (and -DG[EIY]-, handled in D);` |
|       - | 5287 | `			 * J in -GE-, -GI-, -GY- when not GG; else K */` |
|      15 | 5288 | `			char cNext = META_NEXT;` |
|      15 | 5289 | `			if( cNext == 'H' ){` |
|      17 | 5290 | `				if( !(META_NOGHTOF(META_BACK(3)) \|\| META_BACK(4) == 'H') ){` |
|       9 | 5291 | `					META_PHONIZE('F');` |
|       9 | 5292 | `					nSkip++;` |
|       5 | 5293 | `				}` |
|      11 | 5294 | `			}else if( cNext == 'N' ){` |
|     ! 0 | 5295 | `				char cAfterNext = META_AFTERNEXT;` |
|     ! 0 | 5296 | `				if( !META_IS_ALPHA(cAfterNext) \|\|` |
|     ! 0 | 5297 | `				    (cAfterNext == 'E' && META_UP(MetaLookahead(zIn,nLen,w_idx,3)) == 'D') ){` |
|       - | 5298 | `					/* dropped */` |
|     ! 0 | 5299 | `				}else{` |
|     ! 0 | 5300 | `					META_PHONIZE('K');` |
|     ! 0 | 5301 | `				}` |
|       7 | 5302 | `			}else if( META_MAKESOFT(cNext) && cPrev != 'G' ){` |
|       3 | 5303 | `				META_PHONIZE('J');` |
|       2 | 5304 | `			}else{` |
|       5 | 5305 | `				META_PHONIZE('K');` |
|       - | 5306 | `			}` |
|      15 | 5307 | `			break;` |
|       - | 5308 | `		}` |
|       3 | 5309 | `		case 'H':` |
|       - | 5310 | `			/* H before a vowel and not after C, G, P, S, T */` |
|       7 | 5311 | `			if( META_ISVOWEL(META_NEXT) && !META_AFFECTH(cPrev) ){` |
|       3 | 5312 | `				META_PHONIZE('H');` |
|       1 | 5313 | `			}` |
|       7 | 5314 | `			break;` |
|       2 | 5315 | `		case 'K':` |
|       - | 5316 | `			/* dropped after C; else K */` |
|       5 | 5317 | `			if( cPrev != 'C' ){` |
|       3 | 5318 | `				META_PHONIZE('K');` |
|       1 | 5319 | `			}` |
|       5 | 5320 | `			break;` |
|       6 | 5321 | `		case 'P':` |
|       - | 5322 | `			/* F before H; else P */` |
|      14 | 5323 | `			if( META_NEXT == 'H' ){` |
|       3 | 5324 | `				META_PHONIZE('F');` |
|       2 | 5325 | `			}else{` |
|      12 | 5326 | `				META_PHONIZE('P');` |
|       - | 5327 | `			}` |
|      14 | 5328 | `			break;` |
|       2 | 5329 | `		case 'Q':` |
|       5 | 5330 | `			META_PHONIZE('K');` |
|       5 | 5331 | `			break;` |
|      14 | 5332 | `		case 'S':{` |
|       - | 5333 | `			/* 'sh' in -SH-, -SIO-, -SIA-; else S */` |
|      30 | 5334 | `			char cNext = META_NEXT;` |
|       - | 5335 | `			char cAfterNext;` |
|      30 | 5336 | `			if( cNext == 'I' &&` |
|       2 | 5337 | `			    ((cAfterNext = META_AFTERNEXT) == 'O' \|\| cAfterNext == 'A') ){` |
|       3 | 5338 | `				META_PHONIZE(META_SH);` |
|      28 | 5339 | `			}else if( cNext == 'H' ){` |
|     ! 0 | 5340 | `				META_PHONIZE(META_SH);` |
|     ! 0 | 5341 | `				nSkip++;` |
|     ! 0 | 5342 | `			}else{` |
|      28 | 5343 | `				META_PHONIZE('S');` |
|       - | 5344 | `			}` |
|      30 | 5345 | `			break;` |
|       - | 5346 | `		}` |
|      17 | 5347 | `		case 'T':{` |
|       - | 5348 | `			/* 'sh' in -TIA-, -TIO-; 'th' before H; dropped in -TCH-; else T */` |
|      36 | 5349 | `			char cNext = META_NEXT;` |
|       - | 5350 | `			char cAfterNext;` |
|      36 | 5351 | `			if( cNext == 'I' &&` |
|       4 | 5352 | `			    ((cAfterNext = META_AFTERNEXT) == 'O' \|\| cAfterNext == 'A') ){` |
|       5 | 5353 | `				META_PHONIZE(META_SH);` |
|      33 | 5354 | `			}else if( cNext == 'H' ){` |
|      12 | 5355 | `				META_PHONIZE(META_TH);` |
|      12 | 5356 | `				nSkip++;` |
|      26 | 5357 | `			}else if( !(cNext == 'C' && META_AFTERNEXT == 'H') ){` |
|      17 | 5358 | `				META_PHONIZE('T');` |
|       8 | 5359 | `			}` |
|      36 | 5360 | `			break;` |
|       - | 5361 | `		}` |
|       2 | 5362 | `		case 'V':` |
|       5 | 5363 | `			META_PHONIZE('F');` |
|       5 | 5364 | `			break;` |
|       1 | 5365 | `		case 'W':` |
|       - | 5366 | `			/* W before a vowel, else dropped */` |
|       3 | 5367 | `			if( META_ISVOWEL(META_NEXT) ){` |
|     ! 0 | 5368 | `				META_PHONIZE('W');` |
|     ! 0 | 5369 | `			}` |
|       3 | 5370 | `			break;` |
|       2 | 5371 | `		case 'X':` |
|       6 | 5372 | `			META_PHONIZE('K');` |
|       6 | 5373 | `			META_PHONIZE('S');` |
|       6 | 5374 | `			break;` |
|       7 | 5375 | `		case 'Y':` |
|       - | 5376 | `			/* Y before a vowel, else dropped */` |
|      15 | 5377 | `			if( META_ISVOWEL(META_NEXT) ){` |
|       3 | 5378 | `				META_PHONIZE('Y');` |
|       1 | 5379 | `			}` |
|      15 | 5380 | `			break;` |
|       1 | 5381 | `		case 'Z':` |
|       3 | 5382 | `			META_PHONIZE('S');` |
|       3 | 5383 | `			break;` |
|      33 | 5384 | `		case 'F':` |
|       - | 5385 | `		case 'J':` |
|       - | 5386 | `		case 'L':` |
|       - | 5387 | `		case 'M':` |
|       - | 5388 | `		case 'N':` |
|       - | 5389 | `		case 'R':` |
|       - | 5390 | `			/* passed through unchanged */` |
|      68 | 5391 | `			META_PHONIZE(cCurr);` |
|      66 | 5392 | `			break;` |
|      66 | 5393 | `		default:` |
|     132 | 5394 | `			break;` |
|       - | 5395 | `		}` |
|     378 | 5396 | `		w_idx += nSkip;` |
|     190 | 5397 | `	}` |
|      76 | 5398 | `	ph7_result_string(pCtx,zOut,nOut);` |
|      76 | 5399 | `	return PH7_OK;` |
|       - | 5400 | `#undef META_BYTE` |
|       - | 5401 | `#undef META_NEXT` |
|       - | 5402 | `#undef META_PREV` |
|       - | 5403 | `#undef META_BACK` |
|       - | 5404 | `#undef META_AFTERNEXT` |
|       - | 5405 | `#undef META_PHONIZE` |
|      42 | 5406 | `}` |
|       - | 5407 | `/*` |
|       - | 5408 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|       - | 5409 | ` *  Wraps a string to a given number of characters.` |
|       - | 5410 | ` * Parameters` |
|       - | 5411 | ` *  $str` |
|       - | 5412 | ` *   The input string.` |
|       - | 5413 | ` * $width` |
|       - | 5414 | ` *  The column width.` |
|       - | 5415 | ` * $break` |
|       - | 5416 | ` *  The line is broken using the optional break parameter.` |
|       - | 5417 | ` * Return` |
|       - | 5418 | ` *  Returns the given string wrapped at the specified column.` |
|       - | 5419 | ` */` |
|      28 | 5420 | `PH7_PRIVATE int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5421 | `{` |
|       - | 5422 | `	const char *zIn,*zBreak;` |
|       - | 5423 | `	SyBlob sWorker;` |
|       - | 5424 | `	int iLen,iBreaklen,iWidth,iCut,iStart,iSpace,iCur;` |
|       - | 5425 | `	sxi32 rc;` |
|      30 | 5426 | `	if( nArg < 1 ){` |
|       - | 5427 | `		/* Missing arguments,return the empty string */` |
|     ! 0 | 5428 | `		ph7_result_string(pCtx,"",0);` |
|     ! 0 | 5429 | `		return PH7_OK;` |
|       - | 5430 | `	}` |
|       - | 5431 | `	/* Extract the input string */` |
|      30 | 5432 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|       - | 5433 | `	/* Width (default 75; PHP allows 0/negative — break at every space). */` |
|      30 | 5434 | `	iWidth = 75;` |
|      30 | 5435 | `	if( nArg > 1 ){` |
|      27 | 5436 | `		iWidth = ph7_value_to_int(apArg[1]);` |
|      13 | 5437 | `	}` |
|       - | 5438 | `	/* Break string (default "\n"). */` |
|      30 | 5439 | `	zBreak = "\n";` |
|      30 | 5440 | `	iBreaklen = (int)sizeof(char);` |
|      30 | 5441 | `	if( nArg > 2 ){` |
|      13 | 5442 | `		zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|       6 | 5443 | `	}` |
|       - | 5444 | `	/* Cut long words? (default false). */` |
|      30 | 5445 | `	iCut = 0;` |
|      30 | 5446 | `	if( nArg > 3 ){` |
|       7 | 5447 | `		iCut = ph7_value_to_bool(apArg[3]);` |
|       3 | 5448 | `	}` |
|      30 | 5449 | `	if( iLen < 1 ){` |
|       - | 5450 | `		/* PHP returns the empty string for empty input before validating the other args. */` |
|       8 | 5451 | `		ph7_result_string(pCtx,"",0);` |
|       8 | 5452 | `		return PH7_OK;` |
|       - | 5453 | `	}` |
|       - | 5454 | `	/* PHP 8 domain errors (catchable ValueError). */` |
|      23 | 5455 | `	if( iBreaklen < 1 ){` |
|       3 | 5456 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 5457 | `			"wordwrap(): Argument #3 ($break) must not be empty");` |
|       - | 5458 | `	}` |
|      21 | 5459 | `	if( iWidth == 0 && iCut ){` |
|       3 | 5460 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 5461 | `			"wordwrap(): Argument #4 ($cut_long_words) cannot be true when argument #2 ($width) is 0");` |
|       - | 5462 | `	}` |
|       - | 5463 | `	/*` |
|       - | 5464 | `	 * PHP's algorithm: a single left-to-right pass tracking the start of the` |
|       - | 5465 | `	 * current line (iStart) and the position of the last space seen on it` |
|       - | 5466 | `	 * (iSpace). A break is emitted when the line reaches the width, at the last` |
|       - | 5467 | `	 * space if there was one, otherwise (only when cut is enabled) hard at the` |
|       - | 5468 | `	 * boundary. An existing break sequence in the input resets the line.` |
|       - | 5469 | `	 */` |
|      19 | 5470 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|      19 | 5471 | `	iStart = iSpace = iCur = 0;` |
|      19 | 5472 | `	rc = SXRET_OK;` |
|     551 | 5473 | `	while( iCur < iLen ){` |
|     533 | 5474 | `		if( iBreaklen <= iLen - iCur && SyMemcmp(&zIn[iCur],zBreak,(sxu32)iBreaklen) == 0 ){` |
|       - | 5475 | `			/* Existing break sequence in the input: copy it verbatim and reset the line. */` |
|     ! 0 | 5476 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart + iBreaklen));` |
|     ! 0 | 5477 | `			if( rc != SXRET_OK ){ goto oom; }` |
|     ! 0 | 5478 | `			iCur += iBreaklen;` |
|     ! 0 | 5479 | `			iStart = iSpace = iCur;` |
|     ! 0 | 5480 | `			continue;` |
|     533 | 5481 | `		}else if( zIn[iCur] == ' ' ){` |
|      67 | 5482 | `			if( iCur - iStart >= iWidth ){` |
|       - | 5483 | `				/* The line already fills the width at this space: break here (the space is consumed). */` |
|      13 | 5484 | `				rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|      13 | 5485 | `				if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|      13 | 5486 | `				if( rc != SXRET_OK ){ goto oom; }` |
|      13 | 5487 | `				iStart = iCur + 1;` |
|       6 | 5488 | `			}` |
|      67 | 5489 | `			iSpace = iCur;` |
|     500 | 5490 | `		}else if( iCut && iCur - iStart >= iWidth && iStart >= iSpace ){` |
|       - | 5491 | `			/* A word longer than the width with no space to break at: hard-cut at the boundary. */` |
|       7 | 5492 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|       7 | 5493 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|       7 | 5494 | `			if( rc != SXRET_OK ){ goto oom; }` |
|       7 | 5495 | `			iStart = iSpace = iCur;` |
|     464 | 5496 | `		}else if( iCur - iStart >= iWidth && iStart < iSpace ){` |
|       - | 5497 | `			/* Past the width mid-word: wrap back to the last space (which is consumed). */` |
|      17 | 5498 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iSpace - iStart));` |
|      17 | 5499 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|      17 | 5500 | `			if( rc != SXRET_OK ){ goto oom; }` |
|      17 | 5501 | `			iStart = iSpace = iSpace + 1;` |
|       8 | 5502 | `		}` |
|     533 | 5503 | `		iCur++;` |
|       1 | 5504 | `	}` |
|       - | 5505 | `	/* Emit the trailing chunk. */` |
|      19 | 5506 | `	if( iStart < iCur ){` |
|      19 | 5507 | `		rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|      19 | 5508 | `		if( rc != SXRET_OK ){ goto oom; }` |
|       9 | 5509 | `	}` |
|      19 | 5510 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|      19 | 5511 | `	SyBlobRelease(&sWorker);` |
|      19 | 5512 | `	return PH7_OK;` |
|     ! 0 | 5513 | `oom:` |
|     ! 0 | 5514 | `	SyBlobRelease(&sWorker);` |
|     ! 0 | 5515 | `	return PH7_ContextMemoryError(pCtx);` |
|      16 | 5516 | `}` |
|       - | 5517 | `/*` |
|       - | 5518 | ` * Check if the given character is a member of the given mask.` |
|       - | 5519 | ` * Return TRUE on success. FALSE otherwise.` |
|       - | 5520 | ` * Refer to [strtok()].` |
|       - | 5521 | ` */` |
|   21253 | 5522 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|       4 | 5523 | `{` |
|       - | 5524 | `	int i;` |
|   41652 | 5525 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|   21281 | 5526 | `		if( c == zMask[i] ){` |
|     886 | 5527 | `			if( pOfft ){` |
|     664 | 5528 | `				*pOfft = i;` |
|     481 | 5529 | `			}` |
|     886 | 5530 | `			return TRUE;` |
|       - | 5531 | `		}` |
|   11730 | 5532 | `	}` |
|   20375 | 5533 | `	return FALSE;` |
|   12310 | 5534 | `}` |
|       - | 5535 | `/*` |
|       - | 5536 | ` * Extract a single token from the input stream.` |
|       - | 5537 | ` * Refer to [strtok()].` |
|       - | 5538 | ` */` |
|     238 | 5539 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|       2 | 5540 | `{` |
|     240 | 5541 | `	const char *zIn = *pzIn;` |
|       - | 5542 | `	const char *zPtr;` |
|       - | 5543 | `	/* Ignore leading delimiter */` |
|     244 | 5544 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|       5 | 5545 | `		zIn++;` |
|       1 | 5546 | `	}` |
|     240 | 5547 | `	if( zIn >= zEnd ){` |
|       - | 5548 | `		/* End of input */` |
|     ! 0 | 5549 | `		return SXERR_EOF;` |
|       - | 5550 | `	}` |
|     240 | 5551 | `	zPtr = zIn;` |
|       - | 5552 | `	/* Extract the token */` |
|   14116 | 5553 | `	while( zIn < zEnd ){` |
|   14096 | 5554 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|       - | 5555 | `			/* UTF-8 stream */` |
|     ! 0 | 5556 | `			zIn++;` |
|     ! 0 | 5557 | `			SX_JMP_UTF8(zIn,zEnd);` |
|     ! 0 | 5558 | `		}else{` |
|   14096 | 5559 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|     220 | 5560 | `				break;` |
|       - | 5561 | `			}` |
|   13878 | 5562 | `			zIn++;` |
|       - | 5563 | `		}` |
|       2 | 5564 | `	}` |
|     240 | 5565 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|       - | 5566 | `	/* Update the cursor */` |
|     240 | 5567 | `	*pzIn = zIn;` |
|       - | 5568 | `	/* Return to the caller */` |
|     240 | 5569 | `	return SXRET_OK;` |
|     121 | 5570 | `}` |
|       - | 5571 | `/* strtok auxiliary private data */` |
|       - | 5572 | `typedef struct strtok_aux_data strtok_aux_data;` |
|       - | 5573 | `struct strtok_aux_data` |
|       - | 5574 | `{` |
|       - | 5575 | `	const char *zDup;  /* Complete duplicate of the input */` |
|       - | 5576 | `	const char *zIn;   /* Current input stream */` |
|       - | 5577 | `	const char *zEnd;  /* End of input */` |
|       - | 5578 | `};` |
|       - | 5579 | `/*` |
|       - | 5580 | ` * string strtok(string $str,string $token)` |
|       - | 5581 | ` * string strtok(string $token)` |
|       - | 5582 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|       - | 5583 | ` *  being delimited by any character from token. That is, if you have a string like` |
|       - | 5584 | ` *  "This is an example string" you could tokenize this string into its individual` |
|       - | 5585 | ` *  words by using the space character as the token.` |
|       - | 5586 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|       - | 5587 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|       - | 5588 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|       - | 5589 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|       - | 5590 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|       - | 5591 | ` *  the argument are found.` |
|       - | 5592 | ` * Parameters` |
|       - | 5593 | ` *  $str` |
|       - | 5594 | ` *  The string being split up into smaller strings (tokens).` |
|       - | 5595 | ` * $token` |
|       - | 5596 | ` *  The delimiter used when splitting up str.` |
|       - | 5597 | ` * Return` |
|       - | 5598 | ` *   Current token or FALSE on EOF.` |
|       - | 5599 | ` */` |
|     238 | 5600 | `PH7_PRIVATE int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5601 | `{` |
|       - | 5602 | `	strtok_aux_data *pAux;` |
|       - | 5603 | `	const char *zMask;` |
|       - | 5604 | `	SyString sToken;` |
|       - | 5605 | `	int nMasklen;` |
|       - | 5606 | `	sxi32 rc;` |
|     240 | 5607 | `	if( nArg < 2 ){` |
|       - | 5608 | `		/* Extract top aux data */` |
|       5 | 5609 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|       5 | 5610 | `		if( pAux == 0 ){` |
|       - | 5611 | `			/* No aux data,return FALSE */` |
|     ! 0 | 5612 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 5613 | `			return PH7_OK;` |
|       - | 5614 | `		}` |
|       5 | 5615 | `		nMasklen = 0;` |
|       5 | 5616 | `		zMask = ""; /* cc warning */` |
|       5 | 5617 | `		if( nArg > 0 ){` |
|       - | 5618 | `			/* Extract the mask */` |
|       5 | 5619 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|       2 | 5620 | `		}` |
|       5 | 5621 | `		if( nMasklen < 1 ){` |
|       - | 5622 | `			/* Invalid mask,return FALSE */` |
|     ! 0 | 5623 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|     ! 0 | 5624 | `			ph7_context_free_chunk(pCtx,pAux);` |
|     ! 0 | 5625 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|     ! 0 | 5626 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 5627 | `			return PH7_OK;` |
|       - | 5628 | `		}` |
|       - | 5629 | `		/* Extract the token */` |
|       5 | 5630 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|       5 | 5631 | `		if( rc != SXRET_OK ){` |
|       - | 5632 | `			/* EOF ,discard the aux data */` |
|     ! 0 | 5633 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|     ! 0 | 5634 | `			ph7_context_free_chunk(pCtx,pAux);` |
|     ! 0 | 5635 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|     ! 0 | 5636 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 5637 | `		}else{` |
|       - | 5638 | `			/* Return the extracted token */` |
|       5 | 5639 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|       - | 5640 | `		}` |
|       3 | 5641 | `	}else{` |
|       - | 5642 | `		const char *zInput,*zCur;` |
|       - | 5643 | `		char *zDup;` |
|       - | 5644 | `		int nLen;` |
|       - | 5645 | `		/* Extract the raw input */` |
|     236 | 5646 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|     236 | 5647 | `		if( nLen < 1 ){` |
|       - | 5648 | `			/* Empty input,return FALSE */` |
|     ! 0 | 5649 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 5650 | `			return PH7_OK;` |
|       - | 5651 | `		}` |
|       - | 5652 | `		/* Extract the mask */` |
|     236 | 5653 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|     236 | 5654 | `		if( nMasklen < 1 ){` |
|       - | 5655 | `			/* Set a default mask */` |
|       - | 5656 | `#define TOK_MASK " \n\t\r\f"` |
|     ! 0 | 5657 | `			zMask = TOK_MASK;` |
|     ! 0 | 5658 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|       - | 5659 | `#undef TOK_MASK` |
|     ! 0 | 5660 | `		}` |
|       - | 5661 | `		/* Extract a single token */` |
|     236 | 5662 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|     236 | 5663 | `		if( rc != SXRET_OK ){` |
|       - | 5664 | `			/* Empty input */` |
|     ! 0 | 5665 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 5666 | `			return PH7_OK;` |
|     ! 0 | 5667 | `		}else{` |
|       - | 5668 | `			/* Return the extracted token */` |
|     236 | 5669 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|       - | 5670 | `		}` |
|       - | 5671 | `		/* Create our auxilliary data and copy the input */` |
|     236 | 5672 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|     236 | 5673 | `		if( pAux ){` |
|     236 | 5674 | `			nLen -= (int)(zInput-zCur);` |
|     236 | 5675 | `			if( nLen < 1 ){` |
|      19 | 5676 | `				ph7_context_free_chunk(pCtx,pAux);` |
|      19 | 5677 | `				return PH7_OK;` |
|       - | 5678 | `			}` |
|       - | 5679 | `			/* Duplicate input */` |
|     218 | 5680 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|     218 | 5681 | `			if( zDup  ){` |
|     218 | 5682 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|       - | 5683 | `				/* Register the aux data */` |
|     218 | 5684 | `				pAux->zDup = pAux->zIn = zDup;` |
|     218 | 5685 | `				pAux->zEnd = &zDup[nLen];` |
|     218 | 5686 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|     108 | 5687 | `			}` |
|     108 | 5688 | `		}` |
|       - | 5689 | `	}` |
|     222 | 5690 | `	return PH7_OK;` |
|     121 | 5691 | `}` |
|       - | 5692 | `/*` |
|       - | 5693 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|       - | 5694 | ` *  Pad a string to a certain length with another string` |
|       - | 5695 | ` * Parameters` |
|       - | 5696 | ` *  $input` |
|       - | 5697 | ` *   The input string.` |
|       - | 5698 | ` * $pad_length` |
|       - | 5699 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|       - | 5700 | ` *   string, no padding takes place.` |
|       - | 5701 | ` * $pad_string` |
|       - | 5702 | ` *   Note:` |
|       - | 5703 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|       - | 5704 | ` *    divided by the pad_string's length.` |
|       - | 5705 | ` * $pad_type` |
|       - | 5706 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|       - | 5707 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|       - | 5708 | ` * Return` |
|       - | 5709 | ` *  The padded string.` |
|       - | 5710 | ` */` |
|    5234 | 5711 | `PH7_PRIVATE int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5712 | `{` |
|       - | 5713 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|       - | 5714 | `	const char *zIn,*zPad;` |
|    5239 | 5715 | `	if( nArg < 2 ){` |
|       - | 5716 | `		/* Missing arguments,return the empty string */` |
|     ! 0 | 5717 | `		ph7_result_string(pCtx,"",0);` |
|     ! 0 | 5718 | `		return PH7_OK;` |
|       - | 5719 | `	}` |
|       - | 5720 | `	/* Extract the target string */` |
|    5239 | 5721 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|       - | 5722 | `	/* Padding length */` |
|       - | 5723 | `	{` |
|    5239 | 5724 | `		sxi64 iTmp = 0;` |
|    5239 | 5725 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_pad",2,"$length","int",&iTmp);` |
|    5239 | 5726 | `		if( rcArg != PH7_OK ){` |
|     ! 0 | 5727 | `			return rcArg;` |
|       - | 5728 | `		}` |
|    5239 | 5729 | `		iRealPad = iPadlen = (int)iTmp;` |
|       - | 5730 | `	}` |
|    5239 | 5731 | `	if( iPadlen > 0 ){` |
|    5237 | 5732 | `		iPadlen -= iLen;` |
|    2616 | 5733 | `	}` |
|    5239 | 5734 | `	if( iPadlen < 1  ){` |
|       - | 5735 | `		/* Return the string verbatim */` |
|      97 | 5736 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      97 | 5737 | `		return PH7_OK;` |
|       - | 5738 | `	}` |
|    5145 | 5739 | `	zPad = " "; /* Whitespace padding */` |
|    5145 | 5740 | `	iStrpad = (int)sizeof(char);` |
|    5145 | 5741 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|    5145 | 5742 | `	if( nArg > 2 ){` |
|       - | 5743 | `		/* Padding string */` |
|      59 | 5744 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      59 | 5745 | `		if( iStrpad < 1 ){` |
|       - | 5746 | `			/* An empty pad string throws a catchable ValueError in PHP 8` |
|       - | 5747 | `			 * (only reached once padding is actually required). */` |
|       3 | 5748 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 5749 | `				"str_pad(): Argument #3 ($pad_string) must not be empty");` |
|       - | 5750 | `		}` |
|      57 | 5751 | `		if( nArg > 3 ){` |
|       - | 5752 | `			/* Padd type. php 8: anything outside LEFT(0)/RIGHT(1)/BOTH(2) is a` |
|       - | 5753 | `			 * catchable ValueError (PHL used to fall back to RIGHT silently);` |
|       - | 5754 | `			 * like the empty-pad check above, php only reaches it once padding` |
|       - | 5755 | `			 * is actually required (probed: str_pad("abc",2," ",9) is "abc"). */` |
|      44 | 5756 | `			iType = ph7_value_to_int(apArg[3]);` |
|      44 | 5757 | `			if( iType < 0 \|\| iType > 2 ){` |
|       5 | 5758 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 5759 | `					"str_pad(): Argument #4 ($pad_type) must be STR_PAD_LEFT, STR_PAD_RIGHT, or STR_PAD_BOTH");` |
|       - | 5760 | `			}` |
|      19 | 5761 | `		}` |
|      25 | 5762 | `	}` |
|    5139 | 5763 | `	iDiv = 1;` |
|    5139 | 5764 | `	if( iType == 2 ){` |
|       3 | 5765 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|       1 | 5766 | `	}` |
|       - | 5767 | `	/* Perform the requested operation */` |
|    5139 | 5768 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      36 | 5769 | `		jPad = iStrpad;` |
|     148 | 5770 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|       - | 5771 | `			/* Padding */` |
|     146 | 5772 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      34 | 5773 | `				break;` |
|       - | 5774 | `			}` |
|     114 | 5775 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      58 | 5776 | `		}` |
|      36 | 5777 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      66 | 5778 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      34 | 5779 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      34 | 5780 | `				if( jPad > iStrpad ){` |
|     ! 0 | 5781 | `					jPad = iStrpad;` |
|     ! 0 | 5782 | `				}` |
|      34 | 5783 | `				if( jPad < 1){` |
|     ! 0 | 5784 | `					break;` |
|       - | 5785 | `				}` |
|      34 | 5786 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|       2 | 5787 | `			}` |
|      16 | 5788 | `		}` |
|      17 | 5789 | `	}` |
|    5139 | 5790 | `	if( iLen > 0 ){` |
|       - | 5791 | `		/* Append the input string */` |
|    5137 | 5792 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|    2566 | 5793 | `	}` |
|    5139 | 5794 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|   51855 | 5795 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|       - | 5796 | `			/* Padding */` |
|   51853 | 5797 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|    5105 | 5798 | `				break;` |
|       - | 5799 | `			}` |
|   46753 | 5800 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|   23379 | 5801 | `		}` |
|   10209 | 5802 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|    5107 | 5803 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|    5107 | 5804 | `			if( jPad > iStrpad ){` |
|     ! 0 | 5805 | `				jPad = iStrpad;` |
|     ! 0 | 5806 | `			}` |
|    5107 | 5807 | `			if( jPad < 1){` |
|     ! 0 | 5808 | `				break;` |
|       - | 5809 | `			}` |
|    5107 | 5810 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|       5 | 5811 | `		}` |
|    2551 | 5812 | `	}` |
|    5139 | 5813 | `	return PH7_OK;` |
|    2622 | 5814 | `}` |
|       - | 5815 | `/*` |
|       - | 5816 | ` * String replacement private data.` |
|       - | 5817 | ` */` |
|       - | 5818 | `typedef struct str_replace_data str_replace_data;` |
|       - | 5819 | `struct str_replace_data` |
|       - | 5820 | `{` |
|       - | 5821 | `	/* Used by the str_replace family to collect the search/replace arguments. */` |
|       - | 5822 | `	SySet *pCollector;  /* Argument collector*/` |
|       - | 5823 | `	ph7_context *pCtx;  /* Call context */` |
|       - | 5824 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|       - | 5825 | `};` |
|       - | 5826 | `/*` |
|       - | 5827 | ` * Remove a substring.` |
|       - | 5828 | ` */` |
|       - | 5829 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|       - | 5830 | `	for(;;){\` |
|       - | 5831 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|       - | 5832 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|       - | 5833 | `		++OFFT;\` |
|       - | 5834 | `	}\` |
|       - | 5835 | `}` |
|       - | 5836 | `/*` |
|       - | 5837 | ` * Shift right and insert algorithm.` |
|       - | 5838 | ` */` |
|       - | 5839 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|       - | 5840 | `		sxu32 INLEN = LEN - OFFT;\` |
|       - | 5841 | `		for(;;){\` |
|       - | 5842 | `			if( LEN > 0 ){ LEN--; }\` |
|       - | 5843 | `			if(INLEN < 1 ) { break; }\` |
|       - | 5844 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|       - | 5845 | `			--INLEN; \` |
|       - | 5846 | `		}\` |
|       - | 5847 | `		for(;;){\` |
|       - | 5848 | `				if(ELEN < 1) { break; }\` |
|       - | 5849 | `				SRC[OFFT] = ENTRY[0];\` |
|       - | 5850 | `				OFFT++;\` |
|       - | 5851 | `				ENTRY++;\` |
|       - | 5852 | `				--ELEN;\` |
|       - | 5853 | `		}\` |
|       - | 5854 | `}` |
|       - | 5855 | `/*` |
|       - | 5856 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|       - | 5857 | ` * replacement string [i.e: zReplace].` |
|       - | 5858 | ` */` |
|   13979 | 5859 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|       5 | 5860 | `{` |
|   13984 | 5861 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|       - | 5862 | `	sxu32 n,m;` |
|   13984 | 5863 | `	n = SyBlobLength(pWorker);` |
|   13984 | 5864 | `	m = nOfft;` |
|       - | 5865 | `	/* Delete the old entry */` |
|  690150 | 5866 | `	STRDEL(zInput,n,m,nLen);` |
|   13984 | 5867 | `	SyBlobLength(pWorker) -= nLen;` |
|   13984 | 5868 | `	if( nReplen > 0 ){` |
|    6067 | 5869 | `		sxi32 iRep = nReplen;` |
|       - | 5870 | `		sxi32 rc;` |
|       - | 5871 | `		/*` |
|       - | 5872 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|       - | 5873 | `		 * string.` |
|       - | 5874 | `		 */` |
|    6067 | 5875 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|    6067 | 5876 | `		if( rc != SXRET_OK ){` |
|       - | 5877 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|       - | 5878 | `			 * instead of returning a partially-replaced string as success. */` |
|     ! 0 | 5879 | `			return rc;` |
|       - | 5880 | `		}` |
|       - | 5881 | `		/* Perform the insertion now */` |
|    6067 | 5882 | `		zInput = (char *)SyBlobData(pWorker);` |
|    6067 | 5883 | `		n = SyBlobLength(pWorker);` |
|  324726 | 5884 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|    6067 | 5885 | `		SyBlobLength(pWorker) += nReplen;` |
|    3024 | 5886 | `	}` |
|   13984 | 5887 | `	return SXRET_OK;` |
|    6910 | 5888 | `}` |
|       - | 5889 | `/*` |
|       - | 5890 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|       - | 5891 | ` * to collect search/replace string.` |
|       - | 5892 | ` * This callback is invoked only if the given argument is of type array.` |
|       - | 5893 | ` */` |
|   11444 | 5894 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|       5 | 5895 | `{` |
|   11449 | 5896 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|       - | 5897 | `	SyString sWorker;` |
|       - | 5898 | `	const char *zIn;` |
|       - | 5899 | `	int nByte;` |
|       - | 5900 | `	/* Extract a string representation of the given argument, USER-VISIBLY: php's` |
|       - | 5901 | ``	 * cast of a search or replace TERM warns `Array to string conversion` for a`` |
|       - | 5902 | `` 	 * nested array and throws the catchable `could not be converted to string` `` |
|       - | 5903 | `	 * for an object with no __toString() -- PHL rendered the latter as the` |
|       - | 5904 | `	 * literal "Object" and said nothing about the former.` |
|       - | 5905 | `	 *` |
|       - | 5906 | `	 * php does NOT stop for the throw: zval_get_string leaves the empty string` |
|       - | 5907 | ``	 * behind and the walk carries on, which is why `&$count` still comes back`` |
|       - | 5908 | `	 * written (0) from a call that threw. The status is on the call context and` |
|       - | 5909 | `	 * OP_CALL lands it once this builtin has finished. Only the FIRST failure` |
|       - | 5910 | `	 * raises -- a second one would land a second Error for one call. */` |
|   11449 | 5911 | `	PH7_ValueToStringUVOnce(pRep->pCtx,pData,&zIn,&nByte);` |
|   11449 | 5912 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|   11449 | 5913 | `	if( nByte > 0 ){` |
|       - | 5914 | `		char *zDup;` |
|       - | 5915 | `		/* Duplicate the chunk */` |
|   10933 | 5916 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|       - | 5917 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|       - | 5918 | `			);` |
|   10933 | 5919 | `		if( zDup == 0 ){` |
|       - | 5920 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|       - | 5921 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|     ! 0 | 5922 | `			pRep->rc = SXERR_MEM;` |
|     ! 0 | 5923 | `			return SXERR_MEM;` |
|       - | 5924 | `		}` |
|   10933 | 5925 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|       - | 5926 | `		/* Save the chunk */` |
|   10933 | 5927 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|    5463 | 5928 | `	}` |
|       - | 5929 | `	/* Save for later processing */` |
|   11449 | 5930 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|       - | 5931 | `	/* All done */` |
|    5721 | 5932 | `	SXUNUSED(pKey); /* cc warning */` |
|   11449 | 5933 | `	return PH7_OK;` |
|    5726 | 5934 | `}` |
|       - | 5935 | `/*` |
|       - | 5936 | ` * Run the collected search/replace pairs over a single subject string, writing` |
|       - | 5937 | ` * the transformed bytes into pOut (reset here). Shared by the scalar-subject and` |
|       - | 5938 | ` * the array-subject (element-wise) paths. The search/replace SySets are walked` |
|       - | 5939 | ` * fresh on every call — cursors are reset here — so each array element is` |
|       - | 5940 | ` * transformed independently, exactly like php. Returns SXRET_OK, or SXERR_MEM` |
|       - | 5941 | ` * on an allocation failure inside StringReplace.` |
|       - | 5942 | ` *` |
|       - | 5943 | ` * *pnCount is INCREMENTED (never reset) by the number of replacements performed,` |
|       - | 5944 | ` * so an array subject accumulates across its elements exactly like php's &$count.` |
|       - | 5945 | ` */` |
|   79964 | 5946 | `static sxi32 StrReplaceOneSubject(` |
|       - | 5947 | `	SyBlob *pOut,             /* Output buffer (reset then filled here) */` |
|       - | 5948 | `	const char *zSubject,     /* Subject bytes */` |
|       - | 5949 | `	sxu32 nSubject,           /* Subject length */` |
|       - | 5950 | `	SySet *pSearch,           /* Collected search terms */` |
|       - | 5951 | `	SySet *pReplace,          /* Collected replacement terms */` |
|       - | 5952 | `	int rep_str,              /* TRUE: a single replacement reused for every search */` |
|       - | 5953 | `	ProcStringMatch xMatch,   /* SyBlobSearch (str_replace) / iPatternMatch (str_ireplace) */` |
|       - | 5954 | `	sxi64 *pnCount            /* Running replacement count (incremented here) */` |
|       - | 5955 | `	)` |
|       5 | 5956 | `{` |
|       - | 5957 | `	SyString *pSearch_,*pReplace_,sEmpty;` |
|       - | 5958 | `	sxi32 rc;` |
|   79969 | 5959 | `	SyBlobReset(pOut);` |
|   79969 | 5960 | `	if( nSubject > 0 ){` |
|   59497 | 5961 | `		rc = SyBlobAppend(pOut,(const void *)zSubject,nSubject);` |
|   59497 | 5962 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 5963 | `			return rc;` |
|       - | 5964 | `		}` |
|   29634 | 5965 | `	}` |
|   79969 | 5966 | `	SyStringInitFromBuf(&sEmpty,"",0);` |
|   79969 | 5967 | `	SySetResetCursor(pSearch);` |
|   79969 | 5968 | `	SySetResetCursor(pReplace);` |
|   79969 | 5969 | `	pSearch_ = pReplace_ = 0; /* cc warning */` |
|  164354 | 5970 | `	while( SXRET_OK == SySetGetNextEntry(pSearch,(void **)&pSearch_) ){` |
|       - | 5971 | `		sxu32 nCount,nOfft;` |
|   84390 | 5972 | `		if( rep_str ){` |
|       - | 5973 | `			/* Single replacement string reused for every search term */` |
|   80584 | 5974 | `			pReplace_ = (SyString *)SySetPeek(pReplace);` |
|   43980 | 5975 | `		}else if( SXRET_OK != SySetGetNextEntry(pReplace,(void **)&pReplace_) ){` |
|       - | 5976 | `			/* 'replace set' has fewer values than the search set: an empty` |
|       - | 5977 | `			 * string is used for the rest of the replacement values. */` |
|       5 | 5978 | `			pReplace_ = 0;` |
|       2 | 5979 | `		}` |
|   84390 | 5980 | `		if( pReplace_ == 0 ){` |
|       5 | 5981 | `			pReplace_ = &sEmpty;` |
|       2 | 5982 | `		}` |
|   84390 | 5983 | `		if( pSearch_->nByte < 1 ){` |
|       - | 5984 | `			/* php ignores an empty search string, but it still CONSUMED a replace` |
|       - | 5985 | `			 * slot above so the remaining pairs stay aligned. */` |
|      31 | 5986 | `			continue;` |
|       - | 5987 | `		}` |
|   84360 | 5988 | `		nOfft = nCount = 0;` |
|   48962 | 5989 | `		for(;;){` |
|   98339 | 5990 | `			if( nCount >= SyBlobLength(pOut) ){` |
|   21479 | 5991 | `				break;` |
|       - | 5992 | `			}` |
|       - | 5993 | `			/* Perform a pattern lookup */` |
|  115098 | 5994 | `			rc = xMatch(SyBlobDataAt(pOut,nCount),SyBlobLength(pOut) - nCount,` |
|   76860 | 5995 | `				(const void *)pSearch_->zString,pSearch_->nByte,&nOfft);` |
|   76865 | 5996 | `			if( rc != SXRET_OK ){` |
|       - | 5997 | `				/* Pattern not found */` |
|   62886 | 5998 | `				break;` |
|       - | 5999 | `			}` |
|       - | 6000 | `			/* Perform the replace operation */` |
|   20889 | 6001 | `			rc = StringReplace(pOut,nCount+nOfft,(int)pSearch_->nByte,` |
|   13979 | 6002 | `				pReplace_->zString,(int)pReplace_->nByte);` |
|   13984 | 6003 | `			if( rc != SXRET_OK ){` |
|       - | 6004 | `				/* Propagate an allocation failure so the caller raises a fatal` |
|       - | 6005 | `				 * instead of returning a partially-replaced result. */` |
|     ! 0 | 6006 | `				return rc;` |
|       - | 6007 | `			}` |
|   13984 | 6008 | `			*pnCount += 1;` |
|       - | 6009 | `			/* Increment offset counter */` |
|   13984 | 6010 | `			nCount += nOfft + pReplace_->nByte;` |
|       5 | 6011 | `		}` |
|       5 | 6012 | `	}` |
|   79969 | 6013 | `	return SXRET_OK;` |
|   39867 | 6014 | `}` |
|       - | 6015 | `/* Per-call state for the array-subject form of str_replace()/str_ireplace(). */` |
|       - | 6016 | `typedef struct str_replace_subject str_replace_subject;` |
|       - | 6017 | `struct str_replace_subject` |
|       - | 6018 | `{` |
|       - | 6019 | `	ph7_value *pResult;    /* Result array (keys preserved) */` |
|       - | 6020 | `	ph7_value *pScratch;   /* Reusable string value for each element */` |
|       - | 6021 | `	SyBlob *pWorker;       /* Scratch output buffer for one element */` |
|       - | 6022 | `	SySet *pSearch;        /* Collected search terms */` |
|       - | 6023 | `	SySet *pReplace;       /* Collected replacement terms */` |
|       - | 6024 | `	ProcStringMatch xMatch;/* Match routine (case-sensitive or not) */` |
|       - | 6025 | `	int rep_str;           /* TRUE: scalar $replace */` |
|       - | 6026 | `	sxi64 nReplaced;       /* Replacements performed so far (&$count) */` |
|       - | 6027 | `	sxi32 rc;              /* SXRET_OK or SXERR_MEM */` |
|       - | 6028 | `	ph7_context *pCtx;     /* Call context (for the coercion's diagnostics) */` |
|       - | 6029 | `};` |
|       - | 6030 | `/*` |
|       - | 6031 | ` * ph7_array_walk() callback over an array $subject: string-cast one element, run` |
|       - | 6032 | ` * the search/replace over it, and insert the result under the element's original` |
|       - | 6033 | ` * key. The cast is php's USER-VISIBLE one: an int/float/bool/null spells itself` |
|       - | 6034 | `` * out, a nested array renders "Array" and warns `Array to string conversion`,`` |
|       - | 6035 | ` * and an object with no __toString() is php's catchable Error rather than the` |
|       - | 6036 | ` * literal "Object" PHL used to hand back.` |
|       - | 6037 | ` */` |
|      54 | 6038 | `static int StrReplaceSubjectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|       2 | 6039 | `{` |
|      56 | 6040 | `	str_replace_subject *pS = (str_replace_subject *)pUserData;` |
|       - | 6041 | `	const char *zSub;` |
|       - | 6042 | `	int nSub;` |
|      56 | 6043 | `	PH7_ValueToStringUVOnce(pS->pCtx,pData,&zSub,&nSub);` |
|      54 | 6044 | `	if( StrReplaceOneSubject(pS->pWorker,zSub,(sxu32)(nSub > 0 ? nSub : 0),` |
|      56 | 6045 | `			pS->pSearch,pS->pReplace,pS->rep_str,pS->xMatch,&pS->nReplaced) != SXRET_OK ){` |
|     ! 0 | 6046 | `		pS->rc = SXERR_MEM;` |
|     ! 0 | 6047 | `		return SXERR_ABORT;` |
|       - | 6048 | `	}` |
|       - | 6049 | `	/* Publish the transformed bytes as a string under the original key. */` |
|      56 | 6050 | `	ph7_value_reset_string_cursor(pS->pScratch);` |
|      54 | 6051 | `	if( SyBlobLength(pS->pWorker) > 0` |
|      53 | 6052 | `	 && ph7_value_string(pS->pScratch,(const char *)SyBlobData(pS->pWorker),` |
|      72 | 6053 | `			(int)SyBlobLength(pS->pWorker)) != SXRET_OK ){` |
|     ! 0 | 6054 | `		pS->rc = SXERR_MEM;` |
|     ! 0 | 6055 | `		return SXERR_ABORT;` |
|       - | 6056 | `	}` |
|      56 | 6057 | `	if( ph7_array_add_elem(pS->pResult,pKey,pS->pScratch) != SXRET_OK ){` |
|     ! 0 | 6058 | `		pS->rc = SXERR_MEM;` |
|     ! 0 | 6059 | `		return SXERR_ABORT;` |
|       - | 6060 | `	}` |
|      56 | 6061 | `	return PH7_OK;` |
|      29 | 6062 | `}` |
|       - | 6063 | `/*` |
|       - | 6064 | ` * Write str_replace()/str_ireplace()'s optional by-reference &$count out-param.` |
|       - | 6065 | ` * The call compiler auto-vivifies argument #4 for these two names` |
|       - | 6066 | ` * (GenStateByRefBuiltinMask in compile.c), so an undefined variable, an array` |
|       - | 6067 | ` * element and a property all arrive with a real slot to write through.` |
|       - | 6068 | ` */` |
|   79948 | 6069 | `static void StrReplaceStoreCount(ph7_context *pCtx,int nArg,ph7_value **apArg,sxi64 nReplaced)` |
|       5 | 6070 | `{` |
|       - | 6071 | `	ph7_value sCount;` |
|   79953 | 6072 | `	if( nArg < 4 ){` |
|   79927 | 6073 | `		return;` |
|       - | 6074 | `	}` |
|      27 | 6075 | `	PH7_MemObjInitFromInt(pCtx->pVm,&sCount,nReplaced);` |
|      27 | 6076 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[3],&sCount);` |
|      27 | 6077 | `	PH7_MemObjRelease(&sCount);` |
|   39859 | 6078 | `}` |
|       - | 6079 | `/*` |
|       - | 6080 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|       - | 6081 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|       - | 6082 | ` *  Replace all occurrences of the search string with the replacement string.` |
|       - | 6083 | ` * Parameters` |
|       - | 6084 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|       - | 6085 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|       - | 6086 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|       - | 6087 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|       - | 6088 | ` *  for every value of search. The converse would not make sense, though.` |
|       - | 6089 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|       - | 6090 | ` * $search` |
|       - | 6091 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|       - | 6092 | ` *  to designate multiple needles.` |
|       - | 6093 | ` * $replace` |
|       - | 6094 | ` *  The replacement value that replaces found search values. An array may be used` |
|       - | 6095 | ` *  to designate multiple replacements.` |
|       - | 6096 | ` * $subject` |
|       - | 6097 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|       - | 6098 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|       - | 6099 | ` *  of subject, and the return value is an array as well.` |
|       - | 6100 | ` * &$count` |
|       - | 6101 | ` *  If passed, this is set to the number of replacements performed — accumulated` |
|       - | 6102 | ` *  over every search term AND, for an array subject, over every element.` |
|       - | 6103 | ` * Return` |
|       - | 6104 | ` * This function returns a string or an array with the replaced values.` |
|       - | 6105 | ` */` |
|   79948 | 6106 | `PH7_PRIVATE int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6107 | `{` |
|       - | 6108 | `	SyString sTemp;` |
|       - | 6109 | `	ProcStringMatch xMatch;` |
|       - | 6110 | `	const char *zIn,*zFunc;` |
|       - | 6111 | `	str_replace_data sRep;` |
|       - | 6112 | `	SyBlob sWorker;` |
|       - | 6113 | `	SySet sReplace;` |
|       - | 6114 | `	SySet sSearch;` |
|       - | 6115 | `	sxi64 nReplaced;` |
|       - | 6116 | `	int rep_str;` |
|       - | 6117 | `	int nByte;` |
|       - | 6118 | `	sxi32 rc;` |
|   79953 | 6119 | `	if( nArg < 3 ){` |
|       - | 6120 | `		/* Missing/Invalid arguments,return null */` |
|     ! 0 | 6121 | `		ph7_result_null(pCtx);` |
|     ! 0 | 6122 | `		return PH7_OK;` |
|       - | 6123 | `	}` |
|       - | 6124 | `	/* Initialize fields */` |
|   79953 | 6125 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|   79953 | 6126 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|   79953 | 6127 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|   79953 | 6128 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|   79953 | 6129 | `	sRep.pCtx = pCtx;` |
|   79953 | 6130 | `	sRep.pCollector = &sSearch;` |
|   79953 | 6131 | `	rep_str = 0;` |
|   79953 | 6132 | `	nReplaced = 0;` |
|       - | 6133 | `	/* Collect the search term(s) — independent of the subject. */` |
|   79953 | 6134 | `	if( ph7_value_is_array(apArg[0]) ){` |
|    3238 | 6135 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|    1621 | 6136 | `	}else{` |
|   76720 | 6137 | `		PH7_ValueToStringUVOnce(pCtx,apArg[0],&zIn,&nByte);` |
|   76720 | 6138 | `		SyStringInitFromBuf(&sTemp,zIn,nByte > 0 ? nByte : 0);` |
|   76720 | 6139 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|       - | 6140 | `	}` |
|       - | 6141 | `	/* Collect the replacement term(s). */` |
|   79953 | 6142 | `	if( ph7_value_is_array(apArg[1]) ){` |
|    1655 | 6143 | `		sRep.pCollector = &sReplace;` |
|    1655 | 6144 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|     830 | 6145 | `	}else{` |
|   78303 | 6146 | `		PH7_ValueToStringUVOnce(pCtx,apArg[1],&zIn,&nByte);` |
|   78303 | 6147 | `		rep_str = 1;` |
|   78303 | 6148 | `		SyStringInitFromBuf(&sTemp,zIn,nByte > 0 ? nByte : 0);` |
|   78303 | 6149 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|       - | 6150 | `	}` |
|       - | 6151 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|   79953 | 6152 | `	if( sRep.rc != SXRET_OK ){` |
|     ! 0 | 6153 | `		SySetRelease(&sSearch);` |
|     ! 0 | 6154 | `		SySetRelease(&sReplace);` |
|     ! 0 | 6155 | `		SyBlobRelease(&sWorker);` |
|     ! 0 | 6156 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 6157 | `	}` |
|       - | 6158 | `	/* Pick the match routine by function name */` |
|   79953 | 6159 | `	zFunc = ph7_function_name(pCtx);` |
|   79953 | 6160 | `	xMatch = SyBlobSearch;` |
|   79953 | 6161 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|       - | 6162 | `		/* Case insensitive pattern match */` |
|      55 | 6163 | `		xMatch = iPatternMatch;` |
|      27 | 6164 | `	}` |
|   79953 | 6165 | `	if( ph7_value_is_array(apArg[2]) ){` |
|       - | 6166 | `		/* Array subject: replace element-wise and RETURN AN ARRAY whose keys` |
|       - | 6167 | `		 * mirror the subject's (php semantics). */` |
|       - | 6168 | `		str_replace_subject sSub;` |
|       - | 6169 | `		ph7_value *pResult,*pScratch;` |
|      40 | 6170 | `		pResult = ph7_context_new_array(pCtx);` |
|      40 | 6171 | `		pScratch = ph7_context_new_scalar(pCtx);` |
|      40 | 6172 | `		if( pResult == 0 \|\| pScratch == 0 ){` |
|     ! 0 | 6173 | `			SySetRelease(&sSearch);` |
|     ! 0 | 6174 | `			SySetRelease(&sReplace);` |
|     ! 0 | 6175 | `			SyBlobRelease(&sWorker);` |
|     ! 0 | 6176 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 6177 | `		}` |
|      40 | 6178 | `		ph7_value_string(pScratch,"",0); /* force string representation */` |
|      40 | 6179 | `		SyZero(&sSub,sizeof(sSub));` |
|      40 | 6180 | `		sSub.pCtx     = pCtx;` |
|      40 | 6181 | `		sSub.pResult  = pResult;` |
|      40 | 6182 | `		sSub.pScratch = pScratch;` |
|      40 | 6183 | `		sSub.pWorker  = &sWorker;` |
|      40 | 6184 | `		sSub.pSearch  = &sSearch;` |
|      40 | 6185 | `		sSub.pReplace = &sReplace;` |
|      40 | 6186 | `		sSub.xMatch   = xMatch;` |
|      40 | 6187 | `		sSub.rep_str  = rep_str;` |
|      40 | 6188 | `		ph7_array_walk(apArg[2],StrReplaceSubjectWalker,&sSub);` |
|      40 | 6189 | `		SySetRelease(&sSearch);` |
|      40 | 6190 | `		SySetRelease(&sReplace);` |
|      40 | 6191 | `		SyBlobRelease(&sWorker);` |
|      40 | 6192 | `		if( sSub.rc != SXRET_OK ){` |
|     ! 0 | 6193 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 6194 | `		}` |
|      40 | 6195 | `		ph7_result_value(pCtx,pResult);` |
|      40 | 6196 | `		StrReplaceStoreCount(pCtx,nArg,apArg,sSub.nReplaced);` |
|      40 | 6197 | `		return PH7_OK;` |
|       - | 6198 | `	}` |
|       - | 6199 | `	/* Scalar subject: run once and return a string. An empty subject yields the` |
|       - | 6200 | `	 * empty string, and a lone empty search term leaves the subject untouched —` |
|       - | 6201 | `	 * both fall out of StrReplaceOneSubject's empty-term skip. */` |
|   79915 | 6202 | `	PH7_ValueToStringUVOnce(pCtx,apArg[2],&zIn,&nByte);` |
|   79915 | 6203 | `	rc = StrReplaceOneSubject(&sWorker,zIn,(sxu32)(nByte > 0 ? nByte : 0),` |
|   39835 | 6204 | `		&sSearch,&sReplace,rep_str,xMatch,&nReplaced);` |
|   79915 | 6205 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 6206 | `		SySetRelease(&sSearch);` |
|     ! 0 | 6207 | `		SySetRelease(&sReplace);` |
|     ! 0 | 6208 | `		SyBlobRelease(&sWorker);` |
|     ! 0 | 6209 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 6210 | `	}` |
|   79915 | 6211 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|   79915 | 6212 | `	SySetRelease(&sSearch);` |
|   79915 | 6213 | `	SySetRelease(&sReplace);` |
|   79915 | 6214 | `	SyBlobRelease(&sWorker);` |
|   79915 | 6215 | `	if( rc != PH7_OK ){` |
|     ! 0 | 6216 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 6217 | `	}` |
|   79915 | 6218 | `	StrReplaceStoreCount(pCtx,nArg,apArg,nReplaced);` |
|   79915 | 6219 | `	return PH7_OK;` |
|   39859 | 6220 | `}` |
|       - | 6221 | `/*` |
|       - | 6222 | ` * strtr() array form: a single (key,value) pair copied out of the replace_pairs` |
|       - | 6223 | ` * array. The bytes are owned by a persistent pool (see strtr_collect) rather than` |
|       - | 6224 | ` * the transient walker values, which HashmapWalk releases after each callback, so` |
|       - | 6225 | ` * we store byte offsets into that pool instead of raw pointers.` |
|       - | 6226 | ` */` |
|       - | 6227 | `typedef struct strtr_entry strtr_entry;` |
|       - | 6228 | `struct strtr_entry` |
|       - | 6229 | `{` |
|       - | 6230 | `	sxu32 nKeyOfft; /* Offset of the search key inside the pool */` |
|       - | 6231 | `	sxu32 nKeyLen;  /* Length of the search key */` |
|       - | 6232 | `	sxu32 nValOfft; /* Offset of the replacement inside the pool */` |
|       - | 6233 | `	sxu32 nValLen;  /* Length of the replacement */` |
|       - | 6234 | `};` |
|       - | 6235 | `typedef struct strtr_collect strtr_collect;` |
|       - | 6236 | `struct strtr_collect` |
|       - | 6237 | `{` |
|       - | 6238 | `	SyBlob *pPool;  /* Byte pool holding copied key + value bytes */` |
|       - | 6239 | `	SySet  *pTable; /* Set of strtr_entry (parallel offsets into pPool) */` |
|       - | 6240 | `	sxi32   rc;     /* Carries an allocation failure (SXERR_MEM) out of the walker */` |
|       - | 6241 | `	sxi32   rcThrow;/* Captured coercion throw; the builtin propagates it */` |
|       - | 6242 | `	ph7_context *pCtx; /* Needed to warn about an empty key */` |
|       - | 6243 | `};` |
|       - | 6244 | `/*` |
|       - | 6245 | ` * Collect one replace_pairs entry into the persistent pool/offset table.` |
|       - | 6246 | ` * PHP coerces both the key and the value to string (an integer key becomes its` |
|       - | 6247 | ` * decimal form) and ignores an empty-string key.` |
|       - | 6248 | ` */` |
|      30 | 6249 | `static int StrtrCollectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|       2 | 6250 | `{` |
|      32 | 6251 | `	strtr_collect *pCol = (strtr_collect *)pUserData;` |
|       - | 6252 | `	const char *zKey,*zVal;` |
|       - | 6253 | `	strtr_entry sEnt;` |
|       - | 6254 | `	int nKey,nVal;` |
|      32 | 6255 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|      32 | 6256 | `	if( nKey < 1 ){` |
|       - | 6257 | `		/* PHP ignores an empty-string key, and warns that it did so. */` |
|       3 | 6258 | `		ph7_context_throw_error_format(pCol->pCtx,PH7_CTX_WARNING,` |
|       - | 6259 | `			"Ignoring replacement of empty string");` |
|       3 | 6260 | `		return PH7_OK;` |
|       - | 6261 | `	}` |
|       - | 6262 | `	/* php's cast of the REPLACEMENT is user-visible (the KEY's is not -- an array` |
|       - | 6263 | `	 * key is already an int or a string): a pair whose value is an array warns` |
|       - | 6264 | ``	 * `Array to string conversion` and translates to "Array", and one whose value`` |
|       - | 6265 | `	 * is an object with no __toString() is php's catchable Error, where PHL` |
|       - | 6266 | `	 * translated to the placeholder "Object". */` |
|       - | 6267 | `	{` |
|      30 | 6268 | `		sxi32 rcSv = PH7_ValueToStringUV(pCol->pCtx,pData,&zVal,&nVal);` |
|      30 | 6269 | `		if( rcSv != SXRET_OK ){` |
|       3 | 6270 | `			pCol->rcThrow = rcSv;` |
|       3 | 6271 | `			return SXERR_ABORT;` |
|       - | 6272 | `		}` |
|       - | 6273 | `	}` |
|      28 | 6274 | `	sEnt.nKeyOfft = SyBlobLength(pCol->pPool);` |
|      28 | 6275 | `	sEnt.nKeyLen  = (sxu32)nKey;` |
|      28 | 6276 | `	if( SyBlobAppend(pCol->pPool,(const void *)zKey,(sxu32)nKey) != SXRET_OK ){` |
|     ! 0 | 6277 | `		pCol->rc = SXERR_MEM;` |
|     ! 0 | 6278 | `		return SXERR_ABORT;` |
|       - | 6279 | `	}` |
|      28 | 6280 | `	sEnt.nValOfft = SyBlobLength(pCol->pPool);` |
|      28 | 6281 | `	sEnt.nValLen  = (sxu32)nVal;` |
|      28 | 6282 | `	if( nVal > 0 && SyBlobAppend(pCol->pPool,(const void *)zVal,(sxu32)nVal) != SXRET_OK ){` |
|     ! 0 | 6283 | `		pCol->rc = SXERR_MEM;` |
|     ! 0 | 6284 | `		return SXERR_ABORT;` |
|       - | 6285 | `	}` |
|      28 | 6286 | `	if( SySetPut(pCol->pTable,(const void *)&sEnt) != SXRET_OK ){` |
|     ! 0 | 6287 | `		pCol->rc = SXERR_MEM;` |
|     ! 0 | 6288 | `		return SXERR_ABORT;` |
|       - | 6289 | `	}` |
|      28 | 6290 | `	return PH7_OK;` |
|      17 | 6291 | `}` |
|       - | 6292 | `/*` |
|       - | 6293 | ` * string strtr(string $str,string $from,string $to)` |
|       - | 6294 | ` * string strtr(string $str,array $replace_pairs)` |
|       - | 6295 | ` *  Translate characters or replace substrings.` |
|       - | 6296 | ` * Parameters` |
|       - | 6297 | ` *  $str` |
|       - | 6298 | ` *  The string being translated.` |
|       - | 6299 | ` * $from` |
|       - | 6300 | ` *  The string being translated to to.` |
|       - | 6301 | ` * $to` |
|       - | 6302 | ` *  The string replacing from.` |
|       - | 6303 | ` * $replace_pairs` |
|       - | 6304 | ` *  The replace_pairs parameter may be used instead of to and` |
|       - | 6305 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|       - | 6306 | ` * Return` |
|       - | 6307 | ` *  The translated string.` |
|       - | 6308 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|       - | 6309 | ` */` |
|     269 | 6310 | `PH7_PRIVATE int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 6311 | `{` |
|       - | 6312 | `	const char *zIn;` |
|       - | 6313 | `	char zGiven[64];` |
|       - | 6314 | `	int nLen;` |
|     274 | 6315 | `	if( nArg < 1 ){` |
|       - | 6316 | `		/* Nothing to replace,return FALSE */` |
|     ! 0 | 6317 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 6318 | `		return PH7_OK;` |
|       - | 6319 | `	}` |
|       - | 6320 | `	/*` |
|       - | 6321 | `	 * php dispatches strtr() on ARITY between two overloads — strtr(string, array)` |
|       - | 6322 | ``	 * and strtr(string, string, string) — so $from's expected type is `array` with`` |
|       - | 6323 | ``	 * two arguments and `string` with three, and the stub's `array\|string` union is`` |
|       - | 6324 | `	 * a wording php itself never emits. One signature cannot express that, so the` |
|       - | 6325 | `	 * shared ZPP screen skips this builtin (azSelfChecked[] in vm_arg_check.c) and` |
|       - | 6326 | `	 * the dispatch happens here, in php's left-to-right argument order.` |
|       - | 6327 | `	 *` |
|       - | 6328 | `	 * Both directions used to pass silently: a 2-argument string $from` |
|       - | 6329 | `	 * (strtr("abc","ab")) returned the subject UNCHANGED, and a 3-argument array` |
|       - | 6330 | `	 * $from was likewise ignored — the caller got its input back as if it had been` |
|       - | 6331 | `	 * translated.` |
|       - | 6332 | `	 */` |
|     274 | 6333 | `	if( !PH7_ArgSatisfiesString(apArg[0]) ){` |
|       4 | 6334 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 6335 | `			"strtr(): Argument #1 ($string) must be of type string, %s given",` |
|       1 | 6336 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|       - | 6337 | `	}` |
|     272 | 6338 | `	if( nArg == 2 ){` |
|      38 | 6339 | `		if( !ph7_value_is_array(apArg[1]) ){` |
|      22 | 6340 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 6341 | `				"strtr(): Argument #2 ($from) must be of type array, %s given",` |
|      14 | 6342 | `				VmValueGivenName(apArg[1],zGiven,sizeof(zGiven)));` |
|       2 | 6343 | `		}` |
|     246 | 6344 | `	}else if( nArg > 2 ){` |
|     235 | 6345 | `		if( !PH7_ArgSatisfiesString(apArg[1]) ){` |
|      10 | 6346 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 6347 | `				"strtr(): Argument #2 ($from) must be of type string, %s given",` |
|       6 | 6348 | `				VmValueGivenName(apArg[1],zGiven,sizeof(zGiven)));` |
|       - | 6349 | `		}` |
|       - | 6350 | ``		/* $to is php's `string`, but a null one stays accepted (php coerces it to`` |
|       - | 6351 | `		 * "" with a deprecation, and both engines answer the subject unchanged). */` |
|     229 | 6352 | `		if( !ph7_value_is_null(apArg[2]) && !PH7_ArgSatisfiesString(apArg[2]) ){` |
|       4 | 6353 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 6354 | `				"strtr(): Argument #3 ($to) must be of type string, %s given",` |
|       2 | 6355 | `				VmValueGivenName(apArg[2],zGiven,sizeof(zGiven)));` |
|       - | 6356 | `		}` |
|     107 | 6357 | `	}` |
|     250 | 6358 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     250 | 6359 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|       - | 6360 | `		/* Invalid arguments */` |
|       5 | 6361 | `		ph7_result_string(pCtx,zIn,nLen);` |
|       5 | 6362 | `		return PH7_OK;` |
|       - | 6363 | `	}` |
|     255 | 6364 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|       - | 6365 | `		strtr_collect sCol;` |
|       - | 6366 | `		SyBlob sPool,sWorker;` |
|       - | 6367 | `		SySet sTable;` |
|       - | 6368 | `		const char *zPool;` |
|       - | 6369 | `		strtr_entry *pEnt;` |
|       - | 6370 | `		sxi32 rc;` |
|       - | 6371 | `		int i,iRun;` |
|       - | 6372 | `		/*` |
|       - | 6373 | `		 * PHP's array-form strtr is a single left-to-right pass over the subject:` |
|       - | 6374 | `		 * at every position it substitutes the LONGEST replace_pairs key that` |
|       - | 6375 | `		 * matches there, then advances past the key (replacements are never` |
|       - | 6376 | `		 * rescanned). It is not a sequential per-key global replace. First copy` |
|       - | 6377 | `		 * the pairs into a persistent pool, then run that scan.` |
|       - | 6378 | `		 */` |
|      22 | 6379 | `		SyBlobInit(&sPool,&pCtx->pVm->sAllocator);` |
|      22 | 6380 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|      22 | 6381 | `		SySetInit(&sTable,&pCtx->pVm->sAllocator,sizeof(strtr_entry));` |
|      22 | 6382 | `		sCol.pPool  = &sPool;` |
|      22 | 6383 | `		sCol.pTable = &sTable;` |
|      22 | 6384 | `		sCol.rc     = SXRET_OK;` |
|      22 | 6385 | `		sCol.rcThrow= SXRET_OK;` |
|      22 | 6386 | `		sCol.pCtx   = pCtx;` |
|      22 | 6387 | `		ph7_array_walk(apArg[1],StrtrCollectWalker,&sCol);` |
|      22 | 6388 | `		if( sCol.rcThrow != SXRET_OK ){` |
|       - | 6389 | `			/* A pair value that could not be coerced threw php's catchable Error:` |
|       - | 6390 | `			 * the call answers nothing at all. */` |
|       3 | 6391 | `			SyBlobRelease(&sPool);` |
|       3 | 6392 | `			SyBlobRelease(&sWorker);` |
|       3 | 6393 | `			SySetRelease(&sTable);` |
|       3 | 6394 | `			return sCol.rcThrow;` |
|       - | 6395 | `		}` |
|      20 | 6396 | `		if( sCol.rc != SXRET_OK ){` |
|       - | 6397 | `			/* Allocation failure while collecting the pairs: surface a fatal */` |
|     ! 0 | 6398 | `			SyBlobRelease(&sPool);` |
|     ! 0 | 6399 | `			SyBlobRelease(&sWorker);` |
|     ! 0 | 6400 | `			SySetRelease(&sTable);` |
|     ! 0 | 6401 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 6402 | `		}` |
|       - | 6403 | `		/* The pool is now stable, so offsets can be resolved against its base. */` |
|      20 | 6404 | `		zPool = (const char *)SyBlobData(&sPool);` |
|      20 | 6405 | `		rc = SXRET_OK;` |
|      20 | 6406 | `		iRun = 0; /* Start of the pending run of unmatched bytes copied verbatim. */` |
|      74 | 6407 | `		for( i = 0 ; i < nLen ; ){` |
|      56 | 6408 | `			strtr_entry *pBest = 0;` |
|      56 | 6409 | `			sxu32 nBest = 0;` |
|       - | 6410 | `			/* Pick the longest key that matches at the current position. */` |
|      56 | 6411 | `			SySetResetCursor(&sTable);` |
|     130 | 6412 | `			while( SXRET_OK == SySetGetNextEntry(&sTable,(void **)&pEnt) ){` |
|      74 | 6413 | `				if( pEnt->nKeyLen > nBest` |
|      69 | 6414 | `					&& pEnt->nKeyLen <= (sxu32)(nLen - i)` |
|      65 | 6415 | `					&& SyMemcmp(zPool + pEnt->nKeyOfft,zIn + i,pEnt->nKeyLen) == 0 ){` |
|      35 | 6416 | `					nBest = pEnt->nKeyLen;` |
|      35 | 6417 | `					pBest = pEnt;` |
|      17 | 6418 | `				}` |
|       2 | 6419 | `			}` |
|      56 | 6420 | `			if( pBest == 0 ){` |
|       - | 6421 | `				/* No key here: extend the literal run and copy it in one shot later. */` |
|      26 | 6422 | `				i++;` |
|      26 | 6423 | `				continue;` |
|       - | 6424 | `			}` |
|       - | 6425 | `			/* Flush the pending literal run, then the replacement. */` |
|      31 | 6426 | `			if( i > iRun ){` |
|       5 | 6427 | `				rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(i - iRun));` |
|       2 | 6428 | `			}` |
|      31 | 6429 | `			if( rc == SXRET_OK && pBest->nValLen > 0 ){` |
|      31 | 6430 | `				rc = SyBlobAppend(&sWorker,zPool + pBest->nValOfft,pBest->nValLen);` |
|      15 | 6431 | `			}` |
|      31 | 6432 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6433 | `				SyBlobRelease(&sPool);` |
|     ! 0 | 6434 | `				SyBlobRelease(&sWorker);` |
|     ! 0 | 6435 | `				SySetRelease(&sTable);` |
|     ! 0 | 6436 | `				return PH7_ContextMemoryError(pCtx);` |
|       - | 6437 | `			}` |
|      31 | 6438 | `			i += (int)pBest->nKeyLen;` |
|      31 | 6439 | `			iRun = i;` |
|       1 | 6440 | `		}` |
|       - | 6441 | `		/* Flush the trailing literal run. */` |
|      20 | 6442 | `		if( nLen > iRun ){` |
|      10 | 6443 | `			rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(nLen - iRun));` |
|      10 | 6444 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6445 | `				SyBlobRelease(&sPool);` |
|     ! 0 | 6446 | `				SyBlobRelease(&sWorker);` |
|     ! 0 | 6447 | `				SySetRelease(&sTable);` |
|     ! 0 | 6448 | `				return PH7_ContextMemoryError(pCtx);` |
|       - | 6449 | `			}` |
|       4 | 6450 | `		}` |
|       - | 6451 | `		/* All done, return the result string */` |
|      29 | 6452 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|      18 | 6453 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|       - | 6454 | `		/* Clean-up */` |
|      20 | 6455 | `		SyBlobRelease(&sPool);` |
|      20 | 6456 | `		SyBlobRelease(&sWorker);` |
|      20 | 6457 | `		SySetRelease(&sTable);` |
|      20 | 6458 | `		if( rc != PH7_OK ){` |
|     ! 0 | 6459 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 6460 | `		}` |
|      11 | 6461 | `	}else{` |
|       - | 6462 | `		int i,flen,tlen,c,iOfft;` |
|       - | 6463 | `		const char *zFrom,*zTo;` |
|     225 | 6464 | `		if( nArg < 3 ){` |
|       - | 6465 | `			/* Nothing to replace */` |
|     ! 0 | 6466 | `			ph7_result_string(pCtx,zIn,nLen);` |
|     ! 0 | 6467 | `			return PH7_OK;` |
|       - | 6468 | `		}` |
|       - | 6469 | `		/* Extract given arguments */` |
|     225 | 6470 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|     225 | 6471 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|     225 | 6472 | `		if( flen < 1 \|\| tlen < 1 ){` |
|       - | 6473 | `			/* Nothing to replace */` |
|     ! 0 | 6474 | `			ph7_result_string(pCtx,zIn,nLen);` |
|     ! 0 | 6475 | `			return PH7_OK;` |
|       - | 6476 | `		}` |
|       - | 6477 | `		/* Start the replace process */` |
|    7142 | 6478 | `		for( i = 0 ; i < nLen ; ++i ){` |
|    6921 | 6479 | `			c = zIn[i];` |
|    6921 | 6480 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|     664 | 6481 | `				if ( iOfft < tlen ){` |
|     664 | 6482 | `					c = zTo[iOfft];` |
|     481 | 6483 | `				}` |
|     481 | 6484 | `			}` |
|    6921 | 6485 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|       - | 6486 |  |
|    5142 | 6487 | `		}` |
|       - | 6488 | `	}` |
|     244 | 6489 | `	return PH7_OK;` |
|     135 | 6490 | `}` |
|       - | 6491 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|       - | 6492 |  |
