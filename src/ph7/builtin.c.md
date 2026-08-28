# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4283/4960 lines (86.35%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `/* filter_var(FILTER_VALIDATE_FLOAT) parses with libc strtod directly because it` |
|      - |    8 | ` * needs errno==ERANGE to reject out-of-range magnitudes; SyStrToReal (also` |
|      - |    9 | ` * strtod-backed nowadays) exposes no range-error signal. */` |
|      - |   10 | `#include <stdlib.h>  /* strtod */` |
|      - |   11 | `#include <math.h>    /* HUGE_VAL */` |
|      - |   12 | `#include <errno.h>   /* ERANGE (strtod range-error signal) */` |
|      - |   13 | `#include <stdio.h>   /* snprintf (printf-family float conversions — correctly` |
|      - |   14 | `                      * rounded digits like php's zend_dtoa; see PH7_InputFormat) */` |
|      - |   15 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |   16 | `/* Forward decl: null-to-string ZPP deprecation notice (defined near the ZPP` |
|      - |   17 | ` * helpers; both live inside the same DISABLE_BUILTIN_FUNC region as every` |
|      - |   18 | ` * caller — the tiny build compiles none of them). */` |
|      - |   19 | `static void StrNullArgNotice(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNum,const char *zParamName);` |
|      - |   20 | `#endif /* PH7_DISABLE_BUILTIN_FUNC */` |
|      - |   21 | `/* This file implement built-in 'foreign' functions for the PH7 engine */` |
|      - |   22 | `/*` |
|      - |   23 | ` * Section:` |
|      - |   24 | ` *    Variable handling Functions.` |
|      - |   25 | ` * Status:` |
|      - |   26 | ` *    Stable.` |
|      - |   27 | ` */` |
|      - |   28 | `/*` |
|      - |   29 | ` * bool is_bool($var)` |
|      - |   30 | ` *  Finds out whether a variable is a boolean.` |
|      - |   31 | ` * Parameters` |
|      - |   32 | ` *   $var: The variable being evaluated.` |
|      - |   33 | ` * Return` |
|      - |   34 | ` *  TRUE if var is a boolean. False otherwise.` |
|      - |   35 | ` */` |
|     50 |   36 | `static int PH7_builtin_is_bool(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   37 | `{` |
|     51 |   38 | `	int res = 0; /* Assume false by default */` |
|     51 |   39 | `	if( nArg > 0 ){` |
|     51 |   40 | `		res = ph7_value_is_bool(apArg[0]);` |
|     25 |   41 | `	}` |
|      - |   42 | `	/* Query result */` |
|     51 |   43 | `	ph7_result_bool(pCtx,res);` |
|     51 |   44 | `	return PH7_OK;` |
|      1 |   45 | `}` |
|      - |   46 | `/*` |
|      - |   47 | ` * bool is_float($var)` |
|      - |   48 | ` * bool is_real($var)` |
|      - |   49 | ` * bool is_double($var)` |
|      - |   50 | ` *  Finds out whether a variable is a float.` |
|      - |   51 | ` * Parameters` |
|      - |   52 | ` *   $var: The variable being evaluated.` |
|      - |   53 | ` * Return` |
|      - |   54 | ` *  TRUE if var is a float. False otherwise.` |
|      - |   55 | ` */` |
|    302 |   56 | `static int PH7_builtin_is_float(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   57 | `{` |
|    303 |   58 | `	int res = 0; /* Assume false by default */` |
|    303 |   59 | `	if( nArg > 0 ){` |
|    303 |   60 | `		res = ph7_value_is_float(apArg[0]);` |
|    151 |   61 | `	}` |
|      - |   62 | `	/* Query result */` |
|    303 |   63 | `	ph7_result_bool(pCtx,res);` |
|    303 |   64 | `	return PH7_OK;` |
|      1 |   65 | `}` |
|      - |   66 | `/*` |
|      - |   67 | ` * bool is_int($var)` |
|      - |   68 | ` * bool is_integer($var)` |
|      - |   69 | ` * bool is_long($var)` |
|      - |   70 | ` *  Finds out whether a variable is an integer.` |
|      - |   71 | ` * Parameters` |
|      - |   72 | ` *   $var: The variable being evaluated.` |
|      - |   73 | ` * Return` |
|      - |   74 | ` *  TRUE if var is an integer. False otherwise.` |
|      - |   75 | ` */` |
|    804 |   76 | `static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |   77 | `{` |
|    807 |   78 | `	int res = 0; /* Assume false by default */` |
|    807 |   79 | `	if( nArg > 0 ){` |
|      - |   80 | `		/* Strict PHP identity: a float is never an int, even when it holds an` |
|      - |   81 | `		 * integer value (1.0). An integer-valued real carries both MEMOBJ_INT` |
|      - |   82 | `		 * (cached) and MEMOBJ_REAL, so REAL must be excluded here. */` |
|    807 |   83 | `		res = ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]);` |
|    402 |   84 | `	}` |
|      - |   85 | `	/* Query result */` |
|    807 |   86 | `	ph7_result_bool(pCtx,res);` |
|    807 |   87 | `	return PH7_OK;` |
|      3 |   88 | `}` |
|      - |   89 | `/*` |
|      - |   90 | ` * bool is_string($var)` |
|      - |   91 | ` *  Finds out whether a variable is a string.` |
|      - |   92 | ` * Parameters` |
|      - |   93 | ` *   $var: The variable being evaluated.` |
|      - |   94 | ` * Return` |
|      - |   95 | ` *  TRUE if var is string. False otherwise.` |
|      - |   96 | ` */` |
|    746 |   97 | `static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |   98 | `{` |
|    748 |   99 | `	int res = 0; /* Assume false by default */` |
|    748 |  100 | `	if( nArg > 0 ){` |
|    748 |  101 | `		res = ph7_value_is_string(apArg[0]);` |
|    373 |  102 | `	}` |
|      - |  103 | `	/* Query result */` |
|    748 |  104 | `	ph7_result_bool(pCtx,res);` |
|    748 |  105 | `	return PH7_OK;` |
|      2 |  106 | `}` |
|      - |  107 | `/*` |
|      - |  108 | ` * bool is_null($var)` |
|      - |  109 | ` *  Finds out whether a variable is NULL.` |
|      - |  110 | ` * Parameters` |
|      - |  111 | ` *   $var: The variable being evaluated.` |
|      - |  112 | ` * Return` |
|      - |  113 | ` *  TRUE if var is NULL. False otherwise.` |
|      - |  114 | ` */` |
|     86 |  115 | `static int PH7_builtin_is_null(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  116 | `{` |
|     89 |  117 | `	int res = 0; /* Assume false by default */` |
|     89 |  118 | `	if( nArg > 0 ){` |
|     89 |  119 | `		res = ph7_value_is_null(apArg[0]);` |
|     43 |  120 | `	}` |
|      - |  121 | `	/* Query result */` |
|     89 |  122 | `	ph7_result_bool(pCtx,res);` |
|     89 |  123 | `	return PH7_OK;` |
|      3 |  124 | `}` |
|      - |  125 | `/*` |
|      - |  126 | ` * bool is_numeric($var)` |
|      - |  127 | ` *  Find out whether a variable is NULL.` |
|      - |  128 | ` * Parameters` |
|      - |  129 | ` *  $var: The variable being evaluated.` |
|      - |  130 | ` * Return` |
|      - |  131 | ` *  True if var is numeric. False otherwise.` |
|      - |  132 | ` */` |
|     60 |  133 | `static int PH7_builtin_is_numeric(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  134 | `{` |
|     65 |  135 | `	int res = 0; /* Assume false by default */` |
|     65 |  136 | `	if( nArg > 0 ){` |
|     65 |  137 | `		res = ph7_value_is_numeric(apArg[0]);` |
|     30 |  138 | `	}` |
|      - |  139 | `	/* Query result */` |
|     65 |  140 | `	ph7_result_bool(pCtx,res);` |
|     65 |  141 | `	return PH7_OK;` |
|      5 |  142 | `}` |
|      - |  143 | `/*` |
|      - |  144 | ` * bool is_scalar($var)` |
|      - |  145 | ` *  Find out whether a variable is a scalar.` |
|      - |  146 | ` * Parameters` |
|      - |  147 | ` *  $var: The variable being evaluated.` |
|      - |  148 | ` * Return` |
|      - |  149 | ` *  True if var is scalar. False otherwise.` |
|      - |  150 | ` */` |
|     12 |  151 | `static int PH7_builtin_is_scalar(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  152 | `{` |
|     13 |  153 | `	int res = 0; /* Assume false by default */` |
|     13 |  154 | `	if( nArg > 0 ){` |
|     13 |  155 | `		res = ph7_value_is_scalar(apArg[0]);` |
|      6 |  156 | `	}` |
|      - |  157 | `	/* Query result */` |
|     13 |  158 | `	ph7_result_bool(pCtx,res);` |
|     13 |  159 | `	return PH7_OK;` |
|      1 |  160 | `}` |
|      - |  161 | `/*` |
|      - |  162 | ` * bool is_array($var)` |
|      - |  163 | ` *  Find out whether a variable is an array.` |
|      - |  164 | ` * Parameters` |
|      - |  165 | ` *  $var: The variable being evaluated.` |
|      - |  166 | ` * Return` |
|      - |  167 | ` *  True if var is an array. False otherwise.` |
|      - |  168 | ` */` |
|    396 |  169 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  170 | `{` |
|    401 |  171 | `	int res = 0; /* Assume false by default */` |
|    401 |  172 | `	if( nArg > 0 ){` |
|    401 |  173 | `		res = ph7_value_is_array(apArg[0]);` |
|    198 |  174 | `	}` |
|      - |  175 | `	/* Query result */` |
|    401 |  176 | `	ph7_result_bool(pCtx,res);` |
|    401 |  177 | `	return PH7_OK;` |
|      5 |  178 | `}` |
|      - |  179 | `/*` |
|      - |  180 | ` * bool is_object($var)` |
|      - |  181 | ` *  Find out whether a variable is an object.` |
|      - |  182 | ` * Parameters` |
|      - |  183 | ` *  $var: The variable being evaluated.` |
|      - |  184 | ` * Return` |
|      - |  185 | ` *  True if var is an object. False otherwise.` |
|      - |  186 | ` */` |
|    356 |  187 | `static int PH7_builtin_is_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  188 | `{` |
|    358 |  189 | `	int res = 0; /* Assume false by default */` |
|    358 |  190 | `	if( nArg > 0 ){` |
|    358 |  191 | `		res = ph7_value_is_object(apArg[0]);` |
|    178 |  192 | `	}` |
|      - |  193 | `	/* Query result */` |
|    358 |  194 | `	ph7_result_bool(pCtx,res);` |
|    358 |  195 | `	return PH7_OK;` |
|      2 |  196 | `}` |
|      - |  197 | `/*` |
|      - |  198 | ` * bool is_resource($var)` |
|      - |  199 | ` *  Find out whether a variable is a resource.` |
|      - |  200 | ` * Parameters` |
|      - |  201 | ` *  $var: The variable being evaluated.` |
|      - |  202 | ` * Return` |
|      - |  203 | ` *  True if a resource. False otherwise.` |
|      - |  204 | ` */` |
|     58 |  205 | `static int PH7_builtin_is_resource(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  206 | `{` |
|     61 |  207 | `	int res = 0; /* Assume false by default */` |
|     61 |  208 | `	if( nArg > 0 ){` |
|     61 |  209 | `		res = ph7_value_is_resource(apArg[0]);` |
|     29 |  210 | `	}` |
|     61 |  211 | `	ph7_result_bool(pCtx,res);` |
|     61 |  212 | `	return PH7_OK;` |
|      3 |  213 | `}` |
|      - |  214 | `/*` |
|      - |  215 | ` * float floatval($var)` |
|      - |  216 | ` *  Get float value of a variable.` |
|      - |  217 | ` * Parameter` |
|      - |  218 | ` *  $var: The variable being processed.` |
|      - |  219 | ` * Return` |
|      - |  220 | ` *  the float value of a variable.` |
|      - |  221 | ` */` |
|      4 |  222 | `static int PH7_builtin_floatval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  223 | `{` |
|      5 |  224 | `	if( nArg < 1 ){` |
|      - |  225 | `		/* return 0.0 */` |
|    ! 0 |  226 | `		ph7_result_double(pCtx,0);` |
|    ! 0 |  227 | `	}else{` |
|      - |  228 | `		double dval;` |
|      - |  229 | `		/* Perform the cast */` |
|      5 |  230 | `		dval = ph7_value_to_double(apArg[0]);` |
|      5 |  231 | `		ph7_result_double(pCtx,dval);` |
|      - |  232 | `	}` |
|      5 |  233 | `	return PH7_OK;` |
|      1 |  234 | `}` |
|      - |  235 | `/*` |
|      - |  236 | ` * int intval($var)` |
|      - |  237 | ` *  Get integer value of a variable.` |
|      - |  238 | ` * Parameter` |
|      - |  239 | ` *  $var: The variable being processed.` |
|      - |  240 | ` * Return` |
|      - |  241 | ` *  the int value of a variable.` |
|      - |  242 | ` */` |
|     46 |  243 | `static int PH7_builtin_intval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  244 | `{` |
|     47 |  245 | `	if( nArg < 1 ){` |
|      - |  246 | `		/* return 0 */` |
|    ! 0 |  247 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  248 | `	}else{` |
|      - |  249 | `		sxi64 iVal;` |
|      - |  250 | `		/* Perform the cast */` |
|     47 |  251 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|     47 |  252 | `		ph7_result_int64(pCtx,iVal);` |
|      - |  253 | `	}` |
|     47 |  254 | `	return PH7_OK;` |
|      1 |  255 | `}` |
|      - |  256 | `/*` |
|      - |  257 | ` * string strval($var)` |
|      - |  258 | ` *  Get the string representation of a variable.` |
|      - |  259 | ` * Parameter` |
|      - |  260 | ` *  $var: The variable being processed.` |
|      - |  261 | ` * Return` |
|      - |  262 | ` *  the string value of a variable.` |
|      - |  263 | ` */` |
|      2 |  264 | `static int PH7_builtin_strval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  265 | `{` |
|      3 |  266 | `	if( nArg < 1 ){` |
|      - |  267 | `		/* return NULL */` |
|    ! 0 |  268 | `		ph7_result_null(pCtx);` |
|    ! 0 |  269 | `	}else{` |
|      - |  270 | `		const char *zVal;` |
|      3 |  271 | `		int iLen = 0; /* cc -O6 warning */` |
|      - |  272 | `		/* Perform the cast */` |
|      3 |  273 | `		zVal = ph7_value_to_string(apArg[0],&iLen);` |
|      3 |  274 | `		ph7_result_string(pCtx,zVal,iLen);` |
|      - |  275 | `	}` |
|      3 |  276 | `	return PH7_OK;` |
|      1 |  277 | `}` |
|      - |  278 | `/*` |
|      - |  279 | ` * bool boolval($var)` |
|      - |  280 | ` *  Get the boolean value of a variable.` |
|      - |  281 | ` * Parameter` |
|      - |  282 | ` *  $var: The variable being processed.` |
|      - |  283 | ` * Return` |
|      - |  284 | ` *  the bool value of a variable.` |
|      - |  285 | ` */` |
|     16 |  286 | `static int PH7_builtin_boolval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  287 | `{` |
|      - |  288 | `	int bVal;` |
|     18 |  289 | `	if( nArg != 1 ){` |
|      4 |  290 | `		return PH7_VmThrowException(pCtx,` |
|      - |  291 | `			"ArgumentCountError",` |
|      - |  292 | `			"boolval() expects exactly 1 argument, %d given",` |
|      1 |  293 | `			nArg` |
|      - |  294 | `			);` |
|      - |  295 | `	}` |
|      - |  296 | `	/* Perform the cast */` |
|     15 |  297 | `	bVal = ph7_value_to_bool(apArg[0]);` |
|     15 |  298 | `	ph7_result_bool(pCtx,bVal);` |
|     15 |  299 | `	return PH7_OK;` |
|     10 |  300 | `}` |
|      - |  301 | `/*` |
|      - |  302 | ` * bool empty($var)` |
|      - |  303 | ` *  Determine whether a variable is empty.` |
|      - |  304 | ` * Parameters` |
|      - |  305 | ` *   $var: The variable being checked.` |
|      - |  306 | ` * Return` |
|      - |  307 | ` *  0 if var has a non-empty and non-zero value.1 otherwise.` |
|      - |  308 | ` */` |
|  34228 |  309 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  310 | `{` |
|  34233 |  311 | `	int res = 1; /* Assume empty by default */` |
|  34233 |  312 | `	if( nArg > 0 ){` |
|  34231 |  313 | `		res = ph7_value_is_empty(apArg[0]);` |
|  17113 |  314 | `	}` |
|  34233 |  315 | `	ph7_result_bool(pCtx,res);` |
|  34233 |  316 | `	return PH7_OK;` |
|      - |  317 |  |
|      5 |  318 | `}` |
|      - |  319 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |  320 | `#define PH7_NEED_BUILTIN_REG 1` |
|      - |  321 | `#endif` |
|      - |  322 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |  323 | `#define PH7_NEED_FMT_AND_INI 1` |
|      - |  324 | `#endif` |
|      - |  325 |  |
|      - |  326 | `/* Math functions moved to builtin_math.c */` |
|      - |  327 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - |  328 | `/*` |
|      - |  329 | ` * Section:` |
|      - |  330 | ` *    String handling Functions.` |
|      - |  331 | ` * Status:` |
|      - |  332 | ` *    Stable.` |
|      - |  333 | ` */` |
|      - |  334 | `/*` |
|      - |  335 | ` * string substr(string $string,int $start[, int $length ])` |
|      - |  336 | ` *  Return part of a string.` |
|      - |  337 | ` * Parameters` |
|      - |  338 | ` *  $string` |
|      - |  339 | ` *   The input string. Must be one character or longer.` |
|      - |  340 | ` * $start` |
|      - |  341 | ` *   If start is non-negative, the returned string will start at the start'th position` |
|      - |  342 | ` *   in string, counting from zero. For instance, in the string 'abcdef', the character` |
|      - |  343 | ` *   at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - |  344 | ` *   If start is negative, the returned string will start at the start'th character` |
|      - |  345 | ` *   from the end of string.` |
|      - |  346 | ` *   If string is less than or equal to start characters long, FALSE will be returned.` |
|      - |  347 | ` * $length` |
|      - |  348 | ` *   If length is given and is positive, the string returned will contain at most length` |
|      - |  349 | ` *   characters beginning from start (depending on the length of string).` |
|      - |  350 | ` *   If length is given and is negative, then that many characters will be omitted from` |
|      - |  351 | ` *   the end of string (after the start position has been calculated when a start is negative).` |
|      - |  352 | ` *   If start denotes the position of this truncation or beyond, false will be returned.` |
|      - |  353 | ` *   If length is given and is 0, FALSE or NULL an empty string will be returned.` |
|      - |  354 | ` *   If length is omitted, the substring starting from start until the end of the string` |
|      - |  355 | ` *   will be returned.` |
|      - |  356 | ` * Return` |
|      - |  357 | ` *  Returns the extracted part of string, or FALSE on failure or an empty string.` |
|      - |  358 | ` */` |
| 230454 |  359 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  360 | `{` |
| 230459 |  361 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"substr",1,"$string"); }` |
|      - |  362 | `	const char *zSource,*zOfft;` |
|      - |  363 | `	int nOfft,nLen,nSrcLen;` |
| 230459 |  364 | `	if( nArg < 2 ){` |
|      - |  365 | `		/* return FALSE */` |
|    ! 0 |  366 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  367 | `		return PH7_OK;` |
|      - |  368 | `	}` |
|      - |  369 | `	/* Extract the target string */` |
| 230459 |  370 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 230459 |  371 | `	if( nSrcLen < 1 ){` |
|      - |  372 | `		/* Empty string,return FALSE */` |
|  12375 |  373 | `		ph7_result_bool(pCtx,0);` |
|  12375 |  374 | `		return PH7_OK;` |
|      - |  375 | `	}` |
| 218089 |  376 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  377 | `	/* Extract the offset */` |
| 218089 |  378 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 218089 |  379 | `	if( nOfft < 0 ){` |
|  33119 |  380 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  33119 |  381 | `		if( zOfft < zSource ){` |
|      - |  382 | `			/* Invalid offset */` |
|      5 |  383 | `			ph7_result_bool(pCtx,0);` |
|      5 |  384 | `			return PH7_OK;` |
|      - |  385 | `		}` |
|  33115 |  386 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  33115 |  387 | `		nOfft = (int)(zOfft-zSource);` |
| 201530 |  388 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  389 | `		/* Invalid offset */` |
|    235 |  390 | `		ph7_result_bool(pCtx,0);` |
|    235 |  391 | `		return PH7_OK;` |
|    ! 0 |  392 | `	}else{` |
| 184745 |  393 | `		zOfft = &zSource[nOfft];` |
| 184745 |  394 | `		nLen = nSrcLen - nOfft;` |
|      - |  395 | `	}` |
| 217855 |  396 | `	if( nArg > 2 ){` |
|      - |  397 | `		/* Extract the length */` |
| 179677 |  398 | `		nLen = ph7_value_to_int(apArg[2]);` |
| 179677 |  399 | `		if( nLen == 0 ){` |
|      - |  400 | `			/* Invalid length,return an empty string */` |
|      5 |  401 | `			ph7_result_string(pCtx,"",0);` |
|      5 |  402 | `			return PH7_OK;` |
| 179673 |  403 | `		}else if( nLen < 0 ){` |
|  33049 |  404 | `			nLen = nSrcLen + nLen - nOfft;` |
|  33049 |  405 | `			if( nLen < 1 ){` |
|      - |  406 | `				/* Invalid  length */` |
|      3 |  407 | `				nLen = nSrcLen - nOfft;` |
|      1 |  408 | `			}` |
|  16522 |  409 | `		}` |
| 179673 |  410 | `		if( nLen + nOfft > nSrcLen ){` |
|      - |  411 | `			/* Invalid length */` |
|   5923 |  412 | `			nLen = nSrcLen - nOfft;` |
|   2959 |  413 | `		}` |
|  89834 |  414 | `	}` |
|      - |  415 | `	/* Return the substring */` |
| 217851 |  416 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 217851 |  417 | `	return PH7_OK;` |
| 115232 |  418 | `}` |
|      - |  419 | `/*` |
|      - |  420 | ` * int substr_compare(string $main_str,string $str ,int $offset[,int $length[,bool $case_insensitivity = false ]])` |
|      - |  421 | ` *  Binary safe comparison of two strings from an offset, up to length characters.` |
|      - |  422 | ` * Parameters` |
|      - |  423 | ` *  $main_str` |
|      - |  424 | ` *  The main string being compared.` |
|      - |  425 | ` *  $str` |
|      - |  426 | ` *   The secondary string being compared.` |
|      - |  427 | ` * $offset` |
|      - |  428 | ` *  The start position for the comparison. If negative, it starts counting from` |
|      - |  429 | ` *  the end of the string.` |
|      - |  430 | ` * $length` |
|      - |  431 | ` *  The length of the comparison. The default value is the largest of the length` |
|      - |  432 | ` *  of the str compared to the length of main_str less the offset.` |
|      - |  433 | ` * $case_insensitivity` |
|      - |  434 | ` *  If case_insensitivity is TRUE, comparison is case insensitive.` |
|      - |  435 | ` * Return` |
|      - |  436 | ` *  Returns < 0 if main_str from position offset is less than str, > 0 if it is greater than` |
|      - |  437 | ` *  str, and 0 if they are equal. If offset is equal to or greater than the length of main_str` |
|      - |  438 | ` *  or length is set and is less than 1, substr_compare() prints a warning and returns FALSE.` |
|      - |  439 | ` */` |
|     22 |  440 | `static int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  441 | `{` |
|      - |  442 | `	const char *zSource,*zOfft,*zSub;` |
|      - |  443 | `	int nOfft,nLen,nSrcLen,nSublen;` |
|     23 |  444 | `	int iCase = 0;` |
|      - |  445 | `	int rc;` |
|     23 |  446 | `	if( nArg < 3 ){` |
|      - |  447 | `		/* Missing arguments,return FALSE */` |
|    ! 0 |  448 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  449 | `		return PH7_OK;` |
|      - |  450 | `	}` |
|      - |  451 | `	/* Extract the target string */` |
|     23 |  452 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|     23 |  453 | `	if( nSrcLen < 1 ){` |
|      - |  454 | `		/* Empty string,return FALSE */` |
|      3 |  455 | `		ph7_result_bool(pCtx,0);` |
|      3 |  456 | `		return PH7_OK;` |
|      - |  457 | `	}` |
|     21 |  458 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  459 | `	/* Extract the substring */` |
|     21 |  460 | `	zSub = ph7_value_to_string(apArg[1],&nSublen);` |
|     21 |  461 | `	if( nSublen < 1 \|\| nSublen > nSrcLen){` |
|      - |  462 | `		/* Empty string,return FALSE */` |
|      3 |  463 | `		ph7_result_bool(pCtx,0);` |
|      3 |  464 | `		return PH7_OK;` |
|      - |  465 | `	}` |
|      - |  466 | `	/* Extract the offset */` |
|     19 |  467 | `	nOfft = ph7_value_to_int(apArg[2]);` |
|     19 |  468 | `	if( nOfft < 0 ){` |
|      5 |  469 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|      5 |  470 | `		if( zOfft < zSource ){` |
|      - |  471 | `			/* Invalid offset */` |
|      3 |  472 | `			ph7_result_bool(pCtx,0);` |
|      3 |  473 | `			return PH7_OK;` |
|      - |  474 | `		}` |
|      3 |  475 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|      3 |  476 | `		nOfft = (int)(zOfft-zSource);` |
|     16 |  477 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  478 | `		/* Invalid offset */` |
|      3 |  479 | `		ph7_result_bool(pCtx,0);` |
|      3 |  480 | `		return PH7_OK;` |
|    ! 0 |  481 | `	}else{` |
|     13 |  482 | `		zOfft = &zSource[nOfft];` |
|     13 |  483 | `		nLen = nSrcLen - nOfft;` |
|      - |  484 | `	}` |
|     15 |  485 | `	if( nArg > 3 ){` |
|      - |  486 | `		/* Extract the length */` |
|     13 |  487 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 |  488 | `		if( nLen < 1 ){` |
|      - |  489 | `			/* Invalid  length */` |
|      5 |  490 | `			ph7_result_int(pCtx,1);` |
|      5 |  491 | `			return PH7_OK;` |
|      9 |  492 | `		}else if( nLen + nOfft > nSrcLen ){` |
|      - |  493 | `			/* Invalid length */` |
|      3 |  494 | `			nLen = nSrcLen - nOfft;` |
|      1 |  495 | `		}` |
|      9 |  496 | `		if( nArg > 4 ){` |
|      - |  497 | `			/* Case-sensitive or not */` |
|      5 |  498 | `			iCase = ph7_value_to_bool(apArg[4]);` |
|      2 |  499 | `		}` |
|      4 |  500 | `	}` |
|      - |  501 | `	/* Perform the comparison */` |
|     11 |  502 | `	if( iCase ){` |
|      3 |  503 | `		rc = SyStrnicmp(zOfft,zSub,(sxu32)nLen);` |
|      2 |  504 | `	}else{` |
|      9 |  505 | `		rc = SyStrncmp(zOfft,zSub,(sxu32)nLen);` |
|      - |  506 | `	}` |
|      - |  507 | `	/* Comparison result */` |
|     11 |  508 | `	ph7_result_int(pCtx,rc);` |
|     11 |  509 | `	return PH7_OK;` |
|     12 |  510 | `}` |
|      - |  511 | `/*` |
|      - |  512 | ` * int substr_count(string $haystack,string $needle[,int $offset = 0 [,int $length ]])` |
|      - |  513 | ` *  Count the number of substring occurrences.` |
|      - |  514 | ` * Parameters` |
|      - |  515 | ` * $haystack` |
|      - |  516 | ` *   The string to search in` |
|      - |  517 | ` * $needle` |
|      - |  518 | ` *   The substring to search for` |
|      - |  519 | ` * $offset` |
|      - |  520 | ` *  The offset where to start counting` |
|      - |  521 | ` * $length (NOT USED)` |
|      - |  522 | ` *  The maximum length after the specified offset to search for the substring.` |
|      - |  523 | ` *  It outputs a warning if the offset plus the length is greater than the haystack length.` |
|      - |  524 | ` * Return` |
|      - |  525 | ` *  Toral number of substring occurrences.` |
|      - |  526 | ` */` |
|     26 |  527 | `static int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  528 | `{` |
|      - |  529 | `	const char *zText,*zPattern,*zEnd;` |
|      - |  530 | `	int nTextlen,nPatlen;` |
|     27 |  531 | `	int iCount = 0;` |
|      - |  532 | `	sxu32 nOfft;` |
|      - |  533 | `	sxi32 rc;` |
|     27 |  534 | `	if( nArg < 2 ){` |
|      - |  535 | `		/* Missing arguments */` |
|    ! 0 |  536 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  537 | `		return PH7_OK;` |
|      - |  538 | `	}` |
|      - |  539 | `	/* Point to the haystack */` |
|     27 |  540 | `	zText = ph7_value_to_string(apArg[0],&nTextlen);` |
|      - |  541 | `	/* Point to the neddle */` |
|     27 |  542 | `	zPattern = ph7_value_to_string(apArg[1],&nPatlen);` |
|     27 |  543 | `	if( nPatlen < 1 ){` |
|      - |  544 | `		/* Empty needle: PHP 8 throws a catchable ValueError. */` |
|      3 |  545 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  546 | `			"substr_count(): Argument #2 ($needle) must not be empty");` |
|      - |  547 | `	}` |
|      - |  548 | `	/* Apply the optional $offset/$length window before searching. PHP 8 validates` |
|      - |  549 | `	 * both against the haystack (a negative value counts from the end) and throws a` |
|      - |  550 | `	 * catchable ValueError when the result falls outside it — this happens before the` |
|      - |  551 | `	 * needle-fits check, so it fires even when the needle is longer than the haystack. */` |
|     25 |  552 | `	if( nArg > 2 ){` |
|     19 |  553 | `		ph7_int64 iOfft = ph7_value_to_int64(apArg[2]);` |
|     19 |  554 | `		if( iOfft < 0 ){` |
|      5 |  555 | `			iOfft += nTextlen;` |
|      2 |  556 | `		}` |
|     19 |  557 | `		if( iOfft < 0 \|\| iOfft > nTextlen ){` |
|      3 |  558 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  559 | `				"substr_count(): Argument #3 ($offset) must be contained in argument #1 ($haystack)");` |
|      - |  560 | `		}` |
|      - |  561 | `		/* Point to the desired offset and shrink the remaining region */` |
|     17 |  562 | `		zText = &zText[iOfft];` |
|     17 |  563 | `		nTextlen -= (int)iOfft;` |
|      8 |  564 | `	}` |
|     23 |  565 | `	if( nArg > 3 ){` |
|     15 |  566 | `		ph7_int64 nLen = ph7_value_to_int64(apArg[3]);` |
|     15 |  567 | `		if( nLen < 0 ){` |
|      - |  568 | `			/* Negative length is relative to the end of the (offset) haystack */` |
|      5 |  569 | `			nLen += nTextlen;` |
|      2 |  570 | `		}` |
|     15 |  571 | `		if( nLen < 0 \|\| nLen > nTextlen ){` |
|      5 |  572 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  573 | `				"substr_count(): Argument #4 ($length) must be contained in argument #1 ($haystack)");` |
|      - |  574 | `		}` |
|     11 |  575 | `		nTextlen = (int)nLen;` |
|      5 |  576 | `	}` |
|     19 |  577 | `	if( nTextlen < 1 \|\| nPatlen > nTextlen ){` |
|      - |  578 | `		/* The windowed haystack can't contain the needle: zero matches */` |
|      3 |  579 | `		ph7_result_int(pCtx,0);` |
|      3 |  580 | `		return PH7_OK;` |
|      - |  581 | `	}` |
|      - |  582 | `	/* Point to the end of the windowed haystack */` |
|     17 |  583 | `	zEnd = &zText[nTextlen];` |
|      - |  584 | `	/* Perform the search */` |
|     17 |  585 | `	for(;;){` |
|     35 |  586 | `		rc = SyBlobSearch((const void *)zText,(sxu32)(zEnd-zText),(const void *)zPattern,nPatlen,&nOfft);` |
|     35 |  587 | `		if( rc != SXRET_OK ){` |
|      - |  588 | `			/* Pattern not found,break immediately */` |
|     13 |  589 | `			break;` |
|      - |  590 | `		}` |
|      - |  591 | `		/* Increment counter and update the offset */` |
|     23 |  592 | `		iCount++;` |
|     23 |  593 | `		zText += nOfft + nPatlen;` |
|     23 |  594 | `		if( zText >= zEnd ){` |
|      5 |  595 | `			break;` |
|      - |  596 | `		}` |
|      1 |  597 | `	}` |
|      - |  598 | `	/* Pattern count */` |
|     17 |  599 | `	ph7_result_int(pCtx,iCount);` |
|     17 |  600 | `	return PH7_OK;` |
|     14 |  601 | `}` |
|      - |  602 | `/* Forward declarations: defined with the trim/addcslashes and str_contains` |
|      - |  603 | ` * families below. */` |
|      - |  604 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256]);` |
|      - |  605 | `/*` |
|      - |  606 | ` * php 8.1 null-to-non-nullable ZPP deprecation, notice-only form for the` |
|      - |  607 | ` * legacy string builtins that still coerce null to "" themselves: emit` |
|      - |  608 | ``  * `f(): Passing null to parameter #N ($name) of type string is deprecated` `` |
|      - |  609 | ` * when the arg is an actual null, leaving the resolution unchanged.` |
|      - |  610 | ` */` |
| 294736 |  611 | `static void StrNullArgNotice(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,int iArgNum,const char *zParamName)` |
|      5 |  612 | `{` |
| 294741 |  613 | `	if( ph7_value_is_null(pArg) ){` |
|     25 |  614 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |  615 | `			"%s(): Passing null to parameter #%d (%s) of type string is deprecated",` |
|      8 |  616 | `			zFunc,iArgNum,zParamName);` |
|      8 |  617 | `	}` |
| 294741 |  618 | `}` |
|      - |  619 | `static sxi32 StrPredicateResolveArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,` |
|      - |  620 | `	int iArgNum,const char *zParamName,const char *zTypeStr,const char *zNullMsg,` |
|      - |  621 | `	ph7_value *pTmp,const char **pzOut,int *pnOut);` |
|      - |  622 | `/*` |
|      - |  623 | ` * Validate and resolve an int-typed builtin parameter with php-8 ZPP weak-mode` |
|      - |  624 | ` * semantics: ints and bools pass through; null emits the 8.1 deprecation and` |
|      - |  625 | ` * resolves to 0; floats and float-strings convert, with the implicit-conversion` |
|      - |  626 | ` * E_DEPRECATED when lossy and a TypeError when NAN/INF/out of int range;` |
|      - |  627 | ` * integral numeric strings convert exactly; everything else (arrays, resources,` |
|      - |  628 | ` * objects, non-numeric strings) is a TypeError naming zTypeStr (e.g. "int",` |
|      - |  629 | ` * "array\|int"). Returns PH7_OK with *pOut set, or the throw status.` |
|      - |  630 | ` */` |
|    150 |  631 | `static sxi32 IntArgResolve(` |
|      - |  632 | `	ph7_context *pCtx,` |
|      - |  633 | `	ph7_value *pArg,` |
|      - |  634 | `	const char *zFunc,` |
|      - |  635 | `	int iArgNum,` |
|      - |  636 | `	const char *zParamName,` |
|      - |  637 | `	const char *zTypeStr,` |
|      - |  638 | `	sxi64 *pOut` |
|      1 |  639 | `){` |
|    151 |  640 | `	if( ph7_value_is_null(pArg) ){` |
|    ! 0 |  641 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |  642 | `			"%s(): Passing null to parameter #%d (%s) of type %s is deprecated",` |
|    ! 0 |  643 | `			zFunc,iArgNum,zParamName,zTypeStr` |
|      - |  644 | `			);` |
|    ! 0 |  645 | `		*pOut = 0;` |
|    ! 0 |  646 | `		return PH7_OK;` |
|      - |  647 | `	}` |
|    151 |  648 | `	if( ph7_value_is_float(pArg) ){` |
|      5 |  649 | `		double dVal = ph7_value_to_double(pArg);` |
|      - |  650 | `		sxi64 iVal;` |
|      - |  651 | `		/* php: NAN/INF/out-of-int64-range floats fail ZPP outright */` |
|      5 |  652 | `		if( dVal != dVal \|\| dVal >= 9223372036854775808.0 \|\| dVal < -9223372036854775808.0 ){` |
|      7 |  653 | `			return PH7_VmThrowException(pCtx,` |
|      - |  654 | `				"TypeError",` |
|      - |  655 | `				"%s(): Argument #%d (%s) must be of type %s, float given",` |
|      2 |  656 | `				zFunc,iArgNum,zParamName,zTypeStr` |
|      - |  657 | `				);` |
|      - |  658 | `		}` |
|    ! 0 |  659 | `		iVal = (sxi64)dVal;` |
|    ! 0 |  660 | `		if( (double)iVal != dVal ){` |
|    ! 0 |  661 | `			PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |  662 | `				"Implicit conversion from float %s to int loses precision",` |
|    ! 0 |  663 | `				ph7_value_to_string(pArg,0)` |
|      - |  664 | `				);` |
|    ! 0 |  665 | `		}` |
|    ! 0 |  666 | `		*pOut = iVal;` |
|    ! 0 |  667 | `		return PH7_OK;` |
|      - |  668 | `	}` |
|    147 |  669 | `	if( ph7_value_is_string(pArg) ){` |
|      - |  670 | `		const char *zNum;` |
|      - |  671 | `		int nSlen;` |
|     15 |  672 | `		int i,bFloat = 0;` |
|     15 |  673 | `		if( !PH7_MemObjStringIsNumeric(pArg) ){` |
|     16 |  674 | `			return PH7_VmThrowException(pCtx,` |
|      - |  675 | `				"TypeError",` |
|      - |  676 | `				"%s(): Argument #%d (%s) must be of type %s, string given",` |
|      5 |  677 | `				zFunc,iArgNum,zParamName,zTypeStr` |
|      - |  678 | `				);` |
|      - |  679 | `		}` |
|      5 |  680 | `		zNum = ph7_value_to_string(pArg,&nSlen);` |
|      9 |  681 | `		for( i = 0 ; i < nSlen ; i++ ){` |
|      5 |  682 | `			if( zNum[i] == '.' \|\| zNum[i] == 'e' \|\| zNum[i] == 'E' ){` |
|    ! 0 |  683 | `				bFloat = 1;` |
|    ! 0 |  684 | `				break;` |
|      - |  685 | `			}` |
|      3 |  686 | `		}` |
|      5 |  687 | `		if( bFloat ){` |
|    ! 0 |  688 | `			double dVal = 0;` |
|      - |  689 | `			sxi64 iVal;` |
|    ! 0 |  690 | `			SyStrToReal(zNum,(sxu32)nSlen,(void *)&dVal,0);` |
|    ! 0 |  691 | `			if( dVal != dVal \|\| dVal >= 9223372036854775808.0 \|\| dVal < -9223372036854775808.0 ){` |
|    ! 0 |  692 | `				return PH7_VmThrowException(pCtx,` |
|      - |  693 | `					"TypeError",` |
|      - |  694 | `					"%s(): Argument #%d (%s) must be of type %s, string given",` |
|    ! 0 |  695 | `					zFunc,iArgNum,zParamName,zTypeStr` |
|      - |  696 | `					);` |
|      - |  697 | `			}` |
|    ! 0 |  698 | `			iVal = (sxi64)dVal;` |
|    ! 0 |  699 | `			if( (double)iVal != dVal ){` |
|    ! 0 |  700 | `				PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |  701 | `					"Implicit conversion from float-string \"%s\" to int loses precision",` |
|    ! 0 |  702 | `					zNum` |
|      - |  703 | `					);` |
|    ! 0 |  704 | `			}` |
|    ! 0 |  705 | `			*pOut = iVal;` |
|    ! 0 |  706 | `			return PH7_OK;` |
|      - |  707 | `		}` |
|      5 |  708 | `		*pOut = ph7_value_to_int64(pArg);` |
|      5 |  709 | `		return PH7_OK;` |
|      - |  710 | `	}` |
|    133 |  711 | `	if( !ph7_value_is_int(pArg) && !ph7_value_is_bool(pArg) ){` |
|      - |  712 | `		/* Arrays, resources and objects: php names the class for objects */` |
|      5 |  713 | `		const char *zType = ph7_type_name(pArg);` |
|      5 |  714 | `		if( ph7_value_is_object(pArg) ){` |
|      3 |  715 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|      3 |  716 | `			if( pInst && pInst->pClass ){` |
|      3 |  717 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      1 |  718 | `			}` |
|      1 |  719 | `		}` |
|      7 |  720 | `		return PH7_VmThrowException(pCtx,` |
|      - |  721 | `			"TypeError",` |
|      - |  722 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|      2 |  723 | `			zFunc,iArgNum,zParamName,zTypeStr,zType` |
|      - |  724 | `			);` |
|      - |  725 | `	}` |
|    129 |  726 | `	*pOut = ph7_value_to_int64(pArg);` |
|    129 |  727 | `	return PH7_OK;` |
|     76 |  728 | `}` |
|      - |  729 | `/*` |
|      - |  730 | ` * Normalize a substr_replace() offset/length pair against a string of nStrLen` |
|      - |  731 | ` * bytes, exactly like PHP: a negative offset counts from the end (clamped to 0),` |
|      - |  732 | ` * an offset past the end clamps to the end; a negative length leaves that many` |
|      - |  733 | ` * bytes off the end of the remaining region (clamped to 0), and the length is` |
|      - |  734 | ` * finally clamped to the remaining region. Written without f+l additions so an` |
|      - |  735 | ` * INT64_MAX length cannot overflow.` |
|      - |  736 | ` */` |
|     60 |  737 | `static void SubstrReplaceWindow(sxi64 *pF,sxi64 *pL,int nStrLen)` |
|      1 |  738 | `{` |
|     61 |  739 | `	sxi64 f = *pF,l = *pL;` |
|     61 |  740 | `	if( f < 0 ){` |
|      9 |  741 | `		f += nStrLen;` |
|      9 |  742 | `		if( f < 0 ){` |
|      5 |  743 | `			f = 0;` |
|      3 |  744 | `		}` |
|     57 |  745 | `	}else if( f > nStrLen ){` |
|      5 |  746 | `		f = nStrLen;` |
|      2 |  747 | `	}` |
|     61 |  748 | `	if( l < 0 ){` |
|      7 |  749 | `		l += nStrLen - f;` |
|      7 |  750 | `		if( l < 0 ){` |
|      5 |  751 | `			l = 0;` |
|      2 |  752 | `		}` |
|      3 |  753 | `	}` |
|     61 |  754 | `	if( l > nStrLen - f ){` |
|     25 |  755 | `		l = nStrLen - f;` |
|     12 |  756 | `	}` |
|     61 |  757 | `	*pF = f;` |
|     61 |  758 | `	*pL = l;` |
|     61 |  759 | `}` |
|      - |  760 | `/* A replacement string collected out of substr_replace()'s $replace array.` |
|      - |  761 | ` * The bytes live in a shared pool blob (walker values are transient), so the` |
|      - |  762 | ` * item stores pool offsets, mirroring the strtr_entry technique. */` |
|      - |  763 | `typedef struct substr_repl_item substr_repl_item;` |
|      - |  764 | `struct substr_repl_item` |
|      - |  765 | `{` |
|      - |  766 | `	sxu32 nOfft; /* Offset of the string inside the pool */` |
|      - |  767 | `	sxu32 nLen;  /* Length of the string */` |
|      - |  768 | `};` |
|      - |  769 | `typedef struct substr_replace_collect substr_replace_collect;` |
|      - |  770 | `struct substr_replace_collect` |
|      - |  771 | `{` |
|      - |  772 | `	SyBlob *pPool;  /* Byte pool for string items (string walker only) */` |
|      - |  773 | `	SySet *pSet;    /* substr_repl_item set (string) or sxi64 set (int) */` |
|      - |  774 | `	sxi32 rc;       /* SXRET_OK or SXERR_MEM on collector failure */` |
|      - |  775 | `};` |
|      - |  776 | `/* ph7_array_walk() callback: append one $replace element to the pool. */` |
|      6 |  777 | `static int SubstrReplaceStrWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  778 | `{` |
|      7 |  779 | `	substr_replace_collect *pCol = (substr_replace_collect *)pUserData;` |
|      - |  780 | `	substr_repl_item sItem;` |
|      - |  781 | `	const char *zStr;` |
|      - |  782 | `	int nLen;` |
|      3 |  783 | `	SXUNUSED(pKey);` |
|      7 |  784 | `	zStr = ph7_value_to_string(pData,&nLen);` |
|      7 |  785 | `	sItem.nOfft = SyBlobLength(pCol->pPool);` |
|      7 |  786 | `	sItem.nLen = (sxu32)nLen;` |
|      7 |  787 | `	if( nLen > 0 && SXRET_OK != SyBlobAppend(pCol->pPool,(const void *)zStr,(sxu32)nLen) ){` |
|    ! 0 |  788 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  789 | `		return SXERR_ABORT;` |
|      - |  790 | `	}` |
|      7 |  791 | `	if( SXRET_OK != SySetPut(pCol->pSet,(const void *)&sItem) ){` |
|    ! 0 |  792 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  793 | `		return SXERR_ABORT;` |
|      - |  794 | `	}` |
|      7 |  795 | `	return PH7_OK;` |
|      4 |  796 | `}` |
|      - |  797 | `/* ph7_array_walk() callback: collect one $offset/$length element as an int. */` |
|     12 |  798 | `static int SubstrReplaceIntWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  799 | `{` |
|     13 |  800 | `	substr_replace_collect *pCol = (substr_replace_collect *)pUserData;` |
|     13 |  801 | `	sxi64 iVal = ph7_value_to_int64(pData);` |
|      6 |  802 | `	SXUNUSED(pKey);` |
|     13 |  803 | `	if( SXRET_OK != SySetPut(pCol->pSet,(const void *)&iVal) ){` |
|    ! 0 |  804 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  805 | `		return SXERR_ABORT;` |
|      - |  806 | `	}` |
|     13 |  807 | `	return PH7_OK;` |
|      7 |  808 | `}` |
|      - |  809 | `/* Per-element state while walking substr_replace()'s array $string. */` |
|      - |  810 | `typedef struct substr_replace_ctx substr_replace_ctx;` |
|      - |  811 | `struct substr_replace_ctx` |
|      - |  812 | `{` |
|      - |  813 | `	ph7_value *pResult;   /* Result array (keys preserved) */` |
|      - |  814 | `	ph7_value *pScratch;  /* Reusable string value for each element */` |
|      - |  815 | `	SyBlob *pReplPool;    /* Pool behind aRepl items */` |
|      - |  816 | `	SySet *pRepl;         /* substr_repl_item set or NULL when $replace is scalar */` |
|      - |  817 | `	SySet *pFrom;         /* sxi64 set or NULL when $offset is scalar */` |
|      - |  818 | `	SySet *pLen;          /* sxi64 set or NULL when $length is scalar/absent */` |
|      - |  819 | `	sxu32 iReplCur;       /* Next-position cursors into the three sets */` |
|      - |  820 | `	sxu32 iFromCur;` |
|      - |  821 | `	sxu32 iLenCur;` |
|      - |  822 | `	const char *zRepl;    /* Scalar $replace */` |
|      - |  823 | `	int nRepl;` |
|      - |  824 | `	sxi64 iFrom;          /* Scalar $offset */` |
|      - |  825 | `	sxi64 iLen;           /* Scalar $length */` |
|      - |  826 | `	int bLenGiven;        /* FALSE: $length absent/null -> element length */` |
|      - |  827 | `	sxi32 rc;             /* SXRET_OK or SXERR_MEM */` |
|      - |  828 | `};` |
|      - |  829 | `/*` |
|      - |  830 | ` * ph7_array_walk() callback over the array $string: replace the window of one` |
|      - |  831 | ` * element and insert the result under the element's original key. Array-form` |
|      - |  832 | ` * $replace/$offset/$length are consumed positionally; when a set runs out PHP` |
|      - |  833 | ` * falls back to ""/0/element-length respectively.` |
|      - |  834 | ` */` |
|     24 |  835 | `static int SubstrReplaceElemWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  836 | `{` |
|     25 |  837 | `	substr_replace_ctx *pRep = (substr_replace_ctx *)pUserData;` |
|      - |  838 | `	const char *zStr,*zRepl;` |
|      - |  839 | `	sxi64 f,l;` |
|      - |  840 | `	int nLen,nRepl;` |
|     25 |  841 | `	zStr = ph7_value_to_string(pData,&nLen);` |
|      - |  842 | `	/* Positional $replace element ("" when exhausted) */` |
|     25 |  843 | `	if( pRep->pRepl ){` |
|     11 |  844 | `		if( pRep->iReplCur < SySetUsed(pRep->pRepl) ){` |
|      7 |  845 | `			substr_repl_item *pItem = (substr_repl_item *)SySetAt(pRep->pRepl,pRep->iReplCur++);` |
|      7 |  846 | `			zRepl = (const char *)SyBlobDataAt(pRep->pReplPool,pItem->nOfft);` |
|      7 |  847 | `			nRepl = (int)pItem->nLen;` |
|      4 |  848 | `		}else{` |
|      5 |  849 | `			zRepl = "";` |
|      5 |  850 | `			nRepl = 0;` |
|      - |  851 | `		}` |
|      6 |  852 | `	}else{` |
|     15 |  853 | `		zRepl = pRep->zRepl;` |
|     15 |  854 | `		nRepl = pRep->nRepl;` |
|      - |  855 | `	}` |
|      - |  856 | `	/* Positional $offset element (0 when exhausted) */` |
|     25 |  857 | `	if( pRep->pFrom ){` |
|     13 |  858 | `		sxi64 *pVal = 0;` |
|     13 |  859 | `		if( pRep->iFromCur < SySetUsed(pRep->pFrom) ){` |
|      9 |  860 | `			pVal = (sxi64 *)SySetAt(pRep->pFrom,pRep->iFromCur++);` |
|      4 |  861 | `		}` |
|     13 |  862 | `		f = pVal ? *pVal : 0;` |
|      7 |  863 | `	}else{` |
|     13 |  864 | `		f = pRep->iFrom;` |
|      - |  865 | `	}` |
|      - |  866 | `	/* Positional $length element (element length when exhausted) */` |
|     25 |  867 | `	if( pRep->pLen ){` |
|      7 |  868 | `		sxi64 *pVal = 0;` |
|      7 |  869 | `		if( pRep->iLenCur < SySetUsed(pRep->pLen) ){` |
|      5 |  870 | `			pVal = (sxi64 *)SySetAt(pRep->pLen,pRep->iLenCur++);` |
|      2 |  871 | `		}` |
|      7 |  872 | `		l = pVal ? *pVal : nLen;` |
|      4 |  873 | `	}else{` |
|     19 |  874 | `		l = pRep->bLenGiven ? pRep->iLen : nLen;` |
|      - |  875 | `	}` |
|     25 |  876 | `	SubstrReplaceWindow(&f,&l,nLen);` |
|      - |  877 | `	/* Assemble prefix + replacement + suffix in the scratch value */` |
|     25 |  878 | `	ph7_value_reset_string_cursor(pRep->pScratch);` |
|     24 |  879 | `	if( (f > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,zStr,(int)f))` |
|     24 |  880 | `	 \|\| (nRepl > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,zRepl,nRepl))` |
|     40 |  881 | `	 \|\| (nLen - (int)(f+l) > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,&zStr[f+l],nLen - (int)(f+l))) ){` |
|     30 |  882 | `		pRep->rc = SXERR_MEM;` |
|     30 |  883 | `		return SXERR_ABORT;` |
|      - |  884 | `	}` |
|     25 |  885 | `	if( SXRET_OK != ph7_array_add_elem(pRep->pResult,pKey,pRep->pScratch) ){` |
|    ! 0 |  886 | `		pRep->rc = SXERR_MEM;` |
|    ! 0 |  887 | `		return SXERR_ABORT;` |
|      - |  888 | `	}` |
|     25 |  889 | `	return PH7_OK;` |
|     43 |  890 | `}` |
|      - |  891 | `/*` |
|      - |  892 | ` * mixed substr_replace(array\|string $string,array\|string $replace,array\|int $offset[,array\|int\|null $length = null])` |
|      - |  893 | ` *  Replace text within a portion of a string.` |
|      - |  894 | ` * Parameters` |
|      - |  895 | ` *  $string` |
|      - |  896 | ` *   The input string or an array of strings (each element is processed with` |
|      - |  897 | ` *   its own positional replace/offset/length when those are arrays too).` |
|      - |  898 | ` *  $replace` |
|      - |  899 | ` *   The replacement string. When $string is scalar and $replace is an array,` |
|      - |  900 | ` *   only its first element is used (PHP quirk).` |
|      - |  901 | ` *  $offset` |
|      - |  902 | ` *   Window start; negative counts from the end of the string.` |
|      - |  903 | ` *  $length` |
|      - |  904 | ` *   Window length; negative leaves that many bytes at the end; null/absent` |
|      - |  905 | ` *   means "to the end of the string".` |
|      - |  906 | ` * Return` |
|      - |  907 | ` *  The processed string, or an array of processed strings (keys preserved).` |
|      - |  908 | ` * Errors` |
|      - |  909 | ` *  ArgumentCountError on fewer than 3 arguments; TypeError when an array` |
|      - |  910 | ` *  $offset/$length is combined with a scalar $string.` |
|      - |  911 | ` */` |
|     68 |  912 | `static int PH7_builtin_substr_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  913 | `{` |
|      - |  914 | `	ph7_value sStrTmp,sReplTmp;` |
|     69 |  915 | `	const char *zStr = 0,*zRepl = 0;` |
|     69 |  916 | `	int nLen = 0,nRepl = 0;` |
|      - |  917 | `	int bLenGiven;` |
|     69 |  918 | `	sxi64 f = 0,l = 0;` |
|      - |  919 | `	sxi32 rc;` |
|     69 |  920 | `	if( nArg < 3 ){` |
|      7 |  921 | `		return PH7_VmThrowException(pCtx,` |
|      - |  922 | `			"ArgumentCountError",` |
|      - |  923 | `			"substr_replace() expects at least 3 arguments, %d given",` |
|      2 |  924 | `			nArg` |
|      - |  925 | `			);` |
|      - |  926 | `	}` |
|      - |  927 | `	/* $length counts as given unless absent or null (php: ?null semantics) */` |
|     65 |  928 | `	bLenGiven = (nArg > 3 && !ph7_value_is_null(apArg[3]));` |
|      - |  929 | `	/* php ZPP validates all four args, in order, before the body runs: the` |
|      - |  930 | `	 * non-array forms resolve here (null deprecation, __toString objects,` |
|      - |  931 | `	 * numeric strings), arrays pass through to the per-mode handling. */` |
|     65 |  932 | `	PH7_MemObjInit(pCtx->pVm,&sStrTmp);` |
|     65 |  933 | `	PH7_MemObjInit(pCtx->pVm,&sReplTmp);` |
|     65 |  934 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     49 |  935 | `		rc = StrPredicateResolveArg(pCtx,apArg[0],"substr_replace",1,"$string","array\|string",` |
|      - |  936 | `			"substr_replace(): Passing null to parameter #1 ($string) "` |
|      - |  937 | `			"of type array\|string is deprecated",` |
|      - |  938 | `			&sStrTmp,&zStr,&nLen);` |
|     49 |  939 | `		if( rc != PH7_OK ) goto out;` |
|     23 |  940 | `	}` |
|     63 |  941 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|     55 |  942 | `		rc = StrPredicateResolveArg(pCtx,apArg[1],"substr_replace",2,"$replace","array\|string",` |
|      - |  943 | `			"substr_replace(): Passing null to parameter #2 ($replace) "` |
|      - |  944 | `			"of type array\|string is deprecated",` |
|      - |  945 | `			&sReplTmp,&zRepl,&nRepl);` |
|     55 |  946 | `		if( rc != PH7_OK ) goto out;` |
|     25 |  947 | `	}` |
|     59 |  948 | `	if( !ph7_value_is_array(apArg[2]) ){` |
|     51 |  949 | `		rc = IntArgResolve(pCtx,apArg[2],"substr_replace",3,"$offset","array\|int",&f);` |
|     51 |  950 | `		if( rc != PH7_OK ) goto out;` |
|     24 |  951 | `	}` |
|     57 |  952 | `	if( bLenGiven && !ph7_value_is_array(apArg[3]) ){` |
|     31 |  953 | `		rc = IntArgResolve(pCtx,apArg[3],"substr_replace",4,"$length","array\|int\|null",&l);` |
|     31 |  954 | `		if( rc != PH7_OK ) goto out;` |
|     14 |  955 | `	}` |
|     55 |  956 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - |  957 | `		/* Array form: process each element, preserving keys */` |
|      - |  958 | `		substr_replace_ctx sRep;` |
|      - |  959 | `		substr_replace_collect sCol;` |
|      - |  960 | `		SyBlob sReplPool;` |
|      - |  961 | `		SySet sRepl,sFrom,sLen;` |
|      - |  962 | `		ph7_value *pResult,*pScratch;` |
|     15 |  963 | `		sxi32 rcWalk = SXRET_OK;` |
|     15 |  964 | `		SyBlobInit(&sReplPool,&pCtx->pVm->sAllocator);` |
|     15 |  965 | `		SySetInit(&sRepl,&pCtx->pVm->sAllocator,sizeof(substr_repl_item));` |
|     15 |  966 | `		SySetInit(&sFrom,&pCtx->pVm->sAllocator,sizeof(sxi64));` |
|     15 |  967 | `		SySetInit(&sLen,&pCtx->pVm->sAllocator,sizeof(sxi64));` |
|     15 |  968 | `		SyZero(&sRep,sizeof(substr_replace_ctx));` |
|     15 |  969 | `		sRep.bLenGiven = bLenGiven;` |
|     15 |  970 | `		sCol.rc = SXRET_OK;` |
|      - |  971 | `		/* Collect array-form $replace/$offset/$length positionally; the` |
|      - |  972 | `		 * scalar forms were already resolved above. */` |
|     15 |  973 | `		if( ph7_value_is_array(apArg[1]) ){` |
|      5 |  974 | `			sCol.pPool = &sReplPool;` |
|      5 |  975 | `			sCol.pSet = &sRepl;` |
|      5 |  976 | `			ph7_array_walk(apArg[1],SubstrReplaceStrWalker,&sCol);` |
|      5 |  977 | `			sRep.pRepl = &sRepl;` |
|      5 |  978 | `			sRep.pReplPool = &sReplPool;` |
|      3 |  979 | `		}else{` |
|     11 |  980 | `			sRep.zRepl = zRepl;` |
|     11 |  981 | `			sRep.nRepl = nRepl;` |
|      - |  982 | `		}` |
|     15 |  983 | `		if( sCol.rc == SXRET_OK && ph7_value_is_array(apArg[2]) ){` |
|      7 |  984 | `			sCol.pSet = &sFrom;` |
|      7 |  985 | `			ph7_array_walk(apArg[2],SubstrReplaceIntWalker,&sCol);` |
|      7 |  986 | `			sRep.pFrom = &sFrom;` |
|      4 |  987 | `		}else{` |
|      9 |  988 | `			sRep.iFrom = f;` |
|      - |  989 | `		}` |
|     15 |  990 | `		if( sCol.rc == SXRET_OK && bLenGiven ){` |
|      9 |  991 | `			if( ph7_value_is_array(apArg[3]) ){` |
|      5 |  992 | `				sCol.pSet = &sLen;` |
|      5 |  993 | `				ph7_array_walk(apArg[3],SubstrReplaceIntWalker,&sCol);` |
|      5 |  994 | `				sRep.pLen = &sLen;` |
|      3 |  995 | `			}else{` |
|      5 |  996 | `				sRep.iLen = l;` |
|      - |  997 | `			}` |
|      4 |  998 | `		}` |
|     15 |  999 | `		pResult = ph7_context_new_array(pCtx);` |
|     15 | 1000 | `		pScratch = ph7_context_new_scalar(pCtx);` |
|     15 | 1001 | `		if( sCol.rc != SXRET_OK \|\| pResult == 0 \|\| pScratch == 0 ){` |
|    ! 0 | 1002 | `			rcWalk = SXERR_MEM;` |
|    ! 0 | 1003 | `		}else{` |
|     15 | 1004 | `			sRep.pResult = pResult;` |
|     15 | 1005 | `			sRep.pScratch = pScratch;` |
|     15 | 1006 | `			ph7_value_string(pScratch,"",0); /* Force string representation */` |
|     15 | 1007 | `			ph7_array_walk(apArg[0],SubstrReplaceElemWalker,&sRep);` |
|     15 | 1008 | `			rcWalk = sRep.rc;` |
|      - | 1009 | `		}` |
|     15 | 1010 | `		SyBlobRelease(&sReplPool);` |
|     15 | 1011 | `		SySetRelease(&sRepl);` |
|     15 | 1012 | `		SySetRelease(&sFrom);` |
|     15 | 1013 | `		SySetRelease(&sLen);` |
|     15 | 1014 | `		if( rcWalk != SXRET_OK ){` |
|    ! 0 | 1015 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1016 | `			goto out;` |
|      - | 1017 | `		}` |
|     15 | 1018 | `		ph7_result_value(pCtx,pResult);` |
|     15 | 1019 | `		rc = PH7_OK;` |
|     15 | 1020 | `		goto out;` |
|      - | 1021 | `	}` |
|      - | 1022 | `	/* Scalar form: array $offset/$length are a TypeError, array $replace` |
|      - | 1023 | `	 * degrades to its first element (php quirk). */` |
|     41 | 1024 | `	if( ph7_value_is_array(apArg[2]) ){` |
|      3 | 1025 | `		rc = PH7_VmThrowException(pCtx,` |
|      - | 1026 | `			"TypeError",` |
|      - | 1027 | `			"substr_replace(): Argument #3 ($offset) cannot be an array when working on a single string"` |
|      - | 1028 | `			);` |
|      3 | 1029 | `		goto out;` |
|      - | 1030 | `	}` |
|     39 | 1031 | `	if( bLenGiven && ph7_value_is_array(apArg[3]) ){` |
|      3 | 1032 | `		rc = PH7_VmThrowException(pCtx,` |
|      - | 1033 | `			"TypeError",` |
|      - | 1034 | `			"substr_replace(): Argument #4 ($length) cannot be an array when working on a single string"` |
|      - | 1035 | `			);` |
|      3 | 1036 | `		goto out;` |
|      - | 1037 | `	}` |
|     37 | 1038 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 1039 | `		/* First element of the replace array, or "" when empty */` |
|      5 | 1040 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      5 | 1041 | `		zRepl = "";` |
|      5 | 1042 | `		nRepl = 0;` |
|      5 | 1043 | `		if( pMap->pFirst ){` |
|      3 | 1044 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pMap->pFirst->nValIdx);` |
|      3 | 1045 | `			if( pVal ){` |
|      3 | 1046 | `				zRepl = ph7_value_to_string(pVal,&nRepl);` |
|      1 | 1047 | `			}` |
|      1 | 1048 | `		}` |
|      2 | 1049 | `	}` |
|     37 | 1050 | `	if( !bLenGiven ){` |
|     15 | 1051 | `		l = nLen;` |
|      7 | 1052 | `	}` |
|     37 | 1053 | `	SubstrReplaceWindow(&f,&l,nLen);` |
|      - | 1054 | `	/* Assemble prefix + replacement + suffix straight into the call result` |
|      - | 1055 | `	 * (ph7_result_string appends), no scratch buffer needed. */` |
|     37 | 1056 | `	rc = SXRET_OK;` |
|     37 | 1057 | `	if( f > 0 ){` |
|     29 | 1058 | `		rc = ph7_result_string(pCtx,zStr,(int)f);` |
|     14 | 1059 | `	}` |
|     37 | 1060 | `	if( rc == SXRET_OK && nRepl > 0 ){` |
|     33 | 1061 | `		rc = ph7_result_string(pCtx,zRepl,nRepl);` |
|     16 | 1062 | `	}` |
|     37 | 1063 | `	if( rc == SXRET_OK && nLen - (int)(f+l) > 0 ){` |
|     17 | 1064 | `		rc = ph7_result_string(pCtx,&zStr[f+l],nLen - (int)(f+l));` |
|      8 | 1065 | `	}` |
|     37 | 1066 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 1067 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1068 | `		goto out;` |
|      - | 1069 | `	}` |
|      - | 1070 | `	/* Force a string result even when all three segments are empty */` |
|     37 | 1071 | `	rc = ph7_result_string(pCtx,"",0);` |
|     37 | 1072 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 1073 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1074 | `		goto out;` |
|      - | 1075 | `	}` |
|     37 | 1076 | `	rc = PH7_OK;` |
|     32 | 1077 | `out:` |
|     65 | 1078 | `	PH7_MemObjRelease(&sStrTmp);` |
|     65 | 1079 | `	PH7_MemObjRelease(&sReplTmp);` |
|     65 | 1080 | `	return rc;` |
|     35 | 1081 | `}` |
|      - | 1082 | `/*` |
|      - | 1083 | ` * int levenshtein(string $string1,string $string2[,int $insertion_cost = 1[,int $replacement_cost = 1[,int $deletion_cost = 1]]])` |
|      - | 1084 | ` *  Calculate the Levenshtein distance between two strings, byte per byte` |
|      - | 1085 | ` *  (case-sensitive), with optional per-operation costs. Mirrors PHP's` |
|      - | 1086 | ` *  reference_levdist(): two rolling rows over string2.` |
|      - | 1087 | ` * Return` |
|      - | 1088 | ` *  The minimal number of weighted edit operations turning $string1 into` |
|      - | 1089 | ` *  $string2.` |
|      - | 1090 | ` */` |
|     42 | 1091 | `static int PH7_builtin_levenshtein(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1092 | `{` |
|      - | 1093 | `	static const char *azParam[] = { "$insertion_cost","$replacement_cost","$deletion_cost" };` |
|      - | 1094 | `	const char *zStr1,*zStr2;` |
|     43 | 1095 | `	sxi64 iCostIns = 1,iCostRep = 1,iCostDel = 1;` |
|      - | 1096 | `	sxi64 *p1,*p2,*pTmp;` |
|      - | 1097 | `	sxi64 c0,c1,c2;` |
|      - | 1098 | `	ph7_value sTmp1,sTmp2;` |
|      - | 1099 | `	int nLen1,nLen2;` |
|      - | 1100 | `	int i1,i2;` |
|      - | 1101 | `	sxi32 rc;` |
|      - | 1102 | `	int i;` |
|     43 | 1103 | `	if( nArg < 2 ){` |
|      4 | 1104 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1105 | `			"ArgumentCountError",` |
|      - | 1106 | `			"levenshtein() expects at least 2 arguments, %d given",` |
|      1 | 1107 | `			nArg` |
|      - | 1108 | `			);` |
|      - | 1109 | `	}` |
|      - | 1110 | `	/* $string1/$string2: null deprecates to "", __toString objects resolve,` |
|      - | 1111 | `	 * everything non-stringish is a TypeError (php ZPP weak mode). */` |
|     41 | 1112 | `	PH7_MemObjInit(pCtx->pVm,&sTmp1);` |
|     41 | 1113 | `	PH7_MemObjInit(pCtx->pVm,&sTmp2);` |
|     41 | 1114 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"levenshtein",1,"$string1","string",` |
|      - | 1115 | `		"levenshtein(): Passing null to parameter #1 ($string1) "` |
|      - | 1116 | `		"of type string is deprecated",` |
|      - | 1117 | `		&sTmp1,&zStr1,&nLen1);` |
|     41 | 1118 | `	if( rc != PH7_OK ) goto out;` |
|     39 | 1119 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"levenshtein",2,"$string2","string",` |
|      - | 1120 | `		"levenshtein(): Passing null to parameter #2 ($string2) "` |
|      - | 1121 | `		"of type string is deprecated",` |
|      - | 1122 | `		&sTmp2,&zStr2,&nLen2);` |
|     39 | 1123 | `	if( rc != PH7_OK ) goto out;` |
|      - | 1124 | `	/* Optional integer costs */` |
|     63 | 1125 | `	for( i = 2 ; i < nArg && i < 5 ; i++ ){` |
|      - | 1126 | `		sxi64 iVal;` |
|     37 | 1127 | `		rc = IntArgResolve(pCtx,apArg[i],"levenshtein",i+1,azParam[i-2],"int",&iVal);` |
|     37 | 1128 | `		if( rc != PH7_OK ) goto out;` |
|     25 | 1129 | `		if( i == 2 ){` |
|     13 | 1130 | `			iCostIns = iVal;` |
|     19 | 1131 | `		}else if( i == 3 ){` |
|      7 | 1132 | `			iCostRep = iVal;` |
|      4 | 1133 | `		}else{` |
|      7 | 1134 | `			iCostDel = iVal;` |
|      - | 1135 | `		}` |
|     13 | 1136 | `	}` |
|     27 | 1137 | `	if( nLen1 == 0 ){` |
|      3 | 1138 | `		ph7_result_int64(pCtx,(sxi64)nLen2 * iCostIns);` |
|      3 | 1139 | `		rc = PH7_OK;` |
|      3 | 1140 | `		goto out;` |
|      - | 1141 | `	}` |
|     25 | 1142 | `	if( nLen2 == 0 ){` |
|      3 | 1143 | `		ph7_result_int64(pCtx,(sxi64)nLen1 * iCostDel);` |
|      3 | 1144 | `		rc = PH7_OK;` |
|      3 | 1145 | `		goto out;` |
|      - | 1146 | `	}` |
|      - | 1147 | `	/* Two rolling DP rows over string2 (auto-released on return). Reject a` |
|      - | 1148 | `	 * string2 long enough to overflow the 32-bit allocation size. */` |
|     23 | 1149 | `	if( (sxu32)nLen2 >= (SXU32_HIGH / sizeof(sxi64)) - 1 ){` |
|    ! 0 | 1150 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1151 | `		goto out;` |
|      - | 1152 | `	}` |
|     23 | 1153 | `	p1 = (sxi64 *)ph7_context_alloc_chunk(pCtx,(unsigned int)(sizeof(sxi64) * (sxu32)(nLen2 + 1)),FALSE,TRUE);` |
|     23 | 1154 | `	p2 = (sxi64 *)ph7_context_alloc_chunk(pCtx,(unsigned int)(sizeof(sxi64) * (sxu32)(nLen2 + 1)),FALSE,TRUE);` |
|     23 | 1155 | `	if( p1 == 0 \|\| p2 == 0 ){` |
|    ! 0 | 1156 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1157 | `		goto out;` |
|      - | 1158 | `	}` |
|    733 | 1159 | `	for( i2 = 0 ; i2 <= nLen2 ; i2++ ){` |
|    711 | 1160 | `		p1[i2] = (sxi64)i2 * iCostIns;` |
|    356 | 1161 | `	}` |
|    707 | 1162 | `	for( i1 = 0 ; i1 < nLen1 ; i1++ ){` |
|    685 | 1163 | `		p2[0] = p1[0] + iCostDel;` |
| 181111 | 1164 | `		for( i2 = 0 ; i2 < nLen2 ; i2++ ){` |
| 180427 | 1165 | `			c0 = p1[i2] + ((zStr1[i1] == zStr2[i2]) ? 0 : iCostRep);` |
| 180427 | 1166 | `			c1 = p1[i2 + 1] + iCostDel;` |
| 180427 | 1167 | `			if( c1 < c0 ){` |
|  45393 | 1168 | `				c0 = c1;` |
|  22696 | 1169 | `			}` |
| 180427 | 1170 | `			c2 = p2[i2] + iCostIns;` |
| 180427 | 1171 | `			if( c2 < c0 ){` |
|  44809 | 1172 | `				c0 = c2;` |
|  22404 | 1173 | `			}` |
| 180427 | 1174 | `			p2[i2 + 1] = c0;` |
|  90214 | 1175 | `		}` |
|    685 | 1176 | `		pTmp = p1;` |
|    685 | 1177 | `		p1 = p2;` |
|    685 | 1178 | `		p2 = pTmp;` |
|    343 | 1179 | `	}` |
|     23 | 1180 | `	ph7_result_int64(pCtx,p1[nLen2]);` |
|     23 | 1181 | `	rc = PH7_OK;` |
|     20 | 1182 | `out:` |
|     41 | 1183 | `	PH7_MemObjRelease(&sTmp1);` |
|     41 | 1184 | `	PH7_MemObjRelease(&sTmp2);` |
|     41 | 1185 | `	return rc;` |
|     22 | 1186 | `}` |
|      - | 1187 | `/*` |
|      - | 1188 | ` * Longest common substring scan behind similar_text() — a faithful port of` |
|      - | 1189 | ` * PHP's php_similar_str(): O(n*m) scan recording the first longest run.` |
|      - | 1190 | ` */` |
|     26 | 1191 | `static void SimilarStr(const char *zTxt1,int nLen1,const char *zTxt2,int nLen2,` |
|      - | 1192 | `	int *pPos1,int *pPos2,int *pMax,int *pCount)` |
|      1 | 1193 | `{` |
|      - | 1194 | `	const char *p,*q;` |
|     27 | 1195 | `	const char *zEnd1 = &zTxt1[nLen1];` |
|     27 | 1196 | `	const char *zEnd2 = &zTxt2[nLen2];` |
|      - | 1197 | `	int l;` |
|     27 | 1198 | `	*pMax = 0;` |
|     27 | 1199 | `	*pCount = 0;` |
|    143 | 1200 | `	for( p = zTxt1 ; p < zEnd1 ; p++ ){` |
|    843 | 1201 | `		for( q = zTxt2 ; q < zEnd2 ; q++ ){` |
|    999 | 1202 | `			for( l = 0 ; (p+l < zEnd1) && (q+l < zEnd2) && (p[l] == q[l]) ; l++ );` |
|    727 | 1203 | `			if( l > *pMax ){` |
|     25 | 1204 | `				*pMax = l;` |
|     25 | 1205 | `				*pCount += 1;` |
|     25 | 1206 | `				*pPos1 = (int)(p - zTxt1);` |
|     25 | 1207 | `				*pPos2 = (int)(q - zTxt2);` |
|     12 | 1208 | `			}` |
|    364 | 1209 | `		}` |
|     59 | 1210 | `	}` |
|     27 | 1211 | `}` |
|      - | 1212 | `/*` |
|      - | 1213 | ` * Recursive divide-and-conquer behind similar_text() — a faithful port of` |
|      - | 1214 | `` * PHP's php_similar_char(), including its quirky `count > 1` guard on the`` |
|      - | 1215 | ` * left-side recursion.` |
|      - | 1216 | ` */` |
|     26 | 1217 | `static int SimilarChar(const char *zTxt1,int nLen1,const char *zTxt2,int nLen2)` |
|      1 | 1218 | `{` |
|      - | 1219 | `	int nSum;` |
|     27 | 1220 | `	int nPos1 = 0,nPos2 = 0,nMax,nCount;` |
|     27 | 1221 | `	SimilarStr(zTxt1,nLen1,zTxt2,nLen2,&nPos1,&nPos2,&nMax,&nCount);` |
|     27 | 1222 | `	if( (nSum = nMax) != 0 ){` |
|     25 | 1223 | `		if( nPos1 && nPos2 && nCount > 1 ){` |
|    ! 0 | 1224 | `			nSum += SimilarChar(zTxt1,nPos1,zTxt2,nPos2);` |
|    ! 0 | 1225 | `		}` |
|     25 | 1226 | `		if( (nPos1 + nMax < nLen1) && (nPos2 + nMax < nLen2) ){` |
|     13 | 1227 | `			nSum += SimilarChar(&zTxt1[nPos1 + nMax],nLen1 - nPos1 - nMax,` |
|      8 | 1228 | `				&zTxt2[nPos2 + nMax],nLen2 - nPos2 - nMax);` |
|      4 | 1229 | `		}` |
|     12 | 1230 | `	}` |
|     27 | 1231 | `	return nSum;` |
|      1 | 1232 | `}` |
|      - | 1233 | `/*` |
|      - | 1234 | ` * int similar_text(string $string1,string $string2[,float &$percent])` |
|      - | 1235 | ` *  Calculate the similarity between two strings, as the number of matching` |
|      - | 1236 | ` *  characters found by PHP's greedy longest-common-substring recursion.` |
|      - | 1237 | ` *  When $percent is given it receives the similarity in percent:` |
|      - | 1238 | ` *  matching * 200 / (len1 + len2).` |
|      - | 1239 | ` * Return` |
|      - | 1240 | ` *  The number of matching characters in both strings.` |
|      - | 1241 | ` */` |
|     28 | 1242 | `static int PH7_builtin_similar_text(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1243 | `{` |
|      - | 1244 | `	const char *zStr1,*zStr2;` |
|      - | 1245 | `	ph7_value sTmp1,sTmp2;` |
|      - | 1246 | `	int nLen1,nLen2;` |
|      - | 1247 | `	int nSim;` |
|      - | 1248 | `	sxi32 rc;` |
|     29 | 1249 | `	if( nArg < 2 ){` |
|      4 | 1250 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1251 | `			"ArgumentCountError",` |
|      - | 1252 | `			"similar_text() expects at least 2 arguments, %d given",` |
|      1 | 1253 | `			nArg` |
|      - | 1254 | `			);` |
|      - | 1255 | `	}` |
|     27 | 1256 | `	PH7_MemObjInit(pCtx->pVm,&sTmp1);` |
|     27 | 1257 | `	PH7_MemObjInit(pCtx->pVm,&sTmp2);` |
|     27 | 1258 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"similar_text",1,"$string1","string",` |
|      - | 1259 | `		"similar_text(): Passing null to parameter #1 ($string1) "` |
|      - | 1260 | `		"of type string is deprecated",` |
|      - | 1261 | `		&sTmp1,&zStr1,&nLen1);` |
|     27 | 1262 | `	if( rc != PH7_OK ) goto out;` |
|     25 | 1263 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"similar_text",2,"$string2","string",` |
|      - | 1264 | `		"similar_text(): Passing null to parameter #2 ($string2) "` |
|      - | 1265 | `		"of type string is deprecated",` |
|      - | 1266 | `		&sTmp2,&zStr2,&nLen2);` |
|     25 | 1267 | `	if( rc != PH7_OK ) goto out;` |
|     23 | 1268 | `	if( nLen1 + nLen2 == 0 ){` |
|      5 | 1269 | `		nSim = 0;` |
|      3 | 1270 | `	}else{` |
|     19 | 1271 | `		nSim = SimilarChar(zStr1,nLen1,zStr2,nLen2);` |
|      - | 1272 | `	}` |
|     23 | 1273 | `	if( nArg > 2 ){` |
|      - | 1274 | `		/* Write the percentage through the by-ref out-param */` |
|      7 | 1275 | `		ph7_value *pPercent = ph7_context_new_scalar(pCtx);` |
|      7 | 1276 | `		if( pPercent == 0 ){` |
|    ! 0 | 1277 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1278 | `			goto out;` |
|    ! 0 | 1279 | `		}else{` |
|      7 | 1280 | `			double dPct = (nLen1 + nLen2 == 0) ? 0.0 : (double)nSim * 200.0 / (double)(nLen1 + nLen2);` |
|      7 | 1281 | `			ph7_value_double(pPercent,dPct);` |
|      7 | 1282 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],pPercent);` |
|      - | 1283 | `		}` |
|      3 | 1284 | `	}` |
|     23 | 1285 | `	ph7_result_int(pCtx,nSim);` |
|     23 | 1286 | `	rc = PH7_OK;` |
|     13 | 1287 | `out:` |
|     27 | 1288 | `	PH7_MemObjRelease(&sTmp1);` |
|     27 | 1289 | `	PH7_MemObjRelease(&sTmp2);` |
|     27 | 1290 | `	return rc;` |
|     15 | 1291 | `}` |
|      - | 1292 | `/*` |
|      - | 1293 | ` * array\|int str_word_count(string $string[,int $format = 0[,?string $characters = null]])` |
|      - | 1294 | ` *  Count (or return) the words inside a string. A word is a run of alphabetic` |
|      - | 1295 | ` *  characters, which may contain (but not start the string with) "'" and "-";` |
|      - | 1296 | ` *  $characters adds extra bytes to the word set ("a..z" ranges supported, as` |
|      - | 1297 | ` *  in PHP's php_charmask).` |
|      - | 1298 | ` *  $format: 0 -> word count, 1 -> array of words, 2 -> array of words keyed` |
|      - | 1299 | ` *  by their byte position in $string.` |
|      - | 1300 | ` * Errors` |
|      - | 1301 | ` *  ValueError when $format is not 0, 1 or 2.` |
|      - | 1302 | ` */` |
|     52 | 1303 | `static int PH7_builtin_str_word_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1304 | `{` |
|      - | 1305 | `	const char *zIn,*zEnd,*zPtr;` |
|     53 | 1306 | `	ph7_value *pArray = 0,*pValue = 0;` |
|      - | 1307 | `	ph7_value sTmp,sListTmp;` |
|      - | 1308 | `	char aMask[256];` |
|     53 | 1309 | `	int bMask = 0;` |
|     53 | 1310 | `	int iFormat = 0;` |
|     53 | 1311 | `	int nCount = 0;` |
|      - | 1312 | `	int nLen;` |
|      - | 1313 | `	sxi32 rc;` |
|     53 | 1314 | `	if( nArg < 1 ){` |
|      4 | 1315 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1316 | `			"ArgumentCountError",` |
|      - | 1317 | `			"str_word_count() expects at least 1 argument, %d given",` |
|      1 | 1318 | `			nArg` |
|      - | 1319 | `			);` |
|      - | 1320 | `	}` |
|     51 | 1321 | `	PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|     51 | 1322 | `	PH7_MemObjInit(pCtx->pVm,&sListTmp);` |
|     51 | 1323 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_word_count",1,"$string","string",` |
|      - | 1324 | `		"str_word_count(): Passing null to parameter #1 ($string) "` |
|      - | 1325 | `		"of type string is deprecated",` |
|      - | 1326 | `		&sTmp,&zIn,&nLen);` |
|     51 | 1327 | `	if( rc != PH7_OK ) goto out;` |
|     49 | 1328 | `	if( nArg > 1 ){` |
|      - | 1329 | `		sxi64 iVal;` |
|     35 | 1330 | `		rc = IntArgResolve(pCtx,apArg[1],"str_word_count",2,"$format","int",&iVal);` |
|     37 | 1331 | `		if( rc != PH7_OK ) goto out;` |
|     33 | 1332 | `		if( iVal < 0 \|\| iVal > 2 ){` |
|      5 | 1333 | `			rc = PH7_VmThrowException(pCtx,` |
|      - | 1334 | `				"ValueError",` |
|      - | 1335 | `				"str_word_count(): Argument #2 ($format) must be a valid format value"` |
|      - | 1336 | `				);` |
|      5 | 1337 | `			goto out;` |
|      - | 1338 | `		}` |
|     29 | 1339 | `		iFormat = (int)iVal;` |
|     14 | 1340 | `	}` |
|     43 | 1341 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      - | 1342 | `		/* $characters is ?string: null (skipped above) simply keeps the` |
|      - | 1343 | `		 * default word set, no deprecation. */` |
|      - | 1344 | `		const char *zList;` |
|      - | 1345 | `		int nList;` |
|     17 | 1346 | `		rc = StrPredicateResolveArg(pCtx,apArg[2],"str_word_count",3,"$characters","?string",` |
|      - | 1347 | `			"" /* unreachable: null never gets here */,` |
|      - | 1348 | `			&sListTmp,&zList,&nList);` |
|     17 | 1349 | `		if( rc != PH7_OK ) goto out;` |
|     13 | 1350 | `		PH7_BuildCharMask(pCtx,zList,nList,aMask);` |
|     13 | 1351 | `		bMask = 1;` |
|      6 | 1352 | `	}` |
|     39 | 1353 | `	if( iFormat != 0 ){` |
|     25 | 1354 | `		pArray = ph7_context_new_array(pCtx);` |
|     25 | 1355 | `		pValue = ph7_context_new_scalar(pCtx);` |
|     25 | 1356 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1357 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1358 | `			goto out;` |
|      - | 1359 | `		}` |
|     12 | 1360 | `	}` |
|     39 | 1361 | `	zPtr = zIn;` |
|     39 | 1362 | `	zEnd = &zIn[nLen];` |
|     39 | 1363 | `	if( nLen > 0 ){` |
|      - | 1364 | `		/* php: the string's first byte cannot be ' or -, and its last byte` |
|      - | 1365 | `		 * cannot be -, unless the charlist explicitly allows them. */` |
|     33 | 1366 | `		if( (zPtr[0] == '\'' && (!bMask \|\| !aMask[(unsigned char)'\''])) \|\|` |
|     28 | 1367 | `			(zPtr[0] == '-'  && (!bMask \|\| !aMask[(unsigned char)'-'])) ){` |
|      9 | 1368 | `			zPtr++;` |
|      4 | 1369 | `		}` |
|     33 | 1370 | `		if( zEnd[-1] == '-' && (!bMask \|\| !aMask[(unsigned char)'-']) ){` |
|      9 | 1371 | `			zEnd--;` |
|      4 | 1372 | `		}` |
|     16 | 1373 | `	}` |
|    135 | 1374 | `	while( zPtr < zEnd ){` |
|     91 | 1375 | `		const char *zStart = zPtr;` |
|    477 | 1376 | `		while( zPtr < zEnd && ( SyisAlpha((unsigned char)zPtr[0])` |
|    253 | 1377 | `			\|\| (bMask && aMask[(unsigned char)zPtr[0]])` |
|     98 | 1378 | `			\|\| zPtr[0] == '\'' \|\| zPtr[0] == '-' ) ){` |
|    339 | 1379 | `			zPtr++;` |
|      1 | 1380 | `		}` |
|     97 | 1381 | `		if( zPtr > zStart ){` |
|     91 | 1382 | `			if( iFormat == 0 ){` |
|     19 | 1383 | `				nCount++;` |
|     10 | 1384 | `			}else{` |
|     73 | 1385 | `				ph7_value_reset_string_cursor(pValue);` |
|     73 | 1386 | `				if( SXRET_OK != ph7_value_string(pValue,zStart,(int)(zPtr-zStart)) ){` |
|    ! 0 | 1387 | `					rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1388 | `					goto out;` |
|      - | 1389 | `				}` |
|     73 | 1390 | `				if( iFormat == 1 ){` |
|     59 | 1391 | `					if( SXRET_OK != ph7_array_add_elem(pArray,0,pValue) ){` |
|    ! 0 | 1392 | `						rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1393 | `						goto out;` |
|      - | 1394 | `					}` |
|     30 | 1395 | `				}else{` |
|     15 | 1396 | `					if( SXRET_OK != ph7_array_add_intkey_elem(pArray,(int)(zStart-zIn),pValue) ){` |
|    ! 0 | 1397 | `						rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1398 | `						goto out;` |
|      - | 1399 | `					}` |
|      - | 1400 | `				}` |
|      - | 1401 | `			}` |
|     45 | 1402 | `		}` |
|     97 | 1403 | `		zPtr++;` |
|      1 | 1404 | `	}` |
|     37 | 1405 | `	if( iFormat == 0 ){` |
|     13 | 1406 | `		ph7_result_int(pCtx,nCount);` |
|      7 | 1407 | `	}else{` |
|     25 | 1408 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1409 | `	}` |
|     37 | 1410 | `	rc = PH7_OK;` |
|     24 | 1411 | `out:` |
|     49 | 1412 | `	PH7_MemObjRelease(&sTmp);` |
|     49 | 1413 | `	PH7_MemObjRelease(&sListTmp);` |
|     49 | 1414 | `	return rc;` |
|     26 | 1415 | `}` |
|      - | 1416 | `/*` |
|      - | 1417 | ` * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])` |
|      - | 1418 | ` *   Split a string into smaller chunks.` |
|      - | 1419 | ` * Parameters` |
|      - | 1420 | ` *  $body` |
|      - | 1421 | ` *   The string to be chunked.` |
|      - | 1422 | ` * $chunklen` |
|      - | 1423 | ` *   The chunk length.` |
|      - | 1424 | ` * $end` |
|      - | 1425 | ` *   The line ending sequence.` |
|      - | 1426 | ` * Return` |
|      - | 1427 | ` *  The chunked string or NULL on failure.` |
|      - | 1428 | ` */` |
|     14 | 1429 | `static int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1430 | `{` |
|     15 | 1431 | `	const char *zIn,*zEnd,*zSep = "\r\n";` |
|      - | 1432 | `	int nSepLen,nChunkLen,nLen;` |
|     15 | 1433 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1434 | `		/* Nothing to split,return null */` |
|      3 | 1435 | `		ph7_result_null(pCtx);` |
|      3 | 1436 | `		return PH7_OK;` |
|      - | 1437 | `	}` |
|      - | 1438 | `	/* initialize/Extract arguments */` |
|     13 | 1439 | `	nSepLen = (int)sizeof("\r\n") - 1;` |
|     13 | 1440 | `	nChunkLen = 76;` |
|     13 | 1441 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 1442 | `	zEnd = &zIn[nLen];` |
|     13 | 1443 | `	if( nArg > 1 ){` |
|      - | 1444 | `		/* Chunk length */` |
|     13 | 1445 | `		nChunkLen = ph7_value_to_int(apArg[1]);` |
|     13 | 1446 | `		if( nChunkLen < 1 ){` |
|      - | 1447 | `			/* PHP 8 throws a catchable ValueError for a non-positive length. */` |
|      3 | 1448 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1449 | `				"chunk_split(): Argument #2 ($length) must be greater than 0");` |
|      - | 1450 | `		}` |
|     11 | 1451 | `		if( nArg > 2 ){` |
|      - | 1452 | `			/* Separator */` |
|      9 | 1453 | `			zSep = ph7_value_to_string(apArg[2],&nSepLen);` |
|      9 | 1454 | `			if( nSepLen < 1 ){` |
|      - | 1455 | `				/* Switch back to the default separator */` |
|      3 | 1456 | `				zSep = "\r\n";` |
|      3 | 1457 | `				nSepLen = (int)sizeof("\r\n") - 1;` |
|      1 | 1458 | `			}` |
|      4 | 1459 | `		}` |
|      5 | 1460 | `	}` |
|      - | 1461 | `	/* Perform the requested operation */` |
|     11 | 1462 | `	if( nChunkLen > nLen ){` |
|      - | 1463 | `		/* Nothing to split,return the string and the separator */` |
|      7 | 1464 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);` |
|      7 | 1465 | `		return PH7_OK;` |
|      - | 1466 | `	}` |
|     17 | 1467 | `	while( zIn < zEnd ){` |
|     13 | 1468 | `		if( nChunkLen > (int)(zEnd-zIn) ){` |
|      3 | 1469 | `			nChunkLen = (int)(zEnd - zIn);` |
|      1 | 1470 | `		}` |
|      - | 1471 | `		/* Append the chunk and the separator */` |
|     13 | 1472 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);` |
|      - | 1473 | `		/* Point beyond the chunk */` |
|     13 | 1474 | `		zIn += nChunkLen;` |
|      1 | 1475 | `	}` |
|      5 | 1476 | `	return PH7_OK;` |
|      8 | 1477 | `}` |
|      - | 1478 | `/*` |
|      - | 1479 | ` * string addslashes(string $str)` |
|      - | 1480 | ` *  Quote string with slashes.` |
|      - | 1481 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1482 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1483 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1484 | ` * Parameter` |
|      - | 1485 | ` *  str: The string to be escaped.` |
|      - | 1486 | ` * Return` |
|      - | 1487 | ` *  Returns the escaped string` |
|      - | 1488 | ` */` |
|     24 | 1489 | `static int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1490 | `{` |
|      - | 1491 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1492 | `	int nLen;` |
|      - | 1493 | `	/* PHP enforces exactly one argument. */` |
|     28 | 1494 | `	if( nArg != 1 ){` |
|      8 | 1495 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1496 | `			"ArgumentCountError",` |
|      - | 1497 | `			"addslashes() expects exactly 1 argument, %d given",` |
|      2 | 1498 | `			nArg` |
|      - | 1499 | `			);` |
|      - | 1500 | `	}` |
|      - | 1501 | `	/* NULL is deprecated and treated as an empty string; other invalid` |
|      - | 1502 | `	 * types still produce a TypeError. */` |
|     22 | 1503 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 1504 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 1505 | `			E_DEPRECATED,` |
|      - | 1506 | `			"addslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - | 1507 | `			);` |
|      - | 1508 | `		/* fall through so conversion below yields empty string */` |
|      1 | 1509 | `	}` |
|      - | 1510 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     29 | 1511 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     29 | 1512 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     18 | 1513 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 1514 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1515 | `			"TypeError",` |
|      - | 1516 | `			"addslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 1517 | `			ph7_type_name(apArg[0])` |
|      - | 1518 | `			);` |
|      - | 1519 | `	}` |
|      - | 1520 | `	/* Convert to string representation first and obtain length. */` |
|     19 | 1521 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 1522 | `	if( nLen < 1 ){` |
|      - | 1523 | `		/* Return the empty string */` |
|      5 | 1524 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1525 | `		return PH7_OK;` |
|      - | 1526 | `	}` |
|     15 | 1527 | `	zEnd = &zIn[nLen];` |
|     15 | 1528 | `	zCur = 0; /* cc warning */` |
|     20 | 1529 | `	for(;;){` |
|     41 | 1530 | `		if( zIn >= zEnd ){` |
|      - | 1531 | `			/* No more input */` |
|     15 | 1532 | `			break;` |
|      - | 1533 | `		}` |
|     27 | 1534 | `		zCur = zIn;` |
|      - | 1535 | `		/* scan until a character that needs escaping (', ", \\, or NUL) */` |
|     89 | 1536 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' && zIn[0] != '\0' ){` |
|     63 | 1537 | `			zIn++;` |
|      1 | 1538 | `		}` |
|     27 | 1539 | `		if( zIn > zCur ){` |
|      - | 1540 | `			/* Append raw contents */` |
|     23 | 1541 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 1542 | `		}` |
|     27 | 1543 | `		if( zIn < zEnd ){` |
|     17 | 1544 | `			int c = zIn[0];` |
|     17 | 1545 | `			if( c == '\0' ){` |
|      - | 1546 | `				/* PHP escapes NUL as "\\0" (two characters) */` |
|      3 | 1547 | `				ph7_result_string(pCtx,"\\0",2);` |
|      2 | 1548 | `			}else{` |
|     15 | 1549 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1550 | `			}` |
|      8 | 1551 | `		}` |
|     27 | 1552 | `		zIn++;` |
|      1 | 1553 | `	}` |
|     15 | 1554 | `	return PH7_OK;` |
|     16 | 1555 | `}` |
|      - | 1556 | `/*` |
|      - | 1557 | ``  * Build a 256-entry membership mask from a PHP charlist, expanding `a..z` `` |
|      - | 1558 | ` * byte ranges exactly like PHP's php_charmask(). On return aMask[c] != 0 iff` |
|      - | 1559 | ` * the byte c belongs to the set. Emits the PHP-exact warnings for the three` |
|      - | 1560 | ` * malformed-range shapes (ph7_context_throw_error_format prepends the active` |
|      - | 1561 | ` * function name, so the messages omit it); on a bad range the surrounding` |
|      - | 1562 | ` * bytes are still added and the scan never aborts. Reads only within` |
|      - | 1563 | ` * [zList, zList+nLen).` |
|      - | 1564 | ` *` |
|      - | 1565 | ` * Use ONLY for the builtins whose charlist expands ranges the way PHP's` |
|      - | 1566 | ` * php_charmask() does: trim/ltrim/rtrim/addcslashes (and quotemeta, whose set` |
|      - | 1567 | ` * is a fixed literal with no ".."). Do NOT route strspn/strcspn/strtok/strpbrk` |
|      - | 1568 | ` * through this — PHP treats their charlists literally, so expanding "a..z" here` |
|      - | 1569 | ` * would be a behavior regression plus spurious "Invalid '..'-range" warnings.` |
|      - | 1570 | ` */` |
|    100 | 1571 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256])` |
|      4 | 1572 | `{` |
|    104 | 1573 | `	const unsigned char *zIn  = (const unsigned char *)zList;` |
|    104 | 1574 | `	const unsigned char *zEnd = zIn + (nLen > 0 ? nLen : 0);` |
|    104 | 1575 | `	SyZero(aMask,256);` |
|    366 | 1576 | `	for( ; zIn < zEnd ; zIn++ ){` |
|    266 | 1577 | `		int c = zIn[0];` |
|    266 | 1578 | `		if( zIn + 3 < zEnd && zIn[1] == '.' && zIn[2] == '.' && zIn[3] >= c ){` |
|      - | 1579 | `			/* Valid incrementing range c..zIn[3] */` |
|     22 | 1580 | `			int hi = zIn[3],k;` |
|    386 | 1581 | `			for( k = c ; k <= hi ; k++ ){` |
|    366 | 1582 | `				aMask[k] = 1;` |
|    184 | 1583 | `			}` |
|     22 | 1584 | `			zIn += 3; /* the loop's ++ then steps past the range end */` |
|    265 | 1585 | `		}else if( zIn + 1 < zEnd && zIn[0] == '.' && zIn[1] == '.' ){` |
|      - | 1586 | `			/* Malformed range: mirror php_charmask's three diagnostics. */` |
|      - | 1587 | `			const char *zMsg;` |
|     20 | 1588 | `			if( (const unsigned char *)zList >= zIn ){` |
|      6 | 1589 | `				zMsg = "no character to the left of '..'";` |
|     18 | 1590 | `			}else if( zIn + 2 >= zEnd ){` |
|      6 | 1591 | `				zMsg = "no character to the right of '..'";` |
|     14 | 1592 | `			}else if( zIn[-1] > zIn[2] ){` |
|     12 | 1593 | `				zMsg = "'..'-range needs to be incrementing";` |
|      7 | 1594 | `			}else{` |
|    ! 0 | 1595 | `				zMsg = 0; /* catch-all (e.g. a..b..c) */` |
|      - | 1596 | `			}` |
|     20 | 1597 | `			if( zMsg ){` |
|     29 | 1598 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      9 | 1599 | `					"Invalid '..'-range, %s",zMsg);` |
|     11 | 1600 | `			}else{` |
|    ! 0 | 1601 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1602 | `					"Invalid '..'-range");` |
|      - | 1603 | `			}` |
|      - | 1604 | `			/* Do not consume the dots: the loop's ++ steps one byte so the` |
|      - | 1605 | `			 * dots are re-scanned as literals, exactly like php_charmask. */` |
|     11 | 1606 | `		}else{` |
|    228 | 1607 | `			aMask[c] = 1;` |
|      - | 1608 | `		}` |
|    135 | 1609 | `	}` |
|    104 | 1610 | `}` |
|      - | 1611 | `/*` |
|      - | 1612 | ` * string addcslashes(string $str,string $charlist)` |
|      - | 1613 | ` *  Quote string with slashes in a C style.` |
|      - | 1614 | ` * Parameter` |
|      - | 1615 | ` *  $str:` |
|      - | 1616 | ` *    The string to be escaped.` |
|      - | 1617 | ` *  $charlist:` |
|      - | 1618 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|      - | 1619 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|      - | 1620 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|      - | 1621 | ` * Return` |
|      - | 1622 | ` *  Returns the escaped string.` |
|      - | 1623 | ` * Note:` |
|      - | 1624 | ` *  Character ranges [i.e: 'A..Z'] are supported (see PH7_BuildCharMask).` |
|      - | 1625 | ` */` |
|     40 | 1626 | `static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1627 | `{` |
|      - | 1628 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - | 1629 | `	char aMask[256];` |
|      - | 1630 | `	int nLen,nMask;` |
|      - | 1631 | `	/* PHP enforces exactly two arguments. */` |
|     45 | 1632 | `	if( nArg != 2 ){` |
|      8 | 1633 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1634 | `			"ArgumentCountError",` |
|      - | 1635 | `			"addcslashes() expects exactly 2 arguments, %d given",` |
|      2 | 1636 | `			nArg` |
|      - | 1637 | `			);` |
|      - | 1638 | `	}` |
|      - | 1639 | `	/* First argument must be a string-ish value.  NULL is deprecated and` |
|      - | 1640 | `	 * treated as the empty string (PHP 8.1). */` |
|     40 | 1641 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      - | 1642 | `		/* Emit deprecation only once, similar to PHP behaviour. */` |
|      3 | 1643 | `		PH7_VmThrowError(pCtx->pVm,0,/* iErr will be patched to 8192 below */` |
|      - | 1644 | `			E_DEPRECATED,` |
|      - | 1645 | `			"addcslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - | 1646 | `			);` |
|      - | 1647 | `		/* treat as empty string; fall through to conversion logic */` |
|     52 | 1648 | `	} else if( ph7_value_is_array(apArg[0]) \|\|` |
|     52 | 1649 | `	          ph7_value_is_object(apArg[0]) \|\|` |
|     32 | 1650 | `	          ph7_value_is_resource(apArg[0]) ){` |
|      4 | 1651 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1652 | `			"TypeError",` |
|      - | 1653 | `			"addcslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 1654 | `			ph7_type_name(apArg[0])` |
|      - | 1655 | `			);` |
|      - | 1656 | `	}` |
|      - | 1657 | `	/* Second argument must be a string.  NULL is deprecated and treated as` |
|      - | 1658 | `	 * an empty mask per PHP semantics.  Arrays/objects/resources still` |
|      - | 1659 | `	 * trigger a TypeError. */` |
|     38 | 1660 | `	if( ph7_value_is_null(apArg[1]) ){` |
|      3 | 1661 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 1662 | `			E_DEPRECATED,` |
|      - | 1663 | `			"addcslashes(): Passing null to parameter #2 ($characters) of type string is deprecated"` |
|      - | 1664 | `			);` |
|      - | 1665 | `		/* allow through so it becomes empty string below */` |
|     49 | 1666 | `	} else if( ph7_value_is_array(apArg[1]) \|\|` |
|     49 | 1667 | `	          ph7_value_is_object(apArg[1]) \|\|` |
|     30 | 1668 | `	          ph7_value_is_resource(apArg[1]) ){` |
|      4 | 1669 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1670 | `			"TypeError",` |
|      - | 1671 | `			"addcslashes(): Argument #2 ($characters) must be of type string, %s given",` |
|      2 | 1672 | `			ph7_type_name(apArg[1])` |
|      - | 1673 | `			);` |
|      - | 1674 | `	}` |
|      - | 1675 | `	/* Extract the string to process */` |
|     35 | 1676 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1677 | `	/* NULL would never reach here due to the check above. */` |
|     35 | 1678 | `	if( nLen < 1 ){` |
|      - | 1679 | `		/* Empty string returns itself. */` |
|      5 | 1680 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      5 | 1681 | `		return PH7_OK;` |
|      - | 1682 | `	}` |
|      - | 1683 | ``	/* Extract the desired mask and expand any `a..z` ranges into a lookup. */`` |
|     31 | 1684 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|     31 | 1685 | `	PH7_BuildCharMask(pCtx,zMask,nMask,aMask);` |
|     31 | 1686 | `	zEnd = &zIn[nLen];` |
|     31 | 1687 | `	zCur = 0; /* cc warning */` |
|     37 | 1688 | `	for(;;){` |
|     77 | 1689 | `		if( zIn >= zEnd ){` |
|      - | 1690 | `			/* No more input */` |
|     31 | 1691 | `			break;` |
|      - | 1692 | `		}` |
|     49 | 1693 | `		zCur = zIn;` |
|    125 | 1694 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     79 | 1695 | `			zIn++;` |
|      3 | 1696 | `		}` |
|     49 | 1697 | `		if( zIn > zCur ){` |
|      - | 1698 | `			/* Append raw contents */` |
|     43 | 1699 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     20 | 1700 | `		}` |
|     49 | 1701 | `		if( zIn < zEnd ){` |
|      - | 1702 | `			/* Make sure we treat the byte as unsigned to avoid negative values` |
|      - | 1703 | `			 * on platforms where char is signed. */` |
|     29 | 1704 | `			int c = (unsigned char)zIn[0];` |
|      - | 1705 | `			/* Handle special C-like escapes for common control characters first.` |
|      - | 1706 | `			 * PHP outputs "\n" "\r" "\t" "\v" "\f" when those chars are` |
|      - | 1707 | `			 * in the mask. NUL is left to the octal conversion below. */` |
|     29 | 1708 | `			if( c == '\n' ){` |
|      3 | 1709 | `				ph7_result_string(pCtx,"\\n",2);` |
|     28 | 1710 | `			}else if( c == '\r' ){` |
|      3 | 1711 | `				ph7_result_string(pCtx,"\\r",2);` |
|     26 | 1712 | `			}else if( c == '\t' ){` |
|      3 | 1713 | `				ph7_result_string(pCtx,"\\t",2);` |
|     24 | 1714 | `			}else if( c == '\v' ){` |
|      3 | 1715 | `				ph7_result_string(pCtx,"\\v",2);` |
|     22 | 1716 | `			}else if( c == '\f' ){` |
|      3 | 1717 | `				ph7_result_string(pCtx,"\\f",2);` |
|     20 | 1718 | `			}else if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - | 1719 | `				/* Convert to octal.  PHP always emits three-digit zero-padded` |
|      - | 1720 | `				 * octal escapes (\001 not \1). */` |
|      7 | 1721 | `				ph7_result_string_format(pCtx,"\\%03o",c);` |
|      4 | 1722 | `			}else{` |
|     13 | 1723 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1724 | `			}` |
|     13 | 1725 | `		}` |
|     49 | 1726 | `		zIn++;` |
|      3 | 1727 | `	}` |
|     31 | 1728 | `	return PH7_OK;` |
|     25 | 1729 | `}` |
|      - | 1730 | `/*` |
|      - | 1731 | ` * string quotemeta(string $str)` |
|      - | 1732 | ` *  Quote meta characters.` |
|      - | 1733 | ` * Parameter` |
|      - | 1734 | ` *  $str:` |
|      - | 1735 | ` *    The string to be escaped.` |
|      - | 1736 | ` * Return` |
|      - | 1737 | ` *  Returns the escaped string.` |
|      - | 1738 | `*/` |
|     10 | 1739 | `static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1740 | `{` |
|      - | 1741 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1742 | `	char aMask[256];` |
|      - | 1743 | `	int nLen;` |
|     12 | 1744 | `	if( nArg < 1 ){` |
|      - | 1745 | `		/* Nothing to process,retun NULL */` |
|    ! 0 | 1746 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1747 | `		return PH7_OK;` |
|      - | 1748 | `	}` |
|      - | 1749 | `	/* Extract the string to process */` |
|     12 | 1750 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     12 | 1751 | `	if( nLen < 1 ){` |
|      - | 1752 | `		/* Return the empty string */` |
|      3 | 1753 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1754 | `		return PH7_OK;` |
|      - | 1755 | `	}` |
|      - | 1756 | `	/* Fixed meta-character set (no ranges); build the lookup once. */` |
|     10 | 1757 | `	PH7_BuildCharMask(pCtx,".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1,aMask);` |
|     10 | 1758 | `	zEnd = &zIn[nLen];` |
|     10 | 1759 | `	zCur = 0; /* cc warning */` |
|     22 | 1760 | `	for(;;){` |
|     46 | 1761 | `		if( zIn >= zEnd ){` |
|      - | 1762 | `			/* No more input */` |
|     10 | 1763 | `			break;` |
|      - | 1764 | `		}` |
|     38 | 1765 | `		zCur = zIn;` |
|     76 | 1766 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     40 | 1767 | `			zIn++;` |
|      2 | 1768 | `		}` |
|     38 | 1769 | `		if( zIn > zCur ){` |
|      - | 1770 | `			/* Append raw contents */` |
|     20 | 1771 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      9 | 1772 | `		}` |
|     38 | 1773 | `		if( zIn < zEnd ){` |
|     36 | 1774 | `			int c = zIn[0];` |
|     36 | 1775 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     17 | 1776 | `		}` |
|     38 | 1777 | `		zIn++;` |
|      2 | 1778 | `	}` |
|     10 | 1779 | `	return PH7_OK;` |
|      7 | 1780 | `}` |
|      - | 1781 | `/*` |
|      - | 1782 | ` * string stripslashes(string $str)` |
|      - | 1783 | ` *  Un-quotes a quoted string.` |
|      - | 1784 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1785 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1786 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1787 | ` * Parameter` |
|      - | 1788 | ` *  $str` |
|      - | 1789 | ` *   The input string.` |
|      - | 1790 | ` * Return` |
|      - | 1791 | ` *  Returns a string with backslashes stripped off.` |
|      - | 1792 | ` */` |
|      6 | 1793 | `static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1794 | `{` |
|      - | 1795 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1796 | `	int nLen;` |
|      7 | 1797 | `	if( nArg < 1 ){` |
|      - | 1798 | `		/* Nothing to process,retun NULL */` |
|    ! 0 | 1799 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1800 | `		return PH7_OK;` |
|      - | 1801 | `	}` |
|      - | 1802 | `	/* Extract the string to process */` |
|      7 | 1803 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 1804 | `	if( zIn == 0 ){` |
|    ! 0 | 1805 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1806 | `		return PH7_OK;` |
|      - | 1807 | `	}` |
|      7 | 1808 | `	zEnd = &zIn[nLen];` |
|      7 | 1809 | `	zCur = 0; /* cc warning */` |
|      - | 1810 | `	/* Encode the string */` |
|      4 | 1811 | `	for(;;){` |
|      9 | 1812 | `		if( zIn >= zEnd ){` |
|      - | 1813 | `			/* No more input */` |
|      5 | 1814 | `			break;` |
|      - | 1815 | `		}` |
|      5 | 1816 | `		zCur = zIn;` |
|     17 | 1817 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 | 1818 | `			zIn++;` |
|      1 | 1819 | `		}` |
|      5 | 1820 | `		if( zIn > zCur ){` |
|      - | 1821 | `			/* Append raw contents */` |
|      5 | 1822 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 1823 | `		}` |
|      5 | 1824 | `		if( &zIn[1] < zEnd ){` |
|      3 | 1825 | `			int c = zIn[1];` |
|      3 | 1826 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - | 1827 | `				/* Ignore the backslash */` |
|      3 | 1828 | `				zIn++;` |
|      1 | 1829 | `			}` |
|      2 | 1830 | `		}else{` |
|      3 | 1831 | `			break;` |
|      - | 1832 | `		}` |
|      1 | 1833 | `	}` |
|      7 | 1834 | `	return PH7_OK;` |
|      4 | 1835 | `}` |
|      - | 1836 | `/*` |
|      - | 1837 | ` * UTF-8-aware HTML entity machinery, shared by htmlspecialchars/htmlentities/` |
|      - | 1838 | ` * htmlspecialchars_decode/html_entity_decode/get_html_translation_table.` |
|      - | 1839 | ` * The implementations live further down in this file, next to the filter_var` |
|      - | 1840 | ` * FULL_SPECIAL_CHARS machinery they reuse (aHtml401Ent[]/FvHtml401Lookup()/` |
|      - | 1841 | ` * FvUtf8Next()). Semantics are byte-exact vs php 8.5.7; PHL is UTF-8-only` |
|      - | 1842 | ` * so every charset argument other than a UTF-8 alias gets PHP's` |
|      - | 1843 | ` * unsupported-charset warning and is treated as UTF-8.` |
|      - | 1844 | ` *` |
|      - | 1845 | ` * Flag model (the PHP-exact ENT_* values, see constant.c): bit 1 = encode/` |
|      - | 1846 | ` * decode single quotes, bit 2 = double quotes (ENT_QUOTES=3, ENT_COMPAT=2,` |
|      - | 1847 | ` * ENT_NOQUOTES=0); bits 16\|32 select the doctype (0=HTML401, 16=XML1,` |
|      - | 1848 | ` * 32=XHTML, 48=HTML5); ENT_IGNORE=4 drops invalid UTF-8 bytes (wins over` |
|      - | 1849 | ` * ENT_SUBSTITUTE=8, which replaces each with U+FFFD; with neither set the` |
|      - | 1850 | ` * whole result collapses to ""); ENT_DISALLOWED=128 substitutes valid but` |
|      - | 1851 | ` * doctype-disallowed codepoints. The shared default is` |
|      - | 1852 | ` * ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 = 11.` |
|      - | 1853 | ` */` |
|      - | 1854 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bAll,int bDoubleEncode);` |
|      - | 1855 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bFull);` |
|      - | 1856 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx);` |
|      - | 1857 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags);` |
|      - | 1858 | `/*` |
|      - | 1859 | ` * string htmlspecialchars(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1860 | ` *                         [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1861 | ` *  Convert the special characters & < > " ' to HTML entities.` |
|      - | 1862 | ` * Return` |
|      - | 1863 | ` *  The escaped string or NULL on failure.` |
|      - | 1864 | ` */` |
|     42 | 1865 | `static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1866 | `{` |
|     43 | 1867 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1868 | `	const char *zIn;` |
|     43 | 1869 | `	int nLen,bDouble = 1;` |
|     43 | 1870 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1871 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 1872 | `		ph7_result_null(pCtx);` |
|      3 | 1873 | `		return PH7_OK;` |
|      - | 1874 | `	}` |
|      - | 1875 | `	/* Extract the target string */` |
|     41 | 1876 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 1877 | `	if( nArg > 1 ){` |
|     35 | 1878 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     17 | 1879 | `	}` |
|     41 | 1880 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     41 | 1881 | `	if( nArg > 3 ){` |
|      7 | 1882 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      3 | 1883 | `	}` |
|     41 | 1884 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,0,bDouble);` |
|     41 | 1885 | `	return PH7_OK;` |
|     22 | 1886 | `}` |
|      - | 1887 | `/*` |
|      - | 1888 | ` * string htmlspecialchars_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401])` |
|      - | 1889 | ` *  Convert the special HTML entities (&amp; &lt; &gt; &quot; and the` |
|      - | 1890 | ` *  numeric/doctype forms of the two quotes) back to characters.` |
|      - | 1891 | ` * Return` |
|      - | 1892 | ` *  The unescaped string or NULL on failure.` |
|      - | 1893 | ` */` |
|     22 | 1894 | `static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1895 | `{` |
|     23 | 1896 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1897 | `	const char *zIn;` |
|      - | 1898 | `	int nLen;` |
|     23 | 1899 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1900 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 1901 | `		ph7_result_null(pCtx);` |
|      3 | 1902 | `		return PH7_OK;` |
|      - | 1903 | `	}` |
|      - | 1904 | `	/* Extract the target string */` |
|     21 | 1905 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     21 | 1906 | `	if( nArg > 1 ){` |
|      9 | 1907 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1908 | `	}` |
|     21 | 1909 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,0);` |
|     21 | 1910 | `	return PH7_OK;` |
|     12 | 1911 | `}` |
|      - | 1912 | `/*` |
|      - | 1913 | ` * array get_html_translation_table(int $table = HTML_SPECIALCHARS` |
|      - | 1914 | ` *      [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 [, string $encoding = "UTF-8"]])` |
|      - | 1915 | ` *  Return the translation table used by htmlspecialchars() (HTML_SPECIALCHARS)` |
|      - | 1916 | ` *  or htmlentities() (HTML_ENTITIES) as character => entity pairs.` |
|      - | 1917 | ` * Return` |
|      - | 1918 | ` *  The translation table as an array or NULL on failure.` |
|      - | 1919 | ` */` |
|     12 | 1920 | `static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1921 | `{` |
|     13 | 1922 | `	int iTable = 0; /* HTML_SPECIALCHARS */` |
|     13 | 1923 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|     13 | 1924 | `	if( nArg > 0 ){` |
|     11 | 1925 | `		iTable = ph7_value_to_int(apArg[0]);` |
|      5 | 1926 | `	}` |
|     13 | 1927 | `	if( nArg > 1 ){` |
|      9 | 1928 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1929 | `	}` |
|     13 | 1930 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     13 | 1931 | `	HtmlTranslationTable(pCtx,iTable,iFlags);` |
|     13 | 1932 | `	return PH7_OK;` |
|      1 | 1933 | `}` |
|      - | 1934 | `/*` |
|      - | 1935 | ` * string htmlentities(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1936 | ` *                     [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1937 | ` *  Convert all applicable characters to HTML entities: the specials plus` |
|      - | 1938 | ` *  every codepoint with an HTML 4.01 named entity (aHtml401Ent[]).` |
|      - | 1939 | ` * Return` |
|      - | 1940 | ` *  The encoded string or NULL on failure.` |
|      - | 1941 | ` */` |
|     30 | 1942 | `static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1943 | `{` |
|     31 | 1944 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1945 | `	const char *zIn;` |
|     31 | 1946 | `	int nLen,bDouble = 1;` |
|     31 | 1947 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1948 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1949 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1950 | `		return PH7_OK;` |
|      - | 1951 | `	}` |
|      - | 1952 | `	/* Extract the target string */` |
|     31 | 1953 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 1954 | `	if( nArg > 1 ){` |
|     19 | 1955 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1956 | `	}` |
|     31 | 1957 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     31 | 1958 | `	if( nArg > 3 ){` |
|      3 | 1959 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      1 | 1960 | `	}` |
|     31 | 1961 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,1,bDouble);` |
|     31 | 1962 | `	return PH7_OK;` |
|     16 | 1963 | `}` |
|      - | 1964 | `/*` |
|      - | 1965 | ` * string html_entity_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1966 | ` *                           [, string $encoding = "UTF-8"]])` |
|      - | 1967 | ` *  Convert HTML entities (named — case-sensitive — and numeric, decimal or` |
|      - | 1968 | ` *  hex) back to their UTF-8 characters. The reverse of htmlentities().` |
|      - | 1969 | ` * Return` |
|      - | 1970 | ` *  The decoded string or NULL on failure.` |
|      - | 1971 | ` */` |
|     58 | 1972 | `static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1973 | `{` |
|     59 | 1974 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1975 | `	const char *zIn;` |
|      - | 1976 | `	int nLen;` |
|     59 | 1977 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1978 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1979 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1980 | `		return PH7_OK;` |
|      - | 1981 | `	}` |
|      - | 1982 | `	/* Extract the target string */` |
|     59 | 1983 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     59 | 1984 | `	if( nArg > 1 ){` |
|     27 | 1985 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     13 | 1986 | `	}` |
|     59 | 1987 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     59 | 1988 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,1);` |
|     59 | 1989 | `	return PH7_OK;` |
|     30 | 1990 | `}` |
|      - | 1991 | `/*` |
|      - | 1992 | ` * int strlen($string)` |
|      - | 1993 | ` *  return the length of the given string.` |
|      - | 1994 | ` * Parameter` |
|      - | 1995 | ` *  string: The string being measured for length.` |
|      - | 1996 | ` * Return` |
|      - | 1997 | ` *  length of the given string.` |
|      - | 1998 | ` */` |
|  12950 | 1999 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2000 | `{` |
|  12955 | 2001 | `	int iLen = 0;` |
|  12955 | 2002 | `	if( nArg > 0 ){` |
|  12955 | 2003 | `		StrNullArgNotice(pCtx,apArg[0],"strlen",1,"$string");` |
|  12955 | 2004 | `		ph7_value_to_string(apArg[0],&iLen);` |
|   6475 | 2005 | `	}` |
|      - | 2006 | `	/* String length */` |
|  12955 | 2007 | `	ph7_result_int(pCtx,iLen);` |
|  12955 | 2008 | `	return PH7_OK;` |
|      5 | 2009 | `}` |
|      - | 2010 | `/*` |
|      - | 2011 | ` * int strcmp(string $str1,string $str2)` |
|      - | 2012 | ` *  Perform a binary safe string comparison.` |
|      - | 2013 | ` * Parameter` |
|      - | 2014 | ` *  str1: The first string` |
|      - | 2015 | ` *  str2: The second string` |
|      - | 2016 | ` * Return` |
|      - | 2017 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2018 | ` *  than str2, and 0 if they are equal.` |
|      - | 2019 | ` */` |
|     72 | 2020 | `static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2021 | `{` |
|      - | 2022 | `	const char *z1,*z2;` |
|      - | 2023 | `	int n1,n2;` |
|      - | 2024 | `	int res;` |
|     73 | 2025 | `	if( nArg < 2 ){` |
|    ! 0 | 2026 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 2027 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 2028 | `		return PH7_OK;` |
|      - | 2029 | `	}` |
|      - | 2030 | `	/* Perform the comparison */` |
|     73 | 2031 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     73 | 2032 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     73 | 2033 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2034 | `	/* Comparison result */` |
|     73 | 2035 | `	ph7_result_int(pCtx,res);` |
|     73 | 2036 | `	return PH7_OK;` |
|     37 | 2037 | `}` |
|      - | 2038 | `/*` |
|      - | 2039 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 2040 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 2041 | ` * Parameter` |
|      - | 2042 | ` *  str1: The first string` |
|      - | 2043 | ` *  str2: The second string` |
|      - | 2044 | ` * Return` |
|      - | 2045 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2046 | ` *  than str2, and 0 if they are equal.` |
|      - | 2047 | ` */` |
|     18 | 2048 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2049 | `{` |
|      - | 2050 | `	const char *z1,*z2;` |
|      - | 2051 | `	int res;` |
|      - | 2052 | `	int n;` |
|     19 | 2053 | `	if( nArg < 3 ){` |
|      - | 2054 | `		/* Perform a standard comparison */` |
|    ! 0 | 2055 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 2056 | `	}` |
|      - | 2057 | `	/* Desired comparison length */` |
|     19 | 2058 | `	n  = ph7_value_to_int(apArg[2]);` |
|     19 | 2059 | `	if( n < 0 ){` |
|      - | 2060 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 2061 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2062 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 2063 | `			ph7_function_name(pCtx));` |
|      - | 2064 | `	}` |
|      - | 2065 | `	/* Perform the comparison */` |
|     17 | 2066 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     17 | 2067 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     17 | 2068 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 2069 | `	/* Comparison result */` |
|     17 | 2070 | `	ph7_result_int(pCtx,res);` |
|     17 | 2071 | `	return PH7_OK;` |
|     10 | 2072 | `}` |
|      - | 2073 | `/*` |
|      - | 2074 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 2075 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 2076 | ` * Parameter` |
|      - | 2077 | ` *  str1: The first string` |
|      - | 2078 | ` *  str2: The second string` |
|      - | 2079 | ` * Return` |
|      - | 2080 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2081 | ` *  than str2, and 0 if they are equal.` |
|      - | 2082 | ` */` |
|    140 | 2083 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2084 | `{` |
|      - | 2085 | `	const char *z1,*z2;` |
|      - | 2086 | `	int n1,n2;` |
|      - | 2087 | `	int res;` |
|    141 | 2088 | `	if( nArg < 2 ){` |
|    ! 0 | 2089 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 2090 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 2091 | `		return PH7_OK;` |
|      - | 2092 | `	}` |
|      - | 2093 | `	/* Perform the comparison */` |
|    141 | 2094 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|    141 | 2095 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|    141 | 2096 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2097 | `	/* Comparison result */` |
|    141 | 2098 | `	ph7_result_int(pCtx,res);` |
|    141 | 2099 | `	return PH7_OK;` |
|     71 | 2100 | `}` |
|      - | 2101 | `/*` |
|      - | 2102 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 2103 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 2104 | ` * Parameter` |
|      - | 2105 | ` *  $str1: The first string` |
|      - | 2106 | ` *  $str2: The second string` |
|      - | 2107 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 2108 | ` * Return` |
|      - | 2109 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2110 | ` *  than str2, and 0 if they are equal.` |
|      - | 2111 | ` */` |
|     42 | 2112 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2113 | `{` |
|      - | 2114 | `	const char *z1,*z2;` |
|      - | 2115 | `	int res;` |
|      - | 2116 | `	int n;` |
|     47 | 2117 | `	if( nArg < 3 ){` |
|      - | 2118 | `		/* Perform a standard comparison */` |
|    ! 0 | 2119 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 2120 | `	}` |
|      - | 2121 | `	/* Desired comparison length */` |
|     47 | 2122 | `	n  = ph7_value_to_int(apArg[2]);` |
|     47 | 2123 | `	if( n < 0 ){` |
|      - | 2124 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 2125 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2126 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 2127 | `			ph7_function_name(pCtx));` |
|      - | 2128 | `	}` |
|      - | 2129 | `	/* Perform the comparison */` |
|     45 | 2130 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     45 | 2131 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     45 | 2132 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 2133 | `	/* Comparison result */` |
|     45 | 2134 | `	ph7_result_int(pCtx,res);` |
|     45 | 2135 | `	return PH7_OK;` |
|     26 | 2136 | `}` |
|      - | 2137 | `/*` |
|      - | 2138 | ` * Implode context [i.e: it's private data].` |
|      - | 2139 | ` * A pointer to the following structure is forwarded` |
|      - | 2140 | ` * verbatim to the array walker callback defined below.` |
|      - | 2141 | ` */` |
|      - | 2142 | `struct implode_data {` |
|      - | 2143 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 2144 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 2145 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 2146 | `	int nSeplen;          /* Separator length */` |
|      - | 2147 | `	int bFirst;           /* TRUE if first call */` |
|      - | 2148 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 2149 | `	sxi32 rc;             /* Captured allocation rc; SXERR_MEM => the builtin raises an OOM fatal */` |
|      - | 2150 | `};` |
|      - | 2151 | `/*` |
|      - | 2152 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 2153 | ` * The following routine is invoked for each array entry passed` |
|      - | 2154 | ` * to the implode() function.` |
|      - | 2155 | ` */` |
| 146912 | 2156 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 2157 | `{` |
|  73456 | 2158 | `	SXUNUSED(pKey);` |
| 146917 | 2159 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 2160 | `	const char *zData;` |
|      - | 2161 | `	int nLen;` |
| 146917 | 2162 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 2163 | `		if( pData->nSeplen > 0 ){` |
|      3 | 2164 | `			if( !pData->bFirst ){` |
|      - | 2165 | `				/* append the separator first */` |
|      3 | 2166 | `				if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 2167 | `					pData->rc = SXERR_MEM;` |
|    ! 0 | 2168 | `					return PH7_ABORT;` |
|      - | 2169 | `				}` |
|      2 | 2170 | `			}else{` |
|    ! 0 | 2171 | `				pData->bFirst = 0;` |
|      - | 2172 | `			}` |
|      1 | 2173 | `		}` |
|      - | 2174 | `		/* Recurse */` |
|      3 | 2175 | `		pData->bFirst = 1;` |
|      3 | 2176 | `		pData->nRecCount++;` |
|      3 | 2177 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 2178 | `		pData->nRecCount--;` |
|      - | 2179 | `		/* Propagate an allocation failure surfaced deeper in the recursion. */` |
|      3 | 2180 | `		if( pData->rc != SXRET_OK ){` |
|    ! 0 | 2181 | `			return PH7_ABORT;` |
|      - | 2182 | `		}` |
|      3 | 2183 | `		return PH7_OK;` |
|      - | 2184 | `	}` |
|      - | 2185 | `	/* Extract the string representation of the entry value */` |
| 146915 | 2186 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 2187 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 146915 | 2188 | `	if( pData->bFirst ){` |
|  33509 | 2189 | `		pData->bFirst = 0;` |
| 130163 | 2190 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 2191 | `		/* append the separator first */` |
| 113395 | 2192 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 2193 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 2194 | `			return PH7_ABORT;` |
|      - | 2195 | `		}` |
|  56695 | 2196 | `	}` |
|      - | 2197 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 146915 | 2198 | `	if( nLen > 0 ){` |
| 134547 | 2199 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2200 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 2201 | `			return PH7_ABORT;` |
|      - | 2202 | `		}` |
|  67271 | 2203 | `	}` |
| 146915 | 2204 | `	return PH7_OK;` |
|  73461 | 2205 | `}` |
|      - | 2206 | `/*` |
|      - | 2207 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 2208 | ` * string implode(array $pieces,...)` |
|      - | 2209 | ` *  Join array elements with a string.` |
|      - | 2210 | ` * $glue` |
|      - | 2211 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 2212 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 2213 | ` * $pieces` |
|      - | 2214 | ` *   The array of strings to implode.` |
|      - | 2215 | ` * Return` |
|      - | 2216 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 2217 | ` *  order, with the glue string between each element.` |
|      - | 2218 | ` */` |
|  33530 | 2219 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2220 | `{` |
|      - | 2221 | `	struct implode_data imp_data;` |
|  33535 | 2222 | `	int i = 1;` |
|  33535 | 2223 | `	if( nArg < 1 ){` |
|      - | 2224 | `		/* Missing argument,return NULL */` |
|    ! 0 | 2225 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2226 | `		return PH7_OK;` |
|      - | 2227 | `	}` |
|      - | 2228 | `	/* Prepare the implode context */` |
|  33535 | 2229 | `	imp_data.pCtx = pCtx;` |
|  33535 | 2230 | `	imp_data.bRecursive = 0;` |
|  33535 | 2231 | `	imp_data.bFirst = 1;` |
|  33535 | 2232 | `	imp_data.nRecCount = 0;` |
|  33535 | 2233 | `	imp_data.rc = SXRET_OK;` |
|  33535 | 2234 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  33533 | 2235 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  16769 | 2236 | `	}else{` |
|      3 | 2237 | `		imp_data.zSep = 0;` |
|      3 | 2238 | `		imp_data.nSeplen = 0;` |
|      3 | 2239 | `		i = 0;` |
|      - | 2240 | `	}` |
|  33535 | 2241 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2242 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2243 | `	}` |
|      - | 2244 | `	/* Start the 'join' process */` |
|  67065 | 2245 | `	while( i < nArg ){` |
|  33535 | 2246 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2247 | `			/* Iterate throw array entries */` |
|  33535 | 2248 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2249 | `			/* Surface a callback allocation failure as a fatal */` |
|  33535 | 2250 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2251 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2252 | `			}` |
|  16770 | 2253 | `		}else{` |
|      - | 2254 | `			const char *zData;` |
|      - | 2255 | `			int nLen;` |
|      - | 2256 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 2257 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2258 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 2259 | `			if( imp_data.bFirst ){` |
|    ! 0 | 2260 | `				imp_data.bFirst = 0;` |
|    ! 0 | 2261 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2262 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2263 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2264 | `				}` |
|    ! 0 | 2265 | `			}` |
|      - | 2266 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 2267 | `			if( nLen > 0 ){` |
|    ! 0 | 2268 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2269 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2270 | `				}` |
|    ! 0 | 2271 | `			}` |
|      - | 2272 | `		}` |
|  33535 | 2273 | `		i++;` |
|      5 | 2274 | `	}` |
|  33535 | 2275 | `	return PH7_OK;` |
|  16770 | 2276 | `}` |
|      - | 2277 | `/*` |
|      - | 2278 | ` * Symisc eXtension:` |
|      - | 2279 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 2280 | ` * Purpose` |
|      - | 2281 | ` *  Same as implode() but recurse on arrays.` |
|      - | 2282 | ` * Example:` |
|      - | 2283 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 2284 | ` *   echo implode_recursive("/",$a);` |
|      - | 2285 | ` *   Will output` |
|      - | 2286 | ` *     usr/home/dean.` |
|      - | 2287 | ` *   While the standard implode would produce.` |
|      - | 2288 | ` *    usr/Array.` |
|      - | 2289 | ` * Parameter` |
|      - | 2290 | ` *  Refer to implode().` |
|      - | 2291 | ` * Return` |
|      - | 2292 | ` *  Refer to implode().` |
|      - | 2293 | ` */` |
|     12 | 2294 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2295 | `{` |
|      - | 2296 | `	struct implode_data imp_data;` |
|     13 | 2297 | `	int i = 1;` |
|     13 | 2298 | `	if( nArg < 1 ){` |
|      - | 2299 | `		/* Missing argument,return NULL */` |
|      3 | 2300 | `		ph7_result_null(pCtx);` |
|      3 | 2301 | `		return PH7_OK;` |
|      - | 2302 | `	}` |
|      - | 2303 | `	/* Prepare the implode context */` |
|     11 | 2304 | `	imp_data.pCtx = pCtx;` |
|     11 | 2305 | `	imp_data.bRecursive = 1;` |
|     11 | 2306 | `	imp_data.bFirst = 1;` |
|     11 | 2307 | `	imp_data.nRecCount = 0;` |
|     11 | 2308 | `	imp_data.rc = SXRET_OK;` |
|     11 | 2309 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 2310 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 2311 | `	}else{` |
|    ! 0 | 2312 | `		imp_data.zSep = 0;` |
|    ! 0 | 2313 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 2314 | `		i = 0;` |
|      - | 2315 | `	}` |
|     11 | 2316 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2317 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2318 | `	}` |
|      - | 2319 | `	/* Start the 'join' process */` |
|     21 | 2320 | `	while( i < nArg ){` |
|     11 | 2321 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2322 | `			/* Iterate throw array entries */` |
|      3 | 2323 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2324 | `			/* Surface a callback allocation failure as a fatal */` |
|      3 | 2325 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2326 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2327 | `			}` |
|      2 | 2328 | `		}else{` |
|      - | 2329 | `			const char *zData;` |
|      - | 2330 | `			int nLen;` |
|      - | 2331 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 2332 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2333 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 2334 | `			if( imp_data.bFirst ){` |
|      9 | 2335 | `				imp_data.bFirst = 0;` |
|      4 | 2336 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2337 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2338 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2339 | `				}` |
|    ! 0 | 2340 | `			}` |
|      - | 2341 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 2342 | `			if( nLen > 0 ){` |
|      9 | 2343 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2344 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2345 | `				}` |
|      4 | 2346 | `			}` |
|      - | 2347 | `		}` |
|     11 | 2348 | `		i++;` |
|      1 | 2349 | `	}` |
|     11 | 2350 | `	return PH7_OK;` |
|      7 | 2351 | `}` |
|      - | 2352 | `/*` |
|      - | 2353 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 2354 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 2355 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 2356 | ` * Parameters` |
|      - | 2357 | ` *  $delimiter` |
|      - | 2358 | ` *   The boundary string.` |
|      - | 2359 | ` * $string` |
|      - | 2360 | ` *   The input string.` |
|      - | 2361 | ` * $limit` |
|      - | 2362 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 2363 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 2364 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 2365 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 2366 | ` * Returns` |
|      - | 2367 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 2368 | ` *  on boundaries formed by the delimiter.` |
|      - | 2369 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 2370 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 2371 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 2372 | ` *  will be returned.` |
|      - | 2373 | ` * NOTE:` |
|      - | 2374 | ` *  Negative limit is not supported.` |
|      - | 2375 | ` */` |
|   6544 | 2376 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2377 | `{` |
|      - | 2378 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2379 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2380 | `	ph7_value *pArray;` |
|      - | 2381 | `	ph7_value *pValue;` |
|      - | 2382 | `	sxu32 nOfft;` |
|      - | 2383 | `	sxi32 rc;` |
|   6549 | 2384 | `	if( nArg < 2 ){` |
|      - | 2385 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2386 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2387 | `		return PH7_OK;` |
|      - | 2388 | `	}` |
|      - | 2389 | `	/* Extract the delimiter */` |
|   6549 | 2390 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   6549 | 2391 | `	if( nDelim < 1 ){` |
|      - | 2392 | `		/* Empty delimiter: PHP 8 throws a catchable ValueError. */` |
|      5 | 2393 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2394 | `			"explode(): Argument #1 ($separator) must not be empty");` |
|      - | 2395 | `	}` |
|      - | 2396 | `	/* Extract the string */` |
|   6545 | 2397 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   6545 | 2398 | `	if( nStrlen < 1 ){` |
|      - | 2399 | `		/* Empty string: normally an array with a single empty element (PHP behavior).` |
|      - | 2400 | `		 * A negative limit drops the last -limit components, so the sole empty` |
|      - | 2401 | `		 * component is dropped and the result is an empty array. */` |
|      7 | 2402 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      7 | 2403 | `		if( pArrayTmp == 0 ){` |
|      - | 2404 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 2405 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2406 | `			return PH7_OK;` |
|      - | 2407 | `		}` |
|      7 | 2408 | `		if( !(nArg > 2 && ph7_value_to_int(apArg[2]) < 0) ){` |
|      5 | 2409 | `			ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      5 | 2410 | `			if( pValueTmp == 0 ){` |
|      - | 2411 | `				/* Out of memory,return FALSE */` |
|    ! 0 | 2412 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 2413 | `				return PH7_OK;` |
|      - | 2414 | `			}` |
|      5 | 2415 | `			ph7_value_string(pValueTmp, "", 0);` |
|      5 | 2416 | `			if( ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp) != SXRET_OK ){` |
|    ! 0 | 2417 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2418 | `			}` |
|      2 | 2419 | `		}` |
|      7 | 2420 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      7 | 2421 | `		return PH7_OK;` |
|      - | 2422 | `	}` |
|      - | 2423 | `	/* Point to the end of the string */` |
|   6539 | 2424 | `	zEnd = &zString[nStrlen];` |
|      - | 2425 | `	/* Create the array */` |
|   6539 | 2426 | `	pArray =  ph7_context_new_array(pCtx);` |
|   6539 | 2427 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   6539 | 2428 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2429 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2430 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2431 | `		return PH7_OK;` |
|      - | 2432 | `	}` |
|      - | 2433 | `	/* Set a defualt limit */` |
|   6539 | 2434 | `	iLimit = SXI32_HIGH;` |
|   6539 | 2435 | `	if( nArg > 2 ){` |
|     38 | 2436 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     38 | 2437 | `		if( iLimit < 0 ){` |
|      - | 2438 | `			/* Negative limit: keep all components except the last -iLimit (PHP).` |
|      - | 2439 | `			 * Pre-count the components (delimiters + 1), then emit only the first` |
|      - | 2440 | `			 * nKeep CLEAN components — no trailing-remainder merge (the difference` |
|      - | 2441 | `			 * from the positive path). nKeep <= 0 drops everything -> empty array. */` |
|     17 | 2442 | `			int nTotal = 1,nKeep;` |
|     17 | 2443 | `			const char *zScan = zString;` |
|      - | 2444 | `			sxu32 nScanOfft;` |
|     57 | 2445 | `			while( SyBlobSearch(zScan,(sxu32)(zEnd - zScan),zDelim,nDelim,&nScanOfft) == SXRET_OK ){` |
|     41 | 2446 | `				nTotal++;` |
|     41 | 2447 | `				zScan = &zScan[nScanOfft + nDelim];` |
|      1 | 2448 | `			}` |
|     17 | 2449 | `			nKeep = nTotal + iLimit; /* iLimit < 0, so this is nTotal - (-iLimit) */` |
|     49 | 2450 | `			while( nKeep > (int)ph7_array_count(pArray)` |
|     39 | 2451 | `				&& SyBlobSearch(zString,(sxu32)(zEnd - zString),zDelim,nDelim,&nOfft) == SXRET_OK ){` |
|      - | 2452 | `				/* Emit the next clean component */` |
|     23 | 2453 | `				zCur = &zString[nOfft];` |
|     23 | 2454 | `				ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|     23 | 2455 | `				if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2456 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2457 | `				}` |
|     23 | 2458 | `				zString = &zCur[nDelim];` |
|     23 | 2459 | `				ph7_value_reset_string_cursor(pValue);` |
|      1 | 2460 | `			}` |
|     17 | 2461 | `			ph7_result_value(pCtx,pArray);` |
|     17 | 2462 | `			return PH7_OK;` |
|      - | 2463 | `		}` |
|     22 | 2464 | `		if( iLimit == 0 ){` |
|      5 | 2465 | `			iLimit = 1;` |
|      2 | 2466 | `		}` |
|     22 | 2467 | `		iLimit--;` |
|      9 | 2468 | `	}` |
|      - | 2469 | `	/* Start exploding */` |
|  79428 | 2470 | `	for(;;){` |
| 158861 | 2471 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 158861 | 2472 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2473 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   6523 | 2474 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   6523 | 2475 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2476 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2477 | `			}` |
|   6523 | 2478 | `			break;` |
|      - | 2479 | `		}` |
|      - | 2480 | `		/* Point to the desired offset */` |
| 152343 | 2481 | `		zCur = &zString[nOfft];` |
|      - | 2482 | `		/* Perform the store operation (may be empty) */` |
| 152343 | 2483 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 152343 | 2484 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2485 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2486 | `		}` |
|      - | 2487 | `		/* Point beyond the delimiter */` |
| 152343 | 2488 | `		zString = &zCur[nDelim];` |
|      - | 2489 | `		/* Reset the cursor */` |
| 152343 | 2490 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 2491 | `	}` |
|      - | 2492 | `	/* Return the freshly created array */` |
|   6523 | 2493 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2494 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2495 | `	 * released as soon we return from this foregin function.` |
|      - | 2496 | `	 */` |
|   6523 | 2497 | `	return PH7_OK;` |
|   3277 | 2498 | `}` |
|      - | 2499 | `/*` |
|      - | 2500 | ` * string trim(string $str[,string $charlist ])` |
|      - | 2501 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2502 | ` * Parameters` |
|      - | 2503 | ` *  $str` |
|      - | 2504 | ` *   The string that will be trimmed.` |
|      - | 2505 | ` * $charlist` |
|      - | 2506 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2507 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2508 | ` *   With .. you can specify a range of characters.` |
|      - | 2509 | ` * Returns.` |
|      - | 2510 | ` *  Thr processed string.` |
|      - | 2511 | ` * NOTE:` |
|      - | 2512 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2513 | ` */` |
|  14764 | 2514 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2515 | `{` |
|  14769 | 2516 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"trim",1,"$string"); }` |
|      - | 2517 | `	const char *zString;` |
|      - | 2518 | `	int nLen;` |
|  14769 | 2519 | `	if( nArg < 1 ){` |
|      - | 2520 | `		/* Missing arguments,return null */` |
|    ! 0 | 2521 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2522 | `		return PH7_OK;` |
|      - | 2523 | `	}` |
|      - | 2524 | `	/* Extract the target string */` |
|  14769 | 2525 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  14769 | 2526 | `	if( nLen < 1 ){` |
|      - | 2527 | `		/* Empty string,return */` |
|   1289 | 2528 | `		ph7_result_string(pCtx,"",0);` |
|   1289 | 2529 | `		return PH7_OK;` |
|      - | 2530 | `	}` |
|      - | 2531 | `	/* Start the trim process */` |
|  13485 | 2532 | `	if( nArg < 2 ){` |
|      - | 2533 | `		SyString sStr;` |
|      - | 2534 | `		/* Remove white spaces and NUL bytes */` |
|  13455 | 2535 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  33753 | 2536 | `		SyStringFullTrimSafe(&sStr);` |
|  13455 | 2537 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   6730 | 2538 | `	}else{` |
|      - | 2539 | `		/* Char list */` |
|      - | 2540 | `		const char *zList;` |
|      - | 2541 | `		int nListlen;` |
|     33 | 2542 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     33 | 2543 | `		if( nListlen < 1 ){` |
|      - | 2544 | `			/* Return the string unchanged */` |
|      6 | 2545 | `			ph7_result_string(pCtx,zString,nLen);` |
|      4 | 2546 | `		}else{` |
|      - | 2547 | `			char aMask[256];` |
|     29 | 2548 | `			const char *zEnd = &zString[nLen];` |
|     29 | 2549 | `			const char *zCur = zString;` |
|     29 | 2550 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2551 | `			/* Left trim */` |
|     79 | 2552 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     53 | 2553 | `				zCur++;` |
|      3 | 2554 | `			}` |
|      - | 2555 | `			/* Right trim */` |
|     79 | 2556 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     53 | 2557 | `				zEnd--;` |
|      3 | 2558 | `			}` |
|     29 | 2559 | `			if( zCur >= zEnd ){` |
|      - | 2560 | `				/* Return the empty string */` |
|    ! 0 | 2561 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2562 | `			}else{` |
|     29 | 2563 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2564 | `			}` |
|      - | 2565 | `		}` |
|      - | 2566 | `	}` |
|  13485 | 2567 | `	return PH7_OK;` |
|   7387 | 2568 | `}` |
|      - | 2569 | `/*` |
|      - | 2570 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 2571 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 2572 | ` * Parameters` |
|      - | 2573 | ` *  $str` |
|      - | 2574 | ` *   The string that will be trimmed.` |
|      - | 2575 | ` * $charlist` |
|      - | 2576 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2577 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2578 | ` *   With .. you can specify a range of characters.` |
|      - | 2579 | ` * Returns.` |
|      - | 2580 | ` *  Thr processed string.` |
|      - | 2581 | ` * NOTE:` |
|      - | 2582 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2583 | ` */` |
|     30 | 2584 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2585 | `{` |
|     33 | 2586 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"rtrim",1,"$string"); }` |
|      - | 2587 | `	const char *zString;` |
|      - | 2588 | `	int nLen;` |
|     33 | 2589 | `	if( nArg < 1 ){` |
|      - | 2590 | `		/* Missing arguments,return null */` |
|    ! 0 | 2591 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2592 | `		return PH7_OK;` |
|      - | 2593 | `	}` |
|      - | 2594 | `	/* Extract the target string */` |
|     33 | 2595 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     33 | 2596 | `	if( nLen < 1 ){` |
|      - | 2597 | `		/* Empty string,return */` |
|      7 | 2598 | `		ph7_result_string(pCtx,"",0);` |
|      7 | 2599 | `		return PH7_OK;` |
|      - | 2600 | `	}` |
|      - | 2601 | `	/* Start the trim process */` |
|     27 | 2602 | `	if( nArg < 2 ){` |
|      - | 2603 | `		SyString sStr;` |
|      - | 2604 | `		/* Remove white spaces and NUL bytes*/` |
|     17 | 2605 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     41 | 2606 | `		SyStringRightTrimSafe(&sStr);` |
|     17 | 2607 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      9 | 2608 | `	}else{` |
|      - | 2609 | `		/* Char list */` |
|      - | 2610 | `		const char *zList;` |
|      - | 2611 | `		int nListlen;` |
|     11 | 2612 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     11 | 2613 | `		if( nListlen < 1 ){` |
|      - | 2614 | `			/* Return the string unchanged */` |
|    ! 0 | 2615 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 2616 | `		}else{` |
|      - | 2617 | `			char aMask[256];` |
|     11 | 2618 | `			const char *zEnd = &zString[nLen];` |
|     11 | 2619 | `			const char *zCur = zString;` |
|     11 | 2620 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2621 | `			/* Right trim */` |
|     29 | 2622 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     20 | 2623 | `				zEnd--;` |
|      2 | 2624 | `			}` |
|     11 | 2625 | `			if( zEnd <= zCur ){` |
|      - | 2626 | `				/* Return the empty string */` |
|    ! 0 | 2627 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2628 | `			}else{` |
|     11 | 2629 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2630 | `			}` |
|      - | 2631 | `		}` |
|      - | 2632 | `	}` |
|     27 | 2633 | `	return PH7_OK;` |
|     18 | 2634 | `}` |
|      - | 2635 | `/*` |
|      - | 2636 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 2637 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2638 | ` * Parameters` |
|      - | 2639 | ` *  $str` |
|      - | 2640 | ` *   The string that will be trimmed.` |
|      - | 2641 | ` * $charlist` |
|      - | 2642 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2643 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2644 | ` *   With .. you can specify a range of characters.` |
|      - | 2645 | ` * Returns.` |
|      - | 2646 | ` *  Thr processed string.` |
|      - | 2647 | ` * NOTE:` |
|      - | 2648 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2649 | ` */` |
|     48 | 2650 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2651 | `{` |
|     53 | 2652 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"ltrim",1,"$string"); }` |
|      - | 2653 | `	const char *zString;` |
|      - | 2654 | `	int nLen;` |
|     53 | 2655 | `	if( nArg < 1 ){` |
|      - | 2656 | `		/* Missing arguments,return null */` |
|    ! 0 | 2657 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2658 | `		return PH7_OK;` |
|      - | 2659 | `	}` |
|      - | 2660 | `	/* Extract the target string */` |
|     53 | 2661 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     53 | 2662 | `	if( nLen < 1 ){` |
|      - | 2663 | `		/* Empty string,return */` |
|     29 | 2664 | `		ph7_result_string(pCtx,"",0);` |
|     29 | 2665 | `		return PH7_OK;` |
|      - | 2666 | `	}` |
|      - | 2667 | `	/* Start the trim process */` |
|     27 | 2668 | `	if( nArg < 2 ){` |
|      - | 2669 | `		SyString sStr;` |
|      - | 2670 | `		/* Remove white spaces and NUL byte */` |
|      5 | 2671 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     13 | 2672 | `		SyStringLeftTrimSafe(&sStr);` |
|      5 | 2673 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      3 | 2674 | `	}else{` |
|      - | 2675 | `		/* Char list */` |
|      - | 2676 | `		const char *zList;` |
|      - | 2677 | `		int nListlen;` |
|     23 | 2678 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     23 | 2679 | `		if( nListlen < 1 ){` |
|      - | 2680 | `			/* Return the string unchanged */` |
|      3 | 2681 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2682 | `		}else{` |
|      - | 2683 | `			char aMask[256];` |
|     21 | 2684 | `			const char *zEnd = &zString[nLen];` |
|     21 | 2685 | `			const char *zCur = zString;` |
|     21 | 2686 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2687 | `			/* Left trim */` |
|     55 | 2688 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     37 | 2689 | `				zCur++;` |
|      3 | 2690 | `			}` |
|     21 | 2691 | `			if( zCur >= zEnd ){` |
|      - | 2692 | `				/* Return the empty string */` |
|    ! 0 | 2693 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2694 | `			}else{` |
|     21 | 2695 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2696 | `			}` |
|      - | 2697 | `		}` |
|      - | 2698 | `	}` |
|     27 | 2699 | `	return PH7_OK;` |
|     29 | 2700 | `}` |
|      - | 2701 | `/*` |
|      - | 2702 | ` * string strtolower(string $str)` |
|      - | 2703 | ` *  Make a string lowercase.` |
|      - | 2704 | ` * Parameters` |
|      - | 2705 | ` *  $str` |
|      - | 2706 | ` *   The input string.` |
|      - | 2707 | ` * Returns.` |
|      - | 2708 | ` *  The lowercased string.` |
|      - | 2709 | ` */` |
|  33514 | 2710 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2711 | `{` |
|  33519 | 2712 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtolower",1,"$string"); }` |
|      - | 2713 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2714 | `	int nLen;` |
|  33519 | 2715 | `	if( nArg < 1 ){` |
|      - | 2716 | `		/* Missing arguments,return null */` |
|    ! 0 | 2717 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2718 | `		return PH7_OK;` |
|      - | 2719 | `	}` |
|      - | 2720 | `	/* Extract the target string */` |
|  33519 | 2721 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  33519 | 2722 | `	if( nLen < 1 ){` |
|      - | 2723 | `		/* Empty string,return */` |
|      5 | 2724 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2725 | `		return PH7_OK;` |
|      - | 2726 | `	}` |
|      - | 2727 | `	/* Perform the requested operation */` |
|  33515 | 2728 | `	zEnd = &zString[nLen];` |
| 105777 | 2729 | `	for(;;){` |
| 211559 | 2730 | `		if( zString >= zEnd ){` |
|      - | 2731 | `			/* No more input,break immediately */` |
|  33515 | 2732 | `			break;` |
|      - | 2733 | `		}` |
| 178049 | 2734 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2735 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2736 | `			zCur = zString;` |
|    ! 0 | 2737 | `			zString++;` |
|    ! 0 | 2738 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2739 | `				zString++;` |
|    ! 0 | 2740 | `			}` |
|      - | 2741 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2742 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2743 | `		}else{` |
| 178049 | 2744 | `			int c = zString[0];` |
| 178049 | 2745 | `			if( SyisUpper(c) ){` |
| 175493 | 2746 | `				c = SyToLower(zString[0]);` |
|  87744 | 2747 | `			}` |
|      - | 2748 | `			/* Append character */` |
| 178049 | 2749 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2750 | `			/* Advance the cursor */` |
| 178049 | 2751 | `			zString++;` |
|      - | 2752 | `		}` |
|      5 | 2753 | `	}` |
|  33515 | 2754 | `	return PH7_OK;` |
|  16762 | 2755 | `}` |
|      - | 2756 | `/*` |
|      - | 2757 | ` * string strtolower(string $str)` |
|      - | 2758 | ` *  Make a string uppercase.` |
|      - | 2759 | ` * Parameters` |
|      - | 2760 | ` *  $str` |
|      - | 2761 | ` *   The input string.` |
|      - | 2762 | ` * Returns.` |
|      - | 2763 | ` *  The uppercased string.` |
|      - | 2764 | ` */` |
|     68 | 2765 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2766 | `{` |
|     72 | 2767 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strtoupper",1,"$string"); }` |
|      - | 2768 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2769 | `	int nLen;` |
|     72 | 2770 | `	if( nArg < 1 ){` |
|      - | 2771 | `		/* Missing arguments,return null */` |
|    ! 0 | 2772 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2773 | `		return PH7_OK;` |
|      - | 2774 | `	}` |
|      - | 2775 | `	/* Extract the target string */` |
|     72 | 2776 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     72 | 2777 | `	if( nLen < 1 ){` |
|      - | 2778 | `		/* Empty string,return */` |
|      5 | 2779 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2780 | `		return PH7_OK;` |
|      - | 2781 | `	}` |
|      - | 2782 | `	/* Perform the requested operation */` |
|     68 | 2783 | `	zEnd = &zString[nLen];` |
|    135 | 2784 | `	for(;;){` |
|    274 | 2785 | `		if( zString >= zEnd ){` |
|      - | 2786 | `			/* No more input,break immediately */` |
|     68 | 2787 | `			break;` |
|      - | 2788 | `		}` |
|    210 | 2789 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2790 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2791 | `			zCur = zString;` |
|    ! 0 | 2792 | `			zString++;` |
|    ! 0 | 2793 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2794 | `				zString++;` |
|    ! 0 | 2795 | `			}` |
|      - | 2796 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2797 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2798 | `		}else{` |
|    210 | 2799 | `			int c = zString[0];` |
|    210 | 2800 | `			if( SyisLower(c) ){` |
|    200 | 2801 | `				c = SyToUpper(zString[0]);` |
|     98 | 2802 | `			}` |
|      - | 2803 | `			/* Append character */` |
|    210 | 2804 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2805 | `			/* Advance the cursor */` |
|    210 | 2806 | `			zString++;` |
|      - | 2807 | `		}` |
|      4 | 2808 | `	}` |
|     68 | 2809 | `	return PH7_OK;` |
|     38 | 2810 | `}` |
|      - | 2811 | `/*` |
|      - | 2812 | ` * string ucfirst(string $str)` |
|      - | 2813 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2814 | ` *  character is alphabetic.` |
|      - | 2815 | ` * Parameters` |
|      - | 2816 | ` *  $str` |
|      - | 2817 | ` *   The input string.` |
|      - | 2818 | ` * Returns.` |
|      - | 2819 | ` *  The processed string.` |
|      - | 2820 | ` */` |
|      4 | 2821 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2822 | `{` |
|      - | 2823 | `	const char *zString,*zEnd;` |
|      - | 2824 | `	int nLen,c;` |
|      5 | 2825 | `	if( nArg < 1 ){` |
|      - | 2826 | `		/* Missing arguments,return null */` |
|    ! 0 | 2827 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2828 | `		return PH7_OK;` |
|      - | 2829 | `	}` |
|      - | 2830 | `	/* Extract the target string */` |
|      5 | 2831 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2832 | `	if( nLen < 1 ){` |
|      - | 2833 | `		/* Empty string,return */` |
|      3 | 2834 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2835 | `		return PH7_OK;` |
|      - | 2836 | `	}` |
|      - | 2837 | `	/* Perform the requested operation */` |
|      3 | 2838 | `	zEnd = &zString[nLen];` |
|      3 | 2839 | `	c = zString[0];` |
|      3 | 2840 | `	if( SyisLower(c) ){` |
|      3 | 2841 | `		c = SyToUpper(c);` |
|      1 | 2842 | `	}` |
|      - | 2843 | `	/* Append the first character */` |
|      3 | 2844 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2845 | `	zString++;` |
|      3 | 2846 | `	if( zString < zEnd ){` |
|      - | 2847 | `		/* Append the rest of the input verbatim */` |
|      3 | 2848 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2849 | `	}` |
|      3 | 2850 | `	return PH7_OK;` |
|      3 | 2851 | `}` |
|      - | 2852 | `/*` |
|      - | 2853 | ` * string lcfirst(string $str)` |
|      - | 2854 | ` *  Make a string's first character lowercase.` |
|      - | 2855 | ` * Parameters` |
|      - | 2856 | ` *  $str` |
|      - | 2857 | ` *   The input string.` |
|      - | 2858 | ` * Returns.` |
|      - | 2859 | ` *  The processed string.` |
|      - | 2860 | ` */` |
|      4 | 2861 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2862 | `{` |
|      - | 2863 | `	const char *zString,*zEnd;` |
|      - | 2864 | `	int nLen,c;` |
|      5 | 2865 | `	if( nArg < 1 ){` |
|      - | 2866 | `		/* Missing arguments,return null */` |
|    ! 0 | 2867 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2868 | `		return PH7_OK;` |
|      - | 2869 | `	}` |
|      - | 2870 | `	/* Extract the target string */` |
|      5 | 2871 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2872 | `	if( nLen < 1 ){` |
|      - | 2873 | `		/* Empty string,return */` |
|      3 | 2874 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2875 | `		return PH7_OK;` |
|      - | 2876 | `	}` |
|      - | 2877 | `	/* Perform the requested operation */` |
|      3 | 2878 | `	zEnd = &zString[nLen];` |
|      3 | 2879 | `	c = zString[0];` |
|      3 | 2880 | `	if( SyisUpper(c) ){` |
|      3 | 2881 | `		c = SyToLower(c);` |
|      1 | 2882 | `	}` |
|      - | 2883 | `	/* Append the first character */` |
|      3 | 2884 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2885 | `	zString++;` |
|      3 | 2886 | `	if( zString < zEnd ){` |
|      - | 2887 | `		/* Append the rest of the input verbatim */` |
|      3 | 2888 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2889 | `	}` |
|      3 | 2890 | `	return PH7_OK;` |
|      3 | 2891 | `}` |
|      - | 2892 | `/*` |
|      - | 2893 | ` * int ord(string $string)` |
|      - | 2894 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 2895 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 2896 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 2897 | ` * Parameters` |
|      - | 2898 | ` *  $string` |
|      - | 2899 | ` *   The input string.` |
|      - | 2900 | ` * Returns` |
|      - | 2901 | ` *  The ASCII value as an integer.` |
|      - | 2902 | ` */` |
|     56 | 2903 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2904 | `{` |
|      - | 2905 | `	const char *zString;` |
|      - | 2906 | `	int nLen,c;` |
|      - | 2907 | `	/* PHP requires exactly one argument. */` |
|     59 | 2908 | `	if( nArg != 1 ){` |
|      8 | 2909 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2910 | `			"ArgumentCountError",` |
|      - | 2911 | `			"ord() expects exactly 1 argument, %d given",` |
|      2 | 2912 | `			nArg` |
|      - | 2913 | `			);` |
|      - | 2914 | `	}` |
|      - | 2915 | `	/* Passing null is deprecated (E_DEPRECATED).  PHP emits this before` |
|      - | 2916 | `	 * the empty-string deprecation, so we check null first. */` |
|     53 | 2917 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 2918 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2919 | `			"ord(): Passing null to parameter #1 ($character) "` |
|      - | 2920 | `			"of type string is deprecated"` |
|      - | 2921 | `			);` |
|      1 | 2922 | `	}` |
|      - | 2923 | `	/* Extract the target string */` |
|     53 | 2924 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     53 | 2925 | `	if( nLen < 1 ){` |
|      - | 2926 | `		/* Empty string is deprecated (E_DEPRECATED). */` |
|      5 | 2927 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2928 | `			"ord(): Providing an empty string is deprecated"` |
|      - | 2929 | `			);` |
|      5 | 2930 | `		ph7_result_int(pCtx,0);` |
|      5 | 2931 | `		return PH7_OK;` |
|      - | 2932 | `	}` |
|      - | 2933 | `	/* A string longer than one byte is deprecated (E_DEPRECATED). */` |
|     49 | 2934 | `	if( nLen > 1 ){` |
|      7 | 2935 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2936 | `			"ord(): Providing a string that is not one byte long "` |
|      - | 2937 | `			"is deprecated. Use ord($str[0]) instead"` |
|      - | 2938 | `			);` |
|      3 | 2939 | `	}` |
|      - | 2940 | `	/* Extract the ASCII value of the first character */` |
|     49 | 2941 | `	c = (unsigned char)zString[0];` |
|      - | 2942 | `	/* Return that value */` |
|     49 | 2943 | `	ph7_result_int(pCtx,c);` |
|     49 | 2944 | `	return PH7_OK;` |
|     31 | 2945 | `}` |
|      - | 2946 | `/*` |
|      - | 2947 | ` * string chr(int $codepoint)` |
|      - | 2948 | ` *  Returns a one-character string containing the character specified` |
|      - | 2949 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 2950 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 2951 | ` * Parameters` |
|      - | 2952 | ` *  $codepoint` |
|      - | 2953 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 2954 | ` *   will be constrained to a single byte.` |
|      - | 2955 | ` * Returns` |
|      - | 2956 | ` *  A single-character string.` |
|      - | 2957 | ` */` |
|   7116 | 2958 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2959 | `{` |
|      - | 2960 | `	int c;` |
|      - | 2961 | `	unsigned char ch;` |
|      - | 2962 | `	/* PHP requires exactly one argument. */` |
|   7120 | 2963 | `	if( nArg != 1 ){` |
|      8 | 2964 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2965 | `			"ArgumentCountError",` |
|      - | 2966 | `			"chr() expects exactly 1 argument, %d given",` |
|      2 | 2967 | `			nArg` |
|      - | 2968 | `			);` |
|      - | 2969 | `	}` |
|      - | 2970 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 2971 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 2972 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 2973 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|   7114 | 2974 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      - | 2975 | `		char zBuf[120];` |
|      4 | 2976 | `		SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 2977 | `			"Implicit conversion from float %g to int loses precision",` |
|      1 | 2978 | `			ph7_value_to_double(apArg[0])` |
|      - | 2979 | `			);` |
|      3 | 2980 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);` |
|      1 | 2981 | `	}` |
|      - | 2982 | `	/* Extract the codepoint. */` |
|   7114 | 2983 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 2984 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 2985 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 2986 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 2987 | `	 * name to avoid the API double-prefixing it. */` |
|   7114 | 2988 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 2989 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 2990 | `			E_DEPRECATED,` |
|      - | 2991 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 2992 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 2993 | `			"The value used will be constrained using % 256"` |
|      - | 2994 | `			);` |
|      2 | 2995 | `	}` |
|      - | 2996 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 2997 | `	 * when taking the address of a wider int. */` |
|   7114 | 2998 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 2999 | `	/* Return the specified character */` |
|   7114 | 3000 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|   7114 | 3001 | `	return PH7_OK;` |
|   3562 | 3002 | `}` |
|      - | 3003 | `/*` |
|      - | 3004 | ` * Binary to hex consumer callback.` |
|      - | 3005 | ` * This callback is the default consumer used by the hash functions` |
|      - | 3006 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 3007 | ` */` |
|   3118 | 3008 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 3009 | `{` |
|      - | 3010 | `	/* Append hex chunk verbatim */` |
|   3120 | 3011 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   3120 | 3012 | `	return SXRET_OK;` |
|      2 | 3013 | `}` |
|      - | 3014 |  |
|      - | 3015 | `/*` |
|      - | 3016 | ` * string bin2hex(string $str)` |
|      - | 3017 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 3018 | ` * Parameters` |
|      - | 3019 | ` *  $str` |
|      - | 3020 | ` *   The input string.` |
|      - | 3021 | ` * Returns.` |
|      - | 3022 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 3023 | ` */` |
|    138 | 3024 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3025 | `{` |
|      - | 3026 | `	const char *zString;` |
|      - | 3027 | `	int nLen;` |
|      - | 3028 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|    143 | 3029 | `	if( nArg != 1 ){` |
|      8 | 3030 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3031 | `			"ArgumentCountError",` |
|      - | 3032 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|      2 | 3033 | `			nArg` |
|      - | 3034 | `			);` |
|      - | 3035 | `	}` |
|      - | 3036 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 3037 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 3038 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 3039 | `	 */` |
|    204 | 3040 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|    130 | 3041 | `		( ph7_value_is_object(apArg[0]) &&` |
|      3 | 3042 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|      2 | 3043 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|      1 | 3044 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 3045 | `		)` |
|      - | 3046 | `	){` |
|      9 | 3047 | `		const char *zType = ph7_type_name(apArg[0]);` |
|      9 | 3048 | `		if( ph7_value_is_object(apArg[0]) ){` |
|      3 | 3049 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      3 | 3050 | `			if( pInst && pInst->pClass ){` |
|      3 | 3051 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      1 | 3052 | `			}` |
|      1 | 3053 | `		}` |
|     12 | 3054 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3055 | `			"TypeError",` |
|      - | 3056 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|      3 | 3057 | `			zType` |
|      - | 3058 | `			);` |
|      - | 3059 | `	}` |
|      - | 3060 | `	/* Extract the target string */` |
|    130 | 3061 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    130 | 3062 | `	if( nLen < 1 ){` |
|      - | 3063 | `		/* Empty string,return */` |
|     13 | 3064 | `		ph7_result_string(pCtx,"",0);` |
|     13 | 3065 | `		return PH7_OK;` |
|      - | 3066 | `	}` |
|      - | 3067 | `	/* Perform the requested operation */` |
|    118 | 3068 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|    118 | 3069 | `	return PH7_OK;` |
|     74 | 3070 | `}` |
|      - | 3071 |  |
|      - | 3072 | `/* Search callback signature */` |
|      - | 3073 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 3074 | `/*` |
|      - | 3075 | ` * Case-insensitive pattern match.` |
|      - | 3076 | ` * Brute force is the default search method used here.` |
|      - | 3077 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 3078 | ` * well for short/medium texts on modern hardware.` |
|      - | 3079 | ` */` |
|    298 | 3080 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      2 | 3081 | `{` |
|    300 | 3082 | `	const char *zpIn = (const char *)pPattern;` |
|    300 | 3083 | `	const char *zIn = (const char *)pText;` |
|    300 | 3084 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    300 | 3085 | `	const char *zEnd = &zIn[nLen];` |
|      - | 3086 | `	const char *zPtr,*zPtr2;` |
|      - | 3087 | `	int c,d;` |
|    300 | 3088 | `	if( iPatLen > nLen ){` |
|      - | 3089 | `		/* Don't bother processing */` |
|     67 | 3090 | `		return SXERR_NOTFOUND;` |
|      - | 3091 | `	}` |
|    860 | 3092 | `	for(;;){` |
|   1722 | 3093 | `		if( zIn >= zEnd ){` |
|    194 | 3094 | `			break;` |
|      - | 3095 | `		}` |
|   1530 | 3096 | `		c = SyToLower(zIn[0]);` |
|   1530 | 3097 | `		d = SyToLower(zpIn[0]);` |
|   1530 | 3098 | `		if( c == d ){` |
|    182 | 3099 | `			zPtr   = &zIn[1];` |
|    182 | 3100 | `			zPtr2  = &zpIn[1];` |
|    141 | 3101 | `			for(;;){` |
|    284 | 3102 | `				if( zPtr2 >= zpEnd ){` |
|      - | 3103 | `					/* Pattern found */` |
|     41 | 3104 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 3105 | `					return SXRET_OK;` |
|      - | 3106 | `				}` |
|    244 | 3107 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 3108 | `					break;` |
|      - | 3109 | `				}` |
|    244 | 3110 | `				c = SyToLower(zPtr[0]);` |
|    244 | 3111 | `				d = SyToLower(zPtr2[0]);` |
|    244 | 3112 | `				if( c != d ){` |
|    142 | 3113 | `					break;` |
|      - | 3114 | `				}` |
|    103 | 3115 | `				zPtr++; zPtr2++;` |
|      1 | 3116 | `			}` |
|     70 | 3117 | `		}` |
|   1490 | 3118 | `		zIn++;` |
|      2 | 3119 | `	}` |
|      - | 3120 | `	/* Pattern not found */` |
|    194 | 3121 | `	return SXERR_NOTFOUND;` |
|    151 | 3122 | `}` |
|      - | 3123 | `/*` |
|      - | 3124 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3125 | ` *  Find the first occurrence of a string.` |
|      - | 3126 | ` * Parameters` |
|      - | 3127 | ` *  $haystack` |
|      - | 3128 | ` *   The input string.` |
|      - | 3129 | ` * $needle` |
|      - | 3130 | ` *   Search pattern (must be a string).` |
|      - | 3131 | ` * $before_needle` |
|      - | 3132 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3133 | ` *   of the needle (excluding the needle).` |
|      - | 3134 | ` * Return` |
|      - | 3135 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3136 | ` */` |
|      6 | 3137 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3138 | `{` |
|      7 | 3139 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3140 | `	const char *zBlob,*zPattern;` |
|      - | 3141 | `	int nLen,nPatLen;` |
|      - | 3142 | `	sxu32 nOfft;` |
|      - | 3143 | `	sxi32 rc;` |
|      7 | 3144 | `	if( nArg < 2 ){` |
|      - | 3145 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3146 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3147 | `		return PH7_OK;` |
|      - | 3148 | `	}` |
|      - | 3149 | `	/* Extract the needle and the haystack */` |
|      7 | 3150 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 3151 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 3152 | `	nOfft = 0; /* cc warning */` |
|      9 | 3153 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3154 | `		int before = 0;` |
|      - | 3155 | `		/* Perform the lookup */` |
|      5 | 3156 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3157 | `		if( rc != SXRET_OK ){` |
|      - | 3158 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3159 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3160 | `			return PH7_OK;` |
|      - | 3161 | `		}` |
|      - | 3162 | `		/* Return the portion of the string */` |
|      5 | 3163 | `		if( nArg > 2 ){` |
|      3 | 3164 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3165 | `		}` |
|      5 | 3166 | `		if( before ){` |
|      3 | 3167 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3168 | `		}else{` |
|      3 | 3169 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3170 | `		}` |
|      3 | 3171 | `	}else{` |
|      3 | 3172 | `		ph7_result_bool(pCtx,0);` |
|      - | 3173 | `	}` |
|      7 | 3174 | `	return PH7_OK;` |
|      4 | 3175 | `}` |
|      - | 3176 | `/*` |
|      - | 3177 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3178 | ` *  Case-insensitive strstr().` |
|      - | 3179 | ` * Parameters` |
|      - | 3180 | ` *  $haystack` |
|      - | 3181 | ` *   The input string.` |
|      - | 3182 | ` * $needle` |
|      - | 3183 | ` *   Search pattern (must be a string).` |
|      - | 3184 | ` * $before_needle` |
|      - | 3185 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3186 | ` *   of the needle (excluding the needle).` |
|      - | 3187 | ` * Return` |
|      - | 3188 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3189 | ` */` |
|      4 | 3190 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3191 | `{` |
|      5 | 3192 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3193 | `	const char *zBlob,*zPattern;` |
|      - | 3194 | `	int nLen,nPatLen;` |
|      - | 3195 | `	sxu32 nOfft;` |
|      - | 3196 | `	sxi32 rc;` |
|      5 | 3197 | `	if( nArg < 2 ){` |
|      - | 3198 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3199 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3200 | `		return PH7_OK;` |
|      - | 3201 | `	}` |
|      - | 3202 | `	/* Extract the needle and the haystack */` |
|      5 | 3203 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3204 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 3205 | `	nOfft = 0; /* cc warning */` |
|      7 | 3206 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3207 | `		int before = 0;` |
|      - | 3208 | `		/* Perform the lookup */` |
|      5 | 3209 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3210 | `		if( rc != SXRET_OK ){` |
|      - | 3211 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3212 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3213 | `			return PH7_OK;` |
|      - | 3214 | `		}` |
|      - | 3215 | `		/* Return the portion of the string */` |
|      5 | 3216 | `		if( nArg > 2 ){` |
|      3 | 3217 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3218 | `		}` |
|      5 | 3219 | `		if( before ){` |
|      3 | 3220 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3221 | `		}else{` |
|      3 | 3222 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3223 | `		}` |
|      3 | 3224 | `	}else{` |
|    ! 0 | 3225 | `		ph7_result_bool(pCtx,0);` |
|      - | 3226 | `	}` |
|      5 | 3227 | `	return PH7_OK;` |
|      3 | 3228 | `}` |
|      - | 3229 | `/*` |
|      - | 3230 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3231 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 3232 | ` * Parameters` |
|      - | 3233 | ` *  $haystack` |
|      - | 3234 | ` *   The input string.` |
|      - | 3235 | ` * $needle` |
|      - | 3236 | ` *   Search pattern (must be a string).` |
|      - | 3237 | ` * $offset` |
|      - | 3238 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3239 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3240 | ` *   of haystack.` |
|      - | 3241 | ` * Return` |
|      - | 3242 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3243 | ` */` |
|   1454 | 3244 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3245 | `{` |
|   1459 | 3246 | `	if( nArg > 0 ){ StrNullArgNotice(pCtx,apArg[0],"strpos",1,"$haystack"); }` |
|   1459 | 3247 | `	if( nArg > 1 ){ StrNullArgNotice(pCtx,apArg[1],"strpos",2,"$needle"); }` |
|   1459 | 3248 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3249 | `	const char *zBlob,*zPattern;` |
|      - | 3250 | `	int nLen,nPatLen,nStart;` |
|      - | 3251 | `	sxu32 nOfft;` |
|      - | 3252 | `	sxi32 rc;` |
|   1459 | 3253 | `	if( nArg < 2 ){` |
|      - | 3254 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3255 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3256 | `		return PH7_OK;` |
|      - | 3257 | `	}` |
|      - | 3258 | `	/* Extract the needle and the haystack */` |
|   1459 | 3259 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|   1459 | 3260 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|   1459 | 3261 | `	nOfft = 0; /* cc warning */` |
|   1459 | 3262 | `	nStart = 0;` |
|      - | 3263 | `	/* Peek the starting offset if available */` |
|   1459 | 3264 | `	if( nArg > 2 ){` |
|    ! 0 | 3265 | `		nStart = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 3266 | `		if( nStart < 0 ){` |
|    ! 0 | 3267 | `			nStart = -nStart;` |
|    ! 0 | 3268 | `		}` |
|    ! 0 | 3269 | `		if( nStart >= nLen ){` |
|      - | 3270 | `			/* Invalid offset */` |
|    ! 0 | 3271 | `			nStart = 0;` |
|    ! 0 | 3272 | `		}else{` |
|    ! 0 | 3273 | `			zBlob += nStart;` |
|    ! 0 | 3274 | `			nLen -= nStart;` |
|      - | 3275 | `		}` |
|    ! 0 | 3276 | `	}` |
|   1459 | 3277 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3278 | `		/* Perform the lookup */` |
|   1457 | 3279 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|   1457 | 3280 | `		if( rc != SXRET_OK ){` |
|      - | 3281 | `			/* Pattern not found,return FALSE */` |
|    779 | 3282 | `			ph7_result_bool(pCtx,0);` |
|    779 | 3283 | `			return PH7_OK;` |
|      - | 3284 | `		}` |
|      - | 3285 | `		/* Return the pattern position */` |
|    682 | 3286 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|    343 | 3287 | `	}else{` |
|      3 | 3288 | `		ph7_result_bool(pCtx,0);` |
|      - | 3289 | `	}` |
|    684 | 3290 | `	return PH7_OK;` |
|    732 | 3291 | `}` |
|      - | 3292 | `/*` |
|      - | 3293 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|      - | 3294 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|      - | 3295 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|      - | 3296 | ` * TypeError for arrays, resources, and objects without __toString.` |
|      - | 3297 | ` *` |
|      - | 3298 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|      - | 3299 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|      - | 3300 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|      - | 3301 | ` *` |
|      - | 3302 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|      - | 3303 | ` * is valid until pTmp is released or pArg is mutated.` |
|      - | 3304 | ` */` |
|    724 | 3305 | `static sxi32 StrPredicateResolveArg(` |
|      - | 3306 | `	ph7_context *pCtx,` |
|      - | 3307 | `	ph7_value *pArg,` |
|      - | 3308 | `	const char *zFunc,` |
|      - | 3309 | `	int iArgNum,` |
|      - | 3310 | `	const char *zParamName,` |
|      - | 3311 | `	const char *zTypeStr, /* Declared type in the TypeError, e.g. "string" / "?string" */` |
|      - | 3312 | `	const char *zNullMsg,` |
|      - | 3313 | `	ph7_value *pTmp,` |
|      - | 3314 | `	const char **pzOut,` |
|      - | 3315 | `	int *pnOut` |
|      4 | 3316 | `){` |
|    728 | 3317 | `	if( ph7_value_is_null(pArg) ){` |
|     13 | 3318 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zNullMsg);` |
|     13 | 3319 | `		*pzOut = "";` |
|     13 | 3320 | `		*pnOut = 0;` |
|     13 | 3321 | `		return PH7_OK;` |
|      - | 3322 | `	}` |
|   1094 | 3323 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    686 | 3324 | `	    ( ph7_value_is_object(pArg) &&` |
|    105 | 3325 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     70 | 3326 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     35 | 3327 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 3328 | `	    )` |
|      - | 3329 | `	){` |
|     52 | 3330 | `		const char *zType = ph7_type_name(pArg);` |
|     52 | 3331 | `		if( ph7_value_is_object(pArg) ){` |
|     23 | 3332 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     23 | 3333 | `			if( pInst && pInst->pClass ){` |
|     23 | 3334 | `				zType = SyStringData(&pInst->pClass->sName);` |
|     11 | 3335 | `			}` |
|     11 | 3336 | `		}` |
|     76 | 3337 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3338 | `			"TypeError",` |
|      - | 3339 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|     24 | 3340 | `			zFunc, iArgNum, zParamName, zTypeStr, zType` |
|      - | 3341 | `			);` |
|      - | 3342 | `	}` |
|    665 | 3343 | `	if( ph7_value_is_object(pArg) ){` |
|     49 | 3344 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     49 | 3345 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 3346 | `			"__toString",sizeof("__toString")-1);` |
|     49 | 3347 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|     49 | 3348 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|     49 | 3349 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|     49 | 3350 | `		return PH7_OK;` |
|      - | 3351 | `	}` |
|    617 | 3352 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|    617 | 3353 | `	return PH7_OK;` |
|    366 | 3354 | `}` |
|      - | 3355 | `/*` |
|      - | 3356 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 3357 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 3358 | ` * Return` |
|      - | 3359 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 3360 | ` */` |
|     98 | 3361 | `static int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 3362 | `{` |
|      - | 3363 | `	const char *zHaystack,*zNeedle;` |
|      - | 3364 | `	int nHayLen,nNeedleLen;` |
|      - | 3365 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3366 | `	sxi32 rc;` |
|    102 | 3367 | `	if( nArg != 2 ){` |
|     18 | 3368 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3369 | `			"ArgumentCountError",` |
|      - | 3370 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|      5 | 3371 | `			nArg` |
|      - | 3372 | `			);` |
|      - | 3373 | `	}` |
|     90 | 3374 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     90 | 3375 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     90 | 3376 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack","string",` |
|      - | 3377 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|      - | 3378 | `		"of type string is deprecated",` |
|      - | 3379 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     90 | 3380 | `	if( rc != PH7_OK ) goto out;` |
|     83 | 3381 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle","string",` |
|      - | 3382 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|      - | 3383 | `		"of type string is deprecated",` |
|      - | 3384 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     83 | 3385 | `	if( rc != PH7_OK ) goto out;` |
|     79 | 3386 | `	if( nNeedleLen < 1 ){` |
|     13 | 3387 | `		ph7_result_bool(pCtx,1);` |
|     73 | 3388 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3389 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3390 | `	}else{` |
|     88 | 3391 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|     29 | 3392 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|     59 | 3393 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|      - | 3394 | `	}` |
|     79 | 3395 | `	rc = PH7_OK;` |
|     44 | 3396 | `out:` |
|     90 | 3397 | `	PH7_MemObjRelease(&sHayTmp);` |
|     90 | 3398 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     90 | 3399 | `	return rc;` |
|     53 | 3400 | `}` |
|      - | 3401 | `/*` |
|      - | 3402 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 3403 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 3404 | ` * Return` |
|      - | 3405 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 3406 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3407 | ` */` |
|     78 | 3408 | `static int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 3409 | `{` |
|      - | 3410 | `	const char *zHaystack,*zNeedle;` |
|      - | 3411 | `	int nHayLen,nNeedleLen;` |
|      - | 3412 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3413 | `	sxi32 rc;` |
|     82 | 3414 | `	if( nArg != 2 ){` |
|     18 | 3415 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3416 | `			"ArgumentCountError",` |
|      - | 3417 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|      5 | 3418 | `			nArg` |
|      - | 3419 | `			);` |
|      - | 3420 | `	}` |
|     70 | 3421 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 3422 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 3423 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack","string",` |
|      - | 3424 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3425 | `		"of type string is deprecated",` |
|      - | 3426 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 3427 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 3428 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle","string",` |
|      - | 3429 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3430 | `		"of type string is deprecated",` |
|      - | 3431 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 3432 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3433 | `	if( nNeedleLen < 1 ){` |
|     13 | 3434 | `		ph7_result_bool(pCtx,1);` |
|     53 | 3435 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3436 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3437 | `	}else{` |
|     58 | 3438 | `		ph7_result_bool(pCtx,` |
|     38 | 3439 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3440 | `	}` |
|     59 | 3441 | `	rc = PH7_OK;` |
|     34 | 3442 | `out:` |
|     70 | 3443 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 3444 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 3445 | `	return rc;` |
|     43 | 3446 | `}` |
|      - | 3447 | `/*` |
|      - | 3448 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 3449 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 3450 | ` * Return` |
|      - | 3451 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 3452 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3453 | ` */` |
|     78 | 3454 | `static int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 3455 | `{` |
|      - | 3456 | `	const char *zHaystack,*zNeedle;` |
|      - | 3457 | `	int nHayLen,nNeedleLen;` |
|      - | 3458 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3459 | `	sxi32 rc;` |
|     82 | 3460 | `	if( nArg != 2 ){` |
|     18 | 3461 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3462 | `			"ArgumentCountError",` |
|      - | 3463 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|      5 | 3464 | `			nArg` |
|      - | 3465 | `			);` |
|      - | 3466 | `	}` |
|     70 | 3467 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 3468 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 3469 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack","string",` |
|      - | 3470 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3471 | `		"of type string is deprecated",` |
|      - | 3472 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 3473 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 3474 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle","string",` |
|      - | 3475 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3476 | `		"of type string is deprecated",` |
|      - | 3477 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 3478 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3479 | `	if( nNeedleLen < 1 ){` |
|     13 | 3480 | `		ph7_result_bool(pCtx,1);` |
|     53 | 3481 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3482 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3483 | `	}else{` |
|     58 | 3484 | `		ph7_result_bool(pCtx,` |
|     38 | 3485 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3486 | `	}` |
|     59 | 3487 | `	rc = PH7_OK;` |
|     34 | 3488 | `out:` |
|     70 | 3489 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 3490 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 3491 | `	return rc;` |
|     43 | 3492 | `}` |
|      - | 3493 | `/*` |
|      - | 3494 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3495 | ` *  Case-insensitive strpos.` |
|      - | 3496 | ` * Parameters` |
|      - | 3497 | ` *  $haystack` |
|      - | 3498 | ` *   The input string.` |
|      - | 3499 | ` * $needle` |
|      - | 3500 | ` *   Search pattern (must be a string).` |
|      - | 3501 | ` * $offset` |
|      - | 3502 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3503 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3504 | ` *   of haystack.` |
|      - | 3505 | ` * Return` |
|      - | 3506 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3507 | ` */` |
|    196 | 3508 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3509 | `{` |
|    198 | 3510 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3511 | `	const char *zBlob,*zPattern;` |
|      - | 3512 | `	int nLen,nPatLen,nStart;` |
|      - | 3513 | `	sxu32 nOfft;` |
|      - | 3514 | `	sxi32 rc;` |
|    198 | 3515 | `	if( nArg < 2 ){` |
|      - | 3516 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3517 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3518 | `		return PH7_OK;` |
|      - | 3519 | `	}` |
|      - | 3520 | `	/* Extract the needle and the haystack */` |
|    198 | 3521 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    198 | 3522 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    198 | 3523 | `	nOfft = 0; /* cc warning */` |
|    198 | 3524 | `	nStart = 0;` |
|      - | 3525 | `	/* Peek the starting offset if available */` |
|    198 | 3526 | `	if( nArg > 2 ){` |
|      5 | 3527 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 3528 | `		if( nStart < 0 ){` |
|      3 | 3529 | `			nStart = -nStart;` |
|      1 | 3530 | `		}` |
|      5 | 3531 | `		if( nStart >= nLen ){` |
|      - | 3532 | `			/* Invalid offset */` |
|    ! 0 | 3533 | `			nStart = 0;` |
|    ! 0 | 3534 | `		}else{` |
|      5 | 3535 | `			zBlob += nStart;` |
|      5 | 3536 | `			nLen -= nStart;` |
|      - | 3537 | `		}` |
|      2 | 3538 | `	}` |
|    198 | 3539 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3540 | `		/* Perform the lookup */` |
|    198 | 3541 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    198 | 3542 | `		if( rc != SXRET_OK ){` |
|      - | 3543 | `			/* Pattern not found,return FALSE */` |
|    184 | 3544 | `			ph7_result_bool(pCtx,0);` |
|    184 | 3545 | `			return PH7_OK;` |
|      - | 3546 | `		}` |
|      - | 3547 | `		/* Return the pattern position */` |
|     15 | 3548 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 3549 | `	}else{` |
|    ! 0 | 3550 | `		ph7_result_bool(pCtx,0);` |
|      - | 3551 | `	}` |
|     15 | 3552 | `	return PH7_OK;` |
|    100 | 3553 | `}` |
|      - | 3554 | `/*` |
|      - | 3555 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3556 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 3557 | ` * Parameters` |
|      - | 3558 | ` *  $haystack` |
|      - | 3559 | ` *   The input string.` |
|      - | 3560 | ` * $needle` |
|      - | 3561 | ` *   Search pattern (must be a string).` |
|      - | 3562 | ` * $offset` |
|      - | 3563 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3564 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3565 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3566 | ` * Return` |
|      - | 3567 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3568 | ` */` |
|     40 | 3569 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3570 | `{` |
|      - | 3571 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     41 | 3572 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3573 | `	int nLen,nPatLen;` |
|      - | 3574 | `	sxu32 nOfft;` |
|      - | 3575 | `	sxi32 rc;` |
|     41 | 3576 | `	if( nArg < 2 ){` |
|      - | 3577 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3578 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3579 | `		return PH7_OK;` |
|      - | 3580 | `	}` |
|      - | 3581 | `	/* Extract the needle and the haystack */` |
|     41 | 3582 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 3583 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3584 | `	/* Point to the end of the pattern */` |
|     41 | 3585 | `	zPtr = &zBlob[nLen - 1];` |
|     41 | 3586 | `	zEnd = &zBlob[nLen];` |
|      - | 3587 | `	/* Save the starting posistion */` |
|     41 | 3588 | `	zStart = zBlob;` |
|     41 | 3589 | `	nOfft = 0; /* cc warning */` |
|      - | 3590 | `	/* Peek the starting offset if available */` |
|     41 | 3591 | `	if( nArg > 2 ){` |
|      - | 3592 | `		int nStart;` |
|     21 | 3593 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 3594 | `		if( nStart < 0 ){` |
|     11 | 3595 | `			nStart = -nStart;` |
|     11 | 3596 | `			if( nStart >= nLen ){` |
|      - | 3597 | `				/* Invalid offset */` |
|      3 | 3598 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3599 | `				return PH7_OK;` |
|    ! 0 | 3600 | `			}else{` |
|      9 | 3601 | `				nLen -= nStart;` |
|      9 | 3602 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 3603 | `				zEnd = &zBlob[nLen];` |
|      - | 3604 | `			}` |
|      5 | 3605 | `		}else{` |
|     11 | 3606 | `			if( nStart >= nLen ){` |
|      - | 3607 | `				/* Invalid offset */` |
|      5 | 3608 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3609 | `				return PH7_OK;` |
|    ! 0 | 3610 | `			}else{` |
|      7 | 3611 | `				zBlob += nStart;` |
|      7 | 3612 | `				nLen -= nStart;` |
|      - | 3613 | `			}` |
|      - | 3614 | `		}` |
|      7 | 3615 | `	}` |
|     35 | 3616 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3617 | `		/* Perform the lookup */` |
|    121 | 3618 | `		for(;;){` |
|    243 | 3619 | `			if( zBlob >= zPtr ){` |
|     21 | 3620 | `				break;` |
|      - | 3621 | `			}` |
|    223 | 3622 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    223 | 3623 | `			if( rc == SXRET_OK ){` |
|      - | 3624 | `				/* Pattern found,return it's position */` |
|     13 | 3625 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 3626 | `				return PH7_OK;` |
|      - | 3627 | `			}` |
|    211 | 3628 | `			zPtr--;` |
|      1 | 3629 | `		}` |
|      - | 3630 | `		/* Pattern not found,return FALSE */` |
|     21 | 3631 | `		ph7_result_bool(pCtx,0);` |
|     11 | 3632 | `	}else{` |
|      3 | 3633 | `		ph7_result_bool(pCtx,0);` |
|      - | 3634 | `	}` |
|     23 | 3635 | `	return PH7_OK;` |
|     21 | 3636 | `}` |
|      - | 3637 | `/*` |
|      - | 3638 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3639 | ` *  Case-insensitive strrpos.` |
|      - | 3640 | ` * Parameters` |
|      - | 3641 | ` *  $haystack` |
|      - | 3642 | ` *   The input string.` |
|      - | 3643 | ` * $needle` |
|      - | 3644 | ` *   Search pattern (must be a string).` |
|      - | 3645 | ` * $offset` |
|      - | 3646 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3647 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3648 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3649 | ` * Return` |
|      - | 3650 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3651 | ` */` |
|     26 | 3652 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3653 | `{` |
|      - | 3654 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     27 | 3655 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3656 | `	int nLen,nPatLen;` |
|      - | 3657 | `	sxu32 nOfft;` |
|      - | 3658 | `	sxi32 rc;` |
|     27 | 3659 | `	if( nArg < 2 ){` |
|      - | 3660 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3661 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3662 | `		return PH7_OK;` |
|      - | 3663 | `	}` |
|      - | 3664 | `	/* Extract the needle and the haystack */` |
|     27 | 3665 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 3666 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3667 | `	/* Point to the end of the pattern */` |
|     27 | 3668 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3669 | `	zEnd = &zBlob[nLen];` |
|      - | 3670 | `	/* Save the starting posistion */` |
|     27 | 3671 | `	zStart = zBlob;` |
|     27 | 3672 | `	nOfft = 0; /* cc warning */` |
|      - | 3673 | `	/* Peek the starting offset if available */` |
|     27 | 3674 | `	if( nArg > 2 ){` |
|      - | 3675 | `		int nStart;` |
|     15 | 3676 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3677 | `		if( nStart < 0 ){` |
|      7 | 3678 | `			nStart = -nStart;` |
|      7 | 3679 | `			if( nStart >= nLen ){` |
|      - | 3680 | `				/* Invalid offset */` |
|      3 | 3681 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3682 | `				return PH7_OK;` |
|    ! 0 | 3683 | `			}else{` |
|      5 | 3684 | `				nLen -= nStart;` |
|      5 | 3685 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3686 | `				zEnd = &zBlob[nLen];` |
|      - | 3687 | `			}` |
|      3 | 3688 | `		}else{` |
|      9 | 3689 | `			if( nStart >= nLen ){` |
|      - | 3690 | `				/* Invalid offset */` |
|      5 | 3691 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3692 | `				return PH7_OK;` |
|    ! 0 | 3693 | `			}else{` |
|      5 | 3694 | `				zBlob += nStart;` |
|      5 | 3695 | `				nLen -= nStart;` |
|      - | 3696 | `			}` |
|      - | 3697 | `		}` |
|      4 | 3698 | `	}` |
|     21 | 3699 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3700 | `		/* Perform the lookup */` |
|     44 | 3701 | `		for(;;){` |
|     89 | 3702 | `			if( zBlob >= zPtr ){` |
|      9 | 3703 | `				break;` |
|      - | 3704 | `			}` |
|     81 | 3705 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3706 | `			if( rc == SXRET_OK ){` |
|      - | 3707 | `				/* Pattern found,return it's position */` |
|     11 | 3708 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3709 | `				return PH7_OK;` |
|      - | 3710 | `			}` |
|     71 | 3711 | `			zPtr--;` |
|      1 | 3712 | `		}` |
|      - | 3713 | `		/* Pattern not found,return FALSE */` |
|      9 | 3714 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3715 | `	}else{` |
|      3 | 3716 | `		ph7_result_bool(pCtx,0);` |
|      - | 3717 | `	}` |
|     11 | 3718 | `	return PH7_OK;` |
|     14 | 3719 | `}` |
|      - | 3720 | `/*` |
|      - | 3721 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3722 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3723 | ` * Parameters` |
|      - | 3724 | ` *  $haystack` |
|      - | 3725 | ` *   The input string.` |
|      - | 3726 | ` * $needle` |
|      - | 3727 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3728 | ` *  This behavior is different from that of strstr().` |
|      - | 3729 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3730 | ` *  as the ordinal value of a character.` |
|      - | 3731 | ` * Return` |
|      - | 3732 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3733 | ` */` |
|     22 | 3734 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3735 | `{` |
|      - | 3736 | `	const char *zBlob;` |
|      - | 3737 | `	int nLen,c;` |
|     23 | 3738 | `	if( nArg < 2 ){` |
|      - | 3739 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3740 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3741 | `		return PH7_OK;` |
|      - | 3742 | `	}` |
|      - | 3743 | `	/* Extract the haystack */` |
|     23 | 3744 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3745 | `	c = 0; /* cc warning */` |
|     23 | 3746 | `	if( nLen > 0 ){` |
|      - | 3747 | `		sxu32 nOfft;` |
|      - | 3748 | `		sxi32 rc;` |
|     21 | 3749 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3750 | `			const char *zPattern;` |
|     11 | 3751 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3752 | `														 * for NULL pointer.` |
|      - | 3753 | `														 */` |
|     11 | 3754 | `			c = zPattern[0];` |
|      6 | 3755 | `		}else{` |
|      - | 3756 | `			/* Int cast */` |
|     11 | 3757 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3758 | `		}` |
|      - | 3759 | `		/* Perform the lookup */` |
|     21 | 3760 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3761 | `		if( rc != SXRET_OK ){` |
|      - | 3762 | `			/* No such entry,return FALSE */` |
|      7 | 3763 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3764 | `			return PH7_OK;` |
|      - | 3765 | `		}` |
|      - | 3766 | `		/* Return the string portion */` |
|     15 | 3767 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3768 | `	}else{` |
|      3 | 3769 | `		ph7_result_bool(pCtx,0);` |
|      - | 3770 | `	}` |
|     17 | 3771 | `	return PH7_OK;` |
|     12 | 3772 | `}` |
|      - | 3773 | `/*` |
|      - | 3774 | ` * string strrev(string $string)` |
|      - | 3775 | ` *  Reverse a string.` |
|      - | 3776 | ` * Parameters` |
|      - | 3777 | ` *  $string` |
|      - | 3778 | ` *   String to be reversed.` |
|      - | 3779 | ` * Return` |
|      - | 3780 | ` *  The reversed string.` |
|      - | 3781 | ` */` |
|      2 | 3782 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3783 | `{` |
|      - | 3784 | `	const char *zIn,*zEnd;` |
|      - | 3785 | `	int nLen,c;` |
|      3 | 3786 | `	if( nArg < 1 ){` |
|      - | 3787 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3788 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3789 | `		return PH7_OK;` |
|      - | 3790 | `	}` |
|      - | 3791 | `	/* Extract the target string */` |
|      3 | 3792 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3793 | `	if( nLen < 1 ){` |
|      - | 3794 | `		/* Empty string Return null */` |
|    ! 0 | 3795 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3796 | `		return PH7_OK;` |
|      - | 3797 | `	}` |
|      - | 3798 | `	/* Perform the requested operation */` |
|      3 | 3799 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3800 | `	for(;;){` |
|      9 | 3801 | `		if( zEnd < zIn ){` |
|      - | 3802 | `			/* No more input to process */` |
|      3 | 3803 | `			break;` |
|      - | 3804 | `		}` |
|      - | 3805 | `		/* Append current character */` |
|      7 | 3806 | `		c = zEnd[0];` |
|      7 | 3807 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3808 | `		zEnd--;` |
|      1 | 3809 | `	}` |
|      3 | 3810 | `	return PH7_OK;` |
|      2 | 3811 | `}` |
|      - | 3812 | `/*` |
|      - | 3813 | ` * string ucwords(string $string [, string $separators = " \t\r\n\f\v"])` |
|      - | 3814 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3815 | ` *  A word begins at the start of the string and after any character present in` |
|      - | 3816 | ` *  $separators. The default separators are the whitespace characters (space,` |
|      - | 3817 | ` *  horizontal tab, carriage return, newline, form-feed and vertical tab); an` |
|      - | 3818 | ` *  explicit $separators argument REPLACES them (an empty string leaves only the` |
|      - | 3819 | ` *  very first character upper-cased). Like PHP, this is byte-based: only ASCII` |
|      - | 3820 | ` *  bytes are upper-cased and a byte is a separator only if it appears in the set.` |
|      - | 3821 | ` * Parameters` |
|      - | 3822 | ` *  $string` |
|      - | 3823 | ` *   The input string.` |
|      - | 3824 | ` *  $separators` |
|      - | 3825 | ` *   The optional word-boundary characters.` |
|      - | 3826 | ` * Return` |
|      - | 3827 | ` *  The modified string.` |
|      - | 3828 | ` */` |
|     22 | 3829 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3830 | `{` |
|      - | 3831 | `	const char *zIn;` |
|      - | 3832 | `	int nLen,i,iStart;` |
|      - | 3833 | `	char aDelim[256];` |
|     23 | 3834 | `	if( nArg < 1 ){` |
|      - | 3835 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3836 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3837 | `		return PH7_OK;` |
|      - | 3838 | `	}` |
|      - | 3839 | `	/* Build the separator membership table: an explicit $separators argument` |
|      - | 3840 | `	 * replaces the default whitespace set (an empty string clears it). */` |
|     23 | 3841 | `	SyZero(aDelim,(sxu32)sizeof(aDelim));` |
|     23 | 3842 | `	if( nArg > 1 ){` |
|      - | 3843 | `		int nDelim;` |
|      9 | 3844 | `		const char *zDelim = ph7_value_to_string(apArg[1],&nDelim);` |
|     17 | 3845 | `		for( i = 0 ; i < nDelim ; i++ ){` |
|      9 | 3846 | `			aDelim[(unsigned char)zDelim[i]] = 1;` |
|      5 | 3847 | `		}` |
|      5 | 3848 | `	}else{` |
|     15 | 3849 | `		aDelim[(unsigned char)' ']  = 1;` |
|     15 | 3850 | `		aDelim[(unsigned char)'\t'] = 1;` |
|     15 | 3851 | `		aDelim[(unsigned char)'\r'] = 1;` |
|     15 | 3852 | `		aDelim[(unsigned char)'\n'] = 1;` |
|     15 | 3853 | `		aDelim[(unsigned char)'\f'] = 1;` |
|     15 | 3854 | `		aDelim[(unsigned char)'\v'] = 1;` |
|      - | 3855 | `	}` |
|      - | 3856 | `	/* Extract the target string */` |
|     23 | 3857 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3858 | `	if( nLen < 1 ){` |
|      - | 3859 | `		/* Empty string – match PHP semantics */` |
|      3 | 3860 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3861 | `		return PH7_OK;` |
|      - | 3862 | `	}` |
|      - | 3863 | `	/* Upper-case the first byte of each word (the leading byte, or any byte that` |
|      - | 3864 | `	 * follows a separator), appending the untouched runs in between verbatim. */` |
|     21 | 3865 | `	iStart = 0;` |
|    309 | 3866 | `	for( i = 0 ; i < nLen ; i++ ){` |
|    289 | 3867 | `		int c = (unsigned char)zIn[i];` |
|    289 | 3868 | `		if( (i == 0 \|\| aDelim[(unsigned char)zIn[i-1]]) && c < 0x80 && SyisLower(c) ){` |
|     53 | 3869 | `			char up = (char)SyToUpper(c);` |
|     53 | 3870 | `			if( i > iStart ){` |
|     35 | 3871 | `				ph7_result_string(pCtx,&zIn[iStart],i - iStart);` |
|     17 | 3872 | `			}` |
|     53 | 3873 | `			ph7_result_string(pCtx,&up,1);` |
|     53 | 3874 | `			iStart = i + 1;` |
|     26 | 3875 | `		}` |
|    145 | 3876 | `	}` |
|     21 | 3877 | `	if( nLen > iStart ){` |
|     21 | 3878 | `		ph7_result_string(pCtx,&zIn[iStart],nLen - iStart);` |
|     10 | 3879 | `	}` |
|     21 | 3880 | `	return PH7_OK;` |
|     12 | 3881 | `}` |
|      - | 3882 | `/*` |
|      - | 3883 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3884 | ` *  Returns input repeated multiplier times.` |
|      - | 3885 | ` * Parameters` |
|      - | 3886 | ` *  $string` |
|      - | 3887 | ` *   String to be repeated.` |
|      - | 3888 | ` * $multiplier` |
|      - | 3889 | ` *  Number of time the input string should be repeated.` |
|      - | 3890 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3891 | ` *  to 0, the function will return an empty string.` |
|      - | 3892 | ` * Return` |
|      - | 3893 | ` *  The repeated string.` |
|      - | 3894 | ` */` |
|  20434 | 3895 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3896 | `{` |
|      - | 3897 | `	const char *zIn;` |
|      - | 3898 | `	int nLen;` |
|      - | 3899 | `	ph7_int64 nMul;` |
|      - | 3900 | `	int rc;` |
|  20436 | 3901 | `	if( nArg < 2 ){` |
|      - | 3902 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3903 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3904 | `		return PH7_OK;` |
|      - | 3905 | `	}` |
|      - | 3906 | `	/* Extract the target string */` |
|  20436 | 3907 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3908 | `	/* Extract the multiplier as a 64-bit value (a 32-bit read would wrap a large` |
|      - | 3909 | `	 * positive $times into a negative one and trip a spurious ValueError). PHP` |
|      - | 3910 | `	 * validates $times regardless of the string contents: a negative count throws` |
|      - | 3911 | `	 * a catchable ValueError. */` |
|  20436 | 3912 | `	nMul = ph7_value_to_int64(apArg[1]);` |
|  20436 | 3913 | `	if( nMul < 0 ){` |
|      3 | 3914 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 3915 | `			"str_repeat(): Argument #2 ($times) must be greater than or equal to 0");` |
|      - | 3916 | `	}` |
|  20434 | 3917 | `	if( nLen < 1 \|\| nMul < 1 ){` |
|      - | 3918 | `		/* Empty input or a zero multiplier yields the empty string (PHP). */` |
|    ! 0 | 3919 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3920 | `		return PH7_OK;` |
|      - | 3921 | `	}` |
|      - | 3922 | `	/* Perform the requested operation */` |
| 221930 | 3923 | `	for(;;){` |
| 443862 | 3924 | `		if( !nMul ){` |
|  20434 | 3925 | `			break;` |
|      - | 3926 | `		}` |
|      - | 3927 | `		/* Append the copy */` |
| 423430 | 3928 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 423430 | 3929 | `		if( rc != PH7_OK ){` |
|      - | 3930 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 3931 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 3932 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 3933 | `		}` |
| 423430 | 3934 | `		nMul--;` |
|      2 | 3935 | `	}` |
|  20434 | 3936 | `	return PH7_OK;` |
|  10219 | 3937 | `}` |
|      - | 3938 | `/*` |
|      - | 3939 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3940 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3941 | ` * Parameters` |
|      - | 3942 | ` *  $string` |
|      - | 3943 | ` *   The input string.` |
|      - | 3944 | ` * $is_xhtml` |
|      - | 3945 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3946 | ` * Return` |
|      - | 3947 | ` *  The processed string.` |
|      - | 3948 | ` */` |
|      4 | 3949 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3950 | `{` |
|      - | 3951 | `	const char *zIn,*zCur,*zEnd;` |
|      5 | 3952 | `	int is_xhtml = 1; /* Default to XHTML-style '<br />' like PHP */` |
|      - | 3953 | `	int nLen;` |
|      5 | 3954 | `	if( nArg < 1 ){` |
|      - | 3955 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 3956 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3957 | `		return PH7_OK;` |
|      - | 3958 | `	}` |
|      - | 3959 | `	/* Extract the target string */` |
|      5 | 3960 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3961 | `	if( nLen < 1 ){` |
|      - | 3962 | `		/* Empty string,return null */` |
|    ! 0 | 3963 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3964 | `		return PH7_OK;` |
|      - | 3965 | `	}` |
|      5 | 3966 | `	if( nArg > 1 ){` |
|      3 | 3967 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3968 | `	}` |
|      5 | 3969 | `	zEnd = &zIn[nLen];` |
|      - | 3970 | `	/* Perform the requested operation */` |
|      4 | 3971 | `	for(;;){` |
|      9 | 3972 | `		zCur = zIn;` |
|      - | 3973 | `		/* Delimit the string */` |
|     21 | 3974 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3975 | `			zIn++;` |
|      1 | 3976 | `		}` |
|      9 | 3977 | `		if( zCur < zIn ){` |
|      - | 3978 | `			/* Output chunk verbatim */` |
|      9 | 3979 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3980 | `		}` |
|      9 | 3981 | `		if( zIn >= zEnd ){` |
|      - | 3982 | `			/* No more input to process */` |
|      5 | 3983 | `			break;` |
|      - | 3984 | `		}` |
|      - | 3985 | `		/* Output the HTML line break */` |
|      - | 3986 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br />' (space before the slash), otherwise use '<br>' */` |
|      5 | 3987 | `		if( is_xhtml ){` |
|      3 | 3988 | `			ph7_result_string(pCtx,"<br />",(int)sizeof("<br />")-1);` |
|      2 | 3989 | `		}else{` |
|      3 | 3990 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3991 | `		}` |
|      5 | 3992 | `		zCur = zIn;` |
|      - | 3993 | `		/* Append trailing line */` |
|     11 | 3994 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3995 | `			zIn++;` |
|      1 | 3996 | `		}` |
|      5 | 3997 | `		if( zCur < zIn ){` |
|      - | 3998 | `			/* Output chunk verbatim */` |
|      5 | 3999 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 4000 | `		}` |
|      1 | 4001 | `	}` |
|      5 | 4002 | `	return PH7_OK;` |
|      3 | 4003 | `}` |
|      - | 4004 | `/*` |
|      - | 4005 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 4006 | ` *  According to the PHP reference manual.` |
|      - | 4007 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 4008 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 4009 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 4010 | ` * This applies to both sprintf() and printf().` |
|      - | 4011 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 4012 | ` * or more of these elements, in order:` |
|      - | 4013 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 4014 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 4015 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 4016 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 4017 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 4018 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 4019 | ` *   it with a single quote ('). See the examples below.` |
|      - | 4020 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 4021 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 4022 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 4023 | ` *   should result in.` |
|      - | 4024 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 4025 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 4026 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 4027 | ` *   limit to the string.` |
|      - | 4028 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 4029 | ` *       % - a literal percent character. No argument is required.` |
|      - | 4030 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 4031 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 4032 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 4033 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 4034 | ` * 	     for the number of digits after the decimal point.` |
|      - | 4035 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 4036 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 4037 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 4038 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 4039 | ` *       g - shorter of %e and %f.` |
|      - | 4040 | ` *       G - shorter of %E and %f.` |
|      - | 4041 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 4042 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 4043 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 4044 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 4045 | ` */` |
|      - | 4046 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4047 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4048 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 4049 | `/*` |
|      - | 4050 | `** Conversion types fall into various categories as defined by the` |
|      - | 4051 | `** following enumeration.` |
|      - | 4052 | `*/` |
|      - | 4053 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 4054 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 4055 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 4056 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 4057 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 4058 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 4059 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 4060 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 4061 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 4062 |  |
|      - | 4063 | `/*` |
|      - | 4064 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 4065 | `*/` |
|      - | 4066 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 4067 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 4068 | `/*` |
|      - | 4069 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 4070 | `** by an instance of the following structure` |
|      - | 4071 | `*/` |
|      - | 4072 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 4073 | `struct ph7_fmt_info` |
|      - | 4074 | `{` |
|      - | 4075 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 4076 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 4077 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 4078 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 4079 | `  char *charset; /* The character set for conversion */` |
|      - | 4080 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 4081 | `};` |
|      - | 4082 | `/* PH7_PhpFloatShape (php's float-shape post-processing) lives in memobj.c —` |
|      - | 4083 | ` * the default float->string cast needs it even when this whole formatting` |
|      - | 4084 | ` * region is compiled out by PH7_DISABLE_DISK_IO. */` |
|      - | 4085 | `/*` |
|      - | 4086 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 4087 | ` * used conversion types first.` |
|      - | 4088 | ` */` |
|      - | 4089 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 4090 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 4091 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 4092 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 4093 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 4094 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 4095 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 4096 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 4097 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 4098 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4099 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4100 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 4101 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 4102 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 4103 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 4104 | `  /* php's 'h'/'H' are the locale-independent twins of 'g'/'G'; PHL always` |
|      - | 4105 | `   * formats in the C locale, so they behave identically. */` |
|      - | 4106 | `  {  'h',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 4107 | `  {  'H',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 4108 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 4109 | `};` |
|      - | 4110 | `/*` |
|      - | 4111 | ` * PHP 8 raises a catchable ValueError for an unknown conversion specifier` |
|      - | 4112 | ` * (e.g. "%y", or the C-ism "%#x" — '#' is not a php flag). Because printf()` |
|      - | 4113 | ` * and fprintf() stream their output incrementally while sprintf() buffers it,` |
|      - | 4114 | ` * every format builtin calls PH7_FormatValidate (below) to check the whole` |
|      - | 4115 | ` * format string BEFORE formatting so the throw happens with no partial output` |
|      - | 4116 | ` * escaping (php buffers the entire result and only emits it on success). This` |
|      - | 4117 | ` * scan mirrors the specifier-locating logic of the main format loop below.` |
|      - | 4118 | ` * On the first unknown specifier, stores it in *pBad and returns TRUE; returns` |
|      - | 4119 | ` * FALSE when every specifier is known. (A found-flag rather than a sentinel` |
|      - | 4120 | ` * char, so a NUL specifier byte — "%\0" — is still reported, not mistaken for` |
|      - | 4121 | ` * "all valid".)` |
|      - | 4122 | ` */` |
|    412 | 4123 | `static int FormatUnknownSpec(const char *zIn,int nByte,int *pBad)` |
|      1 | 4124 | `{` |
|    413 | 4125 | `	const char *zEnd = &zIn[nByte];` |
|      - | 4126 | `	int c,idx;` |
|   3449 | 4127 | `	while( zIn < zEnd ){` |
|   3057 | 4128 | `		if( zIn[0] != '%' ){` |
|   2265 | 4129 | `			zIn++;` |
|   2265 | 4130 | `			continue;` |
|      - | 4131 | `		}` |
|    793 | 4132 | `		zIn++; /* jump the percent sign */` |
|      - | 4133 | `		/* php-supported flags: '-', '+', ' ', '0' and the "'<pad>'" custom-pad` |
|      - | 4134 | `		 * form. '#' is intentionally NOT treated as a flag so it surfaces as an` |
|      - | 4135 | `		 * unknown specifier, matching php. */` |
|    977 | 4136 | `		while( zIn < zEnd ){` |
|    975 | 4137 | `			c = zIn[0];` |
|    975 | 4138 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|    185 | 4139 | `				zIn++;` |
|    185 | 4140 | `				continue;` |
|      - | 4141 | `			}` |
|    791 | 4142 | `			if( c=='\'' ){` |
|    ! 0 | 4143 | `				zIn++;` |
|    ! 0 | 4144 | `				if( zIn < zEnd ){` |
|    ! 0 | 4145 | `					zIn++; /* the custom pad character */` |
|    ! 0 | 4146 | `				}` |
|    ! 0 | 4147 | `				continue;` |
|      - | 4148 | `			}` |
|    791 | 4149 | `			break;` |
|    ! 0 | 4150 | `		}` |
|      - | 4151 | `		/* field width */` |
|   1009 | 4152 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    217 | 4153 | `			zIn++;` |
|      1 | 4154 | `		}` |
|      - | 4155 | `		/* positional specifier ($) — php parses flags AFTER it (e.g. "%1$-10s"),` |
|      - | 4156 | `		 * so skip the full flag set and width again, mirroring the main loop. */` |
|    793 | 4157 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
|    ! 0 | 4158 | `			zIn++;` |
|    ! 0 | 4159 | `			while( zIn < zEnd ){` |
|    ! 0 | 4160 | `				c = zIn[0];` |
|    ! 0 | 4161 | `				if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|    ! 0 | 4162 | `					zIn++;` |
|    ! 0 | 4163 | `					continue;` |
|      - | 4164 | `				}` |
|    ! 0 | 4165 | `				if( c=='\'' ){` |
|    ! 0 | 4166 | `					zIn++;` |
|    ! 0 | 4167 | `					if( zIn < zEnd ){` |
|    ! 0 | 4168 | `						zIn++;` |
|    ! 0 | 4169 | `					}` |
|    ! 0 | 4170 | `					continue;` |
|      - | 4171 | `				}` |
|    ! 0 | 4172 | `				break;` |
|    ! 0 | 4173 | `			}` |
|    ! 0 | 4174 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    ! 0 | 4175 | `				zIn++;` |
|    ! 0 | 4176 | `			}` |
|    ! 0 | 4177 | `		}` |
|      - | 4178 | `		/* precision */` |
|    793 | 4179 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|     87 | 4180 | `			zIn++;` |
|    183 | 4181 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|     97 | 4182 | `				zIn++;` |
|      1 | 4183 | `			}` |
|     43 | 4184 | `		}` |
|      - | 4185 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|    793 | 4186 | `		if( zIn < zEnd && zIn[0]=='l' ){` |
|     11 | 4187 | `			zIn++;` |
|      5 | 4188 | `		}` |
|    793 | 4189 | `		if( zIn >= zEnd ){` |
|      - | 4190 | `			/* A dangling '%' with no specifier: PHL's legacy path silently` |
|      - | 4191 | `			 * truncates here (recorded residual); nothing to validate. */` |
|      3 | 4192 | `			break;` |
|      - | 4193 | `		}` |
|    791 | 4194 | `		c = zIn[0];` |
|    791 | 4195 | `		zIn++; /* jump the conversion specifier */` |
|   3333 | 4196 | `		for( idx = 0 ; idx < (int)SX_ARRAYSIZE(aFmt) ; idx++ ){` |
|   3315 | 4197 | `			if( c == aFmt[idx].fmttype ){` |
|    773 | 4198 | `				break;` |
|      - | 4199 | `			}` |
|   1272 | 4200 | `		}` |
|    791 | 4201 | `		if( idx >= (int)SX_ARRAYSIZE(aFmt) ){` |
|     19 | 4202 | `			*pBad = c; /* unknown specifier */` |
|     19 | 4203 | `			return TRUE;` |
|      - | 4204 | `		}` |
|      1 | 4205 | `	}` |
|    395 | 4206 | `	return FALSE;` |
|    207 | 4207 | `}` |
|      - | 4208 | `/*` |
|      - | 4209 | ` * Validate a printf-style format string. PHP 8 raises a catchable ValueError for` |
|      - | 4210 | ` * an unknown conversion specifier, thrown before any output is produced. Every` |
|      - | 4211 | ` * format builtin (sprintf/printf/vprintf/vsprintf/fprintf/vfprintf) calls this` |
|      - | 4212 | ` * up-front, then propagates the returned status verbatim (PH7_EXCEPTION when the` |
|      - | 4213 | ` * throw is caught in place, PH7_ABORT when it goes uncaught).` |
|      - | 4214 | ` * Returns PH7_OK when the format is valid.` |
|      - | 4215 | ` */` |
|    412 | 4216 | `PH7_PRIVATE sxi32 PH7_FormatValidate(ph7_context *pCtx,const char *zFormat,int nByte)` |
|      1 | 4217 | `{` |
|    413 | 4218 | `	int badSpec = 0;` |
|    413 | 4219 | `	if( FormatUnknownSpec(zFormat,nByte,&badSpec) ){` |
|     28 | 4220 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      9 | 4221 | `			"Unknown format specifier \"%c\"",badSpec);` |
|      - | 4222 | `	}` |
|    395 | 4223 | `	return PH7_OK;` |
|    207 | 4224 | `}` |
|      - | 4225 | `/*` |
|      - | 4226 | `` * PHP 8: a printf-family `$format` argument is a `string` parameter — scalars`` |
|      - | 4227 | ` * (int/float/bool) and null coerce to a string, but an array/object/resource` |
|      - | 4228 | ` * raises a catchable TypeError. iArg is the 1-based argument position ($format` |
|      - | 4229 | ` * is #1 for sprintf/printf/vprintf/vsprintf, #2 for fprintf/vfprintf). Returns` |
|      - | 4230 | ` * PH7_OK when the value is string-coercible (the caller then uses` |
|      - | 4231 | ` * ph7_value_to_string, which renders scalars/null verbatim).` |
|      - | 4232 | ` */` |
|    432 | 4233 | `PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg)` |
|      1 | 4234 | `{` |
|    433 | 4235 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg) \|\| ph7_value_is_resource(pArg) ){` |
|      - | 4236 | `		char zBuf[64];` |
|     13 | 4237 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4238 | `			"%s(): Argument #%d ($format) must be of type string, %s given",` |
|      4 | 4239 | `			ph7_function_name(pCtx),iArg,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|      - | 4240 | `	}` |
|    425 | 4241 | `	return PH7_OK;` |
|    217 | 4242 | `}` |
|      - | 4243 | `/*` |
|      - | 4244 | ` * Format a given string.` |
|      - | 4245 | ` * The root program.  All variations call this core.` |
|      - | 4246 | ` * INPUTS:` |
|      - | 4247 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 4248 | ` *            1. A pointer to the call context.` |
|      - | 4249 | ` *            2. A pointer to the list of characters to be output` |
|      - | 4250 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 4251 | ` *            3. An integer number of characters to be output.` |
|      - | 4252 | ` *               (Note: This number might be zero.)` |
|      - | 4253 | ` *            4. Upper layer private data.` |
|      - | 4254 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 4255 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 4256 | ` */` |
|    394 | 4257 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 4258 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 4259 | `	ph7_context *pCtx,  /* call context */` |
|      - | 4260 | `	const char *zIn,    /* Format string */` |
|      - | 4261 | `	int nByte,          /* Format string length */` |
|      - | 4262 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 4263 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 4264 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 4265 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 4266 | `	)` |
|      1 | 4267 | `{` |
|    395 | 4268 | `	char spaces[] = "                                                  ";` |
|      - | 4269 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    395 | 4270 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 4271 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 4272 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 4273 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 4274 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 4275 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 4276 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 4277 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 4278 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 4279 | `	ph7_int64 iVal;` |
|      - | 4280 | `	int precision;           /* Precision of the current field */` |
|      - | 4281 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 4282 | `	int c,rc,n;` |
|      - | 4283 | `	int length;              /* Length of the field */` |
|      - | 4284 | `	int prefix;` |
|      - | 4285 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 4286 | `	int width;               /* Width of the current field */` |
|      - | 4287 | `	int idx;` |
|    395 | 4288 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 4289 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 4290 | `	/* An unknown conversion specifier is rejected up-front by PH7_FormatValidate()` |
|      - | 4291 | `	 * (called by every format builtin before this routine), so the specifier set` |
|      - | 4292 | `	 * seen here is always valid. */` |
|      - | 4293 | `	/* Start the format process */` |
|    583 | 4294 | `	for(;;){` |
|   1167 | 4295 | `		zCur = zIn;` |
|   3417 | 4296 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|   2251 | 4297 | `			zIn++;` |
|      1 | 4298 | `		}` |
|   1167 | 4299 | `		if( zCur < zIn ){` |
|      - | 4300 | `			/* Consume chunk verbatim */` |
|    725 | 4301 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|    725 | 4302 | `			if( rc != SXRET_OK ){` |
|      - | 4303 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|    ! 0 | 4304 | `				break;` |
|      - | 4305 | `			}` |
|    362 | 4306 | `		}` |
|   1167 | 4307 | `		if( zIn >= zEnd ){` |
|      - | 4308 | `			/* No more input to process,break immediately */` |
|    393 | 4309 | `			break;` |
|      - | 4310 | `		}` |
|      - | 4311 | `		/* Find out what flags are present */` |
|    775 | 4312 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    774 | 4313 | `			flag_alternateform = flag_zeropad = 0;` |
|    775 | 4314 | `		zIn++; /* Jump the precent sign */` |
|    387 | 4315 | `		do{` |
|    959 | 4316 | `			c = zIn[0];` |
|    959 | 4317 | `			switch( c ){` |
|     15 | 4318 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      7 | 4319 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 4320 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    159 | 4321 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 4322 | `			case '\'':` |
|    ! 0 | 4323 | `				zIn++;` |
|    ! 0 | 4324 | `				if( zIn < zEnd ){` |
|      - | 4325 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 4326 | `					c = zIn[0];` |
|    ! 0 | 4327 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4328 | `						spaces[idx] = (char)c;` |
|    ! 0 | 4329 | `					}` |
|    ! 0 | 4330 | `					c = 0;` |
|    ! 0 | 4331 | `				}` |
|    ! 0 | 4332 | `				break;` |
|    774 | 4333 | `			default:                                       break;` |
|      - | 4334 | `			}` |
|    959 | 4335 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 4336 | `		/* Get the field width */` |
|    775 | 4337 | `		width = 0;` |
|   1378 | 4338 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    217 | 4339 | `			width = width*10 + (zIn[0] - '0');` |
|    217 | 4340 | `			zIn++;` |
|      1 | 4341 | `		}` |
|    775 | 4342 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 4343 | `			/* Position specifer */` |
|    ! 0 | 4344 | `			if( width > 0 ){` |
|    ! 0 | 4345 | `				n = width;` |
|    ! 0 | 4346 | `				if( vf && n > 0 ){` |
|    ! 0 | 4347 | `					n--;` |
|    ! 0 | 4348 | `				}` |
|    ! 0 | 4349 | `			}` |
|    ! 0 | 4350 | `			zIn++;` |
|    ! 0 | 4351 | `			width = 0;` |
|      - | 4352 | `			/* php's grammar is %argnum$<flags><width>: the flags come AFTER the` |
|      - | 4353 | `			 * positional, so re-parse the full flag set here (e.g. "%1$-10s"),` |
|      - | 4354 | `			 * not just zero-padding. */` |
|    ! 0 | 4355 | `			do{` |
|    ! 0 | 4356 | `				c = zIn[0];` |
|    ! 0 | 4357 | `				switch( c ){` |
|    ! 0 | 4358 | `				case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|    ! 0 | 4359 | `				case '+':   flag_plussign = 1;        c = 0;   break;` |
|    ! 0 | 4360 | `				case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    ! 0 | 4361 | `				case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 4362 | `				case '\'':` |
|    ! 0 | 4363 | `					zIn++;` |
|    ! 0 | 4364 | `					if( zIn < zEnd ){` |
|    ! 0 | 4365 | `						c = zIn[0];` |
|    ! 0 | 4366 | `						for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4367 | `							spaces[idx] = (char)c;` |
|    ! 0 | 4368 | `						}` |
|    ! 0 | 4369 | `						c = 0;` |
|    ! 0 | 4370 | `					}` |
|    ! 0 | 4371 | `					break;` |
|    ! 0 | 4372 | `				default:                                       break;` |
|      - | 4373 | `				}` |
|    ! 0 | 4374 | `			}while( c==0 && (zIn++ < zEnd) );` |
|    ! 0 | 4375 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 4376 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 4377 | `				zIn++;` |
|    ! 0 | 4378 | `			}` |
|    ! 0 | 4379 | `		}` |
|    775 | 4380 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 4381 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 4382 | `		}` |
|      - | 4383 | `		/* Get the precision */` |
|    775 | 4384 | `		precision = -1;` |
|    775 | 4385 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     87 | 4386 | `			precision = 0;` |
|     87 | 4387 | `			zIn++;` |
|    226 | 4388 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     97 | 4389 | `				precision = precision*10 + (zIn[0] - '0');` |
|     97 | 4390 | `				zIn++;` |
|      1 | 4391 | `			}` |
|     43 | 4392 | `		}` |
|      - | 4393 | `		/* Consume a single 'l' length modifier (a C-ism php accepts and ignores,` |
|      - | 4394 | `		 * e.g. "%ld"); PH7_FormatValidate mirrors this. Exactly one is skipped:` |
|      - | 4395 | `		 * in "%lld" the second 'l' becomes the (unknown) specifier, just like php. */` |
|    775 | 4396 | `		if( zIn < zEnd && zIn[0] == 'l' ){` |
|      9 | 4397 | `			zIn++;` |
|      4 | 4398 | `		}` |
|    775 | 4399 | `		if( zIn >= zEnd ){` |
|      - | 4400 | `			/* No more input */` |
|      3 | 4401 | `			break;` |
|      - | 4402 | `		}` |
|      - | 4403 | `		/* Fetch the info entry for the field */` |
|    773 | 4404 | `		pInfo = 0;` |
|    773 | 4405 | `		xtype = PH7_FMT_ERROR;` |
|    773 | 4406 | `		c = zIn[0];` |
|    773 | 4407 | `		zIn++; /* Jump the format specifer */` |
|   3009 | 4408 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|   3009 | 4409 | `			if( c==aFmt[idx].fmttype ){` |
|    773 | 4410 | `				pInfo = &aFmt[idx];` |
|    773 | 4411 | `				xtype = pInfo->type;` |
|    773 | 4412 | `				break;` |
|      - | 4413 | `			}` |
|   1119 | 4414 | `		}` |
|    773 | 4415 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    773 | 4416 | `		length = 0;` |
|      - | 4417 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 4418 | `		 /*` |
|      - | 4419 | `		  ** At this point, variables are initialized as follows:` |
|      - | 4420 | `		  **` |
|      - | 4421 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 4422 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 4423 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 4424 | `		  **                               field width was negative.` |
|      - | 4425 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 4426 | `		  **                               the conversion character.` |
|      - | 4427 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 4428 | `		  **   width                       The specified field width.  This is` |
|      - | 4429 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 4430 | `		  **   precision                   The specified precision.  The default` |
|      - | 4431 | `		  **                               is -1.` |
|      - | 4432 | `		  */` |
|    773 | 4433 | `		switch(xtype){` |
|      3 | 4434 | `		case PH7_FMT_PERCENT:` |
|      - | 4435 | `			/* A literal percent character */` |
|      7 | 4436 | `			zWorker[0] = '%';` |
|      7 | 4437 | `			length = (int)sizeof(char);` |
|      7 | 4438 | `			break;` |
|      3 | 4439 | `		case PH7_FMT_CHARX:` |
|      - | 4440 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 4441 | `			 * with that ASCII value` |
|      - | 4442 | `			 */` |
|      7 | 4443 | `			pArg = NEXT_ARG;` |
|      7 | 4444 | `			if( pArg == 0 ){` |
|      3 | 4445 | `				c = 0;` |
|      2 | 4446 | `			}else{` |
|      5 | 4447 | `				c = ph7_value_to_int(pArg);` |
|      - | 4448 | `			}` |
|      - | 4449 | `			/* NUL byte is an acceptable value */` |
|      7 | 4450 | `			zWorker[0] = (char)c;` |
|      7 | 4451 | `			length = (int)sizeof(char);` |
|      7 | 4452 | `			break;` |
|    162 | 4453 | `		case PH7_FMT_STRING:` |
|      - | 4454 | `			/* the argument is treated as and presented as a string */` |
|    325 | 4455 | `			pArg = NEXT_ARG;` |
|    325 | 4456 | `			if( pArg == 0 ){` |
|    ! 0 | 4457 | `				length = 0;` |
|    ! 0 | 4458 | `			}else{` |
|    325 | 4459 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 4460 | `			}` |
|    325 | 4461 | `			if( length < 1 ){` |
|    ! 0 | 4462 | `				zBuf = " ";` |
|    ! 0 | 4463 | `				length = (int)sizeof(char);` |
|    ! 0 | 4464 | `			}` |
|    325 | 4465 | `			if( precision>=0 && precision<length ){` |
|      3 | 4466 | `				length = precision;` |
|      1 | 4467 | `			}` |
|    325 | 4468 | `			if( flag_zeropad ){` |
|      - | 4469 | `				/* zero-padding works on strings too */` |
|    ! 0 | 4470 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4471 | `					spaces[idx] = '0';` |
|    ! 0 | 4472 | `				}` |
|    ! 0 | 4473 | `			}` |
|    325 | 4474 | `			break;` |
|    130 | 4475 | `		case PH7_FMT_RADIX:` |
|    261 | 4476 | `			pArg = NEXT_ARG;` |
|    261 | 4477 | `			if( pArg == 0 ){` |
|    ! 0 | 4478 | `				iVal = 0;` |
|    ! 0 | 4479 | `			}else{` |
|    261 | 4480 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 4481 | `			}` |
|      - | 4482 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|    261 | 4483 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 4484 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 4485 | `			}` |
|      - | 4486 | `#if 1` |
|      - | 4487 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 4488 | `        ** I think this is stupid.*/` |
|    261 | 4489 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 4490 | `#else` |
|      - | 4491 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 4492 | `        ** but leave the prefix for hex.*/` |
|      - | 4493 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 4494 | `#endif` |
|    261 | 4495 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|    237 | 4496 | `          if( iVal<0 ){` |
|     25 | 4497 | `            iVal = -iVal;` |
|      - | 4498 | `			/* Ticket 1433-003 */` |
|     25 | 4499 | `			if( iVal < 0 ){` |
|      - | 4500 | `				/* Overflow */` |
|    ! 0 | 4501 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4502 | `			}` |
|     25 | 4503 | `            prefix = '-';` |
|    225 | 4504 | `          }else if( flag_plussign )  prefix = '+';` |
|    211 | 4505 | `          else if( flag_blanksign )  prefix = ' ';` |
|    209 | 4506 | `          else                       prefix = 0;` |
|    119 | 4507 | `        }else{` |
|     25 | 4508 | `			if( iVal<0 ){` |
|    ! 0 | 4509 | `				iVal = -iVal;` |
|      - | 4510 | `				/* Ticket 1433-003 */` |
|    ! 0 | 4511 | `				if( iVal < 0 ){` |
|      - | 4512 | `					/* Overflow */` |
|    ! 0 | 4513 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4514 | `				}` |
|    ! 0 | 4515 | `			}` |
|     25 | 4516 | `			prefix = 0;` |
|      - | 4517 | `		}` |
|    261 | 4518 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|    149 | 4519 | `          precision = width-(prefix!=0);` |
|     74 | 4520 | `        }` |
|    261 | 4521 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 4522 | `        {` |
|      - | 4523 | `          register char *cset;      /* Use registers for speed */` |
|      - | 4524 | `          register int base;` |
|    261 | 4525 | `          cset = pInfo->charset;` |
|    261 | 4526 | `          base = pInfo->base;` |
|    130 | 4527 | `          do{                                           /* Convert to ascii */` |
|    333 | 4528 | `            *(--zBuf) = cset[iVal%base];` |
|    333 | 4529 | `            iVal = iVal/base;` |
|    333 | 4530 | `          }while( iVal>0 );` |
|      - | 4531 | `        }` |
|    261 | 4532 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    427 | 4533 | `        for(idx=precision-length; idx>0; idx--){` |
|    167 | 4534 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|     84 | 4535 | `        }` |
|    261 | 4536 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|    261 | 4537 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 4538 | `          char *pre, x;` |
|    ! 0 | 4539 | `          pre = pInfo->prefix;` |
|    ! 0 | 4540 | `          if( *zBuf!=pre[0] ){` |
|    ! 0 | 4541 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|    ! 0 | 4542 | `          }` |
|    ! 0 | 4543 | `        }` |
|    261 | 4544 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    261 | 4545 | `		break;` |
|     88 | 4546 | `		case PH7_FMT_FLOAT:` |
|      - | 4547 | `		case PH7_FMT_EXP:` |
|      - | 4548 | `		case PH7_FMT_GENERIC:{` |
|      - | 4549 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 4550 | `		double realvalue;` |
|      - | 4551 | `		char zFmt[8];` |
|      - | 4552 | `		int nOut, nFmt;` |
|    177 | 4553 | `		pArg = NEXT_ARG;` |
|    177 | 4554 | `		if( pArg == 0 ){` |
|    ! 0 | 4555 | `			realvalue = 0;` |
|    ! 0 | 4556 | `		}else{` |
|    177 | 4557 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 4558 | `		}` |
|      - | 4559 | `		/* php prints the IEEE specials bare — NaN / INF / -INF with no width` |
|      - | 4560 | `		 * padding, precision, or sign flags (php_sprintf_appenddouble). */` |
|    177 | 4561 | `		if( PH7_IS_NAN(realvalue) ){` |
|     21 | 4562 | `			zBuf = "NaN";` |
|     21 | 4563 | `			length = 3;` |
|     21 | 4564 | `			width = 0;` |
|     21 | 4565 | `			break;` |
|      - | 4566 | `		}` |
|    157 | 4567 | `		if( PH7_IS_INF(realvalue) ){` |
|     37 | 4568 | `			if( realvalue < 0.0 ){` |
|     15 | 4569 | `				zBuf = "-INF";` |
|     15 | 4570 | `				length = 4;` |
|      8 | 4571 | `			}else{` |
|     23 | 4572 | `				zBuf = "INF";` |
|     23 | 4573 | `				length = 3;` |
|      - | 4574 | `			}` |
|     37 | 4575 | `			width = 0;` |
|     37 | 4576 | `			break;` |
|      - | 4577 | `		}` |
|    121 | 4578 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|    121 | 4579 | `		if( precision > 53 ){` |
|      - | 4580 | `			/* php's FORMAT_CONV_MAX_PRECISION cap, with the same E_NOTICE` |
|      - | 4581 | `			 * (message prefixed with the active function's name, like` |
|      - | 4582 | `			 * php_error_docref). */` |
|      - | 4583 | `			char zMsg[160];` |
|      4 | 4584 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 4585 | `				"%z(): Requested precision of %d digits was truncated to PHP maximum of %d digits",` |
|      2 | 4586 | `				&pCtx->pFunc->sName,precision,53);` |
|      3 | 4587 | `			PH7_VmThrowError(pCtx->pVm,0,E_NOTICE,zMsg);` |
|      3 | 4588 | `			precision = 53;` |
|      1 | 4589 | `		}` |
|      - | 4590 | ``		/* php's %f/%e extract the sign via `num < 0`, so negative zero prints`` |
|      - | 4591 | `		 * unsigned there — while %g (php_gcvt on the raw value) keeps "-0". */` |
|    121 | 4592 | `		if( xtype!=PH7_FMT_GENERIC && realvalue == 0.0 ){` |
|      9 | 4593 | `			realvalue = 0.0;` |
|      4 | 4594 | `		}` |
|      - | 4595 | `		/* php's float conversions are correctly rounded (zend_dtoa); use libc` |
|      - | 4596 | `		 * snprintf as the digit engine (the byte-exact-floats rule — the old` |
|      - | 4597 | `		 * hand-rolled vxGetdigit loop stopped at 16 significant digits, so` |
|      - | 4598 | `		 * e.g. %f of 1e308 printed zeros where php prints the exact binary64` |
|      - | 4599 | `		 * expansion), then post-process into php's exact shapes below. */` |
|    121 | 4600 | `		nFmt = 0;` |
|    121 | 4601 | `		zFmt[nFmt++] = '%';` |
|    121 | 4602 | `		if( flag_alternateform ) zFmt[nFmt++] = '#';` |
|      - | 4603 | `		/* php's ' ' flag selects space PADDING (its default), not C's` |
|      - | 4604 | `		 * space-for-positive-sign — so flag_blanksign is NOT forwarded. */` |
|    121 | 4605 | `		if( flag_plussign ) zFmt[nFmt++] = '+';` |
|    121 | 4606 | `		zFmt[nFmt++] = '.';` |
|    121 | 4607 | `		zFmt[nFmt++] = '*';` |
|    165 | 4608 | `		zFmt[nFmt++] = (char)(xtype==PH7_FMT_FLOAT ? 'f' :` |
|     30 | 4609 | `			(xtype==PH7_FMT_EXP ? ((pInfo->charset[0]=='E') ? 'E' : 'e')` |
|     28 | 4610 | `			                    : ((pInfo->charset[0]=='E') ? 'G' : 'g')));` |
|    121 | 4611 | `		zFmt[nFmt] = 0;` |
|    121 | 4612 | `		nOut = snprintf(zWorker,sizeof(zWorker),zFmt,precision,realvalue);` |
|    121 | 4613 | `		if( nOut < 0 \|\| nOut >= (int)sizeof(zWorker) ){` |
|      - | 4614 | `			/* Cannot happen with precision capped at 53 (%f of DBL_MAX is` |
|      - | 4615 | `			 * ~365 bytes); keep the truncated output rather than overrun. */` |
|    ! 0 | 4616 | `			nOut = (int)SyStrlen(zWorker);` |
|    ! 0 | 4617 | `		}` |
|    121 | 4618 | `		nOut = (int)PH7_PhpFloatShape(zWorker,(sxi32)nOut,xtype==PH7_FMT_GENERIC);` |
|    121 | 4619 | `		zBuf = zWorker;` |
|    121 | 4620 | `		length = nOut;` |
|      - | 4621 | `		/* Let the zero-pad block below insert zeros between the sign (written` |
|      - | 4622 | `		 * by snprintf) and the first digit, as before. */` |
|    121 | 4623 | `		prefix = (zWorker[0]=='-' \|\| zWorker[0]=='+' \|\| zWorker[0]==' ') ? zWorker[0] : 0;` |
|      - | 4624 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 4625 | `        ** set and we are not left justified */` |
|    121 | 4626 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 4627 | `          int i;` |
|      7 | 4628 | `          int nPad = width - length;` |
|     51 | 4629 | `          for(i=width; i>=nPad; i--){` |
|     45 | 4630 | `            zBuf[i] = zBuf[i-nPad];` |
|     23 | 4631 | `          }` |
|      7 | 4632 | `          i = prefix!=0;` |
|     29 | 4633 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      7 | 4634 | `          length = width;` |
|      3 | 4635 | `        }` |
|      - | 4636 | `#else` |
|      - | 4637 | `         zBuf = " ";` |
|      - | 4638 | `		 length = (int)sizeof(char);` |
|      - | 4639 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|    121 | 4640 | `		 break;` |
|      - | 4641 | `							 }` |
|    ! 0 | 4642 | `		default:` |
|      - | 4643 | `			/* Unreachable: PH7_FormatValidate() rejects unknown specifiers with a` |
|      - | 4644 | `			 * catchable ValueError before formatting begins. Kept as a defensive` |
|      - | 4645 | `			 * no-op that emits nothing. */` |
|    ! 0 | 4646 | `			length = 0;` |
|    ! 0 | 4647 | `			break;` |
|      - | 4648 | `		}` |
|      - | 4649 | `		 /*` |
|      - | 4650 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 4651 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 4652 | `		 ** the output.` |
|      - | 4653 | `		 */` |
|    773 | 4654 | `    if( !flag_leftjustify ){` |
|      - | 4655 | `      register int nspace;` |
|    759 | 4656 | `      nspace = width-length;` |
|    759 | 4657 | `      if( nspace>0 ){` |
|      7 | 4658 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4659 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4660 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4661 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4662 | `			}` |
|    ! 0 | 4663 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4664 | `        }` |
|      7 | 4665 | `        if( nspace>0 ){` |
|      7 | 4666 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      7 | 4667 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4668 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4669 | `			}` |
|      3 | 4670 | `		}` |
|      3 | 4671 | `      }` |
|    379 | 4672 | `    }` |
|    773 | 4673 | `    if( length>0 ){` |
|    773 | 4674 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    773 | 4675 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4676 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4677 | `		}` |
|    386 | 4678 | `    }` |
|    773 | 4679 | `    if( flag_leftjustify ){` |
|      - | 4680 | `      register int nspace;` |
|     15 | 4681 | `      nspace = width-length;` |
|     15 | 4682 | `      if( nspace>0 ){` |
|     11 | 4683 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4684 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4685 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4686 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4687 | `			}` |
|    ! 0 | 4688 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4689 | `        }` |
|     11 | 4690 | `        if( nspace>0 ){` |
|     11 | 4691 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|     11 | 4692 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4693 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4694 | `			}` |
|      5 | 4695 | `		}` |
|      5 | 4696 | `      }` |
|      7 | 4697 | `    }` |
|      1 | 4698 | ` }/* for(;;) */` |
|    395 | 4699 | `	return SXRET_OK;` |
|    198 | 4700 | `}` |
|      - | 4701 | `/*` |
|      - | 4702 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 4703 | ` */` |
|    352 | 4704 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4705 | `{` |
|      - | 4706 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|      - | 4707 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|      - | 4708 | `	 * non-OK rc also stops the format loop. */` |
|    353 | 4709 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|    353 | 4710 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|    353 | 4711 | `	return *pRc;` |
|      1 | 4712 | `}` |
|      - | 4713 | `/*` |
|      - | 4714 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 4715 | ` *  Return a formatted string.` |
|      - | 4716 | ` * Parameters` |
|      - | 4717 | ` *  $format` |
|      - | 4718 | ` *    The format string (see block comment above)` |
|      - | 4719 | ` * Return` |
|      - | 4720 | ` *  A string produced according to the formatting string format.` |
|      - | 4721 | ` */` |
|    188 | 4722 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4723 | `{` |
|      - | 4724 | `	const char *zFormat;` |
|    189 | 4725 | `	sxi32 rc = SXRET_OK;` |
|      - | 4726 | `	int nLen;` |
|    189 | 4727 | `	if( nArg < 1 ){` |
|      - | 4728 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4729 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4730 | `		return PH7_OK;` |
|      - | 4731 | `	}` |
|      - | 4732 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|    189 | 4733 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    189 | 4734 | `	if( rc != PH7_OK ){` |
|      5 | 4735 | `		return rc;` |
|      - | 4736 | `	}` |
|      - | 4737 | `	/* Extract the string format (scalars/null coerce). */` |
|    185 | 4738 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    185 | 4739 | `	if( nLen < 1 ){` |
|      - | 4740 | `		/* Empty string */` |
|    ! 0 | 4741 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4742 | `		return PH7_OK;` |
|      - | 4743 | `	}` |
|      - | 4744 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4745 | `	 * output; propagate the throw status verbatim. */` |
|    185 | 4746 | `	rc = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    185 | 4747 | `	if( rc != PH7_OK ){` |
|     17 | 4748 | `		return rc;` |
|      - | 4749 | `	}` |
|      - | 4750 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|    169 | 4751 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|    169 | 4752 | `	if( rc != SXRET_OK ){` |
|      - | 4753 | `		/* The result append ran out of memory: raise a fatal rather than` |
|      - | 4754 | `		 * returning a silently-truncated string. */` |
|    ! 0 | 4755 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4756 | `	}` |
|    169 | 4757 | `	return PH7_OK;` |
|     95 | 4758 | `}` |
|      - | 4759 | `/*` |
|      - | 4760 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 4761 | ` */` |
|   1130 | 4762 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4763 | `{` |
|   1131 | 4764 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 4765 | `	/* Call the VM output consumer directly */` |
|   1131 | 4766 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 4767 | `	/* Increment counter */` |
|   1131 | 4768 | `	*pCounter += nLen;` |
|   1131 | 4769 | `	return PH7_OK;` |
|      1 | 4770 | `}` |
|      - | 4771 | `/*` |
|      - | 4772 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 4773 | ` *  Output a formatted string.` |
|      - | 4774 | ` * Parameters` |
|      - | 4775 | ` *  $format` |
|      - | 4776 | ` *   See sprintf() for a description of format.` |
|      - | 4777 | ` * Return` |
|      - | 4778 | ` *  The length of the outputted string.` |
|      - | 4779 | ` */` |
|    200 | 4780 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4781 | `{` |
|    201 | 4782 | `	ph7_int64 nCounter = 0;` |
|      - | 4783 | `	const char *zFormat;` |
|      - | 4784 | `	int nLen;` |
|    201 | 4785 | `	if( nArg < 1 ){` |
|      - | 4786 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4787 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4788 | `		return PH7_OK;` |
|      - | 4789 | `	}` |
|      - | 4790 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|      - | 4791 | `	{` |
|    201 | 4792 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    201 | 4793 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 4794 | `			return rcf;` |
|      - | 4795 | `		}` |
|      - | 4796 | `	}` |
|      - | 4797 | `	/* Extract the string format (scalars/null coerce). */` |
|    201 | 4798 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    201 | 4799 | `	if( nLen < 1 ){` |
|      - | 4800 | `		/* Empty string */` |
|    ! 0 | 4801 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4802 | `		return PH7_OK;` |
|      - | 4803 | `	}` |
|      - | 4804 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4805 | `	 * output; propagate the throw status verbatim. */` |
|      - | 4806 | `	{` |
|    201 | 4807 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    201 | 4808 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 4809 | `			return rcv;` |
|      - | 4810 | `		}` |
|      - | 4811 | `	}` |
|      - | 4812 | `	/* Format the string */` |
|    201 | 4813 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 4814 | `	/* Return the length of the outputted string */` |
|    201 | 4815 | `	ph7_result_int64(pCtx,nCounter);` |
|    201 | 4816 | `	return PH7_OK;` |
|    101 | 4817 | `}` |
|      - | 4818 | `/*` |
|      - | 4819 | ` * int vprintf(string $format,array $args)` |
|      - | 4820 | ` *  Output a formatted string.` |
|      - | 4821 | ` * Parameters` |
|      - | 4822 | ` *  $format` |
|      - | 4823 | ` *   See sprintf() for a description of format.` |
|      - | 4824 | ` * Return` |
|      - | 4825 | ` *  The length of the outputted string.` |
|      - | 4826 | ` */` |
|      4 | 4827 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4828 | `{` |
|      5 | 4829 | `	ph7_int64 nCounter = 0;` |
|      - | 4830 | `	const char *zFormat;` |
|      - | 4831 | `	ph7_hashmap *pMap;` |
|      - | 4832 | `	SySet sArg;` |
|      - | 4833 | `	int nLen,n;` |
|      - | 4834 | `	sxi32 rcFmt;` |
|      5 | 4835 | `	if( nArg < 2 ){` |
|      - | 4836 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4837 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4838 | `		return PH7_OK;` |
|      - | 4839 | `	}` |
|      - | 4840 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|      5 | 4841 | `	rcFmt = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|      5 | 4842 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4843 | `		return rcFmt;` |
|      - | 4844 | `	}` |
|      5 | 4845 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 4846 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 4847 | `		char zBuf[64];` |
|      4 | 4848 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4849 | `			"vprintf(): Argument #2 ($values) must be of type array, %s given",` |
|      2 | 4850 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 4851 | `	}` |
|      - | 4852 | `	/* Extract the string format (scalars/null coerce). */` |
|      3 | 4853 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4854 | `	if( nLen < 1 ){` |
|      - | 4855 | `		/* Empty string */` |
|    ! 0 | 4856 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4857 | `		return PH7_OK;` |
|      - | 4858 | `	}` |
|      - | 4859 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4860 | `	 * output; propagate the throw status verbatim. */` |
|      3 | 4861 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      3 | 4862 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4863 | `		return rcFmt;` |
|      - | 4864 | `	}` |
|      - | 4865 | `	/* Point to the hashmap */` |
|      3 | 4866 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4867 | `	/* Extract arguments from the hashmap */` |
|      3 | 4868 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4869 | `	/* Format the string */` |
|      3 | 4870 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 4871 | `	/* Release the container */` |
|      3 | 4872 | `	SySetRelease(&sArg);` |
|      - | 4873 | `	/* Return the length of the outputted string */` |
|      3 | 4874 | `	ph7_result_int64(pCtx,nCounter);` |
|      3 | 4875 | `	return PH7_OK;` |
|      3 | 4876 | `}` |
|      - | 4877 | `/*` |
|      - | 4878 | ` * int vsprintf(string $format,array $args)` |
|      - | 4879 | ` *  Output a formatted string.` |
|      - | 4880 | ` * Parameters` |
|      - | 4881 | ` *  $format` |
|      - | 4882 | ` *   See sprintf() for a description of format.` |
|      - | 4883 | ` * Return` |
|      - | 4884 | ` *  A string produced according to the formatting string format.` |
|      - | 4885 | ` */` |
|     22 | 4886 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4887 | `{` |
|      - | 4888 | `	const char *zFormat;` |
|      - | 4889 | `	ph7_hashmap *pMap;` |
|      - | 4890 | `	SySet sArg;` |
|     23 | 4891 | `	sxi32 rc = SXRET_OK;` |
|      - | 4892 | `	sxi32 rcFmt;` |
|      - | 4893 | `	int nLen,n;` |
|     23 | 4894 | `	if( nArg < 2 ){` |
|      - | 4895 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4896 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4897 | `		return PH7_OK;` |
|      - | 4898 | `	}` |
|      - | 4899 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|     23 | 4900 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|     23 | 4901 | `	if( rc != PH7_OK ){` |
|      5 | 4902 | `		return rc;` |
|      - | 4903 | `	}` |
|     19 | 4904 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 4905 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 4906 | `		char zBuf[64];` |
|     16 | 4907 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4908 | `			"vsprintf(): Argument #2 ($values) must be of type array, %s given",` |
|     10 | 4909 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 4910 | `	}` |
|      - | 4911 | `	/* Extract the string format (scalars/null coerce). */` |
|      9 | 4912 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 4913 | `	if( nLen < 1 ){` |
|      - | 4914 | `		/* Empty string */` |
|    ! 0 | 4915 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4916 | `		return PH7_OK;` |
|      - | 4917 | `	}` |
|      - | 4918 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4919 | `	 * output; propagate the throw status verbatim. */` |
|      9 | 4920 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      9 | 4921 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4922 | `		return rcFmt;` |
|      - | 4923 | `	}` |
|      - | 4924 | `	/* Point to hashmap */` |
|      9 | 4925 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4926 | `	/* Extract arguments from the hashmap */` |
|      9 | 4927 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4928 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|      9 | 4929 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|      - | 4930 | `	/* Release the container */` |
|      9 | 4931 | `	SySetRelease(&sArg);` |
|      9 | 4932 | `	if( rc != SXRET_OK ){` |
|      - | 4933 | `		/* The result append ran out of memory: raise a fatal. */` |
|    ! 0 | 4934 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4935 | `	}` |
|      9 | 4936 | `	return PH7_OK;` |
|     12 | 4937 | `}` |
|      - | 4938 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4939 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4940 | `/*` |
|      - | 4941 | ` * Symisc eXtension.` |
|      - | 4942 | ` * string size_format(int64 $size)` |
|      - | 4943 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 4944 | ` *  Example:` |
|      - | 4945 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 4946 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 4947 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 4948 | ` * Parameter` |
|      - | 4949 | ` *  $size` |
|      - | 4950 | ` *    Entity size in bytes.` |
|      - | 4951 | ` * Return` |
|      - | 4952 | ` *   Formatted string representation of the given size.` |
|      - | 4953 | ` */` |
|     24 | 4954 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4955 | `{` |
|      - | 4956 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 4957 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 4958 | `	sxi32 nRest,i_32;` |
|      - | 4959 | `	ph7_int64 iSize;` |
|     25 | 4960 | `	int c = -1; /* index in zUnit[] */` |
|      - | 4961 |  |
|     25 | 4962 | `	if( nArg < 1 ){` |
|      - | 4963 | `		/* Missing argument,return the empty string */` |
|      3 | 4964 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4965 | `		return PH7_OK;` |
|      - | 4966 | `	}` |
|      - | 4967 | `	/* Extract the given size */` |
|     23 | 4968 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 4969 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 4970 | `		/* Don't bother formatting,return immediately */` |
|      5 | 4971 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 4972 | `		return PH7_OK;` |
|      - | 4973 | `	}` |
|     19 | 4974 | `	for(;;){` |
|     39 | 4975 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 4976 | `		iSize >>= 10;` |
|     39 | 4977 | `		c++;` |
|     39 | 4978 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 4979 | `			break;` |
|      - | 4980 | `		}` |
|      1 | 4981 | `	}` |
|     19 | 4982 | `	nRest /= 100;` |
|     19 | 4983 | `	if( nRest > 9 ){` |
|    ! 0 | 4984 | `		nRest = 9;` |
|    ! 0 | 4985 | `	}` |
|     19 | 4986 | `	if( iSize > 999 ){` |
|    ! 0 | 4987 | `		c++;` |
|    ! 0 | 4988 | `		nRest = 9;` |
|    ! 0 | 4989 | `		iSize = 0;` |
|    ! 0 | 4990 | `	}` |
|     19 | 4991 | `	i_32 = (sxi32)iSize;` |
|      - | 4992 | `	/* Format */` |
|     19 | 4993 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 4994 | `	return PH7_OK;` |
|     13 | 4995 | `}` |
|      - | 4996 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4997 | `/*` |
|      - | 4998 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 4999 | ` *   Calculate the md5 hash of a string.` |
|      - | 5000 | ` * Parameter` |
|      - | 5001 | ` *  $str` |
|      - | 5002 | ` *   Input string` |
|      - | 5003 | ` * $raw_output` |
|      - | 5004 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 5005 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 5006 | ` * Return` |
|      - | 5007 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 5008 | ` */` |
|     12 | 5009 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5010 | `{` |
|      - | 5011 | `	unsigned char zDigest[16];` |
|     13 | 5012 | `	int raw_output = FALSE;` |
|      - | 5013 | `	const void *pIn;` |
|      - | 5014 | `	int nLen;` |
|     13 | 5015 | `	if( nArg < 1 ){` |
|      - | 5016 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5017 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5018 | `		return PH7_OK;` |
|      - | 5019 | `	}` |
|      - | 5020 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 5021 | `	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */` |
|     13 | 5022 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 5023 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 5024 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 5025 | `	}` |
|      - | 5026 | `	/* Compute the MD5 digest */` |
|     13 | 5027 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|     13 | 5028 | `	if( raw_output ){` |
|      - | 5029 | `		/* Output raw digest */` |
|      5 | 5030 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 5031 | `	}else{` |
|      - | 5032 | `		/* Perform a binary to hex conversion */` |
|      9 | 5033 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 5034 | `	}` |
|     13 | 5035 | `	return PH7_OK;` |
|      7 | 5036 | `}` |
|      - | 5037 | `/*` |
|      - | 5038 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 5039 | ` *   Calculate the sha1 hash of a string.` |
|      - | 5040 | ` * Parameter` |
|      - | 5041 | ` *  $str` |
|      - | 5042 | ` *   Input string` |
|      - | 5043 | ` * $raw_output` |
|      - | 5044 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 5045 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 5046 | ` * Return` |
|      - | 5047 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 5048 | ` */` |
|     10 | 5049 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5050 | `{` |
|      - | 5051 | `	unsigned char zDigest[20];` |
|     11 | 5052 | `	int raw_output = FALSE;` |
|      - | 5053 | `	const void *pIn;` |
|      - | 5054 | `	int nLen;` |
|     11 | 5055 | `	if( nArg < 1 ){` |
|      - | 5056 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5057 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5058 | `		return PH7_OK;` |
|      - | 5059 | `	}` |
|      - | 5060 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 5061 | `	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */` |
|     11 | 5062 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5063 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 5064 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 5065 | `	}` |
|      - | 5066 | `	/* Compute the SHA1 digest */` |
|     11 | 5067 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|     11 | 5068 | `	if( raw_output ){` |
|      - | 5069 | `		/* Output raw digest */` |
|      5 | 5070 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 5071 | `	}else{` |
|      - | 5072 | `		/* Perform a binary to hex conversion */` |
|      7 | 5073 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 5074 | `	}` |
|     11 | 5075 | `	return PH7_OK;` |
|      6 | 5076 | `}` |
|      - | 5077 | `/*` |
|      - | 5078 | ` * int64 crc32(string $str)` |
|      - | 5079 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 5080 | ` * Parameter` |
|      - | 5081 | ` *  $str` |
|      - | 5082 | ` *   Input string` |
|      - | 5083 | ` * Return` |
|      - | 5084 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 5085 | ` */` |
|      2 | 5086 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5087 | `{` |
|      - | 5088 | `	const void *pIn;` |
|      - | 5089 | `	sxu32 nCRC;` |
|      - | 5090 | `	int nLen;` |
|      3 | 5091 | `	if( nArg < 1 ){` |
|      - | 5092 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 5093 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5094 | `		return PH7_OK;` |
|      - | 5095 | `	}` |
|      - | 5096 | `	/* Extract the input string */` |
|      3 | 5097 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5098 | `	if( nLen < 1 ){` |
|      - | 5099 | `		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike` |
|      - | 5100 | `		 * md5()/sha1(), whose empty-string digests are non-zero. */` |
|    ! 0 | 5101 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5102 | `		return PH7_OK;` |
|      - | 5103 | `	}` |
|      - | 5104 | `	/* Calculate the sum */` |
|      3 | 5105 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 5106 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 5107 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 5108 | `	return PH7_OK;` |
|      2 | 5109 | `}` |
|      - | 5110 | `/*` |
|      - | 5111 | ` * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is` |
|      - | 5112 | ` * described by a small record so one dispatch (and one generic HMAC) serves them` |
|      - | 5113 | ` * all. Thin adapters normalize the differing context types and the reversed` |
|      - | 5114 | ` * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.` |
|      - | 5115 | ` */` |
|     11 | 5116 | `static void HashMd5Init(HashCtx *c){ MD5Init(&c->md5); }` |
|     15 | 5117 | `static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }` |
|     11 | 5118 | `static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }` |
|     11 | 5119 | `static void HashSha1Init(HashCtx *c){ SHA1Init(&c->sha1); }` |
|     15 | 5120 | `static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }` |
|     11 | 5121 | `static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }` |
|      9 | 5122 | `static void HashSha224Init(HashCtx *c){ SHA224Init(&c->sha256); }` |
|     33 | 5123 | `static void HashSha256Init(HashCtx *c){ SHA256Init(&c->sha256); }` |
|     57 | 5124 | `static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }` |
|     41 | 5125 | `static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }` |
|      9 | 5126 | `static void HashSha384Init(HashCtx *c){ SHA384Init(&c->sha512); }` |
|     15 | 5127 | `static void HashSha512Init(HashCtx *c){ SHA512Init(&c->sha512); }` |
|     27 | 5128 | `static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }` |
|     23 | 5129 | `static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }` |
|      - | 5130 | `typedef struct HashAlgo HashAlgo;` |
|      - | 5131 | `struct HashAlgo {` |
|      - | 5132 | `	const char *zName;   /* lowercase canonical name */` |
|      - | 5133 | `	int nDigestLen;      /* output bytes: 16/20/28/32/48/64 */` |
|      - | 5134 | `	int nBlockLen;       /* internal block bytes (for HMAC): 64 or 128 */` |
|      - | 5135 | `	void (*xInit)(HashCtx *);` |
|      - | 5136 | `	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);` |
|      - | 5137 | `	void (*xFinal)(HashCtx *,unsigned char *);` |
|      - | 5138 | `};` |
|      - | 5139 | `static const HashAlgo aHashAlgo[] = {` |
|      - | 5140 | `	{ "md5",    16, 64,  HashMd5Init,    HashMd5Update,    HashMd5Final    },` |
|      - | 5141 | `	{ "sha1",   20, 64,  HashSha1Init,   HashSha1Update,   HashSha1Final   },` |
|      - | 5142 | `	{ "sha224", 28, 64,  HashSha224Init, HashSha256Update, HashSha256Final },` |
|      - | 5143 | `	{ "sha256", 32, 64,  HashSha256Init, HashSha256Update, HashSha256Final },` |
|      - | 5144 | `	{ "sha384", 48, 128, HashSha384Init, HashSha512Update, HashSha512Final },` |
|      - | 5145 | `	{ "sha512", 64, 128, HashSha512Init, HashSha512Update, HashSha512Final },` |
|      - | 5146 | `};` |
|      - | 5147 | `/* Case-insensitive algorithm lookup (PHP accepts 'SHA256' etc.). */` |
|     73 | 5148 | `static const HashAlgo * HashFindAlgo(const char *zName,int nLen){` |
|      - | 5149 | `	sxu32 i;` |
|    279 | 5150 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|    272 | 5151 | `		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen` |
|    211 | 5152 | `			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){` |
|     67 | 5153 | `			return &aHashAlgo[i];` |
|      - | 5154 | `		}` |
|    106 | 5155 | `	}` |
|      6 | 5156 | `	return 0;` |
|     38 | 5157 | `}` |
|      - | 5158 | `/*` |
|      - | 5159 | ` * string hash(string $algo,string $data[,bool $binary = false])` |
|      - | 5160 | ` *   Generate a hash value (message digest).` |
|      - | 5161 | ` */` |
|     54 | 5162 | `static int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5163 | `{` |
|      - | 5164 | `	const HashAlgo *pAlgo;` |
|      - | 5165 | `	const char *zAlgo,*zData;` |
|     56 | 5166 | `	int nAlgoLen,nDataLen,raw_output = FALSE;` |
|      - | 5167 | `	HashCtx sCtx;` |
|      - | 5168 | `	unsigned char zDigest[64];` |
|     56 | 5169 | `	if( nArg < 2 ){` |
|    ! 0 | 5170 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5171 | `			"hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5172 | `	}` |
|     56 | 5173 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     56 | 5174 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     56 | 5175 | `	if( pAlgo == 0 ){` |
|      3 | 5176 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5177 | `			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - | 5178 | `	}` |
|     53 | 5179 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     53 | 5180 | `	if( nArg > 2 ){` |
|      9 | 5181 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|      4 | 5182 | `	}` |
|     53 | 5183 | `	pAlgo->xInit(&sCtx);` |
|     53 | 5184 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     53 | 5185 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     53 | 5186 | `	if( raw_output ){` |
|      9 | 5187 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      5 | 5188 | `	}else{` |
|     45 | 5189 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 5190 | `	}` |
|     53 | 5191 | `	return PH7_OK;` |
|     29 | 5192 | `}` |
|      - | 5193 | `/*` |
|      - | 5194 | ` * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])` |
|      - | 5195 | ` *   Generate a keyed hash value using the HMAC method (RFC 2104).` |
|      - | 5196 | ` */` |
|     16 | 5197 | `static int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5198 | `{` |
|      - | 5199 | `	const HashAlgo *pAlgo;` |
|      - | 5200 | `	const char *zAlgo,*zData,*zKey;` |
|     18 | 5201 | `	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;` |
|      - | 5202 | `	HashCtx sCtx;` |
|      - | 5203 | `	unsigned char zKeyBlock[128],zIpad[128],zOpad[128],zInner[64],zDigest[64];` |
|      - | 5204 | `	int i,nBlock,nDigest;` |
|     18 | 5205 | `	if( nArg < 3 ){` |
|    ! 0 | 5206 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5207 | `			"hash_hmac() expects at least 3 arguments, %d given",nArg);` |
|      - | 5208 | `	}` |
|     18 | 5209 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     18 | 5210 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     18 | 5211 | `	if( pAlgo == 0 ){` |
|      3 | 5212 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5213 | `			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 5214 | `	}` |
|     15 | 5215 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     15 | 5216 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|     15 | 5217 | `	if( nArg > 3 ){` |
|      3 | 5218 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|      1 | 5219 | `	}` |
|     15 | 5220 | `	nBlock = pAlgo->nBlockLen;` |
|     15 | 5221 | `	nDigest = pAlgo->nDigestLen;` |
|      - | 5222 | `	/* Reduce the key to a single block: hash it if longer than the block, then` |
|      - | 5223 | `	 * zero-pad (a short or empty key is just zero-padded). */` |
|     15 | 5224 | `	SyZero(zKeyBlock,sizeof(zKeyBlock));` |
|     15 | 5225 | `	if( nKeyLen > nBlock ){` |
|      3 | 5226 | `		pAlgo->xInit(&sCtx);` |
|      3 | 5227 | `		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);` |
|      3 | 5228 | `		pAlgo->xFinal(&sCtx,zKeyBlock);` |
|     14 | 5229 | `	}else if( nKeyLen > 0 ){` |
|     11 | 5230 | `		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);` |
|      5 | 5231 | `	}` |
|   1039 | 5232 | `	for( i = 0; i < nBlock; i++ ){` |
|   1025 | 5233 | `		zIpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);` |
|   1025 | 5234 | `		zOpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);` |
|    513 | 5235 | `	}` |
|      - | 5236 | `	/* inner = H((key ^ ipad) \|\| data) */` |
|     15 | 5237 | `	pAlgo->xInit(&sCtx);` |
|     15 | 5238 | `	pAlgo->xUpdate(&sCtx,zIpad,(unsigned int)nBlock);` |
|     15 | 5239 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     15 | 5240 | `	pAlgo->xFinal(&sCtx,zInner);` |
|      - | 5241 | `	/* out = H((key ^ opad) \|\| inner) */` |
|     15 | 5242 | `	pAlgo->xInit(&sCtx);` |
|     15 | 5243 | `	pAlgo->xUpdate(&sCtx,zOpad,(unsigned int)nBlock);` |
|     15 | 5244 | `	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)nDigest);` |
|     15 | 5245 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     15 | 5246 | `	if( raw_output ){` |
|      3 | 5247 | `		ph7_result_string(pCtx,(const char *)zDigest,nDigest);` |
|      2 | 5248 | `	}else{` |
|     13 | 5249 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);` |
|      - | 5250 | `	}` |
|     15 | 5251 | `	return PH7_OK;` |
|     10 | 5252 | `}` |
|      - | 5253 | `/*` |
|      - | 5254 | ` * bool hash_equals(string $known_string,string $user_string)` |
|      - | 5255 | ` *   Timing-attack-safe string comparison.` |
|      - | 5256 | ` */` |
|     14 | 5257 | `static int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 5258 | `{` |
|      - | 5259 | `	const char *zKnown,*zUser;` |
|      - | 5260 | `	int nKnown,nUser,i;` |
|     17 | 5261 | `	volatile unsigned char vDiff = 0;` |
|     17 | 5262 | `	if( nArg < 2 ){` |
|    ! 0 | 5263 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5264 | `			"hash_equals() expects exactly 2 arguments, %d given",nArg);` |
|      - | 5265 | `	}` |
|     17 | 5266 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      4 | 5267 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5268 | `			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",` |
|      1 | 5269 | `			ph7_type_name(apArg[0]));` |
|      - | 5270 | `	}` |
|     14 | 5271 | `	if( !ph7_value_is_string(apArg[1]) ){` |
|      4 | 5272 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5273 | `			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",` |
|      2 | 5274 | `			ph7_type_name(apArg[1]));` |
|      - | 5275 | `	}` |
|     11 | 5276 | `	zKnown = ph7_value_to_string(apArg[0],&nKnown);` |
|     11 | 5277 | `	zUser = ph7_value_to_string(apArg[1],&nUser);` |
|     11 | 5278 | `	if( nKnown != nUser ){` |
|      5 | 5279 | `		ph7_result_bool(pCtx,0);` |
|      5 | 5280 | `		return PH7_OK;` |
|      - | 5281 | `	}` |
|      - | 5282 | `	/* Constant-time: read every byte, never short-circuit. */` |
|     19 | 5283 | `	for( i = 0; i < nKnown; i++ ){` |
|     13 | 5284 | `		vDiff \|= (unsigned char)(zKnown[i] ^ zUser[i]);` |
|      7 | 5285 | `	}` |
|      7 | 5286 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|      7 | 5287 | `	return PH7_OK;` |
|     10 | 5288 | `}` |
|      - | 5289 | `/*` |
|      - | 5290 | ` * array hash_algos(void)` |
|      - | 5291 | ` *   Return a list of the registered hashing algorithms.` |
|      - | 5292 | ` */` |
|      2 | 5293 | `static int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5294 | `{` |
|      - | 5295 | `	ph7_value *pArray,*pValue;` |
|      - | 5296 | `	sxu32 i;` |
|      1 | 5297 | `	SXUNUSED(nArg);` |
|      1 | 5298 | `	SXUNUSED(apArg);` |
|      3 | 5299 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5300 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 5301 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 5302 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5303 | `		return PH7_OK;` |
|      - | 5304 | `	}` |
|     15 | 5305 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|     13 | 5306 | `		ph7_value_string(pValue,aHashAlgo[i].zName,-1);` |
|     13 | 5307 | `		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);` |
|     13 | 5308 | `		ph7_value_reset_string_cursor(pValue);` |
|      7 | 5309 | `	}` |
|      3 | 5310 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5311 | `	return PH7_OK;` |
|      2 | 5312 | `}` |
|      - | 5313 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 5314 | `/*` |
|      - | 5315 | ` * password_* (bcrypt). These live in ext/standard in real PHP — outside the` |
|      - | 5316 | ` * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.` |
|      - | 5317 | ` */` |
|      - | 5318 | `/*` |
|      - | 5319 | ` * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a` |
|      - | 5320 | ` * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).` |
|      - | 5321 | ` */` |
|     40 | 5322 | `static int BcryptParseHash(const char *zHash,int nHash,int *piCost)` |
|      1 | 5323 | `{` |
|      - | 5324 | `	int iCost;` |
|     40 | 5325 | `	if( nHash != 60 \|\| zHash[0] != '$' \|\| zHash[1] != '2' \|\| zHash[3] != '$'` |
|     29 | 5326 | `		\|\| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){` |
|     13 | 5327 | `		return FALSE;` |
|      - | 5328 | `	}` |
|     29 | 5329 | `	if( zHash[4] < '0' \|\| zHash[4] > '9' \|\| zHash[5] < '0' \|\| zHash[5] > '9' \|\| zHash[6] != '$' ){` |
|    ! 0 | 5330 | `		return FALSE;` |
|      - | 5331 | `	}` |
|     29 | 5332 | `	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');` |
|     29 | 5333 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      3 | 5334 | `		return FALSE;` |
|      - | 5335 | `	}` |
|     27 | 5336 | `	if( piCost ){ *piCost = iCost; }` |
|     27 | 5337 | `	return TRUE;` |
|     21 | 5338 | `}` |
|      - | 5339 | `/*` |
|      - | 5340 | ` * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the` |
|      - | 5341 | ` * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.` |
|      - | 5342 | ` */` |
|     20 | 5343 | `static int BcryptIsBcryptAlgo(ph7_value *pAlgo)` |
|      3 | 5344 | `{` |
|     23 | 5345 | `	if( ph7_value_is_null(pAlgo) ){` |
|    ! 0 | 5346 | `		return TRUE;` |
|      - | 5347 | `	}` |
|     23 | 5348 | `	if( ph7_value_is_string(pAlgo) ){` |
|      - | 5349 | `		int nAlgo;` |
|     23 | 5350 | `		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);` |
|     23 | 5351 | `		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );` |
|      - | 5352 | `	}` |
|    ! 0 | 5353 | `	return FALSE;` |
|     13 | 5354 | `}` |
|      - | 5355 | `/*` |
|      - | 5356 | ` * bool\|string password_hash(string $password,string\|int\|null $algo[,array $options])` |
|      - | 5357 | ` *  Create a bcrypt hash of the password.` |
|      - | 5358 | ` */` |
|     16 | 5359 | `static int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 5360 | `{` |
|      - | 5361 | `	const char *zPwd;` |
|     19 | 5362 | `	int nPwd,iCost = 12;` |
|      - | 5363 | `	unsigned char aSalt[16];` |
|      - | 5364 | `	char zHash[60];` |
|     19 | 5365 | `	if( nArg < 2 ){` |
|    ! 0 | 5366 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5367 | `			"password_hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5368 | `	}` |
|     19 | 5369 | `	if( !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      3 | 5370 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5371 | `			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");` |
|      - | 5372 | `	}` |
|      - | 5373 | `	/* cost from $options['cost'] (default 12). */` |
|     16 | 5374 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|     14 | 5375 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     14 | 5376 | `		if( pCost ){ iCost = ph7_value_to_int(pCost); }` |
|      6 | 5377 | `	}` |
|     16 | 5378 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      4 | 5379 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 5380 | `			"Invalid bcrypt cost parameter specified: %d",iCost);` |
|      - | 5381 | `	}` |
|     13 | 5382 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     13 | 5383 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 5384 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 5385 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 5386 | `	}` |
|     13 | 5387 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){` |
|    ! 0 | 5388 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5389 | `		return PH7_OK;` |
|      - | 5390 | `	}` |
|     13 | 5391 | `	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));` |
|     13 | 5392 | `	return PH7_OK;` |
|     11 | 5393 | `}` |
|      - | 5394 | `/*` |
|      - | 5395 | ` * bool password_verify(string $password,string $hash)` |
|      - | 5396 | ` *  Verify a password against a bcrypt hash. Never throws on a malformed hash.` |
|      - | 5397 | ` */` |
|     28 | 5398 | `static int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5399 | `{` |
|      - | 5400 | `	const char *zPwd,*zHash;` |
|      - | 5401 | `	int nPwd,nHash,iCost,i;` |
|      - | 5402 | `	unsigned char aSalt[16];` |
|      - | 5403 | `	char zComputed[60];` |
|     29 | 5404 | `	volatile unsigned char vDiff = 0;` |
|     29 | 5405 | `	if( nArg < 2 ){` |
|    ! 0 | 5406 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5407 | `			"password_verify() expects exactly 2 arguments, %d given",nArg);` |
|      - | 5408 | `	}` |
|     29 | 5409 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     29 | 5410 | `	zHash = ph7_value_to_string(apArg[1],&nHash);` |
|     29 | 5411 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|     11 | 5412 | `		ph7_result_bool(pCtx,0);` |
|     11 | 5413 | `		return PH7_OK;` |
|      - | 5414 | `	}` |
|      - | 5415 | `	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */` |
|     19 | 5416 | `	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 5417 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5418 | `		return PH7_OK;` |
|      - | 5419 | `	}` |
|     19 | 5420 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){` |
|    ! 0 | 5421 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5422 | `		return PH7_OK;` |
|      - | 5423 | `	}` |
|      - | 5424 | `	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps` |
|      - | 5425 | `	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */` |
|    577 | 5426 | `	for( i = 29; i < 60; i++ ){` |
|    559 | 5427 | `		vDiff \|= (unsigned char)(zComputed[i] ^ zHash[i]);` |
|    280 | 5428 | `	}` |
|     19 | 5429 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|     19 | 5430 | `	return PH7_OK;` |
|     15 | 5431 | `}` |
|      - | 5432 | `/*` |
|      - | 5433 | ` * array password_get_info(string $hash)` |
|      - | 5434 | ` *  Return ["algo"=>id\|null, "algoName"=>name, "options"=>[...]].` |
|      - | 5435 | ` */` |
|      6 | 5436 | `static int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5437 | `{` |
|      7 | 5438 | `	const char *zHash = "";` |
|      7 | 5439 | `	int nHash,iCost = 0,bBcrypt = 0;` |
|      - | 5440 | `	ph7_value *pArray,*pOptions,*pVal;` |
|      7 | 5441 | `	if( nArg > 0 ){` |
|      7 | 5442 | `		zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 5443 | `		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);` |
|      3 | 5444 | `	}` |
|      7 | 5445 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 5446 | `	pOptions = ph7_context_new_array(pCtx);` |
|      7 | 5447 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      7 | 5448 | `	if( pArray == 0 \|\| pOptions == 0 \|\| pVal == 0 ){` |
|    ! 0 | 5449 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5450 | `		return PH7_OK;` |
|      - | 5451 | `	}` |
|      7 | 5452 | `	if( bBcrypt ){` |
|      5 | 5453 | `		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */` |
|      5 | 5454 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 5455 | `		ph7_value_reset_string_cursor(pVal);` |
|      5 | 5456 | `		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);` |
|      5 | 5457 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 5458 | `		ph7_value_int(pVal,iCost);` |
|      5 | 5459 | `		ph7_array_add_strkey_elem(pOptions,"cost",pVal);` |
|      3 | 5460 | `	}else{` |
|      3 | 5461 | `		ph7_value_null(pVal);                          /* algo => null */` |
|      3 | 5462 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      3 | 5463 | `		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);` |
|      3 | 5464 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      - | 5465 | `	}` |
|      7 | 5466 | `	ph7_array_add_strkey_elem(pArray,"options",pOptions);` |
|      7 | 5467 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 5468 | `	return PH7_OK;` |
|      4 | 5469 | `}` |
|      - | 5470 | `/*` |
|      - | 5471 | ` * bool password_needs_rehash(string $hash,string\|int\|null $algo[,array $options])` |
|      - | 5472 | ` *  True if the hash was not made with the given algo/options.` |
|      - | 5473 | ` */` |
|      6 | 5474 | `static int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5475 | `{` |
|      - | 5476 | `	const char *zHash;` |
|      7 | 5477 | `	int nHash,iCost = 0,iWantCost = 12;` |
|      7 | 5478 | `	if( nArg < 2 ){` |
|    ! 0 | 5479 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5480 | `			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5481 | `	}` |
|      7 | 5482 | `	zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 5483 | `	if( !BcryptParseHash(zHash,nHash,&iCost) \|\| !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      - | 5484 | `		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */` |
|      3 | 5485 | `		ph7_result_bool(pCtx,1);` |
|      3 | 5486 | `		return PH7_OK;` |
|      - | 5487 | `	}` |
|      5 | 5488 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      5 | 5489 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|      5 | 5490 | `		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }` |
|      2 | 5491 | `	}` |
|      5 | 5492 | `	ph7_result_bool(pCtx,iCost != iWantCost);` |
|      5 | 5493 | `	return PH7_OK;` |
|      4 | 5494 | `}` |
|      - | 5495 | `/*` |
|      - | 5496 | ` * filter_var() — input validation and sanitization (the ext/filter API).` |
|      - | 5497 | ` *` |
|      - | 5498 | ` * Filter and flag identifiers (values match PHP 8.5; the constants themselves` |
|      - | 5499 | ` * are registered in constant.c). The validate filters are hand-rolled rather` |
|      - | 5500 | ` * than delegating to SyStrToInt64/SyStrToReal: the former silently skips leading` |
|      - | 5501 | ` * zeros and cannot signal overflow, and the latter treats ',' as a decimal point` |
|      - | 5502 | ` * unconditionally — neither matches PHP's filter semantics.` |
|      - | 5503 | ` */` |
|      - | 5504 | `#define FV_VALIDATE_INT     257` |
|      - | 5505 | `#define FV_VALIDATE_BOOLEAN 258` |
|      - | 5506 | `#define FV_VALIDATE_FLOAT   259` |
|      - | 5507 | `#define FV_VALIDATE_REGEXP  272` |
|      - | 5508 | `#define FV_VALIDATE_URL     273` |
|      - | 5509 | `#define FV_VALIDATE_EMAIL   274` |
|      - | 5510 | `#define FV_VALIDATE_IP      275` |
|      - | 5511 | `#define FV_VALIDATE_MAC     276` |
|      - | 5512 | `#define FV_VALIDATE_DOMAIN  277` |
|      - | 5513 | `#define FV_SANITIZE_SPECIAL_CHARS      515` |
|      - | 5514 | `#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */` |
|      - | 5515 | `#define FV_SANITIZE_EMAIL   517` |
|      - | 5516 | `#define FV_SANITIZE_URL     518` |
|      - | 5517 | `#define FV_SANITIZE_NUMBER_INT   519` |
|      - | 5518 | `#define FV_SANITIZE_NUMBER_FLOAT 520` |
|      - | 5519 | `#define FV_SANITIZE_FULL_SPECIAL_CHARS 522` |
|      - | 5520 | `#define FV_FLAG_ALLOW_OCTAL  1` |
|      - | 5521 | `#define FV_FLAG_ALLOW_HEX    2` |
|      - | 5522 | `#define FV_FLAG_STRIP_LOW    4` |
|      - | 5523 | `#define FV_FLAG_STRIP_HIGH   8` |
|      - | 5524 | `#define FV_FLAG_ENCODE_LOW   16` |
|      - | 5525 | `#define FV_FLAG_ENCODE_HIGH  32` |
|      - | 5526 | `#define FV_FLAG_ENCODE_AMP   64` |
|      - | 5527 | `#define FV_FLAG_NO_ENCODE_QUOTES 128` |
|      - | 5528 | `#define FV_FLAG_STRIP_BACKTICK   512` |
|      - | 5529 | `#define FV_FLAG_ALLOW_FRACTION   4096` |
|      - | 5530 | `#define FV_FLAG_ALLOW_THOUSAND   8192` |
|      - | 5531 | `#define FV_FLAG_ALLOW_SCIENTIFIC 16384` |
|      - | 5532 | `#define FV_FLAG_IPV4  1048576` |
|      - | 5533 | `#define FV_FLAG_IPV6  2097152` |
|      - | 5534 | `#define FV_NULL_ON_FAILURE 134217728` |
|      - | 5535 | `/* The subset of flags the UNSAFE_RAW/DEFAULT string filter (FvSanitizeString)` |
|      - | 5536 | ` * acts on: when none are set the filter is a verbatim pass-through, so FV_DEFAULT` |
|      - | 5537 | ` * can shortcut. Keep this in sync with FvSanitizeString's flag handling. */` |
|      - | 5538 | `#define FV_FLAG_STRING_MASK (FV_FLAG_STRIP_LOW\|FV_FLAG_STRIP_HIGH\|FV_FLAG_STRIP_BACKTICK \` |
|      - | 5539 | `                            \|FV_FLAG_ENCODE_LOW\|FV_FLAG_ENCODE_HIGH\|FV_FLAG_ENCODE_AMP)` |
|      - | 5540 |  |
|      - | 5541 | `/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.` |
|      - | 5542 | ` * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */` |
|    153 | 5543 | `static void FvTrim(const char **pz,int *pn){` |
|    153 | 5544 | `	const char *z = *pz;` |
|    153 | 5545 | `	int n = *pn;` |
|    157 | 5546 | `	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }` |
|    161 | 5547 | `	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }` |
|    153 | 5548 | `	*pz = z; *pn = n;` |
|    153 | 5549 | `}` |
|      - | 5550 | `/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     57 | 5551 | `static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){` |
|     57 | 5552 | `	int neg = 0, i;` |
|     57 | 5553 | `	sxu64 u = 0;` |
|     57 | 5554 | `	FvTrim(&z,&n);` |
|     57 | 5555 | `	if( n==0 ){ return 0; }` |
|     51 | 5556 | `	if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }` |
|     51 | 5557 | `	if( n==0 ){ return 0; }` |
|     49 | 5558 | `	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'\|\|z[1]=='X') ){` |
|      3 | 5559 | `		z += 2; n -= 2;` |
|      3 | 5560 | `		if( n==0 ){ return 0; }` |
|      7 | 5561 | `		for( i=0; i<n; i++ ){` |
|      5 | 5562 | `			int h = SyHexToint((unsigned char)z[i]);` |
|      5 | 5563 | `			if( h<0 ){ return 0; }` |
|      5 | 5564 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }` |
|      5 | 5565 | `			u = u*16 + (sxu64)h;` |
|      3 | 5566 | `		}` |
|     48 | 5567 | `	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){` |
|      9 | 5568 | `		for( i=0; i<n; i++ ){` |
|      7 | 5569 | `			if( z[i]<'0' \|\| z[i]>'7' ){ return 0; }` |
|      7 | 5570 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }` |
|      7 | 5571 | `			u = u*8 + (sxu64)(z[i]-'0');` |
|      4 | 5572 | `		}` |
|      2 | 5573 | `	}else{` |
|     45 | 5574 | `		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */` |
|    201 | 5575 | `		for( i=0; i<n; i++ ){` |
|    173 | 5576 | `			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|    161 | 5577 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }` |
|    161 | 5578 | `			u = u*10 + (sxu64)(z[i]-'0');` |
|     81 | 5579 | `		}` |
|      - | 5580 | `	}` |
|     33 | 5581 | `	if( neg ){` |
|      5 | 5582 | `		if( u > 0x8000000000000000ULL ){ return 0; }` |
|      5 | 5583 | `		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */` |
|      3 | 5584 | `	}else{` |
|     29 | 5585 | `		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }` |
|     27 | 5586 | `		*pOut = (ph7_int64)u;` |
|      - | 5587 | `	}` |
|     31 | 5588 | `	return 1;` |
|     29 | 5589 | `}` |
|      - | 5590 | `/* FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     69 | 5591 | `static int FvValidateFloat(const char *z,int n,int flags,double *pOut){` |
|      - | 5592 | `	char zBuf[512];` |
|     69 | 5593 | `	int i, m = 0, seenDigit = 0;` |
|     69 | 5594 | `	const char *zv; int nv; double d = 0;` |
|     69 | 5595 | `	FvTrim(&z,&n);` |
|      - | 5596 | `	/* Bound the input: zBuf[512] holds the thousand-separator-stripped copy, and` |
|      - | 5597 | `	 * the cap also rejects the pathological 500+ digit floats PHP refuses. */` |
|     69 | 5598 | `	if( n==0 \|\| n>500 ){ return 0; }` |
|     69 | 5599 | `	if( flags & FV_FLAG_ALLOW_THOUSAND ){` |
|      - | 5600 | `		/* Commas are optional, but when present they must group the integer part` |
|      - | 5601 | `		 * into a leading run of 1..3 digits followed by groups of exactly 3` |
|      - | 5602 | `		 * ("1,000" ok, "1,5"/"1234,567" rejected). Strip them into zBuf and reject` |
|      - | 5603 | `		 * a comma anywhere in the fractional/exponent tail. */` |
|     25 | 5604 | `		int s = 0, intEnd, segStart, segIdx, hasComma = 0;` |
|     25 | 5605 | `		if( s<n && (z[s]=='+'\|\|z[s]=='-') ){ zBuf[m++] = z[s]; s++; }` |
|     25 | 5606 | `		intEnd = s;` |
|    167 | 5607 | `		while( intEnd<n && z[intEnd]!='.' && z[intEnd]!='e' && z[intEnd]!='E' ){` |
|    143 | 5608 | `			if( z[intEnd]==',' ){ hasComma = 1; }` |
|    143 | 5609 | `			intEnd++;` |
|      1 | 5610 | `		}` |
|     25 | 5611 | `		if( hasComma ){` |
|     25 | 5612 | `			segStart = s; segIdx = 0;` |
|    165 | 5613 | `			for( i=s; i<=intEnd; i++ ){` |
|    151 | 5614 | `				if( i==intEnd \|\| z[i]==',' ){` |
|     49 | 5615 | `					int segLen = i - segStart, k;` |
|     49 | 5616 | `					if( segIdx==0 ){ if( segLen<1 \|\| segLen>3 ){ return 0; } }` |
|     25 | 5617 | `					else if( segLen!=3 ){ return 0; }` |
|    119 | 5618 | `					for( k=segStart; k<i; k++ ){` |
|     81 | 5619 | `						if( !SyisDigit((unsigned char)z[k]) ){ return 0; }` |
|     81 | 5620 | `						zBuf[m++] = z[k];` |
|     41 | 5621 | `					}` |
|     39 | 5622 | `					segStart = i+1; segIdx++;` |
|     19 | 5623 | `				}` |
|     71 | 5624 | `			}` |
|      8 | 5625 | `		}else{` |
|    ! 0 | 5626 | `			for( i=s; i<intEnd; i++ ){ zBuf[m++] = z[i]; }` |
|      - | 5627 | `		}` |
|     27 | 5628 | `		for( i=intEnd; i<n; i++ ){` |
|     13 | 5629 | `			if( z[i]==',' ){ return 0; }` |
|     13 | 5630 | `			zBuf[m++] = z[i];` |
|      7 | 5631 | `		}` |
|     15 | 5632 | `		zv = zBuf; nv = m;` |
|      8 | 5633 | `	}else{` |
|     45 | 5634 | `		zv = z; nv = n;` |
|      - | 5635 | `	}` |
|     59 | 5636 | `	i = 0;` |
|     59 | 5637 | `	if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|    167 | 5638 | `	while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     59 | 5639 | `	if( i<nv && zv[i]=='.' ){` |
|     21 | 5640 | `		i++;` |
|     39 | 5641 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     10 | 5642 | `	}` |
|     59 | 5643 | `	if( !seenDigit ){ return 0; }` |
|     57 | 5644 | `	if( i<nv && (zv[i]=='e'\|\|zv[i]=='E') ){` |
|     29 | 5645 | `		i++;` |
|     29 | 5646 | `		if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|     29 | 5647 | `		if( i>=nv \|\| !SyisDigit((unsigned char)zv[i]) ){ return 0; }` |
|    105 | 5648 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; }` |
|     14 | 5649 | `	}` |
|     57 | 5650 | `	if( i!=nv ){ return 0; } /* trailing junk */` |
|      - | 5651 | `	/* The grammar above guarantees zv[0..nv) is a clean ASCII decimal float (no hex /` |
|      - | 5652 | `	 * inf / nan / trailing junk), so it is safe to hand to libc strtod, which — unlike` |
|      - | 5653 | `	 * SyStrToReal (15 sig-digits + exponent clamped to 308, so it silently saturates` |
|      - | 5654 | `	 * overflowing magnitudes to a finite value) — is overflow/underflow-aware and` |
|      - | 5655 | `	 * correctly rounded. strtod needs a NUL-terminated string: the ALLOW_THOUSAND path` |
|      - | 5656 | `	 * already built the span in zBuf (zv==zBuf); the plain path must copy it there (z is` |
|      - | 5657 | `	 * const + not NUL-terminated). nv <= n <= 500 < sizeof(zBuf) by the cap above.` |
|      - | 5658 | `	 * Matches PHP 8.5 byte-for-byte: reject overflow (-> +/-INF) and total underflow` |
|      - | 5659 | `	 * (-> 0.0), keep subnormals (nonzero, errno==ERANGE) and a genuine "0" (errno==0). */` |
|     53 | 5660 | `	if( zv != zBuf ){ SyMemcpy(zv,zBuf,(sxu32)nv); }` |
|     53 | 5661 | `	zBuf[nv] = 0;` |
|     53 | 5662 | `	errno = 0;` |
|     53 | 5663 | `	d = strtod(zBuf,0);` |
|     53 | 5664 | `	if( errno == ERANGE && (d == HUGE_VAL \|\| d == -HUGE_VAL \|\| d == 0.0) ){` |
|     15 | 5665 | `		return 0;` |
|      - | 5666 | `	}` |
|     39 | 5667 | `	*pOut = d;` |
|     39 | 5668 | `	return 1;` |
|     35 | 5669 | `}` |
|      - | 5670 | `/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),` |
|      - | 5671 | ` * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as` |
|      - | 5672 | ` * false, NOT failures. */` |
|     33 | 5673 | `static int FvValidateBool(const char *z,int n,int *pBool){` |
|     33 | 5674 | `	FvTrim(&z,&n);` |
|     32 | 5675 | `	if( (n==1 && z[0]=='1') \|\| (n==4 && SyStrnicmp(z,"true",4)==0)` |
|     25 | 5676 | `	    \|\| (n==2 && SyStrnicmp(z,"on",2)==0) \|\| (n==3 && SyStrnicmp(z,"yes",3)==0) ){` |
|     11 | 5677 | `		*pBool = 1; return 1;` |
|      - | 5678 | `	}` |
|     22 | 5679 | `	if( n==0 \|\| (n==1 && z[0]=='0') \|\| (n==5 && SyStrnicmp(z,"false",5)==0)` |
|     11 | 5680 | `	    \|\| (n==3 && SyStrnicmp(z,"off",3)==0) \|\| (n==2 && SyStrnicmp(z,"no",2)==0) ){` |
|     11 | 5681 | `		*pBool = 0; return 1;` |
|      - | 5682 | `	}` |
|      9 | 5683 | `	return 0;` |
|     15 | 5684 | `}` |
|      - | 5685 | `/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. */` |
|     33 | 5686 | `static int FvValidateIp4(const char *z,int n){` |
|     33 | 5687 | `	int i = 0, parts = 0;` |
|     77 | 5688 | `	while( i<n ){` |
|     65 | 5689 | `		int val = 0, digits = 0, start = i;` |
|    143 | 5690 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|     85 | 5691 | `			val = val*10 + (z[i]-'0');` |
|     85 | 5692 | `			if( val>255 ){ return 0; }` |
|     79 | 5693 | `			digits++; i++;` |
|      1 | 5694 | `		}` |
|     59 | 5695 | `		if( digits==0 \|\| digits>3 ){ return 0; }` |
|     49 | 5696 | `		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */` |
|     45 | 5697 | `		parts++;` |
|     45 | 5698 | `		if( parts>4 ){ return 0; }` |
|     45 | 5699 | `		if( i<n ){` |
|     33 | 5700 | `			if( z[i]!='.' ){ return 0; }` |
|     33 | 5701 | `			i++;` |
|     33 | 5702 | `			if( i>=n ){ return 0; } /* trailing dot */` |
|     16 | 5703 | `		}` |
|      1 | 5704 | `	}` |
|     13 | 5705 | `	return parts==4;` |
|     17 | 5706 | `}` |
|      - | 5707 | `/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),` |
|      - | 5708 | ` * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1. */` |
|     19 | 5709 | `static int FvIp6Hextets(const char *z,int n){` |
|     19 | 5710 | `	int i = 0, segStart = 0, groups = 0;` |
|     19 | 5711 | `	if( n==0 ){ return 0; }` |
|    145 | 5712 | `	while( i<=n ){` |
|    133 | 5713 | `		if( i==n \|\| z[i]==':' ){` |
|     23 | 5714 | `			int segLen = i - segStart, j, isV4 = 0;` |
|     23 | 5715 | `			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */` |
|     77 | 5716 | `			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }` |
|     23 | 5717 | `			if( isV4 ){` |
|     11 | 5718 | `				if( i!=n ){ return -1; } /* IPv4 only as the final token */` |
|     11 | 5719 | `				if( !FvValidateIp4(z+segStart,segLen) ){ return -1; }` |
|      5 | 5720 | `				groups += 2;` |
|      3 | 5721 | `			}else{` |
|     13 | 5722 | `				if( segLen>4 ){ return -1; }` |
|     47 | 5723 | `				for( j=segStart; j<i; j++ ){ if( SyHexToint((unsigned char)z[j])<0 ){ return -1; } }` |
|     13 | 5724 | `				groups++;` |
|      - | 5725 | `			}` |
|     17 | 5726 | `			segStart = i+1;` |
|      8 | 5727 | `		}` |
|    127 | 5728 | `		i++;` |
|      1 | 5729 | `	}` |
|     13 | 5730 | `	return groups;` |
|     10 | 5731 | `}` |
|      - | 5732 | `/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present. */` |
|     19 | 5733 | `static int FvValidateIp6(const char *z,int n){` |
|     19 | 5734 | `	const char *zDbl = 0;` |
|      - | 5735 | `	int i, ga, gb;` |
|    139 | 5736 | `	for( i=0; i+1<n; i++ ){` |
|    123 | 5737 | `		if( z[i]==':' && z[i+1]==':' ){` |
|     13 | 5738 | `			if( zDbl ){ return 0; } /* a second "::" is invalid */` |
|     11 | 5739 | `			zDbl = z+i;` |
|      5 | 5740 | `		}` |
|     61 | 5741 | `	}` |
|     17 | 5742 | `	if( zDbl==0 ){` |
|      9 | 5743 | `		return FvIp6Hextets(z,n)==8;` |
|    ! 0 | 5744 | `	}else{` |
|      9 | 5745 | `		int lenA = (int)(zDbl - z);` |
|      9 | 5746 | `		int lenB = n - lenA - 2;` |
|      9 | 5747 | `		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA);` |
|      9 | 5748 | `		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB);` |
|      9 | 5749 | `		if( ga<0 \|\| gb<0 ){ return 0; }` |
|      9 | 5750 | `		return (ga+gb)<=7; /* "::" stands for at least one zero group */` |
|      - | 5751 | `	}` |
|     10 | 5752 | `}` |
|     25 | 5753 | `static int FvValidateIp(const char *z,int n,int flags){` |
|     25 | 5754 | `	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);` |
|     25 | 5755 | `	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */` |
|     25 | 5756 | `	if( v4 && FvValidateIp4(z,n) ){ return 1; }` |
|     21 | 5757 | `	if( v6 && FvValidateIp6(z,n) ){ return 1; }` |
|     13 | 5758 | `	return 0;` |
|     13 | 5759 | `}` |
|      - | 5760 | `/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */` |
|     11 | 5761 | `static int FvValidateMac(const char *z,int n){` |
|      - | 5762 | `	char sep;` |
|      - | 5763 | `	int i;` |
|     11 | 5764 | `	if( n!=17 ){ return 0; }` |
|      7 | 5765 | `	sep = z[2];` |
|      7 | 5766 | `	if( sep!=':' && sep!='-' ){ return 0; }` |
|    105 | 5767 | `	for( i=0; i<17; i++ ){` |
|    101 | 5768 | `		if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }` |
|     71 | 5769 | `		else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|     50 | 5770 | `	}` |
|      5 | 5771 | `	return 1;` |
|      6 | 5772 | `}` |
|      - | 5773 | `/* FILTER_VALIDATE_EMAIL (best-effort: covers the common cases, not quoted local` |
|      - | 5774 | ` * parts or IP-literal domains). */` |
|     28 | 5775 | `static int FvValidateEmail(const char *z,int n){` |
|     28 | 5776 | `	int at = -1, i, localLen, domLen, labelStart, dotCount = 0;` |
|      - | 5777 | `	const char *zDom;` |
|     28 | 5778 | `	if( n==0 \|\| n>320 ){ return 0; }` |
|    201 | 5779 | `	for( i=0; i<n; i++ ){` |
|    181 | 5780 | `		if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }` |
|     91 | 5781 | `	}` |
|     21 | 5782 | `	if( at<=0 \|\| at==n-1 ){ return 0; } /* one '@', non-empty local and domain */` |
|     21 | 5783 | `	localLen = at;` |
|     21 | 5784 | `	zDom = z + at + 1;` |
|     21 | 5785 | `	domLen = n - at - 1;` |
|     21 | 5786 | `	if( z[0]=='.' \|\| z[at-1]=='.' ){ return 0; }` |
|     57 | 5787 | `	for( i=0; i<localLen; i++ ){` |
|     43 | 5788 | `		unsigned char c = (unsigned char)z[i];` |
|     43 | 5789 | `		if( c<=' ' ){ return 0; }` |
|     41 | 5790 | `		if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }` |
|     20 | 5791 | `	}` |
|     15 | 5792 | `	if( zDom[0]=='.' \|\| zDom[domLen-1]=='.' ){ return 0; }` |
|     13 | 5793 | `	labelStart = 0;` |
|     85 | 5794 | `	for( i=0; i<=domLen; i++ ){` |
|     75 | 5795 | `		if( i==domLen \|\| zDom[i]=='.' ){` |
|     25 | 5796 | `			int ll = i - labelStart;` |
|     25 | 5797 | `			if( ll==0 ){ return 0; } /* consecutive dots */` |
|     23 | 5798 | `			if( zDom[labelStart]=='-' \|\| zDom[i-1]=='-' ){ return 0; }` |
|     23 | 5799 | `			if( i<domLen ){ dotCount++; }` |
|     23 | 5800 | `			labelStart = i+1;` |
|     12 | 5801 | `		}else{` |
|     51 | 5802 | `			unsigned char c = (unsigned char)zDom[i];` |
|     51 | 5803 | `			if( !((c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9')\|\|c=='-') ){ return 0; }` |
|      - | 5804 | `		}` |
|     37 | 5805 | `	}` |
|     11 | 5806 | `	if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */` |
|      9 | 5807 | `	return 1;` |
|     15 | 5808 | `}` |
|      - | 5809 | `/* FILTER_VALIDATE_DOMAIN (lenient, matching PHP without FILTER_FLAG_HOSTNAME). */` |
|     11 | 5810 | `static int FvValidateDomain(const char *z,int n){` |
|      - | 5811 | `	int i;` |
|     11 | 5812 | `	if( n<1 \|\| n>253 \|\| z[0]=='.' ){ return 0; }` |
|     81 | 5813 | `	for( i=0; i<n; i++ ){` |
|     75 | 5814 | `		unsigned char c = (unsigned char)z[i];` |
|     75 | 5815 | `		if( c<=' ' ){ return 0; }` |
|     75 | 5816 | `		if( c=='.' && i+1<n && z[i+1]=='.' ){ return 0; }` |
|     37 | 5817 | `	}` |
|      7 | 5818 | `	return 1;` |
|      6 | 5819 | `}` |
|      - | 5820 | `/* FILTER_VALIDATE_URL: require a scheme and a host (PHP's filter is itself` |
|      - | 5821 | ` * parse_url-based, so PH7_VmHttpSplitURI tracks it closely). */` |
|     15 | 5822 | `static int FvValidateUrl(const char *z,int n){` |
|      - | 5823 | `	SyhttpUri sUri;` |
|     15 | 5824 | `	if( n==0 ){ return 0; }` |
|     15 | 5825 | `	SyZero(&sUri,(sxu32)sizeof(sUri));` |
|     15 | 5826 | `	if( PH7_VmHttpSplitURI(&sUri,z,(sxu32)n)!=SXRET_OK ){ return 0; }` |
|     15 | 5827 | `	return sUri.sScheme.nByte!=0 && sUri.sHost.nByte!=0;` |
|      8 | 5828 | `}` |
|      - | 5829 | `/* The Fv sanitizers build their result by appending directly to the call` |
|      - | 5830 | ` * context (ph7_result_string accumulates, like htmlspecialchars), emitting each` |
|      - | 5831 | ` * kept run in one call and seeding "" so an all-stripped input yields "". */` |
|      - | 5832 | `/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */` |
|     37 | 5833 | `static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){` |
|     37 | 5834 | `	int i, runStart = 0;` |
|     37 | 5835 | `	ph7_result_string(pCtx,"",0);` |
|     97 | 5836 | `	for( i=0; i<n; i++ ){` |
|     91 | 5837 | `		char c = z[i];` |
|     91 | 5838 | `		int keep = (c>='0'&&c<='9') \|\| c=='+' \|\| c=='-';` |
|     91 | 5839 | `		if( !keep && isFloat ){` |
|     38 | 5840 | `			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))` |
|     23 | 5841 | `			    \|\| (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))` |
|     36 | 5842 | `			    \|\| ((c=='e'\|\|c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));` |
|     12 | 5843 | `		}` |
|     61 | 5844 | `		if( !keep ){` |
|     33 | 5845 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     33 | 5846 | `			runStart = i+1;` |
|     16 | 5847 | `		}` |
|     31 | 5848 | `	}` |
|      7 | 5849 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 5850 | `}` |
|      - | 5851 | `/* Return non-zero when byte c must be stripped under the STRIP_* flags. Shared` |
|      - | 5852 | ` * by the UNSAFE_RAW string filter and SANITIZE_SPECIAL_CHARS. STRIP_LOW drops` |
|      - | 5853 | `` * bytes <32, STRIP_HIGH drops bytes >=127 (incl. DEL), STRIP_BACKTICK drops '`'.`` |
|      - | 5854 | ` * Matches php_filter_strip(); verified byte-exact vs php 8.5.7. */` |
|    287 | 5855 | `static int FvStripByte(unsigned char c,int flags){` |
|    287 | 5856 | `	if( (flags & FV_FLAG_STRIP_LOW)      && c<32 )    { return 1; }` |
|    281 | 5857 | `	if( (flags & FV_FLAG_STRIP_HIGH)     && c>=127 )  { return 1; }` |
|    269 | 5858 | `	if( (flags & FV_FLAG_STRIP_BACKTICK) && c==0x60 ) { return 1; }` |
|    267 | 5859 | `	return 0;` |
|    144 | 5860 | `}` |
|      - | 5861 | `/* FILTER_UNSAFE_RAW / FILTER_DEFAULT with flags: no default transform, but the` |
|      - | 5862 | ` * STRIP/ENCODE flags apply. Precedence (per php_filter_unsafe_raw, verified` |
|      - | 5863 | ` * vs php 8.5.7): a byte is first tested for stripping; a surviving byte is then` |
|      - | 5864 | ` * encoded as a decimal numeric entity if ENCODE_LOW (<32) / ENCODE_HIGH (>=127)` |
|      - | 5865 | ` * is set, and '&' becomes "&#38;" under ENCODE_AMP. So STRIP_LOW\|ENCODE_LOW` |
|      - | 5866 | ` * strips (nothing left to encode). Bytes are treated individually — ENCODE_HIGH` |
|      - | 5867 | ` * numeric-encodes each byte of a multibyte sequence separately, not the codepoint. */` |
|     25 | 5868 | `static void FvSanitizeString(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 5869 | `	int i, runStart = 0;` |
|     25 | 5870 | `	ph7_result_string(pCtx,"",0);` |
|    193 | 5871 | `	for( i=0; i<n; i++ ){` |
|    179 | 5872 | `		unsigned char c = (unsigned char)z[i];` |
|    179 | 5873 | `		if( FvStripByte(c,flags) ){` |
|     13 | 5874 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     13 | 5875 | `			runStart = i+1;` |
|     13 | 5876 | `			continue;` |
|      - | 5877 | `		}` |
|    167 | 5878 | `		if( c=='&' && (flags & FV_FLAG_ENCODE_AMP) ){` |
|      3 | 5879 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      3 | 5880 | `			ph7_result_string(pCtx,"&#38;",-1);` |
|      3 | 5881 | `			runStart = i+1;` |
|    166 | 5882 | `		}else if( (c<32 && (flags & FV_FLAG_ENCODE_LOW))` |
|    164 | 5883 | `		       \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     37 | 5884 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 5885 | `			ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|      9 | 5886 | `			runStart = i+1;` |
|      4 | 5887 | `		}` |
|     79 | 5888 | `	}` |
|     15 | 5889 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     15 | 5890 | `}` |
|      - | 5891 | `/* FILTER_SANITIZE_SPECIAL_CHARS: encode <>&"' and every control byte <32 as a` |
|      - | 5892 | ` * decimal numeric entity (&#60; &#38; &#34; ...). The STRIP_* flags remove bytes` |
|      - | 5893 | ` * before encoding; ENCODE_HIGH numeric-encodes surviving bytes >=127. Bytes >=128` |
|      - | 5894 | ` * are otherwise passed through verbatim (this filter is NOT UTF-8-aware — only the` |
|      - | 5895 | ` * FULL variant is). Byte-exact vs php 8.5.7. */` |
|     13 | 5896 | `static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int flags){` |
|     13 | 5897 | `	int i, runStart = 0;` |
|      - | 5898 | `	const char *zEnt;` |
|     13 | 5899 | `	ph7_result_string(pCtx,"",0);` |
|    131 | 5900 | `	for( i=0; i<n; i++ ){` |
|    119 | 5901 | `		unsigned char c = (unsigned char)z[i];` |
|    119 | 5902 | `		if( FvStripByte(c,flags) ){` |
|      9 | 5903 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 5904 | `			runStart = i+1;` |
|      9 | 5905 | `			continue;` |
|      - | 5906 | `		}` |
|    111 | 5907 | `		switch( c ){` |
|      3 | 5908 | `		case '<':  zEnt = "&#60;"; break;` |
|      3 | 5909 | `		case '>':  zEnt = "&#62;"; break;` |
|     11 | 5910 | `		case '&':  zEnt = "&#38;"; break;` |
|      3 | 5911 | `		case '"':  zEnt = "&#34;"; break;` |
|      3 | 5912 | `		case '\'': zEnt = "&#39;"; break;` |
|     46 | 5913 | `		default:` |
|      - | 5914 | `			/* Control bytes <32 are always numeric-encoded; bytes >=127 only when` |
|      - | 5915 | `			 * ENCODE_HIGH is set. Everything else stays in the current run. */` |
|     93 | 5916 | `			if( c<32 \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     17 | 5917 | `				if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     17 | 5918 | `				ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|     17 | 5919 | `				runStart = i+1;` |
|      8 | 5920 | `			}` |
|     93 | 5921 | `			continue; /* keep in the current run */` |
|      - | 5922 | `		}` |
|     19 | 5923 | `		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     19 | 5924 | `		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */` |
|     19 | 5925 | `		runStart = i+1;` |
|     10 | 5926 | `	}` |
|     13 | 5927 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     13 | 5928 | `}` |
|      - | 5929 | `/* HTML 4.01 named-entity table (codepoint -> "&name;") used by the UTF-8-aware` |
|      - | 5930 | ` * FULL_SPECIAL_CHARS filter, sorted ascending by codepoint for binary search.` |
|      - | 5931 | ` * Generated from php 8.5.7 (the exact set php_escape_html_entities emits for the` |
|      - | 5932 | ` * default document type); the five inline specials <>&"' are handled separately,` |
|      - | 5933 | ` * so every entry here is a codepoint >=0xA0. 248 rows. */` |
|      - | 5934 | `static const struct { sxu32 cp; const char *zEnt; } aHtml401Ent[] = {` |
|      - | 5935 | `	{0x00A0,"&nbsp;"},{0x00A1,"&iexcl;"},{0x00A2,"&cent;"},{0x00A3,"&pound;"},` |
|      - | 5936 | `	{0x00A4,"&curren;"},{0x00A5,"&yen;"},{0x00A6,"&brvbar;"},{0x00A7,"&sect;"},` |
|      - | 5937 | `	{0x00A8,"&uml;"},{0x00A9,"&copy;"},{0x00AA,"&ordf;"},{0x00AB,"&laquo;"},` |
|      - | 5938 | `	{0x00AC,"&not;"},{0x00AD,"&shy;"},{0x00AE,"&reg;"},{0x00AF,"&macr;"},` |
|      - | 5939 | `	{0x00B0,"&deg;"},{0x00B1,"&plusmn;"},{0x00B2,"&sup2;"},{0x00B3,"&sup3;"},` |
|      - | 5940 | `	{0x00B4,"&acute;"},{0x00B5,"&micro;"},{0x00B6,"&para;"},{0x00B7,"&middot;"},` |
|      - | 5941 | `	{0x00B8,"&cedil;"},{0x00B9,"&sup1;"},{0x00BA,"&ordm;"},{0x00BB,"&raquo;"},` |
|      - | 5942 | `	{0x00BC,"&frac14;"},{0x00BD,"&frac12;"},{0x00BE,"&frac34;"},{0x00BF,"&iquest;"},` |
|      - | 5943 | `	{0x00C0,"&Agrave;"},{0x00C1,"&Aacute;"},{0x00C2,"&Acirc;"},{0x00C3,"&Atilde;"},` |
|      - | 5944 | `	{0x00C4,"&Auml;"},{0x00C5,"&Aring;"},{0x00C6,"&AElig;"},{0x00C7,"&Ccedil;"},` |
|      - | 5945 | `	{0x00C8,"&Egrave;"},{0x00C9,"&Eacute;"},{0x00CA,"&Ecirc;"},{0x00CB,"&Euml;"},` |
|      - | 5946 | `	{0x00CC,"&Igrave;"},{0x00CD,"&Iacute;"},{0x00CE,"&Icirc;"},{0x00CF,"&Iuml;"},` |
|      - | 5947 | `	{0x00D0,"&ETH;"},{0x00D1,"&Ntilde;"},{0x00D2,"&Ograve;"},{0x00D3,"&Oacute;"},` |
|      - | 5948 | `	{0x00D4,"&Ocirc;"},{0x00D5,"&Otilde;"},{0x00D6,"&Ouml;"},{0x00D7,"&times;"},` |
|      - | 5949 | `	{0x00D8,"&Oslash;"},{0x00D9,"&Ugrave;"},{0x00DA,"&Uacute;"},{0x00DB,"&Ucirc;"},` |
|      - | 5950 | `	{0x00DC,"&Uuml;"},{0x00DD,"&Yacute;"},{0x00DE,"&THORN;"},{0x00DF,"&szlig;"},` |
|      - | 5951 | `	{0x00E0,"&agrave;"},{0x00E1,"&aacute;"},{0x00E2,"&acirc;"},{0x00E3,"&atilde;"},` |
|      - | 5952 | `	{0x00E4,"&auml;"},{0x00E5,"&aring;"},{0x00E6,"&aelig;"},{0x00E7,"&ccedil;"},` |
|      - | 5953 | `	{0x00E8,"&egrave;"},{0x00E9,"&eacute;"},{0x00EA,"&ecirc;"},{0x00EB,"&euml;"},` |
|      - | 5954 | `	{0x00EC,"&igrave;"},{0x00ED,"&iacute;"},{0x00EE,"&icirc;"},{0x00EF,"&iuml;"},` |
|      - | 5955 | `	{0x00F0,"&eth;"},{0x00F1,"&ntilde;"},{0x00F2,"&ograve;"},{0x00F3,"&oacute;"},` |
|      - | 5956 | `	{0x00F4,"&ocirc;"},{0x00F5,"&otilde;"},{0x00F6,"&ouml;"},{0x00F7,"&divide;"},` |
|      - | 5957 | `	{0x00F8,"&oslash;"},{0x00F9,"&ugrave;"},{0x00FA,"&uacute;"},{0x00FB,"&ucirc;"},` |
|      - | 5958 | `	{0x00FC,"&uuml;"},{0x00FD,"&yacute;"},{0x00FE,"&thorn;"},{0x00FF,"&yuml;"},` |
|      - | 5959 | `	{0x0152,"&OElig;"},{0x0153,"&oelig;"},{0x0160,"&Scaron;"},{0x0161,"&scaron;"},` |
|      - | 5960 | `	{0x0178,"&Yuml;"},{0x0192,"&fnof;"},{0x02C6,"&circ;"},{0x02DC,"&tilde;"},` |
|      - | 5961 | `	{0x0391,"&Alpha;"},{0x0392,"&Beta;"},{0x0393,"&Gamma;"},{0x0394,"&Delta;"},` |
|      - | 5962 | `	{0x0395,"&Epsilon;"},{0x0396,"&Zeta;"},{0x0397,"&Eta;"},{0x0398,"&Theta;"},` |
|      - | 5963 | `	{0x0399,"&Iota;"},{0x039A,"&Kappa;"},{0x039B,"&Lambda;"},{0x039C,"&Mu;"},` |
|      - | 5964 | `	{0x039D,"&Nu;"},{0x039E,"&Xi;"},{0x039F,"&Omicron;"},{0x03A0,"&Pi;"},` |
|      - | 5965 | `	{0x03A1,"&Rho;"},{0x03A3,"&Sigma;"},{0x03A4,"&Tau;"},{0x03A5,"&Upsilon;"},` |
|      - | 5966 | `	{0x03A6,"&Phi;"},{0x03A7,"&Chi;"},{0x03A8,"&Psi;"},{0x03A9,"&Omega;"},` |
|      - | 5967 | `	{0x03B1,"&alpha;"},{0x03B2,"&beta;"},{0x03B3,"&gamma;"},{0x03B4,"&delta;"},` |
|      - | 5968 | `	{0x03B5,"&epsilon;"},{0x03B6,"&zeta;"},{0x03B7,"&eta;"},{0x03B8,"&theta;"},` |
|      - | 5969 | `	{0x03B9,"&iota;"},{0x03BA,"&kappa;"},{0x03BB,"&lambda;"},{0x03BC,"&mu;"},` |
|      - | 5970 | `	{0x03BD,"&nu;"},{0x03BE,"&xi;"},{0x03BF,"&omicron;"},{0x03C0,"&pi;"},` |
|      - | 5971 | `	{0x03C1,"&rho;"},{0x03C2,"&sigmaf;"},{0x03C3,"&sigma;"},{0x03C4,"&tau;"},` |
|      - | 5972 | `	{0x03C5,"&upsilon;"},{0x03C6,"&phi;"},{0x03C7,"&chi;"},{0x03C8,"&psi;"},` |
|      - | 5973 | `	{0x03C9,"&omega;"},{0x03D1,"&thetasym;"},{0x03D2,"&upsih;"},{0x03D6,"&piv;"},` |
|      - | 5974 | `	{0x2002,"&ensp;"},{0x2003,"&emsp;"},{0x2009,"&thinsp;"},{0x200C,"&zwnj;"},` |
|      - | 5975 | `	{0x200D,"&zwj;"},{0x200E,"&lrm;"},{0x200F,"&rlm;"},{0x2013,"&ndash;"},` |
|      - | 5976 | `	{0x2014,"&mdash;"},{0x2018,"&lsquo;"},{0x2019,"&rsquo;"},{0x201A,"&sbquo;"},` |
|      - | 5977 | `	{0x201C,"&ldquo;"},{0x201D,"&rdquo;"},{0x201E,"&bdquo;"},{0x2020,"&dagger;"},` |
|      - | 5978 | `	{0x2021,"&Dagger;"},{0x2022,"&bull;"},{0x2026,"&hellip;"},{0x2030,"&permil;"},` |
|      - | 5979 | `	{0x2032,"&prime;"},{0x2033,"&Prime;"},{0x2039,"&lsaquo;"},{0x203A,"&rsaquo;"},` |
|      - | 5980 | `	{0x203E,"&oline;"},{0x2044,"&frasl;"},{0x20AC,"&euro;"},{0x2111,"&image;"},` |
|      - | 5981 | `	{0x2118,"&weierp;"},{0x211C,"&real;"},{0x2122,"&trade;"},{0x2135,"&alefsym;"},` |
|      - | 5982 | `	{0x2190,"&larr;"},{0x2191,"&uarr;"},{0x2192,"&rarr;"},{0x2193,"&darr;"},` |
|      - | 5983 | `	{0x2194,"&harr;"},{0x21B5,"&crarr;"},{0x21D0,"&lArr;"},{0x21D1,"&uArr;"},` |
|      - | 5984 | `	{0x21D2,"&rArr;"},{0x21D3,"&dArr;"},{0x21D4,"&hArr;"},{0x2200,"&forall;"},` |
|      - | 5985 | `	{0x2202,"&part;"},{0x2203,"&exist;"},{0x2205,"&empty;"},{0x2207,"&nabla;"},` |
|      - | 5986 | `	{0x2208,"&isin;"},{0x2209,"&notin;"},{0x220B,"&ni;"},{0x220F,"&prod;"},` |
|      - | 5987 | `	{0x2211,"&sum;"},{0x2212,"&minus;"},{0x2217,"&lowast;"},{0x221A,"&radic;"},` |
|      - | 5988 | `	{0x221D,"&prop;"},{0x221E,"&infin;"},{0x2220,"&ang;"},{0x2227,"&and;"},` |
|      - | 5989 | `	{0x2228,"&or;"},{0x2229,"&cap;"},{0x222A,"&cup;"},{0x222B,"&int;"},` |
|      - | 5990 | `	{0x2234,"&there4;"},{0x223C,"&sim;"},{0x2245,"&cong;"},{0x2248,"&asymp;"},` |
|      - | 5991 | `	{0x2260,"&ne;"},{0x2261,"&equiv;"},{0x2264,"&le;"},{0x2265,"&ge;"},` |
|      - | 5992 | `	{0x2282,"&sub;"},{0x2283,"&sup;"},{0x2284,"&nsub;"},{0x2286,"&sube;"},` |
|      - | 5993 | `	{0x2287,"&supe;"},{0x2295,"&oplus;"},{0x2297,"&otimes;"},{0x22A5,"&perp;"},` |
|      - | 5994 | `	{0x22C5,"&sdot;"},{0x2308,"&lceil;"},{0x2309,"&rceil;"},{0x230A,"&lfloor;"},` |
|      - | 5995 | `	{0x230B,"&rfloor;"},{0x2329,"&lang;"},{0x232A,"&rang;"},{0x25CA,"&loz;"},` |
|      - | 5996 | `	{0x2660,"&spades;"},{0x2663,"&clubs;"},{0x2665,"&hearts;"},{0x2666,"&diams;"}` |
|      - | 5997 | `};` |
|      - | 5998 | `/* Binary-search aHtml401Ent[] for cp; return its "&name;" entity or 0. */` |
|     41 | 5999 | `static const char *FvHtml401Lookup(sxu32 cp){` |
|     41 | 6000 | `	int lo = 0, hi = (int)SX_ARRAYSIZE(aHtml401Ent) - 1;` |
|    323 | 6001 | `	while( lo <= hi ){` |
|    309 | 6002 | `		int mid = (lo + hi) / 2;` |
|    309 | 6003 | `		sxu32 c = aHtml401Ent[mid].cp;` |
|    309 | 6004 | `		if( c == cp ){ return aHtml401Ent[mid].zEnt; }` |
|    283 | 6005 | `		if( c < cp ){ lo = mid + 1; } else { hi = mid - 1; }` |
|      1 | 6006 | `	}` |
|     15 | 6007 | `	return 0;` |
|     21 | 6008 | `}` |
|      - | 6009 | `/* Decode one strict-UTF-8 sequence at p (< zEnd). On success returns its byte` |
|      - | 6010 | ` * length (1..4) and sets *pCp to the codepoint; on any malformed, overlong,` |
|      - | 6011 | ` * surrogate, truncated or out-of-range (>U+10FFFF) sequence returns 0. Matches` |
|      - | 6012 | ` * PHP's UTF-8 validation used by FULL_SPECIAL_CHARS (verified vs php 8.5.7). */` |
|    101 | 6013 | `static int FvUtf8Next(const unsigned char *p,const unsigned char *zEnd,sxu32 *pCp){` |
|    101 | 6014 | `	unsigned char c = p[0];` |
|    101 | 6015 | `	if( c < 0x80 ){ *pCp = c; return 1; }` |
|    101 | 6016 | `	if( c < 0xC2 ){ return 0; }              /* 0x80-0xBF stray cont / 0xC0-0xC1 overlong */` |
|     99 | 6017 | `	if( c < 0xE0 ){                          /* 2-byte: U+0080..U+07FF */` |
|     47 | 6018 | `		if( zEnd-p < 2 \|\| (p[1]&0xC0)!=0x80 ){ return 0; }` |
|     45 | 6019 | `		*pCp = ((sxu32)(c&0x1F)<<6) \| (p[1]&0x3F);` |
|     45 | 6020 | `		return 2;` |
|      - | 6021 | `	}` |
|     53 | 6022 | `	if( c < 0xF0 ){                          /* 3-byte: U+0800..U+FFFF minus surrogates */` |
|      - | 6023 | `		sxu32 cp;` |
|     47 | 6024 | `		if( zEnd-p < 3 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 ){ return 0; }` |
|     33 | 6025 | `		cp = ((sxu32)(c&0x0F)<<12) \| ((sxu32)(p[1]&0x3F)<<6) \| (p[2]&0x3F);` |
|     33 | 6026 | `		if( cp < 0x800 \|\| (cp>=0xD800 && cp<=0xDFFF) ){ return 0; }` |
|     29 | 6027 | `		*pCp = cp;` |
|     29 | 6028 | `		return 3;` |
|      - | 6029 | `	}` |
|      7 | 6030 | `	if( c < 0xF5 ){                          /* 4-byte: U+10000..U+10FFFF */` |
|      - | 6031 | `		sxu32 cp;` |
|      5 | 6032 | `		if( zEnd-p < 4 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 \|\| (p[3]&0xC0)!=0x80 ){ return 0; }` |
|      5 | 6033 | `		cp = ((sxu32)(c&0x07)<<18) \| ((sxu32)(p[1]&0x3F)<<12) \| ((sxu32)(p[2]&0x3F)<<6) \| (p[3]&0x3F);` |
|      5 | 6034 | `		if( cp < 0x10000 \|\| cp > 0x10FFFF ){ return 0; }` |
|      5 | 6035 | `		*pCp = cp;` |
|      5 | 6036 | `		return 4;` |
|      - | 6037 | `	}` |
|      3 | 6038 | `	return 0;                                /* 0xF5-0xFF */` |
|     51 | 6039 | `}` |
|      - | 6040 | `/* FILTER_SANITIZE_FULL_SPECIAL_CHARS: htmlentities-style, UTF-8-aware. Encodes` |
|      - | 6041 | ` * <>&"' as named entities ("'" -> &#039;; quotes suppressed under NO_ENCODE_QUOTES),` |
|      - | 6042 | ` * and every valid UTF-8 codepoint with an HTML 4.01 named entity as that entity;` |
|      - | 6043 | ` * valid codepoints without a named entity (and low control bytes) pass through` |
|      - | 6044 | ` * verbatim. If the input contains ANY invalid UTF-8 the whole result is "".` |
|      - | 6045 | ` * The STRIP/ENCODE flags do NOT apply to this filter (only NO_ENCODE_QUOTES).` |
|      - | 6046 | ` * php's filter does NOT re-encode valid pre-existing entities ("&amp;" stays,` |
|      - | 6047 | ` * "&bogus;" becomes "&amp;bogus;"), i.e. double_encode=false semantics —` |
|      - | 6048 | ` * exactly htmlentities(ENT_QUOTES\|ENT_HTML401, double_encode: false), so this` |
|      - | 6049 | ` * delegates to the shared encoder. Byte-exact vs php 8.5.7. */` |
|     25 | 6050 | `static void FvSanitizeFull(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 6051 | `	int iEntFlags = (flags & FV_FLAG_NO_ENCODE_QUOTES) ? 0 : PH7_ENT_QUOTES;` |
|     25 | 6052 | `	HtmlEscape(pCtx,z,n,iEntFlags,1/*bAll*/,0/*bDoubleEncode*/);` |
|     25 | 6053 | `}` |
|      - | 6054 | `/* ---------------------------------------------------------------------------` |
|      - | 6055 | ` * UTF-8-aware HTML entity core (htmlspecialchars/htmlentities family).` |
|      - | 6056 | ` * Prototyped next to the five builtins earlier in this file; lives here so it` |
|      - | 6057 | ` * can share aHtml401Ent[]/FvHtml401Lookup()/FvUtf8Next() with the filter_var` |
|      - | 6058 | ` * FULL_SPECIAL_CHARS filter above. Byte-exact vs php 8.5.7 (oracle-swept).` |
|      - | 6059 | ` * ------------------------------------------------------------------------ */` |
|      - | 6060 | `/* Encode cp as UTF-8 into zBuf (>= 4 bytes); return the byte length 1..4.` |
|      - | 6061 | ` * Thin wrapper over the engine-wide SX_WRITE_UTF8 (sxmacros.h). */` |
|    585 | 6062 | `static int HtmlCpUtf8(sxu32 cp,char *zBuf){` |
|    585 | 6063 | `	sxu8 *z = (sxu8 *)zBuf;` |
|    585 | 6064 | `	SX_WRITE_UTF8(z,cp);` |
|    585 | 6065 | `	return (int)(z - (sxu8 *)zBuf);` |
|      1 | 6066 | `}` |
|      - | 6067 | `/* Doctype-allowed codepoint test (php's unicode_cp_is_allowed) — gates what a` |
|      - | 6068 | ` * numeric reference may DECODE to. Oracle-pinned per doctype: HTML401` |
|      - | 6069 | ` * disallows C0 (except TAB/LF/CR) and DEL..U+009F; XML1 and XHTML share the` |
|      - | 6070 | ` * XML rules — DEL..U+009F allowed, U+FFFE/U+FFFF excluded; HTML5 swaps CR` |
|      - | 6071 | ` * for FF (0x0C) and excludes the noncharacters (U+FDD0..U+FDEF and every` |
|      - | 6072 | ` * U+xFFFE/U+xFFFF). Surrogates are disallowed everywhere. */` |
|     91 | 6073 | `static int HtmlCpAllowed(sxu32 cp,int iFlags){` |
|     91 | 6074 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|     91 | 6075 | `	if( cp==0x09 \|\| cp==0x0A ){ return 1; }` |
|     87 | 6076 | `	if( cp==0x0D ){ return iDoc != PH7_ENT_DOC_HTML5; }` |
|     85 | 6077 | `	if( cp==0x0C ){ return iDoc == PH7_ENT_DOC_HTML5; }` |
|     85 | 6078 | `	if( cp < 0x20 \|\| cp > 0x10FFFF ){ return 0; }` |
|     79 | 6079 | `	if( cp>=0xD800 && cp<=0xDFFF ){ return 0; }` |
|     77 | 6080 | `	if( cp>=0x7F && cp<=0x9F ){ return iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML; }` |
|     71 | 6081 | `	if( iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML ){` |
|    ! 0 | 6082 | `		return cp!=0xFFFE && cp!=0xFFFF;` |
|      - | 6083 | `	}` |
|     71 | 6084 | `	if( iDoc == PH7_ENT_DOC_HTML5 ){` |
|      9 | 6085 | `		if( cp>=0xFDD0 && cp<=0xFDEF ){ return 0; }` |
|      9 | 6086 | `		if( (cp & 0xFFFF) >= 0xFFFE ){ return 0; }` |
|      4 | 6087 | `	}` |
|     71 | 6088 | `	return 1;` |
|     46 | 6089 | `}` |
|      - | 6090 | `/* The ENT_DISALLOWED gate for RAW characters on the ENCODE side. Same as the` |
|      - | 6091 | ` * decode gate except CR under HTML5: php's encode-side unicode_cp_is_allowed` |
|      - | 6092 | ` * keeps a literal "\r" verbatim under ENT_HTML5\|ENT_DISALLOWED while the` |
|      - | 6093 | ` * decode side leaves "&#13;" un-decoded (oracle-pinned at flags 176). */` |
|      9 | 6094 | `static int HtmlCpAllowedEncode(sxu32 cp,int iFlags){` |
|      9 | 6095 | `	if( cp==0x0D && (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 ){ return 1; }` |
|      9 | 6096 | `	return HtmlCpAllowed(cp,iFlags);` |
|      5 | 6097 | `}` |
|      - | 6098 | `/* Numeric-reference validity for the double_encode=false "is this already a` |
|      - | 6099 | ` * valid entity" test — a MUCH looser predicate than the decode gate above:` |
|      - | 6100 | ` * any codepoint <= U+10FFFF is valid (controls and surrogates included, every` |
|      - | 6101 | ` * doctype). ENT_DISALLOWED re-tightens non-HTML401 doctypes to the decode` |
|      - | 6102 | ` * gate, except that HTML5 exempts surrogates. All oracle-pinned: &#0; and` |
|      - | 6103 | ` * &#xD800; stay verbatim at flags 11 and 139; flags -1 (HTML5+DISALLOWED)` |
|      - | 6104 | ` * re-encodes &#0; and &#x10FFFF; but still keeps &#xD800;; flags 144` |
|      - | 6105 | ` * (XML1+DISALLOWED) re-encodes &#xD800;. */` |
|      9 | 6106 | `static int HtmlNumericAllowed(sxu32 cp,int iFlags){` |
|      9 | 6107 | `	if( cp > 0x10FFFF ){ return 0; }` |
|      7 | 6108 | `	if( (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML401 ){ return 1; /* never tightened */ }` |
|    ! 0 | 6109 | `	if( (iFlags & PH7_ENT_DISALLOWED)` |
|    ! 0 | 6110 | `	 && !((iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 && cp>=0xD800 && cp<=0xDFFF)` |
|    ! 0 | 6111 | `	 && !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|    ! 0 | 6112 | `	return 1;` |
|      5 | 6113 | `}` |
|      - | 6114 | `/* How many bytes the malformed UTF-8 sequence at p consumes — php's` |
|      - | 6115 | ` * get_next_char failure step (one U+FFFD substitution / one ENT_IGNORE drop` |
|      - | 6116 | ` * per MAXIMAL invalid subpart, not per byte): a prefix-valid sequence eats` |
|      - | 6117 | ` * its continuation bytes ("\xE0\x80\xAF" is ONE unit) while a byte that could` |
|      - | 6118 | ` * start a new sequence is left for the next round. */` |
|      5 | 6119 | `static int HtmlUtf8Trail(unsigned char c){ return c>=0x80 && c<=0xBF; }` |
|     11 | 6120 | `static int HtmlUtf8Lead(unsigned char c){ return c<0x80 \|\| (c>=0xC2 && c<=0xF4); }` |
|     15 | 6121 | `static int HtmlUtf8FailAdvance(const unsigned char *p,const unsigned char *zEnd){` |
|     15 | 6122 | `	unsigned char c = p[0];` |
|     15 | 6123 | `	int nAvail = (int)(zEnd - p);` |
|     15 | 6124 | `	if( c < 0xC2 \|\| c > 0xF4 ){ return 1; } /* stray trail / C0-C1 / F5-FF */` |
|     13 | 6125 | `	if( c < 0xE0 ){` |
|      3 | 6126 | `		if( nAvail < 2 ){ return 1; }` |
|      3 | 6127 | `		return HtmlUtf8Lead(p[1]) ? 1 : 2;` |
|      - | 6128 | `	}` |
|     11 | 6129 | `	if( c < 0xF0 ){` |
|     11 | 6130 | `		if( nAvail >= 3 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) ){` |
|      3 | 6131 | `			return 3; /* complete but overlong/surrogate */` |
|      - | 6132 | `		}` |
|      9 | 6133 | `		if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 6134 | `		if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 6135 | `		return 3;` |
|      - | 6136 | `	}` |
|    ! 0 | 6137 | `	if( nAvail >= 4 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) && HtmlUtf8Trail(p[3]) ){` |
|    ! 0 | 6138 | `		return 4; /* complete but overlong / > U+10FFFF */` |
|      - | 6139 | `	}` |
|    ! 0 | 6140 | `	if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 6141 | `	if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 6142 | `	if( nAvail < 4 \|\| HtmlUtf8Lead(p[3]) ){ return 3; }` |
|    ! 0 | 6143 | `	return 4;` |
|      8 | 6144 | `}` |
|      - | 6145 | `/* The basic special entities, shared by named matching, the hsc_decode` |
|      - | 6146 | ` * numeric whitelist and the translation-table builder so the sets can never` |
|      - | 6147 | ` * drift apart. (&apos; is not an HTML 4.01 entity — doctype-gated below.) */` |
|      - | 6148 | `static const struct { const char *zEnt; int n; sxu32 cp; } aHtmlSpecEnt[] = {` |
|      - | 6149 | `	{"&amp;",5,38},{"&lt;",4,60},{"&gt;",4,62},{"&quot;",6,34},{"&apos;",6,39}` |
|      - | 6150 | `};` |
|      - | 6151 | `/* Does this doctype consult the named-entity table (aHtml401Ent)? XML 1.0 has` |
|      - | 6152 | ` * no named entities beyond the specials; XHTML/HTML5 are approximated by the` |
|      - | 6153 | ` * HTML 4.01 table (documented divergence). */` |
|     63 | 6154 | `static int HtmlDocHasNamedTable(int iDoc){` |
|     63 | 6155 | `	return iDoc != PH7_ENT_DOC_XML1;` |
|      1 | 6156 | `}` |
|      - | 6157 | `/* The single-quote entity per doctype. Oracle-pinned asymmetry: for every` |
|      - | 6158 | ` * non-HTML401 doctype htmlspecialchars emits &apos; while htmlentities` |
|      - | 6159 | ` * (bEntities) keeps &#039; under XHTML too. The translation table mirrors` |
|      - | 6160 | ` * whichever function the requested table belongs to. */` |
|     29 | 6161 | `static const char *HtmlAposEntity(int iDoc,int bEntities){` |
|     29 | 6162 | `	if( iDoc == PH7_ENT_DOC_HTML401 \|\| (bEntities && iDoc == PH7_ENT_DOC_XHTML) ){` |
|     21 | 6163 | `		return "&#039;";` |
|      - | 6164 | `	}` |
|      9 | 6165 | `	return "&apos;";` |
|     15 | 6166 | `}` |
|      - | 6167 | `/* Try to parse one HTML entity at z (z[0]=='&', z < zEnd). bFull selects the` |
|      - | 6168 | ` * html_entity_decode set (doctype named table + any allowed numeric ref) vs` |
|      - | 6169 | ` * the htmlspecialchars_decode set (the basic specials + quote numerics only).` |
|      - | 6170 | ` * Named matching is case-SENSITIVE and the ';' is required (both PHP-exact);` |
|      - | 6171 | ` * numeric refs accept dec/hex (x or X) with any number of leading zeros but` |
|      - | 6172 | ` * reject out-of-range, surrogate and doctype-disallowed codepoints (the` |
|      - | 6173 | ` * caller then leaves the source verbatim). Quote-flag gating is NOT applied` |
|      - | 6174 | ` * here — the same routine doubles as the "is this a valid entity" test for` |
|      - | 6175 | ` * double_encode=false, which ignores the quote bits (oracle-pinned).` |
|      - | 6176 | ` * bEncodeCheck selects the looser HtmlNumericAllowed predicate used by that` |
|      - | 6177 | ` * double_encode test; decode callers pass 0 for the HtmlCpAllowed gate.` |
|      - | 6178 | ` * On success sets *pCp / *pnConsumed and returns 1. */` |
|    172 | 6179 | `static int HtmlParseEntity(const unsigned char *z,const unsigned char *zEnd,` |
|      1 | 6180 | `                           int iFlags,int bFull,int bEncodeCheck,sxu32 *pCp,int *pnConsumed){` |
|    173 | 6181 | `	int nAvail = (int)(zEnd - z);` |
|    173 | 6182 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6183 | `	sxu32 n;` |
|    173 | 6184 | `	if( nAvail < 4 ){ return 0; } /* shortest entities: &lt; &#9; */` |
|    169 | 6185 | `	if( z[1] == '#' ){` |
|      - | 6186 | `		/* Numeric reference */` |
|     89 | 6187 | `		sxu32 cp = 0;` |
|     89 | 6188 | `		int i = 2, bHex = 0, nDig = 0;` |
|     89 | 6189 | `		if( z[i]=='x' \|\| z[i]=='X' ){ bHex = 1; i++; }` |
|    317 | 6190 | `		for( ; i < nAvail && z[i] != ';' ; i++ ){` |
|      - | 6191 | `			int v;` |
|    221 | 6192 | `			unsigned char c = z[i];` |
|    221 | 6193 | `			if( c>='0' && c<='9' ){ v = c - '0'; }` |
|     17 | 6194 | `			else if( bHex && c>='a' && c<='f' ){ v = c - 'a' + 10; }` |
|     17 | 6195 | `			else if( bHex && c>='A' && c<='F' ){ v = c - 'A' + 10; }` |
|    ! 0 | 6196 | `			else { return 0; }` |
|      - | 6197 | `			/* Stop accumulating once out of range (keeps validating the shape;` |
|      - | 6198 | `			 * max intermediate is 0x10FFFF*16+15, no sxu32 overflow). */` |
|    221 | 6199 | `			if( cp <= 0x10FFFF ){ cp = cp * (bHex ? 16 : 10) + (sxu32)v; }` |
|    221 | 6200 | `			nDig++;` |
|    111 | 6201 | `		}` |
|     97 | 6202 | `		if( nDig == 0 \|\| i >= nAvail ){ return 0; } /* no digits / no ';' */` |
|     97 | 6203 | `		if( bEncodeCheck ? !HtmlNumericAllowed(cp,iFlags) : !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|     83 | 6204 | `		if( !bFull ){` |
|      - | 6205 | `			/* hsc_decode: numeric refs to the five specials only. */` |
|     99 | 6206 | `			for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) && aHtmlSpecEnt[n].cp != cp ; n++ ){}` |
|     25 | 6207 | `			if( n >= SX_ARRAYSIZE(aHtmlSpecEnt) ){ return 0; }` |
|     11 | 6208 | `		}` |
|     75 | 6209 | `		*pCp = cp;` |
|     75 | 6210 | `		*pnConsumed = i + 1;` |
|     75 | 6211 | `		return 1;` |
|      - | 6212 | `	}` |
|      - | 6213 | `	/* Named reference — every entity name starts with a letter, so anything` |
|      - | 6214 | `	 * else can bail out before touching the tables. */` |
|     81 | 6215 | `	if( !((z[1]>='a' && z[1]<='z') \|\| (z[1]>='A' && z[1]<='Z')) ){ return 0; }` |
|    287 | 6216 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) ; n++ ){` |
|    265 | 6217 | `		if( aHtmlSpecEnt[n].cp == 39 && iDoc == PH7_ENT_DOC_HTML401 ){ continue; }` |
|    243 | 6218 | `		if( nAvail >= aHtmlSpecEnt[n].n && SyMemcmp(z,aHtmlSpecEnt[n].zEnt,(sxu32)aHtmlSpecEnt[n].n) == 0 ){` |
|     53 | 6219 | `			*pCp = aHtmlSpecEnt[n].cp;` |
|     53 | 6220 | `			*pnConsumed = aHtmlSpecEnt[n].n;` |
|     53 | 6221 | `			return 1;` |
|      - | 6222 | `		}` |
|     96 | 6223 | `	}` |
|     23 | 6224 | `	if( bFull && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 6225 | `		/* Linear scan of the 248-row table: runs only at '&'-then-letter` |
|      - | 6226 | `		 * positions and guarantees the decode set can never drift from the` |
|      - | 6227 | `		 * encode table. The first-letter guard skips the SyStrlen/SyMemcmp` |
|      - | 6228 | `		 * for ~96% of rows. */` |
|   3369 | 6229 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|      - | 6230 | `			sxu32 nEnt;` |
|   3357 | 6231 | `			if( z[1] != (unsigned char)aHtml401Ent[n].zEnt[1] ){ continue; }` |
|    121 | 6232 | `			nEnt = SyStrlen(aHtml401Ent[n].zEnt);` |
|    121 | 6233 | `			if( (sxu32)nAvail >= nEnt && SyMemcmp(z,aHtml401Ent[n].zEnt,nEnt) == 0 ){` |
|      7 | 6234 | `				*pCp = aHtml401Ent[n].cp;` |
|      7 | 6235 | `				*pnConsumed = (int)nEnt;` |
|      7 | 6236 | `				return 1;` |
|      - | 6237 | `			}` |
|     58 | 6238 | `		}` |
|      6 | 6239 | `	}` |
|     17 | 6240 | `	return 0;` |
|     88 | 6241 | `}` |
|      - | 6242 | `/* Shared encoder for htmlspecialchars (bAll=0) and htmlentities (bAll=1).` |
|      - | 6243 | ` * Invalid UTF-8 policy: ENT_IGNORE drops the byte (and wins over SUBSTITUTE),` |
|      - | 6244 | ` * ENT_SUBSTITUTE emits one U+FFFD per invalid byte, neither -> the whole` |
|      - | 6245 | ` * result is "" (pre-validated in a first pass: the accumulating result API` |
|      - | 6246 | ` * cannot roll back — same reason FvSanitizeFull is two-pass). */` |
|     94 | 6247 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 6248 | `                       int iFlags,int bAll,int bDoubleEncode){` |
|     95 | 6249 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     95 | 6250 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|      - | 6251 | `	const unsigned char *runStart;` |
|     95 | 6252 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6253 | `	sxu32 cp;` |
|     95 | 6254 | `	if( (iFlags & (PH7_ENT_IGNORE\|PH7_ENT_SUBSTITUTE)) == 0 ){` |
|      - | 6255 | `		/* Pass 1: any malformed sequence rejects the entire input. ASCII` |
|      - | 6256 | `		 * bytes cannot be malformed, so skip them without the decoder. */` |
|    381 | 6257 | `		while( p < zEnd ){` |
|      - | 6258 | `			int len;` |
|    323 | 6259 | `			if( *p < 0x80 ){ p++; continue; }` |
|     37 | 6260 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     37 | 6261 | `			if( len == 0 ){ ph7_result_string(pCtx,"",0); return; }` |
|     27 | 6262 | `			p += len;` |
|      1 | 6263 | `		}` |
|     59 | 6264 | `		p = (const unsigned char *)zIn;` |
|     29 | 6265 | `	}` |
|     85 | 6266 | `	runStart = p;` |
|     85 | 6267 | `	ph7_result_string(pCtx,"",0);` |
|    455 | 6268 | `	while( p < zEnd ){` |
|    371 | 6269 | `		const char *zEnt = 0;` |
|      - | 6270 | `		int len;` |
|    371 | 6271 | `		if( *p < 0x80 ){` |
|    307 | 6272 | `			len = 1;` |
|    307 | 6273 | `			switch( *p ){` |
|     25 | 6274 | `			case '<': zEnt = "&lt;"; break;` |
|     25 | 6275 | `			case '>': zEnt = "&gt;"; break;` |
|     18 | 6276 | `			case '&':` |
|     37 | 6277 | `				zEnt = "&amp;";` |
|     37 | 6278 | `				if( !bDoubleEncode ){` |
|      - | 6279 | `					sxu32 eCp; int nEat;` |
|     25 | 6280 | `					if( HtmlParseEntity(p,zEnd,iFlags,1,1,&eCp,&nEat) ){` |
|      - | 6281 | `						/* A valid existing entity: keep it verbatim. */` |
|     13 | 6282 | `						zEnt = 0;` |
|     13 | 6283 | `						len = nEat;` |
|      6 | 6284 | `					}` |
|     12 | 6285 | `				}` |
|     37 | 6286 | `				break;` |
|     10 | 6287 | `			case '"':` |
|     21 | 6288 | `				if( iFlags & PH7_ENT_QUOTE_DOUBLE ){ zEnt = "&quot;"; }` |
|     21 | 6289 | `				break;` |
|     12 | 6290 | `			case '\'':` |
|     25 | 6291 | `				if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|     23 | 6292 | `					zEnt = HtmlAposEntity(iDoc,bAll);` |
|     11 | 6293 | `				}` |
|     25 | 6294 | `				break;` |
|     89 | 6295 | `			default:` |
|    179 | 6296 | `				if( (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode((sxu32)*p,iFlags) ){` |
|    ! 0 | 6297 | `					zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 6298 | `				}` |
|    178 | 6299 | `				break;` |
|      - | 6300 | `			}` |
|    154 | 6301 | `		}else{` |
|     65 | 6302 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     65 | 6303 | `			if( len == 0 ){` |
|      - | 6304 | `				/* Malformed subpart (IGNORE or SUBSTITUTE is set, else pass 1` |
|      - | 6305 | `				 * would have rejected): drop it or emit ONE U+FFFD for the` |
|      - | 6306 | `				 * whole unit (php substitutes per maximal invalid subpart). */` |
|     15 | 6307 | `				if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|     15 | 6308 | `				if( (iFlags & PH7_ENT_IGNORE) == 0 ){ ph7_result_string(pCtx,"\xEF\xBF\xBD",3); }` |
|     15 | 6309 | `				p += HtmlUtf8FailAdvance(p,zEnd);` |
|     15 | 6310 | `				runStart = p;` |
|     15 | 6311 | `				continue;` |
|      - | 6312 | `			}` |
|     51 | 6313 | `			if( bAll && HtmlDocHasNamedTable(iDoc) ){` |
|     41 | 6314 | `				zEnt = FvHtml401Lookup(cp);` |
|     20 | 6315 | `			}` |
|     51 | 6316 | `			if( zEnt == 0 && (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode(cp,iFlags) ){` |
|    ! 0 | 6317 | `				zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 6318 | `			}` |
|      - | 6319 | `		}` |
|    357 | 6320 | `		if( zEnt ){` |
|    135 | 6321 | `			if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|    135 | 6322 | `			ph7_result_string(pCtx,zEnt,-1);` |
|    135 | 6323 | `			runStart = p + len;` |
|     67 | 6324 | `		}` |
|    357 | 6325 | `		p += len;` |
|      1 | 6326 | `	}` |
|     85 | 6327 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     48 | 6328 | `}` |
|      - | 6329 | `/* Shared decoder for html_entity_decode (bFull=1) and htmlspecialchars_decode` |
|      - | 6330 | ` * (bFull=0). Quote refs (cp 34/39, named or numeric) are gated by the quote` |
|      - | 6331 | ` * bits and left verbatim when suppressed; an invalid entity leaves its '&'` |
|      - | 6332 | ` * verbatim and rescans right after it, which also yields PHP's no-double-` |
|      - | 6333 | ` * decode behavior ("&amp;lt;" -> "&lt;"). */` |
|     82 | 6334 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 6335 | `                         int iFlags,int bFull){` |
|     83 | 6336 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     83 | 6337 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|     83 | 6338 | `	const unsigned char *runStart = p;` |
|     83 | 6339 | `	ph7_result_string(pCtx,"",0);` |
|    557 | 6340 | `	while( p < zEnd ){` |
|      - | 6341 | `		sxu32 cp;` |
|      - | 6342 | `		int nEat;` |
|    510 | 6343 | `		if( *p != '&' ){ p++; continue; }` |
|    155 | 6344 | `		if( !HtmlParseEntity(p,zEnd,iFlags,bFull,0,&cp,&nEat) ){ p++; continue; }` |
|    124 | 6345 | `		if( (cp == 34 && (iFlags & PH7_ENT_QUOTE_DOUBLE) == 0)` |
|    117 | 6346 | `		 \|\| (cp == 39 && (iFlags & PH7_ENT_QUOTE_SINGLE) == 0) ){` |
|      - | 6347 | `			/* Suppressed quote: leave the entity source verbatim. */` |
|     37 | 6348 | `			p += nEat;` |
|     37 | 6349 | `			continue;` |
|      - | 6350 | `		}` |
|     89 | 6351 | `		if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|      - | 6352 | `		{` |
|      - | 6353 | `			char zBuf[4];` |
|     89 | 6354 | `			int n = HtmlCpUtf8(cp,zBuf);` |
|     89 | 6355 | `			ph7_result_string(pCtx,zBuf,n);` |
|      - | 6356 | `		}` |
|     89 | 6357 | `		p += nEat;` |
|     89 | 6358 | `		runStart = p;` |
|      1 | 6359 | `	}` |
|     79 | 6360 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     79 | 6361 | `}` |
|      - | 6362 | `/* Validate the optional charset argument at apArg[idx]: UTF-8 aliases (and` |
|      - | 6363 | ` * ""/NULL meaning the default) are accepted; anything else — including` |
|      - | 6364 | ` * php-supported single-byte charsets like ISO-8859-1, PHL is UTF-8-only by` |
|      - | 6365 | ` * policy — raises PHP's unsupported-charset warning and is treated as` |
|      - | 6366 | ` * UTF-8 (ph7_context_throw_error_format prepends the function name). */` |
|    141 | 6367 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx){` |
|      - | 6368 | `	const char *zCs;` |
|      - | 6369 | `	int nCs;` |
|    148 | 6370 | `	if( nArg <= idx \|\| ph7_value_is_null(apArg[idx]) ){ return; }` |
|     15 | 6371 | `	zCs = ph7_value_to_string(apArg[idx],&nCs);` |
|     15 | 6372 | `	if( nCs == 0 ){ return; } /* "" selects the default charset (UTF-8) */` |
|     13 | 6373 | `	if( nCs == 5 && SyStrnicmp(zCs,"UTF-8",5) == 0 ){` |
|     13 | 6374 | `		return; /* php accepts only "UTF-8" (any case) silently — "UTF8" warns */` |
|      - | 6375 | `	}` |
|    ! 0 | 6376 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6377 | `		"Charset \"%.*s\" is not supported, assuming UTF-8",nCs,zCs);` |
|     71 | 6378 | `}` |
|      - | 6379 | `/* get_html_translation_table() worker: character (UTF-8 bytes) => entity.` |
|      - | 6380 | ` * The five specials come first in byte order, then — for HTML_ENTITIES with a` |
|      - | 6381 | ` * named-table doctype — the 248 aHtml401Ent rows ascending (oracle-pinned` |
|      - | 6382 | ` * ordering; 253 entries under the defaults). */` |
|    549 | 6383 | `static void HtmlTableAdd(ph7_value *pArray,ph7_value *pValue,const char *zKey,const char *zEnt){` |
|    549 | 6384 | `	ph7_value_string(pValue,zEnt,-1);` |
|    549 | 6385 | `	ph7_array_add_strkey_elem(pArray,zKey,pValue);` |
|    549 | 6386 | `	ph7_value_reset_string_cursor(pValue);` |
|    549 | 6387 | `}` |
|     13 | 6388 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags){` |
|      - | 6389 | `	ph7_value *pArray,*pValue;` |
|     13 | 6390 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6391 | `	sxu32 n;` |
|     13 | 6392 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 6393 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 6394 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|    ! 0 | 6395 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6396 | `		return;` |
|      - | 6397 | `	}` |
|     13 | 6398 | `	if( iFlags & PH7_ENT_QUOTE_DOUBLE ){` |
|     11 | 6399 | `		HtmlTableAdd(pArray,pValue,"\"","&quot;");` |
|      5 | 6400 | `	}` |
|     13 | 6401 | `	HtmlTableAdd(pArray,pValue,"&","&amp;");` |
|     13 | 6402 | `	if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|      - | 6403 | `		/* The apostrophe row mirrors the function each table belongs to:` |
|      - | 6404 | `		 * SPECIALCHARS follows htmlspecialchars, ENTITIES follows` |
|      - | 6405 | `		 * htmlentities (oracle-pinned at flags 35). */` |
|      7 | 6406 | `		HtmlTableAdd(pArray,pValue,"'",HtmlAposEntity(iDoc,iTable != 0));` |
|      3 | 6407 | `	}` |
|     13 | 6408 | `	HtmlTableAdd(pArray,pValue,"<","&lt;");` |
|     13 | 6409 | `	HtmlTableAdd(pArray,pValue,">","&gt;");` |
|     13 | 6410 | `	if( iTable != 0 /*php: any non-HTML_SPECIALCHARS table => entities*/ && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 6411 | `		char zKey[8];` |
|    499 | 6412 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|    497 | 6413 | `			int nK = HtmlCpUtf8(aHtml401Ent[n].cp,zKey);` |
|    497 | 6414 | `			zKey[nK] = 0;` |
|    497 | 6415 | `			HtmlTableAdd(pArray,pValue,zKey,aHtml401Ent[n].zEnt);` |
|    249 | 6416 | `		}` |
|      1 | 6417 | `	}` |
|     13 | 6418 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 6419 | `}` |
|     25 | 6420 | `static int FvEmailAllowed(unsigned char c){` |
|     25 | 6421 | `	if( (c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9') ){ return 1; }` |
|     16 | 6422 | `	return c=='!'\|\|c=='#'\|\|c=='$'\|\|c=='%'\|\|c=='&'\|\|c=='\''\|\|c=='*'\|\|c=='+'` |
|     10 | 6423 | ``	    \|\| c=='-'\|\|c=='='\|\|c=='?'\|\|c=='^'\|\|c=='_'\|\|c=='`'\|\|c=='{'\|\|c=='\|'`` |
|     15 | 6424 | `	    \|\| c=='}'\|\|c=='~'\|\|c=='@'\|\|c=='.'\|\|c=='['\|\|c==']';` |
|     13 | 6425 | `}` |
|     23 | 6426 | `static int FvUrlAllowed(unsigned char c){` |
|     23 | 6427 | `	return c>=33 && c<=126; /* PHP keeps every printable ASCII byte except space */` |
|      1 | 6428 | `}` |
|      - | 6429 | `/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */` |
|      5 | 6430 | `static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){` |
|      5 | 6431 | `	int i, runStart = 0;` |
|      5 | 6432 | `	ph7_result_string(pCtx,"",0);` |
|     51 | 6433 | `	for( i=0; i<n; i++ ){` |
|     47 | 6434 | `		unsigned char c = (unsigned char)z[i];` |
|     47 | 6435 | `		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){` |
|     11 | 6436 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     11 | 6437 | `			runStart = i+1;` |
|      5 | 6438 | `		}` |
|     24 | 6439 | `	}` |
|      5 | 6440 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      5 | 6441 | `}` |
|      - | 6442 | `/*` |
|      - | 6443 | ` * Apply the selected filter to one already-resolved input value and write the` |
|      - | 6444 | ` * result into pCtx. Shared by filter_var() and filter_input(): the caller has` |
|      - | 6445 | ` * already parsed $filter/$flags/$options. On validation failure the 'default'` |
|      - | 6446 | ` * option (if any) is returned, else null when FILTER_NULL_ON_FAILURE is set,` |
|      - | 6447 | ` * else false. A validating filter that passes returns the (string) input` |
|      - | 6448 | ` * unchanged; a sanitizer writes its transformed output directly.` |
|      - | 6449 | ` */` |
|    316 | 6450 | `static int FvApplyFilter(ph7_context *pCtx,ph7_value *pInput,` |
|      - | 6451 | `                         int iFilter,int iFlags,ph7_value *pOpts,` |
|      - | 6452 | `                         ph7_value *pDefault)` |
|      3 | 6453 | `{` |
|    319 | 6454 | `	int bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|      - | 6455 | `	const char *zVal; int nVal;` |
|      - | 6456 | `	/* An array/object input fails every scalar filter. */` |
|    319 | 6457 | `	if( ph7_value_is_array(pInput) ){ goto fail; }` |
|    317 | 6458 | `	zVal = ph7_value_to_string(pInput,&nVal);` |
|    317 | 6459 | `	switch( iFilter ){` |
|     28 | 6460 | `	case FV_VALIDATE_INT: {` |
|      - | 6461 | `		ph7_int64 v;` |
|     58 | 6462 | `		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }` |
|     31 | 6463 | `		if( pOpts ){` |
|      7 | 6464 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|      7 | 6465 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|      7 | 6466 | `			if( pMin && v<ph7_value_to_int64(pMin) ){ goto fail; }` |
|      7 | 6467 | `			if( pMax && v>ph7_value_to_int64(pMax) ){ goto fail; }` |
|      2 | 6468 | `		}` |
|     29 | 6469 | `		ph7_result_int64(pCtx,v);` |
|     29 | 6470 | `		return PH7_OK;` |
|      - | 6471 | `	}` |
|     34 | 6472 | `	case FV_VALIDATE_FLOAT: {` |
|      - | 6473 | `		double d;` |
|     69 | 6474 | `		if( !FvValidateFloat(zVal,nVal,iFlags,&d) ){ goto fail; }` |
|     39 | 6475 | `		ph7_result_double(pCtx,d);` |
|     39 | 6476 | `		return PH7_OK;` |
|      - | 6477 | `	}` |
|     14 | 6478 | `	case FV_VALIDATE_BOOLEAN: {` |
|      - | 6479 | `		int b;` |
|     29 | 6480 | `		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }` |
|     21 | 6481 | `		ph7_result_bool(pCtx,b);` |
|     21 | 6482 | `		return PH7_OK;` |
|      - | 6483 | `	}` |
|     25 | 6484 | `	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|     11 | 6485 | `	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;` |
|     28 | 6486 | `	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(zVal,nVal) ){ goto fail; }     goto pass;` |
|     11 | 6487 | `	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal) ){ goto fail; }    goto pass;` |
|     15 | 6488 | `	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal) ){ goto fail; }       goto pass;` |
|      3 | 6489 | `	case FV_VALIDATE_REGEXP: {` |
|      - | 6490 | `#ifdef PH7_ENABLE_PCRE` |
|      8 | 6491 | `		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;` |
|      8 | 6492 | `		const char *zRe; int nRe, matched = 0;` |
|      8 | 6493 | `		if( pRe==0 ){` |
|      3 | 6494 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6495 | `				"filter_var(): \"regexp\" option is missing");` |
|      - | 6496 | `		}` |
|      5 | 6497 | `		zRe = ph7_value_to_string(pRe,&nRe);` |
|      5 | 6498 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK \|\| !matched ){ goto fail; }` |
|      3 | 6499 | `		goto pass;` |
|      - | 6500 | `#else` |
|      - | 6501 | `		goto fail;` |
|      - | 6502 | `#endif` |
|      - | 6503 | `	}` |
|      3 | 6504 | `	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;` |
|      5 | 6505 | `	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;` |
|     13 | 6506 | `	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,iFlags); return PH7_OK;` |
|     25 | 6507 | `	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeFull(pCtx,zVal,nVal,iFlags);    return PH7_OK;` |
|      3 | 6508 | `	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 6509 | `	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;` |
|     13 | 6510 | `	case FV_DEFAULT:` |
|      - | 6511 | `		/* FILTER_UNSAFE_RAW / FILTER_DEFAULT: pass through unchanged unless a` |
|      - | 6512 | `		 * STRIP/ENCODE flag is set, in which case apply the string filter. */` |
|     28 | 6513 | `		if( iFlags & FV_FLAG_STRING_MASK ){` |
|     15 | 6514 | `			FvSanitizeString(pCtx,zVal,nVal,iFlags);` |
|     15 | 6515 | `			return PH7_OK;` |
|      - | 6516 | `		}` |
|     14 | 6517 | `		goto pass;` |
|    ! 0 | 6518 | `	default:` |
|    ! 0 | 6519 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6520 | `			"Unknown filter with ID %d",iFilter);` |
|    ! 0 | 6521 | `		break; /* unknown filter id -> fail */` |
|    ! 0 | 6522 | `	}` |
|     58 | 6523 | `fail:` |
|    118 | 6524 | `	if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|    114 | 6525 | `	else if( bNull ){ ph7_result_null(pCtx); }` |
|    108 | 6526 | `	else { ph7_result_bool(pCtx,0); }` |
|    118 | 6527 | `	return PH7_OK;` |
|     26 | 6528 | `pass: /* validation passed: return the (string) input unchanged */` |
|     54 | 6529 | `	ph7_result_string(pCtx,zVal,nVal);` |
|     54 | 6530 | `	return PH7_OK;` |
|    161 | 6531 | `}` |
|      - | 6532 | `/*` |
|      - | 6533 | ` * Parse the ($filter, $options) pair shared by filter_var()/filter_input() out` |
|      - | 6534 | ` * of apArg[iBase] ($filter) and apArg[iBase+1] ($options): $options is either a` |
|      - | 6535 | ` * plain flags int, or an array with 'flags' and an 'options' sub-array (whose` |
|      - | 6536 | ` * 'default' entry is the fallback value). Fills the four output pointers;` |
|      - | 6537 | ` * unset outputs keep the caller-provided defaults.` |
|      - | 6538 | ` */` |
|    328 | 6539 | `static void FvParseFilterArgs(int nArg,ph7_value **apArg,int iBase,` |
|      - | 6540 | `                              int *piFilter,int *piFlags,` |
|      - | 6541 | `                              ph7_value **ppOpts,ph7_value **ppDefault)` |
|      3 | 6542 | `{` |
|    331 | 6543 | `	if( nArg>iBase ){ *piFilter = ph7_value_to_int(apArg[iBase]); }` |
|    331 | 6544 | `	if( nArg>iBase+1 ){` |
|     88 | 6545 | `		if( ph7_value_is_array(apArg[iBase+1]) ){` |
|     42 | 6546 | `			ph7_value *pF = ph7_array_fetch(apArg[iBase+1],"flags",(int)sizeof("flags")-1);` |
|     42 | 6547 | `			if( pF ){ *piFlags = ph7_value_to_int(pF); }` |
|     42 | 6548 | `			*ppOpts = ph7_array_fetch(apArg[iBase+1],"options",(int)sizeof("options")-1);` |
|     42 | 6549 | `			if( *ppOpts && !ph7_value_is_array(*ppOpts) ){ *ppOpts = 0; }` |
|     42 | 6550 | `			if( *ppOpts ){ *ppDefault = ph7_array_fetch(*ppOpts,"default",(int)sizeof("default")-1); }` |
|     22 | 6551 | `		}else{` |
|     48 | 6552 | `			*piFlags = ph7_value_to_int(apArg[iBase+1]);` |
|      - | 6553 | `		}` |
|     43 | 6554 | `	}` |
|    331 | 6555 | `}` |
|      - | 6556 | `/*` |
|      - | 6557 | ` * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 6558 | ` *  Validate or sanitize a value; see FvApplyFilter for the failure semantics.` |
|      - | 6559 | ` */` |
|    306 | 6560 | `static int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6561 | `{` |
|    308 | 6562 | `	int iFilter = FV_DEFAULT, iFlags = 0;` |
|    308 | 6563 | `	ph7_value *pOpts = 0, *pDefault = 0;` |
|    308 | 6564 | `	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    308 | 6565 | `	FvParseFilterArgs(nArg,apArg,1,&iFilter,&iFlags,&pOpts,&pDefault);` |
|    308 | 6566 | `	return FvApplyFilter(pCtx,apArg[0],iFilter,iFlags,pOpts,pDefault);` |
|    155 | 6567 | `}` |
|      - | 6568 | `/*` |
|      - | 6569 | ` * filter_input($type, $var_name, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 6570 | ` *  Look up $var_name in the requested INPUT_* superglobal, then apply the` |
|      - | 6571 | ` *  filter. Semantics verified byte-for-byte against php 8.5:` |
|      - | 6572 | ` *   - variable NOT set: 'default' option wins, else false when` |
|      - | 6573 | ` *     FILTER_NULL_ON_FAILURE is set, else null. (Note the null/false roles are` |
|      - | 6574 | ` *     INVERTED relative to a present value that fails validation, which yields` |
|      - | 6575 | ` *     default > null-if-NULL_ON_FAILURE > false via FvApplyFilter.)` |
|      - | 6576 | ` *   - variable present: delegate to FvApplyFilter.` |
|      - | 6577 | ` *  Divergence: php reads a SAPI snapshot of the original request variables` |
|      - | 6578 | ` *  captured at startup; PHL reads the live superglobal. In CLI they match for` |
|      - | 6579 | ` *  the SAPI-registered keys (SCRIPT_NAME/PHP_SELF/DOCUMENT_ROOT); keys added` |
|      - | 6580 | ` *  only to the live $_SERVER (REQUEST_TIME/PWD/…) are visible here but not in` |
|      - | 6581 | ` *  php's snapshot.` |
|      - | 6582 | ` */` |
|     28 | 6583 | `static int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6584 | `{` |
|     30 | 6585 | `	int iType, iFilter = FV_DEFAULT, iFlags = 0;` |
|     30 | 6586 | `	ph7_value *pOpts = 0, *pDefault = 0, *pSuper, *pElem;` |
|      - | 6587 | `	const char *zVar, *zSuper; int nVar; sxu32 nSuper;` |
|     30 | 6588 | `	if( nArg<2 ){` |
|      7 | 6589 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|      2 | 6590 | `			"filter_input() expects at least 2 arguments, %d given",nArg);` |
|      - | 6591 | `	}` |
|     26 | 6592 | `	iType = ph7_value_to_int(apArg[0]);` |
|     26 | 6593 | `	switch( iType ){` |
|      3 | 6594 | `	case 0: zSuper = "_POST";   nSuper = (sxu32)sizeof("_POST")-1;   break; /* INPUT_POST */` |
|      3 | 6595 | `	case 1: zSuper = "_GET";    nSuper = (sxu32)sizeof("_GET")-1;    break; /* INPUT_GET */` |
|    ! 0 | 6596 | `	case 2: zSuper = "_COOKIE"; nSuper = (sxu32)sizeof("_COOKIE")-1; break; /* INPUT_COOKIE */` |
|    ! 0 | 6597 | `	case 4: zSuper = "_ENV";    nSuper = (sxu32)sizeof("_ENV")-1;    break; /* INPUT_ENV */` |
|     19 | 6598 | `	case 5: zSuper = "_SERVER"; nSuper = (sxu32)sizeof("_SERVER")-1; break; /* INPUT_SERVER */` |
|      1 | 6599 | `	default:` |
|      3 | 6600 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6601 | `			"filter_input(): Argument #1 ($type) must be an INPUT_* constant");` |
|      - | 6602 | `	}` |
|     23 | 6603 | `	zVar = ph7_value_to_string(apArg[1],&nVar);` |
|     23 | 6604 | `	FvParseFilterArgs(nArg,apArg,2,&iFilter,&iFlags,&pOpts,&pDefault);` |
|      - | 6605 | `	/* Resolve the variable from the superglobal (missing/non-array -> not set). */` |
|     23 | 6606 | `	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);` |
|     23 | 6607 | `	pElem = (pSuper && ph7_value_is_array(pSuper))` |
|     33 | 6608 | `		? ph7_array_fetch(pSuper,zVar,nVar) : 0;` |
|     23 | 6609 | `	if( pElem==0 ){` |
|      - | 6610 | `		/* Variable not set: default > false(if NULL_ON_FAILURE) > null. Note the` |
|      - | 6611 | `		 * false/null roles are inverted vs FvApplyFilter's present-but-fails path. */` |
|     13 | 6612 | `		if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|      9 | 6613 | `		else if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_bool(pCtx,0); }` |
|      7 | 6614 | `		else { ph7_result_null(pCtx); }` |
|     13 | 6615 | `		return PH7_OK;` |
|      - | 6616 | `	}` |
|     11 | 6617 | `	return FvApplyFilter(pCtx,pElem,iFilter,iFlags,pOpts,pDefault);` |
|     16 | 6618 | `}` |
|      - | 6619 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6620 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6621 | `/*` |
|      - | 6622 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 6623 |  |
|      - | 6624 | ` */` |
|      4 | 6625 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 6626 | `	const char *zInput, /* Raw input */` |
|      - | 6627 | `	int nByte,  /* Input length */` |
|      - | 6628 | `	int delim,  /* Delimiter */` |
|      - | 6629 | `	int encl,   /* Enclosure */` |
|      - | 6630 | `	int escape,  /* Escape character */` |
|      - | 6631 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 6632 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 6633 | `	)` |
|      1 | 6634 | `{` |
|      5 | 6635 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 6636 | `	const char *zIn = zInput;` |
|      - | 6637 | `	const char *zPtr;` |
|      - | 6638 | `	int isEnc;` |
|      - | 6639 | `	/* Start processing */` |
|      8 | 6640 | `	for(;;){` |
|     17 | 6641 | `		if( zIn >= zEnd ){` |
|      - | 6642 | `			/* No more input to process */` |
|      5 | 6643 | `			break;` |
|      - | 6644 | `		}` |
|     13 | 6645 | `		isEnc = 0;` |
|     13 | 6646 | `		zPtr = zIn;` |
|      - | 6647 | `		/* Find the first delimiter */` |
|     27 | 6648 | `		while( zIn < zEnd ){` |
|     23 | 6649 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 6650 | `				/* Delimiter found,break imediately */` |
|      5 | 6651 | `				break;` |
|     15 | 6652 | `			}else if( zIn[0] == encl ){` |
|      - | 6653 | `				/* Inside enclosure? */` |
|    ! 0 | 6654 | `				isEnc = !isEnc;` |
|     15 | 6655 | `			}else if( zIn[0] == escape ){` |
|      - | 6656 | `				/* Escape sequence */` |
|    ! 0 | 6657 | `				zIn++;` |
|    ! 0 | 6658 | `			}` |
|      - | 6659 | `			/* Advance the cursor */` |
|     15 | 6660 | `			zIn++;` |
|      1 | 6661 | `		}` |
|     13 | 6662 | `		if( zIn > zPtr ){` |
|     13 | 6663 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 6664 | `			sxi32 rc;` |
|      - | 6665 | `			/* Invoke the supllied callback */` |
|     13 | 6666 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 6667 | `				zPtr++;` |
|    ! 0 | 6668 | `				nByteChunk-=2;` |
|    ! 0 | 6669 | `			}` |
|     13 | 6670 | `			if( nByteChunk > 0 ){` |
|     13 | 6671 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 6672 | `				if( rc == SXERR_ABORT ){` |
|      - | 6673 | `					/* User callback request an operation abort */` |
|    ! 0 | 6674 | `					break;` |
|      - | 6675 | `				}` |
|      6 | 6676 | `			}` |
|      6 | 6677 | `		}` |
|      - | 6678 | `		/* Ignore trailing delimiter */` |
|     21 | 6679 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 6680 | `			zIn++;` |
|      1 | 6681 | `		}` |
|      1 | 6682 | `	}` |
|      5 | 6683 | `	return SXRET_OK;` |
|      1 | 6684 | `}` |
|      - | 6685 | `/*` |
|      - | 6686 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 6687 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 6688 | ` * argument to this callback.` |
|      - | 6689 | ` */` |
|     12 | 6690 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 6691 | `{` |
|     13 | 6692 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 6693 | `	ph7_value sEntry;` |
|      - | 6694 | `	SyString sToken;` |
|      - | 6695 | `	/* Insert the token in the given array */` |
|     13 | 6696 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 6697 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 6698 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 6699 | `	if( sToken.nByte < 1){` |
|    ! 0 | 6700 | `		return SXRET_OK;` |
|      - | 6701 | `	}` |
|     13 | 6702 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 6703 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 6704 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 6705 | `	return SXRET_OK;` |
|      7 | 6706 | `}` |
|      - | 6707 | `/*` |
|      - | 6708 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 6709 | ` *  Parse a CSV string into an array.` |
|      - | 6710 | ` * Parameters` |
|      - | 6711 | ` *  $input` |
|      - | 6712 | ` *   The string to parse.` |
|      - | 6713 | ` *  $delimiter` |
|      - | 6714 | ` *   Set the field delimiter (one character only).` |
|      - | 6715 | ` *  $enclosure` |
|      - | 6716 | ` *   Set the field enclosure character (one character only).` |
|      - | 6717 | ` *  $escape` |
|      - | 6718 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 6719 | ` * Return` |
|      - | 6720 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 6721 | ` */` |
|      2 | 6722 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6723 | `{` |
|      - | 6724 | `	const char *zInput,*zPtr;` |
|      - | 6725 | `	ph7_value *pArray;` |
|      3 | 6726 | `	int delim  = ',';   /* Delimiter */` |
|      3 | 6727 | `	int encl   = '"' ;  /* Enclosure */` |
|      3 | 6728 | `	int escape = '\\';  /* Escape character */` |
|      - | 6729 | `	int nLen;` |
|      3 | 6730 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6731 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 6732 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6733 | `		return PH7_OK;` |
|      - | 6734 | `	}` |
|      - | 6735 | `	/* Extract the raw input */` |
|      3 | 6736 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 6737 | `	if( nArg > 1 ){` |
|      - | 6738 | `		int i;` |
|      3 | 6739 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 6740 | `			/* Extract the delimiter */` |
|      3 | 6741 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 6742 | `			if( i > 0 ){` |
|      3 | 6743 | `				delim = zPtr[0];` |
|      1 | 6744 | `			}` |
|      1 | 6745 | `		}` |
|      3 | 6746 | `		if( nArg > 2 ){` |
|      3 | 6747 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 6748 | `				/* Extract the enclosure */` |
|      3 | 6749 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 6750 | `				if( i > 0 ){` |
|      3 | 6751 | `					encl = zPtr[0];` |
|      1 | 6752 | `				}` |
|      1 | 6753 | `			}` |
|      3 | 6754 | `			if( nArg > 3 ){` |
|      3 | 6755 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 6756 | `					/* Extract the escape character */` |
|      3 | 6757 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 6758 | `					if( i > 0 ){` |
|      3 | 6759 | `						escape = zPtr[0];` |
|      1 | 6760 | `					}` |
|      1 | 6761 | `				}` |
|      1 | 6762 | `			}` |
|      1 | 6763 | `		}` |
|      1 | 6764 | `	}` |
|      - | 6765 | `	/* Create our array */` |
|      3 | 6766 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 6767 | `	if( pArray == 0 ){` |
|      - | 6768 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|    ! 0 | 6769 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6770 | `	}` |
|      - | 6771 | `	/* Parse the raw input */` |
|      3 | 6772 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 6773 | `	/* Return the freshly created array */` |
|      3 | 6774 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 6775 | `	return PH7_OK;` |
|      2 | 6776 | `}` |
|      - | 6777 | `/*` |
|      - | 6778 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 6779 | ` * container.` |
|      - | 6780 | ` * Refer to [strip_tags()].` |
|      - | 6781 | ` */` |
|     10 | 6782 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 6783 | `{` |
|     11 | 6784 | `	const char *zEnd = &zTag[nByte];` |
|      - | 6785 | `	const char *zPtr;` |
|      - | 6786 | `	SyString sEntry;` |
|      - | 6787 | `	/* Strip tags */` |
|     10 | 6788 | `	for(;;){` |
|     45 | 6789 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 6790 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 6791 | `				zTag++;` |
|      1 | 6792 | `		}` |
|     21 | 6793 | `		if( zTag >= zEnd ){` |
|     11 | 6794 | `			break;` |
|      - | 6795 | `		}` |
|     11 | 6796 | `		zPtr = zTag;` |
|      - | 6797 | `		/* Delimit the tag */` |
|     25 | 6798 | `		while(zTag < zEnd ){` |
|     25 | 6799 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 6800 | `				/* UTF-8 stream */` |
|      3 | 6801 | `				zTag++;` |
|      5 | 6802 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 6803 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 6804 | `				break;` |
|    ! 0 | 6805 | `			}else{` |
|     13 | 6806 | `				zTag++;` |
|      - | 6807 | `			}` |
|      1 | 6808 | `		}` |
|     11 | 6809 | `		if( zTag > zPtr ){` |
|      - | 6810 | `			/* Perform the insertion */` |
|     11 | 6811 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 6812 | `			SyStringFullTrim(&sEntry);` |
|     11 | 6813 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 6814 | `		}` |
|      - | 6815 | `		/* Jump the trailing '>' */` |
|     11 | 6816 | `		zTag++;` |
|      1 | 6817 | `	}` |
|     11 | 6818 | `	return SXRET_OK;` |
|      1 | 6819 | `}` |
|      - | 6820 | `/*` |
|      - | 6821 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 6822 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 6823 | ` * Refer to [strip_tags()].` |
|      - | 6824 | ` */` |
|     36 | 6825 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 6826 | `{` |
|     37 | 6827 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 6828 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 6829 | `		SyString sTag;` |
|     85 | 6830 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 6831 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 6832 | `			zTag++;` |
|      1 | 6833 | `		}` |
|      - | 6834 | `		/* Delimit the tag */` |
|     25 | 6835 | `		zCur = zTag;` |
|     77 | 6836 | `		while(zTag < zEnd ){` |
|     77 | 6837 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 6838 | `				/* UTF-8 stream */` |
|      5 | 6839 | `				zTag++;` |
|      9 | 6840 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 6841 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 6842 | `				break;` |
|    ! 0 | 6843 | `			}else{` |
|     49 | 6844 | `				zTag++;` |
|      - | 6845 | `			}` |
|      1 | 6846 | `		}` |
|     25 | 6847 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 6848 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 6849 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 6850 | `		if( sTag.nByte > 0 ){` |
|      - | 6851 | `			SyString *aEntry,*pEntry;` |
|      - | 6852 | `			sxi32 rc;` |
|      - | 6853 | `			sxu32 n;` |
|      - | 6854 | `			/* Perform the lookup */` |
|     25 | 6855 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 6856 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 6857 | `				pEntry = &aEntry[n];` |
|      - | 6858 | `				/* Do the comparison */` |
|     25 | 6859 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 6860 | `				if( !rc ){` |
|     21 | 6861 | `					return SXRET_OK;` |
|      - | 6862 | `				}` |
|      3 | 6863 | `			}` |
|      2 | 6864 | `		}` |
|      2 | 6865 | `	}` |
|      - | 6866 | `	/* No such tag */` |
|     17 | 6867 | `	return SXERR_NOTFOUND;` |
|     19 | 6868 | `}` |
|      - | 6869 | `/*` |
|      - | 6870 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 6871 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 6872 | ` * Refer to [strip_tags()].` |
|      - | 6873 | ` */` |
|     16 | 6874 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 6875 | `{` |
|     17 | 6876 | `	const char *zEnd = &zIn[nByte];` |
|      - | 6877 | `	const char *zPtr,*zTag;` |
|      - | 6878 | `	SySet sSet;` |
|      - | 6879 | `	/* initialize the set of allowed tags */` |
|     17 | 6880 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 6881 | `	if( nTaglen > 0 ){` |
|      - | 6882 | `		/* Set of allowed tags */` |
|     11 | 6883 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 6884 | `	}` |
|      - | 6885 | `	/* Set the empty string */` |
|     17 | 6886 | `	ph7_result_string(pCtx,"",0);` |
|      - | 6887 | `	/* Start processing */` |
|     26 | 6888 | `	for(;;){` |
|     53 | 6889 | `		if(zIn >= zEnd){` |
|      - | 6890 | `			/* No more input to process */` |
|     15 | 6891 | `			break;` |
|      - | 6892 | `		}` |
|     39 | 6893 | `		zPtr = zIn;` |
|      - | 6894 | `		/* Find a tag */` |
|    133 | 6895 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 6896 | `			zIn++;` |
|      1 | 6897 | `		}` |
|     39 | 6898 | `		if( zIn > zPtr ){` |
|      - | 6899 | `			/* Consume raw input */` |
|     21 | 6900 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 6901 | `		}` |
|      - | 6902 | `		/* Ignore trailing null bytes */` |
|     39 | 6903 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 6904 | `			zIn++;` |
|    ! 0 | 6905 | `		}` |
|     39 | 6906 | `		if(zIn >= zEnd){` |
|      - | 6907 | `			/* No more input to process */` |
|      3 | 6908 | `			break;` |
|      - | 6909 | `		}` |
|     37 | 6910 | `		if( zIn[0] == '<' ){` |
|      - | 6911 | `			sxi32 rc;` |
|     37 | 6912 | `			zTag = zIn++;` |
|      - | 6913 | `			/* Delimit the tag */` |
|    127 | 6914 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 6915 | `				zIn++;` |
|      1 | 6916 | `			}` |
|     37 | 6917 | `			if( zIn < zEnd ){` |
|     37 | 6918 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 6919 | `			}` |
|      - | 6920 | `			/* Query the set */` |
|     37 | 6921 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 6922 | `			if( rc == SXRET_OK ){` |
|      - | 6923 | `				/* Keep the tag */` |
|     21 | 6924 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 6925 | `			}` |
|     18 | 6926 | `		}` |
|      1 | 6927 | `	}` |
|      - | 6928 | `	/* Cleanup */` |
|     17 | 6929 | `	SySetRelease(&sSet);` |
|     17 | 6930 | `	return SXRET_OK;` |
|      1 | 6931 | `}` |
|      - | 6932 | `/*` |
|      - | 6933 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 6934 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 6935 | ` * Parameters` |
|      - | 6936 | ` *  $str` |
|      - | 6937 | ` *  The input string.` |
|      - | 6938 | ` * $allowable_tags` |
|      - | 6939 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 6940 | ` * Return` |
|      - | 6941 | ` *  Returns the stripped string.` |
|      - | 6942 | ` */` |
|     14 | 6943 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6944 | `{` |
|     15 | 6945 | `	const char *zTaglist = 0;` |
|      - | 6946 | `	const char *zString;` |
|     15 | 6947 | `	int nTaglen = 0;` |
|      - | 6948 | `	int nLen;` |
|     15 | 6949 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6950 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 6951 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 6952 | `		return PH7_OK;` |
|      - | 6953 | `	}` |
|      - | 6954 | `	/* Point to the raw string */` |
|     15 | 6955 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 6956 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 6957 | `		/* Allowed tag */` |
|     11 | 6958 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 6959 | `	}` |
|      - | 6960 | `	/* Process input */` |
|     15 | 6961 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 6962 | `	return PH7_OK;` |
|      8 | 6963 | `}` |
|      - | 6964 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6965 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6966 | `/*` |
|      - | 6967 | ` * string str_shuffle(string $str)` |
|      - | 6968 |  |
|      - | 6969 | ` *  Randomly shuffles a string.` |
|      - | 6970 | ` * Parameters` |
|      - | 6971 | ` *  $str` |
|      - | 6972 | ` *   The input string.` |
|      - | 6973 | ` * Return` |
|      - | 6974 | ` *  Returns the shuffled string.` |
|      - | 6975 | ` */` |
|     10 | 6976 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6977 | `{` |
|      - | 6978 | `	const char *zString;` |
|      - | 6979 | `	int nLen,i,c;` |
|      - | 6980 | `	sxu32 iR;` |
|     11 | 6981 | `	if( nArg < 1 ){` |
|      - | 6982 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 6983 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 6984 | `		return PH7_OK;` |
|      - | 6985 | `	}` |
|      - | 6986 | `	/* Extract the target string */` |
|     11 | 6987 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 6988 | `	if( nLen < 1 ){` |
|      - | 6989 | `		/* Nothing to shuffle */` |
|      3 | 6990 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 6991 | `		return PH7_OK;` |
|      - | 6992 | `	}` |
|      - | 6993 | `	/* Shuffle the string */` |
|     43 | 6994 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 6995 | `		/* Generate a random number first */` |
|     35 | 6996 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 6997 | `		/* Extract a random offset */` |
|     35 | 6998 | `		c = zString[iR % nLen];` |
|      - | 6999 | `		/* Append it */` |
|     35 | 7000 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 7001 | `	}` |
|      9 | 7002 | `	return PH7_OK;` |
|      6 | 7003 | `}` |
|      - | 7004 | `/*` |
|      - | 7005 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 7006 | ` *  Convert a string to an array.` |
|      - | 7007 | ` * Parameters` |
|      - | 7008 | ` * $string` |
|      - | 7009 | ` *  The input string.` |
|      - | 7010 | ` * $split_length` |
|      - | 7011 | ` *  Maximum length of the chunk.` |
|      - | 7012 | ` * Return` |
|      - | 7013 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 7014 | ` *  except possibly the last one which may be shorter.` |
|      - | 7015 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 7016 | ` *  as the first (and only) array element.` |
|      - | 7017 | ` *  An empty string returns an empty array.` |
|      - | 7018 | ` * Errors` |
|      - | 7019 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 7020 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 7021 | ` *  ValueError if $split_length is less than 1.` |
|      - | 7022 | ` */` |
|     28 | 7023 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7024 | `{` |
|      - | 7025 | `	const char *zString,*zEnd;` |
|      - | 7026 | `	ph7_value *pArray,*pValue;` |
|      - | 7027 | `	int split_len;` |
|      - | 7028 | `	int nLen;` |
|     33 | 7029 | `	if( nArg < 1 ){` |
|      4 | 7030 | `		return PH7_VmThrowException(pCtx,` |
|      - | 7031 | `			"ArgumentCountError",` |
|      - | 7032 | `			"str_split() expects at least 1 argument, %d given",` |
|      1 | 7033 | `			nArg` |
|      - | 7034 | `			);` |
|      - | 7035 | `	}` |
|      - | 7036 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     38 | 7037 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     40 | 7038 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 7039 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 7040 | `		return PH7_VmThrowException(pCtx,` |
|      - | 7041 | `			"TypeError",` |
|      - | 7042 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 7043 | `			ph7_type_name(apArg[0])` |
|      - | 7044 | `			);` |
|      - | 7045 | `	}` |
|      - | 7046 | `	/* Point to the target string */` |
|     27 | 7047 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 7048 | `	split_len = (int)sizeof(char);` |
|     27 | 7049 | `	if( nArg > 1 ){` |
|      - | 7050 | `		/* Split length */` |
|     17 | 7051 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 7052 | `		if( split_len < 1 ){` |
|      6 | 7053 | `			return PH7_VmThrowException(pCtx,` |
|      - | 7054 | `				"ValueError",` |
|      - | 7055 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 7056 | `				);` |
|      - | 7057 | `		}` |
|     11 | 7058 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 7059 | `			split_len = nLen;` |
|      1 | 7060 | `		}` |
|      5 | 7061 | `	}` |
|      - | 7062 | `	/* Create the array and the scalar value */` |
|     21 | 7063 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 7064 | `	/*Chunk value */` |
|     21 | 7065 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 7066 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 7067 | `		/* Return FALSE */` |
|    ! 0 | 7068 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7069 | `		return PH7_OK;` |
|      - | 7070 | `	}` |
|      - | 7071 | `	/* Point to the end of the string */` |
|     21 | 7072 | `	zEnd = &zString[nLen];` |
|      - | 7073 | `	/* Perform the requested operation */` |
|     48 | 7074 | `	for(;;){` |
|      - | 7075 | `		int nMax;` |
|     59 | 7076 | `		if( zString >= zEnd ){` |
|      - | 7077 | `			/* No more input to process */` |
|     21 | 7078 | `			break;` |
|      - | 7079 | `		}` |
|     39 | 7080 | `		nMax = (int)(zEnd-zString);` |
|     39 | 7081 | `		if( nMax < split_len ){` |
|      3 | 7082 | `			split_len = nMax;` |
|      1 | 7083 | `		}` |
|      - | 7084 | `		/* Copy the current chunk */` |
|     39 | 7085 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 7086 | `		/* Insert it */` |
|     39 | 7087 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 7088 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 7089 | `		}` |
|      - | 7090 | `		/* reset the string cursor */` |
|     39 | 7091 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 7092 | `		/* Update position */` |
|     39 | 7093 | `		zString += split_len;` |
|      1 | 7094 | `	}` |
|      - | 7095 | `	/*` |
|      - | 7096 | `	 * Return the array.` |
|      - | 7097 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 7098 | `	 * upon we return from this function.` |
|      - | 7099 | `	 */` |
|     21 | 7100 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 7101 | `	return PH7_OK;` |
|     19 | 7102 | `}` |
|      - | 7103 | `/*` |
|      - | 7104 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 7105 | ` * Refer to [strspn()].` |
|      - | 7106 | ` */` |
|     28 | 7107 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 7108 | `{` |
|     29 | 7109 | `	const char *zIn = *pzIn;` |
|      - | 7110 | `	const char *zPtr;` |
|      - | 7111 | `	/* Ignore leading white spaces */` |
|     29 | 7112 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 7113 | `		zIn++;` |
|    ! 0 | 7114 | `	}` |
|     29 | 7115 | `	if( zIn >= zEnd ){` |
|      - | 7116 | `		/* End of input */` |
|    ! 0 | 7117 | `		return SXERR_EOF;` |
|      - | 7118 | `	}` |
|     29 | 7119 | `	zPtr = zIn;` |
|      - | 7120 | `	/* Extract the token */` |
|    201 | 7121 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 7122 | `		zIn++;` |
|      1 | 7123 | `	}` |
|     29 | 7124 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 7125 | `	/* Synchronize pointers */` |
|     29 | 7126 | `	*pzIn = zIn;` |
|      - | 7127 | `	/* Return to the caller */` |
|     29 | 7128 | `	return SXRET_OK;` |
|     15 | 7129 | `}` |
|      - | 7130 | `/*` |
|      - | 7131 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 7132 | ` * return the longest match.` |
|      - | 7133 | ` * Refer to [strspn()].` |
|      - | 7134 | ` */` |
|     18 | 7135 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 7136 | `{` |
|     19 | 7137 | `	const char *zEnd = &zString[nLen];` |
|     19 | 7138 | `	const char *zIn = zString;` |
|      - | 7139 | `	int i,c;` |
|     45 | 7140 | `	for(;;){` |
|     91 | 7141 | `		if( zString >= zEnd ){` |
|      7 | 7142 | `			break;` |
|      - | 7143 | `		}` |
|      - | 7144 | `		/* Extract current character */` |
|     85 | 7145 | `		c = zString[0];` |
|      - | 7146 | `		/* Perform the lookup */` |
|    383 | 7147 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 7148 | `			if( c == zMask[i] ){` |
|      - | 7149 | `				/* Character found */` |
|     73 | 7150 | `				break;` |
|      - | 7151 | `			}` |
|    150 | 7152 | `		}` |
|     85 | 7153 | `		if( i >= nMaskLen ){` |
|      - | 7154 | `			/* Character not in the current mask,break immediately */` |
|     13 | 7155 | `			break;` |
|      - | 7156 | `		}` |
|      - | 7157 | `		/* Advance cursor */` |
|     73 | 7158 | `		zString++;` |
|      1 | 7159 | `	}` |
|      - | 7160 | `	/* Longest match */` |
|     19 | 7161 | `	return (int)(zString-zIn);` |
|      1 | 7162 | `}` |
|      - | 7163 | `/*` |
|      - | 7164 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 7165 | ` * Refer to [strcspn()].` |
|      - | 7166 | ` */` |
|     10 | 7167 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 7168 | `{` |
|     11 | 7169 | `	const char *zEnd = &zString[nLen];` |
|     11 | 7170 | `	const char *zIn = zString;` |
|      - | 7171 | `	int i,c;` |
|     12 | 7172 | `	for(;;){` |
|     25 | 7173 | `		if( zString >= zEnd ){` |
|      3 | 7174 | `			break;` |
|      - | 7175 | `		}` |
|      - | 7176 | `		/* Extract current character */` |
|     23 | 7177 | `		c = zString[0];` |
|      - | 7178 | `		/* Perform the lookup */` |
|     51 | 7179 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 7180 | `			if( c == zMask[i] ){` |
|      9 | 7181 | `				break;` |
|      - | 7182 | `			}` |
|     15 | 7183 | `		}` |
|     23 | 7184 | `		if( i < nMaskLen ){` |
|      - | 7185 | `			/* Character in the current mask,break immediately */` |
|      9 | 7186 | `			break;` |
|      - | 7187 | `		}` |
|      - | 7188 | `		/* Advance cursor */` |
|     15 | 7189 | `		zString++;` |
|      1 | 7190 | `	}` |
|      - | 7191 | `	/* Longest match */` |
|     11 | 7192 | `	return (int)(zString-zIn);` |
|      1 | 7193 | `}` |
|      - | 7194 | `/*` |
|      - | 7195 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 7196 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 7197 | ` *  of characters contained within a given mask.` |
|      - | 7198 | ` * Parameters` |
|      - | 7199 | ` * $str` |
|      - | 7200 | ` *  The input string.` |
|      - | 7201 | ` * $mask` |
|      - | 7202 | ` *  The list of allowable characters.` |
|      - | 7203 | ` * $start` |
|      - | 7204 | ` *  The position in subject to start searching.` |
|      - | 7205 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 7206 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 7207 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 7208 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 7209 | ` *  start'th position from the end of subject.` |
|      - | 7210 | ` * $length` |
|      - | 7211 | ` *  The length of the segment from subject to examine.` |
|      - | 7212 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 7213 | ` *  characters after the starting position.` |
|      - | 7214 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 7215 | ` *  position up to length characters from the end of subject.` |
|      - | 7216 | ` * Return` |
|      - | 7217 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 7218 | ` * in mask.` |
|      - | 7219 | ` */` |
|     24 | 7220 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7221 | `{` |
|      - | 7222 | `	const char *zString,*zMask,*zEnd;` |
|      - | 7223 | `	int iMasklen,iLen;` |
|      - | 7224 | `	SyString sToken;` |
|     25 | 7225 | `	int iCount = 0;` |
|      - | 7226 | `	int rc;` |
|     25 | 7227 | `	if( nArg < 2 ){` |
|      - | 7228 | `		/* Missing agruments,return zero */` |
|    ! 0 | 7229 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7230 | `		return PH7_OK;` |
|      - | 7231 | `	}` |
|      - | 7232 | `	/* Extract the target string */` |
|     25 | 7233 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7234 | `	/* Extract the mask */` |
|     25 | 7235 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 7236 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 7237 | `		/* Nothing to process,return zero */` |
|      7 | 7238 | `		ph7_result_int(pCtx,0);` |
|      7 | 7239 | `		return PH7_OK;` |
|      - | 7240 | `	}` |
|     19 | 7241 | `	if( nArg > 2 ){` |
|      - | 7242 | `		int nOfft;` |
|      - | 7243 | `		/* Extract the offset */` |
|      9 | 7244 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 7245 | `		if( nOfft < 0 ){` |
|    ! 0 | 7246 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 7247 | `			if( zBase > zString ){` |
|    ! 0 | 7248 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 7249 | `				zString = zBase;` |
|    ! 0 | 7250 | `			}else{` |
|      - | 7251 | `				/* Invalid offset */` |
|    ! 0 | 7252 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7253 | `				return PH7_OK;` |
|      - | 7254 | `			}` |
|    ! 0 | 7255 | `		}else{` |
|      9 | 7256 | `			if( nOfft >= iLen ){` |
|      - | 7257 | `				/* Invalid offset */` |
|    ! 0 | 7258 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7259 | `				return PH7_OK;` |
|    ! 0 | 7260 | `			}else{` |
|      - | 7261 | `				/* Update offset */` |
|      9 | 7262 | `				zString += nOfft;` |
|      9 | 7263 | `				iLen -= nOfft;` |
|      - | 7264 | `			}` |
|      - | 7265 | `		}` |
|      9 | 7266 | `		if( nArg > 3 ){` |
|      - | 7267 | `			int iUserlen;` |
|      - | 7268 | `			/* Extract the desired length */` |
|      9 | 7269 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 7270 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 7271 | `				iLen = iUserlen;` |
|      2 | 7272 | `			}` |
|      4 | 7273 | `		}` |
|      4 | 7274 | `	}` |
|      - | 7275 | `	/* Point to the end of the string */` |
|     19 | 7276 | `	zEnd = &zString[iLen];` |
|      - | 7277 | `	/* Extract the first non-space token */` |
|     19 | 7278 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 7279 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 7280 | `		/* Compare against the current mask */` |
|     19 | 7281 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 7282 | `	}` |
|      - | 7283 | `	/* Longest match */` |
|     19 | 7284 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 7285 | `	return PH7_OK;` |
|     13 | 7286 | `}` |
|      - | 7287 | `/*` |
|      - | 7288 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 7289 | ` *  Find length of initial segment not matching mask.` |
|      - | 7290 | ` * Parameters` |
|      - | 7291 | ` * $str` |
|      - | 7292 | ` *  The input string.` |
|      - | 7293 | ` * $mask` |
|      - | 7294 | ` *  The list of not allowed characters.` |
|      - | 7295 | ` * $start` |
|      - | 7296 | ` *  The position in subject to start searching.` |
|      - | 7297 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 7298 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 7299 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 7300 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 7301 | ` *  start'th position from the end of subject.` |
|      - | 7302 | ` * $length` |
|      - | 7303 | ` *  The length of the segment from subject to examine.` |
|      - | 7304 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 7305 | ` *  characters after the starting position.` |
|      - | 7306 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 7307 | ` *  position up to length characters from the end of subject.` |
|      - | 7308 | ` * Return` |
|      - | 7309 | ` *  Returns the length of the segment as an integer.` |
|      - | 7310 | ` */` |
|     14 | 7311 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7312 | `{` |
|      - | 7313 | `	const char *zString,*zMask,*zEnd;` |
|      - | 7314 | `	int iMasklen,iLen;` |
|      - | 7315 | `	SyString sToken;` |
|     15 | 7316 | `	int iCount = 0;` |
|      - | 7317 | `	int rc;` |
|     15 | 7318 | `	if( nArg < 2 ){` |
|      - | 7319 | `		/* Missing agruments,return zero */` |
|    ! 0 | 7320 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7321 | `		return PH7_OK;` |
|      - | 7322 | `	}` |
|      - | 7323 | `	/* Extract the target string */` |
|     15 | 7324 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7325 | `	/* Extract the mask */` |
|     15 | 7326 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 7327 | `	if( iLen < 1 ){` |
|      - | 7328 | `		/* Nothing to process,return zero */` |
|    ! 0 | 7329 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7330 | `		return PH7_OK;` |
|      - | 7331 | `	}` |
|     15 | 7332 | `	if( iMasklen < 1 ){` |
|      - | 7333 | `		/* No given mask,return the string length */` |
|      3 | 7334 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 7335 | `		return PH7_OK;` |
|      - | 7336 | `	}` |
|     13 | 7337 | `	if( nArg > 2 ){` |
|      - | 7338 | `		int nOfft;` |
|      - | 7339 | `		/* Extract the offset */` |
|     11 | 7340 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 7341 | `		if( nOfft < 0 ){` |
|    ! 0 | 7342 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 7343 | `			if( zBase > zString ){` |
|    ! 0 | 7344 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 7345 | `				zString = zBase;` |
|    ! 0 | 7346 | `			}else{` |
|      - | 7347 | `				/* Invalid offset */` |
|    ! 0 | 7348 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7349 | `				return PH7_OK;` |
|      - | 7350 | `			}` |
|    ! 0 | 7351 | `		}else{` |
|     11 | 7352 | `			if( nOfft >= iLen ){` |
|      - | 7353 | `				/* Invalid offset */` |
|      3 | 7354 | `				ph7_result_int(pCtx,0);` |
|      3 | 7355 | `				return PH7_OK;` |
|    ! 0 | 7356 | `			}else{` |
|      - | 7357 | `				/* Update offset */` |
|      9 | 7358 | `				zString += nOfft;` |
|      9 | 7359 | `				iLen -= nOfft;` |
|      - | 7360 | `			}` |
|      - | 7361 | `		}` |
|      9 | 7362 | `		if( nArg > 3 ){` |
|      - | 7363 | `			int iUserlen;` |
|      - | 7364 | `			/* Extract the desired length */` |
|    ! 0 | 7365 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 7366 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 7367 | `				iLen = iUserlen;` |
|    ! 0 | 7368 | `			}` |
|    ! 0 | 7369 | `		}` |
|      4 | 7370 | `	}` |
|      - | 7371 | `	/* Point to the end of the string */` |
|     11 | 7372 | `	zEnd = &zString[iLen];` |
|      - | 7373 | `	/* Extract the first non-space token */` |
|     11 | 7374 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 7375 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 7376 | `		/* Compare against the current mask */` |
|     11 | 7377 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 7378 | `	}` |
|      - | 7379 | `	/* Longest match */` |
|     11 | 7380 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 7381 | `	return PH7_OK;` |
|      8 | 7382 | `}` |
|      - | 7383 | `/*` |
|      - | 7384 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 7385 | ` *  Search a string for any of a set of characters.` |
|      - | 7386 | ` * Parameters` |
|      - | 7387 | ` *  $haystack` |
|      - | 7388 | ` *   The string where char_list is looked for.` |
|      - | 7389 | ` *  $char_list` |
|      - | 7390 | ` *   This parameter is case sensitive.` |
|      - | 7391 | ` * Return` |
|      - | 7392 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 7393 | ` */` |
|      4 | 7394 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7395 | `{` |
|      - | 7396 | `	const char *zString,*zList,*zEnd;` |
|      - | 7397 | `	int iLen,iListLen,i,c;` |
|      - | 7398 | `	sxu32 nOfft,nMax;` |
|      - | 7399 | `	sxi32 rc;` |
|      5 | 7400 | `	if( nArg < 2 ){` |
|      - | 7401 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7402 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7403 | `		return PH7_OK;` |
|      - | 7404 | `	}` |
|      - | 7405 | `	/* Extract the haystack and the char list */` |
|      5 | 7406 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 7407 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 7408 | `	if( iLen < 1 ){` |
|      - | 7409 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7410 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7411 | `		return PH7_OK;` |
|      - | 7412 | `	}` |
|      - | 7413 | `	/* Point to the end of the string */` |
|      5 | 7414 | `	zEnd = &zString[iLen];` |
|      5 | 7415 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 7416 | `	/* perform the requested operation */` |
|     15 | 7417 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 7418 | `		c = zList[i];` |
|     11 | 7419 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 7420 | `		if( rc == SXRET_OK ){` |
|      5 | 7421 | `			if( nMax < nOfft ){` |
|      3 | 7422 | `				nOfft = nMax;` |
|      1 | 7423 | `			}` |
|      2 | 7424 | `		}` |
|      6 | 7425 | `	}` |
|      5 | 7426 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 7427 | `		/* No such substring,return FALSE */` |
|      3 | 7428 | `		ph7_result_bool(pCtx,0);` |
|      2 | 7429 | `	}else{` |
|      - | 7430 | `		/* Return the substring */` |
|      3 | 7431 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 7432 | `	}` |
|      5 | 7433 | `	return PH7_OK;` |
|      3 | 7434 | `}` |
|      - | 7435 | `/* SPDX-SnippetBegin */` |
|      - | 7436 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 7437 | `/* SPDX-License-Identifier: blessing */` |
|      - | 7438 | `/*` |
|      - | 7439 | ` * string soundex(string $str)` |
|      - | 7440 | ` *  Calculate the soundex key of a string.` |
|      - | 7441 | ` * Parameters` |
|      - | 7442 | ` *  $str` |
|      - | 7443 | ` *   The input string.` |
|      - | 7444 | ` * Return` |
|      - | 7445 | ` *  Returns the soundex key as a string.` |
|      - | 7446 | ` * Note:` |
|      - | 7447 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 7448 | ` * source tree.` |
|      - | 7449 | ` */` |
|     22 | 7450 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7451 | `{` |
|      - | 7452 | `	const unsigned char *zIn;` |
|      - | 7453 | `	char zResult[8];` |
|      - | 7454 | `	int i, j;` |
|      - | 7455 | `	static const unsigned char iCode[] = {` |
|      - | 7456 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7457 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7458 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7459 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7460 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 7461 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 7462 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 7463 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 7464 | `	};` |
|     23 | 7465 | `	if( nArg < 1 ){` |
|      - | 7466 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7467 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7468 | `		return PH7_OK;` |
|      - | 7469 | `	}` |
|     23 | 7470 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     35 | 7471 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     23 | 7472 | `	if( zIn[i] ){` |
|     17 | 7473 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 7474 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 7475 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 7476 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 7477 | `			if( code>0 ){` |
|     45 | 7478 | `				if( code!=prevcode ){` |
|     33 | 7479 | `					prevcode = (unsigned char)code;` |
|     33 | 7480 | `					zResult[j++] = (char)code + '0';` |
|     16 | 7481 | `				}` |
|     23 | 7482 | `			}else{` |
|     49 | 7483 | `				prevcode = 0;` |
|      - | 7484 | `			}` |
|     47 | 7485 | `		}` |
|     33 | 7486 | `		while( j<4 ){` |
|     17 | 7487 | `			zResult[j++] = '0';` |
|      1 | 7488 | `		}` |
|     17 | 7489 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 7490 | `	}else{` |
|      - | 7491 | `	  /* No alphabetic character: PHP returns "0000" (not the SQLite "?000"). */` |
|      7 | 7492 | `	  ph7_result_string(pCtx,"0000",4);` |
|      - | 7493 | `	}` |
|     23 | 7494 | `	return PH7_OK;` |
|     12 | 7495 | `}` |
|      - | 7496 | `/* SPDX-SnippetEnd */` |
|      - | 7497 | `/*` |
|      - | 7498 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 7499 | ` *  Wraps a string to a given number of characters.` |
|      - | 7500 | ` * Parameters` |
|      - | 7501 | ` *  $str` |
|      - | 7502 | ` *   The input string.` |
|      - | 7503 | ` * $width` |
|      - | 7504 | ` *  The column width.` |
|      - | 7505 | ` * $break` |
|      - | 7506 | ` *  The line is broken using the optional break parameter.` |
|      - | 7507 | ` * Return` |
|      - | 7508 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 7509 | ` */` |
|     26 | 7510 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7511 | `{` |
|      - | 7512 | `	const char *zIn,*zBreak;` |
|      - | 7513 | `	SyBlob sWorker;` |
|      - | 7514 | `	int iLen,iBreaklen,iWidth,iCut,iStart,iSpace,iCur;` |
|      - | 7515 | `	sxi32 rc;` |
|     27 | 7516 | `	if( nArg < 1 ){` |
|      - | 7517 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7518 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7519 | `		return PH7_OK;` |
|      - | 7520 | `	}` |
|      - | 7521 | `	/* Extract the input string */` |
|     27 | 7522 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7523 | `	/* Width (default 75; PHP allows 0/negative — break at every space). */` |
|     27 | 7524 | `	iWidth = 75;` |
|     27 | 7525 | `	if( nArg > 1 ){` |
|     27 | 7526 | `		iWidth = ph7_value_to_int(apArg[1]);` |
|     13 | 7527 | `	}` |
|      - | 7528 | `	/* Break string (default "\n"). */` |
|     27 | 7529 | `	zBreak = "\n";` |
|     27 | 7530 | `	iBreaklen = (int)sizeof(char);` |
|     27 | 7531 | `	if( nArg > 2 ){` |
|     13 | 7532 | `		zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      6 | 7533 | `	}` |
|      - | 7534 | `	/* Cut long words? (default false). */` |
|     27 | 7535 | `	iCut = 0;` |
|     27 | 7536 | `	if( nArg > 3 ){` |
|      7 | 7537 | `		iCut = ph7_value_to_bool(apArg[3]);` |
|      3 | 7538 | `	}` |
|     27 | 7539 | `	if( iLen < 1 ){` |
|      - | 7540 | `		/* PHP returns the empty string for empty input before validating the other args. */` |
|      5 | 7541 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 7542 | `		return PH7_OK;` |
|      - | 7543 | `	}` |
|      - | 7544 | `	/* PHP 8 domain errors (catchable ValueError). */` |
|     23 | 7545 | `	if( iBreaklen < 1 ){` |
|      3 | 7546 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7547 | `			"wordwrap(): Argument #3 ($break) must not be empty");` |
|      - | 7548 | `	}` |
|     21 | 7549 | `	if( iWidth == 0 && iCut ){` |
|      3 | 7550 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7551 | `			"wordwrap(): Argument #4 ($cut_long_words) cannot be true when argument #2 ($width) is 0");` |
|      - | 7552 | `	}` |
|      - | 7553 | `	/*` |
|      - | 7554 | `	 * PHP's algorithm: a single left-to-right pass tracking the start of the` |
|      - | 7555 | `	 * current line (iStart) and the position of the last space seen on it` |
|      - | 7556 | `	 * (iSpace). A break is emitted when the line reaches the width, at the last` |
|      - | 7557 | `	 * space if there was one, otherwise (only when cut is enabled) hard at the` |
|      - | 7558 | `	 * boundary. An existing break sequence in the input resets the line.` |
|      - | 7559 | `	 */` |
|     19 | 7560 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     19 | 7561 | `	iStart = iSpace = iCur = 0;` |
|     19 | 7562 | `	rc = SXRET_OK;` |
|    551 | 7563 | `	while( iCur < iLen ){` |
|    533 | 7564 | `		if( iBreaklen <= iLen - iCur && SyMemcmp(&zIn[iCur],zBreak,(sxu32)iBreaklen) == 0 ){` |
|      - | 7565 | `			/* Existing break sequence in the input: copy it verbatim and reset the line. */` |
|    ! 0 | 7566 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart + iBreaklen));` |
|    ! 0 | 7567 | `			if( rc != SXRET_OK ){ goto oom; }` |
|    ! 0 | 7568 | `			iCur += iBreaklen;` |
|    ! 0 | 7569 | `			iStart = iSpace = iCur;` |
|    ! 0 | 7570 | `			continue;` |
|    533 | 7571 | `		}else if( zIn[iCur] == ' ' ){` |
|     67 | 7572 | `			if( iCur - iStart >= iWidth ){` |
|      - | 7573 | `				/* The line already fills the width at this space: break here (the space is consumed). */` |
|     13 | 7574 | `				rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     13 | 7575 | `				if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     13 | 7576 | `				if( rc != SXRET_OK ){ goto oom; }` |
|     13 | 7577 | `				iStart = iCur + 1;` |
|      6 | 7578 | `			}` |
|     67 | 7579 | `			iSpace = iCur;` |
|    500 | 7580 | `		}else if( iCut && iCur - iStart >= iWidth && iStart >= iSpace ){` |
|      - | 7581 | `			/* A word longer than the width with no space to break at: hard-cut at the boundary. */` |
|      7 | 7582 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|      7 | 7583 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|      7 | 7584 | `			if( rc != SXRET_OK ){ goto oom; }` |
|      7 | 7585 | `			iStart = iSpace = iCur;` |
|    464 | 7586 | `		}else if( iCur - iStart >= iWidth && iStart < iSpace ){` |
|      - | 7587 | `			/* Past the width mid-word: wrap back to the last space (which is consumed). */` |
|     17 | 7588 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iSpace - iStart));` |
|     17 | 7589 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     17 | 7590 | `			if( rc != SXRET_OK ){ goto oom; }` |
|     17 | 7591 | `			iStart = iSpace = iSpace + 1;` |
|      8 | 7592 | `		}` |
|    533 | 7593 | `		iCur++;` |
|      1 | 7594 | `	}` |
|      - | 7595 | `	/* Emit the trailing chunk. */` |
|     19 | 7596 | `	if( iStart < iCur ){` |
|     19 | 7597 | `		rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     19 | 7598 | `		if( rc != SXRET_OK ){ goto oom; }` |
|      9 | 7599 | `	}` |
|     19 | 7600 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|     19 | 7601 | `	SyBlobRelease(&sWorker);` |
|     19 | 7602 | `	return PH7_OK;` |
|    ! 0 | 7603 | `oom:` |
|    ! 0 | 7604 | `	SyBlobRelease(&sWorker);` |
|    ! 0 | 7605 | `	return PH7_ContextMemoryError(pCtx);` |
|     14 | 7606 | `}` |
|      - | 7607 | `/*` |
|      - | 7608 | ` * Check if the given character is a member of the given mask.` |
|      - | 7609 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 7610 | ` * Refer to [strtok()].` |
|      - | 7611 | ` */` |
|     30 | 7612 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 7613 | `{` |
|      - | 7614 | `	int i;` |
|     57 | 7615 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 7616 | `		if( c == zMask[i] ){` |
|     13 | 7617 | `			if( pOfft ){` |
|      5 | 7618 | `				*pOfft = i;` |
|      2 | 7619 | `			}` |
|     13 | 7620 | `			return TRUE;` |
|      - | 7621 | `		}` |
|     14 | 7622 | `	}` |
|     19 | 7623 | `	return FALSE;` |
|     16 | 7624 | `}` |
|      - | 7625 | `/*` |
|      - | 7626 | ` * Extract a single token from the input stream.` |
|      - | 7627 | ` * Refer to [strtok()].` |
|      - | 7628 | ` */` |
|      6 | 7629 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 7630 | `{` |
|      7 | 7631 | `	const char *zIn = *pzIn;` |
|      - | 7632 | `	const char *zPtr;` |
|      - | 7633 | `	/* Ignore leading delimiter */` |
|     11 | 7634 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 7635 | `		zIn++;` |
|      1 | 7636 | `	}` |
|      7 | 7637 | `	if( zIn >= zEnd ){` |
|      - | 7638 | `		/* End of input */` |
|    ! 0 | 7639 | `		return SXERR_EOF;` |
|      - | 7640 | `	}` |
|      7 | 7641 | `	zPtr = zIn;` |
|      - | 7642 | `	/* Extract the token */` |
|     13 | 7643 | `	while( zIn < zEnd ){` |
|     11 | 7644 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 7645 | `			/* UTF-8 stream */` |
|    ! 0 | 7646 | `			zIn++;` |
|    ! 0 | 7647 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 7648 | `		}else{` |
|     11 | 7649 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 7650 | `				break;` |
|      - | 7651 | `			}` |
|      7 | 7652 | `			zIn++;` |
|      - | 7653 | `		}` |
|      1 | 7654 | `	}` |
|      7 | 7655 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 7656 | `	/* Update the cursor */` |
|      7 | 7657 | `	*pzIn = zIn;` |
|      - | 7658 | `	/* Return to the caller */` |
|      7 | 7659 | `	return SXRET_OK;` |
|      4 | 7660 | `}` |
|      - | 7661 | `/* strtok auxiliary private data */` |
|      - | 7662 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 7663 | `struct strtok_aux_data` |
|      - | 7664 | `{` |
|      - | 7665 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 7666 | `	const char *zIn;   /* Current input stream */` |
|      - | 7667 | `	const char *zEnd;  /* End of input */` |
|      - | 7668 | `};` |
|      - | 7669 | `/*` |
|      - | 7670 | ` * string strtok(string $str,string $token)` |
|      - | 7671 | ` * string strtok(string $token)` |
|      - | 7672 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 7673 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 7674 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 7675 | ` *  words by using the space character as the token.` |
|      - | 7676 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 7677 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 7678 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 7679 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 7680 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 7681 | ` *  the argument are found.` |
|      - | 7682 | ` * Parameters` |
|      - | 7683 | ` *  $str` |
|      - | 7684 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 7685 | ` * $token` |
|      - | 7686 | ` *  The delimiter used when splitting up str.` |
|      - | 7687 | ` * Return` |
|      - | 7688 | ` *   Current token or FALSE on EOF.` |
|      - | 7689 | ` */` |
|      6 | 7690 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7691 | `{` |
|      - | 7692 | `	strtok_aux_data *pAux;` |
|      - | 7693 | `	const char *zMask;` |
|      - | 7694 | `	SyString sToken;` |
|      - | 7695 | `	int nMasklen;` |
|      - | 7696 | `	sxi32 rc;` |
|      7 | 7697 | `	if( nArg < 2 ){` |
|      - | 7698 | `		/* Extract top aux data */` |
|      5 | 7699 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      5 | 7700 | `		if( pAux == 0 ){` |
|      - | 7701 | `			/* No aux data,return FALSE */` |
|    ! 0 | 7702 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7703 | `			return PH7_OK;` |
|      - | 7704 | `		}` |
|      5 | 7705 | `		nMasklen = 0;` |
|      5 | 7706 | `		zMask = ""; /* cc warning */` |
|      5 | 7707 | `		if( nArg > 0 ){` |
|      - | 7708 | `			/* Extract the mask */` |
|      5 | 7709 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 7710 | `		}` |
|      5 | 7711 | `		if( nMasklen < 1 ){` |
|      - | 7712 | `			/* Invalid mask,return FALSE */` |
|    ! 0 | 7713 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 7714 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7715 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 7716 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7717 | `			return PH7_OK;` |
|      - | 7718 | `		}` |
|      - | 7719 | `		/* Extract the token */` |
|      5 | 7720 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 7721 | `		if( rc != SXRET_OK ){` |
|      - | 7722 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 7723 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 7724 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7725 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 7726 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7727 | `		}else{` |
|      - | 7728 | `			/* Return the extracted token */` |
|      5 | 7729 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 7730 | `		}` |
|      3 | 7731 | `	}else{` |
|      - | 7732 | `		const char *zInput,*zCur;` |
|      - | 7733 | `		char *zDup;` |
|      - | 7734 | `		int nLen;` |
|      - | 7735 | `		/* Extract the raw input */` |
|      3 | 7736 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 7737 | `		if( nLen < 1 ){` |
|      - | 7738 | `			/* Empty input,return FALSE */` |
|    ! 0 | 7739 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7740 | `			return PH7_OK;` |
|      - | 7741 | `		}` |
|      - | 7742 | `		/* Extract the mask */` |
|      3 | 7743 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 7744 | `		if( nMasklen < 1 ){` |
|      - | 7745 | `			/* Set a default mask */` |
|      - | 7746 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 7747 | `			zMask = TOK_MASK;` |
|    ! 0 | 7748 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 7749 | `#undef TOK_MASK` |
|    ! 0 | 7750 | `		}` |
|      - | 7751 | `		/* Extract a single token */` |
|      3 | 7752 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 7753 | `		if( rc != SXRET_OK ){` |
|      - | 7754 | `			/* Empty input */` |
|    ! 0 | 7755 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7756 | `			return PH7_OK;` |
|    ! 0 | 7757 | `		}else{` |
|      - | 7758 | `			/* Return the extracted token */` |
|      3 | 7759 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 7760 | `		}` |
|      - | 7761 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 7762 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 7763 | `		if( pAux ){` |
|      3 | 7764 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 7765 | `			if( nLen < 1 ){` |
|    ! 0 | 7766 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7767 | `				return PH7_OK;` |
|      - | 7768 | `			}` |
|      - | 7769 | `			/* Duplicate input */` |
|      3 | 7770 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 7771 | `			if( zDup  ){` |
|      3 | 7772 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 7773 | `				/* Register the aux data */` |
|      3 | 7774 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 7775 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 7776 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 7777 | `			}` |
|      1 | 7778 | `		}` |
|      - | 7779 | `	}` |
|      7 | 7780 | `	return PH7_OK;` |
|      4 | 7781 | `}` |
|      - | 7782 | `/*` |
|      - | 7783 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 7784 | ` *  Pad a string to a certain length with another string` |
|      - | 7785 | ` * Parameters` |
|      - | 7786 | ` *  $input` |
|      - | 7787 | ` *   The input string.` |
|      - | 7788 | ` * $pad_length` |
|      - | 7789 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 7790 | ` *   string, no padding takes place.` |
|      - | 7791 | ` * $pad_string` |
|      - | 7792 | ` *   Note:` |
|      - | 7793 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 7794 | ` *    divided by the pad_string's length.` |
|      - | 7795 | ` * $pad_type` |
|      - | 7796 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 7797 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 7798 | ` * Return` |
|      - | 7799 | ` *  The padded string.` |
|      - | 7800 | ` */` |
|     10 | 7801 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7802 | `{` |
|      - | 7803 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 7804 | `	const char *zIn,*zPad;` |
|     11 | 7805 | `	if( nArg < 2 ){` |
|      - | 7806 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7807 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7808 | `		return PH7_OK;` |
|      - | 7809 | `	}` |
|      - | 7810 | `	/* Extract the target string */` |
|     11 | 7811 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7812 | `	/* Padding length */` |
|     11 | 7813 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|     11 | 7814 | `	if( iPadlen > 0 ){` |
|      9 | 7815 | `		iPadlen -= iLen;` |
|      4 | 7816 | `	}` |
|     11 | 7817 | `	if( iPadlen < 1  ){` |
|      - | 7818 | `		/* Return the string verbatim */` |
|      5 | 7819 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      5 | 7820 | `		return PH7_OK;` |
|      - | 7821 | `	}` |
|      7 | 7822 | `	zPad = " "; /* Whitespace padding */` |
|      7 | 7823 | `	iStrpad = (int)sizeof(char);` |
|      7 | 7824 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      7 | 7825 | `	if( nArg > 2 ){` |
|      - | 7826 | `		/* Padding string */` |
|      7 | 7827 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      7 | 7828 | `		if( iStrpad < 1 ){` |
|      - | 7829 | `			/* An empty pad string throws a catchable ValueError in PHP 8` |
|      - | 7830 | `			 * (only reached once padding is actually required). */` |
|      3 | 7831 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7832 | `				"str_pad(): Argument #3 ($pad_string) must not be empty");` |
|      - | 7833 | `		}` |
|      5 | 7834 | `		if( nArg > 3 ){` |
|      - | 7835 | `			/* Padd type */` |
|      5 | 7836 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 7837 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 7838 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 7839 | `			}` |
|      2 | 7840 | `		}` |
|      2 | 7841 | `	}` |
|      5 | 7842 | `	iDiv = 1;` |
|      5 | 7843 | `	if( iType == 2 ){` |
|    ! 0 | 7844 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 7845 | `	}` |
|      - | 7846 | `	/* Perform the requested operation */` |
|      5 | 7847 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 7848 | `		jPad = iStrpad;` |
|      5 | 7849 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 7850 | `			/* Padding */` |
|      5 | 7851 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 7852 | `				break;` |
|      - | 7853 | `			}` |
|      3 | 7854 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7855 | `		}` |
|      3 | 7856 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 7857 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 7858 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 7859 | `				if( jPad > iStrpad ){` |
|    ! 0 | 7860 | `					jPad = iStrpad;` |
|    ! 0 | 7861 | `				}` |
|      3 | 7862 | `				if( jPad < 1){` |
|    ! 0 | 7863 | `					break;` |
|      - | 7864 | `				}` |
|      3 | 7865 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 7866 | `			}` |
|      1 | 7867 | `		}` |
|      1 | 7868 | `	}` |
|      5 | 7869 | `	if( iLen > 0 ){` |
|      - | 7870 | `		/* Append the input string */` |
|      5 | 7871 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7872 | `	}` |
|      5 | 7873 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 7874 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 7875 | `			/* Padding */` |
|      5 | 7876 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 7877 | `				break;` |
|      - | 7878 | `			}` |
|      3 | 7879 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7880 | `		}` |
|      5 | 7881 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 7882 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 7883 | `			if( jPad > iStrpad ){` |
|    ! 0 | 7884 | `				jPad = iStrpad;` |
|    ! 0 | 7885 | `			}` |
|      3 | 7886 | `			if( jPad < 1){` |
|    ! 0 | 7887 | `				break;` |
|      - | 7888 | `			}` |
|      3 | 7889 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 7890 | `		}` |
|      1 | 7891 | `	}` |
|      5 | 7892 | `	return PH7_OK;` |
|      6 | 7893 | `}` |
|      - | 7894 | `/*` |
|      - | 7895 | ` * String replacement private data.` |
|      - | 7896 | ` */` |
|      - | 7897 | `typedef struct str_replace_data str_replace_data;` |
|      - | 7898 | `struct str_replace_data` |
|      - | 7899 | `{` |
|      - | 7900 | `	/* Used by the str_replace family to collect the search/replace arguments. */` |
|      - | 7901 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 7902 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 7903 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 7904 | `};` |
|      - | 7905 | `/*` |
|      - | 7906 | ` * Remove a substring.` |
|      - | 7907 | ` */` |
|      - | 7908 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 7909 | `	for(;;){\` |
|      - | 7910 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 7911 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 7912 | `		++OFFT;\` |
|      - | 7913 | `	}\` |
|      - | 7914 | `}` |
|      - | 7915 | `/*` |
|      - | 7916 | ` * Shift right and insert algorithm.` |
|      - | 7917 | ` */` |
|      - | 7918 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 7919 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 7920 | `		for(;;){\` |
|      - | 7921 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 7922 | `			if(INLEN < 1 ) { break; }\` |
|      - | 7923 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 7924 | `			--INLEN; \` |
|      - | 7925 | `		}\` |
|      - | 7926 | `		for(;;){\` |
|      - | 7927 | `				if(ELEN < 1) { break; }\` |
|      - | 7928 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 7929 | `				OFFT++;\` |
|      - | 7930 | `				ENTRY++;\` |
|      - | 7931 | `				--ELEN;\` |
|      - | 7932 | `		}\` |
|      - | 7933 | `}` |
|      - | 7934 | `/*` |
|      - | 7935 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 7936 | ` * replacement string [i.e: zReplace].` |
|      - | 7937 | ` */` |
|     54 | 7938 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      5 | 7939 | `{` |
|     59 | 7940 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 7941 | `	sxu32 n,m;` |
|     59 | 7942 | `	n = SyBlobLength(pWorker);` |
|     59 | 7943 | `	m = nOfft;` |
|      - | 7944 | `	/* Delete the old entry */` |
|   6689 | 7945 | `	STRDEL(zInput,n,m,nLen);` |
|     59 | 7946 | `	SyBlobLength(pWorker) -= nLen;` |
|     59 | 7947 | `	if( nReplen > 0 ){` |
|     53 | 7948 | `		sxi32 iRep = nReplen;` |
|      - | 7949 | `		sxi32 rc;` |
|      - | 7950 | `		/*` |
|      - | 7951 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 7952 | `		 * string.` |
|      - | 7953 | `		 */` |
|     53 | 7954 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     53 | 7955 | `		if( rc != SXRET_OK ){` |
|      - | 7956 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 7957 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 7958 | `			return rc;` |
|      - | 7959 | `		}` |
|      - | 7960 | `		/* Perform the insertion now */` |
|     53 | 7961 | `		zInput = (char *)SyBlobData(pWorker);` |
|     53 | 7962 | `		n = SyBlobLength(pWorker);` |
|   6481 | 7963 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     53 | 7964 | `		SyBlobLength(pWorker) += nReplen;` |
|     24 | 7965 | `	}` |
|     59 | 7966 | `	return SXRET_OK;` |
|     32 | 7967 | `}` |
|      - | 7968 | `/*` |
|      - | 7969 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 7970 | ` * to collect search/replace string.` |
|      - | 7971 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 7972 | ` */` |
|     94 | 7973 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      5 | 7974 | `{` |
|     99 | 7975 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 7976 | `	SyString sWorker;` |
|      - | 7977 | `	const char *zIn;` |
|      - | 7978 | `	int nByte;` |
|      - | 7979 | `	/* Extract a string representation of the given argument */` |
|     99 | 7980 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     99 | 7981 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     99 | 7982 | `	if( nByte > 0 ){` |
|      - | 7983 | `		char *zDup;` |
|      - | 7984 | `		/* Duplicate the chunk */` |
|     97 | 7985 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 7986 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 7987 | `			);` |
|     97 | 7988 | `		if( zDup == 0 ){` |
|      - | 7989 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 7990 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 7991 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 7992 | `			return SXERR_MEM;` |
|      - | 7993 | `		}` |
|     97 | 7994 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 7995 | `		/* Save the chunk */` |
|     97 | 7996 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     46 | 7997 | `	}` |
|      - | 7998 | `	/* Save for later processing */` |
|     99 | 7999 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 8000 | `	/* All done */` |
|     47 | 8001 | `	SXUNUSED(pKey); /* cc warning */` |
|     99 | 8002 | `	return PH7_OK;` |
|     52 | 8003 | `}` |
|      - | 8004 | `/*` |
|      - | 8005 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 8006 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 8007 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 8008 | ` * Parameters` |
|      - | 8009 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 8010 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 8011 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 8012 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 8013 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 8014 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 8015 | ` * $search` |
|      - | 8016 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 8017 | ` *  to designate multiple needles.` |
|      - | 8018 | ` * $replace` |
|      - | 8019 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 8020 | ` *  to designate multiple replacements.` |
|      - | 8021 | ` * $subject` |
|      - | 8022 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 8023 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 8024 | ` *  of subject, and the return value is an array as well.` |
|      - | 8025 | ` * $count (Not used)` |
|      - | 8026 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 8027 | ` * Return` |
|      - | 8028 | ` * This function returns a string or an array with the replaced values.` |
|      - | 8029 | ` */` |
|  29692 | 8030 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 8031 | `{` |
|      - | 8032 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 8033 | `	ProcStringMatch xMatch;` |
|      - | 8034 | `	const char *zIn,*zFunc;` |
|      - | 8035 | `	str_replace_data sRep;` |
|      - | 8036 | `	SyBlob sWorker;` |
|      - | 8037 | `	SySet sReplace;` |
|      - | 8038 | `	SySet sSearch;` |
|      - | 8039 | `	int rep_str;` |
|      - | 8040 | `	int nByte;` |
|      - | 8041 | `	sxi32 rc;` |
|  29697 | 8042 | `	if( nArg < 3 ){` |
|      - | 8043 | `		/* Missing/Invalid arguments,return null */` |
|    ! 0 | 8044 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8045 | `		return PH7_OK;` |
|      - | 8046 | `	}` |
|      - | 8047 | `	/* Initialize fields */` |
|  29697 | 8048 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  29697 | 8049 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  29697 | 8050 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  29697 | 8051 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  29697 | 8052 | `	sRep.pCtx = pCtx;` |
|  29697 | 8053 | `	sRep.pCollector = &sSearch;` |
|  29697 | 8054 | `	rep_str = 0;` |
|      - | 8055 | `	/* Extract the subject */` |
|  29697 | 8056 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  29697 | 8057 | `	if( nByte < 1 ){` |
|      - | 8058 | `		/* Nothing to replace,return the empty string */` |
|     29 | 8059 | `		ph7_result_string(pCtx,"",0);` |
|     29 | 8060 | `		return PH7_OK;` |
|      - | 8061 | `	}` |
|      - | 8062 | `	/* Copy the subject */` |
|  29669 | 8063 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 8064 | `	/* Search string */` |
|  29669 | 8065 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 8066 | `		/* Collect search string */` |
|     47 | 8067 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|     26 | 8068 | `	}else{` |
|      - | 8069 | `		/* Single pattern */` |
|  29627 | 8070 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  29627 | 8071 | `		if( nByte < 1 ){` |
|      - | 8072 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 8073 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 8074 | `			return PH7_OK;` |
|      - | 8075 | `		}` |
|  29623 | 8076 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 8077 | `		/* Save for later processing */` |
|  29623 | 8078 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 8079 | `	}` |
|      - | 8080 | `	/* Replace string */` |
|  29665 | 8081 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 8082 | `		/* Collect replace string */` |
|      7 | 8083 | `		sRep.pCollector = &sReplace;` |
|      7 | 8084 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 8085 | `	}else{` |
|      - | 8086 | `		/* Single needle */` |
|  29659 | 8087 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  29659 | 8088 | `		rep_str = 1;` |
|  29659 | 8089 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 8090 | `		/* Save for later processing */` |
|  29659 | 8091 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 8092 | `	}` |
|      - | 8093 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  29665 | 8094 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 8095 | `		SySetRelease(&sSearch);` |
|    ! 0 | 8096 | `		SySetRelease(&sReplace);` |
|    ! 0 | 8097 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 8098 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8099 | `	}` |
|      - | 8100 | `	/* Reset loop cursors */` |
|  29665 | 8101 | `	SySetResetCursor(&sSearch);` |
|  29665 | 8102 | `	SySetResetCursor(&sReplace);` |
|  29665 | 8103 | `	pReplace = pSearch = 0; /* cc warning */` |
|  29665 | 8104 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 8105 | `	/* Extract function name */` |
|  29665 | 8106 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 8107 | `	/* Set the default pattern match routine */` |
|  29665 | 8108 | `	xMatch = SyBlobSearch;` |
|  29665 | 8109 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 8110 | `		/* Case insensitive pattern match */` |
|     11 | 8111 | `		xMatch = iPatternMatch;` |
|      5 | 8112 | `	}` |
|      - | 8113 | `	/* Start the replace process */` |
|  59367 | 8114 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 8115 | `		sxu32 nCount,nOfft;` |
|  29707 | 8116 | `		if( pSearch->nByte <  1 ){` |
|      - | 8117 | `			/* Empty string,ignore */` |
|      3 | 8118 | `			continue;` |
|      - | 8119 | `		}` |
|      - | 8120 | `		/* Extract the replace string */` |
|  29705 | 8121 | `		if( rep_str ){` |
|  29695 | 8122 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  14850 | 8123 | `		}else{` |
|     11 | 8124 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 8125 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 8126 | `				 * An empty string is used for the rest of replacement values` |
|      - | 8127 | `				 */` |
|      3 | 8128 | `				pReplace = 0;` |
|      1 | 8129 | `			}` |
|      - | 8130 | `		}` |
|  29705 | 8131 | `		if( pReplace == 0 ){` |
|      - | 8132 | `			/* Use an empty string instead */` |
|      3 | 8133 | `			pReplace = &sTemp;` |
|      1 | 8134 | `		}` |
|  29705 | 8135 | `		nOfft = nCount = 0;` |
|  14877 | 8136 | `		for(;;){` |
|  29759 | 8137 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 8138 | `				break;` |
|      - | 8139 | `			}` |
|      - | 8140 | `			/* Perform a pattern lookup */` |
|  44618 | 8141 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  29742 | 8142 | `				pSearch->nByte,&nOfft);` |
|  29747 | 8143 | `			if( rc != SXRET_OK ){` |
|      - | 8144 | `				/* Pattern not found */` |
|  29693 | 8145 | `				break;` |
|      - | 8146 | `			}` |
|      - | 8147 | `			/* Perform the replace operation */` |
|     59 | 8148 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     59 | 8149 | `			if( rc != SXRET_OK ){` |
|      - | 8150 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 8151 | `				SySetRelease(&sSearch);` |
|    ! 0 | 8152 | `				SySetRelease(&sReplace);` |
|    ! 0 | 8153 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8154 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8155 | `			}` |
|      - | 8156 | `			/* Increment offset counter */` |
|     59 | 8157 | `			nCount += nOfft + pReplace->nByte;` |
|      5 | 8158 | `		}` |
|      5 | 8159 | `	}` |
|      - | 8160 | `	/* All done,clean-up the mess left behind */` |
|  29665 | 8161 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  29665 | 8162 | `	SySetRelease(&sSearch);` |
|  29665 | 8163 | `	SySetRelease(&sReplace);` |
|  29665 | 8164 | `	SyBlobRelease(&sWorker);` |
|  29665 | 8165 | `	if( rc != PH7_OK ){` |
|    ! 0 | 8166 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8167 | `	}` |
|  29665 | 8168 | `	return PH7_OK;` |
|  14851 | 8169 | `}` |
|      - | 8170 | `/*` |
|      - | 8171 | ` * strtr() array form: a single (key,value) pair copied out of the replace_pairs` |
|      - | 8172 | ` * array. The bytes are owned by a persistent pool (see strtr_collect) rather than` |
|      - | 8173 | ` * the transient walker values, which HashmapWalk releases after each callback, so` |
|      - | 8174 | ` * we store byte offsets into that pool instead of raw pointers.` |
|      - | 8175 | ` */` |
|      - | 8176 | `typedef struct strtr_entry strtr_entry;` |
|      - | 8177 | `struct strtr_entry` |
|      - | 8178 | `{` |
|      - | 8179 | `	sxu32 nKeyOfft; /* Offset of the search key inside the pool */` |
|      - | 8180 | `	sxu32 nKeyLen;  /* Length of the search key */` |
|      - | 8181 | `	sxu32 nValOfft; /* Offset of the replacement inside the pool */` |
|      - | 8182 | `	sxu32 nValLen;  /* Length of the replacement */` |
|      - | 8183 | `};` |
|      - | 8184 | `typedef struct strtr_collect strtr_collect;` |
|      - | 8185 | `struct strtr_collect` |
|      - | 8186 | `{` |
|      - | 8187 | `	SyBlob *pPool;  /* Byte pool holding copied key + value bytes */` |
|      - | 8188 | `	SySet  *pTable; /* Set of strtr_entry (parallel offsets into pPool) */` |
|      - | 8189 | `	sxi32   rc;     /* Carries an allocation failure (SXERR_MEM) out of the walker */` |
|      - | 8190 | `};` |
|      - | 8191 | `/*` |
|      - | 8192 | ` * Collect one replace_pairs entry into the persistent pool/offset table.` |
|      - | 8193 | ` * PHP coerces both the key and the value to string (an integer key becomes its` |
|      - | 8194 | ` * decimal form) and ignores an empty-string key.` |
|      - | 8195 | ` */` |
|     20 | 8196 | `static int StrtrCollectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 8197 | `{` |
|     21 | 8198 | `	strtr_collect *pCol = (strtr_collect *)pUserData;` |
|      - | 8199 | `	const char *zKey,*zVal;` |
|      - | 8200 | `	strtr_entry sEnt;` |
|      - | 8201 | `	int nKey,nVal;` |
|     21 | 8202 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     21 | 8203 | `	if( nKey < 1 ){` |
|      - | 8204 | `		/* PHP ignores an empty-string key (it also emits a warning we do not replicate). */` |
|    ! 0 | 8205 | `		return PH7_OK;` |
|      - | 8206 | `	}` |
|     21 | 8207 | `	zVal = ph7_value_to_string(pData,&nVal);` |
|     21 | 8208 | `	sEnt.nKeyOfft = SyBlobLength(pCol->pPool);` |
|     21 | 8209 | `	sEnt.nKeyLen  = (sxu32)nKey;` |
|     21 | 8210 | `	if( SyBlobAppend(pCol->pPool,(const void *)zKey,(sxu32)nKey) != SXRET_OK ){` |
|    ! 0 | 8211 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8212 | `		return SXERR_ABORT;` |
|      - | 8213 | `	}` |
|     21 | 8214 | `	sEnt.nValOfft = SyBlobLength(pCol->pPool);` |
|     21 | 8215 | `	sEnt.nValLen  = (sxu32)nVal;` |
|     21 | 8216 | `	if( nVal > 0 && SyBlobAppend(pCol->pPool,(const void *)zVal,(sxu32)nVal) != SXRET_OK ){` |
|    ! 0 | 8217 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8218 | `		return SXERR_ABORT;` |
|      - | 8219 | `	}` |
|     21 | 8220 | `	if( SySetPut(pCol->pTable,(const void *)&sEnt) != SXRET_OK ){` |
|    ! 0 | 8221 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8222 | `		return SXERR_ABORT;` |
|      - | 8223 | `	}` |
|     21 | 8224 | `	return PH7_OK;` |
|     11 | 8225 | `}` |
|      - | 8226 | `/*` |
|      - | 8227 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 8228 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 8229 | ` *  Translate characters or replace substrings.` |
|      - | 8230 | ` * Parameters` |
|      - | 8231 | ` *  $str` |
|      - | 8232 | ` *  The string being translated.` |
|      - | 8233 | ` * $from` |
|      - | 8234 | ` *  The string being translated to to.` |
|      - | 8235 | ` * $to` |
|      - | 8236 | ` *  The string replacing from.` |
|      - | 8237 | ` * $replace_pairs` |
|      - | 8238 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 8239 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 8240 | ` * Return` |
|      - | 8241 | ` *  The translated string.` |
|      - | 8242 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 8243 | ` */` |
|     12 | 8244 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8245 | `{` |
|      - | 8246 | `	const char *zIn;` |
|      - | 8247 | `	int nLen;` |
|     13 | 8248 | `	if( nArg < 1 ){` |
|      - | 8249 | `		/* Nothing to replace,return FALSE */` |
|    ! 0 | 8250 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8251 | `		return PH7_OK;` |
|      - | 8252 | `	}` |
|     13 | 8253 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 8254 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 8255 | `		/* Invalid arguments */` |
|    ! 0 | 8256 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8257 | `		return PH7_OK;` |
|      - | 8258 | `	}` |
|     18 | 8259 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 8260 | `		strtr_collect sCol;` |
|      - | 8261 | `		SyBlob sPool,sWorker;` |
|      - | 8262 | `		SySet sTable;` |
|      - | 8263 | `		const char *zPool;` |
|      - | 8264 | `		strtr_entry *pEnt;` |
|      - | 8265 | `		sxi32 rc;` |
|      - | 8266 | `		int i,iRun;` |
|      - | 8267 | `		/*` |
|      - | 8268 | `		 * PHP's array-form strtr is a single left-to-right pass over the subject:` |
|      - | 8269 | `		 * at every position it substitutes the LONGEST replace_pairs key that` |
|      - | 8270 | `		 * matches there, then advances past the key (replacements are never` |
|      - | 8271 | `		 * rescanned). It is not a sequential per-key global replace. First copy` |
|      - | 8272 | `		 * the pairs into a persistent pool, then run that scan.` |
|      - | 8273 | `		 */` |
|     11 | 8274 | `		SyBlobInit(&sPool,&pCtx->pVm->sAllocator);` |
|     11 | 8275 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     11 | 8276 | `		SySetInit(&sTable,&pCtx->pVm->sAllocator,sizeof(strtr_entry));` |
|     11 | 8277 | `		sCol.pPool  = &sPool;` |
|     11 | 8278 | `		sCol.pTable = &sTable;` |
|     11 | 8279 | `		sCol.rc     = SXRET_OK;` |
|     11 | 8280 | `		ph7_array_walk(apArg[1],StrtrCollectWalker,&sCol);` |
|     11 | 8281 | `		if( sCol.rc != SXRET_OK ){` |
|      - | 8282 | `			/* Allocation failure while collecting the pairs: surface a fatal */` |
|    ! 0 | 8283 | `			SyBlobRelease(&sPool);` |
|    ! 0 | 8284 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 8285 | `			SySetRelease(&sTable);` |
|    ! 0 | 8286 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 8287 | `		}` |
|      - | 8288 | `		/* The pool is now stable, so offsets can be resolved against its base. */` |
|     11 | 8289 | `		zPool = (const char *)SyBlobData(&sPool);` |
|     11 | 8290 | `		rc = SXRET_OK;` |
|     11 | 8291 | `		iRun = 0; /* Start of the pending run of unmatched bytes copied verbatim. */` |
|     43 | 8292 | `		for( i = 0 ; i < nLen ; ){` |
|     33 | 8293 | `			strtr_entry *pBest = 0;` |
|     33 | 8294 | `			sxu32 nBest = 0;` |
|      - | 8295 | `			/* Pick the longest key that matches at the current position. */` |
|     33 | 8296 | `			SySetResetCursor(&sTable);` |
|     97 | 8297 | `			while( SXRET_OK == SySetGetNextEntry(&sTable,(void **)&pEnt) ){` |
|     64 | 8298 | `				if( pEnt->nKeyLen > nBest` |
|     60 | 8299 | `					&& pEnt->nKeyLen <= (sxu32)(nLen - i)` |
|     56 | 8300 | `					&& SyMemcmp(zPool + pEnt->nKeyOfft,zIn + i,pEnt->nKeyLen) == 0 ){` |
|     29 | 8301 | `					nBest = pEnt->nKeyLen;` |
|     29 | 8302 | `					pBest = pEnt;` |
|     14 | 8303 | `				}` |
|      1 | 8304 | `			}` |
|     33 | 8305 | `			if( pBest == 0 ){` |
|      - | 8306 | `				/* No key here: extend the literal run and copy it in one shot later. */` |
|      9 | 8307 | `				i++;` |
|      9 | 8308 | `				continue;` |
|      - | 8309 | `			}` |
|      - | 8310 | `			/* Flush the pending literal run, then the replacement. */` |
|     25 | 8311 | `			if( i > iRun ){` |
|      5 | 8312 | `				rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(i - iRun));` |
|      2 | 8313 | `			}` |
|     25 | 8314 | `			if( rc == SXRET_OK && pBest->nValLen > 0 ){` |
|     25 | 8315 | `				rc = SyBlobAppend(&sWorker,zPool + pBest->nValOfft,pBest->nValLen);` |
|     12 | 8316 | `			}` |
|     25 | 8317 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8318 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 8319 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8320 | `				SySetRelease(&sTable);` |
|    ! 0 | 8321 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8322 | `			}` |
|     25 | 8323 | `			i += (int)pBest->nKeyLen;` |
|     25 | 8324 | `			iRun = i;` |
|      1 | 8325 | `		}` |
|      - | 8326 | `		/* Flush the trailing literal run. */` |
|     11 | 8327 | `		if( nLen > iRun ){` |
|      3 | 8328 | `			rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(nLen - iRun));` |
|      3 | 8329 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8330 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 8331 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8332 | `				SySetRelease(&sTable);` |
|    ! 0 | 8333 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8334 | `			}` |
|      1 | 8335 | `		}` |
|      - | 8336 | `		/* All done, return the result string */` |
|     16 | 8337 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|     10 | 8338 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 8339 | `		/* Clean-up */` |
|     11 | 8340 | `		SyBlobRelease(&sPool);` |
|     11 | 8341 | `		SyBlobRelease(&sWorker);` |
|     11 | 8342 | `		SySetRelease(&sTable);` |
|     11 | 8343 | `		if( rc != PH7_OK ){` |
|    ! 0 | 8344 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 8345 | `		}` |
|      6 | 8346 | `	}else{` |
|      - | 8347 | `		int i,flen,tlen,c,iOfft;` |
|      - | 8348 | `		const char *zFrom,*zTo;` |
|      3 | 8349 | `		if( nArg < 3 ){` |
|      - | 8350 | `			/* Nothing to replace */` |
|    ! 0 | 8351 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8352 | `			return PH7_OK;` |
|      - | 8353 | `		}` |
|      - | 8354 | `		/* Extract given arguments */` |
|      3 | 8355 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 8356 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 8357 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 8358 | `			/* Nothing to replace */` |
|    ! 0 | 8359 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8360 | `			return PH7_OK;` |
|      - | 8361 | `		}` |
|      - | 8362 | `		/* Start the replace process */` |
|     13 | 8363 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 8364 | `			c = zIn[i];` |
|     11 | 8365 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 8366 | `				if ( iOfft < tlen ){` |
|      5 | 8367 | `					c = zTo[iOfft];` |
|      2 | 8368 | `				}` |
|      2 | 8369 | `			}` |
|     11 | 8370 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 8371 |  |
|      6 | 8372 | `		}` |
|      - | 8373 | `	}` |
|     13 | 8374 | `	return PH7_OK;` |
|      7 | 8375 | `}` |
|      - | 8376 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8377 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8378 | `/*` |
|      - | 8379 | ` * Parse an INI string.` |
|      - | 8380 |  |
|      - | 8381 | ` * According to wikipedia` |
|      - | 8382 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 8383 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 8384 | ` *  Format` |
|      - | 8385 | `*    Properties` |
|      - | 8386 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 8387 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 8388 | `*     Example:` |
|      - | 8389 | `*      name=value` |
|      - | 8390 | `*    Sections` |
|      - | 8391 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 8392 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 8393 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 8394 | `*     or the end of the file. Sections may not be nested.` |
|      - | 8395 | `*     Example:` |
|      - | 8396 | `*      [section]` |
|      - | 8397 | `*   Comments` |
|      - | 8398 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 8399 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 8400 | `*/` |
|     12 | 8401 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 8402 | `{` |
|      - | 8403 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 8404 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 8405 | `	SyHashEntry *pEntry;` |
|      - | 8406 | `	SyString sEntry;` |
|      - | 8407 | `	SyHash sHash;` |
|      - | 8408 | `	int c;` |
|      - | 8409 | `	/* Create an empty array and worker variables */` |
|     13 | 8410 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 8411 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 8412 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 8413 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 8414 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|    ! 0 | 8415 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8416 | `	}` |
|     13 | 8417 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 8418 | `	pCur = pArray;` |
|      - | 8419 | `	/* Start the parse process */` |
|     21 | 8420 | `	for(;;){` |
|      - | 8421 | `		/* Ignore leading white spaces */` |
|     69 | 8422 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 8423 | `			zIn++;` |
|      1 | 8424 | `		}` |
|     43 | 8425 | `		if( zIn >= zEnd ){` |
|      - | 8426 | `			/* No more input to process */` |
|     13 | 8427 | `			break;` |
|      - | 8428 | `		}` |
|     31 | 8429 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 8430 | `			/* Comment til the end of line */` |
|    ! 0 | 8431 | `			zIn++;` |
|    ! 0 | 8432 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 8433 | `				zIn++;` |
|    ! 0 | 8434 | `			}` |
|    ! 0 | 8435 | `			continue;` |
|      - | 8436 | `		}` |
|      - | 8437 | `		/* Reset the string cursor of the working variable */` |
|     31 | 8438 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 8439 | `		if( zIn[0] == '[' ){` |
|      - | 8440 | `			/* Section: Extract the section name */` |
|      9 | 8441 | `			zIn++;` |
|      9 | 8442 | `			zCur = zIn;` |
|     73 | 8443 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 8444 | `				zIn++;` |
|      1 | 8445 | `			}` |
|      9 | 8446 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 8447 | `				/* Save the section name */` |
|      5 | 8448 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 8449 | `				SyStringFullTrim(&sEntry);` |
|      5 | 8450 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 8451 | `				if( sEntry.nByte > 0 ){` |
|      - | 8452 | `					/* Associate an array with the section */` |
|      5 | 8453 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 8454 | `					if( pSection ){` |
|      5 | 8455 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 8456 | `						pCur = pSection;` |
|      2 | 8457 | `					}` |
|      2 | 8458 | `				}` |
|      2 | 8459 | `			}` |
|      9 | 8460 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 8461 | `		}else{` |
|      - | 8462 | `			ph7_value *pOldCur;` |
|      - | 8463 | `			int is_array;` |
|      - | 8464 | `			int iLen;` |
|      - | 8465 | `			/* Properties */` |
|     23 | 8466 | `			is_array = 0;` |
|     23 | 8467 | `			zCur = zIn;` |
|     23 | 8468 | `			iLen = 0; /* cc warning */` |
|     23 | 8469 | `			pOldCur = pCur;` |
|    155 | 8470 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 8471 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 8472 | `					/* Array */` |
|    ! 0 | 8473 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 8474 | `					is_array = 1;` |
|    ! 0 | 8475 | `					if( iLen > 0 ){` |
|    ! 0 | 8476 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 8477 | `						/* Query the hashtable */` |
|    ! 0 | 8478 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 8479 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 8480 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 8481 | `						if( pEntry ){` |
|    ! 0 | 8482 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 8483 | `						}else{` |
|      - | 8484 | `							/* Create an empty array */` |
|    ! 0 | 8485 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 8486 | `							if( pvArr ){` |
|      - | 8487 | `								/* Save the entry */` |
|    ! 0 | 8488 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 8489 | `								/* Insert the entry */` |
|    ! 0 | 8490 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 8491 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 8492 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 8493 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 8494 | `							}` |
|      - | 8495 | `						}` |
|    ! 0 | 8496 | `						if( pvArr ){` |
|    ! 0 | 8497 | `							pCur = pvArr;` |
|    ! 0 | 8498 | `						}` |
|    ! 0 | 8499 | `					}` |
|    ! 0 | 8500 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 8501 | `						zIn++;` |
|    ! 0 | 8502 | `					}` |
|    ! 0 | 8503 | `				}` |
|    133 | 8504 | `				zIn++;` |
|      1 | 8505 | `			}` |
|     23 | 8506 | `			if( !is_array ){` |
|     23 | 8507 | `				iLen = (int)(zIn-zCur);` |
|     11 | 8508 | `			}` |
|      - | 8509 | `			/* Trim the key */` |
|     23 | 8510 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 8511 | `			SyStringFullTrim(&sEntry);` |
|     23 | 8512 | `			if( sEntry.nByte > 0 ){` |
|     23 | 8513 | `				if( !is_array ){` |
|      - | 8514 | `					/* Save the key name */` |
|     23 | 8515 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 8516 | `				}` |
|      - | 8517 | `				/* extract key value */` |
|     23 | 8518 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 8519 | `				zIn++; /* '=' */` |
|     39 | 8520 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 8521 | `					zIn++;` |
|      1 | 8522 | `				}` |
|     23 | 8523 | `				if( zIn < zEnd ){` |
|     21 | 8524 | `					zCur = zIn;` |
|     21 | 8525 | `					c = zIn[0];` |
|     21 | 8526 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 8527 | `						zIn++;` |
|      - | 8528 | `						/* Delimit the value */` |
|    ! 0 | 8529 | `						while( zIn < zEnd ){` |
|    ! 0 | 8530 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 8531 | `								break;` |
|      - | 8532 | `							}` |
|    ! 0 | 8533 | `							zIn++;` |
|    ! 0 | 8534 | `						}` |
|    ! 0 | 8535 | `						if( zIn < zEnd ){` |
|    ! 0 | 8536 | `							zIn++;` |
|    ! 0 | 8537 | `						}` |
|    ! 0 | 8538 | `					}else{` |
|    125 | 8539 | `						while( zIn < zEnd ){` |
|    123 | 8540 | `							if( zIn[0] == '\n' ){` |
|     19 | 8541 | `								if( zIn[-1] != '\\' ){` |
|     19 | 8542 | `									break;` |
|    ! 0 | 8543 | `								}` |
|    105 | 8544 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 8545 | `								/* Inline comments */` |
|    ! 0 | 8546 | `								break;` |
|      - | 8547 | `							}` |
|    105 | 8548 | `							zIn++;` |
|      1 | 8549 | `						}` |
|      - | 8550 | `					}` |
|      - | 8551 | `					/* Trim the value */` |
|     21 | 8552 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 8553 | `					SyStringFullTrim(&sEntry);` |
|     21 | 8554 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 8555 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 8556 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 8557 | `					}` |
|     21 | 8558 | `					if( sEntry.nByte > 0 ){` |
|     21 | 8559 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 8560 | `					}` |
|      - | 8561 | `					/* Insert the key and it's value */` |
|     21 | 8562 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 8563 | `				}` |
|     12 | 8564 | `			}else{` |
|    ! 0 | 8565 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 8566 | `					zIn++;` |
|    ! 0 | 8567 | `				}` |
|      - | 8568 | `			}` |
|     23 | 8569 | `			pCur = pOldCur;` |
|      - | 8570 | `		}` |
|      1 | 8571 | `	}` |
|     13 | 8572 | `	SyHashRelease(&sHash);` |
|      - | 8573 | `	/* Return the parse of the INI string */` |
|     13 | 8574 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 8575 | `	return SXRET_OK;` |
|      7 | 8576 | `}` |
|      - | 8577 | `/*` |
|      - | 8578 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 8579 | ` *  Parse a configuration string.` |
|      - | 8580 | ` * Parameters` |
|      - | 8581 | ` *  $ini` |
|      - | 8582 | ` *   The contents of the ini file being parsed.` |
|      - | 8583 | ` *  $process_sections` |
|      - | 8584 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 8585 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 8586 | ` *  $scanner_mode (Not used)` |
|      - | 8587 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 8588 | ` *   then option values will not be parsed.` |
|      - | 8589 | ` * Return` |
|      - | 8590 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 8591 | ` */` |
|     10 | 8592 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8593 | `{` |
|      - | 8594 | `	const char *zIni;` |
|      - | 8595 | `	int nByte;` |
|     11 | 8596 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8597 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 8598 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8599 | `		return PH7_OK;` |
|      - | 8600 | `	}` |
|      - | 8601 | `	/* Extract the raw INI buffer */` |
|     11 | 8602 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 8603 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|     11 | 8604 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      6 | 8605 | `}` |
|      - | 8606 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8607 |  |
|      - | 8608 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8609 |  |
|      - | 8610 | `/*` |
|      - | 8611 | ` * Ctype Functions.` |
|      - | 8612 | ` * Status:` |
|      - | 8613 | ` *    Stable.` |
|      - | 8614 | ` */` |
|      - | 8615 | `/*` |
|      - | 8616 | ` * bool ctype_alnum(string $text)` |
|      - | 8617 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 8618 | ` * Parameters` |
|      - | 8619 | ` *  $text` |
|      - | 8620 | ` *   The tested string.` |
|      - | 8621 | ` * Return` |
|      - | 8622 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 8623 | ` */` |
|     14 | 8624 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8625 | `{` |
|      - | 8626 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8627 | `	int nLen;` |
|     15 | 8628 | `	if( nArg < 1 ){` |
|      - | 8629 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8630 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8631 | `		return PH7_OK;` |
|      - | 8632 | `	}` |
|      - | 8633 | `	/* Extract the target string */` |
|     15 | 8634 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 8635 | `	zEnd = &zIn[nLen];` |
|     15 | 8636 | `	if( nLen < 1 ){` |
|      - | 8637 | `		/* Empty string,return FALSE */` |
|      3 | 8638 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8639 | `		return PH7_OK;` |
|      - | 8640 | `	}` |
|      - | 8641 | `	/* Perform the requested operation */` |
|     32 | 8642 | `	for(;;){` |
|     65 | 8643 | `		if( zIn >= zEnd ){` |
|      - | 8644 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8645 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8646 | `			return PH7_OK;` |
|      - | 8647 | `		}` |
|     57 | 8648 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 8649 | `			break;` |
|      - | 8650 | `		}` |
|      - | 8651 | `		/* Point to the next character */` |
|     53 | 8652 | `		zIn++;` |
|      1 | 8653 | `	}` |
|      - | 8654 | `	/* The test failed,return FALSE */` |
|      5 | 8655 | `	ph7_result_bool(pCtx,0);` |
|      5 | 8656 | `	return PH7_OK;` |
|      8 | 8657 | `}` |
|      - | 8658 | `/*` |
|      - | 8659 | ` * bool ctype_alpha(string $text)` |
|      - | 8660 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 8661 | ` * Parameters` |
|      - | 8662 | ` *  $text` |
|      - | 8663 | ` *   The tested string.` |
|      - | 8664 | ` * Return` |
|      - | 8665 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 8666 | ` */` |
|     16 | 8667 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8668 | `{` |
|      - | 8669 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8670 | `	int nLen;` |
|     17 | 8671 | `	if( nArg < 1 ){` |
|      - | 8672 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8673 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8674 | `		return PH7_OK;` |
|      - | 8675 | `	}` |
|      - | 8676 | `	/* Extract the target string */` |
|     17 | 8677 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8678 | `	zEnd = &zIn[nLen];` |
|     17 | 8679 | `	if( nLen < 1 ){` |
|      - | 8680 | `		/* Empty string,return FALSE */` |
|      3 | 8681 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8682 | `		return PH7_OK;` |
|      - | 8683 | `	}` |
|      - | 8684 | `	/* Perform the requested operation */` |
|     42 | 8685 | `	for(;;){` |
|     85 | 8686 | `		if( zIn >= zEnd ){` |
|      - | 8687 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8688 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8689 | `			return PH7_OK;` |
|      - | 8690 | `		}` |
|     77 | 8691 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 8692 | `			break;` |
|      - | 8693 | `		}` |
|      - | 8694 | `		/* Point to the next character */` |
|     71 | 8695 | `		zIn++;` |
|      1 | 8696 | `	}` |
|      - | 8697 | `	/* The test failed,return FALSE */` |
|      7 | 8698 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8699 | `	return PH7_OK;` |
|      9 | 8700 | `}` |
|      - | 8701 | `/*` |
|      - | 8702 | ` * bool ctype_cntrl(string $text)` |
|      - | 8703 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 8704 | ` * Parameters` |
|      - | 8705 | ` *  $text` |
|      - | 8706 | ` *   The tested string.` |
|      - | 8707 | ` * Return` |
|      - | 8708 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 8709 | ` */` |
|     16 | 8710 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8711 | `{` |
|      - | 8712 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8713 | `	int nLen;` |
|     17 | 8714 | `	if( nArg < 1 ){` |
|      - | 8715 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8716 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8717 | `		return PH7_OK;` |
|      - | 8718 | `	}` |
|      - | 8719 | `	/* Extract the target string */` |
|     17 | 8720 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8721 | `	zEnd = &zIn[nLen];` |
|     17 | 8722 | `	if( nLen < 1 ){` |
|      - | 8723 | `		/* Empty string,return FALSE */` |
|      3 | 8724 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8725 | `		return PH7_OK;` |
|      - | 8726 | `	}` |
|      - | 8727 | `	/* Perform the requested operation */` |
|     14 | 8728 | `	for(;;){` |
|     29 | 8729 | `		if( zIn >= zEnd ){` |
|      - | 8730 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8731 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8732 | `			return PH7_OK;` |
|      - | 8733 | `		}` |
|     21 | 8734 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8735 | `			/* UTF-8 stream  */` |
|    ! 0 | 8736 | `			break;` |
|      - | 8737 | `		}` |
|     21 | 8738 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 8739 | `			break;` |
|      - | 8740 | `		}` |
|      - | 8741 | `		/* Point to the next character */` |
|     15 | 8742 | `		zIn++;` |
|      1 | 8743 | `	}` |
|      - | 8744 | `	/* The test failed,return FALSE */` |
|      7 | 8745 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8746 | `	return PH7_OK;` |
|      9 | 8747 | `}` |
|      - | 8748 | `/*` |
|      - | 8749 | ` * bool ctype_digit(string $text)` |
|      - | 8750 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 8751 | ` * Parameters` |
|      - | 8752 | ` *  $text` |
|      - | 8753 | ` *   The tested string.` |
|      - | 8754 | ` * Return` |
|      - | 8755 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 8756 | ` */` |
|   1840 | 8757 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 8758 | `{` |
|      - | 8759 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8760 | `	int nLen;` |
|   1845 | 8761 | `	if( nArg < 1 ){` |
|      - | 8762 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8763 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8764 | `		return PH7_OK;` |
|      - | 8765 | `	}` |
|      - | 8766 | `	/* Extract the target string */` |
|   1845 | 8767 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1845 | 8768 | `	zEnd = &zIn[nLen];` |
|   1845 | 8769 | `	if( nLen < 1 ){` |
|      - | 8770 | `		/* Empty string,return FALSE */` |
|      3 | 8771 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8772 | `		return PH7_OK;` |
|      - | 8773 | `	}` |
|      - | 8774 | `	/* Perform the requested operation */` |
|   1708 | 8775 | `	for(;;){` |
|   3421 | 8776 | `		if( zIn >= zEnd ){` |
|      - | 8777 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1533 | 8778 | `			ph7_result_bool(pCtx,1);` |
|   1533 | 8779 | `			return PH7_OK;` |
|      - | 8780 | `		}` |
|   1893 | 8781 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8782 | `			/* UTF-8 stream  */` |
|    ! 0 | 8783 | `			break;` |
|      - | 8784 | `		}` |
|   1893 | 8785 | `		if( !SyisDigit(zIn[0]) ){` |
|    315 | 8786 | `			break;` |
|      - | 8787 | `		}` |
|      - | 8788 | `		/* Point to the next character */` |
|   1583 | 8789 | `		zIn++;` |
|      5 | 8790 | `	}` |
|      - | 8791 | `	/* The test failed,return FALSE */` |
|    315 | 8792 | `	ph7_result_bool(pCtx,0);` |
|    315 | 8793 | `	return PH7_OK;` |
|    925 | 8794 | `}` |
|      - | 8795 | `/*` |
|      - | 8796 | ` * bool ctype_xdigit(string $text)` |
|      - | 8797 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 8798 | ` * Parameters` |
|      - | 8799 | ` *  $text` |
|      - | 8800 | ` *   The tested string.` |
|      - | 8801 | ` * Return` |
|      - | 8802 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 8803 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 8804 | ` */` |
|     18 | 8805 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8806 | `{` |
|      - | 8807 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8808 | `	int nLen;` |
|     19 | 8809 | `	if( nArg < 1 ){` |
|      - | 8810 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8811 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8812 | `		return PH7_OK;` |
|      - | 8813 | `	}` |
|      - | 8814 | `	/* Extract the target string */` |
|     19 | 8815 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 8816 | `	zEnd = &zIn[nLen];` |
|     19 | 8817 | `	if( nLen < 1 ){` |
|      - | 8818 | `		/* Empty string,return FALSE */` |
|      3 | 8819 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8820 | `		return PH7_OK;` |
|      - | 8821 | `	}` |
|      - | 8822 | `	/* Perform the requested operation */` |
|     46 | 8823 | `	for(;;){` |
|     93 | 8824 | `		if( zIn >= zEnd ){` |
|      - | 8825 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 8826 | `			ph7_result_bool(pCtx,1);` |
|     11 | 8827 | `			return PH7_OK;` |
|      - | 8828 | `		}` |
|     83 | 8829 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8830 | `			/* UTF-8 stream  */` |
|    ! 0 | 8831 | `			break;` |
|      - | 8832 | `		}` |
|     83 | 8833 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 8834 | `			break;` |
|      - | 8835 | `		}` |
|      - | 8836 | `		/* Point to the next character */` |
|     77 | 8837 | `		zIn++;` |
|      1 | 8838 | `	}` |
|      - | 8839 | `	/* The test failed,return FALSE */` |
|      7 | 8840 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8841 | `	return PH7_OK;` |
|     10 | 8842 | `}` |
|      - | 8843 | `/*` |
|      - | 8844 | ` * bool ctype_graph(string $text)` |
|      - | 8845 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 8846 | ` * Parameters` |
|      - | 8847 | ` *  $text` |
|      - | 8848 | ` *   The tested string.` |
|      - | 8849 | ` * Return` |
|      - | 8850 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 8851 | ` * (no white space), FALSE otherwise.` |
|      - | 8852 | ` */` |
|     16 | 8853 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8854 | `{` |
|      - | 8855 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8856 | `	int nLen;` |
|     17 | 8857 | `	if( nArg < 1 ){` |
|      - | 8858 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8859 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8860 | `		return PH7_OK;` |
|      - | 8861 | `	}` |
|      - | 8862 | `	/* Extract the target string */` |
|     17 | 8863 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8864 | `	zEnd = &zIn[nLen];` |
|     17 | 8865 | `	if( nLen < 1 ){` |
|      - | 8866 | `		/* Empty string,return FALSE */` |
|      3 | 8867 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8868 | `		return PH7_OK;` |
|      - | 8869 | `	}` |
|      - | 8870 | `	/* Perform the requested operation */` |
|     57 | 8871 | `	for(;;){` |
|    115 | 8872 | `		if( zIn >= zEnd ){` |
|      - | 8873 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8874 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8875 | `			return PH7_OK;` |
|      - | 8876 | `		}` |
|    107 | 8877 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8878 | `			/* UTF-8 stream  */` |
|    ! 0 | 8879 | `			break;` |
|      - | 8880 | `		}` |
|    107 | 8881 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 8882 | `			break;` |
|      - | 8883 | `		}` |
|      - | 8884 | `		/* Point to the next character */` |
|    101 | 8885 | `		zIn++;` |
|      1 | 8886 | `	}` |
|      - | 8887 | `	/* The test failed,return FALSE */` |
|      7 | 8888 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8889 | `	return PH7_OK;` |
|      9 | 8890 | `}` |
|      - | 8891 | `/*` |
|      - | 8892 | ` * bool ctype_print(string $text)` |
|      - | 8893 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 8894 | ` * Parameters` |
|      - | 8895 | ` *  $text` |
|      - | 8896 | ` *   The tested string.` |
|      - | 8897 | ` * Return` |
|      - | 8898 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 8899 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 8900 | ` *  or control function at all.` |
|      - | 8901 | ` */` |
|     16 | 8902 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8903 | `{` |
|      - | 8904 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8905 | `	int nLen;` |
|     17 | 8906 | `	if( nArg < 1 ){` |
|      - | 8907 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8908 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8909 | `		return PH7_OK;` |
|      - | 8910 | `	}` |
|      - | 8911 | `	/* Extract the target string */` |
|     17 | 8912 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8913 | `	zEnd = &zIn[nLen];` |
|     17 | 8914 | `	if( nLen < 1 ){` |
|      - | 8915 | `		/* Empty string,return FALSE */` |
|      3 | 8916 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8917 | `		return PH7_OK;` |
|      - | 8918 | `	}` |
|      - | 8919 | `	/* Perform the requested operation */` |
|     63 | 8920 | `	for(;;){` |
|    127 | 8921 | `		if( zIn >= zEnd ){` |
|      - | 8922 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8923 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8924 | `			return PH7_OK;` |
|      - | 8925 | `		}` |
|    119 | 8926 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8927 | `			/* UTF-8 stream  */` |
|    ! 0 | 8928 | `			break;` |
|      - | 8929 | `		}` |
|    119 | 8930 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 8931 | `			break;` |
|      - | 8932 | `		}` |
|      - | 8933 | `		/* Point to the next character */` |
|    113 | 8934 | `		zIn++;` |
|      1 | 8935 | `	}` |
|      - | 8936 | `	/* The test failed,return FALSE */` |
|      7 | 8937 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8938 | `	return PH7_OK;` |
|      9 | 8939 | `}` |
|      - | 8940 | `/*` |
|      - | 8941 | ` * bool ctype_punct(string $text)` |
|      - | 8942 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 8943 | ` * Parameters` |
|      - | 8944 | ` *  $text` |
|      - | 8945 | ` *   The tested string.` |
|      - | 8946 | ` * Return` |
|      - | 8947 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 8948 | ` *  digit or blank, FALSE otherwise.` |
|      - | 8949 | ` */` |
|     18 | 8950 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8951 | `{` |
|      - | 8952 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8953 | `	int nLen;` |
|     19 | 8954 | `	if( nArg < 1 ){` |
|      - | 8955 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8956 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8957 | `		return PH7_OK;` |
|      - | 8958 | `	}` |
|      - | 8959 | `	/* Extract the target string */` |
|     19 | 8960 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 8961 | `	zEnd = &zIn[nLen];` |
|     19 | 8962 | `	if( nLen < 1 ){` |
|      - | 8963 | `		/* Empty string,return FALSE */` |
|      3 | 8964 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8965 | `		return PH7_OK;` |
|      - | 8966 | `	}` |
|      - | 8967 | `	/* Perform the requested operation */` |
|     38 | 8968 | `	for(;;){` |
|     77 | 8969 | `		if( zIn >= zEnd ){` |
|      - | 8970 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8971 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8972 | `			return PH7_OK;` |
|      - | 8973 | `		}` |
|     69 | 8974 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8975 | `			/* UTF-8 stream  */` |
|    ! 0 | 8976 | `			break;` |
|      - | 8977 | `		}` |
|     69 | 8978 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 8979 | `			break;` |
|      - | 8980 | `		}` |
|      - | 8981 | `		/* Point to the next character */` |
|     61 | 8982 | `		zIn++;` |
|      1 | 8983 | `	}` |
|      - | 8984 | `	/* The test failed,return FALSE */` |
|      9 | 8985 | `	ph7_result_bool(pCtx,0);` |
|      9 | 8986 | `	return PH7_OK;` |
|     10 | 8987 | `}` |
|      - | 8988 | `/*` |
|      - | 8989 | ` * bool ctype_space(string $text)` |
|      - | 8990 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 8991 | ` * Parameters` |
|      - | 8992 | ` *  $text` |
|      - | 8993 | ` *   The tested string.` |
|      - | 8994 | ` * Return` |
|      - | 8995 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 8996 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 8997 | ` *  and form feed characters.` |
|      - | 8998 | ` */` |
|  62891 | 8999 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 9000 | `{` |
|      - | 9001 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9002 | `	int nLen;` |
|  62896 | 9003 | `	if( nArg < 1 ){` |
|      - | 9004 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9005 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9006 | `		return PH7_OK;` |
|      - | 9007 | `	}` |
|      - | 9008 | `	/* Extract the target string */` |
|  62896 | 9009 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  62896 | 9010 | `	zEnd = &zIn[nLen];` |
|  62896 | 9011 | `	if( nLen < 1 ){` |
|      - | 9012 | `		/* Empty string,return FALSE */` |
|      3 | 9013 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9014 | `		return PH7_OK;` |
|      - | 9015 | `	}` |
|      - | 9016 | `	/* Perform the requested operation */` |
|  32559 | 9017 | `	for(;;){` |
|  65038 | 9018 | `		if( zIn >= zEnd ){` |
|      - | 9019 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   2125 | 9020 | `			ph7_result_bool(pCtx,1);` |
|   2125 | 9021 | `			return PH7_OK;` |
|      - | 9022 | `		}` |
|  62918 | 9023 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9024 | `			/* UTF-8 stream  */` |
|    ! 0 | 9025 | `			break;` |
|      - | 9026 | `		}` |
|  62918 | 9027 | `		if( !SyisSpace(zIn[0]) ){` |
|  60774 | 9028 | `			break;` |
|      - | 9029 | `		}` |
|      - | 9030 | `		/* Point to the next character */` |
|   2149 | 9031 | `		zIn++;` |
|      5 | 9032 | `	}` |
|      - | 9033 | `	/* The test failed,return FALSE */` |
|  60774 | 9034 | `	ph7_result_bool(pCtx,0);` |
|  60774 | 9035 | `	return PH7_OK;` |
|  31493 | 9036 | `}` |
|      - | 9037 | `/*` |
|      - | 9038 | ` * bool ctype_lower(string $text)` |
|      - | 9039 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 9040 | ` * Parameters` |
|      - | 9041 | ` *  $text` |
|      - | 9042 | ` *   The tested string.` |
|      - | 9043 | ` * Return` |
|      - | 9044 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 9045 | ` */` |
|     16 | 9046 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9047 | `{` |
|      - | 9048 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9049 | `	int nLen;` |
|     17 | 9050 | `	if( nArg < 1 ){` |
|      - | 9051 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9052 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9053 | `		return PH7_OK;` |
|      - | 9054 | `	}` |
|      - | 9055 | `	/* Extract the target string */` |
|     17 | 9056 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9057 | `	zEnd = &zIn[nLen];` |
|     17 | 9058 | `	if( nLen < 1 ){` |
|      - | 9059 | `		/* Empty string,return FALSE */` |
|      3 | 9060 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9061 | `		return PH7_OK;` |
|      - | 9062 | `	}` |
|      - | 9063 | `	/* Perform the requested operation */` |
|     27 | 9064 | `	for(;;){` |
|     55 | 9065 | `		if( zIn >= zEnd ){` |
|      - | 9066 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 9067 | `			ph7_result_bool(pCtx,1);` |
|      5 | 9068 | `			return PH7_OK;` |
|      - | 9069 | `		}` |
|     51 | 9070 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 9071 | `			break;` |
|      - | 9072 | `		}` |
|      - | 9073 | `		/* Point to the next character */` |
|     41 | 9074 | `		zIn++;` |
|      1 | 9075 | `	}` |
|      - | 9076 | `	/* The test failed,return FALSE */` |
|     11 | 9077 | `	ph7_result_bool(pCtx,0);` |
|     11 | 9078 | `	return PH7_OK;` |
|      9 | 9079 | `}` |
|      - | 9080 | `/*` |
|      - | 9081 | ` * bool ctype_upper(string $text)` |
|      - | 9082 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 9083 | ` * Parameters` |
|      - | 9084 | ` *  $text` |
|      - | 9085 | ` *   The tested string.` |
|      - | 9086 | ` * Return` |
|      - | 9087 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 9088 | ` */` |
|     16 | 9089 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9090 | `{` |
|      - | 9091 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9092 | `	int nLen;` |
|     17 | 9093 | `	if( nArg < 1 ){` |
|      - | 9094 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9095 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9096 | `		return PH7_OK;` |
|      - | 9097 | `	}` |
|      - | 9098 | `	/* Extract the target string */` |
|     17 | 9099 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9100 | `	zEnd = &zIn[nLen];` |
|     17 | 9101 | `	if( nLen < 1 ){` |
|      - | 9102 | `		/* Empty string,return FALSE */` |
|      3 | 9103 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9104 | `		return PH7_OK;` |
|      - | 9105 | `	}` |
|      - | 9106 | `	/* Perform the requested operation */` |
|     28 | 9107 | `	for(;;){` |
|     57 | 9108 | `		if( zIn >= zEnd ){` |
|      - | 9109 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 9110 | `			ph7_result_bool(pCtx,1);` |
|      5 | 9111 | `			return PH7_OK;` |
|      - | 9112 | `		}` |
|     53 | 9113 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 9114 | `			break;` |
|      - | 9115 | `		}` |
|      - | 9116 | `		/* Point to the next character */` |
|     43 | 9117 | `		zIn++;` |
|      1 | 9118 | `	}` |
|      - | 9119 | `	/* The test failed,return FALSE */` |
|     11 | 9120 | `	ph7_result_bool(pCtx,0);` |
|     11 | 9121 | `	return PH7_OK;` |
|      9 | 9122 | `}` |
|      - | 9123 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 9124 | `/*` |
|      - | 9125 | ` * Section:` |
|      - | 9126 | ` *    URL handling Functions.` |
|      - | 9127 | ` * Status:` |
|      - | 9128 | ` *    Stable.` |
|      - | 9129 | ` */` |
|      - | 9130 | `/*` |
|      - | 9131 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 9132 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 9133 | ` */` |
|   1026 | 9134 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 9135 | `{` |
|      - | 9136 | `	/* Store in the call context result buffer */` |
|   1028 | 9137 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 9138 | `	return SXRET_OK;` |
|      2 | 9139 | `}` |
|      - | 9140 | `/*` |
|      - | 9141 | ` * string base64_encode(string $data)` |
|      - | 9142 | ` * string convert_uuencode(string $data)` |
|      - | 9143 | ` *  Encodes data with MIME base64` |
|      - | 9144 | ` * Parameter` |
|      - | 9145 | ` *  $data` |
|      - | 9146 | ` *    Data to encode` |
|      - | 9147 | ` * Return` |
|      - | 9148 | ` *  Encoded data or FALSE on failure.` |
|      - | 9149 | ` */` |
|      6 | 9150 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9151 | `{` |
|      - | 9152 | `	const char *zIn;` |
|      - | 9153 | `	int nLen;` |
|      7 | 9154 | `	if( nArg < 1 ){` |
|      - | 9155 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9156 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9157 | `		return PH7_OK;` |
|      - | 9158 | `	}` |
|      - | 9159 | `	/* Extract the input string */` |
|      7 | 9160 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 9161 | `	if( nLen < 1 ){` |
|      - | 9162 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9163 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9164 | `		return PH7_OK;` |
|      - | 9165 | `	}` |
|      - | 9166 | `	/* Perform the BASE64 encoding */` |
|      7 | 9167 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 9168 | `	return PH7_OK;` |
|      4 | 9169 | `}` |
|      - | 9170 | `/*` |
|      - | 9171 | ` * string base64_decode(string $data)` |
|      - | 9172 | ` * string convert_uudecode(string $data)` |
|      - | 9173 | ` *  Decodes data encoded with MIME base64` |
|      - | 9174 | ` * Parameter` |
|      - | 9175 | ` *  $data` |
|      - | 9176 | ` *    Encoded data.` |
|      - | 9177 | ` * Return` |
|      - | 9178 | ` *  Returns the original data or FALSE on failure.` |
|      - | 9179 | ` */` |
|     34 | 9180 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 9181 | `{` |
|      - | 9182 | `	const char *zIn;` |
|      - | 9183 | `	int nLen;` |
|     36 | 9184 | `	if( nArg < 1 ){` |
|      - | 9185 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9186 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9187 | `		return PH7_OK;` |
|      - | 9188 | `	}` |
|      - | 9189 | `	/* Extract the input string */` |
|     36 | 9190 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 9191 | `	if( nLen < 1 ){` |
|      - | 9192 | `		/* Nothing to process,return FALSE */` |
|      3 | 9193 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9194 | `		return PH7_OK;` |
|      - | 9195 | `	}` |
|      - | 9196 | `	/* Perform the BASE64 decoding */` |
|     34 | 9197 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 9198 | `	return PH7_OK;` |
|     19 | 9199 | `}` |
|      - | 9200 | `/*` |
|      - | 9201 | ` * string urlencode(string $str)` |
|      - | 9202 | ` *  URL encoding` |
|      - | 9203 | ` * Parameter` |
|      - | 9204 | ` *  $data` |
|      - | 9205 | ` *   Input string.` |
|      - | 9206 | ` * Return` |
|      - | 9207 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 9208 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 9209 | ` *  encoded as plus (+) signs.` |
|      - | 9210 | ` */` |
|      4 | 9211 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9212 | `{` |
|      - | 9213 | `	const char *zIn;` |
|      - | 9214 | `	int nLen;` |
|      5 | 9215 | `	if( nArg < 1 ){` |
|      - | 9216 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9217 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9218 | `		return PH7_OK;` |
|      - | 9219 | `	}` |
|      - | 9220 | `	/* Extract the input string */` |
|      5 | 9221 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 9222 | `	if( nLen < 1 ){` |
|      - | 9223 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9224 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9225 | `		return PH7_OK;` |
|      - | 9226 | `	}` |
|      - | 9227 | `	/* Perform the URL encoding */` |
|      5 | 9228 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 9229 | `	return PH7_OK;` |
|      3 | 9230 | `}` |
|      - | 9231 | `/*` |
|      - | 9232 | ` * string urldecode(string $str)` |
|      - | 9233 | ` *  Decodes any %## encoding in the given string.` |
|      - | 9234 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 9235 | ` * Parameter` |
|      - | 9236 | ` *  $data` |
|      - | 9237 | ` *    Input string.` |
|      - | 9238 | ` * Return` |
|      - | 9239 | ` *  Decoded URL or FALSE on failure.` |
|      - | 9240 | ` */` |
|      6 | 9241 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9242 | `{` |
|      - | 9243 | `	const char *zIn;` |
|      - | 9244 | `	int nLen;` |
|      7 | 9245 | `	if( nArg < 1 ){` |
|      - | 9246 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9247 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9248 | `		return PH7_OK;` |
|      - | 9249 | `	}` |
|      - | 9250 | `	/* Extract the input string */` |
|      7 | 9251 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 9252 | `	if( nLen < 1 ){` |
|      - | 9253 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9254 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9255 | `		return PH7_OK;` |
|      - | 9256 | `	}` |
|      - | 9257 | `	/* Perform the URL decoding */` |
|      7 | 9258 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 9259 | `	return PH7_OK;` |
|      4 | 9260 | `}` |
|      - | 9261 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9262 | `/* Table of the built-in functions */` |
|      - | 9263 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 9264 | `	   /* Variable handling functions */` |
|      - | 9265 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 9266 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 9267 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 9268 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 9269 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 9270 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 9271 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 9272 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 9273 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 9274 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 9275 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 9276 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 9277 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 9278 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 9279 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 9280 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 9281 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 9282 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 9283 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 9284 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 9285 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9286 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 9287 | `	   /* Math functions */` |
|      - | 9288 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 9289 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 9290 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 9291 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 9292 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 9293 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 9294 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 9295 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 9296 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 9297 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 9298 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 9299 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 9300 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 9301 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 9302 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 9303 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 9304 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 9305 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 9306 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 9307 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 9308 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 9309 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 9310 | `	{ "round",    PH7_builtin_round        },` |
|      - | 9311 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 9312 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 9313 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 9314 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 9315 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 9316 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 9317 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 9318 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 9319 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 9320 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9321 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9322 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 9323 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9324 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9325 | `	   /* String handling functions */` |
|      - | 9326 |  |
|      - | 9327 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 9328 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 9329 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 9330 | `	{ "substr_replace",  PH7_builtin_substr_replace },` |
|      - | 9331 | `	{ "levenshtein",     PH7_builtin_levenshtein },` |
|      - | 9332 | `	{ "similar_text",    PH7_builtin_similar_text },` |
|      - | 9333 | `	{ "str_word_count",  PH7_builtin_str_word_count },` |
|      - | 9334 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 9335 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 9336 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 9337 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 9338 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 9339 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 9340 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 9341 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 9342 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 9343 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 9344 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 9345 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 9346 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 9347 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 9348 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 9349 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 9350 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 9351 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 9352 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 9353 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 9354 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 9355 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 9356 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 9357 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 9358 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 9359 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 9360 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 9361 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 9362 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 9363 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 9364 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 9365 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 9366 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 9367 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 9368 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 9369 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 9370 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 9371 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 9372 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 9373 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 9374 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 9375 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 9376 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 9377 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 9378 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 9379 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 9380 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 9381 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 9382 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 9383 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9384 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9385 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 9386 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 9387 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 9388 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 9389 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9390 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9391 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 9392 |  |
|      - | 9393 |  |
|      - | 9394 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 9395 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 9396 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 9397 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 9398 | `	{ "hash",         PH7_builtin_hash      },` |
|      - | 9399 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|      - | 9400 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|      - | 9401 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|      - | 9402 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 9403 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|      - | 9404 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|      - | 9405 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|      - | 9406 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|      - | 9407 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|      - | 9408 | `	{ "filter_input",          PH7_builtin_filter_input },` |
|      - | 9409 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9410 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9411 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 9412 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 9413 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9414 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9415 |  |
|      - | 9416 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 9417 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 9418 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 9419 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 9420 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 9421 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 9422 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 9423 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 9424 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 9425 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 9426 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 9427 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 9428 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9429 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9430 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 9431 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9432 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9433 |  |
|      - | 9434 | `	         /* Ctype functions */` |
|      - | 9435 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 9436 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 9437 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 9438 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 9439 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 9440 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 9441 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 9442 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 9443 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 9444 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 9445 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 9446 | `	         /* Time functions */` |
|      - | 9447 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 9448 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 9449 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 9450 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 9451 | `	{ "date",        PH7_builtin_date         },` |
|      - | 9452 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 9453 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 9454 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 9455 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 9456 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 9457 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 9458 | `	{ "date_default_timezone_get", PH7_builtin_date_default_timezone_get },` |
|      - | 9459 | `	{ "date_default_timezone_set", PH7_builtin_date_default_timezone_set },` |
|      - | 9460 | `	        /* URL functions */` |
|      - | 9461 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 9462 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 9463 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 9464 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 9465 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 9466 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 9467 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 9468 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 9469 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9470 | `};` |
|      - | 9471 | `/*` |
|      - | 9472 | ` * Register the built-in functions defined above,the array functions` |
|      - | 9473 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 9474 | ` */` |
|   3488 | 9475 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      5 | 9476 | `{` |
|      - | 9477 | `	sxu32 n;` |
| 606917 | 9478 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 603429 | 9479 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 301717 | 9480 | `	}` |
|      - | 9481 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   3493 | 9482 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 9483 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   3493 | 9484 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   3493 | 9485 | `}` |
|      - | 9486 |  |
