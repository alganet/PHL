# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3761/4336 lines (86.74%)

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
|      - |   15 | `/* This file implement built-in 'foreign' functions for the PH7 engine */` |
|      - |   16 | `/*` |
|      - |   17 | ` * Section:` |
|      - |   18 | ` *    Variable handling Functions.` |
|      - |   19 | ` * Status:` |
|      - |   20 | ` *    Stable.` |
|      - |   21 | ` */` |
|      - |   22 | `/*` |
|      - |   23 | ` * bool is_bool($var)` |
|      - |   24 | ` *  Finds out whether a variable is a boolean.` |
|      - |   25 | ` * Parameters` |
|      - |   26 | ` *   $var: The variable being evaluated.` |
|      - |   27 | ` * Return` |
|      - |   28 | ` *  TRUE if var is a boolean. False otherwise.` |
|      - |   29 | ` */` |
|     28 |   30 | `static int PH7_builtin_is_bool(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   31 | `{` |
|     29 |   32 | `	int res = 0; /* Assume false by default */` |
|     29 |   33 | `	if( nArg > 0 ){` |
|     29 |   34 | `		res = ph7_value_is_bool(apArg[0]);` |
|     14 |   35 | `	}` |
|      - |   36 | `	/* Query result */` |
|     29 |   37 | `	ph7_result_bool(pCtx,res);` |
|     29 |   38 | `	return PH7_OK;` |
|      1 |   39 | `}` |
|      - |   40 | `/*` |
|      - |   41 | ` * bool is_float($var)` |
|      - |   42 | ` * bool is_real($var)` |
|      - |   43 | ` * bool is_double($var)` |
|      - |   44 | ` *  Finds out whether a variable is a float.` |
|      - |   45 | ` * Parameters` |
|      - |   46 | ` *   $var: The variable being evaluated.` |
|      - |   47 | ` * Return` |
|      - |   48 | ` *  TRUE if var is a float. False otherwise.` |
|      - |   49 | ` */` |
|    204 |   50 | `static int PH7_builtin_is_float(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   51 | `{` |
|    205 |   52 | `	int res = 0; /* Assume false by default */` |
|    205 |   53 | `	if( nArg > 0 ){` |
|    205 |   54 | `		res = ph7_value_is_float(apArg[0]);` |
|    102 |   55 | `	}` |
|      - |   56 | `	/* Query result */` |
|    205 |   57 | `	ph7_result_bool(pCtx,res);` |
|    205 |   58 | `	return PH7_OK;` |
|      1 |   59 | `}` |
|      - |   60 | `/*` |
|      - |   61 | ` * bool is_int($var)` |
|      - |   62 | ` * bool is_integer($var)` |
|      - |   63 | ` * bool is_long($var)` |
|      - |   64 | ` *  Finds out whether a variable is an integer.` |
|      - |   65 | ` * Parameters` |
|      - |   66 | ` *   $var: The variable being evaluated.` |
|      - |   67 | ` * Return` |
|      - |   68 | ` *  TRUE if var is an integer. False otherwise.` |
|      - |   69 | ` */` |
|    630 |   70 | `static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |   71 | `{` |
|    633 |   72 | `	int res = 0; /* Assume false by default */` |
|    633 |   73 | `	if( nArg > 0 ){` |
|      - |   74 | `		/* Strict PHP identity: a float is never an int, even when it holds an` |
|      - |   75 | `		 * integer value (1.0). An integer-valued real carries both MEMOBJ_INT` |
|      - |   76 | `		 * (cached) and MEMOBJ_REAL, so REAL must be excluded here. */` |
|    633 |   77 | `		res = ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]);` |
|    315 |   78 | `	}` |
|      - |   79 | `	/* Query result */` |
|    633 |   80 | `	ph7_result_bool(pCtx,res);` |
|    633 |   81 | `	return PH7_OK;` |
|      3 |   82 | `}` |
|      - |   83 | `/*` |
|      - |   84 | ` * bool is_string($var)` |
|      - |   85 | ` *  Finds out whether a variable is a string.` |
|      - |   86 | ` * Parameters` |
|      - |   87 | ` *   $var: The variable being evaluated.` |
|      - |   88 | ` * Return` |
|      - |   89 | ` *  TRUE if var is string. False otherwise.` |
|      - |   90 | ` */` |
|    124 |   91 | `static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   92 | `{` |
|    125 |   93 | `	int res = 0; /* Assume false by default */` |
|    125 |   94 | `	if( nArg > 0 ){` |
|    125 |   95 | `		res = ph7_value_is_string(apArg[0]);` |
|     62 |   96 | `	}` |
|      - |   97 | `	/* Query result */` |
|    125 |   98 | `	ph7_result_bool(pCtx,res);` |
|    125 |   99 | `	return PH7_OK;` |
|      1 |  100 | `}` |
|      - |  101 | `/*` |
|      - |  102 | ` * bool is_null($var)` |
|      - |  103 | ` *  Finds out whether a variable is NULL.` |
|      - |  104 | ` * Parameters` |
|      - |  105 | ` *   $var: The variable being evaluated.` |
|      - |  106 | ` * Return` |
|      - |  107 | ` *  TRUE if var is NULL. False otherwise.` |
|      - |  108 | ` */` |
|     84 |  109 | `static int PH7_builtin_is_null(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  110 | `{` |
|     87 |  111 | `	int res = 0; /* Assume false by default */` |
|     87 |  112 | `	if( nArg > 0 ){` |
|     87 |  113 | `		res = ph7_value_is_null(apArg[0]);` |
|     42 |  114 | `	}` |
|      - |  115 | `	/* Query result */` |
|     87 |  116 | `	ph7_result_bool(pCtx,res);` |
|     87 |  117 | `	return PH7_OK;` |
|      3 |  118 | `}` |
|      - |  119 | `/*` |
|      - |  120 | ` * bool is_numeric($var)` |
|      - |  121 | ` *  Find out whether a variable is NULL.` |
|      - |  122 | ` * Parameters` |
|      - |  123 | ` *  $var: The variable being evaluated.` |
|      - |  124 | ` * Return` |
|      - |  125 | ` *  True if var is numeric. False otherwise.` |
|      - |  126 | ` */` |
|     36 |  127 | `static int PH7_builtin_is_numeric(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  128 | `{` |
|     41 |  129 | `	int res = 0; /* Assume false by default */` |
|     41 |  130 | `	if( nArg > 0 ){` |
|     41 |  131 | `		res = ph7_value_is_numeric(apArg[0]);` |
|     18 |  132 | `	}` |
|      - |  133 | `	/* Query result */` |
|     41 |  134 | `	ph7_result_bool(pCtx,res);` |
|     41 |  135 | `	return PH7_OK;` |
|      5 |  136 | `}` |
|      - |  137 | `/*` |
|      - |  138 | ` * bool is_scalar($var)` |
|      - |  139 | ` *  Find out whether a variable is a scalar.` |
|      - |  140 | ` * Parameters` |
|      - |  141 | ` *  $var: The variable being evaluated.` |
|      - |  142 | ` * Return` |
|      - |  143 | ` *  True if var is scalar. False otherwise.` |
|      - |  144 | ` */` |
|     12 |  145 | `static int PH7_builtin_is_scalar(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  146 | `{` |
|     13 |  147 | `	int res = 0; /* Assume false by default */` |
|     13 |  148 | `	if( nArg > 0 ){` |
|     13 |  149 | `		res = ph7_value_is_scalar(apArg[0]);` |
|      6 |  150 | `	}` |
|      - |  151 | `	/* Query result */` |
|     13 |  152 | `	ph7_result_bool(pCtx,res);` |
|     13 |  153 | `	return PH7_OK;` |
|      1 |  154 | `}` |
|      - |  155 | `/*` |
|      - |  156 | ` * bool is_array($var)` |
|      - |  157 | ` *  Find out whether a variable is an array.` |
|      - |  158 | ` * Parameters` |
|      - |  159 | ` *  $var: The variable being evaluated.` |
|      - |  160 | ` * Return` |
|      - |  161 | ` *  True if var is an array. False otherwise.` |
|      - |  162 | ` */` |
|    244 |  163 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  164 | `{` |
|    249 |  165 | `	int res = 0; /* Assume false by default */` |
|    249 |  166 | `	if( nArg > 0 ){` |
|    249 |  167 | `		res = ph7_value_is_array(apArg[0]);` |
|    122 |  168 | `	}` |
|      - |  169 | `	/* Query result */` |
|    249 |  170 | `	ph7_result_bool(pCtx,res);` |
|    249 |  171 | `	return PH7_OK;` |
|      5 |  172 | `}` |
|      - |  173 | `/*` |
|      - |  174 | ` * bool is_object($var)` |
|      - |  175 | ` *  Find out whether a variable is an object.` |
|      - |  176 | ` * Parameters` |
|      - |  177 | ` *  $var: The variable being evaluated.` |
|      - |  178 | ` * Return` |
|      - |  179 | ` *  True if var is an object. False otherwise.` |
|      - |  180 | ` */` |
|     20 |  181 | `static int PH7_builtin_is_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  182 | `{` |
|     21 |  183 | `	int res = 0; /* Assume false by default */` |
|     21 |  184 | `	if( nArg > 0 ){` |
|     21 |  185 | `		res = ph7_value_is_object(apArg[0]);` |
|     10 |  186 | `	}` |
|      - |  187 | `	/* Query result */` |
|     21 |  188 | `	ph7_result_bool(pCtx,res);` |
|     21 |  189 | `	return PH7_OK;` |
|      1 |  190 | `}` |
|      - |  191 | `/*` |
|      - |  192 | ` * bool is_resource($var)` |
|      - |  193 | ` *  Find out whether a variable is a resource.` |
|      - |  194 | ` * Parameters` |
|      - |  195 | ` *  $var: The variable being evaluated.` |
|      - |  196 | ` * Return` |
|      - |  197 | ` *  True if a resource. False otherwise.` |
|      - |  198 | ` */` |
|     58 |  199 | `static int PH7_builtin_is_resource(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  200 | `{` |
|     61 |  201 | `	int res = 0; /* Assume false by default */` |
|     61 |  202 | `	if( nArg > 0 ){` |
|     61 |  203 | `		res = ph7_value_is_resource(apArg[0]);` |
|     29 |  204 | `	}` |
|     61 |  205 | `	ph7_result_bool(pCtx,res);` |
|     61 |  206 | `	return PH7_OK;` |
|      3 |  207 | `}` |
|      - |  208 | `/*` |
|      - |  209 | ` * float floatval($var)` |
|      - |  210 | ` *  Get float value of a variable.` |
|      - |  211 | ` * Parameter` |
|      - |  212 | ` *  $var: The variable being processed.` |
|      - |  213 | ` * Return` |
|      - |  214 | ` *  the float value of a variable.` |
|      - |  215 | ` */` |
|      4 |  216 | `static int PH7_builtin_floatval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  217 | `{` |
|      5 |  218 | `	if( nArg < 1 ){` |
|      - |  219 | `		/* return 0.0 */` |
|    ! 0 |  220 | `		ph7_result_double(pCtx,0);` |
|    ! 0 |  221 | `	}else{` |
|      - |  222 | `		double dval;` |
|      - |  223 | `		/* Perform the cast */` |
|      5 |  224 | `		dval = ph7_value_to_double(apArg[0]);` |
|      5 |  225 | `		ph7_result_double(pCtx,dval);` |
|      - |  226 | `	}` |
|      5 |  227 | `	return PH7_OK;` |
|      1 |  228 | `}` |
|      - |  229 | `/*` |
|      - |  230 | ` * int intval($var)` |
|      - |  231 | ` *  Get integer value of a variable.` |
|      - |  232 | ` * Parameter` |
|      - |  233 | ` *  $var: The variable being processed.` |
|      - |  234 | ` * Return` |
|      - |  235 | ` *  the int value of a variable.` |
|      - |  236 | ` */` |
|     24 |  237 | `static int PH7_builtin_intval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  238 | `{` |
|     25 |  239 | `	if( nArg < 1 ){` |
|      - |  240 | `		/* return 0 */` |
|    ! 0 |  241 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  242 | `	}else{` |
|      - |  243 | `		sxi64 iVal;` |
|      - |  244 | `		/* Perform the cast */` |
|     25 |  245 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|     25 |  246 | `		ph7_result_int64(pCtx,iVal);` |
|      - |  247 | `	}` |
|     25 |  248 | `	return PH7_OK;` |
|      1 |  249 | `}` |
|      - |  250 | `/*` |
|      - |  251 | ` * string strval($var)` |
|      - |  252 | ` *  Get the string representation of a variable.` |
|      - |  253 | ` * Parameter` |
|      - |  254 | ` *  $var: The variable being processed.` |
|      - |  255 | ` * Return` |
|      - |  256 | ` *  the string value of a variable.` |
|      - |  257 | ` */` |
|      2 |  258 | `static int PH7_builtin_strval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  259 | `{` |
|      3 |  260 | `	if( nArg < 1 ){` |
|      - |  261 | `		/* return NULL */` |
|    ! 0 |  262 | `		ph7_result_null(pCtx);` |
|    ! 0 |  263 | `	}else{` |
|      - |  264 | `		const char *zVal;` |
|      3 |  265 | `		int iLen = 0; /* cc -O6 warning */` |
|      - |  266 | `		/* Perform the cast */` |
|      3 |  267 | `		zVal = ph7_value_to_string(apArg[0],&iLen);` |
|      3 |  268 | `		ph7_result_string(pCtx,zVal,iLen);` |
|      - |  269 | `	}` |
|      3 |  270 | `	return PH7_OK;` |
|      1 |  271 | `}` |
|      - |  272 | `/*` |
|      - |  273 | ` * bool boolval($var)` |
|      - |  274 | ` *  Get the boolean value of a variable.` |
|      - |  275 | ` * Parameter` |
|      - |  276 | ` *  $var: The variable being processed.` |
|      - |  277 | ` * Return` |
|      - |  278 | ` *  the bool value of a variable.` |
|      - |  279 | ` */` |
|     16 |  280 | `static int PH7_builtin_boolval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  281 | `{` |
|      - |  282 | `	int bVal;` |
|     18 |  283 | `	if( nArg != 1 ){` |
|      4 |  284 | `		return PH7_VmThrowException(pCtx,` |
|      - |  285 | `			"ArgumentCountError",` |
|      - |  286 | `			"boolval() expects exactly 1 argument, %d given",` |
|      1 |  287 | `			nArg` |
|      - |  288 | `			);` |
|      - |  289 | `	}` |
|      - |  290 | `	/* Perform the cast */` |
|     15 |  291 | `	bVal = ph7_value_to_bool(apArg[0]);` |
|     15 |  292 | `	ph7_result_bool(pCtx,bVal);` |
|     15 |  293 | `	return PH7_OK;` |
|     10 |  294 | `}` |
|      - |  295 | `/*` |
|      - |  296 | ` * bool empty($var)` |
|      - |  297 | ` *  Determine whether a variable is empty.` |
|      - |  298 | ` * Parameters` |
|      - |  299 | ` *   $var: The variable being checked.` |
|      - |  300 | ` * Return` |
|      - |  301 | ` *  0 if var has a non-empty and non-zero value.1 otherwise.` |
|      - |  302 | ` */` |
|  33230 |  303 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  304 | `{` |
|  33235 |  305 | `	int res = 1; /* Assume empty by default */` |
|  33235 |  306 | `	if( nArg > 0 ){` |
|  33233 |  307 | `		res = ph7_value_is_empty(apArg[0]);` |
|  16614 |  308 | `	}` |
|  33235 |  309 | `	ph7_result_bool(pCtx,res);` |
|  33235 |  310 | `	return PH7_OK;` |
|      - |  311 |  |
|      5 |  312 | `}` |
|      - |  313 | `#ifndef PH7_DISABLE_BUILTIN_FUNC` |
|      - |  314 | `#define PH7_NEED_BUILTIN_REG 1` |
|      - |  315 | `#endif` |
|      - |  316 | `#ifndef PH7_DISABLE_DISK_IO` |
|      - |  317 | `#define PH7_NEED_FMT_AND_INI 1` |
|      - |  318 | `#endif` |
|      - |  319 |  |
|      - |  320 | `/* Math functions moved to builtin_math.c */` |
|      - |  321 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - |  322 | `/*` |
|      - |  323 | ` * Section:` |
|      - |  324 | ` *    String handling Functions.` |
|      - |  325 | ` * Status:` |
|      - |  326 | ` *    Stable.` |
|      - |  327 | ` */` |
|      - |  328 | `/*` |
|      - |  329 | ` * string substr(string $string,int $start[, int $length ])` |
|      - |  330 | ` *  Return part of a string.` |
|      - |  331 | ` * Parameters` |
|      - |  332 | ` *  $string` |
|      - |  333 | ` *   The input string. Must be one character or longer.` |
|      - |  334 | ` * $start` |
|      - |  335 | ` *   If start is non-negative, the returned string will start at the start'th position` |
|      - |  336 | ` *   in string, counting from zero. For instance, in the string 'abcdef', the character` |
|      - |  337 | ` *   at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - |  338 | ` *   If start is negative, the returned string will start at the start'th character` |
|      - |  339 | ` *   from the end of string.` |
|      - |  340 | ` *   If string is less than or equal to start characters long, FALSE will be returned.` |
|      - |  341 | ` * $length` |
|      - |  342 | ` *   If length is given and is positive, the string returned will contain at most length` |
|      - |  343 | ` *   characters beginning from start (depending on the length of string).` |
|      - |  344 | ` *   If length is given and is negative, then that many characters will be omitted from` |
|      - |  345 | ` *   the end of string (after the start position has been calculated when a start is negative).` |
|      - |  346 | ` *   If start denotes the position of this truncation or beyond, false will be returned.` |
|      - |  347 | ` *   If length is given and is 0, FALSE or NULL an empty string will be returned.` |
|      - |  348 | ` *   If length is omitted, the substring starting from start until the end of the string` |
|      - |  349 | ` *   will be returned.` |
|      - |  350 | ` * Return` |
|      - |  351 | ` *  Returns the extracted part of string, or FALSE on failure or an empty string.` |
|      - |  352 | ` */` |
| 213810 |  353 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  354 | `{` |
|      - |  355 | `	const char *zSource,*zOfft;` |
|      - |  356 | `	int nOfft,nLen,nSrcLen;` |
| 213815 |  357 | `	if( nArg < 2 ){` |
|      - |  358 | `		/* return FALSE */` |
|    ! 0 |  359 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  360 | `		return PH7_OK;` |
|      - |  361 | `	}` |
|      - |  362 | `	/* Extract the target string */` |
| 213815 |  363 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 213815 |  364 | `	if( nSrcLen < 1 ){` |
|      - |  365 | `		/* Empty string,return FALSE */` |
|  11695 |  366 | `		ph7_result_bool(pCtx,0);` |
|  11695 |  367 | `		return PH7_OK;` |
|      - |  368 | `	}` |
| 202125 |  369 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  370 | `	/* Extract the offset */` |
| 202125 |  371 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 202125 |  372 | `	if( nOfft < 0 ){` |
|  32001 |  373 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  32001 |  374 | `		if( zOfft < zSource ){` |
|      - |  375 | `			/* Invalid offset */` |
|      5 |  376 | `			ph7_result_bool(pCtx,0);` |
|      5 |  377 | `			return PH7_OK;` |
|      - |  378 | `		}` |
|  31997 |  379 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  31997 |  380 | `		nOfft = (int)(zOfft-zSource);` |
| 186125 |  381 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  382 | `		/* Invalid offset */` |
|    215 |  383 | `		ph7_result_bool(pCtx,0);` |
|    215 |  384 | `		return PH7_OK;` |
|    ! 0 |  385 | `	}else{` |
| 169919 |  386 | `		zOfft = &zSource[nOfft];` |
| 169919 |  387 | `		nLen = nSrcLen - nOfft;` |
|      - |  388 | `	}` |
| 201911 |  389 | `	if( nArg > 2 ){` |
|      - |  390 | `		/* Extract the length */` |
| 165855 |  391 | `		nLen = ph7_value_to_int(apArg[2]);` |
| 165855 |  392 | `		if( nLen == 0 ){` |
|      - |  393 | `			/* Invalid length,return an empty string */` |
|      5 |  394 | `			ph7_result_string(pCtx,"",0);` |
|      5 |  395 | `			return PH7_OK;` |
| 165851 |  396 | `		}else if( nLen < 0 ){` |
|  31989 |  397 | `			nLen = nSrcLen + nLen - nOfft;` |
|  31989 |  398 | `			if( nLen < 1 ){` |
|      - |  399 | `				/* Invalid  length */` |
|      3 |  400 | `				nLen = nSrcLen - nOfft;` |
|      1 |  401 | `			}` |
|  15992 |  402 | `		}` |
| 165851 |  403 | `		if( nLen + nOfft > nSrcLen ){` |
|      - |  404 | `			/* Invalid length */` |
|   5147 |  405 | `			nLen = nSrcLen - nOfft;` |
|   2571 |  406 | `		}` |
|  82923 |  407 | `	}` |
|      - |  408 | `	/* Return the substring */` |
| 201907 |  409 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 201907 |  410 | `	return PH7_OK;` |
| 106910 |  411 | `}` |
|      - |  412 | `/*` |
|      - |  413 | ` * int substr_compare(string $main_str,string $str ,int $offset[,int $length[,bool $case_insensitivity = false ]])` |
|      - |  414 | ` *  Binary safe comparison of two strings from an offset, up to length characters.` |
|      - |  415 | ` * Parameters` |
|      - |  416 | ` *  $main_str` |
|      - |  417 | ` *  The main string being compared.` |
|      - |  418 | ` *  $str` |
|      - |  419 | ` *   The secondary string being compared.` |
|      - |  420 | ` * $offset` |
|      - |  421 | ` *  The start position for the comparison. If negative, it starts counting from` |
|      - |  422 | ` *  the end of the string.` |
|      - |  423 | ` * $length` |
|      - |  424 | ` *  The length of the comparison. The default value is the largest of the length` |
|      - |  425 | ` *  of the str compared to the length of main_str less the offset.` |
|      - |  426 | ` * $case_insensitivity` |
|      - |  427 | ` *  If case_insensitivity is TRUE, comparison is case insensitive.` |
|      - |  428 | ` * Return` |
|      - |  429 | ` *  Returns < 0 if main_str from position offset is less than str, > 0 if it is greater than` |
|      - |  430 | ` *  str, and 0 if they are equal. If offset is equal to or greater than the length of main_str` |
|      - |  431 | ` *  or length is set and is less than 1, substr_compare() prints a warning and returns FALSE.` |
|      - |  432 | ` */` |
|     22 |  433 | `static int PH7_builtin_substr_compare(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  434 | `{` |
|      - |  435 | `	const char *zSource,*zOfft,*zSub;` |
|      - |  436 | `	int nOfft,nLen,nSrcLen,nSublen;` |
|     23 |  437 | `	int iCase = 0;` |
|      - |  438 | `	int rc;` |
|     23 |  439 | `	if( nArg < 3 ){` |
|      - |  440 | `		/* Missing arguments,return FALSE */` |
|    ! 0 |  441 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  442 | `		return PH7_OK;` |
|      - |  443 | `	}` |
|      - |  444 | `	/* Extract the target string */` |
|     23 |  445 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
|     23 |  446 | `	if( nSrcLen < 1 ){` |
|      - |  447 | `		/* Empty string,return FALSE */` |
|      3 |  448 | `		ph7_result_bool(pCtx,0);` |
|      3 |  449 | `		return PH7_OK;` |
|      - |  450 | `	}` |
|     21 |  451 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  452 | `	/* Extract the substring */` |
|     21 |  453 | `	zSub = ph7_value_to_string(apArg[1],&nSublen);` |
|     21 |  454 | `	if( nSublen < 1 \|\| nSublen > nSrcLen){` |
|      - |  455 | `		/* Empty string,return FALSE */` |
|      3 |  456 | `		ph7_result_bool(pCtx,0);` |
|      3 |  457 | `		return PH7_OK;` |
|      - |  458 | `	}` |
|      - |  459 | `	/* Extract the offset */` |
|     19 |  460 | `	nOfft = ph7_value_to_int(apArg[2]);` |
|     19 |  461 | `	if( nOfft < 0 ){` |
|      5 |  462 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|      5 |  463 | `		if( zOfft < zSource ){` |
|      - |  464 | `			/* Invalid offset */` |
|      3 |  465 | `			ph7_result_bool(pCtx,0);` |
|      3 |  466 | `			return PH7_OK;` |
|      - |  467 | `		}` |
|      3 |  468 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|      3 |  469 | `		nOfft = (int)(zOfft-zSource);` |
|     16 |  470 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  471 | `		/* Invalid offset */` |
|      3 |  472 | `		ph7_result_bool(pCtx,0);` |
|      3 |  473 | `		return PH7_OK;` |
|    ! 0 |  474 | `	}else{` |
|     13 |  475 | `		zOfft = &zSource[nOfft];` |
|     13 |  476 | `		nLen = nSrcLen - nOfft;` |
|      - |  477 | `	}` |
|     15 |  478 | `	if( nArg > 3 ){` |
|      - |  479 | `		/* Extract the length */` |
|     13 |  480 | `		nLen = ph7_value_to_int(apArg[3]);` |
|     13 |  481 | `		if( nLen < 1 ){` |
|      - |  482 | `			/* Invalid  length */` |
|      5 |  483 | `			ph7_result_int(pCtx,1);` |
|      5 |  484 | `			return PH7_OK;` |
|      9 |  485 | `		}else if( nLen + nOfft > nSrcLen ){` |
|      - |  486 | `			/* Invalid length */` |
|      3 |  487 | `			nLen = nSrcLen - nOfft;` |
|      1 |  488 | `		}` |
|      9 |  489 | `		if( nArg > 4 ){` |
|      - |  490 | `			/* Case-sensitive or not */` |
|      5 |  491 | `			iCase = ph7_value_to_bool(apArg[4]);` |
|      2 |  492 | `		}` |
|      4 |  493 | `	}` |
|      - |  494 | `	/* Perform the comparison */` |
|     11 |  495 | `	if( iCase ){` |
|      3 |  496 | `		rc = SyStrnicmp(zOfft,zSub,(sxu32)nLen);` |
|      2 |  497 | `	}else{` |
|      9 |  498 | `		rc = SyStrncmp(zOfft,zSub,(sxu32)nLen);` |
|      - |  499 | `	}` |
|      - |  500 | `	/* Comparison result */` |
|     11 |  501 | `	ph7_result_int(pCtx,rc);` |
|     11 |  502 | `	return PH7_OK;` |
|     12 |  503 | `}` |
|      - |  504 | `/*` |
|      - |  505 | ` * int substr_count(string $haystack,string $needle[,int $offset = 0 [,int $length ]])` |
|      - |  506 | ` *  Count the number of substring occurrences.` |
|      - |  507 | ` * Parameters` |
|      - |  508 | ` * $haystack` |
|      - |  509 | ` *   The string to search in` |
|      - |  510 | ` * $needle` |
|      - |  511 | ` *   The substring to search for` |
|      - |  512 | ` * $offset` |
|      - |  513 | ` *  The offset where to start counting` |
|      - |  514 | ` * $length (NOT USED)` |
|      - |  515 | ` *  The maximum length after the specified offset to search for the substring.` |
|      - |  516 | ` *  It outputs a warning if the offset plus the length is greater than the haystack length.` |
|      - |  517 | ` * Return` |
|      - |  518 | ` *  Toral number of substring occurrences.` |
|      - |  519 | ` */` |
|     26 |  520 | `static int PH7_builtin_substr_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  521 | `{` |
|      - |  522 | `	const char *zText,*zPattern,*zEnd;` |
|      - |  523 | `	int nTextlen,nPatlen;` |
|     27 |  524 | `	int iCount = 0;` |
|      - |  525 | `	sxu32 nOfft;` |
|      - |  526 | `	sxi32 rc;` |
|     27 |  527 | `	if( nArg < 2 ){` |
|      - |  528 | `		/* Missing arguments */` |
|    ! 0 |  529 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  530 | `		return PH7_OK;` |
|      - |  531 | `	}` |
|      - |  532 | `	/* Point to the haystack */` |
|     27 |  533 | `	zText = ph7_value_to_string(apArg[0],&nTextlen);` |
|      - |  534 | `	/* Point to the neddle */` |
|     27 |  535 | `	zPattern = ph7_value_to_string(apArg[1],&nPatlen);` |
|     27 |  536 | `	if( nPatlen < 1 ){` |
|      - |  537 | `		/* Empty needle: PHP 8 throws a catchable ValueError. */` |
|      3 |  538 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  539 | `			"substr_count(): Argument #2 ($needle) must not be empty");` |
|      - |  540 | `	}` |
|      - |  541 | `	/* Apply the optional $offset/$length window before searching. PHP 8 validates` |
|      - |  542 | `	 * both against the haystack (a negative value counts from the end) and throws a` |
|      - |  543 | `	 * catchable ValueError when the result falls outside it — this happens before the` |
|      - |  544 | `	 * needle-fits check, so it fires even when the needle is longer than the haystack. */` |
|     25 |  545 | `	if( nArg > 2 ){` |
|     19 |  546 | `		ph7_int64 iOfft = ph7_value_to_int64(apArg[2]);` |
|     19 |  547 | `		if( iOfft < 0 ){` |
|      5 |  548 | `			iOfft += nTextlen;` |
|      2 |  549 | `		}` |
|     19 |  550 | `		if( iOfft < 0 \|\| iOfft > nTextlen ){` |
|      3 |  551 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  552 | `				"substr_count(): Argument #3 ($offset) must be contained in argument #1 ($haystack)");` |
|      - |  553 | `		}` |
|      - |  554 | `		/* Point to the desired offset and shrink the remaining region */` |
|     17 |  555 | `		zText = &zText[iOfft];` |
|     17 |  556 | `		nTextlen -= (int)iOfft;` |
|      8 |  557 | `	}` |
|     23 |  558 | `	if( nArg > 3 ){` |
|     15 |  559 | `		ph7_int64 nLen = ph7_value_to_int64(apArg[3]);` |
|     15 |  560 | `		if( nLen < 0 ){` |
|      - |  561 | `			/* Negative length is relative to the end of the (offset) haystack */` |
|      5 |  562 | `			nLen += nTextlen;` |
|      2 |  563 | `		}` |
|     15 |  564 | `		if( nLen < 0 \|\| nLen > nTextlen ){` |
|      5 |  565 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  566 | `				"substr_count(): Argument #4 ($length) must be contained in argument #1 ($haystack)");` |
|      - |  567 | `		}` |
|     11 |  568 | `		nTextlen = (int)nLen;` |
|      5 |  569 | `	}` |
|     19 |  570 | `	if( nTextlen < 1 \|\| nPatlen > nTextlen ){` |
|      - |  571 | `		/* The windowed haystack can't contain the needle: zero matches */` |
|      3 |  572 | `		ph7_result_int(pCtx,0);` |
|      3 |  573 | `		return PH7_OK;` |
|      - |  574 | `	}` |
|      - |  575 | `	/* Point to the end of the windowed haystack */` |
|     17 |  576 | `	zEnd = &zText[nTextlen];` |
|      - |  577 | `	/* Perform the search */` |
|     17 |  578 | `	for(;;){` |
|     35 |  579 | `		rc = SyBlobSearch((const void *)zText,(sxu32)(zEnd-zText),(const void *)zPattern,nPatlen,&nOfft);` |
|     35 |  580 | `		if( rc != SXRET_OK ){` |
|      - |  581 | `			/* Pattern not found,break immediately */` |
|     13 |  582 | `			break;` |
|      - |  583 | `		}` |
|      - |  584 | `		/* Increment counter and update the offset */` |
|     23 |  585 | `		iCount++;` |
|     23 |  586 | `		zText += nOfft + nPatlen;` |
|     23 |  587 | `		if( zText >= zEnd ){` |
|      5 |  588 | `			break;` |
|      - |  589 | `		}` |
|      1 |  590 | `	}` |
|      - |  591 | `	/* Pattern count */` |
|     17 |  592 | `	ph7_result_int(pCtx,iCount);` |
|     17 |  593 | `	return PH7_OK;` |
|     14 |  594 | `}` |
|      - |  595 | `/*` |
|      - |  596 | ` * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])` |
|      - |  597 | ` *   Split a string into smaller chunks.` |
|      - |  598 | ` * Parameters` |
|      - |  599 | ` *  $body` |
|      - |  600 | ` *   The string to be chunked.` |
|      - |  601 | ` * $chunklen` |
|      - |  602 | ` *   The chunk length.` |
|      - |  603 | ` * $end` |
|      - |  604 | ` *   The line ending sequence.` |
|      - |  605 | ` * Return` |
|      - |  606 | ` *  The chunked string or NULL on failure.` |
|      - |  607 | ` */` |
|     14 |  608 | `static int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  609 | `{` |
|     15 |  610 | `	const char *zIn,*zEnd,*zSep = "\r\n";` |
|      - |  611 | `	int nSepLen,nChunkLen,nLen;` |
|     15 |  612 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - |  613 | `		/* Nothing to split,return null */` |
|      3 |  614 | `		ph7_result_null(pCtx);` |
|      3 |  615 | `		return PH7_OK;` |
|      - |  616 | `	}` |
|      - |  617 | `	/* initialize/Extract arguments */` |
|     13 |  618 | `	nSepLen = (int)sizeof("\r\n") - 1;` |
|     13 |  619 | `	nChunkLen = 76;` |
|     13 |  620 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 |  621 | `	zEnd = &zIn[nLen];` |
|     13 |  622 | `	if( nArg > 1 ){` |
|      - |  623 | `		/* Chunk length */` |
|     13 |  624 | `		nChunkLen = ph7_value_to_int(apArg[1]);` |
|     13 |  625 | `		if( nChunkLen < 1 ){` |
|      - |  626 | `			/* PHP 8 throws a catchable ValueError for a non-positive length. */` |
|      3 |  627 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  628 | `				"chunk_split(): Argument #2 ($length) must be greater than 0");` |
|      - |  629 | `		}` |
|     11 |  630 | `		if( nArg > 2 ){` |
|      - |  631 | `			/* Separator */` |
|      9 |  632 | `			zSep = ph7_value_to_string(apArg[2],&nSepLen);` |
|      9 |  633 | `			if( nSepLen < 1 ){` |
|      - |  634 | `				/* Switch back to the default separator */` |
|      3 |  635 | `				zSep = "\r\n";` |
|      3 |  636 | `				nSepLen = (int)sizeof("\r\n") - 1;` |
|      1 |  637 | `			}` |
|      4 |  638 | `		}` |
|      5 |  639 | `	}` |
|      - |  640 | `	/* Perform the requested operation */` |
|     11 |  641 | `	if( nChunkLen > nLen ){` |
|      - |  642 | `		/* Nothing to split,return the string and the separator */` |
|      7 |  643 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);` |
|      7 |  644 | `		return PH7_OK;` |
|      - |  645 | `	}` |
|     17 |  646 | `	while( zIn < zEnd ){` |
|     13 |  647 | `		if( nChunkLen > (int)(zEnd-zIn) ){` |
|      3 |  648 | `			nChunkLen = (int)(zEnd - zIn);` |
|      1 |  649 | `		}` |
|      - |  650 | `		/* Append the chunk and the separator */` |
|     13 |  651 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);` |
|      - |  652 | `		/* Point beyond the chunk */` |
|     13 |  653 | `		zIn += nChunkLen;` |
|      1 |  654 | `	}` |
|      5 |  655 | `	return PH7_OK;` |
|      8 |  656 | `}` |
|      - |  657 | `/*` |
|      - |  658 | ` * string addslashes(string $str)` |
|      - |  659 | ` *  Quote string with slashes.` |
|      - |  660 | ` *  Returns a string with backslashes before characters that need` |
|      - |  661 | ` *  to be quoted in database queries etc. These characters are single` |
|      - |  662 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - |  663 | ` * Parameter` |
|      - |  664 | ` *  str: The string to be escaped.` |
|      - |  665 | ` * Return` |
|      - |  666 | ` *  Returns the escaped string` |
|      - |  667 | ` */` |
|     24 |  668 | `static int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 |  669 | `{` |
|      - |  670 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  671 | `	int nLen;` |
|      - |  672 | `	/* PHP enforces exactly one argument. */` |
|     28 |  673 | `	if( nArg != 1 ){` |
|      8 |  674 | `		return PH7_VmThrowException(pCtx,` |
|      - |  675 | `			"ArgumentCountError",` |
|      - |  676 | `			"addslashes() expects exactly 1 argument, %d given",` |
|      2 |  677 | `			nArg` |
|      - |  678 | `			);` |
|      - |  679 | `	}` |
|      - |  680 | `	/* NULL is deprecated and treated as an empty string; other invalid` |
|      - |  681 | `	 * types still produce a TypeError. */` |
|     22 |  682 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 |  683 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - |  684 | `			E_DEPRECATED,` |
|      - |  685 | `			"addslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - |  686 | `			);` |
|      - |  687 | `		/* fall through so conversion below yields empty string */` |
|      1 |  688 | `	}` |
|      - |  689 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     29 |  690 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     29 |  691 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     18 |  692 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 |  693 | `		return PH7_VmThrowException(pCtx,` |
|      - |  694 | `			"TypeError",` |
|      - |  695 | `			"addslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 |  696 | `			ph7_type_name(apArg[0])` |
|      - |  697 | `			);` |
|      - |  698 | `	}` |
|      - |  699 | `	/* Convert to string representation first and obtain length. */` |
|     19 |  700 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     19 |  701 | `	if( nLen < 1 ){` |
|      - |  702 | `		/* Return the empty string */` |
|      5 |  703 | `		ph7_result_string(pCtx,"",0);` |
|      5 |  704 | `		return PH7_OK;` |
|      - |  705 | `	}` |
|     15 |  706 | `	zEnd = &zIn[nLen];` |
|     15 |  707 | `	zCur = 0; /* cc warning */` |
|     20 |  708 | `	for(;;){` |
|     41 |  709 | `		if( zIn >= zEnd ){` |
|      - |  710 | `			/* No more input */` |
|     15 |  711 | `			break;` |
|      - |  712 | `		}` |
|     27 |  713 | `		zCur = zIn;` |
|      - |  714 | `		/* scan until a character that needs escaping (', ", \\, or NUL) */` |
|     89 |  715 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' && zIn[0] != '\0' ){` |
|     63 |  716 | `			zIn++;` |
|      1 |  717 | `		}` |
|     27 |  718 | `		if( zIn > zCur ){` |
|      - |  719 | `			/* Append raw contents */` |
|     23 |  720 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 |  721 | `		}` |
|     27 |  722 | `		if( zIn < zEnd ){` |
|     17 |  723 | `			int c = zIn[0];` |
|     17 |  724 | `			if( c == '\0' ){` |
|      - |  725 | `				/* PHP escapes NUL as "\\0" (two characters) */` |
|      3 |  726 | `				ph7_result_string(pCtx,"\\0",2);` |
|      2 |  727 | `			}else{` |
|     15 |  728 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - |  729 | `			}` |
|      8 |  730 | `		}` |
|     27 |  731 | `		zIn++;` |
|      1 |  732 | `	}` |
|     15 |  733 | `	return PH7_OK;` |
|     16 |  734 | `}` |
|      - |  735 | `/*` |
|      - |  736 | ``  * Build a 256-entry membership mask from a PHP charlist, expanding `a..z` `` |
|      - |  737 | ` * byte ranges exactly like PHP's php_charmask(). On return aMask[c] != 0 iff` |
|      - |  738 | ` * the byte c belongs to the set. Emits the PHP-exact warnings for the three` |
|      - |  739 | ` * malformed-range shapes (ph7_context_throw_error_format prepends the active` |
|      - |  740 | ` * function name, so the messages omit it); on a bad range the surrounding` |
|      - |  741 | ` * bytes are still added and the scan never aborts. Reads only within` |
|      - |  742 | ` * [zList, zList+nLen).` |
|      - |  743 | ` *` |
|      - |  744 | ` * Use ONLY for the builtins whose charlist expands ranges the way PHP's` |
|      - |  745 | ` * php_charmask() does: trim/ltrim/rtrim/addcslashes (and quotemeta, whose set` |
|      - |  746 | ` * is a fixed literal with no ".."). Do NOT route strspn/strcspn/strtok/strpbrk` |
|      - |  747 | ` * through this — PHP treats their charlists literally, so expanding "a..z" here` |
|      - |  748 | ` * would be a behavior regression plus spurious "Invalid '..'-range" warnings.` |
|      - |  749 | ` */` |
|     78 |  750 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256])` |
|      3 |  751 | `{` |
|     81 |  752 | `	const unsigned char *zIn  = (const unsigned char *)zList;` |
|     81 |  753 | `	const unsigned char *zEnd = zIn + (nLen > 0 ? nLen : 0);` |
|     81 |  754 | `	SyZero(aMask,256);` |
|    291 |  755 | `	for( ; zIn < zEnd ; zIn++ ){` |
|    213 |  756 | `		int c = zIn[0];` |
|    213 |  757 | `		if( zIn + 3 < zEnd && zIn[1] == '.' && zIn[2] == '.' && zIn[3] >= c ){` |
|      - |  758 | `			/* Valid incrementing range c..zIn[3] */` |
|     20 |  759 | `			int hi = zIn[3],k;` |
|    364 |  760 | `			for( k = c ; k <= hi ; k++ ){` |
|    346 |  761 | `				aMask[k] = 1;` |
|    174 |  762 | `			}` |
|     20 |  763 | `			zIn += 3; /* the loop's ++ then steps past the range end */` |
|    213 |  764 | `		}else if( zIn + 1 < zEnd && zIn[0] == '.' && zIn[1] == '.' ){` |
|      - |  765 | `			/* Malformed range: mirror php_charmask's three diagnostics. */` |
|      - |  766 | `			const char *zMsg;` |
|     20 |  767 | `			if( (const unsigned char *)zList >= zIn ){` |
|      6 |  768 | `				zMsg = "no character to the left of '..'";` |
|     18 |  769 | `			}else if( zIn + 2 >= zEnd ){` |
|      6 |  770 | `				zMsg = "no character to the right of '..'";` |
|     14 |  771 | `			}else if( zIn[-1] > zIn[2] ){` |
|     12 |  772 | `				zMsg = "'..'-range needs to be incrementing";` |
|      7 |  773 | `			}else{` |
|    ! 0 |  774 | `				zMsg = 0; /* catch-all (e.g. a..b..c) */` |
|      - |  775 | `			}` |
|     20 |  776 | `			if( zMsg ){` |
|     29 |  777 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      9 |  778 | `					"Invalid '..'-range, %s",zMsg);` |
|     11 |  779 | `			}else{` |
|    ! 0 |  780 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - |  781 | `					"Invalid '..'-range");` |
|      - |  782 | `			}` |
|      - |  783 | `			/* Do not consume the dots: the loop's ++ steps one byte so the` |
|      - |  784 | `			 * dots are re-scanned as literals, exactly like php_charmask. */` |
|     11 |  785 | `		}else{` |
|    177 |  786 | `			aMask[c] = 1;` |
|      - |  787 | `		}` |
|    108 |  788 | `	}` |
|     81 |  789 | `}` |
|      - |  790 | `/*` |
|      - |  791 | ` * string addcslashes(string $str,string $charlist)` |
|      - |  792 | ` *  Quote string with slashes in a C style.` |
|      - |  793 | ` * Parameter` |
|      - |  794 | ` *  $str:` |
|      - |  795 | ` *    The string to be escaped.` |
|      - |  796 | ` *  $charlist:` |
|      - |  797 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|      - |  798 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|      - |  799 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|      - |  800 | ` * Return` |
|      - |  801 | ` *  Returns the escaped string.` |
|      - |  802 | ` * Note:` |
|      - |  803 | ` *  Character ranges [i.e: 'A..Z'] are supported (see PH7_BuildCharMask).` |
|      - |  804 | ` */` |
|     40 |  805 | `static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  806 | `{` |
|      - |  807 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - |  808 | `	char aMask[256];` |
|      - |  809 | `	int nLen,nMask;` |
|      - |  810 | `	/* PHP enforces exactly two arguments. */` |
|     45 |  811 | `	if( nArg != 2 ){` |
|      8 |  812 | `		return PH7_VmThrowException(pCtx,` |
|      - |  813 | `			"ArgumentCountError",` |
|      - |  814 | `			"addcslashes() expects exactly 2 arguments, %d given",` |
|      2 |  815 | `			nArg` |
|      - |  816 | `			);` |
|      - |  817 | `	}` |
|      - |  818 | `	/* First argument must be a string-ish value.  NULL is deprecated and` |
|      - |  819 | `	 * treated as the empty string (PHP 8.1). */` |
|     40 |  820 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      - |  821 | `		/* Emit deprecation only once, similar to PHP behaviour. */` |
|      3 |  822 | `		PH7_VmThrowError(pCtx->pVm,0,/* iErr will be patched to 8192 below */` |
|      - |  823 | `			E_DEPRECATED,` |
|      - |  824 | `			"addcslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - |  825 | `			);` |
|      - |  826 | `		/* treat as empty string; fall through to conversion logic */` |
|     52 |  827 | `	} else if( ph7_value_is_array(apArg[0]) \|\|` |
|     52 |  828 | `	          ph7_value_is_object(apArg[0]) \|\|` |
|     32 |  829 | `	          ph7_value_is_resource(apArg[0]) ){` |
|      4 |  830 | `		return PH7_VmThrowException(pCtx,` |
|      - |  831 | `			"TypeError",` |
|      - |  832 | `			"addcslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 |  833 | `			ph7_type_name(apArg[0])` |
|      - |  834 | `			);` |
|      - |  835 | `	}` |
|      - |  836 | `	/* Second argument must be a string.  NULL is deprecated and treated as` |
|      - |  837 | `	 * an empty mask per PHP semantics.  Arrays/objects/resources still` |
|      - |  838 | `	 * trigger a TypeError. */` |
|     37 |  839 | `	if( ph7_value_is_null(apArg[1]) ){` |
|      3 |  840 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - |  841 | `			E_DEPRECATED,` |
|      - |  842 | `			"addcslashes(): Passing null to parameter #2 ($characters) of type string is deprecated"` |
|      - |  843 | `			);` |
|      - |  844 | `		/* allow through so it becomes empty string below */` |
|     49 |  845 | `	} else if( ph7_value_is_array(apArg[1]) \|\|` |
|     48 |  846 | `	          ph7_value_is_object(apArg[1]) \|\|` |
|     30 |  847 | `	          ph7_value_is_resource(apArg[1]) ){` |
|      4 |  848 | `		return PH7_VmThrowException(pCtx,` |
|      - |  849 | `			"TypeError",` |
|      - |  850 | `			"addcslashes(): Argument #2 ($characters) must be of type string, %s given",` |
|      2 |  851 | `			ph7_type_name(apArg[1])` |
|      - |  852 | `			);` |
|      - |  853 | `	}` |
|      - |  854 | `	/* Extract the string to process */` |
|     35 |  855 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      - |  856 | `	/* NULL would never reach here due to the check above. */` |
|     35 |  857 | `	if( nLen < 1 ){` |
|      - |  858 | `		/* Empty string returns itself. */` |
|      5 |  859 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      5 |  860 | `		return PH7_OK;` |
|      - |  861 | `	}` |
|      - |  862 | ``	/* Extract the desired mask and expand any `a..z` ranges into a lookup. */`` |
|     31 |  863 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|     31 |  864 | `	PH7_BuildCharMask(pCtx,zMask,nMask,aMask);` |
|     31 |  865 | `	zEnd = &zIn[nLen];` |
|     31 |  866 | `	zCur = 0; /* cc warning */` |
|     37 |  867 | `	for(;;){` |
|     77 |  868 | `		if( zIn >= zEnd ){` |
|      - |  869 | `			/* No more input */` |
|     31 |  870 | `			break;` |
|      - |  871 | `		}` |
|     49 |  872 | `		zCur = zIn;` |
|    125 |  873 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     79 |  874 | `			zIn++;` |
|      3 |  875 | `		}` |
|     49 |  876 | `		if( zIn > zCur ){` |
|      - |  877 | `			/* Append raw contents */` |
|     43 |  878 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     20 |  879 | `		}` |
|     49 |  880 | `		if( zIn < zEnd ){` |
|      - |  881 | `			/* Make sure we treat the byte as unsigned to avoid negative values` |
|      - |  882 | `			 * on platforms where char is signed. */` |
|     29 |  883 | `			int c = (unsigned char)zIn[0];` |
|      - |  884 | `			/* Handle special C-like escapes for common control characters first.` |
|      - |  885 | `			 * PHP outputs "\n" "\r" "\t" "\v" "\f" when those chars are` |
|      - |  886 | `			 * in the mask. NUL is left to the octal conversion below. */` |
|     29 |  887 | `			if( c == '\n' ){` |
|      3 |  888 | `				ph7_result_string(pCtx,"\\n",2);` |
|     28 |  889 | `			}else if( c == '\r' ){` |
|      3 |  890 | `				ph7_result_string(pCtx,"\\r",2);` |
|     26 |  891 | `			}else if( c == '\t' ){` |
|      3 |  892 | `				ph7_result_string(pCtx,"\\t",2);` |
|     24 |  893 | `			}else if( c == '\v' ){` |
|      3 |  894 | `				ph7_result_string(pCtx,"\\v",2);` |
|     22 |  895 | `			}else if( c == '\f' ){` |
|      3 |  896 | `				ph7_result_string(pCtx,"\\f",2);` |
|     20 |  897 | `			}else if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - |  898 | `				/* Convert to octal.  PHP always emits three-digit zero-padded` |
|      - |  899 | `				 * octal escapes (\001 not \1). */` |
|      7 |  900 | `				ph7_result_string_format(pCtx,"\\%03o",c);` |
|      4 |  901 | `			}else{` |
|     13 |  902 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - |  903 | `			}` |
|     13 |  904 | `		}` |
|     49 |  905 | `		zIn++;` |
|      3 |  906 | `	}` |
|     31 |  907 | `	return PH7_OK;` |
|     25 |  908 | `}` |
|      - |  909 | `/*` |
|      - |  910 | ` * string quotemeta(string $str)` |
|      - |  911 | ` *  Quote meta characters.` |
|      - |  912 | ` * Parameter` |
|      - |  913 | ` *  $str:` |
|      - |  914 | ` *    The string to be escaped.` |
|      - |  915 | ` * Return` |
|      - |  916 | ` *  Returns the escaped string.` |
|      - |  917 | `*/` |
|     10 |  918 | `static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 |  919 | `{` |
|      - |  920 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  921 | `	char aMask[256];` |
|      - |  922 | `	int nLen;` |
|     12 |  923 | `	if( nArg < 1 ){` |
|      - |  924 | `		/* Nothing to process,retun NULL */` |
|    ! 0 |  925 | `		ph7_result_null(pCtx);` |
|    ! 0 |  926 | `		return PH7_OK;` |
|      - |  927 | `	}` |
|      - |  928 | `	/* Extract the string to process */` |
|     12 |  929 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     12 |  930 | `	if( nLen < 1 ){` |
|      - |  931 | `		/* Return the empty string */` |
|      3 |  932 | `		ph7_result_string(pCtx,"",0);` |
|      3 |  933 | `		return PH7_OK;` |
|      - |  934 | `	}` |
|      - |  935 | `	/* Fixed meta-character set (no ranges); build the lookup once. */` |
|     10 |  936 | `	PH7_BuildCharMask(pCtx,".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1,aMask);` |
|     10 |  937 | `	zEnd = &zIn[nLen];` |
|     10 |  938 | `	zCur = 0; /* cc warning */` |
|     22 |  939 | `	for(;;){` |
|     46 |  940 | `		if( zIn >= zEnd ){` |
|      - |  941 | `			/* No more input */` |
|     10 |  942 | `			break;` |
|      - |  943 | `		}` |
|     38 |  944 | `		zCur = zIn;` |
|     76 |  945 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     40 |  946 | `			zIn++;` |
|      2 |  947 | `		}` |
|     38 |  948 | `		if( zIn > zCur ){` |
|      - |  949 | `			/* Append raw contents */` |
|     20 |  950 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      9 |  951 | `		}` |
|     38 |  952 | `		if( zIn < zEnd ){` |
|     36 |  953 | `			int c = zIn[0];` |
|     36 |  954 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     17 |  955 | `		}` |
|     38 |  956 | `		zIn++;` |
|      2 |  957 | `	}` |
|     10 |  958 | `	return PH7_OK;` |
|      7 |  959 | `}` |
|      - |  960 | `/*` |
|      - |  961 | ` * string stripslashes(string $str)` |
|      - |  962 | ` *  Un-quotes a quoted string.` |
|      - |  963 | ` *  Returns a string with backslashes before characters that need` |
|      - |  964 | ` *  to be quoted in database queries etc. These characters are single` |
|      - |  965 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - |  966 | ` * Parameter` |
|      - |  967 | ` *  $str` |
|      - |  968 | ` *   The input string.` |
|      - |  969 | ` * Return` |
|      - |  970 | ` *  Returns a string with backslashes stripped off.` |
|      - |  971 | ` */` |
|      6 |  972 | `static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  973 | `{` |
|      - |  974 | `	const char *zCur,*zIn,*zEnd;` |
|      - |  975 | `	int nLen;` |
|      7 |  976 | `	if( nArg < 1 ){` |
|      - |  977 | `		/* Nothing to process,retun NULL */` |
|    ! 0 |  978 | `		ph7_result_null(pCtx);` |
|    ! 0 |  979 | `		return PH7_OK;` |
|      - |  980 | `	}` |
|      - |  981 | `	/* Extract the string to process */` |
|      7 |  982 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 |  983 | `	if( zIn == 0 ){` |
|    ! 0 |  984 | `		ph7_result_null(pCtx);` |
|    ! 0 |  985 | `		return PH7_OK;` |
|      - |  986 | `	}` |
|      7 |  987 | `	zEnd = &zIn[nLen];` |
|      7 |  988 | `	zCur = 0; /* cc warning */` |
|      - |  989 | `	/* Encode the string */` |
|      4 |  990 | `	for(;;){` |
|      9 |  991 | `		if( zIn >= zEnd ){` |
|      - |  992 | `			/* No more input */` |
|      5 |  993 | `			break;` |
|      - |  994 | `		}` |
|      5 |  995 | `		zCur = zIn;` |
|     17 |  996 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 |  997 | `			zIn++;` |
|      1 |  998 | `		}` |
|      5 |  999 | `		if( zIn > zCur ){` |
|      - | 1000 | `			/* Append raw contents */` |
|      5 | 1001 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 1002 | `		}` |
|      5 | 1003 | `		if( &zIn[1] < zEnd ){` |
|      3 | 1004 | `			int c = zIn[1];` |
|      3 | 1005 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - | 1006 | `				/* Ignore the backslash */` |
|      3 | 1007 | `				zIn++;` |
|      1 | 1008 | `			}` |
|      2 | 1009 | `		}else{` |
|      3 | 1010 | `			break;` |
|      - | 1011 | `		}` |
|      1 | 1012 | `	}` |
|      7 | 1013 | `	return PH7_OK;` |
|      4 | 1014 | `}` |
|      - | 1015 | `/*` |
|      - | 1016 | ` * UTF-8-aware HTML entity machinery, shared by htmlspecialchars/htmlentities/` |
|      - | 1017 | ` * htmlspecialchars_decode/html_entity_decode/get_html_translation_table.` |
|      - | 1018 | ` * The implementations live further down in this file, next to the filter_var` |
|      - | 1019 | ` * FULL_SPECIAL_CHARS machinery they reuse (aHtml401Ent[]/FvHtml401Lookup()/` |
|      - | 1020 | ` * FvUtf8Next()). Semantics are byte-exact vs php 8.5.7; PHL is UTF-8-only` |
|      - | 1021 | ` * (PLAN.md §6) so every charset argument other than a UTF-8 alias gets PHP's` |
|      - | 1022 | ` * unsupported-charset warning and is treated as UTF-8.` |
|      - | 1023 | ` *` |
|      - | 1024 | ` * Flag model (the PHP-exact ENT_* values, see constant.c): bit 1 = encode/` |
|      - | 1025 | ` * decode single quotes, bit 2 = double quotes (ENT_QUOTES=3, ENT_COMPAT=2,` |
|      - | 1026 | ` * ENT_NOQUOTES=0); bits 16\|32 select the doctype (0=HTML401, 16=XML1,` |
|      - | 1027 | ` * 32=XHTML, 48=HTML5); ENT_IGNORE=4 drops invalid UTF-8 bytes (wins over` |
|      - | 1028 | ` * ENT_SUBSTITUTE=8, which replaces each with U+FFFD; with neither set the` |
|      - | 1029 | ` * whole result collapses to ""); ENT_DISALLOWED=128 substitutes valid but` |
|      - | 1030 | ` * doctype-disallowed codepoints. The shared default is` |
|      - | 1031 | ` * ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 = 11.` |
|      - | 1032 | ` */` |
|      - | 1033 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bAll,int bDoubleEncode);` |
|      - | 1034 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bFull);` |
|      - | 1035 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx);` |
|      - | 1036 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags);` |
|      - | 1037 | `/*` |
|      - | 1038 | ` * string htmlspecialchars(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1039 | ` *                         [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1040 | ` *  Convert the special characters & < > " ' to HTML entities.` |
|      - | 1041 | ` * Return` |
|      - | 1042 | ` *  The escaped string or NULL on failure.` |
|      - | 1043 | ` */` |
|     42 | 1044 | `static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1045 | `{` |
|     43 | 1046 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1047 | `	const char *zIn;` |
|     43 | 1048 | `	int nLen,bDouble = 1;` |
|     43 | 1049 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1050 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 1051 | `		ph7_result_null(pCtx);` |
|      3 | 1052 | `		return PH7_OK;` |
|      - | 1053 | `	}` |
|      - | 1054 | `	/* Extract the target string */` |
|     41 | 1055 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 1056 | `	if( nArg > 1 ){` |
|     35 | 1057 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     17 | 1058 | `	}` |
|     41 | 1059 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     41 | 1060 | `	if( nArg > 3 ){` |
|      7 | 1061 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      3 | 1062 | `	}` |
|     41 | 1063 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,0,bDouble);` |
|     41 | 1064 | `	return PH7_OK;` |
|     22 | 1065 | `}` |
|      - | 1066 | `/*` |
|      - | 1067 | ` * string htmlspecialchars_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401])` |
|      - | 1068 | ` *  Convert the special HTML entities (&amp; &lt; &gt; &quot; and the` |
|      - | 1069 | ` *  numeric/doctype forms of the two quotes) back to characters.` |
|      - | 1070 | ` * Return` |
|      - | 1071 | ` *  The unescaped string or NULL on failure.` |
|      - | 1072 | ` */` |
|     22 | 1073 | `static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1074 | `{` |
|     23 | 1075 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1076 | `	const char *zIn;` |
|      - | 1077 | `	int nLen;` |
|     23 | 1078 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1079 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 1080 | `		ph7_result_null(pCtx);` |
|      3 | 1081 | `		return PH7_OK;` |
|      - | 1082 | `	}` |
|      - | 1083 | `	/* Extract the target string */` |
|     21 | 1084 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     21 | 1085 | `	if( nArg > 1 ){` |
|      9 | 1086 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1087 | `	}` |
|     21 | 1088 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,0);` |
|     21 | 1089 | `	return PH7_OK;` |
|     12 | 1090 | `}` |
|      - | 1091 | `/*` |
|      - | 1092 | ` * array get_html_translation_table(int $table = HTML_SPECIALCHARS` |
|      - | 1093 | ` *      [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 [, string $encoding = "UTF-8"]])` |
|      - | 1094 | ` *  Return the translation table used by htmlspecialchars() (HTML_SPECIALCHARS)` |
|      - | 1095 | ` *  or htmlentities() (HTML_ENTITIES) as character => entity pairs.` |
|      - | 1096 | ` * Return` |
|      - | 1097 | ` *  The translation table as an array or NULL on failure.` |
|      - | 1098 | ` */` |
|     12 | 1099 | `static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1100 | `{` |
|     13 | 1101 | `	int iTable = 0; /* HTML_SPECIALCHARS */` |
|     13 | 1102 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|     13 | 1103 | `	if( nArg > 0 ){` |
|     11 | 1104 | `		iTable = ph7_value_to_int(apArg[0]);` |
|      5 | 1105 | `	}` |
|     13 | 1106 | `	if( nArg > 1 ){` |
|      9 | 1107 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1108 | `	}` |
|     13 | 1109 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     13 | 1110 | `	HtmlTranslationTable(pCtx,iTable,iFlags);` |
|     13 | 1111 | `	return PH7_OK;` |
|      1 | 1112 | `}` |
|      - | 1113 | `/*` |
|      - | 1114 | ` * string htmlentities(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1115 | ` *                     [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1116 | ` *  Convert all applicable characters to HTML entities: the specials plus` |
|      - | 1117 | ` *  every codepoint with an HTML 4.01 named entity (aHtml401Ent[]).` |
|      - | 1118 | ` * Return` |
|      - | 1119 | ` *  The encoded string or NULL on failure.` |
|      - | 1120 | ` */` |
|     30 | 1121 | `static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1122 | `{` |
|     31 | 1123 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1124 | `	const char *zIn;` |
|     31 | 1125 | `	int nLen,bDouble = 1;` |
|     31 | 1126 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1127 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1128 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1129 | `		return PH7_OK;` |
|      - | 1130 | `	}` |
|      - | 1131 | `	/* Extract the target string */` |
|     31 | 1132 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 1133 | `	if( nArg > 1 ){` |
|     19 | 1134 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1135 | `	}` |
|     31 | 1136 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     31 | 1137 | `	if( nArg > 3 ){` |
|      3 | 1138 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      1 | 1139 | `	}` |
|     31 | 1140 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,1,bDouble);` |
|     31 | 1141 | `	return PH7_OK;` |
|     16 | 1142 | `}` |
|      - | 1143 | `/*` |
|      - | 1144 | ` * string html_entity_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1145 | ` *                           [, string $encoding = "UTF-8"]])` |
|      - | 1146 | ` *  Convert HTML entities (named — case-sensitive — and numeric, decimal or` |
|      - | 1147 | ` *  hex) back to their UTF-8 characters. The reverse of htmlentities().` |
|      - | 1148 | ` * Return` |
|      - | 1149 | ` *  The decoded string or NULL on failure.` |
|      - | 1150 | ` */` |
|     58 | 1151 | `static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1152 | `{` |
|     59 | 1153 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1154 | `	const char *zIn;` |
|      - | 1155 | `	int nLen;` |
|     59 | 1156 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1157 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1158 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1159 | `		return PH7_OK;` |
|      - | 1160 | `	}` |
|      - | 1161 | `	/* Extract the target string */` |
|     59 | 1162 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     59 | 1163 | `	if( nArg > 1 ){` |
|     27 | 1164 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     13 | 1165 | `	}` |
|     59 | 1166 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     59 | 1167 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,1);` |
|     59 | 1168 | `	return PH7_OK;` |
|     30 | 1169 | `}` |
|      - | 1170 | `/*` |
|      - | 1171 | ` * int strlen($string)` |
|      - | 1172 | ` *  return the length of the given string.` |
|      - | 1173 | ` * Parameter` |
|      - | 1174 | ` *  string: The string being measured for length.` |
|      - | 1175 | ` * Return` |
|      - | 1176 | ` *  length of the given string.` |
|      - | 1177 | ` */` |
|  11152 | 1178 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1179 | `{` |
|  11157 | 1180 | `	int iLen = 0;` |
|  11157 | 1181 | `	if( nArg > 0 ){` |
|  11157 | 1182 | `		ph7_value_to_string(apArg[0],&iLen);` |
|   5576 | 1183 | `	}` |
|      - | 1184 | `	/* String length */` |
|  11157 | 1185 | `	ph7_result_int(pCtx,iLen);` |
|  11157 | 1186 | `	return PH7_OK;` |
|      5 | 1187 | `}` |
|      - | 1188 | `/*` |
|      - | 1189 | ` * int strcmp(string $str1,string $str2)` |
|      - | 1190 | ` *  Perform a binary safe string comparison.` |
|      - | 1191 | ` * Parameter` |
|      - | 1192 | ` *  str1: The first string` |
|      - | 1193 | ` *  str2: The second string` |
|      - | 1194 | ` * Return` |
|      - | 1195 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1196 | ` *  than str2, and 0 if they are equal.` |
|      - | 1197 | ` */` |
|     72 | 1198 | `static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1199 | `{` |
|      - | 1200 | `	const char *z1,*z2;` |
|      - | 1201 | `	int n1,n2;` |
|      - | 1202 | `	int res;` |
|     73 | 1203 | `	if( nArg < 2 ){` |
|    ! 0 | 1204 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 1205 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 1206 | `		return PH7_OK;` |
|      - | 1207 | `	}` |
|      - | 1208 | `	/* Perform the comparison */` |
|     73 | 1209 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     73 | 1210 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     73 | 1211 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1212 | `	/* Comparison result */` |
|     73 | 1213 | `	ph7_result_int(pCtx,res);` |
|     73 | 1214 | `	return PH7_OK;` |
|     37 | 1215 | `}` |
|      - | 1216 | `/*` |
|      - | 1217 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 1218 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 1219 | ` * Parameter` |
|      - | 1220 | ` *  str1: The first string` |
|      - | 1221 | ` *  str2: The second string` |
|      - | 1222 | ` * Return` |
|      - | 1223 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1224 | ` *  than str2, and 0 if they are equal.` |
|      - | 1225 | ` */` |
|     16 | 1226 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1227 | `{` |
|      - | 1228 | `	const char *z1,*z2;` |
|      - | 1229 | `	int res;` |
|      - | 1230 | `	int n;` |
|     17 | 1231 | `	if( nArg < 3 ){` |
|      - | 1232 | `		/* Perform a standard comparison */` |
|    ! 0 | 1233 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 1234 | `	}` |
|      - | 1235 | `	/* Desired comparison length */` |
|     17 | 1236 | `	n  = ph7_value_to_int(apArg[2]);` |
|     17 | 1237 | `	if( n < 0 ){` |
|      - | 1238 | `		/* Invalid length */` |
|      3 | 1239 | `		ph7_result_int(pCtx,-1);` |
|      3 | 1240 | `		return PH7_OK;` |
|      - | 1241 | `	}` |
|      - | 1242 | `	/* Perform the comparison */` |
|     15 | 1243 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     15 | 1244 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     15 | 1245 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 1246 | `	/* Comparison result */` |
|     15 | 1247 | `	ph7_result_int(pCtx,res);` |
|     15 | 1248 | `	return PH7_OK;` |
|      9 | 1249 | `}` |
|      - | 1250 | `/*` |
|      - | 1251 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 1252 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 1253 | ` * Parameter` |
|      - | 1254 | ` *  str1: The first string` |
|      - | 1255 | ` *  str2: The second string` |
|      - | 1256 | ` * Return` |
|      - | 1257 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1258 | ` *  than str2, and 0 if they are equal.` |
|      - | 1259 | ` */` |
|     14 | 1260 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1261 | `{` |
|      - | 1262 | `	const char *z1,*z2;` |
|      - | 1263 | `	int n1,n2;` |
|      - | 1264 | `	int res;` |
|     15 | 1265 | `	if( nArg < 2 ){` |
|    ! 0 | 1266 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 1267 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 1268 | `		return PH7_OK;` |
|      - | 1269 | `	}` |
|      - | 1270 | `	/* Perform the comparison */` |
|     15 | 1271 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     15 | 1272 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     15 | 1273 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 1274 | `	/* Comparison result */` |
|     15 | 1275 | `	ph7_result_int(pCtx,res);` |
|     15 | 1276 | `	return PH7_OK;` |
|      8 | 1277 | `}` |
|      - | 1278 | `/*` |
|      - | 1279 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 1280 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 1281 | ` * Parameter` |
|      - | 1282 | ` *  $str1: The first string` |
|      - | 1283 | ` *  $str2: The second string` |
|      - | 1284 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 1285 | ` * Return` |
|      - | 1286 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1287 | ` *  than str2, and 0 if they are equal.` |
|      - | 1288 | ` */` |
|      4 | 1289 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1290 | `{` |
|      - | 1291 | `	const char *z1,*z2;` |
|      - | 1292 | `	int res;` |
|      - | 1293 | `	int n;` |
|      5 | 1294 | `	if( nArg < 3 ){` |
|      - | 1295 | `		/* Perform a standard comparison */` |
|    ! 0 | 1296 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 1297 | `	}` |
|      - | 1298 | `	/* Desired comparison length */` |
|      5 | 1299 | `	n  = ph7_value_to_int(apArg[2]);` |
|      5 | 1300 | `	if( n < 0 ){` |
|      - | 1301 | `		/* Invalid length */` |
|    ! 0 | 1302 | `		ph7_result_int(pCtx,-1);` |
|    ! 0 | 1303 | `		return PH7_OK;` |
|      - | 1304 | `	}` |
|      - | 1305 | `	/* Perform the comparison */` |
|      5 | 1306 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|      5 | 1307 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|      5 | 1308 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 1309 | `	/* Comparison result */` |
|      5 | 1310 | `	ph7_result_int(pCtx,res);` |
|      5 | 1311 | `	return PH7_OK;` |
|      3 | 1312 | `}` |
|      - | 1313 | `/*` |
|      - | 1314 | ` * Implode context [i.e: it's private data].` |
|      - | 1315 | ` * A pointer to the following structure is forwarded` |
|      - | 1316 | ` * verbatim to the array walker callback defined below.` |
|      - | 1317 | ` */` |
|      - | 1318 | `struct implode_data {` |
|      - | 1319 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 1320 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 1321 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 1322 | `	int nSeplen;          /* Separator length */` |
|      - | 1323 | `	int bFirst;           /* TRUE if first call */` |
|      - | 1324 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 1325 | `	sxi32 rc;             /* Captured allocation rc; SXERR_MEM => the builtin raises an OOM fatal */` |
|      - | 1326 | `};` |
|      - | 1327 | `/*` |
|      - | 1328 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 1329 | ` * The following routine is invoked for each array entry passed` |
|      - | 1330 | ` * to the implode() function.` |
|      - | 1331 | ` */` |
| 134616 | 1332 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 1333 | `{` |
|  67308 | 1334 | `	SXUNUSED(pKey);` |
| 134621 | 1335 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 1336 | `	const char *zData;` |
|      - | 1337 | `	int nLen;` |
| 134621 | 1338 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 1339 | `		if( pData->nSeplen > 0 ){` |
|      3 | 1340 | `			if( !pData->bFirst ){` |
|      - | 1341 | `				/* append the separator first */` |
|      3 | 1342 | `				if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 1343 | `					pData->rc = SXERR_MEM;` |
|    ! 0 | 1344 | `					return PH7_ABORT;` |
|      - | 1345 | `				}` |
|      2 | 1346 | `			}else{` |
|    ! 0 | 1347 | `				pData->bFirst = 0;` |
|      - | 1348 | `			}` |
|      1 | 1349 | `		}` |
|      - | 1350 | `		/* Recurse */` |
|      3 | 1351 | `		pData->bFirst = 1;` |
|      3 | 1352 | `		pData->nRecCount++;` |
|      3 | 1353 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 1354 | `		pData->nRecCount--;` |
|      - | 1355 | `		/* Propagate an allocation failure surfaced deeper in the recursion. */` |
|      3 | 1356 | `		if( pData->rc != SXRET_OK ){` |
|    ! 0 | 1357 | `			return PH7_ABORT;` |
|      - | 1358 | `		}` |
|      3 | 1359 | `		return PH7_OK;` |
|      - | 1360 | `	}` |
|      - | 1361 | `	/* Extract the string representation of the entry value */` |
| 134619 | 1362 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 1363 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 134619 | 1364 | `	if( pData->bFirst ){` |
|  32369 | 1365 | `		pData->bFirst = 0;` |
| 118437 | 1366 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 1367 | `		/* append the separator first */` |
| 102243 | 1368 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 1369 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1370 | `			return PH7_ABORT;` |
|      - | 1371 | `		}` |
|  51119 | 1372 | `	}` |
|      - | 1373 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 134619 | 1374 | `	if( nLen > 0 ){` |
| 122929 | 1375 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1376 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1377 | `			return PH7_ABORT;` |
|      - | 1378 | `		}` |
|  61462 | 1379 | `	}` |
| 134619 | 1380 | `	return PH7_OK;` |
|  67313 | 1381 | `}` |
|      - | 1382 | `/*` |
|      - | 1383 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 1384 | ` * string implode(array $pieces,...)` |
|      - | 1385 | ` *  Join array elements with a string.` |
|      - | 1386 | ` * $glue` |
|      - | 1387 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 1388 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 1389 | ` * $pieces` |
|      - | 1390 | ` *   The array of strings to implode.` |
|      - | 1391 | ` * Return` |
|      - | 1392 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 1393 | ` *  order, with the glue string between each element.` |
|      - | 1394 | ` */` |
|  32386 | 1395 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1396 | `{` |
|      - | 1397 | `	struct implode_data imp_data;` |
|  32391 | 1398 | `	int i = 1;` |
|  32391 | 1399 | `	if( nArg < 1 ){` |
|      - | 1400 | `		/* Missing argument,return NULL */` |
|    ! 0 | 1401 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1402 | `		return PH7_OK;` |
|      - | 1403 | `	}` |
|      - | 1404 | `	/* Prepare the implode context */` |
|  32391 | 1405 | `	imp_data.pCtx = pCtx;` |
|  32391 | 1406 | `	imp_data.bRecursive = 0;` |
|  32391 | 1407 | `	imp_data.bFirst = 1;` |
|  32391 | 1408 | `	imp_data.nRecCount = 0;` |
|  32391 | 1409 | `	imp_data.rc = SXRET_OK;` |
|  32391 | 1410 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  32389 | 1411 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  16197 | 1412 | `	}else{` |
|      3 | 1413 | `		imp_data.zSep = 0;` |
|      3 | 1414 | `		imp_data.nSeplen = 0;` |
|      3 | 1415 | `		i = 0;` |
|      - | 1416 | `	}` |
|  32391 | 1417 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 1418 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1419 | `	}` |
|      - | 1420 | `	/* Start the 'join' process */` |
|  64777 | 1421 | `	while( i < nArg ){` |
|  32391 | 1422 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1423 | `			/* Iterate throw array entries */` |
|  32391 | 1424 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 1425 | `			/* Surface a callback allocation failure as a fatal */` |
|  32391 | 1426 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 1427 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1428 | `			}` |
|  16198 | 1429 | `		}else{` |
|      - | 1430 | `			const char *zData;` |
|      - | 1431 | `			int nLen;` |
|      - | 1432 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 1433 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1434 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 1435 | `			if( imp_data.bFirst ){` |
|    ! 0 | 1436 | `				imp_data.bFirst = 0;` |
|    ! 0 | 1437 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1438 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 1439 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1440 | `				}` |
|    ! 0 | 1441 | `			}` |
|      - | 1442 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 1443 | `			if( nLen > 0 ){` |
|    ! 0 | 1444 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1445 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1446 | `				}` |
|    ! 0 | 1447 | `			}` |
|      - | 1448 | `		}` |
|  32391 | 1449 | `		i++;` |
|      5 | 1450 | `	}` |
|  32391 | 1451 | `	return PH7_OK;` |
|  16198 | 1452 | `}` |
|      - | 1453 | `/*` |
|      - | 1454 | ` * Symisc eXtension:` |
|      - | 1455 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 1456 | ` * Purpose` |
|      - | 1457 | ` *  Same as implode() but recurse on arrays.` |
|      - | 1458 | ` * Example:` |
|      - | 1459 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 1460 | ` *   echo implode_recursive("/",$a);` |
|      - | 1461 | ` *   Will output` |
|      - | 1462 | ` *     usr/home/dean.` |
|      - | 1463 | ` *   While the standard implode would produce.` |
|      - | 1464 | ` *    usr/Array.` |
|      - | 1465 | ` * Parameter` |
|      - | 1466 | ` *  Refer to implode().` |
|      - | 1467 | ` * Return` |
|      - | 1468 | ` *  Refer to implode().` |
|      - | 1469 | ` */` |
|     12 | 1470 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1471 | `{` |
|      - | 1472 | `	struct implode_data imp_data;` |
|     13 | 1473 | `	int i = 1;` |
|     13 | 1474 | `	if( nArg < 1 ){` |
|      - | 1475 | `		/* Missing argument,return NULL */` |
|      3 | 1476 | `		ph7_result_null(pCtx);` |
|      3 | 1477 | `		return PH7_OK;` |
|      - | 1478 | `	}` |
|      - | 1479 | `	/* Prepare the implode context */` |
|     11 | 1480 | `	imp_data.pCtx = pCtx;` |
|     11 | 1481 | `	imp_data.bRecursive = 1;` |
|     11 | 1482 | `	imp_data.bFirst = 1;` |
|     11 | 1483 | `	imp_data.nRecCount = 0;` |
|     11 | 1484 | `	imp_data.rc = SXRET_OK;` |
|     11 | 1485 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 1486 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 1487 | `	}else{` |
|    ! 0 | 1488 | `		imp_data.zSep = 0;` |
|    ! 0 | 1489 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 1490 | `		i = 0;` |
|      - | 1491 | `	}` |
|     11 | 1492 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 1493 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1494 | `	}` |
|      - | 1495 | `	/* Start the 'join' process */` |
|     21 | 1496 | `	while( i < nArg ){` |
|     11 | 1497 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1498 | `			/* Iterate throw array entries */` |
|      3 | 1499 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 1500 | `			/* Surface a callback allocation failure as a fatal */` |
|      3 | 1501 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 1502 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1503 | `			}` |
|      2 | 1504 | `		}else{` |
|      - | 1505 | `			const char *zData;` |
|      - | 1506 | `			int nLen;` |
|      - | 1507 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 1508 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 1509 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 1510 | `			if( imp_data.bFirst ){` |
|      9 | 1511 | `				imp_data.bFirst = 0;` |
|      4 | 1512 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 1513 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 1514 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1515 | `				}` |
|    ! 0 | 1516 | `			}` |
|      - | 1517 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 1518 | `			if( nLen > 0 ){` |
|      9 | 1519 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1520 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1521 | `				}` |
|      4 | 1522 | `			}` |
|      - | 1523 | `		}` |
|     11 | 1524 | `		i++;` |
|      1 | 1525 | `	}` |
|     11 | 1526 | `	return PH7_OK;` |
|      7 | 1527 | `}` |
|      - | 1528 | `/*` |
|      - | 1529 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 1530 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 1531 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 1532 | ` * Parameters` |
|      - | 1533 | ` *  $delimiter` |
|      - | 1534 | ` *   The boundary string.` |
|      - | 1535 | ` * $string` |
|      - | 1536 | ` *   The input string.` |
|      - | 1537 | ` * $limit` |
|      - | 1538 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 1539 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 1540 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 1541 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 1542 | ` * Returns` |
|      - | 1543 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 1544 | ` *  on boundaries formed by the delimiter.` |
|      - | 1545 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 1546 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 1547 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 1548 | ` *  will be returned.` |
|      - | 1549 | ` * NOTE:` |
|      - | 1550 | ` *  Negative limit is not supported.` |
|      - | 1551 | ` */` |
|   6274 | 1552 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1553 | `{` |
|      - | 1554 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 1555 | `	int nDelim,nStrlen,iLimit;` |
|      - | 1556 | `	ph7_value *pArray;` |
|      - | 1557 | `	ph7_value *pValue;` |
|      - | 1558 | `	sxu32 nOfft;` |
|      - | 1559 | `	sxi32 rc;` |
|   6279 | 1560 | `	if( nArg < 2 ){` |
|      - | 1561 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 1562 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1563 | `		return PH7_OK;` |
|      - | 1564 | `	}` |
|      - | 1565 | `	/* Extract the delimiter */` |
|   6279 | 1566 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   6279 | 1567 | `	if( nDelim < 1 ){` |
|      - | 1568 | `		/* Empty delimiter: PHP 8 throws a catchable ValueError. */` |
|      3 | 1569 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1570 | `			"explode(): Argument #1 ($separator) must not be empty");` |
|      - | 1571 | `	}` |
|      - | 1572 | `	/* Extract the string */` |
|   6277 | 1573 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   6277 | 1574 | `	if( nStrlen < 1 ){` |
|      - | 1575 | `		/* Empty string: normally an array with a single empty element (PHP behavior).` |
|      - | 1576 | `		 * A negative limit drops the last -limit components, so the sole empty` |
|      - | 1577 | `		 * component is dropped and the result is an empty array. */` |
|      7 | 1578 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      7 | 1579 | `		if( pArrayTmp == 0 ){` |
|      - | 1580 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 1581 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 1582 | `			return PH7_OK;` |
|      - | 1583 | `		}` |
|      7 | 1584 | `		if( !(nArg > 2 && ph7_value_to_int(apArg[2]) < 0) ){` |
|      5 | 1585 | `			ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      5 | 1586 | `			if( pValueTmp == 0 ){` |
|      - | 1587 | `				/* Out of memory,return FALSE */` |
|    ! 0 | 1588 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 1589 | `				return PH7_OK;` |
|      - | 1590 | `			}` |
|      5 | 1591 | `			ph7_value_string(pValueTmp, "", 0);` |
|      5 | 1592 | `			if( ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp) != SXRET_OK ){` |
|    ! 0 | 1593 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1594 | `			}` |
|      2 | 1595 | `		}` |
|      7 | 1596 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      7 | 1597 | `		return PH7_OK;` |
|      - | 1598 | `	}` |
|      - | 1599 | `	/* Point to the end of the string */` |
|   6271 | 1600 | `	zEnd = &zString[nStrlen];` |
|      - | 1601 | `	/* Create the array */` |
|   6271 | 1602 | `	pArray =  ph7_context_new_array(pCtx);` |
|   6271 | 1603 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   6271 | 1604 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 1605 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 1606 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1607 | `		return PH7_OK;` |
|      - | 1608 | `	}` |
|      - | 1609 | `	/* Set a defualt limit */` |
|   6271 | 1610 | `	iLimit = SXI32_HIGH;` |
|   6271 | 1611 | `	if( nArg > 2 ){` |
|     38 | 1612 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     38 | 1613 | `		if( iLimit < 0 ){` |
|      - | 1614 | `			/* Negative limit: keep all components except the last -iLimit (PHP).` |
|      - | 1615 | `			 * Pre-count the components (delimiters + 1), then emit only the first` |
|      - | 1616 | `			 * nKeep CLEAN components — no trailing-remainder merge (the difference` |
|      - | 1617 | `			 * from the positive path). nKeep <= 0 drops everything -> empty array. */` |
|     17 | 1618 | `			int nTotal = 1,nKeep;` |
|     17 | 1619 | `			const char *zScan = zString;` |
|      - | 1620 | `			sxu32 nScanOfft;` |
|     57 | 1621 | `			while( SyBlobSearch(zScan,(sxu32)(zEnd - zScan),zDelim,nDelim,&nScanOfft) == SXRET_OK ){` |
|     41 | 1622 | `				nTotal++;` |
|     41 | 1623 | `				zScan = &zScan[nScanOfft + nDelim];` |
|      1 | 1624 | `			}` |
|     17 | 1625 | `			nKeep = nTotal + iLimit; /* iLimit < 0, so this is nTotal - (-iLimit) */` |
|     49 | 1626 | `			while( nKeep > (int)ph7_array_count(pArray)` |
|     39 | 1627 | `				&& SyBlobSearch(zString,(sxu32)(zEnd - zString),zDelim,nDelim,&nOfft) == SXRET_OK ){` |
|      - | 1628 | `				/* Emit the next clean component */` |
|     23 | 1629 | `				zCur = &zString[nOfft];` |
|     23 | 1630 | `				ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|     23 | 1631 | `				if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 1632 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 1633 | `				}` |
|     23 | 1634 | `				zString = &zCur[nDelim];` |
|     23 | 1635 | `				ph7_value_reset_string_cursor(pValue);` |
|      1 | 1636 | `			}` |
|     17 | 1637 | `			ph7_result_value(pCtx,pArray);` |
|     17 | 1638 | `			return PH7_OK;` |
|      - | 1639 | `		}` |
|     22 | 1640 | `		if( iLimit == 0 ){` |
|      5 | 1641 | `			iLimit = 1;` |
|      2 | 1642 | `		}` |
|     22 | 1643 | `		iLimit--;` |
|      9 | 1644 | `	}` |
|      - | 1645 | `	/* Start exploding */` |
|  72806 | 1646 | `	for(;;){` |
| 145617 | 1647 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 145617 | 1648 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 1649 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   6255 | 1650 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   6255 | 1651 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 1652 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1653 | `			}` |
|   6255 | 1654 | `			break;` |
|      - | 1655 | `		}` |
|      - | 1656 | `		/* Point to the desired offset */` |
| 139367 | 1657 | `		zCur = &zString[nOfft];` |
|      - | 1658 | `		/* Perform the store operation (may be empty) */` |
| 139367 | 1659 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 139367 | 1660 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 1661 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 1662 | `		}` |
|      - | 1663 | `		/* Point beyond the delimiter */` |
| 139367 | 1664 | `		zString = &zCur[nDelim];` |
|      - | 1665 | `		/* Reset the cursor */` |
| 139367 | 1666 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 1667 | `	}` |
|      - | 1668 | `	/* Return the freshly created array */` |
|   6255 | 1669 | `	ph7_result_value(pCtx,pArray);` |
|      - | 1670 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 1671 | `	 * released as soon we return from this foregin function.` |
|      - | 1672 | `	 */` |
|   6255 | 1673 | `	return PH7_OK;` |
|   3142 | 1674 | `}` |
|      - | 1675 | `/*` |
|      - | 1676 | ` * string trim(string $str[,string $charlist ])` |
|      - | 1677 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 1678 | ` * Parameters` |
|      - | 1679 | ` *  $str` |
|      - | 1680 | ` *   The string that will be trimmed.` |
|      - | 1681 | ` * $charlist` |
|      - | 1682 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1683 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1684 | ` *   With .. you can specify a range of characters.` |
|      - | 1685 | ` * Returns.` |
|      - | 1686 | ` *  Thr processed string.` |
|      - | 1687 | ` * NOTE:` |
|      - | 1688 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 1689 | ` */` |
|  13870 | 1690 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1691 | `{` |
|      - | 1692 | `	const char *zString;` |
|      - | 1693 | `	int nLen;` |
|  13875 | 1694 | `	if( nArg < 1 ){` |
|      - | 1695 | `		/* Missing arguments,return null */` |
|    ! 0 | 1696 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1697 | `		return PH7_OK;` |
|      - | 1698 | `	}` |
|      - | 1699 | `	/* Extract the target string */` |
|  13875 | 1700 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  13875 | 1701 | `	if( nLen < 1 ){` |
|      - | 1702 | `		/* Empty string,return */` |
|   1331 | 1703 | `		ph7_result_string(pCtx,"",0);` |
|   1331 | 1704 | `		return PH7_OK;` |
|      - | 1705 | `	}` |
|      - | 1706 | `	/* Start the trim process */` |
|  12549 | 1707 | `	if( nArg < 2 ){` |
|      - | 1708 | `		SyString sStr;` |
|      - | 1709 | `		/* Remove white spaces and NUL bytes */` |
|  12519 | 1710 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  31263 | 1711 | `		SyStringFullTrimSafe(&sStr);` |
|  12519 | 1712 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   6262 | 1713 | `	}else{` |
|      - | 1714 | `		/* Char list */` |
|      - | 1715 | `		const char *zList;` |
|      - | 1716 | `		int nListlen;` |
|     33 | 1717 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     33 | 1718 | `		if( nListlen < 1 ){` |
|      - | 1719 | `			/* Return the string unchanged */` |
|      6 | 1720 | `			ph7_result_string(pCtx,zString,nLen);` |
|      4 | 1721 | `		}else{` |
|      - | 1722 | `			char aMask[256];` |
|     29 | 1723 | `			const char *zEnd = &zString[nLen];` |
|     29 | 1724 | `			const char *zCur = zString;` |
|     29 | 1725 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 1726 | `			/* Left trim */` |
|     79 | 1727 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     53 | 1728 | `				zCur++;` |
|      3 | 1729 | `			}` |
|      - | 1730 | `			/* Right trim */` |
|     79 | 1731 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     53 | 1732 | `				zEnd--;` |
|      3 | 1733 | `			}` |
|     29 | 1734 | `			if( zCur >= zEnd ){` |
|      - | 1735 | `				/* Return the empty string */` |
|    ! 0 | 1736 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1737 | `			}else{` |
|     29 | 1738 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 1739 | `			}` |
|      - | 1740 | `		}` |
|      - | 1741 | `	}` |
|  12549 | 1742 | `	return PH7_OK;` |
|   6940 | 1743 | `}` |
|      - | 1744 | `/*` |
|      - | 1745 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 1746 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 1747 | ` * Parameters` |
|      - | 1748 | ` *  $str` |
|      - | 1749 | ` *   The string that will be trimmed.` |
|      - | 1750 | ` * $charlist` |
|      - | 1751 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1752 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1753 | ` *   With .. you can specify a range of characters.` |
|      - | 1754 | ` * Returns.` |
|      - | 1755 | ` *  Thr processed string.` |
|      - | 1756 | ` * NOTE:` |
|      - | 1757 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 1758 | ` */` |
|     28 | 1759 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 1760 | `{` |
|      - | 1761 | `	const char *zString;` |
|      - | 1762 | `	int nLen;` |
|     31 | 1763 | `	if( nArg < 1 ){` |
|      - | 1764 | `		/* Missing arguments,return null */` |
|    ! 0 | 1765 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1766 | `		return PH7_OK;` |
|      - | 1767 | `	}` |
|      - | 1768 | `	/* Extract the target string */` |
|     31 | 1769 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 1770 | `	if( nLen < 1 ){` |
|      - | 1771 | `		/* Empty string,return */` |
|      5 | 1772 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1773 | `		return PH7_OK;` |
|      - | 1774 | `	}` |
|      - | 1775 | `	/* Start the trim process */` |
|     27 | 1776 | `	if( nArg < 2 ){` |
|      - | 1777 | `		SyString sStr;` |
|      - | 1778 | `		/* Remove white spaces and NUL bytes*/` |
|     17 | 1779 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     41 | 1780 | `		SyStringRightTrimSafe(&sStr);` |
|     17 | 1781 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      9 | 1782 | `	}else{` |
|      - | 1783 | `		/* Char list */` |
|      - | 1784 | `		const char *zList;` |
|      - | 1785 | `		int nListlen;` |
|     11 | 1786 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     11 | 1787 | `		if( nListlen < 1 ){` |
|      - | 1788 | `			/* Return the string unchanged */` |
|    ! 0 | 1789 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 1790 | `		}else{` |
|      - | 1791 | `			char aMask[256];` |
|     11 | 1792 | `			const char *zEnd = &zString[nLen];` |
|     11 | 1793 | `			const char *zCur = zString;` |
|     11 | 1794 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 1795 | `			/* Right trim */` |
|     29 | 1796 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     20 | 1797 | `				zEnd--;` |
|      2 | 1798 | `			}` |
|     11 | 1799 | `			if( zEnd <= zCur ){` |
|      - | 1800 | `				/* Return the empty string */` |
|    ! 0 | 1801 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1802 | `			}else{` |
|     11 | 1803 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 1804 | `			}` |
|      - | 1805 | `		}` |
|      - | 1806 | `	}` |
|     27 | 1807 | `	return PH7_OK;` |
|     17 | 1808 | `}` |
|      - | 1809 | `/*` |
|      - | 1810 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 1811 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 1812 | ` * Parameters` |
|      - | 1813 | ` *  $str` |
|      - | 1814 | ` *   The string that will be trimmed.` |
|      - | 1815 | ` * $charlist` |
|      - | 1816 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 1817 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 1818 | ` *   With .. you can specify a range of characters.` |
|      - | 1819 | ` * Returns.` |
|      - | 1820 | ` *  Thr processed string.` |
|      - | 1821 | ` * NOTE:` |
|      - | 1822 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 1823 | ` */` |
|     12 | 1824 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1825 | `{` |
|      - | 1826 | `	const char *zString;` |
|      - | 1827 | `	int nLen;` |
|     14 | 1828 | `	if( nArg < 1 ){` |
|      - | 1829 | `		/* Missing arguments,return null */` |
|    ! 0 | 1830 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1831 | `		return PH7_OK;` |
|      - | 1832 | `	}` |
|      - | 1833 | `	/* Extract the target string */` |
|     14 | 1834 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     14 | 1835 | `	if( nLen < 1 ){` |
|      - | 1836 | `		/* Empty string,return */` |
|    ! 0 | 1837 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1838 | `		return PH7_OK;` |
|      - | 1839 | `	}` |
|      - | 1840 | `	/* Start the trim process */` |
|     14 | 1841 | `	if( nArg < 2 ){` |
|      - | 1842 | `		SyString sStr;` |
|      - | 1843 | `		/* Remove white spaces and NUL byte */` |
|      3 | 1844 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|      8 | 1845 | `		SyStringLeftTrimSafe(&sStr);` |
|      3 | 1846 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      2 | 1847 | `	}else{` |
|      - | 1848 | `		/* Char list */` |
|      - | 1849 | `		const char *zList;` |
|      - | 1850 | `		int nListlen;` |
|     12 | 1851 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     12 | 1852 | `		if( nListlen < 1 ){` |
|      - | 1853 | `			/* Return the string unchanged */` |
|      3 | 1854 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 1855 | `		}else{` |
|      - | 1856 | `			char aMask[256];` |
|     10 | 1857 | `			const char *zEnd = &zString[nLen];` |
|     10 | 1858 | `			const char *zCur = zString;` |
|     10 | 1859 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 1860 | `			/* Left trim */` |
|     28 | 1861 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     20 | 1862 | `				zCur++;` |
|      2 | 1863 | `			}` |
|     10 | 1864 | `			if( zCur >= zEnd ){` |
|      - | 1865 | `				/* Return the empty string */` |
|    ! 0 | 1866 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 1867 | `			}else{` |
|     10 | 1868 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 1869 | `			}` |
|      - | 1870 | `		}` |
|      - | 1871 | `	}` |
|     14 | 1872 | `	return PH7_OK;` |
|      8 | 1873 | `}` |
|      - | 1874 | `/*` |
|      - | 1875 | ` * string strtolower(string $str)` |
|      - | 1876 | ` *  Make a string lowercase.` |
|      - | 1877 | ` * Parameters` |
|      - | 1878 | ` *  $str` |
|      - | 1879 | ` *   The input string.` |
|      - | 1880 | ` * Returns.` |
|      - | 1881 | ` *  The lowercased string.` |
|      - | 1882 | ` */` |
|  31984 | 1883 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1884 | `{` |
|      - | 1885 | `	const char *zString,*zCur,*zEnd;` |
|      - | 1886 | `	int nLen;` |
|  31989 | 1887 | `	if( nArg < 1 ){` |
|      - | 1888 | `		/* Missing arguments,return null */` |
|    ! 0 | 1889 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1890 | `		return PH7_OK;` |
|      - | 1891 | `	}` |
|      - | 1892 | `	/* Extract the target string */` |
|  31989 | 1893 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  31989 | 1894 | `	if( nLen < 1 ){` |
|      - | 1895 | `		/* Empty string,return */` |
|      3 | 1896 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1897 | `		return PH7_OK;` |
|      - | 1898 | `	}` |
|      - | 1899 | `	/* Perform the requested operation */` |
|  31987 | 1900 | `	zEnd = &zString[nLen];` |
| 100662 | 1901 | `	for(;;){` |
| 201329 | 1902 | `		if( zString >= zEnd ){` |
|      - | 1903 | `			/* No more input,break immediately */` |
|  31987 | 1904 | `			break;` |
|      - | 1905 | `		}` |
| 169347 | 1906 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 1907 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 1908 | `			zCur = zString;` |
|    ! 0 | 1909 | `			zString++;` |
|    ! 0 | 1910 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 1911 | `				zString++;` |
|    ! 0 | 1912 | `			}` |
|      - | 1913 | `			/* Append UTF-8 stream */` |
|    ! 0 | 1914 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 1915 | `		}else{` |
| 169347 | 1916 | `			int c = zString[0];` |
| 169347 | 1917 | `			if( SyisUpper(c) ){` |
| 169345 | 1918 | `				c = SyToLower(zString[0]);` |
|  84670 | 1919 | `			}` |
|      - | 1920 | `			/* Append character */` |
| 169347 | 1921 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 1922 | `			/* Advance the cursor */` |
| 169347 | 1923 | `			zString++;` |
|      - | 1924 | `		}` |
|      5 | 1925 | `	}` |
|  31987 | 1926 | `	return PH7_OK;` |
|  15997 | 1927 | `}` |
|      - | 1928 | `/*` |
|      - | 1929 | ` * string strtolower(string $str)` |
|      - | 1930 | ` *  Make a string uppercase.` |
|      - | 1931 | ` * Parameters` |
|      - | 1932 | ` *  $str` |
|      - | 1933 | ` *   The input string.` |
|      - | 1934 | ` * Returns.` |
|      - | 1935 | ` *  The uppercased string.` |
|      - | 1936 | ` */` |
|     44 | 1937 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1938 | `{` |
|      - | 1939 | `	const char *zString,*zCur,*zEnd;` |
|      - | 1940 | `	int nLen;` |
|     48 | 1941 | `	if( nArg < 1 ){` |
|      - | 1942 | `		/* Missing arguments,return null */` |
|    ! 0 | 1943 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1944 | `		return PH7_OK;` |
|      - | 1945 | `	}` |
|      - | 1946 | `	/* Extract the target string */` |
|     48 | 1947 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     48 | 1948 | `	if( nLen < 1 ){` |
|      - | 1949 | `		/* Empty string,return */` |
|      3 | 1950 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1951 | `		return PH7_OK;` |
|      - | 1952 | `	}` |
|      - | 1953 | `	/* Perform the requested operation */` |
|     46 | 1954 | `	zEnd = &zString[nLen];` |
|    107 | 1955 | `	for(;;){` |
|    218 | 1956 | `		if( zString >= zEnd ){` |
|      - | 1957 | `			/* No more input,break immediately */` |
|     46 | 1958 | `			break;` |
|      - | 1959 | `		}` |
|    176 | 1960 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 1961 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 1962 | `			zCur = zString;` |
|    ! 0 | 1963 | `			zString++;` |
|    ! 0 | 1964 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 1965 | `				zString++;` |
|    ! 0 | 1966 | `			}` |
|      - | 1967 | `			/* Append UTF-8 stream */` |
|    ! 0 | 1968 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 1969 | `		}else{` |
|    176 | 1970 | `			int c = zString[0];` |
|    176 | 1971 | `			if( SyisLower(c) ){` |
|    170 | 1972 | `				c = SyToUpper(zString[0]);` |
|     83 | 1973 | `			}` |
|      - | 1974 | `			/* Append character */` |
|    176 | 1975 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 1976 | `			/* Advance the cursor */` |
|    176 | 1977 | `			zString++;` |
|      - | 1978 | `		}` |
|      4 | 1979 | `	}` |
|     46 | 1980 | `	return PH7_OK;` |
|     26 | 1981 | `}` |
|      - | 1982 | `/*` |
|      - | 1983 | ` * string ucfirst(string $str)` |
|      - | 1984 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 1985 | ` *  character is alphabetic.` |
|      - | 1986 | ` * Parameters` |
|      - | 1987 | ` *  $str` |
|      - | 1988 | ` *   The input string.` |
|      - | 1989 | ` * Returns.` |
|      - | 1990 | ` *  The processed string.` |
|      - | 1991 | ` */` |
|      4 | 1992 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1993 | `{` |
|      - | 1994 | `	const char *zString,*zEnd;` |
|      - | 1995 | `	int nLen,c;` |
|      5 | 1996 | `	if( nArg < 1 ){` |
|      - | 1997 | `		/* Missing arguments,return null */` |
|    ! 0 | 1998 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1999 | `		return PH7_OK;` |
|      - | 2000 | `	}` |
|      - | 2001 | `	/* Extract the target string */` |
|      5 | 2002 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2003 | `	if( nLen < 1 ){` |
|      - | 2004 | `		/* Empty string,return */` |
|      3 | 2005 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2006 | `		return PH7_OK;` |
|      - | 2007 | `	}` |
|      - | 2008 | `	/* Perform the requested operation */` |
|      3 | 2009 | `	zEnd = &zString[nLen];` |
|      3 | 2010 | `	c = zString[0];` |
|      3 | 2011 | `	if( SyisLower(c) ){` |
|      3 | 2012 | `		c = SyToUpper(c);` |
|      1 | 2013 | `	}` |
|      - | 2014 | `	/* Append the first character */` |
|      3 | 2015 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2016 | `	zString++;` |
|      3 | 2017 | `	if( zString < zEnd ){` |
|      - | 2018 | `		/* Append the rest of the input verbatim */` |
|      3 | 2019 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2020 | `	}` |
|      3 | 2021 | `	return PH7_OK;` |
|      3 | 2022 | `}` |
|      - | 2023 | `/*` |
|      - | 2024 | ` * string lcfirst(string $str)` |
|      - | 2025 | ` *  Make a string's first character lowercase.` |
|      - | 2026 | ` * Parameters` |
|      - | 2027 | ` *  $str` |
|      - | 2028 | ` *   The input string.` |
|      - | 2029 | ` * Returns.` |
|      - | 2030 | ` *  The processed string.` |
|      - | 2031 | ` */` |
|      4 | 2032 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2033 | `{` |
|      - | 2034 | `	const char *zString,*zEnd;` |
|      - | 2035 | `	int nLen,c;` |
|      5 | 2036 | `	if( nArg < 1 ){` |
|      - | 2037 | `		/* Missing arguments,return null */` |
|    ! 0 | 2038 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2039 | `		return PH7_OK;` |
|      - | 2040 | `	}` |
|      - | 2041 | `	/* Extract the target string */` |
|      5 | 2042 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2043 | `	if( nLen < 1 ){` |
|      - | 2044 | `		/* Empty string,return */` |
|      3 | 2045 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2046 | `		return PH7_OK;` |
|      - | 2047 | `	}` |
|      - | 2048 | `	/* Perform the requested operation */` |
|      3 | 2049 | `	zEnd = &zString[nLen];` |
|      3 | 2050 | `	c = zString[0];` |
|      3 | 2051 | `	if( SyisUpper(c) ){` |
|      3 | 2052 | `		c = SyToLower(c);` |
|      1 | 2053 | `	}` |
|      - | 2054 | `	/* Append the first character */` |
|      3 | 2055 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2056 | `	zString++;` |
|      3 | 2057 | `	if( zString < zEnd ){` |
|      - | 2058 | `		/* Append the rest of the input verbatim */` |
|      3 | 2059 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2060 | `	}` |
|      3 | 2061 | `	return PH7_OK;` |
|      3 | 2062 | `}` |
|      - | 2063 | `/*` |
|      - | 2064 | ` * int ord(string $string)` |
|      - | 2065 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 2066 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 2067 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 2068 | ` * Parameters` |
|      - | 2069 | ` *  $string` |
|      - | 2070 | ` *   The input string.` |
|      - | 2071 | ` * Returns` |
|      - | 2072 | ` *  The ASCII value as an integer.` |
|      - | 2073 | ` */` |
|     56 | 2074 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2075 | `{` |
|      - | 2076 | `	const char *zString;` |
|      - | 2077 | `	int nLen,c;` |
|      - | 2078 | `	/* PHP requires exactly one argument. */` |
|     59 | 2079 | `	if( nArg != 1 ){` |
|      8 | 2080 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2081 | `			"ArgumentCountError",` |
|      - | 2082 | `			"ord() expects exactly 1 argument, %d given",` |
|      2 | 2083 | `			nArg` |
|      - | 2084 | `			);` |
|      - | 2085 | `	}` |
|      - | 2086 | `	/* Passing null is deprecated (E_DEPRECATED).  PHP emits this before` |
|      - | 2087 | `	 * the empty-string deprecation, so we check null first. */` |
|     53 | 2088 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 2089 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2090 | `			"ord(): Passing null to parameter #1 ($character) "` |
|      - | 2091 | `			"of type string is deprecated"` |
|      - | 2092 | `			);` |
|      1 | 2093 | `	}` |
|      - | 2094 | `	/* Extract the target string */` |
|     53 | 2095 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     53 | 2096 | `	if( nLen < 1 ){` |
|      - | 2097 | `		/* Empty string is deprecated (E_DEPRECATED). */` |
|      5 | 2098 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2099 | `			"ord(): Providing an empty string is deprecated"` |
|      - | 2100 | `			);` |
|      5 | 2101 | `		ph7_result_int(pCtx,0);` |
|      5 | 2102 | `		return PH7_OK;` |
|      - | 2103 | `	}` |
|      - | 2104 | `	/* A string longer than one byte is deprecated (E_DEPRECATED). */` |
|     49 | 2105 | `	if( nLen > 1 ){` |
|      7 | 2106 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2107 | `			"ord(): Providing a string that is not one byte long "` |
|      - | 2108 | `			"is deprecated. Use ord($str[0]) instead"` |
|      - | 2109 | `			);` |
|      3 | 2110 | `	}` |
|      - | 2111 | `	/* Extract the ASCII value of the first character */` |
|     49 | 2112 | `	c = (unsigned char)zString[0];` |
|      - | 2113 | `	/* Return that value */` |
|     49 | 2114 | `	ph7_result_int(pCtx,c);` |
|     49 | 2115 | `	return PH7_OK;` |
|     31 | 2116 | `}` |
|      - | 2117 | `/*` |
|      - | 2118 | ` * string chr(int $codepoint)` |
|      - | 2119 | ` *  Returns a one-character string containing the character specified` |
|      - | 2120 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 2121 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 2122 | ` * Parameters` |
|      - | 2123 | ` *  $codepoint` |
|      - | 2124 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 2125 | ` *   will be constrained to a single byte.` |
|      - | 2126 | ` * Returns` |
|      - | 2127 | ` *  A single-character string.` |
|      - | 2128 | ` */` |
|     48 | 2129 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2130 | `{` |
|      - | 2131 | `	int c;` |
|      - | 2132 | `	unsigned char ch;` |
|      - | 2133 | `	/* PHP requires exactly one argument. */` |
|     51 | 2134 | `	if( nArg != 1 ){` |
|      8 | 2135 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2136 | `			"ArgumentCountError",` |
|      - | 2137 | `			"chr() expects exactly 1 argument, %d given",` |
|      2 | 2138 | `			nArg` |
|      - | 2139 | `			);` |
|      - | 2140 | `	}` |
|      - | 2141 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 2142 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 2143 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 2144 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|     45 | 2145 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      - | 2146 | `		char zBuf[120];` |
|      4 | 2147 | `		SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 2148 | `			"Implicit conversion from float %g to int loses precision",` |
|      1 | 2149 | `			ph7_value_to_double(apArg[0])` |
|      - | 2150 | `			);` |
|      3 | 2151 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);` |
|      1 | 2152 | `	}` |
|      - | 2153 | `	/* Extract the codepoint. */` |
|     45 | 2154 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 2155 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 2156 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 2157 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 2158 | `	 * name to avoid the API double-prefixing it. */` |
|     45 | 2159 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 2160 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 2161 | `			E_DEPRECATED,` |
|      - | 2162 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 2163 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 2164 | `			"The value used will be constrained using % 256"` |
|      - | 2165 | `			);` |
|      2 | 2166 | `	}` |
|      - | 2167 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 2168 | `	 * when taking the address of a wider int. */` |
|     45 | 2169 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 2170 | `	/* Return the specified character */` |
|     45 | 2171 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|     45 | 2172 | `	return PH7_OK;` |
|     27 | 2173 | `}` |
|      - | 2174 | `/*` |
|      - | 2175 | ` * Binary to hex consumer callback.` |
|      - | 2176 | ` * This callback is the default consumer used by the hash functions` |
|      - | 2177 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 2178 | ` */` |
|   3118 | 2179 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 2180 | `{` |
|      - | 2181 | `	/* Append hex chunk verbatim */` |
|   3120 | 2182 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   3120 | 2183 | `	return SXRET_OK;` |
|      2 | 2184 | `}` |
|      - | 2185 |  |
|      - | 2186 | `/*` |
|      - | 2187 | ` * string bin2hex(string $str)` |
|      - | 2188 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 2189 | ` * Parameters` |
|      - | 2190 | ` *  $str` |
|      - | 2191 | ` *   The input string.` |
|      - | 2192 | ` * Returns.` |
|      - | 2193 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 2194 | ` */` |
|    138 | 2195 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2196 | `{` |
|      - | 2197 | `	const char *zString;` |
|      - | 2198 | `	int nLen;` |
|      - | 2199 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|    143 | 2200 | `	if( nArg != 1 ){` |
|      8 | 2201 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2202 | `			"ArgumentCountError",` |
|      - | 2203 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|      2 | 2204 | `			nArg` |
|      - | 2205 | `			);` |
|      - | 2206 | `	}` |
|      - | 2207 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 2208 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 2209 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 2210 | `	 */` |
|    204 | 2211 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|    130 | 2212 | `		( ph7_value_is_object(apArg[0]) &&` |
|      3 | 2213 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|      2 | 2214 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|      1 | 2215 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 2216 | `		)` |
|      - | 2217 | `	){` |
|      9 | 2218 | `		const char *zType = ph7_type_name(apArg[0]);` |
|      9 | 2219 | `		if( ph7_value_is_object(apArg[0]) ){` |
|      3 | 2220 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      3 | 2221 | `			if( pInst && pInst->pClass ){` |
|      3 | 2222 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      1 | 2223 | `			}` |
|      1 | 2224 | `		}` |
|     12 | 2225 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2226 | `			"TypeError",` |
|      - | 2227 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|      3 | 2228 | `			zType` |
|      - | 2229 | `			);` |
|      - | 2230 | `	}` |
|      - | 2231 | `	/* Extract the target string */` |
|    130 | 2232 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    130 | 2233 | `	if( nLen < 1 ){` |
|      - | 2234 | `		/* Empty string,return */` |
|     13 | 2235 | `		ph7_result_string(pCtx,"",0);` |
|     13 | 2236 | `		return PH7_OK;` |
|      - | 2237 | `	}` |
|      - | 2238 | `	/* Perform the requested operation */` |
|    118 | 2239 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|    118 | 2240 | `	return PH7_OK;` |
|     74 | 2241 | `}` |
|      - | 2242 |  |
|      - | 2243 | `/* Search callback signature */` |
|      - | 2244 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 2245 | `/*` |
|      - | 2246 | ` * Case-insensitive pattern match.` |
|      - | 2247 | ` * Brute force is the default search method used here.` |
|      - | 2248 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 2249 | ` * well for short/medium texts on modern hardware.` |
|      - | 2250 | ` */` |
|    118 | 2251 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      1 | 2252 | `{` |
|    119 | 2253 | `	const char *zpIn = (const char *)pPattern;` |
|    119 | 2254 | `	const char *zIn = (const char *)pText;` |
|    119 | 2255 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    119 | 2256 | `	const char *zEnd = &zIn[nLen];` |
|      - | 2257 | `	const char *zPtr,*zPtr2;` |
|      - | 2258 | `	int c,d;` |
|    119 | 2259 | `	if( iPatLen > nLen ){` |
|      - | 2260 | `		/* Don't bother processing */` |
|     33 | 2261 | `		return SXERR_NOTFOUND;` |
|      - | 2262 | `	}` |
|    242 | 2263 | `	for(;;){` |
|    485 | 2264 | `		if( zIn >= zEnd ){` |
|     47 | 2265 | `			break;` |
|      - | 2266 | `		}` |
|    439 | 2267 | `		c = SyToLower(zIn[0]);` |
|    439 | 2268 | `		d = SyToLower(zpIn[0]);` |
|    439 | 2269 | `		if( c == d ){` |
|     41 | 2270 | `			zPtr   = &zIn[1];` |
|     41 | 2271 | `			zPtr2  = &zpIn[1];` |
|     71 | 2272 | `			for(;;){` |
|    143 | 2273 | `				if( zPtr2 >= zpEnd ){` |
|      - | 2274 | `					/* Pattern found */` |
|     41 | 2275 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 2276 | `					return SXRET_OK;` |
|      - | 2277 | `				}` |
|    103 | 2278 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 2279 | `					break;` |
|      - | 2280 | `				}` |
|    103 | 2281 | `				c = SyToLower(zPtr[0]);` |
|    103 | 2282 | `				d = SyToLower(zPtr2[0]);` |
|    103 | 2283 | `				if( c != d ){` |
|    ! 0 | 2284 | `					break;` |
|      - | 2285 | `				}` |
|    103 | 2286 | `				zPtr++; zPtr2++;` |
|      1 | 2287 | `			}` |
|    ! 0 | 2288 | `		}` |
|    399 | 2289 | `		zIn++;` |
|      1 | 2290 | `	}` |
|      - | 2291 | `	/* Pattern not found */` |
|     47 | 2292 | `	return SXERR_NOTFOUND;` |
|     60 | 2293 | `}` |
|      - | 2294 | `/*` |
|      - | 2295 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2296 | ` *  Find the first occurrence of a string.` |
|      - | 2297 | ` * Parameters` |
|      - | 2298 | ` *  $haystack` |
|      - | 2299 | ` *   The input string.` |
|      - | 2300 | ` * $needle` |
|      - | 2301 | ` *   Search pattern (must be a string).` |
|      - | 2302 | ` * $before_needle` |
|      - | 2303 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2304 | ` *   of the needle (excluding the needle).` |
|      - | 2305 | ` * Return` |
|      - | 2306 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2307 | ` */` |
|      6 | 2308 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2309 | `{` |
|      7 | 2310 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2311 | `	const char *zBlob,*zPattern;` |
|      - | 2312 | `	int nLen,nPatLen;` |
|      - | 2313 | `	sxu32 nOfft;` |
|      - | 2314 | `	sxi32 rc;` |
|      7 | 2315 | `	if( nArg < 2 ){` |
|      - | 2316 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2317 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2318 | `		return PH7_OK;` |
|      - | 2319 | `	}` |
|      - | 2320 | `	/* Extract the needle and the haystack */` |
|      7 | 2321 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 2322 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 2323 | `	nOfft = 0; /* cc warning */` |
|      9 | 2324 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2325 | `		int before = 0;` |
|      - | 2326 | `		/* Perform the lookup */` |
|      5 | 2327 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2328 | `		if( rc != SXRET_OK ){` |
|      - | 2329 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2330 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2331 | `			return PH7_OK;` |
|      - | 2332 | `		}` |
|      - | 2333 | `		/* Return the portion of the string */` |
|      5 | 2334 | `		if( nArg > 2 ){` |
|      3 | 2335 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2336 | `		}` |
|      5 | 2337 | `		if( before ){` |
|      3 | 2338 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2339 | `		}else{` |
|      3 | 2340 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2341 | `		}` |
|      3 | 2342 | `	}else{` |
|      3 | 2343 | `		ph7_result_bool(pCtx,0);` |
|      - | 2344 | `	}` |
|      7 | 2345 | `	return PH7_OK;` |
|      4 | 2346 | `}` |
|      - | 2347 | `/*` |
|      - | 2348 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 2349 | ` *  Case-insensitive strstr().` |
|      - | 2350 | ` * Parameters` |
|      - | 2351 | ` *  $haystack` |
|      - | 2352 | ` *   The input string.` |
|      - | 2353 | ` * $needle` |
|      - | 2354 | ` *   Search pattern (must be a string).` |
|      - | 2355 | ` * $before_needle` |
|      - | 2356 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 2357 | ` *   of the needle (excluding the needle).` |
|      - | 2358 | ` * Return` |
|      - | 2359 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 2360 | ` */` |
|      4 | 2361 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2362 | `{` |
|      5 | 2363 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2364 | `	const char *zBlob,*zPattern;` |
|      - | 2365 | `	int nLen,nPatLen;` |
|      - | 2366 | `	sxu32 nOfft;` |
|      - | 2367 | `	sxi32 rc;` |
|      5 | 2368 | `	if( nArg < 2 ){` |
|      - | 2369 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2370 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2371 | `		return PH7_OK;` |
|      - | 2372 | `	}` |
|      - | 2373 | `	/* Extract the needle and the haystack */` |
|      5 | 2374 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2375 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 2376 | `	nOfft = 0; /* cc warning */` |
|      7 | 2377 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 2378 | `		int before = 0;` |
|      - | 2379 | `		/* Perform the lookup */` |
|      5 | 2380 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 2381 | `		if( rc != SXRET_OK ){` |
|      - | 2382 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 2383 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2384 | `			return PH7_OK;` |
|      - | 2385 | `		}` |
|      - | 2386 | `		/* Return the portion of the string */` |
|      5 | 2387 | `		if( nArg > 2 ){` |
|      3 | 2388 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 2389 | `		}` |
|      5 | 2390 | `		if( before ){` |
|      3 | 2391 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 2392 | `		}else{` |
|      3 | 2393 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 2394 | `		}` |
|      3 | 2395 | `	}else{` |
|    ! 0 | 2396 | `		ph7_result_bool(pCtx,0);` |
|      - | 2397 | `	}` |
|      5 | 2398 | `	return PH7_OK;` |
|      3 | 2399 | `}` |
|      - | 2400 | `/*` |
|      - | 2401 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2402 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 2403 | ` * Parameters` |
|      - | 2404 | ` *  $haystack` |
|      - | 2405 | ` *   The input string.` |
|      - | 2406 | ` * $needle` |
|      - | 2407 | ` *   Search pattern (must be a string).` |
|      - | 2408 | ` * $offset` |
|      - | 2409 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 2410 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 2411 | ` *   of haystack.` |
|      - | 2412 | ` * Return` |
|      - | 2413 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 2414 | ` */` |
|    352 | 2415 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2416 | `{` |
|    357 | 2417 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2418 | `	const char *zBlob,*zPattern;` |
|      - | 2419 | `	int nLen,nPatLen,nStart;` |
|      - | 2420 | `	sxu32 nOfft;` |
|      - | 2421 | `	sxi32 rc;` |
|    357 | 2422 | `	if( nArg < 2 ){` |
|      - | 2423 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2424 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2425 | `		return PH7_OK;` |
|      - | 2426 | `	}` |
|      - | 2427 | `	/* Extract the needle and the haystack */` |
|    357 | 2428 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    357 | 2429 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    357 | 2430 | `	nOfft = 0; /* cc warning */` |
|    357 | 2431 | `	nStart = 0;` |
|      - | 2432 | `	/* Peek the starting offset if available */` |
|    357 | 2433 | `	if( nArg > 2 ){` |
|    ! 0 | 2434 | `		nStart = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 2435 | `		if( nStart < 0 ){` |
|    ! 0 | 2436 | `			nStart = -nStart;` |
|    ! 0 | 2437 | `		}` |
|    ! 0 | 2438 | `		if( nStart >= nLen ){` |
|      - | 2439 | `			/* Invalid offset */` |
|    ! 0 | 2440 | `			nStart = 0;` |
|    ! 0 | 2441 | `		}else{` |
|    ! 0 | 2442 | `			zBlob += nStart;` |
|    ! 0 | 2443 | `			nLen -= nStart;` |
|      - | 2444 | `		}` |
|    ! 0 | 2445 | `	}` |
|    357 | 2446 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2447 | `		/* Perform the lookup */` |
|    355 | 2448 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    355 | 2449 | `		if( rc != SXRET_OK ){` |
|      - | 2450 | `			/* Pattern not found,return FALSE */` |
|    153 | 2451 | `			ph7_result_bool(pCtx,0);` |
|    153 | 2452 | `			return PH7_OK;` |
|      - | 2453 | `		}` |
|      - | 2454 | `		/* Return the pattern position */` |
|    206 | 2455 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|    105 | 2456 | `	}else{` |
|      3 | 2457 | `		ph7_result_bool(pCtx,0);` |
|      - | 2458 | `	}` |
|    208 | 2459 | `	return PH7_OK;` |
|    181 | 2460 | `}` |
|      - | 2461 | `/*` |
|      - | 2462 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|      - | 2463 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|      - | 2464 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|      - | 2465 | ` * TypeError for arrays, resources, and objects without __toString.` |
|      - | 2466 | ` *` |
|      - | 2467 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|      - | 2468 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|      - | 2469 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|      - | 2470 | ` *` |
|      - | 2471 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|      - | 2472 | ` * is valid until pTmp is released or pArg is mutated.` |
|      - | 2473 | ` */` |
|    426 | 2474 | `static sxi32 StrPredicateResolveArg(` |
|      - | 2475 | `	ph7_context *pCtx,` |
|      - | 2476 | `	ph7_value *pArg,` |
|      - | 2477 | `	const char *zFunc,` |
|      - | 2478 | `	int iArgNum,` |
|      - | 2479 | `	const char *zParamName,` |
|      - | 2480 | `	const char *zNullMsg,` |
|      - | 2481 | `	ph7_value *pTmp,` |
|      - | 2482 | `	const char **pzOut,` |
|      - | 2483 | `	int *pnOut` |
|      4 | 2484 | `){` |
|    430 | 2485 | `	if( ph7_value_is_null(pArg) ){` |
|     13 | 2486 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zNullMsg);` |
|     13 | 2487 | `		*pzOut = "";` |
|     13 | 2488 | `		*pnOut = 0;` |
|     13 | 2489 | `		return PH7_OK;` |
|      - | 2490 | `	}` |
|    640 | 2491 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    396 | 2492 | `	    ( ph7_value_is_object(pArg) &&` |
|     72 | 2493 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     48 | 2494 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     24 | 2495 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 2496 | `	    )` |
|      - | 2497 | `	){` |
|     34 | 2498 | `		const char *zType = ph7_type_name(pArg);` |
|     34 | 2499 | `		if( ph7_value_is_object(pArg) ){` |
|     13 | 2500 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     13 | 2501 | `			if( pInst && pInst->pClass ){` |
|     13 | 2502 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      6 | 2503 | `			}` |
|      6 | 2504 | `		}` |
|     49 | 2505 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2506 | `			"TypeError",` |
|      - | 2507 | `			"%s(): Argument #%d (%s) must be of type string, %s given",` |
|     15 | 2508 | `			zFunc, iArgNum, zParamName, zType` |
|      - | 2509 | `			);` |
|      - | 2510 | `	}` |
|    385 | 2511 | `	if( ph7_value_is_object(pArg) ){` |
|     37 | 2512 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     37 | 2513 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 2514 | `			"__toString",sizeof("__toString")-1);` |
|     37 | 2515 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|     37 | 2516 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|     37 | 2517 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|     37 | 2518 | `		return PH7_OK;` |
|      - | 2519 | `	}` |
|    349 | 2520 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|    349 | 2521 | `	return PH7_OK;` |
|    217 | 2522 | `}` |
|      - | 2523 | `/*` |
|      - | 2524 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 2525 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 2526 | ` * Return` |
|      - | 2527 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 2528 | ` */` |
|     96 | 2529 | `static int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2530 | `{` |
|      - | 2531 | `	const char *zHaystack,*zNeedle;` |
|      - | 2532 | `	int nHayLen,nNeedleLen;` |
|      - | 2533 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2534 | `	sxi32 rc;` |
|    100 | 2535 | `	if( nArg != 2 ){` |
|     18 | 2536 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2537 | `			"ArgumentCountError",` |
|      - | 2538 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|      5 | 2539 | `			nArg` |
|      - | 2540 | `			);` |
|      - | 2541 | `	}` |
|     88 | 2542 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     88 | 2543 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     88 | 2544 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack",` |
|      - | 2545 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|      - | 2546 | `		"of type string is deprecated",` |
|      - | 2547 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     88 | 2548 | `	if( rc != PH7_OK ) goto out;` |
|     81 | 2549 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle",` |
|      - | 2550 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|      - | 2551 | `		"of type string is deprecated",` |
|      - | 2552 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     81 | 2553 | `	if( rc != PH7_OK ) goto out;` |
|     77 | 2554 | `	if( nNeedleLen < 1 ){` |
|     13 | 2555 | `		ph7_result_bool(pCtx,1);` |
|     71 | 2556 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2557 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2558 | `	}else{` |
|     85 | 2559 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|     28 | 2560 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|     57 | 2561 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|      - | 2562 | `	}` |
|     77 | 2563 | `	rc = PH7_OK;` |
|     43 | 2564 | `out:` |
|     88 | 2565 | `	PH7_MemObjRelease(&sHayTmp);` |
|     88 | 2566 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     88 | 2567 | `	return rc;` |
|     52 | 2568 | `}` |
|      - | 2569 | `/*` |
|      - | 2570 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 2571 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 2572 | ` * Return` |
|      - | 2573 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 2574 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 2575 | ` */` |
|     78 | 2576 | `static int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2577 | `{` |
|      - | 2578 | `	const char *zHaystack,*zNeedle;` |
|      - | 2579 | `	int nHayLen,nNeedleLen;` |
|      - | 2580 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2581 | `	sxi32 rc;` |
|     82 | 2582 | `	if( nArg != 2 ){` |
|     18 | 2583 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2584 | `			"ArgumentCountError",` |
|      - | 2585 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|      5 | 2586 | `			nArg` |
|      - | 2587 | `			);` |
|      - | 2588 | `	}` |
|     70 | 2589 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 2590 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 2591 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack",` |
|      - | 2592 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 2593 | `		"of type string is deprecated",` |
|      - | 2594 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 2595 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 2596 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle",` |
|      - | 2597 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|      - | 2598 | `		"of type string is deprecated",` |
|      - | 2599 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 2600 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 2601 | `	if( nNeedleLen < 1 ){` |
|     13 | 2602 | `		ph7_result_bool(pCtx,1);` |
|     53 | 2603 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2604 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2605 | `	}else{` |
|     58 | 2606 | `		ph7_result_bool(pCtx,` |
|     38 | 2607 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 2608 | `	}` |
|     59 | 2609 | `	rc = PH7_OK;` |
|     34 | 2610 | `out:` |
|     70 | 2611 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 2612 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 2613 | `	return rc;` |
|     43 | 2614 | `}` |
|      - | 2615 | `/*` |
|      - | 2616 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 2617 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 2618 | ` * Return` |
|      - | 2619 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 2620 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 2621 | ` */` |
|     78 | 2622 | `static int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2623 | `{` |
|      - | 2624 | `	const char *zHaystack,*zNeedle;` |
|      - | 2625 | `	int nHayLen,nNeedleLen;` |
|      - | 2626 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 2627 | `	sxi32 rc;` |
|     82 | 2628 | `	if( nArg != 2 ){` |
|     18 | 2629 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2630 | `			"ArgumentCountError",` |
|      - | 2631 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|      5 | 2632 | `			nArg` |
|      - | 2633 | `			);` |
|      - | 2634 | `	}` |
|     70 | 2635 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 2636 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 2637 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack",` |
|      - | 2638 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 2639 | `		"of type string is deprecated",` |
|      - | 2640 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 2641 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 2642 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle",` |
|      - | 2643 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|      - | 2644 | `		"of type string is deprecated",` |
|      - | 2645 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 2646 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 2647 | `	if( nNeedleLen < 1 ){` |
|     13 | 2648 | `		ph7_result_bool(pCtx,1);` |
|     53 | 2649 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 2650 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2651 | `	}else{` |
|     58 | 2652 | `		ph7_result_bool(pCtx,` |
|     38 | 2653 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 2654 | `	}` |
|     59 | 2655 | `	rc = PH7_OK;` |
|     34 | 2656 | `out:` |
|     70 | 2657 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 2658 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 2659 | `	return rc;` |
|     43 | 2660 | `}` |
|      - | 2661 | `/*` |
|      - | 2662 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2663 | ` *  Case-insensitive strpos.` |
|      - | 2664 | ` * Parameters` |
|      - | 2665 | ` *  $haystack` |
|      - | 2666 | ` *   The input string.` |
|      - | 2667 | ` * $needle` |
|      - | 2668 | ` *   Search pattern (must be a string).` |
|      - | 2669 | ` * $offset` |
|      - | 2670 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 2671 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 2672 | ` *   of haystack.` |
|      - | 2673 | ` * Return` |
|      - | 2674 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 2675 | ` */` |
|     16 | 2676 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2677 | `{` |
|     17 | 2678 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2679 | `	const char *zBlob,*zPattern;` |
|      - | 2680 | `	int nLen,nPatLen,nStart;` |
|      - | 2681 | `	sxu32 nOfft;` |
|      - | 2682 | `	sxi32 rc;` |
|     17 | 2683 | `	if( nArg < 2 ){` |
|      - | 2684 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2685 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2686 | `		return PH7_OK;` |
|      - | 2687 | `	}` |
|      - | 2688 | `	/* Extract the needle and the haystack */` |
|     17 | 2689 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 2690 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|     17 | 2691 | `	nOfft = 0; /* cc warning */` |
|     17 | 2692 | `	nStart = 0;` |
|      - | 2693 | `	/* Peek the starting offset if available */` |
|     17 | 2694 | `	if( nArg > 2 ){` |
|      5 | 2695 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 2696 | `		if( nStart < 0 ){` |
|      3 | 2697 | `			nStart = -nStart;` |
|      1 | 2698 | `		}` |
|      5 | 2699 | `		if( nStart >= nLen ){` |
|      - | 2700 | `			/* Invalid offset */` |
|    ! 0 | 2701 | `			nStart = 0;` |
|    ! 0 | 2702 | `		}else{` |
|      5 | 2703 | `			zBlob += nStart;` |
|      5 | 2704 | `			nLen -= nStart;` |
|      - | 2705 | `		}` |
|      2 | 2706 | `	}` |
|     17 | 2707 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2708 | `		/* Perform the lookup */` |
|     17 | 2709 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|     17 | 2710 | `		if( rc != SXRET_OK ){` |
|      - | 2711 | `			/* Pattern not found,return FALSE */` |
|      3 | 2712 | `			ph7_result_bool(pCtx,0);` |
|      3 | 2713 | `			return PH7_OK;` |
|      - | 2714 | `		}` |
|      - | 2715 | `		/* Return the pattern position */` |
|     15 | 2716 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 2717 | `	}else{` |
|    ! 0 | 2718 | `		ph7_result_bool(pCtx,0);` |
|      - | 2719 | `	}` |
|     15 | 2720 | `	return PH7_OK;` |
|      9 | 2721 | `}` |
|      - | 2722 | `/*` |
|      - | 2723 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2724 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 2725 | ` * Parameters` |
|      - | 2726 | ` *  $haystack` |
|      - | 2727 | ` *   The input string.` |
|      - | 2728 | ` * $needle` |
|      - | 2729 | ` *   Search pattern (must be a string).` |
|      - | 2730 | ` * $offset` |
|      - | 2731 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 2732 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 2733 | ` *   characters from the end of the string, searching backwards.` |
|      - | 2734 | ` * Return` |
|      - | 2735 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 2736 | ` */` |
|     30 | 2737 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2738 | `{` |
|      - | 2739 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     31 | 2740 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 2741 | `	int nLen,nPatLen;` |
|      - | 2742 | `	sxu32 nOfft;` |
|      - | 2743 | `	sxi32 rc;` |
|     31 | 2744 | `	if( nArg < 2 ){` |
|      - | 2745 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2746 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2747 | `		return PH7_OK;` |
|      - | 2748 | `	}` |
|      - | 2749 | `	/* Extract the needle and the haystack */` |
|     31 | 2750 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 2751 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 2752 | `	/* Point to the end of the pattern */` |
|     31 | 2753 | `	zPtr = &zBlob[nLen - 1];` |
|     31 | 2754 | `	zEnd = &zBlob[nLen];` |
|      - | 2755 | `	/* Save the starting posistion */` |
|     31 | 2756 | `	zStart = zBlob;` |
|     31 | 2757 | `	nOfft = 0; /* cc warning */` |
|      - | 2758 | `	/* Peek the starting offset if available */` |
|     31 | 2759 | `	if( nArg > 2 ){` |
|      - | 2760 | `		int nStart;` |
|     21 | 2761 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 2762 | `		if( nStart < 0 ){` |
|     11 | 2763 | `			nStart = -nStart;` |
|     11 | 2764 | `			if( nStart >= nLen ){` |
|      - | 2765 | `				/* Invalid offset */` |
|      3 | 2766 | `				ph7_result_bool(pCtx,0);` |
|      3 | 2767 | `				return PH7_OK;` |
|    ! 0 | 2768 | `			}else{` |
|      9 | 2769 | `				nLen -= nStart;` |
|      9 | 2770 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 2771 | `				zEnd = &zBlob[nLen];` |
|      - | 2772 | `			}` |
|      5 | 2773 | `		}else{` |
|     11 | 2774 | `			if( nStart >= nLen ){` |
|      - | 2775 | `				/* Invalid offset */` |
|      5 | 2776 | `				ph7_result_bool(pCtx,0);` |
|      5 | 2777 | `				return PH7_OK;` |
|    ! 0 | 2778 | `			}else{` |
|      7 | 2779 | `				zBlob += nStart;` |
|      7 | 2780 | `				nLen -= nStart;` |
|      - | 2781 | `			}` |
|      - | 2782 | `		}` |
|      7 | 2783 | `	}` |
|     25 | 2784 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2785 | `		/* Perform the lookup */` |
|     57 | 2786 | `		for(;;){` |
|    115 | 2787 | `			if( zBlob >= zPtr ){` |
|     11 | 2788 | `				break;` |
|      - | 2789 | `			}` |
|    105 | 2790 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    105 | 2791 | `			if( rc == SXRET_OK ){` |
|      - | 2792 | `				/* Pattern found,return it's position */` |
|     13 | 2793 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 2794 | `				return PH7_OK;` |
|      - | 2795 | `			}` |
|     93 | 2796 | `			zPtr--;` |
|      1 | 2797 | `		}` |
|      - | 2798 | `		/* Pattern not found,return FALSE */` |
|     11 | 2799 | `		ph7_result_bool(pCtx,0);` |
|      6 | 2800 | `	}else{` |
|      3 | 2801 | `		ph7_result_bool(pCtx,0);` |
|      - | 2802 | `	}` |
|     13 | 2803 | `	return PH7_OK;` |
|     16 | 2804 | `}` |
|      - | 2805 | `/*` |
|      - | 2806 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 2807 | ` *  Case-insensitive strrpos.` |
|      - | 2808 | ` * Parameters` |
|      - | 2809 | ` *  $haystack` |
|      - | 2810 | ` *   The input string.` |
|      - | 2811 | ` * $needle` |
|      - | 2812 | ` *   Search pattern (must be a string).` |
|      - | 2813 | ` * $offset` |
|      - | 2814 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 2815 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 2816 | ` *   characters from the end of the string, searching backwards.` |
|      - | 2817 | ` * Return` |
|      - | 2818 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 2819 | ` */` |
|     26 | 2820 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2821 | `{` |
|      - | 2822 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     27 | 2823 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 2824 | `	int nLen,nPatLen;` |
|      - | 2825 | `	sxu32 nOfft;` |
|      - | 2826 | `	sxi32 rc;` |
|     27 | 2827 | `	if( nArg < 2 ){` |
|      - | 2828 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2829 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2830 | `		return PH7_OK;` |
|      - | 2831 | `	}` |
|      - | 2832 | `	/* Extract the needle and the haystack */` |
|     27 | 2833 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 2834 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 2835 | `	/* Point to the end of the pattern */` |
|     27 | 2836 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 2837 | `	zEnd = &zBlob[nLen];` |
|      - | 2838 | `	/* Save the starting posistion */` |
|     27 | 2839 | `	zStart = zBlob;` |
|     27 | 2840 | `	nOfft = 0; /* cc warning */` |
|      - | 2841 | `	/* Peek the starting offset if available */` |
|     27 | 2842 | `	if( nArg > 2 ){` |
|      - | 2843 | `		int nStart;` |
|     15 | 2844 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 2845 | `		if( nStart < 0 ){` |
|      7 | 2846 | `			nStart = -nStart;` |
|      7 | 2847 | `			if( nStart >= nLen ){` |
|      - | 2848 | `				/* Invalid offset */` |
|      3 | 2849 | `				ph7_result_bool(pCtx,0);` |
|      3 | 2850 | `				return PH7_OK;` |
|    ! 0 | 2851 | `			}else{` |
|      5 | 2852 | `				nLen -= nStart;` |
|      5 | 2853 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 2854 | `				zEnd = &zBlob[nLen];` |
|      - | 2855 | `			}` |
|      3 | 2856 | `		}else{` |
|      9 | 2857 | `			if( nStart >= nLen ){` |
|      - | 2858 | `				/* Invalid offset */` |
|      5 | 2859 | `				ph7_result_bool(pCtx,0);` |
|      5 | 2860 | `				return PH7_OK;` |
|    ! 0 | 2861 | `			}else{` |
|      5 | 2862 | `				zBlob += nStart;` |
|      5 | 2863 | `				nLen -= nStart;` |
|      - | 2864 | `			}` |
|      - | 2865 | `		}` |
|      4 | 2866 | `	}` |
|     21 | 2867 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 2868 | `		/* Perform the lookup */` |
|     44 | 2869 | `		for(;;){` |
|     89 | 2870 | `			if( zBlob >= zPtr ){` |
|      9 | 2871 | `				break;` |
|      - | 2872 | `			}` |
|     81 | 2873 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 2874 | `			if( rc == SXRET_OK ){` |
|      - | 2875 | `				/* Pattern found,return it's position */` |
|     11 | 2876 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 2877 | `				return PH7_OK;` |
|      - | 2878 | `			}` |
|     71 | 2879 | `			zPtr--;` |
|      1 | 2880 | `		}` |
|      - | 2881 | `		/* Pattern not found,return FALSE */` |
|      9 | 2882 | `		ph7_result_bool(pCtx,0);` |
|      5 | 2883 | `	}else{` |
|      3 | 2884 | `		ph7_result_bool(pCtx,0);` |
|      - | 2885 | `	}` |
|     11 | 2886 | `	return PH7_OK;` |
|     14 | 2887 | `}` |
|      - | 2888 | `/*` |
|      - | 2889 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 2890 | ` *  Find the last occurrence of a character in a string.` |
|      - | 2891 | ` * Parameters` |
|      - | 2892 | ` *  $haystack` |
|      - | 2893 | ` *   The input string.` |
|      - | 2894 | ` * $needle` |
|      - | 2895 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 2896 | ` *  This behavior is different from that of strstr().` |
|      - | 2897 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 2898 | ` *  as the ordinal value of a character.` |
|      - | 2899 | ` * Return` |
|      - | 2900 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 2901 | ` */` |
|     22 | 2902 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2903 | `{` |
|      - | 2904 | `	const char *zBlob;` |
|      - | 2905 | `	int nLen,c;` |
|     23 | 2906 | `	if( nArg < 2 ){` |
|      - | 2907 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2908 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2909 | `		return PH7_OK;` |
|      - | 2910 | `	}` |
|      - | 2911 | `	/* Extract the haystack */` |
|     23 | 2912 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 2913 | `	c = 0; /* cc warning */` |
|     23 | 2914 | `	if( nLen > 0 ){` |
|      - | 2915 | `		sxu32 nOfft;` |
|      - | 2916 | `		sxi32 rc;` |
|     21 | 2917 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 2918 | `			const char *zPattern;` |
|     11 | 2919 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 2920 | `														 * for NULL pointer.` |
|      - | 2921 | `														 */` |
|     11 | 2922 | `			c = zPattern[0];` |
|      6 | 2923 | `		}else{` |
|      - | 2924 | `			/* Int cast */` |
|     11 | 2925 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 2926 | `		}` |
|      - | 2927 | `		/* Perform the lookup */` |
|     21 | 2928 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 2929 | `		if( rc != SXRET_OK ){` |
|      - | 2930 | `			/* No such entry,return FALSE */` |
|      7 | 2931 | `			ph7_result_bool(pCtx,0);` |
|      7 | 2932 | `			return PH7_OK;` |
|      - | 2933 | `		}` |
|      - | 2934 | `		/* Return the string portion */` |
|     15 | 2935 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 2936 | `	}else{` |
|      3 | 2937 | `		ph7_result_bool(pCtx,0);` |
|      - | 2938 | `	}` |
|     17 | 2939 | `	return PH7_OK;` |
|     12 | 2940 | `}` |
|      - | 2941 | `/*` |
|      - | 2942 | ` * string strrev(string $string)` |
|      - | 2943 | ` *  Reverse a string.` |
|      - | 2944 | ` * Parameters` |
|      - | 2945 | ` *  $string` |
|      - | 2946 | ` *   String to be reversed.` |
|      - | 2947 | ` * Return` |
|      - | 2948 | ` *  The reversed string.` |
|      - | 2949 | ` */` |
|      2 | 2950 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2951 | `{` |
|      - | 2952 | `	const char *zIn,*zEnd;` |
|      - | 2953 | `	int nLen,c;` |
|      3 | 2954 | `	if( nArg < 1 ){` |
|      - | 2955 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 2956 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2957 | `		return PH7_OK;` |
|      - | 2958 | `	}` |
|      - | 2959 | `	/* Extract the target string */` |
|      3 | 2960 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 2961 | `	if( nLen < 1 ){` |
|      - | 2962 | `		/* Empty string Return null */` |
|    ! 0 | 2963 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2964 | `		return PH7_OK;` |
|      - | 2965 | `	}` |
|      - | 2966 | `	/* Perform the requested operation */` |
|      3 | 2967 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 2968 | `	for(;;){` |
|      9 | 2969 | `		if( zEnd < zIn ){` |
|      - | 2970 | `			/* No more input to process */` |
|      3 | 2971 | `			break;` |
|      - | 2972 | `		}` |
|      - | 2973 | `		/* Append current character */` |
|      7 | 2974 | `		c = zEnd[0];` |
|      7 | 2975 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 2976 | `		zEnd--;` |
|      1 | 2977 | `	}` |
|      3 | 2978 | `	return PH7_OK;` |
|      2 | 2979 | `}` |
|      - | 2980 | `/*` |
|      - | 2981 | ` * string ucwords(string $string)` |
|      - | 2982 | ` *  Uppercase the first character of each word in a string.` |
|      - | 2983 | ` *  The definition of a word is any string of characters that is immediately after` |
|      - | 2984 | ` *  a whitespace (These are: space, form-feed, newline, carriage return, horizontal tab, and vertical tab).` |
|      - | 2985 | ` * Parameters` |
|      - | 2986 | ` *  $string` |
|      - | 2987 | ` *   The input string.` |
|      - | 2988 | ` * Return` |
|      - | 2989 | ` *  The modified string..` |
|      - | 2990 | ` */` |
|     12 | 2991 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2992 | `{` |
|      - | 2993 | `	const char *zIn,*zCur,*zEnd;` |
|      - | 2994 | `	int nLen,c;` |
|     13 | 2995 | `	if( nArg < 1 ){` |
|      - | 2996 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 2997 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2998 | `		return PH7_OK;` |
|      - | 2999 | `	}` |
|      - | 3000 | `	/* Extract the target string */` |
|     13 | 3001 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 3002 | `	if( nLen < 1 ){` |
|      - | 3003 | `		/* Empty string – match PHP semantics */` |
|      3 | 3004 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3005 | `		return PH7_OK;` |
|      - | 3006 | `	}` |
|      - | 3007 | `	/* Perform the requested operation */` |
|     11 | 3008 | `	zEnd = &zIn[nLen];` |
|     21 | 3009 | `	for(;;){` |
|      - | 3010 | `		/* Jump leading white spaces */` |
|     43 | 3011 | `		zCur = zIn;` |
|     65 | 3012 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0x80 && SyisSpace(zIn[0]) ){` |
|     23 | 3013 | `			zIn++;` |
|      1 | 3014 | `		}` |
|     43 | 3015 | `		if( zCur < zIn ){` |
|      - | 3016 | `			/* Append white space stream */` |
|     23 | 3017 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 3018 | `		}` |
|     43 | 3019 | `		if( zIn >= zEnd ){` |
|      - | 3020 | `			/* No more input to process */` |
|     11 | 3021 | `			break;` |
|      - | 3022 | `		}` |
|     33 | 3023 | `		c = zIn[0];` |
|     33 | 3024 | `		if( c < 0x80 && SyisLower(c) ){` |
|     29 | 3025 | `			c = SyToUpper(c);` |
|     14 | 3026 | `		}` |
|      - | 3027 | `		/* Append the upper-cased character */` |
|     33 | 3028 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     33 | 3029 | `		zIn++;` |
|     33 | 3030 | `		zCur = zIn;` |
|      - | 3031 | `		/* Append the word varbatim */` |
|    149 | 3032 | `		while( zIn < zEnd ){` |
|    139 | 3033 | `			if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 3034 | `				/* UTF-8 stream */` |
|    ! 0 | 3035 | `				zIn++;` |
|    ! 0 | 3036 | `				SX_JMP_UTF8(zIn,zEnd);` |
|    138 | 3037 | `			}else if( !SyisSpace(zIn[0]) ){` |
|    117 | 3038 | `				zIn++;` |
|     59 | 3039 | `			}else{` |
|     23 | 3040 | `				break;` |
|      - | 3041 | `			}` |
|      1 | 3042 | `		}` |
|     33 | 3043 | `		if( zCur < zIn ){` |
|     33 | 3044 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     16 | 3045 | `		}` |
|      1 | 3046 | `	}` |
|     11 | 3047 | `	return PH7_OK;` |
|      7 | 3048 | `}` |
|      - | 3049 | `/*` |
|      - | 3050 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3051 | ` *  Returns input repeated multiplier times.` |
|      - | 3052 | ` * Parameters` |
|      - | 3053 | ` *  $string` |
|      - | 3054 | ` *   String to be repeated.` |
|      - | 3055 | ` * $multiplier` |
|      - | 3056 | ` *  Number of time the input string should be repeated.` |
|      - | 3057 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3058 | ` *  to 0, the function will return an empty string.` |
|      - | 3059 | ` * Return` |
|      - | 3060 | ` *  The repeated string.` |
|      - | 3061 | ` */` |
|  20426 | 3062 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3063 | `{` |
|      - | 3064 | `	const char *zIn;` |
|      - | 3065 | `	int nLen;` |
|      - | 3066 | `	ph7_int64 nMul;` |
|      - | 3067 | `	int rc;` |
|  20428 | 3068 | `	if( nArg < 2 ){` |
|      - | 3069 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3070 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3071 | `		return PH7_OK;` |
|      - | 3072 | `	}` |
|      - | 3073 | `	/* Extract the target string */` |
|  20428 | 3074 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3075 | `	/* Extract the multiplier as a 64-bit value (a 32-bit read would wrap a large` |
|      - | 3076 | `	 * positive $times into a negative one and trip a spurious ValueError). PHP` |
|      - | 3077 | `	 * validates $times regardless of the string contents: a negative count throws` |
|      - | 3078 | `	 * a catchable ValueError. */` |
|  20428 | 3079 | `	nMul = ph7_value_to_int64(apArg[1]);` |
|  20428 | 3080 | `	if( nMul < 0 ){` |
|      3 | 3081 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 3082 | `			"str_repeat(): Argument #2 ($times) must be greater than or equal to 0");` |
|      - | 3083 | `	}` |
|  20426 | 3084 | `	if( nLen < 1 \|\| nMul < 1 ){` |
|      - | 3085 | `		/* Empty input or a zero multiplier yields the empty string (PHP). */` |
|    ! 0 | 3086 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3087 | `		return PH7_OK;` |
|      - | 3088 | `	}` |
|      - | 3089 | `	/* Perform the requested operation */` |
| 221499 | 3090 | `	for(;;){` |
| 443000 | 3091 | `		if( !nMul ){` |
|  20426 | 3092 | `			break;` |
|      - | 3093 | `		}` |
|      - | 3094 | `		/* Append the copy */` |
| 422576 | 3095 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 422576 | 3096 | `		if( rc != PH7_OK ){` |
|      - | 3097 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 3098 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 3099 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 3100 | `		}` |
| 422576 | 3101 | `		nMul--;` |
|      2 | 3102 | `	}` |
|  20426 | 3103 | `	return PH7_OK;` |
|  10215 | 3104 | `}` |
|      - | 3105 | `/*` |
|      - | 3106 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3107 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3108 | ` * Parameters` |
|      - | 3109 | ` *  $string` |
|      - | 3110 | ` *   The input string.` |
|      - | 3111 | ` * $is_xhtml` |
|      - | 3112 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3113 | ` * Return` |
|      - | 3114 | ` *  The processed string.` |
|      - | 3115 | ` */` |
|      4 | 3116 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3117 | `{` |
|      - | 3118 | `	const char *zIn,*zCur,*zEnd;` |
|      5 | 3119 | `	int is_xhtml = 1; /* Default to XHTML-style '<br />' like PHP */` |
|      - | 3120 | `	int nLen;` |
|      5 | 3121 | `	if( nArg < 1 ){` |
|      - | 3122 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 3123 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3124 | `		return PH7_OK;` |
|      - | 3125 | `	}` |
|      - | 3126 | `	/* Extract the target string */` |
|      5 | 3127 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3128 | `	if( nLen < 1 ){` |
|      - | 3129 | `		/* Empty string,return null */` |
|    ! 0 | 3130 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3131 | `		return PH7_OK;` |
|      - | 3132 | `	}` |
|      5 | 3133 | `	if( nArg > 1 ){` |
|      3 | 3134 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3135 | `	}` |
|      5 | 3136 | `	zEnd = &zIn[nLen];` |
|      - | 3137 | `	/* Perform the requested operation */` |
|      4 | 3138 | `	for(;;){` |
|      9 | 3139 | `		zCur = zIn;` |
|      - | 3140 | `		/* Delimit the string */` |
|     21 | 3141 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3142 | `			zIn++;` |
|      1 | 3143 | `		}` |
|      9 | 3144 | `		if( zCur < zIn ){` |
|      - | 3145 | `			/* Output chunk verbatim */` |
|      9 | 3146 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3147 | `		}` |
|      9 | 3148 | `		if( zIn >= zEnd ){` |
|      - | 3149 | `			/* No more input to process */` |
|      5 | 3150 | `			break;` |
|      - | 3151 | `		}` |
|      - | 3152 | `		/* Output the HTML line break */` |
|      - | 3153 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br />' (space before the slash), otherwise use '<br>' */` |
|      5 | 3154 | `		if( is_xhtml ){` |
|      3 | 3155 | `			ph7_result_string(pCtx,"<br />",(int)sizeof("<br />")-1);` |
|      2 | 3156 | `		}else{` |
|      3 | 3157 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3158 | `		}` |
|      5 | 3159 | `		zCur = zIn;` |
|      - | 3160 | `		/* Append trailing line */` |
|     11 | 3161 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3162 | `			zIn++;` |
|      1 | 3163 | `		}` |
|      5 | 3164 | `		if( zCur < zIn ){` |
|      - | 3165 | `			/* Output chunk verbatim */` |
|      5 | 3166 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3167 | `		}` |
|      1 | 3168 | `	}` |
|      5 | 3169 | `	return PH7_OK;` |
|      3 | 3170 | `}` |
|      - | 3171 | `/*` |
|      - | 3172 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3173 | ` *  According to the PHP reference manual.` |
|      - | 3174 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3175 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3176 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3177 | ` * This applies to both sprintf() and printf().` |
|      - | 3178 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3179 | ` * or more of these elements, in order:` |
|      - | 3180 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3181 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3182 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 3183 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 3184 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 3185 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 3186 | ` *   it with a single quote ('). See the examples below.` |
|      - | 3187 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 3188 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 3189 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 3190 | ` *   should result in.` |
|      - | 3191 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 3192 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 3193 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 3194 | ` *   limit to the string.` |
|      - | 3195 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 3196 | ` *       % - a literal percent character. No argument is required.` |
|      - | 3197 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 3198 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 3199 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 3200 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 3201 | ` * 	     for the number of digits after the decimal point.` |
|      - | 3202 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 3203 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 3204 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 3205 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 3206 | ` *       g - shorter of %e and %f.` |
|      - | 3207 | ` *       G - shorter of %E and %f.` |
|      - | 3208 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 3209 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 3210 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 3211 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 3212 | ` */` |
|      - | 3213 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3214 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 3215 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 3216 | `/*` |
|      - | 3217 | `** Conversion types fall into various categories as defined by the` |
|      - | 3218 | `** following enumeration.` |
|      - | 3219 | `*/` |
|      - | 3220 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 3221 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 3222 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 3223 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 3224 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 3225 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 3226 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 3227 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 3228 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 3229 |  |
|      - | 3230 | `/*` |
|      - | 3231 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 3232 | `*/` |
|      - | 3233 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 3234 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 3235 | `/*` |
|      - | 3236 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 3237 | `** by an instance of the following structure` |
|      - | 3238 | `*/` |
|      - | 3239 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 3240 | `struct ph7_fmt_info` |
|      - | 3241 | `{` |
|      - | 3242 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 3243 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 3244 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 3245 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 3246 | `  char *charset; /* The character set for conversion */` |
|      - | 3247 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 3248 | `};` |
|      - | 3249 | `/* PH7_PhpFloatShape (php's float-shape post-processing) lives in memobj.c —` |
|      - | 3250 | ` * the default float->string cast needs it even when this whole formatting` |
|      - | 3251 | ` * region is compiled out by PH7_DISABLE_DISK_IO. */` |
|      - | 3252 | `/*` |
|      - | 3253 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 3254 | ` * used conversion types first.` |
|      - | 3255 | ` */` |
|      - | 3256 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 3257 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 3258 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 3259 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 3260 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 3261 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 3262 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 3263 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 3264 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 3265 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3266 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3267 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 3268 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 3269 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 3270 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 3271 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 3272 | `};` |
|      - | 3273 | `/*` |
|      - | 3274 | ` * Format a given string.` |
|      - | 3275 | ` * The root program.  All variations call this core.` |
|      - | 3276 | ` * INPUTS:` |
|      - | 3277 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 3278 | ` *            1. A pointer to the call context.` |
|      - | 3279 | ` *            2. A pointer to the list of characters to be output` |
|      - | 3280 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 3281 | ` *            3. An integer number of characters to be output.` |
|      - | 3282 | ` *               (Note: This number might be zero.)` |
|      - | 3283 | ` *            4. Upper layer private data.` |
|      - | 3284 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 3285 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 3286 | ` */` |
|    298 | 3287 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 3288 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 3289 | `	ph7_context *pCtx,  /* call context */` |
|      - | 3290 | `	const char *zIn,    /* Format string */` |
|      - | 3291 | `	int nByte,          /* Format string length */` |
|      - | 3292 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 3293 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 3294 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 3295 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 3296 | `	)` |
|      1 | 3297 | `{` |
|    299 | 3298 | `	char spaces[] = "                                                  ";` |
|      - | 3299 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    299 | 3300 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 3301 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 3302 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 3303 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 3304 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 3305 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 3306 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 3307 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 3308 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 3309 | `	ph7_int64 iVal;` |
|      - | 3310 | `	int precision;           /* Precision of the current field */` |
|      - | 3311 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 3312 | `	int c,rc,n;` |
|      - | 3313 | `	int length;              /* Length of the field */` |
|      - | 3314 | `	int prefix;` |
|      - | 3315 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 3316 | `	int width;               /* Width of the current field */` |
|      - | 3317 | `	int idx;` |
|    299 | 3318 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 3319 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 3320 | `	/* Start the format process */` |
|    458 | 3321 | `	for(;;){` |
|    917 | 3322 | `		zCur = zIn;` |
|   3115 | 3323 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|   2199 | 3324 | `			zIn++;` |
|      1 | 3325 | `		}` |
|    917 | 3326 | `		if( zCur < zIn ){` |
|      - | 3327 | `			/* Consume chunk verbatim */` |
|    657 | 3328 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|    657 | 3329 | `			if( rc != SXRET_OK ){` |
|      - | 3330 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|    ! 0 | 3331 | `				break;` |
|      - | 3332 | `			}` |
|    328 | 3333 | `		}` |
|    917 | 3334 | `		if( zIn >= zEnd ){` |
|      - | 3335 | `			/* No more input to process,break immediately */` |
|    297 | 3336 | `			break;` |
|      - | 3337 | `		}` |
|      - | 3338 | `		/* Find out what flags are present */` |
|    621 | 3339 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    620 | 3340 | `			flag_alternateform = flag_zeropad = 0;` |
|    621 | 3341 | `		zIn++; /* Jump the precent sign */` |
|    310 | 3342 | `		do{` |
|    671 | 3343 | `			c = zIn[0];` |
|    671 | 3344 | `			switch( c ){` |
|     15 | 3345 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      7 | 3346 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 3347 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|      9 | 3348 | `			case '#':   flag_alternateform = 1;   c = 0;   break;` |
|     17 | 3349 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 3350 | `			case '\'':` |
|    ! 0 | 3351 | `				zIn++;` |
|    ! 0 | 3352 | `				if( zIn < zEnd ){` |
|      - | 3353 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 3354 | `					c = zIn[0];` |
|    ! 0 | 3355 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3356 | `						spaces[idx] = (char)c;` |
|    ! 0 | 3357 | `					}` |
|    ! 0 | 3358 | `					c = 0;` |
|    ! 0 | 3359 | `				}` |
|    ! 0 | 3360 | `				break;` |
|    620 | 3361 | `			default:                                       break;` |
|      - | 3362 | `			}` |
|    671 | 3363 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 3364 | `		/* Get the field width */` |
|    621 | 3365 | `		width = 0;` |
|   1003 | 3366 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     73 | 3367 | `			width = width*10 + (zIn[0] - '0');` |
|     73 | 3368 | `			zIn++;` |
|      1 | 3369 | `		}` |
|    621 | 3370 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 3371 | `			/* Position specifer */` |
|    ! 0 | 3372 | `			if( width > 0 ){` |
|    ! 0 | 3373 | `				n = width;` |
|    ! 0 | 3374 | `				if( vf && n > 0 ){` |
|    ! 0 | 3375 | `					n--;` |
|    ! 0 | 3376 | `				}` |
|    ! 0 | 3377 | `			}` |
|    ! 0 | 3378 | `			zIn++;` |
|    ! 0 | 3379 | `			width = 0;` |
|    ! 0 | 3380 | `			if( zIn < zEnd && zIn[0] == '0' ){` |
|    ! 0 | 3381 | `				flag_zeropad = 1;` |
|    ! 0 | 3382 | `				zIn++;` |
|    ! 0 | 3383 | `			}` |
|    ! 0 | 3384 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 3385 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 3386 | `				zIn++;` |
|    ! 0 | 3387 | `			}` |
|    ! 0 | 3388 | `		}` |
|    621 | 3389 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 3390 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 3391 | `		}` |
|      - | 3392 | `		/* Get the precision */` |
|    621 | 3393 | `		precision = -1;` |
|    621 | 3394 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     85 | 3395 | `			precision = 0;` |
|     85 | 3396 | `			zIn++;` |
|    221 | 3397 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     95 | 3398 | `				precision = precision*10 + (zIn[0] - '0');` |
|     95 | 3399 | `				zIn++;` |
|      1 | 3400 | `			}` |
|     42 | 3401 | `		}` |
|    621 | 3402 | `		if( zIn >= zEnd ){` |
|      - | 3403 | `			/* No more input */` |
|      3 | 3404 | `			break;` |
|      - | 3405 | `		}` |
|      - | 3406 | `		/* Fetch the info entry for the field */` |
|    619 | 3407 | `		pInfo = 0;` |
|    619 | 3408 | `		xtype = PH7_FMT_ERROR;` |
|    619 | 3409 | `		c = zIn[0];` |
|    619 | 3410 | `		zIn++; /* Jump the format specifer */` |
|   2785 | 3411 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|   2783 | 3412 | `			if( c==aFmt[idx].fmttype ){` |
|    617 | 3413 | `				pInfo = &aFmt[idx];` |
|    617 | 3414 | `				xtype = pInfo->type;` |
|    617 | 3415 | `				break;` |
|      - | 3416 | `			}` |
|   1084 | 3417 | `		}` |
|    619 | 3418 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    619 | 3419 | `		length = 0;` |
|      - | 3420 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 3421 | `		 /*` |
|      - | 3422 | `		  ** At this point, variables are initialized as follows:` |
|      - | 3423 | `		  **` |
|      - | 3424 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 3425 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 3426 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 3427 | `		  **                               field width was negative.` |
|      - | 3428 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 3429 | `		  **                               the conversion character.` |
|      - | 3430 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 3431 | `		  **   width                       The specified field width.  This is` |
|      - | 3432 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 3433 | `		  **   precision                   The specified precision.  The default` |
|      - | 3434 | `		  **                               is -1.` |
|      - | 3435 | `		  */` |
|    619 | 3436 | `		switch(xtype){` |
|      3 | 3437 | `		case PH7_FMT_PERCENT:` |
|      - | 3438 | `			/* A literal percent character */` |
|      7 | 3439 | `			zWorker[0] = '%';` |
|      7 | 3440 | `			length = (int)sizeof(char);` |
|      7 | 3441 | `			break;` |
|      3 | 3442 | `		case PH7_FMT_CHARX:` |
|      - | 3443 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 3444 | `			 * with that ASCII value` |
|      - | 3445 | `			 */` |
|      7 | 3446 | `			pArg = NEXT_ARG;` |
|      7 | 3447 | `			if( pArg == 0 ){` |
|      3 | 3448 | `				c = 0;` |
|      2 | 3449 | `			}else{` |
|      5 | 3450 | `				c = ph7_value_to_int(pArg);` |
|      - | 3451 | `			}` |
|      - | 3452 | `			/* NUL byte is an acceptable value */` |
|      7 | 3453 | `			zWorker[0] = (char)c;` |
|      7 | 3454 | `			length = (int)sizeof(char);` |
|      7 | 3455 | `			break;` |
|    159 | 3456 | `		case PH7_FMT_STRING:` |
|      - | 3457 | `			/* the argument is treated as and presented as a string */` |
|    319 | 3458 | `			pArg = NEXT_ARG;` |
|    319 | 3459 | `			if( pArg == 0 ){` |
|    ! 0 | 3460 | `				length = 0;` |
|    ! 0 | 3461 | `			}else{` |
|    319 | 3462 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 3463 | `			}` |
|    319 | 3464 | `			if( length < 1 ){` |
|    ! 0 | 3465 | `				zBuf = " ";` |
|    ! 0 | 3466 | `				length = (int)sizeof(char);` |
|    ! 0 | 3467 | `			}` |
|    319 | 3468 | `			if( precision>=0 && precision<length ){` |
|      3 | 3469 | `				length = precision;` |
|      1 | 3470 | `			}` |
|    319 | 3471 | `			if( flag_zeropad ){` |
|      - | 3472 | `				/* zero-padding works on strings too */` |
|    ! 0 | 3473 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3474 | `					spaces[idx] = '0';` |
|    ! 0 | 3475 | `				}` |
|    ! 0 | 3476 | `			}` |
|    319 | 3477 | `			break;` |
|     59 | 3478 | `		case PH7_FMT_RADIX:` |
|    119 | 3479 | `			pArg = NEXT_ARG;` |
|    119 | 3480 | `			if( pArg == 0 ){` |
|    ! 0 | 3481 | `				iVal = 0;` |
|    ! 0 | 3482 | `			}else{` |
|    119 | 3483 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 3484 | `			}` |
|      - | 3485 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|    119 | 3486 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 3487 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 3488 | `			}` |
|      - | 3489 | `#if 1` |
|      - | 3490 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 3491 | `        ** I think this is stupid.*/` |
|    119 | 3492 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 3493 | `#else` |
|      - | 3494 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 3495 | `        ** but leave the prefix for hex.*/` |
|      - | 3496 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 3497 | `#endif` |
|    119 | 3498 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|     89 | 3499 | `          if( iVal<0 ){` |
|     25 | 3500 | `            iVal = -iVal;` |
|      - | 3501 | `			/* Ticket 1433-003 */` |
|     25 | 3502 | `			if( iVal < 0 ){` |
|      - | 3503 | `				/* Overflow */` |
|    ! 0 | 3504 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3505 | `			}` |
|     25 | 3506 | `            prefix = '-';` |
|     77 | 3507 | `          }else if( flag_plussign )  prefix = '+';` |
|     63 | 3508 | `          else if( flag_blanksign )  prefix = ' ';` |
|     61 | 3509 | `          else                       prefix = 0;` |
|     45 | 3510 | `        }else{` |
|     31 | 3511 | `			if( iVal<0 ){` |
|    ! 0 | 3512 | `				iVal = -iVal;` |
|      - | 3513 | `				/* Ticket 1433-003 */` |
|    ! 0 | 3514 | `				if( iVal < 0 ){` |
|      - | 3515 | `					/* Overflow */` |
|    ! 0 | 3516 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3517 | `				}` |
|    ! 0 | 3518 | `			}` |
|     31 | 3519 | `			prefix = 0;` |
|      - | 3520 | `		}` |
|    119 | 3521 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      7 | 3522 | `          precision = width-(prefix!=0);` |
|      3 | 3523 | `        }` |
|    119 | 3524 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 3525 | `        {` |
|      - | 3526 | `          register char *cset;      /* Use registers for speed */` |
|      - | 3527 | `          register int base;` |
|    119 | 3528 | `          cset = pInfo->charset;` |
|    119 | 3529 | `          base = pInfo->base;` |
|     59 | 3530 | `          do{                                           /* Convert to ascii */` |
|    187 | 3531 | `            *(--zBuf) = cset[iVal%base];` |
|    187 | 3532 | `            iVal = iVal/base;` |
|    187 | 3533 | `          }while( iVal>0 );` |
|      - | 3534 | `        }` |
|    119 | 3535 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    141 | 3536 | `        for(idx=precision-length; idx>0; idx--){` |
|     23 | 3537 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|     12 | 3538 | `        }` |
|    119 | 3539 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|    119 | 3540 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 3541 | `          char *pre, x;` |
|      9 | 3542 | `          pre = pInfo->prefix;` |
|      9 | 3543 | `          if( *zBuf!=pre[0] ){` |
|     23 | 3544 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|      4 | 3545 | `          }` |
|      4 | 3546 | `        }` |
|    119 | 3547 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    119 | 3548 | `		break;` |
|     84 | 3549 | `		case PH7_FMT_FLOAT:` |
|      - | 3550 | `		case PH7_FMT_EXP:` |
|      - | 3551 | `		case PH7_FMT_GENERIC:{` |
|      - | 3552 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3553 | `		double realvalue;` |
|      - | 3554 | `		char zFmt[8];` |
|      - | 3555 | `		int nOut, nFmt;` |
|    169 | 3556 | `		pArg = NEXT_ARG;` |
|    169 | 3557 | `		if( pArg == 0 ){` |
|    ! 0 | 3558 | `			realvalue = 0;` |
|    ! 0 | 3559 | `		}else{` |
|    169 | 3560 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 3561 | `		}` |
|      - | 3562 | `		/* php prints the IEEE specials bare — NaN / INF / -INF with no width` |
|      - | 3563 | `		 * padding, precision, or sign flags (php_sprintf_appenddouble). */` |
|    169 | 3564 | `		if( PH7_IS_NAN(realvalue) ){` |
|     21 | 3565 | `			zBuf = "NaN";` |
|     21 | 3566 | `			length = 3;` |
|     21 | 3567 | `			width = 0;` |
|     21 | 3568 | `			break;` |
|      - | 3569 | `		}` |
|    149 | 3570 | `		if( PH7_IS_INF(realvalue) ){` |
|     37 | 3571 | `			if( realvalue < 0.0 ){` |
|     15 | 3572 | `				zBuf = "-INF";` |
|     15 | 3573 | `				length = 4;` |
|      8 | 3574 | `			}else{` |
|     23 | 3575 | `				zBuf = "INF";` |
|     23 | 3576 | `				length = 3;` |
|      - | 3577 | `			}` |
|     37 | 3578 | `			width = 0;` |
|     37 | 3579 | `			break;` |
|      - | 3580 | `		}` |
|    113 | 3581 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|    113 | 3582 | `		if( precision > 53 ){` |
|      - | 3583 | `			/* php's FORMAT_CONV_MAX_PRECISION cap, with the same E_NOTICE` |
|      - | 3584 | `			 * (message prefixed with the active function's name, like` |
|      - | 3585 | `			 * php_error_docref). */` |
|      - | 3586 | `			char zMsg[160];` |
|      4 | 3587 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 3588 | `				"%z(): Requested precision of %d digits was truncated to PHP maximum of %d digits",` |
|      2 | 3589 | `				&pCtx->pFunc->sName,precision,53);` |
|      3 | 3590 | `			PH7_VmThrowError(pCtx->pVm,0,E_NOTICE,zMsg);` |
|      3 | 3591 | `			precision = 53;` |
|      1 | 3592 | `		}` |
|      - | 3593 | ``		/* php's %f/%e extract the sign via `num < 0`, so negative zero prints`` |
|      - | 3594 | `		 * unsigned there — while %g (php_gcvt on the raw value) keeps "-0". */` |
|    113 | 3595 | `		if( xtype!=PH7_FMT_GENERIC && realvalue == 0.0 ){` |
|      9 | 3596 | `			realvalue = 0.0;` |
|      4 | 3597 | `		}` |
|      - | 3598 | `		/* php's float conversions are correctly rounded (zend_dtoa); use libc` |
|      - | 3599 | `		 * snprintf as the digit engine (the byte-exact-floats rule — the old` |
|      - | 3600 | `		 * hand-rolled vxGetdigit loop stopped at 16 significant digits, so` |
|      - | 3601 | `		 * e.g. %f of 1e308 printed zeros where php prints the exact binary64` |
|      - | 3602 | `		 * expansion), then post-process into php's exact shapes below. */` |
|    113 | 3603 | `		nFmt = 0;` |
|    113 | 3604 | `		zFmt[nFmt++] = '%';` |
|    113 | 3605 | `		if( flag_alternateform ) zFmt[nFmt++] = '#';` |
|      - | 3606 | `		/* php's ' ' flag selects space PADDING (its default), not C's` |
|      - | 3607 | `		 * space-for-positive-sign — so flag_blanksign is NOT forwarded. */` |
|    113 | 3608 | `		if( flag_plussign ) zFmt[nFmt++] = '+';` |
|    113 | 3609 | `		zFmt[nFmt++] = '.';` |
|    113 | 3610 | `		zFmt[nFmt++] = '*';` |
|    151 | 3611 | `		zFmt[nFmt++] = (char)(xtype==PH7_FMT_FLOAT ? 'f' :` |
|     27 | 3612 | `			(xtype==PH7_FMT_EXP ? ((pInfo->charset[0]=='E') ? 'E' : 'e')` |
|     22 | 3613 | `			                    : ((pInfo->charset[0]=='E') ? 'G' : 'g')));` |
|    113 | 3614 | `		zFmt[nFmt] = 0;` |
|    113 | 3615 | `		nOut = snprintf(zWorker,sizeof(zWorker),zFmt,precision,realvalue);` |
|    113 | 3616 | `		if( nOut < 0 \|\| nOut >= (int)sizeof(zWorker) ){` |
|      - | 3617 | `			/* Cannot happen with precision capped at 53 (%f of DBL_MAX is` |
|      - | 3618 | `			 * ~365 bytes); keep the truncated output rather than overrun. */` |
|    ! 0 | 3619 | `			nOut = (int)SyStrlen(zWorker);` |
|    ! 0 | 3620 | `		}` |
|    113 | 3621 | `		nOut = (int)PH7_PhpFloatShape(zWorker,(sxi32)nOut,xtype==PH7_FMT_GENERIC);` |
|    113 | 3622 | `		zBuf = zWorker;` |
|    113 | 3623 | `		length = nOut;` |
|      - | 3624 | `		/* Let the zero-pad block below insert zeros between the sign (written` |
|      - | 3625 | `		 * by snprintf) and the first digit, as before. */` |
|    113 | 3626 | `		prefix = (zWorker[0]=='-' \|\| zWorker[0]=='+' \|\| zWorker[0]==' ') ? zWorker[0] : 0;` |
|      - | 3627 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 3628 | `        ** set and we are not left justified */` |
|    113 | 3629 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 3630 | `          int i;` |
|      7 | 3631 | `          int nPad = width - length;` |
|     51 | 3632 | `          for(i=width; i>=nPad; i--){` |
|     45 | 3633 | `            zBuf[i] = zBuf[i-nPad];` |
|     23 | 3634 | `          }` |
|      7 | 3635 | `          i = prefix!=0;` |
|     29 | 3636 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      7 | 3637 | `          length = width;` |
|      3 | 3638 | `        }` |
|      - | 3639 | `#else` |
|      - | 3640 | `         zBuf = " ";` |
|      - | 3641 | `		 length = (int)sizeof(char);` |
|      - | 3642 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|    113 | 3643 | `		 break;` |
|      - | 3644 | `							 }` |
|      1 | 3645 | `		default:` |
|      - | 3646 | `			/* Invalid format specifer */` |
|      3 | 3647 | `			zWorker[0] = '?';` |
|      3 | 3648 | `			length = (int)sizeof(char);` |
|      2 | 3649 | `			break;` |
|      - | 3650 | `		}` |
|      - | 3651 | `		 /*` |
|      - | 3652 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 3653 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 3654 | `		 ** the output.` |
|      - | 3655 | `		 */` |
|    619 | 3656 | `    if( !flag_leftjustify ){` |
|      - | 3657 | `      register int nspace;` |
|    605 | 3658 | `      nspace = width-length;` |
|    605 | 3659 | `      if( nspace>0 ){` |
|      5 | 3660 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3661 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3662 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3663 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3664 | `			}` |
|    ! 0 | 3665 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3666 | `        }` |
|      5 | 3667 | `        if( nspace>0 ){` |
|      5 | 3668 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      5 | 3669 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3670 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3671 | `			}` |
|      2 | 3672 | `		}` |
|      2 | 3673 | `      }` |
|    302 | 3674 | `    }` |
|    619 | 3675 | `    if( length>0 ){` |
|    619 | 3676 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    619 | 3677 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3678 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3679 | `		}` |
|    309 | 3680 | `    }` |
|    619 | 3681 | `    if( flag_leftjustify ){` |
|      - | 3682 | `      register int nspace;` |
|     15 | 3683 | `      nspace = width-length;` |
|     15 | 3684 | `      if( nspace>0 ){` |
|     11 | 3685 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3686 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3687 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3688 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3689 | `			}` |
|    ! 0 | 3690 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3691 | `        }` |
|     11 | 3692 | `        if( nspace>0 ){` |
|     11 | 3693 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|     11 | 3694 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3695 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3696 | `			}` |
|      5 | 3697 | `		}` |
|      5 | 3698 | `      }` |
|      7 | 3699 | `    }` |
|      1 | 3700 | ` }/* for(;;) */` |
|    299 | 3701 | `	return SXRET_OK;` |
|    150 | 3702 | `}` |
|      - | 3703 | `/*` |
|      - | 3704 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 3705 | ` */` |
|    120 | 3706 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 3707 | `{` |
|      - | 3708 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|      - | 3709 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|      - | 3710 | `	 * non-OK rc also stops the format loop. */` |
|    121 | 3711 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|    121 | 3712 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|    121 | 3713 | `	return *pRc;` |
|      1 | 3714 | `}` |
|      - | 3715 | `/*` |
|      - | 3716 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 3717 | ` *  Return a formatted string.` |
|      - | 3718 | ` * Parameters` |
|      - | 3719 | ` *  $format` |
|      - | 3720 | ` *    The format string (see block comment above)` |
|      - | 3721 | ` * Return` |
|      - | 3722 | ` *  A string produced according to the formatting string format.` |
|      - | 3723 | ` */` |
|     72 | 3724 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3725 | `{` |
|      - | 3726 | `	const char *zFormat;` |
|     73 | 3727 | `	sxi32 rc = SXRET_OK;` |
|      - | 3728 | `	int nLen;` |
|     73 | 3729 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3730 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 3731 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3732 | `		return PH7_OK;` |
|      - | 3733 | `	}` |
|      - | 3734 | `	/* Extract the string format */` |
|     73 | 3735 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|     73 | 3736 | `	if( nLen < 1 ){` |
|      - | 3737 | `		/* Empty string */` |
|    ! 0 | 3738 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3739 | `		return PH7_OK;` |
|      - | 3740 | `	}` |
|      - | 3741 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|     73 | 3742 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|     73 | 3743 | `	if( rc != SXRET_OK ){` |
|      - | 3744 | `		/* The result append ran out of memory: raise a fatal rather than` |
|      - | 3745 | `		 * returning a silently-truncated string. */` |
|    ! 0 | 3746 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 3747 | `	}` |
|     73 | 3748 | `	return PH7_OK;` |
|     37 | 3749 | `}` |
|      - | 3750 | `/*` |
|      - | 3751 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 3752 | ` */` |
|   1130 | 3753 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 3754 | `{` |
|   1131 | 3755 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 3756 | `	/* Call the VM output consumer directly */` |
|   1131 | 3757 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 3758 | `	/* Increment counter */` |
|   1131 | 3759 | `	*pCounter += nLen;` |
|   1131 | 3760 | `	return PH7_OK;` |
|      1 | 3761 | `}` |
|      - | 3762 | `/*` |
|      - | 3763 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 3764 | ` *  Output a formatted string.` |
|      - | 3765 | ` * Parameters` |
|      - | 3766 | ` *  $format` |
|      - | 3767 | ` *   See sprintf() for a description of format.` |
|      - | 3768 | ` * Return` |
|      - | 3769 | ` *  The length of the outputted string.` |
|      - | 3770 | ` */` |
|    200 | 3771 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3772 | `{` |
|    201 | 3773 | `	ph7_int64 nCounter = 0;` |
|      - | 3774 | `	const char *zFormat;` |
|      - | 3775 | `	int nLen;` |
|    201 | 3776 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 3777 | `		/* Missing/Invalid arguments,return 0 */` |
|    ! 0 | 3778 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3779 | `		return PH7_OK;` |
|      - | 3780 | `	}` |
|      - | 3781 | `	/* Extract the string format */` |
|    201 | 3782 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    201 | 3783 | `	if( nLen < 1 ){` |
|      - | 3784 | `		/* Empty string */` |
|    ! 0 | 3785 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3786 | `		return PH7_OK;` |
|      - | 3787 | `	}` |
|      - | 3788 | `	/* Format the string */` |
|    201 | 3789 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 3790 | `	/* Return the length of the outputted string */` |
|    201 | 3791 | `	ph7_result_int64(pCtx,nCounter);` |
|    201 | 3792 | `	return PH7_OK;` |
|    101 | 3793 | `}` |
|      - | 3794 | `/*` |
|      - | 3795 | ` * int vprintf(string $format,array $args)` |
|      - | 3796 | ` *  Output a formatted string.` |
|      - | 3797 | ` * Parameters` |
|      - | 3798 | ` *  $format` |
|      - | 3799 | ` *   See sprintf() for a description of format.` |
|      - | 3800 | ` * Return` |
|      - | 3801 | ` *  The length of the outputted string.` |
|      - | 3802 | ` */` |
|      2 | 3803 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3804 | `{` |
|      3 | 3805 | `	ph7_int64 nCounter = 0;` |
|      - | 3806 | `	const char *zFormat;` |
|      - | 3807 | `	ph7_hashmap *pMap;` |
|      - | 3808 | `	SySet sArg;` |
|      - | 3809 | `	int nLen,n;` |
|      3 | 3810 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 3811 | `		/* Missing/Invalid arguments,return 0 */` |
|    ! 0 | 3812 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3813 | `		return PH7_OK;` |
|      - | 3814 | `	}` |
|      - | 3815 | `	/* Extract the string format */` |
|      3 | 3816 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3817 | `	if( nLen < 1 ){` |
|      - | 3818 | `		/* Empty string */` |
|    ! 0 | 3819 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3820 | `		return PH7_OK;` |
|      - | 3821 | `	}` |
|      - | 3822 | `	/* Point to the hashmap */` |
|      3 | 3823 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 3824 | `	/* Extract arguments from the hashmap */` |
|      3 | 3825 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 3826 | `	/* Format the string */` |
|      3 | 3827 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 3828 | `	/* Return the length of the outputted string */` |
|      3 | 3829 | `	ph7_result_int64(pCtx,nCounter);` |
|      - | 3830 | `	/* Release the container */` |
|      3 | 3831 | `	SySetRelease(&sArg);` |
|      3 | 3832 | `	return PH7_OK;` |
|      2 | 3833 | `}` |
|      - | 3834 | `/*` |
|      - | 3835 | ` * int vsprintf(string $format,array $args)` |
|      - | 3836 | ` *  Output a formatted string.` |
|      - | 3837 | ` * Parameters` |
|      - | 3838 | ` *  $format` |
|      - | 3839 | ` *   See sprintf() for a description of format.` |
|      - | 3840 | ` * Return` |
|      - | 3841 | ` *  A string produced according to the formatting string format.` |
|      - | 3842 | ` */` |
|      6 | 3843 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3844 | `{` |
|      - | 3845 | `	const char *zFormat;` |
|      - | 3846 | `	ph7_hashmap *pMap;` |
|      - | 3847 | `	SySet sArg;` |
|      7 | 3848 | `	sxi32 rc = SXRET_OK;` |
|      - | 3849 | `	int nLen,n;` |
|      7 | 3850 | `	if( nArg < 2 \|\| !ph7_value_is_string(apArg[0]) \|\| !ph7_value_is_array(apArg[1]) ){` |
|      - | 3851 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 3852 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3853 | `		return PH7_OK;` |
|      - | 3854 | `	}` |
|      - | 3855 | `	/* Extract the string format */` |
|      7 | 3856 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 3857 | `	if( nLen < 1 ){` |
|      - | 3858 | `		/* Empty string */` |
|    ! 0 | 3859 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3860 | `		return PH7_OK;` |
|      - | 3861 | `	}` |
|      - | 3862 | `	/* Point to hashmap */` |
|      7 | 3863 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 3864 | `	/* Extract arguments from the hashmap */` |
|      7 | 3865 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 3866 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|      7 | 3867 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|      - | 3868 | `	/* Release the container */` |
|      7 | 3869 | `	SySetRelease(&sArg);` |
|      7 | 3870 | `	if( rc != SXRET_OK ){` |
|      - | 3871 | `		/* The result append ran out of memory: raise a fatal. */` |
|    ! 0 | 3872 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 3873 | `	}` |
|      7 | 3874 | `	return PH7_OK;` |
|      4 | 3875 | `}` |
|      - | 3876 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 3877 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 3878 | `/*` |
|      - | 3879 | ` * Symisc eXtension.` |
|      - | 3880 | ` * string size_format(int64 $size)` |
|      - | 3881 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 3882 | ` *  Example:` |
|      - | 3883 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 3884 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 3885 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 3886 | ` * Parameter` |
|      - | 3887 | ` *  $size` |
|      - | 3888 | ` *    Entity size in bytes.` |
|      - | 3889 | ` * Return` |
|      - | 3890 | ` *   Formatted string representation of the given size.` |
|      - | 3891 | ` */` |
|     24 | 3892 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3893 | `{` |
|      - | 3894 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 3895 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 3896 | `	sxi32 nRest,i_32;` |
|      - | 3897 | `	ph7_int64 iSize;` |
|     25 | 3898 | `	int c = -1; /* index in zUnit[] */` |
|      - | 3899 |  |
|     25 | 3900 | `	if( nArg < 1 ){` |
|      - | 3901 | `		/* Missing argument,return the empty string */` |
|      3 | 3902 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3903 | `		return PH7_OK;` |
|      - | 3904 | `	}` |
|      - | 3905 | `	/* Extract the given size */` |
|     23 | 3906 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 3907 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 3908 | `		/* Don't bother formatting,return immediately */` |
|      5 | 3909 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 3910 | `		return PH7_OK;` |
|      - | 3911 | `	}` |
|     19 | 3912 | `	for(;;){` |
|     39 | 3913 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 3914 | `		iSize >>= 10;` |
|     39 | 3915 | `		c++;` |
|     39 | 3916 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 3917 | `			break;` |
|      - | 3918 | `		}` |
|      1 | 3919 | `	}` |
|     19 | 3920 | `	nRest /= 100;` |
|     19 | 3921 | `	if( nRest > 9 ){` |
|    ! 0 | 3922 | `		nRest = 9;` |
|    ! 0 | 3923 | `	}` |
|     19 | 3924 | `	if( iSize > 999 ){` |
|    ! 0 | 3925 | `		c++;` |
|    ! 0 | 3926 | `		nRest = 9;` |
|    ! 0 | 3927 | `		iSize = 0;` |
|    ! 0 | 3928 | `	}` |
|     19 | 3929 | `	i_32 = (sxi32)iSize;` |
|      - | 3930 | `	/* Format */` |
|     19 | 3931 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 3932 | `	return PH7_OK;` |
|     13 | 3933 | `}` |
|      - | 3934 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 3935 | `/*` |
|      - | 3936 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 3937 | ` *   Calculate the md5 hash of a string.` |
|      - | 3938 | ` * Parameter` |
|      - | 3939 | ` *  $str` |
|      - | 3940 | ` *   Input string` |
|      - | 3941 | ` * $raw_output` |
|      - | 3942 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 3943 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 3944 | ` * Return` |
|      - | 3945 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 3946 | ` */` |
|     12 | 3947 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3948 | `{` |
|      - | 3949 | `	unsigned char zDigest[16];` |
|     13 | 3950 | `	int raw_output = FALSE;` |
|      - | 3951 | `	const void *pIn;` |
|      - | 3952 | `	int nLen;` |
|     13 | 3953 | `	if( nArg < 1 ){` |
|      - | 3954 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 3955 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3956 | `		return PH7_OK;` |
|      - | 3957 | `	}` |
|      - | 3958 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 3959 | `	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */` |
|     13 | 3960 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 3961 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 3962 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 3963 | `	}` |
|      - | 3964 | `	/* Compute the MD5 digest */` |
|     13 | 3965 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|     13 | 3966 | `	if( raw_output ){` |
|      - | 3967 | `		/* Output raw digest */` |
|      5 | 3968 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 3969 | `	}else{` |
|      - | 3970 | `		/* Perform a binary to hex conversion */` |
|      9 | 3971 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 3972 | `	}` |
|     13 | 3973 | `	return PH7_OK;` |
|      7 | 3974 | `}` |
|      - | 3975 | `/*` |
|      - | 3976 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 3977 | ` *   Calculate the sha1 hash of a string.` |
|      - | 3978 | ` * Parameter` |
|      - | 3979 | ` *  $str` |
|      - | 3980 | ` *   Input string` |
|      - | 3981 | ` * $raw_output` |
|      - | 3982 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 3983 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 3984 | ` * Return` |
|      - | 3985 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 3986 | ` */` |
|     10 | 3987 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3988 | `{` |
|      - | 3989 | `	unsigned char zDigest[20];` |
|     11 | 3990 | `	int raw_output = FALSE;` |
|      - | 3991 | `	const void *pIn;` |
|      - | 3992 | `	int nLen;` |
|     11 | 3993 | `	if( nArg < 1 ){` |
|      - | 3994 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 3995 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3996 | `		return PH7_OK;` |
|      - | 3997 | `	}` |
|      - | 3998 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 3999 | `	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */` |
|     11 | 4000 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 4001 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 4002 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 4003 | `	}` |
|      - | 4004 | `	/* Compute the SHA1 digest */` |
|     11 | 4005 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|     11 | 4006 | `	if( raw_output ){` |
|      - | 4007 | `		/* Output raw digest */` |
|      5 | 4008 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 4009 | `	}else{` |
|      - | 4010 | `		/* Perform a binary to hex conversion */` |
|      7 | 4011 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4012 | `	}` |
|     11 | 4013 | `	return PH7_OK;` |
|      6 | 4014 | `}` |
|      - | 4015 | `/*` |
|      - | 4016 | ` * int64 crc32(string $str)` |
|      - | 4017 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 4018 | ` * Parameter` |
|      - | 4019 | ` *  $str` |
|      - | 4020 | ` *   Input string` |
|      - | 4021 | ` * Return` |
|      - | 4022 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 4023 | ` */` |
|      2 | 4024 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4025 | `{` |
|      - | 4026 | `	const void *pIn;` |
|      - | 4027 | `	sxu32 nCRC;` |
|      - | 4028 | `	int nLen;` |
|      3 | 4029 | `	if( nArg < 1 ){` |
|      - | 4030 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4031 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4032 | `		return PH7_OK;` |
|      - | 4033 | `	}` |
|      - | 4034 | `	/* Extract the input string */` |
|      3 | 4035 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4036 | `	if( nLen < 1 ){` |
|      - | 4037 | `		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike` |
|      - | 4038 | `		 * md5()/sha1(), whose empty-string digests are non-zero. */` |
|    ! 0 | 4039 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4040 | `		return PH7_OK;` |
|      - | 4041 | `	}` |
|      - | 4042 | `	/* Calculate the sum */` |
|      3 | 4043 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 4044 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 4045 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 4046 | `	return PH7_OK;` |
|      2 | 4047 | `}` |
|      - | 4048 | `/*` |
|      - | 4049 | ` * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is` |
|      - | 4050 | ` * described by a small record so one dispatch (and one generic HMAC) serves them` |
|      - | 4051 | ` * all. Thin adapters normalize the differing context types and the reversed` |
|      - | 4052 | ` * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.` |
|      - | 4053 | ` */` |
|     11 | 4054 | `static void HashMd5Init(HashCtx *c){ MD5Init(&c->md5); }` |
|     15 | 4055 | `static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }` |
|     11 | 4056 | `static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }` |
|     11 | 4057 | `static void HashSha1Init(HashCtx *c){ SHA1Init(&c->sha1); }` |
|     15 | 4058 | `static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }` |
|     11 | 4059 | `static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }` |
|      9 | 4060 | `static void HashSha224Init(HashCtx *c){ SHA224Init(&c->sha256); }` |
|     33 | 4061 | `static void HashSha256Init(HashCtx *c){ SHA256Init(&c->sha256); }` |
|     57 | 4062 | `static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }` |
|     41 | 4063 | `static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }` |
|      9 | 4064 | `static void HashSha384Init(HashCtx *c){ SHA384Init(&c->sha512); }` |
|     15 | 4065 | `static void HashSha512Init(HashCtx *c){ SHA512Init(&c->sha512); }` |
|     27 | 4066 | `static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }` |
|     23 | 4067 | `static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }` |
|      - | 4068 | `typedef struct HashAlgo HashAlgo;` |
|      - | 4069 | `struct HashAlgo {` |
|      - | 4070 | `	const char *zName;   /* lowercase canonical name */` |
|      - | 4071 | `	int nDigestLen;      /* output bytes: 16/20/28/32/48/64 */` |
|      - | 4072 | `	int nBlockLen;       /* internal block bytes (for HMAC): 64 or 128 */` |
|      - | 4073 | `	void (*xInit)(HashCtx *);` |
|      - | 4074 | `	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);` |
|      - | 4075 | `	void (*xFinal)(HashCtx *,unsigned char *);` |
|      - | 4076 | `};` |
|      - | 4077 | `static const HashAlgo aHashAlgo[] = {` |
|      - | 4078 | `	{ "md5",    16, 64,  HashMd5Init,    HashMd5Update,    HashMd5Final    },` |
|      - | 4079 | `	{ "sha1",   20, 64,  HashSha1Init,   HashSha1Update,   HashSha1Final   },` |
|      - | 4080 | `	{ "sha224", 28, 64,  HashSha224Init, HashSha256Update, HashSha256Final },` |
|      - | 4081 | `	{ "sha256", 32, 64,  HashSha256Init, HashSha256Update, HashSha256Final },` |
|      - | 4082 | `	{ "sha384", 48, 128, HashSha384Init, HashSha512Update, HashSha512Final },` |
|      - | 4083 | `	{ "sha512", 64, 128, HashSha512Init, HashSha512Update, HashSha512Final },` |
|      - | 4084 | `};` |
|      - | 4085 | `/* Case-insensitive algorithm lookup (PHP accepts 'SHA256' etc.). */` |
|     73 | 4086 | `static const HashAlgo * HashFindAlgo(const char *zName,int nLen){` |
|      - | 4087 | `	sxu32 i;` |
|    279 | 4088 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|    272 | 4089 | `		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen` |
|    211 | 4090 | `			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){` |
|     67 | 4091 | `			return &aHashAlgo[i];` |
|      - | 4092 | `		}` |
|    106 | 4093 | `	}` |
|      6 | 4094 | `	return 0;` |
|     38 | 4095 | `}` |
|      - | 4096 | `/*` |
|      - | 4097 | ` * string hash(string $algo,string $data[,bool $binary = false])` |
|      - | 4098 | ` *   Generate a hash value (message digest).` |
|      - | 4099 | ` */` |
|     54 | 4100 | `static int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4101 | `{` |
|      - | 4102 | `	const HashAlgo *pAlgo;` |
|      - | 4103 | `	const char *zAlgo,*zData;` |
|     56 | 4104 | `	int nAlgoLen,nDataLen,raw_output = FALSE;` |
|      - | 4105 | `	HashCtx sCtx;` |
|      - | 4106 | `	unsigned char zDigest[64];` |
|     56 | 4107 | `	if( nArg < 2 ){` |
|    ! 0 | 4108 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4109 | `			"hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4110 | `	}` |
|     56 | 4111 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     56 | 4112 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     56 | 4113 | `	if( pAlgo == 0 ){` |
|      3 | 4114 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4115 | `			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - | 4116 | `	}` |
|     53 | 4117 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     53 | 4118 | `	if( nArg > 2 ){` |
|      9 | 4119 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|      4 | 4120 | `	}` |
|     53 | 4121 | `	pAlgo->xInit(&sCtx);` |
|     53 | 4122 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     53 | 4123 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     53 | 4124 | `	if( raw_output ){` |
|      9 | 4125 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      5 | 4126 | `	}else{` |
|     45 | 4127 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 4128 | `	}` |
|     53 | 4129 | `	return PH7_OK;` |
|     29 | 4130 | `}` |
|      - | 4131 | `/*` |
|      - | 4132 | ` * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])` |
|      - | 4133 | ` *   Generate a keyed hash value using the HMAC method (RFC 2104).` |
|      - | 4134 | ` */` |
|     16 | 4135 | `static int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4136 | `{` |
|      - | 4137 | `	const HashAlgo *pAlgo;` |
|      - | 4138 | `	const char *zAlgo,*zData,*zKey;` |
|     18 | 4139 | `	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;` |
|      - | 4140 | `	HashCtx sCtx;` |
|      - | 4141 | `	unsigned char zKeyBlock[128],zIpad[128],zOpad[128],zInner[64],zDigest[64];` |
|      - | 4142 | `	int i,nBlock,nDigest;` |
|     18 | 4143 | `	if( nArg < 3 ){` |
|    ! 0 | 4144 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4145 | `			"hash_hmac() expects at least 3 arguments, %d given",nArg);` |
|      - | 4146 | `	}` |
|     18 | 4147 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     18 | 4148 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     18 | 4149 | `	if( pAlgo == 0 ){` |
|      3 | 4150 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4151 | `			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 4152 | `	}` |
|     15 | 4153 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     15 | 4154 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|     15 | 4155 | `	if( nArg > 3 ){` |
|      3 | 4156 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|      1 | 4157 | `	}` |
|     15 | 4158 | `	nBlock = pAlgo->nBlockLen;` |
|     15 | 4159 | `	nDigest = pAlgo->nDigestLen;` |
|      - | 4160 | `	/* Reduce the key to a single block: hash it if longer than the block, then` |
|      - | 4161 | `	 * zero-pad (a short or empty key is just zero-padded). */` |
|     15 | 4162 | `	SyZero(zKeyBlock,sizeof(zKeyBlock));` |
|     15 | 4163 | `	if( nKeyLen > nBlock ){` |
|      3 | 4164 | `		pAlgo->xInit(&sCtx);` |
|      3 | 4165 | `		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);` |
|      3 | 4166 | `		pAlgo->xFinal(&sCtx,zKeyBlock);` |
|     14 | 4167 | `	}else if( nKeyLen > 0 ){` |
|     11 | 4168 | `		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);` |
|      5 | 4169 | `	}` |
|   1039 | 4170 | `	for( i = 0; i < nBlock; i++ ){` |
|   1025 | 4171 | `		zIpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);` |
|   1025 | 4172 | `		zOpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);` |
|    513 | 4173 | `	}` |
|      - | 4174 | `	/* inner = H((key ^ ipad) \|\| data) */` |
|     15 | 4175 | `	pAlgo->xInit(&sCtx);` |
|     15 | 4176 | `	pAlgo->xUpdate(&sCtx,zIpad,(unsigned int)nBlock);` |
|     15 | 4177 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     15 | 4178 | `	pAlgo->xFinal(&sCtx,zInner);` |
|      - | 4179 | `	/* out = H((key ^ opad) \|\| inner) */` |
|     15 | 4180 | `	pAlgo->xInit(&sCtx);` |
|     15 | 4181 | `	pAlgo->xUpdate(&sCtx,zOpad,(unsigned int)nBlock);` |
|     15 | 4182 | `	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)nDigest);` |
|     15 | 4183 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     15 | 4184 | `	if( raw_output ){` |
|      3 | 4185 | `		ph7_result_string(pCtx,(const char *)zDigest,nDigest);` |
|      2 | 4186 | `	}else{` |
|     13 | 4187 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);` |
|      - | 4188 | `	}` |
|     15 | 4189 | `	return PH7_OK;` |
|     10 | 4190 | `}` |
|      - | 4191 | `/*` |
|      - | 4192 | ` * bool hash_equals(string $known_string,string $user_string)` |
|      - | 4193 | ` *   Timing-attack-safe string comparison.` |
|      - | 4194 | ` */` |
|     14 | 4195 | `static int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4196 | `{` |
|      - | 4197 | `	const char *zKnown,*zUser;` |
|      - | 4198 | `	int nKnown,nUser,i;` |
|     17 | 4199 | `	volatile unsigned char vDiff = 0;` |
|     17 | 4200 | `	if( nArg < 2 ){` |
|    ! 0 | 4201 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4202 | `			"hash_equals() expects exactly 2 arguments, %d given",nArg);` |
|      - | 4203 | `	}` |
|     17 | 4204 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      4 | 4205 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4206 | `			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",` |
|      1 | 4207 | `			ph7_type_name(apArg[0]));` |
|      - | 4208 | `	}` |
|     14 | 4209 | `	if( !ph7_value_is_string(apArg[1]) ){` |
|      4 | 4210 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4211 | `			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",` |
|      2 | 4212 | `			ph7_type_name(apArg[1]));` |
|      - | 4213 | `	}` |
|     11 | 4214 | `	zKnown = ph7_value_to_string(apArg[0],&nKnown);` |
|     11 | 4215 | `	zUser = ph7_value_to_string(apArg[1],&nUser);` |
|     11 | 4216 | `	if( nKnown != nUser ){` |
|      5 | 4217 | `		ph7_result_bool(pCtx,0);` |
|      5 | 4218 | `		return PH7_OK;` |
|      - | 4219 | `	}` |
|      - | 4220 | `	/* Constant-time: read every byte, never short-circuit. */` |
|     19 | 4221 | `	for( i = 0; i < nKnown; i++ ){` |
|     13 | 4222 | `		vDiff \|= (unsigned char)(zKnown[i] ^ zUser[i]);` |
|      7 | 4223 | `	}` |
|      7 | 4224 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|      7 | 4225 | `	return PH7_OK;` |
|     10 | 4226 | `}` |
|      - | 4227 | `/*` |
|      - | 4228 | ` * array hash_algos(void)` |
|      - | 4229 | ` *   Return a list of the registered hashing algorithms.` |
|      - | 4230 | ` */` |
|      2 | 4231 | `static int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4232 | `{` |
|      - | 4233 | `	ph7_value *pArray,*pValue;` |
|      - | 4234 | `	sxu32 i;` |
|      1 | 4235 | `	SXUNUSED(nArg);` |
|      1 | 4236 | `	SXUNUSED(apArg);` |
|      3 | 4237 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4238 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 4239 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 4240 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4241 | `		return PH7_OK;` |
|      - | 4242 | `	}` |
|     15 | 4243 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|     13 | 4244 | `		ph7_value_string(pValue,aHashAlgo[i].zName,-1);` |
|     13 | 4245 | `		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);` |
|     13 | 4246 | `		ph7_value_reset_string_cursor(pValue);` |
|      7 | 4247 | `	}` |
|      3 | 4248 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4249 | `	return PH7_OK;` |
|      2 | 4250 | `}` |
|      - | 4251 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 4252 | `/*` |
|      - | 4253 | ` * password_* (bcrypt). These live in ext/standard in real PHP — outside the` |
|      - | 4254 | ` * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.` |
|      - | 4255 | ` */` |
|      - | 4256 | `/*` |
|      - | 4257 | ` * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a` |
|      - | 4258 | ` * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).` |
|      - | 4259 | ` */` |
|     40 | 4260 | `static int BcryptParseHash(const char *zHash,int nHash,int *piCost)` |
|      1 | 4261 | `{` |
|      - | 4262 | `	int iCost;` |
|     40 | 4263 | `	if( nHash != 60 \|\| zHash[0] != '$' \|\| zHash[1] != '2' \|\| zHash[3] != '$'` |
|     29 | 4264 | `		\|\| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){` |
|     13 | 4265 | `		return FALSE;` |
|      - | 4266 | `	}` |
|     29 | 4267 | `	if( zHash[4] < '0' \|\| zHash[4] > '9' \|\| zHash[5] < '0' \|\| zHash[5] > '9' \|\| zHash[6] != '$' ){` |
|    ! 0 | 4268 | `		return FALSE;` |
|      - | 4269 | `	}` |
|     29 | 4270 | `	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');` |
|     29 | 4271 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      3 | 4272 | `		return FALSE;` |
|      - | 4273 | `	}` |
|     27 | 4274 | `	if( piCost ){ *piCost = iCost; }` |
|     27 | 4275 | `	return TRUE;` |
|     21 | 4276 | `}` |
|      - | 4277 | `/*` |
|      - | 4278 | ` * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the` |
|      - | 4279 | ` * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.` |
|      - | 4280 | ` */` |
|     20 | 4281 | `static int BcryptIsBcryptAlgo(ph7_value *pAlgo)` |
|      3 | 4282 | `{` |
|     23 | 4283 | `	if( ph7_value_is_null(pAlgo) ){` |
|    ! 0 | 4284 | `		return TRUE;` |
|      - | 4285 | `	}` |
|     23 | 4286 | `	if( ph7_value_is_string(pAlgo) ){` |
|      - | 4287 | `		int nAlgo;` |
|     23 | 4288 | `		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);` |
|     23 | 4289 | `		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );` |
|      - | 4290 | `	}` |
|    ! 0 | 4291 | `	return FALSE;` |
|     13 | 4292 | `}` |
|      - | 4293 | `/*` |
|      - | 4294 | ` * bool\|string password_hash(string $password,string\|int\|null $algo[,array $options])` |
|      - | 4295 | ` *  Create a bcrypt hash of the password.` |
|      - | 4296 | ` */` |
|     16 | 4297 | `static int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4298 | `{` |
|      - | 4299 | `	const char *zPwd;` |
|     19 | 4300 | `	int nPwd,iCost = 12;` |
|      - | 4301 | `	unsigned char aSalt[16];` |
|      - | 4302 | `	char zHash[60];` |
|     19 | 4303 | `	if( nArg < 2 ){` |
|    ! 0 | 4304 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4305 | `			"password_hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4306 | `	}` |
|     19 | 4307 | `	if( !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      3 | 4308 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4309 | `			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");` |
|      - | 4310 | `	}` |
|      - | 4311 | `	/* cost from $options['cost'] (default 12). */` |
|     16 | 4312 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|     14 | 4313 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     14 | 4314 | `		if( pCost ){ iCost = ph7_value_to_int(pCost); }` |
|      6 | 4315 | `	}` |
|     16 | 4316 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      4 | 4317 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 4318 | `			"Invalid bcrypt cost parameter specified: %d",iCost);` |
|      - | 4319 | `	}` |
|     13 | 4320 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     13 | 4321 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 4322 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 4323 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 4324 | `	}` |
|     13 | 4325 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){` |
|    ! 0 | 4326 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4327 | `		return PH7_OK;` |
|      - | 4328 | `	}` |
|     13 | 4329 | `	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));` |
|     13 | 4330 | `	return PH7_OK;` |
|     11 | 4331 | `}` |
|      - | 4332 | `/*` |
|      - | 4333 | ` * bool password_verify(string $password,string $hash)` |
|      - | 4334 | ` *  Verify a password against a bcrypt hash. Never throws on a malformed hash.` |
|      - | 4335 | ` */` |
|     28 | 4336 | `static int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4337 | `{` |
|      - | 4338 | `	const char *zPwd,*zHash;` |
|      - | 4339 | `	int nPwd,nHash,iCost,i;` |
|      - | 4340 | `	unsigned char aSalt[16];` |
|      - | 4341 | `	char zComputed[60];` |
|     29 | 4342 | `	volatile unsigned char vDiff = 0;` |
|     29 | 4343 | `	if( nArg < 2 ){` |
|    ! 0 | 4344 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4345 | `			"password_verify() expects exactly 2 arguments, %d given",nArg);` |
|      - | 4346 | `	}` |
|     29 | 4347 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     29 | 4348 | `	zHash = ph7_value_to_string(apArg[1],&nHash);` |
|     29 | 4349 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|     11 | 4350 | `		ph7_result_bool(pCtx,0);` |
|     11 | 4351 | `		return PH7_OK;` |
|      - | 4352 | `	}` |
|      - | 4353 | `	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */` |
|     19 | 4354 | `	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 4355 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4356 | `		return PH7_OK;` |
|      - | 4357 | `	}` |
|     19 | 4358 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){` |
|    ! 0 | 4359 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4360 | `		return PH7_OK;` |
|      - | 4361 | `	}` |
|      - | 4362 | `	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps` |
|      - | 4363 | `	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */` |
|    577 | 4364 | `	for( i = 29; i < 60; i++ ){` |
|    559 | 4365 | `		vDiff \|= (unsigned char)(zComputed[i] ^ zHash[i]);` |
|    280 | 4366 | `	}` |
|     19 | 4367 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|     19 | 4368 | `	return PH7_OK;` |
|     15 | 4369 | `}` |
|      - | 4370 | `/*` |
|      - | 4371 | ` * array password_get_info(string $hash)` |
|      - | 4372 | ` *  Return ["algo"=>id\|null, "algoName"=>name, "options"=>[...]].` |
|      - | 4373 | ` */` |
|      6 | 4374 | `static int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4375 | `{` |
|      7 | 4376 | `	const char *zHash = "";` |
|      7 | 4377 | `	int nHash,iCost = 0,bBcrypt = 0;` |
|      - | 4378 | `	ph7_value *pArray,*pOptions,*pVal;` |
|      7 | 4379 | `	if( nArg > 0 ){` |
|      7 | 4380 | `		zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 4381 | `		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);` |
|      3 | 4382 | `	}` |
|      7 | 4383 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 4384 | `	pOptions = ph7_context_new_array(pCtx);` |
|      7 | 4385 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      7 | 4386 | `	if( pArray == 0 \|\| pOptions == 0 \|\| pVal == 0 ){` |
|    ! 0 | 4387 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4388 | `		return PH7_OK;` |
|      - | 4389 | `	}` |
|      7 | 4390 | `	if( bBcrypt ){` |
|      5 | 4391 | `		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */` |
|      5 | 4392 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 4393 | `		ph7_value_reset_string_cursor(pVal);` |
|      5 | 4394 | `		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);` |
|      5 | 4395 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 4396 | `		ph7_value_int(pVal,iCost);` |
|      5 | 4397 | `		ph7_array_add_strkey_elem(pOptions,"cost",pVal);` |
|      3 | 4398 | `	}else{` |
|      3 | 4399 | `		ph7_value_null(pVal);                          /* algo => null */` |
|      3 | 4400 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      3 | 4401 | `		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);` |
|      3 | 4402 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      - | 4403 | `	}` |
|      7 | 4404 | `	ph7_array_add_strkey_elem(pArray,"options",pOptions);` |
|      7 | 4405 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 4406 | `	return PH7_OK;` |
|      4 | 4407 | `}` |
|      - | 4408 | `/*` |
|      - | 4409 | ` * bool password_needs_rehash(string $hash,string\|int\|null $algo[,array $options])` |
|      - | 4410 | ` *  True if the hash was not made with the given algo/options.` |
|      - | 4411 | ` */` |
|      6 | 4412 | `static int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4413 | `{` |
|      - | 4414 | `	const char *zHash;` |
|      7 | 4415 | `	int nHash,iCost = 0,iWantCost = 12;` |
|      7 | 4416 | `	if( nArg < 2 ){` |
|    ! 0 | 4417 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4418 | `			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4419 | `	}` |
|      7 | 4420 | `	zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 4421 | `	if( !BcryptParseHash(zHash,nHash,&iCost) \|\| !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      - | 4422 | `		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */` |
|      3 | 4423 | `		ph7_result_bool(pCtx,1);` |
|      3 | 4424 | `		return PH7_OK;` |
|      - | 4425 | `	}` |
|      5 | 4426 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      5 | 4427 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|      5 | 4428 | `		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }` |
|      2 | 4429 | `	}` |
|      5 | 4430 | `	ph7_result_bool(pCtx,iCost != iWantCost);` |
|      5 | 4431 | `	return PH7_OK;` |
|      4 | 4432 | `}` |
|      - | 4433 | `/*` |
|      - | 4434 | ` * filter_var() — input validation and sanitization (the ext/filter API).` |
|      - | 4435 | ` *` |
|      - | 4436 | ` * Filter and flag identifiers (values match PHP 8.5; the constants themselves` |
|      - | 4437 | ` * are registered in constant.c). The validate filters are hand-rolled rather` |
|      - | 4438 | ` * than delegating to SyStrToInt64/SyStrToReal: the former silently skips leading` |
|      - | 4439 | ` * zeros and cannot signal overflow, and the latter treats ',' as a decimal point` |
|      - | 4440 | ` * unconditionally — neither matches PHP's filter semantics.` |
|      - | 4441 | ` */` |
|      - | 4442 | `#define FV_VALIDATE_INT     257` |
|      - | 4443 | `#define FV_VALIDATE_BOOLEAN 258` |
|      - | 4444 | `#define FV_VALIDATE_FLOAT   259` |
|      - | 4445 | `#define FV_VALIDATE_REGEXP  272` |
|      - | 4446 | `#define FV_VALIDATE_URL     273` |
|      - | 4447 | `#define FV_VALIDATE_EMAIL   274` |
|      - | 4448 | `#define FV_VALIDATE_IP      275` |
|      - | 4449 | `#define FV_VALIDATE_MAC     276` |
|      - | 4450 | `#define FV_VALIDATE_DOMAIN  277` |
|      - | 4451 | `#define FV_SANITIZE_SPECIAL_CHARS      515` |
|      - | 4452 | `#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */` |
|      - | 4453 | `#define FV_SANITIZE_EMAIL   517` |
|      - | 4454 | `#define FV_SANITIZE_URL     518` |
|      - | 4455 | `#define FV_SANITIZE_NUMBER_INT   519` |
|      - | 4456 | `#define FV_SANITIZE_NUMBER_FLOAT 520` |
|      - | 4457 | `#define FV_SANITIZE_FULL_SPECIAL_CHARS 522` |
|      - | 4458 | `#define FV_FLAG_ALLOW_OCTAL  1` |
|      - | 4459 | `#define FV_FLAG_ALLOW_HEX    2` |
|      - | 4460 | `#define FV_FLAG_STRIP_LOW    4` |
|      - | 4461 | `#define FV_FLAG_STRIP_HIGH   8` |
|      - | 4462 | `#define FV_FLAG_ENCODE_LOW   16` |
|      - | 4463 | `#define FV_FLAG_ENCODE_HIGH  32` |
|      - | 4464 | `#define FV_FLAG_ENCODE_AMP   64` |
|      - | 4465 | `#define FV_FLAG_NO_ENCODE_QUOTES 128` |
|      - | 4466 | `#define FV_FLAG_STRIP_BACKTICK   512` |
|      - | 4467 | `#define FV_FLAG_ALLOW_FRACTION   4096` |
|      - | 4468 | `#define FV_FLAG_ALLOW_THOUSAND   8192` |
|      - | 4469 | `#define FV_FLAG_ALLOW_SCIENTIFIC 16384` |
|      - | 4470 | `#define FV_FLAG_IPV4  1048576` |
|      - | 4471 | `#define FV_FLAG_IPV6  2097152` |
|      - | 4472 | `#define FV_NULL_ON_FAILURE 134217728` |
|      - | 4473 | `/* The subset of flags the UNSAFE_RAW/DEFAULT string filter (FvSanitizeString)` |
|      - | 4474 | ` * acts on: when none are set the filter is a verbatim pass-through, so FV_DEFAULT` |
|      - | 4475 | ` * can shortcut. Keep this in sync with FvSanitizeString's flag handling. */` |
|      - | 4476 | `#define FV_FLAG_STRING_MASK (FV_FLAG_STRIP_LOW\|FV_FLAG_STRIP_HIGH\|FV_FLAG_STRIP_BACKTICK \` |
|      - | 4477 | `                            \|FV_FLAG_ENCODE_LOW\|FV_FLAG_ENCODE_HIGH\|FV_FLAG_ENCODE_AMP)` |
|      - | 4478 |  |
|      - | 4479 | `/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.` |
|      - | 4480 | ` * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */` |
|    153 | 4481 | `static void FvTrim(const char **pz,int *pn){` |
|    153 | 4482 | `	const char *z = *pz;` |
|    153 | 4483 | `	int n = *pn;` |
|    157 | 4484 | `	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }` |
|    161 | 4485 | `	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }` |
|    153 | 4486 | `	*pz = z; *pn = n;` |
|    153 | 4487 | `}` |
|      - | 4488 | `/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     57 | 4489 | `static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){` |
|     57 | 4490 | `	int neg = 0, i;` |
|     57 | 4491 | `	sxu64 u = 0;` |
|     57 | 4492 | `	FvTrim(&z,&n);` |
|     57 | 4493 | `	if( n==0 ){ return 0; }` |
|     51 | 4494 | `	if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }` |
|     51 | 4495 | `	if( n==0 ){ return 0; }` |
|     49 | 4496 | `	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'\|\|z[1]=='X') ){` |
|      3 | 4497 | `		z += 2; n -= 2;` |
|      3 | 4498 | `		if( n==0 ){ return 0; }` |
|      7 | 4499 | `		for( i=0; i<n; i++ ){` |
|      5 | 4500 | `			int h = SyHexToint((unsigned char)z[i]);` |
|      5 | 4501 | `			if( h<0 ){ return 0; }` |
|      5 | 4502 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }` |
|      5 | 4503 | `			u = u*16 + (sxu64)h;` |
|      3 | 4504 | `		}` |
|     48 | 4505 | `	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){` |
|      9 | 4506 | `		for( i=0; i<n; i++ ){` |
|      7 | 4507 | `			if( z[i]<'0' \|\| z[i]>'7' ){ return 0; }` |
|      7 | 4508 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }` |
|      7 | 4509 | `			u = u*8 + (sxu64)(z[i]-'0');` |
|      4 | 4510 | `		}` |
|      2 | 4511 | `	}else{` |
|     45 | 4512 | `		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */` |
|    201 | 4513 | `		for( i=0; i<n; i++ ){` |
|    173 | 4514 | `			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|    161 | 4515 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }` |
|    161 | 4516 | `			u = u*10 + (sxu64)(z[i]-'0');` |
|     81 | 4517 | `		}` |
|      - | 4518 | `	}` |
|     33 | 4519 | `	if( neg ){` |
|      5 | 4520 | `		if( u > 0x8000000000000000ULL ){ return 0; }` |
|      5 | 4521 | `		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */` |
|      3 | 4522 | `	}else{` |
|     29 | 4523 | `		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }` |
|     27 | 4524 | `		*pOut = (ph7_int64)u;` |
|      - | 4525 | `	}` |
|     31 | 4526 | `	return 1;` |
|     29 | 4527 | `}` |
|      - | 4528 | `/* FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     69 | 4529 | `static int FvValidateFloat(const char *z,int n,int flags,double *pOut){` |
|      - | 4530 | `	char zBuf[512];` |
|     69 | 4531 | `	int i, m = 0, seenDigit = 0;` |
|     69 | 4532 | `	const char *zv; int nv; double d = 0;` |
|     69 | 4533 | `	FvTrim(&z,&n);` |
|      - | 4534 | `	/* Bound the input: zBuf[512] holds the thousand-separator-stripped copy, and` |
|      - | 4535 | `	 * the cap also rejects the pathological 500+ digit floats PHP refuses. */` |
|     69 | 4536 | `	if( n==0 \|\| n>500 ){ return 0; }` |
|     69 | 4537 | `	if( flags & FV_FLAG_ALLOW_THOUSAND ){` |
|      - | 4538 | `		/* Commas are optional, but when present they must group the integer part` |
|      - | 4539 | `		 * into a leading run of 1..3 digits followed by groups of exactly 3` |
|      - | 4540 | `		 * ("1,000" ok, "1,5"/"1234,567" rejected). Strip them into zBuf and reject` |
|      - | 4541 | `		 * a comma anywhere in the fractional/exponent tail. */` |
|     25 | 4542 | `		int s = 0, intEnd, segStart, segIdx, hasComma = 0;` |
|     25 | 4543 | `		if( s<n && (z[s]=='+'\|\|z[s]=='-') ){ zBuf[m++] = z[s]; s++; }` |
|     25 | 4544 | `		intEnd = s;` |
|    167 | 4545 | `		while( intEnd<n && z[intEnd]!='.' && z[intEnd]!='e' && z[intEnd]!='E' ){` |
|    143 | 4546 | `			if( z[intEnd]==',' ){ hasComma = 1; }` |
|    143 | 4547 | `			intEnd++;` |
|      1 | 4548 | `		}` |
|     25 | 4549 | `		if( hasComma ){` |
|     25 | 4550 | `			segStart = s; segIdx = 0;` |
|    165 | 4551 | `			for( i=s; i<=intEnd; i++ ){` |
|    151 | 4552 | `				if( i==intEnd \|\| z[i]==',' ){` |
|     49 | 4553 | `					int segLen = i - segStart, k;` |
|     49 | 4554 | `					if( segIdx==0 ){ if( segLen<1 \|\| segLen>3 ){ return 0; } }` |
|     25 | 4555 | `					else if( segLen!=3 ){ return 0; }` |
|    119 | 4556 | `					for( k=segStart; k<i; k++ ){` |
|     81 | 4557 | `						if( !SyisDigit((unsigned char)z[k]) ){ return 0; }` |
|     81 | 4558 | `						zBuf[m++] = z[k];` |
|     41 | 4559 | `					}` |
|     39 | 4560 | `					segStart = i+1; segIdx++;` |
|     19 | 4561 | `				}` |
|     71 | 4562 | `			}` |
|      8 | 4563 | `		}else{` |
|    ! 0 | 4564 | `			for( i=s; i<intEnd; i++ ){ zBuf[m++] = z[i]; }` |
|      - | 4565 | `		}` |
|     27 | 4566 | `		for( i=intEnd; i<n; i++ ){` |
|     13 | 4567 | `			if( z[i]==',' ){ return 0; }` |
|     13 | 4568 | `			zBuf[m++] = z[i];` |
|      7 | 4569 | `		}` |
|     15 | 4570 | `		zv = zBuf; nv = m;` |
|      8 | 4571 | `	}else{` |
|     45 | 4572 | `		zv = z; nv = n;` |
|      - | 4573 | `	}` |
|     59 | 4574 | `	i = 0;` |
|     59 | 4575 | `	if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|    167 | 4576 | `	while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     59 | 4577 | `	if( i<nv && zv[i]=='.' ){` |
|     21 | 4578 | `		i++;` |
|     39 | 4579 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     10 | 4580 | `	}` |
|     59 | 4581 | `	if( !seenDigit ){ return 0; }` |
|     57 | 4582 | `	if( i<nv && (zv[i]=='e'\|\|zv[i]=='E') ){` |
|     29 | 4583 | `		i++;` |
|     29 | 4584 | `		if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|     29 | 4585 | `		if( i>=nv \|\| !SyisDigit((unsigned char)zv[i]) ){ return 0; }` |
|    105 | 4586 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; }` |
|     14 | 4587 | `	}` |
|     57 | 4588 | `	if( i!=nv ){ return 0; } /* trailing junk */` |
|      - | 4589 | `	/* The grammar above guarantees zv[0..nv) is a clean ASCII decimal float (no hex /` |
|      - | 4590 | `	 * inf / nan / trailing junk), so it is safe to hand to libc strtod, which — unlike` |
|      - | 4591 | `	 * SyStrToReal (15 sig-digits + exponent clamped to 308, so it silently saturates` |
|      - | 4592 | `	 * overflowing magnitudes to a finite value) — is overflow/underflow-aware and` |
|      - | 4593 | `	 * correctly rounded. strtod needs a NUL-terminated string: the ALLOW_THOUSAND path` |
|      - | 4594 | `	 * already built the span in zBuf (zv==zBuf); the plain path must copy it there (z is` |
|      - | 4595 | `	 * const + not NUL-terminated). nv <= n <= 500 < sizeof(zBuf) by the cap above.` |
|      - | 4596 | `	 * Matches PHP 8.5 byte-for-byte: reject overflow (-> +/-INF) and total underflow` |
|      - | 4597 | `	 * (-> 0.0), keep subnormals (nonzero, errno==ERANGE) and a genuine "0" (errno==0). */` |
|     53 | 4598 | `	if( zv != zBuf ){ SyMemcpy(zv,zBuf,(sxu32)nv); }` |
|     53 | 4599 | `	zBuf[nv] = 0;` |
|     53 | 4600 | `	errno = 0;` |
|     53 | 4601 | `	d = strtod(zBuf,0);` |
|     53 | 4602 | `	if( errno == ERANGE && (d == HUGE_VAL \|\| d == -HUGE_VAL \|\| d == 0.0) ){` |
|     15 | 4603 | `		return 0;` |
|      - | 4604 | `	}` |
|     39 | 4605 | `	*pOut = d;` |
|     39 | 4606 | `	return 1;` |
|     35 | 4607 | `}` |
|      - | 4608 | `/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),` |
|      - | 4609 | ` * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as` |
|      - | 4610 | ` * false, NOT failures. */` |
|     33 | 4611 | `static int FvValidateBool(const char *z,int n,int *pBool){` |
|     33 | 4612 | `	FvTrim(&z,&n);` |
|     32 | 4613 | `	if( (n==1 && z[0]=='1') \|\| (n==4 && SyStrnicmp(z,"true",4)==0)` |
|     25 | 4614 | `	    \|\| (n==2 && SyStrnicmp(z,"on",2)==0) \|\| (n==3 && SyStrnicmp(z,"yes",3)==0) ){` |
|     11 | 4615 | `		*pBool = 1; return 1;` |
|      - | 4616 | `	}` |
|     22 | 4617 | `	if( n==0 \|\| (n==1 && z[0]=='0') \|\| (n==5 && SyStrnicmp(z,"false",5)==0)` |
|     11 | 4618 | `	    \|\| (n==3 && SyStrnicmp(z,"off",3)==0) \|\| (n==2 && SyStrnicmp(z,"no",2)==0) ){` |
|     11 | 4619 | `		*pBool = 0; return 1;` |
|      - | 4620 | `	}` |
|      9 | 4621 | `	return 0;` |
|     15 | 4622 | `}` |
|      - | 4623 | `/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. */` |
|     33 | 4624 | `static int FvValidateIp4(const char *z,int n){` |
|     33 | 4625 | `	int i = 0, parts = 0;` |
|     77 | 4626 | `	while( i<n ){` |
|     65 | 4627 | `		int val = 0, digits = 0, start = i;` |
|    143 | 4628 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|     85 | 4629 | `			val = val*10 + (z[i]-'0');` |
|     85 | 4630 | `			if( val>255 ){ return 0; }` |
|     79 | 4631 | `			digits++; i++;` |
|      1 | 4632 | `		}` |
|     59 | 4633 | `		if( digits==0 \|\| digits>3 ){ return 0; }` |
|     49 | 4634 | `		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */` |
|     45 | 4635 | `		parts++;` |
|     45 | 4636 | `		if( parts>4 ){ return 0; }` |
|     45 | 4637 | `		if( i<n ){` |
|     33 | 4638 | `			if( z[i]!='.' ){ return 0; }` |
|     33 | 4639 | `			i++;` |
|     33 | 4640 | `			if( i>=n ){ return 0; } /* trailing dot */` |
|     16 | 4641 | `		}` |
|      1 | 4642 | `	}` |
|     13 | 4643 | `	return parts==4;` |
|     17 | 4644 | `}` |
|      - | 4645 | `/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),` |
|      - | 4646 | ` * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1. */` |
|     19 | 4647 | `static int FvIp6Hextets(const char *z,int n){` |
|     19 | 4648 | `	int i = 0, segStart = 0, groups = 0;` |
|     19 | 4649 | `	if( n==0 ){ return 0; }` |
|    145 | 4650 | `	while( i<=n ){` |
|    133 | 4651 | `		if( i==n \|\| z[i]==':' ){` |
|     23 | 4652 | `			int segLen = i - segStart, j, isV4 = 0;` |
|     23 | 4653 | `			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */` |
|     77 | 4654 | `			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }` |
|     23 | 4655 | `			if( isV4 ){` |
|     11 | 4656 | `				if( i!=n ){ return -1; } /* IPv4 only as the final token */` |
|     11 | 4657 | `				if( !FvValidateIp4(z+segStart,segLen) ){ return -1; }` |
|      5 | 4658 | `				groups += 2;` |
|      3 | 4659 | `			}else{` |
|     13 | 4660 | `				if( segLen>4 ){ return -1; }` |
|     47 | 4661 | `				for( j=segStart; j<i; j++ ){ if( SyHexToint((unsigned char)z[j])<0 ){ return -1; } }` |
|     13 | 4662 | `				groups++;` |
|      - | 4663 | `			}` |
|     17 | 4664 | `			segStart = i+1;` |
|      8 | 4665 | `		}` |
|    127 | 4666 | `		i++;` |
|      1 | 4667 | `	}` |
|     13 | 4668 | `	return groups;` |
|     10 | 4669 | `}` |
|      - | 4670 | `/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present. */` |
|     19 | 4671 | `static int FvValidateIp6(const char *z,int n){` |
|     19 | 4672 | `	const char *zDbl = 0;` |
|      - | 4673 | `	int i, ga, gb;` |
|    139 | 4674 | `	for( i=0; i+1<n; i++ ){` |
|    123 | 4675 | `		if( z[i]==':' && z[i+1]==':' ){` |
|     13 | 4676 | `			if( zDbl ){ return 0; } /* a second "::" is invalid */` |
|     11 | 4677 | `			zDbl = z+i;` |
|      5 | 4678 | `		}` |
|     61 | 4679 | `	}` |
|     17 | 4680 | `	if( zDbl==0 ){` |
|      9 | 4681 | `		return FvIp6Hextets(z,n)==8;` |
|    ! 0 | 4682 | `	}else{` |
|      9 | 4683 | `		int lenA = (int)(zDbl - z);` |
|      9 | 4684 | `		int lenB = n - lenA - 2;` |
|      9 | 4685 | `		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA);` |
|      9 | 4686 | `		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB);` |
|      9 | 4687 | `		if( ga<0 \|\| gb<0 ){ return 0; }` |
|      9 | 4688 | `		return (ga+gb)<=7; /* "::" stands for at least one zero group */` |
|      - | 4689 | `	}` |
|     10 | 4690 | `}` |
|     25 | 4691 | `static int FvValidateIp(const char *z,int n,int flags){` |
|     25 | 4692 | `	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);` |
|     25 | 4693 | `	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */` |
|     25 | 4694 | `	if( v4 && FvValidateIp4(z,n) ){ return 1; }` |
|     21 | 4695 | `	if( v6 && FvValidateIp6(z,n) ){ return 1; }` |
|     13 | 4696 | `	return 0;` |
|     13 | 4697 | `}` |
|      - | 4698 | `/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */` |
|     11 | 4699 | `static int FvValidateMac(const char *z,int n){` |
|      - | 4700 | `	char sep;` |
|      - | 4701 | `	int i;` |
|     11 | 4702 | `	if( n!=17 ){ return 0; }` |
|      7 | 4703 | `	sep = z[2];` |
|      7 | 4704 | `	if( sep!=':' && sep!='-' ){ return 0; }` |
|    105 | 4705 | `	for( i=0; i<17; i++ ){` |
|    101 | 4706 | `		if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }` |
|     71 | 4707 | `		else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|     50 | 4708 | `	}` |
|      5 | 4709 | `	return 1;` |
|      6 | 4710 | `}` |
|      - | 4711 | `/* FILTER_VALIDATE_EMAIL (best-effort: covers the common cases, not quoted local` |
|      - | 4712 | ` * parts or IP-literal domains). */` |
|     28 | 4713 | `static int FvValidateEmail(const char *z,int n){` |
|     28 | 4714 | `	int at = -1, i, localLen, domLen, labelStart, dotCount = 0;` |
|      - | 4715 | `	const char *zDom;` |
|     28 | 4716 | `	if( n==0 \|\| n>320 ){ return 0; }` |
|    201 | 4717 | `	for( i=0; i<n; i++ ){` |
|    181 | 4718 | `		if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }` |
|     91 | 4719 | `	}` |
|     21 | 4720 | `	if( at<=0 \|\| at==n-1 ){ return 0; } /* one '@', non-empty local and domain */` |
|     21 | 4721 | `	localLen = at;` |
|     21 | 4722 | `	zDom = z + at + 1;` |
|     21 | 4723 | `	domLen = n - at - 1;` |
|     21 | 4724 | `	if( z[0]=='.' \|\| z[at-1]=='.' ){ return 0; }` |
|     57 | 4725 | `	for( i=0; i<localLen; i++ ){` |
|     43 | 4726 | `		unsigned char c = (unsigned char)z[i];` |
|     43 | 4727 | `		if( c<=' ' ){ return 0; }` |
|     41 | 4728 | `		if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }` |
|     20 | 4729 | `	}` |
|     15 | 4730 | `	if( zDom[0]=='.' \|\| zDom[domLen-1]=='.' ){ return 0; }` |
|     13 | 4731 | `	labelStart = 0;` |
|     85 | 4732 | `	for( i=0; i<=domLen; i++ ){` |
|     75 | 4733 | `		if( i==domLen \|\| zDom[i]=='.' ){` |
|     25 | 4734 | `			int ll = i - labelStart;` |
|     25 | 4735 | `			if( ll==0 ){ return 0; } /* consecutive dots */` |
|     23 | 4736 | `			if( zDom[labelStart]=='-' \|\| zDom[i-1]=='-' ){ return 0; }` |
|     23 | 4737 | `			if( i<domLen ){ dotCount++; }` |
|     23 | 4738 | `			labelStart = i+1;` |
|     12 | 4739 | `		}else{` |
|     51 | 4740 | `			unsigned char c = (unsigned char)zDom[i];` |
|     51 | 4741 | `			if( !((c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9')\|\|c=='-') ){ return 0; }` |
|      - | 4742 | `		}` |
|     37 | 4743 | `	}` |
|     11 | 4744 | `	if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */` |
|      9 | 4745 | `	return 1;` |
|     15 | 4746 | `}` |
|      - | 4747 | `/* FILTER_VALIDATE_DOMAIN (lenient, matching PHP without FILTER_FLAG_HOSTNAME). */` |
|     11 | 4748 | `static int FvValidateDomain(const char *z,int n){` |
|      - | 4749 | `	int i;` |
|     11 | 4750 | `	if( n<1 \|\| n>253 \|\| z[0]=='.' ){ return 0; }` |
|     81 | 4751 | `	for( i=0; i<n; i++ ){` |
|     75 | 4752 | `		unsigned char c = (unsigned char)z[i];` |
|     75 | 4753 | `		if( c<=' ' ){ return 0; }` |
|     75 | 4754 | `		if( c=='.' && i+1<n && z[i+1]=='.' ){ return 0; }` |
|     37 | 4755 | `	}` |
|      7 | 4756 | `	return 1;` |
|      6 | 4757 | `}` |
|      - | 4758 | `/* FILTER_VALIDATE_URL: require a scheme and a host (PHP's filter is itself` |
|      - | 4759 | ` * parse_url-based, so PH7_VmHttpSplitURI tracks it closely). */` |
|     15 | 4760 | `static int FvValidateUrl(const char *z,int n){` |
|      - | 4761 | `	SyhttpUri sUri;` |
|     15 | 4762 | `	if( n==0 ){ return 0; }` |
|     15 | 4763 | `	SyZero(&sUri,(sxu32)sizeof(sUri));` |
|     15 | 4764 | `	if( PH7_VmHttpSplitURI(&sUri,z,(sxu32)n)!=SXRET_OK ){ return 0; }` |
|     15 | 4765 | `	return sUri.sScheme.nByte!=0 && sUri.sHost.nByte!=0;` |
|      8 | 4766 | `}` |
|      - | 4767 | `/* The Fv sanitizers build their result by appending directly to the call` |
|      - | 4768 | ` * context (ph7_result_string accumulates, like htmlspecialchars), emitting each` |
|      - | 4769 | ` * kept run in one call and seeding "" so an all-stripped input yields "". */` |
|      - | 4770 | `/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */` |
|     37 | 4771 | `static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){` |
|     37 | 4772 | `	int i, runStart = 0;` |
|     37 | 4773 | `	ph7_result_string(pCtx,"",0);` |
|     97 | 4774 | `	for( i=0; i<n; i++ ){` |
|     91 | 4775 | `		char c = z[i];` |
|     91 | 4776 | `		int keep = (c>='0'&&c<='9') \|\| c=='+' \|\| c=='-';` |
|     91 | 4777 | `		if( !keep && isFloat ){` |
|     38 | 4778 | `			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))` |
|     23 | 4779 | `			    \|\| (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))` |
|     36 | 4780 | `			    \|\| ((c=='e'\|\|c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));` |
|     12 | 4781 | `		}` |
|     61 | 4782 | `		if( !keep ){` |
|     33 | 4783 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     33 | 4784 | `			runStart = i+1;` |
|     16 | 4785 | `		}` |
|     31 | 4786 | `	}` |
|      7 | 4787 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 4788 | `}` |
|      - | 4789 | `/* Return non-zero when byte c must be stripped under the STRIP_* flags. Shared` |
|      - | 4790 | ` * by the UNSAFE_RAW string filter and SANITIZE_SPECIAL_CHARS. STRIP_LOW drops` |
|      - | 4791 | `` * bytes <32, STRIP_HIGH drops bytes >=127 (incl. DEL), STRIP_BACKTICK drops '`'.`` |
|      - | 4792 | ` * Matches php_filter_strip(); verified byte-exact vs php 8.5.7. */` |
|    287 | 4793 | `static int FvStripByte(unsigned char c,int flags){` |
|    287 | 4794 | `	if( (flags & FV_FLAG_STRIP_LOW)      && c<32 )    { return 1; }` |
|    281 | 4795 | `	if( (flags & FV_FLAG_STRIP_HIGH)     && c>=127 )  { return 1; }` |
|    269 | 4796 | `	if( (flags & FV_FLAG_STRIP_BACKTICK) && c==0x60 ) { return 1; }` |
|    267 | 4797 | `	return 0;` |
|    144 | 4798 | `}` |
|      - | 4799 | `/* FILTER_UNSAFE_RAW / FILTER_DEFAULT with flags: no default transform, but the` |
|      - | 4800 | ` * STRIP/ENCODE flags apply. Precedence (per php_filter_unsafe_raw, verified` |
|      - | 4801 | ` * vs php 8.5.7): a byte is first tested for stripping; a surviving byte is then` |
|      - | 4802 | ` * encoded as a decimal numeric entity if ENCODE_LOW (<32) / ENCODE_HIGH (>=127)` |
|      - | 4803 | ` * is set, and '&' becomes "&#38;" under ENCODE_AMP. So STRIP_LOW\|ENCODE_LOW` |
|      - | 4804 | ` * strips (nothing left to encode). Bytes are treated individually — ENCODE_HIGH` |
|      - | 4805 | ` * numeric-encodes each byte of a multibyte sequence separately, not the codepoint. */` |
|     25 | 4806 | `static void FvSanitizeString(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 4807 | `	int i, runStart = 0;` |
|     25 | 4808 | `	ph7_result_string(pCtx,"",0);` |
|    193 | 4809 | `	for( i=0; i<n; i++ ){` |
|    179 | 4810 | `		unsigned char c = (unsigned char)z[i];` |
|    179 | 4811 | `		if( FvStripByte(c,flags) ){` |
|     13 | 4812 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     13 | 4813 | `			runStart = i+1;` |
|     13 | 4814 | `			continue;` |
|      - | 4815 | `		}` |
|    167 | 4816 | `		if( c=='&' && (flags & FV_FLAG_ENCODE_AMP) ){` |
|      3 | 4817 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      3 | 4818 | `			ph7_result_string(pCtx,"&#38;",-1);` |
|      3 | 4819 | `			runStart = i+1;` |
|    166 | 4820 | `		}else if( (c<32 && (flags & FV_FLAG_ENCODE_LOW))` |
|    164 | 4821 | `		       \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     37 | 4822 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 4823 | `			ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|      9 | 4824 | `			runStart = i+1;` |
|      4 | 4825 | `		}` |
|     79 | 4826 | `	}` |
|     15 | 4827 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     15 | 4828 | `}` |
|      - | 4829 | `/* FILTER_SANITIZE_SPECIAL_CHARS: encode <>&"' and every control byte <32 as a` |
|      - | 4830 | ` * decimal numeric entity (&#60; &#38; &#34; ...). The STRIP_* flags remove bytes` |
|      - | 4831 | ` * before encoding; ENCODE_HIGH numeric-encodes surviving bytes >=127. Bytes >=128` |
|      - | 4832 | ` * are otherwise passed through verbatim (this filter is NOT UTF-8-aware — only the` |
|      - | 4833 | ` * FULL variant is). Byte-exact vs php 8.5.7. */` |
|     13 | 4834 | `static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int flags){` |
|     13 | 4835 | `	int i, runStart = 0;` |
|      - | 4836 | `	const char *zEnt;` |
|     13 | 4837 | `	ph7_result_string(pCtx,"",0);` |
|    131 | 4838 | `	for( i=0; i<n; i++ ){` |
|    119 | 4839 | `		unsigned char c = (unsigned char)z[i];` |
|    119 | 4840 | `		if( FvStripByte(c,flags) ){` |
|      9 | 4841 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 4842 | `			runStart = i+1;` |
|      9 | 4843 | `			continue;` |
|      - | 4844 | `		}` |
|    111 | 4845 | `		switch( c ){` |
|      3 | 4846 | `		case '<':  zEnt = "&#60;"; break;` |
|      3 | 4847 | `		case '>':  zEnt = "&#62;"; break;` |
|     11 | 4848 | `		case '&':  zEnt = "&#38;"; break;` |
|      3 | 4849 | `		case '"':  zEnt = "&#34;"; break;` |
|      3 | 4850 | `		case '\'': zEnt = "&#39;"; break;` |
|     46 | 4851 | `		default:` |
|      - | 4852 | `			/* Control bytes <32 are always numeric-encoded; bytes >=127 only when` |
|      - | 4853 | `			 * ENCODE_HIGH is set. Everything else stays in the current run. */` |
|     93 | 4854 | `			if( c<32 \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     17 | 4855 | `				if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     17 | 4856 | `				ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|     17 | 4857 | `				runStart = i+1;` |
|      8 | 4858 | `			}` |
|     93 | 4859 | `			continue; /* keep in the current run */` |
|      - | 4860 | `		}` |
|     19 | 4861 | `		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     19 | 4862 | `		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */` |
|     19 | 4863 | `		runStart = i+1;` |
|     10 | 4864 | `	}` |
|     13 | 4865 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     13 | 4866 | `}` |
|      - | 4867 | `/* HTML 4.01 named-entity table (codepoint -> "&name;") used by the UTF-8-aware` |
|      - | 4868 | ` * FULL_SPECIAL_CHARS filter, sorted ascending by codepoint for binary search.` |
|      - | 4869 | ` * Generated from php 8.5.7 (the exact set php_escape_html_entities emits for the` |
|      - | 4870 | ` * default document type); the five inline specials <>&"' are handled separately,` |
|      - | 4871 | ` * so every entry here is a codepoint >=0xA0. 248 rows. */` |
|      - | 4872 | `static const struct { sxu32 cp; const char *zEnt; } aHtml401Ent[] = {` |
|      - | 4873 | `	{0x00A0,"&nbsp;"},{0x00A1,"&iexcl;"},{0x00A2,"&cent;"},{0x00A3,"&pound;"},` |
|      - | 4874 | `	{0x00A4,"&curren;"},{0x00A5,"&yen;"},{0x00A6,"&brvbar;"},{0x00A7,"&sect;"},` |
|      - | 4875 | `	{0x00A8,"&uml;"},{0x00A9,"&copy;"},{0x00AA,"&ordf;"},{0x00AB,"&laquo;"},` |
|      - | 4876 | `	{0x00AC,"&not;"},{0x00AD,"&shy;"},{0x00AE,"&reg;"},{0x00AF,"&macr;"},` |
|      - | 4877 | `	{0x00B0,"&deg;"},{0x00B1,"&plusmn;"},{0x00B2,"&sup2;"},{0x00B3,"&sup3;"},` |
|      - | 4878 | `	{0x00B4,"&acute;"},{0x00B5,"&micro;"},{0x00B6,"&para;"},{0x00B7,"&middot;"},` |
|      - | 4879 | `	{0x00B8,"&cedil;"},{0x00B9,"&sup1;"},{0x00BA,"&ordm;"},{0x00BB,"&raquo;"},` |
|      - | 4880 | `	{0x00BC,"&frac14;"},{0x00BD,"&frac12;"},{0x00BE,"&frac34;"},{0x00BF,"&iquest;"},` |
|      - | 4881 | `	{0x00C0,"&Agrave;"},{0x00C1,"&Aacute;"},{0x00C2,"&Acirc;"},{0x00C3,"&Atilde;"},` |
|      - | 4882 | `	{0x00C4,"&Auml;"},{0x00C5,"&Aring;"},{0x00C6,"&AElig;"},{0x00C7,"&Ccedil;"},` |
|      - | 4883 | `	{0x00C8,"&Egrave;"},{0x00C9,"&Eacute;"},{0x00CA,"&Ecirc;"},{0x00CB,"&Euml;"},` |
|      - | 4884 | `	{0x00CC,"&Igrave;"},{0x00CD,"&Iacute;"},{0x00CE,"&Icirc;"},{0x00CF,"&Iuml;"},` |
|      - | 4885 | `	{0x00D0,"&ETH;"},{0x00D1,"&Ntilde;"},{0x00D2,"&Ograve;"},{0x00D3,"&Oacute;"},` |
|      - | 4886 | `	{0x00D4,"&Ocirc;"},{0x00D5,"&Otilde;"},{0x00D6,"&Ouml;"},{0x00D7,"&times;"},` |
|      - | 4887 | `	{0x00D8,"&Oslash;"},{0x00D9,"&Ugrave;"},{0x00DA,"&Uacute;"},{0x00DB,"&Ucirc;"},` |
|      - | 4888 | `	{0x00DC,"&Uuml;"},{0x00DD,"&Yacute;"},{0x00DE,"&THORN;"},{0x00DF,"&szlig;"},` |
|      - | 4889 | `	{0x00E0,"&agrave;"},{0x00E1,"&aacute;"},{0x00E2,"&acirc;"},{0x00E3,"&atilde;"},` |
|      - | 4890 | `	{0x00E4,"&auml;"},{0x00E5,"&aring;"},{0x00E6,"&aelig;"},{0x00E7,"&ccedil;"},` |
|      - | 4891 | `	{0x00E8,"&egrave;"},{0x00E9,"&eacute;"},{0x00EA,"&ecirc;"},{0x00EB,"&euml;"},` |
|      - | 4892 | `	{0x00EC,"&igrave;"},{0x00ED,"&iacute;"},{0x00EE,"&icirc;"},{0x00EF,"&iuml;"},` |
|      - | 4893 | `	{0x00F0,"&eth;"},{0x00F1,"&ntilde;"},{0x00F2,"&ograve;"},{0x00F3,"&oacute;"},` |
|      - | 4894 | `	{0x00F4,"&ocirc;"},{0x00F5,"&otilde;"},{0x00F6,"&ouml;"},{0x00F7,"&divide;"},` |
|      - | 4895 | `	{0x00F8,"&oslash;"},{0x00F9,"&ugrave;"},{0x00FA,"&uacute;"},{0x00FB,"&ucirc;"},` |
|      - | 4896 | `	{0x00FC,"&uuml;"},{0x00FD,"&yacute;"},{0x00FE,"&thorn;"},{0x00FF,"&yuml;"},` |
|      - | 4897 | `	{0x0152,"&OElig;"},{0x0153,"&oelig;"},{0x0160,"&Scaron;"},{0x0161,"&scaron;"},` |
|      - | 4898 | `	{0x0178,"&Yuml;"},{0x0192,"&fnof;"},{0x02C6,"&circ;"},{0x02DC,"&tilde;"},` |
|      - | 4899 | `	{0x0391,"&Alpha;"},{0x0392,"&Beta;"},{0x0393,"&Gamma;"},{0x0394,"&Delta;"},` |
|      - | 4900 | `	{0x0395,"&Epsilon;"},{0x0396,"&Zeta;"},{0x0397,"&Eta;"},{0x0398,"&Theta;"},` |
|      - | 4901 | `	{0x0399,"&Iota;"},{0x039A,"&Kappa;"},{0x039B,"&Lambda;"},{0x039C,"&Mu;"},` |
|      - | 4902 | `	{0x039D,"&Nu;"},{0x039E,"&Xi;"},{0x039F,"&Omicron;"},{0x03A0,"&Pi;"},` |
|      - | 4903 | `	{0x03A1,"&Rho;"},{0x03A3,"&Sigma;"},{0x03A4,"&Tau;"},{0x03A5,"&Upsilon;"},` |
|      - | 4904 | `	{0x03A6,"&Phi;"},{0x03A7,"&Chi;"},{0x03A8,"&Psi;"},{0x03A9,"&Omega;"},` |
|      - | 4905 | `	{0x03B1,"&alpha;"},{0x03B2,"&beta;"},{0x03B3,"&gamma;"},{0x03B4,"&delta;"},` |
|      - | 4906 | `	{0x03B5,"&epsilon;"},{0x03B6,"&zeta;"},{0x03B7,"&eta;"},{0x03B8,"&theta;"},` |
|      - | 4907 | `	{0x03B9,"&iota;"},{0x03BA,"&kappa;"},{0x03BB,"&lambda;"},{0x03BC,"&mu;"},` |
|      - | 4908 | `	{0x03BD,"&nu;"},{0x03BE,"&xi;"},{0x03BF,"&omicron;"},{0x03C0,"&pi;"},` |
|      - | 4909 | `	{0x03C1,"&rho;"},{0x03C2,"&sigmaf;"},{0x03C3,"&sigma;"},{0x03C4,"&tau;"},` |
|      - | 4910 | `	{0x03C5,"&upsilon;"},{0x03C6,"&phi;"},{0x03C7,"&chi;"},{0x03C8,"&psi;"},` |
|      - | 4911 | `	{0x03C9,"&omega;"},{0x03D1,"&thetasym;"},{0x03D2,"&upsih;"},{0x03D6,"&piv;"},` |
|      - | 4912 | `	{0x2002,"&ensp;"},{0x2003,"&emsp;"},{0x2009,"&thinsp;"},{0x200C,"&zwnj;"},` |
|      - | 4913 | `	{0x200D,"&zwj;"},{0x200E,"&lrm;"},{0x200F,"&rlm;"},{0x2013,"&ndash;"},` |
|      - | 4914 | `	{0x2014,"&mdash;"},{0x2018,"&lsquo;"},{0x2019,"&rsquo;"},{0x201A,"&sbquo;"},` |
|      - | 4915 | `	{0x201C,"&ldquo;"},{0x201D,"&rdquo;"},{0x201E,"&bdquo;"},{0x2020,"&dagger;"},` |
|      - | 4916 | `	{0x2021,"&Dagger;"},{0x2022,"&bull;"},{0x2026,"&hellip;"},{0x2030,"&permil;"},` |
|      - | 4917 | `	{0x2032,"&prime;"},{0x2033,"&Prime;"},{0x2039,"&lsaquo;"},{0x203A,"&rsaquo;"},` |
|      - | 4918 | `	{0x203E,"&oline;"},{0x2044,"&frasl;"},{0x20AC,"&euro;"},{0x2111,"&image;"},` |
|      - | 4919 | `	{0x2118,"&weierp;"},{0x211C,"&real;"},{0x2122,"&trade;"},{0x2135,"&alefsym;"},` |
|      - | 4920 | `	{0x2190,"&larr;"},{0x2191,"&uarr;"},{0x2192,"&rarr;"},{0x2193,"&darr;"},` |
|      - | 4921 | `	{0x2194,"&harr;"},{0x21B5,"&crarr;"},{0x21D0,"&lArr;"},{0x21D1,"&uArr;"},` |
|      - | 4922 | `	{0x21D2,"&rArr;"},{0x21D3,"&dArr;"},{0x21D4,"&hArr;"},{0x2200,"&forall;"},` |
|      - | 4923 | `	{0x2202,"&part;"},{0x2203,"&exist;"},{0x2205,"&empty;"},{0x2207,"&nabla;"},` |
|      - | 4924 | `	{0x2208,"&isin;"},{0x2209,"&notin;"},{0x220B,"&ni;"},{0x220F,"&prod;"},` |
|      - | 4925 | `	{0x2211,"&sum;"},{0x2212,"&minus;"},{0x2217,"&lowast;"},{0x221A,"&radic;"},` |
|      - | 4926 | `	{0x221D,"&prop;"},{0x221E,"&infin;"},{0x2220,"&ang;"},{0x2227,"&and;"},` |
|      - | 4927 | `	{0x2228,"&or;"},{0x2229,"&cap;"},{0x222A,"&cup;"},{0x222B,"&int;"},` |
|      - | 4928 | `	{0x2234,"&there4;"},{0x223C,"&sim;"},{0x2245,"&cong;"},{0x2248,"&asymp;"},` |
|      - | 4929 | `	{0x2260,"&ne;"},{0x2261,"&equiv;"},{0x2264,"&le;"},{0x2265,"&ge;"},` |
|      - | 4930 | `	{0x2282,"&sub;"},{0x2283,"&sup;"},{0x2284,"&nsub;"},{0x2286,"&sube;"},` |
|      - | 4931 | `	{0x2287,"&supe;"},{0x2295,"&oplus;"},{0x2297,"&otimes;"},{0x22A5,"&perp;"},` |
|      - | 4932 | `	{0x22C5,"&sdot;"},{0x2308,"&lceil;"},{0x2309,"&rceil;"},{0x230A,"&lfloor;"},` |
|      - | 4933 | `	{0x230B,"&rfloor;"},{0x2329,"&lang;"},{0x232A,"&rang;"},{0x25CA,"&loz;"},` |
|      - | 4934 | `	{0x2660,"&spades;"},{0x2663,"&clubs;"},{0x2665,"&hearts;"},{0x2666,"&diams;"}` |
|      - | 4935 | `};` |
|      - | 4936 | `/* Binary-search aHtml401Ent[] for cp; return its "&name;" entity or 0. */` |
|     41 | 4937 | `static const char *FvHtml401Lookup(sxu32 cp){` |
|     41 | 4938 | `	int lo = 0, hi = (int)SX_ARRAYSIZE(aHtml401Ent) - 1;` |
|    323 | 4939 | `	while( lo <= hi ){` |
|    309 | 4940 | `		int mid = (lo + hi) / 2;` |
|    309 | 4941 | `		sxu32 c = aHtml401Ent[mid].cp;` |
|    309 | 4942 | `		if( c == cp ){ return aHtml401Ent[mid].zEnt; }` |
|    283 | 4943 | `		if( c < cp ){ lo = mid + 1; } else { hi = mid - 1; }` |
|      1 | 4944 | `	}` |
|     15 | 4945 | `	return 0;` |
|     21 | 4946 | `}` |
|      - | 4947 | `/* Decode one strict-UTF-8 sequence at p (< zEnd). On success returns its byte` |
|      - | 4948 | ` * length (1..4) and sets *pCp to the codepoint; on any malformed, overlong,` |
|      - | 4949 | ` * surrogate, truncated or out-of-range (>U+10FFFF) sequence returns 0. Matches` |
|      - | 4950 | ` * PHP's UTF-8 validation used by FULL_SPECIAL_CHARS (verified vs php 8.5.7). */` |
|    101 | 4951 | `static int FvUtf8Next(const unsigned char *p,const unsigned char *zEnd,sxu32 *pCp){` |
|    101 | 4952 | `	unsigned char c = p[0];` |
|    101 | 4953 | `	if( c < 0x80 ){ *pCp = c; return 1; }` |
|    101 | 4954 | `	if( c < 0xC2 ){ return 0; }              /* 0x80-0xBF stray cont / 0xC0-0xC1 overlong */` |
|     99 | 4955 | `	if( c < 0xE0 ){                          /* 2-byte: U+0080..U+07FF */` |
|     47 | 4956 | `		if( zEnd-p < 2 \|\| (p[1]&0xC0)!=0x80 ){ return 0; }` |
|     45 | 4957 | `		*pCp = ((sxu32)(c&0x1F)<<6) \| (p[1]&0x3F);` |
|     45 | 4958 | `		return 2;` |
|      - | 4959 | `	}` |
|     53 | 4960 | `	if( c < 0xF0 ){                          /* 3-byte: U+0800..U+FFFF minus surrogates */` |
|      - | 4961 | `		sxu32 cp;` |
|     47 | 4962 | `		if( zEnd-p < 3 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 ){ return 0; }` |
|     33 | 4963 | `		cp = ((sxu32)(c&0x0F)<<12) \| ((sxu32)(p[1]&0x3F)<<6) \| (p[2]&0x3F);` |
|     33 | 4964 | `		if( cp < 0x800 \|\| (cp>=0xD800 && cp<=0xDFFF) ){ return 0; }` |
|     29 | 4965 | `		*pCp = cp;` |
|     29 | 4966 | `		return 3;` |
|      - | 4967 | `	}` |
|      7 | 4968 | `	if( c < 0xF5 ){                          /* 4-byte: U+10000..U+10FFFF */` |
|      - | 4969 | `		sxu32 cp;` |
|      5 | 4970 | `		if( zEnd-p < 4 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 \|\| (p[3]&0xC0)!=0x80 ){ return 0; }` |
|      5 | 4971 | `		cp = ((sxu32)(c&0x07)<<18) \| ((sxu32)(p[1]&0x3F)<<12) \| ((sxu32)(p[2]&0x3F)<<6) \| (p[3]&0x3F);` |
|      5 | 4972 | `		if( cp < 0x10000 \|\| cp > 0x10FFFF ){ return 0; }` |
|      5 | 4973 | `		*pCp = cp;` |
|      5 | 4974 | `		return 4;` |
|      - | 4975 | `	}` |
|      3 | 4976 | `	return 0;                                /* 0xF5-0xFF */` |
|     51 | 4977 | `}` |
|      - | 4978 | `/* FILTER_SANITIZE_FULL_SPECIAL_CHARS: htmlentities-style, UTF-8-aware. Encodes` |
|      - | 4979 | ` * <>&"' as named entities ("'" -> &#039;; quotes suppressed under NO_ENCODE_QUOTES),` |
|      - | 4980 | ` * and every valid UTF-8 codepoint with an HTML 4.01 named entity as that entity;` |
|      - | 4981 | ` * valid codepoints without a named entity (and low control bytes) pass through` |
|      - | 4982 | ` * verbatim. If the input contains ANY invalid UTF-8 the whole result is "".` |
|      - | 4983 | ` * The STRIP/ENCODE flags do NOT apply to this filter (only NO_ENCODE_QUOTES).` |
|      - | 4984 | ` * php's filter does NOT re-encode valid pre-existing entities ("&amp;" stays,` |
|      - | 4985 | ` * "&bogus;" becomes "&amp;bogus;"), i.e. double_encode=false semantics —` |
|      - | 4986 | ` * exactly htmlentities(ENT_QUOTES\|ENT_HTML401, double_encode: false), so this` |
|      - | 4987 | ` * delegates to the shared encoder. Byte-exact vs php 8.5.7. */` |
|     25 | 4988 | `static void FvSanitizeFull(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 4989 | `	int iEntFlags = (flags & FV_FLAG_NO_ENCODE_QUOTES) ? 0 : PH7_ENT_QUOTES;` |
|     25 | 4990 | `	HtmlEscape(pCtx,z,n,iEntFlags,1/*bAll*/,0/*bDoubleEncode*/);` |
|     25 | 4991 | `}` |
|      - | 4992 | `/* ---------------------------------------------------------------------------` |
|      - | 4993 | ` * UTF-8-aware HTML entity core (htmlspecialchars/htmlentities family).` |
|      - | 4994 | ` * Prototyped next to the five builtins earlier in this file; lives here so it` |
|      - | 4995 | ` * can share aHtml401Ent[]/FvHtml401Lookup()/FvUtf8Next() with the filter_var` |
|      - | 4996 | ` * FULL_SPECIAL_CHARS filter above. Byte-exact vs php 8.5.7 (oracle-swept).` |
|      - | 4997 | ` * ------------------------------------------------------------------------ */` |
|      - | 4998 | `/* Encode cp as UTF-8 into zBuf (>= 4 bytes); return the byte length 1..4.` |
|      - | 4999 | ` * Thin wrapper over the engine-wide SX_WRITE_UTF8 (sxmacros.h). */` |
|    585 | 5000 | `static int HtmlCpUtf8(sxu32 cp,char *zBuf){` |
|    585 | 5001 | `	sxu8 *z = (sxu8 *)zBuf;` |
|    585 | 5002 | `	SX_WRITE_UTF8(z,cp);` |
|    585 | 5003 | `	return (int)(z - (sxu8 *)zBuf);` |
|      1 | 5004 | `}` |
|      - | 5005 | `/* Doctype-allowed codepoint test (php's unicode_cp_is_allowed) — gates what a` |
|      - | 5006 | ` * numeric reference may DECODE to. Oracle-pinned per doctype: HTML401` |
|      - | 5007 | ` * disallows C0 (except TAB/LF/CR) and DEL..U+009F; XML1 and XHTML share the` |
|      - | 5008 | ` * XML rules — DEL..U+009F allowed, U+FFFE/U+FFFF excluded; HTML5 swaps CR` |
|      - | 5009 | ` * for FF (0x0C) and excludes the noncharacters (U+FDD0..U+FDEF and every` |
|      - | 5010 | ` * U+xFFFE/U+xFFFF). Surrogates are disallowed everywhere. */` |
|     91 | 5011 | `static int HtmlCpAllowed(sxu32 cp,int iFlags){` |
|     91 | 5012 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|     91 | 5013 | `	if( cp==0x09 \|\| cp==0x0A ){ return 1; }` |
|     87 | 5014 | `	if( cp==0x0D ){ return iDoc != PH7_ENT_DOC_HTML5; }` |
|     85 | 5015 | `	if( cp==0x0C ){ return iDoc == PH7_ENT_DOC_HTML5; }` |
|     85 | 5016 | `	if( cp < 0x20 \|\| cp > 0x10FFFF ){ return 0; }` |
|     79 | 5017 | `	if( cp>=0xD800 && cp<=0xDFFF ){ return 0; }` |
|     77 | 5018 | `	if( cp>=0x7F && cp<=0x9F ){ return iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML; }` |
|     71 | 5019 | `	if( iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML ){` |
|    ! 0 | 5020 | `		return cp!=0xFFFE && cp!=0xFFFF;` |
|      - | 5021 | `	}` |
|     71 | 5022 | `	if( iDoc == PH7_ENT_DOC_HTML5 ){` |
|      9 | 5023 | `		if( cp>=0xFDD0 && cp<=0xFDEF ){ return 0; }` |
|      9 | 5024 | `		if( (cp & 0xFFFF) >= 0xFFFE ){ return 0; }` |
|      4 | 5025 | `	}` |
|     71 | 5026 | `	return 1;` |
|     46 | 5027 | `}` |
|      - | 5028 | `/* The ENT_DISALLOWED gate for RAW characters on the ENCODE side. Same as the` |
|      - | 5029 | ` * decode gate except CR under HTML5: php's encode-side unicode_cp_is_allowed` |
|      - | 5030 | ` * keeps a literal "\r" verbatim under ENT_HTML5\|ENT_DISALLOWED while the` |
|      - | 5031 | ` * decode side leaves "&#13;" un-decoded (oracle-pinned at flags 176). */` |
|      9 | 5032 | `static int HtmlCpAllowedEncode(sxu32 cp,int iFlags){` |
|      9 | 5033 | `	if( cp==0x0D && (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 ){ return 1; }` |
|      9 | 5034 | `	return HtmlCpAllowed(cp,iFlags);` |
|      5 | 5035 | `}` |
|      - | 5036 | `/* Numeric-reference validity for the double_encode=false "is this already a` |
|      - | 5037 | ` * valid entity" test — a MUCH looser predicate than the decode gate above:` |
|      - | 5038 | ` * any codepoint <= U+10FFFF is valid (controls and surrogates included, every` |
|      - | 5039 | ` * doctype). ENT_DISALLOWED re-tightens non-HTML401 doctypes to the decode` |
|      - | 5040 | ` * gate, except that HTML5 exempts surrogates. All oracle-pinned: &#0; and` |
|      - | 5041 | ` * &#xD800; stay verbatim at flags 11 and 139; flags -1 (HTML5+DISALLOWED)` |
|      - | 5042 | ` * re-encodes &#0; and &#x10FFFF; but still keeps &#xD800;; flags 144` |
|      - | 5043 | ` * (XML1+DISALLOWED) re-encodes &#xD800;. */` |
|      9 | 5044 | `static int HtmlNumericAllowed(sxu32 cp,int iFlags){` |
|      9 | 5045 | `	if( cp > 0x10FFFF ){ return 0; }` |
|      7 | 5046 | `	if( (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML401 ){ return 1; /* never tightened */ }` |
|    ! 0 | 5047 | `	if( (iFlags & PH7_ENT_DISALLOWED)` |
|    ! 0 | 5048 | `	 && !((iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 && cp>=0xD800 && cp<=0xDFFF)` |
|    ! 0 | 5049 | `	 && !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|    ! 0 | 5050 | `	return 1;` |
|      5 | 5051 | `}` |
|      - | 5052 | `/* How many bytes the malformed UTF-8 sequence at p consumes — php's` |
|      - | 5053 | ` * get_next_char failure step (one U+FFFD substitution / one ENT_IGNORE drop` |
|      - | 5054 | ` * per MAXIMAL invalid subpart, not per byte): a prefix-valid sequence eats` |
|      - | 5055 | ` * its continuation bytes ("\xE0\x80\xAF" is ONE unit) while a byte that could` |
|      - | 5056 | ` * start a new sequence is left for the next round. */` |
|      5 | 5057 | `static int HtmlUtf8Trail(unsigned char c){ return c>=0x80 && c<=0xBF; }` |
|     11 | 5058 | `static int HtmlUtf8Lead(unsigned char c){ return c<0x80 \|\| (c>=0xC2 && c<=0xF4); }` |
|     15 | 5059 | `static int HtmlUtf8FailAdvance(const unsigned char *p,const unsigned char *zEnd){` |
|     15 | 5060 | `	unsigned char c = p[0];` |
|     15 | 5061 | `	int nAvail = (int)(zEnd - p);` |
|     15 | 5062 | `	if( c < 0xC2 \|\| c > 0xF4 ){ return 1; } /* stray trail / C0-C1 / F5-FF */` |
|     13 | 5063 | `	if( c < 0xE0 ){` |
|      3 | 5064 | `		if( nAvail < 2 ){ return 1; }` |
|      3 | 5065 | `		return HtmlUtf8Lead(p[1]) ? 1 : 2;` |
|      - | 5066 | `	}` |
|     11 | 5067 | `	if( c < 0xF0 ){` |
|     11 | 5068 | `		if( nAvail >= 3 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) ){` |
|      3 | 5069 | `			return 3; /* complete but overlong/surrogate */` |
|      - | 5070 | `		}` |
|      9 | 5071 | `		if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 5072 | `		if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 5073 | `		return 3;` |
|      - | 5074 | `	}` |
|    ! 0 | 5075 | `	if( nAvail >= 4 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) && HtmlUtf8Trail(p[3]) ){` |
|    ! 0 | 5076 | `		return 4; /* complete but overlong / > U+10FFFF */` |
|      - | 5077 | `	}` |
|    ! 0 | 5078 | `	if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 5079 | `	if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 5080 | `	if( nAvail < 4 \|\| HtmlUtf8Lead(p[3]) ){ return 3; }` |
|    ! 0 | 5081 | `	return 4;` |
|      8 | 5082 | `}` |
|      - | 5083 | `/* The basic special entities, shared by named matching, the hsc_decode` |
|      - | 5084 | ` * numeric whitelist and the translation-table builder so the sets can never` |
|      - | 5085 | ` * drift apart. (&apos; is not an HTML 4.01 entity — doctype-gated below.) */` |
|      - | 5086 | `static const struct { const char *zEnt; int n; sxu32 cp; } aHtmlSpecEnt[] = {` |
|      - | 5087 | `	{"&amp;",5,38},{"&lt;",4,60},{"&gt;",4,62},{"&quot;",6,34},{"&apos;",6,39}` |
|      - | 5088 | `};` |
|      - | 5089 | `/* Does this doctype consult the named-entity table (aHtml401Ent)? XML 1.0 has` |
|      - | 5090 | ` * no named entities beyond the specials; XHTML/HTML5 are approximated by the` |
|      - | 5091 | ` * HTML 4.01 table (documented divergence, PLAN.md §3.9). */` |
|     63 | 5092 | `static int HtmlDocHasNamedTable(int iDoc){` |
|     63 | 5093 | `	return iDoc != PH7_ENT_DOC_XML1;` |
|      1 | 5094 | `}` |
|      - | 5095 | `/* The single-quote entity per doctype. Oracle-pinned asymmetry: for every` |
|      - | 5096 | ` * non-HTML401 doctype htmlspecialchars emits &apos; while htmlentities` |
|      - | 5097 | ` * (bEntities) keeps &#039; under XHTML too. The translation table mirrors` |
|      - | 5098 | ` * whichever function the requested table belongs to. */` |
|     29 | 5099 | `static const char *HtmlAposEntity(int iDoc,int bEntities){` |
|     29 | 5100 | `	if( iDoc == PH7_ENT_DOC_HTML401 \|\| (bEntities && iDoc == PH7_ENT_DOC_XHTML) ){` |
|     21 | 5101 | `		return "&#039;";` |
|      - | 5102 | `	}` |
|      9 | 5103 | `	return "&apos;";` |
|     15 | 5104 | `}` |
|      - | 5105 | `/* Try to parse one HTML entity at z (z[0]=='&', z < zEnd). bFull selects the` |
|      - | 5106 | ` * html_entity_decode set (doctype named table + any allowed numeric ref) vs` |
|      - | 5107 | ` * the htmlspecialchars_decode set (the basic specials + quote numerics only).` |
|      - | 5108 | ` * Named matching is case-SENSITIVE and the ';' is required (both PHP-exact);` |
|      - | 5109 | ` * numeric refs accept dec/hex (x or X) with any number of leading zeros but` |
|      - | 5110 | ` * reject out-of-range, surrogate and doctype-disallowed codepoints (the` |
|      - | 5111 | ` * caller then leaves the source verbatim). Quote-flag gating is NOT applied` |
|      - | 5112 | ` * here — the same routine doubles as the "is this a valid entity" test for` |
|      - | 5113 | ` * double_encode=false, which ignores the quote bits (oracle-pinned).` |
|      - | 5114 | ` * bEncodeCheck selects the looser HtmlNumericAllowed predicate used by that` |
|      - | 5115 | ` * double_encode test; decode callers pass 0 for the HtmlCpAllowed gate.` |
|      - | 5116 | ` * On success sets *pCp / *pnConsumed and returns 1. */` |
|    172 | 5117 | `static int HtmlParseEntity(const unsigned char *z,const unsigned char *zEnd,` |
|      1 | 5118 | `                           int iFlags,int bFull,int bEncodeCheck,sxu32 *pCp,int *pnConsumed){` |
|    173 | 5119 | `	int nAvail = (int)(zEnd - z);` |
|    173 | 5120 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 5121 | `	sxu32 n;` |
|    173 | 5122 | `	if( nAvail < 4 ){ return 0; } /* shortest entities: &lt; &#9; */` |
|    169 | 5123 | `	if( z[1] == '#' ){` |
|      - | 5124 | `		/* Numeric reference */` |
|     89 | 5125 | `		sxu32 cp = 0;` |
|     89 | 5126 | `		int i = 2, bHex = 0, nDig = 0;` |
|     89 | 5127 | `		if( z[i]=='x' \|\| z[i]=='X' ){ bHex = 1; i++; }` |
|    317 | 5128 | `		for( ; i < nAvail && z[i] != ';' ; i++ ){` |
|      - | 5129 | `			int v;` |
|    221 | 5130 | `			unsigned char c = z[i];` |
|    221 | 5131 | `			if( c>='0' && c<='9' ){ v = c - '0'; }` |
|     17 | 5132 | `			else if( bHex && c>='a' && c<='f' ){ v = c - 'a' + 10; }` |
|     17 | 5133 | `			else if( bHex && c>='A' && c<='F' ){ v = c - 'A' + 10; }` |
|    ! 0 | 5134 | `			else { return 0; }` |
|      - | 5135 | `			/* Stop accumulating once out of range (keeps validating the shape;` |
|      - | 5136 | `			 * max intermediate is 0x10FFFF*16+15, no sxu32 overflow). */` |
|    221 | 5137 | `			if( cp <= 0x10FFFF ){ cp = cp * (bHex ? 16 : 10) + (sxu32)v; }` |
|    221 | 5138 | `			nDig++;` |
|    111 | 5139 | `		}` |
|     97 | 5140 | `		if( nDig == 0 \|\| i >= nAvail ){ return 0; } /* no digits / no ';' */` |
|     97 | 5141 | `		if( bEncodeCheck ? !HtmlNumericAllowed(cp,iFlags) : !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|     83 | 5142 | `		if( !bFull ){` |
|      - | 5143 | `			/* hsc_decode: numeric refs to the five specials only. */` |
|     99 | 5144 | `			for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) && aHtmlSpecEnt[n].cp != cp ; n++ ){}` |
|     25 | 5145 | `			if( n >= SX_ARRAYSIZE(aHtmlSpecEnt) ){ return 0; }` |
|     11 | 5146 | `		}` |
|     75 | 5147 | `		*pCp = cp;` |
|     75 | 5148 | `		*pnConsumed = i + 1;` |
|     75 | 5149 | `		return 1;` |
|      - | 5150 | `	}` |
|      - | 5151 | `	/* Named reference — every entity name starts with a letter, so anything` |
|      - | 5152 | `	 * else can bail out before touching the tables. */` |
|     81 | 5153 | `	if( !((z[1]>='a' && z[1]<='z') \|\| (z[1]>='A' && z[1]<='Z')) ){ return 0; }` |
|    287 | 5154 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) ; n++ ){` |
|    265 | 5155 | `		if( aHtmlSpecEnt[n].cp == 39 && iDoc == PH7_ENT_DOC_HTML401 ){ continue; }` |
|    243 | 5156 | `		if( nAvail >= aHtmlSpecEnt[n].n && SyMemcmp(z,aHtmlSpecEnt[n].zEnt,(sxu32)aHtmlSpecEnt[n].n) == 0 ){` |
|     53 | 5157 | `			*pCp = aHtmlSpecEnt[n].cp;` |
|     53 | 5158 | `			*pnConsumed = aHtmlSpecEnt[n].n;` |
|     53 | 5159 | `			return 1;` |
|      - | 5160 | `		}` |
|     96 | 5161 | `	}` |
|     23 | 5162 | `	if( bFull && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 5163 | `		/* Linear scan of the 248-row table: runs only at '&'-then-letter` |
|      - | 5164 | `		 * positions and guarantees the decode set can never drift from the` |
|      - | 5165 | `		 * encode table. The first-letter guard skips the SyStrlen/SyMemcmp` |
|      - | 5166 | `		 * for ~96% of rows. */` |
|   3369 | 5167 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|      - | 5168 | `			sxu32 nEnt;` |
|   3357 | 5169 | `			if( z[1] != (unsigned char)aHtml401Ent[n].zEnt[1] ){ continue; }` |
|    121 | 5170 | `			nEnt = SyStrlen(aHtml401Ent[n].zEnt);` |
|    121 | 5171 | `			if( (sxu32)nAvail >= nEnt && SyMemcmp(z,aHtml401Ent[n].zEnt,nEnt) == 0 ){` |
|      7 | 5172 | `				*pCp = aHtml401Ent[n].cp;` |
|      7 | 5173 | `				*pnConsumed = (int)nEnt;` |
|      7 | 5174 | `				return 1;` |
|      - | 5175 | `			}` |
|     58 | 5176 | `		}` |
|      6 | 5177 | `	}` |
|     17 | 5178 | `	return 0;` |
|     88 | 5179 | `}` |
|      - | 5180 | `/* Shared encoder for htmlspecialchars (bAll=0) and htmlentities (bAll=1).` |
|      - | 5181 | ` * Invalid UTF-8 policy: ENT_IGNORE drops the byte (and wins over SUBSTITUTE),` |
|      - | 5182 | ` * ENT_SUBSTITUTE emits one U+FFFD per invalid byte, neither -> the whole` |
|      - | 5183 | ` * result is "" (pre-validated in a first pass: the accumulating result API` |
|      - | 5184 | ` * cannot roll back — same reason FvSanitizeFull is two-pass). */` |
|     94 | 5185 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 5186 | `                       int iFlags,int bAll,int bDoubleEncode){` |
|     95 | 5187 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     95 | 5188 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|      - | 5189 | `	const unsigned char *runStart;` |
|     95 | 5190 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 5191 | `	sxu32 cp;` |
|     95 | 5192 | `	if( (iFlags & (PH7_ENT_IGNORE\|PH7_ENT_SUBSTITUTE)) == 0 ){` |
|      - | 5193 | `		/* Pass 1: any malformed sequence rejects the entire input. ASCII` |
|      - | 5194 | `		 * bytes cannot be malformed, so skip them without the decoder. */` |
|    381 | 5195 | `		while( p < zEnd ){` |
|      - | 5196 | `			int len;` |
|    323 | 5197 | `			if( *p < 0x80 ){ p++; continue; }` |
|     37 | 5198 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     37 | 5199 | `			if( len == 0 ){ ph7_result_string(pCtx,"",0); return; }` |
|     27 | 5200 | `			p += len;` |
|      1 | 5201 | `		}` |
|     59 | 5202 | `		p = (const unsigned char *)zIn;` |
|     29 | 5203 | `	}` |
|     85 | 5204 | `	runStart = p;` |
|     85 | 5205 | `	ph7_result_string(pCtx,"",0);` |
|    455 | 5206 | `	while( p < zEnd ){` |
|    371 | 5207 | `		const char *zEnt = 0;` |
|      - | 5208 | `		int len;` |
|    371 | 5209 | `		if( *p < 0x80 ){` |
|    307 | 5210 | `			len = 1;` |
|    307 | 5211 | `			switch( *p ){` |
|     25 | 5212 | `			case '<': zEnt = "&lt;"; break;` |
|     25 | 5213 | `			case '>': zEnt = "&gt;"; break;` |
|     18 | 5214 | `			case '&':` |
|     37 | 5215 | `				zEnt = "&amp;";` |
|     37 | 5216 | `				if( !bDoubleEncode ){` |
|      - | 5217 | `					sxu32 eCp; int nEat;` |
|     25 | 5218 | `					if( HtmlParseEntity(p,zEnd,iFlags,1,1,&eCp,&nEat) ){` |
|      - | 5219 | `						/* A valid existing entity: keep it verbatim. */` |
|     13 | 5220 | `						zEnt = 0;` |
|     13 | 5221 | `						len = nEat;` |
|      6 | 5222 | `					}` |
|     12 | 5223 | `				}` |
|     37 | 5224 | `				break;` |
|     10 | 5225 | `			case '"':` |
|     21 | 5226 | `				if( iFlags & PH7_ENT_QUOTE_DOUBLE ){ zEnt = "&quot;"; }` |
|     21 | 5227 | `				break;` |
|     12 | 5228 | `			case '\'':` |
|     25 | 5229 | `				if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|     23 | 5230 | `					zEnt = HtmlAposEntity(iDoc,bAll);` |
|     11 | 5231 | `				}` |
|     25 | 5232 | `				break;` |
|     89 | 5233 | `			default:` |
|    179 | 5234 | `				if( (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode((sxu32)*p,iFlags) ){` |
|    ! 0 | 5235 | `					zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 5236 | `				}` |
|    178 | 5237 | `				break;` |
|      - | 5238 | `			}` |
|    154 | 5239 | `		}else{` |
|     65 | 5240 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     65 | 5241 | `			if( len == 0 ){` |
|      - | 5242 | `				/* Malformed subpart (IGNORE or SUBSTITUTE is set, else pass 1` |
|      - | 5243 | `				 * would have rejected): drop it or emit ONE U+FFFD for the` |
|      - | 5244 | `				 * whole unit (php substitutes per maximal invalid subpart). */` |
|     15 | 5245 | `				if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|     15 | 5246 | `				if( (iFlags & PH7_ENT_IGNORE) == 0 ){ ph7_result_string(pCtx,"\xEF\xBF\xBD",3); }` |
|     15 | 5247 | `				p += HtmlUtf8FailAdvance(p,zEnd);` |
|     15 | 5248 | `				runStart = p;` |
|     15 | 5249 | `				continue;` |
|      - | 5250 | `			}` |
|     51 | 5251 | `			if( bAll && HtmlDocHasNamedTable(iDoc) ){` |
|     41 | 5252 | `				zEnt = FvHtml401Lookup(cp);` |
|     20 | 5253 | `			}` |
|     51 | 5254 | `			if( zEnt == 0 && (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode(cp,iFlags) ){` |
|    ! 0 | 5255 | `				zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 5256 | `			}` |
|      - | 5257 | `		}` |
|    357 | 5258 | `		if( zEnt ){` |
|    135 | 5259 | `			if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|    135 | 5260 | `			ph7_result_string(pCtx,zEnt,-1);` |
|    135 | 5261 | `			runStart = p + len;` |
|     67 | 5262 | `		}` |
|    357 | 5263 | `		p += len;` |
|      1 | 5264 | `	}` |
|     85 | 5265 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     48 | 5266 | `}` |
|      - | 5267 | `/* Shared decoder for html_entity_decode (bFull=1) and htmlspecialchars_decode` |
|      - | 5268 | ` * (bFull=0). Quote refs (cp 34/39, named or numeric) are gated by the quote` |
|      - | 5269 | ` * bits and left verbatim when suppressed; an invalid entity leaves its '&'` |
|      - | 5270 | ` * verbatim and rescans right after it, which also yields PHP's no-double-` |
|      - | 5271 | ` * decode behavior ("&amp;lt;" -> "&lt;"). */` |
|     82 | 5272 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 5273 | `                         int iFlags,int bFull){` |
|     83 | 5274 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     83 | 5275 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|     83 | 5276 | `	const unsigned char *runStart = p;` |
|     83 | 5277 | `	ph7_result_string(pCtx,"",0);` |
|    557 | 5278 | `	while( p < zEnd ){` |
|      - | 5279 | `		sxu32 cp;` |
|      - | 5280 | `		int nEat;` |
|    510 | 5281 | `		if( *p != '&' ){ p++; continue; }` |
|    155 | 5282 | `		if( !HtmlParseEntity(p,zEnd,iFlags,bFull,0,&cp,&nEat) ){ p++; continue; }` |
|    124 | 5283 | `		if( (cp == 34 && (iFlags & PH7_ENT_QUOTE_DOUBLE) == 0)` |
|    117 | 5284 | `		 \|\| (cp == 39 && (iFlags & PH7_ENT_QUOTE_SINGLE) == 0) ){` |
|      - | 5285 | `			/* Suppressed quote: leave the entity source verbatim. */` |
|     37 | 5286 | `			p += nEat;` |
|     37 | 5287 | `			continue;` |
|      - | 5288 | `		}` |
|     89 | 5289 | `		if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|      - | 5290 | `		{` |
|      - | 5291 | `			char zBuf[4];` |
|     89 | 5292 | `			int n = HtmlCpUtf8(cp,zBuf);` |
|     89 | 5293 | `			ph7_result_string(pCtx,zBuf,n);` |
|      - | 5294 | `		}` |
|     89 | 5295 | `		p += nEat;` |
|     89 | 5296 | `		runStart = p;` |
|      1 | 5297 | `	}` |
|     79 | 5298 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     79 | 5299 | `}` |
|      - | 5300 | `/* Validate the optional charset argument at apArg[idx]: UTF-8 aliases (and` |
|      - | 5301 | ` * ""/NULL meaning the default) are accepted; anything else — including` |
|      - | 5302 | ` * php-supported single-byte charsets like ISO-8859-1, PHL is UTF-8-only per` |
|      - | 5303 | ` * PLAN.md §6 — raises PHP's unsupported-charset warning and is treated as` |
|      - | 5304 | ` * UTF-8 (ph7_context_throw_error_format prepends the function name). */` |
|    141 | 5305 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx){` |
|      - | 5306 | `	const char *zCs;` |
|      - | 5307 | `	int nCs;` |
|    148 | 5308 | `	if( nArg <= idx \|\| ph7_value_is_null(apArg[idx]) ){ return; }` |
|     15 | 5309 | `	zCs = ph7_value_to_string(apArg[idx],&nCs);` |
|     15 | 5310 | `	if( nCs == 0 ){ return; } /* "" selects the default charset (UTF-8) */` |
|     13 | 5311 | `	if( nCs == 5 && SyStrnicmp(zCs,"UTF-8",5) == 0 ){` |
|     13 | 5312 | `		return; /* php accepts only "UTF-8" (any case) silently — "UTF8" warns */` |
|      - | 5313 | `	}` |
|    ! 0 | 5314 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5315 | `		"Charset \"%.*s\" is not supported, assuming UTF-8",nCs,zCs);` |
|     71 | 5316 | `}` |
|      - | 5317 | `/* get_html_translation_table() worker: character (UTF-8 bytes) => entity.` |
|      - | 5318 | ` * The five specials come first in byte order, then — for HTML_ENTITIES with a` |
|      - | 5319 | ` * named-table doctype — the 248 aHtml401Ent rows ascending (oracle-pinned` |
|      - | 5320 | ` * ordering; 253 entries under the defaults). */` |
|    549 | 5321 | `static void HtmlTableAdd(ph7_value *pArray,ph7_value *pValue,const char *zKey,const char *zEnt){` |
|    549 | 5322 | `	ph7_value_string(pValue,zEnt,-1);` |
|    549 | 5323 | `	ph7_array_add_strkey_elem(pArray,zKey,pValue);` |
|    549 | 5324 | `	ph7_value_reset_string_cursor(pValue);` |
|    549 | 5325 | `}` |
|     13 | 5326 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags){` |
|      - | 5327 | `	ph7_value *pArray,*pValue;` |
|     13 | 5328 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 5329 | `	sxu32 n;` |
|     13 | 5330 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 5331 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 5332 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|    ! 0 | 5333 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5334 | `		return;` |
|      - | 5335 | `	}` |
|     13 | 5336 | `	if( iFlags & PH7_ENT_QUOTE_DOUBLE ){` |
|     11 | 5337 | `		HtmlTableAdd(pArray,pValue,"\"","&quot;");` |
|      5 | 5338 | `	}` |
|     13 | 5339 | `	HtmlTableAdd(pArray,pValue,"&","&amp;");` |
|     13 | 5340 | `	if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|      - | 5341 | `		/* The apostrophe row mirrors the function each table belongs to:` |
|      - | 5342 | `		 * SPECIALCHARS follows htmlspecialchars, ENTITIES follows` |
|      - | 5343 | `		 * htmlentities (oracle-pinned at flags 35). */` |
|      7 | 5344 | `		HtmlTableAdd(pArray,pValue,"'",HtmlAposEntity(iDoc,iTable != 0));` |
|      3 | 5345 | `	}` |
|     13 | 5346 | `	HtmlTableAdd(pArray,pValue,"<","&lt;");` |
|     13 | 5347 | `	HtmlTableAdd(pArray,pValue,">","&gt;");` |
|     13 | 5348 | `	if( iTable != 0 /*php: any non-HTML_SPECIALCHARS table => entities*/ && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 5349 | `		char zKey[8];` |
|    499 | 5350 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|    497 | 5351 | `			int nK = HtmlCpUtf8(aHtml401Ent[n].cp,zKey);` |
|    497 | 5352 | `			zKey[nK] = 0;` |
|    497 | 5353 | `			HtmlTableAdd(pArray,pValue,zKey,aHtml401Ent[n].zEnt);` |
|    249 | 5354 | `		}` |
|      1 | 5355 | `	}` |
|     13 | 5356 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 5357 | `}` |
|     25 | 5358 | `static int FvEmailAllowed(unsigned char c){` |
|     25 | 5359 | `	if( (c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9') ){ return 1; }` |
|     16 | 5360 | `	return c=='!'\|\|c=='#'\|\|c=='$'\|\|c=='%'\|\|c=='&'\|\|c=='\''\|\|c=='*'\|\|c=='+'` |
|     10 | 5361 | ``	    \|\| c=='-'\|\|c=='='\|\|c=='?'\|\|c=='^'\|\|c=='_'\|\|c=='`'\|\|c=='{'\|\|c=='\|'`` |
|     15 | 5362 | `	    \|\| c=='}'\|\|c=='~'\|\|c=='@'\|\|c=='.'\|\|c=='['\|\|c==']';` |
|     13 | 5363 | `}` |
|     23 | 5364 | `static int FvUrlAllowed(unsigned char c){` |
|     23 | 5365 | `	return c>=33 && c<=126; /* PHP keeps every printable ASCII byte except space */` |
|      1 | 5366 | `}` |
|      - | 5367 | `/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */` |
|      5 | 5368 | `static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){` |
|      5 | 5369 | `	int i, runStart = 0;` |
|      5 | 5370 | `	ph7_result_string(pCtx,"",0);` |
|     51 | 5371 | `	for( i=0; i<n; i++ ){` |
|     47 | 5372 | `		unsigned char c = (unsigned char)z[i];` |
|     47 | 5373 | `		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){` |
|     11 | 5374 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     11 | 5375 | `			runStart = i+1;` |
|      5 | 5376 | `		}` |
|     24 | 5377 | `	}` |
|      5 | 5378 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      5 | 5379 | `}` |
|      - | 5380 | `/*` |
|      - | 5381 | ` * Apply the selected filter to one already-resolved input value and write the` |
|      - | 5382 | ` * result into pCtx. Shared by filter_var() and filter_input(): the caller has` |
|      - | 5383 | ` * already parsed $filter/$flags/$options. On validation failure the 'default'` |
|      - | 5384 | ` * option (if any) is returned, else null when FILTER_NULL_ON_FAILURE is set,` |
|      - | 5385 | ` * else false. A validating filter that passes returns the (string) input` |
|      - | 5386 | ` * unchanged; a sanitizer writes its transformed output directly.` |
|      - | 5387 | ` */` |
|    316 | 5388 | `static int FvApplyFilter(ph7_context *pCtx,ph7_value *pInput,` |
|      - | 5389 | `                         int iFilter,int iFlags,ph7_value *pOpts,` |
|      - | 5390 | `                         ph7_value *pDefault)` |
|      3 | 5391 | `{` |
|    319 | 5392 | `	int bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|      - | 5393 | `	const char *zVal; int nVal;` |
|      - | 5394 | `	/* An array/object input fails every scalar filter. */` |
|    319 | 5395 | `	if( ph7_value_is_array(pInput) ){ goto fail; }` |
|    317 | 5396 | `	zVal = ph7_value_to_string(pInput,&nVal);` |
|    317 | 5397 | `	switch( iFilter ){` |
|     28 | 5398 | `	case FV_VALIDATE_INT: {` |
|      - | 5399 | `		ph7_int64 v;` |
|     58 | 5400 | `		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }` |
|     31 | 5401 | `		if( pOpts ){` |
|      7 | 5402 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|      7 | 5403 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|      7 | 5404 | `			if( pMin && v<ph7_value_to_int64(pMin) ){ goto fail; }` |
|      7 | 5405 | `			if( pMax && v>ph7_value_to_int64(pMax) ){ goto fail; }` |
|      2 | 5406 | `		}` |
|     29 | 5407 | `		ph7_result_int64(pCtx,v);` |
|     29 | 5408 | `		return PH7_OK;` |
|      - | 5409 | `	}` |
|     34 | 5410 | `	case FV_VALIDATE_FLOAT: {` |
|      - | 5411 | `		double d;` |
|     69 | 5412 | `		if( !FvValidateFloat(zVal,nVal,iFlags,&d) ){ goto fail; }` |
|     39 | 5413 | `		ph7_result_double(pCtx,d);` |
|     39 | 5414 | `		return PH7_OK;` |
|      - | 5415 | `	}` |
|     14 | 5416 | `	case FV_VALIDATE_BOOLEAN: {` |
|      - | 5417 | `		int b;` |
|     29 | 5418 | `		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }` |
|     21 | 5419 | `		ph7_result_bool(pCtx,b);` |
|     21 | 5420 | `		return PH7_OK;` |
|      - | 5421 | `	}` |
|     25 | 5422 | `	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|     11 | 5423 | `	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;` |
|     28 | 5424 | `	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(zVal,nVal) ){ goto fail; }     goto pass;` |
|     11 | 5425 | `	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal) ){ goto fail; }    goto pass;` |
|     15 | 5426 | `	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal) ){ goto fail; }       goto pass;` |
|      3 | 5427 | `	case FV_VALIDATE_REGEXP: {` |
|      - | 5428 | `#ifdef PH7_ENABLE_PCRE` |
|      8 | 5429 | `		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;` |
|      8 | 5430 | `		const char *zRe; int nRe, matched = 0;` |
|      8 | 5431 | `		if( pRe==0 ){` |
|      3 | 5432 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5433 | `				"filter_var(): \"regexp\" option is missing");` |
|      - | 5434 | `		}` |
|      5 | 5435 | `		zRe = ph7_value_to_string(pRe,&nRe);` |
|      5 | 5436 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK \|\| !matched ){ goto fail; }` |
|      3 | 5437 | `		goto pass;` |
|      - | 5438 | `#else` |
|      - | 5439 | `		goto fail;` |
|      - | 5440 | `#endif` |
|      - | 5441 | `	}` |
|      3 | 5442 | `	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;` |
|      5 | 5443 | `	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;` |
|     13 | 5444 | `	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,iFlags); return PH7_OK;` |
|     25 | 5445 | `	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeFull(pCtx,zVal,nVal,iFlags);    return PH7_OK;` |
|      3 | 5446 | `	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 5447 | `	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;` |
|     13 | 5448 | `	case FV_DEFAULT:` |
|      - | 5449 | `		/* FILTER_UNSAFE_RAW / FILTER_DEFAULT: pass through unchanged unless a` |
|      - | 5450 | `		 * STRIP/ENCODE flag is set, in which case apply the string filter. */` |
|     28 | 5451 | `		if( iFlags & FV_FLAG_STRING_MASK ){` |
|     15 | 5452 | `			FvSanitizeString(pCtx,zVal,nVal,iFlags);` |
|     15 | 5453 | `			return PH7_OK;` |
|      - | 5454 | `		}` |
|     14 | 5455 | `		goto pass;` |
|    ! 0 | 5456 | `	default:` |
|    ! 0 | 5457 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5458 | `			"Unknown filter with ID %d",iFilter);` |
|    ! 0 | 5459 | `		break; /* unknown filter id -> fail */` |
|    ! 0 | 5460 | `	}` |
|     58 | 5461 | `fail:` |
|    118 | 5462 | `	if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|    114 | 5463 | `	else if( bNull ){ ph7_result_null(pCtx); }` |
|    108 | 5464 | `	else { ph7_result_bool(pCtx,0); }` |
|    118 | 5465 | `	return PH7_OK;` |
|     26 | 5466 | `pass: /* validation passed: return the (string) input unchanged */` |
|     54 | 5467 | `	ph7_result_string(pCtx,zVal,nVal);` |
|     54 | 5468 | `	return PH7_OK;` |
|    161 | 5469 | `}` |
|      - | 5470 | `/*` |
|      - | 5471 | ` * Parse the ($filter, $options) pair shared by filter_var()/filter_input() out` |
|      - | 5472 | ` * of apArg[iBase] ($filter) and apArg[iBase+1] ($options): $options is either a` |
|      - | 5473 | ` * plain flags int, or an array with 'flags' and an 'options' sub-array (whose` |
|      - | 5474 | ` * 'default' entry is the fallback value). Fills the four output pointers;` |
|      - | 5475 | ` * unset outputs keep the caller-provided defaults.` |
|      - | 5476 | ` */` |
|    328 | 5477 | `static void FvParseFilterArgs(int nArg,ph7_value **apArg,int iBase,` |
|      - | 5478 | `                              int *piFilter,int *piFlags,` |
|      - | 5479 | `                              ph7_value **ppOpts,ph7_value **ppDefault)` |
|      3 | 5480 | `{` |
|    331 | 5481 | `	if( nArg>iBase ){ *piFilter = ph7_value_to_int(apArg[iBase]); }` |
|    331 | 5482 | `	if( nArg>iBase+1 ){` |
|     88 | 5483 | `		if( ph7_value_is_array(apArg[iBase+1]) ){` |
|     42 | 5484 | `			ph7_value *pF = ph7_array_fetch(apArg[iBase+1],"flags",(int)sizeof("flags")-1);` |
|     42 | 5485 | `			if( pF ){ *piFlags = ph7_value_to_int(pF); }` |
|     42 | 5486 | `			*ppOpts = ph7_array_fetch(apArg[iBase+1],"options",(int)sizeof("options")-1);` |
|     42 | 5487 | `			if( *ppOpts && !ph7_value_is_array(*ppOpts) ){ *ppOpts = 0; }` |
|     42 | 5488 | `			if( *ppOpts ){ *ppDefault = ph7_array_fetch(*ppOpts,"default",(int)sizeof("default")-1); }` |
|     22 | 5489 | `		}else{` |
|     48 | 5490 | `			*piFlags = ph7_value_to_int(apArg[iBase+1]);` |
|      - | 5491 | `		}` |
|     43 | 5492 | `	}` |
|    331 | 5493 | `}` |
|      - | 5494 | `/*` |
|      - | 5495 | ` * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 5496 | ` *  Validate or sanitize a value; see FvApplyFilter for the failure semantics.` |
|      - | 5497 | ` */` |
|    306 | 5498 | `static int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5499 | `{` |
|    308 | 5500 | `	int iFilter = FV_DEFAULT, iFlags = 0;` |
|    308 | 5501 | `	ph7_value *pOpts = 0, *pDefault = 0;` |
|    308 | 5502 | `	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    308 | 5503 | `	FvParseFilterArgs(nArg,apArg,1,&iFilter,&iFlags,&pOpts,&pDefault);` |
|    308 | 5504 | `	return FvApplyFilter(pCtx,apArg[0],iFilter,iFlags,pOpts,pDefault);` |
|    155 | 5505 | `}` |
|      - | 5506 | `/*` |
|      - | 5507 | ` * filter_input($type, $var_name, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 5508 | ` *  Look up $var_name in the requested INPUT_* superglobal, then apply the` |
|      - | 5509 | ` *  filter. Semantics verified byte-for-byte against php 8.5:` |
|      - | 5510 | ` *   - variable NOT set: 'default' option wins, else false when` |
|      - | 5511 | ` *     FILTER_NULL_ON_FAILURE is set, else null. (Note the null/false roles are` |
|      - | 5512 | ` *     INVERTED relative to a present value that fails validation, which yields` |
|      - | 5513 | ` *     default > null-if-NULL_ON_FAILURE > false via FvApplyFilter.)` |
|      - | 5514 | ` *   - variable present: delegate to FvApplyFilter.` |
|      - | 5515 | ` *  Divergence: php reads a SAPI snapshot of the original request variables` |
|      - | 5516 | ` *  captured at startup; PHL reads the live superglobal. In CLI they match for` |
|      - | 5517 | ` *  the SAPI-registered keys (SCRIPT_NAME/PHP_SELF/DOCUMENT_ROOT); keys added` |
|      - | 5518 | ` *  only to the live $_SERVER (REQUEST_TIME/PWD/…) are visible here but not in` |
|      - | 5519 | ` *  php's snapshot.` |
|      - | 5520 | ` */` |
|     28 | 5521 | `static int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5522 | `{` |
|     30 | 5523 | `	int iType, iFilter = FV_DEFAULT, iFlags = 0;` |
|     30 | 5524 | `	ph7_value *pOpts = 0, *pDefault = 0, *pSuper, *pElem;` |
|      - | 5525 | `	const char *zVar, *zSuper; int nVar; sxu32 nSuper;` |
|     30 | 5526 | `	if( nArg<2 ){` |
|      7 | 5527 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|      2 | 5528 | `			"filter_input() expects at least 2 arguments, %d given",nArg);` |
|      - | 5529 | `	}` |
|     26 | 5530 | `	iType = ph7_value_to_int(apArg[0]);` |
|     26 | 5531 | `	switch( iType ){` |
|      3 | 5532 | `	case 0: zSuper = "_POST";   nSuper = (sxu32)sizeof("_POST")-1;   break; /* INPUT_POST */` |
|      3 | 5533 | `	case 1: zSuper = "_GET";    nSuper = (sxu32)sizeof("_GET")-1;    break; /* INPUT_GET */` |
|    ! 0 | 5534 | `	case 2: zSuper = "_COOKIE"; nSuper = (sxu32)sizeof("_COOKIE")-1; break; /* INPUT_COOKIE */` |
|    ! 0 | 5535 | `	case 4: zSuper = "_ENV";    nSuper = (sxu32)sizeof("_ENV")-1;    break; /* INPUT_ENV */` |
|     19 | 5536 | `	case 5: zSuper = "_SERVER"; nSuper = (sxu32)sizeof("_SERVER")-1; break; /* INPUT_SERVER */` |
|      1 | 5537 | `	default:` |
|      3 | 5538 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5539 | `			"filter_input(): Argument #1 ($type) must be an INPUT_* constant");` |
|      - | 5540 | `	}` |
|     23 | 5541 | `	zVar = ph7_value_to_string(apArg[1],&nVar);` |
|     23 | 5542 | `	FvParseFilterArgs(nArg,apArg,2,&iFilter,&iFlags,&pOpts,&pDefault);` |
|      - | 5543 | `	/* Resolve the variable from the superglobal (missing/non-array -> not set). */` |
|     23 | 5544 | `	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);` |
|     23 | 5545 | `	pElem = (pSuper && ph7_value_is_array(pSuper))` |
|     33 | 5546 | `		? ph7_array_fetch(pSuper,zVar,nVar) : 0;` |
|     23 | 5547 | `	if( pElem==0 ){` |
|      - | 5548 | `		/* Variable not set: default > false(if NULL_ON_FAILURE) > null. Note the` |
|      - | 5549 | `		 * false/null roles are inverted vs FvApplyFilter's present-but-fails path. */` |
|     13 | 5550 | `		if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|      9 | 5551 | `		else if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_bool(pCtx,0); }` |
|      7 | 5552 | `		else { ph7_result_null(pCtx); }` |
|     13 | 5553 | `		return PH7_OK;` |
|      - | 5554 | `	}` |
|     11 | 5555 | `	return FvApplyFilter(pCtx,pElem,iFilter,iFlags,pOpts,pDefault);` |
|     16 | 5556 | `}` |
|      - | 5557 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 5558 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 5559 | `/*` |
|      - | 5560 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 5561 |  |
|      - | 5562 | ` */` |
|      4 | 5563 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 5564 | `	const char *zInput, /* Raw input */` |
|      - | 5565 | `	int nByte,  /* Input length */` |
|      - | 5566 | `	int delim,  /* Delimiter */` |
|      - | 5567 | `	int encl,   /* Enclosure */` |
|      - | 5568 | `	int escape,  /* Escape character */` |
|      - | 5569 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 5570 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 5571 | `	)` |
|      1 | 5572 | `{` |
|      5 | 5573 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 5574 | `	const char *zIn = zInput;` |
|      - | 5575 | `	const char *zPtr;` |
|      - | 5576 | `	int isEnc;` |
|      - | 5577 | `	/* Start processing */` |
|      8 | 5578 | `	for(;;){` |
|     17 | 5579 | `		if( zIn >= zEnd ){` |
|      - | 5580 | `			/* No more input to process */` |
|      5 | 5581 | `			break;` |
|      - | 5582 | `		}` |
|     13 | 5583 | `		isEnc = 0;` |
|     13 | 5584 | `		zPtr = zIn;` |
|      - | 5585 | `		/* Find the first delimiter */` |
|     27 | 5586 | `		while( zIn < zEnd ){` |
|     23 | 5587 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 5588 | `				/* Delimiter found,break imediately */` |
|      5 | 5589 | `				break;` |
|     15 | 5590 | `			}else if( zIn[0] == encl ){` |
|      - | 5591 | `				/* Inside enclosure? */` |
|    ! 0 | 5592 | `				isEnc = !isEnc;` |
|     15 | 5593 | `			}else if( zIn[0] == escape ){` |
|      - | 5594 | `				/* Escape sequence */` |
|    ! 0 | 5595 | `				zIn++;` |
|    ! 0 | 5596 | `			}` |
|      - | 5597 | `			/* Advance the cursor */` |
|     15 | 5598 | `			zIn++;` |
|      1 | 5599 | `		}` |
|     13 | 5600 | `		if( zIn > zPtr ){` |
|     13 | 5601 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 5602 | `			sxi32 rc;` |
|      - | 5603 | `			/* Invoke the supllied callback */` |
|     13 | 5604 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 5605 | `				zPtr++;` |
|    ! 0 | 5606 | `				nByteChunk-=2;` |
|    ! 0 | 5607 | `			}` |
|     13 | 5608 | `			if( nByteChunk > 0 ){` |
|     13 | 5609 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 5610 | `				if( rc == SXERR_ABORT ){` |
|      - | 5611 | `					/* User callback request an operation abort */` |
|    ! 0 | 5612 | `					break;` |
|      - | 5613 | `				}` |
|      6 | 5614 | `			}` |
|      6 | 5615 | `		}` |
|      - | 5616 | `		/* Ignore trailing delimiter */` |
|     21 | 5617 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 5618 | `			zIn++;` |
|      1 | 5619 | `		}` |
|      1 | 5620 | `	}` |
|      5 | 5621 | `	return SXRET_OK;` |
|      1 | 5622 | `}` |
|      - | 5623 | `/*` |
|      - | 5624 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 5625 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 5626 | ` * argument to this callback.` |
|      - | 5627 | ` */` |
|     12 | 5628 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 5629 | `{` |
|     13 | 5630 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 5631 | `	ph7_value sEntry;` |
|      - | 5632 | `	SyString sToken;` |
|      - | 5633 | `	/* Insert the token in the given array */` |
|     13 | 5634 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 5635 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 5636 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 5637 | `	if( sToken.nByte < 1){` |
|    ! 0 | 5638 | `		return SXRET_OK;` |
|      - | 5639 | `	}` |
|     13 | 5640 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 5641 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 5642 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 5643 | `	return SXRET_OK;` |
|      7 | 5644 | `}` |
|      - | 5645 | `/*` |
|      - | 5646 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 5647 | ` *  Parse a CSV string into an array.` |
|      - | 5648 | ` * Parameters` |
|      - | 5649 | ` *  $input` |
|      - | 5650 | ` *   The string to parse.` |
|      - | 5651 | ` *  $delimiter` |
|      - | 5652 | ` *   Set the field delimiter (one character only).` |
|      - | 5653 | ` *  $enclosure` |
|      - | 5654 | ` *   Set the field enclosure character (one character only).` |
|      - | 5655 | ` *  $escape` |
|      - | 5656 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 5657 | ` * Return` |
|      - | 5658 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 5659 | ` */` |
|      2 | 5660 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5661 | `{` |
|      - | 5662 | `	const char *zInput,*zPtr;` |
|      - | 5663 | `	ph7_value *pArray;` |
|      3 | 5664 | `	int delim  = ',';   /* Delimiter */` |
|      3 | 5665 | `	int encl   = '"' ;  /* Enclosure */` |
|      3 | 5666 | `	int escape = '\\';  /* Escape character */` |
|      - | 5667 | `	int nLen;` |
|      3 | 5668 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5669 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 5670 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5671 | `		return PH7_OK;` |
|      - | 5672 | `	}` |
|      - | 5673 | `	/* Extract the raw input */` |
|      3 | 5674 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5675 | `	if( nArg > 1 ){` |
|      - | 5676 | `		int i;` |
|      3 | 5677 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 5678 | `			/* Extract the delimiter */` |
|      3 | 5679 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 5680 | `			if( i > 0 ){` |
|      3 | 5681 | `				delim = zPtr[0];` |
|      1 | 5682 | `			}` |
|      1 | 5683 | `		}` |
|      3 | 5684 | `		if( nArg > 2 ){` |
|      3 | 5685 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 5686 | `				/* Extract the enclosure */` |
|      3 | 5687 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 5688 | `				if( i > 0 ){` |
|      3 | 5689 | `					encl = zPtr[0];` |
|      1 | 5690 | `				}` |
|      1 | 5691 | `			}` |
|      3 | 5692 | `			if( nArg > 3 ){` |
|      3 | 5693 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 5694 | `					/* Extract the escape character */` |
|      3 | 5695 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 5696 | `					if( i > 0 ){` |
|      3 | 5697 | `						escape = zPtr[0];` |
|      1 | 5698 | `					}` |
|      1 | 5699 | `				}` |
|      1 | 5700 | `			}` |
|      1 | 5701 | `		}` |
|      1 | 5702 | `	}` |
|      - | 5703 | `	/* Create our array */` |
|      3 | 5704 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5705 | `	if( pArray == 0 ){` |
|      - | 5706 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|    ! 0 | 5707 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5708 | `	}` |
|      - | 5709 | `	/* Parse the raw input */` |
|      3 | 5710 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 5711 | `	/* Return the freshly created array */` |
|      3 | 5712 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5713 | `	return PH7_OK;` |
|      2 | 5714 | `}` |
|      - | 5715 | `/*` |
|      - | 5716 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 5717 | ` * container.` |
|      - | 5718 | ` * Refer to [strip_tags()].` |
|      - | 5719 | ` */` |
|     10 | 5720 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5721 | `{` |
|     11 | 5722 | `	const char *zEnd = &zTag[nByte];` |
|      - | 5723 | `	const char *zPtr;` |
|      - | 5724 | `	SyString sEntry;` |
|      - | 5725 | `	/* Strip tags */` |
|     10 | 5726 | `	for(;;){` |
|     45 | 5727 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 5728 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 5729 | `				zTag++;` |
|      1 | 5730 | `		}` |
|     21 | 5731 | `		if( zTag >= zEnd ){` |
|     11 | 5732 | `			break;` |
|      - | 5733 | `		}` |
|     11 | 5734 | `		zPtr = zTag;` |
|      - | 5735 | `		/* Delimit the tag */` |
|     25 | 5736 | `		while(zTag < zEnd ){` |
|     25 | 5737 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5738 | `				/* UTF-8 stream */` |
|      3 | 5739 | `				zTag++;` |
|      5 | 5740 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 5741 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 5742 | `				break;` |
|    ! 0 | 5743 | `			}else{` |
|     13 | 5744 | `				zTag++;` |
|      - | 5745 | `			}` |
|      1 | 5746 | `		}` |
|     11 | 5747 | `		if( zTag > zPtr ){` |
|      - | 5748 | `			/* Perform the insertion */` |
|     11 | 5749 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 5750 | `			SyStringFullTrim(&sEntry);` |
|     11 | 5751 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 5752 | `		}` |
|      - | 5753 | `		/* Jump the trailing '>' */` |
|     11 | 5754 | `		zTag++;` |
|      1 | 5755 | `	}` |
|     11 | 5756 | `	return SXRET_OK;` |
|      1 | 5757 | `}` |
|      - | 5758 | `/*` |
|      - | 5759 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 5760 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 5761 | ` * Refer to [strip_tags()].` |
|      - | 5762 | ` */` |
|     36 | 5763 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5764 | `{` |
|     37 | 5765 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 5766 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 5767 | `		SyString sTag;` |
|     85 | 5768 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 5769 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 5770 | `			zTag++;` |
|      1 | 5771 | `		}` |
|      - | 5772 | `		/* Delimit the tag */` |
|     25 | 5773 | `		zCur = zTag;` |
|     77 | 5774 | `		while(zTag < zEnd ){` |
|     77 | 5775 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5776 | `				/* UTF-8 stream */` |
|      5 | 5777 | `				zTag++;` |
|      9 | 5778 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 5779 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 5780 | `				break;` |
|    ! 0 | 5781 | `			}else{` |
|     49 | 5782 | `				zTag++;` |
|      - | 5783 | `			}` |
|      1 | 5784 | `		}` |
|     25 | 5785 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 5786 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 5787 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 5788 | `		if( sTag.nByte > 0 ){` |
|      - | 5789 | `			SyString *aEntry,*pEntry;` |
|      - | 5790 | `			sxi32 rc;` |
|      - | 5791 | `			sxu32 n;` |
|      - | 5792 | `			/* Perform the lookup */` |
|     25 | 5793 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 5794 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 5795 | `				pEntry = &aEntry[n];` |
|      - | 5796 | `				/* Do the comparison */` |
|     25 | 5797 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 5798 | `				if( !rc ){` |
|     21 | 5799 | `					return SXRET_OK;` |
|      - | 5800 | `				}` |
|      3 | 5801 | `			}` |
|      2 | 5802 | `		}` |
|      2 | 5803 | `	}` |
|      - | 5804 | `	/* No such tag */` |
|     17 | 5805 | `	return SXERR_NOTFOUND;` |
|     19 | 5806 | `}` |
|      - | 5807 | `/*` |
|      - | 5808 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 5809 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 5810 | ` * Refer to [strip_tags()].` |
|      - | 5811 | ` */` |
|     16 | 5812 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 5813 | `{` |
|     17 | 5814 | `	const char *zEnd = &zIn[nByte];` |
|      - | 5815 | `	const char *zPtr,*zTag;` |
|      - | 5816 | `	SySet sSet;` |
|      - | 5817 | `	/* initialize the set of allowed tags */` |
|     17 | 5818 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 5819 | `	if( nTaglen > 0 ){` |
|      - | 5820 | `		/* Set of allowed tags */` |
|     11 | 5821 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 5822 | `	}` |
|      - | 5823 | `	/* Set the empty string */` |
|     17 | 5824 | `	ph7_result_string(pCtx,"",0);` |
|      - | 5825 | `	/* Start processing */` |
|     26 | 5826 | `	for(;;){` |
|     53 | 5827 | `		if(zIn >= zEnd){` |
|      - | 5828 | `			/* No more input to process */` |
|     15 | 5829 | `			break;` |
|      - | 5830 | `		}` |
|     39 | 5831 | `		zPtr = zIn;` |
|      - | 5832 | `		/* Find a tag */` |
|    133 | 5833 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 5834 | `			zIn++;` |
|      1 | 5835 | `		}` |
|     39 | 5836 | `		if( zIn > zPtr ){` |
|      - | 5837 | `			/* Consume raw input */` |
|     21 | 5838 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 5839 | `		}` |
|      - | 5840 | `		/* Ignore trailing null bytes */` |
|     39 | 5841 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 5842 | `			zIn++;` |
|    ! 0 | 5843 | `		}` |
|     39 | 5844 | `		if(zIn >= zEnd){` |
|      - | 5845 | `			/* No more input to process */` |
|      3 | 5846 | `			break;` |
|      - | 5847 | `		}` |
|     37 | 5848 | `		if( zIn[0] == '<' ){` |
|      - | 5849 | `			sxi32 rc;` |
|     37 | 5850 | `			zTag = zIn++;` |
|      - | 5851 | `			/* Delimit the tag */` |
|    127 | 5852 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 5853 | `				zIn++;` |
|      1 | 5854 | `			}` |
|     37 | 5855 | `			if( zIn < zEnd ){` |
|     37 | 5856 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 5857 | `			}` |
|      - | 5858 | `			/* Query the set */` |
|     37 | 5859 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 5860 | `			if( rc == SXRET_OK ){` |
|      - | 5861 | `				/* Keep the tag */` |
|     21 | 5862 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 5863 | `			}` |
|     18 | 5864 | `		}` |
|      1 | 5865 | `	}` |
|      - | 5866 | `	/* Cleanup */` |
|     17 | 5867 | `	SySetRelease(&sSet);` |
|     17 | 5868 | `	return SXRET_OK;` |
|      1 | 5869 | `}` |
|      - | 5870 | `/*` |
|      - | 5871 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 5872 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 5873 | ` * Parameters` |
|      - | 5874 | ` *  $str` |
|      - | 5875 | ` *  The input string.` |
|      - | 5876 | ` * $allowable_tags` |
|      - | 5877 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 5878 | ` * Return` |
|      - | 5879 | ` *  Returns the stripped string.` |
|      - | 5880 | ` */` |
|     14 | 5881 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5882 | `{` |
|     15 | 5883 | `	const char *zTaglist = 0;` |
|      - | 5884 | `	const char *zString;` |
|     15 | 5885 | `	int nTaglen = 0;` |
|      - | 5886 | `	int nLen;` |
|     15 | 5887 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5888 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 5889 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5890 | `		return PH7_OK;` |
|      - | 5891 | `	}` |
|      - | 5892 | `	/* Point to the raw string */` |
|     15 | 5893 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 5894 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 5895 | `		/* Allowed tag */` |
|     11 | 5896 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 5897 | `	}` |
|      - | 5898 | `	/* Process input */` |
|     15 | 5899 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 5900 | `	return PH7_OK;` |
|      8 | 5901 | `}` |
|      - | 5902 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 5903 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 5904 | `/*` |
|      - | 5905 | ` * string str_shuffle(string $str)` |
|      - | 5906 |  |
|      - | 5907 | ` *  Randomly shuffles a string.` |
|      - | 5908 | ` * Parameters` |
|      - | 5909 | ` *  $str` |
|      - | 5910 | ` *   The input string.` |
|      - | 5911 | ` * Return` |
|      - | 5912 | ` *  Returns the shuffled string.` |
|      - | 5913 | ` */` |
|     10 | 5914 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5915 | `{` |
|      - | 5916 | `	const char *zString;` |
|      - | 5917 | `	int nLen,i,c;` |
|      - | 5918 | `	sxu32 iR;` |
|     11 | 5919 | `	if( nArg < 1 ){` |
|      - | 5920 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5921 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5922 | `		return PH7_OK;` |
|      - | 5923 | `	}` |
|      - | 5924 | `	/* Extract the target string */` |
|     11 | 5925 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5926 | `	if( nLen < 1 ){` |
|      - | 5927 | `		/* Nothing to shuffle */` |
|      3 | 5928 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 5929 | `		return PH7_OK;` |
|      - | 5930 | `	}` |
|      - | 5931 | `	/* Shuffle the string */` |
|     43 | 5932 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 5933 | `		/* Generate a random number first */` |
|     35 | 5934 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 5935 | `		/* Extract a random offset */` |
|     35 | 5936 | `		c = zString[iR % nLen];` |
|      - | 5937 | `		/* Append it */` |
|     35 | 5938 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 5939 | `	}` |
|      9 | 5940 | `	return PH7_OK;` |
|      6 | 5941 | `}` |
|      - | 5942 | `/*` |
|      - | 5943 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 5944 | ` *  Convert a string to an array.` |
|      - | 5945 | ` * Parameters` |
|      - | 5946 | ` * $string` |
|      - | 5947 | ` *  The input string.` |
|      - | 5948 | ` * $split_length` |
|      - | 5949 | ` *  Maximum length of the chunk.` |
|      - | 5950 | ` * Return` |
|      - | 5951 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 5952 | ` *  except possibly the last one which may be shorter.` |
|      - | 5953 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 5954 | ` *  as the first (and only) array element.` |
|      - | 5955 | ` *  An empty string returns an empty array.` |
|      - | 5956 | ` * Errors` |
|      - | 5957 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 5958 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 5959 | ` *  ValueError if $split_length is less than 1.` |
|      - | 5960 | ` */` |
|     28 | 5961 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 5962 | `{` |
|      - | 5963 | `	const char *zString,*zEnd;` |
|      - | 5964 | `	ph7_value *pArray,*pValue;` |
|      - | 5965 | `	int split_len;` |
|      - | 5966 | `	int nLen;` |
|     33 | 5967 | `	if( nArg < 1 ){` |
|      4 | 5968 | `		return PH7_VmThrowException(pCtx,` |
|      - | 5969 | `			"ArgumentCountError",` |
|      - | 5970 | `			"str_split() expects at least 1 argument, %d given",` |
|      1 | 5971 | `			nArg` |
|      - | 5972 | `			);` |
|      - | 5973 | `	}` |
|      - | 5974 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     38 | 5975 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     40 | 5976 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 5977 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 5978 | `		return PH7_VmThrowException(pCtx,` |
|      - | 5979 | `			"TypeError",` |
|      - | 5980 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 5981 | `			ph7_type_name(apArg[0])` |
|      - | 5982 | `			);` |
|      - | 5983 | `	}` |
|      - | 5984 | `	/* Point to the target string */` |
|     27 | 5985 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 5986 | `	split_len = (int)sizeof(char);` |
|     27 | 5987 | `	if( nArg > 1 ){` |
|      - | 5988 | `		/* Split length */` |
|     17 | 5989 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 5990 | `		if( split_len < 1 ){` |
|      6 | 5991 | `			return PH7_VmThrowException(pCtx,` |
|      - | 5992 | `				"ValueError",` |
|      - | 5993 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 5994 | `				);` |
|      - | 5995 | `		}` |
|     11 | 5996 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 5997 | `			split_len = nLen;` |
|      1 | 5998 | `		}` |
|      5 | 5999 | `	}` |
|      - | 6000 | `	/* Create the array and the scalar value */` |
|     21 | 6001 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 6002 | `	/*Chunk value */` |
|     21 | 6003 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 6004 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 6005 | `		/* Return FALSE */` |
|    ! 0 | 6006 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6007 | `		return PH7_OK;` |
|      - | 6008 | `	}` |
|      - | 6009 | `	/* Point to the end of the string */` |
|     21 | 6010 | `	zEnd = &zString[nLen];` |
|      - | 6011 | `	/* Perform the requested operation */` |
|     48 | 6012 | `	for(;;){` |
|      - | 6013 | `		int nMax;` |
|     59 | 6014 | `		if( zString >= zEnd ){` |
|      - | 6015 | `			/* No more input to process */` |
|     21 | 6016 | `			break;` |
|      - | 6017 | `		}` |
|     39 | 6018 | `		nMax = (int)(zEnd-zString);` |
|     39 | 6019 | `		if( nMax < split_len ){` |
|      3 | 6020 | `			split_len = nMax;` |
|      1 | 6021 | `		}` |
|      - | 6022 | `		/* Copy the current chunk */` |
|     39 | 6023 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 6024 | `		/* Insert it */` |
|     39 | 6025 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 6026 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6027 | `		}` |
|      - | 6028 | `		/* reset the string cursor */` |
|     39 | 6029 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 6030 | `		/* Update position */` |
|     39 | 6031 | `		zString += split_len;` |
|      1 | 6032 | `	}` |
|      - | 6033 | `	/*` |
|      - | 6034 | `	 * Return the array.` |
|      - | 6035 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 6036 | `	 * upon we return from this function.` |
|      - | 6037 | `	 */` |
|     21 | 6038 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 6039 | `	return PH7_OK;` |
|     19 | 6040 | `}` |
|      - | 6041 | `/*` |
|      - | 6042 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 6043 | ` * Refer to [strspn()].` |
|      - | 6044 | ` */` |
|     28 | 6045 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 6046 | `{` |
|     29 | 6047 | `	const char *zIn = *pzIn;` |
|      - | 6048 | `	const char *zPtr;` |
|      - | 6049 | `	/* Ignore leading white spaces */` |
|     29 | 6050 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 6051 | `		zIn++;` |
|    ! 0 | 6052 | `	}` |
|     29 | 6053 | `	if( zIn >= zEnd ){` |
|      - | 6054 | `		/* End of input */` |
|    ! 0 | 6055 | `		return SXERR_EOF;` |
|      - | 6056 | `	}` |
|     29 | 6057 | `	zPtr = zIn;` |
|      - | 6058 | `	/* Extract the token */` |
|    201 | 6059 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 6060 | `		zIn++;` |
|      1 | 6061 | `	}` |
|     29 | 6062 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 6063 | `	/* Synchronize pointers */` |
|     29 | 6064 | `	*pzIn = zIn;` |
|      - | 6065 | `	/* Return to the caller */` |
|     29 | 6066 | `	return SXRET_OK;` |
|     15 | 6067 | `}` |
|      - | 6068 | `/*` |
|      - | 6069 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 6070 | ` * return the longest match.` |
|      - | 6071 | ` * Refer to [strspn()].` |
|      - | 6072 | ` */` |
|     18 | 6073 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 6074 | `{` |
|     19 | 6075 | `	const char *zEnd = &zString[nLen];` |
|     19 | 6076 | `	const char *zIn = zString;` |
|      - | 6077 | `	int i,c;` |
|     45 | 6078 | `	for(;;){` |
|     91 | 6079 | `		if( zString >= zEnd ){` |
|      7 | 6080 | `			break;` |
|      - | 6081 | `		}` |
|      - | 6082 | `		/* Extract current character */` |
|     85 | 6083 | `		c = zString[0];` |
|      - | 6084 | `		/* Perform the lookup */` |
|    383 | 6085 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 6086 | `			if( c == zMask[i] ){` |
|      - | 6087 | `				/* Character found */` |
|     73 | 6088 | `				break;` |
|      - | 6089 | `			}` |
|    150 | 6090 | `		}` |
|     85 | 6091 | `		if( i >= nMaskLen ){` |
|      - | 6092 | `			/* Character not in the current mask,break immediately */` |
|     13 | 6093 | `			break;` |
|      - | 6094 | `		}` |
|      - | 6095 | `		/* Advance cursor */` |
|     73 | 6096 | `		zString++;` |
|      1 | 6097 | `	}` |
|      - | 6098 | `	/* Longest match */` |
|     19 | 6099 | `	return (int)(zString-zIn);` |
|      1 | 6100 | `}` |
|      - | 6101 | `/*` |
|      - | 6102 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 6103 | ` * Refer to [strcspn()].` |
|      - | 6104 | ` */` |
|     10 | 6105 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 6106 | `{` |
|     11 | 6107 | `	const char *zEnd = &zString[nLen];` |
|     11 | 6108 | `	const char *zIn = zString;` |
|      - | 6109 | `	int i,c;` |
|     12 | 6110 | `	for(;;){` |
|     25 | 6111 | `		if( zString >= zEnd ){` |
|      3 | 6112 | `			break;` |
|      - | 6113 | `		}` |
|      - | 6114 | `		/* Extract current character */` |
|     23 | 6115 | `		c = zString[0];` |
|      - | 6116 | `		/* Perform the lookup */` |
|     51 | 6117 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 6118 | `			if( c == zMask[i] ){` |
|      9 | 6119 | `				break;` |
|      - | 6120 | `			}` |
|     15 | 6121 | `		}` |
|     23 | 6122 | `		if( i < nMaskLen ){` |
|      - | 6123 | `			/* Character in the current mask,break immediately */` |
|      9 | 6124 | `			break;` |
|      - | 6125 | `		}` |
|      - | 6126 | `		/* Advance cursor */` |
|     15 | 6127 | `		zString++;` |
|      1 | 6128 | `	}` |
|      - | 6129 | `	/* Longest match */` |
|     11 | 6130 | `	return (int)(zString-zIn);` |
|      1 | 6131 | `}` |
|      - | 6132 | `/*` |
|      - | 6133 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 6134 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 6135 | ` *  of characters contained within a given mask.` |
|      - | 6136 | ` * Parameters` |
|      - | 6137 | ` * $str` |
|      - | 6138 | ` *  The input string.` |
|      - | 6139 | ` * $mask` |
|      - | 6140 | ` *  The list of allowable characters.` |
|      - | 6141 | ` * $start` |
|      - | 6142 | ` *  The position in subject to start searching.` |
|      - | 6143 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 6144 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 6145 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 6146 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 6147 | ` *  start'th position from the end of subject.` |
|      - | 6148 | ` * $length` |
|      - | 6149 | ` *  The length of the segment from subject to examine.` |
|      - | 6150 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 6151 | ` *  characters after the starting position.` |
|      - | 6152 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 6153 | ` *  position up to length characters from the end of subject.` |
|      - | 6154 | ` * Return` |
|      - | 6155 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 6156 | ` * in mask.` |
|      - | 6157 | ` */` |
|     24 | 6158 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6159 | `{` |
|      - | 6160 | `	const char *zString,*zMask,*zEnd;` |
|      - | 6161 | `	int iMasklen,iLen;` |
|      - | 6162 | `	SyString sToken;` |
|     25 | 6163 | `	int iCount = 0;` |
|      - | 6164 | `	int rc;` |
|     25 | 6165 | `	if( nArg < 2 ){` |
|      - | 6166 | `		/* Missing agruments,return zero */` |
|    ! 0 | 6167 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 6168 | `		return PH7_OK;` |
|      - | 6169 | `	}` |
|      - | 6170 | `	/* Extract the target string */` |
|     25 | 6171 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6172 | `	/* Extract the mask */` |
|     25 | 6173 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 6174 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 6175 | `		/* Nothing to process,return zero */` |
|      7 | 6176 | `		ph7_result_int(pCtx,0);` |
|      7 | 6177 | `		return PH7_OK;` |
|      - | 6178 | `	}` |
|     19 | 6179 | `	if( nArg > 2 ){` |
|      - | 6180 | `		int nOfft;` |
|      - | 6181 | `		/* Extract the offset */` |
|      9 | 6182 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 6183 | `		if( nOfft < 0 ){` |
|    ! 0 | 6184 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 6185 | `			if( zBase > zString ){` |
|    ! 0 | 6186 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 6187 | `				zString = zBase;` |
|    ! 0 | 6188 | `			}else{` |
|      - | 6189 | `				/* Invalid offset */` |
|    ! 0 | 6190 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 6191 | `				return PH7_OK;` |
|      - | 6192 | `			}` |
|    ! 0 | 6193 | `		}else{` |
|      9 | 6194 | `			if( nOfft >= iLen ){` |
|      - | 6195 | `				/* Invalid offset */` |
|    ! 0 | 6196 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 6197 | `				return PH7_OK;` |
|    ! 0 | 6198 | `			}else{` |
|      - | 6199 | `				/* Update offset */` |
|      9 | 6200 | `				zString += nOfft;` |
|      9 | 6201 | `				iLen -= nOfft;` |
|      - | 6202 | `			}` |
|      - | 6203 | `		}` |
|      9 | 6204 | `		if( nArg > 3 ){` |
|      - | 6205 | `			int iUserlen;` |
|      - | 6206 | `			/* Extract the desired length */` |
|      9 | 6207 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 6208 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 6209 | `				iLen = iUserlen;` |
|      2 | 6210 | `			}` |
|      4 | 6211 | `		}` |
|      4 | 6212 | `	}` |
|      - | 6213 | `	/* Point to the end of the string */` |
|     19 | 6214 | `	zEnd = &zString[iLen];` |
|      - | 6215 | `	/* Extract the first non-space token */` |
|     19 | 6216 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 6217 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 6218 | `		/* Compare against the current mask */` |
|     19 | 6219 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 6220 | `	}` |
|      - | 6221 | `	/* Longest match */` |
|     19 | 6222 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 6223 | `	return PH7_OK;` |
|     13 | 6224 | `}` |
|      - | 6225 | `/*` |
|      - | 6226 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 6227 | ` *  Find length of initial segment not matching mask.` |
|      - | 6228 | ` * Parameters` |
|      - | 6229 | ` * $str` |
|      - | 6230 | ` *  The input string.` |
|      - | 6231 | ` * $mask` |
|      - | 6232 | ` *  The list of not allowed characters.` |
|      - | 6233 | ` * $start` |
|      - | 6234 | ` *  The position in subject to start searching.` |
|      - | 6235 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 6236 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 6237 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 6238 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 6239 | ` *  start'th position from the end of subject.` |
|      - | 6240 | ` * $length` |
|      - | 6241 | ` *  The length of the segment from subject to examine.` |
|      - | 6242 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 6243 | ` *  characters after the starting position.` |
|      - | 6244 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 6245 | ` *  position up to length characters from the end of subject.` |
|      - | 6246 | ` * Return` |
|      - | 6247 | ` *  Returns the length of the segment as an integer.` |
|      - | 6248 | ` */` |
|     14 | 6249 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6250 | `{` |
|      - | 6251 | `	const char *zString,*zMask,*zEnd;` |
|      - | 6252 | `	int iMasklen,iLen;` |
|      - | 6253 | `	SyString sToken;` |
|     15 | 6254 | `	int iCount = 0;` |
|      - | 6255 | `	int rc;` |
|     15 | 6256 | `	if( nArg < 2 ){` |
|      - | 6257 | `		/* Missing agruments,return zero */` |
|    ! 0 | 6258 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 6259 | `		return PH7_OK;` |
|      - | 6260 | `	}` |
|      - | 6261 | `	/* Extract the target string */` |
|     15 | 6262 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6263 | `	/* Extract the mask */` |
|     15 | 6264 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 6265 | `	if( iLen < 1 ){` |
|      - | 6266 | `		/* Nothing to process,return zero */` |
|    ! 0 | 6267 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 6268 | `		return PH7_OK;` |
|      - | 6269 | `	}` |
|     15 | 6270 | `	if( iMasklen < 1 ){` |
|      - | 6271 | `		/* No given mask,return the string length */` |
|      3 | 6272 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 6273 | `		return PH7_OK;` |
|      - | 6274 | `	}` |
|     13 | 6275 | `	if( nArg > 2 ){` |
|      - | 6276 | `		int nOfft;` |
|      - | 6277 | `		/* Extract the offset */` |
|     11 | 6278 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 6279 | `		if( nOfft < 0 ){` |
|    ! 0 | 6280 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 6281 | `			if( zBase > zString ){` |
|    ! 0 | 6282 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 6283 | `				zString = zBase;` |
|    ! 0 | 6284 | `			}else{` |
|      - | 6285 | `				/* Invalid offset */` |
|    ! 0 | 6286 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 6287 | `				return PH7_OK;` |
|      - | 6288 | `			}` |
|    ! 0 | 6289 | `		}else{` |
|     11 | 6290 | `			if( nOfft >= iLen ){` |
|      - | 6291 | `				/* Invalid offset */` |
|      3 | 6292 | `				ph7_result_int(pCtx,0);` |
|      3 | 6293 | `				return PH7_OK;` |
|    ! 0 | 6294 | `			}else{` |
|      - | 6295 | `				/* Update offset */` |
|      9 | 6296 | `				zString += nOfft;` |
|      9 | 6297 | `				iLen -= nOfft;` |
|      - | 6298 | `			}` |
|      - | 6299 | `		}` |
|      9 | 6300 | `		if( nArg > 3 ){` |
|      - | 6301 | `			int iUserlen;` |
|      - | 6302 | `			/* Extract the desired length */` |
|    ! 0 | 6303 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 6304 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 6305 | `				iLen = iUserlen;` |
|    ! 0 | 6306 | `			}` |
|    ! 0 | 6307 | `		}` |
|      4 | 6308 | `	}` |
|      - | 6309 | `	/* Point to the end of the string */` |
|     11 | 6310 | `	zEnd = &zString[iLen];` |
|      - | 6311 | `	/* Extract the first non-space token */` |
|     11 | 6312 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 6313 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 6314 | `		/* Compare against the current mask */` |
|     11 | 6315 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 6316 | `	}` |
|      - | 6317 | `	/* Longest match */` |
|     11 | 6318 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 6319 | `	return PH7_OK;` |
|      8 | 6320 | `}` |
|      - | 6321 | `/*` |
|      - | 6322 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 6323 | ` *  Search a string for any of a set of characters.` |
|      - | 6324 | ` * Parameters` |
|      - | 6325 | ` *  $haystack` |
|      - | 6326 | ` *   The string where char_list is looked for.` |
|      - | 6327 | ` *  $char_list` |
|      - | 6328 | ` *   This parameter is case sensitive.` |
|      - | 6329 | ` * Return` |
|      - | 6330 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 6331 | ` */` |
|      4 | 6332 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6333 | `{` |
|      - | 6334 | `	const char *zString,*zList,*zEnd;` |
|      - | 6335 | `	int iLen,iListLen,i,c;` |
|      - | 6336 | `	sxu32 nOfft,nMax;` |
|      - | 6337 | `	sxi32 rc;` |
|      5 | 6338 | `	if( nArg < 2 ){` |
|      - | 6339 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 6340 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6341 | `		return PH7_OK;` |
|      - | 6342 | `	}` |
|      - | 6343 | `	/* Extract the haystack and the char list */` |
|      5 | 6344 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 6345 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 6346 | `	if( iLen < 1 ){` |
|      - | 6347 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 6348 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6349 | `		return PH7_OK;` |
|      - | 6350 | `	}` |
|      - | 6351 | `	/* Point to the end of the string */` |
|      5 | 6352 | `	zEnd = &zString[iLen];` |
|      5 | 6353 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 6354 | `	/* perform the requested operation */` |
|     15 | 6355 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 6356 | `		c = zList[i];` |
|     11 | 6357 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 6358 | `		if( rc == SXRET_OK ){` |
|      5 | 6359 | `			if( nMax < nOfft ){` |
|      3 | 6360 | `				nOfft = nMax;` |
|      1 | 6361 | `			}` |
|      2 | 6362 | `		}` |
|      6 | 6363 | `	}` |
|      5 | 6364 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 6365 | `		/* No such substring,return FALSE */` |
|      3 | 6366 | `		ph7_result_bool(pCtx,0);` |
|      2 | 6367 | `	}else{` |
|      - | 6368 | `		/* Return the substring */` |
|      3 | 6369 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 6370 | `	}` |
|      5 | 6371 | `	return PH7_OK;` |
|      3 | 6372 | `}` |
|      - | 6373 | `/* SPDX-SnippetBegin */` |
|      - | 6374 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 6375 | `/* SPDX-License-Identifier: blessing */` |
|      - | 6376 | `/*` |
|      - | 6377 | ` * string soundex(string $str)` |
|      - | 6378 | ` *  Calculate the soundex key of a string.` |
|      - | 6379 | ` * Parameters` |
|      - | 6380 | ` *  $str` |
|      - | 6381 | ` *   The input string.` |
|      - | 6382 | ` * Return` |
|      - | 6383 | ` *  Returns the soundex key as a string.` |
|      - | 6384 | ` * Note:` |
|      - | 6385 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 6386 | ` * source tree.` |
|      - | 6387 | ` */` |
|     22 | 6388 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6389 | `{` |
|      - | 6390 | `	const unsigned char *zIn;` |
|      - | 6391 | `	char zResult[8];` |
|      - | 6392 | `	int i, j;` |
|      - | 6393 | `	static const unsigned char iCode[] = {` |
|      - | 6394 |  |
|      - | 6395 |  |
|      - | 6396 |  |
|      - | 6397 |  |
|      - | 6398 |  |
|      - | 6399 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 6400 |  |
|      - | 6401 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 6402 | `	};` |
|     23 | 6403 | `	if( nArg < 1 ){` |
|      - | 6404 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 6405 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 6406 | `		return PH7_OK;` |
|      - | 6407 | `	}` |
|     23 | 6408 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     35 | 6409 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     23 | 6410 | `	if( zIn[i] ){` |
|     17 | 6411 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 6412 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 6413 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 6414 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 6415 | `			if( code>0 ){` |
|     45 | 6416 | `				if( code!=prevcode ){` |
|     33 | 6417 | `					prevcode = (unsigned char)code;` |
|     33 | 6418 | `					zResult[j++] = (char)code + '0';` |
|     16 | 6419 | `				}` |
|     23 | 6420 | `			}else{` |
|     49 | 6421 | `				prevcode = 0;` |
|      - | 6422 | `			}` |
|     47 | 6423 | `		}` |
|     33 | 6424 | `		while( j<4 ){` |
|     17 | 6425 | `			zResult[j++] = '0';` |
|      1 | 6426 | `		}` |
|     17 | 6427 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 6428 | `	}else{` |
|      - | 6429 | `	  /* No alphabetic character: PHP returns "0000" (not the SQLite "?000"). */` |
|      7 | 6430 | `	  ph7_result_string(pCtx,"0000",4);` |
|      - | 6431 | `	}` |
|     23 | 6432 | `	return PH7_OK;` |
|     12 | 6433 | `}` |
|      - | 6434 | `/* SPDX-SnippetEnd */` |
|      - | 6435 | `/*` |
|      - | 6436 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 6437 | ` *  Wraps a string to a given number of characters.` |
|      - | 6438 | ` * Parameters` |
|      - | 6439 | ` *  $str` |
|      - | 6440 | ` *   The input string.` |
|      - | 6441 | ` * $width` |
|      - | 6442 | ` *  The column width.` |
|      - | 6443 | ` * $break` |
|      - | 6444 | ` *  The line is broken using the optional break parameter.` |
|      - | 6445 | ` * Return` |
|      - | 6446 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 6447 | ` */` |
|     26 | 6448 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6449 | `{` |
|      - | 6450 | `	const char *zIn,*zBreak;` |
|      - | 6451 | `	SyBlob sWorker;` |
|      - | 6452 | `	int iLen,iBreaklen,iWidth,iCut,iStart,iSpace,iCur;` |
|      - | 6453 | `	sxi32 rc;` |
|     27 | 6454 | `	if( nArg < 1 ){` |
|      - | 6455 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 6456 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 6457 | `		return PH7_OK;` |
|      - | 6458 | `	}` |
|      - | 6459 | `	/* Extract the input string */` |
|     27 | 6460 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6461 | `	/* Width (default 75; PHP allows 0/negative — break at every space). */` |
|     27 | 6462 | `	iWidth = 75;` |
|     27 | 6463 | `	if( nArg > 1 ){` |
|     27 | 6464 | `		iWidth = ph7_value_to_int(apArg[1]);` |
|     13 | 6465 | `	}` |
|      - | 6466 | `	/* Break string (default "\n"). */` |
|     27 | 6467 | `	zBreak = "\n";` |
|     27 | 6468 | `	iBreaklen = (int)sizeof(char);` |
|     27 | 6469 | `	if( nArg > 2 ){` |
|     13 | 6470 | `		zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      6 | 6471 | `	}` |
|      - | 6472 | `	/* Cut long words? (default false). */` |
|     27 | 6473 | `	iCut = 0;` |
|     27 | 6474 | `	if( nArg > 3 ){` |
|      7 | 6475 | `		iCut = ph7_value_to_bool(apArg[3]);` |
|      3 | 6476 | `	}` |
|     27 | 6477 | `	if( iLen < 1 ){` |
|      - | 6478 | `		/* PHP returns the empty string for empty input before validating the other args. */` |
|      5 | 6479 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 6480 | `		return PH7_OK;` |
|      - | 6481 | `	}` |
|      - | 6482 | `	/* PHP 8 domain errors (catchable ValueError). */` |
|     23 | 6483 | `	if( iBreaklen < 1 ){` |
|      3 | 6484 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6485 | `			"wordwrap(): Argument #3 ($break) must not be empty");` |
|      - | 6486 | `	}` |
|     21 | 6487 | `	if( iWidth == 0 && iCut ){` |
|      3 | 6488 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6489 | `			"wordwrap(): Argument #4 ($cut_long_words) cannot be true when argument #2 ($width) is 0");` |
|      - | 6490 | `	}` |
|      - | 6491 | `	/*` |
|      - | 6492 | `	 * PHP's algorithm: a single left-to-right pass tracking the start of the` |
|      - | 6493 | `	 * current line (iStart) and the position of the last space seen on it` |
|      - | 6494 | `	 * (iSpace). A break is emitted when the line reaches the width, at the last` |
|      - | 6495 | `	 * space if there was one, otherwise (only when cut is enabled) hard at the` |
|      - | 6496 | `	 * boundary. An existing break sequence in the input resets the line.` |
|      - | 6497 | `	 */` |
|     19 | 6498 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     19 | 6499 | `	iStart = iSpace = iCur = 0;` |
|     19 | 6500 | `	rc = SXRET_OK;` |
|    551 | 6501 | `	while( iCur < iLen ){` |
|    533 | 6502 | `		if( iBreaklen <= iLen - iCur && SyMemcmp(&zIn[iCur],zBreak,(sxu32)iBreaklen) == 0 ){` |
|      - | 6503 | `			/* Existing break sequence in the input: copy it verbatim and reset the line. */` |
|    ! 0 | 6504 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart + iBreaklen));` |
|    ! 0 | 6505 | `			if( rc != SXRET_OK ){ goto oom; }` |
|    ! 0 | 6506 | `			iCur += iBreaklen;` |
|    ! 0 | 6507 | `			iStart = iSpace = iCur;` |
|    ! 0 | 6508 | `			continue;` |
|    533 | 6509 | `		}else if( zIn[iCur] == ' ' ){` |
|     67 | 6510 | `			if( iCur - iStart >= iWidth ){` |
|      - | 6511 | `				/* The line already fills the width at this space: break here (the space is consumed). */` |
|     13 | 6512 | `				rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     13 | 6513 | `				if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     13 | 6514 | `				if( rc != SXRET_OK ){ goto oom; }` |
|     13 | 6515 | `				iStart = iCur + 1;` |
|      6 | 6516 | `			}` |
|     67 | 6517 | `			iSpace = iCur;` |
|    500 | 6518 | `		}else if( iCut && iCur - iStart >= iWidth && iStart >= iSpace ){` |
|      - | 6519 | `			/* A word longer than the width with no space to break at: hard-cut at the boundary. */` |
|      7 | 6520 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|      7 | 6521 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|      7 | 6522 | `			if( rc != SXRET_OK ){ goto oom; }` |
|      7 | 6523 | `			iStart = iSpace = iCur;` |
|    464 | 6524 | `		}else if( iCur - iStart >= iWidth && iStart < iSpace ){` |
|      - | 6525 | `			/* Past the width mid-word: wrap back to the last space (which is consumed). */` |
|     17 | 6526 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iSpace - iStart));` |
|     17 | 6527 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     17 | 6528 | `			if( rc != SXRET_OK ){ goto oom; }` |
|     17 | 6529 | `			iStart = iSpace = iSpace + 1;` |
|      8 | 6530 | `		}` |
|    533 | 6531 | `		iCur++;` |
|      1 | 6532 | `	}` |
|      - | 6533 | `	/* Emit the trailing chunk. */` |
|     19 | 6534 | `	if( iStart < iCur ){` |
|     19 | 6535 | `		rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     19 | 6536 | `		if( rc != SXRET_OK ){ goto oom; }` |
|      9 | 6537 | `	}` |
|     19 | 6538 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|     19 | 6539 | `	SyBlobRelease(&sWorker);` |
|     19 | 6540 | `	return PH7_OK;` |
|    ! 0 | 6541 | `oom:` |
|    ! 0 | 6542 | `	SyBlobRelease(&sWorker);` |
|    ! 0 | 6543 | `	return PH7_ContextMemoryError(pCtx);` |
|     14 | 6544 | `}` |
|      - | 6545 | `/*` |
|      - | 6546 | ` * Check if the given character is a member of the given mask.` |
|      - | 6547 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 6548 | ` * Refer to [strtok()].` |
|      - | 6549 | ` */` |
|     30 | 6550 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 6551 | `{` |
|      - | 6552 | `	int i;` |
|     57 | 6553 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 6554 | `		if( c == zMask[i] ){` |
|     13 | 6555 | `			if( pOfft ){` |
|      5 | 6556 | `				*pOfft = i;` |
|      2 | 6557 | `			}` |
|     13 | 6558 | `			return TRUE;` |
|      - | 6559 | `		}` |
|     14 | 6560 | `	}` |
|     19 | 6561 | `	return FALSE;` |
|     16 | 6562 | `}` |
|      - | 6563 | `/*` |
|      - | 6564 | ` * Extract a single token from the input stream.` |
|      - | 6565 | ` * Refer to [strtok()].` |
|      - | 6566 | ` */` |
|      6 | 6567 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 6568 | `{` |
|      7 | 6569 | `	const char *zIn = *pzIn;` |
|      - | 6570 | `	const char *zPtr;` |
|      - | 6571 | `	/* Ignore leading delimiter */` |
|     11 | 6572 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 6573 | `		zIn++;` |
|      1 | 6574 | `	}` |
|      7 | 6575 | `	if( zIn >= zEnd ){` |
|      - | 6576 | `		/* End of input */` |
|    ! 0 | 6577 | `		return SXERR_EOF;` |
|      - | 6578 | `	}` |
|      7 | 6579 | `	zPtr = zIn;` |
|      - | 6580 | `	/* Extract the token */` |
|     13 | 6581 | `	while( zIn < zEnd ){` |
|     11 | 6582 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 6583 | `			/* UTF-8 stream */` |
|    ! 0 | 6584 | `			zIn++;` |
|    ! 0 | 6585 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 6586 | `		}else{` |
|     11 | 6587 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 6588 | `				break;` |
|      - | 6589 | `			}` |
|      7 | 6590 | `			zIn++;` |
|      - | 6591 | `		}` |
|      1 | 6592 | `	}` |
|      7 | 6593 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 6594 | `	/* Update the cursor */` |
|      7 | 6595 | `	*pzIn = zIn;` |
|      - | 6596 | `	/* Return to the caller */` |
|      7 | 6597 | `	return SXRET_OK;` |
|      4 | 6598 | `}` |
|      - | 6599 | `/* strtok auxiliary private data */` |
|      - | 6600 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 6601 | `struct strtok_aux_data` |
|      - | 6602 | `{` |
|      - | 6603 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 6604 | `	const char *zIn;   /* Current input stream */` |
|      - | 6605 | `	const char *zEnd;  /* End of input */` |
|      - | 6606 | `};` |
|      - | 6607 | `/*` |
|      - | 6608 | ` * string strtok(string $str,string $token)` |
|      - | 6609 | ` * string strtok(string $token)` |
|      - | 6610 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 6611 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 6612 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 6613 | ` *  words by using the space character as the token.` |
|      - | 6614 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 6615 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 6616 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 6617 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 6618 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 6619 | ` *  the argument are found.` |
|      - | 6620 | ` * Parameters` |
|      - | 6621 | ` *  $str` |
|      - | 6622 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 6623 | ` * $token` |
|      - | 6624 | ` *  The delimiter used when splitting up str.` |
|      - | 6625 | ` * Return` |
|      - | 6626 | ` *   Current token or FALSE on EOF.` |
|      - | 6627 | ` */` |
|      6 | 6628 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6629 | `{` |
|      - | 6630 | `	strtok_aux_data *pAux;` |
|      - | 6631 | `	const char *zMask;` |
|      - | 6632 | `	SyString sToken;` |
|      - | 6633 | `	int nMasklen;` |
|      - | 6634 | `	sxi32 rc;` |
|      7 | 6635 | `	if( nArg < 2 ){` |
|      - | 6636 | `		/* Extract top aux data */` |
|      5 | 6637 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      5 | 6638 | `		if( pAux == 0 ){` |
|      - | 6639 | `			/* No aux data,return FALSE */` |
|    ! 0 | 6640 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6641 | `			return PH7_OK;` |
|      - | 6642 | `		}` |
|      5 | 6643 | `		nMasklen = 0;` |
|      5 | 6644 | `		zMask = ""; /* cc warning */` |
|      5 | 6645 | `		if( nArg > 0 ){` |
|      - | 6646 | `			/* Extract the mask */` |
|      5 | 6647 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 6648 | `		}` |
|      5 | 6649 | `		if( nMasklen < 1 ){` |
|      - | 6650 | `			/* Invalid mask,return FALSE */` |
|    ! 0 | 6651 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 6652 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6653 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 6654 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6655 | `			return PH7_OK;` |
|      - | 6656 | `		}` |
|      - | 6657 | `		/* Extract the token */` |
|      5 | 6658 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 6659 | `		if( rc != SXRET_OK ){` |
|      - | 6660 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 6661 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 6662 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6663 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 6664 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6665 | `		}else{` |
|      - | 6666 | `			/* Return the extracted token */` |
|      5 | 6667 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6668 | `		}` |
|      3 | 6669 | `	}else{` |
|      - | 6670 | `		const char *zInput,*zCur;` |
|      - | 6671 | `		char *zDup;` |
|      - | 6672 | `		int nLen;` |
|      - | 6673 | `		/* Extract the raw input */` |
|      3 | 6674 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 6675 | `		if( nLen < 1 ){` |
|      - | 6676 | `			/* Empty input,return FALSE */` |
|    ! 0 | 6677 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6678 | `			return PH7_OK;` |
|      - | 6679 | `		}` |
|      - | 6680 | `		/* Extract the mask */` |
|      3 | 6681 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 6682 | `		if( nMasklen < 1 ){` |
|      - | 6683 | `			/* Set a default mask */` |
|      - | 6684 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 6685 | `			zMask = TOK_MASK;` |
|    ! 0 | 6686 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 6687 | `#undef TOK_MASK` |
|    ! 0 | 6688 | `		}` |
|      - | 6689 | `		/* Extract a single token */` |
|      3 | 6690 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 6691 | `		if( rc != SXRET_OK ){` |
|      - | 6692 | `			/* Empty input */` |
|    ! 0 | 6693 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6694 | `			return PH7_OK;` |
|    ! 0 | 6695 | `		}else{` |
|      - | 6696 | `			/* Return the extracted token */` |
|      3 | 6697 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6698 | `		}` |
|      - | 6699 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 6700 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 6701 | `		if( pAux ){` |
|      3 | 6702 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 6703 | `			if( nLen < 1 ){` |
|    ! 0 | 6704 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6705 | `				return PH7_OK;` |
|      - | 6706 | `			}` |
|      - | 6707 | `			/* Duplicate input */` |
|      3 | 6708 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 6709 | `			if( zDup  ){` |
|      3 | 6710 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 6711 | `				/* Register the aux data */` |
|      3 | 6712 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 6713 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 6714 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 6715 | `			}` |
|      1 | 6716 | `		}` |
|      - | 6717 | `	}` |
|      7 | 6718 | `	return PH7_OK;` |
|      4 | 6719 | `}` |
|      - | 6720 | `/*` |
|      - | 6721 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 6722 | ` *  Pad a string to a certain length with another string` |
|      - | 6723 | ` * Parameters` |
|      - | 6724 | ` *  $input` |
|      - | 6725 | ` *   The input string.` |
|      - | 6726 | ` * $pad_length` |
|      - | 6727 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 6728 | ` *   string, no padding takes place.` |
|      - | 6729 | ` * $pad_string` |
|      - | 6730 | ` *   Note:` |
|      - | 6731 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 6732 | ` *    divided by the pad_string's length.` |
|      - | 6733 | ` * $pad_type` |
|      - | 6734 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 6735 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 6736 | ` * Return` |
|      - | 6737 | ` *  The padded string.` |
|      - | 6738 | ` */` |
|     10 | 6739 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6740 | `{` |
|      - | 6741 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 6742 | `	const char *zIn,*zPad;` |
|     11 | 6743 | `	if( nArg < 2 ){` |
|      - | 6744 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 6745 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 6746 | `		return PH7_OK;` |
|      - | 6747 | `	}` |
|      - | 6748 | `	/* Extract the target string */` |
|     11 | 6749 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6750 | `	/* Padding length */` |
|     11 | 6751 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|     11 | 6752 | `	if( iPadlen > 0 ){` |
|      9 | 6753 | `		iPadlen -= iLen;` |
|      4 | 6754 | `	}` |
|     11 | 6755 | `	if( iPadlen < 1  ){` |
|      - | 6756 | `		/* Return the string verbatim */` |
|      5 | 6757 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      5 | 6758 | `		return PH7_OK;` |
|      - | 6759 | `	}` |
|      7 | 6760 | `	zPad = " "; /* Whitespace padding */` |
|      7 | 6761 | `	iStrpad = (int)sizeof(char);` |
|      7 | 6762 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      7 | 6763 | `	if( nArg > 2 ){` |
|      - | 6764 | `		/* Padding string */` |
|      7 | 6765 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      7 | 6766 | `		if( iStrpad < 1 ){` |
|      - | 6767 | `			/* An empty pad string throws a catchable ValueError in PHP 8` |
|      - | 6768 | `			 * (only reached once padding is actually required). */` |
|      3 | 6769 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6770 | `				"str_pad(): Argument #3 ($pad_string) must not be empty");` |
|      - | 6771 | `		}` |
|      5 | 6772 | `		if( nArg > 3 ){` |
|      - | 6773 | `			/* Padd type */` |
|      5 | 6774 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 6775 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 6776 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 6777 | `			}` |
|      2 | 6778 | `		}` |
|      2 | 6779 | `	}` |
|      5 | 6780 | `	iDiv = 1;` |
|      5 | 6781 | `	if( iType == 2 ){` |
|    ! 0 | 6782 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 6783 | `	}` |
|      - | 6784 | `	/* Perform the requested operation */` |
|      5 | 6785 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 6786 | `		jPad = iStrpad;` |
|      5 | 6787 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 6788 | `			/* Padding */` |
|      5 | 6789 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 6790 | `				break;` |
|      - | 6791 | `			}` |
|      3 | 6792 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 6793 | `		}` |
|      3 | 6794 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 6795 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 6796 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 6797 | `				if( jPad > iStrpad ){` |
|    ! 0 | 6798 | `					jPad = iStrpad;` |
|    ! 0 | 6799 | `				}` |
|      3 | 6800 | `				if( jPad < 1){` |
|    ! 0 | 6801 | `					break;` |
|      - | 6802 | `				}` |
|      3 | 6803 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 6804 | `			}` |
|      1 | 6805 | `		}` |
|      1 | 6806 | `	}` |
|      5 | 6807 | `	if( iLen > 0 ){` |
|      - | 6808 | `		/* Append the input string */` |
|      5 | 6809 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 6810 | `	}` |
|      5 | 6811 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 6812 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 6813 | `			/* Padding */` |
|      5 | 6814 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 6815 | `				break;` |
|      - | 6816 | `			}` |
|      3 | 6817 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 6818 | `		}` |
|      5 | 6819 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 6820 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 6821 | `			if( jPad > iStrpad ){` |
|    ! 0 | 6822 | `				jPad = iStrpad;` |
|    ! 0 | 6823 | `			}` |
|      3 | 6824 | `			if( jPad < 1){` |
|    ! 0 | 6825 | `				break;` |
|      - | 6826 | `			}` |
|      3 | 6827 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 6828 | `		}` |
|      1 | 6829 | `	}` |
|      5 | 6830 | `	return PH7_OK;` |
|      6 | 6831 | `}` |
|      - | 6832 | `/*` |
|      - | 6833 | ` * String replacement private data.` |
|      - | 6834 | ` */` |
|      - | 6835 | `typedef struct str_replace_data str_replace_data;` |
|      - | 6836 | `struct str_replace_data` |
|      - | 6837 | `{` |
|      - | 6838 | `	/* Used by the str_replace family to collect the search/replace arguments. */` |
|      - | 6839 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 6840 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 6841 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 6842 | `};` |
|      - | 6843 | `/*` |
|      - | 6844 | ` * Remove a substring.` |
|      - | 6845 | ` */` |
|      - | 6846 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 6847 | `	for(;;){\` |
|      - | 6848 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 6849 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 6850 | `		++OFFT;\` |
|      - | 6851 | `	}\` |
|      - | 6852 | `}` |
|      - | 6853 | `/*` |
|      - | 6854 | ` * Shift right and insert algorithm.` |
|      - | 6855 | ` */` |
|      - | 6856 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 6857 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 6858 | `		for(;;){\` |
|      - | 6859 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 6860 | `			if(INLEN < 1 ) { break; }\` |
|      - | 6861 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 6862 | `			--INLEN; \` |
|      - | 6863 | `		}\` |
|      - | 6864 | `		for(;;){\` |
|      - | 6865 | `				if(ELEN < 1) { break; }\` |
|      - | 6866 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 6867 | `				OFFT++;\` |
|      - | 6868 | `				ENTRY++;\` |
|      - | 6869 | `				--ELEN;\` |
|      - | 6870 | `		}\` |
|      - | 6871 | `}` |
|      - | 6872 | `/*` |
|      - | 6873 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 6874 | ` * replacement string [i.e: zReplace].` |
|      - | 6875 | ` */` |
|     32 | 6876 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 6877 | `{` |
|     33 | 6878 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 6879 | `	sxu32 n,m;` |
|     33 | 6880 | `	n = SyBlobLength(pWorker);` |
|     33 | 6881 | `	m = nOfft;` |
|      - | 6882 | `	/* Delete the old entry */` |
|    429 | 6883 | `	STRDEL(zInput,n,m,nLen);` |
|     33 | 6884 | `	SyBlobLength(pWorker) -= nLen;` |
|     33 | 6885 | `	if( nReplen > 0 ){` |
|     27 | 6886 | `		sxi32 iRep = nReplen;` |
|      - | 6887 | `		sxi32 rc;` |
|      - | 6888 | `		/*` |
|      - | 6889 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 6890 | `		 * string.` |
|      - | 6891 | `		 */` |
|     27 | 6892 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     27 | 6893 | `		if( rc != SXRET_OK ){` |
|      - | 6894 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 6895 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 6896 | `			return rc;` |
|      - | 6897 | `		}` |
|      - | 6898 | `		/* Perform the insertion now */` |
|     27 | 6899 | `		zInput = (char *)SyBlobData(pWorker);` |
|     27 | 6900 | `		n = SyBlobLength(pWorker);` |
|    129 | 6901 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     27 | 6902 | `		SyBlobLength(pWorker) += nReplen;` |
|     13 | 6903 | `	}` |
|     33 | 6904 | `	return SXRET_OK;` |
|     17 | 6905 | `}` |
|      - | 6906 | `/*` |
|      - | 6907 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 6908 | ` * to collect search/replace string.` |
|      - | 6909 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 6910 | ` */` |
|     26 | 6911 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 6912 | `{` |
|     27 | 6913 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 6914 | `	SyString sWorker;` |
|      - | 6915 | `	const char *zIn;` |
|      - | 6916 | `	int nByte;` |
|      - | 6917 | `	/* Extract a string representation of the given argument */` |
|     27 | 6918 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 6919 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 6920 | `	if( nByte > 0 ){` |
|      - | 6921 | `		char *zDup;` |
|      - | 6922 | `		/* Duplicate the chunk */` |
|     25 | 6923 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 6924 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 6925 | `			);` |
|     25 | 6926 | `		if( zDup == 0 ){` |
|      - | 6927 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 6928 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 6929 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 6930 | `			return SXERR_MEM;` |
|      - | 6931 | `		}` |
|     25 | 6932 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 6933 | `		/* Save the chunk */` |
|     25 | 6934 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 6935 | `	}` |
|      - | 6936 | `	/* Save for later processing */` |
|     27 | 6937 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 6938 | `	/* All done */` |
|     13 | 6939 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 6940 | `	return PH7_OK;` |
|     14 | 6941 | `}` |
|      - | 6942 | `/*` |
|      - | 6943 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6944 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 6945 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 6946 | ` * Parameters` |
|      - | 6947 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 6948 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 6949 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 6950 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 6951 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 6952 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 6953 | ` * $search` |
|      - | 6954 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 6955 | ` *  to designate multiple needles.` |
|      - | 6956 | ` * $replace` |
|      - | 6957 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 6958 | ` *  to designate multiple replacements.` |
|      - | 6959 | ` * $subject` |
|      - | 6960 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 6961 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 6962 | ` *  of subject, and the return value is an array as well.` |
|      - | 6963 | ` * $count (Not used)` |
|      - | 6964 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 6965 | ` * Return` |
|      - | 6966 | ` * This function returns a string or an array with the replaced values.` |
|      - | 6967 | ` */` |
|  28658 | 6968 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 6969 | `{` |
|      - | 6970 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 6971 | `	ProcStringMatch xMatch;` |
|      - | 6972 | `	const char *zIn,*zFunc;` |
|      - | 6973 | `	str_replace_data sRep;` |
|      - | 6974 | `	SyBlob sWorker;` |
|      - | 6975 | `	SySet sReplace;` |
|      - | 6976 | `	SySet sSearch;` |
|      - | 6977 | `	int rep_str;` |
|      - | 6978 | `	int nByte;` |
|      - | 6979 | `	sxi32 rc;` |
|  28663 | 6980 | `	if( nArg < 3 ){` |
|      - | 6981 | `		/* Missing/Invalid arguments,return null */` |
|    ! 0 | 6982 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6983 | `		return PH7_OK;` |
|      - | 6984 | `	}` |
|      - | 6985 | `	/* Initialize fields */` |
|  28663 | 6986 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  28663 | 6987 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  28663 | 6988 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  28663 | 6989 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  28663 | 6990 | `	sRep.pCtx = pCtx;` |
|  28663 | 6991 | `	sRep.pCollector = &sSearch;` |
|  28663 | 6992 | `	rep_str = 0;` |
|      - | 6993 | `	/* Extract the subject */` |
|  28663 | 6994 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  28663 | 6995 | `	if( nByte < 1 ){` |
|      - | 6996 | `		/* Nothing to replace,return the empty string */` |
|     29 | 6997 | `		ph7_result_string(pCtx,"",0);` |
|     29 | 6998 | `		return PH7_OK;` |
|      - | 6999 | `	}` |
|      - | 7000 | `	/* Copy the subject */` |
|  28635 | 7001 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 7002 | `	/* Search string */` |
|  28635 | 7003 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 7004 | `		/* Collect search string */` |
|      9 | 7005 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 7006 | `	}else{` |
|      - | 7007 | `		/* Single pattern */` |
|  28627 | 7008 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  28627 | 7009 | `		if( nByte < 1 ){` |
|      - | 7010 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 7011 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 7012 | `			return PH7_OK;` |
|      - | 7013 | `		}` |
|  28623 | 7014 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 7015 | `		/* Save for later processing */` |
|  28623 | 7016 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 7017 | `	}` |
|      - | 7018 | `	/* Replace string */` |
|  28631 | 7019 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 7020 | `		/* Collect replace string */` |
|      7 | 7021 | `		sRep.pCollector = &sReplace;` |
|      7 | 7022 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 7023 | `	}else{` |
|      - | 7024 | `		/* Single needle */` |
|  28625 | 7025 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  28625 | 7026 | `		rep_str = 1;` |
|  28625 | 7027 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 7028 | `		/* Save for later processing */` |
|  28625 | 7029 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 7030 | `	}` |
|      - | 7031 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  28631 | 7032 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 7033 | `		SySetRelease(&sSearch);` |
|    ! 0 | 7034 | `		SySetRelease(&sReplace);` |
|    ! 0 | 7035 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 7036 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7037 | `	}` |
|      - | 7038 | `	/* Reset loop cursors */` |
|  28631 | 7039 | `	SySetResetCursor(&sSearch);` |
|  28631 | 7040 | `	SySetResetCursor(&sReplace);` |
|  28631 | 7041 | `	pReplace = pSearch = 0; /* cc warning */` |
|  28631 | 7042 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 7043 | `	/* Extract function name */` |
|  28631 | 7044 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 7045 | `	/* Set the default pattern match routine */` |
|  28631 | 7046 | `	xMatch = SyBlobSearch;` |
|  28631 | 7047 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 7048 | `		/* Case insensitive pattern match */` |
|     11 | 7049 | `		xMatch = iPatternMatch;` |
|      5 | 7050 | `	}` |
|      - | 7051 | `	/* Start the replace process */` |
|  57265 | 7052 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 7053 | `		sxu32 nCount,nOfft;` |
|  28639 | 7054 | `		if( pSearch->nByte <  1 ){` |
|      - | 7055 | `			/* Empty string,ignore */` |
|      3 | 7056 | `			continue;` |
|      - | 7057 | `		}` |
|      - | 7058 | `		/* Extract the replace string */` |
|  28637 | 7059 | `		if( rep_str ){` |
|  28627 | 7060 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  14316 | 7061 | `		}else{` |
|     11 | 7062 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 7063 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 7064 | `				 * An empty string is used for the rest of replacement values` |
|      - | 7065 | `				 */` |
|      3 | 7066 | `				pReplace = 0;` |
|      1 | 7067 | `			}` |
|      - | 7068 | `		}` |
|  28637 | 7069 | `		if( pReplace == 0 ){` |
|      - | 7070 | `			/* Use an empty string instead */` |
|      3 | 7071 | `			pReplace = &sTemp;` |
|      1 | 7072 | `		}` |
|  28637 | 7073 | `		nOfft = nCount = 0;` |
|  14332 | 7074 | `		for(;;){` |
|  28669 | 7075 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 7076 | `				break;` |
|      - | 7077 | `			}` |
|      - | 7078 | `			/* Perform a pattern lookup */` |
|  42983 | 7079 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  28652 | 7080 | `				pSearch->nByte,&nOfft);` |
|  28657 | 7081 | `			if( rc != SXRET_OK ){` |
|      - | 7082 | `				/* Pattern not found */` |
|  28625 | 7083 | `				break;` |
|      - | 7084 | `			}` |
|      - | 7085 | `			/* Perform the replace operation */` |
|     33 | 7086 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     33 | 7087 | `			if( rc != SXRET_OK ){` |
|      - | 7088 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 7089 | `				SySetRelease(&sSearch);` |
|    ! 0 | 7090 | `				SySetRelease(&sReplace);` |
|    ! 0 | 7091 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 7092 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 7093 | `			}` |
|      - | 7094 | `			/* Increment offset counter */` |
|     33 | 7095 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 7096 | `		}` |
|      5 | 7097 | `	}` |
|      - | 7098 | `	/* All done,clean-up the mess left behind */` |
|  28631 | 7099 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  28631 | 7100 | `	SySetRelease(&sSearch);` |
|  28631 | 7101 | `	SySetRelease(&sReplace);` |
|  28631 | 7102 | `	SyBlobRelease(&sWorker);` |
|  28631 | 7103 | `	if( rc != PH7_OK ){` |
|    ! 0 | 7104 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7105 | `	}` |
|  28631 | 7106 | `	return PH7_OK;` |
|  14334 | 7107 | `}` |
|      - | 7108 | `/*` |
|      - | 7109 | ` * strtr() array form: a single (key,value) pair copied out of the replace_pairs` |
|      - | 7110 | ` * array. The bytes are owned by a persistent pool (see strtr_collect) rather than` |
|      - | 7111 | ` * the transient walker values, which HashmapWalk releases after each callback, so` |
|      - | 7112 | ` * we store byte offsets into that pool instead of raw pointers.` |
|      - | 7113 | ` */` |
|      - | 7114 | `typedef struct strtr_entry strtr_entry;` |
|      - | 7115 | `struct strtr_entry` |
|      - | 7116 | `{` |
|      - | 7117 | `	sxu32 nKeyOfft; /* Offset of the search key inside the pool */` |
|      - | 7118 | `	sxu32 nKeyLen;  /* Length of the search key */` |
|      - | 7119 | `	sxu32 nValOfft; /* Offset of the replacement inside the pool */` |
|      - | 7120 | `	sxu32 nValLen;  /* Length of the replacement */` |
|      - | 7121 | `};` |
|      - | 7122 | `typedef struct strtr_collect strtr_collect;` |
|      - | 7123 | `struct strtr_collect` |
|      - | 7124 | `{` |
|      - | 7125 | `	SyBlob *pPool;  /* Byte pool holding copied key + value bytes */` |
|      - | 7126 | `	SySet  *pTable; /* Set of strtr_entry (parallel offsets into pPool) */` |
|      - | 7127 | `	sxi32   rc;     /* Carries an allocation failure (SXERR_MEM) out of the walker */` |
|      - | 7128 | `};` |
|      - | 7129 | `/*` |
|      - | 7130 | ` * Collect one replace_pairs entry into the persistent pool/offset table.` |
|      - | 7131 | ` * PHP coerces both the key and the value to string (an integer key becomes its` |
|      - | 7132 | ` * decimal form) and ignores an empty-string key.` |
|      - | 7133 | ` */` |
|     20 | 7134 | `static int StrtrCollectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 7135 | `{` |
|     21 | 7136 | `	strtr_collect *pCol = (strtr_collect *)pUserData;` |
|      - | 7137 | `	const char *zKey,*zVal;` |
|      - | 7138 | `	strtr_entry sEnt;` |
|      - | 7139 | `	int nKey,nVal;` |
|     21 | 7140 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     21 | 7141 | `	if( nKey < 1 ){` |
|      - | 7142 | `		/* PHP ignores an empty-string key (it also emits a warning we do not replicate). */` |
|    ! 0 | 7143 | `		return PH7_OK;` |
|      - | 7144 | `	}` |
|     21 | 7145 | `	zVal = ph7_value_to_string(pData,&nVal);` |
|     21 | 7146 | `	sEnt.nKeyOfft = SyBlobLength(pCol->pPool);` |
|     21 | 7147 | `	sEnt.nKeyLen  = (sxu32)nKey;` |
|     21 | 7148 | `	if( SyBlobAppend(pCol->pPool,(const void *)zKey,(sxu32)nKey) != SXRET_OK ){` |
|    ! 0 | 7149 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 7150 | `		return SXERR_ABORT;` |
|      - | 7151 | `	}` |
|     21 | 7152 | `	sEnt.nValOfft = SyBlobLength(pCol->pPool);` |
|     21 | 7153 | `	sEnt.nValLen  = (sxu32)nVal;` |
|     21 | 7154 | `	if( nVal > 0 && SyBlobAppend(pCol->pPool,(const void *)zVal,(sxu32)nVal) != SXRET_OK ){` |
|    ! 0 | 7155 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 7156 | `		return SXERR_ABORT;` |
|      - | 7157 | `	}` |
|     21 | 7158 | `	if( SySetPut(pCol->pTable,(const void *)&sEnt) != SXRET_OK ){` |
|    ! 0 | 7159 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 7160 | `		return SXERR_ABORT;` |
|      - | 7161 | `	}` |
|     21 | 7162 | `	return PH7_OK;` |
|     11 | 7163 | `}` |
|      - | 7164 | `/*` |
|      - | 7165 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 7166 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 7167 | ` *  Translate characters or replace substrings.` |
|      - | 7168 | ` * Parameters` |
|      - | 7169 | ` *  $str` |
|      - | 7170 | ` *  The string being translated.` |
|      - | 7171 | ` * $from` |
|      - | 7172 | ` *  The string being translated to to.` |
|      - | 7173 | ` * $to` |
|      - | 7174 | ` *  The string replacing from.` |
|      - | 7175 | ` * $replace_pairs` |
|      - | 7176 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 7177 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 7178 | ` * Return` |
|      - | 7179 | ` *  The translated string.` |
|      - | 7180 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 7181 | ` */` |
|     12 | 7182 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7183 | `{` |
|      - | 7184 | `	const char *zIn;` |
|      - | 7185 | `	int nLen;` |
|     13 | 7186 | `	if( nArg < 1 ){` |
|      - | 7187 | `		/* Nothing to replace,return FALSE */` |
|    ! 0 | 7188 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7189 | `		return PH7_OK;` |
|      - | 7190 | `	}` |
|     13 | 7191 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 7192 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 7193 | `		/* Invalid arguments */` |
|    ! 0 | 7194 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 7195 | `		return PH7_OK;` |
|      - | 7196 | `	}` |
|     18 | 7197 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 7198 | `		strtr_collect sCol;` |
|      - | 7199 | `		SyBlob sPool,sWorker;` |
|      - | 7200 | `		SySet sTable;` |
|      - | 7201 | `		const char *zPool;` |
|      - | 7202 | `		strtr_entry *pEnt;` |
|      - | 7203 | `		sxi32 rc;` |
|      - | 7204 | `		int i,iRun;` |
|      - | 7205 | `		/*` |
|      - | 7206 | `		 * PHP's array-form strtr is a single left-to-right pass over the subject:` |
|      - | 7207 | `		 * at every position it substitutes the LONGEST replace_pairs key that` |
|      - | 7208 | `		 * matches there, then advances past the key (replacements are never` |
|      - | 7209 | `		 * rescanned). It is not a sequential per-key global replace. First copy` |
|      - | 7210 | `		 * the pairs into a persistent pool, then run that scan.` |
|      - | 7211 | `		 */` |
|     11 | 7212 | `		SyBlobInit(&sPool,&pCtx->pVm->sAllocator);` |
|     11 | 7213 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     11 | 7214 | `		SySetInit(&sTable,&pCtx->pVm->sAllocator,sizeof(strtr_entry));` |
|     11 | 7215 | `		sCol.pPool  = &sPool;` |
|     11 | 7216 | `		sCol.pTable = &sTable;` |
|     11 | 7217 | `		sCol.rc     = SXRET_OK;` |
|     11 | 7218 | `		ph7_array_walk(apArg[1],StrtrCollectWalker,&sCol);` |
|     11 | 7219 | `		if( sCol.rc != SXRET_OK ){` |
|      - | 7220 | `			/* Allocation failure while collecting the pairs: surface a fatal */` |
|    ! 0 | 7221 | `			SyBlobRelease(&sPool);` |
|    ! 0 | 7222 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 7223 | `			SySetRelease(&sTable);` |
|    ! 0 | 7224 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 7225 | `		}` |
|      - | 7226 | `		/* The pool is now stable, so offsets can be resolved against its base. */` |
|     11 | 7227 | `		zPool = (const char *)SyBlobData(&sPool);` |
|     11 | 7228 | `		rc = SXRET_OK;` |
|     11 | 7229 | `		iRun = 0; /* Start of the pending run of unmatched bytes copied verbatim. */` |
|     43 | 7230 | `		for( i = 0 ; i < nLen ; ){` |
|     33 | 7231 | `			strtr_entry *pBest = 0;` |
|     33 | 7232 | `			sxu32 nBest = 0;` |
|      - | 7233 | `			/* Pick the longest key that matches at the current position. */` |
|     33 | 7234 | `			SySetResetCursor(&sTable);` |
|     97 | 7235 | `			while( SXRET_OK == SySetGetNextEntry(&sTable,(void **)&pEnt) ){` |
|     64 | 7236 | `				if( pEnt->nKeyLen > nBest` |
|     60 | 7237 | `					&& pEnt->nKeyLen <= (sxu32)(nLen - i)` |
|     56 | 7238 | `					&& SyMemcmp(zPool + pEnt->nKeyOfft,zIn + i,pEnt->nKeyLen) == 0 ){` |
|     29 | 7239 | `					nBest = pEnt->nKeyLen;` |
|     29 | 7240 | `					pBest = pEnt;` |
|     14 | 7241 | `				}` |
|      1 | 7242 | `			}` |
|     33 | 7243 | `			if( pBest == 0 ){` |
|      - | 7244 | `				/* No key here: extend the literal run and copy it in one shot later. */` |
|      9 | 7245 | `				i++;` |
|      9 | 7246 | `				continue;` |
|      - | 7247 | `			}` |
|      - | 7248 | `			/* Flush the pending literal run, then the replacement. */` |
|     25 | 7249 | `			if( i > iRun ){` |
|      5 | 7250 | `				rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(i - iRun));` |
|      2 | 7251 | `			}` |
|     25 | 7252 | `			if( rc == SXRET_OK && pBest->nValLen > 0 ){` |
|     25 | 7253 | `				rc = SyBlobAppend(&sWorker,zPool + pBest->nValOfft,pBest->nValLen);` |
|     12 | 7254 | `			}` |
|     25 | 7255 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 7256 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 7257 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 7258 | `				SySetRelease(&sTable);` |
|    ! 0 | 7259 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 7260 | `			}` |
|     25 | 7261 | `			i += (int)pBest->nKeyLen;` |
|     25 | 7262 | `			iRun = i;` |
|      1 | 7263 | `		}` |
|      - | 7264 | `		/* Flush the trailing literal run. */` |
|     11 | 7265 | `		if( nLen > iRun ){` |
|      3 | 7266 | `			rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(nLen - iRun));` |
|      3 | 7267 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 7268 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 7269 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 7270 | `				SySetRelease(&sTable);` |
|    ! 0 | 7271 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 7272 | `			}` |
|      1 | 7273 | `		}` |
|      - | 7274 | `		/* All done, return the result string */` |
|     16 | 7275 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|     10 | 7276 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 7277 | `		/* Clean-up */` |
|     11 | 7278 | `		SyBlobRelease(&sPool);` |
|     11 | 7279 | `		SyBlobRelease(&sWorker);` |
|     11 | 7280 | `		SySetRelease(&sTable);` |
|     11 | 7281 | `		if( rc != PH7_OK ){` |
|    ! 0 | 7282 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 7283 | `		}` |
|      6 | 7284 | `	}else{` |
|      - | 7285 | `		int i,flen,tlen,c,iOfft;` |
|      - | 7286 | `		const char *zFrom,*zTo;` |
|      3 | 7287 | `		if( nArg < 3 ){` |
|      - | 7288 | `			/* Nothing to replace */` |
|    ! 0 | 7289 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 7290 | `			return PH7_OK;` |
|      - | 7291 | `		}` |
|      - | 7292 | `		/* Extract given arguments */` |
|      3 | 7293 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 7294 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 7295 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 7296 | `			/* Nothing to replace */` |
|    ! 0 | 7297 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 7298 | `			return PH7_OK;` |
|      - | 7299 | `		}` |
|      - | 7300 | `		/* Start the replace process */` |
|     13 | 7301 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 7302 | `			c = zIn[i];` |
|     11 | 7303 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 7304 | `				if ( iOfft < tlen ){` |
|      5 | 7305 | `					c = zTo[iOfft];` |
|      2 | 7306 | `				}` |
|      2 | 7307 | `			}` |
|     11 | 7308 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 7309 |  |
|      6 | 7310 | `		}` |
|      - | 7311 | `	}` |
|     13 | 7312 | `	return PH7_OK;` |
|      7 | 7313 | `}` |
|      - | 7314 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7315 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7316 | `/*` |
|      - | 7317 | ` * Parse an INI string.` |
|      - | 7318 |  |
|      - | 7319 | ` * According to wikipedia` |
|      - | 7320 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 7321 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 7322 | ` *  Format` |
|      - | 7323 | `*    Properties` |
|      - | 7324 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 7325 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 7326 | `*     Example:` |
|      - | 7327 | `*      name=value` |
|      - | 7328 | `*    Sections` |
|      - | 7329 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 7330 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 7331 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 7332 | `*     or the end of the file. Sections may not be nested.` |
|      - | 7333 | `*     Example:` |
|      - | 7334 | `*      [section]` |
|      - | 7335 | `*   Comments` |
|      - | 7336 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 7337 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 7338 | `*/` |
|     12 | 7339 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 7340 | `{` |
|      - | 7341 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 7342 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 7343 | `	SyHashEntry *pEntry;` |
|      - | 7344 | `	SyString sEntry;` |
|      - | 7345 | `	SyHash sHash;` |
|      - | 7346 | `	int c;` |
|      - | 7347 | `	/* Create an empty array and worker variables */` |
|     13 | 7348 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 7349 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 7350 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 7351 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 7352 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|    ! 0 | 7353 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7354 | `	}` |
|     13 | 7355 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 7356 | `	pCur = pArray;` |
|      - | 7357 | `	/* Start the parse process */` |
|     21 | 7358 | `	for(;;){` |
|      - | 7359 | `		/* Ignore leading white spaces */` |
|     69 | 7360 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 7361 | `			zIn++;` |
|      1 | 7362 | `		}` |
|     43 | 7363 | `		if( zIn >= zEnd ){` |
|      - | 7364 | `			/* No more input to process */` |
|     13 | 7365 | `			break;` |
|      - | 7366 | `		}` |
|     31 | 7367 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 7368 | `			/* Comment til the end of line */` |
|    ! 0 | 7369 | `			zIn++;` |
|    ! 0 | 7370 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 7371 | `				zIn++;` |
|    ! 0 | 7372 | `			}` |
|    ! 0 | 7373 | `			continue;` |
|      - | 7374 | `		}` |
|      - | 7375 | `		/* Reset the string cursor of the working variable */` |
|     31 | 7376 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 7377 | `		if( zIn[0] == '[' ){` |
|      - | 7378 | `			/* Section: Extract the section name */` |
|      9 | 7379 | `			zIn++;` |
|      9 | 7380 | `			zCur = zIn;` |
|     73 | 7381 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 7382 | `				zIn++;` |
|      1 | 7383 | `			}` |
|      9 | 7384 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 7385 | `				/* Save the section name */` |
|      5 | 7386 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 7387 | `				SyStringFullTrim(&sEntry);` |
|      5 | 7388 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 7389 | `				if( sEntry.nByte > 0 ){` |
|      - | 7390 | `					/* Associate an array with the section */` |
|      5 | 7391 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 7392 | `					if( pSection ){` |
|      5 | 7393 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 7394 | `						pCur = pSection;` |
|      2 | 7395 | `					}` |
|      2 | 7396 | `				}` |
|      2 | 7397 | `			}` |
|      9 | 7398 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 7399 | `		}else{` |
|      - | 7400 | `			ph7_value *pOldCur;` |
|      - | 7401 | `			int is_array;` |
|      - | 7402 | `			int iLen;` |
|      - | 7403 | `			/* Properties */` |
|     23 | 7404 | `			is_array = 0;` |
|     23 | 7405 | `			zCur = zIn;` |
|     23 | 7406 | `			iLen = 0; /* cc warning */` |
|     23 | 7407 | `			pOldCur = pCur;` |
|    155 | 7408 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 7409 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 7410 | `					/* Array */` |
|    ! 0 | 7411 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 7412 | `					is_array = 1;` |
|    ! 0 | 7413 | `					if( iLen > 0 ){` |
|    ! 0 | 7414 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 7415 | `						/* Query the hashtable */` |
|    ! 0 | 7416 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 7417 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 7418 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 7419 | `						if( pEntry ){` |
|    ! 0 | 7420 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 7421 | `						}else{` |
|      - | 7422 | `							/* Create an empty array */` |
|    ! 0 | 7423 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 7424 | `							if( pvArr ){` |
|      - | 7425 | `								/* Save the entry */` |
|    ! 0 | 7426 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 7427 | `								/* Insert the entry */` |
|    ! 0 | 7428 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 7429 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 7430 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 7431 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 7432 | `							}` |
|      - | 7433 | `						}` |
|    ! 0 | 7434 | `						if( pvArr ){` |
|    ! 0 | 7435 | `							pCur = pvArr;` |
|    ! 0 | 7436 | `						}` |
|    ! 0 | 7437 | `					}` |
|    ! 0 | 7438 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 7439 | `						zIn++;` |
|    ! 0 | 7440 | `					}` |
|    ! 0 | 7441 | `				}` |
|    133 | 7442 | `				zIn++;` |
|      1 | 7443 | `			}` |
|     23 | 7444 | `			if( !is_array ){` |
|     23 | 7445 | `				iLen = (int)(zIn-zCur);` |
|     11 | 7446 | `			}` |
|      - | 7447 | `			/* Trim the key */` |
|     23 | 7448 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 7449 | `			SyStringFullTrim(&sEntry);` |
|     23 | 7450 | `			if( sEntry.nByte > 0 ){` |
|     23 | 7451 | `				if( !is_array ){` |
|      - | 7452 | `					/* Save the key name */` |
|     23 | 7453 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 7454 | `				}` |
|      - | 7455 | `				/* extract key value */` |
|     23 | 7456 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 7457 | `				zIn++; /* '=' */` |
|     39 | 7458 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 7459 | `					zIn++;` |
|      1 | 7460 | `				}` |
|     23 | 7461 | `				if( zIn < zEnd ){` |
|     21 | 7462 | `					zCur = zIn;` |
|     21 | 7463 | `					c = zIn[0];` |
|     21 | 7464 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 7465 | `						zIn++;` |
|      - | 7466 | `						/* Delimit the value */` |
|    ! 0 | 7467 | `						while( zIn < zEnd ){` |
|    ! 0 | 7468 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 7469 | `								break;` |
|      - | 7470 | `							}` |
|    ! 0 | 7471 | `							zIn++;` |
|    ! 0 | 7472 | `						}` |
|    ! 0 | 7473 | `						if( zIn < zEnd ){` |
|    ! 0 | 7474 | `							zIn++;` |
|    ! 0 | 7475 | `						}` |
|    ! 0 | 7476 | `					}else{` |
|    125 | 7477 | `						while( zIn < zEnd ){` |
|    123 | 7478 | `							if( zIn[0] == '\n' ){` |
|     19 | 7479 | `								if( zIn[-1] != '\\' ){` |
|     19 | 7480 | `									break;` |
|    ! 0 | 7481 | `								}` |
|    105 | 7482 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 7483 | `								/* Inline comments */` |
|    ! 0 | 7484 | `								break;` |
|      - | 7485 | `							}` |
|    105 | 7486 | `							zIn++;` |
|      1 | 7487 | `						}` |
|      - | 7488 | `					}` |
|      - | 7489 | `					/* Trim the value */` |
|     21 | 7490 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 7491 | `					SyStringFullTrim(&sEntry);` |
|     21 | 7492 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 7493 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 7494 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 7495 | `					}` |
|     21 | 7496 | `					if( sEntry.nByte > 0 ){` |
|     21 | 7497 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 7498 | `					}` |
|      - | 7499 | `					/* Insert the key and it's value */` |
|     21 | 7500 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 7501 | `				}` |
|     12 | 7502 | `			}else{` |
|    ! 0 | 7503 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 7504 | `					zIn++;` |
|    ! 0 | 7505 | `				}` |
|      - | 7506 | `			}` |
|     23 | 7507 | `			pCur = pOldCur;` |
|      - | 7508 | `		}` |
|      1 | 7509 | `	}` |
|     13 | 7510 | `	SyHashRelease(&sHash);` |
|      - | 7511 | `	/* Return the parse of the INI string */` |
|     13 | 7512 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 7513 | `	return SXRET_OK;` |
|      7 | 7514 | `}` |
|      - | 7515 | `/*` |
|      - | 7516 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 7517 | ` *  Parse a configuration string.` |
|      - | 7518 | ` * Parameters` |
|      - | 7519 | ` *  $ini` |
|      - | 7520 | ` *   The contents of the ini file being parsed.` |
|      - | 7521 | ` *  $process_sections` |
|      - | 7522 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 7523 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 7524 | ` *  $scanner_mode (Not used)` |
|      - | 7525 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 7526 | ` *   then option values will not be parsed.` |
|      - | 7527 | ` * Return` |
|      - | 7528 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 7529 | ` */` |
|     10 | 7530 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7531 | `{` |
|      - | 7532 | `	const char *zIni;` |
|      - | 7533 | `	int nByte;` |
|     11 | 7534 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7535 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 7536 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7537 | `		return PH7_OK;` |
|      - | 7538 | `	}` |
|      - | 7539 | `	/* Extract the raw INI buffer */` |
|     11 | 7540 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 7541 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|     11 | 7542 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      6 | 7543 | `}` |
|      - | 7544 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7545 |  |
|      - | 7546 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7547 |  |
|      - | 7548 | `/*` |
|      - | 7549 | ` * Ctype Functions.` |
|      - | 7550 | ` * Status:` |
|      - | 7551 | ` *    Stable.` |
|      - | 7552 | ` */` |
|      - | 7553 | `/*` |
|      - | 7554 | ` * bool ctype_alnum(string $text)` |
|      - | 7555 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 7556 | ` * Parameters` |
|      - | 7557 | ` *  $text` |
|      - | 7558 | ` *   The tested string.` |
|      - | 7559 | ` * Return` |
|      - | 7560 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 7561 | ` */` |
|     14 | 7562 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7563 | `{` |
|      - | 7564 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7565 | `	int nLen;` |
|     15 | 7566 | `	if( nArg < 1 ){` |
|      - | 7567 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7568 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7569 | `		return PH7_OK;` |
|      - | 7570 | `	}` |
|      - | 7571 | `	/* Extract the target string */` |
|     15 | 7572 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 7573 | `	zEnd = &zIn[nLen];` |
|     15 | 7574 | `	if( nLen < 1 ){` |
|      - | 7575 | `		/* Empty string,return FALSE */` |
|      3 | 7576 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7577 | `		return PH7_OK;` |
|      - | 7578 | `	}` |
|      - | 7579 | `	/* Perform the requested operation */` |
|     32 | 7580 | `	for(;;){` |
|     65 | 7581 | `		if( zIn >= zEnd ){` |
|      - | 7582 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7583 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7584 | `			return PH7_OK;` |
|      - | 7585 | `		}` |
|     57 | 7586 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 7587 | `			break;` |
|      - | 7588 | `		}` |
|      - | 7589 | `		/* Point to the next character */` |
|     53 | 7590 | `		zIn++;` |
|      1 | 7591 | `	}` |
|      - | 7592 | `	/* The test failed,return FALSE */` |
|      5 | 7593 | `	ph7_result_bool(pCtx,0);` |
|      5 | 7594 | `	return PH7_OK;` |
|      8 | 7595 | `}` |
|      - | 7596 | `/*` |
|      - | 7597 | ` * bool ctype_alpha(string $text)` |
|      - | 7598 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 7599 | ` * Parameters` |
|      - | 7600 | ` *  $text` |
|      - | 7601 | ` *   The tested string.` |
|      - | 7602 | ` * Return` |
|      - | 7603 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 7604 | ` */` |
|     16 | 7605 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7606 | `{` |
|      - | 7607 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7608 | `	int nLen;` |
|     17 | 7609 | `	if( nArg < 1 ){` |
|      - | 7610 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7611 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7612 | `		return PH7_OK;` |
|      - | 7613 | `	}` |
|      - | 7614 | `	/* Extract the target string */` |
|     17 | 7615 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7616 | `	zEnd = &zIn[nLen];` |
|     17 | 7617 | `	if( nLen < 1 ){` |
|      - | 7618 | `		/* Empty string,return FALSE */` |
|      3 | 7619 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7620 | `		return PH7_OK;` |
|      - | 7621 | `	}` |
|      - | 7622 | `	/* Perform the requested operation */` |
|     42 | 7623 | `	for(;;){` |
|     85 | 7624 | `		if( zIn >= zEnd ){` |
|      - | 7625 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7626 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7627 | `			return PH7_OK;` |
|      - | 7628 | `		}` |
|     77 | 7629 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 7630 | `			break;` |
|      - | 7631 | `		}` |
|      - | 7632 | `		/* Point to the next character */` |
|     71 | 7633 | `		zIn++;` |
|      1 | 7634 | `	}` |
|      - | 7635 | `	/* The test failed,return FALSE */` |
|      7 | 7636 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7637 | `	return PH7_OK;` |
|      9 | 7638 | `}` |
|      - | 7639 | `/*` |
|      - | 7640 | ` * bool ctype_cntrl(string $text)` |
|      - | 7641 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 7642 | ` * Parameters` |
|      - | 7643 | ` *  $text` |
|      - | 7644 | ` *   The tested string.` |
|      - | 7645 | ` * Return` |
|      - | 7646 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 7647 | ` */` |
|     16 | 7648 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7649 | `{` |
|      - | 7650 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7651 | `	int nLen;` |
|     17 | 7652 | `	if( nArg < 1 ){` |
|      - | 7653 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7654 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7655 | `		return PH7_OK;` |
|      - | 7656 | `	}` |
|      - | 7657 | `	/* Extract the target string */` |
|     17 | 7658 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7659 | `	zEnd = &zIn[nLen];` |
|     17 | 7660 | `	if( nLen < 1 ){` |
|      - | 7661 | `		/* Empty string,return FALSE */` |
|      3 | 7662 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7663 | `		return PH7_OK;` |
|      - | 7664 | `	}` |
|      - | 7665 | `	/* Perform the requested operation */` |
|     14 | 7666 | `	for(;;){` |
|     29 | 7667 | `		if( zIn >= zEnd ){` |
|      - | 7668 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7669 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7670 | `			return PH7_OK;` |
|      - | 7671 | `		}` |
|     21 | 7672 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7673 | `			/* UTF-8 stream  */` |
|    ! 0 | 7674 | `			break;` |
|      - | 7675 | `		}` |
|     21 | 7676 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 7677 | `			break;` |
|      - | 7678 | `		}` |
|      - | 7679 | `		/* Point to the next character */` |
|     15 | 7680 | `		zIn++;` |
|      1 | 7681 | `	}` |
|      - | 7682 | `	/* The test failed,return FALSE */` |
|      7 | 7683 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7684 | `	return PH7_OK;` |
|      9 | 7685 | `}` |
|      - | 7686 | `/*` |
|      - | 7687 | ` * bool ctype_digit(string $text)` |
|      - | 7688 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 7689 | ` * Parameters` |
|      - | 7690 | ` *  $text` |
|      - | 7691 | ` *   The tested string.` |
|      - | 7692 | ` * Return` |
|      - | 7693 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 7694 | ` */` |
|   1615 | 7695 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7696 | `{` |
|      - | 7697 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7698 | `	int nLen;` |
|   1620 | 7699 | `	if( nArg < 1 ){` |
|      - | 7700 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7701 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7702 | `		return PH7_OK;` |
|      - | 7703 | `	}` |
|      - | 7704 | `	/* Extract the target string */` |
|   1620 | 7705 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1620 | 7706 | `	zEnd = &zIn[nLen];` |
|   1620 | 7707 | `	if( nLen < 1 ){` |
|      - | 7708 | `		/* Empty string,return FALSE */` |
|      3 | 7709 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7710 | `		return PH7_OK;` |
|      - | 7711 | `	}` |
|      - | 7712 | `	/* Perform the requested operation */` |
|   1517 | 7713 | `	for(;;){` |
|   3037 | 7714 | `		if( zIn >= zEnd ){` |
|      - | 7715 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1374 | 7716 | `			ph7_result_bool(pCtx,1);` |
|   1374 | 7717 | `			return PH7_OK;` |
|      - | 7718 | `		}` |
|   1668 | 7719 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7720 | `			/* UTF-8 stream  */` |
|    ! 0 | 7721 | `			break;` |
|      - | 7722 | `		}` |
|   1668 | 7723 | `		if( !SyisDigit(zIn[0]) ){` |
|    249 | 7724 | `			break;` |
|      - | 7725 | `		}` |
|      - | 7726 | `		/* Point to the next character */` |
|   1424 | 7727 | `		zIn++;` |
|      5 | 7728 | `	}` |
|      - | 7729 | `	/* The test failed,return FALSE */` |
|    249 | 7730 | `	ph7_result_bool(pCtx,0);` |
|    249 | 7731 | `	return PH7_OK;` |
|    813 | 7732 | `}` |
|      - | 7733 | `/*` |
|      - | 7734 | ` * bool ctype_xdigit(string $text)` |
|      - | 7735 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 7736 | ` * Parameters` |
|      - | 7737 | ` *  $text` |
|      - | 7738 | ` *   The tested string.` |
|      - | 7739 | ` * Return` |
|      - | 7740 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 7741 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 7742 | ` */` |
|     18 | 7743 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7744 | `{` |
|      - | 7745 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7746 | `	int nLen;` |
|     19 | 7747 | `	if( nArg < 1 ){` |
|      - | 7748 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7749 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7750 | `		return PH7_OK;` |
|      - | 7751 | `	}` |
|      - | 7752 | `	/* Extract the target string */` |
|     19 | 7753 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7754 | `	zEnd = &zIn[nLen];` |
|     19 | 7755 | `	if( nLen < 1 ){` |
|      - | 7756 | `		/* Empty string,return FALSE */` |
|      3 | 7757 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7758 | `		return PH7_OK;` |
|      - | 7759 | `	}` |
|      - | 7760 | `	/* Perform the requested operation */` |
|     46 | 7761 | `	for(;;){` |
|     93 | 7762 | `		if( zIn >= zEnd ){` |
|      - | 7763 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 7764 | `			ph7_result_bool(pCtx,1);` |
|     11 | 7765 | `			return PH7_OK;` |
|      - | 7766 | `		}` |
|     83 | 7767 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7768 | `			/* UTF-8 stream  */` |
|    ! 0 | 7769 | `			break;` |
|      - | 7770 | `		}` |
|     83 | 7771 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 7772 | `			break;` |
|      - | 7773 | `		}` |
|      - | 7774 | `		/* Point to the next character */` |
|     77 | 7775 | `		zIn++;` |
|      1 | 7776 | `	}` |
|      - | 7777 | `	/* The test failed,return FALSE */` |
|      7 | 7778 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7779 | `	return PH7_OK;` |
|     10 | 7780 | `}` |
|      - | 7781 | `/*` |
|      - | 7782 | ` * bool ctype_graph(string $text)` |
|      - | 7783 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 7784 | ` * Parameters` |
|      - | 7785 | ` *  $text` |
|      - | 7786 | ` *   The tested string.` |
|      - | 7787 | ` * Return` |
|      - | 7788 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 7789 | ` * (no white space), FALSE otherwise.` |
|      - | 7790 | ` */` |
|     16 | 7791 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7792 | `{` |
|      - | 7793 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7794 | `	int nLen;` |
|     17 | 7795 | `	if( nArg < 1 ){` |
|      - | 7796 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7797 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7798 | `		return PH7_OK;` |
|      - | 7799 | `	}` |
|      - | 7800 | `	/* Extract the target string */` |
|     17 | 7801 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7802 | `	zEnd = &zIn[nLen];` |
|     17 | 7803 | `	if( nLen < 1 ){` |
|      - | 7804 | `		/* Empty string,return FALSE */` |
|      3 | 7805 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7806 | `		return PH7_OK;` |
|      - | 7807 | `	}` |
|      - | 7808 | `	/* Perform the requested operation */` |
|     57 | 7809 | `	for(;;){` |
|    115 | 7810 | `		if( zIn >= zEnd ){` |
|      - | 7811 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7812 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7813 | `			return PH7_OK;` |
|      - | 7814 | `		}` |
|    107 | 7815 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7816 | `			/* UTF-8 stream  */` |
|    ! 0 | 7817 | `			break;` |
|      - | 7818 | `		}` |
|    107 | 7819 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 7820 | `			break;` |
|      - | 7821 | `		}` |
|      - | 7822 | `		/* Point to the next character */` |
|    101 | 7823 | `		zIn++;` |
|      1 | 7824 | `	}` |
|      - | 7825 | `	/* The test failed,return FALSE */` |
|      7 | 7826 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7827 | `	return PH7_OK;` |
|      9 | 7828 | `}` |
|      - | 7829 | `/*` |
|      - | 7830 | ` * bool ctype_print(string $text)` |
|      - | 7831 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 7832 | ` * Parameters` |
|      - | 7833 | ` *  $text` |
|      - | 7834 | ` *   The tested string.` |
|      - | 7835 | ` * Return` |
|      - | 7836 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 7837 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 7838 | ` *  or control function at all.` |
|      - | 7839 | ` */` |
|     16 | 7840 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7841 | `{` |
|      - | 7842 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7843 | `	int nLen;` |
|     17 | 7844 | `	if( nArg < 1 ){` |
|      - | 7845 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7846 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7847 | `		return PH7_OK;` |
|      - | 7848 | `	}` |
|      - | 7849 | `	/* Extract the target string */` |
|     17 | 7850 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7851 | `	zEnd = &zIn[nLen];` |
|     17 | 7852 | `	if( nLen < 1 ){` |
|      - | 7853 | `		/* Empty string,return FALSE */` |
|      3 | 7854 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7855 | `		return PH7_OK;` |
|      - | 7856 | `	}` |
|      - | 7857 | `	/* Perform the requested operation */` |
|     63 | 7858 | `	for(;;){` |
|    127 | 7859 | `		if( zIn >= zEnd ){` |
|      - | 7860 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7861 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7862 | `			return PH7_OK;` |
|      - | 7863 | `		}` |
|    119 | 7864 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7865 | `			/* UTF-8 stream  */` |
|    ! 0 | 7866 | `			break;` |
|      - | 7867 | `		}` |
|    119 | 7868 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 7869 | `			break;` |
|      - | 7870 | `		}` |
|      - | 7871 | `		/* Point to the next character */` |
|    113 | 7872 | `		zIn++;` |
|      1 | 7873 | `	}` |
|      - | 7874 | `	/* The test failed,return FALSE */` |
|      7 | 7875 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7876 | `	return PH7_OK;` |
|      9 | 7877 | `}` |
|      - | 7878 | `/*` |
|      - | 7879 | ` * bool ctype_punct(string $text)` |
|      - | 7880 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 7881 | ` * Parameters` |
|      - | 7882 | ` *  $text` |
|      - | 7883 | ` *   The tested string.` |
|      - | 7884 | ` * Return` |
|      - | 7885 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 7886 | ` *  digit or blank, FALSE otherwise.` |
|      - | 7887 | ` */` |
|     18 | 7888 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7889 | `{` |
|      - | 7890 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7891 | `	int nLen;` |
|     19 | 7892 | `	if( nArg < 1 ){` |
|      - | 7893 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7894 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7895 | `		return PH7_OK;` |
|      - | 7896 | `	}` |
|      - | 7897 | `	/* Extract the target string */` |
|     19 | 7898 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7899 | `	zEnd = &zIn[nLen];` |
|     19 | 7900 | `	if( nLen < 1 ){` |
|      - | 7901 | `		/* Empty string,return FALSE */` |
|      3 | 7902 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7903 | `		return PH7_OK;` |
|      - | 7904 | `	}` |
|      - | 7905 | `	/* Perform the requested operation */` |
|     38 | 7906 | `	for(;;){` |
|     77 | 7907 | `		if( zIn >= zEnd ){` |
|      - | 7908 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7909 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7910 | `			return PH7_OK;` |
|      - | 7911 | `		}` |
|     69 | 7912 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7913 | `			/* UTF-8 stream  */` |
|    ! 0 | 7914 | `			break;` |
|      - | 7915 | `		}` |
|     69 | 7916 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 7917 | `			break;` |
|      - | 7918 | `		}` |
|      - | 7919 | `		/* Point to the next character */` |
|     61 | 7920 | `		zIn++;` |
|      1 | 7921 | `	}` |
|      - | 7922 | `	/* The test failed,return FALSE */` |
|      9 | 7923 | `	ph7_result_bool(pCtx,0);` |
|      9 | 7924 | `	return PH7_OK;` |
|     10 | 7925 | `}` |
|      - | 7926 | `/*` |
|      - | 7927 | ` * bool ctype_space(string $text)` |
|      - | 7928 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 7929 | ` * Parameters` |
|      - | 7930 | ` *  $text` |
|      - | 7931 | ` *   The tested string.` |
|      - | 7932 | ` * Return` |
|      - | 7933 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 7934 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 7935 | ` *  and form feed characters.` |
|      - | 7936 | ` */` |
|  62342 | 7937 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7938 | `{` |
|      - | 7939 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7940 | `	int nLen;` |
|  62347 | 7941 | `	if( nArg < 1 ){` |
|      - | 7942 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7943 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7944 | `		return PH7_OK;` |
|      - | 7945 | `	}` |
|      - | 7946 | `	/* Extract the target string */` |
|  62347 | 7947 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  62347 | 7948 | `	zEnd = &zIn[nLen];` |
|  62347 | 7949 | `	if( nLen < 1 ){` |
|      - | 7950 | `		/* Empty string,return FALSE */` |
|      3 | 7951 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7952 | `		return PH7_OK;` |
|      - | 7953 | `	}` |
|      - | 7954 | `	/* Perform the requested operation */` |
|  32280 | 7955 | `	for(;;){` |
|  64479 | 7956 | `		if( zIn >= zEnd ){` |
|      - | 7957 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   2115 | 7958 | `			ph7_result_bool(pCtx,1);` |
|   2115 | 7959 | `			return PH7_OK;` |
|      - | 7960 | `		}` |
|  62369 | 7961 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7962 | `			/* UTF-8 stream  */` |
|    ! 0 | 7963 | `			break;` |
|      - | 7964 | `		}` |
|  62369 | 7965 | `		if( !SyisSpace(zIn[0]) ){` |
|  60235 | 7966 | `			break;` |
|      - | 7967 | `		}` |
|      - | 7968 | `		/* Point to the next character */` |
|   2139 | 7969 | `		zIn++;` |
|      5 | 7970 | `	}` |
|      - | 7971 | `	/* The test failed,return FALSE */` |
|  60235 | 7972 | `	ph7_result_bool(pCtx,0);` |
|  60235 | 7973 | `	return PH7_OK;` |
|  31219 | 7974 | `}` |
|      - | 7975 | `/*` |
|      - | 7976 | ` * bool ctype_lower(string $text)` |
|      - | 7977 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 7978 | ` * Parameters` |
|      - | 7979 | ` *  $text` |
|      - | 7980 | ` *   The tested string.` |
|      - | 7981 | ` * Return` |
|      - | 7982 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 7983 | ` */` |
|     16 | 7984 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7985 | `{` |
|      - | 7986 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7987 | `	int nLen;` |
|     17 | 7988 | `	if( nArg < 1 ){` |
|      - | 7989 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7990 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7991 | `		return PH7_OK;` |
|      - | 7992 | `	}` |
|      - | 7993 | `	/* Extract the target string */` |
|     17 | 7994 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7995 | `	zEnd = &zIn[nLen];` |
|     17 | 7996 | `	if( nLen < 1 ){` |
|      - | 7997 | `		/* Empty string,return FALSE */` |
|      3 | 7998 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7999 | `		return PH7_OK;` |
|      - | 8000 | `	}` |
|      - | 8001 | `	/* Perform the requested operation */` |
|     27 | 8002 | `	for(;;){` |
|     55 | 8003 | `		if( zIn >= zEnd ){` |
|      - | 8004 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 8005 | `			ph7_result_bool(pCtx,1);` |
|      5 | 8006 | `			return PH7_OK;` |
|      - | 8007 | `		}` |
|     51 | 8008 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 8009 | `			break;` |
|      - | 8010 | `		}` |
|      - | 8011 | `		/* Point to the next character */` |
|     41 | 8012 | `		zIn++;` |
|      1 | 8013 | `	}` |
|      - | 8014 | `	/* The test failed,return FALSE */` |
|     11 | 8015 | `	ph7_result_bool(pCtx,0);` |
|     11 | 8016 | `	return PH7_OK;` |
|      9 | 8017 | `}` |
|      - | 8018 | `/*` |
|      - | 8019 | ` * bool ctype_upper(string $text)` |
|      - | 8020 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 8021 | ` * Parameters` |
|      - | 8022 | ` *  $text` |
|      - | 8023 | ` *   The tested string.` |
|      - | 8024 | ` * Return` |
|      - | 8025 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 8026 | ` */` |
|     16 | 8027 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8028 | `{` |
|      - | 8029 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8030 | `	int nLen;` |
|     17 | 8031 | `	if( nArg < 1 ){` |
|      - | 8032 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8033 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8034 | `		return PH7_OK;` |
|      - | 8035 | `	}` |
|      - | 8036 | `	/* Extract the target string */` |
|     17 | 8037 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8038 | `	zEnd = &zIn[nLen];` |
|     17 | 8039 | `	if( nLen < 1 ){` |
|      - | 8040 | `		/* Empty string,return FALSE */` |
|      3 | 8041 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8042 | `		return PH7_OK;` |
|      - | 8043 | `	}` |
|      - | 8044 | `	/* Perform the requested operation */` |
|     28 | 8045 | `	for(;;){` |
|     57 | 8046 | `		if( zIn >= zEnd ){` |
|      - | 8047 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 8048 | `			ph7_result_bool(pCtx,1);` |
|      5 | 8049 | `			return PH7_OK;` |
|      - | 8050 | `		}` |
|     53 | 8051 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 8052 | `			break;` |
|      - | 8053 | `		}` |
|      - | 8054 | `		/* Point to the next character */` |
|     43 | 8055 | `		zIn++;` |
|      1 | 8056 | `	}` |
|      - | 8057 | `	/* The test failed,return FALSE */` |
|     11 | 8058 | `	ph7_result_bool(pCtx,0);` |
|     11 | 8059 | `	return PH7_OK;` |
|      9 | 8060 | `}` |
|      - | 8061 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 8062 | `/*` |
|      - | 8063 | ` * Section:` |
|      - | 8064 | ` *    URL handling Functions.` |
|      - | 8065 | ` * Status:` |
|      - | 8066 | ` *    Stable.` |
|      - | 8067 | ` */` |
|      - | 8068 | `/*` |
|      - | 8069 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 8070 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 8071 | ` */` |
|   1026 | 8072 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 8073 | `{` |
|      - | 8074 | `	/* Store in the call context result buffer */` |
|   1028 | 8075 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 8076 | `	return SXRET_OK;` |
|      2 | 8077 | `}` |
|      - | 8078 | `/*` |
|      - | 8079 | ` * string base64_encode(string $data)` |
|      - | 8080 | ` * string convert_uuencode(string $data)` |
|      - | 8081 | ` *  Encodes data with MIME base64` |
|      - | 8082 | ` * Parameter` |
|      - | 8083 | ` *  $data` |
|      - | 8084 | ` *    Data to encode` |
|      - | 8085 | ` * Return` |
|      - | 8086 | ` *  Encoded data or FALSE on failure.` |
|      - | 8087 | ` */` |
|      6 | 8088 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8089 | `{` |
|      - | 8090 | `	const char *zIn;` |
|      - | 8091 | `	int nLen;` |
|      7 | 8092 | `	if( nArg < 1 ){` |
|      - | 8093 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8094 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8095 | `		return PH7_OK;` |
|      - | 8096 | `	}` |
|      - | 8097 | `	/* Extract the input string */` |
|      7 | 8098 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8099 | `	if( nLen < 1 ){` |
|      - | 8100 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8101 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8102 | `		return PH7_OK;` |
|      - | 8103 | `	}` |
|      - | 8104 | `	/* Perform the BASE64 encoding */` |
|      7 | 8105 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 8106 | `	return PH7_OK;` |
|      4 | 8107 | `}` |
|      - | 8108 | `/*` |
|      - | 8109 | ` * string base64_decode(string $data)` |
|      - | 8110 | ` * string convert_uudecode(string $data)` |
|      - | 8111 | ` *  Decodes data encoded with MIME base64` |
|      - | 8112 | ` * Parameter` |
|      - | 8113 | ` *  $data` |
|      - | 8114 | ` *    Encoded data.` |
|      - | 8115 | ` * Return` |
|      - | 8116 | ` *  Returns the original data or FALSE on failure.` |
|      - | 8117 | ` */` |
|     34 | 8118 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8119 | `{` |
|      - | 8120 | `	const char *zIn;` |
|      - | 8121 | `	int nLen;` |
|     36 | 8122 | `	if( nArg < 1 ){` |
|      - | 8123 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8124 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8125 | `		return PH7_OK;` |
|      - | 8126 | `	}` |
|      - | 8127 | `	/* Extract the input string */` |
|     36 | 8128 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 8129 | `	if( nLen < 1 ){` |
|      - | 8130 | `		/* Nothing to process,return FALSE */` |
|      3 | 8131 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8132 | `		return PH7_OK;` |
|      - | 8133 | `	}` |
|      - | 8134 | `	/* Perform the BASE64 decoding */` |
|     34 | 8135 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 8136 | `	return PH7_OK;` |
|     19 | 8137 | `}` |
|      - | 8138 | `/*` |
|      - | 8139 | ` * string urlencode(string $str)` |
|      - | 8140 | ` *  URL encoding` |
|      - | 8141 | ` * Parameter` |
|      - | 8142 | ` *  $data` |
|      - | 8143 | ` *   Input string.` |
|      - | 8144 | ` * Return` |
|      - | 8145 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 8146 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 8147 | ` *  encoded as plus (+) signs.` |
|      - | 8148 | ` */` |
|      4 | 8149 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8150 | `{` |
|      - | 8151 | `	const char *zIn;` |
|      - | 8152 | `	int nLen;` |
|      5 | 8153 | `	if( nArg < 1 ){` |
|      - | 8154 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8155 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8156 | `		return PH7_OK;` |
|      - | 8157 | `	}` |
|      - | 8158 | `	/* Extract the input string */` |
|      5 | 8159 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 8160 | `	if( nLen < 1 ){` |
|      - | 8161 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8162 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8163 | `		return PH7_OK;` |
|      - | 8164 | `	}` |
|      - | 8165 | `	/* Perform the URL encoding */` |
|      5 | 8166 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 8167 | `	return PH7_OK;` |
|      3 | 8168 | `}` |
|      - | 8169 | `/*` |
|      - | 8170 | ` * string urldecode(string $str)` |
|      - | 8171 | ` *  Decodes any %## encoding in the given string.` |
|      - | 8172 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 8173 | ` * Parameter` |
|      - | 8174 | ` *  $data` |
|      - | 8175 | ` *    Input string.` |
|      - | 8176 | ` * Return` |
|      - | 8177 | ` *  Decoded URL or FALSE on failure.` |
|      - | 8178 | ` */` |
|      6 | 8179 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8180 | `{` |
|      - | 8181 | `	const char *zIn;` |
|      - | 8182 | `	int nLen;` |
|      7 | 8183 | `	if( nArg < 1 ){` |
|      - | 8184 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8185 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8186 | `		return PH7_OK;` |
|      - | 8187 | `	}` |
|      - | 8188 | `	/* Extract the input string */` |
|      7 | 8189 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8190 | `	if( nLen < 1 ){` |
|      - | 8191 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8192 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8193 | `		return PH7_OK;` |
|      - | 8194 | `	}` |
|      - | 8195 | `	/* Perform the URL decoding */` |
|      7 | 8196 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 8197 | `	return PH7_OK;` |
|      4 | 8198 | `}` |
|      - | 8199 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8200 | `/* Table of the built-in functions */` |
|      - | 8201 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 8202 | `	   /* Variable handling functions */` |
|      - | 8203 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 8204 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 8205 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 8206 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 8207 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 8208 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 8209 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 8210 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 8211 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 8212 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 8213 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 8214 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 8215 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 8216 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 8217 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 8218 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 8219 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 8220 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 8221 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 8222 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 8223 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8224 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 8225 | `	   /* Math functions */` |
|      - | 8226 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 8227 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 8228 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 8229 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 8230 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 8231 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 8232 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 8233 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 8234 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 8235 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 8236 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 8237 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 8238 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 8239 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 8240 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 8241 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 8242 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 8243 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 8244 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 8245 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 8246 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 8247 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 8248 | `	{ "round",    PH7_builtin_round        },` |
|      - | 8249 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 8250 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 8251 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 8252 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 8253 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 8254 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 8255 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 8256 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 8257 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 8258 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8259 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8260 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 8261 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8262 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8263 | `	   /* String handling functions */` |
|      - | 8264 |  |
|      - | 8265 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 8266 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 8267 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 8268 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 8269 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 8270 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 8271 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 8272 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 8273 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 8274 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 8275 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 8276 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 8277 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 8278 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 8279 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 8280 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 8281 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 8282 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 8283 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 8284 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 8285 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 8286 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 8287 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 8288 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 8289 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 8290 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 8291 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 8292 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 8293 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 8294 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 8295 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 8296 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 8297 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 8298 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 8299 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 8300 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 8301 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 8302 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 8303 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 8304 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 8305 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 8306 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 8307 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 8308 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 8309 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 8310 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 8311 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 8312 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 8313 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 8314 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 8315 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 8316 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 8317 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8318 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8319 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 8320 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 8321 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 8322 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 8323 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8324 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8325 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 8326 |  |
|      - | 8327 |  |
|      - | 8328 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 8329 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 8330 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 8331 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 8332 | `	{ "hash",         PH7_builtin_hash      },` |
|      - | 8333 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|      - | 8334 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|      - | 8335 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|      - | 8336 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 8337 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|      - | 8338 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|      - | 8339 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|      - | 8340 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|      - | 8341 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|      - | 8342 | `	{ "filter_input",          PH7_builtin_filter_input },` |
|      - | 8343 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8344 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8345 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 8346 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 8347 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8348 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8349 |  |
|      - | 8350 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 8351 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 8352 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 8353 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 8354 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 8355 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 8356 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 8357 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 8358 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 8359 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 8360 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 8361 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 8362 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8363 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8364 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 8365 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8366 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8367 |  |
|      - | 8368 | `	         /* Ctype functions */` |
|      - | 8369 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 8370 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 8371 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 8372 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 8373 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 8374 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 8375 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 8376 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 8377 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 8378 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 8379 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 8380 | `	         /* Time functions */` |
|      - | 8381 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 8382 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 8383 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 8384 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 8385 | `	{ "date",        PH7_builtin_date         },` |
|      - | 8386 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 8387 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 8388 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 8389 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 8390 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 8391 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 8392 | `	        /* URL functions */` |
|      - | 8393 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 8394 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 8395 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 8396 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 8397 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 8398 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 8399 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 8400 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 8401 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8402 | `};` |
|      - | 8403 | `/*` |
|      - | 8404 | ` * Register the built-in functions defined above,the array functions` |
|      - | 8405 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 8406 | ` */` |
|   3476 | 8407 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      5 | 8408 | `{` |
|      - | 8409 | `	sxu32 n;` |
| 583973 | 8410 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 580497 | 8411 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 290251 | 8412 | `	}` |
|      - | 8413 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   3481 | 8414 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 8415 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   3481 | 8416 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   3481 | 8417 | `}` |
|      - | 8418 |  |
