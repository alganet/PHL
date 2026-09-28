# src/ph7/builtin_string.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2746/3155 lines (87.04%)

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
|  540514 |   60 | `PH7_PRIVATE int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 |   61 | `{` |
|       - |   62 | `	const char *zSource;` |
|       - |   63 | `	int nSrcLen;` |
|       - |   64 | `	sxi64 iStart,iEnd;` |
|  540519 |   65 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"substr",1,"$string"); }` |
|  540519 |   66 | `	if( nArg < 2 ){` |
|       - |   67 | `		/* Arity is enforced at the call boundary; nothing sensible to return here. */` |
|     ! 0 |   68 | `		ph7_result_string(pCtx,"",0);` |
|     ! 0 |   69 | `		return PH7_OK;` |
|       - |   70 | `	}` |
|       - |   71 | `	/* Extract the target string */` |
|  540519 |   72 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|       - |   73 | `	/* Extract the offset */` |
|       - |   74 | `	{` |
|  540519 |   75 | `		sxi64 iTmp = 0;` |
|  540519 |   76 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"substr",2,"$offset","int",&iTmp);` |
|  540519 |   77 | `		if( rcArg != PH7_OK ){` |
|     ! 0 |   78 | `			return rcArg;` |
|       - |   79 | `		}` |
|  540519 |   80 | `		iStart = iTmp;` |
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
|  540519 |   92 | `	if( iStart < 0 ){` |
|   44399 |   93 | `		iStart += nSrcLen;` |
|   44399 |   94 | `		if( iStart < 0 ){` |
|       5 |   95 | `			iStart = 0;` |
|       7 |   96 | `		}` |
|  518322 |   97 | `	}else if( iStart > nSrcLen ){` |
|       7 |   98 | `		iStart = nSrcLen;` |
|       3 |   99 | `	}` |
|  540519 |  100 | `	iEnd = nSrcLen;` |
|  540519 |  101 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|  413235 |  102 | `		sxi64 iLen = 0;` |
|  413235 |  103 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],"substr",3,"$length","?int",&iLen);` |
|  413235 |  104 | `		if( rcArg != PH7_OK ){` |
|     ! 0 |  105 | `			return rcArg;` |
|       - |  106 | `		}` |
|  413235 |  107 | `		if( iLen < 0 ){` |
|   43269 |  108 | `			iEnd = (sxi64)nSrcLen + iLen;` |
|  391603 |  109 | `		}else if( iLen > (sxi64)nSrcLen - iStart ){` |
|   33849 |  110 | `			iEnd = nSrcLen;` |
|   16927 |  111 | `		}else{` |
|  336127 |  112 | `			iEnd = iStart + iLen;` |
|       - |  113 | `		}` |
|  206792 |  114 | `	}` |
|  540519 |  115 | `	if( iEnd < iStart ){` |
|       3 |  116 | `		iEnd = iStart;` |
|       1 |  117 | `	}` |
|  540519 |  118 | `	ph7_result_string(pCtx,&zSource[iStart],(int)(iEnd - iStart));` |
|  540519 |  119 | `	return PH7_OK;` |
|  270659 |  120 | `}` |
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
|     140 |  231 | `PH7_PRIVATE int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 |  232 | `{` |
|       - |  233 | `	const char *zText,*zPattern,*zEnd;` |
|       - |  234 | `	int nTextlen,nPatlen;` |
|     142 |  235 | `	int iCount = 0;` |
|       - |  236 | `	sxu32 nOfft;` |
|       - |  237 | `	sxi32 rc;` |
|     142 |  238 | `	if( nArg < 2 ){` |
|       - |  239 | `		/* Missing arguments */` |
|     ! 0 |  240 | `		ph7_result_int(pCtx,0);` |
|     ! 0 |  241 | `		return PH7_OK;` |
|       - |  242 | `	}` |
|       - |  243 | `	/* Point to the haystack */` |
|     142 |  244 | `	zText = ph7_value_to_string(apArg[0],&nTextlen);` |
|       - |  245 | `	/* Point to the neddle */` |
|     142 |  246 | `	zPattern = ph7_value_to_string(apArg[1],&nPatlen);` |
|     142 |  247 | `	if( nPatlen < 1 ){` |
|       - |  248 | `		/* Empty needle: PHP 8 throws a catchable ValueError. */` |
|       3 |  249 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - |  250 | `			"substr_count(): Argument #2 ($needle) must not be empty");` |
|       - |  251 | `	}` |
|       - |  252 | `	/* Apply the optional $offset/$length window before searching. PHP 8 validates` |
|       - |  253 | `	 * both against the haystack (a negative value counts from the end) and throws a` |
|       - |  254 | `	 * catchable ValueError when the result falls outside it — this happens before the` |
|       - |  255 | `	 * needle-fits check, so it fires even when the needle is longer than the haystack. */` |
|     140 |  256 | `	if( nArg > 2 ){` |
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
|     138 |  269 | `	if( nArg > 3 ){` |
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
|     134 |  281 | `	if( nTextlen < 1 \|\| nPatlen > nTextlen ){` |
|       - |  282 | `		/* The windowed haystack can't contain the needle: zero matches */` |
|       3 |  283 | `		ph7_result_int(pCtx,0);` |
|       3 |  284 | `		return PH7_OK;` |
|       - |  285 | `	}` |
|       - |  286 | `	/* Point to the end of the windowed haystack */` |
|     132 |  287 | `	zEnd = &zText[nTextlen];` |
|       - |  288 | `	/* Perform the search */` |
|     106 |  289 | `	for(;;){` |
|     214 |  290 | `		rc = SyBlobSearch((const void *)zText,(sxu32)(zEnd-zText),(const void *)zPattern,nPatlen,&nOfft);` |
|     214 |  291 | `		if( rc != SXRET_OK ){` |
|       - |  292 | `			/* Pattern not found,break immediately */` |
|     108 |  293 | `			break;` |
|       - |  294 | `		}` |
|       - |  295 | `		/* Increment counter and update the offset */` |
|     108 |  296 | `		iCount++;` |
|     108 |  297 | `		zText += nOfft + nPatlen;` |
|     108 |  298 | `		if( zText >= zEnd ){` |
|      26 |  299 | `			break;` |
|       - |  300 | `		}` |
|       2 |  301 | `	}` |
|       - |  302 | `	/* Pattern count */` |
|     132 |  303 | `	ph7_result_int(pCtx,iCount);` |
|     132 |  304 | `	return PH7_OK;` |
|      72 |  305 | `}` |
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
|  791193 |  323 | `static void StrNullArgNotice(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNum,const char *zParamName)` |
|       5 |  324 | `{` |
|  791198 |  325 | `	if( ph7_value_is_null(pArg) ){` |
|     ! 0 |  326 | `		PH7_VmThrowException(pCtx,"TypeError",` |
|       - |  327 | `			"%s(): Argument #%d (%s) must be of type string, null given",` |
|     ! 0 |  328 | `			zFunc,iArgNum,zParamName);` |
|     ! 0 |  329 | `	}` |
|  791198 |  330 | `}` |
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
|       3 |  658 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pMap->pFirst->nValIdx);` |
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
|       - |  858 | `	const char *zStr1,*zStr2;` |
|       - |  859 | `	ph7_value sTmp1,sTmp2;` |
|       - |  860 | `	int nLen1,nLen2;` |
|       - |  861 | `	int nSim;` |
|       - |  862 | `	sxi32 rc;` |
|      25 |  863 | `	if( nArg < 2 ){` |
|     ! 0 |  864 | `		return PH7_VmThrowException(pCtx,` |
|       - |  865 | `			"ArgumentCountError",` |
|       - |  866 | `			"similar_text() expects at least 2 arguments, %d given",` |
|     ! 0 |  867 | `			nArg` |
|       - |  868 | `			);` |
|       - |  869 | `	}` |
|      25 |  870 | `	PH7_MemObjInit(pCtx->pVm,&sTmp1);` |
|      25 |  871 | `	PH7_MemObjInit(pCtx->pVm,&sTmp2);` |
|      25 |  872 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"similar_text",1,"$string1","string",` |
|       - |  873 | `		"similar_text(): Passing null to parameter #1 ($string1) "` |
|       - |  874 | `		"of type string is deprecated",` |
|       - |  875 | `		&sTmp1,&zStr1,&nLen1);` |
|      25 |  876 | `	if( rc != PH7_OK ) goto out;` |
|      25 |  877 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"similar_text",2,"$string2","string",` |
|       - |  878 | `		"similar_text(): Passing null to parameter #2 ($string2) "` |
|       - |  879 | `		"of type string is deprecated",` |
|       - |  880 | `		&sTmp2,&zStr2,&nLen2);` |
|      25 |  881 | `	if( rc != PH7_OK ) goto out;` |
|      25 |  882 | `	if( nLen1 + nLen2 == 0 ){` |
|       5 |  883 | `		nSim = 0;` |
|       3 |  884 | `	}else{` |
|      21 |  885 | `		nSim = SimilarChar(zStr1,nLen1,zStr2,nLen2);` |
|       - |  886 | `	}` |
|      25 |  887 | `	if( nArg > 2 ){` |
|       - |  888 | `		/* Write the percentage through the by-ref out-param */` |
|       9 |  889 | `		ph7_value *pPercent = ph7_context_new_scalar(pCtx);` |
|       9 |  890 | `		if( pPercent == 0 ){` |
|     ! 0 |  891 | `			rc = PH7_ContextMemoryError(pCtx);` |
|     ! 0 |  892 | `			goto out;` |
|     ! 0 |  893 | `		}else{` |
|       9 |  894 | `			double dPct = (nLen1 + nLen2 == 0) ? 0.0 : (double)nSim * 200.0 / (double)(nLen1 + nLen2);` |
|       9 |  895 | `			ph7_value_double(pPercent,dPct);` |
|       9 |  896 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],pPercent);` |
|       - |  897 | `		}` |
|       4 |  898 | `	}` |
|      25 |  899 | `	ph7_result_int(pCtx,nSim);` |
|      25 |  900 | `	rc = PH7_OK;` |
|      12 |  901 | `out:` |
|      25 |  902 | `	PH7_MemObjRelease(&sTmp1);` |
|      25 |  903 | `	PH7_MemObjRelease(&sTmp2);` |
|      25 |  904 | `	return rc;` |
|      13 |  905 | `}` |
|       - |  906 | `/*` |
|       - |  907 | ` * array\|int str_word_count(string $string[,int $format = 0[,?string $characters = null]])` |
|       - |  908 | ` *  Count (or return) the words inside a string. A word is a run of alphabetic` |
|       - |  909 | ` *  characters, which may contain (but not start the string with) "'" and "-";` |
|       - |  910 | ` *  $characters adds extra bytes to the word set ("a..z" ranges supported, as` |
|       - |  911 | ` *  in PHP's php_charmask).` |
|       - |  912 | ` *  $format: 0 -> word count, 1 -> array of words, 2 -> array of words keyed` |
|       - |  913 | ` *  by their byte position in $string.` |
|       - |  914 | ` * Errors` |
|       - |  915 | ` *  ValueError when $format is not 0, 1 or 2.` |
|       - |  916 | ` */` |
|      42 |  917 | `PH7_PRIVATE int PH7_builtin_str_word_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 |  918 | `{` |
|       - |  919 | `	const char *zIn,*zEnd,*zPtr;` |
|      43 |  920 | `	ph7_value *pArray = 0,*pValue = 0;` |
|       - |  921 | `	ph7_value sTmp,sListTmp;` |
|       - |  922 | `	char aMask[256];` |
|      43 |  923 | `	int bMask = 0;` |
|      43 |  924 | `	int iFormat = 0;` |
|      43 |  925 | `	int nCount = 0;` |
|       - |  926 | `	int nLen;` |
|       - |  927 | `	sxi32 rc;` |
|      43 |  928 | `	if( nArg < 1 ){` |
|     ! 0 |  929 | `		return PH7_VmThrowException(pCtx,` |
|       - |  930 | `			"ArgumentCountError",` |
|       - |  931 | `			"str_word_count() expects at least 1 argument, %d given",` |
|     ! 0 |  932 | `			nArg` |
|       - |  933 | `			);` |
|       - |  934 | `	}` |
|      43 |  935 | `	PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|      43 |  936 | `	PH7_MemObjInit(pCtx->pVm,&sListTmp);` |
|      43 |  937 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_word_count",1,"$string","string",` |
|       - |  938 | `		"str_word_count(): Passing null to parameter #1 ($string) "` |
|       - |  939 | `		"of type string is deprecated",` |
|       - |  940 | `		&sTmp,&zIn,&nLen);` |
|      43 |  941 | `	if( rc != PH7_OK ) goto out;` |
|      43 |  942 | `	if( nArg > 1 ){` |
|       - |  943 | `		sxi64 iVal;` |
|      29 |  944 | `		rc = PH7_IntArgResolve(pCtx,apArg[1],"str_word_count",2,"$format","int",&iVal);` |
|      31 |  945 | `		if( rc != PH7_OK ) goto out;` |
|      29 |  946 | `		if( iVal < 0 \|\| iVal > 2 ){` |
|       5 |  947 | `			rc = PH7_VmThrowException(pCtx,` |
|       - |  948 | `				"ValueError",` |
|       - |  949 | `				"str_word_count(): Argument #2 ($format) must be a valid format value"` |
|       - |  950 | `				);` |
|       5 |  951 | `			goto out;` |
|       - |  952 | `		}` |
|      25 |  953 | `		iFormat = (int)iVal;` |
|      12 |  954 | `	}` |
|      39 |  955 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|       - |  956 | `		/* $characters is ?string: null (skipped above) simply keeps the` |
|       - |  957 | `		 * default word set, no deprecation. */` |
|       - |  958 | `		const char *zList;` |
|       - |  959 | `		int nList;` |
|      13 |  960 | `		rc = StrPredicateResolveArg(pCtx,apArg[2],"str_word_count",3,"$characters","?string",` |
|       - |  961 | `			"" /* unreachable: null never gets here */,` |
|       - |  962 | `			&sListTmp,&zList,&nList);` |
|      13 |  963 | `		if( rc != PH7_OK ) goto out;` |
|      13 |  964 | `		PH7_BuildCharMask(pCtx,zList,nList,aMask);` |
|      13 |  965 | `		bMask = 1;` |
|       6 |  966 | `	}` |
|      39 |  967 | `	if( iFormat != 0 ){` |
|      25 |  968 | `		pArray = ph7_context_new_array(pCtx);` |
|      25 |  969 | `		pValue = ph7_context_new_scalar(pCtx);` |
|      25 |  970 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|     ! 0 |  971 | `			rc = PH7_ContextMemoryError(pCtx);` |
|     ! 0 |  972 | `			goto out;` |
|       - |  973 | `		}` |
|      12 |  974 | `	}` |
|      39 |  975 | `	zPtr = zIn;` |
|      39 |  976 | `	zEnd = &zIn[nLen];` |
|      39 |  977 | `	if( nLen > 0 ){` |
|       - |  978 | `		/* php: the string's first byte cannot be ' or -, and its last byte` |
|       - |  979 | `		 * cannot be -, unless the charlist explicitly allows them. */` |
|      33 |  980 | `		if( (zPtr[0] == '\'' && (!bMask \|\| !aMask[(unsigned char)'\''])) \|\|` |
|      28 |  981 | `			(zPtr[0] == '-'  && (!bMask \|\| !aMask[(unsigned char)'-'])) ){` |
|       9 |  982 | `			zPtr++;` |
|       4 |  983 | `		}` |
|      33 |  984 | `		if( zEnd[-1] == '-' && (!bMask \|\| !aMask[(unsigned char)'-']) ){` |
|       9 |  985 | `			zEnd--;` |
|       4 |  986 | `		}` |
|      16 |  987 | `	}` |
|     135 |  988 | `	while( zPtr < zEnd ){` |
|      91 |  989 | `		const char *zStart = zPtr;` |
|     477 |  990 | `		while( zPtr < zEnd && ( SyisAlpha((unsigned char)zPtr[0])` |
|     253 |  991 | `			\|\| (bMask && aMask[(unsigned char)zPtr[0]])` |
|      98 |  992 | `			\|\| zPtr[0] == '\'' \|\| zPtr[0] == '-' ) ){` |
|     339 |  993 | `			zPtr++;` |
|       1 |  994 | `		}` |
|      97 |  995 | `		if( zPtr > zStart ){` |
|      91 |  996 | `			if( iFormat == 0 ){` |
|      19 |  997 | `				nCount++;` |
|      10 |  998 | `			}else{` |
|      73 |  999 | `				ph7_value_reset_string_cursor(pValue);` |
|      73 | 1000 | `				if( SXRET_OK != ph7_value_string(pValue,zStart,(int)(zPtr-zStart)) ){` |
|     ! 0 | 1001 | `					rc = PH7_ContextMemoryError(pCtx);` |
|     ! 0 | 1002 | `					goto out;` |
|       - | 1003 | `				}` |
|      73 | 1004 | `				if( iFormat == 1 ){` |
|      59 | 1005 | `					if( SXRET_OK != ph7_array_add_elem(pArray,0,pValue) ){` |
|     ! 0 | 1006 | `						rc = PH7_ContextMemoryError(pCtx);` |
|     ! 0 | 1007 | `						goto out;` |
|       - | 1008 | `					}` |
|      30 | 1009 | `				}else{` |
|      15 | 1010 | `					if( SXRET_OK != ph7_array_add_intkey_elem(pArray,(int)(zStart-zIn),pValue) ){` |
|     ! 0 | 1011 | `						rc = PH7_ContextMemoryError(pCtx);` |
|     ! 0 | 1012 | `						goto out;` |
|       - | 1013 | `					}` |
|       - | 1014 | `				}` |
|       - | 1015 | `			}` |
|      45 | 1016 | `		}` |
|      97 | 1017 | `		zPtr++;` |
|       1 | 1018 | `	}` |
|      37 | 1019 | `	if( iFormat == 0 ){` |
|      13 | 1020 | `		ph7_result_int(pCtx,nCount);` |
|       7 | 1021 | `	}else{` |
|      25 | 1022 | `		ph7_result_value(pCtx,pArray);` |
|       - | 1023 | `	}` |
|      37 | 1024 | `	rc = PH7_OK;` |
|      20 | 1025 | `out:` |
|      41 | 1026 | `	PH7_MemObjRelease(&sTmp);` |
|      41 | 1027 | `	PH7_MemObjRelease(&sListTmp);` |
|      41 | 1028 | `	return rc;` |
|      21 | 1029 | `}` |
|       - | 1030 | `/*` |
|       - | 1031 | ` * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])` |
|       - | 1032 | ` *   Split a string into smaller chunks.` |
|       - | 1033 | ` * Parameters` |
|       - | 1034 | ` *  $body` |
|       - | 1035 | ` *   The string to be chunked.` |
|       - | 1036 | ` * $chunklen` |
|       - | 1037 | ` *   The chunk length.` |
|       - | 1038 | ` * $end` |
|       - | 1039 | ` *   The line ending sequence.` |
|       - | 1040 | ` * Return` |
|       - | 1041 | ` *  The chunked string or NULL on failure.` |
|       - | 1042 | ` */` |
|      14 | 1043 | `PH7_PRIVATE int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1044 | `{` |
|      15 | 1045 | `	const char *zIn,*zEnd,*zSep = "\r\n";` |
|       - | 1046 | `	int nSepLen,nChunkLen,nLen;` |
|       - | 1047 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|       - | 1048 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|      15 | 1049 | `	if( nArg < 1 ){` |
|       - | 1050 | `		/* Nothing to split,return null */` |
|     ! 0 | 1051 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1052 | `		return PH7_OK;` |
|       - | 1053 | `	}` |
|       - | 1054 | `	/* initialize/Extract arguments */` |
|      15 | 1055 | `	nSepLen = (int)sizeof("\r\n") - 1;` |
|      15 | 1056 | `	nChunkLen = 76;` |
|      15 | 1057 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      15 | 1058 | `	zEnd = &zIn[nLen];` |
|      15 | 1059 | `	if( nArg > 1 ){` |
|       - | 1060 | `		/* Chunk length */` |
|      13 | 1061 | `		nChunkLen = ph7_value_to_int(apArg[1]);` |
|      13 | 1062 | `		if( nChunkLen < 1 ){` |
|       - | 1063 | `			/* PHP 8 throws a catchable ValueError for a non-positive length. */` |
|       3 | 1064 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1065 | `				"chunk_split(): Argument #2 ($length) must be greater than 0");` |
|       - | 1066 | `		}` |
|      11 | 1067 | `		if( nArg > 2 ){` |
|       - | 1068 | `			/* Separator */` |
|       9 | 1069 | `			zSep = ph7_value_to_string(apArg[2],&nSepLen);` |
|       9 | 1070 | `			if( nSepLen < 1 ){` |
|       - | 1071 | `				/* Switch back to the default separator */` |
|       3 | 1072 | `				zSep = "\r\n";` |
|       3 | 1073 | `				nSepLen = (int)sizeof("\r\n") - 1;` |
|       1 | 1074 | `			}` |
|       4 | 1075 | `		}` |
|       5 | 1076 | `	}` |
|       - | 1077 | `	/* Perform the requested operation */` |
|      13 | 1078 | `	if( nChunkLen > nLen ){` |
|       - | 1079 | `		/* Nothing to split,return the string and the separator */` |
|       9 | 1080 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);` |
|       9 | 1081 | `		return PH7_OK;` |
|       - | 1082 | `	}` |
|      17 | 1083 | `	while( zIn < zEnd ){` |
|      13 | 1084 | `		if( nChunkLen > (int)(zEnd-zIn) ){` |
|       3 | 1085 | `			nChunkLen = (int)(zEnd - zIn);` |
|       1 | 1086 | `		}` |
|       - | 1087 | `		/* Append the chunk and the separator */` |
|      13 | 1088 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);` |
|       - | 1089 | `		/* Point beyond the chunk */` |
|      13 | 1090 | `		zIn += nChunkLen;` |
|       1 | 1091 | `	}` |
|       5 | 1092 | `	return PH7_OK;` |
|       8 | 1093 | `}` |
|       - | 1094 | `/*` |
|       - | 1095 | ` * string addslashes(string $str)` |
|       - | 1096 | ` *  Quote string with slashes.` |
|       - | 1097 | ` *  Returns a string with backslashes before characters that need` |
|       - | 1098 | ` *  to be quoted in database queries etc. These characters are single` |
|       - | 1099 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|       - | 1100 | ` * Parameter` |
|       - | 1101 | ` *  str: The string to be escaped.` |
|       - | 1102 | ` * Return` |
|       - | 1103 | ` *  Returns the escaped string` |
|       - | 1104 | ` */` |
|      18 | 1105 | `PH7_PRIVATE int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1106 | `{` |
|       - | 1107 | `	const char *zCur,*zIn,*zEnd;` |
|       - | 1108 | `	int nLen;` |
|       - | 1109 | `	/* PHP enforces exactly one argument. */` |
|      20 | 1110 | `	if( nArg != 1 ){` |
|     ! 0 | 1111 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1112 | `			"ArgumentCountError",` |
|       - | 1113 | `			"addslashes() expects exactly 1 argument, %d given",` |
|     ! 0 | 1114 | `			nArg` |
|       - | 1115 | `			);` |
|       - | 1116 | `	}` |
|       - | 1117 | `	/* php only DEPRECATES null here; PHL rejects it. */` |
|      20 | 1118 | `	if( ph7_value_is_null(apArg[0]) ){` |
|     ! 0 | 1119 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 1120 | `			"addslashes(): Argument #1 ($string) must be of type string, null given"` |
|       - | 1121 | `			);` |
|       - | 1122 | `	}` |
|       - | 1123 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|      27 | 1124 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|      29 | 1125 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|      18 | 1126 | `	    ph7_value_is_resource(apArg[0]) ){` |
|     ! 0 | 1127 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1128 | `			"TypeError",` |
|       - | 1129 | `			"addslashes(): Argument #1 ($string) must be of type string, %s given",` |
|     ! 0 | 1130 | `			ph7_type_name(apArg[0])` |
|       - | 1131 | `			);` |
|       - | 1132 | `	}` |
|       - | 1133 | `	/* Convert to string representation first and obtain length. */` |
|      20 | 1134 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      20 | 1135 | `	if( nLen < 1 ){` |
|       - | 1136 | `		/* Return the empty string */` |
|       6 | 1137 | `		ph7_result_string(pCtx,"",0);` |
|       6 | 1138 | `		return PH7_OK;` |
|       - | 1139 | `	}` |
|      15 | 1140 | `	zEnd = &zIn[nLen];` |
|      15 | 1141 | `	zCur = 0; /* cc warning */` |
|      20 | 1142 | `	for(;;){` |
|      41 | 1143 | `		if( zIn >= zEnd ){` |
|       - | 1144 | `			/* No more input */` |
|      15 | 1145 | `			break;` |
|       - | 1146 | `		}` |
|      27 | 1147 | `		zCur = zIn;` |
|       - | 1148 | `		/* scan until a character that needs escaping (', ", \\, or NUL) */` |
|      89 | 1149 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' && zIn[0] != '\0' ){` |
|      63 | 1150 | `			zIn++;` |
|       1 | 1151 | `		}` |
|      27 | 1152 | `		if( zIn > zCur ){` |
|       - | 1153 | `			/* Append raw contents */` |
|      23 | 1154 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      11 | 1155 | `		}` |
|      27 | 1156 | `		if( zIn < zEnd ){` |
|      17 | 1157 | `			int c = zIn[0];` |
|      17 | 1158 | `			if( c == '\0' ){` |
|       - | 1159 | `				/* PHP escapes NUL as "\\0" (two characters) */` |
|       3 | 1160 | `				ph7_result_string(pCtx,"\\0",2);` |
|       2 | 1161 | `			}else{` |
|      15 | 1162 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|       - | 1163 | `			}` |
|       8 | 1164 | `		}` |
|      27 | 1165 | `		zIn++;` |
|       1 | 1166 | `	}` |
|      15 | 1167 | `	return PH7_OK;` |
|      11 | 1168 | `}` |
|       - | 1169 | `/*` |
|       - | 1170 | ``  * Build a 256-entry membership mask from a PHP charlist, expanding `a..z` `` |
|       - | 1171 | ` * byte ranges exactly like PHP's php_charmask(). On return aMask[c] != 0 iff` |
|       - | 1172 | ` * the byte c belongs to the set. Emits the PHP-exact warnings for the three` |
|       - | 1173 | ` * malformed-range shapes (ph7_context_throw_error_format prepends the active` |
|       - | 1174 | ` * function name, so the messages omit it); on a bad range the surrounding` |
|       - | 1175 | ` * bytes are still added and the scan never aborts. Reads only within` |
|       - | 1176 | ` * [zList, zList+nLen).` |
|       - | 1177 | ` *` |
|       - | 1178 | ` * Use ONLY for the builtins whose charlist expands ranges the way PHP's` |
|       - | 1179 | ` * php_charmask() does: trim/ltrim/rtrim/addcslashes (and quotemeta, whose set` |
|       - | 1180 | ` * is a fixed literal with no ".."). Do NOT route strspn/strcspn/strtok/strpbrk` |
|       - | 1181 | ` * through this — PHP treats their charlists literally, so expanding "a..z" here` |
|       - | 1182 | ` * would be a behavior regression plus spurious "Invalid '..'-range" warnings.` |
|       - | 1183 | ` */` |
|    1212 | 1184 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256])` |
|       5 | 1185 | `{` |
|    1217 | 1186 | `	const unsigned char *zIn  = (const unsigned char *)zList;` |
|    1217 | 1187 | `	const unsigned char *zEnd = zIn + (nLen > 0 ? nLen : 0);` |
|    1217 | 1188 | `	SyZero(aMask,256);` |
|    3003 | 1189 | `	for( ; zIn < zEnd ; zIn++ ){` |
|    1791 | 1190 | `		int c = zIn[0];` |
|    1791 | 1191 | `		if( zIn + 3 < zEnd && zIn[1] == '.' && zIn[2] == '.' && zIn[3] >= c ){` |
|       - | 1192 | `			/* Valid incrementing range c..zIn[3] */` |
|     230 | 1193 | `			int hi = zIn[3],k;` |
|    7186 | 1194 | `			for( k = c ; k <= hi ; k++ ){` |
|    6960 | 1195 | `				aMask[k] = 1;` |
|    3482 | 1196 | `			}` |
|     230 | 1197 | `			zIn += 3; /* the loop's ++ then steps past the range end */` |
|    1690 | 1198 | `		}else if( zIn + 1 < zEnd && zIn[0] == '.' && zIn[1] == '.' ){` |
|       - | 1199 | `			/* Malformed range: mirror php_charmask's three diagnostics. */` |
|       - | 1200 | `			const char *zMsg;` |
|      27 | 1201 | `			if( (const unsigned char *)zList >= zIn ){` |
|       6 | 1202 | `				zMsg = "no character to the left of '..'";` |
|      25 | 1203 | `			}else if( zIn + 2 >= zEnd ){` |
|       6 | 1204 | `				zMsg = "no character to the right of '..'";` |
|      21 | 1205 | `			}else if( zIn[-1] > zIn[2] ){` |
|      19 | 1206 | `				zMsg = "'..'-range needs to be incrementing";` |
|      11 | 1207 | `			}else{` |
|     ! 0 | 1208 | `				zMsg = 0; /* catch-all (e.g. a..b..c) */` |
|       - | 1209 | `			}` |
|      27 | 1210 | `			if( zMsg ){` |
|      39 | 1211 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      12 | 1212 | `					"Invalid '..'-range, %s",zMsg);` |
|      15 | 1213 | `			}else{` |
|     ! 0 | 1214 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|       - | 1215 | `					"Invalid '..'-range");` |
|       - | 1216 | `			}` |
|       - | 1217 | `			/* Do not consume the dots: the loop's ++ steps one byte so the` |
|       - | 1218 | `			 * dots are re-scanned as literals, exactly like php_charmask. */` |
|      15 | 1219 | `		}else{` |
|    1541 | 1220 | `			aMask[c] = 1;` |
|       - | 1221 | `		}` |
|     898 | 1222 | `	}` |
|    1217 | 1223 | `}` |
|       - | 1224 | `/*` |
|       - | 1225 | ` * string addcslashes(string $str,string $charlist)` |
|       - | 1226 | ` *  Quote string with slashes in a C style.` |
|       - | 1227 | ` * Parameter` |
|       - | 1228 | ` *  $str:` |
|       - | 1229 | ` *    The string to be escaped.` |
|       - | 1230 | ` *  $charlist:` |
|       - | 1231 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|       - | 1232 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|       - | 1233 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|       - | 1234 | ` * Return` |
|       - | 1235 | ` *  Returns the escaped string.` |
|       - | 1236 | ` * Note:` |
|       - | 1237 | ` *  Character ranges [i.e: 'A..Z'] are supported (see PH7_BuildCharMask).` |
|       - | 1238 | ` */` |
|     242 | 1239 | `PH7_PRIVATE int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 1240 | `{` |
|       - | 1241 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|       - | 1242 | `	char aMask[256];` |
|       - | 1243 | `	int nLen,nMask;` |
|       - | 1244 | `	/* PHP enforces exactly two arguments. */` |
|     246 | 1245 | `	if( nArg != 2 ){` |
|     ! 0 | 1246 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1247 | `			"ArgumentCountError",` |
|       - | 1248 | `			"addcslashes() expects exactly 2 arguments, %d given",` |
|     ! 0 | 1249 | `			nArg` |
|       - | 1250 | `			);` |
|       - | 1251 | `	}` |
|       - | 1252 | `	/* php only DEPRECATES null here; PHL rejects it. */` |
|     246 | 1253 | `	if( ph7_value_is_null(apArg[0]) ){` |
|     ! 0 | 1254 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1255 | `			"TypeError",` |
|       - | 1256 | `			"addcslashes(): Argument #1 ($string) must be of type string, null given"` |
|       - | 1257 | `			);` |
|     363 | 1258 | `	} else if( ph7_value_is_array(apArg[0]) \|\|` |
|     367 | 1259 | `	          ph7_value_is_object(apArg[0]) \|\|` |
|     242 | 1260 | `	          ph7_value_is_resource(apArg[0]) ){` |
|     ! 0 | 1261 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1262 | `			"TypeError",` |
|       - | 1263 | `			"addcslashes(): Argument #1 ($string) must be of type string, %s given",` |
|     ! 0 | 1264 | `			ph7_type_name(apArg[0])` |
|       - | 1265 | `			);` |
|       - | 1266 | `	}` |
|       - | 1267 | `	/* php only DEPRECATES null here; PHL rejects it. */` |
|     246 | 1268 | `	if( ph7_value_is_null(apArg[1]) ){` |
|     ! 0 | 1269 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1270 | `			"TypeError",` |
|       - | 1271 | `			"addcslashes(): Argument #2 ($characters) must be of type string, null given"` |
|       - | 1272 | `			);` |
|     363 | 1273 | `	} else if( ph7_value_is_array(apArg[1]) \|\|` |
|     367 | 1274 | `	          ph7_value_is_object(apArg[1]) \|\|` |
|     242 | 1275 | `	          ph7_value_is_resource(apArg[1]) ){` |
|     ! 0 | 1276 | `		return PH7_VmThrowException(pCtx,` |
|       - | 1277 | `			"TypeError",` |
|       - | 1278 | `			"addcslashes(): Argument #2 ($characters) must be of type string, %s given",` |
|     ! 0 | 1279 | `			ph7_type_name(apArg[1])` |
|       - | 1280 | `			);` |
|       - | 1281 | `	}` |
|       - | 1282 | `	/* Extract the string to process */` |
|     246 | 1283 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|       - | 1284 | `	/* NULL would never reach here due to the check above. */` |
|     246 | 1285 | `	if( nLen < 1 ){` |
|       - | 1286 | `		/* Empty string returns itself. */` |
|      12 | 1287 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      12 | 1288 | `		return PH7_OK;` |
|       - | 1289 | `	}` |
|       - | 1290 | ``	/* Extract the desired mask and expand any `a..z` ranges into a lookup. */`` |
|     236 | 1291 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|     236 | 1292 | `	PH7_BuildCharMask(pCtx,zMask,nMask,aMask);` |
|     236 | 1293 | `	zEnd = &zIn[nLen];` |
|     236 | 1294 | `	zCur = 0; /* cc warning */` |
|     252 | 1295 | `	for(;;){` |
|     508 | 1296 | `		if( zIn >= zEnd ){` |
|       - | 1297 | `			/* No more input */` |
|     236 | 1298 | `			break;` |
|       - | 1299 | `		}` |
|     276 | 1300 | `		zCur = zIn;` |
|    4232 | 1301 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|    3960 | 1302 | `			zIn++;` |
|       4 | 1303 | `		}` |
|     276 | 1304 | `		if( zIn > zCur ){` |
|       - | 1305 | `			/* Append raw contents */` |
|     266 | 1306 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     131 | 1307 | `		}` |
|     276 | 1308 | `		if( zIn < zEnd ){` |
|       - | 1309 | `			/* Make sure we treat the byte as unsigned to avoid negative values` |
|       - | 1310 | `			 * on platforms where char is signed. */` |
|      54 | 1311 | `			int c = (unsigned char)zIn[0];` |
|       - | 1312 | `			/* Handle special C-like escapes for common control characters first.` |
|       - | 1313 | `			 * PHP outputs "\n" "\r" "\t" "\v" "\f" when those chars are` |
|       - | 1314 | `			 * in the mask. NUL is left to the octal conversion below. */` |
|      54 | 1315 | `			if( c == '\n' ){` |
|       8 | 1316 | `				ph7_result_string(pCtx,"\\n",2);` |
|      51 | 1317 | `			}else if( c == '\r' ){` |
|       3 | 1318 | `				ph7_result_string(pCtx,"\\r",2);` |
|      47 | 1319 | `			}else if( c == '\t' ){` |
|       3 | 1320 | `				ph7_result_string(pCtx,"\\t",2);` |
|      45 | 1321 | `			}else if( c == '\v' ){` |
|       3 | 1322 | `				ph7_result_string(pCtx,"\\v",2);` |
|      43 | 1323 | `			}else if( c == '\f' ){` |
|       3 | 1324 | `				ph7_result_string(pCtx,"\\f",2);` |
|      41 | 1325 | `			}else if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|       - | 1326 | `				/* Convert to octal.  PHP always emits three-digit zero-padded` |
|       - | 1327 | `				 * octal escapes (\001 not \1). */` |
|      29 | 1328 | `				ph7_result_string_format(pCtx,"\\%03o",c);` |
|      16 | 1329 | `			}else{` |
|      13 | 1330 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|       - | 1331 | `			}` |
|      25 | 1332 | `		}` |
|     276 | 1333 | `		zIn++;` |
|       4 | 1334 | `	}` |
|     236 | 1335 | `	return PH7_OK;` |
|     125 | 1336 | `}` |
|       - | 1337 | `/*` |
|       - | 1338 | ` * string quotemeta(string $str)` |
|       - | 1339 | ` *  Quote meta characters.` |
|       - | 1340 | ` * Parameter` |
|       - | 1341 | ` *  $str:` |
|       - | 1342 | ` *    The string to be escaped.` |
|       - | 1343 | ` * Return` |
|       - | 1344 | ` *  Returns the escaped string.` |
|       - | 1345 | `*/` |
|      12 | 1346 | `PH7_PRIVATE int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1347 | `{` |
|       - | 1348 | `	const char *zCur,*zIn,*zEnd;` |
|       - | 1349 | `	char aMask[256];` |
|       - | 1350 | `	int nLen;` |
|      15 | 1351 | `	if( nArg < 1 ){` |
|       - | 1352 | `		/* Nothing to process,retun NULL */` |
|     ! 0 | 1353 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1354 | `		return PH7_OK;` |
|       - | 1355 | `	}` |
|       - | 1356 | `	/* Extract the string to process */` |
|      15 | 1357 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      15 | 1358 | `	if( nLen < 1 ){` |
|       - | 1359 | `		/* Return the empty string */` |
|       6 | 1360 | `		ph7_result_string(pCtx,"",0);` |
|       6 | 1361 | `		return PH7_OK;` |
|       - | 1362 | `	}` |
|       - | 1363 | `	/* Fixed meta-character set (no ranges); build the lookup once. */` |
|      10 | 1364 | `	PH7_BuildCharMask(pCtx,".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1,aMask);` |
|      10 | 1365 | `	zEnd = &zIn[nLen];` |
|      10 | 1366 | `	zCur = 0; /* cc warning */` |
|      22 | 1367 | `	for(;;){` |
|      46 | 1368 | `		if( zIn >= zEnd ){` |
|       - | 1369 | `			/* No more input */` |
|      10 | 1370 | `			break;` |
|       - | 1371 | `		}` |
|      38 | 1372 | `		zCur = zIn;` |
|      76 | 1373 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|      40 | 1374 | `			zIn++;` |
|       2 | 1375 | `		}` |
|      38 | 1376 | `		if( zIn > zCur ){` |
|       - | 1377 | `			/* Append raw contents */` |
|      20 | 1378 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|       9 | 1379 | `		}` |
|      38 | 1380 | `		if( zIn < zEnd ){` |
|      36 | 1381 | `			int c = zIn[0];` |
|      36 | 1382 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|      17 | 1383 | `		}` |
|      38 | 1384 | `		zIn++;` |
|       2 | 1385 | `	}` |
|      10 | 1386 | `	return PH7_OK;` |
|       9 | 1387 | `}` |
|       - | 1388 | `/*` |
|       - | 1389 | ` * string stripslashes(string $str)` |
|       - | 1390 | ` *  Un-quotes a quoted string.` |
|       - | 1391 | ` *  Returns a string with backslashes before characters that need` |
|       - | 1392 | ` *  to be quoted in database queries etc. These characters are single` |
|       - | 1393 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|       - | 1394 | ` * Parameter` |
|       - | 1395 | ` *  $str` |
|       - | 1396 | ` *   The input string.` |
|       - | 1397 | ` * Return` |
|       - | 1398 | ` *  Returns a string with backslashes stripped off.` |
|       - | 1399 | ` */` |
|       8 | 1400 | `PH7_PRIVATE int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1401 | `{` |
|       - | 1402 | `	const char *zCur,*zIn,*zEnd;` |
|       - | 1403 | `	int nLen;` |
|      10 | 1404 | `	if( nArg < 1 ){` |
|       - | 1405 | `		/* Nothing to process,retun NULL */` |
|     ! 0 | 1406 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1407 | `		return PH7_OK;` |
|       - | 1408 | `	}` |
|       - | 1409 | `	/* Extract the string to process */` |
|      10 | 1410 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      10 | 1411 | `	if( zIn == 0 ){` |
|     ! 0 | 1412 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1413 | `		return PH7_OK;` |
|       - | 1414 | `	}` |
|      10 | 1415 | `	zEnd = &zIn[nLen];` |
|      10 | 1416 | `	zCur = 0; /* cc warning */` |
|       - | 1417 | `	/* Seed an empty string result: the loop below only ever APPENDS, so without` |
|       - | 1418 | `	 * this an empty input would leave the return value untouched and answer` |
|       - | 1419 | `	 * NULL where php answers "". */` |
|      10 | 1420 | `	ph7_result_string(pCtx,"",0);` |
|       - | 1421 | `	/* Encode the string */` |
|       5 | 1422 | `	for(;;){` |
|      12 | 1423 | `		if( zIn >= zEnd ){` |
|       - | 1424 | `			/* No more input */` |
|       6 | 1425 | `			break;` |
|       - | 1426 | `		}` |
|       7 | 1427 | `		zCur = zIn;` |
|      19 | 1428 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|      13 | 1429 | `			zIn++;` |
|       1 | 1430 | `		}` |
|       7 | 1431 | `		if( zIn > zCur ){` |
|       - | 1432 | `			/* Append raw contents */` |
|       5 | 1433 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|       2 | 1434 | `		}` |
|       7 | 1435 | `		if( &zIn[1] < zEnd ){` |
|       3 | 1436 | `			int c = zIn[1];` |
|       3 | 1437 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|       - | 1438 | `				/* Ignore the backslash */` |
|       3 | 1439 | `				zIn++;` |
|       1 | 1440 | `			}` |
|       2 | 1441 | `		}else{` |
|       5 | 1442 | `			break;` |
|       - | 1443 | `		}` |
|       1 | 1444 | `	}` |
|      10 | 1445 | `	return PH7_OK;` |
|       6 | 1446 | `}` |
|       - | 1447 | `/*` |
|       - | 1448 | ` * UTF-8-aware HTML entity machinery, shared by htmlspecialchars/htmlentities/` |
|       - | 1449 | ` * htmlspecialchars_decode/html_entity_decode/get_html_translation_table.` |
|       - | 1450 | ` * The implementations live further down in this file, next to the filter_var` |
|       - | 1451 | ` * FULL_SPECIAL_CHARS machinery they reuse (aHtml401Ent[]/FvHtml401Lookup()/` |
|       - | 1452 | ` * FvUtf8Next()). Semantics are byte-exact vs php 8.5.7; PHL is UTF-8-only` |
|       - | 1453 | ` * so every charset argument other than a UTF-8 alias gets PHP's` |
|       - | 1454 | ` * unsupported-charset warning and is treated as UTF-8.` |
|       - | 1455 | ` *` |
|       - | 1456 | ` * Flag model (the PHP-exact ENT_* values, see constant.c): bit 1 = encode/` |
|       - | 1457 | ` * decode single quotes, bit 2 = double quotes (ENT_QUOTES=3, ENT_COMPAT=2,` |
|       - | 1458 | ` * ENT_NOQUOTES=0); bits 16\|32 select the doctype (0=HTML401, 16=XML1,` |
|       - | 1459 | ` * 32=XHTML, 48=HTML5); ENT_IGNORE=4 drops invalid UTF-8 bytes (wins over` |
|       - | 1460 | ` * ENT_SUBSTITUTE=8, which replaces each with U+FFFD; with neither set the` |
|       - | 1461 | ` * whole result collapses to ""); ENT_DISALLOWED=128 substitutes valid but` |
|       - | 1462 | ` * doctype-disallowed codepoints. The shared default is` |
|       - | 1463 | ` * ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 = 11.` |
|       - | 1464 | ` */` |
|       - | 1465 | `/*` |
|       - | 1466 | ` * string htmlspecialchars(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|       - | 1467 | ` *                         [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|       - | 1468 | ` *  Convert the special characters & < > " ' to HTML entities.` |
|       - | 1469 | ` * Return` |
|       - | 1470 | ` *  The escaped string or NULL on failure.` |
|       - | 1471 | ` */` |
|      74 | 1472 | `PH7_PRIVATE int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1473 | `{` |
|      76 | 1474 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|       - | 1475 | `	const char *zIn;` |
|      76 | 1476 | `	int nLen,bDouble = 1,iCs;` |
|       - | 1477 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|       - | 1478 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|      76 | 1479 | `	if( nArg < 1 ){` |
|       - | 1480 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 1481 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1482 | `		return PH7_OK;` |
|       - | 1483 | `	}` |
|       - | 1484 | `	/* Extract the target string */` |
|      76 | 1485 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      76 | 1486 | `	if( nArg > 1 ){` |
|      68 | 1487 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      33 | 1488 | `	}` |
|      76 | 1489 | `	iCs = HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|      76 | 1490 | `	if( nArg > 3 ){` |
|       7 | 1491 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|       3 | 1492 | `	}` |
|      76 | 1493 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,0,bDouble,iCs);` |
|      76 | 1494 | `	return PH7_OK;` |
|      39 | 1495 | `}` |
|       - | 1496 | `/*` |
|       - | 1497 | ` * string htmlspecialchars_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401])` |
|       - | 1498 | ` *  Convert the special HTML entities (&amp; &lt; &gt; &quot; and the` |
|       - | 1499 | ` *  numeric/doctype forms of the two quotes) back to characters.` |
|       - | 1500 | ` * Return` |
|       - | 1501 | ` *  The unescaped string or NULL on failure.` |
|       - | 1502 | ` */` |
|      22 | 1503 | `PH7_PRIVATE int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1504 | `{` |
|      23 | 1505 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|       - | 1506 | `	const char *zIn;` |
|       - | 1507 | `	int nLen;` |
|       - | 1508 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|       - | 1509 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|      23 | 1510 | `	if( nArg < 1 ){` |
|       - | 1511 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 1512 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1513 | `		return PH7_OK;` |
|       - | 1514 | `	}` |
|       - | 1515 | `	/* Extract the target string */` |
|      23 | 1516 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      23 | 1517 | `	if( nArg > 1 ){` |
|       9 | 1518 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|       4 | 1519 | `	}` |
|       - | 1520 | `	/* htmlspecialchars_decode() takes no charset: the five specials are ASCII in` |
|       - | 1521 | `	 * every charset php models here. */` |
|      23 | 1522 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,0,PH7_HTML_CS_UTF8);` |
|      23 | 1523 | `	return PH7_OK;` |
|      12 | 1524 | `}` |
|       - | 1525 | `/*` |
|       - | 1526 | ` * array get_html_translation_table(int $table = HTML_SPECIALCHARS` |
|       - | 1527 | ` *      [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 [, string $encoding = "UTF-8"]])` |
|       - | 1528 | ` *  Return the translation table used by htmlspecialchars() (HTML_SPECIALCHARS)` |
|       - | 1529 | ` *  or htmlentities() (HTML_ENTITIES) as character => entity pairs.` |
|       - | 1530 | ` * Return` |
|       - | 1531 | ` *  The translation table as an array or NULL on failure.` |
|       - | 1532 | ` */` |
|      42 | 1533 | `PH7_PRIVATE int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1534 | `{` |
|      44 | 1535 | `	int iTable = 0; /* HTML_SPECIALCHARS */` |
|      44 | 1536 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      44 | 1537 | `	if( nArg > 0 ){` |
|      42 | 1538 | `		iTable = ph7_value_to_int(apArg[0]);` |
|      20 | 1539 | `	}` |
|      44 | 1540 | `	if( nArg > 1 ){` |
|      40 | 1541 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      19 | 1542 | `	}` |
|      44 | 1543 | `	HtmlTranslationTable(pCtx,iTable,iFlags,HtmlCheckCharset(pCtx,nArg,apArg,2));` |
|      44 | 1544 | `	return PH7_OK;` |
|       2 | 1545 | `}` |
|       - | 1546 | `/*` |
|       - | 1547 | ` * string htmlentities(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|       - | 1548 | ` *                     [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|       - | 1549 | ` *  Convert all applicable characters to HTML entities: the specials plus` |
|       - | 1550 | ` *  every codepoint with an HTML 4.01 named entity (aHtml401Ent[]).` |
|       - | 1551 | ` * Return` |
|       - | 1552 | ` *  The encoded string or NULL on failure.` |
|       - | 1553 | ` */` |
|      62 | 1554 | `PH7_PRIVATE int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1555 | `{` |
|      64 | 1556 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|       - | 1557 | `	const char *zIn;` |
|      64 | 1558 | `	int nLen,bDouble = 1,iCs;` |
|       - | 1559 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|       - | 1560 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|      64 | 1561 | `	if( nArg < 1 ){` |
|       - | 1562 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 1563 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1564 | `		return PH7_OK;` |
|       - | 1565 | `	}` |
|       - | 1566 | `	/* Extract the target string */` |
|      64 | 1567 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      64 | 1568 | `	if( nArg > 1 ){` |
|      52 | 1569 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      25 | 1570 | `	}` |
|      64 | 1571 | `	iCs = HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|      64 | 1572 | `	if( nArg > 3 ){` |
|       6 | 1573 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|       2 | 1574 | `	}` |
|      64 | 1575 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,1,bDouble,iCs);` |
|      64 | 1576 | `	return PH7_OK;` |
|      33 | 1577 | `}` |
|       - | 1578 | `/*` |
|       - | 1579 | ` * string html_entity_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|       - | 1580 | ` *                           [, string $encoding = "UTF-8"]])` |
|       - | 1581 | ` *  Convert HTML entities (named — case-sensitive — and numeric, decimal or` |
|       - | 1582 | ` *  hex) back to their UTF-8 characters. The reverse of htmlentities().` |
|       - | 1583 | ` * Return` |
|       - | 1584 | ` *  The decoded string or NULL on failure.` |
|       - | 1585 | ` */` |
|      64 | 1586 | `PH7_PRIVATE int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1587 | `{` |
|      66 | 1588 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|       - | 1589 | `	const char *zIn;` |
|       - | 1590 | `	int nLen;` |
|       - | 1591 | `	/* php coerces a scalar argument to string here (weak mode); the shared ZPP` |
|       - | 1592 | `	 * screen in vm.c has already rejected the values that cannot coerce. */` |
|      66 | 1593 | `	if( nArg < 1 ){` |
|       - | 1594 | `		/* Missing/Invalid arguments,return NULL */` |
|     ! 0 | 1595 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1596 | `		return PH7_OK;` |
|       - | 1597 | `	}` |
|       - | 1598 | `	/* Extract the target string */` |
|      66 | 1599 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      66 | 1600 | `	if( nArg > 1 ){` |
|      34 | 1601 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      16 | 1602 | `	}` |
|      66 | 1603 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,1,HtmlCheckCharset(pCtx,nArg,apArg,2));` |
|      66 | 1604 | `	return PH7_OK;` |
|      34 | 1605 | `}` |
|       - | 1606 | `/*` |
|       - | 1607 | ` * int strlen($string)` |
|       - | 1608 | ` *  return the length of the given string.` |
|       - | 1609 | ` * Parameter` |
|       - | 1610 | ` *  string: The string being measured for length.` |
|       - | 1611 | ` * Return` |
|       - | 1612 | ` *  length of the given string.` |
|       - | 1613 | ` */` |
|  168423 | 1614 | `PH7_PRIVATE int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 1615 | `{` |
|  168428 | 1616 | `	int iLen = 0;` |
|  168428 | 1617 | `	if( nArg > 0 ){` |
|  168428 | 1618 | `		StrNullArgNotice(pCtx,apArg[0],"strlen",1,"$string");` |
|  168428 | 1619 | `		ph7_value_to_string(apArg[0],&iLen);` |
|   84648 | 1620 | `	}` |
|       - | 1621 | `	/* String length */` |
|  168428 | 1622 | `	ph7_result_int(pCtx,iLen);` |
|  168428 | 1623 | `	return PH7_OK;` |
|       5 | 1624 | `}` |
|       - | 1625 | `/*` |
|       - | 1626 | ` * int strcmp(string $str1,string $str2)` |
|       - | 1627 | ` *  Perform a binary safe string comparison.` |
|       - | 1628 | ` * Parameter` |
|       - | 1629 | ` *  str1: The first string` |
|       - | 1630 | ` *  str2: The second string` |
|       - | 1631 | ` * Return` |
|       - | 1632 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|       - | 1633 | ` *  than str2, and 0 if they are equal.` |
|       - | 1634 | ` */` |
|      96 | 1635 | `PH7_PRIVATE int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 1636 | `{` |
|       - | 1637 | `	const char *z1,*z2;` |
|       - | 1638 | `	int n1,n2;` |
|       - | 1639 | `	int res;` |
|      99 | 1640 | `	if( nArg < 2 ){` |
|     ! 0 | 1641 | `		res = nArg == 0 ? 0 : 1;` |
|     ! 0 | 1642 | `		ph7_result_int(pCtx,res);` |
|     ! 0 | 1643 | `		return PH7_OK;` |
|       - | 1644 | `	}` |
|       - | 1645 | `	/* Perform the comparison */` |
|      99 | 1646 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|      99 | 1647 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|      99 | 1648 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|       - | 1649 | `	/* Comparison result */` |
|      99 | 1650 | `	ph7_result_int(pCtx,res);` |
|      99 | 1651 | `	return PH7_OK;` |
|      51 | 1652 | `}` |
|       - | 1653 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|       - | 1654 | `/*` |
|       - | 1655 | ` * The natural-order comparison core lives OUTSIDE the PH7_DISABLE_BUILTIN_FUNC` |
|       - | 1656 | ` * guard: hashmap.c's SORT_NATURAL path (always compiled) calls PH7_StrNatCmp, so` |
|       - | 1657 | ` * it must exist in the tiny build too. [[tiny-build-disk-io-guard-fragility]]` |
|       - | 1658 | ` */` |
|       - | 1659 | `/*` |
|       - | 1660 | ` * Natural-order comparison core (Martin Pool's natcompare as adapted by php's` |
|       - | 1661 | ` * ext/standard/strnatcmp.c): digit runs compare numerically — the longer run` |
|       - | 1662 | ` * wins, a leading zero flips to fractional first-difference-wins semantics —` |
|       - | 1663 | ` * everything else compares bytewise with whitespace skipped.` |
|       - | 1664 | ` */` |
|     166 | 1665 | `static int StrNatCompareRight(const char **pa,const char *aEnd,const char **pb,const char *bEnd)` |
|       3 | 1666 | `{` |
|     169 | 1667 | `	int bias = 0;` |
|     275 | 1668 | `	for(;;){` |
|     361 | 1669 | `		int da = (*pa < aEnd) && SyisDigit(**pa);` |
|     361 | 1670 | `		int db = (*pb < bEnd) && SyisDigit(**pb);` |
|     361 | 1671 | `		if( !da && !db ){ return bias; }` |
|     289 | 1672 | `		if( !da ){ return -1; }` |
|     259 | 1673 | `		if( !db ){ return 1; }` |
|     195 | 1674 | `		if( **pa < **pb ){ if( !bias ){ bias = -1; } }` |
|     123 | 1675 | `		else if( **pa > **pb ){ if( !bias ){ bias = 1; } }` |
|     195 | 1676 | `		(*pa)++;` |
|     195 | 1677 | `		(*pb)++;` |
|       3 | 1678 | `	}` |
|      86 | 1679 | `}` |
|       4 | 1680 | `static int StrNatCompareLeft(const char **pa,const char *aEnd,const char **pb,const char *bEnd)` |
|       1 | 1681 | `{` |
|       2 | 1682 | `	for(;;){` |
|       5 | 1683 | `		int da = (*pa < aEnd) && SyisDigit(**pa);` |
|       5 | 1684 | `		int db = (*pb < bEnd) && SyisDigit(**pb);` |
|       5 | 1685 | `		if( !da && !db ){ return 0; }` |
|       5 | 1686 | `		if( !da ){ return -1; }` |
|       5 | 1687 | `		if( !db ){ return 1; }` |
|       5 | 1688 | `		if( **pa < **pb ){ return -1; }` |
|     ! 0 | 1689 | `		if( **pa > **pb ){ return 1; }` |
|     ! 0 | 1690 | `		(*pa)++;` |
|     ! 0 | 1691 | `		(*pb)++;` |
|     ! 0 | 1692 | `	}` |
|       3 | 1693 | `}` |
|     248 | 1694 | `PH7_PRIVATE int PH7_StrNatCmp(const char *zA,int nA,const char *zB,int nB,int bFold)` |
|       3 | 1695 | `{` |
|     251 | 1696 | `	const char *a = zA,*aEnd = &zA[nA];` |
|     251 | 1697 | `	const char *b = zB,*bEnd = &zB[nB];` |
|     642 | 1698 | `	for(;;){` |
|       - | 1699 | `		int ca,cb;` |
|     777 | 1700 | `		while( a < aEnd && SyisSpace(a[0]) ){ a++; }` |
|     775 | 1701 | `		while( b < bEnd && SyisSpace(b[0]) ){ b++; }` |
|     775 | 1702 | `		ca = (a < aEnd) ? (unsigned char)a[0] : 0;` |
|     775 | 1703 | `		cb = (b < bEnd) ? (unsigned char)b[0] : 0;` |
|     775 | 1704 | `		if( SyisDigit(ca) && SyisDigit(cb) ){` |
|     171 | 1705 | `			int r = (ca == '0' \|\| cb == '0')` |
|       4 | 1706 | `				? StrNatCompareLeft(&a,aEnd,&b,bEnd)` |
|     251 | 1707 | `				: StrNatCompareRight(&a,aEnd,&b,bEnd);` |
|     173 | 1708 | `			if( r ){ return r; }` |
|      14 | 1709 | `			continue;` |
|       - | 1710 | `		}` |
|     605 | 1711 | `		if( ca == 0 && cb == 0 ){ return 0; }` |
|     583 | 1712 | `		if( bFold ){` |
|     285 | 1713 | `			ca = SyToLower(ca);` |
|     285 | 1714 | `			cb = SyToLower(cb);` |
|     141 | 1715 | `		}` |
|     583 | 1716 | `		if( ca < cb ){ return -1; }` |
|     535 | 1717 | `		if( ca > cb ){ return 1; }` |
|     515 | 1718 | `		a++;` |
|     515 | 1719 | `		b++;` |
|       3 | 1720 | `	}` |
|     127 | 1721 | `}` |
|       - | 1722 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|       - | 1723 | `/*` |
|       - | 1724 | ` * int strnatcmp(string $string1, string $string2)` |
|       - | 1725 | ` * int strnatcasecmp(string $string1, string $string2)` |
|       - | 1726 | ` *  Natural-order string comparison ("img2" < "img10"), case folded for the` |
|       - | 1727 | ` *  latter. php 8.2+ normalizes the result to -1/0/1.` |
|       - | 1728 | ` */` |
|      62 | 1729 | `PH7_PRIVATE int PH7_builtin_strnatcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 1730 | `{` |
|       - | 1731 | `	const char *z1,*z2,*zFunc;` |
|       - | 1732 | `	int n1,n2,bFold;` |
|      64 | 1733 | `	if( nArg < 2 ){` |
|     ! 0 | 1734 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 1735 | `		return PH7_OK;` |
|       - | 1736 | `	}` |
|      64 | 1737 | `	zFunc = ph7_function_name(pCtx);` |
|       - | 1738 | `	/* Both names carry a 'c' at that offset -- "strnat\|c\|mp" as much as` |
|       - | 1739 | `	 * "strnat\|c\|asecmp" -- so testing it alone made strnatcmp() fold as well, and` |
|       - | 1740 | `	 * natsort()/ArrayObject::natsort() (prelude wrappers over strnatcmp) with it:` |
|       - | 1741 | `	 * 'Hello' and 'hello' compared EQUAL where php answers -1. */` |
|      64 | 1742 | `	bFold = SyStrnicmp(zFunc,"strnatcase",sizeof("strnatcase")-1) == 0;` |
|      64 | 1743 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|      64 | 1744 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|      64 | 1745 | `	ph7_result_int(pCtx,PH7_StrNatCmp(z1,n1,z2,n2,bFold));` |
|      64 | 1746 | `	return PH7_OK;` |
|      33 | 1747 | `}` |
|       - | 1748 | `/*` |
|       - | 1749 | ` * php's special version forms and their ordering` |
|       - | 1750 | ` * (compare_special_version_forms(), ext/standard/versioning.c). A form matches` |
|       - | 1751 | ` * a component by PREFIX -- "alpha3" is an alpha, "RC1" an RC -- and "#" is the` |
|       - | 1752 | ` * marker php compares a NUMERIC component as, spelled "#N#" at the call sites.` |
|       - | 1753 | ` * An unrecognized component ranks -1, BELOW dev.` |
|       - | 1754 | ` */` |
|       - | 1755 | `static const struct VersionForm {` |
|       - | 1756 | `	const char *zName;` |
|       - | 1757 | `	int nLen;` |
|       - | 1758 | `	int iOrder;` |
|       - | 1759 | `} aVersionForm[] = {` |
|       - | 1760 | `	{ "dev", 3, 0 }, { "alpha", 5, 1 }, { "a",  1, 1 }, { "beta", 4, 2 },` |
|       - | 1761 | `	{ "b",   1, 2 }, { "RC",    2, 3 }, { "rc", 2, 3 }, { "#",    1, 4 },` |
|       - | 1762 | `	{ "pl",  2, 5 }, { "p",     1, 5 },` |
|       - | 1763 | `};` |
|     144 | 1764 | `static int VersionFormOrder(const char *zPart)` |
|       1 | 1765 | `{` |
|       - | 1766 | `	sxu32 n;` |
|     977 | 1767 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVersionForm) ; ++n ){` |
|     955 | 1768 | `		if( SyStrncmp(zPart,aVersionForm[n].zName,(sxu32)aVersionForm[n].nLen) == 0 ){` |
|     123 | 1769 | `			return aVersionForm[n].iOrder;` |
|       - | 1770 | `		}` |
|     417 | 1771 | `	}` |
|      23 | 1772 | `	return -1;` |
|      73 | 1773 | `}` |
|      72 | 1774 | `static int VersionSpecialCmp(const char *zPart1,const char *zPart2)` |
|       1 | 1775 | `{` |
|      73 | 1776 | `	int iOrd1 = VersionFormOrder(zPart1);` |
|      73 | 1777 | `	int iOrd2 = VersionFormOrder(zPart2);` |
|      73 | 1778 | `	return iOrd1 < iOrd2 ? -1 : (iOrd1 > iOrd2 ? 1 : 0);` |
|       1 | 1779 | `}` |
|       - | 1780 | `/*` |
|       - | 1781 | ` * php_canonicalize_version(): '-', '_', '+' and every other non-alphanumeric` |
|       - | 1782 | ` * byte become '.', and a '.' is inserted at each digit<->non-digit boundary.` |
|       - | 1783 | ` * The FIRST byte is copied verbatim -- even a separator -- so "-1"` |
|       - | 1784 | ` * canonicalizes to "-.1" and not to "1", and its leading "-" then compares as` |
|       - | 1785 | ` * an unrecognized form. A trailing '.' is dropped rather than left as an empty` |
|       - | 1786 | ` * last component. zOut must hold 2*nLen + 2 bytes.` |
|       - | 1787 | ` */` |
|     518 | 1788 | `static void VersionCanonicalize(const char *zIn,sxu32 nLen,char *zOut)` |
|       1 | 1789 | `{` |
|       - | 1790 | `	const char *p,*pEnd;` |
|     519 | 1791 | `	char *q = zOut;` |
|       - | 1792 | `	int lp;` |
|     519 | 1793 | `	if( nLen < 1 ){` |
|     ! 0 | 1794 | `		zOut[0] = 0;` |
|     ! 0 | 1795 | `		return;` |
|       - | 1796 | `	}` |
|     519 | 1797 | `	lp = (unsigned char)zIn[0];` |
|     519 | 1798 | `	*q++ = (char)lp;` |
|     519 | 1799 | `	pEnd = &zIn[nLen];` |
|    1433 | 1800 | `	for( p = &zIn[1] ; p < pEnd ; lp = (unsigned char)*p++ ){` |
|     977 | 1801 | `		int c  = (unsigned char)p[0];` |
|     977 | 1802 | `		int lq = (unsigned char)q[-1];` |
|     977 | 1803 | `		if( c == '-' \|\| c == '_' \|\| c == '+' ){` |
|     181 | 1804 | `			if( lq != '.' ){ *q++ = '.'; }` |
|    1165 | 1805 | `		}else if( (!SyisDigit(lp) && lp != '.' && SyisDigit(c)) \|\|` |
|     846 | 1806 | `		          (SyisDigit(lp) && !SyisDigit(c) && c != '.') ){` |
|     199 | 1807 | `			if( lq != '.' ){ *q++ = '.'; }` |
|     109 | 1808 | `			*q++ = (char)c;` |
|     831 | 1809 | `		}else if( !SyisAlphaNum(c) ){` |
|     323 | 1810 | `			if( lq != '.' ){ *q++ = '.'; }` |
|     162 | 1811 | `		}else{` |
|     455 | 1812 | `			*q++ = (char)c;` |
|       - | 1813 | `		}` |
|     458 | 1814 | `	}` |
|     457 | 1815 | `	if( q[-1] == '.' ){` |
|      13 | 1816 | `		q[-1] = 0;` |
|       7 | 1817 | `	}else{` |
|     445 | 1818 | `		q[0] = 0;` |
|       - | 1819 | `	}` |
|     229 | 1820 | `}` |
|       - | 1821 | `/* strtol() over a canonical numeric component, saturating like the C library. */` |
|     680 | 1822 | `static sxi64 VersionPartToInt(const char *zPart)` |
|       1 | 1823 | `{` |
|     681 | 1824 | `	sxi64 iVal = 0;` |
|    1367 | 1825 | `	while( SyisDigit((unsigned char)zPart[0]) ){` |
|     687 | 1826 | `		if( iVal > (SXI64_HIGH - 9) / 10 ){` |
|     ! 0 | 1827 | `			return SXI64_HIGH;` |
|       - | 1828 | `		}` |
|     687 | 1829 | `		iVal = iVal * 10 + (zPart[0] - '0');` |
|     687 | 1830 | `		zPart++;` |
|       1 | 1831 | `	}` |
|     681 | 1832 | `	return iVal;` |
|     341 | 1833 | `}` |
|     824 | 1834 | `static char * VersionNextDot(char *zPart)` |
|       1 | 1835 | `{` |
|    1813 | 1836 | `	while( zPart[0] && zPart[0] != '.' ){ zPart++; }` |
|     825 | 1837 | `	return zPart[0] ? zPart : 0;` |
|       1 | 1838 | `}` |
|       - | 1839 | `/*` |
|       - | 1840 | ` * php_version_compare() over two CANONICAL buffers -- the caller has already` |
|       - | 1841 | ` * applied php's empty-operand shortcut to the ORIGINAL strings. Both buffers` |
|       - | 1842 | ` * are written in place (the walk NUL-terminates each component where php's` |
|       - | 1843 | ` * strchr does), so they must be writable copies.` |
|       - | 1844 | ` *` |
|       - | 1845 | ` * Where php recurses on the leftover of the longer version, this loops: the` |
|       - | 1846 | ` * recursion is a tail call, and its depth would otherwise grow with the` |
|       - | 1847 | ` * component count of an attacker-supplied string.` |
|       - | 1848 | ` */` |
|     232 | 1849 | `static int VersionCompareCanon(char *zV1,char *zV2)` |
|       1 | 1850 | `{` |
|     233 | 1851 | `	char zMark[] = "#N#";   /* php's "this component is a number" marker */` |
|     140 | 1852 | `	for(;;){` |
|       - | 1853 | `		char *p1,*p2,*n1,*n2;` |
|     257 | 1854 | `		int cmp = 0;` |
|     257 | 1855 | `		p1 = n1 = zV1;` |
|     257 | 1856 | `		p2 = n2 = zV2;` |
|     525 | 1857 | `		while( p1[0] && p2[0] && n1 && n2 ){` |
|     413 | 1858 | `			if( (n1 = VersionNextDot(p1)) != 0 ){ n1[0] = 0; }` |
|     413 | 1859 | `			if( (n2 = VersionNextDot(p2)) != 0 ){ n2[0] = 0; }` |
|     413 | 1860 | `			if( SyisDigit((unsigned char)p1[0]) && SyisDigit((unsigned char)p2[0]) ){` |
|     341 | 1861 | `				sxi64 l1 = VersionPartToInt(p1);` |
|     341 | 1862 | `				sxi64 l2 = VersionPartToInt(p2);` |
|     341 | 1863 | `				cmp = l1 < l2 ? -1 : (l1 > l2 ? 1 : 0);` |
|     243 | 1864 | `			}else if( !SyisDigit((unsigned char)p1[0]) && !SyisDigit((unsigned char)p2[0]) ){` |
|      59 | 1865 | `				cmp = VersionSpecialCmp(p1,p2);` |
|      44 | 1866 | `			}else if( SyisDigit((unsigned char)p1[0]) ){` |
|       5 | 1867 | `				cmp = VersionSpecialCmp(zMark,p2);` |
|       3 | 1868 | `			}else{` |
|      11 | 1869 | `				cmp = VersionSpecialCmp(p1,zMark);` |
|       - | 1870 | `			}` |
|     413 | 1871 | `			if( cmp != 0 ){ break; }` |
|     269 | 1872 | `			if( n1 ){ p1 = &n1[1]; }` |
|     269 | 1873 | `			if( n2 ){ p2 = &n2[1]; }` |
|       1 | 1874 | `		}` |
|     257 | 1875 | `		if( cmp != 0 ){` |
|     145 | 1876 | `			return cmp;` |
|       - | 1877 | `		}` |
|       - | 1878 | `		/*` |
|       - | 1879 | `		 * Equal so far and one side has components left, so php asks whether the` |
|       - | 1880 | `		 * next one outranks a plain number: "1.2.3" > "1.2" but "1.2.dev" < "1.2".` |
|       - | 1881 | `		 * The leftover can still hold dots ("1" vs "1#2" leaves "#.2"), which is` |
|       - | 1882 | `		 * why this is a whole comparison and not one special-form lookup.` |
|       - | 1883 | `		 */` |
|     113 | 1884 | `		if( n1 ){` |
|      25 | 1885 | `			if( SyisDigit((unsigned char)p1[0]) ){ return 1; }` |
|      23 | 1886 | `			if( p1[0] == 0 ){ return -1; }   /* php's empty-operand shortcut */` |
|      19 | 1887 | `			zV1 = p1;` |
|      19 | 1888 | `			zV2 = zMark;` |
|      98 | 1889 | `		}else if( n2 ){` |
|      29 | 1890 | `			if( SyisDigit((unsigned char)p2[0]) ){ return -1; }` |
|       7 | 1891 | `			if( p2[0] == 0 ){ return 1; }` |
|       7 | 1892 | `			zV1 = zMark;` |
|       7 | 1893 | `			zV2 = p2;` |
|       4 | 1894 | `		}else{` |
|      61 | 1895 | `			return 0;` |
|       - | 1896 | `		}` |
|       1 | 1897 | `	}` |
|     117 | 1898 | `}` |
|       - | 1899 | `/*` |
|       - | 1900 | ` * int\|bool version_compare(string $version1,string $version2,?string $operator = null)` |
|       - | 1901 | ` *  Compare two "PHP-standardized" version number strings: -1/0/1 without an` |
|       - | 1902 | ` *  $operator, the operator's verdict with one. An operator php does not know` |
|       - | 1903 | ` *  is a ValueError (php 8 stopped answering NULL for it).` |
|       - | 1904 | ` */` |
|     244 | 1905 | `PH7_PRIVATE int PH7_builtin_version_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1906 | `{` |
|       - | 1907 | `	static const struct VersionOp {` |
|       - | 1908 | `		const char *zName;` |
|       - | 1909 | `		int nLen;` |
|       - | 1910 | `		int bLt,bEq,bGt;   /* answer for cmp < 0, cmp == 0, cmp > 0 */` |
|       - | 1911 | `	} aVersionOp[] = {` |
|       - | 1912 | `		{ "<", 1, 1,0,0 }, { "lt", 2, 1,0,0 }, { "<=",2, 1,1,0 }, { "le",2, 1,1,0 },` |
|       - | 1913 | `		{ ">", 1, 0,0,1 }, { "gt", 2, 0,0,1 }, { ">=",2, 0,1,1 }, { "ge",2, 0,1,1 },` |
|       - | 1914 | `		{ "==",2, 0,1,0 }, { "=",  1, 0,1,0 }, { "eq",2, 0,1,0 },` |
|       - | 1915 | `		{ "!=",2, 1,0,1 }, { "<>", 2, 1,0,1 }, { "ne",2, 1,0,1 },` |
|       - | 1916 | `	};` |
|       - | 1917 | `	const char *zV1,*zV2,*zOp;` |
|       - | 1918 | `	sxu32 n1,n2,n;` |
|       - | 1919 | `	int cmp,nOp;` |
|     245 | 1920 | `	if( nArg < 2 ){` |
|       - | 1921 | `		/* Arity is enforced from aBuiltinSig[] before the call. */` |
|     ! 0 | 1922 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1923 | `		return PH7_OK;` |
|       - | 1924 | `	}` |
|       - | 1925 | `	/* php reads both operands as NUL-terminated C strings, so an embedded NUL` |
|       - | 1926 | `	 * ends the version there. ph7_value_to_string() null-appends. */` |
|     245 | 1927 | `	zV1 = ph7_value_to_string(apArg[0],0);` |
|     245 | 1928 | `	zV2 = ph7_value_to_string(apArg[1],0);` |
|     245 | 1929 | `	n1 = SyStrlen(zV1);` |
|     245 | 1930 | `	n2 = SyStrlen(zV2);` |
|     245 | 1931 | `	if( n1 < 1 \|\| n2 < 1 ){` |
|      13 | 1932 | `		cmp = (n1 == n2) ? 0 : (n1 > 0 ? 1 : -1);` |
|       7 | 1933 | `	}else{` |
|       - | 1934 | `		char *zC1,*zC2;` |
|     233 | 1935 | `		zC1 = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(2 * n1 + 2),FALSE,TRUE);` |
|     233 | 1936 | `		zC2 = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(2 * n2 + 2),FALSE,TRUE);` |
|     233 | 1937 | `		if( zC1 == 0 \|\| zC2 == 0 ){` |
|     ! 0 | 1938 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 1939 | `		}` |
|       - | 1940 | `		/* A version already starting with '#' is php's own marker: it bypasses` |
|       - | 1941 | `		 * canonicalization so that "#N#" survives as one component. */` |
|     233 | 1942 | `		if( zV1[0] == '#' ){ SyMemcpy(zV1,zC1,n1 + 1); }else{ VersionCanonicalize(zV1,n1,zC1); }` |
|     233 | 1943 | `		if( zV2[0] == '#' ){ SyMemcpy(zV2,zC2,n2 + 1); }else{ VersionCanonicalize(zV2,n2,zC2); }` |
|     233 | 1944 | `		cmp = VersionCompareCanon(zC1,zC2);` |
|       - | 1945 | `	}` |
|     245 | 1946 | `	if( nArg < 3 \|\| ph7_value_is_null(apArg[2]) ){` |
|      85 | 1947 | `		ph7_result_int(pCtx,cmp);` |
|      85 | 1948 | `		return PH7_OK;` |
|       - | 1949 | `	}` |
|     161 | 1950 | `	zOp = ph7_value_to_string(apArg[2],&nOp);` |
|    1141 | 1951 | `	for( n = 0 ; n < SX_ARRAYSIZE(aVersionOp) ; ++n ){` |
|    1125 | 1952 | `		if( nOp == aVersionOp[n].nLen && SyMemcmp(zOp,aVersionOp[n].zName,(sxu32)nOp) == 0 ){` |
|     213 | 1953 | `			ph7_result_bool(pCtx,cmp < 0 ? aVersionOp[n].bLt` |
|      68 | 1954 | `				: (cmp > 0 ? aVersionOp[n].bGt : aVersionOp[n].bEq));` |
|     145 | 1955 | `			return PH7_OK;` |
|       - | 1956 | `		}` |
|     491 | 1957 | `	}` |
|      17 | 1958 | `	return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1959 | `		"version_compare(): Argument #3 ($operator) must be a valid comparison operator");` |
|     123 | 1960 | `}` |
|       - | 1961 | `/*` |
|       - | 1962 | ` * int strncmp(string $str1,string $str2,int n)` |
|       - | 1963 | ` *  Perform a binary safe string comparison of the first n characters.` |
|       - | 1964 | ` * Parameter` |
|       - | 1965 | ` *  str1: The first string` |
|       - | 1966 | ` *  str2: The second string` |
|       - | 1967 | ` * Return` |
|       - | 1968 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|       - | 1969 | ` *  than str2, and 0 if they are equal.` |
|       - | 1970 | ` */` |
|    2588 | 1971 | `PH7_PRIVATE int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 1972 | `{` |
|       - | 1973 | `	const char *z1,*z2;` |
|       - | 1974 | `	int res;` |
|       - | 1975 | `	int n;` |
|    2589 | 1976 | `	if( nArg < 3 ){` |
|       - | 1977 | `		/* Perform a standard comparison */` |
|     ! 0 | 1978 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|       - | 1979 | `	}` |
|       - | 1980 | `	/* Desired comparison length */` |
|    2589 | 1981 | `	n  = ph7_value_to_int(apArg[2]);` |
|    2589 | 1982 | `	if( n < 0 ){` |
|       - | 1983 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|       4 | 1984 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 1985 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|       1 | 1986 | `			ph7_function_name(pCtx));` |
|       - | 1987 | `	}` |
|       - | 1988 | `	/* Perform the comparison */` |
|    2587 | 1989 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|    2587 | 1990 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|    2587 | 1991 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|       - | 1992 | `	/* Comparison result */` |
|    2587 | 1993 | `	ph7_result_int(pCtx,res);` |
|    2587 | 1994 | `	return PH7_OK;` |
|    1295 | 1995 | `}` |
|       - | 1996 | `/*` |
|       - | 1997 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|       - | 1998 | ` *  Perform a binary safe case-insensitive string comparison.` |
|       - | 1999 | ` * Parameter` |
|       - | 2000 | ` *  str1: The first string` |
|       - | 2001 | ` *  str2: The second string` |
|       - | 2002 | ` * Return` |
|       - | 2003 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|       - | 2004 | ` *  than str2, and 0 if they are equal.` |
|       - | 2005 | ` */` |
|     112 | 2006 | `PH7_PRIVATE int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2007 | `{` |
|       - | 2008 | `	const char *z1,*z2;` |
|       - | 2009 | `	int n1,n2;` |
|       - | 2010 | `	int res;` |
|     115 | 2011 | `	if( nArg < 2 ){` |
|     ! 0 | 2012 | `		res = nArg == 0 ? 0 : 1;` |
|     ! 0 | 2013 | `		ph7_result_int(pCtx,res);` |
|     ! 0 | 2014 | `		return PH7_OK;` |
|       - | 2015 | `	}` |
|       - | 2016 | `	/* Perform the comparison */` |
|     115 | 2017 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     115 | 2018 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     115 | 2019 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|       - | 2020 | `	/* Comparison result */` |
|     115 | 2021 | `	ph7_result_int(pCtx,res);` |
|     115 | 2022 | `	return PH7_OK;` |
|      59 | 2023 | `}` |
|       - | 2024 | `/*` |
|       - | 2025 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|       - | 2026 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|       - | 2027 | ` * Parameter` |
|       - | 2028 | ` *  $str1: The first string` |
|       - | 2029 | ` *  $str2: The second string` |
|       - | 2030 | ` *  $len:  The length of strings to be used in the comparison.` |
|       - | 2031 | ` * Return` |
|       - | 2032 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|       - | 2033 | ` *  than str2, and 0 if they are equal.` |
|       - | 2034 | ` */` |
|     178 | 2035 | `PH7_PRIVATE int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2036 | `{` |
|       - | 2037 | `	const char *z1,*z2;` |
|       - | 2038 | `	int res;` |
|       - | 2039 | `	int n;` |
|     183 | 2040 | `	if( nArg < 3 ){` |
|       - | 2041 | `		/* Perform a standard comparison */` |
|     ! 0 | 2042 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|       - | 2043 | `	}` |
|       - | 2044 | `	/* Desired comparison length */` |
|     183 | 2045 | `	n  = ph7_value_to_int(apArg[2]);` |
|     183 | 2046 | `	if( n < 0 ){` |
|       - | 2047 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|       4 | 2048 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 2049 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|       1 | 2050 | `			ph7_function_name(pCtx));` |
|       - | 2051 | `	}` |
|       - | 2052 | `	/* Perform the comparison */` |
|     181 | 2053 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     181 | 2054 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     181 | 2055 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|       - | 2056 | `	/* Comparison result */` |
|     181 | 2057 | `	ph7_result_int(pCtx,res);` |
|     181 | 2058 | `	return PH7_OK;` |
|      94 | 2059 | `}` |
|       - | 2060 | `/*` |
|       - | 2061 | ` * Implode context [i.e: it's private data].` |
|       - | 2062 | ` * A pointer to the following structure is forwarded` |
|       - | 2063 | ` * verbatim to the array walker callback defined below.` |
|       - | 2064 | ` */` |
|       - | 2065 | `struct implode_data {` |
|       - | 2066 | `	ph7_context *pCtx;    /* Call context */` |
|       - | 2067 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|       - | 2068 | `	const char *zSep;     /* Arguments separator if any */` |
|       - | 2069 | `	int nSeplen;          /* Separator length */` |
|       - | 2070 | `	int bFirst;           /* TRUE if first call */` |
|       - | 2071 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|       - | 2072 | `	sxi32 rc;             /* Captured allocation rc; SXERR_MEM => the builtin raises an OOM fatal */` |
|       - | 2073 | `	sxi32 rcThrow;        /* Captured coercion throw; the builtin propagates it instead of a result */` |
|       - | 2074 | `};` |
|       - | 2075 | `/*` |
|       - | 2076 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|       - | 2077 | ` * The following routine is invoked for each array entry passed` |
|       - | 2078 | ` * to the implode() function.` |
|       - | 2079 | ` */` |
|  334482 | 2080 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|       5 | 2081 | `{` |
|  167241 | 2082 | `	SXUNUSED(pKey);` |
|  334487 | 2083 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|       - | 2084 | `	const char *zData;` |
|       - | 2085 | `	int nLen;` |
|  334487 | 2086 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|       3 | 2087 | `		if( pData->nSeplen > 0 ){` |
|       3 | 2088 | `			if( !pData->bFirst ){` |
|       - | 2089 | `				/* append the separator first */` |
|       3 | 2090 | `				if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|     ! 0 | 2091 | `					pData->rc = SXERR_MEM;` |
|     ! 0 | 2092 | `					return PH7_ABORT;` |
|       - | 2093 | `				}` |
|       2 | 2094 | `			}else{` |
|     ! 0 | 2095 | `				pData->bFirst = 0;` |
|       - | 2096 | `			}` |
|       1 | 2097 | `		}` |
|       - | 2098 | `		/* Recurse */` |
|       3 | 2099 | `		pData->bFirst = 1;` |
|       3 | 2100 | `		pData->nRecCount++;` |
|       3 | 2101 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|       3 | 2102 | `		pData->nRecCount--;` |
|       - | 2103 | `		/* Propagate an allocation failure surfaced deeper in the recursion. */` |
|       3 | 2104 | `		if( pData->rc != SXRET_OK ){` |
|     ! 0 | 2105 | `			return PH7_ABORT;` |
|       - | 2106 | `		}` |
|       3 | 2107 | `		return PH7_OK;` |
|       - | 2108 | `	}` |
|       - | 2109 | `	/* Extract the string representation of the entry value, USER-VISIBLY: an` |
|       - | 2110 | `	 * element that is itself an array renders as "Array" and warns, and one that` |
|       - | 2111 | `	 * is an object with no __toString() is php's catchable Error (it used to` |
|       - | 2112 | `	 * render as the literal "Object", §2). The walk cannot return a status, so` |
|       - | 2113 | `	 * park it on the context struct and abort. */` |
|       - | 2114 | `	{` |
|  334485 | 2115 | `		sxi32 rcSv = PH7_ValueToStringUV(pData->pCtx,pValue,&zData,&nLen);` |
|  334485 | 2116 | `		if( rcSv != SXRET_OK ){` |
|      18 | 2117 | `			pData->rcThrow = rcSv;` |
|      18 | 2118 | `			return PH7_ABORT;` |
|       - | 2119 | `		}` |
|       - | 2120 | `	}` |
|       - | 2121 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
|  334469 | 2122 | `	if( pData->bFirst ){` |
|   45339 | 2123 | `		pData->bFirst = 0;` |
|  311802 | 2124 | `	}else if( pData->nSeplen > 0 ){` |
|       - | 2125 | `		/* append the separator first */` |
|  287473 | 2126 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|     ! 0 | 2127 | `			pData->rc = SXERR_MEM;` |
|     ! 0 | 2128 | `			return PH7_ABORT;` |
|       - | 2129 | `		}` |
|  143734 | 2130 | `	}` |
|       - | 2131 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
|  334469 | 2132 | `	if( nLen > 0 ){` |
|  312891 | 2133 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|     ! 0 | 2134 | `			pData->rc = SXERR_MEM;` |
|     ! 0 | 2135 | `			return PH7_ABORT;` |
|       - | 2136 | `		}` |
|  156443 | 2137 | `	}` |
|  334469 | 2138 | `	return PH7_OK;` |
|  167246 | 2139 | `}` |
|       - | 2140 | `/*` |
|       - | 2141 | ` * string implode(string $glue,array $pieces,...)` |
|       - | 2142 | ` * string implode(array $pieces,...)` |
|       - | 2143 | ` *  Join array elements with a string.` |
|       - | 2144 | ` * $glue` |
|       - | 2145 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|       - | 2146 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|       - | 2147 | ` * $pieces` |
|       - | 2148 | ` *   The array of strings to implode.` |
|       - | 2149 | ` * Return` |
|       - | 2150 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|       - | 2151 | ` *  order, with the glue string between each element.` |
|       - | 2152 | ` */` |
|   45596 | 2153 | `PH7_PRIVATE int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2154 | `{` |
|       - | 2155 | `	struct implode_data imp_data;` |
|       - | 2156 | `	/*` |
|       - | 2157 | `` 	 * php's contract for implode()/join(): one function, a `array\|string $separator` `` |
|       - | 2158 | ``	 * and a `?array $array` row, and a body that resolves the two ARITIES. The`` |
|       - | 2159 | `	 * messages report the overload as already resolved -- an $array argument in` |
|       - | 2160 | ``	 * position #2 means #1 is the SEPARATOR and must be a `string`, never the`` |
|       - | 2161 | `	 * union (which is only what the two arities accept BETWEEN them), and an ARRAY` |
|       - | 2162 | `	 * in position #1 with a second argument is that same #1 error. One aBuiltinSig` |
|       - | 2163 | `	 * row cannot say either (it drives both the accepted set and the message),` |
|       - | 2164 | `	 * which is why implode/join sit on azSelfChecked[] with strtr() and check` |
|       - | 2165 | `	 * their own rows here.` |
|       - | 2166 | `	 *` |
|       - | 2167 | ``	 * The messages name the INVOKED function: php reports `join(): ...` for`` |
|       - | 2168 | ``	 * join(), where this builtin used to hardcode `implode(): ...`.`` |
|       - | 2169 | `	 *` |
|       - | 2170 | `	 * One divergence, twin-paired in` |
|       - | 2171 | `	 * 002-integration/function/implode_separator_type{,_zend}.phpt. php 8.5 words` |
|       - | 2172 | `	 * the three cases below through a SPECIALIZED handler that only a DIRECT,` |
|       - | 2173 | ``	 * compile-time-resolved `implode(...)` call reaches; join(),`` |
|       - | 2174 | ``	 * `$f='implode'; $f(...)` and call_user_func('implode', ...) fall back to a`` |
|       - | 2175 | ``	 * generic path that answers differently (`array\|string` for the first, a #2`` |
|       - | 2176 | `	 * error for the second, and "ab" for the third). It is a call-FORM` |
|       - | 2177 | `	 * specialization, not a semantic rule -- it does not change with opcache off` |
|       - | 2178 | `	 * -- so PHL gives every call form the one contract php's direct calls use,` |
|       - | 2179 | `	 * which is the form real code writes and the form the corpus pins.` |
|       - | 2180 | `	 */` |
|   45601 | 2181 | `	const char *zName = ph7_function_name(pCtx);` |
|   45601 | 2182 | `	int i = 1;` |
|   45601 | 2183 | `	if( nArg < 1 ){` |
|       - | 2184 | `		/* Missing argument,return NULL */` |
|     ! 0 | 2185 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2186 | `		return PH7_OK;` |
|       - | 2187 | `	}` |
|       - | 2188 | `	/* Prepare the implode context */` |
|   45601 | 2189 | `	imp_data.pCtx = pCtx;` |
|   45601 | 2190 | `	imp_data.bRecursive = 0;` |
|   45601 | 2191 | `	imp_data.bFirst = 1;` |
|   45601 | 2192 | `	imp_data.nRecCount = 0;` |
|   45601 | 2193 | `	imp_data.rc = SXRET_OK;` |
|   45601 | 2194 | `	imp_data.rcThrow = SXRET_OK;` |
|   45601 | 2195 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|   45579 | 2196 | `		if( apArg[0]->iFlags & MEMOBJ_NULL ){` |
|       - | 2197 | `			/* php only DEPRECATES null for the union parameter and coerces it to` |
|       - | 2198 | `			 * ""; PHL rejects it (§10 null-strictness), naming php's DECLARED type` |
|       - | 2199 | ``			 * -- the one case where `array\|string` is the right wording, because`` |
|       - | 2200 | `			 * php never narrows the union for a value it accepts. */` |
|     ! 0 | 2201 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     ! 0 | 2202 | `				"%s(): Argument #1 ($separator) must be of type array\|string, null given",zName);` |
|       - | 2203 | `		}` |
|   45579 | 2204 | `		if( nArg < 2 \|\| ph7_value_is_null(apArg[1]) ){` |
|       - | 2205 | ``			/* php: a string separator REQUIRES the array. `implode("x")` and`` |
|       - | 2206 | ``			 * `implode("x", null)` both answered "" -- the `?array` in php's`` |
|       - | 2207 | `			 * signature is the DEFAULT's type, not a value it accepts. */` |
|      17 | 2208 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 2209 | `				"%s(): If argument #1 ($separator) is of type string, "` |
|       5 | 2210 | `				"argument #2 ($array) must be of type array, null given",zName);` |
|       - | 2211 | `		}` |
|   45569 | 2212 | `		if( !PH7_ArgSatisfiesString(apArg[0]) ){` |
|       - | 2213 | `			/* The overload is resolved, so #1 is the separator and must be a` |
|       - | 2214 | `			 * STRING. PHL used to fall through to the central screen here and` |
|       - | 2215 | ``			 * report the whole `array\|string` union instead. */`` |
|       - | 2216 | `			char zBuf[64];` |
|      10 | 2217 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 2218 | `				"%s(): Argument #1 ($separator) must be of type string, %s given",` |
|       3 | 2219 | `				zName,VmValueGivenName(apArg[0],zBuf,sizeof(zBuf)));` |
|       - | 2220 | `		}` |
|   45563 | 2221 | `		if( !ph7_value_is_array(apArg[1]) ){` |
|       - | 2222 | `			/* php: implode($glue, $pieces) requires an ARRAY. PH7 stringified` |
|       - | 2223 | `			 * whatever it was handed, so implode(",", 5) quietly returned "5". */` |
|       - | 2224 | `			char zBuf[64];` |
|      12 | 2225 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 2226 | `				"%s(): Argument #2 ($array) must be of type ?array, %s given",` |
|       6 | 2227 | `				zName,VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|       - | 2228 | `		}` |
|   45557 | 2229 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|   22781 | 2230 | `	}else{` |
|      25 | 2231 | `		if( nArg > 1 ){` |
|       - | 2232 | `			/* php 8 removed the legacy swapped order: implode($pieces, $glue) is a` |
|       - | 2233 | `			 * TypeError whatever $glue holds (PHL used to swap silently, a wrong` |
|       - | 2234 | `			 * ANSWER when the caller meant php's signature). One array argument` |
|       - | 2235 | `			 * alone stays the legal ""-glue form. */` |
|      26 | 2236 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       8 | 2237 | `				"%s(): Argument #1 ($separator) must be of type string, array given",zName);` |
|       - | 2238 | `		}` |
|       9 | 2239 | `		imp_data.zSep = 0;` |
|       9 | 2240 | `		imp_data.nSeplen = 0;` |
|       9 | 2241 | `		i = 0;` |
|       - | 2242 | `	}` |
|   45563 | 2243 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|     ! 0 | 2244 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 2245 | `	}` |
|       - | 2246 | `	/* Start the 'join' process */` |
|   91105 | 2247 | `	while( i < nArg ){` |
|   45563 | 2248 | `		if( ph7_value_is_array(apArg[i]) ){` |
|       - | 2249 | `			/* Iterate throw array entries */` |
|   45563 | 2250 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|       - | 2251 | `			/* An element whose coercion threw ends the join with that throw */` |
|   45563 | 2252 | `			if( imp_data.rcThrow != SXRET_OK ){` |
|      18 | 2253 | `				return imp_data.rcThrow;` |
|       - | 2254 | `			}` |
|       - | 2255 | `			/* Surface a callback allocation failure as a fatal */` |
|   45547 | 2256 | `			if( imp_data.rc != SXRET_OK ){` |
|     ! 0 | 2257 | `				return PH7_ContextMemoryError(pCtx);` |
|       - | 2258 | `			}` |
|   22776 | 2259 | `		}else{` |
|       - | 2260 | `			const char *zData;` |
|       - | 2261 | `			int nLen;` |
|       - | 2262 | `			/* Extract the string representation of the ph7 value (user-visible) */` |
|     ! 0 | 2263 | `			sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[i],&zData,&nLen);` |
|     ! 0 | 2264 | `			if( rcSv != SXRET_OK ){` |
|     ! 0 | 2265 | `				return rcSv;` |
|       - | 2266 | `			}` |
|       - | 2267 | `			/* Manage separator insertion regardless of string length */` |
|     ! 0 | 2268 | `			if( imp_data.bFirst ){` |
|     ! 0 | 2269 | `				imp_data.bFirst = 0;` |
|     ! 0 | 2270 | `			}else if( imp_data.nSeplen > 0 ){` |
|     ! 0 | 2271 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|     ! 0 | 2272 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 2273 | `				}` |
|     ! 0 | 2274 | `			}` |
|       - | 2275 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|     ! 0 | 2276 | `			if( nLen > 0 ){` |
|     ! 0 | 2277 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|     ! 0 | 2278 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 2279 | `				}` |
|     ! 0 | 2280 | `			}` |
|       - | 2281 | `		}` |
|   45547 | 2282 | `		i++;` |
|       5 | 2283 | `	}` |
|   45547 | 2284 | `	return PH7_OK;` |
|   22803 | 2285 | `}` |
|       - | 2286 | `/*` |
|       - | 2287 | ` * Symisc eXtension:` |
|       - | 2288 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|       - | 2289 | ` * Purpose` |
|       - | 2290 | ` *  Same as implode() but recurse on arrays.` |
|       - | 2291 | ` * Example:` |
|       - | 2292 | ` *   $a = array('usr',array('home','dean'));` |
|       - | 2293 | ` *   echo implode_recursive("/",$a);` |
|       - | 2294 | ` *   Will output` |
|       - | 2295 | ` *     usr/home/dean.` |
|       - | 2296 | ` *   While the standard implode would produce.` |
|       - | 2297 | ` *    usr/Array.` |
|       - | 2298 | ` * Parameter` |
|       - | 2299 | ` *  Refer to implode().` |
|       - | 2300 | ` * Return` |
|       - | 2301 | ` *  Refer to implode().` |
|       - | 2302 | ` */` |
|      12 | 2303 | `PH7_PRIVATE int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 2304 | `{` |
|       - | 2305 | `	struct implode_data imp_data;` |
|      13 | 2306 | `	int i = 1;` |
|      13 | 2307 | `	if( nArg < 1 ){` |
|       - | 2308 | `		/* Missing argument,return NULL */` |
|       3 | 2309 | `		ph7_result_null(pCtx);` |
|       3 | 2310 | `		return PH7_OK;` |
|       - | 2311 | `	}` |
|       - | 2312 | `	/* Prepare the implode context */` |
|      11 | 2313 | `	imp_data.pCtx = pCtx;` |
|      11 | 2314 | `	imp_data.bRecursive = 1;` |
|      11 | 2315 | `	imp_data.bFirst = 1;` |
|      11 | 2316 | `	imp_data.nRecCount = 0;` |
|      11 | 2317 | `	imp_data.rc = SXRET_OK;` |
|      11 | 2318 | `	imp_data.rcThrow = SXRET_OK;` |
|      11 | 2319 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      11 | 2320 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|       6 | 2321 | `	}else{` |
|     ! 0 | 2322 | `		imp_data.zSep = 0;` |
|     ! 0 | 2323 | `		imp_data.nSeplen = 0;` |
|     ! 0 | 2324 | `		i = 0;` |
|       - | 2325 | `	}` |
|      11 | 2326 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|     ! 0 | 2327 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 2328 | `	}` |
|       - | 2329 | `	/* Start the 'join' process */` |
|      21 | 2330 | `	while( i < nArg ){` |
|      11 | 2331 | `		if( ph7_value_is_array(apArg[i]) ){` |
|       - | 2332 | `			/* Iterate throw array entries */` |
|       3 | 2333 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|       - | 2334 | `			/* An element whose coercion threw ends the join with that throw */` |
|       3 | 2335 | `			if( imp_data.rcThrow != SXRET_OK ){` |
|     ! 0 | 2336 | `				return imp_data.rcThrow;` |
|       - | 2337 | `			}` |
|       - | 2338 | `			/* Surface a callback allocation failure as a fatal */` |
|       3 | 2339 | `			if( imp_data.rc != SXRET_OK ){` |
|     ! 0 | 2340 | `				return PH7_ContextMemoryError(pCtx);` |
|       - | 2341 | `			}` |
|       2 | 2342 | `		}else{` |
|       - | 2343 | `			const char *zData;` |
|       - | 2344 | `			int nLen;` |
|       - | 2345 | `			/* Extract the string representation of the ph7 value (user-visible) */` |
|       9 | 2346 | `			sxi32 rcSv = PH7_ValueToStringUV(pCtx,apArg[i],&zData,&nLen);` |
|       9 | 2347 | `			if( rcSv != SXRET_OK ){` |
|     ! 0 | 2348 | `				return rcSv;` |
|       - | 2349 | `			}` |
|       - | 2350 | `			/* Manage separator insertion regardless of string length */` |
|       9 | 2351 | `			if( imp_data.bFirst ){` |
|       9 | 2352 | `				imp_data.bFirst = 0;` |
|       4 | 2353 | `			}else if( imp_data.nSeplen > 0 ){` |
|     ! 0 | 2354 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|     ! 0 | 2355 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 2356 | `				}` |
|     ! 0 | 2357 | `			}` |
|       - | 2358 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|       9 | 2359 | `			if( nLen > 0 ){` |
|       9 | 2360 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|     ! 0 | 2361 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 2362 | `				}` |
|       4 | 2363 | `			}` |
|       - | 2364 | `		}` |
|      11 | 2365 | `		i++;` |
|       1 | 2366 | `	}` |
|      11 | 2367 | `	return PH7_OK;` |
|       7 | 2368 | `}` |
|       - | 2369 | `/*` |
|       - | 2370 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|       - | 2371 | ` *  Returns an array of strings, each of which is a substring of string` |
|       - | 2372 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|       - | 2373 | ` * Parameters` |
|       - | 2374 | ` *  $delimiter` |
|       - | 2375 | ` *   The boundary string.` |
|       - | 2376 | ` * $string` |
|       - | 2377 | ` *   The input string.` |
|       - | 2378 | ` * $limit` |
|       - | 2379 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|       - | 2380 | ` *   of limit elements with the last element containing the rest of string.` |
|       - | 2381 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|       - | 2382 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|       - | 2383 | ` * Returns` |
|       - | 2384 | ` *  Returns an array of strings created by splitting the string parameter` |
|       - | 2385 | ` *  on boundaries formed by the delimiter.` |
|       - | 2386 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|       - | 2387 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|       - | 2388 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|       - | 2389 | ` *  will be returned.` |
|       - | 2390 | ` * NOTE:` |
|       - | 2391 | ` *  Negative limit is not supported.` |
|       - | 2392 | ` */` |
|  399258 | 2393 | `PH7_PRIVATE int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2394 | `{` |
|       - | 2395 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|       - | 2396 | `	int nDelim,nStrlen,iLimit;` |
|       - | 2397 | `	ph7_value *pArray;` |
|       - | 2398 | `	ph7_value *pValue;` |
|       - | 2399 | `	sxu32 nOfft;` |
|       - | 2400 | `	sxi32 rc;` |
|  399263 | 2401 | `	if( nArg < 2 ){` |
|       - | 2402 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 2403 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2404 | `		return PH7_OK;` |
|       - | 2405 | `	}` |
|       - | 2406 | `	/* Extract the delimiter */` |
|  399263 | 2407 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|  399263 | 2408 | `	if( nDelim < 1 ){` |
|       - | 2409 | `		/* Empty delimiter: PHP 8 throws a catchable ValueError. */` |
|       5 | 2410 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 2411 | `			"explode(): Argument #1 ($separator) must not be empty");` |
|       - | 2412 | `	}` |
|       - | 2413 | `	/* Extract the string */` |
|  399259 | 2414 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|  399259 | 2415 | `	if( nStrlen < 1 ){` |
|       - | 2416 | `		/* Empty string: normally an array with a single empty element (PHP behavior).` |
|       - | 2417 | `		 * A negative limit drops the last -limit components, so the sole empty` |
|       - | 2418 | `		 * component is dropped and the result is an empty array. */` |
|       7 | 2419 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|       7 | 2420 | `		if( pArrayTmp == 0 ){` |
|       - | 2421 | `			/* Out of memory,return FALSE */` |
|     ! 0 | 2422 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 2423 | `			return PH7_OK;` |
|       - | 2424 | `		}` |
|       7 | 2425 | `		if( !(nArg > 2 && ph7_value_to_int(apArg[2]) < 0) ){` |
|       5 | 2426 | `			ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|       5 | 2427 | `			if( pValueTmp == 0 ){` |
|       - | 2428 | `				/* Out of memory,return FALSE */` |
|     ! 0 | 2429 | `				ph7_result_bool(pCtx,0);` |
|     ! 0 | 2430 | `				return PH7_OK;` |
|       - | 2431 | `			}` |
|       5 | 2432 | `			ph7_value_string(pValueTmp, "", 0);` |
|       5 | 2433 | `			if( ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp) != SXRET_OK ){` |
|     ! 0 | 2434 | `				return PH7_ContextMemoryError(pCtx);` |
|       - | 2435 | `			}` |
|       2 | 2436 | `		}` |
|       7 | 2437 | `		ph7_result_value(pCtx, pArrayTmp);` |
|       7 | 2438 | `		return PH7_OK;` |
|       - | 2439 | `	}` |
|       - | 2440 | `	/* Point to the end of the string */` |
|  399253 | 2441 | `	zEnd = &zString[nStrlen];` |
|       - | 2442 | `	/* Create the array */` |
|  399253 | 2443 | `	pArray =  ph7_context_new_array(pCtx);` |
|  399253 | 2444 | `	pValue = ph7_context_new_scalar(pCtx);` |
|  399253 | 2445 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|       - | 2446 | `		/* Out of memory,return FALSE */` |
|     ! 0 | 2447 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 2448 | `		return PH7_OK;` |
|       - | 2449 | `	}` |
|       - | 2450 | `	/* Set a defualt limit */` |
|  399253 | 2451 | `	iLimit = SXI32_HIGH;` |
|  399253 | 2452 | `	if( nArg > 2 ){` |
|     103 | 2453 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     103 | 2454 | `		if( iLimit < 0 ){` |
|       - | 2455 | `			/* Negative limit: keep all components except the last -iLimit (PHP).` |
|       - | 2456 | `			 * Pre-count the components (delimiters + 1), then emit only the first` |
|       - | 2457 | `			 * nKeep CLEAN components — no trailing-remainder merge (the difference` |
|       - | 2458 | `			 * from the positive path). nKeep <= 0 drops everything -> empty array. */` |
|      17 | 2459 | `			int nTotal = 1,nKeep;` |
|      17 | 2460 | `			const char *zScan = zString;` |
|       - | 2461 | `			sxu32 nScanOfft;` |
|      57 | 2462 | `			while( SyBlobSearch(zScan,(sxu32)(zEnd - zScan),zDelim,nDelim,&nScanOfft) == SXRET_OK ){` |
|      41 | 2463 | `				nTotal++;` |
|      41 | 2464 | `				zScan = &zScan[nScanOfft + nDelim];` |
|       1 | 2465 | `			}` |
|      17 | 2466 | `			nKeep = nTotal + iLimit; /* iLimit < 0, so this is nTotal - (-iLimit) */` |
|      49 | 2467 | `			while( nKeep > (int)ph7_array_count(pArray)` |
|      39 | 2468 | `				&& SyBlobSearch(zString,(sxu32)(zEnd - zString),zDelim,nDelim,&nOfft) == SXRET_OK ){` |
|       - | 2469 | `				/* Emit the next clean component */` |
|      23 | 2470 | `				zCur = &zString[nOfft];` |
|      23 | 2471 | `				ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|      23 | 2472 | `				if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|     ! 0 | 2473 | `					return PH7_ContextMemoryError(pCtx);` |
|       - | 2474 | `				}` |
|      23 | 2475 | `				zString = &zCur[nDelim];` |
|      23 | 2476 | `				ph7_value_reset_string_cursor(pValue);` |
|       1 | 2477 | `			}` |
|      17 | 2478 | `			ph7_result_value(pCtx,pArray);` |
|      17 | 2479 | `			return PH7_OK;` |
|       - | 2480 | `		}` |
|      87 | 2481 | `		if( iLimit == 0 ){` |
|       5 | 2482 | `			iLimit = 1;` |
|       2 | 2483 | `		}` |
|      87 | 2484 | `		iLimit--;` |
|      41 | 2485 | `	}` |
|       - | 2486 | `	/* Start exploding */` |
|  757899 | 2487 | `	for(;;){` |
| 1515803 | 2488 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 1515803 | 2489 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|       - | 2490 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|  399237 | 2491 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|  399237 | 2492 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|     ! 0 | 2493 | `				return PH7_ContextMemoryError(pCtx);` |
|       - | 2494 | `			}` |
|  399237 | 2495 | `			break;` |
|       - | 2496 | `		}` |
|       - | 2497 | `		/* Point to the desired offset */` |
| 1116571 | 2498 | `		zCur = &zString[nOfft];` |
|       - | 2499 | `		/* Perform the store operation (may be empty) */` |
| 1116571 | 2500 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 1116571 | 2501 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|     ! 0 | 2502 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 2503 | `		}` |
|       - | 2504 | `		/* Point beyond the delimiter */` |
| 1116571 | 2505 | `		zString = &zCur[nDelim];` |
|       - | 2506 | `		/* Reset the cursor */` |
| 1116571 | 2507 | `		ph7_value_reset_string_cursor(pValue);` |
|       5 | 2508 | `	}` |
|       - | 2509 | `	/* Return the freshly created array */` |
|  399237 | 2510 | `	ph7_result_value(pCtx,pArray);` |
|       - | 2511 | `	/* NOTE that every allocated ph7_value will be automatically` |
|       - | 2512 | `	 * released as soon we return from this foregin function.` |
|       - | 2513 | `	 */` |
|  399237 | 2514 | `	return PH7_OK;` |
|  199634 | 2515 | `}` |
|       - | 2516 | `/*` |
|       - | 2517 | ` * string trim(string $str[,string $charlist ])` |
|       - | 2518 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|       - | 2519 | ` * Parameters` |
|       - | 2520 | ` *  $str` |
|       - | 2521 | ` *   The string that will be trimmed.` |
|       - | 2522 | ` * $charlist` |
|       - | 2523 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|       - | 2524 | ` *   Simply list all characters that you want to be stripped.` |
|       - | 2525 | ` *   With .. you can specify a range of characters.` |
|       - | 2526 | ` * Returns.` |
|       - | 2527 | ` *  Thr processed string.` |
|       - | 2528 | ` * NOTE:` |
|       - | 2529 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|       - | 2530 | ` */` |
|   27952 | 2531 | `PH7_PRIVATE int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2532 | `{` |
|   27957 | 2533 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"trim",1,"$string"); }` |
|       - | 2534 | `	const char *zString;` |
|       - | 2535 | `	int nLen;` |
|   27957 | 2536 | `	if( nArg < 1 ){` |
|       - | 2537 | `		/* Missing arguments,return null */` |
|     ! 0 | 2538 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2539 | `		return PH7_OK;` |
|       - | 2540 | `	}` |
|       - | 2541 | `	/* Extract the target string */` |
|   27957 | 2542 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|   27957 | 2543 | `	if( nLen < 1 ){` |
|       - | 2544 | `		/* Empty string,return */` |
|    9341 | 2545 | `		ph7_result_string(pCtx,"",0);` |
|    9341 | 2546 | `		return PH7_OK;` |
|       - | 2547 | `	}` |
|       - | 2548 | `	/* Start the trim process */` |
|   18621 | 2549 | `	if( nArg < 2 ){` |
|       - | 2550 | `		SyString sStr;` |
|       - | 2551 | `		/* Remove white spaces and NUL bytes */` |
|   18585 | 2552 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|   48229 | 2553 | `		SyStringFullTrimSafe(&sStr);` |
|   18585 | 2554 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|    9295 | 2555 | `	}else{` |
|       - | 2556 | `		/* Char list */` |
|       - | 2557 | `		const char *zList;` |
|       - | 2558 | `		int nListlen;` |
|      40 | 2559 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|      40 | 2560 | `		if( nListlen < 1 ){` |
|       - | 2561 | `			/* Return the string unchanged */` |
|       6 | 2562 | `			ph7_result_string(pCtx,zString,nLen);` |
|       4 | 2563 | `		}else{` |
|       - | 2564 | `			char aMask[256];` |
|      36 | 2565 | `			const char *zEnd = &zString[nLen];` |
|      36 | 2566 | `			const char *zCur = zString;` |
|      36 | 2567 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|       - | 2568 | `			/* Left trim */` |
|      92 | 2569 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|      60 | 2570 | `				zCur++;` |
|       4 | 2571 | `			}` |
|       - | 2572 | `			/* Right trim */` |
|      86 | 2573 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|      53 | 2574 | `				zEnd--;` |
|       3 | 2575 | `			}` |
|      36 | 2576 | `			if( zCur >= zEnd ){` |
|       - | 2577 | `				/* Return the empty string */` |
|     ! 0 | 2578 | `				ph7_result_string(pCtx,"",0);` |
|     ! 0 | 2579 | `			}else{` |
|      36 | 2580 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|       - | 2581 | `			}` |
|       - | 2582 | `		}` |
|       - | 2583 | `	}` |
|   18621 | 2584 | `	return PH7_OK;` |
|   13981 | 2585 | `}` |
|       - | 2586 | `/*` |
|       - | 2587 | ` * string rtrim(string $str[,string $charlist ])` |
|       - | 2588 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|       - | 2589 | ` * Parameters` |
|       - | 2590 | ` *  $str` |
|       - | 2591 | ` *   The string that will be trimmed.` |
|       - | 2592 | ` * $charlist` |
|       - | 2593 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|       - | 2594 | ` *   Simply list all characters that you want to be stripped.` |
|       - | 2595 | ` *   With .. you can specify a range of characters.` |
|       - | 2596 | ` * Returns.` |
|       - | 2597 | ` *  Thr processed string.` |
|       - | 2598 | ` * NOTE:` |
|       - | 2599 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|       - | 2600 | ` */` |
|     846 | 2601 | `PH7_PRIVATE int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2602 | `{` |
|     851 | 2603 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"rtrim",1,"$string"); }` |
|       - | 2604 | `	const char *zString;` |
|       - | 2605 | `	int nLen;` |
|     851 | 2606 | `	if( nArg < 1 ){` |
|       - | 2607 | `		/* Missing arguments,return null */` |
|     ! 0 | 2608 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2609 | `		return PH7_OK;` |
|       - | 2610 | `	}` |
|       - | 2611 | `	/* Extract the target string */` |
|     851 | 2612 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     851 | 2613 | `	if( nLen < 1 ){` |
|       - | 2614 | `		/* Empty string,return */` |
|      22 | 2615 | `		ph7_result_string(pCtx,"",0);` |
|      22 | 2616 | `		return PH7_OK;` |
|       - | 2617 | `	}` |
|       - | 2618 | `	/* Start the trim process */` |
|     831 | 2619 | `	if( nArg < 2 ){` |
|       - | 2620 | `		SyString sStr;` |
|       - | 2621 | `		/* Remove white spaces and NUL bytes*/` |
|      77 | 2622 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     195 | 2623 | `		SyStringRightTrimSafe(&sStr);` |
|      77 | 2624 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      39 | 2625 | `	}else{` |
|       - | 2626 | `		/* Char list */` |
|       - | 2627 | `		const char *zList;` |
|       - | 2628 | `		int nListlen;` |
|     755 | 2629 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     755 | 2630 | `		if( nListlen < 1 ){` |
|       - | 2631 | `			/* Return the string unchanged */` |
|     ! 0 | 2632 | `			ph7_result_string(pCtx,zString,nLen);` |
|     ! 0 | 2633 | `		}else{` |
|       - | 2634 | `			char aMask[256];` |
|     755 | 2635 | `			const char *zEnd = &zString[nLen];` |
|     755 | 2636 | `			const char *zCur = zString;` |
|     755 | 2637 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|       - | 2638 | `			/* Right trim */` |
|     915 | 2639 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     163 | 2640 | `				zEnd--;` |
|       3 | 2641 | `			}` |
|     755 | 2642 | `			if( zEnd <= zCur ){` |
|       - | 2643 | `				/* Return the empty string */` |
|      14 | 2644 | `				ph7_result_string(pCtx,"",0);` |
|       7 | 2645 | `			}else{` |
|     741 | 2646 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|       - | 2647 | `			}` |
|       - | 2648 | `		}` |
|       - | 2649 | `	}` |
|     831 | 2650 | `	return PH7_OK;` |
|     428 | 2651 | `}` |
|       - | 2652 | `/*` |
|       - | 2653 | ` * string ltrim(string $str[,string $charlist ])` |
|       - | 2654 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|       - | 2655 | ` * Parameters` |
|       - | 2656 | ` *  $str` |
|       - | 2657 | ` *   The string that will be trimmed.` |
|       - | 2658 | ` * $charlist` |
|       - | 2659 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|       - | 2660 | ` *   Simply list all characters that you want to be stripped.` |
|       - | 2661 | ` *   With .. you can specify a range of characters.` |
|       - | 2662 | ` * Returns.` |
|       - | 2663 | ` *  Thr processed string.` |
|       - | 2664 | ` * NOTE:` |
|       - | 2665 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|       - | 2666 | ` */` |
|     212 | 2667 | `PH7_PRIVATE int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2668 | `{` |
|     217 | 2669 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"ltrim",1,"$string"); }` |
|       - | 2670 | `	const char *zString;` |
|       - | 2671 | `	int nLen;` |
|     217 | 2672 | `	if( nArg < 1 ){` |
|       - | 2673 | `		/* Missing arguments,return null */` |
|     ! 0 | 2674 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2675 | `		return PH7_OK;` |
|       - | 2676 | `	}` |
|       - | 2677 | `	/* Extract the target string */` |
|     217 | 2678 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     217 | 2679 | `	if( nLen < 1 ){` |
|       - | 2680 | `		/* Empty string,return */` |
|      34 | 2681 | `		ph7_result_string(pCtx,"",0);` |
|      34 | 2682 | `		return PH7_OK;` |
|       - | 2683 | `	}` |
|       - | 2684 | `	/* Start the trim process */` |
|     187 | 2685 | `	if( nArg < 2 ){` |
|       - | 2686 | `		SyString sStr;` |
|       - | 2687 | `		/* Remove white spaces and NUL byte */` |
|       3 | 2688 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|       8 | 2689 | `		SyStringLeftTrimSafe(&sStr);` |
|       3 | 2690 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|       2 | 2691 | `	}else{` |
|       - | 2692 | `		/* Char list */` |
|       - | 2693 | `		const char *zList;` |
|       - | 2694 | `		int nListlen;` |
|     185 | 2695 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     185 | 2696 | `		if( nListlen < 1 ){` |
|       - | 2697 | `			/* Return the string unchanged */` |
|       3 | 2698 | `			ph7_result_string(pCtx,zString,nLen);` |
|       2 | 2699 | `		}else{` |
|       - | 2700 | `			char aMask[256];` |
|     183 | 2701 | `			const char *zEnd = &zString[nLen];` |
|     183 | 2702 | `			const char *zCur = zString;` |
|     183 | 2703 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|       - | 2704 | `			/* Left trim */` |
|     413 | 2705 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     235 | 2706 | `				zCur++;` |
|       5 | 2707 | `			}` |
|     183 | 2708 | `			if( zCur >= zEnd ){` |
|       - | 2709 | `				/* Return the empty string */` |
|     ! 0 | 2710 | `				ph7_result_string(pCtx,"",0);` |
|     ! 0 | 2711 | `			}else{` |
|     183 | 2712 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|       - | 2713 | `			}` |
|       - | 2714 | `		}` |
|       - | 2715 | `	}` |
|     187 | 2716 | `	return PH7_OK;` |
|     111 | 2717 | `}` |
|       - | 2718 | `/*` |
|       - | 2719 | ` * string strtolower(string $str)` |
|       - | 2720 | ` *  Make a string lowercase.` |
|       - | 2721 | ` * Parameters` |
|       - | 2722 | ` *  $str` |
|       - | 2723 | ` *   The input string.` |
|       - | 2724 | ` * Returns.` |
|       - | 2725 | ` *  The lowercased string.` |
|       - | 2726 | ` */` |
|   45470 | 2727 | `PH7_PRIVATE int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2728 | `{` |
|   45475 | 2729 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtolower",1,"$string"); }` |
|       - | 2730 | `	const char *zString,*zCur,*zEnd;` |
|       - | 2731 | `	int nLen;` |
|   45475 | 2732 | `	if( nArg < 1 ){` |
|       - | 2733 | `		/* Missing arguments,return null */` |
|     ! 0 | 2734 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2735 | `		return PH7_OK;` |
|       - | 2736 | `	}` |
|       - | 2737 | `	/* Extract the target string */` |
|   45475 | 2738 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|   45475 | 2739 | `	if( nLen < 1 ){` |
|       - | 2740 | `		/* Empty string,return */` |
|       6 | 2741 | `		ph7_result_string(pCtx,"",0);` |
|       6 | 2742 | `		return PH7_OK;` |
|       - | 2743 | `	}` |
|       - | 2744 | `	/* Perform the requested operation */` |
|   45471 | 2745 | `	zEnd = &zString[nLen];` |
|  152904 | 2746 | `	for(;;){` |
|  305813 | 2747 | `		if( zString >= zEnd ){` |
|       - | 2748 | `			/* No more input,break immediately */` |
|   45471 | 2749 | `			break;` |
|       - | 2750 | `		}` |
|  260347 | 2751 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|       - | 2752 | `			/* UTF-8 stream,output verbatim */` |
|       9 | 2753 | `			zCur = zString;` |
|       9 | 2754 | `			zString++;` |
|      13 | 2755 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|       5 | 2756 | `				zString++;` |
|       1 | 2757 | `			}` |
|       - | 2758 | `			/* Append UTF-8 stream */` |
|       9 | 2759 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|       5 | 2760 | `		}else{` |
|  260339 | 2761 | `			int c = zString[0];` |
|  260339 | 2762 | `			if( SyisUpper(c) ){` |
|  232897 | 2763 | `				c = SyToLower(zString[0]);` |
|  116446 | 2764 | `			}` |
|       - | 2765 | `			/* Append character */` |
|  260339 | 2766 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|       - | 2767 | `			/* Advance the cursor */` |
|  260339 | 2768 | `			zString++;` |
|       - | 2769 | `		}` |
|       5 | 2770 | `	}` |
|   45471 | 2771 | `	return PH7_OK;` |
|   22740 | 2772 | `}` |
|       - | 2773 | `/*` |
|       - | 2774 | ` * string strtolower(string $str)` |
|       - | 2775 | ` *  Make a string uppercase.` |
|       - | 2776 | ` * Parameters` |
|       - | 2777 | ` *  $str` |
|       - | 2778 | ` *   The input string.` |
|       - | 2779 | ` * Returns.` |
|       - | 2780 | ` *  The uppercased string.` |
|       - | 2781 | ` */` |
|     216 | 2782 | `PH7_PRIVATE int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2783 | `{` |
|     221 | 2784 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtoupper",1,"$string"); }` |
|       - | 2785 | `	const char *zString,*zCur,*zEnd;` |
|       - | 2786 | `	int nLen;` |
|     221 | 2787 | `	if( nArg < 1 ){` |
|       - | 2788 | `		/* Missing arguments,return null */` |
|     ! 0 | 2789 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2790 | `		return PH7_OK;` |
|       - | 2791 | `	}` |
|       - | 2792 | `	/* Extract the target string */` |
|     221 | 2793 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     221 | 2794 | `	if( nLen < 1 ){` |
|       - | 2795 | `		/* Empty string,return */` |
|       9 | 2796 | `		ph7_result_string(pCtx,"",0);` |
|       9 | 2797 | `		return PH7_OK;` |
|       - | 2798 | `	}` |
|       - | 2799 | `	/* Perform the requested operation */` |
|     215 | 2800 | `	zEnd = &zString[nLen];` |
|     590 | 2801 | `	for(;;){` |
|    1230 | 2802 | `		if( zString >= zEnd ){` |
|       - | 2803 | `			/* No more input,break immediately */` |
|     215 | 2804 | `			break;` |
|       - | 2805 | `		}` |
|    1020 | 2806 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|       - | 2807 | `			/* UTF-8 stream,output verbatim */` |
|       9 | 2808 | `			zCur = zString;` |
|       9 | 2809 | `			zString++;` |
|      13 | 2810 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|       5 | 2811 | `				zString++;` |
|       1 | 2812 | `			}` |
|       - | 2813 | `			/* Append UTF-8 stream */` |
|       9 | 2814 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|       5 | 2815 | `		}else{` |
|    1012 | 2816 | `			int c = zString[0];` |
|    1012 | 2817 | `			if( SyisLower(c) ){` |
|     850 | 2818 | `				c = SyToUpper(zString[0]);` |
|     406 | 2819 | `			}` |
|       - | 2820 | `			/* Append character */` |
|    1012 | 2821 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|       - | 2822 | `			/* Advance the cursor */` |
|    1012 | 2823 | `			zString++;` |
|       - | 2824 | `		}` |
|       5 | 2825 | `	}` |
|     215 | 2826 | `	return PH7_OK;` |
|     113 | 2827 | `}` |
|       - | 2828 | `/*` |
|       - | 2829 | ` * string ucfirst(string $str)` |
|       - | 2830 | ` *  Returns a string with the first character of str capitalized, if that` |
|       - | 2831 | ` *  character is alphabetic.` |
|       - | 2832 | ` * Parameters` |
|       - | 2833 | ` *  $str` |
|       - | 2834 | ` *   The input string.` |
|       - | 2835 | ` * Returns.` |
|       - | 2836 | ` *  The processed string.` |
|       - | 2837 | ` */` |
|      10 | 2838 | `PH7_PRIVATE int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2839 | `{` |
|       - | 2840 | `	const char *zString,*zEnd;` |
|       - | 2841 | `	int nLen,c;` |
|      12 | 2842 | `	if( nArg < 1 ){` |
|       - | 2843 | `		/* Missing arguments,return null */` |
|     ! 0 | 2844 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2845 | `		return PH7_OK;` |
|       - | 2846 | `	}` |
|       - | 2847 | `	/* Extract the target string */` |
|      12 | 2848 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      12 | 2849 | `	if( nLen < 1 ){` |
|       - | 2850 | `		/* Empty string,return */` |
|       6 | 2851 | `		ph7_result_string(pCtx,"",0);` |
|       6 | 2852 | `		return PH7_OK;` |
|       - | 2853 | `	}` |
|       - | 2854 | `	/* Perform the requested operation */` |
|       7 | 2855 | `	zEnd = &zString[nLen];` |
|       7 | 2856 | `	c = zString[0];` |
|       7 | 2857 | `	if( SyisLower(c) ){` |
|       5 | 2858 | `		c = SyToUpper(c);` |
|       2 | 2859 | `	}` |
|       - | 2860 | `	/* Append the first character */` |
|       7 | 2861 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|       7 | 2862 | `	zString++;` |
|       7 | 2863 | `	if( zString < zEnd ){` |
|       - | 2864 | `		/* Append the rest of the input verbatim */` |
|       7 | 2865 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|       3 | 2866 | `	}` |
|       7 | 2867 | `	return PH7_OK;` |
|       7 | 2868 | `}` |
|       - | 2869 | `/*` |
|       - | 2870 | ` * string lcfirst(string $str)` |
|       - | 2871 | ` *  Make a string's first character lowercase.` |
|       - | 2872 | ` * Parameters` |
|       - | 2873 | ` *  $str` |
|       - | 2874 | ` *   The input string.` |
|       - | 2875 | ` * Returns.` |
|       - | 2876 | ` *  The processed string.` |
|       - | 2877 | ` */` |
|       8 | 2878 | `PH7_PRIVATE int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 2879 | `{` |
|       - | 2880 | `	const char *zString,*zEnd;` |
|       - | 2881 | `	int nLen,c;` |
|      10 | 2882 | `	if( nArg < 1 ){` |
|       - | 2883 | `		/* Missing arguments,return null */` |
|     ! 0 | 2884 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2885 | `		return PH7_OK;` |
|       - | 2886 | `	}` |
|       - | 2887 | `	/* Extract the target string */` |
|      10 | 2888 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      10 | 2889 | `	if( nLen < 1 ){` |
|       - | 2890 | `		/* Empty string,return */` |
|       6 | 2891 | `		ph7_result_string(pCtx,"",0);` |
|       6 | 2892 | `		return PH7_OK;` |
|       - | 2893 | `	}` |
|       - | 2894 | `	/* Perform the requested operation */` |
|       5 | 2895 | `	zEnd = &zString[nLen];` |
|       5 | 2896 | `	c = zString[0];` |
|       5 | 2897 | `	if( SyisUpper(c) ){` |
|       3 | 2898 | `		c = SyToLower(c);` |
|       1 | 2899 | `	}` |
|       - | 2900 | `	/* Append the first character */` |
|       5 | 2901 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|       5 | 2902 | `	zString++;` |
|       5 | 2903 | `	if( zString < zEnd ){` |
|       - | 2904 | `		/* Append the rest of the input verbatim */` |
|       5 | 2905 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|       2 | 2906 | `	}` |
|       5 | 2907 | `	return PH7_OK;` |
|       6 | 2908 | `}` |
|       - | 2909 | `/*` |
|       - | 2910 | ` * int ord(string $string)` |
|       - | 2911 | ` *  Returns the ASCII value of the first character of string.` |
|       - | 2912 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|       - | 2913 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|       - | 2914 | ` * Parameters` |
|       - | 2915 | ` *  $string` |
|       - | 2916 | ` *   The input string.` |
|       - | 2917 | ` * Returns` |
|       - | 2918 | ` *  The ASCII value as an integer.` |
|       - | 2919 | ` */` |
|      90 | 2920 | `PH7_PRIVATE int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 2921 | `{` |
|       - | 2922 | `	const char *zString;` |
|       - | 2923 | `	int nLen,c;` |
|       - | 2924 | `	/* PHP requires exactly one argument. */` |
|      93 | 2925 | `	if( nArg != 1 ){` |
|     ! 0 | 2926 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2927 | `			"ArgumentCountError",` |
|       - | 2928 | `			"ord() expects exactly 1 argument, %d given",` |
|     ! 0 | 2929 | `			nArg` |
|       - | 2930 | `			);` |
|       - | 2931 | `	}` |
|       - | 2932 | `	/* php only DEPRECATES null here; PHL rejects it. */` |
|      93 | 2933 | `	if( ph7_value_is_null(apArg[0]) ){` |
|     ! 0 | 2934 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 2935 | `			"ord(): Argument #1 ($character) must be of type string, null given"` |
|       - | 2936 | `			);` |
|       - | 2937 | `	}` |
|       - | 2938 | `	/* Extract the target string */` |
|      93 | 2939 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      93 | 2940 | `	if( nLen < 1 ){` |
|       - | 2941 | `		/* php only DEPRECATES an empty string here; PHL rejects it. */` |
|       3 | 2942 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 2943 | `			"ord(): Argument #1 ($character) must not be empty"` |
|       - | 2944 | `			);` |
|       - | 2945 | `	}` |
|       - | 2946 | `	/* A string longer than one byte: php DEPRECATES it; PHL rejects it. */` |
|      91 | 2947 | `	if( nLen > 1 ){` |
|       3 | 2948 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 2949 | `			"ord(): Argument #1 ($character) must be a single byte, use ord($str[0]) instead"` |
|       - | 2950 | `			);` |
|       - | 2951 | `	}` |
|       - | 2952 | `	/* Extract the ASCII value of the first character */` |
|      89 | 2953 | `	c = (unsigned char)zString[0];` |
|       - | 2954 | `	/* Return that value */` |
|      89 | 2955 | `	ph7_result_int(pCtx,c);` |
|      89 | 2956 | `	return PH7_OK;` |
|      48 | 2957 | `}` |
|       - | 2958 | `/*` |
|       - | 2959 | ` * string chr(int $codepoint)` |
|       - | 2960 | ` *  Returns a one-character string containing the character specified` |
|       - | 2961 | ` *  by the given codepoint, which must be in the [0, 255] range.` |
|       - | 2962 | ` * Parameters` |
|       - | 2963 | ` *  $codepoint` |
|       - | 2964 | ` *   An integer codepoint in [0, 255]. php merely deprecates values` |
|       - | 2965 | ` *   outside that range (constraining them with % 256); PHL rejects` |
|       - | 2966 | ` *   them with a ValueError (scope policy).` |
|       - | 2967 | ` * Returns` |
|       - | 2968 | ` *  A single-character string.` |
|       - | 2969 | ` */` |
|   30674 | 2970 | `PH7_PRIVATE int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 2971 | `{` |
|       - | 2972 | `	int c;` |
|       - | 2973 | `	unsigned char ch;` |
|       - | 2974 | `	/* PHP requires exactly one argument. */` |
|   30679 | 2975 | `	if( nArg != 1 ){` |
|     ! 0 | 2976 | `		return PH7_VmThrowException(pCtx,` |
|       - | 2977 | `			"ArgumentCountError",` |
|       - | 2978 | `			"chr() expects exactly 1 argument, %d given",` |
|     ! 0 | 2979 | `			nArg` |
|       - | 2980 | `			);` |
|       - | 2981 | `	}` |
|       - | 2982 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|       - | 2983 | `	 * PHP does not prefix this message with "chr():", so we call` |
|       - | 2984 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|       - | 2985 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|   30679 | 2986 | `	if( ph7_value_is_float(apArg[0]) ){` |
|     ! 0 | 2987 | `		double d = ph7_value_to_double(apArg[0]);` |
|       - | 2988 | ``		/* The range test comes FIRST: `(sxi64)d` is undefined outside it, and a`` |
|       - | 2989 | `		 * test of an undefined cast is one an optimiser may delete. NaN and both` |
|       - | 2990 | `		 * infinities fail it, which is the answer wanted anyway. */` |
|     ! 0 | 2991 | `		if( !PH7_RealFitsInt64(d) \|\| d != (double)(sxi64)d ){` |
|       - | 2992 | `			/* php only DEPRECATES a lossy float->int here; PHL rejects it. */` |
|     ! 0 | 2993 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 2994 | `				"chr(): Argument #1 ($codepoint) must be of type int, float given");` |
|       - | 2995 | `		}` |
|     ! 0 | 2996 | `	}` |
|       - | 2997 | `	/* Extract the codepoint. */` |
|   30679 | 2998 | `	c = ph7_value_to_int(apArg[0]);` |
|       - | 2999 | `	/* php only DEPRECATES an out-of-range codepoint (constraining it with % 256);` |
|       - | 3000 | `	 * PHL targets php's non-deprecated surface and rejects it loudly, matching the` |
|       - | 3001 | `	 * lossy-float branch above. This was the last engine site still emitting` |
|       - | 3002 | `	 * E_DEPRECATED — the scope policy says none remain. */` |
|   30679 | 3003 | `	if( c < 0 \|\| c > 255 ){` |
|       5 | 3004 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 3005 | `			"chr(): Argument #1 ($codepoint) must be between 0 and 255");` |
|       - | 3006 | `	}` |
|       - | 3007 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|       - | 3008 | `	 * when taking the address of a wider int. */` |
|   30675 | 3009 | `	ch = (unsigned char)(c & 0xFF);` |
|       - | 3010 | `	/* Return the specified character */` |
|   30675 | 3011 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|   30675 | 3012 | `	return PH7_OK;` |
|   15519 | 3013 | `}` |
|       - | 3014 | `/*` |
|       - | 3015 | ` * Binary to hex consumer callback.` |
|       - | 3016 | ` * This callback is the default consumer used by the hash functions` |
|       - | 3017 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|       - | 3018 | ` */` |
|   17800 | 3019 | `PH7_PRIVATE int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|       5 | 3020 | `{` |
|       - | 3021 | `	/* Append hex chunk verbatim */` |
|   17805 | 3022 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   17805 | 3023 | `	return SXRET_OK;` |
|       5 | 3024 | `}` |
|       - | 3025 |  |
|       - | 3026 | `/*` |
|       - | 3027 | ` * string bin2hex(string $str)` |
|       - | 3028 | ` *  Convert binary data into hexadecimal representation.` |
|       - | 3029 | ` * Parameters` |
|       - | 3030 | ` *  $str` |
|       - | 3031 | ` *   The input string.` |
|       - | 3032 | ` * Returns.` |
|       - | 3033 | ` *  Returns the hexadecimal representation of the given string.` |
|       - | 3034 | ` */` |
|    1678 | 3035 | `PH7_PRIVATE int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3036 | `{` |
|       - | 3037 | `	const char *zString;` |
|       - | 3038 | `	int nLen;` |
|       - | 3039 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|    1683 | 3040 | `	if( nArg != 1 ){` |
|     ! 0 | 3041 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3042 | `			"ArgumentCountError",` |
|       - | 3043 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|     ! 0 | 3044 | `			nArg` |
|       - | 3045 | `			);` |
|       - | 3046 | `	}` |
|       - | 3047 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|       - | 3048 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|       - | 3049 | `	 * Objects without __toString() must also raise a TypeError.` |
|       - | 3050 | `	 */` |
|    2522 | 3051 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|     839 | 3052 | `		( ph7_value_is_object(apArg[0]) &&` |
|     ! 0 | 3053 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|     ! 0 | 3054 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|     ! 0 | 3055 | `			"__toString",sizeof("__toString")-1) == 0` |
|       - | 3056 | `		)` |
|       - | 3057 | `	){` |
|     ! 0 | 3058 | `		const char *zType = ph7_type_name(apArg[0]);` |
|     ! 0 | 3059 | `		if( ph7_value_is_object(apArg[0]) ){` |
|     ! 0 | 3060 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     ! 0 | 3061 | `			if( pInst && pInst->pClass ){` |
|     ! 0 | 3062 | `				zType = SyStringData(&pInst->pClass->sName);` |
|     ! 0 | 3063 | `			}` |
|     ! 0 | 3064 | `		}` |
|     ! 0 | 3065 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3066 | `			"TypeError",` |
|       - | 3067 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|     ! 0 | 3068 | `			zType` |
|       - | 3069 | `			);` |
|       - | 3070 | `	}` |
|       - | 3071 | `	/* Extract the target string */` |
|    1683 | 3072 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    1683 | 3073 | `	if( nLen < 1 ){` |
|       - | 3074 | `		/* Empty string,return */` |
|      96 | 3075 | `		ph7_result_string(pCtx,"",0);` |
|      96 | 3076 | `		return PH7_OK;` |
|       - | 3077 | `	}` |
|       - | 3078 | `	/* Perform the requested operation */` |
|    1589 | 3079 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|    1589 | 3080 | `	return PH7_OK;` |
|     844 | 3081 | `}` |
|       - | 3082 |  |
|       - | 3083 | `/* Search callback signature */` |
|       - | 3084 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|       - | 3085 | `/*` |
|       - | 3086 | ` * Case-insensitive pattern match.` |
|       - | 3087 | ` * Brute force is the default search method used here.` |
|       - | 3088 | ` * This is due to the fact that brute-forcing works quite` |
|       - | 3089 | ` * well for short/medium texts on modern hardware.` |
|       - | 3090 | ` */` |
|     234 | 3091 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|       5 | 3092 | `{` |
|     239 | 3093 | `	const char *zpIn = (const char *)pPattern;` |
|     239 | 3094 | `	const char *zIn = (const char *)pText;` |
|     239 | 3095 | `	const char *zpEnd = &zpIn[iPatLen];` |
|     239 | 3096 | `	const char *zEnd = &zIn[nLen];` |
|       - | 3097 | `	const char *zPtr,*zPtr2;` |
|       - | 3098 | `	int c,d;` |
|     239 | 3099 | `	if( iPatLen > nLen ){` |
|       - | 3100 | `		/* Don't bother processing */` |
|      26 | 3101 | `		return SXERR_NOTFOUND;` |
|       - | 3102 | `	}` |
|    2265 | 3103 | `	for(;;){` |
|    4733 | 3104 | `		if( zIn >= zEnd ){` |
|     107 | 3105 | `			break;` |
|       - | 3106 | `		}` |
|    4627 | 3107 | `		c = SyToLower(zIn[0]);` |
|    4627 | 3108 | `		d = SyToLower(zpIn[0]);` |
|    4627 | 3109 | `		if( c == d ){` |
|     340 | 3110 | `			zPtr   = &zIn[1];` |
|     340 | 3111 | `			zPtr2  = &zpIn[1];` |
|     830 | 3112 | `			for(;;){` |
|    1311 | 3113 | `				if( zPtr2 >= zpEnd ){` |
|       - | 3114 | `					/* Pattern found */` |
|     108 | 3115 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     108 | 3116 | `					return SXRET_OK;` |
|       - | 3117 | `				}` |
|    1208 | 3118 | `				if( zPtr >= zEnd ){` |
|       2 | 3119 | `					break;` |
|       - | 3120 | `				}` |
|    1206 | 3121 | `				c = SyToLower(zPtr[0]);` |
|    1206 | 3122 | `				d = SyToLower(zPtr2[0]);` |
|    1206 | 3123 | `				if( c != d ){` |
|     231 | 3124 | `					break;` |
|       - | 3125 | `				}` |
|     976 | 3126 | `				zPtr++; zPtr2++;` |
|       5 | 3127 | `			}` |
|     113 | 3128 | `		}` |
|    4520 | 3129 | `		zIn++;` |
|       1 | 3130 | `	}` |
|       - | 3131 | `	/* Pattern not found */` |
|     107 | 3132 | `	return SXERR_NOTFOUND;` |
|     122 | 3133 | `}` |
|       - | 3134 | `/*` |
|       - | 3135 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|       - | 3136 | ` *  Find the first occurrence of a string.` |
|       - | 3137 | ` * Parameters` |
|       - | 3138 | ` *  $haystack` |
|       - | 3139 | ` *   The input string.` |
|       - | 3140 | ` * $needle` |
|       - | 3141 | ` *   Search pattern (must be a string).` |
|       - | 3142 | ` * $before_needle` |
|       - | 3143 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|       - | 3144 | ` *   of the needle (excluding the needle).` |
|       - | 3145 | ` * Return` |
|       - | 3146 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|       - | 3147 | ` */` |
|      44 | 3148 | `PH7_PRIVATE int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3149 | `{` |
|      46 | 3150 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|       - | 3151 | `	const char *zBlob,*zPattern;` |
|       - | 3152 | `	int nLen,nPatLen;` |
|       - | 3153 | `	sxu32 nOfft;` |
|       - | 3154 | `	sxi32 rc;` |
|      46 | 3155 | `	if( nArg < 2 ){` |
|       - | 3156 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3157 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3158 | `		return PH7_OK;` |
|       - | 3159 | `	}` |
|       - | 3160 | `	/* Extract the needle and the haystack */` |
|      46 | 3161 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      46 | 3162 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      46 | 3163 | `	nOfft = 0; /* cc warning */` |
|      46 | 3164 | `	if( nPatLen < 1 ){` |
|       - | 3165 | `		/* php 8: the empty needle matches at position 0, so the whole haystack` |
|       - | 3166 | `		 * is returned (and nothing at all when $before_needle is set). */` |
|       7 | 3167 | `		if( nArg > 2 && ph7_value_to_bool(apArg[2]) ){` |
|       3 | 3168 | `			ph7_result_string(pCtx,"",0);` |
|       2 | 3169 | `		}else{` |
|       5 | 3170 | `			ph7_result_string(pCtx,zBlob,nLen);` |
|       - | 3171 | `		}` |
|       7 | 3172 | `		return PH7_OK;` |
|       - | 3173 | `	}` |
|      40 | 3174 | `	if( nLen > 0 ){` |
|      40 | 3175 | `		int before = 0;` |
|       - | 3176 | `		/* Perform the lookup */` |
|      40 | 3177 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      40 | 3178 | `		if( rc != SXRET_OK ){` |
|       - | 3179 | `			/* Pattern not found,return FALSE */` |
|       3 | 3180 | `			ph7_result_bool(pCtx,0);` |
|       3 | 3181 | `			return PH7_OK;` |
|       - | 3182 | `		}` |
|       - | 3183 | `		/* Return the portion of the string */` |
|      38 | 3184 | `		if( nArg > 2 ){` |
|      30 | 3185 | `			before = ph7_value_to_int(apArg[2]);` |
|      14 | 3186 | `		}` |
|      38 | 3187 | `		if( before ){` |
|      30 | 3188 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      16 | 3189 | `		}else{` |
|       9 | 3190 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|       - | 3191 | `		}` |
|      20 | 3192 | `	}else{` |
|     ! 0 | 3193 | `		ph7_result_bool(pCtx,0);` |
|       - | 3194 | `	}` |
|      38 | 3195 | `	return PH7_OK;` |
|      24 | 3196 | `}` |
|       - | 3197 | `/*` |
|       - | 3198 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|       - | 3199 | ` *  Case-insensitive strstr().` |
|       - | 3200 | ` * Parameters` |
|       - | 3201 | ` *  $haystack` |
|       - | 3202 | ` *   The input string.` |
|       - | 3203 | ` * $needle` |
|       - | 3204 | ` *   Search pattern (must be a string).` |
|       - | 3205 | ` * $before_needle` |
|       - | 3206 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|       - | 3207 | ` *   of the needle (excluding the needle).` |
|       - | 3208 | ` * Return` |
|       - | 3209 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|       - | 3210 | ` */` |
|       6 | 3211 | `PH7_PRIVATE int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3212 | `{` |
|       7 | 3213 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|       - | 3214 | `	const char *zBlob,*zPattern;` |
|       - | 3215 | `	int nLen,nPatLen;` |
|       - | 3216 | `	sxu32 nOfft;` |
|       - | 3217 | `	sxi32 rc;` |
|       7 | 3218 | `	if( nArg < 2 ){` |
|       - | 3219 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3220 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3221 | `		return PH7_OK;` |
|       - | 3222 | `	}` |
|       - | 3223 | `	/* Extract the needle and the haystack */` |
|       7 | 3224 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|       7 | 3225 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|       7 | 3226 | `	nOfft = 0; /* cc warning */` |
|       7 | 3227 | `	if( nPatLen < 1 ){` |
|       - | 3228 | `		/* php 8: the empty needle matches at position 0, so the whole haystack` |
|       - | 3229 | `		 * is returned (and nothing at all when $before_needle is set). */` |
|       3 | 3230 | `		if( nArg > 2 && ph7_value_to_bool(apArg[2]) ){` |
|     ! 0 | 3231 | `			ph7_result_string(pCtx,"",0);` |
|     ! 0 | 3232 | `		}else{` |
|       3 | 3233 | `			ph7_result_string(pCtx,zBlob,nLen);` |
|       - | 3234 | `		}` |
|       3 | 3235 | `		return PH7_OK;` |
|       - | 3236 | `	}` |
|       5 | 3237 | `	if( nLen > 0 ){` |
|       5 | 3238 | `		int before = 0;` |
|       - | 3239 | `		/* Perform the lookup */` |
|       5 | 3240 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|       5 | 3241 | `		if( rc != SXRET_OK ){` |
|       - | 3242 | `			/* Pattern not found,return FALSE */` |
|     ! 0 | 3243 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 3244 | `			return PH7_OK;` |
|       - | 3245 | `		}` |
|       - | 3246 | `		/* Return the portion of the string */` |
|       5 | 3247 | `		if( nArg > 2 ){` |
|       3 | 3248 | `			before = ph7_value_to_int(apArg[2]);` |
|       1 | 3249 | `		}` |
|       5 | 3250 | `		if( before ){` |
|       3 | 3251 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|       2 | 3252 | `		}else{` |
|       3 | 3253 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|       - | 3254 | `		}` |
|       3 | 3255 | `	}else{` |
|     ! 0 | 3256 | `		ph7_result_bool(pCtx,0);` |
|       - | 3257 | `	}` |
|       5 | 3258 | `	return PH7_OK;` |
|       4 | 3259 | `}` |
|       - | 3260 | `/*` |
|       - | 3261 | ` * Resolve the $offset argument shared by strpos()/stripos().` |
|       - | 3262 | ` *` |
|       - | 3263 | ` * php requires -strlen($haystack) <= $offset <= strlen($haystack) and throws` |
|       - | 3264 | ` * ValueError otherwise; a negative offset counts back from the end. PHL used to` |
|       - | 3265 | ` * negate a negative offset and silently clamp an out-of-range one to zero, so` |
|       - | 3266 | ` * strpos("Hello","l",100) answered 2 where php raises — an argument error` |
|       - | 3267 | ` * turned into a wrong answer.` |
|       - | 3268 | ` *` |
|       - | 3269 | ` * On success *pnStart receives the resolved non-negative offset.` |
|       - | 3270 | ` */` |
|      10 | 3271 | `static sxi32 StrSearchOffset(` |
|       - | 3272 | `	ph7_context *pCtx,` |
|       - | 3273 | `	ph7_value *pArg,` |
|       - | 3274 | `	int nLen,` |
|       - | 3275 | `	const char *zFunc,` |
|       - | 3276 | `	int *pnStart` |
|       - | 3277 | `	)` |
|       1 | 3278 | `{` |
|      11 | 3279 | `	ph7_int64 iOfft = ph7_value_to_int64(pArg);` |
|       - | 3280 | `	/* Compare without negating iOfft: -INT64_MIN would overflow. */` |
|      11 | 3281 | `	if( iOfft < 0 ? (iOfft < -(ph7_int64)nLen) : (iOfft > (ph7_int64)nLen) ){` |
|       8 | 3282 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       4 | 3283 | `			"%s(): Argument #3 ($offset) must be contained in argument #1 ($haystack)",zFunc);` |
|       - | 3284 | `	}` |
|      11 | 3285 | `	*pnStart = (int)(iOfft < 0 ? (ph7_int64)nLen + iOfft : iOfft);` |
|      11 | 3286 | `	return PH7_OK;` |
|      10 | 3287 | `}` |
|       - | 3288 | `/*` |
|       - | 3289 | ` * Resolve the window of match START positions for strrpos()/strripos().` |
|       - | 3290 | ` *` |
|       - | 3291 | ` * php's rule is asymmetric in the sign of $offset: a non-negative offset is a` |
|       - | 3292 | ` * LOWER bound on where the match may start, while a negative one is an UPPER` |
|       - | 3293 | ` * bound counted back from the end of the haystack (zend_memnrstr). The range` |
|       - | 3294 | ` * check is the same as StrSearchOffset()'s.` |
|       - | 3295 | ` *` |
|       - | 3296 | ` * On success the closed interval [*pnMin,*pnMax] holds every position at which` |
|       - | 3297 | ` * a match is allowed to begin; it is empty (max < min) when the needle cannot` |
|       - | 3298 | ` * fit, which the caller reports as FALSE.` |
|       - | 3299 | ` */` |
|     656 | 3300 | `static sxi32 StrRSearchWindow(` |
|       - | 3301 | `	ph7_context *pCtx,` |
|       - | 3302 | `	ph7_value *pArg, /* The $offset argument, or NULL when it was omitted */` |
|       - | 3303 | `	int nLen,` |
|       - | 3304 | `	int nPatLen,` |
|       - | 3305 | `	const char *zFunc,` |
|       - | 3306 | `	int *pnMin,` |
|       - | 3307 | `	int *pnMax` |
|       - | 3308 | `	)` |
|       5 | 3309 | `{` |
|     661 | 3310 | `	int nMin = 0;` |
|     661 | 3311 | `	int nMax = nLen - nPatLen;` |
|     661 | 3312 | `	if( pArg ){` |
|      47 | 3313 | `		ph7_int64 iOfft = ph7_value_to_int64(pArg);` |
|      47 | 3314 | `		if( iOfft < 0 ? (iOfft < -(ph7_int64)nLen) : (iOfft > (ph7_int64)nLen) ){` |
|      33 | 3315 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      14 | 3316 | `				"%s(): Argument #3 ($offset) must be contained in argument #1 ($haystack)",zFunc);` |
|       - | 3317 | `		}` |
|      29 | 3318 | `		if( iOfft < 0 ){` |
|      15 | 3319 | `			int nLimit = nLen + (int)iOfft;` |
|      15 | 3320 | `			if( nMax > nLimit ){` |
|      15 | 3321 | `				nMax = nLimit;` |
|       7 | 3322 | `			}` |
|       8 | 3323 | `		}else{` |
|      15 | 3324 | `			nMin = (int)iOfft;` |
|       - | 3325 | `		}` |
|      14 | 3326 | `	}` |
|     643 | 3327 | `	*pnMin = nMin;` |
|     643 | 3328 | `	*pnMax = nMax;` |
|     643 | 3329 | `	return PH7_OK;` |
|     328 | 3330 | `}` |
|       - | 3331 | `/*` |
|       - | 3332 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|       - | 3333 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|       - | 3334 | ` * Parameters` |
|       - | 3335 | ` *  $haystack` |
|       - | 3336 | ` *   The input string.` |
|       - | 3337 | ` * $needle` |
|       - | 3338 | ` *   Search pattern (must be a string).` |
|       - | 3339 | ` * $offset` |
|       - | 3340 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|       - | 3341 | ` *   to start searching. The position returned is still relative to the beginning` |
|       - | 3342 | ` *   of haystack.` |
|       - | 3343 | ` * Return` |
|       - | 3344 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|       - | 3345 | ` */` |
|    3780 | 3346 | `PH7_PRIVATE int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3347 | `{` |
|    3785 | 3348 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strpos",1,"$haystack"); }` |
|    3785 | 3349 | `	if( nArg > 1 ){ StrNullArgNotice(pCtx,apArg[1],"strpos",2,"$needle"); }` |
|    3785 | 3350 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|       - | 3351 | `	const char *zBlob,*zPattern;` |
|       - | 3352 | `	int nLen,nPatLen,nStart;` |
|       - | 3353 | `	sxu32 nOfft;` |
|       - | 3354 | `	sxi32 rc;` |
|    3785 | 3355 | `	if( nArg < 2 ){` |
|       - | 3356 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3357 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3358 | `		return PH7_OK;` |
|       - | 3359 | `	}` |
|       - | 3360 | `	/* Extract the needle and the haystack */` |
|    3785 | 3361 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    3785 | 3362 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    3785 | 3363 | `	nOfft = 0; /* cc warning */` |
|    3785 | 3364 | `	nStart = 0;` |
|       - | 3365 | `	/* Peek the starting offset if available */` |
|    3785 | 3366 | `	if( nArg > 2 ){` |
|       7 | 3367 | `		rc = StrSearchOffset(pCtx,apArg[2],nLen,"strpos",&nStart);` |
|       7 | 3368 | `		if( rc != PH7_OK ){` |
|     ! 0 | 3369 | `			return rc;` |
|       - | 3370 | `		}` |
|       3 | 3371 | `	}` |
|    3785 | 3372 | `	if( nPatLen < 1 ){` |
|       - | 3373 | `		/* php 8 treats the empty needle as matching at the search offset. */` |
|      11 | 3374 | `		ph7_result_int64(pCtx,(ph7_int64)nStart);` |
|      11 | 3375 | `		return PH7_OK;` |
|       - | 3376 | `	}` |
|    3775 | 3377 | `	zBlob += nStart;` |
|    3775 | 3378 | `	nLen -= nStart;` |
|    3775 | 3379 | `	if( nLen > 0 ){` |
|       - | 3380 | `		/* Perform the lookup */` |
|    3733 | 3381 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    3733 | 3382 | `		if( rc != SXRET_OK ){` |
|       - | 3383 | `			/* Pattern not found,return FALSE */` |
|    3123 | 3384 | `			ph7_result_bool(pCtx,0);` |
|    3123 | 3385 | `			return PH7_OK;` |
|       - | 3386 | `		}` |
|       - | 3387 | `		/* Return the pattern position */` |
|     615 | 3388 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|     310 | 3389 | `	}else{` |
|      44 | 3390 | `		ph7_result_bool(pCtx,0);` |
|       - | 3391 | `	}` |
|     657 | 3392 | `	return PH7_OK;` |
|    1895 | 3393 | `}` |
|       - | 3394 | `/*` |
|       - | 3395 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|       - | 3396 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|       - | 3397 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|       - | 3398 | ` * TypeError for arrays, resources, and objects without __toString.` |
|       - | 3399 | ` *` |
|       - | 3400 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|       - | 3401 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|       - | 3402 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|       - | 3403 | ` *` |
|       - | 3404 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|       - | 3405 | ` * is valid until pTmp is released or pArg is mutated.` |
|       - | 3406 | ` */` |
|   15772 | 3407 | `static sxi32 StrPredicateResolveArg(` |
|       - | 3408 | `	ph7_context *pCtx,` |
|       - | 3409 | `	ph7_value *pArg,` |
|       - | 3410 | `	const char *zFunc,` |
|       - | 3411 | `	int iArgNum,` |
|       - | 3412 | `	const char *zParamName,` |
|       - | 3413 | `	const char *zTypeStr, /* Declared type in the TypeError, e.g. "string" / "?string" */` |
|       - | 3414 | `	const char *zNullMsg,` |
|       - | 3415 | `	ph7_value *pTmp,` |
|       - | 3416 | `	const char **pzOut,` |
|       - | 3417 | `	int *pnOut` |
|       5 | 3418 | `){` |
|    7885 | 3419 | `	SXUNUSED(zNullMsg); /* php's deprecation text — PHL rejects null instead of coercing */` |
|   15777 | 3420 | `	if( ph7_value_is_null(pArg) ){` |
|       - | 3421 | `		/* php only DEPRECATES null here; PHL rejects it with the TypeError php will` |
|       - | 3422 | `		 * eventually raise. */` |
|     ! 0 | 3423 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 3424 | `			"%s(): Argument #%d (%s) must be of type %s, null given",` |
|     ! 0 | 3425 | `			zFunc,iArgNum,zParamName,zTypeStr);` |
|       - | 3426 | `	}` |
|   23688 | 3427 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|   15772 | 3428 | `	    ( ph7_value_is_object(pArg) &&` |
|      72 | 3429 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|      48 | 3430 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|      24 | 3431 | `	        "__toString",sizeof("__toString")-1) == 0` |
|       - | 3432 | `	    )` |
|       - | 3433 | `	){` |
|     ! 0 | 3434 | `		const char *zType = ph7_type_name(pArg);` |
|     ! 0 | 3435 | `		if( ph7_value_is_object(pArg) ){` |
|     ! 0 | 3436 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     ! 0 | 3437 | `			if( pInst && pInst->pClass ){` |
|     ! 0 | 3438 | `				zType = SyStringData(&pInst->pClass->sName);` |
|     ! 0 | 3439 | `			}` |
|     ! 0 | 3440 | `		}` |
|     ! 0 | 3441 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3442 | `			"TypeError",` |
|       - | 3443 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|     ! 0 | 3444 | `			zFunc, iArgNum, zParamName, zTypeStr, zType` |
|       - | 3445 | `			);` |
|       - | 3446 | `	}` |
|   15777 | 3447 | `	if( ph7_value_is_object(pArg) ){` |
|      49 | 3448 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|      49 | 3449 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|       - | 3450 | `			"__toString",sizeof("__toString")-1);` |
|      49 | 3451 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|      49 | 3452 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|      49 | 3453 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|      49 | 3454 | `		return PH7_OK;` |
|       - | 3455 | `	}` |
|   15729 | 3456 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|   15729 | 3457 | `	return PH7_OK;` |
|    7890 | 3458 | `}` |
|       - | 3459 | `/*` |
|       - | 3460 | ` * bool str_contains(string $haystack, string $needle)` |
|       - | 3461 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|       - | 3462 | ` * Return` |
|       - | 3463 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|       - | 3464 | ` */` |
|    7149 | 3465 | `PH7_PRIVATE int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3466 | `{` |
|       - | 3467 | `	const char *zHaystack,*zNeedle;` |
|       - | 3468 | `	int nHayLen,nNeedleLen;` |
|       - | 3469 | `	ph7_value sHayTmp,sNeedleTmp;` |
|       - | 3470 | `	sxi32 rc;` |
|    7154 | 3471 | `	if( nArg != 2 ){` |
|     ! 0 | 3472 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3473 | `			"ArgumentCountError",` |
|       - | 3474 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|     ! 0 | 3475 | `			nArg` |
|       - | 3476 | `			);` |
|       - | 3477 | `	}` |
|    7154 | 3478 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|    7154 | 3479 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|    7154 | 3480 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack","string",` |
|       - | 3481 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|       - | 3482 | `		"of type string is deprecated",` |
|       - | 3483 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|    7154 | 3484 | `	if( rc != PH7_OK ) goto out;` |
|    7154 | 3485 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle","string",` |
|       - | 3486 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|       - | 3487 | `		"of type string is deprecated",` |
|       - | 3488 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|    7154 | 3489 | `	if( rc != PH7_OK ) goto out;` |
|    7154 | 3490 | `	if( nNeedleLen < 1 ){` |
|      11 | 3491 | `		ph7_result_bool(pCtx,1);` |
|    7149 | 3492 | `	}else if( nHayLen < nNeedleLen ){` |
|      24 | 3493 | `		ph7_result_bool(pCtx,0);` |
|      17 | 3494 | `	}else{` |
|   10674 | 3495 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|    3553 | 3496 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|    7121 | 3497 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|       - | 3498 | `	}` |
|    7154 | 3499 | `	rc = PH7_OK;` |
|    3575 | 3500 | `out:` |
|    7154 | 3501 | `	PH7_MemObjRelease(&sHayTmp);` |
|    7154 | 3502 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|    7154 | 3503 | `	return rc;` |
|    3579 | 3504 | `}` |
|       - | 3505 | `/*` |
|       - | 3506 | ` * bool str_starts_with(string $haystack, string $needle)` |
|       - | 3507 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|       - | 3508 | ` * Return` |
|       - | 3509 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|       - | 3510 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|       - | 3511 | ` */` |
|     554 | 3512 | `PH7_PRIVATE int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3513 | `{` |
|       - | 3514 | `	const char *zHaystack,*zNeedle;` |
|       - | 3515 | `	int nHayLen,nNeedleLen;` |
|       - | 3516 | `	ph7_value sHayTmp,sNeedleTmp;` |
|       - | 3517 | `	sxi32 rc;` |
|     559 | 3518 | `	if( nArg != 2 ){` |
|     ! 0 | 3519 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3520 | `			"ArgumentCountError",` |
|       - | 3521 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|     ! 0 | 3522 | `			nArg` |
|       - | 3523 | `			);` |
|       - | 3524 | `	}` |
|     559 | 3525 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     559 | 3526 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     559 | 3527 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack","string",` |
|       - | 3528 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|       - | 3529 | `		"of type string is deprecated",` |
|       - | 3530 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     559 | 3531 | `	if( rc != PH7_OK ) goto out;` |
|     559 | 3532 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle","string",` |
|       - | 3533 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|       - | 3534 | `		"of type string is deprecated",` |
|       - | 3535 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     559 | 3536 | `	if( rc != PH7_OK ) goto out;` |
|     559 | 3537 | `	if( nNeedleLen < 1 ){` |
|      11 | 3538 | `		ph7_result_bool(pCtx,1);` |
|     554 | 3539 | `	}else if( nHayLen < nNeedleLen ){` |
|       7 | 3540 | `		ph7_result_bool(pCtx,0);` |
|       4 | 3541 | `	}else{` |
|     812 | 3542 | `		ph7_result_bool(pCtx,` |
|     538 | 3543 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|       - | 3544 | `	}` |
|     559 | 3545 | `	rc = PH7_OK;` |
|     277 | 3546 | `out:` |
|     559 | 3547 | `	PH7_MemObjRelease(&sHayTmp);` |
|     559 | 3548 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     559 | 3549 | `	return rc;` |
|     282 | 3550 | `}` |
|       - | 3551 | `/*` |
|       - | 3552 | ` * bool str_ends_with(string $haystack, string $needle)` |
|       - | 3553 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|       - | 3554 | ` * Return` |
|       - | 3555 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|       - | 3556 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|       - | 3557 | ` */` |
|      60 | 3558 | `PH7_PRIVATE int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3559 | `{` |
|       - | 3560 | `	const char *zHaystack,*zNeedle;` |
|       - | 3561 | `	int nHayLen,nNeedleLen;` |
|       - | 3562 | `	ph7_value sHayTmp,sNeedleTmp;` |
|       - | 3563 | `	sxi32 rc;` |
|      61 | 3564 | `	if( nArg != 2 ){` |
|     ! 0 | 3565 | `		return PH7_VmThrowException(pCtx,` |
|       - | 3566 | `			"ArgumentCountError",` |
|       - | 3567 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|     ! 0 | 3568 | `			nArg` |
|       - | 3569 | `			);` |
|       - | 3570 | `	}` |
|      61 | 3571 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|      61 | 3572 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|      61 | 3573 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack","string",` |
|       - | 3574 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|       - | 3575 | `		"of type string is deprecated",` |
|       - | 3576 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|      61 | 3577 | `	if( rc != PH7_OK ) goto out;` |
|      61 | 3578 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle","string",` |
|       - | 3579 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|       - | 3580 | `		"of type string is deprecated",` |
|       - | 3581 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|      61 | 3582 | `	if( rc != PH7_OK ) goto out;` |
|      61 | 3583 | `	if( nNeedleLen < 1 ){` |
|      11 | 3584 | `		ph7_result_bool(pCtx,1);` |
|      56 | 3585 | `	}else if( nHayLen < nNeedleLen ){` |
|       7 | 3586 | `		ph7_result_bool(pCtx,0);` |
|       4 | 3587 | `	}else{` |
|      67 | 3588 | `		ph7_result_bool(pCtx,` |
|      44 | 3589 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|       - | 3590 | `	}` |
|      61 | 3591 | `	rc = PH7_OK;` |
|      30 | 3592 | `out:` |
|      61 | 3593 | `	PH7_MemObjRelease(&sHayTmp);` |
|      61 | 3594 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|      61 | 3595 | `	return rc;` |
|      31 | 3596 | `}` |
|       - | 3597 | `/*` |
|       - | 3598 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|       - | 3599 | ` *  Case-insensitive strpos.` |
|       - | 3600 | ` * Parameters` |
|       - | 3601 | ` *  $haystack` |
|       - | 3602 | ` *   The input string.` |
|       - | 3603 | ` * $needle` |
|       - | 3604 | ` *   Search pattern (must be a string).` |
|       - | 3605 | ` * $offset` |
|       - | 3606 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|       - | 3607 | ` *   to start searching. The position returned is still relative to the beginning` |
|       - | 3608 | ` *   of haystack.` |
|       - | 3609 | ` * Return` |
|       - | 3610 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|       - | 3611 | ` */` |
|     106 | 3612 | `PH7_PRIVATE int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3613 | `{` |
|     111 | 3614 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|       - | 3615 | `	const char *zBlob,*zPattern;` |
|       - | 3616 | `	int nLen,nPatLen,nStart;` |
|       - | 3617 | `	sxu32 nOfft;` |
|       - | 3618 | `	sxi32 rc;` |
|     111 | 3619 | `	if( nArg < 2 ){` |
|       - | 3620 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3621 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3622 | `		return PH7_OK;` |
|       - | 3623 | `	}` |
|       - | 3624 | `	/* Extract the needle and the haystack */` |
|     111 | 3625 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     111 | 3626 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     111 | 3627 | `	nOfft = 0; /* cc warning */` |
|     111 | 3628 | `	nStart = 0;` |
|       - | 3629 | `	/* Peek the starting offset if available */` |
|     111 | 3630 | `	if( nArg > 2 ){` |
|       5 | 3631 | `		rc = StrSearchOffset(pCtx,apArg[2],nLen,"stripos",&nStart);` |
|       5 | 3632 | `		if( rc != PH7_OK ){` |
|     ! 0 | 3633 | `			return rc;` |
|       - | 3634 | `		}` |
|       2 | 3635 | `	}` |
|     111 | 3636 | `	if( nPatLen < 1 ){` |
|       - | 3637 | `		/* php 8 treats the empty needle as matching at the search offset. */` |
|       3 | 3638 | `		ph7_result_int64(pCtx,(ph7_int64)nStart);` |
|       3 | 3639 | `		return PH7_OK;` |
|       - | 3640 | `	}` |
|     109 | 3641 | `	zBlob += nStart;` |
|     109 | 3642 | `	nLen -= nStart;` |
|     109 | 3643 | `	if( nLen > 0 ){` |
|       - | 3644 | `		/* Perform the lookup */` |
|     101 | 3645 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     101 | 3646 | `		if( rc != SXRET_OK ){` |
|       - | 3647 | `			/* Pattern not found,return FALSE */` |
|      52 | 3648 | `			ph7_result_bool(pCtx,0);` |
|      52 | 3649 | `			return PH7_OK;` |
|       - | 3650 | `		}` |
|       - | 3651 | `		/* Return the pattern position */` |
|      50 | 3652 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      33 | 3653 | `	}else{` |
|       8 | 3654 | `		ph7_result_bool(pCtx,0);` |
|       - | 3655 | `	}` |
|      58 | 3656 | `	return PH7_OK;` |
|      58 | 3657 | `}` |
|       - | 3658 | `/*` |
|       - | 3659 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|       - | 3660 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|       - | 3661 | ` * Parameters` |
|       - | 3662 | ` *  $haystack` |
|       - | 3663 | ` *   The input string.` |
|       - | 3664 | ` * $needle` |
|       - | 3665 | ` *   Search pattern (must be a string).` |
|       - | 3666 | ` * $offset` |
|       - | 3667 | ` *   If specified, search will start this number of characters counted from the beginning` |
|       - | 3668 | ` *   of the string. If the value is negative, search will instead start from that many` |
|       - | 3669 | ` *   characters from the end of the string, searching backwards.` |
|       - | 3670 | ` * Return` |
|       - | 3671 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|       - | 3672 | ` */` |
|     612 | 3673 | `PH7_PRIVATE int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 3674 | `{` |
|       - | 3675 | `	const char *zBlob,*zPattern;` |
|     617 | 3676 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|       - | 3677 | `	int nLen,nPatLen,i;` |
|     617 | 3678 | `	int nMin = 0,nMax = 0;` |
|       - | 3679 | `	sxu32 nOfft;` |
|       - | 3680 | `	sxi32 rc;` |
|     617 | 3681 | `	if( nArg < 2 ){` |
|       - | 3682 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3683 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3684 | `		return PH7_OK;` |
|       - | 3685 | `	}` |
|       - | 3686 | `	/* Extract the needle and the haystack */` |
|     617 | 3687 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     617 | 3688 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     617 | 3689 | `	nOfft = 0; /* cc warning */` |
|       - | 3690 | `	/* Resolve the range of positions the match may start at */` |
|     617 | 3691 | `	rc = StrRSearchWindow(pCtx,nArg > 2 ? apArg[2] : 0,nLen,nPatLen,"strrpos",&nMin,&nMax);` |
|     617 | 3692 | `	if( rc != PH7_OK ){` |
|       5 | 3693 | `		return rc;` |
|       - | 3694 | `	}` |
|     613 | 3695 | `	if( nPatLen < 1 ){` |
|       - | 3696 | `		/* php 8: the empty needle matches everywhere, so the LAST match is the` |
|       - | 3697 | `		 * highest position the window allows. */` |
|      11 | 3698 | `		ph7_result_int64(pCtx,(ph7_int64)nMax);` |
|      11 | 3699 | `		return PH7_OK;` |
|       - | 3700 | `	}` |
|       - | 3701 | `	/* Walk backwards, comparing at each candidate position. Searching a window` |
|       - | 3702 | `	 * exactly as long as the needle makes the match test an equality test while` |
|       - | 3703 | `	 * still going through xPatternMatch, which carries the case folding. */` |
|    2641 | 3704 | `	for( i = nMax ; i >= nMin ; --i ){` |
|    2621 | 3705 | `		rc = xPatternMatch((const void *)&zBlob[i],(sxu32)nPatLen,(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    2621 | 3706 | `		if( rc == SXRET_OK ){` |
|       - | 3707 | `			/* Pattern found,return it's position */` |
|     583 | 3708 | `			ph7_result_int64(pCtx,(ph7_int64)i);` |
|     583 | 3709 | `			return PH7_OK;` |
|       - | 3710 | `		}` |
|    1024 | 3711 | `	}` |
|       - | 3712 | `	/* Pattern not found,return FALSE */` |
|      21 | 3713 | `	ph7_result_bool(pCtx,0);` |
|      21 | 3714 | `	return PH7_OK;` |
|     311 | 3715 | `}` |
|       - | 3716 | `/*` |
|       - | 3717 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|       - | 3718 | ` *  Case-insensitive strrpos.` |
|       - | 3719 | ` * Parameters` |
|       - | 3720 | ` *  $haystack` |
|       - | 3721 | ` *   The input string.` |
|       - | 3722 | ` * $needle` |
|       - | 3723 | ` *   Search pattern (must be a string).` |
|       - | 3724 | ` * $offset` |
|       - | 3725 | ` *   If specified, search will start this number of characters counted from the beginning` |
|       - | 3726 | ` *   of the string. If the value is negative, search will instead start from that many` |
|       - | 3727 | ` *   characters from the end of the string, searching backwards.` |
|       - | 3728 | ` * Return` |
|       - | 3729 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|       - | 3730 | ` */` |
|      34 | 3731 | `PH7_PRIVATE int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3732 | `{` |
|       - | 3733 | `	const char *zBlob,*zPattern;` |
|      35 | 3734 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|       - | 3735 | `	int nLen,nPatLen,i;` |
|      35 | 3736 | `	int nMin = 0,nMax = 0;` |
|       - | 3737 | `	sxu32 nOfft;` |
|       - | 3738 | `	sxi32 rc;` |
|      35 | 3739 | `	if( nArg < 2 ){` |
|       - | 3740 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3741 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3742 | `		return PH7_OK;` |
|       - | 3743 | `	}` |
|       - | 3744 | `	/* Extract the needle and the haystack */` |
|      35 | 3745 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      35 | 3746 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      35 | 3747 | `	nOfft = 0; /* cc warning */` |
|       - | 3748 | `	/* Resolve the range of positions the match may start at */` |
|      35 | 3749 | `	rc = StrRSearchWindow(pCtx,nArg > 2 ? apArg[2] : 0,nLen,nPatLen,"strripos",&nMin,&nMax);` |
|      35 | 3750 | `	if( rc != PH7_OK ){` |
|       5 | 3751 | `		return rc;` |
|       - | 3752 | `	}` |
|      31 | 3753 | `	if( nPatLen < 1 ){` |
|       - | 3754 | `		/* php 8: the empty needle matches everywhere, so the LAST match is the` |
|       - | 3755 | `		 * highest position the window allows. */` |
|      11 | 3756 | `		ph7_result_int64(pCtx,(ph7_int64)nMax);` |
|      11 | 3757 | `		return PH7_OK;` |
|       - | 3758 | `	}` |
|       - | 3759 | `	/* Walk backwards, comparing at each candidate position (see strrpos). */` |
|      49 | 3760 | `	for( i = nMax ; i >= nMin ; --i ){` |
|      45 | 3761 | `		rc = xPatternMatch((const void *)&zBlob[i],(sxu32)nPatLen,(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|      45 | 3762 | `		if( rc == SXRET_OK ){` |
|       - | 3763 | `			/* Pattern found,return it's position */` |
|      17 | 3764 | `			ph7_result_int64(pCtx,(ph7_int64)i);` |
|      17 | 3765 | `			return PH7_OK;` |
|       - | 3766 | `		}` |
|      15 | 3767 | `	}` |
|       - | 3768 | `	/* Pattern not found,return FALSE */` |
|       5 | 3769 | `	ph7_result_bool(pCtx,0);` |
|       5 | 3770 | `	return PH7_OK;` |
|      18 | 3771 | `}` |
|       - | 3772 | `/*` |
|       - | 3773 | ` * int strrchr(string $haystack,mixed $needle)` |
|       - | 3774 | ` *  Find the last occurrence of a character in a string.` |
|       - | 3775 | ` * Parameters` |
|       - | 3776 | ` *  $haystack` |
|       - | 3777 | ` *   The input string.` |
|       - | 3778 | ` * $needle` |
|       - | 3779 | ` *  If needle contains more than one character, only the first is used.` |
|       - | 3780 | ` *  This behavior is different from that of strstr().` |
|       - | 3781 | ` *  If needle is not a string, it is converted to an integer and applied` |
|       - | 3782 | ` *  as the ordinal value of a character.` |
|       - | 3783 | ` * Return` |
|       - | 3784 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|       - | 3785 | ` */` |
|      68 | 3786 | `PH7_PRIVATE int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 3787 | `{` |
|       - | 3788 | `	const char *zBlob;` |
|       - | 3789 | `	int nLen,c;` |
|      69 | 3790 | `	if( nArg < 2 ){` |
|       - | 3791 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 3792 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 3793 | `		return PH7_OK;` |
|       - | 3794 | `	}` |
|       - | 3795 | `	/* Extract the haystack */` |
|      69 | 3796 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      69 | 3797 | `	c = 0; /* cc warning */` |
|      69 | 3798 | `	if( nLen > 0 ){` |
|       - | 3799 | `		const char *zPattern;` |
|       - | 3800 | `		int nPatLen;` |
|       - | 3801 | `		sxu32 nOfft;` |
|       - | 3802 | `		sxi32 rc;` |
|       - | 3803 | `		/* php 8 casts the needle to string and uses only its first character.` |
|       - | 3804 | `		 * The old "if not a string, take it as an ordinal" reading was php 7` |
|       - | 3805 | `		 * behaviour, removed in php 8: strrchr("hello world",111) now looks for` |
|       - | 3806 | `		 * "1", not "o". An empty needle matches nothing. */` |
|      65 | 3807 | `		zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      65 | 3808 | `		if( nPatLen < 1 ){` |
|       5 | 3809 | `			ph7_result_bool(pCtx,0);` |
|      22 | 3810 | `			return PH7_OK;` |
|       - | 3811 | `		}` |
|      61 | 3812 | `		c = zPattern[0];` |
|       - | 3813 | `		/* Perform the lookup */` |
|      61 | 3814 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|      61 | 3815 | `		if( rc != SXRET_OK ){` |
|       - | 3816 | `			/* No such entry,return FALSE */` |
|      11 | 3817 | `			ph7_result_bool(pCtx,0);` |
|      11 | 3818 | `			return PH7_OK;` |
|       - | 3819 | `		}` |
|       - | 3820 | `		/* php 8.3's $before_needle: TRUE answers everything in FRONT of the last` |
|       - | 3821 | `		 * occurrence instead of the occurrence and everything after it. It was` |
|       - | 3822 | `		 * declared in aBuiltinSig[], screened as a bool and then never read, so` |
|       - | 3823 | ``		 * `strrchr($path, '/', true)` -- the ordinary way to take a dirname off a`` |
|       - | 3824 | `		 * delimiter -- answered the BASENAME, with the delimiter still on it. */` |
|      51 | 3825 | `		if( nArg > 2 && ph7_value_to_bool(apArg[2]) ){` |
|      25 | 3826 | `			ph7_result_string(pCtx,zBlob,(int)nOfft);` |
|      25 | 3827 | `			return PH7_OK;` |
|       - | 3828 | `		}` |
|       - | 3829 | `		/* Return the string portion */` |
|      27 | 3830 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      14 | 3831 | `	}else{` |
|       5 | 3832 | `		ph7_result_bool(pCtx,0);` |
|       - | 3833 | `	}` |
|      31 | 3834 | `	return PH7_OK;` |
|      35 | 3835 | `}` |
|       - | 3836 | `/*` |
|       - | 3837 | ` * string strrev(string $string)` |
|       - | 3838 | ` *  Reverse a string.` |
|       - | 3839 | ` * Parameters` |
|       - | 3840 | ` *  $string` |
|       - | 3841 | ` *   String to be reversed.` |
|       - | 3842 | ` * Return` |
|       - | 3843 | ` *  The reversed string.` |
|       - | 3844 | ` */` |
|      16 | 3845 | `PH7_PRIVATE int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3846 | `{` |
|       - | 3847 | `	const char *zIn,*zEnd;` |
|       - | 3848 | `	int nLen,c;` |
|      18 | 3849 | `	if( nArg < 1 ){` |
|       - | 3850 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3851 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3852 | `		return PH7_OK;` |
|       - | 3853 | `	}` |
|       - | 3854 | `	/* Extract the target string */` |
|      18 | 3855 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      18 | 3856 | `	if( nLen < 1 ){` |
|       - | 3857 | `		/* php answers the empty STRING here, not null */` |
|       3 | 3858 | `		ph7_result_string(pCtx,"",0);` |
|       3 | 3859 | `		return PH7_OK;` |
|       - | 3860 | `	}` |
|       - | 3861 | `	/* Perform the requested operation */` |
|      16 | 3862 | `	zEnd = &zIn[nLen - 1];` |
|      25 | 3863 | `	for(;;){` |
|      52 | 3864 | `		if( zEnd < zIn ){` |
|       - | 3865 | `			/* No more input to process */` |
|      16 | 3866 | `			break;` |
|       - | 3867 | `		}` |
|       - | 3868 | `		/* Append current character */` |
|      38 | 3869 | `		c = zEnd[0];` |
|      38 | 3870 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      38 | 3871 | `		zEnd--;` |
|       2 | 3872 | `	}` |
|      16 | 3873 | `	return PH7_OK;` |
|      10 | 3874 | `}` |
|       - | 3875 | `/*` |
|       - | 3876 | ` * string ucwords(string $string [, string $separators = " \t\r\n\f\v"])` |
|       - | 3877 | ` *  Uppercase the first character of each word in a string.` |
|       - | 3878 | ` *  A word begins at the start of the string and after any character present in` |
|       - | 3879 | ` *  $separators. The default separators are the whitespace characters (space,` |
|       - | 3880 | ` *  horizontal tab, carriage return, newline, form-feed and vertical tab); an` |
|       - | 3881 | ` *  explicit $separators argument REPLACES them (an empty string leaves only the` |
|       - | 3882 | ` *  very first character upper-cased). Like PHP, this is byte-based: only ASCII` |
|       - | 3883 | ` *  bytes are upper-cased and a byte is a separator only if it appears in the set.` |
|       - | 3884 | ` * Parameters` |
|       - | 3885 | ` *  $string` |
|       - | 3886 | ` *   The input string.` |
|       - | 3887 | ` *  $separators` |
|       - | 3888 | ` *   The optional word-boundary characters.` |
|       - | 3889 | ` * Return` |
|       - | 3890 | ` *  The modified string.` |
|       - | 3891 | ` */` |
|      26 | 3892 | `PH7_PRIVATE int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 3893 | `{` |
|       - | 3894 | `	const char *zIn;` |
|       - | 3895 | `	int nLen,i,iStart;` |
|       - | 3896 | `	char aDelim[256];` |
|      28 | 3897 | `	if( nArg < 1 ){` |
|       - | 3898 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3899 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3900 | `		return PH7_OK;` |
|       - | 3901 | `	}` |
|       - | 3902 | `	/* Build the separator membership table: an explicit $separators argument` |
|       - | 3903 | `	 * replaces the default whitespace set (an empty string clears it). */` |
|      28 | 3904 | `	SyZero(aDelim,(sxu32)sizeof(aDelim));` |
|      28 | 3905 | `	if( nArg > 1 ){` |
|       - | 3906 | `		int nDelim;` |
|       9 | 3907 | `		const char *zDelim = ph7_value_to_string(apArg[1],&nDelim);` |
|      17 | 3908 | `		for( i = 0 ; i < nDelim ; i++ ){` |
|       9 | 3909 | `			aDelim[(unsigned char)zDelim[i]] = 1;` |
|       5 | 3910 | `		}` |
|       5 | 3911 | `	}else{` |
|      20 | 3912 | `		aDelim[(unsigned char)' ']  = 1;` |
|      20 | 3913 | `		aDelim[(unsigned char)'\t'] = 1;` |
|      20 | 3914 | `		aDelim[(unsigned char)'\r'] = 1;` |
|      20 | 3915 | `		aDelim[(unsigned char)'\n'] = 1;` |
|      20 | 3916 | `		aDelim[(unsigned char)'\f'] = 1;` |
|      20 | 3917 | `		aDelim[(unsigned char)'\v'] = 1;` |
|       - | 3918 | `	}` |
|       - | 3919 | `	/* Extract the target string */` |
|      28 | 3920 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      28 | 3921 | `	if( nLen < 1 ){` |
|       - | 3922 | `		/* Empty string – match PHP semantics */` |
|       6 | 3923 | `		ph7_result_string(pCtx,"",0);` |
|       6 | 3924 | `		return PH7_OK;` |
|       - | 3925 | `	}` |
|       - | 3926 | `	/* Upper-case the first byte of each word (the leading byte, or any byte that` |
|       - | 3927 | `	 * follows a separator), appending the untouched runs in between verbatim. */` |
|      23 | 3928 | `	iStart = 0;` |
|     325 | 3929 | `	for( i = 0 ; i < nLen ; i++ ){` |
|     303 | 3930 | `		int c = (unsigned char)zIn[i];` |
|     303 | 3931 | `		if( (i == 0 \|\| aDelim[(unsigned char)zIn[i-1]]) && c < 0x80 && SyisLower(c) ){` |
|      55 | 3932 | `			char up = (char)SyToUpper(c);` |
|      55 | 3933 | `			if( i > iStart ){` |
|      37 | 3934 | `				ph7_result_string(pCtx,&zIn[iStart],i - iStart);` |
|      18 | 3935 | `			}` |
|      55 | 3936 | `			ph7_result_string(pCtx,&up,1);` |
|      55 | 3937 | `			iStart = i + 1;` |
|      27 | 3938 | `		}` |
|     152 | 3939 | `	}` |
|      23 | 3940 | `	if( nLen > iStart ){` |
|      23 | 3941 | `		ph7_result_string(pCtx,&zIn[iStart],nLen - iStart);` |
|      11 | 3942 | `	}` |
|      23 | 3943 | `	return PH7_OK;` |
|      15 | 3944 | `}` |
|       - | 3945 | `/*` |
|       - | 3946 | ` * string str_repeat(string $input,int $multiplier)` |
|       - | 3947 | ` *  Returns input repeated multiplier times.` |
|       - | 3948 | ` * Parameters` |
|       - | 3949 | ` *  $string` |
|       - | 3950 | ` *   String to be repeated.` |
|       - | 3951 | ` * $multiplier` |
|       - | 3952 | ` *  Number of time the input string should be repeated.` |
|       - | 3953 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|       - | 3954 | ` *  to 0, the function will return an empty string.` |
|       - | 3955 | ` * Return` |
|       - | 3956 | ` *  The repeated string.` |
|       - | 3957 | ` */` |
|   21406 | 3958 | `PH7_PRIVATE int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       4 | 3959 | `{` |
|       - | 3960 | `	const char *zIn;` |
|       - | 3961 | `	int nLen;` |
|       - | 3962 | `	ph7_int64 nMul;` |
|       - | 3963 | `	int rc;` |
|   21410 | 3964 | `	if( nArg < 2 ){` |
|       - | 3965 | `		/* Missing arguments,return NULL */` |
|     ! 0 | 3966 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3967 | `		return PH7_OK;` |
|       - | 3968 | `	}` |
|       - | 3969 | `	/* Extract the target string */` |
|   21410 | 3970 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|       - | 3971 | `	/* Resolve $times through the shared ZPP helper so a lossy float / float-string` |
|       - | 3972 | `	 * carries php's precision deprecation and NAN/INF/non-numeric fail with php's` |
|       - | 3973 | `	 * TypeError — a bare ph7_value_to_int64() coerced them silently. */` |
|       - | 3974 | `	{` |
|   21410 | 3975 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_repeat",2,"$times","int",&nMul);` |
|   21410 | 3976 | `		if( rcArg != PH7_OK ){` |
|     ! 0 | 3977 | `			return rcArg;` |
|       - | 3978 | `		}` |
|       - | 3979 | `	}` |
|   21410 | 3980 | `	if( nMul < 0 ){` |
|       3 | 3981 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 3982 | `			"str_repeat(): Argument #2 ($times) must be greater than or equal to 0");` |
|       - | 3983 | `	}` |
|   21408 | 3984 | `	if( nLen < 1 \|\| nMul < 1 ){` |
|       - | 3985 | `		/* Empty input or a zero multiplier yields the empty string (PHP). */` |
|       5 | 3986 | `		ph7_result_string(pCtx,"",0);` |
|       5 | 3987 | `		return PH7_OK;` |
|       - | 3988 | `	}` |
|       - | 3989 | `	/* Perform the requested operation */` |
|  955433 | 3990 | `	for(;;){` |
| 1910870 | 3991 | `		if( !nMul ){` |
|   21404 | 3992 | `			break;` |
|       - | 3993 | `		}` |
|       - | 3994 | `		/* Append the copy */` |
| 1889470 | 3995 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 1889470 | 3996 | `		if( rc != PH7_OK ){` |
|       - | 3997 | `			/* Allocation failed: surface a fatal instead of returning a` |
|       - | 3998 | `			 * silently-truncated string with a success status. */` |
|     ! 0 | 3999 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 4000 | `		}` |
| 1889470 | 4001 | `		nMul--;` |
|       4 | 4002 | `	}` |
|   21404 | 4003 | `	return PH7_OK;` |
|   10707 | 4004 | `}` |
|       - | 4005 | `/*` |
|       - | 4006 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|       - | 4007 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|       - | 4008 | ` * Parameters` |
|       - | 4009 | ` *  $string` |
|       - | 4010 | ` *   The input string.` |
|       - | 4011 | ` * $is_xhtml` |
|       - | 4012 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|       - | 4013 | ` * Return` |
|       - | 4014 | ` *  The processed string.` |
|       - | 4015 | ` */` |
|       8 | 4016 | `PH7_PRIVATE int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4017 | `{` |
|       - | 4018 | `	const char *zIn,*zCur,*zEnd;` |
|      10 | 4019 | `	int is_xhtml = 1; /* Default to XHTML-style '<br />' like PHP */` |
|       - | 4020 | `	int nLen;` |
|      10 | 4021 | `	if( nArg < 1 ){` |
|       - | 4022 | `		/* Missing arguments,return the empty string */` |
|     ! 0 | 4023 | `		ph7_result_string(pCtx,"",0);` |
|     ! 0 | 4024 | `		return PH7_OK;` |
|       - | 4025 | `	}` |
|       - | 4026 | `	/* Extract the target string */` |
|      10 | 4027 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      10 | 4028 | `	if( nLen < 1 ){` |
|       - | 4029 | `		/* php answers the empty STRING here, not null */` |
|       3 | 4030 | `		ph7_result_string(pCtx,"",0);` |
|       3 | 4031 | `		return PH7_OK;` |
|       - | 4032 | `	}` |
|       8 | 4033 | `	if( nArg > 1 ){` |
|       3 | 4034 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|       1 | 4035 | `	}` |
|       8 | 4036 | `	zEnd = &zIn[nLen];` |
|       - | 4037 | `	/* Perform the requested operation */` |
|       6 | 4038 | `	for(;;){` |
|      14 | 4039 | `		zCur = zIn;` |
|       - | 4040 | `		/* Delimit the string */` |
|      32 | 4041 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      14 | 4042 | `			zIn++;` |
|       2 | 4043 | `		}` |
|      14 | 4044 | `		if( zCur < zIn ){` |
|       - | 4045 | `			/* Output chunk verbatim */` |
|      14 | 4046 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|       6 | 4047 | `		}` |
|      14 | 4048 | `		if( zIn >= zEnd ){` |
|       - | 4049 | `			/* No more input to process */` |
|       8 | 4050 | `			break;` |
|       - | 4051 | `		}` |
|       - | 4052 | `		/* Output the HTML line break */` |
|       - | 4053 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br />' (space before the slash), otherwise use '<br>' */` |
|       8 | 4054 | `		if( is_xhtml ){` |
|       6 | 4055 | `			ph7_result_string(pCtx,"<br />",(int)sizeof("<br />")-1);` |
|       4 | 4056 | `		}else{` |
|       3 | 4057 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|       - | 4058 | `		}` |
|       8 | 4059 | `		zCur = zIn;` |
|       - | 4060 | `		/* Append trailing line */` |
|      17 | 4061 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|       8 | 4062 | `			zIn++;` |
|       2 | 4063 | `		}` |
|       8 | 4064 | `		if( zCur < zIn ){` |
|       - | 4065 | `			/* Output chunk verbatim */` |
|       8 | 4066 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|       3 | 4067 | `		}` |
|       2 | 4068 | `	}` |
|       8 | 4069 | `	return PH7_OK;` |
|       6 | 4070 | `}` |
|       - | 4071 | `/*` |
|       - | 4072 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|       - | 4073 | ` *  According to the PHP reference manual.` |
|       - | 4074 | ` * The format string is composed of zero or more directives: ordinary characters` |
|       - | 4075 | ` * (excluding %) that are copied directly to the result, and conversion` |
|       - | 4076 | ` * specifications, each of which results in fetching its own parameter.` |
|       - | 4077 | ` * This applies to both sprintf() and printf().` |
|       - | 4078 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|       - | 4079 | ` * or more of these elements, in order:` |
|       - | 4080 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|       - | 4081 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|       - | 4082 | ` *   positive numbers to have the + sign attached as well.` |
|       - | 4083 | ` *   An optional padding specifier that says what character will be used for padding` |
|       - | 4084 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|       - | 4085 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|       - | 4086 | ` *   it with a single quote ('). See the examples below.` |
|       - | 4087 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|       - | 4088 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|       - | 4089 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|       - | 4090 | ` *   should result in.` |
|       - | 4091 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|       - | 4092 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|       - | 4093 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|       - | 4094 | ` *   limit to the string.` |
|       - | 4095 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|       - | 4096 | ` *       % - a literal percent character. No argument is required.` |
|       - | 4097 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|       - | 4098 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|       - | 4099 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|       - | 4100 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|       - | 4101 | ` * 	     for the number of digits after the decimal point.` |
|       - | 4102 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|       - | 4103 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|       - | 4104 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|       - | 4105 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|       - | 4106 | ` *       g - shorter of %e and %f.` |
|       - | 4107 | ` *       G - shorter of %E and %f.` |
|       - | 4108 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|       - | 4109 | ` *       s - the argument is treated as and presented as a string.` |
|       - | 4110 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|       - | 4111 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|       - | 4112 | ` */` |
|       - | 4113 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|       - | 4114 | `#ifdef PH7_NEED_BUILTIN_REG` |
|       - | 4115 | `/*` |
|       - | 4116 | ` * Symisc eXtension.` |
|       - | 4117 | ` * string size_format(int64 $size)` |
|       - | 4118 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|       - | 4119 | ` *  Example:` |
|       - | 4120 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|       - | 4121 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|       - | 4122 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|       - | 4123 | ` * Parameter` |
|       - | 4124 | ` *  $size` |
|       - | 4125 | ` *    Entity size in bytes.` |
|       - | 4126 | ` * Return` |
|       - | 4127 | ` *   Formatted string representation of the given size.` |
|       - | 4128 | ` */` |
|      24 | 4129 | `PH7_PRIVATE int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4130 | `{` |
|       - | 4131 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|       - | 4132 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|       - | 4133 | `	sxi32 nRest,i_32;` |
|       - | 4134 | `	ph7_int64 iSize;` |
|      25 | 4135 | `	int c = -1; /* index in zUnit[] */` |
|       - | 4136 |  |
|      25 | 4137 | `	if( nArg < 1 ){` |
|       - | 4138 | `		/* Missing argument,return the empty string */` |
|       3 | 4139 | `		ph7_result_string(pCtx,"",0);` |
|       3 | 4140 | `		return PH7_OK;` |
|       - | 4141 | `	}` |
|       - | 4142 | `	/* Extract the given size */` |
|      23 | 4143 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|      23 | 4144 | `	if( iSize < 100 /* Bytes */ ){` |
|       - | 4145 | `		/* Don't bother formatting,return immediately */` |
|       5 | 4146 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|       5 | 4147 | `		return PH7_OK;` |
|       - | 4148 | `	}` |
|      19 | 4149 | `	for(;;){` |
|      39 | 4150 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|      39 | 4151 | `		iSize >>= 10;` |
|      39 | 4152 | `		c++;` |
|      39 | 4153 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|      19 | 4154 | `			break;` |
|       - | 4155 | `		}` |
|       1 | 4156 | `	}` |
|      19 | 4157 | `	nRest /= 100;` |
|      19 | 4158 | `	if( nRest > 9 ){` |
|     ! 0 | 4159 | `		nRest = 9;` |
|     ! 0 | 4160 | `	}` |
|      19 | 4161 | `	if( iSize > 999 ){` |
|     ! 0 | 4162 | `		c++;` |
|     ! 0 | 4163 | `		nRest = 9;` |
|     ! 0 | 4164 | `		iSize = 0;` |
|     ! 0 | 4165 | `	}` |
|      19 | 4166 | `	i_32 = (sxi32)iSize;` |
|       - | 4167 | `	/* Format */` |
|      19 | 4168 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|      19 | 4169 | `	return PH7_OK;` |
|      13 | 4170 | `}` |
|       - | 4171 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|       - | 4172 | `#ifdef PH7_NEED_BUILTIN_REG` |
|       - | 4173 | `/*` |
|       - | 4174 | ` * string str_shuffle(string $str)` |
|       - | 4175 |  |
|       - | 4176 | ` *  Randomly shuffles a string.` |
|       - | 4177 | ` * Parameters` |
|       - | 4178 | ` *  $str` |
|       - | 4179 | ` *   The input string.` |
|       - | 4180 | ` * Return` |
|       - | 4181 | ` *  Returns the shuffled string.` |
|       - | 4182 | ` */` |
|      20 | 4183 | `PH7_PRIVATE int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4184 | `{` |
|       - | 4185 | `	const char *zString;` |
|       - | 4186 | `	int nLen,i,c;` |
|       - | 4187 | `	sxu32 iR;` |
|      22 | 4188 | `	if( nArg < 1 ){` |
|       - | 4189 | `		/* Missing arguments,return the empty string */` |
|     ! 0 | 4190 | `		ph7_result_string(pCtx,"",0);` |
|     ! 0 | 4191 | `		return PH7_OK;` |
|       - | 4192 | `	}` |
|       - | 4193 | `	/* Extract the target string */` |
|      22 | 4194 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      22 | 4195 | `	if( nLen < 1 ){` |
|       - | 4196 | `		/* Nothing to shuffle */` |
|       6 | 4197 | `		ph7_result_string(pCtx,"",0);` |
|       6 | 4198 | `		return PH7_OK;` |
|       - | 4199 | `	}` |
|       - | 4200 | `	/* php's Fisher-Yates, drawn from the same generator in the same order, so a` |
|       - | 4201 | `	 * seeded shuffle answers php's string.` |
|       - | 4202 | `	 *` |
|       - | 4203 | `	 * What was here picked each output byte independently — WITH replacement — so` |
|       - | 4204 | `` 	 * the answer was not a permutation of the input at all: `str_shuffle($alpha)` `` |
|       - | 4205 | `	 * came back with letters repeated and letters missing, which is the one thing` |
|       - | 4206 | `	 * the documented "randomly shuffles a string" cannot do. The idiom it breaks` |
|       - | 4207 | `	 * is the common one: shuffling an alphabet and slicing a token out of it. */` |
|       - | 4208 | `	{` |
|      18 | 4209 | `		char *zOut = (char *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,(sxu32)nLen);` |
|      18 | 4210 | `		if( zOut == 0 ){` |
|     ! 0 | 4211 | `			return PH7_VmMemoryError(pCtx->pVm);` |
|       - | 4212 | `		}` |
|      18 | 4213 | `		SyMemcpy(zString,zOut,(sxu32)nLen);` |
|     254 | 4214 | `		for( i = nLen - 1 ; i > 0 ; --i ){` |
|     238 | 4215 | `			iR = (sxu32)PH7_VmMtRandRange(pCtx->pVm,0,(sxi64)i);` |
|     238 | 4216 | `			if( (int)iR != i ){` |
|     204 | 4217 | `				c = zOut[i];` |
|     204 | 4218 | `				zOut[i] = zOut[iR];` |
|     204 | 4219 | `				zOut[iR] = (char)c;` |
|     101 | 4220 | `			}` |
|     120 | 4221 | `		}` |
|      18 | 4222 | `		ph7_result_string(pCtx,zOut,nLen);` |
|      18 | 4223 | `		SyMemBackendFree(&pCtx->pVm->sAllocator,zOut);` |
|       - | 4224 | `	}` |
|      18 | 4225 | `	return PH7_OK;` |
|      12 | 4226 | `}` |
|       - | 4227 | `/*` |
|       - | 4228 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|       - | 4229 | ` *  Convert a string to an array.` |
|       - | 4230 | ` * Parameters` |
|       - | 4231 | ` * $string` |
|       - | 4232 | ` *  The input string.` |
|       - | 4233 | ` * $split_length` |
|       - | 4234 | ` *  Maximum length of the chunk.` |
|       - | 4235 | ` * Return` |
|       - | 4236 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|       - | 4237 | ` *  except possibly the last one which may be shorter.` |
|       - | 4238 | ` *  If split_length exceeds the string length, the entire string is returned` |
|       - | 4239 | ` *  as the first (and only) array element.` |
|       - | 4240 | ` *  An empty string returns an empty array.` |
|       - | 4241 | ` * Errors` |
|       - | 4242 | ` *  ArgumentCountError if no arguments are given.` |
|       - | 4243 | ` *  TypeError if $string is an array, object or resource.` |
|       - | 4244 | ` *  ValueError if $split_length is less than 1.` |
|       - | 4245 | ` */` |
|      34 | 4246 | `PH7_PRIVATE int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 4247 | `{` |
|       - | 4248 | `	const char *zString,*zEnd;` |
|       - | 4249 | `	ph7_value *pArray,*pValue;` |
|       - | 4250 | `	int split_len;` |
|       - | 4251 | `	int nLen;` |
|      37 | 4252 | `	if( nArg < 1 ){` |
|     ! 0 | 4253 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4254 | `			"ArgumentCountError",` |
|       - | 4255 | `			"str_split() expects at least 1 argument, %d given",` |
|     ! 0 | 4256 | `			nArg` |
|       - | 4257 | `			);` |
|       - | 4258 | `	}` |
|       - | 4259 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|      51 | 4260 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|      54 | 4261 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|      34 | 4262 | `	    ph7_value_is_resource(apArg[0]) ){` |
|     ! 0 | 4263 | `		return PH7_VmThrowException(pCtx,` |
|       - | 4264 | `			"TypeError",` |
|       - | 4265 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|     ! 0 | 4266 | `			ph7_type_name(apArg[0])` |
|       - | 4267 | `			);` |
|       - | 4268 | `	}` |
|       - | 4269 | `	/* Point to the target string */` |
|      37 | 4270 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      37 | 4271 | `	split_len = (int)sizeof(char);` |
|      37 | 4272 | `	if( nArg > 1 ){` |
|       - | 4273 | `		/* Split length */` |
|      19 | 4274 | `		split_len = ph7_value_to_int(apArg[1]);` |
|      19 | 4275 | `		if( split_len < 1 ){` |
|       6 | 4276 | `			return PH7_VmThrowException(pCtx,` |
|       - | 4277 | `				"ValueError",` |
|       - | 4278 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|       - | 4279 | `				);` |
|       - | 4280 | `		}` |
|      13 | 4281 | `		if( split_len > nLen && nLen > 0 ){` |
|       3 | 4282 | `			split_len = nLen;` |
|       1 | 4283 | `		}` |
|       6 | 4284 | `	}` |
|       - | 4285 | `	/* Create the array and the scalar value */` |
|      31 | 4286 | `	pArray = ph7_context_new_array(pCtx);` |
|       - | 4287 | `	/*Chunk value */` |
|      31 | 4288 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      31 | 4289 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|       - | 4290 | `		/* Return FALSE */` |
|     ! 0 | 4291 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4292 | `		return PH7_OK;` |
|       - | 4293 | `	}` |
|       - | 4294 | `	/* Point to the end of the string */` |
|      31 | 4295 | `	zEnd = &zString[nLen];` |
|       - | 4296 | `	/* Perform the requested operation */` |
|     209 | 4297 | `	for(;;){` |
|       - | 4298 | `		int nMax;` |
|     225 | 4299 | `		if( zString >= zEnd ){` |
|       - | 4300 | `			/* No more input to process */` |
|      31 | 4301 | `			break;` |
|       - | 4302 | `		}` |
|     195 | 4303 | `		nMax = (int)(zEnd-zString);` |
|     195 | 4304 | `		if( nMax < split_len ){` |
|       5 | 4305 | `			split_len = nMax;` |
|       2 | 4306 | `		}` |
|       - | 4307 | `		/* Copy the current chunk */` |
|     195 | 4308 | `		ph7_value_string(pValue,zString,split_len);` |
|       - | 4309 | `		/* Insert it */` |
|     195 | 4310 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|     ! 0 | 4311 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 4312 | `		}` |
|       - | 4313 | `		/* reset the string cursor */` |
|     195 | 4314 | `		ph7_value_reset_string_cursor(pValue);` |
|       - | 4315 | `		/* Update position */` |
|     195 | 4316 | `		zString += split_len;` |
|       1 | 4317 | `	}` |
|       - | 4318 | `	/*` |
|       - | 4319 | `	 * Return the array.` |
|       - | 4320 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|       - | 4321 | `	 * upon we return from this function.` |
|       - | 4322 | `	 */` |
|      31 | 4323 | `	ph7_result_value(pCtx,pArray);` |
|      31 | 4324 | `	return PH7_OK;` |
|      20 | 4325 | `}` |
|       - | 4326 | `/*` |
|       - | 4327 | ` * array\|string count_chars(string $string[,int $mode = 0 ])` |
|       - | 4328 | ` *  How many times each byte value occurs in a string.` |
|       - | 4329 | ` * Parameters` |
|       - | 4330 | ` *  $string` |
|       - | 4331 | ` *   The examined string.` |
|       - | 4332 | ` *  $mode` |
|       - | 4333 | ` *   0: an array of all 256 byte values -> frequency.` |
|       - | 4334 | ` *   1: only the byte values that occur.` |
|       - | 4335 | ` *   2: only the byte values that do not.` |
|       - | 4336 | ` *   3: a string of the byte values that occur.` |
|       - | 4337 | ` *   4: a string of the byte values that do not.` |
|       - | 4338 | ` * Return` |
|       - | 4339 | ` *  An array for modes 0-2, a string for modes 3-4.` |
|       - | 4340 | ` */` |
|      86 | 4341 | `PH7_PRIVATE int PH7_builtin_count_chars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       3 | 4342 | `{` |
|       - | 4343 | `	sxu32 aCount[256];` |
|       - | 4344 | `	const unsigned char *zIn;` |
|       - | 4345 | `	ph7_value *pArray,*pValue;` |
|       - | 4346 | `	char zOut[256];` |
|      89 | 4347 | `	int nOut = 0;` |
|      89 | 4348 | `	int nLen = 0;` |
|      89 | 4349 | `	int iMode = 0;` |
|       - | 4350 | `	int i;` |
|      89 | 4351 | `	if( nArg < 1 ){` |
|       - | 4352 | `		/* Arity is enforced from aBuiltinSig[] before the call. */` |
|     ! 0 | 4353 | `		ph7_result_null(pCtx);` |
|     ! 0 | 4354 | `		return PH7_OK;` |
|       - | 4355 | `	}` |
|      89 | 4356 | `	if( nArg > 1 ){` |
|       - | 4357 | ``		/* php declares `int $mode`; the shared screen refuses the values no`` |
|       - | 4358 | `		 * coercion can reach (array, object, resource, null), leaving the string` |
|       - | 4359 | `		 * and float narrowing to the builtin -- both of which php only DEPRECATES` |
|       - | 4360 | `		 * and PHL rejects (§10). */` |
|      89 | 4361 | `		if( ph7_value_is_string(apArg[1]) ){` |
|       - | 4362 | `			/* php wants the WHOLE string to be numeric (surrounding whitespace` |
|       - | 4363 | `			 * aside): "2abc" and "0x2" are TypeErrors, not 2. A float-shaped one` |
|       - | 4364 | `			 * that would LOSE something is §10's refusal of a deprecation. */` |
|       - | 4365 | `			double d;` |
|       7 | 4366 | `			if( !PH7_MemObjStringIsNumeric(apArg[1]) ){` |
|     ! 0 | 4367 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 4368 | `					"count_chars(): Argument #2 ($mode) must be of type int, string given");` |
|       - | 4369 | `			}` |
|       7 | 4370 | `			d = ph7_value_to_double(apArg[1]);` |
|       7 | 4371 | `			if( !PH7_RealFitsInt64(d) \|\| d != (double)(sxi64)d ){` |
|     ! 0 | 4372 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 4373 | `					"count_chars(): Argument #2 ($mode) must be of type int, string given");` |
|       1 | 4374 | `			}` |
|      86 | 4375 | `		}else if( ph7_value_is_float(apArg[1]) ){` |
|       3 | 4376 | `			double d = ph7_value_to_double(apArg[1]);` |
|       - | 4377 | `			/* Range first: see chr() above -- an out-of-range cast is undefined. */` |
|       3 | 4378 | `			if( !PH7_RealFitsInt64(d) \|\| d != (double)(sxi64)d ){` |
|     ! 0 | 4379 | `				return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 4380 | `					"count_chars(): Argument #2 ($mode) must be of type int, float given");` |
|       - | 4381 | `			}` |
|       1 | 4382 | `		}` |
|      89 | 4383 | `		iMode = ph7_value_to_int(apArg[1]);` |
|      89 | 4384 | `		if( iMode < 0 \|\| iMode > 4 ){` |
|       7 | 4385 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 4386 | `				"count_chars(): Argument #2 ($mode) must be between 0 and 4 (inclusive)");` |
|       - | 4387 | `		}` |
|      40 | 4388 | `	}` |
|       - | 4389 | `	/* Binary safe: the string is counted by LENGTH, so an embedded NUL is a byte` |
|       - | 4390 | `	 * value like any other. */` |
|      83 | 4391 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   20563 | 4392 | `	for( i = 0 ; i < 256 ; ++i ){` |
|   20483 | 4393 | `		aCount[i] = 0;` |
|   10243 | 4394 | `	}` |
|     725 | 4395 | `	for( i = 0 ; i < nLen ; ++i ){` |
|     645 | 4396 | `		aCount[zIn[i]]++;` |
|     324 | 4397 | `	}` |
|      83 | 4398 | `	if( iMode >= 3 ){` |
|       - | 4399 | `		/* 3 = the bytes that occur, 4 = the bytes that do not. */` |
|    6684 | 4400 | `		for( i = 0 ; i < 256 ; ++i ){` |
|    6658 | 4401 | `			if( (aCount[i] != 0) == (iMode == 3) ){` |
|    1568 | 4402 | `				zOut[nOut++] = (char)i;` |
|     783 | 4403 | `			}` |
|    3330 | 4404 | `		}` |
|      28 | 4405 | `		ph7_result_string(pCtx,zOut,nOut);` |
|      28 | 4406 | `		return PH7_OK;` |
|       - | 4407 | `	}` |
|      57 | 4408 | `	pArray = ph7_context_new_array(pCtx);` |
|      57 | 4409 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      57 | 4410 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|     ! 0 | 4411 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 4412 | `	}` |
|   13881 | 4413 | `	for( i = 0 ; i < 256 ; ++i ){` |
|       - | 4414 | `		/* 0 = every byte value, 1 = the ones that occur, 2 = the ones that do not` |
|       - | 4415 | `		 * (php stores their count, which is always 0). */` |
|   13827 | 4416 | `		if( iMode != 0 && (aCount[i] != 0) != (iMode == 1) ){` |
|   10271 | 4417 | `			continue;` |
|       - | 4418 | `		}` |
|    3559 | 4419 | `		ph7_value_int64(pValue,(ph7_int64)aCount[i]);` |
|    3559 | 4420 | `		if( ph7_array_add_intkey_elem(pArray,i,pValue) != SXRET_OK ){` |
|     ! 0 | 4421 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 4422 | `		}` |
|    1781 | 4423 | `	}` |
|      57 | 4424 | `	ph7_result_value(pCtx,pArray);` |
|      57 | 4425 | `	return PH7_OK;` |
|      46 | 4426 | `}` |
|       - | 4427 | `/*` |
|       - | 4428 | ` * Check if the given string contains only characters from the given mask.` |
|       - | 4429 | ` * return the longest match.` |
|       - | 4430 | ` * Refer to [strspn()].` |
|       - | 4431 | ` */` |
|      66 | 4432 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|       1 | 4433 | `{` |
|      67 | 4434 | `	const char *zEnd = &zString[nLen];` |
|      67 | 4435 | `	const char *zIn = zString;` |
|       - | 4436 | `	int i,c;` |
|     110 | 4437 | `	for(;;){` |
|     221 | 4438 | `		if( zString >= zEnd ){` |
|      45 | 4439 | `			break;` |
|       - | 4440 | `		}` |
|       - | 4441 | `		/* Extract current character */` |
|     177 | 4442 | `		c = zString[0];` |
|       - | 4443 | `		/* Perform the lookup */` |
|     589 | 4444 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     567 | 4445 | `			if( c == zMask[i] ){` |
|       - | 4446 | `				/* Character found */` |
|     155 | 4447 | `				break;` |
|       - | 4448 | `			}` |
|     207 | 4449 | `		}` |
|     177 | 4450 | `		if( i >= nMaskLen ){` |
|       - | 4451 | `			/* Character not in the current mask,break immediately */` |
|      23 | 4452 | `			break;` |
|       - | 4453 | `		}` |
|       - | 4454 | `		/* Advance cursor */` |
|     155 | 4455 | `		zString++;` |
|       1 | 4456 | `	}` |
|       - | 4457 | `	/* Longest match */` |
|      67 | 4458 | `	return (int)(zString-zIn);` |
|       1 | 4459 | `}` |
|       - | 4460 | `/*` |
|       - | 4461 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|       - | 4462 | ` * Refer to [strcspn()].` |
|       - | 4463 | ` */` |
|     340 | 4464 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|       5 | 4465 | `{` |
|     345 | 4466 | `	const char *zEnd = &zString[nLen];` |
|     345 | 4467 | `	const char *zIn = zString;` |
|       - | 4468 | `	int i,c;` |
|    9345 | 4469 | `	for(;;){` |
|   12478 | 4470 | `		if( zString >= zEnd ){` |
|     297 | 4471 | `			break;` |
|       - | 4472 | `		}` |
|       - | 4473 | `		/* Extract current character */` |
|   12186 | 4474 | `		c = zString[0];` |
|       - | 4475 | `		/* Perform the lookup */` |
|   48347 | 4476 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|   36214 | 4477 | `			if( c == zMask[i] ){` |
|      49 | 4478 | `				break;` |
|       - | 4479 | `			}` |
|   27411 | 4480 | `		}` |
|   12186 | 4481 | `		if( i < nMaskLen ){` |
|       - | 4482 | `			/* Character in the current mask,break immediately */` |
|      49 | 4483 | `			break;` |
|       - | 4484 | `		}` |
|       - | 4485 | `		/* Advance cursor */` |
|   12138 | 4486 | `		zString++;` |
|       5 | 4487 | `	}` |
|       - | 4488 | `	/* Longest match */` |
|     345 | 4489 | `	return (int)(zString-zIn);` |
|       5 | 4490 | `}` |
|       - | 4491 | `/*` |
|       - | 4492 | ` * Shared body of strspn()/strcspn(): resolve php's ($offset,$length) window over` |
|       - | 4493 | ` * $string, then measure the span from the window's first byte.` |
|       - | 4494 | ` *` |
|       - | 4495 | ` * php's window rules (ext/standard/string.c, php_spn_common_handler) — a negative` |
|       - | 4496 | ` * $offset counts back from the end and CLAMPS to 0 (it is never "invalid"); an` |
|       - | 4497 | ` * $offset past the end clamps to the end, so the window is empty and the answer is` |
|       - | 4498 | ` * 0; a negative $length leaves that many bytes off the end of the remaining span` |
|       - | 4499 | ` * and clamps to 0; a zero-length window answers 0. PH7 answered 0 for a negative` |
|       - | 4500 | ` * offset that reached past the start, IGNORED a zero or negative $length entirely` |
|       - | 4501 | ` * (measuring the whole rest of the string instead), and truncated the offset to` |
|       - | 4502 | `` * `int`, so a 64-bit offset wrapped into a valid one.`` |
|       - | 4503 | ` *` |
|       - | 4504 | ` * PH7 also ran the scan over the first WHITESPACE-DELIMITED TOKEN rather than over` |
|       - | 4505 | ` * the raw window (leading spaces skipped, scan stopped at the next space), so` |
|       - | 4506 | ` * strspn("a b c","abc ") answered 1 where php answers 5 and strspn("  abc","abc")` |
|       - | 4507 | ` * answered 3 where php answers 0 — silent wrong answers on ordinary input. php` |
|       - | 4508 | ` * scans raw bytes; so does this.` |
|       - | 4509 | ` *` |
|       - | 4510 | ` * An empty $mask needs no special case: the mask lookup fails for every byte, so` |
|       - | 4511 | ` * strspn stops at once (0) and strcspn runs to the end of the window (its length),` |
|       - | 4512 | ` * which is exactly what php answers.` |
|       - | 4513 | ` */` |
|     406 | 4514 | `static int StrSpnCommonHandler(` |
|       - | 4515 | `	ph7_context *pCtx,    /* Call context */` |
|       - | 4516 | `	int nArg,             /* Argument count */` |
|       - | 4517 | `	ph7_value **apArg,    /* Arguments */` |
|       - | 4518 | `	int bComplement       /* TRUE for strcspn() */` |
|       - | 4519 | `	)` |
|       5 | 4520 | `{` |
|     411 | 4521 | `	const char *zFunc = bComplement ? "strcspn" : "strspn";` |
|       - | 4522 | `	const char *zString,*zMask;` |
|       - | 4523 | `	int iMasklen,iLen;` |
|       - | 4524 | `	sxi64 iStart,iSpan;` |
|     411 | 4525 | `	if( nArg < 2 ){` |
|       - | 4526 | `		/* Arity is enforced at the call boundary; nothing sensible to return here. */` |
|     ! 0 | 4527 | `		ph7_result_int(pCtx,0);` |
|     ! 0 | 4528 | `		return PH7_OK;` |
|       - | 4529 | `	}` |
|       - | 4530 | `	/* Extract the target string and the mask */` |
|     411 | 4531 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|     411 | 4532 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     411 | 4533 | `	if( iLen < 0 ){` |
|     ! 0 | 4534 | `		iLen = 0;` |
|     ! 0 | 4535 | `	}` |
|     411 | 4536 | `	if( iMasklen < 0 ){` |
|     ! 0 | 4537 | `		iMasklen = 0;` |
|     ! 0 | 4538 | `	}` |
|     411 | 4539 | `	iStart = 0;` |
|     411 | 4540 | `	if( nArg > 2 ){` |
|      73 | 4541 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[2],zFunc,3,"$offset","int",&iStart);` |
|      73 | 4542 | `		if( rcArg != PH7_OK ){` |
|     ! 0 | 4543 | `			return rcArg;` |
|       - | 4544 | `		}` |
|      73 | 4545 | `		if( iStart < 0 ){` |
|       - | 4546 | `			/* Count back from the end, clamped to the start (guarded so an` |
|       - | 4547 | `			 * INT64_MIN offset cannot overflow the addition). */` |
|      13 | 4548 | `			iStart = ( iStart < -(sxi64)iLen ) ? 0 : iStart + iLen;` |
|      67 | 4549 | `		}else if( iStart > (sxi64)iLen ){` |
|       9 | 4550 | `			iStart = iLen;` |
|       4 | 4551 | `		}` |
|      36 | 4552 | `	}` |
|     411 | 4553 | `	iSpan = (sxi64)iLen - iStart;` |
|     411 | 4554 | `	if( nArg > 3 && !ph7_value_is_null(apArg[3]) ){` |
|      39 | 4555 | `		sxi64 iUserlen = 0;` |
|      39 | 4556 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[3],zFunc,4,"$length","?int",&iUserlen);` |
|      39 | 4557 | `		if( rcArg != PH7_OK ){` |
|     ! 0 | 4558 | `			return rcArg;` |
|       - | 4559 | `		}` |
|      39 | 4560 | `		if( iUserlen < 0 ){` |
|       - | 4561 | `			/* Leave \|$length\| bytes off the end of the remaining span (guarded` |
|       - | 4562 | `			 * against an INT64_MIN underflow the same way). */` |
|      21 | 4563 | `			iSpan = ( iUserlen < -iSpan ) ? 0 : iSpan + iUserlen;` |
|      29 | 4564 | `		}else if( iUserlen < iSpan ){` |
|      11 | 4565 | `			iSpan = iUserlen;` |
|       5 | 4566 | `		}` |
|      19 | 4567 | `	}` |
|     614 | 4568 | `	ph7_result_int(pCtx,bComplement` |
|     340 | 4569 | `		? LongestStringMask2(&zString[iStart],(int)iSpan,zMask,iMasklen)` |
|      66 | 4570 | `		: LongestStringMask(&zString[iStart],(int)iSpan,zMask,iMasklen));` |
|     411 | 4571 | `	return PH7_OK;` |
|     208 | 4572 | `}` |
|       - | 4573 | `/*` |
|       - | 4574 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|       - | 4575 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|       - | 4576 | ` *  of characters contained within a given mask.` |
|       - | 4577 | ` * Parameters` |
|       - | 4578 | ` * $str` |
|       - | 4579 | ` *  The input string.` |
|       - | 4580 | ` * $mask` |
|       - | 4581 | ` *  The list of allowable characters.` |
|       - | 4582 | ` * $start` |
|       - | 4583 | ` *  The position in subject to start searching.` |
|       - | 4584 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|       - | 4585 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|       - | 4586 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|       - | 4587 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|       - | 4588 | ` *  start'th position from the end of subject.` |
|       - | 4589 | ` * $length` |
|       - | 4590 | ` *  The length of the segment from subject to examine.` |
|       - | 4591 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|       - | 4592 | ` *  characters after the starting position.` |
|       - | 4593 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|       - | 4594 | ` *  position up to length characters from the end of subject.` |
|       - | 4595 | ` * Return` |
|       - | 4596 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|       - | 4597 | ` * in mask.` |
|       - | 4598 | ` */` |
|      66 | 4599 | `PH7_PRIVATE int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4600 | `{` |
|      67 | 4601 | `	return StrSpnCommonHandler(pCtx,nArg,apArg,0);` |
|       1 | 4602 | `}` |
|       - | 4603 | `/*` |
|       - | 4604 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|       - | 4605 | ` *  Find length of initial segment not matching mask.` |
|       - | 4606 | ` * Parameters` |
|       - | 4607 | ` * $str` |
|       - | 4608 | ` *  The input string.` |
|       - | 4609 | ` * $mask` |
|       - | 4610 | ` *  The list of not allowed characters.` |
|       - | 4611 | ` * $start` |
|       - | 4612 | ` *  The position in subject to start searching.` |
|       - | 4613 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|       - | 4614 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|       - | 4615 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|       - | 4616 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|       - | 4617 | ` *  start'th position from the end of subject.` |
|       - | 4618 | ` * $length` |
|       - | 4619 | ` *  The length of the segment from subject to examine.` |
|       - | 4620 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|       - | 4621 | ` *  characters after the starting position.` |
|       - | 4622 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|       - | 4623 | ` *  position up to length characters from the end of subject.` |
|       - | 4624 | ` * Return` |
|       - | 4625 | ` *  Returns the length of the segment as an integer.` |
|       - | 4626 | ` */` |
|     340 | 4627 | `PH7_PRIVATE int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 4628 | `{` |
|     345 | 4629 | `	return StrSpnCommonHandler(pCtx,nArg,apArg,1);` |
|       5 | 4630 | `}` |
|       - | 4631 | `/*` |
|       - | 4632 | ` * string strpbrk(string $haystack,string $char_list)` |
|       - | 4633 | ` *  Search a string for any of a set of characters.` |
|       - | 4634 | ` * Parameters` |
|       - | 4635 | ` *  $haystack` |
|       - | 4636 | ` *   The string where char_list is looked for.` |
|       - | 4637 | ` *  $char_list` |
|       - | 4638 | ` *   This parameter is case sensitive.` |
|       - | 4639 | ` * Return` |
|       - | 4640 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|       - | 4641 | ` */` |
|      14 | 4642 | `PH7_PRIVATE int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4643 | `{` |
|       - | 4644 | `	const char *zString,*zList,*zEnd;` |
|       - | 4645 | `	int iLen,iListLen,i,c;` |
|       - | 4646 | `	sxu32 nOfft,nMax;` |
|       - | 4647 | `	sxi32 rc;` |
|      15 | 4648 | `	if( nArg < 2 ){` |
|       - | 4649 | `		/* Missing arguments,return FALSE */` |
|     ! 0 | 4650 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 4651 | `		return PH7_OK;` |
|       - | 4652 | `	}` |
|       - | 4653 | `	/* Extract the haystack and the char list */` |
|      15 | 4654 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      15 | 4655 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      15 | 4656 | `	if( iListLen < 1 ){` |
|       - | 4657 | `		/* An empty set can never match, so php rejects it rather than answering` |
|       - | 4658 | `		 * a FALSE indistinguishable from "not found" (checked BEFORE the haystack,` |
|       - | 4659 | `		 * so strpbrk("","") throws too). */` |
|       5 | 4660 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 4661 | `			"strpbrk(): Argument #2 ($characters) must be a non-empty string");` |
|       - | 4662 | `	}` |
|      11 | 4663 | `	if( iLen < 1 ){` |
|       - | 4664 | `		/* Nothing to process,return FALSE */` |
|       3 | 4665 | `		ph7_result_bool(pCtx,0);` |
|       3 | 4666 | `		return PH7_OK;` |
|       - | 4667 | `	}` |
|       - | 4668 | `	/* Point to the end of the string */` |
|       9 | 4669 | `	zEnd = &zString[iLen];` |
|       9 | 4670 | `	nOfft = nMax = SXU32_HIGH;` |
|       - | 4671 | `	/* perform the requested operation */` |
|      25 | 4672 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|      17 | 4673 | `		c = zList[i];` |
|      17 | 4674 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|      17 | 4675 | `		if( rc == SXRET_OK ){` |
|       9 | 4676 | `			if( nMax < nOfft ){` |
|       5 | 4677 | `				nOfft = nMax;` |
|       2 | 4678 | `			}` |
|       4 | 4679 | `		}` |
|       9 | 4680 | `	}` |
|       9 | 4681 | `	if( nOfft == SXU32_HIGH ){` |
|       - | 4682 | `		/* No such substring,return FALSE */` |
|       5 | 4683 | `		ph7_result_bool(pCtx,0);` |
|       3 | 4684 | `	}else{` |
|       - | 4685 | `		/* Return the substring */` |
|       5 | 4686 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|       - | 4687 | `	}` |
|       9 | 4688 | `	return PH7_OK;` |
|       8 | 4689 | `}` |
|       - | 4690 | `/*` |
|       - | 4691 | ` * string soundex(string $str)` |
|       - | 4692 | ` *  Calculate the soundex key of a string.` |
|       - | 4693 | ` * Parameters` |
|       - | 4694 | ` *  $str` |
|       - | 4695 | ` *   The input string.` |
|       - | 4696 | ` * Return` |
|       - | 4697 | ` *  Returns the soundex key as a string.` |
|       - | 4698 | ` * Note:` |
|       - | 4699 | ` *  Knuth's algorithm as php implements it (ext/standard/soundex.c). The` |
|       - | 4700 | ` *  previous implementation came from SQLite and diverged from php on three` |
|       - | 4701 | ` *  counts, each of them a silent wrong answer:` |
|       - | 4702 | ` *` |
|       - | 4703 | ` *   - a NON-LETTER inside the word RESET the "same code in a row" state, so` |
|       - | 4704 | ` *     soundex("S s") answered S200 where php answers S000 and soundex("b1b")` |
|       - | 4705 | ` *     answered B100 where php answers B000. php simply skips anything that is` |
|       - | 4706 | ` *     not a letter; only a VOWEL separates two consonants sharing a code.` |
|       - | 4707 | ` *   - the scan stopped at the first byte >= 0xC0, taking every UTF-8 lead byte` |
|       - | 4708 | ` *     for a letter and copying it raw into the key: soundex("\xff\xfe") answered` |
|       - | 4709 | ` *     "\xff000" where php answers "0000", and a leading accent HID the letters` |
|       - | 4710 | ` *     behind it (soundex("éa") answered "\xc3000" for php's A000). Inside the` |
|       - | 4711 | `` *     loop the table was indexed by `byte & 0x7f`, which folds high bytes onto`` |
|       - | 4712 | ` *     ASCII letters and invents codes for them.` |
|       - | 4713 | ` *   - the input was walked as a NUL-terminated C string, so soundex("a\0b")` |
|       - | 4714 | ` *     stopped at the NUL (A000) where php walks the whole php string (A100).` |
|       - | 4715 | ` *` |
|       - | 4716 | ` *  Classification is ASCII-only, matching php's own A-Z table (§7's locale` |
|       - | 4717 | ` *  dependence family: the old code asked libc's isalpha() through SyisAlpha,` |
|       - | 4718 | ` *  which answers differently under a non-C LC_CTYPE).` |
|       - | 4719 | ` */` |
|      52 | 4720 | `PH7_PRIVATE int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 4721 | `{` |
|       - | 4722 | `	/* Code per letter A-Z; 0 means "no code" (a vowel, plus H/W/Y) */` |
|       - | 4723 | `	static const char zCode[] = "01230120022455012623010202";` |
|       - | 4724 | `	const unsigned char *zIn;` |
|       - | 4725 | `	char zResult[4];` |
|      53 | 4726 | `	int nByte,i,nOut = 0,iLast = -1;` |
|      53 | 4727 | `	if( nArg < 1 ){` |
|       - | 4728 | `		/* Missing arguments,return the empty string */` |
|     ! 0 | 4729 | `		ph7_result_string(pCtx,"",0);` |
|     ! 0 | 4730 | `		return PH7_OK;` |
|       - | 4731 | `	}` |
|      53 | 4732 | `	zIn = (const unsigned char *)ph7_value_to_string(apArg[0],&nByte);` |
|     265 | 4733 | `	for( i = 0 ; i < nByte && nOut < 4 ; ++i ){` |
|     213 | 4734 | `		int c = zIn[i];` |
|       - | 4735 | `		int code;` |
|     213 | 4736 | `		if( c >= 'a' && c <= 'z' ){` |
|     139 | 4737 | `			c -= 'a' - 'A';` |
|      69 | 4738 | `		}` |
|     213 | 4739 | `		if( c < 'A' \|\| c > 'Z' ){` |
|      35 | 4740 | `			continue; /* not a letter: skipped outright, state untouched */` |
|       - | 4741 | `		}` |
|     179 | 4742 | `		code = zCode[c - 'A'] - '0';` |
|     179 | 4743 | `		if( nOut == 0 ){` |
|       - | 4744 | `			/* The key opens with the first letter itself */` |
|      43 | 4745 | `			zResult[nOut++] = (char)c;` |
|     158 | 4746 | `		}else if( code != iLast && code != 0 ){` |
|      69 | 4747 | `			zResult[nOut++] = (char)(code + '0');` |
|      34 | 4748 | `		}` |
|     179 | 4749 | `		iLast = code;` |
|      90 | 4750 | `	}` |
|       - | 4751 | `	/* Pad to four characters. A string with no letter at all pads from nothing,` |
|       - | 4752 | `	 * which is php's "0000" (an empty input included). */` |
|     151 | 4753 | `	while( nOut < 4 ){` |
|      99 | 4754 | `		zResult[nOut++] = '0';` |
|       1 | 4755 | `	}` |
|      53 | 4756 | `	ph7_result_string(pCtx,zResult,4);` |
|      53 | 4757 | `	return PH7_OK;` |
|      27 | 4758 | `}` |
|       - | 4759 | `/*` |
|       - | 4760 | ` * string str_rot13(string $string)` |
|       - | 4761 | ` *  Perform the ROT13 transform: each ASCII letter is rotated 13 places through` |
|       - | 4762 | ` *  its own alphabet, everything else (digits, punctuation, high bytes, NULs)` |
|       - | 4763 | ` *  passes through untouched. ROT13 is its own inverse.` |
|       - | 4764 | ` */` |
|      20 | 4765 | `PH7_PRIVATE int PH7_builtin_str_rot13(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4766 | `{` |
|       - | 4767 | `	const char *zIn;` |
|       - | 4768 | `	char *zOut;` |
|       - | 4769 | `	int nLen,i;` |
|      22 | 4770 | `	if( nArg < 1 ){` |
|       - | 4771 | `		/* Missing arguments,return the empty string */` |
|     ! 0 | 4772 | `		ph7_result_string(pCtx,"",0);` |
|     ! 0 | 4773 | `		return PH7_OK;` |
|       - | 4774 | `	}` |
|      22 | 4775 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      22 | 4776 | `	if( nLen < 1 ){` |
|       3 | 4777 | `		ph7_result_string(pCtx,"",0);` |
|       3 | 4778 | `		return PH7_OK;` |
|       - | 4779 | `	}` |
|      20 | 4780 | `	zOut = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)nLen,FALSE,TRUE);` |
|      20 | 4781 | `	if( zOut == 0 ){` |
|     ! 0 | 4782 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 4783 | `	}` |
|     204 | 4784 | `	for( i = 0 ; i < nLen ; i++ ){` |
|     190 | 4785 | `		int c = (unsigned char)zIn[i];` |
|     190 | 4786 | `		if( (c >= 'a' && c <= 'm') \|\| (c >= 'A' && c <= 'M') ){` |
|      75 | 4787 | `			c += 13;` |
|     155 | 4788 | `		}else if( (c >= 'n' && c <= 'z') \|\| (c >= 'N' && c <= 'Z') ){` |
|      69 | 4789 | `			c -= 13;` |
|      34 | 4790 | `		}` |
|     186 | 4791 | `		zOut[i] = (char)c;` |
|      94 | 4792 | `	}` |
|      16 | 4793 | `	ph7_result_string(pCtx,zOut,nLen);` |
|      16 | 4794 | `	return PH7_OK;` |
|      10 | 4795 | `}` |
|       - | 4796 | `/*` |
|       - | 4797 | ` * Character-class table for metaphone(), php's _codes[] (ext/standard/` |
|       - | 4798 | ` * metaphone.c, itself from CPAN Text-Metaphone), indexed by 'A'..'Z':` |
|       - | 4799 | ` * bit 1 vowel (AEIOU) · bit 2 passes through unchanged (FJMNR) · bit 4 forms a` |
|       - | 4800 | ` * diphthong before H (CGPST) · bit 8 makes C and G soft (EIY) · bit 16 keeps a` |
|       - | 4801 | ` * GH from becoming F (BDH).` |
|       - | 4802 | ` */` |
|       - | 4803 | `static const char aMetaCode[26] = {` |
|       - | 4804 | `	1,16,4,16,9,2,4,16,9,2,0,2,2,2,1,4,0,2,4,4,1,0,0,0,8,0` |
|       - | 4805 | `};` |
|       - | 4806 | `/* Classification is ASCII-only, php's own table (§7 locale-dependence family:` |
|       - | 4807 | ` * libc's isalpha()/toupper() answer differently under a non-C LC_CTYPE). */` |
|       - | 4808 | `#define META_IS_ALPHA(c) (((c) >= 'A' && (c) <= 'Z') \|\| ((c) >= 'a' && (c) <= 'z'))` |
|       - | 4809 | `#define META_UP(c)       (((c) >= 'a' && (c) <= 'z') ? (char)((c) - ('a' - 'A')) : (char)(c))` |
|       - | 4810 | `#define META_ENCODE(c)   (((c) >= 'A' && (c) <= 'Z') ? aMetaCode[(c) - 'A'] : 0)` |
|       - | 4811 | `#define META_ISVOWEL(c)  (META_ENCODE(c) & 1)  /* AEIOU */` |
|       - | 4812 | `#define META_AFFECTH(c)  (META_ENCODE(c) & 4)  /* CGPST */` |
|       - | 4813 | `#define META_MAKESOFT(c) (META_ENCODE(c) & 8)  /* EIY */` |
|       - | 4814 | `#define META_NOGHTOF(c)  (META_ENCODE(c) & 16) /* BDH */` |
|       - | 4815 | `/* php's special phoneme encodings: 'sh' and 'th' */` |
|       - | 4816 | `#define META_SH '\x58' /* 'X' */` |
|       - | 4817 | `#define META_TH '\x30' /* '0' */` |
|       - | 4818 | `/*` |
|       - | 4819 | ` * php's Lookahead(): step up to nHow bytes forward from iFrom, stopping early` |
|       - | 4820 | ` * at a NUL, and answer the byte at the stop position. The php original walks a` |
|       - | 4821 | ` * NUL-terminated buffer; this walks the same way over a bounded one, treating` |
|       - | 4822 | ` * the end of the buffer as the NUL.` |
|       - | 4823 | ` */` |
|     ! 0 | 4824 | `static char MetaLookahead(const char *zIn,sxu32 nLen,sxu32 iFrom,sxu32 nHow)` |
|     ! 0 | 4825 | `{` |
|       - | 4826 | `	sxu32 idx;` |
|     ! 0 | 4827 | `	for( idx = 0 ; idx < nHow ; idx++ ){` |
|     ! 0 | 4828 | `		if( iFrom + idx >= nLen \|\| zIn[iFrom + idx] == '\0' ){` |
|     ! 0 | 4829 | `			break;` |
|       - | 4830 | `		}` |
|     ! 0 | 4831 | `	}` |
|     ! 0 | 4832 | `	return (iFrom + idx < nLen) ? zIn[iFrom + idx] : '\0';` |
|     ! 0 | 4833 | `}` |
|       - | 4834 | `/*` |
|       - | 4835 | ` * string metaphone(string $string, int $max_phonemes = 0)` |
|       - | 4836 | ` *  Break an english phrase down into its phonemes. Faithful port of php-src` |
|       - | 4837 | `` *  PHP-8.5 ext/standard/metaphone.c (the `traditional` flavour, which is the`` |
|       - | 4838 | ` *  only one php's own function invokes — the non-traditional Christ/School/` |
|       - | 4839 | ` *  SCHW branches are compiled out there and are not ported). Like php's, the` |
|       - | 4840 | ` *  scan stops at an embedded NUL: the original walks a NUL-terminated buffer.` |
|       - | 4841 | ` * Return` |
|       - | 4842 | ` *  The phonemes as a string of A-Z plus php's two special encodings` |
|       - | 4843 | ` *  ('X' for "sh", '0' for "th"); "" when no letter is reached.` |
|       - | 4844 | ` */` |
|      80 | 4845 | `PH7_PRIVATE int PH7_builtin_metaphone(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 4846 | `{` |
|       - | 4847 | `	const char *zIn;` |
|       - | 4848 | `	char *zOut;` |
|      82 | 4849 | `	sxi64 iMaxPhonemes = 0;` |
|      82 | 4850 | `	sxu32 w_idx = 0,nLen;` |
|      82 | 4851 | `	int nOut = 0;` |
|       - | 4852 | `	int nByte;` |
|       - | 4853 | `	char cCurr;` |
|       - | 4854 | `	/* Bounded reads standing in for the php original's NUL-terminated ones. */` |
|       - | 4855 | `#define META_BYTE(i)   ((sxu32)(i) < nLen ? zIn[(i)] : '\0')` |
|       - | 4856 | `#define META_NEXT      (META_UP(META_BYTE(w_idx + 1)))` |
|       - | 4857 | `#define META_PREV      (w_idx >= 1 ? META_UP(META_BYTE(w_idx - 1)) : '\0')` |
|       - | 4858 | `#define META_BACK(n)   (w_idx >= (sxu32)(n) ? META_UP(META_BYTE(w_idx - (sxu32)(n))) : '\0')` |
|       - | 4859 | `#define META_AFTERNEXT (META_BYTE(w_idx + 1) != '\0' ? META_UP(META_BYTE(w_idx + 2)) : '\0')` |
|       - | 4860 | `#define META_PHONIZE(c) do { zOut[nOut++] = (c); } while(0)` |
|      82 | 4861 | `	if( nArg < 1 ){` |
|       - | 4862 | `		/* Missing arguments,return the empty string */` |
|     ! 0 | 4863 | `		ph7_result_string(pCtx,"",0);` |
|     ! 0 | 4864 | `		return PH7_OK;` |
|       - | 4865 | `	}` |
|      82 | 4866 | `	zIn = ph7_value_to_string(apArg[0],&nByte);` |
|      82 | 4867 | `	if( nArg > 1 ){` |
|      13 | 4868 | `		iMaxPhonemes = ph7_value_to_int64(apArg[1]);` |
|      13 | 4869 | `		if( iMaxPhonemes < 0 ){` |
|       3 | 4870 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 4871 | `				"metaphone(): Argument #2 ($max_phonemes) must be greater than or equal to 0");` |
|       - | 4872 | `		}` |
|       5 | 4873 | `	}` |
|      80 | 4874 | `	nLen = (sxu32)(nByte > 0 ? nByte : 0);` |
|       - | 4875 | `	/* Two output bytes per input letter ('X' phonizes "KS") is the ceiling, so` |
|       - | 4876 | `	 * one allocation covers the whole run — php grows its buffer instead. */` |
|      80 | 4877 | `	zOut = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(2 * nLen + 4),FALSE,TRUE);` |
|      80 | 4878 | `	if( zOut == 0 ){` |
|     ! 0 | 4879 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 4880 | `	}` |
|       - | 4881 | `	/* Find the first letter; nothing but non-letters answers "". */` |
|      98 | 4882 | `	for( ; !META_IS_ALPHA(cCurr = META_BYTE(w_idx)) ; w_idx++ ){` |
|      23 | 4883 | `		if( cCurr == '\0' ){` |
|       5 | 4884 | `			ph7_result_string(pCtx,"",0);` |
|       5 | 4885 | `			return PH7_OK;` |
|       - | 4886 | `		}` |
|      10 | 4887 | `	}` |
|       - | 4888 | `	/* The first phoneme is processed specially. A case that neither phonizes` |
|       - | 4889 | `	 * nor advances leaves the letter for the main loop to read as an ordinary` |
|       - | 4890 | `	 * one (php's own structure). */` |
|      76 | 4891 | `	cCurr = META_UP(cCurr);` |
|      76 | 4892 | `	switch( cCurr ){` |
|       3 | 4893 | `	case 'A':` |
|       - | 4894 | `		/* AE becomes E; a vowel at the beginning is preserved */` |
|       8 | 4895 | `		if( META_NEXT == 'E' ){` |
|     ! 0 | 4896 | `			META_PHONIZE('E');` |
|     ! 0 | 4897 | `			w_idx += 2;` |
|     ! 0 | 4898 | `		}else{` |
|       8 | 4899 | `			META_PHONIZE('A');` |
|       8 | 4900 | `			w_idx++;` |
|       - | 4901 | `		}` |
|       8 | 4902 | `		break;` |
|       5 | 4903 | `	case 'G':` |
|       - | 4904 | `	case 'K':` |
|       - | 4905 | `	case 'P':` |
|       - | 4906 | `		/* [GKP]N becomes N */` |
|      11 | 4907 | `		if( META_NEXT == 'N' ){` |
|       5 | 4908 | `			META_PHONIZE('N');` |
|       5 | 4909 | `			w_idx += 2;` |
|       2 | 4910 | `		}` |
|      11 | 4911 | `		break;` |
|       3 | 4912 | `	case 'W':{` |
|       - | 4913 | `		/* WR becomes R; WH and W before a vowel keep the W; else dropped */` |
|       7 | 4914 | `		char cNext = META_NEXT;` |
|       7 | 4915 | `		if( cNext == 'R' ){` |
|       3 | 4916 | `			META_PHONIZE('R');` |
|       3 | 4917 | `			w_idx += 2;` |
|       6 | 4918 | `		}else if( cNext == 'H' \|\| META_ISVOWEL(cNext) ){` |
|       5 | 4919 | `			META_PHONIZE('W');` |
|       5 | 4920 | `			w_idx += 2;` |
|       2 | 4921 | `		}` |
|       7 | 4922 | `		break;` |
|       - | 4923 | `	}` |
|       1 | 4924 | `	case 'X':` |
|       - | 4925 | `		/* X becomes S */` |
|       3 | 4926 | `		META_PHONIZE('S');` |
|       3 | 4927 | `		w_idx++;` |
|       3 | 4928 | `		break;` |
|       2 | 4929 | `	case 'E':` |
|       - | 4930 | `	case 'I':` |
|       - | 4931 | `	case 'O':` |
|       - | 4932 | `	case 'U':` |
|       - | 4933 | `		/* Vowels are kept (A handled above) */` |
|       5 | 4934 | `		META_PHONIZE(cCurr);` |
|       5 | 4935 | `		w_idx++;` |
|       4 | 4936 | `		break;` |
|      23 | 4937 | `	default:` |
|      46 | 4938 | `		break;` |
|       - | 4939 | `	}` |
|       - | 4940 | `	/* On to the metaphoning */` |
|     658 | 4941 | `	for( ; (cCurr = META_BYTE(w_idx)) != '\0' &&` |
|     604 | 4942 | `	       (iMaxPhonemes == 0 \|\| (sxi64)nOut < iMaxPhonemes) ; w_idx++ ){` |
|       - | 4943 | `		/* Letters an encoding below consumed along with this one */` |
|     388 | 4944 | `		sxu32 nSkip = 0;` |
|       - | 4945 | `		char cPrev;` |
|     388 | 4946 | `		if( !META_IS_ALPHA(cCurr) ){` |
|       5 | 4947 | `			continue;` |
|       - | 4948 | `		}` |
|     384 | 4949 | `		cCurr = META_UP(cCurr);` |
|     384 | 4950 | `		cPrev = META_PREV;` |
|       - | 4951 | `		/* Drop duplicates, except CC */` |
|     384 | 4952 | `		if( cCurr == cPrev && cCurr != 'C' ){` |
|       7 | 4953 | `			continue;` |
|       - | 4954 | `		}` |
|     378 | 4955 | `		switch( cCurr ){` |
|       3 | 4956 | `		case 'B':` |
|       - | 4957 | `			/* B unless in MB */` |
|       8 | 4958 | `			if( cPrev != 'M' ){` |
|       3 | 4959 | `				META_PHONIZE('B');` |
|       1 | 4960 | `			}` |
|       8 | 4961 | `			break;` |
|      17 | 4962 | `		case 'C':{` |
|       - | 4963 | `			/* 'sh' in -CIA- and -CH-; S in -CI-, -CE-, -CY-;` |
|       - | 4964 | `			 * dropped in -SCI-, -SCE-, -SCY-; else K */` |
|      36 | 4965 | `			char cNext = META_NEXT;` |
|      36 | 4966 | `			if( META_MAKESOFT(cNext) ){ /* C[IEY] */` |
|       7 | 4967 | `				if( cNext == 'I' && META_AFTERNEXT == 'A' ){ /* CIA */` |
|     ! 0 | 4968 | `					META_PHONIZE(META_SH);` |
|       6 | 4969 | `				}else if( cPrev == 'S' ){` |
|       - | 4970 | `					/* dropped */` |
|       2 | 4971 | `				}else{` |
|       5 | 4972 | `					META_PHONIZE('S');` |
|       1 | 4973 | `				}` |
|      33 | 4974 | `			}else if( cNext == 'H' ){` |
|      15 | 4975 | `				META_PHONIZE(META_SH);` |
|      15 | 4976 | `				nSkip++;` |
|       8 | 4977 | `			}else{` |
|      16 | 4978 | `				META_PHONIZE('K');` |
|       - | 4979 | `			}` |
|      36 | 4980 | `			break;` |
|       - | 4981 | `		}` |
|       5 | 4982 | `		case 'D':` |
|       - | 4983 | `			/* J in -DGE-, -DGI-, -DGY-; else T */` |
|      14 | 4984 | `			if( META_NEXT == 'G' && META_MAKESOFT(META_AFTERNEXT) ){` |
|       3 | 4985 | `				META_PHONIZE('J');` |
|       3 | 4986 | `				nSkip++;` |
|       2 | 4987 | `			}else{` |
|       9 | 4988 | `				META_PHONIZE('T');` |
|       - | 4989 | `			}` |
|      11 | 4990 | `			break;` |
|       7 | 4991 | `		case 'G':{` |
|       - | 4992 | `			/* F in -GH unless B--GH, D--GH, -H--GH, -H---GH (silent there);` |
|       - | 4993 | `			 * dropped in -GN, -GNED (and -DG[EIY]-, handled in D);` |
|       - | 4994 | `			 * J in -GE-, -GI-, -GY- when not GG; else K */` |
|      15 | 4995 | `			char cNext = META_NEXT;` |
|      15 | 4996 | `			if( cNext == 'H' ){` |
|      17 | 4997 | `				if( !(META_NOGHTOF(META_BACK(3)) \|\| META_BACK(4) == 'H') ){` |
|       9 | 4998 | `					META_PHONIZE('F');` |
|       9 | 4999 | `					nSkip++;` |
|       5 | 5000 | `				}` |
|      11 | 5001 | `			}else if( cNext == 'N' ){` |
|     ! 0 | 5002 | `				char cAfterNext = META_AFTERNEXT;` |
|     ! 0 | 5003 | `				if( !META_IS_ALPHA(cAfterNext) \|\|` |
|     ! 0 | 5004 | `				    (cAfterNext == 'E' && META_UP(MetaLookahead(zIn,nLen,w_idx,3)) == 'D') ){` |
|       - | 5005 | `					/* dropped */` |
|     ! 0 | 5006 | `				}else{` |
|     ! 0 | 5007 | `					META_PHONIZE('K');` |
|     ! 0 | 5008 | `				}` |
|       7 | 5009 | `			}else if( META_MAKESOFT(cNext) && cPrev != 'G' ){` |
|       3 | 5010 | `				META_PHONIZE('J');` |
|       2 | 5011 | `			}else{` |
|       5 | 5012 | `				META_PHONIZE('K');` |
|       - | 5013 | `			}` |
|      15 | 5014 | `			break;` |
|       - | 5015 | `		}` |
|       3 | 5016 | `		case 'H':` |
|       - | 5017 | `			/* H before a vowel and not after C, G, P, S, T */` |
|       7 | 5018 | `			if( META_ISVOWEL(META_NEXT) && !META_AFFECTH(cPrev) ){` |
|       3 | 5019 | `				META_PHONIZE('H');` |
|       1 | 5020 | `			}` |
|       7 | 5021 | `			break;` |
|       2 | 5022 | `		case 'K':` |
|       - | 5023 | `			/* dropped after C; else K */` |
|       5 | 5024 | `			if( cPrev != 'C' ){` |
|       3 | 5025 | `				META_PHONIZE('K');` |
|       1 | 5026 | `			}` |
|       5 | 5027 | `			break;` |
|       6 | 5028 | `		case 'P':` |
|       - | 5029 | `			/* F before H; else P */` |
|      14 | 5030 | `			if( META_NEXT == 'H' ){` |
|       3 | 5031 | `				META_PHONIZE('F');` |
|       2 | 5032 | `			}else{` |
|      12 | 5033 | `				META_PHONIZE('P');` |
|       - | 5034 | `			}` |
|      14 | 5035 | `			break;` |
|       2 | 5036 | `		case 'Q':` |
|       5 | 5037 | `			META_PHONIZE('K');` |
|       5 | 5038 | `			break;` |
|      14 | 5039 | `		case 'S':{` |
|       - | 5040 | `			/* 'sh' in -SH-, -SIO-, -SIA-; else S */` |
|      30 | 5041 | `			char cNext = META_NEXT;` |
|       - | 5042 | `			char cAfterNext;` |
|      30 | 5043 | `			if( cNext == 'I' &&` |
|       2 | 5044 | `			    ((cAfterNext = META_AFTERNEXT) == 'O' \|\| cAfterNext == 'A') ){` |
|       3 | 5045 | `				META_PHONIZE(META_SH);` |
|      28 | 5046 | `			}else if( cNext == 'H' ){` |
|     ! 0 | 5047 | `				META_PHONIZE(META_SH);` |
|     ! 0 | 5048 | `				nSkip++;` |
|     ! 0 | 5049 | `			}else{` |
|      28 | 5050 | `				META_PHONIZE('S');` |
|       - | 5051 | `			}` |
|      30 | 5052 | `			break;` |
|       - | 5053 | `		}` |
|      17 | 5054 | `		case 'T':{` |
|       - | 5055 | `			/* 'sh' in -TIA-, -TIO-; 'th' before H; dropped in -TCH-; else T */` |
|      36 | 5056 | `			char cNext = META_NEXT;` |
|       - | 5057 | `			char cAfterNext;` |
|      36 | 5058 | `			if( cNext == 'I' &&` |
|       4 | 5059 | `			    ((cAfterNext = META_AFTERNEXT) == 'O' \|\| cAfterNext == 'A') ){` |
|       5 | 5060 | `				META_PHONIZE(META_SH);` |
|      33 | 5061 | `			}else if( cNext == 'H' ){` |
|      12 | 5062 | `				META_PHONIZE(META_TH);` |
|      12 | 5063 | `				nSkip++;` |
|      26 | 5064 | `			}else if( !(cNext == 'C' && META_AFTERNEXT == 'H') ){` |
|      17 | 5065 | `				META_PHONIZE('T');` |
|       8 | 5066 | `			}` |
|      36 | 5067 | `			break;` |
|       - | 5068 | `		}` |
|       2 | 5069 | `		case 'V':` |
|       5 | 5070 | `			META_PHONIZE('F');` |
|       5 | 5071 | `			break;` |
|       1 | 5072 | `		case 'W':` |
|       - | 5073 | `			/* W before a vowel, else dropped */` |
|       3 | 5074 | `			if( META_ISVOWEL(META_NEXT) ){` |
|     ! 0 | 5075 | `				META_PHONIZE('W');` |
|     ! 0 | 5076 | `			}` |
|       3 | 5077 | `			break;` |
|       2 | 5078 | `		case 'X':` |
|       6 | 5079 | `			META_PHONIZE('K');` |
|       6 | 5080 | `			META_PHONIZE('S');` |
|       6 | 5081 | `			break;` |
|       7 | 5082 | `		case 'Y':` |
|       - | 5083 | `			/* Y before a vowel, else dropped */` |
|      15 | 5084 | `			if( META_ISVOWEL(META_NEXT) ){` |
|       3 | 5085 | `				META_PHONIZE('Y');` |
|       1 | 5086 | `			}` |
|      15 | 5087 | `			break;` |
|       1 | 5088 | `		case 'Z':` |
|       3 | 5089 | `			META_PHONIZE('S');` |
|       3 | 5090 | `			break;` |
|      33 | 5091 | `		case 'F':` |
|       - | 5092 | `		case 'J':` |
|       - | 5093 | `		case 'L':` |
|       - | 5094 | `		case 'M':` |
|       - | 5095 | `		case 'N':` |
|       - | 5096 | `		case 'R':` |
|       - | 5097 | `			/* passed through unchanged */` |
|      68 | 5098 | `			META_PHONIZE(cCurr);` |
|      66 | 5099 | `			break;` |
|      66 | 5100 | `		default:` |
|     132 | 5101 | `			break;` |
|       - | 5102 | `		}` |
|     378 | 5103 | `		w_idx += nSkip;` |
|     190 | 5104 | `	}` |
|      76 | 5105 | `	ph7_result_string(pCtx,zOut,nOut);` |
|      76 | 5106 | `	return PH7_OK;` |
|       - | 5107 | `#undef META_BYTE` |
|       - | 5108 | `#undef META_NEXT` |
|       - | 5109 | `#undef META_PREV` |
|       - | 5110 | `#undef META_BACK` |
|       - | 5111 | `#undef META_AFTERNEXT` |
|       - | 5112 | `#undef META_PHONIZE` |
|      42 | 5113 | `}` |
|       - | 5114 | `/*` |
|       - | 5115 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|       - | 5116 | ` *  Wraps a string to a given number of characters.` |
|       - | 5117 | ` * Parameters` |
|       - | 5118 | ` *  $str` |
|       - | 5119 | ` *   The input string.` |
|       - | 5120 | ` * $width` |
|       - | 5121 | ` *  The column width.` |
|       - | 5122 | ` * $break` |
|       - | 5123 | ` *  The line is broken using the optional break parameter.` |
|       - | 5124 | ` * Return` |
|       - | 5125 | ` *  Returns the given string wrapped at the specified column.` |
|       - | 5126 | ` */` |
|      28 | 5127 | `PH7_PRIVATE int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       2 | 5128 | `{` |
|       - | 5129 | `	const char *zIn,*zBreak;` |
|       - | 5130 | `	SyBlob sWorker;` |
|       - | 5131 | `	int iLen,iBreaklen,iWidth,iCut,iStart,iSpace,iCur;` |
|       - | 5132 | `	sxi32 rc;` |
|      30 | 5133 | `	if( nArg < 1 ){` |
|       - | 5134 | `		/* Missing arguments,return the empty string */` |
|     ! 0 | 5135 | `		ph7_result_string(pCtx,"",0);` |
|     ! 0 | 5136 | `		return PH7_OK;` |
|       - | 5137 | `	}` |
|       - | 5138 | `	/* Extract the input string */` |
|      30 | 5139 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|       - | 5140 | `	/* Width (default 75; PHP allows 0/negative — break at every space). */` |
|      30 | 5141 | `	iWidth = 75;` |
|      30 | 5142 | `	if( nArg > 1 ){` |
|      27 | 5143 | `		iWidth = ph7_value_to_int(apArg[1]);` |
|      13 | 5144 | `	}` |
|       - | 5145 | `	/* Break string (default "\n"). */` |
|      30 | 5146 | `	zBreak = "\n";` |
|      30 | 5147 | `	iBreaklen = (int)sizeof(char);` |
|      30 | 5148 | `	if( nArg > 2 ){` |
|      13 | 5149 | `		zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|       6 | 5150 | `	}` |
|       - | 5151 | `	/* Cut long words? (default false). */` |
|      30 | 5152 | `	iCut = 0;` |
|      30 | 5153 | `	if( nArg > 3 ){` |
|       7 | 5154 | `		iCut = ph7_value_to_bool(apArg[3]);` |
|       3 | 5155 | `	}` |
|      30 | 5156 | `	if( iLen < 1 ){` |
|       - | 5157 | `		/* PHP returns the empty string for empty input before validating the other args. */` |
|       8 | 5158 | `		ph7_result_string(pCtx,"",0);` |
|       8 | 5159 | `		return PH7_OK;` |
|       - | 5160 | `	}` |
|       - | 5161 | `	/* PHP 8 domain errors (catchable ValueError). */` |
|      23 | 5162 | `	if( iBreaklen < 1 ){` |
|       3 | 5163 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 5164 | `			"wordwrap(): Argument #3 ($break) must not be empty");` |
|       - | 5165 | `	}` |
|      21 | 5166 | `	if( iWidth == 0 && iCut ){` |
|       3 | 5167 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 5168 | `			"wordwrap(): Argument #4 ($cut_long_words) cannot be true when argument #2 ($width) is 0");` |
|       - | 5169 | `	}` |
|       - | 5170 | `	/*` |
|       - | 5171 | `	 * PHP's algorithm: a single left-to-right pass tracking the start of the` |
|       - | 5172 | `	 * current line (iStart) and the position of the last space seen on it` |
|       - | 5173 | `	 * (iSpace). A break is emitted when the line reaches the width, at the last` |
|       - | 5174 | `	 * space if there was one, otherwise (only when cut is enabled) hard at the` |
|       - | 5175 | `	 * boundary. An existing break sequence in the input resets the line.` |
|       - | 5176 | `	 */` |
|      19 | 5177 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|      19 | 5178 | `	iStart = iSpace = iCur = 0;` |
|      19 | 5179 | `	rc = SXRET_OK;` |
|     551 | 5180 | `	while( iCur < iLen ){` |
|     533 | 5181 | `		if( iBreaklen <= iLen - iCur && SyMemcmp(&zIn[iCur],zBreak,(sxu32)iBreaklen) == 0 ){` |
|       - | 5182 | `			/* Existing break sequence in the input: copy it verbatim and reset the line. */` |
|     ! 0 | 5183 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart + iBreaklen));` |
|     ! 0 | 5184 | `			if( rc != SXRET_OK ){ goto oom; }` |
|     ! 0 | 5185 | `			iCur += iBreaklen;` |
|     ! 0 | 5186 | `			iStart = iSpace = iCur;` |
|     ! 0 | 5187 | `			continue;` |
|     533 | 5188 | `		}else if( zIn[iCur] == ' ' ){` |
|      67 | 5189 | `			if( iCur - iStart >= iWidth ){` |
|       - | 5190 | `				/* The line already fills the width at this space: break here (the space is consumed). */` |
|      13 | 5191 | `				rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|      13 | 5192 | `				if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|      13 | 5193 | `				if( rc != SXRET_OK ){ goto oom; }` |
|      13 | 5194 | `				iStart = iCur + 1;` |
|       6 | 5195 | `			}` |
|      67 | 5196 | `			iSpace = iCur;` |
|     500 | 5197 | `		}else if( iCut && iCur - iStart >= iWidth && iStart >= iSpace ){` |
|       - | 5198 | `			/* A word longer than the width with no space to break at: hard-cut at the boundary. */` |
|       7 | 5199 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|       7 | 5200 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|       7 | 5201 | `			if( rc != SXRET_OK ){ goto oom; }` |
|       7 | 5202 | `			iStart = iSpace = iCur;` |
|     464 | 5203 | `		}else if( iCur - iStart >= iWidth && iStart < iSpace ){` |
|       - | 5204 | `			/* Past the width mid-word: wrap back to the last space (which is consumed). */` |
|      17 | 5205 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iSpace - iStart));` |
|      17 | 5206 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|      17 | 5207 | `			if( rc != SXRET_OK ){ goto oom; }` |
|      17 | 5208 | `			iStart = iSpace = iSpace + 1;` |
|       8 | 5209 | `		}` |
|     533 | 5210 | `		iCur++;` |
|       1 | 5211 | `	}` |
|       - | 5212 | `	/* Emit the trailing chunk. */` |
|      19 | 5213 | `	if( iStart < iCur ){` |
|      19 | 5214 | `		rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|      19 | 5215 | `		if( rc != SXRET_OK ){ goto oom; }` |
|       9 | 5216 | `	}` |
|      19 | 5217 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|      19 | 5218 | `	SyBlobRelease(&sWorker);` |
|      19 | 5219 | `	return PH7_OK;` |
|     ! 0 | 5220 | `oom:` |
|     ! 0 | 5221 | `	SyBlobRelease(&sWorker);` |
|     ! 0 | 5222 | `	return PH7_ContextMemoryError(pCtx);` |
|      16 | 5223 | `}` |
|       - | 5224 | `/*` |
|       - | 5225 | ` * Check if the given character is a member of the given mask.` |
|       - | 5226 | ` * Return TRUE on success. FALSE otherwise.` |
|       - | 5227 | ` * Refer to [strtok()].` |
|       - | 5228 | ` */` |
|     870 | 5229 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|       1 | 5230 | `{` |
|       - | 5231 | `	int i;` |
|    1727 | 5232 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     895 | 5233 | `		if( c == zMask[i] ){` |
|      39 | 5234 | `			if( pOfft ){` |
|      19 | 5235 | `				*pOfft = i;` |
|       9 | 5236 | `			}` |
|      39 | 5237 | `			return TRUE;` |
|       - | 5238 | `		}` |
|     429 | 5239 | `	}` |
|     833 | 5240 | `	return FALSE;` |
|     436 | 5241 | `}` |
|       - | 5242 | `/*` |
|       - | 5243 | ` * Extract a single token from the input stream.` |
|       - | 5244 | ` * Refer to [strtok()].` |
|       - | 5245 | ` */` |
|      18 | 5246 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|       1 | 5247 | `{` |
|      19 | 5248 | `	const char *zIn = *pzIn;` |
|       - | 5249 | `	const char *zPtr;` |
|       - | 5250 | `	/* Ignore leading delimiter */` |
|      23 | 5251 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|       5 | 5252 | `		zIn++;` |
|       1 | 5253 | `	}` |
|      19 | 5254 | `	if( zIn >= zEnd ){` |
|       - | 5255 | `		/* End of input */` |
|     ! 0 | 5256 | `		return SXERR_EOF;` |
|       - | 5257 | `	}` |
|      19 | 5258 | `	zPtr = zIn;` |
|       - | 5259 | `	/* Extract the token */` |
|     709 | 5260 | `	while( zIn < zEnd ){` |
|     707 | 5261 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|       - | 5262 | `			/* UTF-8 stream */` |
|     ! 0 | 5263 | `			zIn++;` |
|     ! 0 | 5264 | `			SX_JMP_UTF8(zIn,zEnd);` |
|     ! 0 | 5265 | `		}else{` |
|     707 | 5266 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      17 | 5267 | `				break;` |
|       - | 5268 | `			}` |
|     691 | 5269 | `			zIn++;` |
|       - | 5270 | `		}` |
|       1 | 5271 | `	}` |
|      19 | 5272 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|       - | 5273 | `	/* Update the cursor */` |
|      19 | 5274 | `	*pzIn = zIn;` |
|       - | 5275 | `	/* Return to the caller */` |
|      19 | 5276 | `	return SXRET_OK;` |
|      10 | 5277 | `}` |
|       - | 5278 | `/* strtok auxiliary private data */` |
|       - | 5279 | `typedef struct strtok_aux_data strtok_aux_data;` |
|       - | 5280 | `struct strtok_aux_data` |
|       - | 5281 | `{` |
|       - | 5282 | `	const char *zDup;  /* Complete duplicate of the input */` |
|       - | 5283 | `	const char *zIn;   /* Current input stream */` |
|       - | 5284 | `	const char *zEnd;  /* End of input */` |
|       - | 5285 | `};` |
|       - | 5286 | `/*` |
|       - | 5287 | ` * string strtok(string $str,string $token)` |
|       - | 5288 | ` * string strtok(string $token)` |
|       - | 5289 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|       - | 5290 | ` *  being delimited by any character from token. That is, if you have a string like` |
|       - | 5291 | ` *  "This is an example string" you could tokenize this string into its individual` |
|       - | 5292 | ` *  words by using the space character as the token.` |
|       - | 5293 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|       - | 5294 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|       - | 5295 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|       - | 5296 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|       - | 5297 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|       - | 5298 | ` *  the argument are found.` |
|       - | 5299 | ` * Parameters` |
|       - | 5300 | ` *  $str` |
|       - | 5301 | ` *  The string being split up into smaller strings (tokens).` |
|       - | 5302 | ` * $token` |
|       - | 5303 | ` *  The delimiter used when splitting up str.` |
|       - | 5304 | ` * Return` |
|       - | 5305 | ` *   Current token or FALSE on EOF.` |
|       - | 5306 | ` */` |
|      18 | 5307 | `PH7_PRIVATE int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5308 | `{` |
|       - | 5309 | `	strtok_aux_data *pAux;` |
|       - | 5310 | `	const char *zMask;` |
|       - | 5311 | `	SyString sToken;` |
|       - | 5312 | `	int nMasklen;` |
|       - | 5313 | `	sxi32 rc;` |
|      19 | 5314 | `	if( nArg < 2 ){` |
|       - | 5315 | `		/* Extract top aux data */` |
|       5 | 5316 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|       5 | 5317 | `		if( pAux == 0 ){` |
|       - | 5318 | `			/* No aux data,return FALSE */` |
|     ! 0 | 5319 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 5320 | `			return PH7_OK;` |
|       - | 5321 | `		}` |
|       5 | 5322 | `		nMasklen = 0;` |
|       5 | 5323 | `		zMask = ""; /* cc warning */` |
|       5 | 5324 | `		if( nArg > 0 ){` |
|       - | 5325 | `			/* Extract the mask */` |
|       5 | 5326 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|       2 | 5327 | `		}` |
|       5 | 5328 | `		if( nMasklen < 1 ){` |
|       - | 5329 | `			/* Invalid mask,return FALSE */` |
|     ! 0 | 5330 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|     ! 0 | 5331 | `			ph7_context_free_chunk(pCtx,pAux);` |
|     ! 0 | 5332 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|     ! 0 | 5333 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 5334 | `			return PH7_OK;` |
|       - | 5335 | `		}` |
|       - | 5336 | `		/* Extract the token */` |
|       5 | 5337 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|       5 | 5338 | `		if( rc != SXRET_OK ){` |
|       - | 5339 | `			/* EOF ,discard the aux data */` |
|     ! 0 | 5340 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|     ! 0 | 5341 | `			ph7_context_free_chunk(pCtx,pAux);` |
|     ! 0 | 5342 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|     ! 0 | 5343 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 5344 | `		}else{` |
|       - | 5345 | `			/* Return the extracted token */` |
|       5 | 5346 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|       - | 5347 | `		}` |
|       3 | 5348 | `	}else{` |
|       - | 5349 | `		const char *zInput,*zCur;` |
|       - | 5350 | `		char *zDup;` |
|       - | 5351 | `		int nLen;` |
|       - | 5352 | `		/* Extract the raw input */` |
|      15 | 5353 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      15 | 5354 | `		if( nLen < 1 ){` |
|       - | 5355 | `			/* Empty input,return FALSE */` |
|     ! 0 | 5356 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 5357 | `			return PH7_OK;` |
|       - | 5358 | `		}` |
|       - | 5359 | `		/* Extract the mask */` |
|      15 | 5360 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      15 | 5361 | `		if( nMasklen < 1 ){` |
|       - | 5362 | `			/* Set a default mask */` |
|       - | 5363 | `#define TOK_MASK " \n\t\r\f"` |
|     ! 0 | 5364 | `			zMask = TOK_MASK;` |
|     ! 0 | 5365 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|       - | 5366 | `#undef TOK_MASK` |
|     ! 0 | 5367 | `		}` |
|       - | 5368 | `		/* Extract a single token */` |
|      15 | 5369 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      15 | 5370 | `		if( rc != SXRET_OK ){` |
|       - | 5371 | `			/* Empty input */` |
|     ! 0 | 5372 | `			ph7_result_bool(pCtx,0);` |
|     ! 0 | 5373 | `			return PH7_OK;` |
|     ! 0 | 5374 | `		}else{` |
|       - | 5375 | `			/* Return the extracted token */` |
|      15 | 5376 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|       - | 5377 | `		}` |
|       - | 5378 | `		/* Create our auxilliary data and copy the input */` |
|      15 | 5379 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      15 | 5380 | `		if( pAux ){` |
|      15 | 5381 | `			nLen -= (int)(zInput-zCur);` |
|      15 | 5382 | `			if( nLen < 1 ){` |
|     ! 0 | 5383 | `				ph7_context_free_chunk(pCtx,pAux);` |
|     ! 0 | 5384 | `				return PH7_OK;` |
|       - | 5385 | `			}` |
|       - | 5386 | `			/* Duplicate input */` |
|      15 | 5387 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      15 | 5388 | `			if( zDup  ){` |
|      15 | 5389 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|       - | 5390 | `				/* Register the aux data */` |
|      15 | 5391 | `				pAux->zDup = pAux->zIn = zDup;` |
|      15 | 5392 | `				pAux->zEnd = &zDup[nLen];` |
|      15 | 5393 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|       7 | 5394 | `			}` |
|       7 | 5395 | `		}` |
|       - | 5396 | `	}` |
|      19 | 5397 | `	return PH7_OK;` |
|      10 | 5398 | `}` |
|       - | 5399 | `/*` |
|       - | 5400 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|       - | 5401 | ` *  Pad a string to a certain length with another string` |
|       - | 5402 | ` * Parameters` |
|       - | 5403 | ` *  $input` |
|       - | 5404 | ` *   The input string.` |
|       - | 5405 | ` * $pad_length` |
|       - | 5406 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|       - | 5407 | ` *   string, no padding takes place.` |
|       - | 5408 | ` * $pad_string` |
|       - | 5409 | ` *   Note:` |
|       - | 5410 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|       - | 5411 | ` *    divided by the pad_string's length.` |
|       - | 5412 | ` * $pad_type` |
|       - | 5413 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|       - | 5414 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|       - | 5415 | ` * Return` |
|       - | 5416 | ` *  The padded string.` |
|       - | 5417 | ` */` |
|    2694 | 5418 | `PH7_PRIVATE int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5419 | `{` |
|       - | 5420 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|       - | 5421 | `	const char *zIn,*zPad;` |
|    2699 | 5422 | `	if( nArg < 2 ){` |
|       - | 5423 | `		/* Missing arguments,return the empty string */` |
|     ! 0 | 5424 | `		ph7_result_string(pCtx,"",0);` |
|     ! 0 | 5425 | `		return PH7_OK;` |
|       - | 5426 | `	}` |
|       - | 5427 | `	/* Extract the target string */` |
|    2699 | 5428 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|       - | 5429 | `	/* Padding length */` |
|       - | 5430 | `	{` |
|    2699 | 5431 | `		sxi64 iTmp = 0;` |
|    2699 | 5432 | `		sxi32 rcArg = PH7_IntArgResolve(pCtx,apArg[1],"str_pad",2,"$length","int",&iTmp);` |
|    2699 | 5433 | `		if( rcArg != PH7_OK ){` |
|     ! 0 | 5434 | `			return rcArg;` |
|       - | 5435 | `		}` |
|    2699 | 5436 | `		iRealPad = iPadlen = (int)iTmp;` |
|       - | 5437 | `	}` |
|    2699 | 5438 | `	if( iPadlen > 0 ){` |
|    2697 | 5439 | `		iPadlen -= iLen;` |
|    1346 | 5440 | `	}` |
|    2699 | 5441 | `	if( iPadlen < 1  ){` |
|       - | 5442 | `		/* Return the string verbatim */` |
|      18 | 5443 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      18 | 5444 | `		return PH7_OK;` |
|       - | 5445 | `	}` |
|    2683 | 5446 | `	zPad = " "; /* Whitespace padding */` |
|    2683 | 5447 | `	iStrpad = (int)sizeof(char);` |
|    2683 | 5448 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|    2683 | 5449 | `	if( nArg > 2 ){` |
|       - | 5450 | `		/* Padding string */` |
|      27 | 5451 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      27 | 5452 | `		if( iStrpad < 1 ){` |
|       - | 5453 | `			/* An empty pad string throws a catchable ValueError in PHP 8` |
|       - | 5454 | `			 * (only reached once padding is actually required). */` |
|       3 | 5455 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 5456 | `				"str_pad(): Argument #3 ($pad_string) must not be empty");` |
|       - | 5457 | `		}` |
|      25 | 5458 | `		if( nArg > 3 ){` |
|       - | 5459 | `			/* Padd type. php 8: anything outside LEFT(0)/RIGHT(1)/BOTH(2) is a` |
|       - | 5460 | `			 * catchable ValueError (PHL used to fall back to RIGHT silently);` |
|       - | 5461 | `			 * like the empty-pad check above, php only reaches it once padding` |
|       - | 5462 | `			 * is actually required (probed: str_pad("abc",2," ",9) is "abc"). */` |
|      21 | 5463 | `			iType = ph7_value_to_int(apArg[3]);` |
|      21 | 5464 | `			if( iType < 0 \|\| iType > 2 ){` |
|       5 | 5465 | `				return PH7_VmThrowException(pCtx,"ValueError",` |
|       - | 5466 | `					"str_pad(): Argument #4 ($pad_type) must be STR_PAD_LEFT, STR_PAD_RIGHT, or STR_PAD_BOTH");` |
|       - | 5467 | `			}` |
|       8 | 5468 | `		}` |
|      10 | 5469 | `	}` |
|    2677 | 5470 | `	iDiv = 1;` |
|    2677 | 5471 | `	if( iType == 2 ){` |
|       3 | 5472 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|       1 | 5473 | `	}` |
|       - | 5474 | `	/* Perform the requested operation */` |
|    2677 | 5475 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      13 | 5476 | `		jPad = iStrpad;` |
|      36 | 5477 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|       - | 5478 | `			/* Padding */` |
|      34 | 5479 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      11 | 5480 | `				break;` |
|       - | 5481 | `			}` |
|      24 | 5482 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      12 | 5483 | `		}` |
|      13 | 5484 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      21 | 5485 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      11 | 5486 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      11 | 5487 | `				if( jPad > iStrpad ){` |
|     ! 0 | 5488 | `					jPad = iStrpad;` |
|     ! 0 | 5489 | `				}` |
|      11 | 5490 | `				if( jPad < 1){` |
|     ! 0 | 5491 | `					break;` |
|       - | 5492 | `				}` |
|      11 | 5493 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|       1 | 5494 | `			}` |
|       5 | 5495 | `		}` |
|       6 | 5496 | `	}` |
|    2677 | 5497 | `	if( iLen > 0 ){` |
|       - | 5498 | `		/* Append the input string */` |
|    2677 | 5499 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|    1336 | 5500 | `	}` |
|    2677 | 5501 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|   25161 | 5502 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|       - | 5503 | `			/* Padding */` |
|   25159 | 5504 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|    2665 | 5505 | `				break;` |
|       - | 5506 | `			}` |
|   22499 | 5507 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|   11252 | 5508 | `		}` |
|    5329 | 5509 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|    2667 | 5510 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|    2667 | 5511 | `			if( jPad > iStrpad ){` |
|     ! 0 | 5512 | `				jPad = iStrpad;` |
|     ! 0 | 5513 | `			}` |
|    2667 | 5514 | `			if( jPad < 1){` |
|     ! 0 | 5515 | `				break;` |
|       - | 5516 | `			}` |
|    2667 | 5517 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|       5 | 5518 | `		}` |
|    1331 | 5519 | `	}` |
|    2677 | 5520 | `	return PH7_OK;` |
|    1352 | 5521 | `}` |
|       - | 5522 | `/*` |
|       - | 5523 | ` * String replacement private data.` |
|       - | 5524 | ` */` |
|       - | 5525 | `typedef struct str_replace_data str_replace_data;` |
|       - | 5526 | `struct str_replace_data` |
|       - | 5527 | `{` |
|       - | 5528 | `	/* Used by the str_replace family to collect the search/replace arguments. */` |
|       - | 5529 | `	SySet *pCollector;  /* Argument collector*/` |
|       - | 5530 | `	ph7_context *pCtx;  /* Call context */` |
|       - | 5531 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|       - | 5532 | `};` |
|       - | 5533 | `/*` |
|       - | 5534 | ` * Remove a substring.` |
|       - | 5535 | ` */` |
|       - | 5536 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|       - | 5537 | `	for(;;){\` |
|       - | 5538 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|       - | 5539 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|       - | 5540 | `		++OFFT;\` |
|       - | 5541 | `	}\` |
|       - | 5542 | `}` |
|       - | 5543 | `/*` |
|       - | 5544 | ` * Shift right and insert algorithm.` |
|       - | 5545 | ` */` |
|       - | 5546 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|       - | 5547 | `		sxu32 INLEN = LEN - OFFT;\` |
|       - | 5548 | `		for(;;){\` |
|       - | 5549 | `			if( LEN > 0 ){ LEN--; }\` |
|       - | 5550 | `			if(INLEN < 1 ) { break; }\` |
|       - | 5551 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|       - | 5552 | `			--INLEN; \` |
|       - | 5553 | `		}\` |
|       - | 5554 | `		for(;;){\` |
|       - | 5555 | `				if(ELEN < 1) { break; }\` |
|       - | 5556 | `				SRC[OFFT] = ENTRY[0];\` |
|       - | 5557 | `				OFFT++;\` |
|       - | 5558 | `				ENTRY++;\` |
|       - | 5559 | `				--ELEN;\` |
|       - | 5560 | `		}\` |
|       - | 5561 | `}` |
|       - | 5562 | `/*` |
|       - | 5563 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|       - | 5564 | ` * replacement string [i.e: zReplace].` |
|       - | 5565 | ` */` |
|    9008 | 5566 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|       5 | 5567 | `{` |
|    9013 | 5568 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|       - | 5569 | `	sxu32 n,m;` |
|    9013 | 5570 | `	n = SyBlobLength(pWorker);` |
|    9013 | 5571 | `	m = nOfft;` |
|       - | 5572 | `	/* Delete the old entry */` |
|  440905 | 5573 | `	STRDEL(zInput,n,m,nLen);` |
|    9013 | 5574 | `	SyBlobLength(pWorker) -= nLen;` |
|    9013 | 5575 | `	if( nReplen > 0 ){` |
|    2897 | 5576 | `		sxi32 iRep = nReplen;` |
|       - | 5577 | `		sxi32 rc;` |
|       - | 5578 | `		/*` |
|       - | 5579 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|       - | 5580 | `		 * string.` |
|       - | 5581 | `		 */` |
|    2897 | 5582 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|    2897 | 5583 | `		if( rc != SXRET_OK ){` |
|       - | 5584 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|       - | 5585 | `			 * instead of returning a partially-replaced string as success. */` |
|     ! 0 | 5586 | `			return rc;` |
|       - | 5587 | `		}` |
|       - | 5588 | `		/* Perform the insertion now */` |
|    2897 | 5589 | `		zInput = (char *)SyBlobData(pWorker);` |
|    2897 | 5590 | `		n = SyBlobLength(pWorker);` |
|  180317 | 5591 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|    2897 | 5592 | `		SyBlobLength(pWorker) += nReplen;` |
|    1446 | 5593 | `	}` |
|    9013 | 5594 | `	return SXRET_OK;` |
|    4509 | 5595 | `}` |
|       - | 5596 | `/*` |
|       - | 5597 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|       - | 5598 | ` * to collect search/replace string.` |
|       - | 5599 | ` * This callback is invoked only if the given argument is of type array.` |
|       - | 5600 | ` */` |
|    7262 | 5601 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|       5 | 5602 | `{` |
|    7267 | 5603 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|       - | 5604 | `	SyString sWorker;` |
|       - | 5605 | `	const char *zIn;` |
|       - | 5606 | `	int nByte;` |
|       - | 5607 | `	/* Extract a string representation of the given argument */` |
|    7267 | 5608 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|    7267 | 5609 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|    7267 | 5610 | `	if( nByte > 0 ){` |
|       - | 5611 | `		char *zDup;` |
|       - | 5612 | `		/* Duplicate the chunk */` |
|    6775 | 5613 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|       - | 5614 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|       - | 5615 | `			);` |
|    6775 | 5616 | `		if( zDup == 0 ){` |
|       - | 5617 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|       - | 5618 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|     ! 0 | 5619 | `			pRep->rc = SXERR_MEM;` |
|     ! 0 | 5620 | `			return SXERR_MEM;` |
|       - | 5621 | `		}` |
|    6775 | 5622 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|       - | 5623 | `		/* Save the chunk */` |
|    6775 | 5624 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|    3385 | 5625 | `	}` |
|       - | 5626 | `	/* Save for later processing */` |
|    7267 | 5627 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|       - | 5628 | `	/* All done */` |
|    3631 | 5629 | `	SXUNUSED(pKey); /* cc warning */` |
|    7267 | 5630 | `	return PH7_OK;` |
|    3636 | 5631 | `}` |
|       - | 5632 | `/*` |
|       - | 5633 | ` * Run the collected search/replace pairs over a single subject string, writing` |
|       - | 5634 | ` * the transformed bytes into pOut (reset here). Shared by the scalar-subject and` |
|       - | 5635 | ` * the array-subject (element-wise) paths. The search/replace SySets are walked` |
|       - | 5636 | ` * fresh on every call — cursors are reset here — so each array element is` |
|       - | 5637 | ` * transformed independently, exactly like php. Returns SXRET_OK, or SXERR_MEM` |
|       - | 5638 | ` * on an allocation failure inside StringReplace.` |
|       - | 5639 | ` *` |
|       - | 5640 | ` * *pnCount is INCREMENTED (never reset) by the number of replacements performed,` |
|       - | 5641 | ` * so an array subject accumulates across its elements exactly like php's &$count.` |
|       - | 5642 | ` */` |
|   63504 | 5643 | `static sxi32 StrReplaceOneSubject(` |
|       - | 5644 | `	SyBlob *pOut,             /* Output buffer (reset then filled here) */` |
|       - | 5645 | `	const char *zSubject,     /* Subject bytes */` |
|       - | 5646 | `	sxu32 nSubject,           /* Subject length */` |
|       - | 5647 | `	SySet *pSearch,           /* Collected search terms */` |
|       - | 5648 | `	SySet *pReplace,          /* Collected replacement terms */` |
|       - | 5649 | `	int rep_str,              /* TRUE: a single replacement reused for every search */` |
|       - | 5650 | `	ProcStringMatch xMatch,   /* SyBlobSearch (str_replace) / iPatternMatch (str_ireplace) */` |
|       - | 5651 | `	sxi64 *pnCount            /* Running replacement count (incremented here) */` |
|       - | 5652 | `	)` |
|       5 | 5653 | `{` |
|       - | 5654 | `	SyString *pSearch_,*pReplace_,sEmpty;` |
|       - | 5655 | `	sxi32 rc;` |
|   63509 | 5656 | `	SyBlobReset(pOut);` |
|   63509 | 5657 | `	if( nSubject > 0 ){` |
|   46263 | 5658 | `		rc = SyBlobAppend(pOut,(const void *)zSubject,nSubject);` |
|   46263 | 5659 | `		if( rc != SXRET_OK ){` |
|     ! 0 | 5660 | `			return rc;` |
|       - | 5661 | `		}` |
|   23129 | 5662 | `	}` |
|   63509 | 5663 | `	SyStringInitFromBuf(&sEmpty,"",0);` |
|   63509 | 5664 | `	SySetResetCursor(pSearch);` |
|   63509 | 5665 | `	SySetResetCursor(pReplace);` |
|   63509 | 5666 | `	pSearch_ = pReplace_ = 0; /* cc warning */` |
|  129849 | 5667 | `	while( SXRET_OK == SySetGetNextEntry(pSearch,(void **)&pSearch_) ){` |
|       - | 5668 | `		sxu32 nCount,nOfft;` |
|   66345 | 5669 | `		if( rep_str ){` |
|       - | 5670 | `			/* Single replacement string reused for every search term */` |
|   63803 | 5671 | `			pReplace_ = (SyString *)SySetPeek(pReplace);` |
|   34445 | 5672 | `		}else if( SXRET_OK != SySetGetNextEntry(pReplace,(void **)&pReplace_) ){` |
|       - | 5673 | `			/* 'replace set' has fewer values than the search set: an empty` |
|       - | 5674 | `			 * string is used for the rest of the replacement values. */` |
|       5 | 5675 | `			pReplace_ = 0;` |
|       2 | 5676 | `		}` |
|   66345 | 5677 | `		if( pReplace_ == 0 ){` |
|       5 | 5678 | `			pReplace_ = &sEmpty;` |
|       2 | 5679 | `		}` |
|   66345 | 5680 | `		if( pSearch_->nByte < 1 ){` |
|       - | 5681 | `			/* php ignores an empty search string, but it still CONSUMED a replace` |
|       - | 5682 | `			 * slot above so the remaining pairs stay aligned. */` |
|      15 | 5683 | `			continue;` |
|       - | 5684 | `		}` |
|   66331 | 5685 | `		nOfft = nCount = 0;` |
|   37667 | 5686 | `		for(;;){` |
|   75339 | 5687 | `			if( nCount >= SyBlobLength(pOut) ){` |
|   17945 | 5688 | `				break;` |
|       - | 5689 | `			}` |
|       - | 5690 | `			/* Perform a pattern lookup */` |
|   86096 | 5691 | `			rc = xMatch(SyBlobDataAt(pOut,nCount),SyBlobLength(pOut) - nCount,` |
|   57394 | 5692 | `				(const void *)pSearch_->zString,pSearch_->nByte,&nOfft);` |
|   57399 | 5693 | `			if( rc != SXRET_OK ){` |
|       - | 5694 | `				/* Pattern not found */` |
|   48391 | 5695 | `				break;` |
|       - | 5696 | `			}` |
|       - | 5697 | `			/* Perform the replace operation */` |
|   13517 | 5698 | `			rc = StringReplace(pOut,nCount+nOfft,(int)pSearch_->nByte,` |
|    9008 | 5699 | `				pReplace_->zString,(int)pReplace_->nByte);` |
|    9013 | 5700 | `			if( rc != SXRET_OK ){` |
|       - | 5701 | `				/* Propagate an allocation failure so the caller raises a fatal` |
|       - | 5702 | `				 * instead of returning a partially-replaced result. */` |
|     ! 0 | 5703 | `				return rc;` |
|       - | 5704 | `			}` |
|    9013 | 5705 | `			*pnCount += 1;` |
|       - | 5706 | `			/* Increment offset counter */` |
|    9013 | 5707 | `			nCount += nOfft + pReplace_->nByte;` |
|       5 | 5708 | `		}` |
|       5 | 5709 | `	}` |
|   63509 | 5710 | `	return SXRET_OK;` |
|   31757 | 5711 | `}` |
|       - | 5712 | `/* Per-call state for the array-subject form of str_replace()/str_ireplace(). */` |
|       - | 5713 | `typedef struct str_replace_subject str_replace_subject;` |
|       - | 5714 | `struct str_replace_subject` |
|       - | 5715 | `{` |
|       - | 5716 | `	ph7_value *pResult;    /* Result array (keys preserved) */` |
|       - | 5717 | `	ph7_value *pScratch;   /* Reusable string value for each element */` |
|       - | 5718 | `	SyBlob *pWorker;       /* Scratch output buffer for one element */` |
|       - | 5719 | `	SySet *pSearch;        /* Collected search terms */` |
|       - | 5720 | `	SySet *pReplace;       /* Collected replacement terms */` |
|       - | 5721 | `	ProcStringMatch xMatch;/* Match routine (case-sensitive or not) */` |
|       - | 5722 | `	int rep_str;           /* TRUE: scalar $replace */` |
|       - | 5723 | `	sxi64 nReplaced;       /* Replacements performed so far (&$count) */` |
|       - | 5724 | `	sxi32 rc;              /* SXRET_OK or SXERR_MEM */` |
|       - | 5725 | `};` |
|       - | 5726 | `/*` |
|       - | 5727 | ` * ph7_array_walk() callback over an array $subject: string-cast one element, run` |
|       - | 5728 | ` * the search/replace over it, and insert the result under the element's original` |
|       - | 5729 | ` * key. A non-string element is coerced exactly like php (int/float/bool/null via` |
|       - | 5730 | ` * their string form). A nested-array element becomes "Array" — the value matches` |
|       - | 5731 | ` * php, but PHL does not emit php's "Array to string conversion" warning here (the` |
|       - | 5732 | ` * engine raises it at echo/interpolation sites, not this C-level cast; a` |
|       - | 5733 | ` * recorded divergence).` |
|       - | 5734 | ` */` |
|      34 | 5735 | `static int StrReplaceSubjectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|       1 | 5736 | `{` |
|      35 | 5737 | `	str_replace_subject *pS = (str_replace_subject *)pUserData;` |
|       - | 5738 | `	const char *zSub;` |
|       - | 5739 | `	int nSub;` |
|       - | 5740 | `	/* php coerces every element to string (same cast used everywhere). */` |
|      35 | 5741 | `	zSub = ph7_value_to_string(pData,&nSub);` |
|      34 | 5742 | `	if( StrReplaceOneSubject(pS->pWorker,zSub,(sxu32)(nSub > 0 ? nSub : 0),` |
|      35 | 5743 | `			pS->pSearch,pS->pReplace,pS->rep_str,pS->xMatch,&pS->nReplaced) != SXRET_OK ){` |
|     ! 0 | 5744 | `		pS->rc = SXERR_MEM;` |
|     ! 0 | 5745 | `		return SXERR_ABORT;` |
|       - | 5746 | `	}` |
|       - | 5747 | `	/* Publish the transformed bytes as a string under the original key. */` |
|      35 | 5748 | `	ph7_value_reset_string_cursor(pS->pScratch);` |
|      34 | 5749 | `	if( SyBlobLength(pS->pWorker) > 0` |
|      33 | 5750 | `	 && ph7_value_string(pS->pScratch,(const char *)SyBlobData(pS->pWorker),` |
|      45 | 5751 | `			(int)SyBlobLength(pS->pWorker)) != SXRET_OK ){` |
|     ! 0 | 5752 | `		pS->rc = SXERR_MEM;` |
|     ! 0 | 5753 | `		return SXERR_ABORT;` |
|       - | 5754 | `	}` |
|      35 | 5755 | `	if( ph7_array_add_elem(pS->pResult,pKey,pS->pScratch) != SXRET_OK ){` |
|     ! 0 | 5756 | `		pS->rc = SXERR_MEM;` |
|     ! 0 | 5757 | `		return SXERR_ABORT;` |
|       - | 5758 | `	}` |
|      35 | 5759 | `	return PH7_OK;` |
|      18 | 5760 | `}` |
|       - | 5761 | `/*` |
|       - | 5762 | ` * Write str_replace()/str_ireplace()'s optional by-reference &$count out-param.` |
|       - | 5763 | ` * The call compiler auto-vivifies argument #4 for these two names` |
|       - | 5764 | ` * (GenStateByRefBuiltinMask in compile.c), so an undefined variable, an array` |
|       - | 5765 | ` * element and a property all arrive with a real slot to write through.` |
|       - | 5766 | ` */` |
|   63482 | 5767 | `static void StrReplaceStoreCount(ph7_context *pCtx,int nArg,ph7_value **apArg,sxi64 nReplaced)` |
|       5 | 5768 | `{` |
|       - | 5769 | `	ph7_value sCount;` |
|   63487 | 5770 | `	if( nArg < 4 ){` |
|   63463 | 5771 | `		return;` |
|       - | 5772 | `	}` |
|      25 | 5773 | `	PH7_MemObjInitFromInt(pCtx->pVm,&sCount,nReplaced);` |
|      25 | 5774 | `	PH7_VmStoreArgByRef(pCtx->pVm,apArg[3],&sCount);` |
|      25 | 5775 | `	PH7_MemObjRelease(&sCount);` |
|   31746 | 5776 | `}` |
|       - | 5777 | `/*` |
|       - | 5778 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|       - | 5779 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|       - | 5780 | ` *  Replace all occurrences of the search string with the replacement string.` |
|       - | 5781 | ` * Parameters` |
|       - | 5782 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|       - | 5783 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|       - | 5784 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|       - | 5785 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|       - | 5786 | ` *  for every value of search. The converse would not make sense, though.` |
|       - | 5787 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|       - | 5788 | ` * $search` |
|       - | 5789 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|       - | 5790 | ` *  to designate multiple needles.` |
|       - | 5791 | ` * $replace` |
|       - | 5792 | ` *  The replacement value that replaces found search values. An array may be used` |
|       - | 5793 | ` *  to designate multiple replacements.` |
|       - | 5794 | ` * $subject` |
|       - | 5795 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|       - | 5796 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|       - | 5797 | ` *  of subject, and the return value is an array as well.` |
|       - | 5798 | ` * &$count` |
|       - | 5799 | ` *  If passed, this is set to the number of replacements performed — accumulated` |
|       - | 5800 | ` *  over every search term AND, for an array subject, over every element.` |
|       - | 5801 | ` * Return` |
|       - | 5802 | ` * This function returns a string or an array with the replaced values.` |
|       - | 5803 | ` */` |
|   63482 | 5804 | `PH7_PRIVATE int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       5 | 5805 | `{` |
|       - | 5806 | `	SyString sTemp;` |
|       - | 5807 | `	ProcStringMatch xMatch;` |
|       - | 5808 | `	const char *zIn,*zFunc;` |
|       - | 5809 | `	str_replace_data sRep;` |
|       - | 5810 | `	SyBlob sWorker;` |
|       - | 5811 | `	SySet sReplace;` |
|       - | 5812 | `	SySet sSearch;` |
|       - | 5813 | `	sxi64 nReplaced;` |
|       - | 5814 | `	int rep_str;` |
|       - | 5815 | `	int nByte;` |
|       - | 5816 | `	sxi32 rc;` |
|   63487 | 5817 | `	if( nArg < 3 ){` |
|       - | 5818 | `		/* Missing/Invalid arguments,return null */` |
|     ! 0 | 5819 | `		ph7_result_null(pCtx);` |
|     ! 0 | 5820 | `		return PH7_OK;` |
|       - | 5821 | `	}` |
|       - | 5822 | `	/* Initialize fields */` |
|   63487 | 5823 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|   63487 | 5824 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|   63487 | 5825 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|   63487 | 5826 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|   63487 | 5827 | `	sRep.pCtx = pCtx;` |
|   63487 | 5828 | `	sRep.pCollector = &sSearch;` |
|   63487 | 5829 | `	rep_str = 0;` |
|   63487 | 5830 | `	nReplaced = 0;` |
|       - | 5831 | `	/* Collect the search term(s) — independent of the subject. */` |
|   63487 | 5832 | `	if( ph7_value_is_array(apArg[0]) ){` |
|    1905 | 5833 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|     955 | 5834 | `	}else{` |
|   61587 | 5835 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|   61587 | 5836 | `		SyStringInitFromBuf(&sTemp,zIn,nByte > 0 ? nByte : 0);` |
|   61587 | 5837 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|       - | 5838 | `	}` |
|       - | 5839 | `	/* Collect the replacement term(s). */` |
|   63487 | 5840 | `	if( ph7_value_is_array(apArg[1]) ){` |
|    1104 | 5841 | `		sRep.pCollector = &sReplace;` |
|    1104 | 5842 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|     554 | 5843 | `	}else{` |
|   62387 | 5844 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|   62387 | 5845 | `		rep_str = 1;` |
|   62387 | 5846 | `		SyStringInitFromBuf(&sTemp,zIn,nByte > 0 ? nByte : 0);` |
|   62387 | 5847 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|       - | 5848 | `	}` |
|       - | 5849 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|   63487 | 5850 | `	if( sRep.rc != SXRET_OK ){` |
|     ! 0 | 5851 | `		SySetRelease(&sSearch);` |
|     ! 0 | 5852 | `		SySetRelease(&sReplace);` |
|     ! 0 | 5853 | `		SyBlobRelease(&sWorker);` |
|     ! 0 | 5854 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 5855 | `	}` |
|       - | 5856 | `	/* Pick the match routine by function name */` |
|   63487 | 5857 | `	zFunc = ph7_function_name(pCtx);` |
|   63487 | 5858 | `	xMatch = SyBlobSearch;` |
|   63487 | 5859 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|       - | 5860 | `		/* Case insensitive pattern match */` |
|      53 | 5861 | `		xMatch = iPatternMatch;` |
|      26 | 5862 | `	}` |
|   63487 | 5863 | `	if( ph7_value_is_array(apArg[2]) ){` |
|       - | 5864 | `		/* Array subject: replace element-wise and RETURN AN ARRAY whose keys` |
|       - | 5865 | `		 * mirror the subject's (php semantics). */` |
|       - | 5866 | `		str_replace_subject sSub;` |
|       - | 5867 | `		ph7_value *pResult,*pScratch;` |
|      13 | 5868 | `		pResult = ph7_context_new_array(pCtx);` |
|      13 | 5869 | `		pScratch = ph7_context_new_scalar(pCtx);` |
|      13 | 5870 | `		if( pResult == 0 \|\| pScratch == 0 ){` |
|     ! 0 | 5871 | `			SySetRelease(&sSearch);` |
|     ! 0 | 5872 | `			SySetRelease(&sReplace);` |
|     ! 0 | 5873 | `			SyBlobRelease(&sWorker);` |
|     ! 0 | 5874 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 5875 | `		}` |
|      13 | 5876 | `		ph7_value_string(pScratch,"",0); /* force string representation */` |
|      13 | 5877 | `		SyZero(&sSub,sizeof(sSub));` |
|      13 | 5878 | `		sSub.pResult  = pResult;` |
|      13 | 5879 | `		sSub.pScratch = pScratch;` |
|      13 | 5880 | `		sSub.pWorker  = &sWorker;` |
|      13 | 5881 | `		sSub.pSearch  = &sSearch;` |
|      13 | 5882 | `		sSub.pReplace = &sReplace;` |
|      13 | 5883 | `		sSub.xMatch   = xMatch;` |
|      13 | 5884 | `		sSub.rep_str  = rep_str;` |
|      13 | 5885 | `		ph7_array_walk(apArg[2],StrReplaceSubjectWalker,&sSub);` |
|      13 | 5886 | `		SySetRelease(&sSearch);` |
|      13 | 5887 | `		SySetRelease(&sReplace);` |
|      13 | 5888 | `		SyBlobRelease(&sWorker);` |
|      13 | 5889 | `		if( sSub.rc != SXRET_OK ){` |
|     ! 0 | 5890 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 5891 | `		}` |
|      13 | 5892 | `		ph7_result_value(pCtx,pResult);` |
|      13 | 5893 | `		StrReplaceStoreCount(pCtx,nArg,apArg,sSub.nReplaced);` |
|      13 | 5894 | `		return PH7_OK;` |
|       - | 5895 | `	}` |
|       - | 5896 | `	/* Scalar subject: run once and return a string. An empty subject yields the` |
|       - | 5897 | `	 * empty string, and a lone empty search term leaves the subject untouched —` |
|       - | 5898 | `	 * both fall out of StrReplaceOneSubject's empty-term skip. */` |
|   63475 | 5899 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|   63475 | 5900 | `	rc = StrReplaceOneSubject(&sWorker,zIn,(sxu32)(nByte > 0 ? nByte : 0),` |
|   31735 | 5901 | `		&sSearch,&sReplace,rep_str,xMatch,&nReplaced);` |
|   63475 | 5902 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 5903 | `		SySetRelease(&sSearch);` |
|     ! 0 | 5904 | `		SySetRelease(&sReplace);` |
|     ! 0 | 5905 | `		SyBlobRelease(&sWorker);` |
|     ! 0 | 5906 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 5907 | `	}` |
|   63475 | 5908 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|   63475 | 5909 | `	SySetRelease(&sSearch);` |
|   63475 | 5910 | `	SySetRelease(&sReplace);` |
|   63475 | 5911 | `	SyBlobRelease(&sWorker);` |
|   63475 | 5912 | `	if( rc != PH7_OK ){` |
|     ! 0 | 5913 | `		return PH7_ContextMemoryError(pCtx);` |
|       - | 5914 | `	}` |
|   63475 | 5915 | `	StrReplaceStoreCount(pCtx,nArg,apArg,nReplaced);` |
|   63475 | 5916 | `	return PH7_OK;` |
|   31746 | 5917 | `}` |
|       - | 5918 | `/*` |
|       - | 5919 | ` * strtr() array form: a single (key,value) pair copied out of the replace_pairs` |
|       - | 5920 | ` * array. The bytes are owned by a persistent pool (see strtr_collect) rather than` |
|       - | 5921 | ` * the transient walker values, which HashmapWalk releases after each callback, so` |
|       - | 5922 | ` * we store byte offsets into that pool instead of raw pointers.` |
|       - | 5923 | ` */` |
|       - | 5924 | `typedef struct strtr_entry strtr_entry;` |
|       - | 5925 | `struct strtr_entry` |
|       - | 5926 | `{` |
|       - | 5927 | `	sxu32 nKeyOfft; /* Offset of the search key inside the pool */` |
|       - | 5928 | `	sxu32 nKeyLen;  /* Length of the search key */` |
|       - | 5929 | `	sxu32 nValOfft; /* Offset of the replacement inside the pool */` |
|       - | 5930 | `	sxu32 nValLen;  /* Length of the replacement */` |
|       - | 5931 | `};` |
|       - | 5932 | `typedef struct strtr_collect strtr_collect;` |
|       - | 5933 | `struct strtr_collect` |
|       - | 5934 | `{` |
|       - | 5935 | `	SyBlob *pPool;  /* Byte pool holding copied key + value bytes */` |
|       - | 5936 | `	SySet  *pTable; /* Set of strtr_entry (parallel offsets into pPool) */` |
|       - | 5937 | `	sxi32   rc;     /* Carries an allocation failure (SXERR_MEM) out of the walker */` |
|       - | 5938 | `	ph7_context *pCtx; /* Needed to warn about an empty key */` |
|       - | 5939 | `};` |
|       - | 5940 | `/*` |
|       - | 5941 | ` * Collect one replace_pairs entry into the persistent pool/offset table.` |
|       - | 5942 | ` * PHP coerces both the key and the value to string (an integer key becomes its` |
|       - | 5943 | ` * decimal form) and ignores an empty-string key.` |
|       - | 5944 | ` */` |
|      22 | 5945 | `static int StrtrCollectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|       1 | 5946 | `{` |
|      23 | 5947 | `	strtr_collect *pCol = (strtr_collect *)pUserData;` |
|       - | 5948 | `	const char *zKey,*zVal;` |
|       - | 5949 | `	strtr_entry sEnt;` |
|       - | 5950 | `	int nKey,nVal;` |
|      23 | 5951 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|      23 | 5952 | `	if( nKey < 1 ){` |
|       - | 5953 | `		/* PHP ignores an empty-string key, and warns that it did so. */` |
|       3 | 5954 | `		ph7_context_throw_error_format(pCol->pCtx,PH7_CTX_WARNING,` |
|       - | 5955 | `			"Ignoring replacement of empty string");` |
|       3 | 5956 | `		return PH7_OK;` |
|       - | 5957 | `	}` |
|      21 | 5958 | `	zVal = ph7_value_to_string(pData,&nVal);` |
|      21 | 5959 | `	sEnt.nKeyOfft = SyBlobLength(pCol->pPool);` |
|      21 | 5960 | `	sEnt.nKeyLen  = (sxu32)nKey;` |
|      21 | 5961 | `	if( SyBlobAppend(pCol->pPool,(const void *)zKey,(sxu32)nKey) != SXRET_OK ){` |
|     ! 0 | 5962 | `		pCol->rc = SXERR_MEM;` |
|     ! 0 | 5963 | `		return SXERR_ABORT;` |
|       - | 5964 | `	}` |
|      21 | 5965 | `	sEnt.nValOfft = SyBlobLength(pCol->pPool);` |
|      21 | 5966 | `	sEnt.nValLen  = (sxu32)nVal;` |
|      21 | 5967 | `	if( nVal > 0 && SyBlobAppend(pCol->pPool,(const void *)zVal,(sxu32)nVal) != SXRET_OK ){` |
|     ! 0 | 5968 | `		pCol->rc = SXERR_MEM;` |
|     ! 0 | 5969 | `		return SXERR_ABORT;` |
|       - | 5970 | `	}` |
|      21 | 5971 | `	if( SySetPut(pCol->pTable,(const void *)&sEnt) != SXRET_OK ){` |
|     ! 0 | 5972 | `		pCol->rc = SXERR_MEM;` |
|     ! 0 | 5973 | `		return SXERR_ABORT;` |
|       - | 5974 | `	}` |
|      21 | 5975 | `	return PH7_OK;` |
|      12 | 5976 | `}` |
|       - | 5977 | `/*` |
|       - | 5978 | ` * string strtr(string $str,string $from,string $to)` |
|       - | 5979 | ` * string strtr(string $str,array $replace_pairs)` |
|       - | 5980 | ` *  Translate characters or replace substrings.` |
|       - | 5981 | ` * Parameters` |
|       - | 5982 | ` *  $str` |
|       - | 5983 | ` *  The string being translated.` |
|       - | 5984 | ` * $from` |
|       - | 5985 | ` *  The string being translated to to.` |
|       - | 5986 | ` * $to` |
|       - | 5987 | ` *  The string replacing from.` |
|       - | 5988 | ` * $replace_pairs` |
|       - | 5989 | ` *  The replace_pairs parameter may be used instead of to and` |
|       - | 5990 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|       - | 5991 | ` * Return` |
|       - | 5992 | ` *  The translated string.` |
|       - | 5993 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|       - | 5994 | ` */` |
|     102 | 5995 | `PH7_PRIVATE int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|       1 | 5996 | `{` |
|       - | 5997 | `	const char *zIn;` |
|       - | 5998 | `	char zGiven[64];` |
|       - | 5999 | `	int nLen;` |
|     103 | 6000 | `	if( nArg < 1 ){` |
|       - | 6001 | `		/* Nothing to replace,return FALSE */` |
|     ! 0 | 6002 | `		ph7_result_bool(pCtx,0);` |
|     ! 0 | 6003 | `		return PH7_OK;` |
|       - | 6004 | `	}` |
|       - | 6005 | `	/*` |
|       - | 6006 | `	 * php dispatches strtr() on ARITY between two overloads — strtr(string, array)` |
|       - | 6007 | ``	 * and strtr(string, string, string) — so $from's expected type is `array` with`` |
|       - | 6008 | ``	 * two arguments and `string` with three, and the stub's `array\|string` union is`` |
|       - | 6009 | `	 * a wording php itself never emits. One signature cannot express that, so the` |
|       - | 6010 | `	 * shared ZPP screen skips this builtin (azSelfChecked[] in vm_arg_check.c) and` |
|       - | 6011 | `	 * the dispatch happens here, in php's left-to-right argument order.` |
|       - | 6012 | `	 *` |
|       - | 6013 | `	 * Both directions used to pass silently: a 2-argument string $from` |
|       - | 6014 | `	 * (strtr("abc","ab")) returned the subject UNCHANGED, and a 3-argument array` |
|       - | 6015 | `	 * $from was likewise ignored — the caller got its input back as if it had been` |
|       - | 6016 | `	 * translated.` |
|       - | 6017 | `	 */` |
|     103 | 6018 | `	if( !PH7_ArgSatisfiesString(apArg[0]) ){` |
|       4 | 6019 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 6020 | `			"strtr(): Argument #1 ($string) must be of type string, %s given",` |
|       1 | 6021 | `			VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|       - | 6022 | `	}` |
|     101 | 6023 | `	if( nArg == 2 ){` |
|      31 | 6024 | `		if( !ph7_value_is_array(apArg[1]) ){` |
|      22 | 6025 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 6026 | `				"strtr(): Argument #2 ($from) must be of type array, %s given",` |
|      14 | 6027 | `				VmValueGivenName(apArg[1],zGiven,sizeof(zGiven)));` |
|       1 | 6028 | `		}` |
|      79 | 6029 | `	}else if( nArg > 2 ){` |
|      71 | 6030 | `		if( !PH7_ArgSatisfiesString(apArg[1]) ){` |
|      10 | 6031 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 6032 | `				"strtr(): Argument #2 ($from) must be of type string, %s given",` |
|       6 | 6033 | `				VmValueGivenName(apArg[1],zGiven,sizeof(zGiven)));` |
|       - | 6034 | `		}` |
|       - | 6035 | ``		/* $to is php's `string`, but a null one stays accepted (php coerces it to`` |
|       - | 6036 | `		 * "" with a deprecation, and both engines answer the subject unchanged). */` |
|      65 | 6037 | `		if( !ph7_value_is_null(apArg[2]) && !PH7_ArgSatisfiesString(apArg[2]) ){` |
|       4 | 6038 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|       - | 6039 | `				"strtr(): Argument #3 ($to) must be of type string, %s given",` |
|       2 | 6040 | `				VmValueGivenName(apArg[2],zGiven,sizeof(zGiven)));` |
|       - | 6041 | `		}` |
|      31 | 6042 | `	}` |
|      79 | 6043 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      79 | 6044 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|       - | 6045 | `		/* Invalid arguments */` |
|       5 | 6046 | `		ph7_result_string(pCtx,zIn,nLen);` |
|       5 | 6047 | `		return PH7_OK;` |
|       - | 6048 | `	}` |
|      82 | 6049 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|       - | 6050 | `		strtr_collect sCol;` |
|       - | 6051 | `		SyBlob sPool,sWorker;` |
|       - | 6052 | `		SySet sTable;` |
|       - | 6053 | `		const char *zPool;` |
|       - | 6054 | `		strtr_entry *pEnt;` |
|       - | 6055 | `		sxi32 rc;` |
|       - | 6056 | `		int i,iRun;` |
|       - | 6057 | `		/*` |
|       - | 6058 | `		 * PHP's array-form strtr is a single left-to-right pass over the subject:` |
|       - | 6059 | `		 * at every position it substitutes the LONGEST replace_pairs key that` |
|       - | 6060 | `		 * matches there, then advances past the key (replacements are never` |
|       - | 6061 | `		 * rescanned). It is not a sequential per-key global replace. First copy` |
|       - | 6062 | `		 * the pairs into a persistent pool, then run that scan.` |
|       - | 6063 | `		 */` |
|      15 | 6064 | `		SyBlobInit(&sPool,&pCtx->pVm->sAllocator);` |
|      15 | 6065 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|      15 | 6066 | `		SySetInit(&sTable,&pCtx->pVm->sAllocator,sizeof(strtr_entry));` |
|      15 | 6067 | `		sCol.pPool  = &sPool;` |
|      15 | 6068 | `		sCol.pTable = &sTable;` |
|      15 | 6069 | `		sCol.rc     = SXRET_OK;` |
|      15 | 6070 | `		sCol.pCtx   = pCtx;` |
|      15 | 6071 | `		ph7_array_walk(apArg[1],StrtrCollectWalker,&sCol);` |
|      15 | 6072 | `		if( sCol.rc != SXRET_OK ){` |
|       - | 6073 | `			/* Allocation failure while collecting the pairs: surface a fatal */` |
|     ! 0 | 6074 | `			SyBlobRelease(&sPool);` |
|     ! 0 | 6075 | `			SyBlobRelease(&sWorker);` |
|     ! 0 | 6076 | `			SySetRelease(&sTable);` |
|     ! 0 | 6077 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 6078 | `		}` |
|       - | 6079 | `		/* The pool is now stable, so offsets can be resolved against its base. */` |
|      15 | 6080 | `		zPool = (const char *)SyBlobData(&sPool);` |
|      15 | 6081 | `		rc = SXRET_OK;` |
|      15 | 6082 | `		iRun = 0; /* Start of the pending run of unmatched bytes copied verbatim. */` |
|      59 | 6083 | `		for( i = 0 ; i < nLen ; ){` |
|      45 | 6084 | `			strtr_entry *pBest = 0;` |
|      45 | 6085 | `			sxu32 nBest = 0;` |
|       - | 6086 | `			/* Pick the longest key that matches at the current position. */` |
|      45 | 6087 | `			SySetResetCursor(&sTable);` |
|     105 | 6088 | `			while( SXRET_OK == SySetGetNextEntry(&sTable,(void **)&pEnt) ){` |
|      60 | 6089 | `				if( pEnt->nKeyLen > nBest` |
|      56 | 6090 | `					&& pEnt->nKeyLen <= (sxu32)(nLen - i)` |
|      52 | 6091 | `					&& SyMemcmp(zPool + pEnt->nKeyOfft,zIn + i,pEnt->nKeyLen) == 0 ){` |
|      31 | 6092 | `					nBest = pEnt->nKeyLen;` |
|      31 | 6093 | `					pBest = pEnt;` |
|      15 | 6094 | `				}` |
|       1 | 6095 | `			}` |
|      45 | 6096 | `			if( pBest == 0 ){` |
|       - | 6097 | `				/* No key here: extend the literal run and copy it in one shot later. */` |
|      19 | 6098 | `				i++;` |
|      19 | 6099 | `				continue;` |
|       - | 6100 | `			}` |
|       - | 6101 | `			/* Flush the pending literal run, then the replacement. */` |
|      27 | 6102 | `			if( i > iRun ){` |
|       5 | 6103 | `				rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(i - iRun));` |
|       2 | 6104 | `			}` |
|      27 | 6105 | `			if( rc == SXRET_OK && pBest->nValLen > 0 ){` |
|      27 | 6106 | `				rc = SyBlobAppend(&sWorker,zPool + pBest->nValOfft,pBest->nValLen);` |
|      13 | 6107 | `			}` |
|      27 | 6108 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6109 | `				SyBlobRelease(&sPool);` |
|     ! 0 | 6110 | `				SyBlobRelease(&sWorker);` |
|     ! 0 | 6111 | `				SySetRelease(&sTable);` |
|     ! 0 | 6112 | `				return PH7_ContextMemoryError(pCtx);` |
|       - | 6113 | `			}` |
|      27 | 6114 | `			i += (int)pBest->nKeyLen;` |
|      27 | 6115 | `			iRun = i;` |
|       1 | 6116 | `		}` |
|       - | 6117 | `		/* Flush the trailing literal run. */` |
|      15 | 6118 | `		if( nLen > iRun ){` |
|       7 | 6119 | `			rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(nLen - iRun));` |
|       7 | 6120 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 6121 | `				SyBlobRelease(&sPool);` |
|     ! 0 | 6122 | `				SyBlobRelease(&sWorker);` |
|     ! 0 | 6123 | `				SySetRelease(&sTable);` |
|     ! 0 | 6124 | `				return PH7_ContextMemoryError(pCtx);` |
|       - | 6125 | `			}` |
|       3 | 6126 | `		}` |
|       - | 6127 | `		/* All done, return the result string */` |
|      22 | 6128 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|      14 | 6129 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|       - | 6130 | `		/* Clean-up */` |
|      15 | 6131 | `		SyBlobRelease(&sPool);` |
|      15 | 6132 | `		SyBlobRelease(&sWorker);` |
|      15 | 6133 | `		SySetRelease(&sTable);` |
|      15 | 6134 | `		if( rc != PH7_OK ){` |
|     ! 0 | 6135 | `			return PH7_ContextMemoryError(pCtx);` |
|       - | 6136 | `		}` |
|       8 | 6137 | `	}else{` |
|       - | 6138 | `		int i,flen,tlen,c,iOfft;` |
|       - | 6139 | `		const char *zFrom,*zTo;` |
|      61 | 6140 | `		if( nArg < 3 ){` |
|       - | 6141 | `			/* Nothing to replace */` |
|     ! 0 | 6142 | `			ph7_result_string(pCtx,zIn,nLen);` |
|     ! 0 | 6143 | `			return PH7_OK;` |
|       - | 6144 | `		}` |
|       - | 6145 | `		/* Extract given arguments */` |
|      61 | 6146 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      61 | 6147 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      61 | 6148 | `		if( flen < 1 \|\| tlen < 1 ){` |
|       - | 6149 | `			/* Nothing to replace */` |
|     ! 0 | 6150 | `			ph7_result_string(pCtx,zIn,nLen);` |
|     ! 0 | 6151 | `			return PH7_OK;` |
|       - | 6152 | `		}` |
|       - | 6153 | `		/* Start the replace process */` |
|     203 | 6154 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     143 | 6155 | `			c = zIn[i];` |
|     143 | 6156 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      19 | 6157 | `				if ( iOfft < tlen ){` |
|      19 | 6158 | `					c = zTo[iOfft];` |
|       9 | 6159 | `				}` |
|       9 | 6160 | `			}` |
|     143 | 6161 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|       - | 6162 |  |
|      72 | 6163 | `		}` |
|       - | 6164 | `	}` |
|      75 | 6165 | `	return PH7_OK;` |
|      52 | 6166 | `}` |
|       - | 6167 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|       - | 6168 |  |
