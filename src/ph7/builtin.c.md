# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3836/4463 lines (85.95%)

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
|     34 |   30 | `static int PH7_builtin_is_bool(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   31 | `{` |
|     35 |   32 | `	int res = 0; /* Assume false by default */` |
|     35 |   33 | `	if( nArg > 0 ){` |
|     35 |   34 | `		res = ph7_value_is_bool(apArg[0]);` |
|     17 |   35 | `	}` |
|      - |   36 | `	/* Query result */` |
|     35 |   37 | `	ph7_result_bool(pCtx,res);` |
|     35 |   38 | `	return PH7_OK;` |
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
|    224 |   50 | `static int PH7_builtin_is_float(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   51 | `{` |
|    225 |   52 | `	int res = 0; /* Assume false by default */` |
|    225 |   53 | `	if( nArg > 0 ){` |
|    225 |   54 | `		res = ph7_value_is_float(apArg[0]);` |
|    112 |   55 | `	}` |
|      - |   56 | `	/* Query result */` |
|    225 |   57 | `	ph7_result_bool(pCtx,res);` |
|    225 |   58 | `	return PH7_OK;` |
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
|    656 |   70 | `static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |   71 | `{` |
|    659 |   72 | `	int res = 0; /* Assume false by default */` |
|    659 |   73 | `	if( nArg > 0 ){` |
|      - |   74 | `		/* Strict PHP identity: a float is never an int, even when it holds an` |
|      - |   75 | `		 * integer value (1.0). An integer-valued real carries both MEMOBJ_INT` |
|      - |   76 | `		 * (cached) and MEMOBJ_REAL, so REAL must be excluded here. */` |
|    659 |   77 | `		res = ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]);` |
|    328 |   78 | `	}` |
|      - |   79 | `	/* Query result */` |
|    659 |   80 | `	ph7_result_bool(pCtx,res);` |
|    659 |   81 | `	return PH7_OK;` |
|      3 |   82 | `}` |
|      - |   83 | `/*` |
|      - |   84 | ` * bool is_string($var)` |
|      - |   85 | ` *  Finds out whether a variable is a string.` |
|      - |   86 | ` * Parameters` |
|      - |   87 | ` *   $var: The variable being evaluated.` |
|      - |   88 | ` * Return` |
|      - |   89 | ` *  TRUE if var is string. False otherwise.` |
|      - |   90 | ` */` |
|    134 |   91 | `static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   92 | `{` |
|    135 |   93 | `	int res = 0; /* Assume false by default */` |
|    135 |   94 | `	if( nArg > 0 ){` |
|    135 |   95 | `		res = ph7_value_is_string(apArg[0]);` |
|     67 |   96 | `	}` |
|      - |   97 | `	/* Query result */` |
|    135 |   98 | `	ph7_result_bool(pCtx,res);` |
|    135 |   99 | `	return PH7_OK;` |
|      1 |  100 | `}` |
|      - |  101 | `/*` |
|      - |  102 | ` * bool is_null($var)` |
|      - |  103 | ` *  Finds out whether a variable is NULL.` |
|      - |  104 | ` * Parameters` |
|      - |  105 | ` *   $var: The variable being evaluated.` |
|      - |  106 | ` * Return` |
|      - |  107 | ` *  TRUE if var is NULL. False otherwise.` |
|      - |  108 | ` */` |
|     86 |  109 | `static int PH7_builtin_is_null(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |  110 | `{` |
|     89 |  111 | `	int res = 0; /* Assume false by default */` |
|     89 |  112 | `	if( nArg > 0 ){` |
|     89 |  113 | `		res = ph7_value_is_null(apArg[0]);` |
|     43 |  114 | `	}` |
|      - |  115 | `	/* Query result */` |
|     89 |  116 | `	ph7_result_bool(pCtx,res);` |
|     89 |  117 | `	return PH7_OK;` |
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
|    276 |  163 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  164 | `{` |
|    281 |  165 | `	int res = 0; /* Assume false by default */` |
|    281 |  166 | `	if( nArg > 0 ){` |
|    281 |  167 | `		res = ph7_value_is_array(apArg[0]);` |
|    138 |  168 | `	}` |
|      - |  169 | `	/* Query result */` |
|    281 |  170 | `	ph7_result_bool(pCtx,res);` |
|    281 |  171 | `	return PH7_OK;` |
|      5 |  172 | `}` |
|      - |  173 | `/*` |
|      - |  174 | ` * bool is_object($var)` |
|      - |  175 | ` *  Find out whether a variable is an object.` |
|      - |  176 | ` * Parameters` |
|      - |  177 | ` *  $var: The variable being evaluated.` |
|      - |  178 | ` * Return` |
|      - |  179 | ` *  True if var is an object. False otherwise.` |
|      - |  180 | ` */` |
|     38 |  181 | `static int PH7_builtin_is_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  182 | `{` |
|     39 |  183 | `	int res = 0; /* Assume false by default */` |
|     39 |  184 | `	if( nArg > 0 ){` |
|     39 |  185 | `		res = ph7_value_is_object(apArg[0]);` |
|     19 |  186 | `	}` |
|      - |  187 | `	/* Query result */` |
|     39 |  188 | `	ph7_result_bool(pCtx,res);` |
|     39 |  189 | `	return PH7_OK;` |
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
|  33420 |  303 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  304 | `{` |
|  33425 |  305 | `	int res = 1; /* Assume empty by default */` |
|  33425 |  306 | `	if( nArg > 0 ){` |
|  33423 |  307 | `		res = ph7_value_is_empty(apArg[0]);` |
|  16709 |  308 | `	}` |
|  33425 |  309 | `	ph7_result_bool(pCtx,res);` |
|  33425 |  310 | `	return PH7_OK;` |
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
| 215702 |  353 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  354 | `{` |
|      - |  355 | `	const char *zSource,*zOfft;` |
|      - |  356 | `	int nOfft,nLen,nSrcLen;` |
| 215707 |  357 | `	if( nArg < 2 ){` |
|      - |  358 | `		/* return FALSE */` |
|    ! 0 |  359 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  360 | `		return PH7_OK;` |
|      - |  361 | `	}` |
|      - |  362 | `	/* Extract the target string */` |
| 215707 |  363 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 215707 |  364 | `	if( nSrcLen < 1 ){` |
|      - |  365 | `		/* Empty string,return FALSE */` |
|  11739 |  366 | `		ph7_result_bool(pCtx,0);` |
|  11739 |  367 | `		return PH7_OK;` |
|      - |  368 | `	}` |
| 203973 |  369 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  370 | `	/* Extract the offset */` |
| 203973 |  371 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 203973 |  372 | `	if( nOfft < 0 ){` |
|  32211 |  373 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  32211 |  374 | `		if( zOfft < zSource ){` |
|      - |  375 | `			/* Invalid offset */` |
|      5 |  376 | `			ph7_result_bool(pCtx,0);` |
|      5 |  377 | `			return PH7_OK;` |
|      - |  378 | `		}` |
|  32207 |  379 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  32207 |  380 | `		nOfft = (int)(zOfft-zSource);` |
| 187868 |  381 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  382 | `		/* Invalid offset */` |
|    215 |  383 | `		ph7_result_bool(pCtx,0);` |
|    215 |  384 | `		return PH7_OK;` |
|    ! 0 |  385 | `	}else{` |
| 171557 |  386 | `		zOfft = &zSource[nOfft];` |
| 171557 |  387 | `		nLen = nSrcLen - nOfft;` |
|      - |  388 | `	}` |
| 203759 |  389 | `	if( nArg > 2 ){` |
|      - |  390 | `		/* Extract the length */` |
| 167493 |  391 | `		nLen = ph7_value_to_int(apArg[2]);` |
| 167493 |  392 | `		if( nLen == 0 ){` |
|      - |  393 | `			/* Invalid length,return an empty string */` |
|      5 |  394 | `			ph7_result_string(pCtx,"",0);` |
|      5 |  395 | `			return PH7_OK;` |
| 167489 |  396 | `		}else if( nLen < 0 ){` |
|  32199 |  397 | `			nLen = nSrcLen + nLen - nOfft;` |
|  32199 |  398 | `			if( nLen < 1 ){` |
|      - |  399 | `				/* Invalid  length */` |
|      3 |  400 | `				nLen = nSrcLen - nOfft;` |
|      1 |  401 | `			}` |
|  16097 |  402 | `		}` |
| 167489 |  403 | `		if( nLen + nOfft > nSrcLen ){` |
|      - |  404 | `			/* Invalid length */` |
|   5205 |  405 | `			nLen = nSrcLen - nOfft;` |
|   2600 |  406 | `		}` |
|  83742 |  407 | `	}` |
|      - |  408 | `	/* Return the substring */` |
| 203755 |  409 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 203755 |  410 | `	return PH7_OK;` |
| 107856 |  411 | `}` |
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
|  11146 | 1178 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1179 | `{` |
|  11151 | 1180 | `	int iLen = 0;` |
|  11151 | 1181 | `	if( nArg > 0 ){` |
|  11151 | 1182 | `		ph7_value_to_string(apArg[0],&iLen);` |
|   5573 | 1183 | `	}` |
|      - | 1184 | `	/* String length */` |
|  11151 | 1185 | `	ph7_result_int(pCtx,iLen);` |
|  11151 | 1186 | `	return PH7_OK;` |
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
| 135878 | 1332 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 1333 | `{` |
|  67939 | 1334 | `	SXUNUSED(pKey);` |
| 135883 | 1335 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 1336 | `	const char *zData;` |
|      - | 1337 | `	int nLen;` |
| 135883 | 1338 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
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
| 135881 | 1362 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 1363 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 135881 | 1364 | `	if( pData->bFirst ){` |
|  32579 | 1365 | `		pData->bFirst = 0;` |
| 119594 | 1366 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 1367 | `		/* append the separator first */` |
| 103295 | 1368 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 1369 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1370 | `			return PH7_ABORT;` |
|      - | 1371 | `		}` |
|  51645 | 1372 | `	}` |
|      - | 1373 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 135881 | 1374 | `	if( nLen > 0 ){` |
| 124147 | 1375 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1376 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1377 | `			return PH7_ABORT;` |
|      - | 1378 | `		}` |
|  62071 | 1379 | `	}` |
| 135881 | 1380 | `	return PH7_OK;` |
|  67944 | 1381 | `}` |
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
|  32596 | 1395 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1396 | `{` |
|      - | 1397 | `	struct implode_data imp_data;` |
|  32601 | 1398 | `	int i = 1;` |
|  32601 | 1399 | `	if( nArg < 1 ){` |
|      - | 1400 | `		/* Missing argument,return NULL */` |
|    ! 0 | 1401 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1402 | `		return PH7_OK;` |
|      - | 1403 | `	}` |
|      - | 1404 | `	/* Prepare the implode context */` |
|  32601 | 1405 | `	imp_data.pCtx = pCtx;` |
|  32601 | 1406 | `	imp_data.bRecursive = 0;` |
|  32601 | 1407 | `	imp_data.bFirst = 1;` |
|  32601 | 1408 | `	imp_data.nRecCount = 0;` |
|  32601 | 1409 | `	imp_data.rc = SXRET_OK;` |
|  32601 | 1410 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  32599 | 1411 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  16302 | 1412 | `	}else{` |
|      3 | 1413 | `		imp_data.zSep = 0;` |
|      3 | 1414 | `		imp_data.nSeplen = 0;` |
|      3 | 1415 | `		i = 0;` |
|      - | 1416 | `	}` |
|  32601 | 1417 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 1418 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1419 | `	}` |
|      - | 1420 | `	/* Start the 'join' process */` |
|  65197 | 1421 | `	while( i < nArg ){` |
|  32601 | 1422 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1423 | `			/* Iterate throw array entries */` |
|  32601 | 1424 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 1425 | `			/* Surface a callback allocation failure as a fatal */` |
|  32601 | 1426 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 1427 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1428 | `			}` |
|  16303 | 1429 | `		}else{` |
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
|  32601 | 1449 | `		i++;` |
|      5 | 1450 | `	}` |
|  32601 | 1451 | `	return PH7_OK;` |
|  16303 | 1452 | `}` |
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
|   6320 | 1552 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1553 | `{` |
|      - | 1554 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 1555 | `	int nDelim,nStrlen,iLimit;` |
|      - | 1556 | `	ph7_value *pArray;` |
|      - | 1557 | `	ph7_value *pValue;` |
|      - | 1558 | `	sxu32 nOfft;` |
|      - | 1559 | `	sxi32 rc;` |
|   6325 | 1560 | `	if( nArg < 2 ){` |
|      - | 1561 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 1562 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1563 | `		return PH7_OK;` |
|      - | 1564 | `	}` |
|      - | 1565 | `	/* Extract the delimiter */` |
|   6325 | 1566 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   6325 | 1567 | `	if( nDelim < 1 ){` |
|      - | 1568 | `		/* Empty delimiter: PHP 8 throws a catchable ValueError. */` |
|      3 | 1569 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1570 | `			"explode(): Argument #1 ($separator) must not be empty");` |
|      - | 1571 | `	}` |
|      - | 1572 | `	/* Extract the string */` |
|   6323 | 1573 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   6323 | 1574 | `	if( nStrlen < 1 ){` |
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
|   6317 | 1600 | `	zEnd = &zString[nStrlen];` |
|      - | 1601 | `	/* Create the array */` |
|   6317 | 1602 | `	pArray =  ph7_context_new_array(pCtx);` |
|   6317 | 1603 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   6317 | 1604 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 1605 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 1606 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1607 | `		return PH7_OK;` |
|      - | 1608 | `	}` |
|      - | 1609 | `	/* Set a defualt limit */` |
|   6317 | 1610 | `	iLimit = SXI32_HIGH;` |
|   6317 | 1611 | `	if( nArg > 2 ){` |
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
|  73542 | 1646 | `	for(;;){` |
| 147089 | 1647 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 147089 | 1648 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 1649 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   6301 | 1650 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   6301 | 1651 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 1652 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1653 | `			}` |
|   6301 | 1654 | `			break;` |
|      - | 1655 | `		}` |
|      - | 1656 | `		/* Point to the desired offset */` |
| 140793 | 1657 | `		zCur = &zString[nOfft];` |
|      - | 1658 | `		/* Perform the store operation (may be empty) */` |
| 140793 | 1659 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 140793 | 1660 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 1661 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 1662 | `		}` |
|      - | 1663 | `		/* Point beyond the delimiter */` |
| 140793 | 1664 | `		zString = &zCur[nDelim];` |
|      - | 1665 | `		/* Reset the cursor */` |
| 140793 | 1666 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 1667 | `	}` |
|      - | 1668 | `	/* Return the freshly created array */` |
|   6301 | 1669 | `	ph7_result_value(pCtx,pArray);` |
|      - | 1670 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 1671 | `	 * released as soon we return from this foregin function.` |
|      - | 1672 | `	 */` |
|   6301 | 1673 | `	return PH7_OK;` |
|   3165 | 1674 | `}` |
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
|  13940 | 1690 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1691 | `{` |
|      - | 1692 | `	const char *zString;` |
|      - | 1693 | `	int nLen;` |
|  13945 | 1694 | `	if( nArg < 1 ){` |
|      - | 1695 | `		/* Missing arguments,return null */` |
|    ! 0 | 1696 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1697 | `		return PH7_OK;` |
|      - | 1698 | `	}` |
|      - | 1699 | `	/* Extract the target string */` |
|  13945 | 1700 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  13945 | 1701 | `	if( nLen < 1 ){` |
|      - | 1702 | `		/* Empty string,return */` |
|   1311 | 1703 | `		ph7_result_string(pCtx,"",0);` |
|   1311 | 1704 | `		return PH7_OK;` |
|      - | 1705 | `	}` |
|      - | 1706 | `	/* Start the trim process */` |
|  12639 | 1707 | `	if( nArg < 2 ){` |
|      - | 1708 | `		SyString sStr;` |
|      - | 1709 | `		/* Remove white spaces and NUL bytes */` |
|  12609 | 1710 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  31493 | 1711 | `		SyStringFullTrimSafe(&sStr);` |
|  12609 | 1712 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   6307 | 1713 | `	}else{` |
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
|  12639 | 1742 | `	return PH7_OK;` |
|   6975 | 1743 | `}` |
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
|  32194 | 1883 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1884 | `{` |
|      - | 1885 | `	const char *zString,*zCur,*zEnd;` |
|      - | 1886 | `	int nLen;` |
|  32199 | 1887 | `	if( nArg < 1 ){` |
|      - | 1888 | `		/* Missing arguments,return null */` |
|    ! 0 | 1889 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1890 | `		return PH7_OK;` |
|      - | 1891 | `	}` |
|      - | 1892 | `	/* Extract the target string */` |
|  32199 | 1893 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  32199 | 1894 | `	if( nLen < 1 ){` |
|      - | 1895 | `		/* Empty string,return */` |
|      3 | 1896 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1897 | `		return PH7_OK;` |
|      - | 1898 | `	}` |
|      - | 1899 | `	/* Perform the requested operation */` |
|  32197 | 1900 | `	zEnd = &zString[nLen];` |
| 101303 | 1901 | `	for(;;){` |
| 202611 | 1902 | `		if( zString >= zEnd ){` |
|      - | 1903 | `			/* No more input,break immediately */` |
|  32197 | 1904 | `			break;` |
|      - | 1905 | `		}` |
| 170419 | 1906 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 1907 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 1908 | `			zCur = zString;` |
|    ! 0 | 1909 | `			zString++;` |
|    ! 0 | 1910 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 1911 | `				zString++;` |
|    ! 0 | 1912 | `			}` |
|      - | 1913 | `			/* Append UTF-8 stream */` |
|    ! 0 | 1914 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 1915 | `		}else{` |
| 170419 | 1916 | `			int c = zString[0];` |
| 170419 | 1917 | `			if( SyisUpper(c) ){` |
| 170417 | 1918 | `				c = SyToLower(zString[0]);` |
|  85206 | 1919 | `			}` |
|      - | 1920 | `			/* Append character */` |
| 170419 | 1921 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 1922 | `			/* Advance the cursor */` |
| 170419 | 1923 | `			zString++;` |
|      - | 1924 | `		}` |
|      5 | 1925 | `	}` |
|  32197 | 1926 | `	return PH7_OK;` |
|  16102 | 1927 | `}` |
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
|  20430 | 3062 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3063 | `{` |
|      - | 3064 | `	const char *zIn;` |
|      - | 3065 | `	int nLen;` |
|      - | 3066 | `	ph7_int64 nMul;` |
|      - | 3067 | `	int rc;` |
|  20432 | 3068 | `	if( nArg < 2 ){` |
|      - | 3069 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3070 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3071 | `		return PH7_OK;` |
|      - | 3072 | `	}` |
|      - | 3073 | `	/* Extract the target string */` |
|  20432 | 3074 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3075 | `	/* Extract the multiplier as a 64-bit value (a 32-bit read would wrap a large` |
|      - | 3076 | `	 * positive $times into a negative one and trip a spurious ValueError). PHP` |
|      - | 3077 | `	 * validates $times regardless of the string contents: a negative count throws` |
|      - | 3078 | `	 * a catchable ValueError. */` |
|  20432 | 3079 | `	nMul = ph7_value_to_int64(apArg[1]);` |
|  20432 | 3080 | `	if( nMul < 0 ){` |
|      3 | 3081 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 3082 | `			"str_repeat(): Argument #2 ($times) must be greater than or equal to 0");` |
|      - | 3083 | `	}` |
|  20430 | 3084 | `	if( nLen < 1 \|\| nMul < 1 ){` |
|      - | 3085 | `		/* Empty input or a zero multiplier yields the empty string (PHP). */` |
|    ! 0 | 3086 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3087 | `		return PH7_OK;` |
|      - | 3088 | `	}` |
|      - | 3089 | `	/* Perform the requested operation */` |
| 221628 | 3090 | `	for(;;){` |
| 443258 | 3091 | `		if( !nMul ){` |
|  20430 | 3092 | `			break;` |
|      - | 3093 | `		}` |
|      - | 3094 | `		/* Append the copy */` |
| 422830 | 3095 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 422830 | 3096 | `		if( rc != PH7_OK ){` |
|      - | 3097 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 3098 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 3099 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 3100 | `		}` |
| 422830 | 3101 | `		nMul--;` |
|      2 | 3102 | `	}` |
|  20430 | 3103 | `	return PH7_OK;` |
|  10217 | 3104 | `}` |
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
|      - | 3271 | `  /* php's 'h'/'H' are the locale-independent twins of 'g'/'G'; PHL always` |
|      - | 3272 | `   * formats in the C locale, so they behave identically. */` |
|      - | 3273 | `  {  'h',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 3274 | `  {  'H',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 3275 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 3276 | `};` |
|      - | 3277 | `/*` |
|      - | 3278 | ` * PHP 8 raises a catchable ValueError for an unknown conversion specifier` |
|      - | 3279 | ` * (e.g. "%y", or the C-ism "%#x" — '#' is not a php flag). Because printf()` |
|      - | 3280 | ` * and fprintf() stream their output incrementally while sprintf() buffers it,` |
|      - | 3281 | ` * every format builtin calls PH7_FormatValidate (below) to check the whole` |
|      - | 3282 | ` * format string BEFORE formatting so the throw happens with no partial output` |
|      - | 3283 | ` * escaping (php buffers the entire result and only emits it on success). This` |
|      - | 3284 | ` * scan mirrors the specifier-locating logic of the main format loop below.` |
|      - | 3285 | ` * On the first unknown specifier, stores it in *pBad and returns TRUE; returns` |
|      - | 3286 | ` * FALSE when every specifier is known. (A found-flag rather than a sentinel` |
|      - | 3287 | ` * char, so a NUL specifier byte — "%\0" — is still reported, not mistaken for` |
|      - | 3288 | ` * "all valid".)` |
|      - | 3289 | ` */` |
|    332 | 3290 | `static int FormatUnknownSpec(const char *zIn,int nByte,int *pBad)` |
|      1 | 3291 | `{` |
|    333 | 3292 | `	const char *zEnd = &zIn[nByte];` |
|      - | 3293 | `	int c,idx;` |
|   3161 | 3294 | `	while( zIn < zEnd ){` |
|   2849 | 3295 | `		if( zIn[0] != '%' ){` |
|   2201 | 3296 | `			zIn++;` |
|   2201 | 3297 | `			continue;` |
|      - | 3298 | `		}` |
|    649 | 3299 | `		zIn++; /* jump the percent sign */` |
|      - | 3300 | `		/* php-supported flags: '-', '+', ' ', '0' and the "'<pad>'" custom-pad` |
|      - | 3301 | `		 * form. '#' is intentionally NOT treated as a flag so it surfaces as an` |
|      - | 3302 | `		 * unknown specifier, matching php. */` |
|    691 | 3303 | `		while( zIn < zEnd ){` |
|    689 | 3304 | `			c = zIn[0];` |
|    689 | 3305 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|     43 | 3306 | `				zIn++;` |
|     43 | 3307 | `				continue;` |
|      - | 3308 | `			}` |
|    647 | 3309 | `			if( c=='\'' ){` |
|    ! 0 | 3310 | `				zIn++;` |
|    ! 0 | 3311 | `				if( zIn < zEnd ){` |
|    ! 0 | 3312 | `					zIn++; /* the custom pad character */` |
|    ! 0 | 3313 | `				}` |
|    ! 0 | 3314 | `				continue;` |
|      - | 3315 | `			}` |
|    647 | 3316 | `			break;` |
|    ! 0 | 3317 | `		}` |
|      - | 3318 | `		/* field width */` |
|    723 | 3319 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|     75 | 3320 | `			zIn++;` |
|      1 | 3321 | `		}` |
|      - | 3322 | `		/* positional specifier ($) — php parses flags AFTER it (e.g. "%1$-10s"),` |
|      - | 3323 | `		 * so skip the full flag set and width again, mirroring the main loop. */` |
|    649 | 3324 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
|    ! 0 | 3325 | `			zIn++;` |
|    ! 0 | 3326 | `			while( zIn < zEnd ){` |
|    ! 0 | 3327 | `				c = zIn[0];` |
|    ! 0 | 3328 | `				if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|    ! 0 | 3329 | `					zIn++;` |
|    ! 0 | 3330 | `					continue;` |
|      - | 3331 | `				}` |
|    ! 0 | 3332 | `				if( c=='\'' ){` |
|    ! 0 | 3333 | `					zIn++;` |
|    ! 0 | 3334 | `					if( zIn < zEnd ){` |
|    ! 0 | 3335 | `						zIn++;` |
|    ! 0 | 3336 | `					}` |
|    ! 0 | 3337 | `					continue;` |
|      - | 3338 | `				}` |
|    ! 0 | 3339 | `				break;` |
|    ! 0 | 3340 | `			}` |
|    ! 0 | 3341 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    ! 0 | 3342 | `				zIn++;` |
|    ! 0 | 3343 | `			}` |
|    ! 0 | 3344 | `		}` |
|      - | 3345 | `		/* precision */` |
|    649 | 3346 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|     87 | 3347 | `			zIn++;` |
|    183 | 3348 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|     97 | 3349 | `				zIn++;` |
|      1 | 3350 | `			}` |
|     43 | 3351 | `		}` |
|      - | 3352 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|    649 | 3353 | `		if( zIn < zEnd && zIn[0]=='l' ){` |
|     11 | 3354 | `			zIn++;` |
|      5 | 3355 | `		}` |
|    649 | 3356 | `		if( zIn >= zEnd ){` |
|      - | 3357 | `			/* A dangling '%' with no specifier: PHL's legacy path silently` |
|      - | 3358 | `			 * truncates here (recorded residual); nothing to validate. */` |
|      3 | 3359 | `			break;` |
|      - | 3360 | `		}` |
|    647 | 3361 | `		c = zIn[0];` |
|    647 | 3362 | `		zIn++; /* jump the conversion specifier */` |
|   3187 | 3363 | `		for( idx = 0 ; idx < (int)SX_ARRAYSIZE(aFmt) ; idx++ ){` |
|   3169 | 3364 | `			if( c == aFmt[idx].fmttype ){` |
|    629 | 3365 | `				break;` |
|      - | 3366 | `			}` |
|   1271 | 3367 | `		}` |
|    647 | 3368 | `		if( idx >= (int)SX_ARRAYSIZE(aFmt) ){` |
|     19 | 3369 | `			*pBad = c; /* unknown specifier */` |
|     19 | 3370 | `			return TRUE;` |
|      - | 3371 | `		}` |
|      1 | 3372 | `	}` |
|    315 | 3373 | `	return FALSE;` |
|    167 | 3374 | `}` |
|      - | 3375 | `/*` |
|      - | 3376 | ` * Validate a printf-style format string. PHP 8 raises a catchable ValueError for` |
|      - | 3377 | ` * an unknown conversion specifier, thrown before any output is produced. Every` |
|      - | 3378 | ` * format builtin (sprintf/printf/vprintf/vsprintf/fprintf/vfprintf) calls this` |
|      - | 3379 | ` * up-front, then propagates the returned status verbatim (PH7_EXCEPTION when the` |
|      - | 3380 | ` * throw is caught in place, PH7_ABORT when it goes uncaught).` |
|      - | 3381 | ` * Returns PH7_OK when the format is valid.` |
|      - | 3382 | ` */` |
|    332 | 3383 | `PH7_PRIVATE sxi32 PH7_FormatValidate(ph7_context *pCtx,const char *zFormat,int nByte)` |
|      1 | 3384 | `{` |
|    333 | 3385 | `	int badSpec = 0;` |
|    333 | 3386 | `	if( FormatUnknownSpec(zFormat,nByte,&badSpec) ){` |
|     28 | 3387 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      9 | 3388 | `			"Unknown format specifier \"%c\"",badSpec);` |
|      - | 3389 | `	}` |
|    315 | 3390 | `	return PH7_OK;` |
|    167 | 3391 | `}` |
|      - | 3392 | `/*` |
|      - | 3393 | `` * PHP 8: a printf-family `$format` argument is a `string` parameter — scalars`` |
|      - | 3394 | ` * (int/float/bool) and null coerce to a string, but an array/object/resource` |
|      - | 3395 | ` * raises a catchable TypeError. iArg is the 1-based argument position ($format` |
|      - | 3396 | ` * is #1 for sprintf/printf/vprintf/vsprintf, #2 for fprintf/vfprintf). Returns` |
|      - | 3397 | ` * PH7_OK when the value is string-coercible (the caller then uses` |
|      - | 3398 | ` * ph7_value_to_string, which renders scalars/null verbatim).` |
|      - | 3399 | ` */` |
|    352 | 3400 | `PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg)` |
|      1 | 3401 | `{` |
|    353 | 3402 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg) \|\| ph7_value_is_resource(pArg) ){` |
|      - | 3403 | `		char zBuf[64];` |
|     13 | 3404 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 3405 | `			"%s(): Argument #%d ($format) must be of type string, %s given",` |
|      4 | 3406 | `			ph7_function_name(pCtx),iArg,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|      - | 3407 | `	}` |
|    345 | 3408 | `	return PH7_OK;` |
|    177 | 3409 | `}` |
|      - | 3410 | `/*` |
|      - | 3411 | ` * Format a given string.` |
|      - | 3412 | ` * The root program.  All variations call this core.` |
|      - | 3413 | ` * INPUTS:` |
|      - | 3414 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 3415 | ` *            1. A pointer to the call context.` |
|      - | 3416 | ` *            2. A pointer to the list of characters to be output` |
|      - | 3417 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 3418 | ` *            3. An integer number of characters to be output.` |
|      - | 3419 | ` *               (Note: This number might be zero.)` |
|      - | 3420 | ` *            4. Upper layer private data.` |
|      - | 3421 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 3422 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 3423 | ` */` |
|    314 | 3424 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 3425 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 3426 | `	ph7_context *pCtx,  /* call context */` |
|      - | 3427 | `	const char *zIn,    /* Format string */` |
|      - | 3428 | `	int nByte,          /* Format string length */` |
|      - | 3429 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 3430 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 3431 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 3432 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 3433 | `	)` |
|      1 | 3434 | `{` |
|    315 | 3435 | `	char spaces[] = "                                                  ";` |
|      - | 3436 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    315 | 3437 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 3438 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 3439 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 3440 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 3441 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 3442 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 3443 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 3444 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 3445 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 3446 | `	ph7_int64 iVal;` |
|      - | 3447 | `	int precision;           /* Precision of the current field */` |
|      - | 3448 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 3449 | `	int c,rc,n;` |
|      - | 3450 | `	int length;              /* Length of the field */` |
|      - | 3451 | `	int prefix;` |
|      - | 3452 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 3453 | `	int width;               /* Width of the current field */` |
|      - | 3454 | `	int idx;` |
|    315 | 3455 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 3456 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 3457 | `	/* An unknown conversion specifier is rejected up-front by PH7_FormatValidate()` |
|      - | 3458 | `	 * (called by every format builtin before this routine), so the specifier set` |
|      - | 3459 | `	 * seen here is always valid. */` |
|      - | 3460 | `	/* Start the format process */` |
|    471 | 3461 | `	for(;;){` |
|    943 | 3462 | `		zCur = zIn;` |
|   3129 | 3463 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|   2187 | 3464 | `			zIn++;` |
|      1 | 3465 | `		}` |
|    943 | 3466 | `		if( zCur < zIn ){` |
|      - | 3467 | `			/* Consume chunk verbatim */` |
|    661 | 3468 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|    661 | 3469 | `			if( rc != SXRET_OK ){` |
|      - | 3470 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|    ! 0 | 3471 | `				break;` |
|      - | 3472 | `			}` |
|    330 | 3473 | `		}` |
|    943 | 3474 | `		if( zIn >= zEnd ){` |
|      - | 3475 | `			/* No more input to process,break immediately */` |
|    313 | 3476 | `			break;` |
|      - | 3477 | `		}` |
|      - | 3478 | `		/* Find out what flags are present */` |
|    631 | 3479 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    630 | 3480 | `			flag_alternateform = flag_zeropad = 0;` |
|    631 | 3481 | `		zIn++; /* Jump the precent sign */` |
|    315 | 3482 | `		do{` |
|    673 | 3483 | `			c = zIn[0];` |
|    673 | 3484 | `			switch( c ){` |
|     15 | 3485 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      7 | 3486 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 3487 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|     17 | 3488 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 3489 | `			case '\'':` |
|    ! 0 | 3490 | `				zIn++;` |
|    ! 0 | 3491 | `				if( zIn < zEnd ){` |
|      - | 3492 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 3493 | `					c = zIn[0];` |
|    ! 0 | 3494 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3495 | `						spaces[idx] = (char)c;` |
|    ! 0 | 3496 | `					}` |
|    ! 0 | 3497 | `					c = 0;` |
|    ! 0 | 3498 | `				}` |
|    ! 0 | 3499 | `				break;` |
|    630 | 3500 | `			default:                                       break;` |
|      - | 3501 | `			}` |
|    673 | 3502 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 3503 | `		/* Get the field width */` |
|    631 | 3504 | `		width = 0;` |
|   1020 | 3505 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     75 | 3506 | `			width = width*10 + (zIn[0] - '0');` |
|     75 | 3507 | `			zIn++;` |
|      1 | 3508 | `		}` |
|    631 | 3509 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 3510 | `			/* Position specifer */` |
|    ! 0 | 3511 | `			if( width > 0 ){` |
|    ! 0 | 3512 | `				n = width;` |
|    ! 0 | 3513 | `				if( vf && n > 0 ){` |
|    ! 0 | 3514 | `					n--;` |
|    ! 0 | 3515 | `				}` |
|    ! 0 | 3516 | `			}` |
|    ! 0 | 3517 | `			zIn++;` |
|    ! 0 | 3518 | `			width = 0;` |
|      - | 3519 | `			/* php's grammar is %argnum$<flags><width>: the flags come AFTER the` |
|      - | 3520 | `			 * positional, so re-parse the full flag set here (e.g. "%1$-10s"),` |
|      - | 3521 | `			 * not just zero-padding. */` |
|    ! 0 | 3522 | `			do{` |
|    ! 0 | 3523 | `				c = zIn[0];` |
|    ! 0 | 3524 | `				switch( c ){` |
|    ! 0 | 3525 | `				case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|    ! 0 | 3526 | `				case '+':   flag_plussign = 1;        c = 0;   break;` |
|    ! 0 | 3527 | `				case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    ! 0 | 3528 | `				case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 3529 | `				case '\'':` |
|    ! 0 | 3530 | `					zIn++;` |
|    ! 0 | 3531 | `					if( zIn < zEnd ){` |
|    ! 0 | 3532 | `						c = zIn[0];` |
|    ! 0 | 3533 | `						for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3534 | `							spaces[idx] = (char)c;` |
|    ! 0 | 3535 | `						}` |
|    ! 0 | 3536 | `						c = 0;` |
|    ! 0 | 3537 | `					}` |
|    ! 0 | 3538 | `					break;` |
|    ! 0 | 3539 | `				default:                                       break;` |
|      - | 3540 | `				}` |
|    ! 0 | 3541 | `			}while( c==0 && (zIn++ < zEnd) );` |
|    ! 0 | 3542 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 3543 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 3544 | `				zIn++;` |
|    ! 0 | 3545 | `			}` |
|    ! 0 | 3546 | `		}` |
|    631 | 3547 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 3548 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 3549 | `		}` |
|      - | 3550 | `		/* Get the precision */` |
|    631 | 3551 | `		precision = -1;` |
|    631 | 3552 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     87 | 3553 | `			precision = 0;` |
|     87 | 3554 | `			zIn++;` |
|    226 | 3555 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     97 | 3556 | `				precision = precision*10 + (zIn[0] - '0');` |
|     97 | 3557 | `				zIn++;` |
|      1 | 3558 | `			}` |
|     43 | 3559 | `		}` |
|      - | 3560 | `		/* Consume a single 'l' length modifier (a C-ism php accepts and ignores,` |
|      - | 3561 | `		 * e.g. "%ld"); PH7_FormatValidate mirrors this. Exactly one is skipped:` |
|      - | 3562 | `		 * in "%lld" the second 'l' becomes the (unknown) specifier, just like php. */` |
|    631 | 3563 | `		if( zIn < zEnd && zIn[0] == 'l' ){` |
|      9 | 3564 | `			zIn++;` |
|      4 | 3565 | `		}` |
|    631 | 3566 | `		if( zIn >= zEnd ){` |
|      - | 3567 | `			/* No more input */` |
|      3 | 3568 | `			break;` |
|      - | 3569 | `		}` |
|      - | 3570 | `		/* Fetch the info entry for the field */` |
|    629 | 3571 | `		pInfo = 0;` |
|    629 | 3572 | `		xtype = PH7_FMT_ERROR;` |
|    629 | 3573 | `		c = zIn[0];` |
|    629 | 3574 | `		zIn++; /* Jump the format specifer */` |
|   2863 | 3575 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|   2863 | 3576 | `			if( c==aFmt[idx].fmttype ){` |
|    629 | 3577 | `				pInfo = &aFmt[idx];` |
|    629 | 3578 | `				xtype = pInfo->type;` |
|    629 | 3579 | `				break;` |
|      - | 3580 | `			}` |
|   1118 | 3581 | `		}` |
|    629 | 3582 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    629 | 3583 | `		length = 0;` |
|      - | 3584 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 3585 | `		 /*` |
|      - | 3586 | `		  ** At this point, variables are initialized as follows:` |
|      - | 3587 | `		  **` |
|      - | 3588 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 3589 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 3590 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 3591 | `		  **                               field width was negative.` |
|      - | 3592 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 3593 | `		  **                               the conversion character.` |
|      - | 3594 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 3595 | `		  **   width                       The specified field width.  This is` |
|      - | 3596 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 3597 | `		  **   precision                   The specified precision.  The default` |
|      - | 3598 | `		  **                               is -1.` |
|      - | 3599 | `		  */` |
|    629 | 3600 | `		switch(xtype){` |
|      3 | 3601 | `		case PH7_FMT_PERCENT:` |
|      - | 3602 | `			/* A literal percent character */` |
|      7 | 3603 | `			zWorker[0] = '%';` |
|      7 | 3604 | `			length = (int)sizeof(char);` |
|      7 | 3605 | `			break;` |
|      3 | 3606 | `		case PH7_FMT_CHARX:` |
|      - | 3607 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 3608 | `			 * with that ASCII value` |
|      - | 3609 | `			 */` |
|      7 | 3610 | `			pArg = NEXT_ARG;` |
|      7 | 3611 | `			if( pArg == 0 ){` |
|      3 | 3612 | `				c = 0;` |
|      2 | 3613 | `			}else{` |
|      5 | 3614 | `				c = ph7_value_to_int(pArg);` |
|      - | 3615 | `			}` |
|      - | 3616 | `			/* NUL byte is an acceptable value */` |
|      7 | 3617 | `			zWorker[0] = (char)c;` |
|      7 | 3618 | `			length = (int)sizeof(char);` |
|      7 | 3619 | `			break;` |
|    161 | 3620 | `		case PH7_FMT_STRING:` |
|      - | 3621 | `			/* the argument is treated as and presented as a string */` |
|    323 | 3622 | `			pArg = NEXT_ARG;` |
|    323 | 3623 | `			if( pArg == 0 ){` |
|    ! 0 | 3624 | `				length = 0;` |
|    ! 0 | 3625 | `			}else{` |
|    323 | 3626 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 3627 | `			}` |
|    323 | 3628 | `			if( length < 1 ){` |
|    ! 0 | 3629 | `				zBuf = " ";` |
|    ! 0 | 3630 | `				length = (int)sizeof(char);` |
|    ! 0 | 3631 | `			}` |
|    323 | 3632 | `			if( precision>=0 && precision<length ){` |
|      3 | 3633 | `				length = precision;` |
|      1 | 3634 | `			}` |
|    323 | 3635 | `			if( flag_zeropad ){` |
|      - | 3636 | `				/* zero-padding works on strings too */` |
|    ! 0 | 3637 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3638 | `					spaces[idx] = '0';` |
|    ! 0 | 3639 | `				}` |
|    ! 0 | 3640 | `			}` |
|    323 | 3641 | `			break;` |
|     59 | 3642 | `		case PH7_FMT_RADIX:` |
|    119 | 3643 | `			pArg = NEXT_ARG;` |
|    119 | 3644 | `			if( pArg == 0 ){` |
|    ! 0 | 3645 | `				iVal = 0;` |
|    ! 0 | 3646 | `			}else{` |
|    119 | 3647 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 3648 | `			}` |
|      - | 3649 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|    119 | 3650 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 3651 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 3652 | `			}` |
|      - | 3653 | `#if 1` |
|      - | 3654 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 3655 | `        ** I think this is stupid.*/` |
|    119 | 3656 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 3657 | `#else` |
|      - | 3658 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 3659 | `        ** but leave the prefix for hex.*/` |
|      - | 3660 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 3661 | `#endif` |
|    119 | 3662 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|     95 | 3663 | `          if( iVal<0 ){` |
|     25 | 3664 | `            iVal = -iVal;` |
|      - | 3665 | `			/* Ticket 1433-003 */` |
|     25 | 3666 | `			if( iVal < 0 ){` |
|      - | 3667 | `				/* Overflow */` |
|    ! 0 | 3668 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3669 | `			}` |
|     25 | 3670 | `            prefix = '-';` |
|     83 | 3671 | `          }else if( flag_plussign )  prefix = '+';` |
|     69 | 3672 | `          else if( flag_blanksign )  prefix = ' ';` |
|     67 | 3673 | `          else                       prefix = 0;` |
|     48 | 3674 | `        }else{` |
|     25 | 3675 | `			if( iVal<0 ){` |
|    ! 0 | 3676 | `				iVal = -iVal;` |
|      - | 3677 | `				/* Ticket 1433-003 */` |
|    ! 0 | 3678 | `				if( iVal < 0 ){` |
|      - | 3679 | `					/* Overflow */` |
|    ! 0 | 3680 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3681 | `				}` |
|    ! 0 | 3682 | `			}` |
|     25 | 3683 | `			prefix = 0;` |
|      - | 3684 | `		}` |
|    119 | 3685 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      7 | 3686 | `          precision = width-(prefix!=0);` |
|      3 | 3687 | `        }` |
|    119 | 3688 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 3689 | `        {` |
|      - | 3690 | `          register char *cset;      /* Use registers for speed */` |
|      - | 3691 | `          register int base;` |
|    119 | 3692 | `          cset = pInfo->charset;` |
|    119 | 3693 | `          base = pInfo->base;` |
|     59 | 3694 | `          do{                                           /* Convert to ascii */` |
|    185 | 3695 | `            *(--zBuf) = cset[iVal%base];` |
|    185 | 3696 | `            iVal = iVal/base;` |
|    185 | 3697 | `          }while( iVal>0 );` |
|      - | 3698 | `        }` |
|    119 | 3699 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    141 | 3700 | `        for(idx=precision-length; idx>0; idx--){` |
|     23 | 3701 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|     12 | 3702 | `        }` |
|    119 | 3703 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|    119 | 3704 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 3705 | `          char *pre, x;` |
|    ! 0 | 3706 | `          pre = pInfo->prefix;` |
|    ! 0 | 3707 | `          if( *zBuf!=pre[0] ){` |
|    ! 0 | 3708 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|    ! 0 | 3709 | `          }` |
|    ! 0 | 3710 | `        }` |
|    119 | 3711 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    119 | 3712 | `		break;` |
|     88 | 3713 | `		case PH7_FMT_FLOAT:` |
|      - | 3714 | `		case PH7_FMT_EXP:` |
|      - | 3715 | `		case PH7_FMT_GENERIC:{` |
|      - | 3716 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3717 | `		double realvalue;` |
|      - | 3718 | `		char zFmt[8];` |
|      - | 3719 | `		int nOut, nFmt;` |
|    177 | 3720 | `		pArg = NEXT_ARG;` |
|    177 | 3721 | `		if( pArg == 0 ){` |
|    ! 0 | 3722 | `			realvalue = 0;` |
|    ! 0 | 3723 | `		}else{` |
|    177 | 3724 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 3725 | `		}` |
|      - | 3726 | `		/* php prints the IEEE specials bare — NaN / INF / -INF with no width` |
|      - | 3727 | `		 * padding, precision, or sign flags (php_sprintf_appenddouble). */` |
|    177 | 3728 | `		if( PH7_IS_NAN(realvalue) ){` |
|     21 | 3729 | `			zBuf = "NaN";` |
|     21 | 3730 | `			length = 3;` |
|     21 | 3731 | `			width = 0;` |
|     21 | 3732 | `			break;` |
|      - | 3733 | `		}` |
|    157 | 3734 | `		if( PH7_IS_INF(realvalue) ){` |
|     37 | 3735 | `			if( realvalue < 0.0 ){` |
|     15 | 3736 | `				zBuf = "-INF";` |
|     15 | 3737 | `				length = 4;` |
|      8 | 3738 | `			}else{` |
|     23 | 3739 | `				zBuf = "INF";` |
|     23 | 3740 | `				length = 3;` |
|      - | 3741 | `			}` |
|     37 | 3742 | `			width = 0;` |
|     37 | 3743 | `			break;` |
|      - | 3744 | `		}` |
|    121 | 3745 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|    121 | 3746 | `		if( precision > 53 ){` |
|      - | 3747 | `			/* php's FORMAT_CONV_MAX_PRECISION cap, with the same E_NOTICE` |
|      - | 3748 | `			 * (message prefixed with the active function's name, like` |
|      - | 3749 | `			 * php_error_docref). */` |
|      - | 3750 | `			char zMsg[160];` |
|      4 | 3751 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 3752 | `				"%z(): Requested precision of %d digits was truncated to PHP maximum of %d digits",` |
|      2 | 3753 | `				&pCtx->pFunc->sName,precision,53);` |
|      3 | 3754 | `			PH7_VmThrowError(pCtx->pVm,0,E_NOTICE,zMsg);` |
|      3 | 3755 | `			precision = 53;` |
|      1 | 3756 | `		}` |
|      - | 3757 | ``		/* php's %f/%e extract the sign via `num < 0`, so negative zero prints`` |
|      - | 3758 | `		 * unsigned there — while %g (php_gcvt on the raw value) keeps "-0". */` |
|    121 | 3759 | `		if( xtype!=PH7_FMT_GENERIC && realvalue == 0.0 ){` |
|      9 | 3760 | `			realvalue = 0.0;` |
|      4 | 3761 | `		}` |
|      - | 3762 | `		/* php's float conversions are correctly rounded (zend_dtoa); use libc` |
|      - | 3763 | `		 * snprintf as the digit engine (the byte-exact-floats rule — the old` |
|      - | 3764 | `		 * hand-rolled vxGetdigit loop stopped at 16 significant digits, so` |
|      - | 3765 | `		 * e.g. %f of 1e308 printed zeros where php prints the exact binary64` |
|      - | 3766 | `		 * expansion), then post-process into php's exact shapes below. */` |
|    121 | 3767 | `		nFmt = 0;` |
|    121 | 3768 | `		zFmt[nFmt++] = '%';` |
|    121 | 3769 | `		if( flag_alternateform ) zFmt[nFmt++] = '#';` |
|      - | 3770 | `		/* php's ' ' flag selects space PADDING (its default), not C's` |
|      - | 3771 | `		 * space-for-positive-sign — so flag_blanksign is NOT forwarded. */` |
|    121 | 3772 | `		if( flag_plussign ) zFmt[nFmt++] = '+';` |
|    121 | 3773 | `		zFmt[nFmt++] = '.';` |
|    121 | 3774 | `		zFmt[nFmt++] = '*';` |
|    165 | 3775 | `		zFmt[nFmt++] = (char)(xtype==PH7_FMT_FLOAT ? 'f' :` |
|     30 | 3776 | `			(xtype==PH7_FMT_EXP ? ((pInfo->charset[0]=='E') ? 'E' : 'e')` |
|     28 | 3777 | `			                    : ((pInfo->charset[0]=='E') ? 'G' : 'g')));` |
|    121 | 3778 | `		zFmt[nFmt] = 0;` |
|    121 | 3779 | `		nOut = snprintf(zWorker,sizeof(zWorker),zFmt,precision,realvalue);` |
|    121 | 3780 | `		if( nOut < 0 \|\| nOut >= (int)sizeof(zWorker) ){` |
|      - | 3781 | `			/* Cannot happen with precision capped at 53 (%f of DBL_MAX is` |
|      - | 3782 | `			 * ~365 bytes); keep the truncated output rather than overrun. */` |
|    ! 0 | 3783 | `			nOut = (int)SyStrlen(zWorker);` |
|    ! 0 | 3784 | `		}` |
|    121 | 3785 | `		nOut = (int)PH7_PhpFloatShape(zWorker,(sxi32)nOut,xtype==PH7_FMT_GENERIC);` |
|    121 | 3786 | `		zBuf = zWorker;` |
|    121 | 3787 | `		length = nOut;` |
|      - | 3788 | `		/* Let the zero-pad block below insert zeros between the sign (written` |
|      - | 3789 | `		 * by snprintf) and the first digit, as before. */` |
|    121 | 3790 | `		prefix = (zWorker[0]=='-' \|\| zWorker[0]=='+' \|\| zWorker[0]==' ') ? zWorker[0] : 0;` |
|      - | 3791 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 3792 | `        ** set and we are not left justified */` |
|    121 | 3793 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 3794 | `          int i;` |
|      7 | 3795 | `          int nPad = width - length;` |
|     51 | 3796 | `          for(i=width; i>=nPad; i--){` |
|     45 | 3797 | `            zBuf[i] = zBuf[i-nPad];` |
|     23 | 3798 | `          }` |
|      7 | 3799 | `          i = prefix!=0;` |
|     29 | 3800 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      7 | 3801 | `          length = width;` |
|      3 | 3802 | `        }` |
|      - | 3803 | `#else` |
|      - | 3804 | `         zBuf = " ";` |
|      - | 3805 | `		 length = (int)sizeof(char);` |
|      - | 3806 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|    121 | 3807 | `		 break;` |
|      - | 3808 | `							 }` |
|    ! 0 | 3809 | `		default:` |
|      - | 3810 | `			/* Unreachable: PH7_FormatValidate() rejects unknown specifiers with a` |
|      - | 3811 | `			 * catchable ValueError before formatting begins. Kept as a defensive` |
|      - | 3812 | `			 * no-op that emits nothing. */` |
|    ! 0 | 3813 | `			length = 0;` |
|    ! 0 | 3814 | `			break;` |
|      - | 3815 | `		}` |
|      - | 3816 | `		 /*` |
|      - | 3817 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 3818 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 3819 | `		 ** the output.` |
|      - | 3820 | `		 */` |
|    629 | 3821 | `    if( !flag_leftjustify ){` |
|      - | 3822 | `      register int nspace;` |
|    615 | 3823 | `      nspace = width-length;` |
|    615 | 3824 | `      if( nspace>0 ){` |
|      7 | 3825 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3826 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3827 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3828 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3829 | `			}` |
|    ! 0 | 3830 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3831 | `        }` |
|      7 | 3832 | `        if( nspace>0 ){` |
|      7 | 3833 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      7 | 3834 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3835 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3836 | `			}` |
|      3 | 3837 | `		}` |
|      3 | 3838 | `      }` |
|    307 | 3839 | `    }` |
|    629 | 3840 | `    if( length>0 ){` |
|    629 | 3841 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    629 | 3842 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3843 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3844 | `		}` |
|    314 | 3845 | `    }` |
|    629 | 3846 | `    if( flag_leftjustify ){` |
|      - | 3847 | `      register int nspace;` |
|     15 | 3848 | `      nspace = width-length;` |
|     15 | 3849 | `      if( nspace>0 ){` |
|     11 | 3850 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3851 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3852 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3853 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3854 | `			}` |
|    ! 0 | 3855 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3856 | `        }` |
|     11 | 3857 | `        if( nspace>0 ){` |
|     11 | 3858 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|     11 | 3859 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3860 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3861 | `			}` |
|      5 | 3862 | `		}` |
|      5 | 3863 | `      }` |
|      7 | 3864 | `    }` |
|      1 | 3865 | ` }/* for(;;) */` |
|    315 | 3866 | `	return SXRET_OK;` |
|    158 | 3867 | `}` |
|      - | 3868 | `/*` |
|      - | 3869 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 3870 | ` */` |
|    144 | 3871 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 3872 | `{` |
|      - | 3873 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|      - | 3874 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|      - | 3875 | `	 * non-OK rc also stops the format loop. */` |
|    145 | 3876 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|    145 | 3877 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|    145 | 3878 | `	return *pRc;` |
|      1 | 3879 | `}` |
|      - | 3880 | `/*` |
|      - | 3881 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 3882 | ` *  Return a formatted string.` |
|      - | 3883 | ` * Parameters` |
|      - | 3884 | ` *  $format` |
|      - | 3885 | ` *    The format string (see block comment above)` |
|      - | 3886 | ` * Return` |
|      - | 3887 | ` *  A string produced according to the formatting string format.` |
|      - | 3888 | ` */` |
|    108 | 3889 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3890 | `{` |
|      - | 3891 | `	const char *zFormat;` |
|    109 | 3892 | `	sxi32 rc = SXRET_OK;` |
|      - | 3893 | `	int nLen;` |
|    109 | 3894 | `	if( nArg < 1 ){` |
|      - | 3895 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 3896 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3897 | `		return PH7_OK;` |
|      - | 3898 | `	}` |
|      - | 3899 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|    109 | 3900 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    109 | 3901 | `	if( rc != PH7_OK ){` |
|      5 | 3902 | `		return rc;` |
|      - | 3903 | `	}` |
|      - | 3904 | `	/* Extract the string format (scalars/null coerce). */` |
|    105 | 3905 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    105 | 3906 | `	if( nLen < 1 ){` |
|      - | 3907 | `		/* Empty string */` |
|    ! 0 | 3908 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3909 | `		return PH7_OK;` |
|      - | 3910 | `	}` |
|      - | 3911 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 3912 | `	 * output; propagate the throw status verbatim. */` |
|    105 | 3913 | `	rc = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    105 | 3914 | `	if( rc != PH7_OK ){` |
|     17 | 3915 | `		return rc;` |
|      - | 3916 | `	}` |
|      - | 3917 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|     89 | 3918 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|     89 | 3919 | `	if( rc != SXRET_OK ){` |
|      - | 3920 | `		/* The result append ran out of memory: raise a fatal rather than` |
|      - | 3921 | `		 * returning a silently-truncated string. */` |
|    ! 0 | 3922 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 3923 | `	}` |
|     89 | 3924 | `	return PH7_OK;` |
|     55 | 3925 | `}` |
|      - | 3926 | `/*` |
|      - | 3927 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 3928 | ` */` |
|   1130 | 3929 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 3930 | `{` |
|   1131 | 3931 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 3932 | `	/* Call the VM output consumer directly */` |
|   1131 | 3933 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 3934 | `	/* Increment counter */` |
|   1131 | 3935 | `	*pCounter += nLen;` |
|   1131 | 3936 | `	return PH7_OK;` |
|      1 | 3937 | `}` |
|      - | 3938 | `/*` |
|      - | 3939 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 3940 | ` *  Output a formatted string.` |
|      - | 3941 | ` * Parameters` |
|      - | 3942 | ` *  $format` |
|      - | 3943 | ` *   See sprintf() for a description of format.` |
|      - | 3944 | ` * Return` |
|      - | 3945 | ` *  The length of the outputted string.` |
|      - | 3946 | ` */` |
|    200 | 3947 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3948 | `{` |
|    201 | 3949 | `	ph7_int64 nCounter = 0;` |
|      - | 3950 | `	const char *zFormat;` |
|      - | 3951 | `	int nLen;` |
|    201 | 3952 | `	if( nArg < 1 ){` |
|      - | 3953 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 3954 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3955 | `		return PH7_OK;` |
|      - | 3956 | `	}` |
|      - | 3957 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|      - | 3958 | `	{` |
|    201 | 3959 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    201 | 3960 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 3961 | `			return rcf;` |
|      - | 3962 | `		}` |
|      - | 3963 | `	}` |
|      - | 3964 | `	/* Extract the string format (scalars/null coerce). */` |
|    201 | 3965 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    201 | 3966 | `	if( nLen < 1 ){` |
|      - | 3967 | `		/* Empty string */` |
|    ! 0 | 3968 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3969 | `		return PH7_OK;` |
|      - | 3970 | `	}` |
|      - | 3971 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 3972 | `	 * output; propagate the throw status verbatim. */` |
|      - | 3973 | `	{` |
|    201 | 3974 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    201 | 3975 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 3976 | `			return rcv;` |
|      - | 3977 | `		}` |
|      - | 3978 | `	}` |
|      - | 3979 | `	/* Format the string */` |
|    201 | 3980 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 3981 | `	/* Return the length of the outputted string */` |
|    201 | 3982 | `	ph7_result_int64(pCtx,nCounter);` |
|    201 | 3983 | `	return PH7_OK;` |
|    101 | 3984 | `}` |
|      - | 3985 | `/*` |
|      - | 3986 | ` * int vprintf(string $format,array $args)` |
|      - | 3987 | ` *  Output a formatted string.` |
|      - | 3988 | ` * Parameters` |
|      - | 3989 | ` *  $format` |
|      - | 3990 | ` *   See sprintf() for a description of format.` |
|      - | 3991 | ` * Return` |
|      - | 3992 | ` *  The length of the outputted string.` |
|      - | 3993 | ` */` |
|      4 | 3994 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3995 | `{` |
|      5 | 3996 | `	ph7_int64 nCounter = 0;` |
|      - | 3997 | `	const char *zFormat;` |
|      - | 3998 | `	ph7_hashmap *pMap;` |
|      - | 3999 | `	SySet sArg;` |
|      - | 4000 | `	int nLen,n;` |
|      - | 4001 | `	sxi32 rcFmt;` |
|      5 | 4002 | `	if( nArg < 2 ){` |
|      - | 4003 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4004 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4005 | `		return PH7_OK;` |
|      - | 4006 | `	}` |
|      - | 4007 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|      5 | 4008 | `	rcFmt = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|      5 | 4009 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4010 | `		return rcFmt;` |
|      - | 4011 | `	}` |
|      5 | 4012 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 4013 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 4014 | `		char zBuf[64];` |
|      4 | 4015 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4016 | `			"vprintf(): Argument #2 ($values) must be of type array, %s given",` |
|      2 | 4017 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 4018 | `	}` |
|      - | 4019 | `	/* Extract the string format (scalars/null coerce). */` |
|      3 | 4020 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4021 | `	if( nLen < 1 ){` |
|      - | 4022 | `		/* Empty string */` |
|    ! 0 | 4023 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4024 | `		return PH7_OK;` |
|      - | 4025 | `	}` |
|      - | 4026 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4027 | `	 * output; propagate the throw status verbatim. */` |
|      3 | 4028 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      3 | 4029 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4030 | `		return rcFmt;` |
|      - | 4031 | `	}` |
|      - | 4032 | `	/* Point to the hashmap */` |
|      3 | 4033 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4034 | `	/* Extract arguments from the hashmap */` |
|      3 | 4035 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4036 | `	/* Format the string */` |
|      3 | 4037 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 4038 | `	/* Release the container */` |
|      3 | 4039 | `	SySetRelease(&sArg);` |
|      - | 4040 | `	/* Return the length of the outputted string */` |
|      3 | 4041 | `	ph7_result_int64(pCtx,nCounter);` |
|      3 | 4042 | `	return PH7_OK;` |
|      3 | 4043 | `}` |
|      - | 4044 | `/*` |
|      - | 4045 | ` * int vsprintf(string $format,array $args)` |
|      - | 4046 | ` *  Output a formatted string.` |
|      - | 4047 | ` * Parameters` |
|      - | 4048 | ` *  $format` |
|      - | 4049 | ` *   See sprintf() for a description of format.` |
|      - | 4050 | ` * Return` |
|      - | 4051 | ` *  A string produced according to the formatting string format.` |
|      - | 4052 | ` */` |
|     22 | 4053 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4054 | `{` |
|      - | 4055 | `	const char *zFormat;` |
|      - | 4056 | `	ph7_hashmap *pMap;` |
|      - | 4057 | `	SySet sArg;` |
|     23 | 4058 | `	sxi32 rc = SXRET_OK;` |
|      - | 4059 | `	sxi32 rcFmt;` |
|      - | 4060 | `	int nLen,n;` |
|     23 | 4061 | `	if( nArg < 2 ){` |
|      - | 4062 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4063 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4064 | `		return PH7_OK;` |
|      - | 4065 | `	}` |
|      - | 4066 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|     23 | 4067 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|     23 | 4068 | `	if( rc != PH7_OK ){` |
|      5 | 4069 | `		return rc;` |
|      - | 4070 | `	}` |
|     19 | 4071 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 4072 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 4073 | `		char zBuf[64];` |
|     16 | 4074 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4075 | `			"vsprintf(): Argument #2 ($values) must be of type array, %s given",` |
|     10 | 4076 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 4077 | `	}` |
|      - | 4078 | `	/* Extract the string format (scalars/null coerce). */` |
|      9 | 4079 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 4080 | `	if( nLen < 1 ){` |
|      - | 4081 | `		/* Empty string */` |
|    ! 0 | 4082 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4083 | `		return PH7_OK;` |
|      - | 4084 | `	}` |
|      - | 4085 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4086 | `	 * output; propagate the throw status verbatim. */` |
|      9 | 4087 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      9 | 4088 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4089 | `		return rcFmt;` |
|      - | 4090 | `	}` |
|      - | 4091 | `	/* Point to hashmap */` |
|      9 | 4092 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4093 | `	/* Extract arguments from the hashmap */` |
|      9 | 4094 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4095 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|      9 | 4096 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|      - | 4097 | `	/* Release the container */` |
|      9 | 4098 | `	SySetRelease(&sArg);` |
|      9 | 4099 | `	if( rc != SXRET_OK ){` |
|      - | 4100 | `		/* The result append ran out of memory: raise a fatal. */` |
|    ! 0 | 4101 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4102 | `	}` |
|      9 | 4103 | `	return PH7_OK;` |
|     12 | 4104 | `}` |
|      - | 4105 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4106 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4107 | `/*` |
|      - | 4108 | ` * Symisc eXtension.` |
|      - | 4109 | ` * string size_format(int64 $size)` |
|      - | 4110 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 4111 | ` *  Example:` |
|      - | 4112 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 4113 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 4114 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 4115 | ` * Parameter` |
|      - | 4116 | ` *  $size` |
|      - | 4117 | ` *    Entity size in bytes.` |
|      - | 4118 | ` * Return` |
|      - | 4119 | ` *   Formatted string representation of the given size.` |
|      - | 4120 | ` */` |
|     24 | 4121 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4122 | `{` |
|      - | 4123 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 4124 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 4125 | `	sxi32 nRest,i_32;` |
|      - | 4126 | `	ph7_int64 iSize;` |
|     25 | 4127 | `	int c = -1; /* index in zUnit[] */` |
|      - | 4128 |  |
|     25 | 4129 | `	if( nArg < 1 ){` |
|      - | 4130 | `		/* Missing argument,return the empty string */` |
|      3 | 4131 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4132 | `		return PH7_OK;` |
|      - | 4133 | `	}` |
|      - | 4134 | `	/* Extract the given size */` |
|     23 | 4135 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 4136 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 4137 | `		/* Don't bother formatting,return immediately */` |
|      5 | 4138 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 4139 | `		return PH7_OK;` |
|      - | 4140 | `	}` |
|     19 | 4141 | `	for(;;){` |
|     39 | 4142 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 4143 | `		iSize >>= 10;` |
|     39 | 4144 | `		c++;` |
|     39 | 4145 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 4146 | `			break;` |
|      - | 4147 | `		}` |
|      1 | 4148 | `	}` |
|     19 | 4149 | `	nRest /= 100;` |
|     19 | 4150 | `	if( nRest > 9 ){` |
|    ! 0 | 4151 | `		nRest = 9;` |
|    ! 0 | 4152 | `	}` |
|     19 | 4153 | `	if( iSize > 999 ){` |
|    ! 0 | 4154 | `		c++;` |
|    ! 0 | 4155 | `		nRest = 9;` |
|    ! 0 | 4156 | `		iSize = 0;` |
|    ! 0 | 4157 | `	}` |
|     19 | 4158 | `	i_32 = (sxi32)iSize;` |
|      - | 4159 | `	/* Format */` |
|     19 | 4160 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 4161 | `	return PH7_OK;` |
|     13 | 4162 | `}` |
|      - | 4163 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4164 | `/*` |
|      - | 4165 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 4166 | ` *   Calculate the md5 hash of a string.` |
|      - | 4167 | ` * Parameter` |
|      - | 4168 | ` *  $str` |
|      - | 4169 | ` *   Input string` |
|      - | 4170 | ` * $raw_output` |
|      - | 4171 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4172 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4173 | ` * Return` |
|      - | 4174 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 4175 | ` */` |
|     12 | 4176 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4177 | `{` |
|      - | 4178 | `	unsigned char zDigest[16];` |
|     13 | 4179 | `	int raw_output = FALSE;` |
|      - | 4180 | `	const void *pIn;` |
|      - | 4181 | `	int nLen;` |
|     13 | 4182 | `	if( nArg < 1 ){` |
|      - | 4183 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4184 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4185 | `		return PH7_OK;` |
|      - | 4186 | `	}` |
|      - | 4187 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 4188 | `	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */` |
|     13 | 4189 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 4190 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 4191 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 4192 | `	}` |
|      - | 4193 | `	/* Compute the MD5 digest */` |
|     13 | 4194 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|     13 | 4195 | `	if( raw_output ){` |
|      - | 4196 | `		/* Output raw digest */` |
|      5 | 4197 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 4198 | `	}else{` |
|      - | 4199 | `		/* Perform a binary to hex conversion */` |
|      9 | 4200 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4201 | `	}` |
|     13 | 4202 | `	return PH7_OK;` |
|      7 | 4203 | `}` |
|      - | 4204 | `/*` |
|      - | 4205 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 4206 | ` *   Calculate the sha1 hash of a string.` |
|      - | 4207 | ` * Parameter` |
|      - | 4208 | ` *  $str` |
|      - | 4209 | ` *   Input string` |
|      - | 4210 | ` * $raw_output` |
|      - | 4211 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4212 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4213 | ` * Return` |
|      - | 4214 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 4215 | ` */` |
|     10 | 4216 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4217 | `{` |
|      - | 4218 | `	unsigned char zDigest[20];` |
|     11 | 4219 | `	int raw_output = FALSE;` |
|      - | 4220 | `	const void *pIn;` |
|      - | 4221 | `	int nLen;` |
|     11 | 4222 | `	if( nArg < 1 ){` |
|      - | 4223 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4224 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4225 | `		return PH7_OK;` |
|      - | 4226 | `	}` |
|      - | 4227 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 4228 | `	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */` |
|     11 | 4229 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 4230 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 4231 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 4232 | `	}` |
|      - | 4233 | `	/* Compute the SHA1 digest */` |
|     11 | 4234 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|     11 | 4235 | `	if( raw_output ){` |
|      - | 4236 | `		/* Output raw digest */` |
|      5 | 4237 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 4238 | `	}else{` |
|      - | 4239 | `		/* Perform a binary to hex conversion */` |
|      7 | 4240 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4241 | `	}` |
|     11 | 4242 | `	return PH7_OK;` |
|      6 | 4243 | `}` |
|      - | 4244 | `/*` |
|      - | 4245 | ` * int64 crc32(string $str)` |
|      - | 4246 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 4247 | ` * Parameter` |
|      - | 4248 | ` *  $str` |
|      - | 4249 | ` *   Input string` |
|      - | 4250 | ` * Return` |
|      - | 4251 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 4252 | ` */` |
|      2 | 4253 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4254 | `{` |
|      - | 4255 | `	const void *pIn;` |
|      - | 4256 | `	sxu32 nCRC;` |
|      - | 4257 | `	int nLen;` |
|      3 | 4258 | `	if( nArg < 1 ){` |
|      - | 4259 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4260 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4261 | `		return PH7_OK;` |
|      - | 4262 | `	}` |
|      - | 4263 | `	/* Extract the input string */` |
|      3 | 4264 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4265 | `	if( nLen < 1 ){` |
|      - | 4266 | `		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike` |
|      - | 4267 | `		 * md5()/sha1(), whose empty-string digests are non-zero. */` |
|    ! 0 | 4268 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4269 | `		return PH7_OK;` |
|      - | 4270 | `	}` |
|      - | 4271 | `	/* Calculate the sum */` |
|      3 | 4272 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 4273 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 4274 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 4275 | `	return PH7_OK;` |
|      2 | 4276 | `}` |
|      - | 4277 | `/*` |
|      - | 4278 | ` * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is` |
|      - | 4279 | ` * described by a small record so one dispatch (and one generic HMAC) serves them` |
|      - | 4280 | ` * all. Thin adapters normalize the differing context types and the reversed` |
|      - | 4281 | ` * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.` |
|      - | 4282 | ` */` |
|     11 | 4283 | `static void HashMd5Init(HashCtx *c){ MD5Init(&c->md5); }` |
|     15 | 4284 | `static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }` |
|     11 | 4285 | `static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }` |
|     11 | 4286 | `static void HashSha1Init(HashCtx *c){ SHA1Init(&c->sha1); }` |
|     15 | 4287 | `static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }` |
|     11 | 4288 | `static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }` |
|      9 | 4289 | `static void HashSha224Init(HashCtx *c){ SHA224Init(&c->sha256); }` |
|     33 | 4290 | `static void HashSha256Init(HashCtx *c){ SHA256Init(&c->sha256); }` |
|     57 | 4291 | `static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }` |
|     41 | 4292 | `static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }` |
|      9 | 4293 | `static void HashSha384Init(HashCtx *c){ SHA384Init(&c->sha512); }` |
|     15 | 4294 | `static void HashSha512Init(HashCtx *c){ SHA512Init(&c->sha512); }` |
|     27 | 4295 | `static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }` |
|     23 | 4296 | `static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }` |
|      - | 4297 | `typedef struct HashAlgo HashAlgo;` |
|      - | 4298 | `struct HashAlgo {` |
|      - | 4299 | `	const char *zName;   /* lowercase canonical name */` |
|      - | 4300 | `	int nDigestLen;      /* output bytes: 16/20/28/32/48/64 */` |
|      - | 4301 | `	int nBlockLen;       /* internal block bytes (for HMAC): 64 or 128 */` |
|      - | 4302 | `	void (*xInit)(HashCtx *);` |
|      - | 4303 | `	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);` |
|      - | 4304 | `	void (*xFinal)(HashCtx *,unsigned char *);` |
|      - | 4305 | `};` |
|      - | 4306 | `static const HashAlgo aHashAlgo[] = {` |
|      - | 4307 | `	{ "md5",    16, 64,  HashMd5Init,    HashMd5Update,    HashMd5Final    },` |
|      - | 4308 | `	{ "sha1",   20, 64,  HashSha1Init,   HashSha1Update,   HashSha1Final   },` |
|      - | 4309 | `	{ "sha224", 28, 64,  HashSha224Init, HashSha256Update, HashSha256Final },` |
|      - | 4310 | `	{ "sha256", 32, 64,  HashSha256Init, HashSha256Update, HashSha256Final },` |
|      - | 4311 | `	{ "sha384", 48, 128, HashSha384Init, HashSha512Update, HashSha512Final },` |
|      - | 4312 | `	{ "sha512", 64, 128, HashSha512Init, HashSha512Update, HashSha512Final },` |
|      - | 4313 | `};` |
|      - | 4314 | `/* Case-insensitive algorithm lookup (PHP accepts 'SHA256' etc.). */` |
|     73 | 4315 | `static const HashAlgo * HashFindAlgo(const char *zName,int nLen){` |
|      - | 4316 | `	sxu32 i;` |
|    279 | 4317 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|    272 | 4318 | `		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen` |
|    211 | 4319 | `			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){` |
|     67 | 4320 | `			return &aHashAlgo[i];` |
|      - | 4321 | `		}` |
|    106 | 4322 | `	}` |
|      6 | 4323 | `	return 0;` |
|     38 | 4324 | `}` |
|      - | 4325 | `/*` |
|      - | 4326 | ` * string hash(string $algo,string $data[,bool $binary = false])` |
|      - | 4327 | ` *   Generate a hash value (message digest).` |
|      - | 4328 | ` */` |
|     54 | 4329 | `static int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4330 | `{` |
|      - | 4331 | `	const HashAlgo *pAlgo;` |
|      - | 4332 | `	const char *zAlgo,*zData;` |
|     56 | 4333 | `	int nAlgoLen,nDataLen,raw_output = FALSE;` |
|      - | 4334 | `	HashCtx sCtx;` |
|      - | 4335 | `	unsigned char zDigest[64];` |
|     56 | 4336 | `	if( nArg < 2 ){` |
|    ! 0 | 4337 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4338 | `			"hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4339 | `	}` |
|     56 | 4340 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     56 | 4341 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     56 | 4342 | `	if( pAlgo == 0 ){` |
|      3 | 4343 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4344 | `			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - | 4345 | `	}` |
|     53 | 4346 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     53 | 4347 | `	if( nArg > 2 ){` |
|      9 | 4348 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|      4 | 4349 | `	}` |
|     53 | 4350 | `	pAlgo->xInit(&sCtx);` |
|     53 | 4351 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     53 | 4352 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     53 | 4353 | `	if( raw_output ){` |
|      9 | 4354 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      5 | 4355 | `	}else{` |
|     45 | 4356 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 4357 | `	}` |
|     53 | 4358 | `	return PH7_OK;` |
|     29 | 4359 | `}` |
|      - | 4360 | `/*` |
|      - | 4361 | ` * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])` |
|      - | 4362 | ` *   Generate a keyed hash value using the HMAC method (RFC 2104).` |
|      - | 4363 | ` */` |
|     16 | 4364 | `static int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4365 | `{` |
|      - | 4366 | `	const HashAlgo *pAlgo;` |
|      - | 4367 | `	const char *zAlgo,*zData,*zKey;` |
|     18 | 4368 | `	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;` |
|      - | 4369 | `	HashCtx sCtx;` |
|      - | 4370 | `	unsigned char zKeyBlock[128],zIpad[128],zOpad[128],zInner[64],zDigest[64];` |
|      - | 4371 | `	int i,nBlock,nDigest;` |
|     18 | 4372 | `	if( nArg < 3 ){` |
|    ! 0 | 4373 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4374 | `			"hash_hmac() expects at least 3 arguments, %d given",nArg);` |
|      - | 4375 | `	}` |
|     18 | 4376 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     18 | 4377 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     18 | 4378 | `	if( pAlgo == 0 ){` |
|      3 | 4379 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4380 | `			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 4381 | `	}` |
|     15 | 4382 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     15 | 4383 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|     15 | 4384 | `	if( nArg > 3 ){` |
|      3 | 4385 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|      1 | 4386 | `	}` |
|     15 | 4387 | `	nBlock = pAlgo->nBlockLen;` |
|     15 | 4388 | `	nDigest = pAlgo->nDigestLen;` |
|      - | 4389 | `	/* Reduce the key to a single block: hash it if longer than the block, then` |
|      - | 4390 | `	 * zero-pad (a short or empty key is just zero-padded). */` |
|     15 | 4391 | `	SyZero(zKeyBlock,sizeof(zKeyBlock));` |
|     15 | 4392 | `	if( nKeyLen > nBlock ){` |
|      3 | 4393 | `		pAlgo->xInit(&sCtx);` |
|      3 | 4394 | `		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);` |
|      3 | 4395 | `		pAlgo->xFinal(&sCtx,zKeyBlock);` |
|     14 | 4396 | `	}else if( nKeyLen > 0 ){` |
|     11 | 4397 | `		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);` |
|      5 | 4398 | `	}` |
|   1039 | 4399 | `	for( i = 0; i < nBlock; i++ ){` |
|   1025 | 4400 | `		zIpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);` |
|   1025 | 4401 | `		zOpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);` |
|    513 | 4402 | `	}` |
|      - | 4403 | `	/* inner = H((key ^ ipad) \|\| data) */` |
|     15 | 4404 | `	pAlgo->xInit(&sCtx);` |
|     15 | 4405 | `	pAlgo->xUpdate(&sCtx,zIpad,(unsigned int)nBlock);` |
|     15 | 4406 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     15 | 4407 | `	pAlgo->xFinal(&sCtx,zInner);` |
|      - | 4408 | `	/* out = H((key ^ opad) \|\| inner) */` |
|     15 | 4409 | `	pAlgo->xInit(&sCtx);` |
|     15 | 4410 | `	pAlgo->xUpdate(&sCtx,zOpad,(unsigned int)nBlock);` |
|     15 | 4411 | `	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)nDigest);` |
|     15 | 4412 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     15 | 4413 | `	if( raw_output ){` |
|      3 | 4414 | `		ph7_result_string(pCtx,(const char *)zDigest,nDigest);` |
|      2 | 4415 | `	}else{` |
|     13 | 4416 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);` |
|      - | 4417 | `	}` |
|     15 | 4418 | `	return PH7_OK;` |
|     10 | 4419 | `}` |
|      - | 4420 | `/*` |
|      - | 4421 | ` * bool hash_equals(string $known_string,string $user_string)` |
|      - | 4422 | ` *   Timing-attack-safe string comparison.` |
|      - | 4423 | ` */` |
|     14 | 4424 | `static int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4425 | `{` |
|      - | 4426 | `	const char *zKnown,*zUser;` |
|      - | 4427 | `	int nKnown,nUser,i;` |
|     17 | 4428 | `	volatile unsigned char vDiff = 0;` |
|     17 | 4429 | `	if( nArg < 2 ){` |
|    ! 0 | 4430 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4431 | `			"hash_equals() expects exactly 2 arguments, %d given",nArg);` |
|      - | 4432 | `	}` |
|     17 | 4433 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      4 | 4434 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4435 | `			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",` |
|      1 | 4436 | `			ph7_type_name(apArg[0]));` |
|      - | 4437 | `	}` |
|     14 | 4438 | `	if( !ph7_value_is_string(apArg[1]) ){` |
|      4 | 4439 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4440 | `			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",` |
|      2 | 4441 | `			ph7_type_name(apArg[1]));` |
|      - | 4442 | `	}` |
|     11 | 4443 | `	zKnown = ph7_value_to_string(apArg[0],&nKnown);` |
|     11 | 4444 | `	zUser = ph7_value_to_string(apArg[1],&nUser);` |
|     11 | 4445 | `	if( nKnown != nUser ){` |
|      5 | 4446 | `		ph7_result_bool(pCtx,0);` |
|      5 | 4447 | `		return PH7_OK;` |
|      - | 4448 | `	}` |
|      - | 4449 | `	/* Constant-time: read every byte, never short-circuit. */` |
|     19 | 4450 | `	for( i = 0; i < nKnown; i++ ){` |
|     13 | 4451 | `		vDiff \|= (unsigned char)(zKnown[i] ^ zUser[i]);` |
|      7 | 4452 | `	}` |
|      7 | 4453 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|      7 | 4454 | `	return PH7_OK;` |
|     10 | 4455 | `}` |
|      - | 4456 | `/*` |
|      - | 4457 | ` * array hash_algos(void)` |
|      - | 4458 | ` *   Return a list of the registered hashing algorithms.` |
|      - | 4459 | ` */` |
|      2 | 4460 | `static int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4461 | `{` |
|      - | 4462 | `	ph7_value *pArray,*pValue;` |
|      - | 4463 | `	sxu32 i;` |
|      1 | 4464 | `	SXUNUSED(nArg);` |
|      1 | 4465 | `	SXUNUSED(apArg);` |
|      3 | 4466 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4467 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 4468 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 4469 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4470 | `		return PH7_OK;` |
|      - | 4471 | `	}` |
|     15 | 4472 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|     13 | 4473 | `		ph7_value_string(pValue,aHashAlgo[i].zName,-1);` |
|     13 | 4474 | `		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);` |
|     13 | 4475 | `		ph7_value_reset_string_cursor(pValue);` |
|      7 | 4476 | `	}` |
|      3 | 4477 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4478 | `	return PH7_OK;` |
|      2 | 4479 | `}` |
|      - | 4480 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 4481 | `/*` |
|      - | 4482 | ` * password_* (bcrypt). These live in ext/standard in real PHP — outside the` |
|      - | 4483 | ` * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.` |
|      - | 4484 | ` */` |
|      - | 4485 | `/*` |
|      - | 4486 | ` * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a` |
|      - | 4487 | ` * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).` |
|      - | 4488 | ` */` |
|     40 | 4489 | `static int BcryptParseHash(const char *zHash,int nHash,int *piCost)` |
|      1 | 4490 | `{` |
|      - | 4491 | `	int iCost;` |
|     40 | 4492 | `	if( nHash != 60 \|\| zHash[0] != '$' \|\| zHash[1] != '2' \|\| zHash[3] != '$'` |
|     29 | 4493 | `		\|\| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){` |
|     13 | 4494 | `		return FALSE;` |
|      - | 4495 | `	}` |
|     29 | 4496 | `	if( zHash[4] < '0' \|\| zHash[4] > '9' \|\| zHash[5] < '0' \|\| zHash[5] > '9' \|\| zHash[6] != '$' ){` |
|    ! 0 | 4497 | `		return FALSE;` |
|      - | 4498 | `	}` |
|     29 | 4499 | `	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');` |
|     29 | 4500 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      3 | 4501 | `		return FALSE;` |
|      - | 4502 | `	}` |
|     27 | 4503 | `	if( piCost ){ *piCost = iCost; }` |
|     27 | 4504 | `	return TRUE;` |
|     21 | 4505 | `}` |
|      - | 4506 | `/*` |
|      - | 4507 | ` * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the` |
|      - | 4508 | ` * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.` |
|      - | 4509 | ` */` |
|     20 | 4510 | `static int BcryptIsBcryptAlgo(ph7_value *pAlgo)` |
|      3 | 4511 | `{` |
|     23 | 4512 | `	if( ph7_value_is_null(pAlgo) ){` |
|    ! 0 | 4513 | `		return TRUE;` |
|      - | 4514 | `	}` |
|     23 | 4515 | `	if( ph7_value_is_string(pAlgo) ){` |
|      - | 4516 | `		int nAlgo;` |
|     23 | 4517 | `		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);` |
|     23 | 4518 | `		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );` |
|      - | 4519 | `	}` |
|    ! 0 | 4520 | `	return FALSE;` |
|     13 | 4521 | `}` |
|      - | 4522 | `/*` |
|      - | 4523 | ` * bool\|string password_hash(string $password,string\|int\|null $algo[,array $options])` |
|      - | 4524 | ` *  Create a bcrypt hash of the password.` |
|      - | 4525 | ` */` |
|     16 | 4526 | `static int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4527 | `{` |
|      - | 4528 | `	const char *zPwd;` |
|     19 | 4529 | `	int nPwd,iCost = 12;` |
|      - | 4530 | `	unsigned char aSalt[16];` |
|      - | 4531 | `	char zHash[60];` |
|     19 | 4532 | `	if( nArg < 2 ){` |
|    ! 0 | 4533 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4534 | `			"password_hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4535 | `	}` |
|     19 | 4536 | `	if( !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      3 | 4537 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4538 | `			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");` |
|      - | 4539 | `	}` |
|      - | 4540 | `	/* cost from $options['cost'] (default 12). */` |
|     16 | 4541 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|     14 | 4542 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     14 | 4543 | `		if( pCost ){ iCost = ph7_value_to_int(pCost); }` |
|      6 | 4544 | `	}` |
|     16 | 4545 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      4 | 4546 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 4547 | `			"Invalid bcrypt cost parameter specified: %d",iCost);` |
|      - | 4548 | `	}` |
|     13 | 4549 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     13 | 4550 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 4551 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 4552 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 4553 | `	}` |
|     13 | 4554 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){` |
|    ! 0 | 4555 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4556 | `		return PH7_OK;` |
|      - | 4557 | `	}` |
|     13 | 4558 | `	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));` |
|     13 | 4559 | `	return PH7_OK;` |
|     11 | 4560 | `}` |
|      - | 4561 | `/*` |
|      - | 4562 | ` * bool password_verify(string $password,string $hash)` |
|      - | 4563 | ` *  Verify a password against a bcrypt hash. Never throws on a malformed hash.` |
|      - | 4564 | ` */` |
|     28 | 4565 | `static int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4566 | `{` |
|      - | 4567 | `	const char *zPwd,*zHash;` |
|      - | 4568 | `	int nPwd,nHash,iCost,i;` |
|      - | 4569 | `	unsigned char aSalt[16];` |
|      - | 4570 | `	char zComputed[60];` |
|     29 | 4571 | `	volatile unsigned char vDiff = 0;` |
|     29 | 4572 | `	if( nArg < 2 ){` |
|    ! 0 | 4573 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4574 | `			"password_verify() expects exactly 2 arguments, %d given",nArg);` |
|      - | 4575 | `	}` |
|     29 | 4576 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     29 | 4577 | `	zHash = ph7_value_to_string(apArg[1],&nHash);` |
|     29 | 4578 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|     11 | 4579 | `		ph7_result_bool(pCtx,0);` |
|     11 | 4580 | `		return PH7_OK;` |
|      - | 4581 | `	}` |
|      - | 4582 | `	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */` |
|     19 | 4583 | `	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 4584 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4585 | `		return PH7_OK;` |
|      - | 4586 | `	}` |
|     19 | 4587 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){` |
|    ! 0 | 4588 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4589 | `		return PH7_OK;` |
|      - | 4590 | `	}` |
|      - | 4591 | `	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps` |
|      - | 4592 | `	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */` |
|    577 | 4593 | `	for( i = 29; i < 60; i++ ){` |
|    559 | 4594 | `		vDiff \|= (unsigned char)(zComputed[i] ^ zHash[i]);` |
|    280 | 4595 | `	}` |
|     19 | 4596 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|     19 | 4597 | `	return PH7_OK;` |
|     15 | 4598 | `}` |
|      - | 4599 | `/*` |
|      - | 4600 | ` * array password_get_info(string $hash)` |
|      - | 4601 | ` *  Return ["algo"=>id\|null, "algoName"=>name, "options"=>[...]].` |
|      - | 4602 | ` */` |
|      6 | 4603 | `static int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4604 | `{` |
|      7 | 4605 | `	const char *zHash = "";` |
|      7 | 4606 | `	int nHash,iCost = 0,bBcrypt = 0;` |
|      - | 4607 | `	ph7_value *pArray,*pOptions,*pVal;` |
|      7 | 4608 | `	if( nArg > 0 ){` |
|      7 | 4609 | `		zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 4610 | `		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);` |
|      3 | 4611 | `	}` |
|      7 | 4612 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 4613 | `	pOptions = ph7_context_new_array(pCtx);` |
|      7 | 4614 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      7 | 4615 | `	if( pArray == 0 \|\| pOptions == 0 \|\| pVal == 0 ){` |
|    ! 0 | 4616 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4617 | `		return PH7_OK;` |
|      - | 4618 | `	}` |
|      7 | 4619 | `	if( bBcrypt ){` |
|      5 | 4620 | `		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */` |
|      5 | 4621 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 4622 | `		ph7_value_reset_string_cursor(pVal);` |
|      5 | 4623 | `		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);` |
|      5 | 4624 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 4625 | `		ph7_value_int(pVal,iCost);` |
|      5 | 4626 | `		ph7_array_add_strkey_elem(pOptions,"cost",pVal);` |
|      3 | 4627 | `	}else{` |
|      3 | 4628 | `		ph7_value_null(pVal);                          /* algo => null */` |
|      3 | 4629 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      3 | 4630 | `		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);` |
|      3 | 4631 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      - | 4632 | `	}` |
|      7 | 4633 | `	ph7_array_add_strkey_elem(pArray,"options",pOptions);` |
|      7 | 4634 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 4635 | `	return PH7_OK;` |
|      4 | 4636 | `}` |
|      - | 4637 | `/*` |
|      - | 4638 | ` * bool password_needs_rehash(string $hash,string\|int\|null $algo[,array $options])` |
|      - | 4639 | ` *  True if the hash was not made with the given algo/options.` |
|      - | 4640 | ` */` |
|      6 | 4641 | `static int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4642 | `{` |
|      - | 4643 | `	const char *zHash;` |
|      7 | 4644 | `	int nHash,iCost = 0,iWantCost = 12;` |
|      7 | 4645 | `	if( nArg < 2 ){` |
|    ! 0 | 4646 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4647 | `			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4648 | `	}` |
|      7 | 4649 | `	zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 4650 | `	if( !BcryptParseHash(zHash,nHash,&iCost) \|\| !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      - | 4651 | `		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */` |
|      3 | 4652 | `		ph7_result_bool(pCtx,1);` |
|      3 | 4653 | `		return PH7_OK;` |
|      - | 4654 | `	}` |
|      5 | 4655 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      5 | 4656 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|      5 | 4657 | `		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }` |
|      2 | 4658 | `	}` |
|      5 | 4659 | `	ph7_result_bool(pCtx,iCost != iWantCost);` |
|      5 | 4660 | `	return PH7_OK;` |
|      4 | 4661 | `}` |
|      - | 4662 | `/*` |
|      - | 4663 | ` * filter_var() — input validation and sanitization (the ext/filter API).` |
|      - | 4664 | ` *` |
|      - | 4665 | ` * Filter and flag identifiers (values match PHP 8.5; the constants themselves` |
|      - | 4666 | ` * are registered in constant.c). The validate filters are hand-rolled rather` |
|      - | 4667 | ` * than delegating to SyStrToInt64/SyStrToReal: the former silently skips leading` |
|      - | 4668 | ` * zeros and cannot signal overflow, and the latter treats ',' as a decimal point` |
|      - | 4669 | ` * unconditionally — neither matches PHP's filter semantics.` |
|      - | 4670 | ` */` |
|      - | 4671 | `#define FV_VALIDATE_INT     257` |
|      - | 4672 | `#define FV_VALIDATE_BOOLEAN 258` |
|      - | 4673 | `#define FV_VALIDATE_FLOAT   259` |
|      - | 4674 | `#define FV_VALIDATE_REGEXP  272` |
|      - | 4675 | `#define FV_VALIDATE_URL     273` |
|      - | 4676 | `#define FV_VALIDATE_EMAIL   274` |
|      - | 4677 | `#define FV_VALIDATE_IP      275` |
|      - | 4678 | `#define FV_VALIDATE_MAC     276` |
|      - | 4679 | `#define FV_VALIDATE_DOMAIN  277` |
|      - | 4680 | `#define FV_SANITIZE_SPECIAL_CHARS      515` |
|      - | 4681 | `#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */` |
|      - | 4682 | `#define FV_SANITIZE_EMAIL   517` |
|      - | 4683 | `#define FV_SANITIZE_URL     518` |
|      - | 4684 | `#define FV_SANITIZE_NUMBER_INT   519` |
|      - | 4685 | `#define FV_SANITIZE_NUMBER_FLOAT 520` |
|      - | 4686 | `#define FV_SANITIZE_FULL_SPECIAL_CHARS 522` |
|      - | 4687 | `#define FV_FLAG_ALLOW_OCTAL  1` |
|      - | 4688 | `#define FV_FLAG_ALLOW_HEX    2` |
|      - | 4689 | `#define FV_FLAG_STRIP_LOW    4` |
|      - | 4690 | `#define FV_FLAG_STRIP_HIGH   8` |
|      - | 4691 | `#define FV_FLAG_ENCODE_LOW   16` |
|      - | 4692 | `#define FV_FLAG_ENCODE_HIGH  32` |
|      - | 4693 | `#define FV_FLAG_ENCODE_AMP   64` |
|      - | 4694 | `#define FV_FLAG_NO_ENCODE_QUOTES 128` |
|      - | 4695 | `#define FV_FLAG_STRIP_BACKTICK   512` |
|      - | 4696 | `#define FV_FLAG_ALLOW_FRACTION   4096` |
|      - | 4697 | `#define FV_FLAG_ALLOW_THOUSAND   8192` |
|      - | 4698 | `#define FV_FLAG_ALLOW_SCIENTIFIC 16384` |
|      - | 4699 | `#define FV_FLAG_IPV4  1048576` |
|      - | 4700 | `#define FV_FLAG_IPV6  2097152` |
|      - | 4701 | `#define FV_NULL_ON_FAILURE 134217728` |
|      - | 4702 | `/* The subset of flags the UNSAFE_RAW/DEFAULT string filter (FvSanitizeString)` |
|      - | 4703 | ` * acts on: when none are set the filter is a verbatim pass-through, so FV_DEFAULT` |
|      - | 4704 | ` * can shortcut. Keep this in sync with FvSanitizeString's flag handling. */` |
|      - | 4705 | `#define FV_FLAG_STRING_MASK (FV_FLAG_STRIP_LOW\|FV_FLAG_STRIP_HIGH\|FV_FLAG_STRIP_BACKTICK \` |
|      - | 4706 | `                            \|FV_FLAG_ENCODE_LOW\|FV_FLAG_ENCODE_HIGH\|FV_FLAG_ENCODE_AMP)` |
|      - | 4707 |  |
|      - | 4708 | `/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.` |
|      - | 4709 | ` * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */` |
|    153 | 4710 | `static void FvTrim(const char **pz,int *pn){` |
|    153 | 4711 | `	const char *z = *pz;` |
|    153 | 4712 | `	int n = *pn;` |
|    157 | 4713 | `	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }` |
|    161 | 4714 | `	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }` |
|    153 | 4715 | `	*pz = z; *pn = n;` |
|    153 | 4716 | `}` |
|      - | 4717 | `/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     57 | 4718 | `static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){` |
|     57 | 4719 | `	int neg = 0, i;` |
|     57 | 4720 | `	sxu64 u = 0;` |
|     57 | 4721 | `	FvTrim(&z,&n);` |
|     57 | 4722 | `	if( n==0 ){ return 0; }` |
|     51 | 4723 | `	if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }` |
|     51 | 4724 | `	if( n==0 ){ return 0; }` |
|     49 | 4725 | `	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'\|\|z[1]=='X') ){` |
|      3 | 4726 | `		z += 2; n -= 2;` |
|      3 | 4727 | `		if( n==0 ){ return 0; }` |
|      7 | 4728 | `		for( i=0; i<n; i++ ){` |
|      5 | 4729 | `			int h = SyHexToint((unsigned char)z[i]);` |
|      5 | 4730 | `			if( h<0 ){ return 0; }` |
|      5 | 4731 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }` |
|      5 | 4732 | `			u = u*16 + (sxu64)h;` |
|      3 | 4733 | `		}` |
|     48 | 4734 | `	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){` |
|      9 | 4735 | `		for( i=0; i<n; i++ ){` |
|      7 | 4736 | `			if( z[i]<'0' \|\| z[i]>'7' ){ return 0; }` |
|      7 | 4737 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }` |
|      7 | 4738 | `			u = u*8 + (sxu64)(z[i]-'0');` |
|      4 | 4739 | `		}` |
|      2 | 4740 | `	}else{` |
|     45 | 4741 | `		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */` |
|    201 | 4742 | `		for( i=0; i<n; i++ ){` |
|    173 | 4743 | `			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|    161 | 4744 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }` |
|    161 | 4745 | `			u = u*10 + (sxu64)(z[i]-'0');` |
|     81 | 4746 | `		}` |
|      - | 4747 | `	}` |
|     33 | 4748 | `	if( neg ){` |
|      5 | 4749 | `		if( u > 0x8000000000000000ULL ){ return 0; }` |
|      5 | 4750 | `		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */` |
|      3 | 4751 | `	}else{` |
|     29 | 4752 | `		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }` |
|     27 | 4753 | `		*pOut = (ph7_int64)u;` |
|      - | 4754 | `	}` |
|     31 | 4755 | `	return 1;` |
|     29 | 4756 | `}` |
|      - | 4757 | `/* FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     69 | 4758 | `static int FvValidateFloat(const char *z,int n,int flags,double *pOut){` |
|      - | 4759 | `	char zBuf[512];` |
|     69 | 4760 | `	int i, m = 0, seenDigit = 0;` |
|     69 | 4761 | `	const char *zv; int nv; double d = 0;` |
|     69 | 4762 | `	FvTrim(&z,&n);` |
|      - | 4763 | `	/* Bound the input: zBuf[512] holds the thousand-separator-stripped copy, and` |
|      - | 4764 | `	 * the cap also rejects the pathological 500+ digit floats PHP refuses. */` |
|     69 | 4765 | `	if( n==0 \|\| n>500 ){ return 0; }` |
|     69 | 4766 | `	if( flags & FV_FLAG_ALLOW_THOUSAND ){` |
|      - | 4767 | `		/* Commas are optional, but when present they must group the integer part` |
|      - | 4768 | `		 * into a leading run of 1..3 digits followed by groups of exactly 3` |
|      - | 4769 | `		 * ("1,000" ok, "1,5"/"1234,567" rejected). Strip them into zBuf and reject` |
|      - | 4770 | `		 * a comma anywhere in the fractional/exponent tail. */` |
|     25 | 4771 | `		int s = 0, intEnd, segStart, segIdx, hasComma = 0;` |
|     25 | 4772 | `		if( s<n && (z[s]=='+'\|\|z[s]=='-') ){ zBuf[m++] = z[s]; s++; }` |
|     25 | 4773 | `		intEnd = s;` |
|    167 | 4774 | `		while( intEnd<n && z[intEnd]!='.' && z[intEnd]!='e' && z[intEnd]!='E' ){` |
|    143 | 4775 | `			if( z[intEnd]==',' ){ hasComma = 1; }` |
|    143 | 4776 | `			intEnd++;` |
|      1 | 4777 | `		}` |
|     25 | 4778 | `		if( hasComma ){` |
|     25 | 4779 | `			segStart = s; segIdx = 0;` |
|    165 | 4780 | `			for( i=s; i<=intEnd; i++ ){` |
|    151 | 4781 | `				if( i==intEnd \|\| z[i]==',' ){` |
|     49 | 4782 | `					int segLen = i - segStart, k;` |
|     49 | 4783 | `					if( segIdx==0 ){ if( segLen<1 \|\| segLen>3 ){ return 0; } }` |
|     25 | 4784 | `					else if( segLen!=3 ){ return 0; }` |
|    119 | 4785 | `					for( k=segStart; k<i; k++ ){` |
|     81 | 4786 | `						if( !SyisDigit((unsigned char)z[k]) ){ return 0; }` |
|     81 | 4787 | `						zBuf[m++] = z[k];` |
|     41 | 4788 | `					}` |
|     39 | 4789 | `					segStart = i+1; segIdx++;` |
|     19 | 4790 | `				}` |
|     71 | 4791 | `			}` |
|      8 | 4792 | `		}else{` |
|    ! 0 | 4793 | `			for( i=s; i<intEnd; i++ ){ zBuf[m++] = z[i]; }` |
|      - | 4794 | `		}` |
|     27 | 4795 | `		for( i=intEnd; i<n; i++ ){` |
|     13 | 4796 | `			if( z[i]==',' ){ return 0; }` |
|     13 | 4797 | `			zBuf[m++] = z[i];` |
|      7 | 4798 | `		}` |
|     15 | 4799 | `		zv = zBuf; nv = m;` |
|      8 | 4800 | `	}else{` |
|     45 | 4801 | `		zv = z; nv = n;` |
|      - | 4802 | `	}` |
|     59 | 4803 | `	i = 0;` |
|     59 | 4804 | `	if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|    167 | 4805 | `	while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     59 | 4806 | `	if( i<nv && zv[i]=='.' ){` |
|     21 | 4807 | `		i++;` |
|     39 | 4808 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     10 | 4809 | `	}` |
|     59 | 4810 | `	if( !seenDigit ){ return 0; }` |
|     57 | 4811 | `	if( i<nv && (zv[i]=='e'\|\|zv[i]=='E') ){` |
|     29 | 4812 | `		i++;` |
|     29 | 4813 | `		if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|     29 | 4814 | `		if( i>=nv \|\| !SyisDigit((unsigned char)zv[i]) ){ return 0; }` |
|    105 | 4815 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; }` |
|     14 | 4816 | `	}` |
|     57 | 4817 | `	if( i!=nv ){ return 0; } /* trailing junk */` |
|      - | 4818 | `	/* The grammar above guarantees zv[0..nv) is a clean ASCII decimal float (no hex /` |
|      - | 4819 | `	 * inf / nan / trailing junk), so it is safe to hand to libc strtod, which — unlike` |
|      - | 4820 | `	 * SyStrToReal (15 sig-digits + exponent clamped to 308, so it silently saturates` |
|      - | 4821 | `	 * overflowing magnitudes to a finite value) — is overflow/underflow-aware and` |
|      - | 4822 | `	 * correctly rounded. strtod needs a NUL-terminated string: the ALLOW_THOUSAND path` |
|      - | 4823 | `	 * already built the span in zBuf (zv==zBuf); the plain path must copy it there (z is` |
|      - | 4824 | `	 * const + not NUL-terminated). nv <= n <= 500 < sizeof(zBuf) by the cap above.` |
|      - | 4825 | `	 * Matches PHP 8.5 byte-for-byte: reject overflow (-> +/-INF) and total underflow` |
|      - | 4826 | `	 * (-> 0.0), keep subnormals (nonzero, errno==ERANGE) and a genuine "0" (errno==0). */` |
|     53 | 4827 | `	if( zv != zBuf ){ SyMemcpy(zv,zBuf,(sxu32)nv); }` |
|     53 | 4828 | `	zBuf[nv] = 0;` |
|     53 | 4829 | `	errno = 0;` |
|     53 | 4830 | `	d = strtod(zBuf,0);` |
|     53 | 4831 | `	if( errno == ERANGE && (d == HUGE_VAL \|\| d == -HUGE_VAL \|\| d == 0.0) ){` |
|     15 | 4832 | `		return 0;` |
|      - | 4833 | `	}` |
|     39 | 4834 | `	*pOut = d;` |
|     39 | 4835 | `	return 1;` |
|     35 | 4836 | `}` |
|      - | 4837 | `/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),` |
|      - | 4838 | ` * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as` |
|      - | 4839 | ` * false, NOT failures. */` |
|     33 | 4840 | `static int FvValidateBool(const char *z,int n,int *pBool){` |
|     33 | 4841 | `	FvTrim(&z,&n);` |
|     32 | 4842 | `	if( (n==1 && z[0]=='1') \|\| (n==4 && SyStrnicmp(z,"true",4)==0)` |
|     25 | 4843 | `	    \|\| (n==2 && SyStrnicmp(z,"on",2)==0) \|\| (n==3 && SyStrnicmp(z,"yes",3)==0) ){` |
|     11 | 4844 | `		*pBool = 1; return 1;` |
|      - | 4845 | `	}` |
|     22 | 4846 | `	if( n==0 \|\| (n==1 && z[0]=='0') \|\| (n==5 && SyStrnicmp(z,"false",5)==0)` |
|     11 | 4847 | `	    \|\| (n==3 && SyStrnicmp(z,"off",3)==0) \|\| (n==2 && SyStrnicmp(z,"no",2)==0) ){` |
|     11 | 4848 | `		*pBool = 0; return 1;` |
|      - | 4849 | `	}` |
|      9 | 4850 | `	return 0;` |
|     15 | 4851 | `}` |
|      - | 4852 | `/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. */` |
|     33 | 4853 | `static int FvValidateIp4(const char *z,int n){` |
|     33 | 4854 | `	int i = 0, parts = 0;` |
|     77 | 4855 | `	while( i<n ){` |
|     65 | 4856 | `		int val = 0, digits = 0, start = i;` |
|    143 | 4857 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|     85 | 4858 | `			val = val*10 + (z[i]-'0');` |
|     85 | 4859 | `			if( val>255 ){ return 0; }` |
|     79 | 4860 | `			digits++; i++;` |
|      1 | 4861 | `		}` |
|     59 | 4862 | `		if( digits==0 \|\| digits>3 ){ return 0; }` |
|     49 | 4863 | `		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */` |
|     45 | 4864 | `		parts++;` |
|     45 | 4865 | `		if( parts>4 ){ return 0; }` |
|     45 | 4866 | `		if( i<n ){` |
|     33 | 4867 | `			if( z[i]!='.' ){ return 0; }` |
|     33 | 4868 | `			i++;` |
|     33 | 4869 | `			if( i>=n ){ return 0; } /* trailing dot */` |
|     16 | 4870 | `		}` |
|      1 | 4871 | `	}` |
|     13 | 4872 | `	return parts==4;` |
|     17 | 4873 | `}` |
|      - | 4874 | `/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),` |
|      - | 4875 | ` * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1. */` |
|     19 | 4876 | `static int FvIp6Hextets(const char *z,int n){` |
|     19 | 4877 | `	int i = 0, segStart = 0, groups = 0;` |
|     19 | 4878 | `	if( n==0 ){ return 0; }` |
|    145 | 4879 | `	while( i<=n ){` |
|    133 | 4880 | `		if( i==n \|\| z[i]==':' ){` |
|     23 | 4881 | `			int segLen = i - segStart, j, isV4 = 0;` |
|     23 | 4882 | `			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */` |
|     77 | 4883 | `			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }` |
|     23 | 4884 | `			if( isV4 ){` |
|     11 | 4885 | `				if( i!=n ){ return -1; } /* IPv4 only as the final token */` |
|     11 | 4886 | `				if( !FvValidateIp4(z+segStart,segLen) ){ return -1; }` |
|      5 | 4887 | `				groups += 2;` |
|      3 | 4888 | `			}else{` |
|     13 | 4889 | `				if( segLen>4 ){ return -1; }` |
|     47 | 4890 | `				for( j=segStart; j<i; j++ ){ if( SyHexToint((unsigned char)z[j])<0 ){ return -1; } }` |
|     13 | 4891 | `				groups++;` |
|      - | 4892 | `			}` |
|     17 | 4893 | `			segStart = i+1;` |
|      8 | 4894 | `		}` |
|    127 | 4895 | `		i++;` |
|      1 | 4896 | `	}` |
|     13 | 4897 | `	return groups;` |
|     10 | 4898 | `}` |
|      - | 4899 | `/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present. */` |
|     19 | 4900 | `static int FvValidateIp6(const char *z,int n){` |
|     19 | 4901 | `	const char *zDbl = 0;` |
|      - | 4902 | `	int i, ga, gb;` |
|    139 | 4903 | `	for( i=0; i+1<n; i++ ){` |
|    123 | 4904 | `		if( z[i]==':' && z[i+1]==':' ){` |
|     13 | 4905 | `			if( zDbl ){ return 0; } /* a second "::" is invalid */` |
|     11 | 4906 | `			zDbl = z+i;` |
|      5 | 4907 | `		}` |
|     61 | 4908 | `	}` |
|     17 | 4909 | `	if( zDbl==0 ){` |
|      9 | 4910 | `		return FvIp6Hextets(z,n)==8;` |
|    ! 0 | 4911 | `	}else{` |
|      9 | 4912 | `		int lenA = (int)(zDbl - z);` |
|      9 | 4913 | `		int lenB = n - lenA - 2;` |
|      9 | 4914 | `		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA);` |
|      9 | 4915 | `		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB);` |
|      9 | 4916 | `		if( ga<0 \|\| gb<0 ){ return 0; }` |
|      9 | 4917 | `		return (ga+gb)<=7; /* "::" stands for at least one zero group */` |
|      - | 4918 | `	}` |
|     10 | 4919 | `}` |
|     25 | 4920 | `static int FvValidateIp(const char *z,int n,int flags){` |
|     25 | 4921 | `	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);` |
|     25 | 4922 | `	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */` |
|     25 | 4923 | `	if( v4 && FvValidateIp4(z,n) ){ return 1; }` |
|     21 | 4924 | `	if( v6 && FvValidateIp6(z,n) ){ return 1; }` |
|     13 | 4925 | `	return 0;` |
|     13 | 4926 | `}` |
|      - | 4927 | `/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */` |
|     11 | 4928 | `static int FvValidateMac(const char *z,int n){` |
|      - | 4929 | `	char sep;` |
|      - | 4930 | `	int i;` |
|     11 | 4931 | `	if( n!=17 ){ return 0; }` |
|      7 | 4932 | `	sep = z[2];` |
|      7 | 4933 | `	if( sep!=':' && sep!='-' ){ return 0; }` |
|    105 | 4934 | `	for( i=0; i<17; i++ ){` |
|    101 | 4935 | `		if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }` |
|     71 | 4936 | `		else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|     50 | 4937 | `	}` |
|      5 | 4938 | `	return 1;` |
|      6 | 4939 | `}` |
|      - | 4940 | `/* FILTER_VALIDATE_EMAIL (best-effort: covers the common cases, not quoted local` |
|      - | 4941 | ` * parts or IP-literal domains). */` |
|     28 | 4942 | `static int FvValidateEmail(const char *z,int n){` |
|     28 | 4943 | `	int at = -1, i, localLen, domLen, labelStart, dotCount = 0;` |
|      - | 4944 | `	const char *zDom;` |
|     28 | 4945 | `	if( n==0 \|\| n>320 ){ return 0; }` |
|    201 | 4946 | `	for( i=0; i<n; i++ ){` |
|    181 | 4947 | `		if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }` |
|     91 | 4948 | `	}` |
|     21 | 4949 | `	if( at<=0 \|\| at==n-1 ){ return 0; } /* one '@', non-empty local and domain */` |
|     21 | 4950 | `	localLen = at;` |
|     21 | 4951 | `	zDom = z + at + 1;` |
|     21 | 4952 | `	domLen = n - at - 1;` |
|     21 | 4953 | `	if( z[0]=='.' \|\| z[at-1]=='.' ){ return 0; }` |
|     57 | 4954 | `	for( i=0; i<localLen; i++ ){` |
|     43 | 4955 | `		unsigned char c = (unsigned char)z[i];` |
|     43 | 4956 | `		if( c<=' ' ){ return 0; }` |
|     41 | 4957 | `		if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }` |
|     20 | 4958 | `	}` |
|     15 | 4959 | `	if( zDom[0]=='.' \|\| zDom[domLen-1]=='.' ){ return 0; }` |
|     13 | 4960 | `	labelStart = 0;` |
|     85 | 4961 | `	for( i=0; i<=domLen; i++ ){` |
|     75 | 4962 | `		if( i==domLen \|\| zDom[i]=='.' ){` |
|     25 | 4963 | `			int ll = i - labelStart;` |
|     25 | 4964 | `			if( ll==0 ){ return 0; } /* consecutive dots */` |
|     23 | 4965 | `			if( zDom[labelStart]=='-' \|\| zDom[i-1]=='-' ){ return 0; }` |
|     23 | 4966 | `			if( i<domLen ){ dotCount++; }` |
|     23 | 4967 | `			labelStart = i+1;` |
|     12 | 4968 | `		}else{` |
|     51 | 4969 | `			unsigned char c = (unsigned char)zDom[i];` |
|     51 | 4970 | `			if( !((c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9')\|\|c=='-') ){ return 0; }` |
|      - | 4971 | `		}` |
|     37 | 4972 | `	}` |
|     11 | 4973 | `	if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */` |
|      9 | 4974 | `	return 1;` |
|     15 | 4975 | `}` |
|      - | 4976 | `/* FILTER_VALIDATE_DOMAIN (lenient, matching PHP without FILTER_FLAG_HOSTNAME). */` |
|     11 | 4977 | `static int FvValidateDomain(const char *z,int n){` |
|      - | 4978 | `	int i;` |
|     11 | 4979 | `	if( n<1 \|\| n>253 \|\| z[0]=='.' ){ return 0; }` |
|     81 | 4980 | `	for( i=0; i<n; i++ ){` |
|     75 | 4981 | `		unsigned char c = (unsigned char)z[i];` |
|     75 | 4982 | `		if( c<=' ' ){ return 0; }` |
|     75 | 4983 | `		if( c=='.' && i+1<n && z[i+1]=='.' ){ return 0; }` |
|     37 | 4984 | `	}` |
|      7 | 4985 | `	return 1;` |
|      6 | 4986 | `}` |
|      - | 4987 | `/* FILTER_VALIDATE_URL: require a scheme and a host (PHP's filter is itself` |
|      - | 4988 | ` * parse_url-based, so PH7_VmHttpSplitURI tracks it closely). */` |
|     15 | 4989 | `static int FvValidateUrl(const char *z,int n){` |
|      - | 4990 | `	SyhttpUri sUri;` |
|     15 | 4991 | `	if( n==0 ){ return 0; }` |
|     15 | 4992 | `	SyZero(&sUri,(sxu32)sizeof(sUri));` |
|     15 | 4993 | `	if( PH7_VmHttpSplitURI(&sUri,z,(sxu32)n)!=SXRET_OK ){ return 0; }` |
|     15 | 4994 | `	return sUri.sScheme.nByte!=0 && sUri.sHost.nByte!=0;` |
|      8 | 4995 | `}` |
|      - | 4996 | `/* The Fv sanitizers build their result by appending directly to the call` |
|      - | 4997 | ` * context (ph7_result_string accumulates, like htmlspecialchars), emitting each` |
|      - | 4998 | ` * kept run in one call and seeding "" so an all-stripped input yields "". */` |
|      - | 4999 | `/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */` |
|     37 | 5000 | `static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){` |
|     37 | 5001 | `	int i, runStart = 0;` |
|     37 | 5002 | `	ph7_result_string(pCtx,"",0);` |
|     97 | 5003 | `	for( i=0; i<n; i++ ){` |
|     91 | 5004 | `		char c = z[i];` |
|     91 | 5005 | `		int keep = (c>='0'&&c<='9') \|\| c=='+' \|\| c=='-';` |
|     91 | 5006 | `		if( !keep && isFloat ){` |
|     38 | 5007 | `			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))` |
|     23 | 5008 | `			    \|\| (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))` |
|     36 | 5009 | `			    \|\| ((c=='e'\|\|c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));` |
|     12 | 5010 | `		}` |
|     61 | 5011 | `		if( !keep ){` |
|     33 | 5012 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     33 | 5013 | `			runStart = i+1;` |
|     16 | 5014 | `		}` |
|     31 | 5015 | `	}` |
|      7 | 5016 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 5017 | `}` |
|      - | 5018 | `/* Return non-zero when byte c must be stripped under the STRIP_* flags. Shared` |
|      - | 5019 | ` * by the UNSAFE_RAW string filter and SANITIZE_SPECIAL_CHARS. STRIP_LOW drops` |
|      - | 5020 | `` * bytes <32, STRIP_HIGH drops bytes >=127 (incl. DEL), STRIP_BACKTICK drops '`'.`` |
|      - | 5021 | ` * Matches php_filter_strip(); verified byte-exact vs php 8.5.7. */` |
|    287 | 5022 | `static int FvStripByte(unsigned char c,int flags){` |
|    287 | 5023 | `	if( (flags & FV_FLAG_STRIP_LOW)      && c<32 )    { return 1; }` |
|    281 | 5024 | `	if( (flags & FV_FLAG_STRIP_HIGH)     && c>=127 )  { return 1; }` |
|    269 | 5025 | `	if( (flags & FV_FLAG_STRIP_BACKTICK) && c==0x60 ) { return 1; }` |
|    267 | 5026 | `	return 0;` |
|    144 | 5027 | `}` |
|      - | 5028 | `/* FILTER_UNSAFE_RAW / FILTER_DEFAULT with flags: no default transform, but the` |
|      - | 5029 | ` * STRIP/ENCODE flags apply. Precedence (per php_filter_unsafe_raw, verified` |
|      - | 5030 | ` * vs php 8.5.7): a byte is first tested for stripping; a surviving byte is then` |
|      - | 5031 | ` * encoded as a decimal numeric entity if ENCODE_LOW (<32) / ENCODE_HIGH (>=127)` |
|      - | 5032 | ` * is set, and '&' becomes "&#38;" under ENCODE_AMP. So STRIP_LOW\|ENCODE_LOW` |
|      - | 5033 | ` * strips (nothing left to encode). Bytes are treated individually — ENCODE_HIGH` |
|      - | 5034 | ` * numeric-encodes each byte of a multibyte sequence separately, not the codepoint. */` |
|     25 | 5035 | `static void FvSanitizeString(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 5036 | `	int i, runStart = 0;` |
|     25 | 5037 | `	ph7_result_string(pCtx,"",0);` |
|    193 | 5038 | `	for( i=0; i<n; i++ ){` |
|    179 | 5039 | `		unsigned char c = (unsigned char)z[i];` |
|    179 | 5040 | `		if( FvStripByte(c,flags) ){` |
|     13 | 5041 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     13 | 5042 | `			runStart = i+1;` |
|     13 | 5043 | `			continue;` |
|      - | 5044 | `		}` |
|    167 | 5045 | `		if( c=='&' && (flags & FV_FLAG_ENCODE_AMP) ){` |
|      3 | 5046 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      3 | 5047 | `			ph7_result_string(pCtx,"&#38;",-1);` |
|      3 | 5048 | `			runStart = i+1;` |
|    166 | 5049 | `		}else if( (c<32 && (flags & FV_FLAG_ENCODE_LOW))` |
|    164 | 5050 | `		       \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     37 | 5051 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 5052 | `			ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|      9 | 5053 | `			runStart = i+1;` |
|      4 | 5054 | `		}` |
|     79 | 5055 | `	}` |
|     15 | 5056 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     15 | 5057 | `}` |
|      - | 5058 | `/* FILTER_SANITIZE_SPECIAL_CHARS: encode <>&"' and every control byte <32 as a` |
|      - | 5059 | ` * decimal numeric entity (&#60; &#38; &#34; ...). The STRIP_* flags remove bytes` |
|      - | 5060 | ` * before encoding; ENCODE_HIGH numeric-encodes surviving bytes >=127. Bytes >=128` |
|      - | 5061 | ` * are otherwise passed through verbatim (this filter is NOT UTF-8-aware — only the` |
|      - | 5062 | ` * FULL variant is). Byte-exact vs php 8.5.7. */` |
|     13 | 5063 | `static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int flags){` |
|     13 | 5064 | `	int i, runStart = 0;` |
|      - | 5065 | `	const char *zEnt;` |
|     13 | 5066 | `	ph7_result_string(pCtx,"",0);` |
|    131 | 5067 | `	for( i=0; i<n; i++ ){` |
|    119 | 5068 | `		unsigned char c = (unsigned char)z[i];` |
|    119 | 5069 | `		if( FvStripByte(c,flags) ){` |
|      9 | 5070 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 5071 | `			runStart = i+1;` |
|      9 | 5072 | `			continue;` |
|      - | 5073 | `		}` |
|    111 | 5074 | `		switch( c ){` |
|      3 | 5075 | `		case '<':  zEnt = "&#60;"; break;` |
|      3 | 5076 | `		case '>':  zEnt = "&#62;"; break;` |
|     11 | 5077 | `		case '&':  zEnt = "&#38;"; break;` |
|      3 | 5078 | `		case '"':  zEnt = "&#34;"; break;` |
|      3 | 5079 | `		case '\'': zEnt = "&#39;"; break;` |
|     46 | 5080 | `		default:` |
|      - | 5081 | `			/* Control bytes <32 are always numeric-encoded; bytes >=127 only when` |
|      - | 5082 | `			 * ENCODE_HIGH is set. Everything else stays in the current run. */` |
|     93 | 5083 | `			if( c<32 \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     17 | 5084 | `				if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     17 | 5085 | `				ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|     17 | 5086 | `				runStart = i+1;` |
|      8 | 5087 | `			}` |
|     93 | 5088 | `			continue; /* keep in the current run */` |
|      - | 5089 | `		}` |
|     19 | 5090 | `		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     19 | 5091 | `		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */` |
|     19 | 5092 | `		runStart = i+1;` |
|     10 | 5093 | `	}` |
|     13 | 5094 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     13 | 5095 | `}` |
|      - | 5096 | `/* HTML 4.01 named-entity table (codepoint -> "&name;") used by the UTF-8-aware` |
|      - | 5097 | ` * FULL_SPECIAL_CHARS filter, sorted ascending by codepoint for binary search.` |
|      - | 5098 | ` * Generated from php 8.5.7 (the exact set php_escape_html_entities emits for the` |
|      - | 5099 | ` * default document type); the five inline specials <>&"' are handled separately,` |
|      - | 5100 | ` * so every entry here is a codepoint >=0xA0. 248 rows. */` |
|      - | 5101 | `static const struct { sxu32 cp; const char *zEnt; } aHtml401Ent[] = {` |
|      - | 5102 | `	{0x00A0,"&nbsp;"},{0x00A1,"&iexcl;"},{0x00A2,"&cent;"},{0x00A3,"&pound;"},` |
|      - | 5103 | `	{0x00A4,"&curren;"},{0x00A5,"&yen;"},{0x00A6,"&brvbar;"},{0x00A7,"&sect;"},` |
|      - | 5104 | `	{0x00A8,"&uml;"},{0x00A9,"&copy;"},{0x00AA,"&ordf;"},{0x00AB,"&laquo;"},` |
|      - | 5105 | `	{0x00AC,"&not;"},{0x00AD,"&shy;"},{0x00AE,"&reg;"},{0x00AF,"&macr;"},` |
|      - | 5106 | `	{0x00B0,"&deg;"},{0x00B1,"&plusmn;"},{0x00B2,"&sup2;"},{0x00B3,"&sup3;"},` |
|      - | 5107 | `	{0x00B4,"&acute;"},{0x00B5,"&micro;"},{0x00B6,"&para;"},{0x00B7,"&middot;"},` |
|      - | 5108 | `	{0x00B8,"&cedil;"},{0x00B9,"&sup1;"},{0x00BA,"&ordm;"},{0x00BB,"&raquo;"},` |
|      - | 5109 | `	{0x00BC,"&frac14;"},{0x00BD,"&frac12;"},{0x00BE,"&frac34;"},{0x00BF,"&iquest;"},` |
|      - | 5110 | `	{0x00C0,"&Agrave;"},{0x00C1,"&Aacute;"},{0x00C2,"&Acirc;"},{0x00C3,"&Atilde;"},` |
|      - | 5111 | `	{0x00C4,"&Auml;"},{0x00C5,"&Aring;"},{0x00C6,"&AElig;"},{0x00C7,"&Ccedil;"},` |
|      - | 5112 | `	{0x00C8,"&Egrave;"},{0x00C9,"&Eacute;"},{0x00CA,"&Ecirc;"},{0x00CB,"&Euml;"},` |
|      - | 5113 | `	{0x00CC,"&Igrave;"},{0x00CD,"&Iacute;"},{0x00CE,"&Icirc;"},{0x00CF,"&Iuml;"},` |
|      - | 5114 | `	{0x00D0,"&ETH;"},{0x00D1,"&Ntilde;"},{0x00D2,"&Ograve;"},{0x00D3,"&Oacute;"},` |
|      - | 5115 | `	{0x00D4,"&Ocirc;"},{0x00D5,"&Otilde;"},{0x00D6,"&Ouml;"},{0x00D7,"&times;"},` |
|      - | 5116 | `	{0x00D8,"&Oslash;"},{0x00D9,"&Ugrave;"},{0x00DA,"&Uacute;"},{0x00DB,"&Ucirc;"},` |
|      - | 5117 | `	{0x00DC,"&Uuml;"},{0x00DD,"&Yacute;"},{0x00DE,"&THORN;"},{0x00DF,"&szlig;"},` |
|      - | 5118 | `	{0x00E0,"&agrave;"},{0x00E1,"&aacute;"},{0x00E2,"&acirc;"},{0x00E3,"&atilde;"},` |
|      - | 5119 | `	{0x00E4,"&auml;"},{0x00E5,"&aring;"},{0x00E6,"&aelig;"},{0x00E7,"&ccedil;"},` |
|      - | 5120 | `	{0x00E8,"&egrave;"},{0x00E9,"&eacute;"},{0x00EA,"&ecirc;"},{0x00EB,"&euml;"},` |
|      - | 5121 | `	{0x00EC,"&igrave;"},{0x00ED,"&iacute;"},{0x00EE,"&icirc;"},{0x00EF,"&iuml;"},` |
|      - | 5122 | `	{0x00F0,"&eth;"},{0x00F1,"&ntilde;"},{0x00F2,"&ograve;"},{0x00F3,"&oacute;"},` |
|      - | 5123 | `	{0x00F4,"&ocirc;"},{0x00F5,"&otilde;"},{0x00F6,"&ouml;"},{0x00F7,"&divide;"},` |
|      - | 5124 | `	{0x00F8,"&oslash;"},{0x00F9,"&ugrave;"},{0x00FA,"&uacute;"},{0x00FB,"&ucirc;"},` |
|      - | 5125 | `	{0x00FC,"&uuml;"},{0x00FD,"&yacute;"},{0x00FE,"&thorn;"},{0x00FF,"&yuml;"},` |
|      - | 5126 | `	{0x0152,"&OElig;"},{0x0153,"&oelig;"},{0x0160,"&Scaron;"},{0x0161,"&scaron;"},` |
|      - | 5127 | `	{0x0178,"&Yuml;"},{0x0192,"&fnof;"},{0x02C6,"&circ;"},{0x02DC,"&tilde;"},` |
|      - | 5128 | `	{0x0391,"&Alpha;"},{0x0392,"&Beta;"},{0x0393,"&Gamma;"},{0x0394,"&Delta;"},` |
|      - | 5129 | `	{0x0395,"&Epsilon;"},{0x0396,"&Zeta;"},{0x0397,"&Eta;"},{0x0398,"&Theta;"},` |
|      - | 5130 | `	{0x0399,"&Iota;"},{0x039A,"&Kappa;"},{0x039B,"&Lambda;"},{0x039C,"&Mu;"},` |
|      - | 5131 | `	{0x039D,"&Nu;"},{0x039E,"&Xi;"},{0x039F,"&Omicron;"},{0x03A0,"&Pi;"},` |
|      - | 5132 | `	{0x03A1,"&Rho;"},{0x03A3,"&Sigma;"},{0x03A4,"&Tau;"},{0x03A5,"&Upsilon;"},` |
|      - | 5133 | `	{0x03A6,"&Phi;"},{0x03A7,"&Chi;"},{0x03A8,"&Psi;"},{0x03A9,"&Omega;"},` |
|      - | 5134 | `	{0x03B1,"&alpha;"},{0x03B2,"&beta;"},{0x03B3,"&gamma;"},{0x03B4,"&delta;"},` |
|      - | 5135 | `	{0x03B5,"&epsilon;"},{0x03B6,"&zeta;"},{0x03B7,"&eta;"},{0x03B8,"&theta;"},` |
|      - | 5136 | `	{0x03B9,"&iota;"},{0x03BA,"&kappa;"},{0x03BB,"&lambda;"},{0x03BC,"&mu;"},` |
|      - | 5137 | `	{0x03BD,"&nu;"},{0x03BE,"&xi;"},{0x03BF,"&omicron;"},{0x03C0,"&pi;"},` |
|      - | 5138 | `	{0x03C1,"&rho;"},{0x03C2,"&sigmaf;"},{0x03C3,"&sigma;"},{0x03C4,"&tau;"},` |
|      - | 5139 | `	{0x03C5,"&upsilon;"},{0x03C6,"&phi;"},{0x03C7,"&chi;"},{0x03C8,"&psi;"},` |
|      - | 5140 | `	{0x03C9,"&omega;"},{0x03D1,"&thetasym;"},{0x03D2,"&upsih;"},{0x03D6,"&piv;"},` |
|      - | 5141 | `	{0x2002,"&ensp;"},{0x2003,"&emsp;"},{0x2009,"&thinsp;"},{0x200C,"&zwnj;"},` |
|      - | 5142 | `	{0x200D,"&zwj;"},{0x200E,"&lrm;"},{0x200F,"&rlm;"},{0x2013,"&ndash;"},` |
|      - | 5143 | `	{0x2014,"&mdash;"},{0x2018,"&lsquo;"},{0x2019,"&rsquo;"},{0x201A,"&sbquo;"},` |
|      - | 5144 | `	{0x201C,"&ldquo;"},{0x201D,"&rdquo;"},{0x201E,"&bdquo;"},{0x2020,"&dagger;"},` |
|      - | 5145 | `	{0x2021,"&Dagger;"},{0x2022,"&bull;"},{0x2026,"&hellip;"},{0x2030,"&permil;"},` |
|      - | 5146 | `	{0x2032,"&prime;"},{0x2033,"&Prime;"},{0x2039,"&lsaquo;"},{0x203A,"&rsaquo;"},` |
|      - | 5147 | `	{0x203E,"&oline;"},{0x2044,"&frasl;"},{0x20AC,"&euro;"},{0x2111,"&image;"},` |
|      - | 5148 | `	{0x2118,"&weierp;"},{0x211C,"&real;"},{0x2122,"&trade;"},{0x2135,"&alefsym;"},` |
|      - | 5149 | `	{0x2190,"&larr;"},{0x2191,"&uarr;"},{0x2192,"&rarr;"},{0x2193,"&darr;"},` |
|      - | 5150 | `	{0x2194,"&harr;"},{0x21B5,"&crarr;"},{0x21D0,"&lArr;"},{0x21D1,"&uArr;"},` |
|      - | 5151 | `	{0x21D2,"&rArr;"},{0x21D3,"&dArr;"},{0x21D4,"&hArr;"},{0x2200,"&forall;"},` |
|      - | 5152 | `	{0x2202,"&part;"},{0x2203,"&exist;"},{0x2205,"&empty;"},{0x2207,"&nabla;"},` |
|      - | 5153 | `	{0x2208,"&isin;"},{0x2209,"&notin;"},{0x220B,"&ni;"},{0x220F,"&prod;"},` |
|      - | 5154 | `	{0x2211,"&sum;"},{0x2212,"&minus;"},{0x2217,"&lowast;"},{0x221A,"&radic;"},` |
|      - | 5155 | `	{0x221D,"&prop;"},{0x221E,"&infin;"},{0x2220,"&ang;"},{0x2227,"&and;"},` |
|      - | 5156 | `	{0x2228,"&or;"},{0x2229,"&cap;"},{0x222A,"&cup;"},{0x222B,"&int;"},` |
|      - | 5157 | `	{0x2234,"&there4;"},{0x223C,"&sim;"},{0x2245,"&cong;"},{0x2248,"&asymp;"},` |
|      - | 5158 | `	{0x2260,"&ne;"},{0x2261,"&equiv;"},{0x2264,"&le;"},{0x2265,"&ge;"},` |
|      - | 5159 | `	{0x2282,"&sub;"},{0x2283,"&sup;"},{0x2284,"&nsub;"},{0x2286,"&sube;"},` |
|      - | 5160 | `	{0x2287,"&supe;"},{0x2295,"&oplus;"},{0x2297,"&otimes;"},{0x22A5,"&perp;"},` |
|      - | 5161 | `	{0x22C5,"&sdot;"},{0x2308,"&lceil;"},{0x2309,"&rceil;"},{0x230A,"&lfloor;"},` |
|      - | 5162 | `	{0x230B,"&rfloor;"},{0x2329,"&lang;"},{0x232A,"&rang;"},{0x25CA,"&loz;"},` |
|      - | 5163 | `	{0x2660,"&spades;"},{0x2663,"&clubs;"},{0x2665,"&hearts;"},{0x2666,"&diams;"}` |
|      - | 5164 | `};` |
|      - | 5165 | `/* Binary-search aHtml401Ent[] for cp; return its "&name;" entity or 0. */` |
|     41 | 5166 | `static const char *FvHtml401Lookup(sxu32 cp){` |
|     41 | 5167 | `	int lo = 0, hi = (int)SX_ARRAYSIZE(aHtml401Ent) - 1;` |
|    323 | 5168 | `	while( lo <= hi ){` |
|    309 | 5169 | `		int mid = (lo + hi) / 2;` |
|    309 | 5170 | `		sxu32 c = aHtml401Ent[mid].cp;` |
|    309 | 5171 | `		if( c == cp ){ return aHtml401Ent[mid].zEnt; }` |
|    283 | 5172 | `		if( c < cp ){ lo = mid + 1; } else { hi = mid - 1; }` |
|      1 | 5173 | `	}` |
|     15 | 5174 | `	return 0;` |
|     21 | 5175 | `}` |
|      - | 5176 | `/* Decode one strict-UTF-8 sequence at p (< zEnd). On success returns its byte` |
|      - | 5177 | ` * length (1..4) and sets *pCp to the codepoint; on any malformed, overlong,` |
|      - | 5178 | ` * surrogate, truncated or out-of-range (>U+10FFFF) sequence returns 0. Matches` |
|      - | 5179 | ` * PHP's UTF-8 validation used by FULL_SPECIAL_CHARS (verified vs php 8.5.7). */` |
|    101 | 5180 | `static int FvUtf8Next(const unsigned char *p,const unsigned char *zEnd,sxu32 *pCp){` |
|    101 | 5181 | `	unsigned char c = p[0];` |
|    101 | 5182 | `	if( c < 0x80 ){ *pCp = c; return 1; }` |
|    101 | 5183 | `	if( c < 0xC2 ){ return 0; }              /* 0x80-0xBF stray cont / 0xC0-0xC1 overlong */` |
|     99 | 5184 | `	if( c < 0xE0 ){                          /* 2-byte: U+0080..U+07FF */` |
|     47 | 5185 | `		if( zEnd-p < 2 \|\| (p[1]&0xC0)!=0x80 ){ return 0; }` |
|     45 | 5186 | `		*pCp = ((sxu32)(c&0x1F)<<6) \| (p[1]&0x3F);` |
|     45 | 5187 | `		return 2;` |
|      - | 5188 | `	}` |
|     53 | 5189 | `	if( c < 0xF0 ){                          /* 3-byte: U+0800..U+FFFF minus surrogates */` |
|      - | 5190 | `		sxu32 cp;` |
|     47 | 5191 | `		if( zEnd-p < 3 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 ){ return 0; }` |
|     33 | 5192 | `		cp = ((sxu32)(c&0x0F)<<12) \| ((sxu32)(p[1]&0x3F)<<6) \| (p[2]&0x3F);` |
|     33 | 5193 | `		if( cp < 0x800 \|\| (cp>=0xD800 && cp<=0xDFFF) ){ return 0; }` |
|     29 | 5194 | `		*pCp = cp;` |
|     29 | 5195 | `		return 3;` |
|      - | 5196 | `	}` |
|      7 | 5197 | `	if( c < 0xF5 ){                          /* 4-byte: U+10000..U+10FFFF */` |
|      - | 5198 | `		sxu32 cp;` |
|      5 | 5199 | `		if( zEnd-p < 4 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 \|\| (p[3]&0xC0)!=0x80 ){ return 0; }` |
|      5 | 5200 | `		cp = ((sxu32)(c&0x07)<<18) \| ((sxu32)(p[1]&0x3F)<<12) \| ((sxu32)(p[2]&0x3F)<<6) \| (p[3]&0x3F);` |
|      5 | 5201 | `		if( cp < 0x10000 \|\| cp > 0x10FFFF ){ return 0; }` |
|      5 | 5202 | `		*pCp = cp;` |
|      5 | 5203 | `		return 4;` |
|      - | 5204 | `	}` |
|      3 | 5205 | `	return 0;                                /* 0xF5-0xFF */` |
|     51 | 5206 | `}` |
|      - | 5207 | `/* FILTER_SANITIZE_FULL_SPECIAL_CHARS: htmlentities-style, UTF-8-aware. Encodes` |
|      - | 5208 | ` * <>&"' as named entities ("'" -> &#039;; quotes suppressed under NO_ENCODE_QUOTES),` |
|      - | 5209 | ` * and every valid UTF-8 codepoint with an HTML 4.01 named entity as that entity;` |
|      - | 5210 | ` * valid codepoints without a named entity (and low control bytes) pass through` |
|      - | 5211 | ` * verbatim. If the input contains ANY invalid UTF-8 the whole result is "".` |
|      - | 5212 | ` * The STRIP/ENCODE flags do NOT apply to this filter (only NO_ENCODE_QUOTES).` |
|      - | 5213 | ` * php's filter does NOT re-encode valid pre-existing entities ("&amp;" stays,` |
|      - | 5214 | ` * "&bogus;" becomes "&amp;bogus;"), i.e. double_encode=false semantics —` |
|      - | 5215 | ` * exactly htmlentities(ENT_QUOTES\|ENT_HTML401, double_encode: false), so this` |
|      - | 5216 | ` * delegates to the shared encoder. Byte-exact vs php 8.5.7. */` |
|     25 | 5217 | `static void FvSanitizeFull(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 5218 | `	int iEntFlags = (flags & FV_FLAG_NO_ENCODE_QUOTES) ? 0 : PH7_ENT_QUOTES;` |
|     25 | 5219 | `	HtmlEscape(pCtx,z,n,iEntFlags,1/*bAll*/,0/*bDoubleEncode*/);` |
|     25 | 5220 | `}` |
|      - | 5221 | `/* ---------------------------------------------------------------------------` |
|      - | 5222 | ` * UTF-8-aware HTML entity core (htmlspecialchars/htmlentities family).` |
|      - | 5223 | ` * Prototyped next to the five builtins earlier in this file; lives here so it` |
|      - | 5224 | ` * can share aHtml401Ent[]/FvHtml401Lookup()/FvUtf8Next() with the filter_var` |
|      - | 5225 | ` * FULL_SPECIAL_CHARS filter above. Byte-exact vs php 8.5.7 (oracle-swept).` |
|      - | 5226 | ` * ------------------------------------------------------------------------ */` |
|      - | 5227 | `/* Encode cp as UTF-8 into zBuf (>= 4 bytes); return the byte length 1..4.` |
|      - | 5228 | ` * Thin wrapper over the engine-wide SX_WRITE_UTF8 (sxmacros.h). */` |
|    585 | 5229 | `static int HtmlCpUtf8(sxu32 cp,char *zBuf){` |
|    585 | 5230 | `	sxu8 *z = (sxu8 *)zBuf;` |
|    585 | 5231 | `	SX_WRITE_UTF8(z,cp);` |
|    585 | 5232 | `	return (int)(z - (sxu8 *)zBuf);` |
|      1 | 5233 | `}` |
|      - | 5234 | `/* Doctype-allowed codepoint test (php's unicode_cp_is_allowed) — gates what a` |
|      - | 5235 | ` * numeric reference may DECODE to. Oracle-pinned per doctype: HTML401` |
|      - | 5236 | ` * disallows C0 (except TAB/LF/CR) and DEL..U+009F; XML1 and XHTML share the` |
|      - | 5237 | ` * XML rules — DEL..U+009F allowed, U+FFFE/U+FFFF excluded; HTML5 swaps CR` |
|      - | 5238 | ` * for FF (0x0C) and excludes the noncharacters (U+FDD0..U+FDEF and every` |
|      - | 5239 | ` * U+xFFFE/U+xFFFF). Surrogates are disallowed everywhere. */` |
|     91 | 5240 | `static int HtmlCpAllowed(sxu32 cp,int iFlags){` |
|     91 | 5241 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|     91 | 5242 | `	if( cp==0x09 \|\| cp==0x0A ){ return 1; }` |
|     87 | 5243 | `	if( cp==0x0D ){ return iDoc != PH7_ENT_DOC_HTML5; }` |
|     85 | 5244 | `	if( cp==0x0C ){ return iDoc == PH7_ENT_DOC_HTML5; }` |
|     85 | 5245 | `	if( cp < 0x20 \|\| cp > 0x10FFFF ){ return 0; }` |
|     79 | 5246 | `	if( cp>=0xD800 && cp<=0xDFFF ){ return 0; }` |
|     77 | 5247 | `	if( cp>=0x7F && cp<=0x9F ){ return iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML; }` |
|     71 | 5248 | `	if( iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML ){` |
|    ! 0 | 5249 | `		return cp!=0xFFFE && cp!=0xFFFF;` |
|      - | 5250 | `	}` |
|     71 | 5251 | `	if( iDoc == PH7_ENT_DOC_HTML5 ){` |
|      9 | 5252 | `		if( cp>=0xFDD0 && cp<=0xFDEF ){ return 0; }` |
|      9 | 5253 | `		if( (cp & 0xFFFF) >= 0xFFFE ){ return 0; }` |
|      4 | 5254 | `	}` |
|     71 | 5255 | `	return 1;` |
|     46 | 5256 | `}` |
|      - | 5257 | `/* The ENT_DISALLOWED gate for RAW characters on the ENCODE side. Same as the` |
|      - | 5258 | ` * decode gate except CR under HTML5: php's encode-side unicode_cp_is_allowed` |
|      - | 5259 | ` * keeps a literal "\r" verbatim under ENT_HTML5\|ENT_DISALLOWED while the` |
|      - | 5260 | ` * decode side leaves "&#13;" un-decoded (oracle-pinned at flags 176). */` |
|      9 | 5261 | `static int HtmlCpAllowedEncode(sxu32 cp,int iFlags){` |
|      9 | 5262 | `	if( cp==0x0D && (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 ){ return 1; }` |
|      9 | 5263 | `	return HtmlCpAllowed(cp,iFlags);` |
|      5 | 5264 | `}` |
|      - | 5265 | `/* Numeric-reference validity for the double_encode=false "is this already a` |
|      - | 5266 | ` * valid entity" test — a MUCH looser predicate than the decode gate above:` |
|      - | 5267 | ` * any codepoint <= U+10FFFF is valid (controls and surrogates included, every` |
|      - | 5268 | ` * doctype). ENT_DISALLOWED re-tightens non-HTML401 doctypes to the decode` |
|      - | 5269 | ` * gate, except that HTML5 exempts surrogates. All oracle-pinned: &#0; and` |
|      - | 5270 | ` * &#xD800; stay verbatim at flags 11 and 139; flags -1 (HTML5+DISALLOWED)` |
|      - | 5271 | ` * re-encodes &#0; and &#x10FFFF; but still keeps &#xD800;; flags 144` |
|      - | 5272 | ` * (XML1+DISALLOWED) re-encodes &#xD800;. */` |
|      9 | 5273 | `static int HtmlNumericAllowed(sxu32 cp,int iFlags){` |
|      9 | 5274 | `	if( cp > 0x10FFFF ){ return 0; }` |
|      7 | 5275 | `	if( (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML401 ){ return 1; /* never tightened */ }` |
|    ! 0 | 5276 | `	if( (iFlags & PH7_ENT_DISALLOWED)` |
|    ! 0 | 5277 | `	 && !((iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 && cp>=0xD800 && cp<=0xDFFF)` |
|    ! 0 | 5278 | `	 && !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|    ! 0 | 5279 | `	return 1;` |
|      5 | 5280 | `}` |
|      - | 5281 | `/* How many bytes the malformed UTF-8 sequence at p consumes — php's` |
|      - | 5282 | ` * get_next_char failure step (one U+FFFD substitution / one ENT_IGNORE drop` |
|      - | 5283 | ` * per MAXIMAL invalid subpart, not per byte): a prefix-valid sequence eats` |
|      - | 5284 | ` * its continuation bytes ("\xE0\x80\xAF" is ONE unit) while a byte that could` |
|      - | 5285 | ` * start a new sequence is left for the next round. */` |
|      5 | 5286 | `static int HtmlUtf8Trail(unsigned char c){ return c>=0x80 && c<=0xBF; }` |
|     11 | 5287 | `static int HtmlUtf8Lead(unsigned char c){ return c<0x80 \|\| (c>=0xC2 && c<=0xF4); }` |
|     15 | 5288 | `static int HtmlUtf8FailAdvance(const unsigned char *p,const unsigned char *zEnd){` |
|     15 | 5289 | `	unsigned char c = p[0];` |
|     15 | 5290 | `	int nAvail = (int)(zEnd - p);` |
|     15 | 5291 | `	if( c < 0xC2 \|\| c > 0xF4 ){ return 1; } /* stray trail / C0-C1 / F5-FF */` |
|     13 | 5292 | `	if( c < 0xE0 ){` |
|      3 | 5293 | `		if( nAvail < 2 ){ return 1; }` |
|      3 | 5294 | `		return HtmlUtf8Lead(p[1]) ? 1 : 2;` |
|      - | 5295 | `	}` |
|     11 | 5296 | `	if( c < 0xF0 ){` |
|     11 | 5297 | `		if( nAvail >= 3 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) ){` |
|      3 | 5298 | `			return 3; /* complete but overlong/surrogate */` |
|      - | 5299 | `		}` |
|      9 | 5300 | `		if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 5301 | `		if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 5302 | `		return 3;` |
|      - | 5303 | `	}` |
|    ! 0 | 5304 | `	if( nAvail >= 4 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) && HtmlUtf8Trail(p[3]) ){` |
|    ! 0 | 5305 | `		return 4; /* complete but overlong / > U+10FFFF */` |
|      - | 5306 | `	}` |
|    ! 0 | 5307 | `	if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 5308 | `	if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 5309 | `	if( nAvail < 4 \|\| HtmlUtf8Lead(p[3]) ){ return 3; }` |
|    ! 0 | 5310 | `	return 4;` |
|      8 | 5311 | `}` |
|      - | 5312 | `/* The basic special entities, shared by named matching, the hsc_decode` |
|      - | 5313 | ` * numeric whitelist and the translation-table builder so the sets can never` |
|      - | 5314 | ` * drift apart. (&apos; is not an HTML 4.01 entity — doctype-gated below.) */` |
|      - | 5315 | `static const struct { const char *zEnt; int n; sxu32 cp; } aHtmlSpecEnt[] = {` |
|      - | 5316 | `	{"&amp;",5,38},{"&lt;",4,60},{"&gt;",4,62},{"&quot;",6,34},{"&apos;",6,39}` |
|      - | 5317 | `};` |
|      - | 5318 | `/* Does this doctype consult the named-entity table (aHtml401Ent)? XML 1.0 has` |
|      - | 5319 | ` * no named entities beyond the specials; XHTML/HTML5 are approximated by the` |
|      - | 5320 | ` * HTML 4.01 table (documented divergence, PLAN.md §3.9). */` |
|     63 | 5321 | `static int HtmlDocHasNamedTable(int iDoc){` |
|     63 | 5322 | `	return iDoc != PH7_ENT_DOC_XML1;` |
|      1 | 5323 | `}` |
|      - | 5324 | `/* The single-quote entity per doctype. Oracle-pinned asymmetry: for every` |
|      - | 5325 | ` * non-HTML401 doctype htmlspecialchars emits &apos; while htmlentities` |
|      - | 5326 | ` * (bEntities) keeps &#039; under XHTML too. The translation table mirrors` |
|      - | 5327 | ` * whichever function the requested table belongs to. */` |
|     29 | 5328 | `static const char *HtmlAposEntity(int iDoc,int bEntities){` |
|     29 | 5329 | `	if( iDoc == PH7_ENT_DOC_HTML401 \|\| (bEntities && iDoc == PH7_ENT_DOC_XHTML) ){` |
|     21 | 5330 | `		return "&#039;";` |
|      - | 5331 | `	}` |
|      9 | 5332 | `	return "&apos;";` |
|     15 | 5333 | `}` |
|      - | 5334 | `/* Try to parse one HTML entity at z (z[0]=='&', z < zEnd). bFull selects the` |
|      - | 5335 | ` * html_entity_decode set (doctype named table + any allowed numeric ref) vs` |
|      - | 5336 | ` * the htmlspecialchars_decode set (the basic specials + quote numerics only).` |
|      - | 5337 | ` * Named matching is case-SENSITIVE and the ';' is required (both PHP-exact);` |
|      - | 5338 | ` * numeric refs accept dec/hex (x or X) with any number of leading zeros but` |
|      - | 5339 | ` * reject out-of-range, surrogate and doctype-disallowed codepoints (the` |
|      - | 5340 | ` * caller then leaves the source verbatim). Quote-flag gating is NOT applied` |
|      - | 5341 | ` * here — the same routine doubles as the "is this a valid entity" test for` |
|      - | 5342 | ` * double_encode=false, which ignores the quote bits (oracle-pinned).` |
|      - | 5343 | ` * bEncodeCheck selects the looser HtmlNumericAllowed predicate used by that` |
|      - | 5344 | ` * double_encode test; decode callers pass 0 for the HtmlCpAllowed gate.` |
|      - | 5345 | ` * On success sets *pCp / *pnConsumed and returns 1. */` |
|    172 | 5346 | `static int HtmlParseEntity(const unsigned char *z,const unsigned char *zEnd,` |
|      1 | 5347 | `                           int iFlags,int bFull,int bEncodeCheck,sxu32 *pCp,int *pnConsumed){` |
|    173 | 5348 | `	int nAvail = (int)(zEnd - z);` |
|    173 | 5349 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 5350 | `	sxu32 n;` |
|    173 | 5351 | `	if( nAvail < 4 ){ return 0; } /* shortest entities: &lt; &#9; */` |
|    169 | 5352 | `	if( z[1] == '#' ){` |
|      - | 5353 | `		/* Numeric reference */` |
|     89 | 5354 | `		sxu32 cp = 0;` |
|     89 | 5355 | `		int i = 2, bHex = 0, nDig = 0;` |
|     89 | 5356 | `		if( z[i]=='x' \|\| z[i]=='X' ){ bHex = 1; i++; }` |
|    317 | 5357 | `		for( ; i < nAvail && z[i] != ';' ; i++ ){` |
|      - | 5358 | `			int v;` |
|    221 | 5359 | `			unsigned char c = z[i];` |
|    221 | 5360 | `			if( c>='0' && c<='9' ){ v = c - '0'; }` |
|     17 | 5361 | `			else if( bHex && c>='a' && c<='f' ){ v = c - 'a' + 10; }` |
|     17 | 5362 | `			else if( bHex && c>='A' && c<='F' ){ v = c - 'A' + 10; }` |
|    ! 0 | 5363 | `			else { return 0; }` |
|      - | 5364 | `			/* Stop accumulating once out of range (keeps validating the shape;` |
|      - | 5365 | `			 * max intermediate is 0x10FFFF*16+15, no sxu32 overflow). */` |
|    221 | 5366 | `			if( cp <= 0x10FFFF ){ cp = cp * (bHex ? 16 : 10) + (sxu32)v; }` |
|    221 | 5367 | `			nDig++;` |
|    111 | 5368 | `		}` |
|     97 | 5369 | `		if( nDig == 0 \|\| i >= nAvail ){ return 0; } /* no digits / no ';' */` |
|     97 | 5370 | `		if( bEncodeCheck ? !HtmlNumericAllowed(cp,iFlags) : !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|     83 | 5371 | `		if( !bFull ){` |
|      - | 5372 | `			/* hsc_decode: numeric refs to the five specials only. */` |
|     99 | 5373 | `			for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) && aHtmlSpecEnt[n].cp != cp ; n++ ){}` |
|     25 | 5374 | `			if( n >= SX_ARRAYSIZE(aHtmlSpecEnt) ){ return 0; }` |
|     11 | 5375 | `		}` |
|     75 | 5376 | `		*pCp = cp;` |
|     75 | 5377 | `		*pnConsumed = i + 1;` |
|     75 | 5378 | `		return 1;` |
|      - | 5379 | `	}` |
|      - | 5380 | `	/* Named reference — every entity name starts with a letter, so anything` |
|      - | 5381 | `	 * else can bail out before touching the tables. */` |
|     81 | 5382 | `	if( !((z[1]>='a' && z[1]<='z') \|\| (z[1]>='A' && z[1]<='Z')) ){ return 0; }` |
|    287 | 5383 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) ; n++ ){` |
|    265 | 5384 | `		if( aHtmlSpecEnt[n].cp == 39 && iDoc == PH7_ENT_DOC_HTML401 ){ continue; }` |
|    243 | 5385 | `		if( nAvail >= aHtmlSpecEnt[n].n && SyMemcmp(z,aHtmlSpecEnt[n].zEnt,(sxu32)aHtmlSpecEnt[n].n) == 0 ){` |
|     53 | 5386 | `			*pCp = aHtmlSpecEnt[n].cp;` |
|     53 | 5387 | `			*pnConsumed = aHtmlSpecEnt[n].n;` |
|     53 | 5388 | `			return 1;` |
|      - | 5389 | `		}` |
|     96 | 5390 | `	}` |
|     23 | 5391 | `	if( bFull && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 5392 | `		/* Linear scan of the 248-row table: runs only at '&'-then-letter` |
|      - | 5393 | `		 * positions and guarantees the decode set can never drift from the` |
|      - | 5394 | `		 * encode table. The first-letter guard skips the SyStrlen/SyMemcmp` |
|      - | 5395 | `		 * for ~96% of rows. */` |
|   3369 | 5396 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|      - | 5397 | `			sxu32 nEnt;` |
|   3357 | 5398 | `			if( z[1] != (unsigned char)aHtml401Ent[n].zEnt[1] ){ continue; }` |
|    121 | 5399 | `			nEnt = SyStrlen(aHtml401Ent[n].zEnt);` |
|    121 | 5400 | `			if( (sxu32)nAvail >= nEnt && SyMemcmp(z,aHtml401Ent[n].zEnt,nEnt) == 0 ){` |
|      7 | 5401 | `				*pCp = aHtml401Ent[n].cp;` |
|      7 | 5402 | `				*pnConsumed = (int)nEnt;` |
|      7 | 5403 | `				return 1;` |
|      - | 5404 | `			}` |
|     58 | 5405 | `		}` |
|      6 | 5406 | `	}` |
|     17 | 5407 | `	return 0;` |
|     88 | 5408 | `}` |
|      - | 5409 | `/* Shared encoder for htmlspecialchars (bAll=0) and htmlentities (bAll=1).` |
|      - | 5410 | ` * Invalid UTF-8 policy: ENT_IGNORE drops the byte (and wins over SUBSTITUTE),` |
|      - | 5411 | ` * ENT_SUBSTITUTE emits one U+FFFD per invalid byte, neither -> the whole` |
|      - | 5412 | ` * result is "" (pre-validated in a first pass: the accumulating result API` |
|      - | 5413 | ` * cannot roll back — same reason FvSanitizeFull is two-pass). */` |
|     94 | 5414 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 5415 | `                       int iFlags,int bAll,int bDoubleEncode){` |
|     95 | 5416 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     95 | 5417 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|      - | 5418 | `	const unsigned char *runStart;` |
|     95 | 5419 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 5420 | `	sxu32 cp;` |
|     95 | 5421 | `	if( (iFlags & (PH7_ENT_IGNORE\|PH7_ENT_SUBSTITUTE)) == 0 ){` |
|      - | 5422 | `		/* Pass 1: any malformed sequence rejects the entire input. ASCII` |
|      - | 5423 | `		 * bytes cannot be malformed, so skip them without the decoder. */` |
|    381 | 5424 | `		while( p < zEnd ){` |
|      - | 5425 | `			int len;` |
|    323 | 5426 | `			if( *p < 0x80 ){ p++; continue; }` |
|     37 | 5427 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     37 | 5428 | `			if( len == 0 ){ ph7_result_string(pCtx,"",0); return; }` |
|     27 | 5429 | `			p += len;` |
|      1 | 5430 | `		}` |
|     59 | 5431 | `		p = (const unsigned char *)zIn;` |
|     29 | 5432 | `	}` |
|     85 | 5433 | `	runStart = p;` |
|     85 | 5434 | `	ph7_result_string(pCtx,"",0);` |
|    455 | 5435 | `	while( p < zEnd ){` |
|    371 | 5436 | `		const char *zEnt = 0;` |
|      - | 5437 | `		int len;` |
|    371 | 5438 | `		if( *p < 0x80 ){` |
|    307 | 5439 | `			len = 1;` |
|    307 | 5440 | `			switch( *p ){` |
|     25 | 5441 | `			case '<': zEnt = "&lt;"; break;` |
|     25 | 5442 | `			case '>': zEnt = "&gt;"; break;` |
|     18 | 5443 | `			case '&':` |
|     37 | 5444 | `				zEnt = "&amp;";` |
|     37 | 5445 | `				if( !bDoubleEncode ){` |
|      - | 5446 | `					sxu32 eCp; int nEat;` |
|     25 | 5447 | `					if( HtmlParseEntity(p,zEnd,iFlags,1,1,&eCp,&nEat) ){` |
|      - | 5448 | `						/* A valid existing entity: keep it verbatim. */` |
|     13 | 5449 | `						zEnt = 0;` |
|     13 | 5450 | `						len = nEat;` |
|      6 | 5451 | `					}` |
|     12 | 5452 | `				}` |
|     37 | 5453 | `				break;` |
|     10 | 5454 | `			case '"':` |
|     21 | 5455 | `				if( iFlags & PH7_ENT_QUOTE_DOUBLE ){ zEnt = "&quot;"; }` |
|     21 | 5456 | `				break;` |
|     12 | 5457 | `			case '\'':` |
|     25 | 5458 | `				if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|     23 | 5459 | `					zEnt = HtmlAposEntity(iDoc,bAll);` |
|     11 | 5460 | `				}` |
|     25 | 5461 | `				break;` |
|     89 | 5462 | `			default:` |
|    179 | 5463 | `				if( (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode((sxu32)*p,iFlags) ){` |
|    ! 0 | 5464 | `					zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 5465 | `				}` |
|    178 | 5466 | `				break;` |
|      - | 5467 | `			}` |
|    154 | 5468 | `		}else{` |
|     65 | 5469 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     65 | 5470 | `			if( len == 0 ){` |
|      - | 5471 | `				/* Malformed subpart (IGNORE or SUBSTITUTE is set, else pass 1` |
|      - | 5472 | `				 * would have rejected): drop it or emit ONE U+FFFD for the` |
|      - | 5473 | `				 * whole unit (php substitutes per maximal invalid subpart). */` |
|     15 | 5474 | `				if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|     15 | 5475 | `				if( (iFlags & PH7_ENT_IGNORE) == 0 ){ ph7_result_string(pCtx,"\xEF\xBF\xBD",3); }` |
|     15 | 5476 | `				p += HtmlUtf8FailAdvance(p,zEnd);` |
|     15 | 5477 | `				runStart = p;` |
|     15 | 5478 | `				continue;` |
|      - | 5479 | `			}` |
|     51 | 5480 | `			if( bAll && HtmlDocHasNamedTable(iDoc) ){` |
|     41 | 5481 | `				zEnt = FvHtml401Lookup(cp);` |
|     20 | 5482 | `			}` |
|     51 | 5483 | `			if( zEnt == 0 && (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode(cp,iFlags) ){` |
|    ! 0 | 5484 | `				zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 5485 | `			}` |
|      - | 5486 | `		}` |
|    357 | 5487 | `		if( zEnt ){` |
|    135 | 5488 | `			if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|    135 | 5489 | `			ph7_result_string(pCtx,zEnt,-1);` |
|    135 | 5490 | `			runStart = p + len;` |
|     67 | 5491 | `		}` |
|    357 | 5492 | `		p += len;` |
|      1 | 5493 | `	}` |
|     85 | 5494 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     48 | 5495 | `}` |
|      - | 5496 | `/* Shared decoder for html_entity_decode (bFull=1) and htmlspecialchars_decode` |
|      - | 5497 | ` * (bFull=0). Quote refs (cp 34/39, named or numeric) are gated by the quote` |
|      - | 5498 | ` * bits and left verbatim when suppressed; an invalid entity leaves its '&'` |
|      - | 5499 | ` * verbatim and rescans right after it, which also yields PHP's no-double-` |
|      - | 5500 | ` * decode behavior ("&amp;lt;" -> "&lt;"). */` |
|     82 | 5501 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 5502 | `                         int iFlags,int bFull){` |
|     83 | 5503 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     83 | 5504 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|     83 | 5505 | `	const unsigned char *runStart = p;` |
|     83 | 5506 | `	ph7_result_string(pCtx,"",0);` |
|    557 | 5507 | `	while( p < zEnd ){` |
|      - | 5508 | `		sxu32 cp;` |
|      - | 5509 | `		int nEat;` |
|    510 | 5510 | `		if( *p != '&' ){ p++; continue; }` |
|    155 | 5511 | `		if( !HtmlParseEntity(p,zEnd,iFlags,bFull,0,&cp,&nEat) ){ p++; continue; }` |
|    124 | 5512 | `		if( (cp == 34 && (iFlags & PH7_ENT_QUOTE_DOUBLE) == 0)` |
|    117 | 5513 | `		 \|\| (cp == 39 && (iFlags & PH7_ENT_QUOTE_SINGLE) == 0) ){` |
|      - | 5514 | `			/* Suppressed quote: leave the entity source verbatim. */` |
|     37 | 5515 | `			p += nEat;` |
|     37 | 5516 | `			continue;` |
|      - | 5517 | `		}` |
|     89 | 5518 | `		if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|      - | 5519 | `		{` |
|      - | 5520 | `			char zBuf[4];` |
|     89 | 5521 | `			int n = HtmlCpUtf8(cp,zBuf);` |
|     89 | 5522 | `			ph7_result_string(pCtx,zBuf,n);` |
|      - | 5523 | `		}` |
|     89 | 5524 | `		p += nEat;` |
|     89 | 5525 | `		runStart = p;` |
|      1 | 5526 | `	}` |
|     79 | 5527 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     79 | 5528 | `}` |
|      - | 5529 | `/* Validate the optional charset argument at apArg[idx]: UTF-8 aliases (and` |
|      - | 5530 | ` * ""/NULL meaning the default) are accepted; anything else — including` |
|      - | 5531 | ` * php-supported single-byte charsets like ISO-8859-1, PHL is UTF-8-only per` |
|      - | 5532 | ` * PLAN.md §6 — raises PHP's unsupported-charset warning and is treated as` |
|      - | 5533 | ` * UTF-8 (ph7_context_throw_error_format prepends the function name). */` |
|    141 | 5534 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx){` |
|      - | 5535 | `	const char *zCs;` |
|      - | 5536 | `	int nCs;` |
|    148 | 5537 | `	if( nArg <= idx \|\| ph7_value_is_null(apArg[idx]) ){ return; }` |
|     15 | 5538 | `	zCs = ph7_value_to_string(apArg[idx],&nCs);` |
|     15 | 5539 | `	if( nCs == 0 ){ return; } /* "" selects the default charset (UTF-8) */` |
|     13 | 5540 | `	if( nCs == 5 && SyStrnicmp(zCs,"UTF-8",5) == 0 ){` |
|     13 | 5541 | `		return; /* php accepts only "UTF-8" (any case) silently — "UTF8" warns */` |
|      - | 5542 | `	}` |
|    ! 0 | 5543 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5544 | `		"Charset \"%.*s\" is not supported, assuming UTF-8",nCs,zCs);` |
|     71 | 5545 | `}` |
|      - | 5546 | `/* get_html_translation_table() worker: character (UTF-8 bytes) => entity.` |
|      - | 5547 | ` * The five specials come first in byte order, then — for HTML_ENTITIES with a` |
|      - | 5548 | ` * named-table doctype — the 248 aHtml401Ent rows ascending (oracle-pinned` |
|      - | 5549 | ` * ordering; 253 entries under the defaults). */` |
|    549 | 5550 | `static void HtmlTableAdd(ph7_value *pArray,ph7_value *pValue,const char *zKey,const char *zEnt){` |
|    549 | 5551 | `	ph7_value_string(pValue,zEnt,-1);` |
|    549 | 5552 | `	ph7_array_add_strkey_elem(pArray,zKey,pValue);` |
|    549 | 5553 | `	ph7_value_reset_string_cursor(pValue);` |
|    549 | 5554 | `}` |
|     13 | 5555 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags){` |
|      - | 5556 | `	ph7_value *pArray,*pValue;` |
|     13 | 5557 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 5558 | `	sxu32 n;` |
|     13 | 5559 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 5560 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 5561 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|    ! 0 | 5562 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5563 | `		return;` |
|      - | 5564 | `	}` |
|     13 | 5565 | `	if( iFlags & PH7_ENT_QUOTE_DOUBLE ){` |
|     11 | 5566 | `		HtmlTableAdd(pArray,pValue,"\"","&quot;");` |
|      5 | 5567 | `	}` |
|     13 | 5568 | `	HtmlTableAdd(pArray,pValue,"&","&amp;");` |
|     13 | 5569 | `	if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|      - | 5570 | `		/* The apostrophe row mirrors the function each table belongs to:` |
|      - | 5571 | `		 * SPECIALCHARS follows htmlspecialchars, ENTITIES follows` |
|      - | 5572 | `		 * htmlentities (oracle-pinned at flags 35). */` |
|      7 | 5573 | `		HtmlTableAdd(pArray,pValue,"'",HtmlAposEntity(iDoc,iTable != 0));` |
|      3 | 5574 | `	}` |
|     13 | 5575 | `	HtmlTableAdd(pArray,pValue,"<","&lt;");` |
|     13 | 5576 | `	HtmlTableAdd(pArray,pValue,">","&gt;");` |
|     13 | 5577 | `	if( iTable != 0 /*php: any non-HTML_SPECIALCHARS table => entities*/ && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 5578 | `		char zKey[8];` |
|    499 | 5579 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|    497 | 5580 | `			int nK = HtmlCpUtf8(aHtml401Ent[n].cp,zKey);` |
|    497 | 5581 | `			zKey[nK] = 0;` |
|    497 | 5582 | `			HtmlTableAdd(pArray,pValue,zKey,aHtml401Ent[n].zEnt);` |
|    249 | 5583 | `		}` |
|      1 | 5584 | `	}` |
|     13 | 5585 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 5586 | `}` |
|     25 | 5587 | `static int FvEmailAllowed(unsigned char c){` |
|     25 | 5588 | `	if( (c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9') ){ return 1; }` |
|     16 | 5589 | `	return c=='!'\|\|c=='#'\|\|c=='$'\|\|c=='%'\|\|c=='&'\|\|c=='\''\|\|c=='*'\|\|c=='+'` |
|     10 | 5590 | ``	    \|\| c=='-'\|\|c=='='\|\|c=='?'\|\|c=='^'\|\|c=='_'\|\|c=='`'\|\|c=='{'\|\|c=='\|'`` |
|     15 | 5591 | `	    \|\| c=='}'\|\|c=='~'\|\|c=='@'\|\|c=='.'\|\|c=='['\|\|c==']';` |
|     13 | 5592 | `}` |
|     23 | 5593 | `static int FvUrlAllowed(unsigned char c){` |
|     23 | 5594 | `	return c>=33 && c<=126; /* PHP keeps every printable ASCII byte except space */` |
|      1 | 5595 | `}` |
|      - | 5596 | `/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */` |
|      5 | 5597 | `static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){` |
|      5 | 5598 | `	int i, runStart = 0;` |
|      5 | 5599 | `	ph7_result_string(pCtx,"",0);` |
|     51 | 5600 | `	for( i=0; i<n; i++ ){` |
|     47 | 5601 | `		unsigned char c = (unsigned char)z[i];` |
|     47 | 5602 | `		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){` |
|     11 | 5603 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     11 | 5604 | `			runStart = i+1;` |
|      5 | 5605 | `		}` |
|     24 | 5606 | `	}` |
|      5 | 5607 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      5 | 5608 | `}` |
|      - | 5609 | `/*` |
|      - | 5610 | ` * Apply the selected filter to one already-resolved input value and write the` |
|      - | 5611 | ` * result into pCtx. Shared by filter_var() and filter_input(): the caller has` |
|      - | 5612 | ` * already parsed $filter/$flags/$options. On validation failure the 'default'` |
|      - | 5613 | ` * option (if any) is returned, else null when FILTER_NULL_ON_FAILURE is set,` |
|      - | 5614 | ` * else false. A validating filter that passes returns the (string) input` |
|      - | 5615 | ` * unchanged; a sanitizer writes its transformed output directly.` |
|      - | 5616 | ` */` |
|    316 | 5617 | `static int FvApplyFilter(ph7_context *pCtx,ph7_value *pInput,` |
|      - | 5618 | `                         int iFilter,int iFlags,ph7_value *pOpts,` |
|      - | 5619 | `                         ph7_value *pDefault)` |
|      3 | 5620 | `{` |
|    319 | 5621 | `	int bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|      - | 5622 | `	const char *zVal; int nVal;` |
|      - | 5623 | `	/* An array/object input fails every scalar filter. */` |
|    319 | 5624 | `	if( ph7_value_is_array(pInput) ){ goto fail; }` |
|    317 | 5625 | `	zVal = ph7_value_to_string(pInput,&nVal);` |
|    317 | 5626 | `	switch( iFilter ){` |
|     28 | 5627 | `	case FV_VALIDATE_INT: {` |
|      - | 5628 | `		ph7_int64 v;` |
|     58 | 5629 | `		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }` |
|     31 | 5630 | `		if( pOpts ){` |
|      7 | 5631 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|      7 | 5632 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|      7 | 5633 | `			if( pMin && v<ph7_value_to_int64(pMin) ){ goto fail; }` |
|      7 | 5634 | `			if( pMax && v>ph7_value_to_int64(pMax) ){ goto fail; }` |
|      2 | 5635 | `		}` |
|     29 | 5636 | `		ph7_result_int64(pCtx,v);` |
|     29 | 5637 | `		return PH7_OK;` |
|      - | 5638 | `	}` |
|     34 | 5639 | `	case FV_VALIDATE_FLOAT: {` |
|      - | 5640 | `		double d;` |
|     69 | 5641 | `		if( !FvValidateFloat(zVal,nVal,iFlags,&d) ){ goto fail; }` |
|     39 | 5642 | `		ph7_result_double(pCtx,d);` |
|     39 | 5643 | `		return PH7_OK;` |
|      - | 5644 | `	}` |
|     14 | 5645 | `	case FV_VALIDATE_BOOLEAN: {` |
|      - | 5646 | `		int b;` |
|     29 | 5647 | `		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }` |
|     21 | 5648 | `		ph7_result_bool(pCtx,b);` |
|     21 | 5649 | `		return PH7_OK;` |
|      - | 5650 | `	}` |
|     25 | 5651 | `	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|     11 | 5652 | `	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;` |
|     28 | 5653 | `	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(zVal,nVal) ){ goto fail; }     goto pass;` |
|     11 | 5654 | `	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal) ){ goto fail; }    goto pass;` |
|     15 | 5655 | `	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal) ){ goto fail; }       goto pass;` |
|      3 | 5656 | `	case FV_VALIDATE_REGEXP: {` |
|      - | 5657 | `#ifdef PH7_ENABLE_PCRE` |
|      8 | 5658 | `		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;` |
|      8 | 5659 | `		const char *zRe; int nRe, matched = 0;` |
|      8 | 5660 | `		if( pRe==0 ){` |
|      3 | 5661 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5662 | `				"filter_var(): \"regexp\" option is missing");` |
|      - | 5663 | `		}` |
|      5 | 5664 | `		zRe = ph7_value_to_string(pRe,&nRe);` |
|      5 | 5665 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK \|\| !matched ){ goto fail; }` |
|      3 | 5666 | `		goto pass;` |
|      - | 5667 | `#else` |
|      - | 5668 | `		goto fail;` |
|      - | 5669 | `#endif` |
|      - | 5670 | `	}` |
|      3 | 5671 | `	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;` |
|      5 | 5672 | `	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;` |
|     13 | 5673 | `	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,iFlags); return PH7_OK;` |
|     25 | 5674 | `	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeFull(pCtx,zVal,nVal,iFlags);    return PH7_OK;` |
|      3 | 5675 | `	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 5676 | `	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;` |
|     13 | 5677 | `	case FV_DEFAULT:` |
|      - | 5678 | `		/* FILTER_UNSAFE_RAW / FILTER_DEFAULT: pass through unchanged unless a` |
|      - | 5679 | `		 * STRIP/ENCODE flag is set, in which case apply the string filter. */` |
|     28 | 5680 | `		if( iFlags & FV_FLAG_STRING_MASK ){` |
|     15 | 5681 | `			FvSanitizeString(pCtx,zVal,nVal,iFlags);` |
|     15 | 5682 | `			return PH7_OK;` |
|      - | 5683 | `		}` |
|     14 | 5684 | `		goto pass;` |
|    ! 0 | 5685 | `	default:` |
|    ! 0 | 5686 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5687 | `			"Unknown filter with ID %d",iFilter);` |
|    ! 0 | 5688 | `		break; /* unknown filter id -> fail */` |
|    ! 0 | 5689 | `	}` |
|     58 | 5690 | `fail:` |
|    118 | 5691 | `	if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|    114 | 5692 | `	else if( bNull ){ ph7_result_null(pCtx); }` |
|    108 | 5693 | `	else { ph7_result_bool(pCtx,0); }` |
|    118 | 5694 | `	return PH7_OK;` |
|     26 | 5695 | `pass: /* validation passed: return the (string) input unchanged */` |
|     54 | 5696 | `	ph7_result_string(pCtx,zVal,nVal);` |
|     54 | 5697 | `	return PH7_OK;` |
|    161 | 5698 | `}` |
|      - | 5699 | `/*` |
|      - | 5700 | ` * Parse the ($filter, $options) pair shared by filter_var()/filter_input() out` |
|      - | 5701 | ` * of apArg[iBase] ($filter) and apArg[iBase+1] ($options): $options is either a` |
|      - | 5702 | ` * plain flags int, or an array with 'flags' and an 'options' sub-array (whose` |
|      - | 5703 | ` * 'default' entry is the fallback value). Fills the four output pointers;` |
|      - | 5704 | ` * unset outputs keep the caller-provided defaults.` |
|      - | 5705 | ` */` |
|    328 | 5706 | `static void FvParseFilterArgs(int nArg,ph7_value **apArg,int iBase,` |
|      - | 5707 | `                              int *piFilter,int *piFlags,` |
|      - | 5708 | `                              ph7_value **ppOpts,ph7_value **ppDefault)` |
|      3 | 5709 | `{` |
|    331 | 5710 | `	if( nArg>iBase ){ *piFilter = ph7_value_to_int(apArg[iBase]); }` |
|    331 | 5711 | `	if( nArg>iBase+1 ){` |
|     88 | 5712 | `		if( ph7_value_is_array(apArg[iBase+1]) ){` |
|     42 | 5713 | `			ph7_value *pF = ph7_array_fetch(apArg[iBase+1],"flags",(int)sizeof("flags")-1);` |
|     42 | 5714 | `			if( pF ){ *piFlags = ph7_value_to_int(pF); }` |
|     42 | 5715 | `			*ppOpts = ph7_array_fetch(apArg[iBase+1],"options",(int)sizeof("options")-1);` |
|     42 | 5716 | `			if( *ppOpts && !ph7_value_is_array(*ppOpts) ){ *ppOpts = 0; }` |
|     42 | 5717 | `			if( *ppOpts ){ *ppDefault = ph7_array_fetch(*ppOpts,"default",(int)sizeof("default")-1); }` |
|     22 | 5718 | `		}else{` |
|     48 | 5719 | `			*piFlags = ph7_value_to_int(apArg[iBase+1]);` |
|      - | 5720 | `		}` |
|     43 | 5721 | `	}` |
|    331 | 5722 | `}` |
|      - | 5723 | `/*` |
|      - | 5724 | ` * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 5725 | ` *  Validate or sanitize a value; see FvApplyFilter for the failure semantics.` |
|      - | 5726 | ` */` |
|    306 | 5727 | `static int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5728 | `{` |
|    308 | 5729 | `	int iFilter = FV_DEFAULT, iFlags = 0;` |
|    308 | 5730 | `	ph7_value *pOpts = 0, *pDefault = 0;` |
|    308 | 5731 | `	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    308 | 5732 | `	FvParseFilterArgs(nArg,apArg,1,&iFilter,&iFlags,&pOpts,&pDefault);` |
|    308 | 5733 | `	return FvApplyFilter(pCtx,apArg[0],iFilter,iFlags,pOpts,pDefault);` |
|    155 | 5734 | `}` |
|      - | 5735 | `/*` |
|      - | 5736 | ` * filter_input($type, $var_name, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 5737 | ` *  Look up $var_name in the requested INPUT_* superglobal, then apply the` |
|      - | 5738 | ` *  filter. Semantics verified byte-for-byte against php 8.5:` |
|      - | 5739 | ` *   - variable NOT set: 'default' option wins, else false when` |
|      - | 5740 | ` *     FILTER_NULL_ON_FAILURE is set, else null. (Note the null/false roles are` |
|      - | 5741 | ` *     INVERTED relative to a present value that fails validation, which yields` |
|      - | 5742 | ` *     default > null-if-NULL_ON_FAILURE > false via FvApplyFilter.)` |
|      - | 5743 | ` *   - variable present: delegate to FvApplyFilter.` |
|      - | 5744 | ` *  Divergence: php reads a SAPI snapshot of the original request variables` |
|      - | 5745 | ` *  captured at startup; PHL reads the live superglobal. In CLI they match for` |
|      - | 5746 | ` *  the SAPI-registered keys (SCRIPT_NAME/PHP_SELF/DOCUMENT_ROOT); keys added` |
|      - | 5747 | ` *  only to the live $_SERVER (REQUEST_TIME/PWD/…) are visible here but not in` |
|      - | 5748 | ` *  php's snapshot.` |
|      - | 5749 | ` */` |
|     28 | 5750 | `static int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5751 | `{` |
|     30 | 5752 | `	int iType, iFilter = FV_DEFAULT, iFlags = 0;` |
|     30 | 5753 | `	ph7_value *pOpts = 0, *pDefault = 0, *pSuper, *pElem;` |
|      - | 5754 | `	const char *zVar, *zSuper; int nVar; sxu32 nSuper;` |
|     30 | 5755 | `	if( nArg<2 ){` |
|      7 | 5756 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|      2 | 5757 | `			"filter_input() expects at least 2 arguments, %d given",nArg);` |
|      - | 5758 | `	}` |
|     26 | 5759 | `	iType = ph7_value_to_int(apArg[0]);` |
|     26 | 5760 | `	switch( iType ){` |
|      3 | 5761 | `	case 0: zSuper = "_POST";   nSuper = (sxu32)sizeof("_POST")-1;   break; /* INPUT_POST */` |
|      3 | 5762 | `	case 1: zSuper = "_GET";    nSuper = (sxu32)sizeof("_GET")-1;    break; /* INPUT_GET */` |
|    ! 0 | 5763 | `	case 2: zSuper = "_COOKIE"; nSuper = (sxu32)sizeof("_COOKIE")-1; break; /* INPUT_COOKIE */` |
|    ! 0 | 5764 | `	case 4: zSuper = "_ENV";    nSuper = (sxu32)sizeof("_ENV")-1;    break; /* INPUT_ENV */` |
|     19 | 5765 | `	case 5: zSuper = "_SERVER"; nSuper = (sxu32)sizeof("_SERVER")-1; break; /* INPUT_SERVER */` |
|      1 | 5766 | `	default:` |
|      3 | 5767 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5768 | `			"filter_input(): Argument #1 ($type) must be an INPUT_* constant");` |
|      - | 5769 | `	}` |
|     23 | 5770 | `	zVar = ph7_value_to_string(apArg[1],&nVar);` |
|     23 | 5771 | `	FvParseFilterArgs(nArg,apArg,2,&iFilter,&iFlags,&pOpts,&pDefault);` |
|      - | 5772 | `	/* Resolve the variable from the superglobal (missing/non-array -> not set). */` |
|     23 | 5773 | `	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);` |
|     23 | 5774 | `	pElem = (pSuper && ph7_value_is_array(pSuper))` |
|     33 | 5775 | `		? ph7_array_fetch(pSuper,zVar,nVar) : 0;` |
|     23 | 5776 | `	if( pElem==0 ){` |
|      - | 5777 | `		/* Variable not set: default > false(if NULL_ON_FAILURE) > null. Note the` |
|      - | 5778 | `		 * false/null roles are inverted vs FvApplyFilter's present-but-fails path. */` |
|     13 | 5779 | `		if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|      9 | 5780 | `		else if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_bool(pCtx,0); }` |
|      7 | 5781 | `		else { ph7_result_null(pCtx); }` |
|     13 | 5782 | `		return PH7_OK;` |
|      - | 5783 | `	}` |
|     11 | 5784 | `	return FvApplyFilter(pCtx,pElem,iFilter,iFlags,pOpts,pDefault);` |
|     16 | 5785 | `}` |
|      - | 5786 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 5787 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 5788 | `/*` |
|      - | 5789 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 5790 |  |
|      - | 5791 | ` */` |
|      4 | 5792 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 5793 | `	const char *zInput, /* Raw input */` |
|      - | 5794 | `	int nByte,  /* Input length */` |
|      - | 5795 | `	int delim,  /* Delimiter */` |
|      - | 5796 | `	int encl,   /* Enclosure */` |
|      - | 5797 | `	int escape,  /* Escape character */` |
|      - | 5798 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 5799 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 5800 | `	)` |
|      1 | 5801 | `{` |
|      5 | 5802 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 5803 | `	const char *zIn = zInput;` |
|      - | 5804 | `	const char *zPtr;` |
|      - | 5805 | `	int isEnc;` |
|      - | 5806 | `	/* Start processing */` |
|      8 | 5807 | `	for(;;){` |
|     17 | 5808 | `		if( zIn >= zEnd ){` |
|      - | 5809 | `			/* No more input to process */` |
|      5 | 5810 | `			break;` |
|      - | 5811 | `		}` |
|     13 | 5812 | `		isEnc = 0;` |
|     13 | 5813 | `		zPtr = zIn;` |
|      - | 5814 | `		/* Find the first delimiter */` |
|     27 | 5815 | `		while( zIn < zEnd ){` |
|     23 | 5816 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 5817 | `				/* Delimiter found,break imediately */` |
|      5 | 5818 | `				break;` |
|     15 | 5819 | `			}else if( zIn[0] == encl ){` |
|      - | 5820 | `				/* Inside enclosure? */` |
|    ! 0 | 5821 | `				isEnc = !isEnc;` |
|     15 | 5822 | `			}else if( zIn[0] == escape ){` |
|      - | 5823 | `				/* Escape sequence */` |
|    ! 0 | 5824 | `				zIn++;` |
|    ! 0 | 5825 | `			}` |
|      - | 5826 | `			/* Advance the cursor */` |
|     15 | 5827 | `			zIn++;` |
|      1 | 5828 | `		}` |
|     13 | 5829 | `		if( zIn > zPtr ){` |
|     13 | 5830 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 5831 | `			sxi32 rc;` |
|      - | 5832 | `			/* Invoke the supllied callback */` |
|     13 | 5833 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 5834 | `				zPtr++;` |
|    ! 0 | 5835 | `				nByteChunk-=2;` |
|    ! 0 | 5836 | `			}` |
|     13 | 5837 | `			if( nByteChunk > 0 ){` |
|     13 | 5838 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 5839 | `				if( rc == SXERR_ABORT ){` |
|      - | 5840 | `					/* User callback request an operation abort */` |
|    ! 0 | 5841 | `					break;` |
|      - | 5842 | `				}` |
|      6 | 5843 | `			}` |
|      6 | 5844 | `		}` |
|      - | 5845 | `		/* Ignore trailing delimiter */` |
|     21 | 5846 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 5847 | `			zIn++;` |
|      1 | 5848 | `		}` |
|      1 | 5849 | `	}` |
|      5 | 5850 | `	return SXRET_OK;` |
|      1 | 5851 | `}` |
|      - | 5852 | `/*` |
|      - | 5853 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 5854 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 5855 | ` * argument to this callback.` |
|      - | 5856 | ` */` |
|     12 | 5857 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 5858 | `{` |
|     13 | 5859 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 5860 | `	ph7_value sEntry;` |
|      - | 5861 | `	SyString sToken;` |
|      - | 5862 | `	/* Insert the token in the given array */` |
|     13 | 5863 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 5864 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 5865 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 5866 | `	if( sToken.nByte < 1){` |
|    ! 0 | 5867 | `		return SXRET_OK;` |
|      - | 5868 | `	}` |
|     13 | 5869 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 5870 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 5871 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 5872 | `	return SXRET_OK;` |
|      7 | 5873 | `}` |
|      - | 5874 | `/*` |
|      - | 5875 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 5876 | ` *  Parse a CSV string into an array.` |
|      - | 5877 | ` * Parameters` |
|      - | 5878 | ` *  $input` |
|      - | 5879 | ` *   The string to parse.` |
|      - | 5880 | ` *  $delimiter` |
|      - | 5881 | ` *   Set the field delimiter (one character only).` |
|      - | 5882 | ` *  $enclosure` |
|      - | 5883 | ` *   Set the field enclosure character (one character only).` |
|      - | 5884 | ` *  $escape` |
|      - | 5885 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 5886 | ` * Return` |
|      - | 5887 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 5888 | ` */` |
|      2 | 5889 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5890 | `{` |
|      - | 5891 | `	const char *zInput,*zPtr;` |
|      - | 5892 | `	ph7_value *pArray;` |
|      3 | 5893 | `	int delim  = ',';   /* Delimiter */` |
|      3 | 5894 | `	int encl   = '"' ;  /* Enclosure */` |
|      3 | 5895 | `	int escape = '\\';  /* Escape character */` |
|      - | 5896 | `	int nLen;` |
|      3 | 5897 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5898 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 5899 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5900 | `		return PH7_OK;` |
|      - | 5901 | `	}` |
|      - | 5902 | `	/* Extract the raw input */` |
|      3 | 5903 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5904 | `	if( nArg > 1 ){` |
|      - | 5905 | `		int i;` |
|      3 | 5906 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 5907 | `			/* Extract the delimiter */` |
|      3 | 5908 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 5909 | `			if( i > 0 ){` |
|      3 | 5910 | `				delim = zPtr[0];` |
|      1 | 5911 | `			}` |
|      1 | 5912 | `		}` |
|      3 | 5913 | `		if( nArg > 2 ){` |
|      3 | 5914 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 5915 | `				/* Extract the enclosure */` |
|      3 | 5916 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 5917 | `				if( i > 0 ){` |
|      3 | 5918 | `					encl = zPtr[0];` |
|      1 | 5919 | `				}` |
|      1 | 5920 | `			}` |
|      3 | 5921 | `			if( nArg > 3 ){` |
|      3 | 5922 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 5923 | `					/* Extract the escape character */` |
|      3 | 5924 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 5925 | `					if( i > 0 ){` |
|      3 | 5926 | `						escape = zPtr[0];` |
|      1 | 5927 | `					}` |
|      1 | 5928 | `				}` |
|      1 | 5929 | `			}` |
|      1 | 5930 | `		}` |
|      1 | 5931 | `	}` |
|      - | 5932 | `	/* Create our array */` |
|      3 | 5933 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5934 | `	if( pArray == 0 ){` |
|      - | 5935 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|    ! 0 | 5936 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5937 | `	}` |
|      - | 5938 | `	/* Parse the raw input */` |
|      3 | 5939 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 5940 | `	/* Return the freshly created array */` |
|      3 | 5941 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5942 | `	return PH7_OK;` |
|      2 | 5943 | `}` |
|      - | 5944 | `/*` |
|      - | 5945 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 5946 | ` * container.` |
|      - | 5947 | ` * Refer to [strip_tags()].` |
|      - | 5948 | ` */` |
|     10 | 5949 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5950 | `{` |
|     11 | 5951 | `	const char *zEnd = &zTag[nByte];` |
|      - | 5952 | `	const char *zPtr;` |
|      - | 5953 | `	SyString sEntry;` |
|      - | 5954 | `	/* Strip tags */` |
|     10 | 5955 | `	for(;;){` |
|     45 | 5956 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 5957 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 5958 | `				zTag++;` |
|      1 | 5959 | `		}` |
|     21 | 5960 | `		if( zTag >= zEnd ){` |
|     11 | 5961 | `			break;` |
|      - | 5962 | `		}` |
|     11 | 5963 | `		zPtr = zTag;` |
|      - | 5964 | `		/* Delimit the tag */` |
|     25 | 5965 | `		while(zTag < zEnd ){` |
|     25 | 5966 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5967 | `				/* UTF-8 stream */` |
|      3 | 5968 | `				zTag++;` |
|      5 | 5969 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 5970 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 5971 | `				break;` |
|    ! 0 | 5972 | `			}else{` |
|     13 | 5973 | `				zTag++;` |
|      - | 5974 | `			}` |
|      1 | 5975 | `		}` |
|     11 | 5976 | `		if( zTag > zPtr ){` |
|      - | 5977 | `			/* Perform the insertion */` |
|     11 | 5978 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 5979 | `			SyStringFullTrim(&sEntry);` |
|     11 | 5980 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 5981 | `		}` |
|      - | 5982 | `		/* Jump the trailing '>' */` |
|     11 | 5983 | `		zTag++;` |
|      1 | 5984 | `	}` |
|     11 | 5985 | `	return SXRET_OK;` |
|      1 | 5986 | `}` |
|      - | 5987 | `/*` |
|      - | 5988 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 5989 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 5990 | ` * Refer to [strip_tags()].` |
|      - | 5991 | ` */` |
|     36 | 5992 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5993 | `{` |
|     37 | 5994 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 5995 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 5996 | `		SyString sTag;` |
|     85 | 5997 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 5998 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 5999 | `			zTag++;` |
|      1 | 6000 | `		}` |
|      - | 6001 | `		/* Delimit the tag */` |
|     25 | 6002 | `		zCur = zTag;` |
|     77 | 6003 | `		while(zTag < zEnd ){` |
|     77 | 6004 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 6005 | `				/* UTF-8 stream */` |
|      5 | 6006 | `				zTag++;` |
|      9 | 6007 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 6008 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 6009 | `				break;` |
|    ! 0 | 6010 | `			}else{` |
|     49 | 6011 | `				zTag++;` |
|      - | 6012 | `			}` |
|      1 | 6013 | `		}` |
|     25 | 6014 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 6015 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 6016 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 6017 | `		if( sTag.nByte > 0 ){` |
|      - | 6018 | `			SyString *aEntry,*pEntry;` |
|      - | 6019 | `			sxi32 rc;` |
|      - | 6020 | `			sxu32 n;` |
|      - | 6021 | `			/* Perform the lookup */` |
|     25 | 6022 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 6023 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 6024 | `				pEntry = &aEntry[n];` |
|      - | 6025 | `				/* Do the comparison */` |
|     25 | 6026 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 6027 | `				if( !rc ){` |
|     21 | 6028 | `					return SXRET_OK;` |
|      - | 6029 | `				}` |
|      3 | 6030 | `			}` |
|      2 | 6031 | `		}` |
|      2 | 6032 | `	}` |
|      - | 6033 | `	/* No such tag */` |
|     17 | 6034 | `	return SXERR_NOTFOUND;` |
|     19 | 6035 | `}` |
|      - | 6036 | `/*` |
|      - | 6037 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 6038 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 6039 | ` * Refer to [strip_tags()].` |
|      - | 6040 | ` */` |
|     16 | 6041 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 6042 | `{` |
|     17 | 6043 | `	const char *zEnd = &zIn[nByte];` |
|      - | 6044 | `	const char *zPtr,*zTag;` |
|      - | 6045 | `	SySet sSet;` |
|      - | 6046 | `	/* initialize the set of allowed tags */` |
|     17 | 6047 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 6048 | `	if( nTaglen > 0 ){` |
|      - | 6049 | `		/* Set of allowed tags */` |
|     11 | 6050 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 6051 | `	}` |
|      - | 6052 | `	/* Set the empty string */` |
|     17 | 6053 | `	ph7_result_string(pCtx,"",0);` |
|      - | 6054 | `	/* Start processing */` |
|     26 | 6055 | `	for(;;){` |
|     53 | 6056 | `		if(zIn >= zEnd){` |
|      - | 6057 | `			/* No more input to process */` |
|     15 | 6058 | `			break;` |
|      - | 6059 | `		}` |
|     39 | 6060 | `		zPtr = zIn;` |
|      - | 6061 | `		/* Find a tag */` |
|    133 | 6062 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 6063 | `			zIn++;` |
|      1 | 6064 | `		}` |
|     39 | 6065 | `		if( zIn > zPtr ){` |
|      - | 6066 | `			/* Consume raw input */` |
|     21 | 6067 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 6068 | `		}` |
|      - | 6069 | `		/* Ignore trailing null bytes */` |
|     39 | 6070 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 6071 | `			zIn++;` |
|    ! 0 | 6072 | `		}` |
|     39 | 6073 | `		if(zIn >= zEnd){` |
|      - | 6074 | `			/* No more input to process */` |
|      3 | 6075 | `			break;` |
|      - | 6076 | `		}` |
|     37 | 6077 | `		if( zIn[0] == '<' ){` |
|      - | 6078 | `			sxi32 rc;` |
|     37 | 6079 | `			zTag = zIn++;` |
|      - | 6080 | `			/* Delimit the tag */` |
|    127 | 6081 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 6082 | `				zIn++;` |
|      1 | 6083 | `			}` |
|     37 | 6084 | `			if( zIn < zEnd ){` |
|     37 | 6085 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 6086 | `			}` |
|      - | 6087 | `			/* Query the set */` |
|     37 | 6088 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 6089 | `			if( rc == SXRET_OK ){` |
|      - | 6090 | `				/* Keep the tag */` |
|     21 | 6091 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 6092 | `			}` |
|     18 | 6093 | `		}` |
|      1 | 6094 | `	}` |
|      - | 6095 | `	/* Cleanup */` |
|     17 | 6096 | `	SySetRelease(&sSet);` |
|     17 | 6097 | `	return SXRET_OK;` |
|      1 | 6098 | `}` |
|      - | 6099 | `/*` |
|      - | 6100 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 6101 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 6102 | ` * Parameters` |
|      - | 6103 | ` *  $str` |
|      - | 6104 | ` *  The input string.` |
|      - | 6105 | ` * $allowable_tags` |
|      - | 6106 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 6107 | ` * Return` |
|      - | 6108 | ` *  Returns the stripped string.` |
|      - | 6109 | ` */` |
|     14 | 6110 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6111 | `{` |
|     15 | 6112 | `	const char *zTaglist = 0;` |
|      - | 6113 | `	const char *zString;` |
|     15 | 6114 | `	int nTaglen = 0;` |
|      - | 6115 | `	int nLen;` |
|     15 | 6116 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6117 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 6118 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 6119 | `		return PH7_OK;` |
|      - | 6120 | `	}` |
|      - | 6121 | `	/* Point to the raw string */` |
|     15 | 6122 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 6123 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 6124 | `		/* Allowed tag */` |
|     11 | 6125 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 6126 | `	}` |
|      - | 6127 | `	/* Process input */` |
|     15 | 6128 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 6129 | `	return PH7_OK;` |
|      8 | 6130 | `}` |
|      - | 6131 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6132 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6133 | `/*` |
|      - | 6134 | ` * string str_shuffle(string $str)` |
|      - | 6135 |  |
|      - | 6136 | ` *  Randomly shuffles a string.` |
|      - | 6137 | ` * Parameters` |
|      - | 6138 | ` *  $str` |
|      - | 6139 | ` *   The input string.` |
|      - | 6140 | ` * Return` |
|      - | 6141 | ` *  Returns the shuffled string.` |
|      - | 6142 | ` */` |
|     10 | 6143 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6144 | `{` |
|      - | 6145 | `	const char *zString;` |
|      - | 6146 | `	int nLen,i,c;` |
|      - | 6147 | `	sxu32 iR;` |
|     11 | 6148 | `	if( nArg < 1 ){` |
|      - | 6149 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 6150 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 6151 | `		return PH7_OK;` |
|      - | 6152 | `	}` |
|      - | 6153 | `	/* Extract the target string */` |
|     11 | 6154 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 6155 | `	if( nLen < 1 ){` |
|      - | 6156 | `		/* Nothing to shuffle */` |
|      3 | 6157 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 6158 | `		return PH7_OK;` |
|      - | 6159 | `	}` |
|      - | 6160 | `	/* Shuffle the string */` |
|     43 | 6161 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 6162 | `		/* Generate a random number first */` |
|     35 | 6163 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 6164 | `		/* Extract a random offset */` |
|     35 | 6165 | `		c = zString[iR % nLen];` |
|      - | 6166 | `		/* Append it */` |
|     35 | 6167 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 6168 | `	}` |
|      9 | 6169 | `	return PH7_OK;` |
|      6 | 6170 | `}` |
|      - | 6171 | `/*` |
|      - | 6172 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 6173 | ` *  Convert a string to an array.` |
|      - | 6174 | ` * Parameters` |
|      - | 6175 | ` * $string` |
|      - | 6176 | ` *  The input string.` |
|      - | 6177 | ` * $split_length` |
|      - | 6178 | ` *  Maximum length of the chunk.` |
|      - | 6179 | ` * Return` |
|      - | 6180 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 6181 | ` *  except possibly the last one which may be shorter.` |
|      - | 6182 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 6183 | ` *  as the first (and only) array element.` |
|      - | 6184 | ` *  An empty string returns an empty array.` |
|      - | 6185 | ` * Errors` |
|      - | 6186 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 6187 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 6188 | ` *  ValueError if $split_length is less than 1.` |
|      - | 6189 | ` */` |
|     28 | 6190 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 6191 | `{` |
|      - | 6192 | `	const char *zString,*zEnd;` |
|      - | 6193 | `	ph7_value *pArray,*pValue;` |
|      - | 6194 | `	int split_len;` |
|      - | 6195 | `	int nLen;` |
|     33 | 6196 | `	if( nArg < 1 ){` |
|      4 | 6197 | `		return PH7_VmThrowException(pCtx,` |
|      - | 6198 | `			"ArgumentCountError",` |
|      - | 6199 | `			"str_split() expects at least 1 argument, %d given",` |
|      1 | 6200 | `			nArg` |
|      - | 6201 | `			);` |
|      - | 6202 | `	}` |
|      - | 6203 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     38 | 6204 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     40 | 6205 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 6206 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 6207 | `		return PH7_VmThrowException(pCtx,` |
|      - | 6208 | `			"TypeError",` |
|      - | 6209 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 6210 | `			ph7_type_name(apArg[0])` |
|      - | 6211 | `			);` |
|      - | 6212 | `	}` |
|      - | 6213 | `	/* Point to the target string */` |
|     27 | 6214 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 6215 | `	split_len = (int)sizeof(char);` |
|     27 | 6216 | `	if( nArg > 1 ){` |
|      - | 6217 | `		/* Split length */` |
|     17 | 6218 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 6219 | `		if( split_len < 1 ){` |
|      6 | 6220 | `			return PH7_VmThrowException(pCtx,` |
|      - | 6221 | `				"ValueError",` |
|      - | 6222 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 6223 | `				);` |
|      - | 6224 | `		}` |
|     11 | 6225 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 6226 | `			split_len = nLen;` |
|      1 | 6227 | `		}` |
|      5 | 6228 | `	}` |
|      - | 6229 | `	/* Create the array and the scalar value */` |
|     21 | 6230 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 6231 | `	/*Chunk value */` |
|     21 | 6232 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 6233 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 6234 | `		/* Return FALSE */` |
|    ! 0 | 6235 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6236 | `		return PH7_OK;` |
|      - | 6237 | `	}` |
|      - | 6238 | `	/* Point to the end of the string */` |
|     21 | 6239 | `	zEnd = &zString[nLen];` |
|      - | 6240 | `	/* Perform the requested operation */` |
|     48 | 6241 | `	for(;;){` |
|      - | 6242 | `		int nMax;` |
|     59 | 6243 | `		if( zString >= zEnd ){` |
|      - | 6244 | `			/* No more input to process */` |
|     21 | 6245 | `			break;` |
|      - | 6246 | `		}` |
|     39 | 6247 | `		nMax = (int)(zEnd-zString);` |
|     39 | 6248 | `		if( nMax < split_len ){` |
|      3 | 6249 | `			split_len = nMax;` |
|      1 | 6250 | `		}` |
|      - | 6251 | `		/* Copy the current chunk */` |
|     39 | 6252 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 6253 | `		/* Insert it */` |
|     39 | 6254 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 6255 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6256 | `		}` |
|      - | 6257 | `		/* reset the string cursor */` |
|     39 | 6258 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 6259 | `		/* Update position */` |
|     39 | 6260 | `		zString += split_len;` |
|      1 | 6261 | `	}` |
|      - | 6262 | `	/*` |
|      - | 6263 | `	 * Return the array.` |
|      - | 6264 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 6265 | `	 * upon we return from this function.` |
|      - | 6266 | `	 */` |
|     21 | 6267 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 6268 | `	return PH7_OK;` |
|     19 | 6269 | `}` |
|      - | 6270 | `/*` |
|      - | 6271 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 6272 | ` * Refer to [strspn()].` |
|      - | 6273 | ` */` |
|     28 | 6274 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 6275 | `{` |
|     29 | 6276 | `	const char *zIn = *pzIn;` |
|      - | 6277 | `	const char *zPtr;` |
|      - | 6278 | `	/* Ignore leading white spaces */` |
|     29 | 6279 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 6280 | `		zIn++;` |
|    ! 0 | 6281 | `	}` |
|     29 | 6282 | `	if( zIn >= zEnd ){` |
|      - | 6283 | `		/* End of input */` |
|    ! 0 | 6284 | `		return SXERR_EOF;` |
|      - | 6285 | `	}` |
|     29 | 6286 | `	zPtr = zIn;` |
|      - | 6287 | `	/* Extract the token */` |
|    201 | 6288 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 6289 | `		zIn++;` |
|      1 | 6290 | `	}` |
|     29 | 6291 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 6292 | `	/* Synchronize pointers */` |
|     29 | 6293 | `	*pzIn = zIn;` |
|      - | 6294 | `	/* Return to the caller */` |
|     29 | 6295 | `	return SXRET_OK;` |
|     15 | 6296 | `}` |
|      - | 6297 | `/*` |
|      - | 6298 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 6299 | ` * return the longest match.` |
|      - | 6300 | ` * Refer to [strspn()].` |
|      - | 6301 | ` */` |
|     18 | 6302 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 6303 | `{` |
|     19 | 6304 | `	const char *zEnd = &zString[nLen];` |
|     19 | 6305 | `	const char *zIn = zString;` |
|      - | 6306 | `	int i,c;` |
|     45 | 6307 | `	for(;;){` |
|     91 | 6308 | `		if( zString >= zEnd ){` |
|      7 | 6309 | `			break;` |
|      - | 6310 | `		}` |
|      - | 6311 | `		/* Extract current character */` |
|     85 | 6312 | `		c = zString[0];` |
|      - | 6313 | `		/* Perform the lookup */` |
|    383 | 6314 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 6315 | `			if( c == zMask[i] ){` |
|      - | 6316 | `				/* Character found */` |
|     73 | 6317 | `				break;` |
|      - | 6318 | `			}` |
|    150 | 6319 | `		}` |
|     85 | 6320 | `		if( i >= nMaskLen ){` |
|      - | 6321 | `			/* Character not in the current mask,break immediately */` |
|     13 | 6322 | `			break;` |
|      - | 6323 | `		}` |
|      - | 6324 | `		/* Advance cursor */` |
|     73 | 6325 | `		zString++;` |
|      1 | 6326 | `	}` |
|      - | 6327 | `	/* Longest match */` |
|     19 | 6328 | `	return (int)(zString-zIn);` |
|      1 | 6329 | `}` |
|      - | 6330 | `/*` |
|      - | 6331 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 6332 | ` * Refer to [strcspn()].` |
|      - | 6333 | ` */` |
|     10 | 6334 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 6335 | `{` |
|     11 | 6336 | `	const char *zEnd = &zString[nLen];` |
|     11 | 6337 | `	const char *zIn = zString;` |
|      - | 6338 | `	int i,c;` |
|     12 | 6339 | `	for(;;){` |
|     25 | 6340 | `		if( zString >= zEnd ){` |
|      3 | 6341 | `			break;` |
|      - | 6342 | `		}` |
|      - | 6343 | `		/* Extract current character */` |
|     23 | 6344 | `		c = zString[0];` |
|      - | 6345 | `		/* Perform the lookup */` |
|     51 | 6346 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 6347 | `			if( c == zMask[i] ){` |
|      9 | 6348 | `				break;` |
|      - | 6349 | `			}` |
|     15 | 6350 | `		}` |
|     23 | 6351 | `		if( i < nMaskLen ){` |
|      - | 6352 | `			/* Character in the current mask,break immediately */` |
|      9 | 6353 | `			break;` |
|      - | 6354 | `		}` |
|      - | 6355 | `		/* Advance cursor */` |
|     15 | 6356 | `		zString++;` |
|      1 | 6357 | `	}` |
|      - | 6358 | `	/* Longest match */` |
|     11 | 6359 | `	return (int)(zString-zIn);` |
|      1 | 6360 | `}` |
|      - | 6361 | `/*` |
|      - | 6362 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 6363 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 6364 | ` *  of characters contained within a given mask.` |
|      - | 6365 | ` * Parameters` |
|      - | 6366 | ` * $str` |
|      - | 6367 | ` *  The input string.` |
|      - | 6368 | ` * $mask` |
|      - | 6369 | ` *  The list of allowable characters.` |
|      - | 6370 | ` * $start` |
|      - | 6371 | ` *  The position in subject to start searching.` |
|      - | 6372 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 6373 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 6374 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 6375 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 6376 | ` *  start'th position from the end of subject.` |
|      - | 6377 | ` * $length` |
|      - | 6378 | ` *  The length of the segment from subject to examine.` |
|      - | 6379 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 6380 | ` *  characters after the starting position.` |
|      - | 6381 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 6382 | ` *  position up to length characters from the end of subject.` |
|      - | 6383 | ` * Return` |
|      - | 6384 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 6385 | ` * in mask.` |
|      - | 6386 | ` */` |
|     24 | 6387 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6388 | `{` |
|      - | 6389 | `	const char *zString,*zMask,*zEnd;` |
|      - | 6390 | `	int iMasklen,iLen;` |
|      - | 6391 | `	SyString sToken;` |
|     25 | 6392 | `	int iCount = 0;` |
|      - | 6393 | `	int rc;` |
|     25 | 6394 | `	if( nArg < 2 ){` |
|      - | 6395 | `		/* Missing agruments,return zero */` |
|    ! 0 | 6396 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 6397 | `		return PH7_OK;` |
|      - | 6398 | `	}` |
|      - | 6399 | `	/* Extract the target string */` |
|     25 | 6400 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6401 | `	/* Extract the mask */` |
|     25 | 6402 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 6403 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 6404 | `		/* Nothing to process,return zero */` |
|      7 | 6405 | `		ph7_result_int(pCtx,0);` |
|      7 | 6406 | `		return PH7_OK;` |
|      - | 6407 | `	}` |
|     19 | 6408 | `	if( nArg > 2 ){` |
|      - | 6409 | `		int nOfft;` |
|      - | 6410 | `		/* Extract the offset */` |
|      9 | 6411 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 6412 | `		if( nOfft < 0 ){` |
|    ! 0 | 6413 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 6414 | `			if( zBase > zString ){` |
|    ! 0 | 6415 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 6416 | `				zString = zBase;` |
|    ! 0 | 6417 | `			}else{` |
|      - | 6418 | `				/* Invalid offset */` |
|    ! 0 | 6419 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 6420 | `				return PH7_OK;` |
|      - | 6421 | `			}` |
|    ! 0 | 6422 | `		}else{` |
|      9 | 6423 | `			if( nOfft >= iLen ){` |
|      - | 6424 | `				/* Invalid offset */` |
|    ! 0 | 6425 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 6426 | `				return PH7_OK;` |
|    ! 0 | 6427 | `			}else{` |
|      - | 6428 | `				/* Update offset */` |
|      9 | 6429 | `				zString += nOfft;` |
|      9 | 6430 | `				iLen -= nOfft;` |
|      - | 6431 | `			}` |
|      - | 6432 | `		}` |
|      9 | 6433 | `		if( nArg > 3 ){` |
|      - | 6434 | `			int iUserlen;` |
|      - | 6435 | `			/* Extract the desired length */` |
|      9 | 6436 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 6437 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 6438 | `				iLen = iUserlen;` |
|      2 | 6439 | `			}` |
|      4 | 6440 | `		}` |
|      4 | 6441 | `	}` |
|      - | 6442 | `	/* Point to the end of the string */` |
|     19 | 6443 | `	zEnd = &zString[iLen];` |
|      - | 6444 | `	/* Extract the first non-space token */` |
|     19 | 6445 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 6446 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 6447 | `		/* Compare against the current mask */` |
|     19 | 6448 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 6449 | `	}` |
|      - | 6450 | `	/* Longest match */` |
|     19 | 6451 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 6452 | `	return PH7_OK;` |
|     13 | 6453 | `}` |
|      - | 6454 | `/*` |
|      - | 6455 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 6456 | ` *  Find length of initial segment not matching mask.` |
|      - | 6457 | ` * Parameters` |
|      - | 6458 | ` * $str` |
|      - | 6459 | ` *  The input string.` |
|      - | 6460 | ` * $mask` |
|      - | 6461 | ` *  The list of not allowed characters.` |
|      - | 6462 | ` * $start` |
|      - | 6463 | ` *  The position in subject to start searching.` |
|      - | 6464 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 6465 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 6466 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 6467 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 6468 | ` *  start'th position from the end of subject.` |
|      - | 6469 | ` * $length` |
|      - | 6470 | ` *  The length of the segment from subject to examine.` |
|      - | 6471 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 6472 | ` *  characters after the starting position.` |
|      - | 6473 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 6474 | ` *  position up to length characters from the end of subject.` |
|      - | 6475 | ` * Return` |
|      - | 6476 | ` *  Returns the length of the segment as an integer.` |
|      - | 6477 | ` */` |
|     14 | 6478 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6479 | `{` |
|      - | 6480 | `	const char *zString,*zMask,*zEnd;` |
|      - | 6481 | `	int iMasklen,iLen;` |
|      - | 6482 | `	SyString sToken;` |
|     15 | 6483 | `	int iCount = 0;` |
|      - | 6484 | `	int rc;` |
|     15 | 6485 | `	if( nArg < 2 ){` |
|      - | 6486 | `		/* Missing agruments,return zero */` |
|    ! 0 | 6487 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 6488 | `		return PH7_OK;` |
|      - | 6489 | `	}` |
|      - | 6490 | `	/* Extract the target string */` |
|     15 | 6491 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6492 | `	/* Extract the mask */` |
|     15 | 6493 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 6494 | `	if( iLen < 1 ){` |
|      - | 6495 | `		/* Nothing to process,return zero */` |
|    ! 0 | 6496 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 6497 | `		return PH7_OK;` |
|      - | 6498 | `	}` |
|     15 | 6499 | `	if( iMasklen < 1 ){` |
|      - | 6500 | `		/* No given mask,return the string length */` |
|      3 | 6501 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 6502 | `		return PH7_OK;` |
|      - | 6503 | `	}` |
|     13 | 6504 | `	if( nArg > 2 ){` |
|      - | 6505 | `		int nOfft;` |
|      - | 6506 | `		/* Extract the offset */` |
|     11 | 6507 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 6508 | `		if( nOfft < 0 ){` |
|    ! 0 | 6509 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 6510 | `			if( zBase > zString ){` |
|    ! 0 | 6511 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 6512 | `				zString = zBase;` |
|    ! 0 | 6513 | `			}else{` |
|      - | 6514 | `				/* Invalid offset */` |
|    ! 0 | 6515 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 6516 | `				return PH7_OK;` |
|      - | 6517 | `			}` |
|    ! 0 | 6518 | `		}else{` |
|     11 | 6519 | `			if( nOfft >= iLen ){` |
|      - | 6520 | `				/* Invalid offset */` |
|      3 | 6521 | `				ph7_result_int(pCtx,0);` |
|      3 | 6522 | `				return PH7_OK;` |
|    ! 0 | 6523 | `			}else{` |
|      - | 6524 | `				/* Update offset */` |
|      9 | 6525 | `				zString += nOfft;` |
|      9 | 6526 | `				iLen -= nOfft;` |
|      - | 6527 | `			}` |
|      - | 6528 | `		}` |
|      9 | 6529 | `		if( nArg > 3 ){` |
|      - | 6530 | `			int iUserlen;` |
|      - | 6531 | `			/* Extract the desired length */` |
|    ! 0 | 6532 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 6533 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 6534 | `				iLen = iUserlen;` |
|    ! 0 | 6535 | `			}` |
|    ! 0 | 6536 | `		}` |
|      4 | 6537 | `	}` |
|      - | 6538 | `	/* Point to the end of the string */` |
|     11 | 6539 | `	zEnd = &zString[iLen];` |
|      - | 6540 | `	/* Extract the first non-space token */` |
|     11 | 6541 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 6542 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 6543 | `		/* Compare against the current mask */` |
|     11 | 6544 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 6545 | `	}` |
|      - | 6546 | `	/* Longest match */` |
|     11 | 6547 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 6548 | `	return PH7_OK;` |
|      8 | 6549 | `}` |
|      - | 6550 | `/*` |
|      - | 6551 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 6552 | ` *  Search a string for any of a set of characters.` |
|      - | 6553 | ` * Parameters` |
|      - | 6554 | ` *  $haystack` |
|      - | 6555 | ` *   The string where char_list is looked for.` |
|      - | 6556 | ` *  $char_list` |
|      - | 6557 | ` *   This parameter is case sensitive.` |
|      - | 6558 | ` * Return` |
|      - | 6559 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 6560 | ` */` |
|      4 | 6561 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6562 | `{` |
|      - | 6563 | `	const char *zString,*zList,*zEnd;` |
|      - | 6564 | `	int iLen,iListLen,i,c;` |
|      - | 6565 | `	sxu32 nOfft,nMax;` |
|      - | 6566 | `	sxi32 rc;` |
|      5 | 6567 | `	if( nArg < 2 ){` |
|      - | 6568 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 6569 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6570 | `		return PH7_OK;` |
|      - | 6571 | `	}` |
|      - | 6572 | `	/* Extract the haystack and the char list */` |
|      5 | 6573 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 6574 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 6575 | `	if( iLen < 1 ){` |
|      - | 6576 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 6577 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6578 | `		return PH7_OK;` |
|      - | 6579 | `	}` |
|      - | 6580 | `	/* Point to the end of the string */` |
|      5 | 6581 | `	zEnd = &zString[iLen];` |
|      5 | 6582 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 6583 | `	/* perform the requested operation */` |
|     15 | 6584 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 6585 | `		c = zList[i];` |
|     11 | 6586 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 6587 | `		if( rc == SXRET_OK ){` |
|      5 | 6588 | `			if( nMax < nOfft ){` |
|      3 | 6589 | `				nOfft = nMax;` |
|      1 | 6590 | `			}` |
|      2 | 6591 | `		}` |
|      6 | 6592 | `	}` |
|      5 | 6593 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 6594 | `		/* No such substring,return FALSE */` |
|      3 | 6595 | `		ph7_result_bool(pCtx,0);` |
|      2 | 6596 | `	}else{` |
|      - | 6597 | `		/* Return the substring */` |
|      3 | 6598 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 6599 | `	}` |
|      5 | 6600 | `	return PH7_OK;` |
|      3 | 6601 | `}` |
|      - | 6602 | `/* SPDX-SnippetBegin */` |
|      - | 6603 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 6604 | `/* SPDX-License-Identifier: blessing */` |
|      - | 6605 | `/*` |
|      - | 6606 | ` * string soundex(string $str)` |
|      - | 6607 | ` *  Calculate the soundex key of a string.` |
|      - | 6608 | ` * Parameters` |
|      - | 6609 | ` *  $str` |
|      - | 6610 | ` *   The input string.` |
|      - | 6611 | ` * Return` |
|      - | 6612 | ` *  Returns the soundex key as a string.` |
|      - | 6613 | ` * Note:` |
|      - | 6614 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 6615 | ` * source tree.` |
|      - | 6616 | ` */` |
|     22 | 6617 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6618 | `{` |
|      - | 6619 | `	const unsigned char *zIn;` |
|      - | 6620 | `	char zResult[8];` |
|      - | 6621 | `	int i, j;` |
|      - | 6622 | `	static const unsigned char iCode[] = {` |
|      - | 6623 |  |
|      - | 6624 |  |
|      - | 6625 |  |
|      - | 6626 |  |
|      - | 6627 |  |
|      - | 6628 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 6629 |  |
|      - | 6630 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 6631 | `	};` |
|     23 | 6632 | `	if( nArg < 1 ){` |
|      - | 6633 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 6634 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 6635 | `		return PH7_OK;` |
|      - | 6636 | `	}` |
|     23 | 6637 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     35 | 6638 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     23 | 6639 | `	if( zIn[i] ){` |
|     17 | 6640 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 6641 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 6642 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 6643 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 6644 | `			if( code>0 ){` |
|     45 | 6645 | `				if( code!=prevcode ){` |
|     33 | 6646 | `					prevcode = (unsigned char)code;` |
|     33 | 6647 | `					zResult[j++] = (char)code + '0';` |
|     16 | 6648 | `				}` |
|     23 | 6649 | `			}else{` |
|     49 | 6650 | `				prevcode = 0;` |
|      - | 6651 | `			}` |
|     47 | 6652 | `		}` |
|     33 | 6653 | `		while( j<4 ){` |
|     17 | 6654 | `			zResult[j++] = '0';` |
|      1 | 6655 | `		}` |
|     17 | 6656 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 6657 | `	}else{` |
|      - | 6658 | `	  /* No alphabetic character: PHP returns "0000" (not the SQLite "?000"). */` |
|      7 | 6659 | `	  ph7_result_string(pCtx,"0000",4);` |
|      - | 6660 | `	}` |
|     23 | 6661 | `	return PH7_OK;` |
|     12 | 6662 | `}` |
|      - | 6663 | `/* SPDX-SnippetEnd */` |
|      - | 6664 | `/*` |
|      - | 6665 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 6666 | ` *  Wraps a string to a given number of characters.` |
|      - | 6667 | ` * Parameters` |
|      - | 6668 | ` *  $str` |
|      - | 6669 | ` *   The input string.` |
|      - | 6670 | ` * $width` |
|      - | 6671 | ` *  The column width.` |
|      - | 6672 | ` * $break` |
|      - | 6673 | ` *  The line is broken using the optional break parameter.` |
|      - | 6674 | ` * Return` |
|      - | 6675 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 6676 | ` */` |
|     26 | 6677 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6678 | `{` |
|      - | 6679 | `	const char *zIn,*zBreak;` |
|      - | 6680 | `	SyBlob sWorker;` |
|      - | 6681 | `	int iLen,iBreaklen,iWidth,iCut,iStart,iSpace,iCur;` |
|      - | 6682 | `	sxi32 rc;` |
|     27 | 6683 | `	if( nArg < 1 ){` |
|      - | 6684 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 6685 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 6686 | `		return PH7_OK;` |
|      - | 6687 | `	}` |
|      - | 6688 | `	/* Extract the input string */` |
|     27 | 6689 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6690 | `	/* Width (default 75; PHP allows 0/negative — break at every space). */` |
|     27 | 6691 | `	iWidth = 75;` |
|     27 | 6692 | `	if( nArg > 1 ){` |
|     27 | 6693 | `		iWidth = ph7_value_to_int(apArg[1]);` |
|     13 | 6694 | `	}` |
|      - | 6695 | `	/* Break string (default "\n"). */` |
|     27 | 6696 | `	zBreak = "\n";` |
|     27 | 6697 | `	iBreaklen = (int)sizeof(char);` |
|     27 | 6698 | `	if( nArg > 2 ){` |
|     13 | 6699 | `		zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      6 | 6700 | `	}` |
|      - | 6701 | `	/* Cut long words? (default false). */` |
|     27 | 6702 | `	iCut = 0;` |
|     27 | 6703 | `	if( nArg > 3 ){` |
|      7 | 6704 | `		iCut = ph7_value_to_bool(apArg[3]);` |
|      3 | 6705 | `	}` |
|     27 | 6706 | `	if( iLen < 1 ){` |
|      - | 6707 | `		/* PHP returns the empty string for empty input before validating the other args. */` |
|      5 | 6708 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 6709 | `		return PH7_OK;` |
|      - | 6710 | `	}` |
|      - | 6711 | `	/* PHP 8 domain errors (catchable ValueError). */` |
|     23 | 6712 | `	if( iBreaklen < 1 ){` |
|      3 | 6713 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6714 | `			"wordwrap(): Argument #3 ($break) must not be empty");` |
|      - | 6715 | `	}` |
|     21 | 6716 | `	if( iWidth == 0 && iCut ){` |
|      3 | 6717 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6718 | `			"wordwrap(): Argument #4 ($cut_long_words) cannot be true when argument #2 ($width) is 0");` |
|      - | 6719 | `	}` |
|      - | 6720 | `	/*` |
|      - | 6721 | `	 * PHP's algorithm: a single left-to-right pass tracking the start of the` |
|      - | 6722 | `	 * current line (iStart) and the position of the last space seen on it` |
|      - | 6723 | `	 * (iSpace). A break is emitted when the line reaches the width, at the last` |
|      - | 6724 | `	 * space if there was one, otherwise (only when cut is enabled) hard at the` |
|      - | 6725 | `	 * boundary. An existing break sequence in the input resets the line.` |
|      - | 6726 | `	 */` |
|     19 | 6727 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     19 | 6728 | `	iStart = iSpace = iCur = 0;` |
|     19 | 6729 | `	rc = SXRET_OK;` |
|    551 | 6730 | `	while( iCur < iLen ){` |
|    533 | 6731 | `		if( iBreaklen <= iLen - iCur && SyMemcmp(&zIn[iCur],zBreak,(sxu32)iBreaklen) == 0 ){` |
|      - | 6732 | `			/* Existing break sequence in the input: copy it verbatim and reset the line. */` |
|    ! 0 | 6733 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart + iBreaklen));` |
|    ! 0 | 6734 | `			if( rc != SXRET_OK ){ goto oom; }` |
|    ! 0 | 6735 | `			iCur += iBreaklen;` |
|    ! 0 | 6736 | `			iStart = iSpace = iCur;` |
|    ! 0 | 6737 | `			continue;` |
|    533 | 6738 | `		}else if( zIn[iCur] == ' ' ){` |
|     67 | 6739 | `			if( iCur - iStart >= iWidth ){` |
|      - | 6740 | `				/* The line already fills the width at this space: break here (the space is consumed). */` |
|     13 | 6741 | `				rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     13 | 6742 | `				if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     13 | 6743 | `				if( rc != SXRET_OK ){ goto oom; }` |
|     13 | 6744 | `				iStart = iCur + 1;` |
|      6 | 6745 | `			}` |
|     67 | 6746 | `			iSpace = iCur;` |
|    500 | 6747 | `		}else if( iCut && iCur - iStart >= iWidth && iStart >= iSpace ){` |
|      - | 6748 | `			/* A word longer than the width with no space to break at: hard-cut at the boundary. */` |
|      7 | 6749 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|      7 | 6750 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|      7 | 6751 | `			if( rc != SXRET_OK ){ goto oom; }` |
|      7 | 6752 | `			iStart = iSpace = iCur;` |
|    464 | 6753 | `		}else if( iCur - iStart >= iWidth && iStart < iSpace ){` |
|      - | 6754 | `			/* Past the width mid-word: wrap back to the last space (which is consumed). */` |
|     17 | 6755 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iSpace - iStart));` |
|     17 | 6756 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     17 | 6757 | `			if( rc != SXRET_OK ){ goto oom; }` |
|     17 | 6758 | `			iStart = iSpace = iSpace + 1;` |
|      8 | 6759 | `		}` |
|    533 | 6760 | `		iCur++;` |
|      1 | 6761 | `	}` |
|      - | 6762 | `	/* Emit the trailing chunk. */` |
|     19 | 6763 | `	if( iStart < iCur ){` |
|     19 | 6764 | `		rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     19 | 6765 | `		if( rc != SXRET_OK ){ goto oom; }` |
|      9 | 6766 | `	}` |
|     19 | 6767 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|     19 | 6768 | `	SyBlobRelease(&sWorker);` |
|     19 | 6769 | `	return PH7_OK;` |
|    ! 0 | 6770 | `oom:` |
|    ! 0 | 6771 | `	SyBlobRelease(&sWorker);` |
|    ! 0 | 6772 | `	return PH7_ContextMemoryError(pCtx);` |
|     14 | 6773 | `}` |
|      - | 6774 | `/*` |
|      - | 6775 | ` * Check if the given character is a member of the given mask.` |
|      - | 6776 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 6777 | ` * Refer to [strtok()].` |
|      - | 6778 | ` */` |
|     30 | 6779 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 6780 | `{` |
|      - | 6781 | `	int i;` |
|     57 | 6782 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 6783 | `		if( c == zMask[i] ){` |
|     13 | 6784 | `			if( pOfft ){` |
|      5 | 6785 | `				*pOfft = i;` |
|      2 | 6786 | `			}` |
|     13 | 6787 | `			return TRUE;` |
|      - | 6788 | `		}` |
|     14 | 6789 | `	}` |
|     19 | 6790 | `	return FALSE;` |
|     16 | 6791 | `}` |
|      - | 6792 | `/*` |
|      - | 6793 | ` * Extract a single token from the input stream.` |
|      - | 6794 | ` * Refer to [strtok()].` |
|      - | 6795 | ` */` |
|      6 | 6796 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 6797 | `{` |
|      7 | 6798 | `	const char *zIn = *pzIn;` |
|      - | 6799 | `	const char *zPtr;` |
|      - | 6800 | `	/* Ignore leading delimiter */` |
|     11 | 6801 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 6802 | `		zIn++;` |
|      1 | 6803 | `	}` |
|      7 | 6804 | `	if( zIn >= zEnd ){` |
|      - | 6805 | `		/* End of input */` |
|    ! 0 | 6806 | `		return SXERR_EOF;` |
|      - | 6807 | `	}` |
|      7 | 6808 | `	zPtr = zIn;` |
|      - | 6809 | `	/* Extract the token */` |
|     13 | 6810 | `	while( zIn < zEnd ){` |
|     11 | 6811 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 6812 | `			/* UTF-8 stream */` |
|    ! 0 | 6813 | `			zIn++;` |
|    ! 0 | 6814 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 6815 | `		}else{` |
|     11 | 6816 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 6817 | `				break;` |
|      - | 6818 | `			}` |
|      7 | 6819 | `			zIn++;` |
|      - | 6820 | `		}` |
|      1 | 6821 | `	}` |
|      7 | 6822 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 6823 | `	/* Update the cursor */` |
|      7 | 6824 | `	*pzIn = zIn;` |
|      - | 6825 | `	/* Return to the caller */` |
|      7 | 6826 | `	return SXRET_OK;` |
|      4 | 6827 | `}` |
|      - | 6828 | `/* strtok auxiliary private data */` |
|      - | 6829 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 6830 | `struct strtok_aux_data` |
|      - | 6831 | `{` |
|      - | 6832 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 6833 | `	const char *zIn;   /* Current input stream */` |
|      - | 6834 | `	const char *zEnd;  /* End of input */` |
|      - | 6835 | `};` |
|      - | 6836 | `/*` |
|      - | 6837 | ` * string strtok(string $str,string $token)` |
|      - | 6838 | ` * string strtok(string $token)` |
|      - | 6839 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 6840 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 6841 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 6842 | ` *  words by using the space character as the token.` |
|      - | 6843 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 6844 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 6845 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 6846 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 6847 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 6848 | ` *  the argument are found.` |
|      - | 6849 | ` * Parameters` |
|      - | 6850 | ` *  $str` |
|      - | 6851 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 6852 | ` * $token` |
|      - | 6853 | ` *  The delimiter used when splitting up str.` |
|      - | 6854 | ` * Return` |
|      - | 6855 | ` *   Current token or FALSE on EOF.` |
|      - | 6856 | ` */` |
|      6 | 6857 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6858 | `{` |
|      - | 6859 | `	strtok_aux_data *pAux;` |
|      - | 6860 | `	const char *zMask;` |
|      - | 6861 | `	SyString sToken;` |
|      - | 6862 | `	int nMasklen;` |
|      - | 6863 | `	sxi32 rc;` |
|      7 | 6864 | `	if( nArg < 2 ){` |
|      - | 6865 | `		/* Extract top aux data */` |
|      5 | 6866 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      5 | 6867 | `		if( pAux == 0 ){` |
|      - | 6868 | `			/* No aux data,return FALSE */` |
|    ! 0 | 6869 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6870 | `			return PH7_OK;` |
|      - | 6871 | `		}` |
|      5 | 6872 | `		nMasklen = 0;` |
|      5 | 6873 | `		zMask = ""; /* cc warning */` |
|      5 | 6874 | `		if( nArg > 0 ){` |
|      - | 6875 | `			/* Extract the mask */` |
|      5 | 6876 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 6877 | `		}` |
|      5 | 6878 | `		if( nMasklen < 1 ){` |
|      - | 6879 | `			/* Invalid mask,return FALSE */` |
|    ! 0 | 6880 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 6881 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6882 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 6883 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6884 | `			return PH7_OK;` |
|      - | 6885 | `		}` |
|      - | 6886 | `		/* Extract the token */` |
|      5 | 6887 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 6888 | `		if( rc != SXRET_OK ){` |
|      - | 6889 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 6890 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 6891 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6892 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 6893 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6894 | `		}else{` |
|      - | 6895 | `			/* Return the extracted token */` |
|      5 | 6896 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6897 | `		}` |
|      3 | 6898 | `	}else{` |
|      - | 6899 | `		const char *zInput,*zCur;` |
|      - | 6900 | `		char *zDup;` |
|      - | 6901 | `		int nLen;` |
|      - | 6902 | `		/* Extract the raw input */` |
|      3 | 6903 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 6904 | `		if( nLen < 1 ){` |
|      - | 6905 | `			/* Empty input,return FALSE */` |
|    ! 0 | 6906 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6907 | `			return PH7_OK;` |
|      - | 6908 | `		}` |
|      - | 6909 | `		/* Extract the mask */` |
|      3 | 6910 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 6911 | `		if( nMasklen < 1 ){` |
|      - | 6912 | `			/* Set a default mask */` |
|      - | 6913 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 6914 | `			zMask = TOK_MASK;` |
|    ! 0 | 6915 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 6916 | `#undef TOK_MASK` |
|    ! 0 | 6917 | `		}` |
|      - | 6918 | `		/* Extract a single token */` |
|      3 | 6919 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 6920 | `		if( rc != SXRET_OK ){` |
|      - | 6921 | `			/* Empty input */` |
|    ! 0 | 6922 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6923 | `			return PH7_OK;` |
|    ! 0 | 6924 | `		}else{` |
|      - | 6925 | `			/* Return the extracted token */` |
|      3 | 6926 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6927 | `		}` |
|      - | 6928 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 6929 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 6930 | `		if( pAux ){` |
|      3 | 6931 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 6932 | `			if( nLen < 1 ){` |
|    ! 0 | 6933 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6934 | `				return PH7_OK;` |
|      - | 6935 | `			}` |
|      - | 6936 | `			/* Duplicate input */` |
|      3 | 6937 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 6938 | `			if( zDup  ){` |
|      3 | 6939 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 6940 | `				/* Register the aux data */` |
|      3 | 6941 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 6942 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 6943 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 6944 | `			}` |
|      1 | 6945 | `		}` |
|      - | 6946 | `	}` |
|      7 | 6947 | `	return PH7_OK;` |
|      4 | 6948 | `}` |
|      - | 6949 | `/*` |
|      - | 6950 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 6951 | ` *  Pad a string to a certain length with another string` |
|      - | 6952 | ` * Parameters` |
|      - | 6953 | ` *  $input` |
|      - | 6954 | ` *   The input string.` |
|      - | 6955 | ` * $pad_length` |
|      - | 6956 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 6957 | ` *   string, no padding takes place.` |
|      - | 6958 | ` * $pad_string` |
|      - | 6959 | ` *   Note:` |
|      - | 6960 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 6961 | ` *    divided by the pad_string's length.` |
|      - | 6962 | ` * $pad_type` |
|      - | 6963 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 6964 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 6965 | ` * Return` |
|      - | 6966 | ` *  The padded string.` |
|      - | 6967 | ` */` |
|     10 | 6968 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6969 | `{` |
|      - | 6970 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 6971 | `	const char *zIn,*zPad;` |
|     11 | 6972 | `	if( nArg < 2 ){` |
|      - | 6973 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 6974 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 6975 | `		return PH7_OK;` |
|      - | 6976 | `	}` |
|      - | 6977 | `	/* Extract the target string */` |
|     11 | 6978 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6979 | `	/* Padding length */` |
|     11 | 6980 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|     11 | 6981 | `	if( iPadlen > 0 ){` |
|      9 | 6982 | `		iPadlen -= iLen;` |
|      4 | 6983 | `	}` |
|     11 | 6984 | `	if( iPadlen < 1  ){` |
|      - | 6985 | `		/* Return the string verbatim */` |
|      5 | 6986 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      5 | 6987 | `		return PH7_OK;` |
|      - | 6988 | `	}` |
|      7 | 6989 | `	zPad = " "; /* Whitespace padding */` |
|      7 | 6990 | `	iStrpad = (int)sizeof(char);` |
|      7 | 6991 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      7 | 6992 | `	if( nArg > 2 ){` |
|      - | 6993 | `		/* Padding string */` |
|      7 | 6994 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      7 | 6995 | `		if( iStrpad < 1 ){` |
|      - | 6996 | `			/* An empty pad string throws a catchable ValueError in PHP 8` |
|      - | 6997 | `			 * (only reached once padding is actually required). */` |
|      3 | 6998 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6999 | `				"str_pad(): Argument #3 ($pad_string) must not be empty");` |
|      - | 7000 | `		}` |
|      5 | 7001 | `		if( nArg > 3 ){` |
|      - | 7002 | `			/* Padd type */` |
|      5 | 7003 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 7004 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 7005 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 7006 | `			}` |
|      2 | 7007 | `		}` |
|      2 | 7008 | `	}` |
|      5 | 7009 | `	iDiv = 1;` |
|      5 | 7010 | `	if( iType == 2 ){` |
|    ! 0 | 7011 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 7012 | `	}` |
|      - | 7013 | `	/* Perform the requested operation */` |
|      5 | 7014 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 7015 | `		jPad = iStrpad;` |
|      5 | 7016 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 7017 | `			/* Padding */` |
|      5 | 7018 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 7019 | `				break;` |
|      - | 7020 | `			}` |
|      3 | 7021 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7022 | `		}` |
|      3 | 7023 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 7024 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 7025 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 7026 | `				if( jPad > iStrpad ){` |
|    ! 0 | 7027 | `					jPad = iStrpad;` |
|    ! 0 | 7028 | `				}` |
|      3 | 7029 | `				if( jPad < 1){` |
|    ! 0 | 7030 | `					break;` |
|      - | 7031 | `				}` |
|      3 | 7032 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 7033 | `			}` |
|      1 | 7034 | `		}` |
|      1 | 7035 | `	}` |
|      5 | 7036 | `	if( iLen > 0 ){` |
|      - | 7037 | `		/* Append the input string */` |
|      5 | 7038 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7039 | `	}` |
|      5 | 7040 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 7041 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 7042 | `			/* Padding */` |
|      5 | 7043 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 7044 | `				break;` |
|      - | 7045 | `			}` |
|      3 | 7046 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7047 | `		}` |
|      5 | 7048 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 7049 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 7050 | `			if( jPad > iStrpad ){` |
|    ! 0 | 7051 | `				jPad = iStrpad;` |
|    ! 0 | 7052 | `			}` |
|      3 | 7053 | `			if( jPad < 1){` |
|    ! 0 | 7054 | `				break;` |
|      - | 7055 | `			}` |
|      3 | 7056 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 7057 | `		}` |
|      1 | 7058 | `	}` |
|      5 | 7059 | `	return PH7_OK;` |
|      6 | 7060 | `}` |
|      - | 7061 | `/*` |
|      - | 7062 | ` * String replacement private data.` |
|      - | 7063 | ` */` |
|      - | 7064 | `typedef struct str_replace_data str_replace_data;` |
|      - | 7065 | `struct str_replace_data` |
|      - | 7066 | `{` |
|      - | 7067 | `	/* Used by the str_replace family to collect the search/replace arguments. */` |
|      - | 7068 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 7069 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 7070 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 7071 | `};` |
|      - | 7072 | `/*` |
|      - | 7073 | ` * Remove a substring.` |
|      - | 7074 | ` */` |
|      - | 7075 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 7076 | `	for(;;){\` |
|      - | 7077 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 7078 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 7079 | `		++OFFT;\` |
|      - | 7080 | `	}\` |
|      - | 7081 | `}` |
|      - | 7082 | `/*` |
|      - | 7083 | ` * Shift right and insert algorithm.` |
|      - | 7084 | ` */` |
|      - | 7085 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 7086 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 7087 | `		for(;;){\` |
|      - | 7088 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 7089 | `			if(INLEN < 1 ) { break; }\` |
|      - | 7090 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 7091 | `			--INLEN; \` |
|      - | 7092 | `		}\` |
|      - | 7093 | `		for(;;){\` |
|      - | 7094 | `				if(ELEN < 1) { break; }\` |
|      - | 7095 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 7096 | `				OFFT++;\` |
|      - | 7097 | `				ENTRY++;\` |
|      - | 7098 | `				--ELEN;\` |
|      - | 7099 | `		}\` |
|      - | 7100 | `}` |
|      - | 7101 | `/*` |
|      - | 7102 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 7103 | ` * replacement string [i.e: zReplace].` |
|      - | 7104 | ` */` |
|     32 | 7105 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 7106 | `{` |
|     33 | 7107 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 7108 | `	sxu32 n,m;` |
|     33 | 7109 | `	n = SyBlobLength(pWorker);` |
|     33 | 7110 | `	m = nOfft;` |
|      - | 7111 | `	/* Delete the old entry */` |
|    429 | 7112 | `	STRDEL(zInput,n,m,nLen);` |
|     33 | 7113 | `	SyBlobLength(pWorker) -= nLen;` |
|     33 | 7114 | `	if( nReplen > 0 ){` |
|     27 | 7115 | `		sxi32 iRep = nReplen;` |
|      - | 7116 | `		sxi32 rc;` |
|      - | 7117 | `		/*` |
|      - | 7118 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 7119 | `		 * string.` |
|      - | 7120 | `		 */` |
|     27 | 7121 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     27 | 7122 | `		if( rc != SXRET_OK ){` |
|      - | 7123 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 7124 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 7125 | `			return rc;` |
|      - | 7126 | `		}` |
|      - | 7127 | `		/* Perform the insertion now */` |
|     27 | 7128 | `		zInput = (char *)SyBlobData(pWorker);` |
|     27 | 7129 | `		n = SyBlobLength(pWorker);` |
|    129 | 7130 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     27 | 7131 | `		SyBlobLength(pWorker) += nReplen;` |
|     13 | 7132 | `	}` |
|     33 | 7133 | `	return SXRET_OK;` |
|     17 | 7134 | `}` |
|      - | 7135 | `/*` |
|      - | 7136 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 7137 | ` * to collect search/replace string.` |
|      - | 7138 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 7139 | ` */` |
|     26 | 7140 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 7141 | `{` |
|     27 | 7142 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 7143 | `	SyString sWorker;` |
|      - | 7144 | `	const char *zIn;` |
|      - | 7145 | `	int nByte;` |
|      - | 7146 | `	/* Extract a string representation of the given argument */` |
|     27 | 7147 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 7148 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 7149 | `	if( nByte > 0 ){` |
|      - | 7150 | `		char *zDup;` |
|      - | 7151 | `		/* Duplicate the chunk */` |
|     25 | 7152 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 7153 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 7154 | `			);` |
|     25 | 7155 | `		if( zDup == 0 ){` |
|      - | 7156 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 7157 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 7158 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 7159 | `			return SXERR_MEM;` |
|      - | 7160 | `		}` |
|     25 | 7161 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 7162 | `		/* Save the chunk */` |
|     25 | 7163 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 7164 | `	}` |
|      - | 7165 | `	/* Save for later processing */` |
|     27 | 7166 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 7167 | `	/* All done */` |
|     13 | 7168 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 7169 | `	return PH7_OK;` |
|     14 | 7170 | `}` |
|      - | 7171 | `/*` |
|      - | 7172 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 7173 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 7174 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 7175 | ` * Parameters` |
|      - | 7176 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 7177 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 7178 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 7179 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 7180 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 7181 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 7182 | ` * $search` |
|      - | 7183 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 7184 | ` *  to designate multiple needles.` |
|      - | 7185 | ` * $replace` |
|      - | 7186 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 7187 | ` *  to designate multiple replacements.` |
|      - | 7188 | ` * $subject` |
|      - | 7189 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 7190 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 7191 | ` *  of subject, and the return value is an array as well.` |
|      - | 7192 | ` * $count (Not used)` |
|      - | 7193 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 7194 | ` * Return` |
|      - | 7195 | ` * This function returns a string or an array with the replaced values.` |
|      - | 7196 | ` */` |
|  28838 | 7197 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7198 | `{` |
|      - | 7199 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 7200 | `	ProcStringMatch xMatch;` |
|      - | 7201 | `	const char *zIn,*zFunc;` |
|      - | 7202 | `	str_replace_data sRep;` |
|      - | 7203 | `	SyBlob sWorker;` |
|      - | 7204 | `	SySet sReplace;` |
|      - | 7205 | `	SySet sSearch;` |
|      - | 7206 | `	int rep_str;` |
|      - | 7207 | `	int nByte;` |
|      - | 7208 | `	sxi32 rc;` |
|  28843 | 7209 | `	if( nArg < 3 ){` |
|      - | 7210 | `		/* Missing/Invalid arguments,return null */` |
|    ! 0 | 7211 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7212 | `		return PH7_OK;` |
|      - | 7213 | `	}` |
|      - | 7214 | `	/* Initialize fields */` |
|  28843 | 7215 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  28843 | 7216 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  28843 | 7217 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  28843 | 7218 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  28843 | 7219 | `	sRep.pCtx = pCtx;` |
|  28843 | 7220 | `	sRep.pCollector = &sSearch;` |
|  28843 | 7221 | `	rep_str = 0;` |
|      - | 7222 | `	/* Extract the subject */` |
|  28843 | 7223 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  28843 | 7224 | `	if( nByte < 1 ){` |
|      - | 7225 | `		/* Nothing to replace,return the empty string */` |
|     29 | 7226 | `		ph7_result_string(pCtx,"",0);` |
|     29 | 7227 | `		return PH7_OK;` |
|      - | 7228 | `	}` |
|      - | 7229 | `	/* Copy the subject */` |
|  28815 | 7230 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 7231 | `	/* Search string */` |
|  28815 | 7232 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 7233 | `		/* Collect search string */` |
|      9 | 7234 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 7235 | `	}else{` |
|      - | 7236 | `		/* Single pattern */` |
|  28807 | 7237 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  28807 | 7238 | `		if( nByte < 1 ){` |
|      - | 7239 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 7240 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 7241 | `			return PH7_OK;` |
|      - | 7242 | `		}` |
|  28803 | 7243 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 7244 | `		/* Save for later processing */` |
|  28803 | 7245 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 7246 | `	}` |
|      - | 7247 | `	/* Replace string */` |
|  28811 | 7248 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 7249 | `		/* Collect replace string */` |
|      7 | 7250 | `		sRep.pCollector = &sReplace;` |
|      7 | 7251 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 7252 | `	}else{` |
|      - | 7253 | `		/* Single needle */` |
|  28805 | 7254 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  28805 | 7255 | `		rep_str = 1;` |
|  28805 | 7256 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 7257 | `		/* Save for later processing */` |
|  28805 | 7258 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 7259 | `	}` |
|      - | 7260 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  28811 | 7261 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 7262 | `		SySetRelease(&sSearch);` |
|    ! 0 | 7263 | `		SySetRelease(&sReplace);` |
|    ! 0 | 7264 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 7265 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7266 | `	}` |
|      - | 7267 | `	/* Reset loop cursors */` |
|  28811 | 7268 | `	SySetResetCursor(&sSearch);` |
|  28811 | 7269 | `	SySetResetCursor(&sReplace);` |
|  28811 | 7270 | `	pReplace = pSearch = 0; /* cc warning */` |
|  28811 | 7271 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 7272 | `	/* Extract function name */` |
|  28811 | 7273 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 7274 | `	/* Set the default pattern match routine */` |
|  28811 | 7275 | `	xMatch = SyBlobSearch;` |
|  28811 | 7276 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 7277 | `		/* Case insensitive pattern match */` |
|     11 | 7278 | `		xMatch = iPatternMatch;` |
|      5 | 7279 | `	}` |
|      - | 7280 | `	/* Start the replace process */` |
|  57625 | 7281 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 7282 | `		sxu32 nCount,nOfft;` |
|  28819 | 7283 | `		if( pSearch->nByte <  1 ){` |
|      - | 7284 | `			/* Empty string,ignore */` |
|      3 | 7285 | `			continue;` |
|      - | 7286 | `		}` |
|      - | 7287 | `		/* Extract the replace string */` |
|  28817 | 7288 | `		if( rep_str ){` |
|  28807 | 7289 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  14406 | 7290 | `		}else{` |
|     11 | 7291 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 7292 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 7293 | `				 * An empty string is used for the rest of replacement values` |
|      - | 7294 | `				 */` |
|      3 | 7295 | `				pReplace = 0;` |
|      1 | 7296 | `			}` |
|      - | 7297 | `		}` |
|  28817 | 7298 | `		if( pReplace == 0 ){` |
|      - | 7299 | `			/* Use an empty string instead */` |
|      3 | 7300 | `			pReplace = &sTemp;` |
|      1 | 7301 | `		}` |
|  28817 | 7302 | `		nOfft = nCount = 0;` |
|  14422 | 7303 | `		for(;;){` |
|  28849 | 7304 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 7305 | `				break;` |
|      - | 7306 | `			}` |
|      - | 7307 | `			/* Perform a pattern lookup */` |
|  43253 | 7308 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  28832 | 7309 | `				pSearch->nByte,&nOfft);` |
|  28837 | 7310 | `			if( rc != SXRET_OK ){` |
|      - | 7311 | `				/* Pattern not found */` |
|  28805 | 7312 | `				break;` |
|      - | 7313 | `			}` |
|      - | 7314 | `			/* Perform the replace operation */` |
|     33 | 7315 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     33 | 7316 | `			if( rc != SXRET_OK ){` |
|      - | 7317 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 7318 | `				SySetRelease(&sSearch);` |
|    ! 0 | 7319 | `				SySetRelease(&sReplace);` |
|    ! 0 | 7320 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 7321 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 7322 | `			}` |
|      - | 7323 | `			/* Increment offset counter */` |
|     33 | 7324 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 7325 | `		}` |
|      5 | 7326 | `	}` |
|      - | 7327 | `	/* All done,clean-up the mess left behind */` |
|  28811 | 7328 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  28811 | 7329 | `	SySetRelease(&sSearch);` |
|  28811 | 7330 | `	SySetRelease(&sReplace);` |
|  28811 | 7331 | `	SyBlobRelease(&sWorker);` |
|  28811 | 7332 | `	if( rc != PH7_OK ){` |
|    ! 0 | 7333 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7334 | `	}` |
|  28811 | 7335 | `	return PH7_OK;` |
|  14424 | 7336 | `}` |
|      - | 7337 | `/*` |
|      - | 7338 | ` * strtr() array form: a single (key,value) pair copied out of the replace_pairs` |
|      - | 7339 | ` * array. The bytes are owned by a persistent pool (see strtr_collect) rather than` |
|      - | 7340 | ` * the transient walker values, which HashmapWalk releases after each callback, so` |
|      - | 7341 | ` * we store byte offsets into that pool instead of raw pointers.` |
|      - | 7342 | ` */` |
|      - | 7343 | `typedef struct strtr_entry strtr_entry;` |
|      - | 7344 | `struct strtr_entry` |
|      - | 7345 | `{` |
|      - | 7346 | `	sxu32 nKeyOfft; /* Offset of the search key inside the pool */` |
|      - | 7347 | `	sxu32 nKeyLen;  /* Length of the search key */` |
|      - | 7348 | `	sxu32 nValOfft; /* Offset of the replacement inside the pool */` |
|      - | 7349 | `	sxu32 nValLen;  /* Length of the replacement */` |
|      - | 7350 | `};` |
|      - | 7351 | `typedef struct strtr_collect strtr_collect;` |
|      - | 7352 | `struct strtr_collect` |
|      - | 7353 | `{` |
|      - | 7354 | `	SyBlob *pPool;  /* Byte pool holding copied key + value bytes */` |
|      - | 7355 | `	SySet  *pTable; /* Set of strtr_entry (parallel offsets into pPool) */` |
|      - | 7356 | `	sxi32   rc;     /* Carries an allocation failure (SXERR_MEM) out of the walker */` |
|      - | 7357 | `};` |
|      - | 7358 | `/*` |
|      - | 7359 | ` * Collect one replace_pairs entry into the persistent pool/offset table.` |
|      - | 7360 | ` * PHP coerces both the key and the value to string (an integer key becomes its` |
|      - | 7361 | ` * decimal form) and ignores an empty-string key.` |
|      - | 7362 | ` */` |
|     20 | 7363 | `static int StrtrCollectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 7364 | `{` |
|     21 | 7365 | `	strtr_collect *pCol = (strtr_collect *)pUserData;` |
|      - | 7366 | `	const char *zKey,*zVal;` |
|      - | 7367 | `	strtr_entry sEnt;` |
|      - | 7368 | `	int nKey,nVal;` |
|     21 | 7369 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     21 | 7370 | `	if( nKey < 1 ){` |
|      - | 7371 | `		/* PHP ignores an empty-string key (it also emits a warning we do not replicate). */` |
|    ! 0 | 7372 | `		return PH7_OK;` |
|      - | 7373 | `	}` |
|     21 | 7374 | `	zVal = ph7_value_to_string(pData,&nVal);` |
|     21 | 7375 | `	sEnt.nKeyOfft = SyBlobLength(pCol->pPool);` |
|     21 | 7376 | `	sEnt.nKeyLen  = (sxu32)nKey;` |
|     21 | 7377 | `	if( SyBlobAppend(pCol->pPool,(const void *)zKey,(sxu32)nKey) != SXRET_OK ){` |
|    ! 0 | 7378 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 7379 | `		return SXERR_ABORT;` |
|      - | 7380 | `	}` |
|     21 | 7381 | `	sEnt.nValOfft = SyBlobLength(pCol->pPool);` |
|     21 | 7382 | `	sEnt.nValLen  = (sxu32)nVal;` |
|     21 | 7383 | `	if( nVal > 0 && SyBlobAppend(pCol->pPool,(const void *)zVal,(sxu32)nVal) != SXRET_OK ){` |
|    ! 0 | 7384 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 7385 | `		return SXERR_ABORT;` |
|      - | 7386 | `	}` |
|     21 | 7387 | `	if( SySetPut(pCol->pTable,(const void *)&sEnt) != SXRET_OK ){` |
|    ! 0 | 7388 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 7389 | `		return SXERR_ABORT;` |
|      - | 7390 | `	}` |
|     21 | 7391 | `	return PH7_OK;` |
|     11 | 7392 | `}` |
|      - | 7393 | `/*` |
|      - | 7394 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 7395 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 7396 | ` *  Translate characters or replace substrings.` |
|      - | 7397 | ` * Parameters` |
|      - | 7398 | ` *  $str` |
|      - | 7399 | ` *  The string being translated.` |
|      - | 7400 | ` * $from` |
|      - | 7401 | ` *  The string being translated to to.` |
|      - | 7402 | ` * $to` |
|      - | 7403 | ` *  The string replacing from.` |
|      - | 7404 | ` * $replace_pairs` |
|      - | 7405 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 7406 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 7407 | ` * Return` |
|      - | 7408 | ` *  The translated string.` |
|      - | 7409 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 7410 | ` */` |
|     12 | 7411 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7412 | `{` |
|      - | 7413 | `	const char *zIn;` |
|      - | 7414 | `	int nLen;` |
|     13 | 7415 | `	if( nArg < 1 ){` |
|      - | 7416 | `		/* Nothing to replace,return FALSE */` |
|    ! 0 | 7417 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7418 | `		return PH7_OK;` |
|      - | 7419 | `	}` |
|     13 | 7420 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 7421 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 7422 | `		/* Invalid arguments */` |
|    ! 0 | 7423 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 7424 | `		return PH7_OK;` |
|      - | 7425 | `	}` |
|     18 | 7426 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 7427 | `		strtr_collect sCol;` |
|      - | 7428 | `		SyBlob sPool,sWorker;` |
|      - | 7429 | `		SySet sTable;` |
|      - | 7430 | `		const char *zPool;` |
|      - | 7431 | `		strtr_entry *pEnt;` |
|      - | 7432 | `		sxi32 rc;` |
|      - | 7433 | `		int i,iRun;` |
|      - | 7434 | `		/*` |
|      - | 7435 | `		 * PHP's array-form strtr is a single left-to-right pass over the subject:` |
|      - | 7436 | `		 * at every position it substitutes the LONGEST replace_pairs key that` |
|      - | 7437 | `		 * matches there, then advances past the key (replacements are never` |
|      - | 7438 | `		 * rescanned). It is not a sequential per-key global replace. First copy` |
|      - | 7439 | `		 * the pairs into a persistent pool, then run that scan.` |
|      - | 7440 | `		 */` |
|     11 | 7441 | `		SyBlobInit(&sPool,&pCtx->pVm->sAllocator);` |
|     11 | 7442 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     11 | 7443 | `		SySetInit(&sTable,&pCtx->pVm->sAllocator,sizeof(strtr_entry));` |
|     11 | 7444 | `		sCol.pPool  = &sPool;` |
|     11 | 7445 | `		sCol.pTable = &sTable;` |
|     11 | 7446 | `		sCol.rc     = SXRET_OK;` |
|     11 | 7447 | `		ph7_array_walk(apArg[1],StrtrCollectWalker,&sCol);` |
|     11 | 7448 | `		if( sCol.rc != SXRET_OK ){` |
|      - | 7449 | `			/* Allocation failure while collecting the pairs: surface a fatal */` |
|    ! 0 | 7450 | `			SyBlobRelease(&sPool);` |
|    ! 0 | 7451 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 7452 | `			SySetRelease(&sTable);` |
|    ! 0 | 7453 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 7454 | `		}` |
|      - | 7455 | `		/* The pool is now stable, so offsets can be resolved against its base. */` |
|     11 | 7456 | `		zPool = (const char *)SyBlobData(&sPool);` |
|     11 | 7457 | `		rc = SXRET_OK;` |
|     11 | 7458 | `		iRun = 0; /* Start of the pending run of unmatched bytes copied verbatim. */` |
|     43 | 7459 | `		for( i = 0 ; i < nLen ; ){` |
|     33 | 7460 | `			strtr_entry *pBest = 0;` |
|     33 | 7461 | `			sxu32 nBest = 0;` |
|      - | 7462 | `			/* Pick the longest key that matches at the current position. */` |
|     33 | 7463 | `			SySetResetCursor(&sTable);` |
|     97 | 7464 | `			while( SXRET_OK == SySetGetNextEntry(&sTable,(void **)&pEnt) ){` |
|     64 | 7465 | `				if( pEnt->nKeyLen > nBest` |
|     60 | 7466 | `					&& pEnt->nKeyLen <= (sxu32)(nLen - i)` |
|     56 | 7467 | `					&& SyMemcmp(zPool + pEnt->nKeyOfft,zIn + i,pEnt->nKeyLen) == 0 ){` |
|     29 | 7468 | `					nBest = pEnt->nKeyLen;` |
|     29 | 7469 | `					pBest = pEnt;` |
|     14 | 7470 | `				}` |
|      1 | 7471 | `			}` |
|     33 | 7472 | `			if( pBest == 0 ){` |
|      - | 7473 | `				/* No key here: extend the literal run and copy it in one shot later. */` |
|      9 | 7474 | `				i++;` |
|      9 | 7475 | `				continue;` |
|      - | 7476 | `			}` |
|      - | 7477 | `			/* Flush the pending literal run, then the replacement. */` |
|     25 | 7478 | `			if( i > iRun ){` |
|      5 | 7479 | `				rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(i - iRun));` |
|      2 | 7480 | `			}` |
|     25 | 7481 | `			if( rc == SXRET_OK && pBest->nValLen > 0 ){` |
|     25 | 7482 | `				rc = SyBlobAppend(&sWorker,zPool + pBest->nValOfft,pBest->nValLen);` |
|     12 | 7483 | `			}` |
|     25 | 7484 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 7485 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 7486 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 7487 | `				SySetRelease(&sTable);` |
|    ! 0 | 7488 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 7489 | `			}` |
|     25 | 7490 | `			i += (int)pBest->nKeyLen;` |
|     25 | 7491 | `			iRun = i;` |
|      1 | 7492 | `		}` |
|      - | 7493 | `		/* Flush the trailing literal run. */` |
|     11 | 7494 | `		if( nLen > iRun ){` |
|      3 | 7495 | `			rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(nLen - iRun));` |
|      3 | 7496 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 7497 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 7498 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 7499 | `				SySetRelease(&sTable);` |
|    ! 0 | 7500 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 7501 | `			}` |
|      1 | 7502 | `		}` |
|      - | 7503 | `		/* All done, return the result string */` |
|     16 | 7504 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|     10 | 7505 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 7506 | `		/* Clean-up */` |
|     11 | 7507 | `		SyBlobRelease(&sPool);` |
|     11 | 7508 | `		SyBlobRelease(&sWorker);` |
|     11 | 7509 | `		SySetRelease(&sTable);` |
|     11 | 7510 | `		if( rc != PH7_OK ){` |
|    ! 0 | 7511 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 7512 | `		}` |
|      6 | 7513 | `	}else{` |
|      - | 7514 | `		int i,flen,tlen,c,iOfft;` |
|      - | 7515 | `		const char *zFrom,*zTo;` |
|      3 | 7516 | `		if( nArg < 3 ){` |
|      - | 7517 | `			/* Nothing to replace */` |
|    ! 0 | 7518 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 7519 | `			return PH7_OK;` |
|      - | 7520 | `		}` |
|      - | 7521 | `		/* Extract given arguments */` |
|      3 | 7522 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 7523 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 7524 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 7525 | `			/* Nothing to replace */` |
|    ! 0 | 7526 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 7527 | `			return PH7_OK;` |
|      - | 7528 | `		}` |
|      - | 7529 | `		/* Start the replace process */` |
|     13 | 7530 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 7531 | `			c = zIn[i];` |
|     11 | 7532 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 7533 | `				if ( iOfft < tlen ){` |
|      5 | 7534 | `					c = zTo[iOfft];` |
|      2 | 7535 | `				}` |
|      2 | 7536 | `			}` |
|     11 | 7537 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 7538 |  |
|      6 | 7539 | `		}` |
|      - | 7540 | `	}` |
|     13 | 7541 | `	return PH7_OK;` |
|      7 | 7542 | `}` |
|      - | 7543 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7544 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7545 | `/*` |
|      - | 7546 | ` * Parse an INI string.` |
|      - | 7547 |  |
|      - | 7548 | ` * According to wikipedia` |
|      - | 7549 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 7550 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 7551 | ` *  Format` |
|      - | 7552 | `*    Properties` |
|      - | 7553 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 7554 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 7555 | `*     Example:` |
|      - | 7556 | `*      name=value` |
|      - | 7557 | `*    Sections` |
|      - | 7558 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 7559 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 7560 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 7561 | `*     or the end of the file. Sections may not be nested.` |
|      - | 7562 | `*     Example:` |
|      - | 7563 | `*      [section]` |
|      - | 7564 | `*   Comments` |
|      - | 7565 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 7566 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 7567 | `*/` |
|     12 | 7568 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 7569 | `{` |
|      - | 7570 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 7571 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 7572 | `	SyHashEntry *pEntry;` |
|      - | 7573 | `	SyString sEntry;` |
|      - | 7574 | `	SyHash sHash;` |
|      - | 7575 | `	int c;` |
|      - | 7576 | `	/* Create an empty array and worker variables */` |
|     13 | 7577 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 7578 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 7579 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 7580 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 7581 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|    ! 0 | 7582 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7583 | `	}` |
|     13 | 7584 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 7585 | `	pCur = pArray;` |
|      - | 7586 | `	/* Start the parse process */` |
|     21 | 7587 | `	for(;;){` |
|      - | 7588 | `		/* Ignore leading white spaces */` |
|     69 | 7589 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 7590 | `			zIn++;` |
|      1 | 7591 | `		}` |
|     43 | 7592 | `		if( zIn >= zEnd ){` |
|      - | 7593 | `			/* No more input to process */` |
|     13 | 7594 | `			break;` |
|      - | 7595 | `		}` |
|     31 | 7596 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 7597 | `			/* Comment til the end of line */` |
|    ! 0 | 7598 | `			zIn++;` |
|    ! 0 | 7599 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 7600 | `				zIn++;` |
|    ! 0 | 7601 | `			}` |
|    ! 0 | 7602 | `			continue;` |
|      - | 7603 | `		}` |
|      - | 7604 | `		/* Reset the string cursor of the working variable */` |
|     31 | 7605 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 7606 | `		if( zIn[0] == '[' ){` |
|      - | 7607 | `			/* Section: Extract the section name */` |
|      9 | 7608 | `			zIn++;` |
|      9 | 7609 | `			zCur = zIn;` |
|     73 | 7610 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 7611 | `				zIn++;` |
|      1 | 7612 | `			}` |
|      9 | 7613 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 7614 | `				/* Save the section name */` |
|      5 | 7615 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 7616 | `				SyStringFullTrim(&sEntry);` |
|      5 | 7617 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 7618 | `				if( sEntry.nByte > 0 ){` |
|      - | 7619 | `					/* Associate an array with the section */` |
|      5 | 7620 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 7621 | `					if( pSection ){` |
|      5 | 7622 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 7623 | `						pCur = pSection;` |
|      2 | 7624 | `					}` |
|      2 | 7625 | `				}` |
|      2 | 7626 | `			}` |
|      9 | 7627 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 7628 | `		}else{` |
|      - | 7629 | `			ph7_value *pOldCur;` |
|      - | 7630 | `			int is_array;` |
|      - | 7631 | `			int iLen;` |
|      - | 7632 | `			/* Properties */` |
|     23 | 7633 | `			is_array = 0;` |
|     23 | 7634 | `			zCur = zIn;` |
|     23 | 7635 | `			iLen = 0; /* cc warning */` |
|     23 | 7636 | `			pOldCur = pCur;` |
|    155 | 7637 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 7638 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 7639 | `					/* Array */` |
|    ! 0 | 7640 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 7641 | `					is_array = 1;` |
|    ! 0 | 7642 | `					if( iLen > 0 ){` |
|    ! 0 | 7643 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 7644 | `						/* Query the hashtable */` |
|    ! 0 | 7645 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 7646 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 7647 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 7648 | `						if( pEntry ){` |
|    ! 0 | 7649 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 7650 | `						}else{` |
|      - | 7651 | `							/* Create an empty array */` |
|    ! 0 | 7652 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 7653 | `							if( pvArr ){` |
|      - | 7654 | `								/* Save the entry */` |
|    ! 0 | 7655 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 7656 | `								/* Insert the entry */` |
|    ! 0 | 7657 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 7658 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 7659 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 7660 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 7661 | `							}` |
|      - | 7662 | `						}` |
|    ! 0 | 7663 | `						if( pvArr ){` |
|    ! 0 | 7664 | `							pCur = pvArr;` |
|    ! 0 | 7665 | `						}` |
|    ! 0 | 7666 | `					}` |
|    ! 0 | 7667 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 7668 | `						zIn++;` |
|    ! 0 | 7669 | `					}` |
|    ! 0 | 7670 | `				}` |
|    133 | 7671 | `				zIn++;` |
|      1 | 7672 | `			}` |
|     23 | 7673 | `			if( !is_array ){` |
|     23 | 7674 | `				iLen = (int)(zIn-zCur);` |
|     11 | 7675 | `			}` |
|      - | 7676 | `			/* Trim the key */` |
|     23 | 7677 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 7678 | `			SyStringFullTrim(&sEntry);` |
|     23 | 7679 | `			if( sEntry.nByte > 0 ){` |
|     23 | 7680 | `				if( !is_array ){` |
|      - | 7681 | `					/* Save the key name */` |
|     23 | 7682 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 7683 | `				}` |
|      - | 7684 | `				/* extract key value */` |
|     23 | 7685 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 7686 | `				zIn++; /* '=' */` |
|     39 | 7687 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 7688 | `					zIn++;` |
|      1 | 7689 | `				}` |
|     23 | 7690 | `				if( zIn < zEnd ){` |
|     21 | 7691 | `					zCur = zIn;` |
|     21 | 7692 | `					c = zIn[0];` |
|     21 | 7693 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 7694 | `						zIn++;` |
|      - | 7695 | `						/* Delimit the value */` |
|    ! 0 | 7696 | `						while( zIn < zEnd ){` |
|    ! 0 | 7697 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 7698 | `								break;` |
|      - | 7699 | `							}` |
|    ! 0 | 7700 | `							zIn++;` |
|    ! 0 | 7701 | `						}` |
|    ! 0 | 7702 | `						if( zIn < zEnd ){` |
|    ! 0 | 7703 | `							zIn++;` |
|    ! 0 | 7704 | `						}` |
|    ! 0 | 7705 | `					}else{` |
|    125 | 7706 | `						while( zIn < zEnd ){` |
|    123 | 7707 | `							if( zIn[0] == '\n' ){` |
|     19 | 7708 | `								if( zIn[-1] != '\\' ){` |
|     19 | 7709 | `									break;` |
|    ! 0 | 7710 | `								}` |
|    105 | 7711 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 7712 | `								/* Inline comments */` |
|    ! 0 | 7713 | `								break;` |
|      - | 7714 | `							}` |
|    105 | 7715 | `							zIn++;` |
|      1 | 7716 | `						}` |
|      - | 7717 | `					}` |
|      - | 7718 | `					/* Trim the value */` |
|     21 | 7719 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 7720 | `					SyStringFullTrim(&sEntry);` |
|     21 | 7721 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 7722 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 7723 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 7724 | `					}` |
|     21 | 7725 | `					if( sEntry.nByte > 0 ){` |
|     21 | 7726 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 7727 | `					}` |
|      - | 7728 | `					/* Insert the key and it's value */` |
|     21 | 7729 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 7730 | `				}` |
|     12 | 7731 | `			}else{` |
|    ! 0 | 7732 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 7733 | `					zIn++;` |
|    ! 0 | 7734 | `				}` |
|      - | 7735 | `			}` |
|     23 | 7736 | `			pCur = pOldCur;` |
|      - | 7737 | `		}` |
|      1 | 7738 | `	}` |
|     13 | 7739 | `	SyHashRelease(&sHash);` |
|      - | 7740 | `	/* Return the parse of the INI string */` |
|     13 | 7741 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 7742 | `	return SXRET_OK;` |
|      7 | 7743 | `}` |
|      - | 7744 | `/*` |
|      - | 7745 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 7746 | ` *  Parse a configuration string.` |
|      - | 7747 | ` * Parameters` |
|      - | 7748 | ` *  $ini` |
|      - | 7749 | ` *   The contents of the ini file being parsed.` |
|      - | 7750 | ` *  $process_sections` |
|      - | 7751 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 7752 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 7753 | ` *  $scanner_mode (Not used)` |
|      - | 7754 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 7755 | ` *   then option values will not be parsed.` |
|      - | 7756 | ` * Return` |
|      - | 7757 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 7758 | ` */` |
|     10 | 7759 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7760 | `{` |
|      - | 7761 | `	const char *zIni;` |
|      - | 7762 | `	int nByte;` |
|     11 | 7763 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7764 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 7765 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7766 | `		return PH7_OK;` |
|      - | 7767 | `	}` |
|      - | 7768 | `	/* Extract the raw INI buffer */` |
|     11 | 7769 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 7770 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|     11 | 7771 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      6 | 7772 | `}` |
|      - | 7773 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7774 |  |
|      - | 7775 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7776 |  |
|      - | 7777 | `/*` |
|      - | 7778 | ` * Ctype Functions.` |
|      - | 7779 | ` * Status:` |
|      - | 7780 | ` *    Stable.` |
|      - | 7781 | ` */` |
|      - | 7782 | `/*` |
|      - | 7783 | ` * bool ctype_alnum(string $text)` |
|      - | 7784 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 7785 | ` * Parameters` |
|      - | 7786 | ` *  $text` |
|      - | 7787 | ` *   The tested string.` |
|      - | 7788 | ` * Return` |
|      - | 7789 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 7790 | ` */` |
|     14 | 7791 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7792 | `{` |
|      - | 7793 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7794 | `	int nLen;` |
|     15 | 7795 | `	if( nArg < 1 ){` |
|      - | 7796 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7797 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7798 | `		return PH7_OK;` |
|      - | 7799 | `	}` |
|      - | 7800 | `	/* Extract the target string */` |
|     15 | 7801 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 7802 | `	zEnd = &zIn[nLen];` |
|     15 | 7803 | `	if( nLen < 1 ){` |
|      - | 7804 | `		/* Empty string,return FALSE */` |
|      3 | 7805 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7806 | `		return PH7_OK;` |
|      - | 7807 | `	}` |
|      - | 7808 | `	/* Perform the requested operation */` |
|     32 | 7809 | `	for(;;){` |
|     65 | 7810 | `		if( zIn >= zEnd ){` |
|      - | 7811 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7812 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7813 | `			return PH7_OK;` |
|      - | 7814 | `		}` |
|     57 | 7815 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 7816 | `			break;` |
|      - | 7817 | `		}` |
|      - | 7818 | `		/* Point to the next character */` |
|     53 | 7819 | `		zIn++;` |
|      1 | 7820 | `	}` |
|      - | 7821 | `	/* The test failed,return FALSE */` |
|      5 | 7822 | `	ph7_result_bool(pCtx,0);` |
|      5 | 7823 | `	return PH7_OK;` |
|      8 | 7824 | `}` |
|      - | 7825 | `/*` |
|      - | 7826 | ` * bool ctype_alpha(string $text)` |
|      - | 7827 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 7828 | ` * Parameters` |
|      - | 7829 | ` *  $text` |
|      - | 7830 | ` *   The tested string.` |
|      - | 7831 | ` * Return` |
|      - | 7832 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 7833 | ` */` |
|     16 | 7834 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7835 | `{` |
|      - | 7836 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7837 | `	int nLen;` |
|     17 | 7838 | `	if( nArg < 1 ){` |
|      - | 7839 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7840 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7841 | `		return PH7_OK;` |
|      - | 7842 | `	}` |
|      - | 7843 | `	/* Extract the target string */` |
|     17 | 7844 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7845 | `	zEnd = &zIn[nLen];` |
|     17 | 7846 | `	if( nLen < 1 ){` |
|      - | 7847 | `		/* Empty string,return FALSE */` |
|      3 | 7848 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7849 | `		return PH7_OK;` |
|      - | 7850 | `	}` |
|      - | 7851 | `	/* Perform the requested operation */` |
|     42 | 7852 | `	for(;;){` |
|     85 | 7853 | `		if( zIn >= zEnd ){` |
|      - | 7854 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7855 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7856 | `			return PH7_OK;` |
|      - | 7857 | `		}` |
|     77 | 7858 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 7859 | `			break;` |
|      - | 7860 | `		}` |
|      - | 7861 | `		/* Point to the next character */` |
|     71 | 7862 | `		zIn++;` |
|      1 | 7863 | `	}` |
|      - | 7864 | `	/* The test failed,return FALSE */` |
|      7 | 7865 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7866 | `	return PH7_OK;` |
|      9 | 7867 | `}` |
|      - | 7868 | `/*` |
|      - | 7869 | ` * bool ctype_cntrl(string $text)` |
|      - | 7870 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 7871 | ` * Parameters` |
|      - | 7872 | ` *  $text` |
|      - | 7873 | ` *   The tested string.` |
|      - | 7874 | ` * Return` |
|      - | 7875 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 7876 | ` */` |
|     16 | 7877 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7878 | `{` |
|      - | 7879 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7880 | `	int nLen;` |
|     17 | 7881 | `	if( nArg < 1 ){` |
|      - | 7882 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7883 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7884 | `		return PH7_OK;` |
|      - | 7885 | `	}` |
|      - | 7886 | `	/* Extract the target string */` |
|     17 | 7887 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7888 | `	zEnd = &zIn[nLen];` |
|     17 | 7889 | `	if( nLen < 1 ){` |
|      - | 7890 | `		/* Empty string,return FALSE */` |
|      3 | 7891 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7892 | `		return PH7_OK;` |
|      - | 7893 | `	}` |
|      - | 7894 | `	/* Perform the requested operation */` |
|     14 | 7895 | `	for(;;){` |
|     29 | 7896 | `		if( zIn >= zEnd ){` |
|      - | 7897 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7898 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7899 | `			return PH7_OK;` |
|      - | 7900 | `		}` |
|     21 | 7901 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7902 | `			/* UTF-8 stream  */` |
|    ! 0 | 7903 | `			break;` |
|      - | 7904 | `		}` |
|     21 | 7905 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 7906 | `			break;` |
|      - | 7907 | `		}` |
|      - | 7908 | `		/* Point to the next character */` |
|     15 | 7909 | `		zIn++;` |
|      1 | 7910 | `	}` |
|      - | 7911 | `	/* The test failed,return FALSE */` |
|      7 | 7912 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7913 | `	return PH7_OK;` |
|      9 | 7914 | `}` |
|      - | 7915 | `/*` |
|      - | 7916 | ` * bool ctype_digit(string $text)` |
|      - | 7917 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 7918 | ` * Parameters` |
|      - | 7919 | ` *  $text` |
|      - | 7920 | ` *   The tested string.` |
|      - | 7921 | ` * Return` |
|      - | 7922 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 7923 | ` */` |
|   1615 | 7924 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7925 | `{` |
|      - | 7926 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7927 | `	int nLen;` |
|   1620 | 7928 | `	if( nArg < 1 ){` |
|      - | 7929 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7930 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7931 | `		return PH7_OK;` |
|      - | 7932 | `	}` |
|      - | 7933 | `	/* Extract the target string */` |
|   1620 | 7934 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1620 | 7935 | `	zEnd = &zIn[nLen];` |
|   1620 | 7936 | `	if( nLen < 1 ){` |
|      - | 7937 | `		/* Empty string,return FALSE */` |
|      3 | 7938 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7939 | `		return PH7_OK;` |
|      - | 7940 | `	}` |
|      - | 7941 | `	/* Perform the requested operation */` |
|   1517 | 7942 | `	for(;;){` |
|   3037 | 7943 | `		if( zIn >= zEnd ){` |
|      - | 7944 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1374 | 7945 | `			ph7_result_bool(pCtx,1);` |
|   1374 | 7946 | `			return PH7_OK;` |
|      - | 7947 | `		}` |
|   1668 | 7948 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7949 | `			/* UTF-8 stream  */` |
|    ! 0 | 7950 | `			break;` |
|      - | 7951 | `		}` |
|   1668 | 7952 | `		if( !SyisDigit(zIn[0]) ){` |
|    249 | 7953 | `			break;` |
|      - | 7954 | `		}` |
|      - | 7955 | `		/* Point to the next character */` |
|   1424 | 7956 | `		zIn++;` |
|      5 | 7957 | `	}` |
|      - | 7958 | `	/* The test failed,return FALSE */` |
|    249 | 7959 | `	ph7_result_bool(pCtx,0);` |
|    249 | 7960 | `	return PH7_OK;` |
|    813 | 7961 | `}` |
|      - | 7962 | `/*` |
|      - | 7963 | ` * bool ctype_xdigit(string $text)` |
|      - | 7964 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 7965 | ` * Parameters` |
|      - | 7966 | ` *  $text` |
|      - | 7967 | ` *   The tested string.` |
|      - | 7968 | ` * Return` |
|      - | 7969 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 7970 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 7971 | ` */` |
|     18 | 7972 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7973 | `{` |
|      - | 7974 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7975 | `	int nLen;` |
|     19 | 7976 | `	if( nArg < 1 ){` |
|      - | 7977 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7978 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7979 | `		return PH7_OK;` |
|      - | 7980 | `	}` |
|      - | 7981 | `	/* Extract the target string */` |
|     19 | 7982 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7983 | `	zEnd = &zIn[nLen];` |
|     19 | 7984 | `	if( nLen < 1 ){` |
|      - | 7985 | `		/* Empty string,return FALSE */` |
|      3 | 7986 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7987 | `		return PH7_OK;` |
|      - | 7988 | `	}` |
|      - | 7989 | `	/* Perform the requested operation */` |
|     46 | 7990 | `	for(;;){` |
|     93 | 7991 | `		if( zIn >= zEnd ){` |
|      - | 7992 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 7993 | `			ph7_result_bool(pCtx,1);` |
|     11 | 7994 | `			return PH7_OK;` |
|      - | 7995 | `		}` |
|     83 | 7996 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7997 | `			/* UTF-8 stream  */` |
|    ! 0 | 7998 | `			break;` |
|      - | 7999 | `		}` |
|     83 | 8000 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 8001 | `			break;` |
|      - | 8002 | `		}` |
|      - | 8003 | `		/* Point to the next character */` |
|     77 | 8004 | `		zIn++;` |
|      1 | 8005 | `	}` |
|      - | 8006 | `	/* The test failed,return FALSE */` |
|      7 | 8007 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8008 | `	return PH7_OK;` |
|     10 | 8009 | `}` |
|      - | 8010 | `/*` |
|      - | 8011 | ` * bool ctype_graph(string $text)` |
|      - | 8012 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 8013 | ` * Parameters` |
|      - | 8014 | ` *  $text` |
|      - | 8015 | ` *   The tested string.` |
|      - | 8016 | ` * Return` |
|      - | 8017 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 8018 | ` * (no white space), FALSE otherwise.` |
|      - | 8019 | ` */` |
|     16 | 8020 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8021 | `{` |
|      - | 8022 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8023 | `	int nLen;` |
|     17 | 8024 | `	if( nArg < 1 ){` |
|      - | 8025 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8026 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8027 | `		return PH7_OK;` |
|      - | 8028 | `	}` |
|      - | 8029 | `	/* Extract the target string */` |
|     17 | 8030 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8031 | `	zEnd = &zIn[nLen];` |
|     17 | 8032 | `	if( nLen < 1 ){` |
|      - | 8033 | `		/* Empty string,return FALSE */` |
|      3 | 8034 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8035 | `		return PH7_OK;` |
|      - | 8036 | `	}` |
|      - | 8037 | `	/* Perform the requested operation */` |
|     57 | 8038 | `	for(;;){` |
|    115 | 8039 | `		if( zIn >= zEnd ){` |
|      - | 8040 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8041 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8042 | `			return PH7_OK;` |
|      - | 8043 | `		}` |
|    107 | 8044 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8045 | `			/* UTF-8 stream  */` |
|    ! 0 | 8046 | `			break;` |
|      - | 8047 | `		}` |
|    107 | 8048 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 8049 | `			break;` |
|      - | 8050 | `		}` |
|      - | 8051 | `		/* Point to the next character */` |
|    101 | 8052 | `		zIn++;` |
|      1 | 8053 | `	}` |
|      - | 8054 | `	/* The test failed,return FALSE */` |
|      7 | 8055 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8056 | `	return PH7_OK;` |
|      9 | 8057 | `}` |
|      - | 8058 | `/*` |
|      - | 8059 | ` * bool ctype_print(string $text)` |
|      - | 8060 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 8061 | ` * Parameters` |
|      - | 8062 | ` *  $text` |
|      - | 8063 | ` *   The tested string.` |
|      - | 8064 | ` * Return` |
|      - | 8065 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 8066 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 8067 | ` *  or control function at all.` |
|      - | 8068 | ` */` |
|     16 | 8069 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8070 | `{` |
|      - | 8071 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8072 | `	int nLen;` |
|     17 | 8073 | `	if( nArg < 1 ){` |
|      - | 8074 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8075 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8076 | `		return PH7_OK;` |
|      - | 8077 | `	}` |
|      - | 8078 | `	/* Extract the target string */` |
|     17 | 8079 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8080 | `	zEnd = &zIn[nLen];` |
|     17 | 8081 | `	if( nLen < 1 ){` |
|      - | 8082 | `		/* Empty string,return FALSE */` |
|      3 | 8083 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8084 | `		return PH7_OK;` |
|      - | 8085 | `	}` |
|      - | 8086 | `	/* Perform the requested operation */` |
|     63 | 8087 | `	for(;;){` |
|    127 | 8088 | `		if( zIn >= zEnd ){` |
|      - | 8089 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8090 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8091 | `			return PH7_OK;` |
|      - | 8092 | `		}` |
|    119 | 8093 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8094 | `			/* UTF-8 stream  */` |
|    ! 0 | 8095 | `			break;` |
|      - | 8096 | `		}` |
|    119 | 8097 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 8098 | `			break;` |
|      - | 8099 | `		}` |
|      - | 8100 | `		/* Point to the next character */` |
|    113 | 8101 | `		zIn++;` |
|      1 | 8102 | `	}` |
|      - | 8103 | `	/* The test failed,return FALSE */` |
|      7 | 8104 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8105 | `	return PH7_OK;` |
|      9 | 8106 | `}` |
|      - | 8107 | `/*` |
|      - | 8108 | ` * bool ctype_punct(string $text)` |
|      - | 8109 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 8110 | ` * Parameters` |
|      - | 8111 | ` *  $text` |
|      - | 8112 | ` *   The tested string.` |
|      - | 8113 | ` * Return` |
|      - | 8114 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 8115 | ` *  digit or blank, FALSE otherwise.` |
|      - | 8116 | ` */` |
|     18 | 8117 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8118 | `{` |
|      - | 8119 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8120 | `	int nLen;` |
|     19 | 8121 | `	if( nArg < 1 ){` |
|      - | 8122 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8123 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8124 | `		return PH7_OK;` |
|      - | 8125 | `	}` |
|      - | 8126 | `	/* Extract the target string */` |
|     19 | 8127 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 8128 | `	zEnd = &zIn[nLen];` |
|     19 | 8129 | `	if( nLen < 1 ){` |
|      - | 8130 | `		/* Empty string,return FALSE */` |
|      3 | 8131 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8132 | `		return PH7_OK;` |
|      - | 8133 | `	}` |
|      - | 8134 | `	/* Perform the requested operation */` |
|     38 | 8135 | `	for(;;){` |
|     77 | 8136 | `		if( zIn >= zEnd ){` |
|      - | 8137 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8138 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8139 | `			return PH7_OK;` |
|      - | 8140 | `		}` |
|     69 | 8141 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8142 | `			/* UTF-8 stream  */` |
|    ! 0 | 8143 | `			break;` |
|      - | 8144 | `		}` |
|     69 | 8145 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 8146 | `			break;` |
|      - | 8147 | `		}` |
|      - | 8148 | `		/* Point to the next character */` |
|     61 | 8149 | `		zIn++;` |
|      1 | 8150 | `	}` |
|      - | 8151 | `	/* The test failed,return FALSE */` |
|      9 | 8152 | `	ph7_result_bool(pCtx,0);` |
|      9 | 8153 | `	return PH7_OK;` |
|     10 | 8154 | `}` |
|      - | 8155 | `/*` |
|      - | 8156 | ` * bool ctype_space(string $text)` |
|      - | 8157 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 8158 | ` * Parameters` |
|      - | 8159 | ` *  $text` |
|      - | 8160 | ` *   The tested string.` |
|      - | 8161 | ` * Return` |
|      - | 8162 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 8163 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 8164 | ` *  and form feed characters.` |
|      - | 8165 | ` */` |
|  62094 | 8166 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 8167 | `{` |
|      - | 8168 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8169 | `	int nLen;` |
|  62099 | 8170 | `	if( nArg < 1 ){` |
|      - | 8171 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8172 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8173 | `		return PH7_OK;` |
|      - | 8174 | `	}` |
|      - | 8175 | `	/* Extract the target string */` |
|  62099 | 8176 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  62099 | 8177 | `	zEnd = &zIn[nLen];` |
|  62099 | 8178 | `	if( nLen < 1 ){` |
|      - | 8179 | `		/* Empty string,return FALSE */` |
|      3 | 8180 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8181 | `		return PH7_OK;` |
|      - | 8182 | `	}` |
|      - | 8183 | `	/* Perform the requested operation */` |
|  32153 | 8184 | `	for(;;){` |
|  64225 | 8185 | `		if( zIn >= zEnd ){` |
|      - | 8186 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   2109 | 8187 | `			ph7_result_bool(pCtx,1);` |
|   2109 | 8188 | `			return PH7_OK;` |
|      - | 8189 | `		}` |
|  62121 | 8190 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8191 | `			/* UTF-8 stream  */` |
|    ! 0 | 8192 | `			break;` |
|      - | 8193 | `		}` |
|  62121 | 8194 | `		if( !SyisSpace(zIn[0]) ){` |
|  59993 | 8195 | `			break;` |
|      - | 8196 | `		}` |
|      - | 8197 | `		/* Point to the next character */` |
|   2133 | 8198 | `		zIn++;` |
|      5 | 8199 | `	}` |
|      - | 8200 | `	/* The test failed,return FALSE */` |
|  59993 | 8201 | `	ph7_result_bool(pCtx,0);` |
|  59993 | 8202 | `	return PH7_OK;` |
|  31095 | 8203 | `}` |
|      - | 8204 | `/*` |
|      - | 8205 | ` * bool ctype_lower(string $text)` |
|      - | 8206 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 8207 | ` * Parameters` |
|      - | 8208 | ` *  $text` |
|      - | 8209 | ` *   The tested string.` |
|      - | 8210 | ` * Return` |
|      - | 8211 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 8212 | ` */` |
|     16 | 8213 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8214 | `{` |
|      - | 8215 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8216 | `	int nLen;` |
|     17 | 8217 | `	if( nArg < 1 ){` |
|      - | 8218 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8219 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8220 | `		return PH7_OK;` |
|      - | 8221 | `	}` |
|      - | 8222 | `	/* Extract the target string */` |
|     17 | 8223 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8224 | `	zEnd = &zIn[nLen];` |
|     17 | 8225 | `	if( nLen < 1 ){` |
|      - | 8226 | `		/* Empty string,return FALSE */` |
|      3 | 8227 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8228 | `		return PH7_OK;` |
|      - | 8229 | `	}` |
|      - | 8230 | `	/* Perform the requested operation */` |
|     27 | 8231 | `	for(;;){` |
|     55 | 8232 | `		if( zIn >= zEnd ){` |
|      - | 8233 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 8234 | `			ph7_result_bool(pCtx,1);` |
|      5 | 8235 | `			return PH7_OK;` |
|      - | 8236 | `		}` |
|     51 | 8237 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 8238 | `			break;` |
|      - | 8239 | `		}` |
|      - | 8240 | `		/* Point to the next character */` |
|     41 | 8241 | `		zIn++;` |
|      1 | 8242 | `	}` |
|      - | 8243 | `	/* The test failed,return FALSE */` |
|     11 | 8244 | `	ph7_result_bool(pCtx,0);` |
|     11 | 8245 | `	return PH7_OK;` |
|      9 | 8246 | `}` |
|      - | 8247 | `/*` |
|      - | 8248 | ` * bool ctype_upper(string $text)` |
|      - | 8249 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 8250 | ` * Parameters` |
|      - | 8251 | ` *  $text` |
|      - | 8252 | ` *   The tested string.` |
|      - | 8253 | ` * Return` |
|      - | 8254 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 8255 | ` */` |
|     16 | 8256 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8257 | `{` |
|      - | 8258 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8259 | `	int nLen;` |
|     17 | 8260 | `	if( nArg < 1 ){` |
|      - | 8261 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8262 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8263 | `		return PH7_OK;` |
|      - | 8264 | `	}` |
|      - | 8265 | `	/* Extract the target string */` |
|     17 | 8266 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8267 | `	zEnd = &zIn[nLen];` |
|     17 | 8268 | `	if( nLen < 1 ){` |
|      - | 8269 | `		/* Empty string,return FALSE */` |
|      3 | 8270 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8271 | `		return PH7_OK;` |
|      - | 8272 | `	}` |
|      - | 8273 | `	/* Perform the requested operation */` |
|     28 | 8274 | `	for(;;){` |
|     57 | 8275 | `		if( zIn >= zEnd ){` |
|      - | 8276 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 8277 | `			ph7_result_bool(pCtx,1);` |
|      5 | 8278 | `			return PH7_OK;` |
|      - | 8279 | `		}` |
|     53 | 8280 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 8281 | `			break;` |
|      - | 8282 | `		}` |
|      - | 8283 | `		/* Point to the next character */` |
|     43 | 8284 | `		zIn++;` |
|      1 | 8285 | `	}` |
|      - | 8286 | `	/* The test failed,return FALSE */` |
|     11 | 8287 | `	ph7_result_bool(pCtx,0);` |
|     11 | 8288 | `	return PH7_OK;` |
|      9 | 8289 | `}` |
|      - | 8290 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 8291 | `/*` |
|      - | 8292 | ` * Section:` |
|      - | 8293 | ` *    URL handling Functions.` |
|      - | 8294 | ` * Status:` |
|      - | 8295 | ` *    Stable.` |
|      - | 8296 | ` */` |
|      - | 8297 | `/*` |
|      - | 8298 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 8299 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 8300 | ` */` |
|   1026 | 8301 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 8302 | `{` |
|      - | 8303 | `	/* Store in the call context result buffer */` |
|   1028 | 8304 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 8305 | `	return SXRET_OK;` |
|      2 | 8306 | `}` |
|      - | 8307 | `/*` |
|      - | 8308 | ` * string base64_encode(string $data)` |
|      - | 8309 | ` * string convert_uuencode(string $data)` |
|      - | 8310 | ` *  Encodes data with MIME base64` |
|      - | 8311 | ` * Parameter` |
|      - | 8312 | ` *  $data` |
|      - | 8313 | ` *    Data to encode` |
|      - | 8314 | ` * Return` |
|      - | 8315 | ` *  Encoded data or FALSE on failure.` |
|      - | 8316 | ` */` |
|      6 | 8317 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8318 | `{` |
|      - | 8319 | `	const char *zIn;` |
|      - | 8320 | `	int nLen;` |
|      7 | 8321 | `	if( nArg < 1 ){` |
|      - | 8322 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8323 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8324 | `		return PH7_OK;` |
|      - | 8325 | `	}` |
|      - | 8326 | `	/* Extract the input string */` |
|      7 | 8327 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8328 | `	if( nLen < 1 ){` |
|      - | 8329 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8330 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8331 | `		return PH7_OK;` |
|      - | 8332 | `	}` |
|      - | 8333 | `	/* Perform the BASE64 encoding */` |
|      7 | 8334 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 8335 | `	return PH7_OK;` |
|      4 | 8336 | `}` |
|      - | 8337 | `/*` |
|      - | 8338 | ` * string base64_decode(string $data)` |
|      - | 8339 | ` * string convert_uudecode(string $data)` |
|      - | 8340 | ` *  Decodes data encoded with MIME base64` |
|      - | 8341 | ` * Parameter` |
|      - | 8342 | ` *  $data` |
|      - | 8343 | ` *    Encoded data.` |
|      - | 8344 | ` * Return` |
|      - | 8345 | ` *  Returns the original data or FALSE on failure.` |
|      - | 8346 | ` */` |
|     34 | 8347 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8348 | `{` |
|      - | 8349 | `	const char *zIn;` |
|      - | 8350 | `	int nLen;` |
|     36 | 8351 | `	if( nArg < 1 ){` |
|      - | 8352 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8353 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8354 | `		return PH7_OK;` |
|      - | 8355 | `	}` |
|      - | 8356 | `	/* Extract the input string */` |
|     36 | 8357 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 8358 | `	if( nLen < 1 ){` |
|      - | 8359 | `		/* Nothing to process,return FALSE */` |
|      3 | 8360 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8361 | `		return PH7_OK;` |
|      - | 8362 | `	}` |
|      - | 8363 | `	/* Perform the BASE64 decoding */` |
|     34 | 8364 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 8365 | `	return PH7_OK;` |
|     19 | 8366 | `}` |
|      - | 8367 | `/*` |
|      - | 8368 | ` * string urlencode(string $str)` |
|      - | 8369 | ` *  URL encoding` |
|      - | 8370 | ` * Parameter` |
|      - | 8371 | ` *  $data` |
|      - | 8372 | ` *   Input string.` |
|      - | 8373 | ` * Return` |
|      - | 8374 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 8375 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 8376 | ` *  encoded as plus (+) signs.` |
|      - | 8377 | ` */` |
|      4 | 8378 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8379 | `{` |
|      - | 8380 | `	const char *zIn;` |
|      - | 8381 | `	int nLen;` |
|      5 | 8382 | `	if( nArg < 1 ){` |
|      - | 8383 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8384 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8385 | `		return PH7_OK;` |
|      - | 8386 | `	}` |
|      - | 8387 | `	/* Extract the input string */` |
|      5 | 8388 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 8389 | `	if( nLen < 1 ){` |
|      - | 8390 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8391 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8392 | `		return PH7_OK;` |
|      - | 8393 | `	}` |
|      - | 8394 | `	/* Perform the URL encoding */` |
|      5 | 8395 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 8396 | `	return PH7_OK;` |
|      3 | 8397 | `}` |
|      - | 8398 | `/*` |
|      - | 8399 | ` * string urldecode(string $str)` |
|      - | 8400 | ` *  Decodes any %## encoding in the given string.` |
|      - | 8401 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 8402 | ` * Parameter` |
|      - | 8403 | ` *  $data` |
|      - | 8404 | ` *    Input string.` |
|      - | 8405 | ` * Return` |
|      - | 8406 | ` *  Decoded URL or FALSE on failure.` |
|      - | 8407 | ` */` |
|      6 | 8408 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8409 | `{` |
|      - | 8410 | `	const char *zIn;` |
|      - | 8411 | `	int nLen;` |
|      7 | 8412 | `	if( nArg < 1 ){` |
|      - | 8413 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8414 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8415 | `		return PH7_OK;` |
|      - | 8416 | `	}` |
|      - | 8417 | `	/* Extract the input string */` |
|      7 | 8418 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8419 | `	if( nLen < 1 ){` |
|      - | 8420 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8421 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8422 | `		return PH7_OK;` |
|      - | 8423 | `	}` |
|      - | 8424 | `	/* Perform the URL decoding */` |
|      7 | 8425 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 8426 | `	return PH7_OK;` |
|      4 | 8427 | `}` |
|      - | 8428 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8429 | `/* Table of the built-in functions */` |
|      - | 8430 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 8431 | `	   /* Variable handling functions */` |
|      - | 8432 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 8433 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 8434 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 8435 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 8436 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 8437 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 8438 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 8439 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 8440 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 8441 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 8442 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 8443 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 8444 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 8445 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 8446 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 8447 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 8448 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 8449 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 8450 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 8451 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 8452 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8453 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 8454 | `	   /* Math functions */` |
|      - | 8455 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 8456 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 8457 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 8458 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 8459 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 8460 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 8461 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 8462 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 8463 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 8464 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 8465 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 8466 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 8467 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 8468 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 8469 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 8470 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 8471 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 8472 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 8473 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 8474 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 8475 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 8476 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 8477 | `	{ "round",    PH7_builtin_round        },` |
|      - | 8478 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 8479 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 8480 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 8481 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 8482 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 8483 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 8484 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 8485 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 8486 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 8487 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8488 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8489 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 8490 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8491 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8492 | `	   /* String handling functions */` |
|      - | 8493 |  |
|      - | 8494 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 8495 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 8496 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 8497 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 8498 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 8499 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 8500 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 8501 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 8502 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 8503 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 8504 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 8505 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 8506 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 8507 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 8508 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 8509 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 8510 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 8511 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 8512 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 8513 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 8514 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 8515 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 8516 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 8517 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 8518 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 8519 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 8520 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 8521 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 8522 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 8523 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 8524 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 8525 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 8526 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 8527 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 8528 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 8529 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 8530 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 8531 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 8532 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 8533 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 8534 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 8535 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 8536 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 8537 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 8538 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 8539 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 8540 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 8541 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 8542 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 8543 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 8544 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 8545 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 8546 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8547 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8548 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 8549 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 8550 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 8551 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 8552 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8553 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8554 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 8555 |  |
|      - | 8556 |  |
|      - | 8557 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 8558 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 8559 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 8560 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 8561 | `	{ "hash",         PH7_builtin_hash      },` |
|      - | 8562 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|      - | 8563 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|      - | 8564 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|      - | 8565 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 8566 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|      - | 8567 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|      - | 8568 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|      - | 8569 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|      - | 8570 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|      - | 8571 | `	{ "filter_input",          PH7_builtin_filter_input },` |
|      - | 8572 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8573 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8574 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 8575 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 8576 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8577 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8578 |  |
|      - | 8579 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 8580 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 8581 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 8582 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 8583 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 8584 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 8585 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 8586 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 8587 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 8588 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 8589 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 8590 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 8591 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8592 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8593 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 8594 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8595 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8596 |  |
|      - | 8597 | `	         /* Ctype functions */` |
|      - | 8598 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 8599 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 8600 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 8601 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 8602 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 8603 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 8604 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 8605 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 8606 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 8607 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 8608 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 8609 | `	         /* Time functions */` |
|      - | 8610 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 8611 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 8612 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 8613 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 8614 | `	{ "date",        PH7_builtin_date         },` |
|      - | 8615 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 8616 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 8617 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 8618 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 8619 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 8620 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 8621 | `	        /* URL functions */` |
|      - | 8622 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 8623 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 8624 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 8625 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 8626 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 8627 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 8628 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 8629 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 8630 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8631 | `};` |
|      - | 8632 | `/*` |
|      - | 8633 | ` * Register the built-in functions defined above,the array functions` |
|      - | 8634 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 8635 | ` */` |
|   3472 | 8636 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      5 | 8637 | `{` |
|      - | 8638 | `	sxu32 n;` |
| 583301 | 8639 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 579829 | 8640 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 289917 | 8641 | `	}` |
|      - | 8642 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   3477 | 8643 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 8644 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   3477 | 8645 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   3477 | 8646 | `}` |
|      - | 8647 |  |
