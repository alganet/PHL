# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 3835/4460 lines (85.99%)

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
|  33436 |  303 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  304 | `{` |
|  33441 |  305 | `	int res = 1; /* Assume empty by default */` |
|  33441 |  306 | `	if( nArg > 0 ){` |
|  33439 |  307 | `		res = ph7_value_is_empty(apArg[0]);` |
|  16717 |  308 | `	}` |
|  33441 |  309 | `	ph7_result_bool(pCtx,res);` |
|  33441 |  310 | `	return PH7_OK;` |
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
| 215888 |  353 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  354 | `{` |
|      - |  355 | `	const char *zSource,*zOfft;` |
|      - |  356 | `	int nOfft,nLen,nSrcLen;` |
| 215893 |  357 | `	if( nArg < 2 ){` |
|      - |  358 | `		/* return FALSE */` |
|    ! 0 |  359 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  360 | `		return PH7_OK;` |
|      - |  361 | `	}` |
|      - |  362 | `	/* Extract the target string */` |
| 215893 |  363 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 215893 |  364 | `	if( nSrcLen < 1 ){` |
|      - |  365 | `		/* Empty string,return FALSE */` |
|  11749 |  366 | `		ph7_result_bool(pCtx,0);` |
|  11749 |  367 | `		return PH7_OK;` |
|      - |  368 | `	}` |
| 204149 |  369 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  370 | `	/* Extract the offset */` |
| 204149 |  371 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 204149 |  372 | `	if( nOfft < 0 ){` |
|  32229 |  373 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  32229 |  374 | `		if( zOfft < zSource ){` |
|      - |  375 | `			/* Invalid offset */` |
|      5 |  376 | `			ph7_result_bool(pCtx,0);` |
|      5 |  377 | `			return PH7_OK;` |
|      - |  378 | `		}` |
|  32225 |  379 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  32225 |  380 | `		nOfft = (int)(zOfft-zSource);` |
| 188035 |  381 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  382 | `		/* Invalid offset */` |
|    215 |  383 | `		ph7_result_bool(pCtx,0);` |
|    215 |  384 | `		return PH7_OK;` |
|    ! 0 |  385 | `	}else{` |
| 171715 |  386 | `		zOfft = &zSource[nOfft];` |
| 171715 |  387 | `		nLen = nSrcLen - nOfft;` |
|      - |  388 | `	}` |
| 203935 |  389 | `	if( nArg > 2 ){` |
|      - |  390 | `		/* Extract the length */` |
| 167651 |  391 | `		nLen = ph7_value_to_int(apArg[2]);` |
| 167651 |  392 | `		if( nLen == 0 ){` |
|      - |  393 | `			/* Invalid length,return an empty string */` |
|      5 |  394 | `			ph7_result_string(pCtx,"",0);` |
|      5 |  395 | `			return PH7_OK;` |
| 167647 |  396 | `		}else if( nLen < 0 ){` |
|  32217 |  397 | `			nLen = nSrcLen + nLen - nOfft;` |
|  32217 |  398 | `			if( nLen < 1 ){` |
|      - |  399 | `				/* Invalid  length */` |
|      3 |  400 | `				nLen = nSrcLen - nOfft;` |
|      1 |  401 | `			}` |
|  16106 |  402 | `		}` |
| 167647 |  403 | `		if( nLen + nOfft > nSrcLen ){` |
|      - |  404 | `			/* Invalid length */` |
|   5219 |  405 | `			nLen = nSrcLen - nOfft;` |
|   2607 |  406 | `		}` |
|  83821 |  407 | `	}` |
|      - |  408 | `	/* Return the substring */` |
| 203931 |  409 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 203931 |  410 | `	return PH7_OK;` |
| 107949 |  411 | `}` |
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
| 136016 | 1332 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 1333 | `{` |
|  68008 | 1334 | `	SXUNUSED(pKey);` |
| 136021 | 1335 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 1336 | `	const char *zData;` |
|      - | 1337 | `	int nLen;` |
| 136021 | 1338 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
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
| 136019 | 1362 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 1363 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 136019 | 1364 | `	if( pData->bFirst ){` |
|  32599 | 1365 | `		pData->bFirst = 0;` |
| 119722 | 1366 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 1367 | `		/* append the separator first */` |
| 103413 | 1368 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 1369 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1370 | `			return PH7_ABORT;` |
|      - | 1371 | `		}` |
|  51704 | 1372 | `	}` |
|      - | 1373 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 136019 | 1374 | `	if( nLen > 0 ){` |
| 124275 | 1375 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 1376 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 1377 | `			return PH7_ABORT;` |
|      - | 1378 | `		}` |
|  62135 | 1379 | `	}` |
| 136019 | 1380 | `	return PH7_OK;` |
|  68013 | 1381 | `}` |
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
|  32616 | 1395 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1396 | `{` |
|      - | 1397 | `	struct implode_data imp_data;` |
|  32621 | 1398 | `	int i = 1;` |
|  32621 | 1399 | `	if( nArg < 1 ){` |
|      - | 1400 | `		/* Missing argument,return NULL */` |
|    ! 0 | 1401 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1402 | `		return PH7_OK;` |
|      - | 1403 | `	}` |
|      - | 1404 | `	/* Prepare the implode context */` |
|  32621 | 1405 | `	imp_data.pCtx = pCtx;` |
|  32621 | 1406 | `	imp_data.bRecursive = 0;` |
|  32621 | 1407 | `	imp_data.bFirst = 1;` |
|  32621 | 1408 | `	imp_data.nRecCount = 0;` |
|  32621 | 1409 | `	imp_data.rc = SXRET_OK;` |
|  32621 | 1410 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  32619 | 1411 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  16312 | 1412 | `	}else{` |
|      3 | 1413 | `		imp_data.zSep = 0;` |
|      3 | 1414 | `		imp_data.nSeplen = 0;` |
|      3 | 1415 | `		i = 0;` |
|      - | 1416 | `	}` |
|  32621 | 1417 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 1418 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 1419 | `	}` |
|      - | 1420 | `	/* Start the 'join' process */` |
|  65237 | 1421 | `	while( i < nArg ){` |
|  32621 | 1422 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 1423 | `			/* Iterate throw array entries */` |
|  32621 | 1424 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 1425 | `			/* Surface a callback allocation failure as a fatal */` |
|  32621 | 1426 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 1427 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1428 | `			}` |
|  16313 | 1429 | `		}else{` |
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
|  32621 | 1449 | `		i++;` |
|      5 | 1450 | `	}` |
|  32621 | 1451 | `	return PH7_OK;` |
|  16313 | 1452 | `}` |
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
|   6324 | 1552 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1553 | `{` |
|      - | 1554 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 1555 | `	int nDelim,nStrlen,iLimit;` |
|      - | 1556 | `	ph7_value *pArray;` |
|      - | 1557 | `	ph7_value *pValue;` |
|      - | 1558 | `	sxu32 nOfft;` |
|      - | 1559 | `	sxi32 rc;` |
|   6329 | 1560 | `	if( nArg < 2 ){` |
|      - | 1561 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 1562 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1563 | `		return PH7_OK;` |
|      - | 1564 | `	}` |
|      - | 1565 | `	/* Extract the delimiter */` |
|   6329 | 1566 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   6329 | 1567 | `	if( nDelim < 1 ){` |
|      - | 1568 | `		/* Empty delimiter: PHP 8 throws a catchable ValueError. */` |
|      3 | 1569 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1570 | `			"explode(): Argument #1 ($separator) must not be empty");` |
|      - | 1571 | `	}` |
|      - | 1572 | `	/* Extract the string */` |
|   6327 | 1573 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   6327 | 1574 | `	if( nStrlen < 1 ){` |
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
|   6321 | 1600 | `	zEnd = &zString[nStrlen];` |
|      - | 1601 | `	/* Create the array */` |
|   6321 | 1602 | `	pArray =  ph7_context_new_array(pCtx);` |
|   6321 | 1603 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   6321 | 1604 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 1605 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 1606 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 1607 | `		return PH7_OK;` |
|      - | 1608 | `	}` |
|      - | 1609 | `	/* Set a defualt limit */` |
|   6321 | 1610 | `	iLimit = SXI32_HIGH;` |
|   6321 | 1611 | `	if( nArg > 2 ){` |
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
|  73617 | 1646 | `	for(;;){` |
| 147239 | 1647 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 147239 | 1648 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 1649 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   6305 | 1650 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   6305 | 1651 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 1652 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 1653 | `			}` |
|   6305 | 1654 | `			break;` |
|      - | 1655 | `		}` |
|      - | 1656 | `		/* Point to the desired offset */` |
| 140939 | 1657 | `		zCur = &zString[nOfft];` |
|      - | 1658 | `		/* Perform the store operation (may be empty) */` |
| 140939 | 1659 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 140939 | 1660 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 1661 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 1662 | `		}` |
|      - | 1663 | `		/* Point beyond the delimiter */` |
| 140939 | 1664 | `		zString = &zCur[nDelim];` |
|      - | 1665 | `		/* Reset the cursor */` |
| 140939 | 1666 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 1667 | `	}` |
|      - | 1668 | `	/* Return the freshly created array */` |
|   6305 | 1669 | `	ph7_result_value(pCtx,pArray);` |
|      - | 1670 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 1671 | `	 * released as soon we return from this foregin function.` |
|      - | 1672 | `	 */` |
|   6305 | 1673 | `	return PH7_OK;` |
|   3167 | 1674 | `}` |
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
|  13946 | 1690 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1691 | `{` |
|      - | 1692 | `	const char *zString;` |
|      - | 1693 | `	int nLen;` |
|  13951 | 1694 | `	if( nArg < 1 ){` |
|      - | 1695 | `		/* Missing arguments,return null */` |
|    ! 0 | 1696 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1697 | `		return PH7_OK;` |
|      - | 1698 | `	}` |
|      - | 1699 | `	/* Extract the target string */` |
|  13951 | 1700 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  13951 | 1701 | `	if( nLen < 1 ){` |
|      - | 1702 | `		/* Empty string,return */` |
|   1309 | 1703 | `		ph7_result_string(pCtx,"",0);` |
|   1309 | 1704 | `		return PH7_OK;` |
|      - | 1705 | `	}` |
|      - | 1706 | `	/* Start the trim process */` |
|  12647 | 1707 | `	if( nArg < 2 ){` |
|      - | 1708 | `		SyString sStr;` |
|      - | 1709 | `		/* Remove white spaces and NUL bytes */` |
|  12617 | 1710 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  31513 | 1711 | `		SyStringFullTrimSafe(&sStr);` |
|  12617 | 1712 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   6311 | 1713 | `	}else{` |
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
|  12647 | 1742 | `	return PH7_OK;` |
|   6978 | 1743 | `}` |
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
|  32212 | 1883 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1884 | `{` |
|      - | 1885 | `	const char *zString,*zCur,*zEnd;` |
|      - | 1886 | `	int nLen;` |
|  32217 | 1887 | `	if( nArg < 1 ){` |
|      - | 1888 | `		/* Missing arguments,return null */` |
|    ! 0 | 1889 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1890 | `		return PH7_OK;` |
|      - | 1891 | `	}` |
|      - | 1892 | `	/* Extract the target string */` |
|  32217 | 1893 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  32217 | 1894 | `	if( nLen < 1 ){` |
|      - | 1895 | `		/* Empty string,return */` |
|      3 | 1896 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1897 | `		return PH7_OK;` |
|      - | 1898 | `	}` |
|      - | 1899 | `	/* Perform the requested operation */` |
|  32215 | 1900 | `	zEnd = &zString[nLen];` |
| 101358 | 1901 | `	for(;;){` |
| 202721 | 1902 | `		if( zString >= zEnd ){` |
|      - | 1903 | `			/* No more input,break immediately */` |
|  32215 | 1904 | `			break;` |
|      - | 1905 | `		}` |
| 170511 | 1906 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 1907 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 1908 | `			zCur = zString;` |
|    ! 0 | 1909 | `			zString++;` |
|    ! 0 | 1910 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 1911 | `				zString++;` |
|    ! 0 | 1912 | `			}` |
|      - | 1913 | `			/* Append UTF-8 stream */` |
|    ! 0 | 1914 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 1915 | `		}else{` |
| 170511 | 1916 | `			int c = zString[0];` |
| 170511 | 1917 | `			if( SyisUpper(c) ){` |
| 170509 | 1918 | `				c = SyToLower(zString[0]);` |
|  85252 | 1919 | `			}` |
|      - | 1920 | `			/* Append character */` |
| 170511 | 1921 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 1922 | `			/* Advance the cursor */` |
| 170511 | 1923 | `			zString++;` |
|      - | 1924 | `		}` |
|      5 | 1925 | `	}` |
|  32215 | 1926 | `	return PH7_OK;` |
|  16111 | 1927 | `}` |
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
|      - | 2981 | ` * string ucwords(string $string [, string $separators = " \t\r\n\f\v"])` |
|      - | 2982 | ` *  Uppercase the first character of each word in a string.` |
|      - | 2983 | ` *  A word begins at the start of the string and after any character present in` |
|      - | 2984 | ` *  $separators. The default separators are the whitespace characters (space,` |
|      - | 2985 | ` *  horizontal tab, carriage return, newline, form-feed and vertical tab); an` |
|      - | 2986 | ` *  explicit $separators argument REPLACES them (an empty string leaves only the` |
|      - | 2987 | ` *  very first character upper-cased). Like PHP, this is byte-based: only ASCII` |
|      - | 2988 | ` *  bytes are upper-cased and a byte is a separator only if it appears in the set.` |
|      - | 2989 | ` * Parameters` |
|      - | 2990 | ` *  $string` |
|      - | 2991 | ` *   The input string.` |
|      - | 2992 | ` *  $separators` |
|      - | 2993 | ` *   The optional word-boundary characters.` |
|      - | 2994 | ` * Return` |
|      - | 2995 | ` *  The modified string.` |
|      - | 2996 | ` */` |
|     22 | 2997 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2998 | `{` |
|      - | 2999 | `	const char *zIn;` |
|      - | 3000 | `	int nLen,i,iStart;` |
|      - | 3001 | `	char aDelim[256];` |
|     23 | 3002 | `	if( nArg < 1 ){` |
|      - | 3003 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3004 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3005 | `		return PH7_OK;` |
|      - | 3006 | `	}` |
|      - | 3007 | `	/* Build the separator membership table: an explicit $separators argument` |
|      - | 3008 | `	 * replaces the default whitespace set (an empty string clears it). */` |
|     23 | 3009 | `	SyZero(aDelim,(sxu32)sizeof(aDelim));` |
|     23 | 3010 | `	if( nArg > 1 ){` |
|      - | 3011 | `		int nDelim;` |
|      9 | 3012 | `		const char *zDelim = ph7_value_to_string(apArg[1],&nDelim);` |
|     17 | 3013 | `		for( i = 0 ; i < nDelim ; i++ ){` |
|      9 | 3014 | `			aDelim[(unsigned char)zDelim[i]] = 1;` |
|      5 | 3015 | `		}` |
|      5 | 3016 | `	}else{` |
|     15 | 3017 | `		aDelim[(unsigned char)' ']  = 1;` |
|     15 | 3018 | `		aDelim[(unsigned char)'\t'] = 1;` |
|     15 | 3019 | `		aDelim[(unsigned char)'\r'] = 1;` |
|     15 | 3020 | `		aDelim[(unsigned char)'\n'] = 1;` |
|     15 | 3021 | `		aDelim[(unsigned char)'\f'] = 1;` |
|     15 | 3022 | `		aDelim[(unsigned char)'\v'] = 1;` |
|      - | 3023 | `	}` |
|      - | 3024 | `	/* Extract the target string */` |
|     23 | 3025 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3026 | `	if( nLen < 1 ){` |
|      - | 3027 | `		/* Empty string – match PHP semantics */` |
|      3 | 3028 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3029 | `		return PH7_OK;` |
|      - | 3030 | `	}` |
|      - | 3031 | `	/* Upper-case the first byte of each word (the leading byte, or any byte that` |
|      - | 3032 | `	 * follows a separator), appending the untouched runs in between verbatim. */` |
|     21 | 3033 | `	iStart = 0;` |
|    309 | 3034 | `	for( i = 0 ; i < nLen ; i++ ){` |
|    289 | 3035 | `		int c = (unsigned char)zIn[i];` |
|    289 | 3036 | `		if( (i == 0 \|\| aDelim[(unsigned char)zIn[i-1]]) && c < 0x80 && SyisLower(c) ){` |
|     53 | 3037 | `			char up = (char)SyToUpper(c);` |
|     53 | 3038 | `			if( i > iStart ){` |
|     35 | 3039 | `				ph7_result_string(pCtx,&zIn[iStart],i - iStart);` |
|     17 | 3040 | `			}` |
|     53 | 3041 | `			ph7_result_string(pCtx,&up,1);` |
|     53 | 3042 | `			iStart = i + 1;` |
|     26 | 3043 | `		}` |
|    145 | 3044 | `	}` |
|     21 | 3045 | `	if( nLen > iStart ){` |
|     21 | 3046 | `		ph7_result_string(pCtx,&zIn[iStart],nLen - iStart);` |
|     10 | 3047 | `	}` |
|     21 | 3048 | `	return PH7_OK;` |
|     12 | 3049 | `}` |
|      - | 3050 | `/*` |
|      - | 3051 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3052 | ` *  Returns input repeated multiplier times.` |
|      - | 3053 | ` * Parameters` |
|      - | 3054 | ` *  $string` |
|      - | 3055 | ` *   String to be repeated.` |
|      - | 3056 | ` * $multiplier` |
|      - | 3057 | ` *  Number of time the input string should be repeated.` |
|      - | 3058 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3059 | ` *  to 0, the function will return an empty string.` |
|      - | 3060 | ` * Return` |
|      - | 3061 | ` *  The repeated string.` |
|      - | 3062 | ` */` |
|  20430 | 3063 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3064 | `{` |
|      - | 3065 | `	const char *zIn;` |
|      - | 3066 | `	int nLen;` |
|      - | 3067 | `	ph7_int64 nMul;` |
|      - | 3068 | `	int rc;` |
|  20432 | 3069 | `	if( nArg < 2 ){` |
|      - | 3070 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3071 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3072 | `		return PH7_OK;` |
|      - | 3073 | `	}` |
|      - | 3074 | `	/* Extract the target string */` |
|  20432 | 3075 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3076 | `	/* Extract the multiplier as a 64-bit value (a 32-bit read would wrap a large` |
|      - | 3077 | `	 * positive $times into a negative one and trip a spurious ValueError). PHP` |
|      - | 3078 | `	 * validates $times regardless of the string contents: a negative count throws` |
|      - | 3079 | `	 * a catchable ValueError. */` |
|  20432 | 3080 | `	nMul = ph7_value_to_int64(apArg[1]);` |
|  20432 | 3081 | `	if( nMul < 0 ){` |
|      3 | 3082 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 3083 | `			"str_repeat(): Argument #2 ($times) must be greater than or equal to 0");` |
|      - | 3084 | `	}` |
|  20430 | 3085 | `	if( nLen < 1 \|\| nMul < 1 ){` |
|      - | 3086 | `		/* Empty input or a zero multiplier yields the empty string (PHP). */` |
|    ! 0 | 3087 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3088 | `		return PH7_OK;` |
|      - | 3089 | `	}` |
|      - | 3090 | `	/* Perform the requested operation */` |
| 221628 | 3091 | `	for(;;){` |
| 443258 | 3092 | `		if( !nMul ){` |
|  20430 | 3093 | `			break;` |
|      - | 3094 | `		}` |
|      - | 3095 | `		/* Append the copy */` |
| 422830 | 3096 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 422830 | 3097 | `		if( rc != PH7_OK ){` |
|      - | 3098 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 3099 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 3100 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 3101 | `		}` |
| 422830 | 3102 | `		nMul--;` |
|      2 | 3103 | `	}` |
|  20430 | 3104 | `	return PH7_OK;` |
|  10217 | 3105 | `}` |
|      - | 3106 | `/*` |
|      - | 3107 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3108 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3109 | ` * Parameters` |
|      - | 3110 | ` *  $string` |
|      - | 3111 | ` *   The input string.` |
|      - | 3112 | ` * $is_xhtml` |
|      - | 3113 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3114 | ` * Return` |
|      - | 3115 | ` *  The processed string.` |
|      - | 3116 | ` */` |
|      4 | 3117 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3118 | `{` |
|      - | 3119 | `	const char *zIn,*zCur,*zEnd;` |
|      5 | 3120 | `	int is_xhtml = 1; /* Default to XHTML-style '<br />' like PHP */` |
|      - | 3121 | `	int nLen;` |
|      5 | 3122 | `	if( nArg < 1 ){` |
|      - | 3123 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 3124 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3125 | `		return PH7_OK;` |
|      - | 3126 | `	}` |
|      - | 3127 | `	/* Extract the target string */` |
|      5 | 3128 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3129 | `	if( nLen < 1 ){` |
|      - | 3130 | `		/* Empty string,return null */` |
|    ! 0 | 3131 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3132 | `		return PH7_OK;` |
|      - | 3133 | `	}` |
|      5 | 3134 | `	if( nArg > 1 ){` |
|      3 | 3135 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3136 | `	}` |
|      5 | 3137 | `	zEnd = &zIn[nLen];` |
|      - | 3138 | `	/* Perform the requested operation */` |
|      4 | 3139 | `	for(;;){` |
|      9 | 3140 | `		zCur = zIn;` |
|      - | 3141 | `		/* Delimit the string */` |
|     21 | 3142 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3143 | `			zIn++;` |
|      1 | 3144 | `		}` |
|      9 | 3145 | `		if( zCur < zIn ){` |
|      - | 3146 | `			/* Output chunk verbatim */` |
|      9 | 3147 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3148 | `		}` |
|      9 | 3149 | `		if( zIn >= zEnd ){` |
|      - | 3150 | `			/* No more input to process */` |
|      5 | 3151 | `			break;` |
|      - | 3152 | `		}` |
|      - | 3153 | `		/* Output the HTML line break */` |
|      - | 3154 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br />' (space before the slash), otherwise use '<br>' */` |
|      5 | 3155 | `		if( is_xhtml ){` |
|      3 | 3156 | `			ph7_result_string(pCtx,"<br />",(int)sizeof("<br />")-1);` |
|      2 | 3157 | `		}else{` |
|      3 | 3158 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3159 | `		}` |
|      5 | 3160 | `		zCur = zIn;` |
|      - | 3161 | `		/* Append trailing line */` |
|     11 | 3162 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3163 | `			zIn++;` |
|      1 | 3164 | `		}` |
|      5 | 3165 | `		if( zCur < zIn ){` |
|      - | 3166 | `			/* Output chunk verbatim */` |
|      5 | 3167 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3168 | `		}` |
|      1 | 3169 | `	}` |
|      5 | 3170 | `	return PH7_OK;` |
|      3 | 3171 | `}` |
|      - | 3172 | `/*` |
|      - | 3173 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3174 | ` *  According to the PHP reference manual.` |
|      - | 3175 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3176 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3177 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3178 | ` * This applies to both sprintf() and printf().` |
|      - | 3179 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3180 | ` * or more of these elements, in order:` |
|      - | 3181 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3182 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3183 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 3184 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 3185 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 3186 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 3187 | ` *   it with a single quote ('). See the examples below.` |
|      - | 3188 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 3189 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 3190 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 3191 | ` *   should result in.` |
|      - | 3192 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 3193 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 3194 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 3195 | ` *   limit to the string.` |
|      - | 3196 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 3197 | ` *       % - a literal percent character. No argument is required.` |
|      - | 3198 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 3199 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 3200 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 3201 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 3202 | ` * 	     for the number of digits after the decimal point.` |
|      - | 3203 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 3204 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 3205 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 3206 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 3207 | ` *       g - shorter of %e and %f.` |
|      - | 3208 | ` *       G - shorter of %E and %f.` |
|      - | 3209 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 3210 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 3211 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 3212 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 3213 | ` */` |
|      - | 3214 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 3215 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 3216 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 3217 | `/*` |
|      - | 3218 | `** Conversion types fall into various categories as defined by the` |
|      - | 3219 | `** following enumeration.` |
|      - | 3220 | `*/` |
|      - | 3221 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 3222 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 3223 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 3224 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 3225 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 3226 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 3227 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 3228 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 3229 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 3230 |  |
|      - | 3231 | `/*` |
|      - | 3232 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 3233 | `*/` |
|      - | 3234 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 3235 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 3236 | `/*` |
|      - | 3237 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 3238 | `** by an instance of the following structure` |
|      - | 3239 | `*/` |
|      - | 3240 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 3241 | `struct ph7_fmt_info` |
|      - | 3242 | `{` |
|      - | 3243 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 3244 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 3245 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 3246 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 3247 | `  char *charset; /* The character set for conversion */` |
|      - | 3248 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 3249 | `};` |
|      - | 3250 | `/* PH7_PhpFloatShape (php's float-shape post-processing) lives in memobj.c —` |
|      - | 3251 | ` * the default float->string cast needs it even when this whole formatting` |
|      - | 3252 | ` * region is compiled out by PH7_DISABLE_DISK_IO. */` |
|      - | 3253 | `/*` |
|      - | 3254 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 3255 | ` * used conversion types first.` |
|      - | 3256 | ` */` |
|      - | 3257 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 3258 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 3259 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 3260 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 3261 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 3262 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 3263 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 3264 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 3265 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 3266 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3267 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 3268 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 3269 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 3270 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 3271 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 3272 | `  /* php's 'h'/'H' are the locale-independent twins of 'g'/'G'; PHL always` |
|      - | 3273 | `   * formats in the C locale, so they behave identically. */` |
|      - | 3274 | `  {  'h',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 3275 | `  {  'H',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 3276 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 3277 | `};` |
|      - | 3278 | `/*` |
|      - | 3279 | ` * PHP 8 raises a catchable ValueError for an unknown conversion specifier` |
|      - | 3280 | ` * (e.g. "%y", or the C-ism "%#x" — '#' is not a php flag). Because printf()` |
|      - | 3281 | ` * and fprintf() stream their output incrementally while sprintf() buffers it,` |
|      - | 3282 | ` * every format builtin calls PH7_FormatValidate (below) to check the whole` |
|      - | 3283 | ` * format string BEFORE formatting so the throw happens with no partial output` |
|      - | 3284 | ` * escaping (php buffers the entire result and only emits it on success). This` |
|      - | 3285 | ` * scan mirrors the specifier-locating logic of the main format loop below.` |
|      - | 3286 | ` * On the first unknown specifier, stores it in *pBad and returns TRUE; returns` |
|      - | 3287 | ` * FALSE when every specifier is known. (A found-flag rather than a sentinel` |
|      - | 3288 | ` * char, so a NUL specifier byte — "%\0" — is still reported, not mistaken for` |
|      - | 3289 | ` * "all valid".)` |
|      - | 3290 | ` */` |
|    332 | 3291 | `static int FormatUnknownSpec(const char *zIn,int nByte,int *pBad)` |
|      1 | 3292 | `{` |
|    333 | 3293 | `	const char *zEnd = &zIn[nByte];` |
|      - | 3294 | `	int c,idx;` |
|   3161 | 3295 | `	while( zIn < zEnd ){` |
|   2849 | 3296 | `		if( zIn[0] != '%' ){` |
|   2201 | 3297 | `			zIn++;` |
|   2201 | 3298 | `			continue;` |
|      - | 3299 | `		}` |
|    649 | 3300 | `		zIn++; /* jump the percent sign */` |
|      - | 3301 | `		/* php-supported flags: '-', '+', ' ', '0' and the "'<pad>'" custom-pad` |
|      - | 3302 | `		 * form. '#' is intentionally NOT treated as a flag so it surfaces as an` |
|      - | 3303 | `		 * unknown specifier, matching php. */` |
|    691 | 3304 | `		while( zIn < zEnd ){` |
|    689 | 3305 | `			c = zIn[0];` |
|    689 | 3306 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|     43 | 3307 | `				zIn++;` |
|     43 | 3308 | `				continue;` |
|      - | 3309 | `			}` |
|    647 | 3310 | `			if( c=='\'' ){` |
|    ! 0 | 3311 | `				zIn++;` |
|    ! 0 | 3312 | `				if( zIn < zEnd ){` |
|    ! 0 | 3313 | `					zIn++; /* the custom pad character */` |
|    ! 0 | 3314 | `				}` |
|    ! 0 | 3315 | `				continue;` |
|      - | 3316 | `			}` |
|    647 | 3317 | `			break;` |
|    ! 0 | 3318 | `		}` |
|      - | 3319 | `		/* field width */` |
|    723 | 3320 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|     75 | 3321 | `			zIn++;` |
|      1 | 3322 | `		}` |
|      - | 3323 | `		/* positional specifier ($) — php parses flags AFTER it (e.g. "%1$-10s"),` |
|      - | 3324 | `		 * so skip the full flag set and width again, mirroring the main loop. */` |
|    649 | 3325 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
|    ! 0 | 3326 | `			zIn++;` |
|    ! 0 | 3327 | `			while( zIn < zEnd ){` |
|    ! 0 | 3328 | `				c = zIn[0];` |
|    ! 0 | 3329 | `				if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|    ! 0 | 3330 | `					zIn++;` |
|    ! 0 | 3331 | `					continue;` |
|      - | 3332 | `				}` |
|    ! 0 | 3333 | `				if( c=='\'' ){` |
|    ! 0 | 3334 | `					zIn++;` |
|    ! 0 | 3335 | `					if( zIn < zEnd ){` |
|    ! 0 | 3336 | `						zIn++;` |
|    ! 0 | 3337 | `					}` |
|    ! 0 | 3338 | `					continue;` |
|      - | 3339 | `				}` |
|    ! 0 | 3340 | `				break;` |
|    ! 0 | 3341 | `			}` |
|    ! 0 | 3342 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    ! 0 | 3343 | `				zIn++;` |
|    ! 0 | 3344 | `			}` |
|    ! 0 | 3345 | `		}` |
|      - | 3346 | `		/* precision */` |
|    649 | 3347 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|     87 | 3348 | `			zIn++;` |
|    183 | 3349 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|     97 | 3350 | `				zIn++;` |
|      1 | 3351 | `			}` |
|     43 | 3352 | `		}` |
|      - | 3353 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|    649 | 3354 | `		if( zIn < zEnd && zIn[0]=='l' ){` |
|     11 | 3355 | `			zIn++;` |
|      5 | 3356 | `		}` |
|    649 | 3357 | `		if( zIn >= zEnd ){` |
|      - | 3358 | `			/* A dangling '%' with no specifier: PHL's legacy path silently` |
|      - | 3359 | `			 * truncates here (recorded residual); nothing to validate. */` |
|      3 | 3360 | `			break;` |
|      - | 3361 | `		}` |
|    647 | 3362 | `		c = zIn[0];` |
|    647 | 3363 | `		zIn++; /* jump the conversion specifier */` |
|   3187 | 3364 | `		for( idx = 0 ; idx < (int)SX_ARRAYSIZE(aFmt) ; idx++ ){` |
|   3169 | 3365 | `			if( c == aFmt[idx].fmttype ){` |
|    629 | 3366 | `				break;` |
|      - | 3367 | `			}` |
|   1271 | 3368 | `		}` |
|    647 | 3369 | `		if( idx >= (int)SX_ARRAYSIZE(aFmt) ){` |
|     19 | 3370 | `			*pBad = c; /* unknown specifier */` |
|     19 | 3371 | `			return TRUE;` |
|      - | 3372 | `		}` |
|      1 | 3373 | `	}` |
|    315 | 3374 | `	return FALSE;` |
|    167 | 3375 | `}` |
|      - | 3376 | `/*` |
|      - | 3377 | ` * Validate a printf-style format string. PHP 8 raises a catchable ValueError for` |
|      - | 3378 | ` * an unknown conversion specifier, thrown before any output is produced. Every` |
|      - | 3379 | ` * format builtin (sprintf/printf/vprintf/vsprintf/fprintf/vfprintf) calls this` |
|      - | 3380 | ` * up-front, then propagates the returned status verbatim (PH7_EXCEPTION when the` |
|      - | 3381 | ` * throw is caught in place, PH7_ABORT when it goes uncaught).` |
|      - | 3382 | ` * Returns PH7_OK when the format is valid.` |
|      - | 3383 | ` */` |
|    332 | 3384 | `PH7_PRIVATE sxi32 PH7_FormatValidate(ph7_context *pCtx,const char *zFormat,int nByte)` |
|      1 | 3385 | `{` |
|    333 | 3386 | `	int badSpec = 0;` |
|    333 | 3387 | `	if( FormatUnknownSpec(zFormat,nByte,&badSpec) ){` |
|     28 | 3388 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      9 | 3389 | `			"Unknown format specifier \"%c\"",badSpec);` |
|      - | 3390 | `	}` |
|    315 | 3391 | `	return PH7_OK;` |
|    167 | 3392 | `}` |
|      - | 3393 | `/*` |
|      - | 3394 | `` * PHP 8: a printf-family `$format` argument is a `string` parameter — scalars`` |
|      - | 3395 | ` * (int/float/bool) and null coerce to a string, but an array/object/resource` |
|      - | 3396 | ` * raises a catchable TypeError. iArg is the 1-based argument position ($format` |
|      - | 3397 | ` * is #1 for sprintf/printf/vprintf/vsprintf, #2 for fprintf/vfprintf). Returns` |
|      - | 3398 | ` * PH7_OK when the value is string-coercible (the caller then uses` |
|      - | 3399 | ` * ph7_value_to_string, which renders scalars/null verbatim).` |
|      - | 3400 | ` */` |
|    352 | 3401 | `PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg)` |
|      1 | 3402 | `{` |
|    353 | 3403 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg) \|\| ph7_value_is_resource(pArg) ){` |
|      - | 3404 | `		char zBuf[64];` |
|     13 | 3405 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 3406 | `			"%s(): Argument #%d ($format) must be of type string, %s given",` |
|      4 | 3407 | `			ph7_function_name(pCtx),iArg,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|      - | 3408 | `	}` |
|    345 | 3409 | `	return PH7_OK;` |
|    177 | 3410 | `}` |
|      - | 3411 | `/*` |
|      - | 3412 | ` * Format a given string.` |
|      - | 3413 | ` * The root program.  All variations call this core.` |
|      - | 3414 | ` * INPUTS:` |
|      - | 3415 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 3416 | ` *            1. A pointer to the call context.` |
|      - | 3417 | ` *            2. A pointer to the list of characters to be output` |
|      - | 3418 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 3419 | ` *            3. An integer number of characters to be output.` |
|      - | 3420 | ` *               (Note: This number might be zero.)` |
|      - | 3421 | ` *            4. Upper layer private data.` |
|      - | 3422 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 3423 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 3424 | ` */` |
|    314 | 3425 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 3426 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 3427 | `	ph7_context *pCtx,  /* call context */` |
|      - | 3428 | `	const char *zIn,    /* Format string */` |
|      - | 3429 | `	int nByte,          /* Format string length */` |
|      - | 3430 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 3431 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 3432 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 3433 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 3434 | `	)` |
|      1 | 3435 | `{` |
|    315 | 3436 | `	char spaces[] = "                                                  ";` |
|      - | 3437 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    315 | 3438 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 3439 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 3440 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 3441 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 3442 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 3443 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 3444 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 3445 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 3446 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 3447 | `	ph7_int64 iVal;` |
|      - | 3448 | `	int precision;           /* Precision of the current field */` |
|      - | 3449 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 3450 | `	int c,rc,n;` |
|      - | 3451 | `	int length;              /* Length of the field */` |
|      - | 3452 | `	int prefix;` |
|      - | 3453 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 3454 | `	int width;               /* Width of the current field */` |
|      - | 3455 | `	int idx;` |
|    315 | 3456 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 3457 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 3458 | `	/* An unknown conversion specifier is rejected up-front by PH7_FormatValidate()` |
|      - | 3459 | `	 * (called by every format builtin before this routine), so the specifier set` |
|      - | 3460 | `	 * seen here is always valid. */` |
|      - | 3461 | `	/* Start the format process */` |
|    471 | 3462 | `	for(;;){` |
|    943 | 3463 | `		zCur = zIn;` |
|   3129 | 3464 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|   2187 | 3465 | `			zIn++;` |
|      1 | 3466 | `		}` |
|    943 | 3467 | `		if( zCur < zIn ){` |
|      - | 3468 | `			/* Consume chunk verbatim */` |
|    661 | 3469 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|    661 | 3470 | `			if( rc != SXRET_OK ){` |
|      - | 3471 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|    ! 0 | 3472 | `				break;` |
|      - | 3473 | `			}` |
|    330 | 3474 | `		}` |
|    943 | 3475 | `		if( zIn >= zEnd ){` |
|      - | 3476 | `			/* No more input to process,break immediately */` |
|    313 | 3477 | `			break;` |
|      - | 3478 | `		}` |
|      - | 3479 | `		/* Find out what flags are present */` |
|    631 | 3480 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    630 | 3481 | `			flag_alternateform = flag_zeropad = 0;` |
|    631 | 3482 | `		zIn++; /* Jump the precent sign */` |
|    315 | 3483 | `		do{` |
|    673 | 3484 | `			c = zIn[0];` |
|    673 | 3485 | `			switch( c ){` |
|     15 | 3486 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      7 | 3487 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 3488 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|     17 | 3489 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 3490 | `			case '\'':` |
|    ! 0 | 3491 | `				zIn++;` |
|    ! 0 | 3492 | `				if( zIn < zEnd ){` |
|      - | 3493 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 3494 | `					c = zIn[0];` |
|    ! 0 | 3495 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3496 | `						spaces[idx] = (char)c;` |
|    ! 0 | 3497 | `					}` |
|    ! 0 | 3498 | `					c = 0;` |
|    ! 0 | 3499 | `				}` |
|    ! 0 | 3500 | `				break;` |
|    630 | 3501 | `			default:                                       break;` |
|      - | 3502 | `			}` |
|    673 | 3503 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 3504 | `		/* Get the field width */` |
|    631 | 3505 | `		width = 0;` |
|   1020 | 3506 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     75 | 3507 | `			width = width*10 + (zIn[0] - '0');` |
|     75 | 3508 | `			zIn++;` |
|      1 | 3509 | `		}` |
|    631 | 3510 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 3511 | `			/* Position specifer */` |
|    ! 0 | 3512 | `			if( width > 0 ){` |
|    ! 0 | 3513 | `				n = width;` |
|    ! 0 | 3514 | `				if( vf && n > 0 ){` |
|    ! 0 | 3515 | `					n--;` |
|    ! 0 | 3516 | `				}` |
|    ! 0 | 3517 | `			}` |
|    ! 0 | 3518 | `			zIn++;` |
|    ! 0 | 3519 | `			width = 0;` |
|      - | 3520 | `			/* php's grammar is %argnum$<flags><width>: the flags come AFTER the` |
|      - | 3521 | `			 * positional, so re-parse the full flag set here (e.g. "%1$-10s"),` |
|      - | 3522 | `			 * not just zero-padding. */` |
|    ! 0 | 3523 | `			do{` |
|    ! 0 | 3524 | `				c = zIn[0];` |
|    ! 0 | 3525 | `				switch( c ){` |
|    ! 0 | 3526 | `				case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|    ! 0 | 3527 | `				case '+':   flag_plussign = 1;        c = 0;   break;` |
|    ! 0 | 3528 | `				case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    ! 0 | 3529 | `				case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 3530 | `				case '\'':` |
|    ! 0 | 3531 | `					zIn++;` |
|    ! 0 | 3532 | `					if( zIn < zEnd ){` |
|    ! 0 | 3533 | `						c = zIn[0];` |
|    ! 0 | 3534 | `						for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3535 | `							spaces[idx] = (char)c;` |
|    ! 0 | 3536 | `						}` |
|    ! 0 | 3537 | `						c = 0;` |
|    ! 0 | 3538 | `					}` |
|    ! 0 | 3539 | `					break;` |
|    ! 0 | 3540 | `				default:                                       break;` |
|      - | 3541 | `				}` |
|    ! 0 | 3542 | `			}while( c==0 && (zIn++ < zEnd) );` |
|    ! 0 | 3543 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 3544 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 3545 | `				zIn++;` |
|    ! 0 | 3546 | `			}` |
|    ! 0 | 3547 | `		}` |
|    631 | 3548 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 3549 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 3550 | `		}` |
|      - | 3551 | `		/* Get the precision */` |
|    631 | 3552 | `		precision = -1;` |
|    631 | 3553 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     87 | 3554 | `			precision = 0;` |
|     87 | 3555 | `			zIn++;` |
|    226 | 3556 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     97 | 3557 | `				precision = precision*10 + (zIn[0] - '0');` |
|     97 | 3558 | `				zIn++;` |
|      1 | 3559 | `			}` |
|     43 | 3560 | `		}` |
|      - | 3561 | `		/* Consume a single 'l' length modifier (a C-ism php accepts and ignores,` |
|      - | 3562 | `		 * e.g. "%ld"); PH7_FormatValidate mirrors this. Exactly one is skipped:` |
|      - | 3563 | `		 * in "%lld" the second 'l' becomes the (unknown) specifier, just like php. */` |
|    631 | 3564 | `		if( zIn < zEnd && zIn[0] == 'l' ){` |
|      9 | 3565 | `			zIn++;` |
|      4 | 3566 | `		}` |
|    631 | 3567 | `		if( zIn >= zEnd ){` |
|      - | 3568 | `			/* No more input */` |
|      3 | 3569 | `			break;` |
|      - | 3570 | `		}` |
|      - | 3571 | `		/* Fetch the info entry for the field */` |
|    629 | 3572 | `		pInfo = 0;` |
|    629 | 3573 | `		xtype = PH7_FMT_ERROR;` |
|    629 | 3574 | `		c = zIn[0];` |
|    629 | 3575 | `		zIn++; /* Jump the format specifer */` |
|   2863 | 3576 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|   2863 | 3577 | `			if( c==aFmt[idx].fmttype ){` |
|    629 | 3578 | `				pInfo = &aFmt[idx];` |
|    629 | 3579 | `				xtype = pInfo->type;` |
|    629 | 3580 | `				break;` |
|      - | 3581 | `			}` |
|   1118 | 3582 | `		}` |
|    629 | 3583 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    629 | 3584 | `		length = 0;` |
|      - | 3585 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 3586 | `		 /*` |
|      - | 3587 | `		  ** At this point, variables are initialized as follows:` |
|      - | 3588 | `		  **` |
|      - | 3589 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 3590 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 3591 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 3592 | `		  **                               field width was negative.` |
|      - | 3593 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 3594 | `		  **                               the conversion character.` |
|      - | 3595 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 3596 | `		  **   width                       The specified field width.  This is` |
|      - | 3597 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 3598 | `		  **   precision                   The specified precision.  The default` |
|      - | 3599 | `		  **                               is -1.` |
|      - | 3600 | `		  */` |
|    629 | 3601 | `		switch(xtype){` |
|      3 | 3602 | `		case PH7_FMT_PERCENT:` |
|      - | 3603 | `			/* A literal percent character */` |
|      7 | 3604 | `			zWorker[0] = '%';` |
|      7 | 3605 | `			length = (int)sizeof(char);` |
|      7 | 3606 | `			break;` |
|      3 | 3607 | `		case PH7_FMT_CHARX:` |
|      - | 3608 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 3609 | `			 * with that ASCII value` |
|      - | 3610 | `			 */` |
|      7 | 3611 | `			pArg = NEXT_ARG;` |
|      7 | 3612 | `			if( pArg == 0 ){` |
|      3 | 3613 | `				c = 0;` |
|      2 | 3614 | `			}else{` |
|      5 | 3615 | `				c = ph7_value_to_int(pArg);` |
|      - | 3616 | `			}` |
|      - | 3617 | `			/* NUL byte is an acceptable value */` |
|      7 | 3618 | `			zWorker[0] = (char)c;` |
|      7 | 3619 | `			length = (int)sizeof(char);` |
|      7 | 3620 | `			break;` |
|    161 | 3621 | `		case PH7_FMT_STRING:` |
|      - | 3622 | `			/* the argument is treated as and presented as a string */` |
|    323 | 3623 | `			pArg = NEXT_ARG;` |
|    323 | 3624 | `			if( pArg == 0 ){` |
|    ! 0 | 3625 | `				length = 0;` |
|    ! 0 | 3626 | `			}else{` |
|    323 | 3627 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 3628 | `			}` |
|    323 | 3629 | `			if( length < 1 ){` |
|    ! 0 | 3630 | `				zBuf = " ";` |
|    ! 0 | 3631 | `				length = (int)sizeof(char);` |
|    ! 0 | 3632 | `			}` |
|    323 | 3633 | `			if( precision>=0 && precision<length ){` |
|      3 | 3634 | `				length = precision;` |
|      1 | 3635 | `			}` |
|    323 | 3636 | `			if( flag_zeropad ){` |
|      - | 3637 | `				/* zero-padding works on strings too */` |
|    ! 0 | 3638 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 3639 | `					spaces[idx] = '0';` |
|    ! 0 | 3640 | `				}` |
|    ! 0 | 3641 | `			}` |
|    323 | 3642 | `			break;` |
|     59 | 3643 | `		case PH7_FMT_RADIX:` |
|    119 | 3644 | `			pArg = NEXT_ARG;` |
|    119 | 3645 | `			if( pArg == 0 ){` |
|    ! 0 | 3646 | `				iVal = 0;` |
|    ! 0 | 3647 | `			}else{` |
|    119 | 3648 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 3649 | `			}` |
|      - | 3650 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|    119 | 3651 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 3652 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 3653 | `			}` |
|      - | 3654 | `#if 1` |
|      - | 3655 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 3656 | `        ** I think this is stupid.*/` |
|    119 | 3657 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 3658 | `#else` |
|      - | 3659 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 3660 | `        ** but leave the prefix for hex.*/` |
|      - | 3661 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 3662 | `#endif` |
|    119 | 3663 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|     95 | 3664 | `          if( iVal<0 ){` |
|     25 | 3665 | `            iVal = -iVal;` |
|      - | 3666 | `			/* Ticket 1433-003 */` |
|     25 | 3667 | `			if( iVal < 0 ){` |
|      - | 3668 | `				/* Overflow */` |
|    ! 0 | 3669 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3670 | `			}` |
|     25 | 3671 | `            prefix = '-';` |
|     83 | 3672 | `          }else if( flag_plussign )  prefix = '+';` |
|     69 | 3673 | `          else if( flag_blanksign )  prefix = ' ';` |
|     67 | 3674 | `          else                       prefix = 0;` |
|     48 | 3675 | `        }else{` |
|     25 | 3676 | `			if( iVal<0 ){` |
|    ! 0 | 3677 | `				iVal = -iVal;` |
|      - | 3678 | `				/* Ticket 1433-003 */` |
|    ! 0 | 3679 | `				if( iVal < 0 ){` |
|      - | 3680 | `					/* Overflow */` |
|    ! 0 | 3681 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 3682 | `				}` |
|    ! 0 | 3683 | `			}` |
|     25 | 3684 | `			prefix = 0;` |
|      - | 3685 | `		}` |
|    119 | 3686 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      7 | 3687 | `          precision = width-(prefix!=0);` |
|      3 | 3688 | `        }` |
|    119 | 3689 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 3690 | `        {` |
|      - | 3691 | `          register char *cset;      /* Use registers for speed */` |
|      - | 3692 | `          register int base;` |
|    119 | 3693 | `          cset = pInfo->charset;` |
|    119 | 3694 | `          base = pInfo->base;` |
|     59 | 3695 | `          do{                                           /* Convert to ascii */` |
|    185 | 3696 | `            *(--zBuf) = cset[iVal%base];` |
|    185 | 3697 | `            iVal = iVal/base;` |
|    185 | 3698 | `          }while( iVal>0 );` |
|      - | 3699 | `        }` |
|    119 | 3700 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    141 | 3701 | `        for(idx=precision-length; idx>0; idx--){` |
|     23 | 3702 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|     12 | 3703 | `        }` |
|    119 | 3704 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|    119 | 3705 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 3706 | `          char *pre, x;` |
|    ! 0 | 3707 | `          pre = pInfo->prefix;` |
|    ! 0 | 3708 | `          if( *zBuf!=pre[0] ){` |
|    ! 0 | 3709 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|    ! 0 | 3710 | `          }` |
|    ! 0 | 3711 | `        }` |
|    119 | 3712 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    119 | 3713 | `		break;` |
|     88 | 3714 | `		case PH7_FMT_FLOAT:` |
|      - | 3715 | `		case PH7_FMT_EXP:` |
|      - | 3716 | `		case PH7_FMT_GENERIC:{` |
|      - | 3717 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 3718 | `		double realvalue;` |
|      - | 3719 | `		char zFmt[8];` |
|      - | 3720 | `		int nOut, nFmt;` |
|    177 | 3721 | `		pArg = NEXT_ARG;` |
|    177 | 3722 | `		if( pArg == 0 ){` |
|    ! 0 | 3723 | `			realvalue = 0;` |
|    ! 0 | 3724 | `		}else{` |
|    177 | 3725 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 3726 | `		}` |
|      - | 3727 | `		/* php prints the IEEE specials bare — NaN / INF / -INF with no width` |
|      - | 3728 | `		 * padding, precision, or sign flags (php_sprintf_appenddouble). */` |
|    177 | 3729 | `		if( PH7_IS_NAN(realvalue) ){` |
|     21 | 3730 | `			zBuf = "NaN";` |
|     21 | 3731 | `			length = 3;` |
|     21 | 3732 | `			width = 0;` |
|     21 | 3733 | `			break;` |
|      - | 3734 | `		}` |
|    157 | 3735 | `		if( PH7_IS_INF(realvalue) ){` |
|     37 | 3736 | `			if( realvalue < 0.0 ){` |
|     15 | 3737 | `				zBuf = "-INF";` |
|     15 | 3738 | `				length = 4;` |
|      8 | 3739 | `			}else{` |
|     23 | 3740 | `				zBuf = "INF";` |
|     23 | 3741 | `				length = 3;` |
|      - | 3742 | `			}` |
|     37 | 3743 | `			width = 0;` |
|     37 | 3744 | `			break;` |
|      - | 3745 | `		}` |
|    121 | 3746 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|    121 | 3747 | `		if( precision > 53 ){` |
|      - | 3748 | `			/* php's FORMAT_CONV_MAX_PRECISION cap, with the same E_NOTICE` |
|      - | 3749 | `			 * (message prefixed with the active function's name, like` |
|      - | 3750 | `			 * php_error_docref). */` |
|      - | 3751 | `			char zMsg[160];` |
|      4 | 3752 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 3753 | `				"%z(): Requested precision of %d digits was truncated to PHP maximum of %d digits",` |
|      2 | 3754 | `				&pCtx->pFunc->sName,precision,53);` |
|      3 | 3755 | `			PH7_VmThrowError(pCtx->pVm,0,E_NOTICE,zMsg);` |
|      3 | 3756 | `			precision = 53;` |
|      1 | 3757 | `		}` |
|      - | 3758 | ``		/* php's %f/%e extract the sign via `num < 0`, so negative zero prints`` |
|      - | 3759 | `		 * unsigned there — while %g (php_gcvt on the raw value) keeps "-0". */` |
|    121 | 3760 | `		if( xtype!=PH7_FMT_GENERIC && realvalue == 0.0 ){` |
|      9 | 3761 | `			realvalue = 0.0;` |
|      4 | 3762 | `		}` |
|      - | 3763 | `		/* php's float conversions are correctly rounded (zend_dtoa); use libc` |
|      - | 3764 | `		 * snprintf as the digit engine (the byte-exact-floats rule — the old` |
|      - | 3765 | `		 * hand-rolled vxGetdigit loop stopped at 16 significant digits, so` |
|      - | 3766 | `		 * e.g. %f of 1e308 printed zeros where php prints the exact binary64` |
|      - | 3767 | `		 * expansion), then post-process into php's exact shapes below. */` |
|    121 | 3768 | `		nFmt = 0;` |
|    121 | 3769 | `		zFmt[nFmt++] = '%';` |
|    121 | 3770 | `		if( flag_alternateform ) zFmt[nFmt++] = '#';` |
|      - | 3771 | `		/* php's ' ' flag selects space PADDING (its default), not C's` |
|      - | 3772 | `		 * space-for-positive-sign — so flag_blanksign is NOT forwarded. */` |
|    121 | 3773 | `		if( flag_plussign ) zFmt[nFmt++] = '+';` |
|    121 | 3774 | `		zFmt[nFmt++] = '.';` |
|    121 | 3775 | `		zFmt[nFmt++] = '*';` |
|    165 | 3776 | `		zFmt[nFmt++] = (char)(xtype==PH7_FMT_FLOAT ? 'f' :` |
|     30 | 3777 | `			(xtype==PH7_FMT_EXP ? ((pInfo->charset[0]=='E') ? 'E' : 'e')` |
|     28 | 3778 | `			                    : ((pInfo->charset[0]=='E') ? 'G' : 'g')));` |
|    121 | 3779 | `		zFmt[nFmt] = 0;` |
|    121 | 3780 | `		nOut = snprintf(zWorker,sizeof(zWorker),zFmt,precision,realvalue);` |
|    121 | 3781 | `		if( nOut < 0 \|\| nOut >= (int)sizeof(zWorker) ){` |
|      - | 3782 | `			/* Cannot happen with precision capped at 53 (%f of DBL_MAX is` |
|      - | 3783 | `			 * ~365 bytes); keep the truncated output rather than overrun. */` |
|    ! 0 | 3784 | `			nOut = (int)SyStrlen(zWorker);` |
|    ! 0 | 3785 | `		}` |
|    121 | 3786 | `		nOut = (int)PH7_PhpFloatShape(zWorker,(sxi32)nOut,xtype==PH7_FMT_GENERIC);` |
|    121 | 3787 | `		zBuf = zWorker;` |
|    121 | 3788 | `		length = nOut;` |
|      - | 3789 | `		/* Let the zero-pad block below insert zeros between the sign (written` |
|      - | 3790 | `		 * by snprintf) and the first digit, as before. */` |
|    121 | 3791 | `		prefix = (zWorker[0]=='-' \|\| zWorker[0]=='+' \|\| zWorker[0]==' ') ? zWorker[0] : 0;` |
|      - | 3792 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 3793 | `        ** set and we are not left justified */` |
|    121 | 3794 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 3795 | `          int i;` |
|      7 | 3796 | `          int nPad = width - length;` |
|     51 | 3797 | `          for(i=width; i>=nPad; i--){` |
|     45 | 3798 | `            zBuf[i] = zBuf[i-nPad];` |
|     23 | 3799 | `          }` |
|      7 | 3800 | `          i = prefix!=0;` |
|     29 | 3801 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      7 | 3802 | `          length = width;` |
|      3 | 3803 | `        }` |
|      - | 3804 | `#else` |
|      - | 3805 | `         zBuf = " ";` |
|      - | 3806 | `		 length = (int)sizeof(char);` |
|      - | 3807 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|    121 | 3808 | `		 break;` |
|      - | 3809 | `							 }` |
|    ! 0 | 3810 | `		default:` |
|      - | 3811 | `			/* Unreachable: PH7_FormatValidate() rejects unknown specifiers with a` |
|      - | 3812 | `			 * catchable ValueError before formatting begins. Kept as a defensive` |
|      - | 3813 | `			 * no-op that emits nothing. */` |
|    ! 0 | 3814 | `			length = 0;` |
|    ! 0 | 3815 | `			break;` |
|      - | 3816 | `		}` |
|      - | 3817 | `		 /*` |
|      - | 3818 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 3819 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 3820 | `		 ** the output.` |
|      - | 3821 | `		 */` |
|    629 | 3822 | `    if( !flag_leftjustify ){` |
|      - | 3823 | `      register int nspace;` |
|    615 | 3824 | `      nspace = width-length;` |
|    615 | 3825 | `      if( nspace>0 ){` |
|      7 | 3826 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3827 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3828 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3829 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3830 | `			}` |
|    ! 0 | 3831 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3832 | `        }` |
|      7 | 3833 | `        if( nspace>0 ){` |
|      7 | 3834 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      7 | 3835 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3836 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3837 | `			}` |
|      3 | 3838 | `		}` |
|      3 | 3839 | `      }` |
|    307 | 3840 | `    }` |
|    629 | 3841 | `    if( length>0 ){` |
|    629 | 3842 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    629 | 3843 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 3844 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3845 | `		}` |
|    314 | 3846 | `    }` |
|    629 | 3847 | `    if( flag_leftjustify ){` |
|      - | 3848 | `      register int nspace;` |
|     15 | 3849 | `      nspace = width-length;` |
|     15 | 3850 | `      if( nspace>0 ){` |
|     11 | 3851 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 3852 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 3853 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3854 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3855 | `			}` |
|    ! 0 | 3856 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 3857 | `        }` |
|     11 | 3858 | `        if( nspace>0 ){` |
|     11 | 3859 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|     11 | 3860 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3861 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 3862 | `			}` |
|      5 | 3863 | `		}` |
|      5 | 3864 | `      }` |
|      7 | 3865 | `    }` |
|      1 | 3866 | ` }/* for(;;) */` |
|    315 | 3867 | `	return SXRET_OK;` |
|    158 | 3868 | `}` |
|      - | 3869 | `/*` |
|      - | 3870 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 3871 | ` */` |
|    144 | 3872 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 3873 | `{` |
|      - | 3874 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|      - | 3875 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|      - | 3876 | `	 * non-OK rc also stops the format loop. */` |
|    145 | 3877 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|    145 | 3878 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|    145 | 3879 | `	return *pRc;` |
|      1 | 3880 | `}` |
|      - | 3881 | `/*` |
|      - | 3882 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 3883 | ` *  Return a formatted string.` |
|      - | 3884 | ` * Parameters` |
|      - | 3885 | ` *  $format` |
|      - | 3886 | ` *    The format string (see block comment above)` |
|      - | 3887 | ` * Return` |
|      - | 3888 | ` *  A string produced according to the formatting string format.` |
|      - | 3889 | ` */` |
|    108 | 3890 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3891 | `{` |
|      - | 3892 | `	const char *zFormat;` |
|    109 | 3893 | `	sxi32 rc = SXRET_OK;` |
|      - | 3894 | `	int nLen;` |
|    109 | 3895 | `	if( nArg < 1 ){` |
|      - | 3896 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 3897 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3898 | `		return PH7_OK;` |
|      - | 3899 | `	}` |
|      - | 3900 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|    109 | 3901 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    109 | 3902 | `	if( rc != PH7_OK ){` |
|      5 | 3903 | `		return rc;` |
|      - | 3904 | `	}` |
|      - | 3905 | `	/* Extract the string format (scalars/null coerce). */` |
|    105 | 3906 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    105 | 3907 | `	if( nLen < 1 ){` |
|      - | 3908 | `		/* Empty string */` |
|    ! 0 | 3909 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3910 | `		return PH7_OK;` |
|      - | 3911 | `	}` |
|      - | 3912 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 3913 | `	 * output; propagate the throw status verbatim. */` |
|    105 | 3914 | `	rc = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    105 | 3915 | `	if( rc != PH7_OK ){` |
|     17 | 3916 | `		return rc;` |
|      - | 3917 | `	}` |
|      - | 3918 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|     89 | 3919 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|     89 | 3920 | `	if( rc != SXRET_OK ){` |
|      - | 3921 | `		/* The result append ran out of memory: raise a fatal rather than` |
|      - | 3922 | `		 * returning a silently-truncated string. */` |
|    ! 0 | 3923 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 3924 | `	}` |
|     89 | 3925 | `	return PH7_OK;` |
|     55 | 3926 | `}` |
|      - | 3927 | `/*` |
|      - | 3928 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 3929 | ` */` |
|   1130 | 3930 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 3931 | `{` |
|   1131 | 3932 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 3933 | `	/* Call the VM output consumer directly */` |
|   1131 | 3934 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 3935 | `	/* Increment counter */` |
|   1131 | 3936 | `	*pCounter += nLen;` |
|   1131 | 3937 | `	return PH7_OK;` |
|      1 | 3938 | `}` |
|      - | 3939 | `/*` |
|      - | 3940 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 3941 | ` *  Output a formatted string.` |
|      - | 3942 | ` * Parameters` |
|      - | 3943 | ` *  $format` |
|      - | 3944 | ` *   See sprintf() for a description of format.` |
|      - | 3945 | ` * Return` |
|      - | 3946 | ` *  The length of the outputted string.` |
|      - | 3947 | ` */` |
|    200 | 3948 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3949 | `{` |
|    201 | 3950 | `	ph7_int64 nCounter = 0;` |
|      - | 3951 | `	const char *zFormat;` |
|      - | 3952 | `	int nLen;` |
|    201 | 3953 | `	if( nArg < 1 ){` |
|      - | 3954 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 3955 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3956 | `		return PH7_OK;` |
|      - | 3957 | `	}` |
|      - | 3958 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|      - | 3959 | `	{` |
|    201 | 3960 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    201 | 3961 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 3962 | `			return rcf;` |
|      - | 3963 | `		}` |
|      - | 3964 | `	}` |
|      - | 3965 | `	/* Extract the string format (scalars/null coerce). */` |
|    201 | 3966 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    201 | 3967 | `	if( nLen < 1 ){` |
|      - | 3968 | `		/* Empty string */` |
|    ! 0 | 3969 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 3970 | `		return PH7_OK;` |
|      - | 3971 | `	}` |
|      - | 3972 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 3973 | `	 * output; propagate the throw status verbatim. */` |
|      - | 3974 | `	{` |
|    201 | 3975 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    201 | 3976 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 3977 | `			return rcv;` |
|      - | 3978 | `		}` |
|      - | 3979 | `	}` |
|      - | 3980 | `	/* Format the string */` |
|    201 | 3981 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 3982 | `	/* Return the length of the outputted string */` |
|    201 | 3983 | `	ph7_result_int64(pCtx,nCounter);` |
|    201 | 3984 | `	return PH7_OK;` |
|    101 | 3985 | `}` |
|      - | 3986 | `/*` |
|      - | 3987 | ` * int vprintf(string $format,array $args)` |
|      - | 3988 | ` *  Output a formatted string.` |
|      - | 3989 | ` * Parameters` |
|      - | 3990 | ` *  $format` |
|      - | 3991 | ` *   See sprintf() for a description of format.` |
|      - | 3992 | ` * Return` |
|      - | 3993 | ` *  The length of the outputted string.` |
|      - | 3994 | ` */` |
|      4 | 3995 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3996 | `{` |
|      5 | 3997 | `	ph7_int64 nCounter = 0;` |
|      - | 3998 | `	const char *zFormat;` |
|      - | 3999 | `	ph7_hashmap *pMap;` |
|      - | 4000 | `	SySet sArg;` |
|      - | 4001 | `	int nLen,n;` |
|      - | 4002 | `	sxi32 rcFmt;` |
|      5 | 4003 | `	if( nArg < 2 ){` |
|      - | 4004 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4005 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4006 | `		return PH7_OK;` |
|      - | 4007 | `	}` |
|      - | 4008 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|      5 | 4009 | `	rcFmt = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|      5 | 4010 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4011 | `		return rcFmt;` |
|      - | 4012 | `	}` |
|      5 | 4013 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 4014 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 4015 | `		char zBuf[64];` |
|      4 | 4016 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4017 | `			"vprintf(): Argument #2 ($values) must be of type array, %s given",` |
|      2 | 4018 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 4019 | `	}` |
|      - | 4020 | `	/* Extract the string format (scalars/null coerce). */` |
|      3 | 4021 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4022 | `	if( nLen < 1 ){` |
|      - | 4023 | `		/* Empty string */` |
|    ! 0 | 4024 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4025 | `		return PH7_OK;` |
|      - | 4026 | `	}` |
|      - | 4027 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4028 | `	 * output; propagate the throw status verbatim. */` |
|      3 | 4029 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      3 | 4030 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4031 | `		return rcFmt;` |
|      - | 4032 | `	}` |
|      - | 4033 | `	/* Point to the hashmap */` |
|      3 | 4034 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4035 | `	/* Extract arguments from the hashmap */` |
|      3 | 4036 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4037 | `	/* Format the string */` |
|      3 | 4038 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 4039 | `	/* Release the container */` |
|      3 | 4040 | `	SySetRelease(&sArg);` |
|      - | 4041 | `	/* Return the length of the outputted string */` |
|      3 | 4042 | `	ph7_result_int64(pCtx,nCounter);` |
|      3 | 4043 | `	return PH7_OK;` |
|      3 | 4044 | `}` |
|      - | 4045 | `/*` |
|      - | 4046 | ` * int vsprintf(string $format,array $args)` |
|      - | 4047 | ` *  Output a formatted string.` |
|      - | 4048 | ` * Parameters` |
|      - | 4049 | ` *  $format` |
|      - | 4050 | ` *   See sprintf() for a description of format.` |
|      - | 4051 | ` * Return` |
|      - | 4052 | ` *  A string produced according to the formatting string format.` |
|      - | 4053 | ` */` |
|     22 | 4054 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4055 | `{` |
|      - | 4056 | `	const char *zFormat;` |
|      - | 4057 | `	ph7_hashmap *pMap;` |
|      - | 4058 | `	SySet sArg;` |
|     23 | 4059 | `	sxi32 rc = SXRET_OK;` |
|      - | 4060 | `	sxi32 rcFmt;` |
|      - | 4061 | `	int nLen,n;` |
|     23 | 4062 | `	if( nArg < 2 ){` |
|      - | 4063 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4064 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4065 | `		return PH7_OK;` |
|      - | 4066 | `	}` |
|      - | 4067 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|     23 | 4068 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|     23 | 4069 | `	if( rc != PH7_OK ){` |
|      5 | 4070 | `		return rc;` |
|      - | 4071 | `	}` |
|     19 | 4072 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 4073 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 4074 | `		char zBuf[64];` |
|     16 | 4075 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4076 | `			"vsprintf(): Argument #2 ($values) must be of type array, %s given",` |
|     10 | 4077 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 4078 | `	}` |
|      - | 4079 | `	/* Extract the string format (scalars/null coerce). */` |
|      9 | 4080 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 4081 | `	if( nLen < 1 ){` |
|      - | 4082 | `		/* Empty string */` |
|    ! 0 | 4083 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4084 | `		return PH7_OK;` |
|      - | 4085 | `	}` |
|      - | 4086 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4087 | `	 * output; propagate the throw status verbatim. */` |
|      9 | 4088 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      9 | 4089 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4090 | `		return rcFmt;` |
|      - | 4091 | `	}` |
|      - | 4092 | `	/* Point to hashmap */` |
|      9 | 4093 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4094 | `	/* Extract arguments from the hashmap */` |
|      9 | 4095 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4096 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|      9 | 4097 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|      - | 4098 | `	/* Release the container */` |
|      9 | 4099 | `	SySetRelease(&sArg);` |
|      9 | 4100 | `	if( rc != SXRET_OK ){` |
|      - | 4101 | `		/* The result append ran out of memory: raise a fatal. */` |
|    ! 0 | 4102 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4103 | `	}` |
|      9 | 4104 | `	return PH7_OK;` |
|     12 | 4105 | `}` |
|      - | 4106 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4107 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4108 | `/*` |
|      - | 4109 | ` * Symisc eXtension.` |
|      - | 4110 | ` * string size_format(int64 $size)` |
|      - | 4111 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 4112 | ` *  Example:` |
|      - | 4113 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 4114 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 4115 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 4116 | ` * Parameter` |
|      - | 4117 | ` *  $size` |
|      - | 4118 | ` *    Entity size in bytes.` |
|      - | 4119 | ` * Return` |
|      - | 4120 | ` *   Formatted string representation of the given size.` |
|      - | 4121 | ` */` |
|     24 | 4122 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4123 | `{` |
|      - | 4124 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 4125 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 4126 | `	sxi32 nRest,i_32;` |
|      - | 4127 | `	ph7_int64 iSize;` |
|     25 | 4128 | `	int c = -1; /* index in zUnit[] */` |
|      - | 4129 |  |
|     25 | 4130 | `	if( nArg < 1 ){` |
|      - | 4131 | `		/* Missing argument,return the empty string */` |
|      3 | 4132 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4133 | `		return PH7_OK;` |
|      - | 4134 | `	}` |
|      - | 4135 | `	/* Extract the given size */` |
|     23 | 4136 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 4137 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 4138 | `		/* Don't bother formatting,return immediately */` |
|      5 | 4139 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 4140 | `		return PH7_OK;` |
|      - | 4141 | `	}` |
|     19 | 4142 | `	for(;;){` |
|     39 | 4143 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 4144 | `		iSize >>= 10;` |
|     39 | 4145 | `		c++;` |
|     39 | 4146 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 4147 | `			break;` |
|      - | 4148 | `		}` |
|      1 | 4149 | `	}` |
|     19 | 4150 | `	nRest /= 100;` |
|     19 | 4151 | `	if( nRest > 9 ){` |
|    ! 0 | 4152 | `		nRest = 9;` |
|    ! 0 | 4153 | `	}` |
|     19 | 4154 | `	if( iSize > 999 ){` |
|    ! 0 | 4155 | `		c++;` |
|    ! 0 | 4156 | `		nRest = 9;` |
|    ! 0 | 4157 | `		iSize = 0;` |
|    ! 0 | 4158 | `	}` |
|     19 | 4159 | `	i_32 = (sxi32)iSize;` |
|      - | 4160 | `	/* Format */` |
|     19 | 4161 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 4162 | `	return PH7_OK;` |
|     13 | 4163 | `}` |
|      - | 4164 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4165 | `/*` |
|      - | 4166 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 4167 | ` *   Calculate the md5 hash of a string.` |
|      - | 4168 | ` * Parameter` |
|      - | 4169 | ` *  $str` |
|      - | 4170 | ` *   Input string` |
|      - | 4171 | ` * $raw_output` |
|      - | 4172 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4173 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4174 | ` * Return` |
|      - | 4175 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 4176 | ` */` |
|     12 | 4177 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4178 | `{` |
|      - | 4179 | `	unsigned char zDigest[16];` |
|     13 | 4180 | `	int raw_output = FALSE;` |
|      - | 4181 | `	const void *pIn;` |
|      - | 4182 | `	int nLen;` |
|     13 | 4183 | `	if( nArg < 1 ){` |
|      - | 4184 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4185 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4186 | `		return PH7_OK;` |
|      - | 4187 | `	}` |
|      - | 4188 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 4189 | `	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */` |
|     13 | 4190 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 4191 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 4192 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 4193 | `	}` |
|      - | 4194 | `	/* Compute the MD5 digest */` |
|     13 | 4195 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|     13 | 4196 | `	if( raw_output ){` |
|      - | 4197 | `		/* Output raw digest */` |
|      5 | 4198 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 4199 | `	}else{` |
|      - | 4200 | `		/* Perform a binary to hex conversion */` |
|      9 | 4201 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4202 | `	}` |
|     13 | 4203 | `	return PH7_OK;` |
|      7 | 4204 | `}` |
|      - | 4205 | `/*` |
|      - | 4206 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 4207 | ` *   Calculate the sha1 hash of a string.` |
|      - | 4208 | ` * Parameter` |
|      - | 4209 | ` *  $str` |
|      - | 4210 | ` *   Input string` |
|      - | 4211 | ` * $raw_output` |
|      - | 4212 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4213 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4214 | ` * Return` |
|      - | 4215 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 4216 | ` */` |
|     10 | 4217 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4218 | `{` |
|      - | 4219 | `	unsigned char zDigest[20];` |
|     11 | 4220 | `	int raw_output = FALSE;` |
|      - | 4221 | `	const void *pIn;` |
|      - | 4222 | `	int nLen;` |
|     11 | 4223 | `	if( nArg < 1 ){` |
|      - | 4224 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4225 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4226 | `		return PH7_OK;` |
|      - | 4227 | `	}` |
|      - | 4228 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 4229 | `	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */` |
|     11 | 4230 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 4231 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 4232 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 4233 | `	}` |
|      - | 4234 | `	/* Compute the SHA1 digest */` |
|     11 | 4235 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|     11 | 4236 | `	if( raw_output ){` |
|      - | 4237 | `		/* Output raw digest */` |
|      5 | 4238 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 4239 | `	}else{` |
|      - | 4240 | `		/* Perform a binary to hex conversion */` |
|      7 | 4241 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 4242 | `	}` |
|     11 | 4243 | `	return PH7_OK;` |
|      6 | 4244 | `}` |
|      - | 4245 | `/*` |
|      - | 4246 | ` * int64 crc32(string $str)` |
|      - | 4247 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 4248 | ` * Parameter` |
|      - | 4249 | ` *  $str` |
|      - | 4250 | ` *   Input string` |
|      - | 4251 | ` * Return` |
|      - | 4252 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 4253 | ` */` |
|      2 | 4254 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4255 | `{` |
|      - | 4256 | `	const void *pIn;` |
|      - | 4257 | `	sxu32 nCRC;` |
|      - | 4258 | `	int nLen;` |
|      3 | 4259 | `	if( nArg < 1 ){` |
|      - | 4260 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4261 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4262 | `		return PH7_OK;` |
|      - | 4263 | `	}` |
|      - | 4264 | `	/* Extract the input string */` |
|      3 | 4265 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4266 | `	if( nLen < 1 ){` |
|      - | 4267 | `		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike` |
|      - | 4268 | `		 * md5()/sha1(), whose empty-string digests are non-zero. */` |
|    ! 0 | 4269 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4270 | `		return PH7_OK;` |
|      - | 4271 | `	}` |
|      - | 4272 | `	/* Calculate the sum */` |
|      3 | 4273 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 4274 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 4275 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 4276 | `	return PH7_OK;` |
|      2 | 4277 | `}` |
|      - | 4278 | `/*` |
|      - | 4279 | ` * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is` |
|      - | 4280 | ` * described by a small record so one dispatch (and one generic HMAC) serves them` |
|      - | 4281 | ` * all. Thin adapters normalize the differing context types and the reversed` |
|      - | 4282 | ` * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.` |
|      - | 4283 | ` */` |
|     11 | 4284 | `static void HashMd5Init(HashCtx *c){ MD5Init(&c->md5); }` |
|     15 | 4285 | `static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }` |
|     11 | 4286 | `static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }` |
|     11 | 4287 | `static void HashSha1Init(HashCtx *c){ SHA1Init(&c->sha1); }` |
|     15 | 4288 | `static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }` |
|     11 | 4289 | `static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }` |
|      9 | 4290 | `static void HashSha224Init(HashCtx *c){ SHA224Init(&c->sha256); }` |
|     33 | 4291 | `static void HashSha256Init(HashCtx *c){ SHA256Init(&c->sha256); }` |
|     57 | 4292 | `static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }` |
|     41 | 4293 | `static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }` |
|      9 | 4294 | `static void HashSha384Init(HashCtx *c){ SHA384Init(&c->sha512); }` |
|     15 | 4295 | `static void HashSha512Init(HashCtx *c){ SHA512Init(&c->sha512); }` |
|     27 | 4296 | `static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }` |
|     23 | 4297 | `static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }` |
|      - | 4298 | `typedef struct HashAlgo HashAlgo;` |
|      - | 4299 | `struct HashAlgo {` |
|      - | 4300 | `	const char *zName;   /* lowercase canonical name */` |
|      - | 4301 | `	int nDigestLen;      /* output bytes: 16/20/28/32/48/64 */` |
|      - | 4302 | `	int nBlockLen;       /* internal block bytes (for HMAC): 64 or 128 */` |
|      - | 4303 | `	void (*xInit)(HashCtx *);` |
|      - | 4304 | `	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);` |
|      - | 4305 | `	void (*xFinal)(HashCtx *,unsigned char *);` |
|      - | 4306 | `};` |
|      - | 4307 | `static const HashAlgo aHashAlgo[] = {` |
|      - | 4308 | `	{ "md5",    16, 64,  HashMd5Init,    HashMd5Update,    HashMd5Final    },` |
|      - | 4309 | `	{ "sha1",   20, 64,  HashSha1Init,   HashSha1Update,   HashSha1Final   },` |
|      - | 4310 | `	{ "sha224", 28, 64,  HashSha224Init, HashSha256Update, HashSha256Final },` |
|      - | 4311 | `	{ "sha256", 32, 64,  HashSha256Init, HashSha256Update, HashSha256Final },` |
|      - | 4312 | `	{ "sha384", 48, 128, HashSha384Init, HashSha512Update, HashSha512Final },` |
|      - | 4313 | `	{ "sha512", 64, 128, HashSha512Init, HashSha512Update, HashSha512Final },` |
|      - | 4314 | `};` |
|      - | 4315 | `/* Case-insensitive algorithm lookup (PHP accepts 'SHA256' etc.). */` |
|     73 | 4316 | `static const HashAlgo * HashFindAlgo(const char *zName,int nLen){` |
|      - | 4317 | `	sxu32 i;` |
|    279 | 4318 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|    272 | 4319 | `		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen` |
|    211 | 4320 | `			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){` |
|     67 | 4321 | `			return &aHashAlgo[i];` |
|      - | 4322 | `		}` |
|    106 | 4323 | `	}` |
|      6 | 4324 | `	return 0;` |
|     38 | 4325 | `}` |
|      - | 4326 | `/*` |
|      - | 4327 | ` * string hash(string $algo,string $data[,bool $binary = false])` |
|      - | 4328 | ` *   Generate a hash value (message digest).` |
|      - | 4329 | ` */` |
|     54 | 4330 | `static int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4331 | `{` |
|      - | 4332 | `	const HashAlgo *pAlgo;` |
|      - | 4333 | `	const char *zAlgo,*zData;` |
|     56 | 4334 | `	int nAlgoLen,nDataLen,raw_output = FALSE;` |
|      - | 4335 | `	HashCtx sCtx;` |
|      - | 4336 | `	unsigned char zDigest[64];` |
|     56 | 4337 | `	if( nArg < 2 ){` |
|    ! 0 | 4338 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4339 | `			"hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4340 | `	}` |
|     56 | 4341 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     56 | 4342 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     56 | 4343 | `	if( pAlgo == 0 ){` |
|      3 | 4344 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4345 | `			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - | 4346 | `	}` |
|     53 | 4347 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     53 | 4348 | `	if( nArg > 2 ){` |
|      9 | 4349 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|      4 | 4350 | `	}` |
|     53 | 4351 | `	pAlgo->xInit(&sCtx);` |
|     53 | 4352 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     53 | 4353 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     53 | 4354 | `	if( raw_output ){` |
|      9 | 4355 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      5 | 4356 | `	}else{` |
|     45 | 4357 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 4358 | `	}` |
|     53 | 4359 | `	return PH7_OK;` |
|     29 | 4360 | `}` |
|      - | 4361 | `/*` |
|      - | 4362 | ` * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])` |
|      - | 4363 | ` *   Generate a keyed hash value using the HMAC method (RFC 2104).` |
|      - | 4364 | ` */` |
|     16 | 4365 | `static int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 4366 | `{` |
|      - | 4367 | `	const HashAlgo *pAlgo;` |
|      - | 4368 | `	const char *zAlgo,*zData,*zKey;` |
|     18 | 4369 | `	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;` |
|      - | 4370 | `	HashCtx sCtx;` |
|      - | 4371 | `	unsigned char zKeyBlock[128],zIpad[128],zOpad[128],zInner[64],zDigest[64];` |
|      - | 4372 | `	int i,nBlock,nDigest;` |
|     18 | 4373 | `	if( nArg < 3 ){` |
|    ! 0 | 4374 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4375 | `			"hash_hmac() expects at least 3 arguments, %d given",nArg);` |
|      - | 4376 | `	}` |
|     18 | 4377 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     18 | 4378 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     18 | 4379 | `	if( pAlgo == 0 ){` |
|      3 | 4380 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4381 | `			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 4382 | `	}` |
|     15 | 4383 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     15 | 4384 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|     15 | 4385 | `	if( nArg > 3 ){` |
|      3 | 4386 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|      1 | 4387 | `	}` |
|     15 | 4388 | `	nBlock = pAlgo->nBlockLen;` |
|     15 | 4389 | `	nDigest = pAlgo->nDigestLen;` |
|      - | 4390 | `	/* Reduce the key to a single block: hash it if longer than the block, then` |
|      - | 4391 | `	 * zero-pad (a short or empty key is just zero-padded). */` |
|     15 | 4392 | `	SyZero(zKeyBlock,sizeof(zKeyBlock));` |
|     15 | 4393 | `	if( nKeyLen > nBlock ){` |
|      3 | 4394 | `		pAlgo->xInit(&sCtx);` |
|      3 | 4395 | `		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);` |
|      3 | 4396 | `		pAlgo->xFinal(&sCtx,zKeyBlock);` |
|     14 | 4397 | `	}else if( nKeyLen > 0 ){` |
|     11 | 4398 | `		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);` |
|      5 | 4399 | `	}` |
|   1039 | 4400 | `	for( i = 0; i < nBlock; i++ ){` |
|   1025 | 4401 | `		zIpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);` |
|   1025 | 4402 | `		zOpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);` |
|    513 | 4403 | `	}` |
|      - | 4404 | `	/* inner = H((key ^ ipad) \|\| data) */` |
|     15 | 4405 | `	pAlgo->xInit(&sCtx);` |
|     15 | 4406 | `	pAlgo->xUpdate(&sCtx,zIpad,(unsigned int)nBlock);` |
|     15 | 4407 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     15 | 4408 | `	pAlgo->xFinal(&sCtx,zInner);` |
|      - | 4409 | `	/* out = H((key ^ opad) \|\| inner) */` |
|     15 | 4410 | `	pAlgo->xInit(&sCtx);` |
|     15 | 4411 | `	pAlgo->xUpdate(&sCtx,zOpad,(unsigned int)nBlock);` |
|     15 | 4412 | `	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)nDigest);` |
|     15 | 4413 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     15 | 4414 | `	if( raw_output ){` |
|      3 | 4415 | `		ph7_result_string(pCtx,(const char *)zDigest,nDigest);` |
|      2 | 4416 | `	}else{` |
|     13 | 4417 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);` |
|      - | 4418 | `	}` |
|     15 | 4419 | `	return PH7_OK;` |
|     10 | 4420 | `}` |
|      - | 4421 | `/*` |
|      - | 4422 | ` * bool hash_equals(string $known_string,string $user_string)` |
|      - | 4423 | ` *   Timing-attack-safe string comparison.` |
|      - | 4424 | ` */` |
|     14 | 4425 | `static int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4426 | `{` |
|      - | 4427 | `	const char *zKnown,*zUser;` |
|      - | 4428 | `	int nKnown,nUser,i;` |
|     17 | 4429 | `	volatile unsigned char vDiff = 0;` |
|     17 | 4430 | `	if( nArg < 2 ){` |
|    ! 0 | 4431 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4432 | `			"hash_equals() expects exactly 2 arguments, %d given",nArg);` |
|      - | 4433 | `	}` |
|     17 | 4434 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      4 | 4435 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4436 | `			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",` |
|      1 | 4437 | `			ph7_type_name(apArg[0]));` |
|      - | 4438 | `	}` |
|     14 | 4439 | `	if( !ph7_value_is_string(apArg[1]) ){` |
|      4 | 4440 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4441 | `			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",` |
|      2 | 4442 | `			ph7_type_name(apArg[1]));` |
|      - | 4443 | `	}` |
|     11 | 4444 | `	zKnown = ph7_value_to_string(apArg[0],&nKnown);` |
|     11 | 4445 | `	zUser = ph7_value_to_string(apArg[1],&nUser);` |
|     11 | 4446 | `	if( nKnown != nUser ){` |
|      5 | 4447 | `		ph7_result_bool(pCtx,0);` |
|      5 | 4448 | `		return PH7_OK;` |
|      - | 4449 | `	}` |
|      - | 4450 | `	/* Constant-time: read every byte, never short-circuit. */` |
|     19 | 4451 | `	for( i = 0; i < nKnown; i++ ){` |
|     13 | 4452 | `		vDiff \|= (unsigned char)(zKnown[i] ^ zUser[i]);` |
|      7 | 4453 | `	}` |
|      7 | 4454 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|      7 | 4455 | `	return PH7_OK;` |
|     10 | 4456 | `}` |
|      - | 4457 | `/*` |
|      - | 4458 | ` * array hash_algos(void)` |
|      - | 4459 | ` *   Return a list of the registered hashing algorithms.` |
|      - | 4460 | ` */` |
|      2 | 4461 | `static int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4462 | `{` |
|      - | 4463 | `	ph7_value *pArray,*pValue;` |
|      - | 4464 | `	sxu32 i;` |
|      1 | 4465 | `	SXUNUSED(nArg);` |
|      1 | 4466 | `	SXUNUSED(apArg);` |
|      3 | 4467 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 4468 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 4469 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 4470 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4471 | `		return PH7_OK;` |
|      - | 4472 | `	}` |
|     15 | 4473 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|     13 | 4474 | `		ph7_value_string(pValue,aHashAlgo[i].zName,-1);` |
|     13 | 4475 | `		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);` |
|     13 | 4476 | `		ph7_value_reset_string_cursor(pValue);` |
|      7 | 4477 | `	}` |
|      3 | 4478 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 4479 | `	return PH7_OK;` |
|      2 | 4480 | `}` |
|      - | 4481 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 4482 | `/*` |
|      - | 4483 | ` * password_* (bcrypt). These live in ext/standard in real PHP — outside the` |
|      - | 4484 | ` * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.` |
|      - | 4485 | ` */` |
|      - | 4486 | `/*` |
|      - | 4487 | ` * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a` |
|      - | 4488 | ` * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).` |
|      - | 4489 | ` */` |
|     40 | 4490 | `static int BcryptParseHash(const char *zHash,int nHash,int *piCost)` |
|      1 | 4491 | `{` |
|      - | 4492 | `	int iCost;` |
|     40 | 4493 | `	if( nHash != 60 \|\| zHash[0] != '$' \|\| zHash[1] != '2' \|\| zHash[3] != '$'` |
|     29 | 4494 | `		\|\| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){` |
|     13 | 4495 | `		return FALSE;` |
|      - | 4496 | `	}` |
|     29 | 4497 | `	if( zHash[4] < '0' \|\| zHash[4] > '9' \|\| zHash[5] < '0' \|\| zHash[5] > '9' \|\| zHash[6] != '$' ){` |
|    ! 0 | 4498 | `		return FALSE;` |
|      - | 4499 | `	}` |
|     29 | 4500 | `	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');` |
|     29 | 4501 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      3 | 4502 | `		return FALSE;` |
|      - | 4503 | `	}` |
|     27 | 4504 | `	if( piCost ){ *piCost = iCost; }` |
|     27 | 4505 | `	return TRUE;` |
|     21 | 4506 | `}` |
|      - | 4507 | `/*` |
|      - | 4508 | ` * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the` |
|      - | 4509 | ` * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.` |
|      - | 4510 | ` */` |
|     20 | 4511 | `static int BcryptIsBcryptAlgo(ph7_value *pAlgo)` |
|      3 | 4512 | `{` |
|     23 | 4513 | `	if( ph7_value_is_null(pAlgo) ){` |
|    ! 0 | 4514 | `		return TRUE;` |
|      - | 4515 | `	}` |
|     23 | 4516 | `	if( ph7_value_is_string(pAlgo) ){` |
|      - | 4517 | `		int nAlgo;` |
|     23 | 4518 | `		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);` |
|     23 | 4519 | `		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );` |
|      - | 4520 | `	}` |
|    ! 0 | 4521 | `	return FALSE;` |
|     13 | 4522 | `}` |
|      - | 4523 | `/*` |
|      - | 4524 | ` * bool\|string password_hash(string $password,string\|int\|null $algo[,array $options])` |
|      - | 4525 | ` *  Create a bcrypt hash of the password.` |
|      - | 4526 | ` */` |
|     16 | 4527 | `static int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 4528 | `{` |
|      - | 4529 | `	const char *zPwd;` |
|     19 | 4530 | `	int nPwd,iCost = 12;` |
|      - | 4531 | `	unsigned char aSalt[16];` |
|      - | 4532 | `	char zHash[60];` |
|     19 | 4533 | `	if( nArg < 2 ){` |
|    ! 0 | 4534 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4535 | `			"password_hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4536 | `	}` |
|     19 | 4537 | `	if( !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      3 | 4538 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 4539 | `			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");` |
|      - | 4540 | `	}` |
|      - | 4541 | `	/* cost from $options['cost'] (default 12). */` |
|     16 | 4542 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|     14 | 4543 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     14 | 4544 | `		if( pCost ){ iCost = ph7_value_to_int(pCost); }` |
|      6 | 4545 | `	}` |
|     16 | 4546 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      4 | 4547 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 4548 | `			"Invalid bcrypt cost parameter specified: %d",iCost);` |
|      - | 4549 | `	}` |
|     13 | 4550 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     13 | 4551 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 4552 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 4553 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 4554 | `	}` |
|     13 | 4555 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){` |
|    ! 0 | 4556 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4557 | `		return PH7_OK;` |
|      - | 4558 | `	}` |
|     13 | 4559 | `	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));` |
|     13 | 4560 | `	return PH7_OK;` |
|     11 | 4561 | `}` |
|      - | 4562 | `/*` |
|      - | 4563 | ` * bool password_verify(string $password,string $hash)` |
|      - | 4564 | ` *  Verify a password against a bcrypt hash. Never throws on a malformed hash.` |
|      - | 4565 | ` */` |
|     28 | 4566 | `static int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4567 | `{` |
|      - | 4568 | `	const char *zPwd,*zHash;` |
|      - | 4569 | `	int nPwd,nHash,iCost,i;` |
|      - | 4570 | `	unsigned char aSalt[16];` |
|      - | 4571 | `	char zComputed[60];` |
|     29 | 4572 | `	volatile unsigned char vDiff = 0;` |
|     29 | 4573 | `	if( nArg < 2 ){` |
|    ! 0 | 4574 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4575 | `			"password_verify() expects exactly 2 arguments, %d given",nArg);` |
|      - | 4576 | `	}` |
|     29 | 4577 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     29 | 4578 | `	zHash = ph7_value_to_string(apArg[1],&nHash);` |
|     29 | 4579 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|     11 | 4580 | `		ph7_result_bool(pCtx,0);` |
|     11 | 4581 | `		return PH7_OK;` |
|      - | 4582 | `	}` |
|      - | 4583 | `	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */` |
|     19 | 4584 | `	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 4585 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4586 | `		return PH7_OK;` |
|      - | 4587 | `	}` |
|     19 | 4588 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){` |
|    ! 0 | 4589 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 4590 | `		return PH7_OK;` |
|      - | 4591 | `	}` |
|      - | 4592 | `	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps` |
|      - | 4593 | `	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */` |
|    577 | 4594 | `	for( i = 29; i < 60; i++ ){` |
|    559 | 4595 | `		vDiff \|= (unsigned char)(zComputed[i] ^ zHash[i]);` |
|    280 | 4596 | `	}` |
|     19 | 4597 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|     19 | 4598 | `	return PH7_OK;` |
|     15 | 4599 | `}` |
|      - | 4600 | `/*` |
|      - | 4601 | ` * array password_get_info(string $hash)` |
|      - | 4602 | ` *  Return ["algo"=>id\|null, "algoName"=>name, "options"=>[...]].` |
|      - | 4603 | ` */` |
|      6 | 4604 | `static int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4605 | `{` |
|      7 | 4606 | `	const char *zHash = "";` |
|      7 | 4607 | `	int nHash,iCost = 0,bBcrypt = 0;` |
|      - | 4608 | `	ph7_value *pArray,*pOptions,*pVal;` |
|      7 | 4609 | `	if( nArg > 0 ){` |
|      7 | 4610 | `		zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 4611 | `		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);` |
|      3 | 4612 | `	}` |
|      7 | 4613 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 4614 | `	pOptions = ph7_context_new_array(pCtx);` |
|      7 | 4615 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      7 | 4616 | `	if( pArray == 0 \|\| pOptions == 0 \|\| pVal == 0 ){` |
|    ! 0 | 4617 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4618 | `		return PH7_OK;` |
|      - | 4619 | `	}` |
|      7 | 4620 | `	if( bBcrypt ){` |
|      5 | 4621 | `		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */` |
|      5 | 4622 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 4623 | `		ph7_value_reset_string_cursor(pVal);` |
|      5 | 4624 | `		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);` |
|      5 | 4625 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 4626 | `		ph7_value_int(pVal,iCost);` |
|      5 | 4627 | `		ph7_array_add_strkey_elem(pOptions,"cost",pVal);` |
|      3 | 4628 | `	}else{` |
|      3 | 4629 | `		ph7_value_null(pVal);                          /* algo => null */` |
|      3 | 4630 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      3 | 4631 | `		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);` |
|      3 | 4632 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      - | 4633 | `	}` |
|      7 | 4634 | `	ph7_array_add_strkey_elem(pArray,"options",pOptions);` |
|      7 | 4635 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 4636 | `	return PH7_OK;` |
|      4 | 4637 | `}` |
|      - | 4638 | `/*` |
|      - | 4639 | ` * bool password_needs_rehash(string $hash,string\|int\|null $algo[,array $options])` |
|      - | 4640 | ` *  True if the hash was not made with the given algo/options.` |
|      - | 4641 | ` */` |
|      6 | 4642 | `static int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4643 | `{` |
|      - | 4644 | `	const char *zHash;` |
|      7 | 4645 | `	int nHash,iCost = 0,iWantCost = 12;` |
|      7 | 4646 | `	if( nArg < 2 ){` |
|    ! 0 | 4647 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 4648 | `			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);` |
|      - | 4649 | `	}` |
|      7 | 4650 | `	zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 4651 | `	if( !BcryptParseHash(zHash,nHash,&iCost) \|\| !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      - | 4652 | `		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */` |
|      3 | 4653 | `		ph7_result_bool(pCtx,1);` |
|      3 | 4654 | `		return PH7_OK;` |
|      - | 4655 | `	}` |
|      5 | 4656 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      5 | 4657 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|      5 | 4658 | `		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }` |
|      2 | 4659 | `	}` |
|      5 | 4660 | `	ph7_result_bool(pCtx,iCost != iWantCost);` |
|      5 | 4661 | `	return PH7_OK;` |
|      4 | 4662 | `}` |
|      - | 4663 | `/*` |
|      - | 4664 | ` * filter_var() — input validation and sanitization (the ext/filter API).` |
|      - | 4665 | ` *` |
|      - | 4666 | ` * Filter and flag identifiers (values match PHP 8.5; the constants themselves` |
|      - | 4667 | ` * are registered in constant.c). The validate filters are hand-rolled rather` |
|      - | 4668 | ` * than delegating to SyStrToInt64/SyStrToReal: the former silently skips leading` |
|      - | 4669 | ` * zeros and cannot signal overflow, and the latter treats ',' as a decimal point` |
|      - | 4670 | ` * unconditionally — neither matches PHP's filter semantics.` |
|      - | 4671 | ` */` |
|      - | 4672 | `#define FV_VALIDATE_INT     257` |
|      - | 4673 | `#define FV_VALIDATE_BOOLEAN 258` |
|      - | 4674 | `#define FV_VALIDATE_FLOAT   259` |
|      - | 4675 | `#define FV_VALIDATE_REGEXP  272` |
|      - | 4676 | `#define FV_VALIDATE_URL     273` |
|      - | 4677 | `#define FV_VALIDATE_EMAIL   274` |
|      - | 4678 | `#define FV_VALIDATE_IP      275` |
|      - | 4679 | `#define FV_VALIDATE_MAC     276` |
|      - | 4680 | `#define FV_VALIDATE_DOMAIN  277` |
|      - | 4681 | `#define FV_SANITIZE_SPECIAL_CHARS      515` |
|      - | 4682 | `#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */` |
|      - | 4683 | `#define FV_SANITIZE_EMAIL   517` |
|      - | 4684 | `#define FV_SANITIZE_URL     518` |
|      - | 4685 | `#define FV_SANITIZE_NUMBER_INT   519` |
|      - | 4686 | `#define FV_SANITIZE_NUMBER_FLOAT 520` |
|      - | 4687 | `#define FV_SANITIZE_FULL_SPECIAL_CHARS 522` |
|      - | 4688 | `#define FV_FLAG_ALLOW_OCTAL  1` |
|      - | 4689 | `#define FV_FLAG_ALLOW_HEX    2` |
|      - | 4690 | `#define FV_FLAG_STRIP_LOW    4` |
|      - | 4691 | `#define FV_FLAG_STRIP_HIGH   8` |
|      - | 4692 | `#define FV_FLAG_ENCODE_LOW   16` |
|      - | 4693 | `#define FV_FLAG_ENCODE_HIGH  32` |
|      - | 4694 | `#define FV_FLAG_ENCODE_AMP   64` |
|      - | 4695 | `#define FV_FLAG_NO_ENCODE_QUOTES 128` |
|      - | 4696 | `#define FV_FLAG_STRIP_BACKTICK   512` |
|      - | 4697 | `#define FV_FLAG_ALLOW_FRACTION   4096` |
|      - | 4698 | `#define FV_FLAG_ALLOW_THOUSAND   8192` |
|      - | 4699 | `#define FV_FLAG_ALLOW_SCIENTIFIC 16384` |
|      - | 4700 | `#define FV_FLAG_IPV4  1048576` |
|      - | 4701 | `#define FV_FLAG_IPV6  2097152` |
|      - | 4702 | `#define FV_NULL_ON_FAILURE 134217728` |
|      - | 4703 | `/* The subset of flags the UNSAFE_RAW/DEFAULT string filter (FvSanitizeString)` |
|      - | 4704 | ` * acts on: when none are set the filter is a verbatim pass-through, so FV_DEFAULT` |
|      - | 4705 | ` * can shortcut. Keep this in sync with FvSanitizeString's flag handling. */` |
|      - | 4706 | `#define FV_FLAG_STRING_MASK (FV_FLAG_STRIP_LOW\|FV_FLAG_STRIP_HIGH\|FV_FLAG_STRIP_BACKTICK \` |
|      - | 4707 | `                            \|FV_FLAG_ENCODE_LOW\|FV_FLAG_ENCODE_HIGH\|FV_FLAG_ENCODE_AMP)` |
|      - | 4708 |  |
|      - | 4709 | `/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.` |
|      - | 4710 | ` * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */` |
|    153 | 4711 | `static void FvTrim(const char **pz,int *pn){` |
|    153 | 4712 | `	const char *z = *pz;` |
|    153 | 4713 | `	int n = *pn;` |
|    157 | 4714 | `	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }` |
|    161 | 4715 | `	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }` |
|    153 | 4716 | `	*pz = z; *pn = n;` |
|    153 | 4717 | `}` |
|      - | 4718 | `/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     57 | 4719 | `static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){` |
|     57 | 4720 | `	int neg = 0, i;` |
|     57 | 4721 | `	sxu64 u = 0;` |
|     57 | 4722 | `	FvTrim(&z,&n);` |
|     57 | 4723 | `	if( n==0 ){ return 0; }` |
|     51 | 4724 | `	if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }` |
|     51 | 4725 | `	if( n==0 ){ return 0; }` |
|     49 | 4726 | `	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'\|\|z[1]=='X') ){` |
|      3 | 4727 | `		z += 2; n -= 2;` |
|      3 | 4728 | `		if( n==0 ){ return 0; }` |
|      7 | 4729 | `		for( i=0; i<n; i++ ){` |
|      5 | 4730 | `			int h = SyHexToint((unsigned char)z[i]);` |
|      5 | 4731 | `			if( h<0 ){ return 0; }` |
|      5 | 4732 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }` |
|      5 | 4733 | `			u = u*16 + (sxu64)h;` |
|      3 | 4734 | `		}` |
|     48 | 4735 | `	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){` |
|      9 | 4736 | `		for( i=0; i<n; i++ ){` |
|      7 | 4737 | `			if( z[i]<'0' \|\| z[i]>'7' ){ return 0; }` |
|      7 | 4738 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }` |
|      7 | 4739 | `			u = u*8 + (sxu64)(z[i]-'0');` |
|      4 | 4740 | `		}` |
|      2 | 4741 | `	}else{` |
|     45 | 4742 | `		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */` |
|    201 | 4743 | `		for( i=0; i<n; i++ ){` |
|    173 | 4744 | `			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|    161 | 4745 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }` |
|    161 | 4746 | `			u = u*10 + (sxu64)(z[i]-'0');` |
|     81 | 4747 | `		}` |
|      - | 4748 | `	}` |
|     33 | 4749 | `	if( neg ){` |
|      5 | 4750 | `		if( u > 0x8000000000000000ULL ){ return 0; }` |
|      5 | 4751 | `		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */` |
|      3 | 4752 | `	}else{` |
|     29 | 4753 | `		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }` |
|     27 | 4754 | `		*pOut = (ph7_int64)u;` |
|      - | 4755 | `	}` |
|     31 | 4756 | `	return 1;` |
|     29 | 4757 | `}` |
|      - | 4758 | `/* FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     69 | 4759 | `static int FvValidateFloat(const char *z,int n,int flags,double *pOut){` |
|      - | 4760 | `	char zBuf[512];` |
|     69 | 4761 | `	int i, m = 0, seenDigit = 0;` |
|     69 | 4762 | `	const char *zv; int nv; double d = 0;` |
|     69 | 4763 | `	FvTrim(&z,&n);` |
|      - | 4764 | `	/* Bound the input: zBuf[512] holds the thousand-separator-stripped copy, and` |
|      - | 4765 | `	 * the cap also rejects the pathological 500+ digit floats PHP refuses. */` |
|     69 | 4766 | `	if( n==0 \|\| n>500 ){ return 0; }` |
|     69 | 4767 | `	if( flags & FV_FLAG_ALLOW_THOUSAND ){` |
|      - | 4768 | `		/* Commas are optional, but when present they must group the integer part` |
|      - | 4769 | `		 * into a leading run of 1..3 digits followed by groups of exactly 3` |
|      - | 4770 | `		 * ("1,000" ok, "1,5"/"1234,567" rejected). Strip them into zBuf and reject` |
|      - | 4771 | `		 * a comma anywhere in the fractional/exponent tail. */` |
|     25 | 4772 | `		int s = 0, intEnd, segStart, segIdx, hasComma = 0;` |
|     25 | 4773 | `		if( s<n && (z[s]=='+'\|\|z[s]=='-') ){ zBuf[m++] = z[s]; s++; }` |
|     25 | 4774 | `		intEnd = s;` |
|    167 | 4775 | `		while( intEnd<n && z[intEnd]!='.' && z[intEnd]!='e' && z[intEnd]!='E' ){` |
|    143 | 4776 | `			if( z[intEnd]==',' ){ hasComma = 1; }` |
|    143 | 4777 | `			intEnd++;` |
|      1 | 4778 | `		}` |
|     25 | 4779 | `		if( hasComma ){` |
|     25 | 4780 | `			segStart = s; segIdx = 0;` |
|    165 | 4781 | `			for( i=s; i<=intEnd; i++ ){` |
|    151 | 4782 | `				if( i==intEnd \|\| z[i]==',' ){` |
|     49 | 4783 | `					int segLen = i - segStart, k;` |
|     49 | 4784 | `					if( segIdx==0 ){ if( segLen<1 \|\| segLen>3 ){ return 0; } }` |
|     25 | 4785 | `					else if( segLen!=3 ){ return 0; }` |
|    119 | 4786 | `					for( k=segStart; k<i; k++ ){` |
|     81 | 4787 | `						if( !SyisDigit((unsigned char)z[k]) ){ return 0; }` |
|     81 | 4788 | `						zBuf[m++] = z[k];` |
|     41 | 4789 | `					}` |
|     39 | 4790 | `					segStart = i+1; segIdx++;` |
|     19 | 4791 | `				}` |
|     71 | 4792 | `			}` |
|      8 | 4793 | `		}else{` |
|    ! 0 | 4794 | `			for( i=s; i<intEnd; i++ ){ zBuf[m++] = z[i]; }` |
|      - | 4795 | `		}` |
|     27 | 4796 | `		for( i=intEnd; i<n; i++ ){` |
|     13 | 4797 | `			if( z[i]==',' ){ return 0; }` |
|     13 | 4798 | `			zBuf[m++] = z[i];` |
|      7 | 4799 | `		}` |
|     15 | 4800 | `		zv = zBuf; nv = m;` |
|      8 | 4801 | `	}else{` |
|     45 | 4802 | `		zv = z; nv = n;` |
|      - | 4803 | `	}` |
|     59 | 4804 | `	i = 0;` |
|     59 | 4805 | `	if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|    167 | 4806 | `	while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     59 | 4807 | `	if( i<nv && zv[i]=='.' ){` |
|     21 | 4808 | `		i++;` |
|     39 | 4809 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     10 | 4810 | `	}` |
|     59 | 4811 | `	if( !seenDigit ){ return 0; }` |
|     57 | 4812 | `	if( i<nv && (zv[i]=='e'\|\|zv[i]=='E') ){` |
|     29 | 4813 | `		i++;` |
|     29 | 4814 | `		if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|     29 | 4815 | `		if( i>=nv \|\| !SyisDigit((unsigned char)zv[i]) ){ return 0; }` |
|    105 | 4816 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; }` |
|     14 | 4817 | `	}` |
|     57 | 4818 | `	if( i!=nv ){ return 0; } /* trailing junk */` |
|      - | 4819 | `	/* The grammar above guarantees zv[0..nv) is a clean ASCII decimal float (no hex /` |
|      - | 4820 | `	 * inf / nan / trailing junk), so it is safe to hand to libc strtod, which — unlike` |
|      - | 4821 | `	 * SyStrToReal (15 sig-digits + exponent clamped to 308, so it silently saturates` |
|      - | 4822 | `	 * overflowing magnitudes to a finite value) — is overflow/underflow-aware and` |
|      - | 4823 | `	 * correctly rounded. strtod needs a NUL-terminated string: the ALLOW_THOUSAND path` |
|      - | 4824 | `	 * already built the span in zBuf (zv==zBuf); the plain path must copy it there (z is` |
|      - | 4825 | `	 * const + not NUL-terminated). nv <= n <= 500 < sizeof(zBuf) by the cap above.` |
|      - | 4826 | `	 * Matches PHP 8.5 byte-for-byte: reject overflow (-> +/-INF) and total underflow` |
|      - | 4827 | `	 * (-> 0.0), keep subnormals (nonzero, errno==ERANGE) and a genuine "0" (errno==0). */` |
|     53 | 4828 | `	if( zv != zBuf ){ SyMemcpy(zv,zBuf,(sxu32)nv); }` |
|     53 | 4829 | `	zBuf[nv] = 0;` |
|     53 | 4830 | `	errno = 0;` |
|     53 | 4831 | `	d = strtod(zBuf,0);` |
|     53 | 4832 | `	if( errno == ERANGE && (d == HUGE_VAL \|\| d == -HUGE_VAL \|\| d == 0.0) ){` |
|     15 | 4833 | `		return 0;` |
|      - | 4834 | `	}` |
|     39 | 4835 | `	*pOut = d;` |
|     39 | 4836 | `	return 1;` |
|     35 | 4837 | `}` |
|      - | 4838 | `/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),` |
|      - | 4839 | ` * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as` |
|      - | 4840 | ` * false, NOT failures. */` |
|     33 | 4841 | `static int FvValidateBool(const char *z,int n,int *pBool){` |
|     33 | 4842 | `	FvTrim(&z,&n);` |
|     32 | 4843 | `	if( (n==1 && z[0]=='1') \|\| (n==4 && SyStrnicmp(z,"true",4)==0)` |
|     25 | 4844 | `	    \|\| (n==2 && SyStrnicmp(z,"on",2)==0) \|\| (n==3 && SyStrnicmp(z,"yes",3)==0) ){` |
|     11 | 4845 | `		*pBool = 1; return 1;` |
|      - | 4846 | `	}` |
|     22 | 4847 | `	if( n==0 \|\| (n==1 && z[0]=='0') \|\| (n==5 && SyStrnicmp(z,"false",5)==0)` |
|     11 | 4848 | `	    \|\| (n==3 && SyStrnicmp(z,"off",3)==0) \|\| (n==2 && SyStrnicmp(z,"no",2)==0) ){` |
|     11 | 4849 | `		*pBool = 0; return 1;` |
|      - | 4850 | `	}` |
|      9 | 4851 | `	return 0;` |
|     15 | 4852 | `}` |
|      - | 4853 | `/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. */` |
|     33 | 4854 | `static int FvValidateIp4(const char *z,int n){` |
|     33 | 4855 | `	int i = 0, parts = 0;` |
|     77 | 4856 | `	while( i<n ){` |
|     65 | 4857 | `		int val = 0, digits = 0, start = i;` |
|    143 | 4858 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|     85 | 4859 | `			val = val*10 + (z[i]-'0');` |
|     85 | 4860 | `			if( val>255 ){ return 0; }` |
|     79 | 4861 | `			digits++; i++;` |
|      1 | 4862 | `		}` |
|     59 | 4863 | `		if( digits==0 \|\| digits>3 ){ return 0; }` |
|     49 | 4864 | `		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */` |
|     45 | 4865 | `		parts++;` |
|     45 | 4866 | `		if( parts>4 ){ return 0; }` |
|     45 | 4867 | `		if( i<n ){` |
|     33 | 4868 | `			if( z[i]!='.' ){ return 0; }` |
|     33 | 4869 | `			i++;` |
|     33 | 4870 | `			if( i>=n ){ return 0; } /* trailing dot */` |
|     16 | 4871 | `		}` |
|      1 | 4872 | `	}` |
|     13 | 4873 | `	return parts==4;` |
|     17 | 4874 | `}` |
|      - | 4875 | `/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),` |
|      - | 4876 | ` * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1. */` |
|     19 | 4877 | `static int FvIp6Hextets(const char *z,int n){` |
|     19 | 4878 | `	int i = 0, segStart = 0, groups = 0;` |
|     19 | 4879 | `	if( n==0 ){ return 0; }` |
|    145 | 4880 | `	while( i<=n ){` |
|    133 | 4881 | `		if( i==n \|\| z[i]==':' ){` |
|     23 | 4882 | `			int segLen = i - segStart, j, isV4 = 0;` |
|     23 | 4883 | `			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */` |
|     77 | 4884 | `			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }` |
|     23 | 4885 | `			if( isV4 ){` |
|     11 | 4886 | `				if( i!=n ){ return -1; } /* IPv4 only as the final token */` |
|     11 | 4887 | `				if( !FvValidateIp4(z+segStart,segLen) ){ return -1; }` |
|      5 | 4888 | `				groups += 2;` |
|      3 | 4889 | `			}else{` |
|     13 | 4890 | `				if( segLen>4 ){ return -1; }` |
|     47 | 4891 | `				for( j=segStart; j<i; j++ ){ if( SyHexToint((unsigned char)z[j])<0 ){ return -1; } }` |
|     13 | 4892 | `				groups++;` |
|      - | 4893 | `			}` |
|     17 | 4894 | `			segStart = i+1;` |
|      8 | 4895 | `		}` |
|    127 | 4896 | `		i++;` |
|      1 | 4897 | `	}` |
|     13 | 4898 | `	return groups;` |
|     10 | 4899 | `}` |
|      - | 4900 | `/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present. */` |
|     19 | 4901 | `static int FvValidateIp6(const char *z,int n){` |
|     19 | 4902 | `	const char *zDbl = 0;` |
|      - | 4903 | `	int i, ga, gb;` |
|    139 | 4904 | `	for( i=0; i+1<n; i++ ){` |
|    123 | 4905 | `		if( z[i]==':' && z[i+1]==':' ){` |
|     13 | 4906 | `			if( zDbl ){ return 0; } /* a second "::" is invalid */` |
|     11 | 4907 | `			zDbl = z+i;` |
|      5 | 4908 | `		}` |
|     61 | 4909 | `	}` |
|     17 | 4910 | `	if( zDbl==0 ){` |
|      9 | 4911 | `		return FvIp6Hextets(z,n)==8;` |
|    ! 0 | 4912 | `	}else{` |
|      9 | 4913 | `		int lenA = (int)(zDbl - z);` |
|      9 | 4914 | `		int lenB = n - lenA - 2;` |
|      9 | 4915 | `		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA);` |
|      9 | 4916 | `		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB);` |
|      9 | 4917 | `		if( ga<0 \|\| gb<0 ){ return 0; }` |
|      9 | 4918 | `		return (ga+gb)<=7; /* "::" stands for at least one zero group */` |
|      - | 4919 | `	}` |
|     10 | 4920 | `}` |
|     25 | 4921 | `static int FvValidateIp(const char *z,int n,int flags){` |
|     25 | 4922 | `	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);` |
|     25 | 4923 | `	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */` |
|     25 | 4924 | `	if( v4 && FvValidateIp4(z,n) ){ return 1; }` |
|     21 | 4925 | `	if( v6 && FvValidateIp6(z,n) ){ return 1; }` |
|     13 | 4926 | `	return 0;` |
|     13 | 4927 | `}` |
|      - | 4928 | `/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */` |
|     11 | 4929 | `static int FvValidateMac(const char *z,int n){` |
|      - | 4930 | `	char sep;` |
|      - | 4931 | `	int i;` |
|     11 | 4932 | `	if( n!=17 ){ return 0; }` |
|      7 | 4933 | `	sep = z[2];` |
|      7 | 4934 | `	if( sep!=':' && sep!='-' ){ return 0; }` |
|    105 | 4935 | `	for( i=0; i<17; i++ ){` |
|    101 | 4936 | `		if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }` |
|     71 | 4937 | `		else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|     50 | 4938 | `	}` |
|      5 | 4939 | `	return 1;` |
|      6 | 4940 | `}` |
|      - | 4941 | `/* FILTER_VALIDATE_EMAIL (best-effort: covers the common cases, not quoted local` |
|      - | 4942 | ` * parts or IP-literal domains). */` |
|     28 | 4943 | `static int FvValidateEmail(const char *z,int n){` |
|     28 | 4944 | `	int at = -1, i, localLen, domLen, labelStart, dotCount = 0;` |
|      - | 4945 | `	const char *zDom;` |
|     28 | 4946 | `	if( n==0 \|\| n>320 ){ return 0; }` |
|    201 | 4947 | `	for( i=0; i<n; i++ ){` |
|    181 | 4948 | `		if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }` |
|     91 | 4949 | `	}` |
|     21 | 4950 | `	if( at<=0 \|\| at==n-1 ){ return 0; } /* one '@', non-empty local and domain */` |
|     21 | 4951 | `	localLen = at;` |
|     21 | 4952 | `	zDom = z + at + 1;` |
|     21 | 4953 | `	domLen = n - at - 1;` |
|     21 | 4954 | `	if( z[0]=='.' \|\| z[at-1]=='.' ){ return 0; }` |
|     57 | 4955 | `	for( i=0; i<localLen; i++ ){` |
|     43 | 4956 | `		unsigned char c = (unsigned char)z[i];` |
|     43 | 4957 | `		if( c<=' ' ){ return 0; }` |
|     41 | 4958 | `		if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }` |
|     20 | 4959 | `	}` |
|     15 | 4960 | `	if( zDom[0]=='.' \|\| zDom[domLen-1]=='.' ){ return 0; }` |
|     13 | 4961 | `	labelStart = 0;` |
|     85 | 4962 | `	for( i=0; i<=domLen; i++ ){` |
|     75 | 4963 | `		if( i==domLen \|\| zDom[i]=='.' ){` |
|     25 | 4964 | `			int ll = i - labelStart;` |
|     25 | 4965 | `			if( ll==0 ){ return 0; } /* consecutive dots */` |
|     23 | 4966 | `			if( zDom[labelStart]=='-' \|\| zDom[i-1]=='-' ){ return 0; }` |
|     23 | 4967 | `			if( i<domLen ){ dotCount++; }` |
|     23 | 4968 | `			labelStart = i+1;` |
|     12 | 4969 | `		}else{` |
|     51 | 4970 | `			unsigned char c = (unsigned char)zDom[i];` |
|     51 | 4971 | `			if( !((c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9')\|\|c=='-') ){ return 0; }` |
|      - | 4972 | `		}` |
|     37 | 4973 | `	}` |
|     11 | 4974 | `	if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */` |
|      9 | 4975 | `	return 1;` |
|     15 | 4976 | `}` |
|      - | 4977 | `/* FILTER_VALIDATE_DOMAIN (lenient, matching PHP without FILTER_FLAG_HOSTNAME). */` |
|     11 | 4978 | `static int FvValidateDomain(const char *z,int n){` |
|      - | 4979 | `	int i;` |
|     11 | 4980 | `	if( n<1 \|\| n>253 \|\| z[0]=='.' ){ return 0; }` |
|     81 | 4981 | `	for( i=0; i<n; i++ ){` |
|     75 | 4982 | `		unsigned char c = (unsigned char)z[i];` |
|     75 | 4983 | `		if( c<=' ' ){ return 0; }` |
|     75 | 4984 | `		if( c=='.' && i+1<n && z[i+1]=='.' ){ return 0; }` |
|     37 | 4985 | `	}` |
|      7 | 4986 | `	return 1;` |
|      6 | 4987 | `}` |
|      - | 4988 | `/* FILTER_VALIDATE_URL: require a scheme and a host (PHP's filter is itself` |
|      - | 4989 | ` * parse_url-based, so PH7_VmHttpSplitURI tracks it closely). */` |
|     15 | 4990 | `static int FvValidateUrl(const char *z,int n){` |
|      - | 4991 | `	SyhttpUri sUri;` |
|     15 | 4992 | `	if( n==0 ){ return 0; }` |
|     15 | 4993 | `	SyZero(&sUri,(sxu32)sizeof(sUri));` |
|     15 | 4994 | `	if( PH7_VmHttpSplitURI(&sUri,z,(sxu32)n)!=SXRET_OK ){ return 0; }` |
|     15 | 4995 | `	return sUri.sScheme.nByte!=0 && sUri.sHost.nByte!=0;` |
|      8 | 4996 | `}` |
|      - | 4997 | `/* The Fv sanitizers build their result by appending directly to the call` |
|      - | 4998 | ` * context (ph7_result_string accumulates, like htmlspecialchars), emitting each` |
|      - | 4999 | ` * kept run in one call and seeding "" so an all-stripped input yields "". */` |
|      - | 5000 | `/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */` |
|     37 | 5001 | `static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){` |
|     37 | 5002 | `	int i, runStart = 0;` |
|     37 | 5003 | `	ph7_result_string(pCtx,"",0);` |
|     97 | 5004 | `	for( i=0; i<n; i++ ){` |
|     91 | 5005 | `		char c = z[i];` |
|     91 | 5006 | `		int keep = (c>='0'&&c<='9') \|\| c=='+' \|\| c=='-';` |
|     91 | 5007 | `		if( !keep && isFloat ){` |
|     38 | 5008 | `			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))` |
|     23 | 5009 | `			    \|\| (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))` |
|     36 | 5010 | `			    \|\| ((c=='e'\|\|c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));` |
|     12 | 5011 | `		}` |
|     61 | 5012 | `		if( !keep ){` |
|     33 | 5013 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     33 | 5014 | `			runStart = i+1;` |
|     16 | 5015 | `		}` |
|     31 | 5016 | `	}` |
|      7 | 5017 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 5018 | `}` |
|      - | 5019 | `/* Return non-zero when byte c must be stripped under the STRIP_* flags. Shared` |
|      - | 5020 | ` * by the UNSAFE_RAW string filter and SANITIZE_SPECIAL_CHARS. STRIP_LOW drops` |
|      - | 5021 | `` * bytes <32, STRIP_HIGH drops bytes >=127 (incl. DEL), STRIP_BACKTICK drops '`'.`` |
|      - | 5022 | ` * Matches php_filter_strip(); verified byte-exact vs php 8.5.7. */` |
|    287 | 5023 | `static int FvStripByte(unsigned char c,int flags){` |
|    287 | 5024 | `	if( (flags & FV_FLAG_STRIP_LOW)      && c<32 )    { return 1; }` |
|    281 | 5025 | `	if( (flags & FV_FLAG_STRIP_HIGH)     && c>=127 )  { return 1; }` |
|    269 | 5026 | `	if( (flags & FV_FLAG_STRIP_BACKTICK) && c==0x60 ) { return 1; }` |
|    267 | 5027 | `	return 0;` |
|    144 | 5028 | `}` |
|      - | 5029 | `/* FILTER_UNSAFE_RAW / FILTER_DEFAULT with flags: no default transform, but the` |
|      - | 5030 | ` * STRIP/ENCODE flags apply. Precedence (per php_filter_unsafe_raw, verified` |
|      - | 5031 | ` * vs php 8.5.7): a byte is first tested for stripping; a surviving byte is then` |
|      - | 5032 | ` * encoded as a decimal numeric entity if ENCODE_LOW (<32) / ENCODE_HIGH (>=127)` |
|      - | 5033 | ` * is set, and '&' becomes "&#38;" under ENCODE_AMP. So STRIP_LOW\|ENCODE_LOW` |
|      - | 5034 | ` * strips (nothing left to encode). Bytes are treated individually — ENCODE_HIGH` |
|      - | 5035 | ` * numeric-encodes each byte of a multibyte sequence separately, not the codepoint. */` |
|     25 | 5036 | `static void FvSanitizeString(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 5037 | `	int i, runStart = 0;` |
|     25 | 5038 | `	ph7_result_string(pCtx,"",0);` |
|    193 | 5039 | `	for( i=0; i<n; i++ ){` |
|    179 | 5040 | `		unsigned char c = (unsigned char)z[i];` |
|    179 | 5041 | `		if( FvStripByte(c,flags) ){` |
|     13 | 5042 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     13 | 5043 | `			runStart = i+1;` |
|     13 | 5044 | `			continue;` |
|      - | 5045 | `		}` |
|    167 | 5046 | `		if( c=='&' && (flags & FV_FLAG_ENCODE_AMP) ){` |
|      3 | 5047 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      3 | 5048 | `			ph7_result_string(pCtx,"&#38;",-1);` |
|      3 | 5049 | `			runStart = i+1;` |
|    166 | 5050 | `		}else if( (c<32 && (flags & FV_FLAG_ENCODE_LOW))` |
|    164 | 5051 | `		       \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     37 | 5052 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 5053 | `			ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|      9 | 5054 | `			runStart = i+1;` |
|      4 | 5055 | `		}` |
|     79 | 5056 | `	}` |
|     15 | 5057 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     15 | 5058 | `}` |
|      - | 5059 | `/* FILTER_SANITIZE_SPECIAL_CHARS: encode <>&"' and every control byte <32 as a` |
|      - | 5060 | ` * decimal numeric entity (&#60; &#38; &#34; ...). The STRIP_* flags remove bytes` |
|      - | 5061 | ` * before encoding; ENCODE_HIGH numeric-encodes surviving bytes >=127. Bytes >=128` |
|      - | 5062 | ` * are otherwise passed through verbatim (this filter is NOT UTF-8-aware — only the` |
|      - | 5063 | ` * FULL variant is). Byte-exact vs php 8.5.7. */` |
|     13 | 5064 | `static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int flags){` |
|     13 | 5065 | `	int i, runStart = 0;` |
|      - | 5066 | `	const char *zEnt;` |
|     13 | 5067 | `	ph7_result_string(pCtx,"",0);` |
|    131 | 5068 | `	for( i=0; i<n; i++ ){` |
|    119 | 5069 | `		unsigned char c = (unsigned char)z[i];` |
|    119 | 5070 | `		if( FvStripByte(c,flags) ){` |
|      9 | 5071 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 5072 | `			runStart = i+1;` |
|      9 | 5073 | `			continue;` |
|      - | 5074 | `		}` |
|    111 | 5075 | `		switch( c ){` |
|      3 | 5076 | `		case '<':  zEnt = "&#60;"; break;` |
|      3 | 5077 | `		case '>':  zEnt = "&#62;"; break;` |
|     11 | 5078 | `		case '&':  zEnt = "&#38;"; break;` |
|      3 | 5079 | `		case '"':  zEnt = "&#34;"; break;` |
|      3 | 5080 | `		case '\'': zEnt = "&#39;"; break;` |
|     46 | 5081 | `		default:` |
|      - | 5082 | `			/* Control bytes <32 are always numeric-encoded; bytes >=127 only when` |
|      - | 5083 | `			 * ENCODE_HIGH is set. Everything else stays in the current run. */` |
|     93 | 5084 | `			if( c<32 \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     17 | 5085 | `				if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     17 | 5086 | `				ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|     17 | 5087 | `				runStart = i+1;` |
|      8 | 5088 | `			}` |
|     93 | 5089 | `			continue; /* keep in the current run */` |
|      - | 5090 | `		}` |
|     19 | 5091 | `		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     19 | 5092 | `		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */` |
|     19 | 5093 | `		runStart = i+1;` |
|     10 | 5094 | `	}` |
|     13 | 5095 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     13 | 5096 | `}` |
|      - | 5097 | `/* HTML 4.01 named-entity table (codepoint -> "&name;") used by the UTF-8-aware` |
|      - | 5098 | ` * FULL_SPECIAL_CHARS filter, sorted ascending by codepoint for binary search.` |
|      - | 5099 | ` * Generated from php 8.5.7 (the exact set php_escape_html_entities emits for the` |
|      - | 5100 | ` * default document type); the five inline specials <>&"' are handled separately,` |
|      - | 5101 | ` * so every entry here is a codepoint >=0xA0. 248 rows. */` |
|      - | 5102 | `static const struct { sxu32 cp; const char *zEnt; } aHtml401Ent[] = {` |
|      - | 5103 | `	{0x00A0,"&nbsp;"},{0x00A1,"&iexcl;"},{0x00A2,"&cent;"},{0x00A3,"&pound;"},` |
|      - | 5104 | `	{0x00A4,"&curren;"},{0x00A5,"&yen;"},{0x00A6,"&brvbar;"},{0x00A7,"&sect;"},` |
|      - | 5105 | `	{0x00A8,"&uml;"},{0x00A9,"&copy;"},{0x00AA,"&ordf;"},{0x00AB,"&laquo;"},` |
|      - | 5106 | `	{0x00AC,"&not;"},{0x00AD,"&shy;"},{0x00AE,"&reg;"},{0x00AF,"&macr;"},` |
|      - | 5107 | `	{0x00B0,"&deg;"},{0x00B1,"&plusmn;"},{0x00B2,"&sup2;"},{0x00B3,"&sup3;"},` |
|      - | 5108 | `	{0x00B4,"&acute;"},{0x00B5,"&micro;"},{0x00B6,"&para;"},{0x00B7,"&middot;"},` |
|      - | 5109 | `	{0x00B8,"&cedil;"},{0x00B9,"&sup1;"},{0x00BA,"&ordm;"},{0x00BB,"&raquo;"},` |
|      - | 5110 | `	{0x00BC,"&frac14;"},{0x00BD,"&frac12;"},{0x00BE,"&frac34;"},{0x00BF,"&iquest;"},` |
|      - | 5111 | `	{0x00C0,"&Agrave;"},{0x00C1,"&Aacute;"},{0x00C2,"&Acirc;"},{0x00C3,"&Atilde;"},` |
|      - | 5112 | `	{0x00C4,"&Auml;"},{0x00C5,"&Aring;"},{0x00C6,"&AElig;"},{0x00C7,"&Ccedil;"},` |
|      - | 5113 | `	{0x00C8,"&Egrave;"},{0x00C9,"&Eacute;"},{0x00CA,"&Ecirc;"},{0x00CB,"&Euml;"},` |
|      - | 5114 | `	{0x00CC,"&Igrave;"},{0x00CD,"&Iacute;"},{0x00CE,"&Icirc;"},{0x00CF,"&Iuml;"},` |
|      - | 5115 | `	{0x00D0,"&ETH;"},{0x00D1,"&Ntilde;"},{0x00D2,"&Ograve;"},{0x00D3,"&Oacute;"},` |
|      - | 5116 | `	{0x00D4,"&Ocirc;"},{0x00D5,"&Otilde;"},{0x00D6,"&Ouml;"},{0x00D7,"&times;"},` |
|      - | 5117 | `	{0x00D8,"&Oslash;"},{0x00D9,"&Ugrave;"},{0x00DA,"&Uacute;"},{0x00DB,"&Ucirc;"},` |
|      - | 5118 | `	{0x00DC,"&Uuml;"},{0x00DD,"&Yacute;"},{0x00DE,"&THORN;"},{0x00DF,"&szlig;"},` |
|      - | 5119 | `	{0x00E0,"&agrave;"},{0x00E1,"&aacute;"},{0x00E2,"&acirc;"},{0x00E3,"&atilde;"},` |
|      - | 5120 | `	{0x00E4,"&auml;"},{0x00E5,"&aring;"},{0x00E6,"&aelig;"},{0x00E7,"&ccedil;"},` |
|      - | 5121 | `	{0x00E8,"&egrave;"},{0x00E9,"&eacute;"},{0x00EA,"&ecirc;"},{0x00EB,"&euml;"},` |
|      - | 5122 | `	{0x00EC,"&igrave;"},{0x00ED,"&iacute;"},{0x00EE,"&icirc;"},{0x00EF,"&iuml;"},` |
|      - | 5123 | `	{0x00F0,"&eth;"},{0x00F1,"&ntilde;"},{0x00F2,"&ograve;"},{0x00F3,"&oacute;"},` |
|      - | 5124 | `	{0x00F4,"&ocirc;"},{0x00F5,"&otilde;"},{0x00F6,"&ouml;"},{0x00F7,"&divide;"},` |
|      - | 5125 | `	{0x00F8,"&oslash;"},{0x00F9,"&ugrave;"},{0x00FA,"&uacute;"},{0x00FB,"&ucirc;"},` |
|      - | 5126 | `	{0x00FC,"&uuml;"},{0x00FD,"&yacute;"},{0x00FE,"&thorn;"},{0x00FF,"&yuml;"},` |
|      - | 5127 | `	{0x0152,"&OElig;"},{0x0153,"&oelig;"},{0x0160,"&Scaron;"},{0x0161,"&scaron;"},` |
|      - | 5128 | `	{0x0178,"&Yuml;"},{0x0192,"&fnof;"},{0x02C6,"&circ;"},{0x02DC,"&tilde;"},` |
|      - | 5129 | `	{0x0391,"&Alpha;"},{0x0392,"&Beta;"},{0x0393,"&Gamma;"},{0x0394,"&Delta;"},` |
|      - | 5130 | `	{0x0395,"&Epsilon;"},{0x0396,"&Zeta;"},{0x0397,"&Eta;"},{0x0398,"&Theta;"},` |
|      - | 5131 | `	{0x0399,"&Iota;"},{0x039A,"&Kappa;"},{0x039B,"&Lambda;"},{0x039C,"&Mu;"},` |
|      - | 5132 | `	{0x039D,"&Nu;"},{0x039E,"&Xi;"},{0x039F,"&Omicron;"},{0x03A0,"&Pi;"},` |
|      - | 5133 | `	{0x03A1,"&Rho;"},{0x03A3,"&Sigma;"},{0x03A4,"&Tau;"},{0x03A5,"&Upsilon;"},` |
|      - | 5134 | `	{0x03A6,"&Phi;"},{0x03A7,"&Chi;"},{0x03A8,"&Psi;"},{0x03A9,"&Omega;"},` |
|      - | 5135 | `	{0x03B1,"&alpha;"},{0x03B2,"&beta;"},{0x03B3,"&gamma;"},{0x03B4,"&delta;"},` |
|      - | 5136 | `	{0x03B5,"&epsilon;"},{0x03B6,"&zeta;"},{0x03B7,"&eta;"},{0x03B8,"&theta;"},` |
|      - | 5137 | `	{0x03B9,"&iota;"},{0x03BA,"&kappa;"},{0x03BB,"&lambda;"},{0x03BC,"&mu;"},` |
|      - | 5138 | `	{0x03BD,"&nu;"},{0x03BE,"&xi;"},{0x03BF,"&omicron;"},{0x03C0,"&pi;"},` |
|      - | 5139 | `	{0x03C1,"&rho;"},{0x03C2,"&sigmaf;"},{0x03C3,"&sigma;"},{0x03C4,"&tau;"},` |
|      - | 5140 | `	{0x03C5,"&upsilon;"},{0x03C6,"&phi;"},{0x03C7,"&chi;"},{0x03C8,"&psi;"},` |
|      - | 5141 | `	{0x03C9,"&omega;"},{0x03D1,"&thetasym;"},{0x03D2,"&upsih;"},{0x03D6,"&piv;"},` |
|      - | 5142 | `	{0x2002,"&ensp;"},{0x2003,"&emsp;"},{0x2009,"&thinsp;"},{0x200C,"&zwnj;"},` |
|      - | 5143 | `	{0x200D,"&zwj;"},{0x200E,"&lrm;"},{0x200F,"&rlm;"},{0x2013,"&ndash;"},` |
|      - | 5144 | `	{0x2014,"&mdash;"},{0x2018,"&lsquo;"},{0x2019,"&rsquo;"},{0x201A,"&sbquo;"},` |
|      - | 5145 | `	{0x201C,"&ldquo;"},{0x201D,"&rdquo;"},{0x201E,"&bdquo;"},{0x2020,"&dagger;"},` |
|      - | 5146 | `	{0x2021,"&Dagger;"},{0x2022,"&bull;"},{0x2026,"&hellip;"},{0x2030,"&permil;"},` |
|      - | 5147 | `	{0x2032,"&prime;"},{0x2033,"&Prime;"},{0x2039,"&lsaquo;"},{0x203A,"&rsaquo;"},` |
|      - | 5148 | `	{0x203E,"&oline;"},{0x2044,"&frasl;"},{0x20AC,"&euro;"},{0x2111,"&image;"},` |
|      - | 5149 | `	{0x2118,"&weierp;"},{0x211C,"&real;"},{0x2122,"&trade;"},{0x2135,"&alefsym;"},` |
|      - | 5150 | `	{0x2190,"&larr;"},{0x2191,"&uarr;"},{0x2192,"&rarr;"},{0x2193,"&darr;"},` |
|      - | 5151 | `	{0x2194,"&harr;"},{0x21B5,"&crarr;"},{0x21D0,"&lArr;"},{0x21D1,"&uArr;"},` |
|      - | 5152 | `	{0x21D2,"&rArr;"},{0x21D3,"&dArr;"},{0x21D4,"&hArr;"},{0x2200,"&forall;"},` |
|      - | 5153 | `	{0x2202,"&part;"},{0x2203,"&exist;"},{0x2205,"&empty;"},{0x2207,"&nabla;"},` |
|      - | 5154 | `	{0x2208,"&isin;"},{0x2209,"&notin;"},{0x220B,"&ni;"},{0x220F,"&prod;"},` |
|      - | 5155 | `	{0x2211,"&sum;"},{0x2212,"&minus;"},{0x2217,"&lowast;"},{0x221A,"&radic;"},` |
|      - | 5156 | `	{0x221D,"&prop;"},{0x221E,"&infin;"},{0x2220,"&ang;"},{0x2227,"&and;"},` |
|      - | 5157 | `	{0x2228,"&or;"},{0x2229,"&cap;"},{0x222A,"&cup;"},{0x222B,"&int;"},` |
|      - | 5158 | `	{0x2234,"&there4;"},{0x223C,"&sim;"},{0x2245,"&cong;"},{0x2248,"&asymp;"},` |
|      - | 5159 | `	{0x2260,"&ne;"},{0x2261,"&equiv;"},{0x2264,"&le;"},{0x2265,"&ge;"},` |
|      - | 5160 | `	{0x2282,"&sub;"},{0x2283,"&sup;"},{0x2284,"&nsub;"},{0x2286,"&sube;"},` |
|      - | 5161 | `	{0x2287,"&supe;"},{0x2295,"&oplus;"},{0x2297,"&otimes;"},{0x22A5,"&perp;"},` |
|      - | 5162 | `	{0x22C5,"&sdot;"},{0x2308,"&lceil;"},{0x2309,"&rceil;"},{0x230A,"&lfloor;"},` |
|      - | 5163 | `	{0x230B,"&rfloor;"},{0x2329,"&lang;"},{0x232A,"&rang;"},{0x25CA,"&loz;"},` |
|      - | 5164 | `	{0x2660,"&spades;"},{0x2663,"&clubs;"},{0x2665,"&hearts;"},{0x2666,"&diams;"}` |
|      - | 5165 | `};` |
|      - | 5166 | `/* Binary-search aHtml401Ent[] for cp; return its "&name;" entity or 0. */` |
|     41 | 5167 | `static const char *FvHtml401Lookup(sxu32 cp){` |
|     41 | 5168 | `	int lo = 0, hi = (int)SX_ARRAYSIZE(aHtml401Ent) - 1;` |
|    323 | 5169 | `	while( lo <= hi ){` |
|    309 | 5170 | `		int mid = (lo + hi) / 2;` |
|    309 | 5171 | `		sxu32 c = aHtml401Ent[mid].cp;` |
|    309 | 5172 | `		if( c == cp ){ return aHtml401Ent[mid].zEnt; }` |
|    283 | 5173 | `		if( c < cp ){ lo = mid + 1; } else { hi = mid - 1; }` |
|      1 | 5174 | `	}` |
|     15 | 5175 | `	return 0;` |
|     21 | 5176 | `}` |
|      - | 5177 | `/* Decode one strict-UTF-8 sequence at p (< zEnd). On success returns its byte` |
|      - | 5178 | ` * length (1..4) and sets *pCp to the codepoint; on any malformed, overlong,` |
|      - | 5179 | ` * surrogate, truncated or out-of-range (>U+10FFFF) sequence returns 0. Matches` |
|      - | 5180 | ` * PHP's UTF-8 validation used by FULL_SPECIAL_CHARS (verified vs php 8.5.7). */` |
|    101 | 5181 | `static int FvUtf8Next(const unsigned char *p,const unsigned char *zEnd,sxu32 *pCp){` |
|    101 | 5182 | `	unsigned char c = p[0];` |
|    101 | 5183 | `	if( c < 0x80 ){ *pCp = c; return 1; }` |
|    101 | 5184 | `	if( c < 0xC2 ){ return 0; }              /* 0x80-0xBF stray cont / 0xC0-0xC1 overlong */` |
|     99 | 5185 | `	if( c < 0xE0 ){                          /* 2-byte: U+0080..U+07FF */` |
|     47 | 5186 | `		if( zEnd-p < 2 \|\| (p[1]&0xC0)!=0x80 ){ return 0; }` |
|     45 | 5187 | `		*pCp = ((sxu32)(c&0x1F)<<6) \| (p[1]&0x3F);` |
|     45 | 5188 | `		return 2;` |
|      - | 5189 | `	}` |
|     53 | 5190 | `	if( c < 0xF0 ){                          /* 3-byte: U+0800..U+FFFF minus surrogates */` |
|      - | 5191 | `		sxu32 cp;` |
|     47 | 5192 | `		if( zEnd-p < 3 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 ){ return 0; }` |
|     33 | 5193 | `		cp = ((sxu32)(c&0x0F)<<12) \| ((sxu32)(p[1]&0x3F)<<6) \| (p[2]&0x3F);` |
|     33 | 5194 | `		if( cp < 0x800 \|\| (cp>=0xD800 && cp<=0xDFFF) ){ return 0; }` |
|     29 | 5195 | `		*pCp = cp;` |
|     29 | 5196 | `		return 3;` |
|      - | 5197 | `	}` |
|      7 | 5198 | `	if( c < 0xF5 ){                          /* 4-byte: U+10000..U+10FFFF */` |
|      - | 5199 | `		sxu32 cp;` |
|      5 | 5200 | `		if( zEnd-p < 4 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 \|\| (p[3]&0xC0)!=0x80 ){ return 0; }` |
|      5 | 5201 | `		cp = ((sxu32)(c&0x07)<<18) \| ((sxu32)(p[1]&0x3F)<<12) \| ((sxu32)(p[2]&0x3F)<<6) \| (p[3]&0x3F);` |
|      5 | 5202 | `		if( cp < 0x10000 \|\| cp > 0x10FFFF ){ return 0; }` |
|      5 | 5203 | `		*pCp = cp;` |
|      5 | 5204 | `		return 4;` |
|      - | 5205 | `	}` |
|      3 | 5206 | `	return 0;                                /* 0xF5-0xFF */` |
|     51 | 5207 | `}` |
|      - | 5208 | `/* FILTER_SANITIZE_FULL_SPECIAL_CHARS: htmlentities-style, UTF-8-aware. Encodes` |
|      - | 5209 | ` * <>&"' as named entities ("'" -> &#039;; quotes suppressed under NO_ENCODE_QUOTES),` |
|      - | 5210 | ` * and every valid UTF-8 codepoint with an HTML 4.01 named entity as that entity;` |
|      - | 5211 | ` * valid codepoints without a named entity (and low control bytes) pass through` |
|      - | 5212 | ` * verbatim. If the input contains ANY invalid UTF-8 the whole result is "".` |
|      - | 5213 | ` * The STRIP/ENCODE flags do NOT apply to this filter (only NO_ENCODE_QUOTES).` |
|      - | 5214 | ` * php's filter does NOT re-encode valid pre-existing entities ("&amp;" stays,` |
|      - | 5215 | ` * "&bogus;" becomes "&amp;bogus;"), i.e. double_encode=false semantics —` |
|      - | 5216 | ` * exactly htmlentities(ENT_QUOTES\|ENT_HTML401, double_encode: false), so this` |
|      - | 5217 | ` * delegates to the shared encoder. Byte-exact vs php 8.5.7. */` |
|     25 | 5218 | `static void FvSanitizeFull(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 5219 | `	int iEntFlags = (flags & FV_FLAG_NO_ENCODE_QUOTES) ? 0 : PH7_ENT_QUOTES;` |
|     25 | 5220 | `	HtmlEscape(pCtx,z,n,iEntFlags,1/*bAll*/,0/*bDoubleEncode*/);` |
|     25 | 5221 | `}` |
|      - | 5222 | `/* ---------------------------------------------------------------------------` |
|      - | 5223 | ` * UTF-8-aware HTML entity core (htmlspecialchars/htmlentities family).` |
|      - | 5224 | ` * Prototyped next to the five builtins earlier in this file; lives here so it` |
|      - | 5225 | ` * can share aHtml401Ent[]/FvHtml401Lookup()/FvUtf8Next() with the filter_var` |
|      - | 5226 | ` * FULL_SPECIAL_CHARS filter above. Byte-exact vs php 8.5.7 (oracle-swept).` |
|      - | 5227 | ` * ------------------------------------------------------------------------ */` |
|      - | 5228 | `/* Encode cp as UTF-8 into zBuf (>= 4 bytes); return the byte length 1..4.` |
|      - | 5229 | ` * Thin wrapper over the engine-wide SX_WRITE_UTF8 (sxmacros.h). */` |
|    585 | 5230 | `static int HtmlCpUtf8(sxu32 cp,char *zBuf){` |
|    585 | 5231 | `	sxu8 *z = (sxu8 *)zBuf;` |
|    585 | 5232 | `	SX_WRITE_UTF8(z,cp);` |
|    585 | 5233 | `	return (int)(z - (sxu8 *)zBuf);` |
|      1 | 5234 | `}` |
|      - | 5235 | `/* Doctype-allowed codepoint test (php's unicode_cp_is_allowed) — gates what a` |
|      - | 5236 | ` * numeric reference may DECODE to. Oracle-pinned per doctype: HTML401` |
|      - | 5237 | ` * disallows C0 (except TAB/LF/CR) and DEL..U+009F; XML1 and XHTML share the` |
|      - | 5238 | ` * XML rules — DEL..U+009F allowed, U+FFFE/U+FFFF excluded; HTML5 swaps CR` |
|      - | 5239 | ` * for FF (0x0C) and excludes the noncharacters (U+FDD0..U+FDEF and every` |
|      - | 5240 | ` * U+xFFFE/U+xFFFF). Surrogates are disallowed everywhere. */` |
|     91 | 5241 | `static int HtmlCpAllowed(sxu32 cp,int iFlags){` |
|     91 | 5242 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|     91 | 5243 | `	if( cp==0x09 \|\| cp==0x0A ){ return 1; }` |
|     87 | 5244 | `	if( cp==0x0D ){ return iDoc != PH7_ENT_DOC_HTML5; }` |
|     85 | 5245 | `	if( cp==0x0C ){ return iDoc == PH7_ENT_DOC_HTML5; }` |
|     85 | 5246 | `	if( cp < 0x20 \|\| cp > 0x10FFFF ){ return 0; }` |
|     79 | 5247 | `	if( cp>=0xD800 && cp<=0xDFFF ){ return 0; }` |
|     77 | 5248 | `	if( cp>=0x7F && cp<=0x9F ){ return iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML; }` |
|     71 | 5249 | `	if( iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML ){` |
|    ! 0 | 5250 | `		return cp!=0xFFFE && cp!=0xFFFF;` |
|      - | 5251 | `	}` |
|     71 | 5252 | `	if( iDoc == PH7_ENT_DOC_HTML5 ){` |
|      9 | 5253 | `		if( cp>=0xFDD0 && cp<=0xFDEF ){ return 0; }` |
|      9 | 5254 | `		if( (cp & 0xFFFF) >= 0xFFFE ){ return 0; }` |
|      4 | 5255 | `	}` |
|     71 | 5256 | `	return 1;` |
|     46 | 5257 | `}` |
|      - | 5258 | `/* The ENT_DISALLOWED gate for RAW characters on the ENCODE side. Same as the` |
|      - | 5259 | ` * decode gate except CR under HTML5: php's encode-side unicode_cp_is_allowed` |
|      - | 5260 | ` * keeps a literal "\r" verbatim under ENT_HTML5\|ENT_DISALLOWED while the` |
|      - | 5261 | ` * decode side leaves "&#13;" un-decoded (oracle-pinned at flags 176). */` |
|      9 | 5262 | `static int HtmlCpAllowedEncode(sxu32 cp,int iFlags){` |
|      9 | 5263 | `	if( cp==0x0D && (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 ){ return 1; }` |
|      9 | 5264 | `	return HtmlCpAllowed(cp,iFlags);` |
|      5 | 5265 | `}` |
|      - | 5266 | `/* Numeric-reference validity for the double_encode=false "is this already a` |
|      - | 5267 | ` * valid entity" test — a MUCH looser predicate than the decode gate above:` |
|      - | 5268 | ` * any codepoint <= U+10FFFF is valid (controls and surrogates included, every` |
|      - | 5269 | ` * doctype). ENT_DISALLOWED re-tightens non-HTML401 doctypes to the decode` |
|      - | 5270 | ` * gate, except that HTML5 exempts surrogates. All oracle-pinned: &#0; and` |
|      - | 5271 | ` * &#xD800; stay verbatim at flags 11 and 139; flags -1 (HTML5+DISALLOWED)` |
|      - | 5272 | ` * re-encodes &#0; and &#x10FFFF; but still keeps &#xD800;; flags 144` |
|      - | 5273 | ` * (XML1+DISALLOWED) re-encodes &#xD800;. */` |
|      9 | 5274 | `static int HtmlNumericAllowed(sxu32 cp,int iFlags){` |
|      9 | 5275 | `	if( cp > 0x10FFFF ){ return 0; }` |
|      7 | 5276 | `	if( (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML401 ){ return 1; /* never tightened */ }` |
|    ! 0 | 5277 | `	if( (iFlags & PH7_ENT_DISALLOWED)` |
|    ! 0 | 5278 | `	 && !((iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 && cp>=0xD800 && cp<=0xDFFF)` |
|    ! 0 | 5279 | `	 && !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|    ! 0 | 5280 | `	return 1;` |
|      5 | 5281 | `}` |
|      - | 5282 | `/* How many bytes the malformed UTF-8 sequence at p consumes — php's` |
|      - | 5283 | ` * get_next_char failure step (one U+FFFD substitution / one ENT_IGNORE drop` |
|      - | 5284 | ` * per MAXIMAL invalid subpart, not per byte): a prefix-valid sequence eats` |
|      - | 5285 | ` * its continuation bytes ("\xE0\x80\xAF" is ONE unit) while a byte that could` |
|      - | 5286 | ` * start a new sequence is left for the next round. */` |
|      5 | 5287 | `static int HtmlUtf8Trail(unsigned char c){ return c>=0x80 && c<=0xBF; }` |
|     11 | 5288 | `static int HtmlUtf8Lead(unsigned char c){ return c<0x80 \|\| (c>=0xC2 && c<=0xF4); }` |
|     15 | 5289 | `static int HtmlUtf8FailAdvance(const unsigned char *p,const unsigned char *zEnd){` |
|     15 | 5290 | `	unsigned char c = p[0];` |
|     15 | 5291 | `	int nAvail = (int)(zEnd - p);` |
|     15 | 5292 | `	if( c < 0xC2 \|\| c > 0xF4 ){ return 1; } /* stray trail / C0-C1 / F5-FF */` |
|     13 | 5293 | `	if( c < 0xE0 ){` |
|      3 | 5294 | `		if( nAvail < 2 ){ return 1; }` |
|      3 | 5295 | `		return HtmlUtf8Lead(p[1]) ? 1 : 2;` |
|      - | 5296 | `	}` |
|     11 | 5297 | `	if( c < 0xF0 ){` |
|     11 | 5298 | `		if( nAvail >= 3 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) ){` |
|      3 | 5299 | `			return 3; /* complete but overlong/surrogate */` |
|      - | 5300 | `		}` |
|      9 | 5301 | `		if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 5302 | `		if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 5303 | `		return 3;` |
|      - | 5304 | `	}` |
|    ! 0 | 5305 | `	if( nAvail >= 4 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) && HtmlUtf8Trail(p[3]) ){` |
|    ! 0 | 5306 | `		return 4; /* complete but overlong / > U+10FFFF */` |
|      - | 5307 | `	}` |
|    ! 0 | 5308 | `	if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 5309 | `	if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 5310 | `	if( nAvail < 4 \|\| HtmlUtf8Lead(p[3]) ){ return 3; }` |
|    ! 0 | 5311 | `	return 4;` |
|      8 | 5312 | `}` |
|      - | 5313 | `/* The basic special entities, shared by named matching, the hsc_decode` |
|      - | 5314 | ` * numeric whitelist and the translation-table builder so the sets can never` |
|      - | 5315 | ` * drift apart. (&apos; is not an HTML 4.01 entity — doctype-gated below.) */` |
|      - | 5316 | `static const struct { const char *zEnt; int n; sxu32 cp; } aHtmlSpecEnt[] = {` |
|      - | 5317 | `	{"&amp;",5,38},{"&lt;",4,60},{"&gt;",4,62},{"&quot;",6,34},{"&apos;",6,39}` |
|      - | 5318 | `};` |
|      - | 5319 | `/* Does this doctype consult the named-entity table (aHtml401Ent)? XML 1.0 has` |
|      - | 5320 | ` * no named entities beyond the specials; XHTML/HTML5 are approximated by the` |
|      - | 5321 | ` * HTML 4.01 table (documented divergence, PLAN.md §3.9). */` |
|     63 | 5322 | `static int HtmlDocHasNamedTable(int iDoc){` |
|     63 | 5323 | `	return iDoc != PH7_ENT_DOC_XML1;` |
|      1 | 5324 | `}` |
|      - | 5325 | `/* The single-quote entity per doctype. Oracle-pinned asymmetry: for every` |
|      - | 5326 | ` * non-HTML401 doctype htmlspecialchars emits &apos; while htmlentities` |
|      - | 5327 | ` * (bEntities) keeps &#039; under XHTML too. The translation table mirrors` |
|      - | 5328 | ` * whichever function the requested table belongs to. */` |
|     29 | 5329 | `static const char *HtmlAposEntity(int iDoc,int bEntities){` |
|     29 | 5330 | `	if( iDoc == PH7_ENT_DOC_HTML401 \|\| (bEntities && iDoc == PH7_ENT_DOC_XHTML) ){` |
|     21 | 5331 | `		return "&#039;";` |
|      - | 5332 | `	}` |
|      9 | 5333 | `	return "&apos;";` |
|     15 | 5334 | `}` |
|      - | 5335 | `/* Try to parse one HTML entity at z (z[0]=='&', z < zEnd). bFull selects the` |
|      - | 5336 | ` * html_entity_decode set (doctype named table + any allowed numeric ref) vs` |
|      - | 5337 | ` * the htmlspecialchars_decode set (the basic specials + quote numerics only).` |
|      - | 5338 | ` * Named matching is case-SENSITIVE and the ';' is required (both PHP-exact);` |
|      - | 5339 | ` * numeric refs accept dec/hex (x or X) with any number of leading zeros but` |
|      - | 5340 | ` * reject out-of-range, surrogate and doctype-disallowed codepoints (the` |
|      - | 5341 | ` * caller then leaves the source verbatim). Quote-flag gating is NOT applied` |
|      - | 5342 | ` * here — the same routine doubles as the "is this a valid entity" test for` |
|      - | 5343 | ` * double_encode=false, which ignores the quote bits (oracle-pinned).` |
|      - | 5344 | ` * bEncodeCheck selects the looser HtmlNumericAllowed predicate used by that` |
|      - | 5345 | ` * double_encode test; decode callers pass 0 for the HtmlCpAllowed gate.` |
|      - | 5346 | ` * On success sets *pCp / *pnConsumed and returns 1. */` |
|    172 | 5347 | `static int HtmlParseEntity(const unsigned char *z,const unsigned char *zEnd,` |
|      1 | 5348 | `                           int iFlags,int bFull,int bEncodeCheck,sxu32 *pCp,int *pnConsumed){` |
|    173 | 5349 | `	int nAvail = (int)(zEnd - z);` |
|    173 | 5350 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 5351 | `	sxu32 n;` |
|    173 | 5352 | `	if( nAvail < 4 ){ return 0; } /* shortest entities: &lt; &#9; */` |
|    169 | 5353 | `	if( z[1] == '#' ){` |
|      - | 5354 | `		/* Numeric reference */` |
|     89 | 5355 | `		sxu32 cp = 0;` |
|     89 | 5356 | `		int i = 2, bHex = 0, nDig = 0;` |
|     89 | 5357 | `		if( z[i]=='x' \|\| z[i]=='X' ){ bHex = 1; i++; }` |
|    317 | 5358 | `		for( ; i < nAvail && z[i] != ';' ; i++ ){` |
|      - | 5359 | `			int v;` |
|    221 | 5360 | `			unsigned char c = z[i];` |
|    221 | 5361 | `			if( c>='0' && c<='9' ){ v = c - '0'; }` |
|     17 | 5362 | `			else if( bHex && c>='a' && c<='f' ){ v = c - 'a' + 10; }` |
|     17 | 5363 | `			else if( bHex && c>='A' && c<='F' ){ v = c - 'A' + 10; }` |
|    ! 0 | 5364 | `			else { return 0; }` |
|      - | 5365 | `			/* Stop accumulating once out of range (keeps validating the shape;` |
|      - | 5366 | `			 * max intermediate is 0x10FFFF*16+15, no sxu32 overflow). */` |
|    221 | 5367 | `			if( cp <= 0x10FFFF ){ cp = cp * (bHex ? 16 : 10) + (sxu32)v; }` |
|    221 | 5368 | `			nDig++;` |
|    111 | 5369 | `		}` |
|     97 | 5370 | `		if( nDig == 0 \|\| i >= nAvail ){ return 0; } /* no digits / no ';' */` |
|     97 | 5371 | `		if( bEncodeCheck ? !HtmlNumericAllowed(cp,iFlags) : !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|     83 | 5372 | `		if( !bFull ){` |
|      - | 5373 | `			/* hsc_decode: numeric refs to the five specials only. */` |
|     99 | 5374 | `			for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) && aHtmlSpecEnt[n].cp != cp ; n++ ){}` |
|     25 | 5375 | `			if( n >= SX_ARRAYSIZE(aHtmlSpecEnt) ){ return 0; }` |
|     11 | 5376 | `		}` |
|     75 | 5377 | `		*pCp = cp;` |
|     75 | 5378 | `		*pnConsumed = i + 1;` |
|     75 | 5379 | `		return 1;` |
|      - | 5380 | `	}` |
|      - | 5381 | `	/* Named reference — every entity name starts with a letter, so anything` |
|      - | 5382 | `	 * else can bail out before touching the tables. */` |
|     81 | 5383 | `	if( !((z[1]>='a' && z[1]<='z') \|\| (z[1]>='A' && z[1]<='Z')) ){ return 0; }` |
|    287 | 5384 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) ; n++ ){` |
|    265 | 5385 | `		if( aHtmlSpecEnt[n].cp == 39 && iDoc == PH7_ENT_DOC_HTML401 ){ continue; }` |
|    243 | 5386 | `		if( nAvail >= aHtmlSpecEnt[n].n && SyMemcmp(z,aHtmlSpecEnt[n].zEnt,(sxu32)aHtmlSpecEnt[n].n) == 0 ){` |
|     53 | 5387 | `			*pCp = aHtmlSpecEnt[n].cp;` |
|     53 | 5388 | `			*pnConsumed = aHtmlSpecEnt[n].n;` |
|     53 | 5389 | `			return 1;` |
|      - | 5390 | `		}` |
|     96 | 5391 | `	}` |
|     23 | 5392 | `	if( bFull && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 5393 | `		/* Linear scan of the 248-row table: runs only at '&'-then-letter` |
|      - | 5394 | `		 * positions and guarantees the decode set can never drift from the` |
|      - | 5395 | `		 * encode table. The first-letter guard skips the SyStrlen/SyMemcmp` |
|      - | 5396 | `		 * for ~96% of rows. */` |
|   3369 | 5397 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|      - | 5398 | `			sxu32 nEnt;` |
|   3357 | 5399 | `			if( z[1] != (unsigned char)aHtml401Ent[n].zEnt[1] ){ continue; }` |
|    121 | 5400 | `			nEnt = SyStrlen(aHtml401Ent[n].zEnt);` |
|    121 | 5401 | `			if( (sxu32)nAvail >= nEnt && SyMemcmp(z,aHtml401Ent[n].zEnt,nEnt) == 0 ){` |
|      7 | 5402 | `				*pCp = aHtml401Ent[n].cp;` |
|      7 | 5403 | `				*pnConsumed = (int)nEnt;` |
|      7 | 5404 | `				return 1;` |
|      - | 5405 | `			}` |
|     58 | 5406 | `		}` |
|      6 | 5407 | `	}` |
|     17 | 5408 | `	return 0;` |
|     88 | 5409 | `}` |
|      - | 5410 | `/* Shared encoder for htmlspecialchars (bAll=0) and htmlentities (bAll=1).` |
|      - | 5411 | ` * Invalid UTF-8 policy: ENT_IGNORE drops the byte (and wins over SUBSTITUTE),` |
|      - | 5412 | ` * ENT_SUBSTITUTE emits one U+FFFD per invalid byte, neither -> the whole` |
|      - | 5413 | ` * result is "" (pre-validated in a first pass: the accumulating result API` |
|      - | 5414 | ` * cannot roll back — same reason FvSanitizeFull is two-pass). */` |
|     94 | 5415 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 5416 | `                       int iFlags,int bAll,int bDoubleEncode){` |
|     95 | 5417 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     95 | 5418 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|      - | 5419 | `	const unsigned char *runStart;` |
|     95 | 5420 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 5421 | `	sxu32 cp;` |
|     95 | 5422 | `	if( (iFlags & (PH7_ENT_IGNORE\|PH7_ENT_SUBSTITUTE)) == 0 ){` |
|      - | 5423 | `		/* Pass 1: any malformed sequence rejects the entire input. ASCII` |
|      - | 5424 | `		 * bytes cannot be malformed, so skip them without the decoder. */` |
|    381 | 5425 | `		while( p < zEnd ){` |
|      - | 5426 | `			int len;` |
|    323 | 5427 | `			if( *p < 0x80 ){ p++; continue; }` |
|     37 | 5428 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     37 | 5429 | `			if( len == 0 ){ ph7_result_string(pCtx,"",0); return; }` |
|     27 | 5430 | `			p += len;` |
|      1 | 5431 | `		}` |
|     59 | 5432 | `		p = (const unsigned char *)zIn;` |
|     29 | 5433 | `	}` |
|     85 | 5434 | `	runStart = p;` |
|     85 | 5435 | `	ph7_result_string(pCtx,"",0);` |
|    455 | 5436 | `	while( p < zEnd ){` |
|    371 | 5437 | `		const char *zEnt = 0;` |
|      - | 5438 | `		int len;` |
|    371 | 5439 | `		if( *p < 0x80 ){` |
|    307 | 5440 | `			len = 1;` |
|    307 | 5441 | `			switch( *p ){` |
|     25 | 5442 | `			case '<': zEnt = "&lt;"; break;` |
|     25 | 5443 | `			case '>': zEnt = "&gt;"; break;` |
|     18 | 5444 | `			case '&':` |
|     37 | 5445 | `				zEnt = "&amp;";` |
|     37 | 5446 | `				if( !bDoubleEncode ){` |
|      - | 5447 | `					sxu32 eCp; int nEat;` |
|     25 | 5448 | `					if( HtmlParseEntity(p,zEnd,iFlags,1,1,&eCp,&nEat) ){` |
|      - | 5449 | `						/* A valid existing entity: keep it verbatim. */` |
|     13 | 5450 | `						zEnt = 0;` |
|     13 | 5451 | `						len = nEat;` |
|      6 | 5452 | `					}` |
|     12 | 5453 | `				}` |
|     37 | 5454 | `				break;` |
|     10 | 5455 | `			case '"':` |
|     21 | 5456 | `				if( iFlags & PH7_ENT_QUOTE_DOUBLE ){ zEnt = "&quot;"; }` |
|     21 | 5457 | `				break;` |
|     12 | 5458 | `			case '\'':` |
|     25 | 5459 | `				if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|     23 | 5460 | `					zEnt = HtmlAposEntity(iDoc,bAll);` |
|     11 | 5461 | `				}` |
|     25 | 5462 | `				break;` |
|     89 | 5463 | `			default:` |
|    179 | 5464 | `				if( (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode((sxu32)*p,iFlags) ){` |
|    ! 0 | 5465 | `					zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 5466 | `				}` |
|    178 | 5467 | `				break;` |
|      - | 5468 | `			}` |
|    154 | 5469 | `		}else{` |
|     65 | 5470 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     65 | 5471 | `			if( len == 0 ){` |
|      - | 5472 | `				/* Malformed subpart (IGNORE or SUBSTITUTE is set, else pass 1` |
|      - | 5473 | `				 * would have rejected): drop it or emit ONE U+FFFD for the` |
|      - | 5474 | `				 * whole unit (php substitutes per maximal invalid subpart). */` |
|     15 | 5475 | `				if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|     15 | 5476 | `				if( (iFlags & PH7_ENT_IGNORE) == 0 ){ ph7_result_string(pCtx,"\xEF\xBF\xBD",3); }` |
|     15 | 5477 | `				p += HtmlUtf8FailAdvance(p,zEnd);` |
|     15 | 5478 | `				runStart = p;` |
|     15 | 5479 | `				continue;` |
|      - | 5480 | `			}` |
|     51 | 5481 | `			if( bAll && HtmlDocHasNamedTable(iDoc) ){` |
|     41 | 5482 | `				zEnt = FvHtml401Lookup(cp);` |
|     20 | 5483 | `			}` |
|     51 | 5484 | `			if( zEnt == 0 && (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode(cp,iFlags) ){` |
|    ! 0 | 5485 | `				zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 5486 | `			}` |
|      - | 5487 | `		}` |
|    357 | 5488 | `		if( zEnt ){` |
|    135 | 5489 | `			if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|    135 | 5490 | `			ph7_result_string(pCtx,zEnt,-1);` |
|    135 | 5491 | `			runStart = p + len;` |
|     67 | 5492 | `		}` |
|    357 | 5493 | `		p += len;` |
|      1 | 5494 | `	}` |
|     85 | 5495 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     48 | 5496 | `}` |
|      - | 5497 | `/* Shared decoder for html_entity_decode (bFull=1) and htmlspecialchars_decode` |
|      - | 5498 | ` * (bFull=0). Quote refs (cp 34/39, named or numeric) are gated by the quote` |
|      - | 5499 | ` * bits and left verbatim when suppressed; an invalid entity leaves its '&'` |
|      - | 5500 | ` * verbatim and rescans right after it, which also yields PHP's no-double-` |
|      - | 5501 | ` * decode behavior ("&amp;lt;" -> "&lt;"). */` |
|     82 | 5502 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 5503 | `                         int iFlags,int bFull){` |
|     83 | 5504 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     83 | 5505 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|     83 | 5506 | `	const unsigned char *runStart = p;` |
|     83 | 5507 | `	ph7_result_string(pCtx,"",0);` |
|    557 | 5508 | `	while( p < zEnd ){` |
|      - | 5509 | `		sxu32 cp;` |
|      - | 5510 | `		int nEat;` |
|    510 | 5511 | `		if( *p != '&' ){ p++; continue; }` |
|    155 | 5512 | `		if( !HtmlParseEntity(p,zEnd,iFlags,bFull,0,&cp,&nEat) ){ p++; continue; }` |
|    124 | 5513 | `		if( (cp == 34 && (iFlags & PH7_ENT_QUOTE_DOUBLE) == 0)` |
|    117 | 5514 | `		 \|\| (cp == 39 && (iFlags & PH7_ENT_QUOTE_SINGLE) == 0) ){` |
|      - | 5515 | `			/* Suppressed quote: leave the entity source verbatim. */` |
|     37 | 5516 | `			p += nEat;` |
|     37 | 5517 | `			continue;` |
|      - | 5518 | `		}` |
|     89 | 5519 | `		if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|      - | 5520 | `		{` |
|      - | 5521 | `			char zBuf[4];` |
|     89 | 5522 | `			int n = HtmlCpUtf8(cp,zBuf);` |
|     89 | 5523 | `			ph7_result_string(pCtx,zBuf,n);` |
|      - | 5524 | `		}` |
|     89 | 5525 | `		p += nEat;` |
|     89 | 5526 | `		runStart = p;` |
|      1 | 5527 | `	}` |
|     79 | 5528 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     79 | 5529 | `}` |
|      - | 5530 | `/* Validate the optional charset argument at apArg[idx]: UTF-8 aliases (and` |
|      - | 5531 | ` * ""/NULL meaning the default) are accepted; anything else — including` |
|      - | 5532 | ` * php-supported single-byte charsets like ISO-8859-1, PHL is UTF-8-only per` |
|      - | 5533 | ` * PLAN.md §6 — raises PHP's unsupported-charset warning and is treated as` |
|      - | 5534 | ` * UTF-8 (ph7_context_throw_error_format prepends the function name). */` |
|    141 | 5535 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx){` |
|      - | 5536 | `	const char *zCs;` |
|      - | 5537 | `	int nCs;` |
|    148 | 5538 | `	if( nArg <= idx \|\| ph7_value_is_null(apArg[idx]) ){ return; }` |
|     15 | 5539 | `	zCs = ph7_value_to_string(apArg[idx],&nCs);` |
|     15 | 5540 | `	if( nCs == 0 ){ return; } /* "" selects the default charset (UTF-8) */` |
|     13 | 5541 | `	if( nCs == 5 && SyStrnicmp(zCs,"UTF-8",5) == 0 ){` |
|     13 | 5542 | `		return; /* php accepts only "UTF-8" (any case) silently — "UTF8" warns */` |
|      - | 5543 | `	}` |
|    ! 0 | 5544 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5545 | `		"Charset \"%.*s\" is not supported, assuming UTF-8",nCs,zCs);` |
|     71 | 5546 | `}` |
|      - | 5547 | `/* get_html_translation_table() worker: character (UTF-8 bytes) => entity.` |
|      - | 5548 | ` * The five specials come first in byte order, then — for HTML_ENTITIES with a` |
|      - | 5549 | ` * named-table doctype — the 248 aHtml401Ent rows ascending (oracle-pinned` |
|      - | 5550 | ` * ordering; 253 entries under the defaults). */` |
|    549 | 5551 | `static void HtmlTableAdd(ph7_value *pArray,ph7_value *pValue,const char *zKey,const char *zEnt){` |
|    549 | 5552 | `	ph7_value_string(pValue,zEnt,-1);` |
|    549 | 5553 | `	ph7_array_add_strkey_elem(pArray,zKey,pValue);` |
|    549 | 5554 | `	ph7_value_reset_string_cursor(pValue);` |
|    549 | 5555 | `}` |
|     13 | 5556 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags){` |
|      - | 5557 | `	ph7_value *pArray,*pValue;` |
|     13 | 5558 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 5559 | `	sxu32 n;` |
|     13 | 5560 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 5561 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 5562 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|    ! 0 | 5563 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5564 | `		return;` |
|      - | 5565 | `	}` |
|     13 | 5566 | `	if( iFlags & PH7_ENT_QUOTE_DOUBLE ){` |
|     11 | 5567 | `		HtmlTableAdd(pArray,pValue,"\"","&quot;");` |
|      5 | 5568 | `	}` |
|     13 | 5569 | `	HtmlTableAdd(pArray,pValue,"&","&amp;");` |
|     13 | 5570 | `	if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|      - | 5571 | `		/* The apostrophe row mirrors the function each table belongs to:` |
|      - | 5572 | `		 * SPECIALCHARS follows htmlspecialchars, ENTITIES follows` |
|      - | 5573 | `		 * htmlentities (oracle-pinned at flags 35). */` |
|      7 | 5574 | `		HtmlTableAdd(pArray,pValue,"'",HtmlAposEntity(iDoc,iTable != 0));` |
|      3 | 5575 | `	}` |
|     13 | 5576 | `	HtmlTableAdd(pArray,pValue,"<","&lt;");` |
|     13 | 5577 | `	HtmlTableAdd(pArray,pValue,">","&gt;");` |
|     13 | 5578 | `	if( iTable != 0 /*php: any non-HTML_SPECIALCHARS table => entities*/ && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 5579 | `		char zKey[8];` |
|    499 | 5580 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|    497 | 5581 | `			int nK = HtmlCpUtf8(aHtml401Ent[n].cp,zKey);` |
|    497 | 5582 | `			zKey[nK] = 0;` |
|    497 | 5583 | `			HtmlTableAdd(pArray,pValue,zKey,aHtml401Ent[n].zEnt);` |
|    249 | 5584 | `		}` |
|      1 | 5585 | `	}` |
|     13 | 5586 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 5587 | `}` |
|     25 | 5588 | `static int FvEmailAllowed(unsigned char c){` |
|     25 | 5589 | `	if( (c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9') ){ return 1; }` |
|     16 | 5590 | `	return c=='!'\|\|c=='#'\|\|c=='$'\|\|c=='%'\|\|c=='&'\|\|c=='\''\|\|c=='*'\|\|c=='+'` |
|     10 | 5591 | ``	    \|\| c=='-'\|\|c=='='\|\|c=='?'\|\|c=='^'\|\|c=='_'\|\|c=='`'\|\|c=='{'\|\|c=='\|'`` |
|     15 | 5592 | `	    \|\| c=='}'\|\|c=='~'\|\|c=='@'\|\|c=='.'\|\|c=='['\|\|c==']';` |
|     13 | 5593 | `}` |
|     23 | 5594 | `static int FvUrlAllowed(unsigned char c){` |
|     23 | 5595 | `	return c>=33 && c<=126; /* PHP keeps every printable ASCII byte except space */` |
|      1 | 5596 | `}` |
|      - | 5597 | `/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */` |
|      5 | 5598 | `static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){` |
|      5 | 5599 | `	int i, runStart = 0;` |
|      5 | 5600 | `	ph7_result_string(pCtx,"",0);` |
|     51 | 5601 | `	for( i=0; i<n; i++ ){` |
|     47 | 5602 | `		unsigned char c = (unsigned char)z[i];` |
|     47 | 5603 | `		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){` |
|     11 | 5604 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     11 | 5605 | `			runStart = i+1;` |
|      5 | 5606 | `		}` |
|     24 | 5607 | `	}` |
|      5 | 5608 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      5 | 5609 | `}` |
|      - | 5610 | `/*` |
|      - | 5611 | ` * Apply the selected filter to one already-resolved input value and write the` |
|      - | 5612 | ` * result into pCtx. Shared by filter_var() and filter_input(): the caller has` |
|      - | 5613 | ` * already parsed $filter/$flags/$options. On validation failure the 'default'` |
|      - | 5614 | ` * option (if any) is returned, else null when FILTER_NULL_ON_FAILURE is set,` |
|      - | 5615 | ` * else false. A validating filter that passes returns the (string) input` |
|      - | 5616 | ` * unchanged; a sanitizer writes its transformed output directly.` |
|      - | 5617 | ` */` |
|    316 | 5618 | `static int FvApplyFilter(ph7_context *pCtx,ph7_value *pInput,` |
|      - | 5619 | `                         int iFilter,int iFlags,ph7_value *pOpts,` |
|      - | 5620 | `                         ph7_value *pDefault)` |
|      3 | 5621 | `{` |
|    319 | 5622 | `	int bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|      - | 5623 | `	const char *zVal; int nVal;` |
|      - | 5624 | `	/* An array/object input fails every scalar filter. */` |
|    319 | 5625 | `	if( ph7_value_is_array(pInput) ){ goto fail; }` |
|    317 | 5626 | `	zVal = ph7_value_to_string(pInput,&nVal);` |
|    317 | 5627 | `	switch( iFilter ){` |
|     28 | 5628 | `	case FV_VALIDATE_INT: {` |
|      - | 5629 | `		ph7_int64 v;` |
|     58 | 5630 | `		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }` |
|     31 | 5631 | `		if( pOpts ){` |
|      7 | 5632 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|      7 | 5633 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|      7 | 5634 | `			if( pMin && v<ph7_value_to_int64(pMin) ){ goto fail; }` |
|      7 | 5635 | `			if( pMax && v>ph7_value_to_int64(pMax) ){ goto fail; }` |
|      2 | 5636 | `		}` |
|     29 | 5637 | `		ph7_result_int64(pCtx,v);` |
|     29 | 5638 | `		return PH7_OK;` |
|      - | 5639 | `	}` |
|     34 | 5640 | `	case FV_VALIDATE_FLOAT: {` |
|      - | 5641 | `		double d;` |
|     69 | 5642 | `		if( !FvValidateFloat(zVal,nVal,iFlags,&d) ){ goto fail; }` |
|     39 | 5643 | `		ph7_result_double(pCtx,d);` |
|     39 | 5644 | `		return PH7_OK;` |
|      - | 5645 | `	}` |
|     14 | 5646 | `	case FV_VALIDATE_BOOLEAN: {` |
|      - | 5647 | `		int b;` |
|     29 | 5648 | `		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }` |
|     21 | 5649 | `		ph7_result_bool(pCtx,b);` |
|     21 | 5650 | `		return PH7_OK;` |
|      - | 5651 | `	}` |
|     25 | 5652 | `	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|     11 | 5653 | `	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;` |
|     28 | 5654 | `	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(zVal,nVal) ){ goto fail; }     goto pass;` |
|     11 | 5655 | `	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal) ){ goto fail; }    goto pass;` |
|     15 | 5656 | `	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal) ){ goto fail; }       goto pass;` |
|      3 | 5657 | `	case FV_VALIDATE_REGEXP: {` |
|      - | 5658 | `#ifdef PH7_ENABLE_PCRE` |
|      8 | 5659 | `		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;` |
|      8 | 5660 | `		const char *zRe; int nRe, matched = 0;` |
|      8 | 5661 | `		if( pRe==0 ){` |
|      3 | 5662 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5663 | `				"filter_var(): \"regexp\" option is missing");` |
|      - | 5664 | `		}` |
|      5 | 5665 | `		zRe = ph7_value_to_string(pRe,&nRe);` |
|      5 | 5666 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK \|\| !matched ){ goto fail; }` |
|      3 | 5667 | `		goto pass;` |
|      - | 5668 | `#else` |
|      - | 5669 | `		goto fail;` |
|      - | 5670 | `#endif` |
|      - | 5671 | `	}` |
|      3 | 5672 | `	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;` |
|      5 | 5673 | `	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;` |
|     13 | 5674 | `	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,iFlags); return PH7_OK;` |
|     25 | 5675 | `	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeFull(pCtx,zVal,nVal,iFlags);    return PH7_OK;` |
|      3 | 5676 | `	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 5677 | `	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;` |
|     13 | 5678 | `	case FV_DEFAULT:` |
|      - | 5679 | `		/* FILTER_UNSAFE_RAW / FILTER_DEFAULT: pass through unchanged unless a` |
|      - | 5680 | `		 * STRIP/ENCODE flag is set, in which case apply the string filter. */` |
|     28 | 5681 | `		if( iFlags & FV_FLAG_STRING_MASK ){` |
|     15 | 5682 | `			FvSanitizeString(pCtx,zVal,nVal,iFlags);` |
|     15 | 5683 | `			return PH7_OK;` |
|      - | 5684 | `		}` |
|     14 | 5685 | `		goto pass;` |
|    ! 0 | 5686 | `	default:` |
|    ! 0 | 5687 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 5688 | `			"Unknown filter with ID %d",iFilter);` |
|    ! 0 | 5689 | `		break; /* unknown filter id -> fail */` |
|    ! 0 | 5690 | `	}` |
|     58 | 5691 | `fail:` |
|    118 | 5692 | `	if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|    114 | 5693 | `	else if( bNull ){ ph7_result_null(pCtx); }` |
|    108 | 5694 | `	else { ph7_result_bool(pCtx,0); }` |
|    118 | 5695 | `	return PH7_OK;` |
|     26 | 5696 | `pass: /* validation passed: return the (string) input unchanged */` |
|     54 | 5697 | `	ph7_result_string(pCtx,zVal,nVal);` |
|     54 | 5698 | `	return PH7_OK;` |
|    161 | 5699 | `}` |
|      - | 5700 | `/*` |
|      - | 5701 | ` * Parse the ($filter, $options) pair shared by filter_var()/filter_input() out` |
|      - | 5702 | ` * of apArg[iBase] ($filter) and apArg[iBase+1] ($options): $options is either a` |
|      - | 5703 | ` * plain flags int, or an array with 'flags' and an 'options' sub-array (whose` |
|      - | 5704 | ` * 'default' entry is the fallback value). Fills the four output pointers;` |
|      - | 5705 | ` * unset outputs keep the caller-provided defaults.` |
|      - | 5706 | ` */` |
|    328 | 5707 | `static void FvParseFilterArgs(int nArg,ph7_value **apArg,int iBase,` |
|      - | 5708 | `                              int *piFilter,int *piFlags,` |
|      - | 5709 | `                              ph7_value **ppOpts,ph7_value **ppDefault)` |
|      3 | 5710 | `{` |
|    331 | 5711 | `	if( nArg>iBase ){ *piFilter = ph7_value_to_int(apArg[iBase]); }` |
|    331 | 5712 | `	if( nArg>iBase+1 ){` |
|     88 | 5713 | `		if( ph7_value_is_array(apArg[iBase+1]) ){` |
|     42 | 5714 | `			ph7_value *pF = ph7_array_fetch(apArg[iBase+1],"flags",(int)sizeof("flags")-1);` |
|     42 | 5715 | `			if( pF ){ *piFlags = ph7_value_to_int(pF); }` |
|     42 | 5716 | `			*ppOpts = ph7_array_fetch(apArg[iBase+1],"options",(int)sizeof("options")-1);` |
|     42 | 5717 | `			if( *ppOpts && !ph7_value_is_array(*ppOpts) ){ *ppOpts = 0; }` |
|     42 | 5718 | `			if( *ppOpts ){ *ppDefault = ph7_array_fetch(*ppOpts,"default",(int)sizeof("default")-1); }` |
|     22 | 5719 | `		}else{` |
|     48 | 5720 | `			*piFlags = ph7_value_to_int(apArg[iBase+1]);` |
|      - | 5721 | `		}` |
|     43 | 5722 | `	}` |
|    331 | 5723 | `}` |
|      - | 5724 | `/*` |
|      - | 5725 | ` * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 5726 | ` *  Validate or sanitize a value; see FvApplyFilter for the failure semantics.` |
|      - | 5727 | ` */` |
|    306 | 5728 | `static int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5729 | `{` |
|    308 | 5730 | `	int iFilter = FV_DEFAULT, iFlags = 0;` |
|    308 | 5731 | `	ph7_value *pOpts = 0, *pDefault = 0;` |
|    308 | 5732 | `	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    308 | 5733 | `	FvParseFilterArgs(nArg,apArg,1,&iFilter,&iFlags,&pOpts,&pDefault);` |
|    308 | 5734 | `	return FvApplyFilter(pCtx,apArg[0],iFilter,iFlags,pOpts,pDefault);` |
|    155 | 5735 | `}` |
|      - | 5736 | `/*` |
|      - | 5737 | ` * filter_input($type, $var_name, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 5738 | ` *  Look up $var_name in the requested INPUT_* superglobal, then apply the` |
|      - | 5739 | ` *  filter. Semantics verified byte-for-byte against php 8.5:` |
|      - | 5740 | ` *   - variable NOT set: 'default' option wins, else false when` |
|      - | 5741 | ` *     FILTER_NULL_ON_FAILURE is set, else null. (Note the null/false roles are` |
|      - | 5742 | ` *     INVERTED relative to a present value that fails validation, which yields` |
|      - | 5743 | ` *     default > null-if-NULL_ON_FAILURE > false via FvApplyFilter.)` |
|      - | 5744 | ` *   - variable present: delegate to FvApplyFilter.` |
|      - | 5745 | ` *  Divergence: php reads a SAPI snapshot of the original request variables` |
|      - | 5746 | ` *  captured at startup; PHL reads the live superglobal. In CLI they match for` |
|      - | 5747 | ` *  the SAPI-registered keys (SCRIPT_NAME/PHP_SELF/DOCUMENT_ROOT); keys added` |
|      - | 5748 | ` *  only to the live $_SERVER (REQUEST_TIME/PWD/…) are visible here but not in` |
|      - | 5749 | ` *  php's snapshot.` |
|      - | 5750 | ` */` |
|     28 | 5751 | `static int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5752 | `{` |
|     30 | 5753 | `	int iType, iFilter = FV_DEFAULT, iFlags = 0;` |
|     30 | 5754 | `	ph7_value *pOpts = 0, *pDefault = 0, *pSuper, *pElem;` |
|      - | 5755 | `	const char *zVar, *zSuper; int nVar; sxu32 nSuper;` |
|     30 | 5756 | `	if( nArg<2 ){` |
|      7 | 5757 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|      2 | 5758 | `			"filter_input() expects at least 2 arguments, %d given",nArg);` |
|      - | 5759 | `	}` |
|     26 | 5760 | `	iType = ph7_value_to_int(apArg[0]);` |
|     26 | 5761 | `	switch( iType ){` |
|      3 | 5762 | `	case 0: zSuper = "_POST";   nSuper = (sxu32)sizeof("_POST")-1;   break; /* INPUT_POST */` |
|      3 | 5763 | `	case 1: zSuper = "_GET";    nSuper = (sxu32)sizeof("_GET")-1;    break; /* INPUT_GET */` |
|    ! 0 | 5764 | `	case 2: zSuper = "_COOKIE"; nSuper = (sxu32)sizeof("_COOKIE")-1; break; /* INPUT_COOKIE */` |
|    ! 0 | 5765 | `	case 4: zSuper = "_ENV";    nSuper = (sxu32)sizeof("_ENV")-1;    break; /* INPUT_ENV */` |
|     19 | 5766 | `	case 5: zSuper = "_SERVER"; nSuper = (sxu32)sizeof("_SERVER")-1; break; /* INPUT_SERVER */` |
|      1 | 5767 | `	default:` |
|      3 | 5768 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5769 | `			"filter_input(): Argument #1 ($type) must be an INPUT_* constant");` |
|      - | 5770 | `	}` |
|     23 | 5771 | `	zVar = ph7_value_to_string(apArg[1],&nVar);` |
|     23 | 5772 | `	FvParseFilterArgs(nArg,apArg,2,&iFilter,&iFlags,&pOpts,&pDefault);` |
|      - | 5773 | `	/* Resolve the variable from the superglobal (missing/non-array -> not set). */` |
|     23 | 5774 | `	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);` |
|     23 | 5775 | `	pElem = (pSuper && ph7_value_is_array(pSuper))` |
|     33 | 5776 | `		? ph7_array_fetch(pSuper,zVar,nVar) : 0;` |
|     23 | 5777 | `	if( pElem==0 ){` |
|      - | 5778 | `		/* Variable not set: default > false(if NULL_ON_FAILURE) > null. Note the` |
|      - | 5779 | `		 * false/null roles are inverted vs FvApplyFilter's present-but-fails path. */` |
|     13 | 5780 | `		if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|      9 | 5781 | `		else if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_bool(pCtx,0); }` |
|      7 | 5782 | `		else { ph7_result_null(pCtx); }` |
|     13 | 5783 | `		return PH7_OK;` |
|      - | 5784 | `	}` |
|     11 | 5785 | `	return FvApplyFilter(pCtx,pElem,iFilter,iFlags,pOpts,pDefault);` |
|     16 | 5786 | `}` |
|      - | 5787 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 5788 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 5789 | `/*` |
|      - | 5790 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 5791 |  |
|      - | 5792 | ` */` |
|      4 | 5793 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 5794 | `	const char *zInput, /* Raw input */` |
|      - | 5795 | `	int nByte,  /* Input length */` |
|      - | 5796 | `	int delim,  /* Delimiter */` |
|      - | 5797 | `	int encl,   /* Enclosure */` |
|      - | 5798 | `	int escape,  /* Escape character */` |
|      - | 5799 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 5800 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 5801 | `	)` |
|      1 | 5802 | `{` |
|      5 | 5803 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 5804 | `	const char *zIn = zInput;` |
|      - | 5805 | `	const char *zPtr;` |
|      - | 5806 | `	int isEnc;` |
|      - | 5807 | `	/* Start processing */` |
|      8 | 5808 | `	for(;;){` |
|     17 | 5809 | `		if( zIn >= zEnd ){` |
|      - | 5810 | `			/* No more input to process */` |
|      5 | 5811 | `			break;` |
|      - | 5812 | `		}` |
|     13 | 5813 | `		isEnc = 0;` |
|     13 | 5814 | `		zPtr = zIn;` |
|      - | 5815 | `		/* Find the first delimiter */` |
|     27 | 5816 | `		while( zIn < zEnd ){` |
|     23 | 5817 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 5818 | `				/* Delimiter found,break imediately */` |
|      5 | 5819 | `				break;` |
|     15 | 5820 | `			}else if( zIn[0] == encl ){` |
|      - | 5821 | `				/* Inside enclosure? */` |
|    ! 0 | 5822 | `				isEnc = !isEnc;` |
|     15 | 5823 | `			}else if( zIn[0] == escape ){` |
|      - | 5824 | `				/* Escape sequence */` |
|    ! 0 | 5825 | `				zIn++;` |
|    ! 0 | 5826 | `			}` |
|      - | 5827 | `			/* Advance the cursor */` |
|     15 | 5828 | `			zIn++;` |
|      1 | 5829 | `		}` |
|     13 | 5830 | `		if( zIn > zPtr ){` |
|     13 | 5831 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 5832 | `			sxi32 rc;` |
|      - | 5833 | `			/* Invoke the supllied callback */` |
|     13 | 5834 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 5835 | `				zPtr++;` |
|    ! 0 | 5836 | `				nByteChunk-=2;` |
|    ! 0 | 5837 | `			}` |
|     13 | 5838 | `			if( nByteChunk > 0 ){` |
|     13 | 5839 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 5840 | `				if( rc == SXERR_ABORT ){` |
|      - | 5841 | `					/* User callback request an operation abort */` |
|    ! 0 | 5842 | `					break;` |
|      - | 5843 | `				}` |
|      6 | 5844 | `			}` |
|      6 | 5845 | `		}` |
|      - | 5846 | `		/* Ignore trailing delimiter */` |
|     21 | 5847 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 5848 | `			zIn++;` |
|      1 | 5849 | `		}` |
|      1 | 5850 | `	}` |
|      5 | 5851 | `	return SXRET_OK;` |
|      1 | 5852 | `}` |
|      - | 5853 | `/*` |
|      - | 5854 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 5855 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 5856 | ` * argument to this callback.` |
|      - | 5857 | ` */` |
|     12 | 5858 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 5859 | `{` |
|     13 | 5860 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 5861 | `	ph7_value sEntry;` |
|      - | 5862 | `	SyString sToken;` |
|      - | 5863 | `	/* Insert the token in the given array */` |
|     13 | 5864 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 5865 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 5866 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 5867 | `	if( sToken.nByte < 1){` |
|    ! 0 | 5868 | `		return SXRET_OK;` |
|      - | 5869 | `	}` |
|     13 | 5870 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 5871 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 5872 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 5873 | `	return SXRET_OK;` |
|      7 | 5874 | `}` |
|      - | 5875 | `/*` |
|      - | 5876 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 5877 | ` *  Parse a CSV string into an array.` |
|      - | 5878 | ` * Parameters` |
|      - | 5879 | ` *  $input` |
|      - | 5880 | ` *   The string to parse.` |
|      - | 5881 | ` *  $delimiter` |
|      - | 5882 | ` *   Set the field delimiter (one character only).` |
|      - | 5883 | ` *  $enclosure` |
|      - | 5884 | ` *   Set the field enclosure character (one character only).` |
|      - | 5885 | ` *  $escape` |
|      - | 5886 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 5887 | ` * Return` |
|      - | 5888 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 5889 | ` */` |
|      2 | 5890 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5891 | `{` |
|      - | 5892 | `	const char *zInput,*zPtr;` |
|      - | 5893 | `	ph7_value *pArray;` |
|      3 | 5894 | `	int delim  = ',';   /* Delimiter */` |
|      3 | 5895 | `	int encl   = '"' ;  /* Enclosure */` |
|      3 | 5896 | `	int escape = '\\';  /* Escape character */` |
|      - | 5897 | `	int nLen;` |
|      3 | 5898 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 5899 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 5900 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5901 | `		return PH7_OK;` |
|      - | 5902 | `	}` |
|      - | 5903 | `	/* Extract the raw input */` |
|      3 | 5904 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5905 | `	if( nArg > 1 ){` |
|      - | 5906 | `		int i;` |
|      3 | 5907 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 5908 | `			/* Extract the delimiter */` |
|      3 | 5909 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 5910 | `			if( i > 0 ){` |
|      3 | 5911 | `				delim = zPtr[0];` |
|      1 | 5912 | `			}` |
|      1 | 5913 | `		}` |
|      3 | 5914 | `		if( nArg > 2 ){` |
|      3 | 5915 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 5916 | `				/* Extract the enclosure */` |
|      3 | 5917 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 5918 | `				if( i > 0 ){` |
|      3 | 5919 | `					encl = zPtr[0];` |
|      1 | 5920 | `				}` |
|      1 | 5921 | `			}` |
|      3 | 5922 | `			if( nArg > 3 ){` |
|      3 | 5923 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 5924 | `					/* Extract the escape character */` |
|      3 | 5925 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 5926 | `					if( i > 0 ){` |
|      3 | 5927 | `						escape = zPtr[0];` |
|      1 | 5928 | `					}` |
|      1 | 5929 | `				}` |
|      1 | 5930 | `			}` |
|      1 | 5931 | `		}` |
|      1 | 5932 | `	}` |
|      - | 5933 | `	/* Create our array */` |
|      3 | 5934 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5935 | `	if( pArray == 0 ){` |
|      - | 5936 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|    ! 0 | 5937 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5938 | `	}` |
|      - | 5939 | `	/* Parse the raw input */` |
|      3 | 5940 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 5941 | `	/* Return the freshly created array */` |
|      3 | 5942 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5943 | `	return PH7_OK;` |
|      2 | 5944 | `}` |
|      - | 5945 | `/*` |
|      - | 5946 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 5947 | ` * container.` |
|      - | 5948 | ` * Refer to [strip_tags()].` |
|      - | 5949 | ` */` |
|     10 | 5950 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5951 | `{` |
|     11 | 5952 | `	const char *zEnd = &zTag[nByte];` |
|      - | 5953 | `	const char *zPtr;` |
|      - | 5954 | `	SyString sEntry;` |
|      - | 5955 | `	/* Strip tags */` |
|     10 | 5956 | `	for(;;){` |
|     45 | 5957 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 5958 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 5959 | `				zTag++;` |
|      1 | 5960 | `		}` |
|     21 | 5961 | `		if( zTag >= zEnd ){` |
|     11 | 5962 | `			break;` |
|      - | 5963 | `		}` |
|     11 | 5964 | `		zPtr = zTag;` |
|      - | 5965 | `		/* Delimit the tag */` |
|     25 | 5966 | `		while(zTag < zEnd ){` |
|     25 | 5967 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 5968 | `				/* UTF-8 stream */` |
|      3 | 5969 | `				zTag++;` |
|      5 | 5970 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 5971 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 5972 | `				break;` |
|    ! 0 | 5973 | `			}else{` |
|     13 | 5974 | `				zTag++;` |
|      - | 5975 | `			}` |
|      1 | 5976 | `		}` |
|     11 | 5977 | `		if( zTag > zPtr ){` |
|      - | 5978 | `			/* Perform the insertion */` |
|     11 | 5979 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 5980 | `			SyStringFullTrim(&sEntry);` |
|     11 | 5981 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 5982 | `		}` |
|      - | 5983 | `		/* Jump the trailing '>' */` |
|     11 | 5984 | `		zTag++;` |
|      1 | 5985 | `	}` |
|     11 | 5986 | `	return SXRET_OK;` |
|      1 | 5987 | `}` |
|      - | 5988 | `/*` |
|      - | 5989 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 5990 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 5991 | ` * Refer to [strip_tags()].` |
|      - | 5992 | ` */` |
|     36 | 5993 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 5994 | `{` |
|     37 | 5995 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 5996 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 5997 | `		SyString sTag;` |
|     85 | 5998 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 5999 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 6000 | `			zTag++;` |
|      1 | 6001 | `		}` |
|      - | 6002 | `		/* Delimit the tag */` |
|     25 | 6003 | `		zCur = zTag;` |
|     77 | 6004 | `		while(zTag < zEnd ){` |
|     77 | 6005 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 6006 | `				/* UTF-8 stream */` |
|      5 | 6007 | `				zTag++;` |
|      9 | 6008 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 6009 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 6010 | `				break;` |
|    ! 0 | 6011 | `			}else{` |
|     49 | 6012 | `				zTag++;` |
|      - | 6013 | `			}` |
|      1 | 6014 | `		}` |
|     25 | 6015 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 6016 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 6017 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 6018 | `		if( sTag.nByte > 0 ){` |
|      - | 6019 | `			SyString *aEntry,*pEntry;` |
|      - | 6020 | `			sxi32 rc;` |
|      - | 6021 | `			sxu32 n;` |
|      - | 6022 | `			/* Perform the lookup */` |
|     25 | 6023 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 6024 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 6025 | `				pEntry = &aEntry[n];` |
|      - | 6026 | `				/* Do the comparison */` |
|     25 | 6027 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 6028 | `				if( !rc ){` |
|     21 | 6029 | `					return SXRET_OK;` |
|      - | 6030 | `				}` |
|      3 | 6031 | `			}` |
|      2 | 6032 | `		}` |
|      2 | 6033 | `	}` |
|      - | 6034 | `	/* No such tag */` |
|     17 | 6035 | `	return SXERR_NOTFOUND;` |
|     19 | 6036 | `}` |
|      - | 6037 | `/*` |
|      - | 6038 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 6039 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 6040 | ` * Refer to [strip_tags()].` |
|      - | 6041 | ` */` |
|     16 | 6042 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 6043 | `{` |
|     17 | 6044 | `	const char *zEnd = &zIn[nByte];` |
|      - | 6045 | `	const char *zPtr,*zTag;` |
|      - | 6046 | `	SySet sSet;` |
|      - | 6047 | `	/* initialize the set of allowed tags */` |
|     17 | 6048 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 6049 | `	if( nTaglen > 0 ){` |
|      - | 6050 | `		/* Set of allowed tags */` |
|     11 | 6051 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 6052 | `	}` |
|      - | 6053 | `	/* Set the empty string */` |
|     17 | 6054 | `	ph7_result_string(pCtx,"",0);` |
|      - | 6055 | `	/* Start processing */` |
|     26 | 6056 | `	for(;;){` |
|     53 | 6057 | `		if(zIn >= zEnd){` |
|      - | 6058 | `			/* No more input to process */` |
|     15 | 6059 | `			break;` |
|      - | 6060 | `		}` |
|     39 | 6061 | `		zPtr = zIn;` |
|      - | 6062 | `		/* Find a tag */` |
|    133 | 6063 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 6064 | `			zIn++;` |
|      1 | 6065 | `		}` |
|     39 | 6066 | `		if( zIn > zPtr ){` |
|      - | 6067 | `			/* Consume raw input */` |
|     21 | 6068 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 6069 | `		}` |
|      - | 6070 | `		/* Ignore trailing null bytes */` |
|     39 | 6071 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 6072 | `			zIn++;` |
|    ! 0 | 6073 | `		}` |
|     39 | 6074 | `		if(zIn >= zEnd){` |
|      - | 6075 | `			/* No more input to process */` |
|      3 | 6076 | `			break;` |
|      - | 6077 | `		}` |
|     37 | 6078 | `		if( zIn[0] == '<' ){` |
|      - | 6079 | `			sxi32 rc;` |
|     37 | 6080 | `			zTag = zIn++;` |
|      - | 6081 | `			/* Delimit the tag */` |
|    127 | 6082 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 6083 | `				zIn++;` |
|      1 | 6084 | `			}` |
|     37 | 6085 | `			if( zIn < zEnd ){` |
|     37 | 6086 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 6087 | `			}` |
|      - | 6088 | `			/* Query the set */` |
|     37 | 6089 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 6090 | `			if( rc == SXRET_OK ){` |
|      - | 6091 | `				/* Keep the tag */` |
|     21 | 6092 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 6093 | `			}` |
|     18 | 6094 | `		}` |
|      1 | 6095 | `	}` |
|      - | 6096 | `	/* Cleanup */` |
|     17 | 6097 | `	SySetRelease(&sSet);` |
|     17 | 6098 | `	return SXRET_OK;` |
|      1 | 6099 | `}` |
|      - | 6100 | `/*` |
|      - | 6101 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 6102 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 6103 | ` * Parameters` |
|      - | 6104 | ` *  $str` |
|      - | 6105 | ` *  The input string.` |
|      - | 6106 | ` * $allowable_tags` |
|      - | 6107 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 6108 | ` * Return` |
|      - | 6109 | ` *  Returns the stripped string.` |
|      - | 6110 | ` */` |
|     14 | 6111 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6112 | `{` |
|     15 | 6113 | `	const char *zTaglist = 0;` |
|      - | 6114 | `	const char *zString;` |
|     15 | 6115 | `	int nTaglen = 0;` |
|      - | 6116 | `	int nLen;` |
|     15 | 6117 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6118 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 6119 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 6120 | `		return PH7_OK;` |
|      - | 6121 | `	}` |
|      - | 6122 | `	/* Point to the raw string */` |
|     15 | 6123 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 6124 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 6125 | `		/* Allowed tag */` |
|     11 | 6126 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 6127 | `	}` |
|      - | 6128 | `	/* Process input */` |
|     15 | 6129 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 6130 | `	return PH7_OK;` |
|      8 | 6131 | `}` |
|      - | 6132 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6133 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6134 | `/*` |
|      - | 6135 | ` * string str_shuffle(string $str)` |
|      - | 6136 |  |
|      - | 6137 | ` *  Randomly shuffles a string.` |
|      - | 6138 | ` * Parameters` |
|      - | 6139 | ` *  $str` |
|      - | 6140 | ` *   The input string.` |
|      - | 6141 | ` * Return` |
|      - | 6142 | ` *  Returns the shuffled string.` |
|      - | 6143 | ` */` |
|     10 | 6144 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6145 | `{` |
|      - | 6146 | `	const char *zString;` |
|      - | 6147 | `	int nLen,i,c;` |
|      - | 6148 | `	sxu32 iR;` |
|     11 | 6149 | `	if( nArg < 1 ){` |
|      - | 6150 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 6151 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 6152 | `		return PH7_OK;` |
|      - | 6153 | `	}` |
|      - | 6154 | `	/* Extract the target string */` |
|     11 | 6155 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 6156 | `	if( nLen < 1 ){` |
|      - | 6157 | `		/* Nothing to shuffle */` |
|      3 | 6158 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 6159 | `		return PH7_OK;` |
|      - | 6160 | `	}` |
|      - | 6161 | `	/* Shuffle the string */` |
|     43 | 6162 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 6163 | `		/* Generate a random number first */` |
|     35 | 6164 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 6165 | `		/* Extract a random offset */` |
|     35 | 6166 | `		c = zString[iR % nLen];` |
|      - | 6167 | `		/* Append it */` |
|     35 | 6168 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 6169 | `	}` |
|      9 | 6170 | `	return PH7_OK;` |
|      6 | 6171 | `}` |
|      - | 6172 | `/*` |
|      - | 6173 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 6174 | ` *  Convert a string to an array.` |
|      - | 6175 | ` * Parameters` |
|      - | 6176 | ` * $string` |
|      - | 6177 | ` *  The input string.` |
|      - | 6178 | ` * $split_length` |
|      - | 6179 | ` *  Maximum length of the chunk.` |
|      - | 6180 | ` * Return` |
|      - | 6181 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 6182 | ` *  except possibly the last one which may be shorter.` |
|      - | 6183 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 6184 | ` *  as the first (and only) array element.` |
|      - | 6185 | ` *  An empty string returns an empty array.` |
|      - | 6186 | ` * Errors` |
|      - | 6187 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 6188 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 6189 | ` *  ValueError if $split_length is less than 1.` |
|      - | 6190 | ` */` |
|     28 | 6191 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 6192 | `{` |
|      - | 6193 | `	const char *zString,*zEnd;` |
|      - | 6194 | `	ph7_value *pArray,*pValue;` |
|      - | 6195 | `	int split_len;` |
|      - | 6196 | `	int nLen;` |
|     33 | 6197 | `	if( nArg < 1 ){` |
|      4 | 6198 | `		return PH7_VmThrowException(pCtx,` |
|      - | 6199 | `			"ArgumentCountError",` |
|      - | 6200 | `			"str_split() expects at least 1 argument, %d given",` |
|      1 | 6201 | `			nArg` |
|      - | 6202 | `			);` |
|      - | 6203 | `	}` |
|      - | 6204 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     38 | 6205 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     40 | 6206 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 6207 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 6208 | `		return PH7_VmThrowException(pCtx,` |
|      - | 6209 | `			"TypeError",` |
|      - | 6210 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 6211 | `			ph7_type_name(apArg[0])` |
|      - | 6212 | `			);` |
|      - | 6213 | `	}` |
|      - | 6214 | `	/* Point to the target string */` |
|     27 | 6215 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 6216 | `	split_len = (int)sizeof(char);` |
|     27 | 6217 | `	if( nArg > 1 ){` |
|      - | 6218 | `		/* Split length */` |
|     17 | 6219 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 6220 | `		if( split_len < 1 ){` |
|      6 | 6221 | `			return PH7_VmThrowException(pCtx,` |
|      - | 6222 | `				"ValueError",` |
|      - | 6223 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 6224 | `				);` |
|      - | 6225 | `		}` |
|     11 | 6226 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 6227 | `			split_len = nLen;` |
|      1 | 6228 | `		}` |
|      5 | 6229 | `	}` |
|      - | 6230 | `	/* Create the array and the scalar value */` |
|     21 | 6231 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 6232 | `	/*Chunk value */` |
|     21 | 6233 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 6234 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 6235 | `		/* Return FALSE */` |
|    ! 0 | 6236 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6237 | `		return PH7_OK;` |
|      - | 6238 | `	}` |
|      - | 6239 | `	/* Point to the end of the string */` |
|     21 | 6240 | `	zEnd = &zString[nLen];` |
|      - | 6241 | `	/* Perform the requested operation */` |
|     48 | 6242 | `	for(;;){` |
|      - | 6243 | `		int nMax;` |
|     59 | 6244 | `		if( zString >= zEnd ){` |
|      - | 6245 | `			/* No more input to process */` |
|     21 | 6246 | `			break;` |
|      - | 6247 | `		}` |
|     39 | 6248 | `		nMax = (int)(zEnd-zString);` |
|     39 | 6249 | `		if( nMax < split_len ){` |
|      3 | 6250 | `			split_len = nMax;` |
|      1 | 6251 | `		}` |
|      - | 6252 | `		/* Copy the current chunk */` |
|     39 | 6253 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 6254 | `		/* Insert it */` |
|     39 | 6255 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 6256 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 6257 | `		}` |
|      - | 6258 | `		/* reset the string cursor */` |
|     39 | 6259 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 6260 | `		/* Update position */` |
|     39 | 6261 | `		zString += split_len;` |
|      1 | 6262 | `	}` |
|      - | 6263 | `	/*` |
|      - | 6264 | `	 * Return the array.` |
|      - | 6265 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 6266 | `	 * upon we return from this function.` |
|      - | 6267 | `	 */` |
|     21 | 6268 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 6269 | `	return PH7_OK;` |
|     19 | 6270 | `}` |
|      - | 6271 | `/*` |
|      - | 6272 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 6273 | ` * Refer to [strspn()].` |
|      - | 6274 | ` */` |
|     28 | 6275 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 6276 | `{` |
|     29 | 6277 | `	const char *zIn = *pzIn;` |
|      - | 6278 | `	const char *zPtr;` |
|      - | 6279 | `	/* Ignore leading white spaces */` |
|     29 | 6280 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 6281 | `		zIn++;` |
|    ! 0 | 6282 | `	}` |
|     29 | 6283 | `	if( zIn >= zEnd ){` |
|      - | 6284 | `		/* End of input */` |
|    ! 0 | 6285 | `		return SXERR_EOF;` |
|      - | 6286 | `	}` |
|     29 | 6287 | `	zPtr = zIn;` |
|      - | 6288 | `	/* Extract the token */` |
|    201 | 6289 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 6290 | `		zIn++;` |
|      1 | 6291 | `	}` |
|     29 | 6292 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 6293 | `	/* Synchronize pointers */` |
|     29 | 6294 | `	*pzIn = zIn;` |
|      - | 6295 | `	/* Return to the caller */` |
|     29 | 6296 | `	return SXRET_OK;` |
|     15 | 6297 | `}` |
|      - | 6298 | `/*` |
|      - | 6299 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 6300 | ` * return the longest match.` |
|      - | 6301 | ` * Refer to [strspn()].` |
|      - | 6302 | ` */` |
|     18 | 6303 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 6304 | `{` |
|     19 | 6305 | `	const char *zEnd = &zString[nLen];` |
|     19 | 6306 | `	const char *zIn = zString;` |
|      - | 6307 | `	int i,c;` |
|     45 | 6308 | `	for(;;){` |
|     91 | 6309 | `		if( zString >= zEnd ){` |
|      7 | 6310 | `			break;` |
|      - | 6311 | `		}` |
|      - | 6312 | `		/* Extract current character */` |
|     85 | 6313 | `		c = zString[0];` |
|      - | 6314 | `		/* Perform the lookup */` |
|    383 | 6315 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 6316 | `			if( c == zMask[i] ){` |
|      - | 6317 | `				/* Character found */` |
|     73 | 6318 | `				break;` |
|      - | 6319 | `			}` |
|    150 | 6320 | `		}` |
|     85 | 6321 | `		if( i >= nMaskLen ){` |
|      - | 6322 | `			/* Character not in the current mask,break immediately */` |
|     13 | 6323 | `			break;` |
|      - | 6324 | `		}` |
|      - | 6325 | `		/* Advance cursor */` |
|     73 | 6326 | `		zString++;` |
|      1 | 6327 | `	}` |
|      - | 6328 | `	/* Longest match */` |
|     19 | 6329 | `	return (int)(zString-zIn);` |
|      1 | 6330 | `}` |
|      - | 6331 | `/*` |
|      - | 6332 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 6333 | ` * Refer to [strcspn()].` |
|      - | 6334 | ` */` |
|     10 | 6335 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 6336 | `{` |
|     11 | 6337 | `	const char *zEnd = &zString[nLen];` |
|     11 | 6338 | `	const char *zIn = zString;` |
|      - | 6339 | `	int i,c;` |
|     12 | 6340 | `	for(;;){` |
|     25 | 6341 | `		if( zString >= zEnd ){` |
|      3 | 6342 | `			break;` |
|      - | 6343 | `		}` |
|      - | 6344 | `		/* Extract current character */` |
|     23 | 6345 | `		c = zString[0];` |
|      - | 6346 | `		/* Perform the lookup */` |
|     51 | 6347 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 6348 | `			if( c == zMask[i] ){` |
|      9 | 6349 | `				break;` |
|      - | 6350 | `			}` |
|     15 | 6351 | `		}` |
|     23 | 6352 | `		if( i < nMaskLen ){` |
|      - | 6353 | `			/* Character in the current mask,break immediately */` |
|      9 | 6354 | `			break;` |
|      - | 6355 | `		}` |
|      - | 6356 | `		/* Advance cursor */` |
|     15 | 6357 | `		zString++;` |
|      1 | 6358 | `	}` |
|      - | 6359 | `	/* Longest match */` |
|     11 | 6360 | `	return (int)(zString-zIn);` |
|      1 | 6361 | `}` |
|      - | 6362 | `/*` |
|      - | 6363 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 6364 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 6365 | ` *  of characters contained within a given mask.` |
|      - | 6366 | ` * Parameters` |
|      - | 6367 | ` * $str` |
|      - | 6368 | ` *  The input string.` |
|      - | 6369 | ` * $mask` |
|      - | 6370 | ` *  The list of allowable characters.` |
|      - | 6371 | ` * $start` |
|      - | 6372 | ` *  The position in subject to start searching.` |
|      - | 6373 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 6374 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 6375 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 6376 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 6377 | ` *  start'th position from the end of subject.` |
|      - | 6378 | ` * $length` |
|      - | 6379 | ` *  The length of the segment from subject to examine.` |
|      - | 6380 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 6381 | ` *  characters after the starting position.` |
|      - | 6382 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 6383 | ` *  position up to length characters from the end of subject.` |
|      - | 6384 | ` * Return` |
|      - | 6385 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 6386 | ` * in mask.` |
|      - | 6387 | ` */` |
|     24 | 6388 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6389 | `{` |
|      - | 6390 | `	const char *zString,*zMask,*zEnd;` |
|      - | 6391 | `	int iMasklen,iLen;` |
|      - | 6392 | `	SyString sToken;` |
|     25 | 6393 | `	int iCount = 0;` |
|      - | 6394 | `	int rc;` |
|     25 | 6395 | `	if( nArg < 2 ){` |
|      - | 6396 | `		/* Missing agruments,return zero */` |
|    ! 0 | 6397 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 6398 | `		return PH7_OK;` |
|      - | 6399 | `	}` |
|      - | 6400 | `	/* Extract the target string */` |
|     25 | 6401 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6402 | `	/* Extract the mask */` |
|     25 | 6403 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 6404 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 6405 | `		/* Nothing to process,return zero */` |
|      7 | 6406 | `		ph7_result_int(pCtx,0);` |
|      7 | 6407 | `		return PH7_OK;` |
|      - | 6408 | `	}` |
|     19 | 6409 | `	if( nArg > 2 ){` |
|      - | 6410 | `		int nOfft;` |
|      - | 6411 | `		/* Extract the offset */` |
|      9 | 6412 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 6413 | `		if( nOfft < 0 ){` |
|    ! 0 | 6414 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 6415 | `			if( zBase > zString ){` |
|    ! 0 | 6416 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 6417 | `				zString = zBase;` |
|    ! 0 | 6418 | `			}else{` |
|      - | 6419 | `				/* Invalid offset */` |
|    ! 0 | 6420 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 6421 | `				return PH7_OK;` |
|      - | 6422 | `			}` |
|    ! 0 | 6423 | `		}else{` |
|      9 | 6424 | `			if( nOfft >= iLen ){` |
|      - | 6425 | `				/* Invalid offset */` |
|    ! 0 | 6426 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 6427 | `				return PH7_OK;` |
|    ! 0 | 6428 | `			}else{` |
|      - | 6429 | `				/* Update offset */` |
|      9 | 6430 | `				zString += nOfft;` |
|      9 | 6431 | `				iLen -= nOfft;` |
|      - | 6432 | `			}` |
|      - | 6433 | `		}` |
|      9 | 6434 | `		if( nArg > 3 ){` |
|      - | 6435 | `			int iUserlen;` |
|      - | 6436 | `			/* Extract the desired length */` |
|      9 | 6437 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 6438 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 6439 | `				iLen = iUserlen;` |
|      2 | 6440 | `			}` |
|      4 | 6441 | `		}` |
|      4 | 6442 | `	}` |
|      - | 6443 | `	/* Point to the end of the string */` |
|     19 | 6444 | `	zEnd = &zString[iLen];` |
|      - | 6445 | `	/* Extract the first non-space token */` |
|     19 | 6446 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 6447 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 6448 | `		/* Compare against the current mask */` |
|     19 | 6449 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 6450 | `	}` |
|      - | 6451 | `	/* Longest match */` |
|     19 | 6452 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 6453 | `	return PH7_OK;` |
|     13 | 6454 | `}` |
|      - | 6455 | `/*` |
|      - | 6456 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 6457 | ` *  Find length of initial segment not matching mask.` |
|      - | 6458 | ` * Parameters` |
|      - | 6459 | ` * $str` |
|      - | 6460 | ` *  The input string.` |
|      - | 6461 | ` * $mask` |
|      - | 6462 | ` *  The list of not allowed characters.` |
|      - | 6463 | ` * $start` |
|      - | 6464 | ` *  The position in subject to start searching.` |
|      - | 6465 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 6466 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 6467 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 6468 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 6469 | ` *  start'th position from the end of subject.` |
|      - | 6470 | ` * $length` |
|      - | 6471 | ` *  The length of the segment from subject to examine.` |
|      - | 6472 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 6473 | ` *  characters after the starting position.` |
|      - | 6474 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 6475 | ` *  position up to length characters from the end of subject.` |
|      - | 6476 | ` * Return` |
|      - | 6477 | ` *  Returns the length of the segment as an integer.` |
|      - | 6478 | ` */` |
|     14 | 6479 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6480 | `{` |
|      - | 6481 | `	const char *zString,*zMask,*zEnd;` |
|      - | 6482 | `	int iMasklen,iLen;` |
|      - | 6483 | `	SyString sToken;` |
|     15 | 6484 | `	int iCount = 0;` |
|      - | 6485 | `	int rc;` |
|     15 | 6486 | `	if( nArg < 2 ){` |
|      - | 6487 | `		/* Missing agruments,return zero */` |
|    ! 0 | 6488 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 6489 | `		return PH7_OK;` |
|      - | 6490 | `	}` |
|      - | 6491 | `	/* Extract the target string */` |
|     15 | 6492 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6493 | `	/* Extract the mask */` |
|     15 | 6494 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 6495 | `	if( iLen < 1 ){` |
|      - | 6496 | `		/* Nothing to process,return zero */` |
|    ! 0 | 6497 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 6498 | `		return PH7_OK;` |
|      - | 6499 | `	}` |
|     15 | 6500 | `	if( iMasklen < 1 ){` |
|      - | 6501 | `		/* No given mask,return the string length */` |
|      3 | 6502 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 6503 | `		return PH7_OK;` |
|      - | 6504 | `	}` |
|     13 | 6505 | `	if( nArg > 2 ){` |
|      - | 6506 | `		int nOfft;` |
|      - | 6507 | `		/* Extract the offset */` |
|     11 | 6508 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 6509 | `		if( nOfft < 0 ){` |
|    ! 0 | 6510 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 6511 | `			if( zBase > zString ){` |
|    ! 0 | 6512 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 6513 | `				zString = zBase;` |
|    ! 0 | 6514 | `			}else{` |
|      - | 6515 | `				/* Invalid offset */` |
|    ! 0 | 6516 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 6517 | `				return PH7_OK;` |
|      - | 6518 | `			}` |
|    ! 0 | 6519 | `		}else{` |
|     11 | 6520 | `			if( nOfft >= iLen ){` |
|      - | 6521 | `				/* Invalid offset */` |
|      3 | 6522 | `				ph7_result_int(pCtx,0);` |
|      3 | 6523 | `				return PH7_OK;` |
|    ! 0 | 6524 | `			}else{` |
|      - | 6525 | `				/* Update offset */` |
|      9 | 6526 | `				zString += nOfft;` |
|      9 | 6527 | `				iLen -= nOfft;` |
|      - | 6528 | `			}` |
|      - | 6529 | `		}` |
|      9 | 6530 | `		if( nArg > 3 ){` |
|      - | 6531 | `			int iUserlen;` |
|      - | 6532 | `			/* Extract the desired length */` |
|    ! 0 | 6533 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 6534 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 6535 | `				iLen = iUserlen;` |
|    ! 0 | 6536 | `			}` |
|    ! 0 | 6537 | `		}` |
|      4 | 6538 | `	}` |
|      - | 6539 | `	/* Point to the end of the string */` |
|     11 | 6540 | `	zEnd = &zString[iLen];` |
|      - | 6541 | `	/* Extract the first non-space token */` |
|     11 | 6542 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 6543 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 6544 | `		/* Compare against the current mask */` |
|     11 | 6545 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 6546 | `	}` |
|      - | 6547 | `	/* Longest match */` |
|     11 | 6548 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 6549 | `	return PH7_OK;` |
|      8 | 6550 | `}` |
|      - | 6551 | `/*` |
|      - | 6552 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 6553 | ` *  Search a string for any of a set of characters.` |
|      - | 6554 | ` * Parameters` |
|      - | 6555 | ` *  $haystack` |
|      - | 6556 | ` *   The string where char_list is looked for.` |
|      - | 6557 | ` *  $char_list` |
|      - | 6558 | ` *   This parameter is case sensitive.` |
|      - | 6559 | ` * Return` |
|      - | 6560 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 6561 | ` */` |
|      4 | 6562 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6563 | `{` |
|      - | 6564 | `	const char *zString,*zList,*zEnd;` |
|      - | 6565 | `	int iLen,iListLen,i,c;` |
|      - | 6566 | `	sxu32 nOfft,nMax;` |
|      - | 6567 | `	sxi32 rc;` |
|      5 | 6568 | `	if( nArg < 2 ){` |
|      - | 6569 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 6570 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6571 | `		return PH7_OK;` |
|      - | 6572 | `	}` |
|      - | 6573 | `	/* Extract the haystack and the char list */` |
|      5 | 6574 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 6575 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 6576 | `	if( iLen < 1 ){` |
|      - | 6577 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 6578 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 6579 | `		return PH7_OK;` |
|      - | 6580 | `	}` |
|      - | 6581 | `	/* Point to the end of the string */` |
|      5 | 6582 | `	zEnd = &zString[iLen];` |
|      5 | 6583 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 6584 | `	/* perform the requested operation */` |
|     15 | 6585 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 6586 | `		c = zList[i];` |
|     11 | 6587 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 6588 | `		if( rc == SXRET_OK ){` |
|      5 | 6589 | `			if( nMax < nOfft ){` |
|      3 | 6590 | `				nOfft = nMax;` |
|      1 | 6591 | `			}` |
|      2 | 6592 | `		}` |
|      6 | 6593 | `	}` |
|      5 | 6594 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 6595 | `		/* No such substring,return FALSE */` |
|      3 | 6596 | `		ph7_result_bool(pCtx,0);` |
|      2 | 6597 | `	}else{` |
|      - | 6598 | `		/* Return the substring */` |
|      3 | 6599 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 6600 | `	}` |
|      5 | 6601 | `	return PH7_OK;` |
|      3 | 6602 | `}` |
|      - | 6603 | `/* SPDX-SnippetBegin */` |
|      - | 6604 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 6605 | `/* SPDX-License-Identifier: blessing */` |
|      - | 6606 | `/*` |
|      - | 6607 | ` * string soundex(string $str)` |
|      - | 6608 | ` *  Calculate the soundex key of a string.` |
|      - | 6609 | ` * Parameters` |
|      - | 6610 | ` *  $str` |
|      - | 6611 | ` *   The input string.` |
|      - | 6612 | ` * Return` |
|      - | 6613 | ` *  Returns the soundex key as a string.` |
|      - | 6614 | ` * Note:` |
|      - | 6615 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 6616 | ` * source tree.` |
|      - | 6617 | ` */` |
|     22 | 6618 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6619 | `{` |
|      - | 6620 | `	const unsigned char *zIn;` |
|      - | 6621 | `	char zResult[8];` |
|      - | 6622 | `	int i, j;` |
|      - | 6623 | `	static const unsigned char iCode[] = {` |
|      - | 6624 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 6625 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 6626 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 6627 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 6628 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 6629 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 6630 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 6631 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 6632 | `	};` |
|     23 | 6633 | `	if( nArg < 1 ){` |
|      - | 6634 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 6635 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 6636 | `		return PH7_OK;` |
|      - | 6637 | `	}` |
|     23 | 6638 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     35 | 6639 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     23 | 6640 | `	if( zIn[i] ){` |
|     17 | 6641 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 6642 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 6643 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 6644 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 6645 | `			if( code>0 ){` |
|     45 | 6646 | `				if( code!=prevcode ){` |
|     33 | 6647 | `					prevcode = (unsigned char)code;` |
|     33 | 6648 | `					zResult[j++] = (char)code + '0';` |
|     16 | 6649 | `				}` |
|     23 | 6650 | `			}else{` |
|     49 | 6651 | `				prevcode = 0;` |
|      - | 6652 | `			}` |
|     47 | 6653 | `		}` |
|     33 | 6654 | `		while( j<4 ){` |
|     17 | 6655 | `			zResult[j++] = '0';` |
|      1 | 6656 | `		}` |
|     17 | 6657 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 6658 | `	}else{` |
|      - | 6659 | `	  /* No alphabetic character: PHP returns "0000" (not the SQLite "?000"). */` |
|      7 | 6660 | `	  ph7_result_string(pCtx,"0000",4);` |
|      - | 6661 | `	}` |
|     23 | 6662 | `	return PH7_OK;` |
|     12 | 6663 | `}` |
|      - | 6664 | `/* SPDX-SnippetEnd */` |
|      - | 6665 | `/*` |
|      - | 6666 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 6667 | ` *  Wraps a string to a given number of characters.` |
|      - | 6668 | ` * Parameters` |
|      - | 6669 | ` *  $str` |
|      - | 6670 | ` *   The input string.` |
|      - | 6671 | ` * $width` |
|      - | 6672 | ` *  The column width.` |
|      - | 6673 | ` * $break` |
|      - | 6674 | ` *  The line is broken using the optional break parameter.` |
|      - | 6675 | ` * Return` |
|      - | 6676 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 6677 | ` */` |
|     26 | 6678 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6679 | `{` |
|      - | 6680 | `	const char *zIn,*zBreak;` |
|      - | 6681 | `	SyBlob sWorker;` |
|      - | 6682 | `	int iLen,iBreaklen,iWidth,iCut,iStart,iSpace,iCur;` |
|      - | 6683 | `	sxi32 rc;` |
|     27 | 6684 | `	if( nArg < 1 ){` |
|      - | 6685 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 6686 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 6687 | `		return PH7_OK;` |
|      - | 6688 | `	}` |
|      - | 6689 | `	/* Extract the input string */` |
|     27 | 6690 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6691 | `	/* Width (default 75; PHP allows 0/negative — break at every space). */` |
|     27 | 6692 | `	iWidth = 75;` |
|     27 | 6693 | `	if( nArg > 1 ){` |
|     27 | 6694 | `		iWidth = ph7_value_to_int(apArg[1]);` |
|     13 | 6695 | `	}` |
|      - | 6696 | `	/* Break string (default "\n"). */` |
|     27 | 6697 | `	zBreak = "\n";` |
|     27 | 6698 | `	iBreaklen = (int)sizeof(char);` |
|     27 | 6699 | `	if( nArg > 2 ){` |
|     13 | 6700 | `		zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      6 | 6701 | `	}` |
|      - | 6702 | `	/* Cut long words? (default false). */` |
|     27 | 6703 | `	iCut = 0;` |
|     27 | 6704 | `	if( nArg > 3 ){` |
|      7 | 6705 | `		iCut = ph7_value_to_bool(apArg[3]);` |
|      3 | 6706 | `	}` |
|     27 | 6707 | `	if( iLen < 1 ){` |
|      - | 6708 | `		/* PHP returns the empty string for empty input before validating the other args. */` |
|      5 | 6709 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 6710 | `		return PH7_OK;` |
|      - | 6711 | `	}` |
|      - | 6712 | `	/* PHP 8 domain errors (catchable ValueError). */` |
|     23 | 6713 | `	if( iBreaklen < 1 ){` |
|      3 | 6714 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6715 | `			"wordwrap(): Argument #3 ($break) must not be empty");` |
|      - | 6716 | `	}` |
|     21 | 6717 | `	if( iWidth == 0 && iCut ){` |
|      3 | 6718 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6719 | `			"wordwrap(): Argument #4 ($cut_long_words) cannot be true when argument #2 ($width) is 0");` |
|      - | 6720 | `	}` |
|      - | 6721 | `	/*` |
|      - | 6722 | `	 * PHP's algorithm: a single left-to-right pass tracking the start of the` |
|      - | 6723 | `	 * current line (iStart) and the position of the last space seen on it` |
|      - | 6724 | `	 * (iSpace). A break is emitted when the line reaches the width, at the last` |
|      - | 6725 | `	 * space if there was one, otherwise (only when cut is enabled) hard at the` |
|      - | 6726 | `	 * boundary. An existing break sequence in the input resets the line.` |
|      - | 6727 | `	 */` |
|     19 | 6728 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     19 | 6729 | `	iStart = iSpace = iCur = 0;` |
|     19 | 6730 | `	rc = SXRET_OK;` |
|    551 | 6731 | `	while( iCur < iLen ){` |
|    533 | 6732 | `		if( iBreaklen <= iLen - iCur && SyMemcmp(&zIn[iCur],zBreak,(sxu32)iBreaklen) == 0 ){` |
|      - | 6733 | `			/* Existing break sequence in the input: copy it verbatim and reset the line. */` |
|    ! 0 | 6734 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart + iBreaklen));` |
|    ! 0 | 6735 | `			if( rc != SXRET_OK ){ goto oom; }` |
|    ! 0 | 6736 | `			iCur += iBreaklen;` |
|    ! 0 | 6737 | `			iStart = iSpace = iCur;` |
|    ! 0 | 6738 | `			continue;` |
|    533 | 6739 | `		}else if( zIn[iCur] == ' ' ){` |
|     67 | 6740 | `			if( iCur - iStart >= iWidth ){` |
|      - | 6741 | `				/* The line already fills the width at this space: break here (the space is consumed). */` |
|     13 | 6742 | `				rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     13 | 6743 | `				if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     13 | 6744 | `				if( rc != SXRET_OK ){ goto oom; }` |
|     13 | 6745 | `				iStart = iCur + 1;` |
|      6 | 6746 | `			}` |
|     67 | 6747 | `			iSpace = iCur;` |
|    500 | 6748 | `		}else if( iCut && iCur - iStart >= iWidth && iStart >= iSpace ){` |
|      - | 6749 | `			/* A word longer than the width with no space to break at: hard-cut at the boundary. */` |
|      7 | 6750 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|      7 | 6751 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|      7 | 6752 | `			if( rc != SXRET_OK ){ goto oom; }` |
|      7 | 6753 | `			iStart = iSpace = iCur;` |
|    464 | 6754 | `		}else if( iCur - iStart >= iWidth && iStart < iSpace ){` |
|      - | 6755 | `			/* Past the width mid-word: wrap back to the last space (which is consumed). */` |
|     17 | 6756 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iSpace - iStart));` |
|     17 | 6757 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     17 | 6758 | `			if( rc != SXRET_OK ){ goto oom; }` |
|     17 | 6759 | `			iStart = iSpace = iSpace + 1;` |
|      8 | 6760 | `		}` |
|    533 | 6761 | `		iCur++;` |
|      1 | 6762 | `	}` |
|      - | 6763 | `	/* Emit the trailing chunk. */` |
|     19 | 6764 | `	if( iStart < iCur ){` |
|     19 | 6765 | `		rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     19 | 6766 | `		if( rc != SXRET_OK ){ goto oom; }` |
|      9 | 6767 | `	}` |
|     19 | 6768 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|     19 | 6769 | `	SyBlobRelease(&sWorker);` |
|     19 | 6770 | `	return PH7_OK;` |
|    ! 0 | 6771 | `oom:` |
|    ! 0 | 6772 | `	SyBlobRelease(&sWorker);` |
|    ! 0 | 6773 | `	return PH7_ContextMemoryError(pCtx);` |
|     14 | 6774 | `}` |
|      - | 6775 | `/*` |
|      - | 6776 | ` * Check if the given character is a member of the given mask.` |
|      - | 6777 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 6778 | ` * Refer to [strtok()].` |
|      - | 6779 | ` */` |
|     30 | 6780 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 6781 | `{` |
|      - | 6782 | `	int i;` |
|     57 | 6783 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 6784 | `		if( c == zMask[i] ){` |
|     13 | 6785 | `			if( pOfft ){` |
|      5 | 6786 | `				*pOfft = i;` |
|      2 | 6787 | `			}` |
|     13 | 6788 | `			return TRUE;` |
|      - | 6789 | `		}` |
|     14 | 6790 | `	}` |
|     19 | 6791 | `	return FALSE;` |
|     16 | 6792 | `}` |
|      - | 6793 | `/*` |
|      - | 6794 | ` * Extract a single token from the input stream.` |
|      - | 6795 | ` * Refer to [strtok()].` |
|      - | 6796 | ` */` |
|      6 | 6797 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 6798 | `{` |
|      7 | 6799 | `	const char *zIn = *pzIn;` |
|      - | 6800 | `	const char *zPtr;` |
|      - | 6801 | `	/* Ignore leading delimiter */` |
|     11 | 6802 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 6803 | `		zIn++;` |
|      1 | 6804 | `	}` |
|      7 | 6805 | `	if( zIn >= zEnd ){` |
|      - | 6806 | `		/* End of input */` |
|    ! 0 | 6807 | `		return SXERR_EOF;` |
|      - | 6808 | `	}` |
|      7 | 6809 | `	zPtr = zIn;` |
|      - | 6810 | `	/* Extract the token */` |
|     13 | 6811 | `	while( zIn < zEnd ){` |
|     11 | 6812 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 6813 | `			/* UTF-8 stream */` |
|    ! 0 | 6814 | `			zIn++;` |
|    ! 0 | 6815 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 6816 | `		}else{` |
|     11 | 6817 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 6818 | `				break;` |
|      - | 6819 | `			}` |
|      7 | 6820 | `			zIn++;` |
|      - | 6821 | `		}` |
|      1 | 6822 | `	}` |
|      7 | 6823 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 6824 | `	/* Update the cursor */` |
|      7 | 6825 | `	*pzIn = zIn;` |
|      - | 6826 | `	/* Return to the caller */` |
|      7 | 6827 | `	return SXRET_OK;` |
|      4 | 6828 | `}` |
|      - | 6829 | `/* strtok auxiliary private data */` |
|      - | 6830 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 6831 | `struct strtok_aux_data` |
|      - | 6832 | `{` |
|      - | 6833 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 6834 | `	const char *zIn;   /* Current input stream */` |
|      - | 6835 | `	const char *zEnd;  /* End of input */` |
|      - | 6836 | `};` |
|      - | 6837 | `/*` |
|      - | 6838 | ` * string strtok(string $str,string $token)` |
|      - | 6839 | ` * string strtok(string $token)` |
|      - | 6840 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 6841 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 6842 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 6843 | ` *  words by using the space character as the token.` |
|      - | 6844 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 6845 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 6846 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 6847 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 6848 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 6849 | ` *  the argument are found.` |
|      - | 6850 | ` * Parameters` |
|      - | 6851 | ` *  $str` |
|      - | 6852 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 6853 | ` * $token` |
|      - | 6854 | ` *  The delimiter used when splitting up str.` |
|      - | 6855 | ` * Return` |
|      - | 6856 | ` *   Current token or FALSE on EOF.` |
|      - | 6857 | ` */` |
|      6 | 6858 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6859 | `{` |
|      - | 6860 | `	strtok_aux_data *pAux;` |
|      - | 6861 | `	const char *zMask;` |
|      - | 6862 | `	SyString sToken;` |
|      - | 6863 | `	int nMasklen;` |
|      - | 6864 | `	sxi32 rc;` |
|      7 | 6865 | `	if( nArg < 2 ){` |
|      - | 6866 | `		/* Extract top aux data */` |
|      5 | 6867 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      5 | 6868 | `		if( pAux == 0 ){` |
|      - | 6869 | `			/* No aux data,return FALSE */` |
|    ! 0 | 6870 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6871 | `			return PH7_OK;` |
|      - | 6872 | `		}` |
|      5 | 6873 | `		nMasklen = 0;` |
|      5 | 6874 | `		zMask = ""; /* cc warning */` |
|      5 | 6875 | `		if( nArg > 0 ){` |
|      - | 6876 | `			/* Extract the mask */` |
|      5 | 6877 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 6878 | `		}` |
|      5 | 6879 | `		if( nMasklen < 1 ){` |
|      - | 6880 | `			/* Invalid mask,return FALSE */` |
|    ! 0 | 6881 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 6882 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6883 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 6884 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6885 | `			return PH7_OK;` |
|      - | 6886 | `		}` |
|      - | 6887 | `		/* Extract the token */` |
|      5 | 6888 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 6889 | `		if( rc != SXRET_OK ){` |
|      - | 6890 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 6891 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 6892 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6893 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 6894 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6895 | `		}else{` |
|      - | 6896 | `			/* Return the extracted token */` |
|      5 | 6897 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6898 | `		}` |
|      3 | 6899 | `	}else{` |
|      - | 6900 | `		const char *zInput,*zCur;` |
|      - | 6901 | `		char *zDup;` |
|      - | 6902 | `		int nLen;` |
|      - | 6903 | `		/* Extract the raw input */` |
|      3 | 6904 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 6905 | `		if( nLen < 1 ){` |
|      - | 6906 | `			/* Empty input,return FALSE */` |
|    ! 0 | 6907 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6908 | `			return PH7_OK;` |
|      - | 6909 | `		}` |
|      - | 6910 | `		/* Extract the mask */` |
|      3 | 6911 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 6912 | `		if( nMasklen < 1 ){` |
|      - | 6913 | `			/* Set a default mask */` |
|      - | 6914 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 6915 | `			zMask = TOK_MASK;` |
|    ! 0 | 6916 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 6917 | `#undef TOK_MASK` |
|    ! 0 | 6918 | `		}` |
|      - | 6919 | `		/* Extract a single token */` |
|      3 | 6920 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 6921 | `		if( rc != SXRET_OK ){` |
|      - | 6922 | `			/* Empty input */` |
|    ! 0 | 6923 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 6924 | `			return PH7_OK;` |
|    ! 0 | 6925 | `		}else{` |
|      - | 6926 | `			/* Return the extracted token */` |
|      3 | 6927 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 6928 | `		}` |
|      - | 6929 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 6930 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 6931 | `		if( pAux ){` |
|      3 | 6932 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 6933 | `			if( nLen < 1 ){` |
|    ! 0 | 6934 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 6935 | `				return PH7_OK;` |
|      - | 6936 | `			}` |
|      - | 6937 | `			/* Duplicate input */` |
|      3 | 6938 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 6939 | `			if( zDup  ){` |
|      3 | 6940 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 6941 | `				/* Register the aux data */` |
|      3 | 6942 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 6943 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 6944 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 6945 | `			}` |
|      1 | 6946 | `		}` |
|      - | 6947 | `	}` |
|      7 | 6948 | `	return PH7_OK;` |
|      4 | 6949 | `}` |
|      - | 6950 | `/*` |
|      - | 6951 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 6952 | ` *  Pad a string to a certain length with another string` |
|      - | 6953 | ` * Parameters` |
|      - | 6954 | ` *  $input` |
|      - | 6955 | ` *   The input string.` |
|      - | 6956 | ` * $pad_length` |
|      - | 6957 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 6958 | ` *   string, no padding takes place.` |
|      - | 6959 | ` * $pad_string` |
|      - | 6960 | ` *   Note:` |
|      - | 6961 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 6962 | ` *    divided by the pad_string's length.` |
|      - | 6963 | ` * $pad_type` |
|      - | 6964 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 6965 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 6966 | ` * Return` |
|      - | 6967 | ` *  The padded string.` |
|      - | 6968 | ` */` |
|     10 | 6969 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6970 | `{` |
|      - | 6971 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 6972 | `	const char *zIn,*zPad;` |
|     11 | 6973 | `	if( nArg < 2 ){` |
|      - | 6974 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 6975 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 6976 | `		return PH7_OK;` |
|      - | 6977 | `	}` |
|      - | 6978 | `	/* Extract the target string */` |
|     11 | 6979 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 6980 | `	/* Padding length */` |
|     11 | 6981 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|     11 | 6982 | `	if( iPadlen > 0 ){` |
|      9 | 6983 | `		iPadlen -= iLen;` |
|      4 | 6984 | `	}` |
|     11 | 6985 | `	if( iPadlen < 1  ){` |
|      - | 6986 | `		/* Return the string verbatim */` |
|      5 | 6987 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      5 | 6988 | `		return PH7_OK;` |
|      - | 6989 | `	}` |
|      7 | 6990 | `	zPad = " "; /* Whitespace padding */` |
|      7 | 6991 | `	iStrpad = (int)sizeof(char);` |
|      7 | 6992 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      7 | 6993 | `	if( nArg > 2 ){` |
|      - | 6994 | `		/* Padding string */` |
|      7 | 6995 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      7 | 6996 | `		if( iStrpad < 1 ){` |
|      - | 6997 | `			/* An empty pad string throws a catchable ValueError in PHP 8` |
|      - | 6998 | `			 * (only reached once padding is actually required). */` |
|      3 | 6999 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7000 | `				"str_pad(): Argument #3 ($pad_string) must not be empty");` |
|      - | 7001 | `		}` |
|      5 | 7002 | `		if( nArg > 3 ){` |
|      - | 7003 | `			/* Padd type */` |
|      5 | 7004 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 7005 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 7006 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 7007 | `			}` |
|      2 | 7008 | `		}` |
|      2 | 7009 | `	}` |
|      5 | 7010 | `	iDiv = 1;` |
|      5 | 7011 | `	if( iType == 2 ){` |
|    ! 0 | 7012 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 7013 | `	}` |
|      - | 7014 | `	/* Perform the requested operation */` |
|      5 | 7015 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 7016 | `		jPad = iStrpad;` |
|      5 | 7017 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 7018 | `			/* Padding */` |
|      5 | 7019 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 7020 | `				break;` |
|      - | 7021 | `			}` |
|      3 | 7022 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7023 | `		}` |
|      3 | 7024 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 7025 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 7026 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 7027 | `				if( jPad > iStrpad ){` |
|    ! 0 | 7028 | `					jPad = iStrpad;` |
|    ! 0 | 7029 | `				}` |
|      3 | 7030 | `				if( jPad < 1){` |
|    ! 0 | 7031 | `					break;` |
|      - | 7032 | `				}` |
|      3 | 7033 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 7034 | `			}` |
|      1 | 7035 | `		}` |
|      1 | 7036 | `	}` |
|      5 | 7037 | `	if( iLen > 0 ){` |
|      - | 7038 | `		/* Append the input string */` |
|      5 | 7039 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7040 | `	}` |
|      5 | 7041 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 7042 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 7043 | `			/* Padding */` |
|      5 | 7044 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 7045 | `				break;` |
|      - | 7046 | `			}` |
|      3 | 7047 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7048 | `		}` |
|      5 | 7049 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 7050 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 7051 | `			if( jPad > iStrpad ){` |
|    ! 0 | 7052 | `				jPad = iStrpad;` |
|    ! 0 | 7053 | `			}` |
|      3 | 7054 | `			if( jPad < 1){` |
|    ! 0 | 7055 | `				break;` |
|      - | 7056 | `			}` |
|      3 | 7057 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 7058 | `		}` |
|      1 | 7059 | `	}` |
|      5 | 7060 | `	return PH7_OK;` |
|      6 | 7061 | `}` |
|      - | 7062 | `/*` |
|      - | 7063 | ` * String replacement private data.` |
|      - | 7064 | ` */` |
|      - | 7065 | `typedef struct str_replace_data str_replace_data;` |
|      - | 7066 | `struct str_replace_data` |
|      - | 7067 | `{` |
|      - | 7068 | `	/* Used by the str_replace family to collect the search/replace arguments. */` |
|      - | 7069 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 7070 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 7071 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 7072 | `};` |
|      - | 7073 | `/*` |
|      - | 7074 | ` * Remove a substring.` |
|      - | 7075 | ` */` |
|      - | 7076 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 7077 | `	for(;;){\` |
|      - | 7078 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 7079 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 7080 | `		++OFFT;\` |
|      - | 7081 | `	}\` |
|      - | 7082 | `}` |
|      - | 7083 | `/*` |
|      - | 7084 | ` * Shift right and insert algorithm.` |
|      - | 7085 | ` */` |
|      - | 7086 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 7087 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 7088 | `		for(;;){\` |
|      - | 7089 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 7090 | `			if(INLEN < 1 ) { break; }\` |
|      - | 7091 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 7092 | `			--INLEN; \` |
|      - | 7093 | `		}\` |
|      - | 7094 | `		for(;;){\` |
|      - | 7095 | `				if(ELEN < 1) { break; }\` |
|      - | 7096 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 7097 | `				OFFT++;\` |
|      - | 7098 | `				ENTRY++;\` |
|      - | 7099 | `				--ELEN;\` |
|      - | 7100 | `		}\` |
|      - | 7101 | `}` |
|      - | 7102 | `/*` |
|      - | 7103 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 7104 | ` * replacement string [i.e: zReplace].` |
|      - | 7105 | ` */` |
|     32 | 7106 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 7107 | `{` |
|     33 | 7108 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 7109 | `	sxu32 n,m;` |
|     33 | 7110 | `	n = SyBlobLength(pWorker);` |
|     33 | 7111 | `	m = nOfft;` |
|      - | 7112 | `	/* Delete the old entry */` |
|    429 | 7113 | `	STRDEL(zInput,n,m,nLen);` |
|     33 | 7114 | `	SyBlobLength(pWorker) -= nLen;` |
|     33 | 7115 | `	if( nReplen > 0 ){` |
|     27 | 7116 | `		sxi32 iRep = nReplen;` |
|      - | 7117 | `		sxi32 rc;` |
|      - | 7118 | `		/*` |
|      - | 7119 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 7120 | `		 * string.` |
|      - | 7121 | `		 */` |
|     27 | 7122 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     27 | 7123 | `		if( rc != SXRET_OK ){` |
|      - | 7124 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 7125 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 7126 | `			return rc;` |
|      - | 7127 | `		}` |
|      - | 7128 | `		/* Perform the insertion now */` |
|     27 | 7129 | `		zInput = (char *)SyBlobData(pWorker);` |
|     27 | 7130 | `		n = SyBlobLength(pWorker);` |
|    129 | 7131 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     27 | 7132 | `		SyBlobLength(pWorker) += nReplen;` |
|     13 | 7133 | `	}` |
|     33 | 7134 | `	return SXRET_OK;` |
|     17 | 7135 | `}` |
|      - | 7136 | `/*` |
|      - | 7137 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 7138 | ` * to collect search/replace string.` |
|      - | 7139 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 7140 | ` */` |
|     26 | 7141 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 7142 | `{` |
|     27 | 7143 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 7144 | `	SyString sWorker;` |
|      - | 7145 | `	const char *zIn;` |
|      - | 7146 | `	int nByte;` |
|      - | 7147 | `	/* Extract a string representation of the given argument */` |
|     27 | 7148 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 7149 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 7150 | `	if( nByte > 0 ){` |
|      - | 7151 | `		char *zDup;` |
|      - | 7152 | `		/* Duplicate the chunk */` |
|     25 | 7153 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 7154 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 7155 | `			);` |
|     25 | 7156 | `		if( zDup == 0 ){` |
|      - | 7157 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 7158 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 7159 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 7160 | `			return SXERR_MEM;` |
|      - | 7161 | `		}` |
|     25 | 7162 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 7163 | `		/* Save the chunk */` |
|     25 | 7164 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 7165 | `	}` |
|      - | 7166 | `	/* Save for later processing */` |
|     27 | 7167 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 7168 | `	/* All done */` |
|     13 | 7169 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 7170 | `	return PH7_OK;` |
|     14 | 7171 | `}` |
|      - | 7172 | `/*` |
|      - | 7173 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 7174 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 7175 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 7176 | ` * Parameters` |
|      - | 7177 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 7178 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 7179 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 7180 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 7181 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 7182 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 7183 | ` * $search` |
|      - | 7184 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 7185 | ` *  to designate multiple needles.` |
|      - | 7186 | ` * $replace` |
|      - | 7187 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 7188 | ` *  to designate multiple replacements.` |
|      - | 7189 | ` * $subject` |
|      - | 7190 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 7191 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 7192 | ` *  of subject, and the return value is an array as well.` |
|      - | 7193 | ` * $count (Not used)` |
|      - | 7194 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 7195 | ` * Return` |
|      - | 7196 | ` * This function returns a string or an array with the replaced values.` |
|      - | 7197 | ` */` |
|  28854 | 7198 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7199 | `{` |
|      - | 7200 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 7201 | `	ProcStringMatch xMatch;` |
|      - | 7202 | `	const char *zIn,*zFunc;` |
|      - | 7203 | `	str_replace_data sRep;` |
|      - | 7204 | `	SyBlob sWorker;` |
|      - | 7205 | `	SySet sReplace;` |
|      - | 7206 | `	SySet sSearch;` |
|      - | 7207 | `	int rep_str;` |
|      - | 7208 | `	int nByte;` |
|      - | 7209 | `	sxi32 rc;` |
|  28859 | 7210 | `	if( nArg < 3 ){` |
|      - | 7211 | `		/* Missing/Invalid arguments,return null */` |
|    ! 0 | 7212 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7213 | `		return PH7_OK;` |
|      - | 7214 | `	}` |
|      - | 7215 | `	/* Initialize fields */` |
|  28859 | 7216 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  28859 | 7217 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  28859 | 7218 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  28859 | 7219 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  28859 | 7220 | `	sRep.pCtx = pCtx;` |
|  28859 | 7221 | `	sRep.pCollector = &sSearch;` |
|  28859 | 7222 | `	rep_str = 0;` |
|      - | 7223 | `	/* Extract the subject */` |
|  28859 | 7224 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  28859 | 7225 | `	if( nByte < 1 ){` |
|      - | 7226 | `		/* Nothing to replace,return the empty string */` |
|     29 | 7227 | `		ph7_result_string(pCtx,"",0);` |
|     29 | 7228 | `		return PH7_OK;` |
|      - | 7229 | `	}` |
|      - | 7230 | `	/* Copy the subject */` |
|  28831 | 7231 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 7232 | `	/* Search string */` |
|  28831 | 7233 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 7234 | `		/* Collect search string */` |
|      9 | 7235 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 7236 | `	}else{` |
|      - | 7237 | `		/* Single pattern */` |
|  28823 | 7238 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  28823 | 7239 | `		if( nByte < 1 ){` |
|      - | 7240 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 7241 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 7242 | `			return PH7_OK;` |
|      - | 7243 | `		}` |
|  28819 | 7244 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 7245 | `		/* Save for later processing */` |
|  28819 | 7246 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 7247 | `	}` |
|      - | 7248 | `	/* Replace string */` |
|  28827 | 7249 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 7250 | `		/* Collect replace string */` |
|      7 | 7251 | `		sRep.pCollector = &sReplace;` |
|      7 | 7252 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 7253 | `	}else{` |
|      - | 7254 | `		/* Single needle */` |
|  28821 | 7255 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  28821 | 7256 | `		rep_str = 1;` |
|  28821 | 7257 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 7258 | `		/* Save for later processing */` |
|  28821 | 7259 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 7260 | `	}` |
|      - | 7261 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  28827 | 7262 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 7263 | `		SySetRelease(&sSearch);` |
|    ! 0 | 7264 | `		SySetRelease(&sReplace);` |
|    ! 0 | 7265 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 7266 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7267 | `	}` |
|      - | 7268 | `	/* Reset loop cursors */` |
|  28827 | 7269 | `	SySetResetCursor(&sSearch);` |
|  28827 | 7270 | `	SySetResetCursor(&sReplace);` |
|  28827 | 7271 | `	pReplace = pSearch = 0; /* cc warning */` |
|  28827 | 7272 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 7273 | `	/* Extract function name */` |
|  28827 | 7274 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 7275 | `	/* Set the default pattern match routine */` |
|  28827 | 7276 | `	xMatch = SyBlobSearch;` |
|  28827 | 7277 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 7278 | `		/* Case insensitive pattern match */` |
|     11 | 7279 | `		xMatch = iPatternMatch;` |
|      5 | 7280 | `	}` |
|      - | 7281 | `	/* Start the replace process */` |
|  57657 | 7282 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 7283 | `		sxu32 nCount,nOfft;` |
|  28835 | 7284 | `		if( pSearch->nByte <  1 ){` |
|      - | 7285 | `			/* Empty string,ignore */` |
|      3 | 7286 | `			continue;` |
|      - | 7287 | `		}` |
|      - | 7288 | `		/* Extract the replace string */` |
|  28833 | 7289 | `		if( rep_str ){` |
|  28823 | 7290 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  14414 | 7291 | `		}else{` |
|     11 | 7292 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 7293 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 7294 | `				 * An empty string is used for the rest of replacement values` |
|      - | 7295 | `				 */` |
|      3 | 7296 | `				pReplace = 0;` |
|      1 | 7297 | `			}` |
|      - | 7298 | `		}` |
|  28833 | 7299 | `		if( pReplace == 0 ){` |
|      - | 7300 | `			/* Use an empty string instead */` |
|      3 | 7301 | `			pReplace = &sTemp;` |
|      1 | 7302 | `		}` |
|  28833 | 7303 | `		nOfft = nCount = 0;` |
|  14430 | 7304 | `		for(;;){` |
|  28865 | 7305 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 7306 | `				break;` |
|      - | 7307 | `			}` |
|      - | 7308 | `			/* Perform a pattern lookup */` |
|  43277 | 7309 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  28848 | 7310 | `				pSearch->nByte,&nOfft);` |
|  28853 | 7311 | `			if( rc != SXRET_OK ){` |
|      - | 7312 | `				/* Pattern not found */` |
|  28821 | 7313 | `				break;` |
|      - | 7314 | `			}` |
|      - | 7315 | `			/* Perform the replace operation */` |
|     33 | 7316 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     33 | 7317 | `			if( rc != SXRET_OK ){` |
|      - | 7318 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 7319 | `				SySetRelease(&sSearch);` |
|    ! 0 | 7320 | `				SySetRelease(&sReplace);` |
|    ! 0 | 7321 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 7322 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 7323 | `			}` |
|      - | 7324 | `			/* Increment offset counter */` |
|     33 | 7325 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 7326 | `		}` |
|      5 | 7327 | `	}` |
|      - | 7328 | `	/* All done,clean-up the mess left behind */` |
|  28827 | 7329 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  28827 | 7330 | `	SySetRelease(&sSearch);` |
|  28827 | 7331 | `	SySetRelease(&sReplace);` |
|  28827 | 7332 | `	SyBlobRelease(&sWorker);` |
|  28827 | 7333 | `	if( rc != PH7_OK ){` |
|    ! 0 | 7334 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7335 | `	}` |
|  28827 | 7336 | `	return PH7_OK;` |
|  14432 | 7337 | `}` |
|      - | 7338 | `/*` |
|      - | 7339 | ` * strtr() array form: a single (key,value) pair copied out of the replace_pairs` |
|      - | 7340 | ` * array. The bytes are owned by a persistent pool (see strtr_collect) rather than` |
|      - | 7341 | ` * the transient walker values, which HashmapWalk releases after each callback, so` |
|      - | 7342 | ` * we store byte offsets into that pool instead of raw pointers.` |
|      - | 7343 | ` */` |
|      - | 7344 | `typedef struct strtr_entry strtr_entry;` |
|      - | 7345 | `struct strtr_entry` |
|      - | 7346 | `{` |
|      - | 7347 | `	sxu32 nKeyOfft; /* Offset of the search key inside the pool */` |
|      - | 7348 | `	sxu32 nKeyLen;  /* Length of the search key */` |
|      - | 7349 | `	sxu32 nValOfft; /* Offset of the replacement inside the pool */` |
|      - | 7350 | `	sxu32 nValLen;  /* Length of the replacement */` |
|      - | 7351 | `};` |
|      - | 7352 | `typedef struct strtr_collect strtr_collect;` |
|      - | 7353 | `struct strtr_collect` |
|      - | 7354 | `{` |
|      - | 7355 | `	SyBlob *pPool;  /* Byte pool holding copied key + value bytes */` |
|      - | 7356 | `	SySet  *pTable; /* Set of strtr_entry (parallel offsets into pPool) */` |
|      - | 7357 | `	sxi32   rc;     /* Carries an allocation failure (SXERR_MEM) out of the walker */` |
|      - | 7358 | `};` |
|      - | 7359 | `/*` |
|      - | 7360 | ` * Collect one replace_pairs entry into the persistent pool/offset table.` |
|      - | 7361 | ` * PHP coerces both the key and the value to string (an integer key becomes its` |
|      - | 7362 | ` * decimal form) and ignores an empty-string key.` |
|      - | 7363 | ` */` |
|     20 | 7364 | `static int StrtrCollectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 7365 | `{` |
|     21 | 7366 | `	strtr_collect *pCol = (strtr_collect *)pUserData;` |
|      - | 7367 | `	const char *zKey,*zVal;` |
|      - | 7368 | `	strtr_entry sEnt;` |
|      - | 7369 | `	int nKey,nVal;` |
|     21 | 7370 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     21 | 7371 | `	if( nKey < 1 ){` |
|      - | 7372 | `		/* PHP ignores an empty-string key (it also emits a warning we do not replicate). */` |
|    ! 0 | 7373 | `		return PH7_OK;` |
|      - | 7374 | `	}` |
|     21 | 7375 | `	zVal = ph7_value_to_string(pData,&nVal);` |
|     21 | 7376 | `	sEnt.nKeyOfft = SyBlobLength(pCol->pPool);` |
|     21 | 7377 | `	sEnt.nKeyLen  = (sxu32)nKey;` |
|     21 | 7378 | `	if( SyBlobAppend(pCol->pPool,(const void *)zKey,(sxu32)nKey) != SXRET_OK ){` |
|    ! 0 | 7379 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 7380 | `		return SXERR_ABORT;` |
|      - | 7381 | `	}` |
|     21 | 7382 | `	sEnt.nValOfft = SyBlobLength(pCol->pPool);` |
|     21 | 7383 | `	sEnt.nValLen  = (sxu32)nVal;` |
|     21 | 7384 | `	if( nVal > 0 && SyBlobAppend(pCol->pPool,(const void *)zVal,(sxu32)nVal) != SXRET_OK ){` |
|    ! 0 | 7385 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 7386 | `		return SXERR_ABORT;` |
|      - | 7387 | `	}` |
|     21 | 7388 | `	if( SySetPut(pCol->pTable,(const void *)&sEnt) != SXRET_OK ){` |
|    ! 0 | 7389 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 7390 | `		return SXERR_ABORT;` |
|      - | 7391 | `	}` |
|     21 | 7392 | `	return PH7_OK;` |
|     11 | 7393 | `}` |
|      - | 7394 | `/*` |
|      - | 7395 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 7396 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 7397 | ` *  Translate characters or replace substrings.` |
|      - | 7398 | ` * Parameters` |
|      - | 7399 | ` *  $str` |
|      - | 7400 | ` *  The string being translated.` |
|      - | 7401 | ` * $from` |
|      - | 7402 | ` *  The string being translated to to.` |
|      - | 7403 | ` * $to` |
|      - | 7404 | ` *  The string replacing from.` |
|      - | 7405 | ` * $replace_pairs` |
|      - | 7406 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 7407 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 7408 | ` * Return` |
|      - | 7409 | ` *  The translated string.` |
|      - | 7410 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 7411 | ` */` |
|     12 | 7412 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7413 | `{` |
|      - | 7414 | `	const char *zIn;` |
|      - | 7415 | `	int nLen;` |
|     13 | 7416 | `	if( nArg < 1 ){` |
|      - | 7417 | `		/* Nothing to replace,return FALSE */` |
|    ! 0 | 7418 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7419 | `		return PH7_OK;` |
|      - | 7420 | `	}` |
|     13 | 7421 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 7422 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 7423 | `		/* Invalid arguments */` |
|    ! 0 | 7424 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 7425 | `		return PH7_OK;` |
|      - | 7426 | `	}` |
|     18 | 7427 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 7428 | `		strtr_collect sCol;` |
|      - | 7429 | `		SyBlob sPool,sWorker;` |
|      - | 7430 | `		SySet sTable;` |
|      - | 7431 | `		const char *zPool;` |
|      - | 7432 | `		strtr_entry *pEnt;` |
|      - | 7433 | `		sxi32 rc;` |
|      - | 7434 | `		int i,iRun;` |
|      - | 7435 | `		/*` |
|      - | 7436 | `		 * PHP's array-form strtr is a single left-to-right pass over the subject:` |
|      - | 7437 | `		 * at every position it substitutes the LONGEST replace_pairs key that` |
|      - | 7438 | `		 * matches there, then advances past the key (replacements are never` |
|      - | 7439 | `		 * rescanned). It is not a sequential per-key global replace. First copy` |
|      - | 7440 | `		 * the pairs into a persistent pool, then run that scan.` |
|      - | 7441 | `		 */` |
|     11 | 7442 | `		SyBlobInit(&sPool,&pCtx->pVm->sAllocator);` |
|     11 | 7443 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     11 | 7444 | `		SySetInit(&sTable,&pCtx->pVm->sAllocator,sizeof(strtr_entry));` |
|     11 | 7445 | `		sCol.pPool  = &sPool;` |
|     11 | 7446 | `		sCol.pTable = &sTable;` |
|     11 | 7447 | `		sCol.rc     = SXRET_OK;` |
|     11 | 7448 | `		ph7_array_walk(apArg[1],StrtrCollectWalker,&sCol);` |
|     11 | 7449 | `		if( sCol.rc != SXRET_OK ){` |
|      - | 7450 | `			/* Allocation failure while collecting the pairs: surface a fatal */` |
|    ! 0 | 7451 | `			SyBlobRelease(&sPool);` |
|    ! 0 | 7452 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 7453 | `			SySetRelease(&sTable);` |
|    ! 0 | 7454 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 7455 | `		}` |
|      - | 7456 | `		/* The pool is now stable, so offsets can be resolved against its base. */` |
|     11 | 7457 | `		zPool = (const char *)SyBlobData(&sPool);` |
|     11 | 7458 | `		rc = SXRET_OK;` |
|     11 | 7459 | `		iRun = 0; /* Start of the pending run of unmatched bytes copied verbatim. */` |
|     43 | 7460 | `		for( i = 0 ; i < nLen ; ){` |
|     33 | 7461 | `			strtr_entry *pBest = 0;` |
|     33 | 7462 | `			sxu32 nBest = 0;` |
|      - | 7463 | `			/* Pick the longest key that matches at the current position. */` |
|     33 | 7464 | `			SySetResetCursor(&sTable);` |
|     97 | 7465 | `			while( SXRET_OK == SySetGetNextEntry(&sTable,(void **)&pEnt) ){` |
|     64 | 7466 | `				if( pEnt->nKeyLen > nBest` |
|     60 | 7467 | `					&& pEnt->nKeyLen <= (sxu32)(nLen - i)` |
|     56 | 7468 | `					&& SyMemcmp(zPool + pEnt->nKeyOfft,zIn + i,pEnt->nKeyLen) == 0 ){` |
|     29 | 7469 | `					nBest = pEnt->nKeyLen;` |
|     29 | 7470 | `					pBest = pEnt;` |
|     14 | 7471 | `				}` |
|      1 | 7472 | `			}` |
|     33 | 7473 | `			if( pBest == 0 ){` |
|      - | 7474 | `				/* No key here: extend the literal run and copy it in one shot later. */` |
|      9 | 7475 | `				i++;` |
|      9 | 7476 | `				continue;` |
|      - | 7477 | `			}` |
|      - | 7478 | `			/* Flush the pending literal run, then the replacement. */` |
|     25 | 7479 | `			if( i > iRun ){` |
|      5 | 7480 | `				rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(i - iRun));` |
|      2 | 7481 | `			}` |
|     25 | 7482 | `			if( rc == SXRET_OK && pBest->nValLen > 0 ){` |
|     25 | 7483 | `				rc = SyBlobAppend(&sWorker,zPool + pBest->nValOfft,pBest->nValLen);` |
|     12 | 7484 | `			}` |
|     25 | 7485 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 7486 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 7487 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 7488 | `				SySetRelease(&sTable);` |
|    ! 0 | 7489 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 7490 | `			}` |
|     25 | 7491 | `			i += (int)pBest->nKeyLen;` |
|     25 | 7492 | `			iRun = i;` |
|      1 | 7493 | `		}` |
|      - | 7494 | `		/* Flush the trailing literal run. */` |
|     11 | 7495 | `		if( nLen > iRun ){` |
|      3 | 7496 | `			rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(nLen - iRun));` |
|      3 | 7497 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 7498 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 7499 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 7500 | `				SySetRelease(&sTable);` |
|    ! 0 | 7501 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 7502 | `			}` |
|      1 | 7503 | `		}` |
|      - | 7504 | `		/* All done, return the result string */` |
|     16 | 7505 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|     10 | 7506 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 7507 | `		/* Clean-up */` |
|     11 | 7508 | `		SyBlobRelease(&sPool);` |
|     11 | 7509 | `		SyBlobRelease(&sWorker);` |
|     11 | 7510 | `		SySetRelease(&sTable);` |
|     11 | 7511 | `		if( rc != PH7_OK ){` |
|    ! 0 | 7512 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 7513 | `		}` |
|      6 | 7514 | `	}else{` |
|      - | 7515 | `		int i,flen,tlen,c,iOfft;` |
|      - | 7516 | `		const char *zFrom,*zTo;` |
|      3 | 7517 | `		if( nArg < 3 ){` |
|      - | 7518 | `			/* Nothing to replace */` |
|    ! 0 | 7519 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 7520 | `			return PH7_OK;` |
|      - | 7521 | `		}` |
|      - | 7522 | `		/* Extract given arguments */` |
|      3 | 7523 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 7524 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 7525 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 7526 | `			/* Nothing to replace */` |
|    ! 0 | 7527 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 7528 | `			return PH7_OK;` |
|      - | 7529 | `		}` |
|      - | 7530 | `		/* Start the replace process */` |
|     13 | 7531 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 7532 | `			c = zIn[i];` |
|     11 | 7533 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 7534 | `				if ( iOfft < tlen ){` |
|      5 | 7535 | `					c = zTo[iOfft];` |
|      2 | 7536 | `				}` |
|      2 | 7537 | `			}` |
|     11 | 7538 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 7539 |  |
|      6 | 7540 | `		}` |
|      - | 7541 | `	}` |
|     13 | 7542 | `	return PH7_OK;` |
|      7 | 7543 | `}` |
|      - | 7544 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 7545 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 7546 | `/*` |
|      - | 7547 | ` * Parse an INI string.` |
|      - | 7548 |  |
|      - | 7549 | ` * According to wikipedia` |
|      - | 7550 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 7551 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 7552 | ` *  Format` |
|      - | 7553 | `*    Properties` |
|      - | 7554 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 7555 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 7556 | `*     Example:` |
|      - | 7557 | `*      name=value` |
|      - | 7558 | `*    Sections` |
|      - | 7559 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 7560 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 7561 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 7562 | `*     or the end of the file. Sections may not be nested.` |
|      - | 7563 | `*     Example:` |
|      - | 7564 | `*      [section]` |
|      - | 7565 | `*   Comments` |
|      - | 7566 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 7567 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 7568 | `*/` |
|     12 | 7569 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 7570 | `{` |
|      - | 7571 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 7572 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 7573 | `	SyHashEntry *pEntry;` |
|      - | 7574 | `	SyString sEntry;` |
|      - | 7575 | `	SyHash sHash;` |
|      - | 7576 | `	int c;` |
|      - | 7577 | `	/* Create an empty array and worker variables */` |
|     13 | 7578 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 7579 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 7580 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 7581 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 7582 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|    ! 0 | 7583 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7584 | `	}` |
|     13 | 7585 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 7586 | `	pCur = pArray;` |
|      - | 7587 | `	/* Start the parse process */` |
|     21 | 7588 | `	for(;;){` |
|      - | 7589 | `		/* Ignore leading white spaces */` |
|     69 | 7590 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 7591 | `			zIn++;` |
|      1 | 7592 | `		}` |
|     43 | 7593 | `		if( zIn >= zEnd ){` |
|      - | 7594 | `			/* No more input to process */` |
|     13 | 7595 | `			break;` |
|      - | 7596 | `		}` |
|     31 | 7597 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 7598 | `			/* Comment til the end of line */` |
|    ! 0 | 7599 | `			zIn++;` |
|    ! 0 | 7600 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 7601 | `				zIn++;` |
|    ! 0 | 7602 | `			}` |
|    ! 0 | 7603 | `			continue;` |
|      - | 7604 | `		}` |
|      - | 7605 | `		/* Reset the string cursor of the working variable */` |
|     31 | 7606 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 7607 | `		if( zIn[0] == '[' ){` |
|      - | 7608 | `			/* Section: Extract the section name */` |
|      9 | 7609 | `			zIn++;` |
|      9 | 7610 | `			zCur = zIn;` |
|     73 | 7611 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 7612 | `				zIn++;` |
|      1 | 7613 | `			}` |
|      9 | 7614 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 7615 | `				/* Save the section name */` |
|      5 | 7616 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 7617 | `				SyStringFullTrim(&sEntry);` |
|      5 | 7618 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 7619 | `				if( sEntry.nByte > 0 ){` |
|      - | 7620 | `					/* Associate an array with the section */` |
|      5 | 7621 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 7622 | `					if( pSection ){` |
|      5 | 7623 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 7624 | `						pCur = pSection;` |
|      2 | 7625 | `					}` |
|      2 | 7626 | `				}` |
|      2 | 7627 | `			}` |
|      9 | 7628 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 7629 | `		}else{` |
|      - | 7630 | `			ph7_value *pOldCur;` |
|      - | 7631 | `			int is_array;` |
|      - | 7632 | `			int iLen;` |
|      - | 7633 | `			/* Properties */` |
|     23 | 7634 | `			is_array = 0;` |
|     23 | 7635 | `			zCur = zIn;` |
|     23 | 7636 | `			iLen = 0; /* cc warning */` |
|     23 | 7637 | `			pOldCur = pCur;` |
|    155 | 7638 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 7639 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 7640 | `					/* Array */` |
|    ! 0 | 7641 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 7642 | `					is_array = 1;` |
|    ! 0 | 7643 | `					if( iLen > 0 ){` |
|    ! 0 | 7644 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 7645 | `						/* Query the hashtable */` |
|    ! 0 | 7646 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 7647 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 7648 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 7649 | `						if( pEntry ){` |
|    ! 0 | 7650 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 7651 | `						}else{` |
|      - | 7652 | `							/* Create an empty array */` |
|    ! 0 | 7653 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 7654 | `							if( pvArr ){` |
|      - | 7655 | `								/* Save the entry */` |
|    ! 0 | 7656 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 7657 | `								/* Insert the entry */` |
|    ! 0 | 7658 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 7659 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 7660 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 7661 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 7662 | `							}` |
|      - | 7663 | `						}` |
|    ! 0 | 7664 | `						if( pvArr ){` |
|    ! 0 | 7665 | `							pCur = pvArr;` |
|    ! 0 | 7666 | `						}` |
|    ! 0 | 7667 | `					}` |
|    ! 0 | 7668 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 7669 | `						zIn++;` |
|    ! 0 | 7670 | `					}` |
|    ! 0 | 7671 | `				}` |
|    133 | 7672 | `				zIn++;` |
|      1 | 7673 | `			}` |
|     23 | 7674 | `			if( !is_array ){` |
|     23 | 7675 | `				iLen = (int)(zIn-zCur);` |
|     11 | 7676 | `			}` |
|      - | 7677 | `			/* Trim the key */` |
|     23 | 7678 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 7679 | `			SyStringFullTrim(&sEntry);` |
|     23 | 7680 | `			if( sEntry.nByte > 0 ){` |
|     23 | 7681 | `				if( !is_array ){` |
|      - | 7682 | `					/* Save the key name */` |
|     23 | 7683 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 7684 | `				}` |
|      - | 7685 | `				/* extract key value */` |
|     23 | 7686 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 7687 | `				zIn++; /* '=' */` |
|     39 | 7688 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 7689 | `					zIn++;` |
|      1 | 7690 | `				}` |
|     23 | 7691 | `				if( zIn < zEnd ){` |
|     21 | 7692 | `					zCur = zIn;` |
|     21 | 7693 | `					c = zIn[0];` |
|     21 | 7694 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 7695 | `						zIn++;` |
|      - | 7696 | `						/* Delimit the value */` |
|    ! 0 | 7697 | `						while( zIn < zEnd ){` |
|    ! 0 | 7698 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 7699 | `								break;` |
|      - | 7700 | `							}` |
|    ! 0 | 7701 | `							zIn++;` |
|    ! 0 | 7702 | `						}` |
|    ! 0 | 7703 | `						if( zIn < zEnd ){` |
|    ! 0 | 7704 | `							zIn++;` |
|    ! 0 | 7705 | `						}` |
|    ! 0 | 7706 | `					}else{` |
|    125 | 7707 | `						while( zIn < zEnd ){` |
|    123 | 7708 | `							if( zIn[0] == '\n' ){` |
|     19 | 7709 | `								if( zIn[-1] != '\\' ){` |
|     19 | 7710 | `									break;` |
|    ! 0 | 7711 | `								}` |
|    105 | 7712 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 7713 | `								/* Inline comments */` |
|    ! 0 | 7714 | `								break;` |
|      - | 7715 | `							}` |
|    105 | 7716 | `							zIn++;` |
|      1 | 7717 | `						}` |
|      - | 7718 | `					}` |
|      - | 7719 | `					/* Trim the value */` |
|     21 | 7720 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 7721 | `					SyStringFullTrim(&sEntry);` |
|     21 | 7722 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 7723 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 7724 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 7725 | `					}` |
|     21 | 7726 | `					if( sEntry.nByte > 0 ){` |
|     21 | 7727 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 7728 | `					}` |
|      - | 7729 | `					/* Insert the key and it's value */` |
|     21 | 7730 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 7731 | `				}` |
|     12 | 7732 | `			}else{` |
|    ! 0 | 7733 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 7734 | `					zIn++;` |
|    ! 0 | 7735 | `				}` |
|      - | 7736 | `			}` |
|     23 | 7737 | `			pCur = pOldCur;` |
|      - | 7738 | `		}` |
|      1 | 7739 | `	}` |
|     13 | 7740 | `	SyHashRelease(&sHash);` |
|      - | 7741 | `	/* Return the parse of the INI string */` |
|     13 | 7742 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 7743 | `	return SXRET_OK;` |
|      7 | 7744 | `}` |
|      - | 7745 | `/*` |
|      - | 7746 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 7747 | ` *  Parse a configuration string.` |
|      - | 7748 | ` * Parameters` |
|      - | 7749 | ` *  $ini` |
|      - | 7750 | ` *   The contents of the ini file being parsed.` |
|      - | 7751 | ` *  $process_sections` |
|      - | 7752 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 7753 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 7754 | ` *  $scanner_mode (Not used)` |
|      - | 7755 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 7756 | ` *   then option values will not be parsed.` |
|      - | 7757 | ` * Return` |
|      - | 7758 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 7759 | ` */` |
|     10 | 7760 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7761 | `{` |
|      - | 7762 | `	const char *zIni;` |
|      - | 7763 | `	int nByte;` |
|     11 | 7764 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 7765 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 7766 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7767 | `		return PH7_OK;` |
|      - | 7768 | `	}` |
|      - | 7769 | `	/* Extract the raw INI buffer */` |
|     11 | 7770 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 7771 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|     11 | 7772 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      6 | 7773 | `}` |
|      - | 7774 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 7775 |  |
|      - | 7776 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 7777 |  |
|      - | 7778 | `/*` |
|      - | 7779 | ` * Ctype Functions.` |
|      - | 7780 | ` * Status:` |
|      - | 7781 | ` *    Stable.` |
|      - | 7782 | ` */` |
|      - | 7783 | `/*` |
|      - | 7784 | ` * bool ctype_alnum(string $text)` |
|      - | 7785 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 7786 | ` * Parameters` |
|      - | 7787 | ` *  $text` |
|      - | 7788 | ` *   The tested string.` |
|      - | 7789 | ` * Return` |
|      - | 7790 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 7791 | ` */` |
|     14 | 7792 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7793 | `{` |
|      - | 7794 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7795 | `	int nLen;` |
|     15 | 7796 | `	if( nArg < 1 ){` |
|      - | 7797 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7798 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7799 | `		return PH7_OK;` |
|      - | 7800 | `	}` |
|      - | 7801 | `	/* Extract the target string */` |
|     15 | 7802 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 7803 | `	zEnd = &zIn[nLen];` |
|     15 | 7804 | `	if( nLen < 1 ){` |
|      - | 7805 | `		/* Empty string,return FALSE */` |
|      3 | 7806 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7807 | `		return PH7_OK;` |
|      - | 7808 | `	}` |
|      - | 7809 | `	/* Perform the requested operation */` |
|     32 | 7810 | `	for(;;){` |
|     65 | 7811 | `		if( zIn >= zEnd ){` |
|      - | 7812 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7813 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7814 | `			return PH7_OK;` |
|      - | 7815 | `		}` |
|     57 | 7816 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 7817 | `			break;` |
|      - | 7818 | `		}` |
|      - | 7819 | `		/* Point to the next character */` |
|     53 | 7820 | `		zIn++;` |
|      1 | 7821 | `	}` |
|      - | 7822 | `	/* The test failed,return FALSE */` |
|      5 | 7823 | `	ph7_result_bool(pCtx,0);` |
|      5 | 7824 | `	return PH7_OK;` |
|      8 | 7825 | `}` |
|      - | 7826 | `/*` |
|      - | 7827 | ` * bool ctype_alpha(string $text)` |
|      - | 7828 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 7829 | ` * Parameters` |
|      - | 7830 | ` *  $text` |
|      - | 7831 | ` *   The tested string.` |
|      - | 7832 | ` * Return` |
|      - | 7833 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 7834 | ` */` |
|     16 | 7835 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7836 | `{` |
|      - | 7837 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7838 | `	int nLen;` |
|     17 | 7839 | `	if( nArg < 1 ){` |
|      - | 7840 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7841 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7842 | `		return PH7_OK;` |
|      - | 7843 | `	}` |
|      - | 7844 | `	/* Extract the target string */` |
|     17 | 7845 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7846 | `	zEnd = &zIn[nLen];` |
|     17 | 7847 | `	if( nLen < 1 ){` |
|      - | 7848 | `		/* Empty string,return FALSE */` |
|      3 | 7849 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7850 | `		return PH7_OK;` |
|      - | 7851 | `	}` |
|      - | 7852 | `	/* Perform the requested operation */` |
|     42 | 7853 | `	for(;;){` |
|     85 | 7854 | `		if( zIn >= zEnd ){` |
|      - | 7855 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7856 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7857 | `			return PH7_OK;` |
|      - | 7858 | `		}` |
|     77 | 7859 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 7860 | `			break;` |
|      - | 7861 | `		}` |
|      - | 7862 | `		/* Point to the next character */` |
|     71 | 7863 | `		zIn++;` |
|      1 | 7864 | `	}` |
|      - | 7865 | `	/* The test failed,return FALSE */` |
|      7 | 7866 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7867 | `	return PH7_OK;` |
|      9 | 7868 | `}` |
|      - | 7869 | `/*` |
|      - | 7870 | ` * bool ctype_cntrl(string $text)` |
|      - | 7871 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 7872 | ` * Parameters` |
|      - | 7873 | ` *  $text` |
|      - | 7874 | ` *   The tested string.` |
|      - | 7875 | ` * Return` |
|      - | 7876 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 7877 | ` */` |
|     16 | 7878 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7879 | `{` |
|      - | 7880 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7881 | `	int nLen;` |
|     17 | 7882 | `	if( nArg < 1 ){` |
|      - | 7883 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7884 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7885 | `		return PH7_OK;` |
|      - | 7886 | `	}` |
|      - | 7887 | `	/* Extract the target string */` |
|     17 | 7888 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 7889 | `	zEnd = &zIn[nLen];` |
|     17 | 7890 | `	if( nLen < 1 ){` |
|      - | 7891 | `		/* Empty string,return FALSE */` |
|      3 | 7892 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7893 | `		return PH7_OK;` |
|      - | 7894 | `	}` |
|      - | 7895 | `	/* Perform the requested operation */` |
|     14 | 7896 | `	for(;;){` |
|     29 | 7897 | `		if( zIn >= zEnd ){` |
|      - | 7898 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 7899 | `			ph7_result_bool(pCtx,1);` |
|      9 | 7900 | `			return PH7_OK;` |
|      - | 7901 | `		}` |
|     21 | 7902 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7903 | `			/* UTF-8 stream  */` |
|    ! 0 | 7904 | `			break;` |
|      - | 7905 | `		}` |
|     21 | 7906 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 7907 | `			break;` |
|      - | 7908 | `		}` |
|      - | 7909 | `		/* Point to the next character */` |
|     15 | 7910 | `		zIn++;` |
|      1 | 7911 | `	}` |
|      - | 7912 | `	/* The test failed,return FALSE */` |
|      7 | 7913 | `	ph7_result_bool(pCtx,0);` |
|      7 | 7914 | `	return PH7_OK;` |
|      9 | 7915 | `}` |
|      - | 7916 | `/*` |
|      - | 7917 | ` * bool ctype_digit(string $text)` |
|      - | 7918 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 7919 | ` * Parameters` |
|      - | 7920 | ` *  $text` |
|      - | 7921 | ` *   The tested string.` |
|      - | 7922 | ` * Return` |
|      - | 7923 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 7924 | ` */` |
|   1614 | 7925 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7926 | `{` |
|      - | 7927 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7928 | `	int nLen;` |
|   1619 | 7929 | `	if( nArg < 1 ){` |
|      - | 7930 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7931 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7932 | `		return PH7_OK;` |
|      - | 7933 | `	}` |
|      - | 7934 | `	/* Extract the target string */` |
|   1619 | 7935 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1619 | 7936 | `	zEnd = &zIn[nLen];` |
|   1619 | 7937 | `	if( nLen < 1 ){` |
|      - | 7938 | `		/* Empty string,return FALSE */` |
|      3 | 7939 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7940 | `		return PH7_OK;` |
|      - | 7941 | `	}` |
|      - | 7942 | `	/* Perform the requested operation */` |
|   1515 | 7943 | `	for(;;){` |
|   3035 | 7944 | `		if( zIn >= zEnd ){` |
|      - | 7945 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1373 | 7946 | `			ph7_result_bool(pCtx,1);` |
|   1373 | 7947 | `			return PH7_OK;` |
|      - | 7948 | `		}` |
|   1667 | 7949 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7950 | `			/* UTF-8 stream  */` |
|    ! 0 | 7951 | `			break;` |
|      - | 7952 | `		}` |
|   1667 | 7953 | `		if( !SyisDigit(zIn[0]) ){` |
|    249 | 7954 | `			break;` |
|      - | 7955 | `		}` |
|      - | 7956 | `		/* Point to the next character */` |
|   1423 | 7957 | `		zIn++;` |
|      5 | 7958 | `	}` |
|      - | 7959 | `	/* The test failed,return FALSE */` |
|    249 | 7960 | `	ph7_result_bool(pCtx,0);` |
|    249 | 7961 | `	return PH7_OK;` |
|    812 | 7962 | `}` |
|      - | 7963 | `/*` |
|      - | 7964 | ` * bool ctype_xdigit(string $text)` |
|      - | 7965 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 7966 | ` * Parameters` |
|      - | 7967 | ` *  $text` |
|      - | 7968 | ` *   The tested string.` |
|      - | 7969 | ` * Return` |
|      - | 7970 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 7971 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 7972 | ` */` |
|     18 | 7973 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7974 | `{` |
|      - | 7975 | `	const unsigned char *zIn,*zEnd;` |
|      - | 7976 | `	int nLen;` |
|     19 | 7977 | `	if( nArg < 1 ){` |
|      - | 7978 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7979 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7980 | `		return PH7_OK;` |
|      - | 7981 | `	}` |
|      - | 7982 | `	/* Extract the target string */` |
|     19 | 7983 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 7984 | `	zEnd = &zIn[nLen];` |
|     19 | 7985 | `	if( nLen < 1 ){` |
|      - | 7986 | `		/* Empty string,return FALSE */` |
|      3 | 7987 | `		ph7_result_bool(pCtx,0);` |
|      3 | 7988 | `		return PH7_OK;` |
|      - | 7989 | `	}` |
|      - | 7990 | `	/* Perform the requested operation */` |
|     46 | 7991 | `	for(;;){` |
|     93 | 7992 | `		if( zIn >= zEnd ){` |
|      - | 7993 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 7994 | `			ph7_result_bool(pCtx,1);` |
|     11 | 7995 | `			return PH7_OK;` |
|      - | 7996 | `		}` |
|     83 | 7997 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 7998 | `			/* UTF-8 stream  */` |
|    ! 0 | 7999 | `			break;` |
|      - | 8000 | `		}` |
|     83 | 8001 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 8002 | `			break;` |
|      - | 8003 | `		}` |
|      - | 8004 | `		/* Point to the next character */` |
|     77 | 8005 | `		zIn++;` |
|      1 | 8006 | `	}` |
|      - | 8007 | `	/* The test failed,return FALSE */` |
|      7 | 8008 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8009 | `	return PH7_OK;` |
|     10 | 8010 | `}` |
|      - | 8011 | `/*` |
|      - | 8012 | ` * bool ctype_graph(string $text)` |
|      - | 8013 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 8014 | ` * Parameters` |
|      - | 8015 | ` *  $text` |
|      - | 8016 | ` *   The tested string.` |
|      - | 8017 | ` * Return` |
|      - | 8018 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 8019 | ` * (no white space), FALSE otherwise.` |
|      - | 8020 | ` */` |
|     16 | 8021 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8022 | `{` |
|      - | 8023 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8024 | `	int nLen;` |
|     17 | 8025 | `	if( nArg < 1 ){` |
|      - | 8026 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8027 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8028 | `		return PH7_OK;` |
|      - | 8029 | `	}` |
|      - | 8030 | `	/* Extract the target string */` |
|     17 | 8031 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8032 | `	zEnd = &zIn[nLen];` |
|     17 | 8033 | `	if( nLen < 1 ){` |
|      - | 8034 | `		/* Empty string,return FALSE */` |
|      3 | 8035 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8036 | `		return PH7_OK;` |
|      - | 8037 | `	}` |
|      - | 8038 | `	/* Perform the requested operation */` |
|     57 | 8039 | `	for(;;){` |
|    115 | 8040 | `		if( zIn >= zEnd ){` |
|      - | 8041 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8042 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8043 | `			return PH7_OK;` |
|      - | 8044 | `		}` |
|    107 | 8045 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8046 | `			/* UTF-8 stream  */` |
|    ! 0 | 8047 | `			break;` |
|      - | 8048 | `		}` |
|    107 | 8049 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 8050 | `			break;` |
|      - | 8051 | `		}` |
|      - | 8052 | `		/* Point to the next character */` |
|    101 | 8053 | `		zIn++;` |
|      1 | 8054 | `	}` |
|      - | 8055 | `	/* The test failed,return FALSE */` |
|      7 | 8056 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8057 | `	return PH7_OK;` |
|      9 | 8058 | `}` |
|      - | 8059 | `/*` |
|      - | 8060 | ` * bool ctype_print(string $text)` |
|      - | 8061 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 8062 | ` * Parameters` |
|      - | 8063 | ` *  $text` |
|      - | 8064 | ` *   The tested string.` |
|      - | 8065 | ` * Return` |
|      - | 8066 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 8067 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 8068 | ` *  or control function at all.` |
|      - | 8069 | ` */` |
|     16 | 8070 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8071 | `{` |
|      - | 8072 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8073 | `	int nLen;` |
|     17 | 8074 | `	if( nArg < 1 ){` |
|      - | 8075 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8076 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8077 | `		return PH7_OK;` |
|      - | 8078 | `	}` |
|      - | 8079 | `	/* Extract the target string */` |
|     17 | 8080 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8081 | `	zEnd = &zIn[nLen];` |
|     17 | 8082 | `	if( nLen < 1 ){` |
|      - | 8083 | `		/* Empty string,return FALSE */` |
|      3 | 8084 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8085 | `		return PH7_OK;` |
|      - | 8086 | `	}` |
|      - | 8087 | `	/* Perform the requested operation */` |
|     63 | 8088 | `	for(;;){` |
|    127 | 8089 | `		if( zIn >= zEnd ){` |
|      - | 8090 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8091 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8092 | `			return PH7_OK;` |
|      - | 8093 | `		}` |
|    119 | 8094 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8095 | `			/* UTF-8 stream  */` |
|    ! 0 | 8096 | `			break;` |
|      - | 8097 | `		}` |
|    119 | 8098 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 8099 | `			break;` |
|      - | 8100 | `		}` |
|      - | 8101 | `		/* Point to the next character */` |
|    113 | 8102 | `		zIn++;` |
|      1 | 8103 | `	}` |
|      - | 8104 | `	/* The test failed,return FALSE */` |
|      7 | 8105 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8106 | `	return PH7_OK;` |
|      9 | 8107 | `}` |
|      - | 8108 | `/*` |
|      - | 8109 | ` * bool ctype_punct(string $text)` |
|      - | 8110 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 8111 | ` * Parameters` |
|      - | 8112 | ` *  $text` |
|      - | 8113 | ` *   The tested string.` |
|      - | 8114 | ` * Return` |
|      - | 8115 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 8116 | ` *  digit or blank, FALSE otherwise.` |
|      - | 8117 | ` */` |
|     18 | 8118 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8119 | `{` |
|      - | 8120 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8121 | `	int nLen;` |
|     19 | 8122 | `	if( nArg < 1 ){` |
|      - | 8123 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8124 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8125 | `		return PH7_OK;` |
|      - | 8126 | `	}` |
|      - | 8127 | `	/* Extract the target string */` |
|     19 | 8128 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 8129 | `	zEnd = &zIn[nLen];` |
|     19 | 8130 | `	if( nLen < 1 ){` |
|      - | 8131 | `		/* Empty string,return FALSE */` |
|      3 | 8132 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8133 | `		return PH7_OK;` |
|      - | 8134 | `	}` |
|      - | 8135 | `	/* Perform the requested operation */` |
|     38 | 8136 | `	for(;;){` |
|     77 | 8137 | `		if( zIn >= zEnd ){` |
|      - | 8138 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8139 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8140 | `			return PH7_OK;` |
|      - | 8141 | `		}` |
|     69 | 8142 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8143 | `			/* UTF-8 stream  */` |
|    ! 0 | 8144 | `			break;` |
|      - | 8145 | `		}` |
|     69 | 8146 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 8147 | `			break;` |
|      - | 8148 | `		}` |
|      - | 8149 | `		/* Point to the next character */` |
|     61 | 8150 | `		zIn++;` |
|      1 | 8151 | `	}` |
|      - | 8152 | `	/* The test failed,return FALSE */` |
|      9 | 8153 | `	ph7_result_bool(pCtx,0);` |
|      9 | 8154 | `	return PH7_OK;` |
|     10 | 8155 | `}` |
|      - | 8156 | `/*` |
|      - | 8157 | ` * bool ctype_space(string $text)` |
|      - | 8158 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 8159 | ` * Parameters` |
|      - | 8160 | ` *  $text` |
|      - | 8161 | ` *   The tested string.` |
|      - | 8162 | ` * Return` |
|      - | 8163 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 8164 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 8165 | ` *  and form feed characters.` |
|      - | 8166 | ` */` |
|  62093 | 8167 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 8168 | `{` |
|      - | 8169 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8170 | `	int nLen;` |
|  62098 | 8171 | `	if( nArg < 1 ){` |
|      - | 8172 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8173 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8174 | `		return PH7_OK;` |
|      - | 8175 | `	}` |
|      - | 8176 | `	/* Extract the target string */` |
|  62098 | 8177 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  62098 | 8178 | `	zEnd = &zIn[nLen];` |
|  62098 | 8179 | `	if( nLen < 1 ){` |
|      - | 8180 | `		/* Empty string,return FALSE */` |
|      3 | 8181 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8182 | `		return PH7_OK;` |
|      - | 8183 | `	}` |
|      - | 8184 | `	/* Perform the requested operation */` |
|  32152 | 8185 | `	for(;;){` |
|  64224 | 8186 | `		if( zIn >= zEnd ){` |
|      - | 8187 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   2109 | 8188 | `			ph7_result_bool(pCtx,1);` |
|   2109 | 8189 | `			return PH7_OK;` |
|      - | 8190 | `		}` |
|  62120 | 8191 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8192 | `			/* UTF-8 stream  */` |
|    ! 0 | 8193 | `			break;` |
|      - | 8194 | `		}` |
|  62120 | 8195 | `		if( !SyisSpace(zIn[0]) ){` |
|  59992 | 8196 | `			break;` |
|      - | 8197 | `		}` |
|      - | 8198 | `		/* Point to the next character */` |
|   2133 | 8199 | `		zIn++;` |
|      5 | 8200 | `	}` |
|      - | 8201 | `	/* The test failed,return FALSE */` |
|  59992 | 8202 | `	ph7_result_bool(pCtx,0);` |
|  59992 | 8203 | `	return PH7_OK;` |
|  31094 | 8204 | `}` |
|      - | 8205 | `/*` |
|      - | 8206 | ` * bool ctype_lower(string $text)` |
|      - | 8207 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 8208 | ` * Parameters` |
|      - | 8209 | ` *  $text` |
|      - | 8210 | ` *   The tested string.` |
|      - | 8211 | ` * Return` |
|      - | 8212 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 8213 | ` */` |
|     16 | 8214 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8215 | `{` |
|      - | 8216 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8217 | `	int nLen;` |
|     17 | 8218 | `	if( nArg < 1 ){` |
|      - | 8219 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8220 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8221 | `		return PH7_OK;` |
|      - | 8222 | `	}` |
|      - | 8223 | `	/* Extract the target string */` |
|     17 | 8224 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8225 | `	zEnd = &zIn[nLen];` |
|     17 | 8226 | `	if( nLen < 1 ){` |
|      - | 8227 | `		/* Empty string,return FALSE */` |
|      3 | 8228 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8229 | `		return PH7_OK;` |
|      - | 8230 | `	}` |
|      - | 8231 | `	/* Perform the requested operation */` |
|     27 | 8232 | `	for(;;){` |
|     55 | 8233 | `		if( zIn >= zEnd ){` |
|      - | 8234 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 8235 | `			ph7_result_bool(pCtx,1);` |
|      5 | 8236 | `			return PH7_OK;` |
|      - | 8237 | `		}` |
|     51 | 8238 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 8239 | `			break;` |
|      - | 8240 | `		}` |
|      - | 8241 | `		/* Point to the next character */` |
|     41 | 8242 | `		zIn++;` |
|      1 | 8243 | `	}` |
|      - | 8244 | `	/* The test failed,return FALSE */` |
|     11 | 8245 | `	ph7_result_bool(pCtx,0);` |
|     11 | 8246 | `	return PH7_OK;` |
|      9 | 8247 | `}` |
|      - | 8248 | `/*` |
|      - | 8249 | ` * bool ctype_upper(string $text)` |
|      - | 8250 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 8251 | ` * Parameters` |
|      - | 8252 | ` *  $text` |
|      - | 8253 | ` *   The tested string.` |
|      - | 8254 | ` * Return` |
|      - | 8255 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 8256 | ` */` |
|     16 | 8257 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8258 | `{` |
|      - | 8259 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8260 | `	int nLen;` |
|     17 | 8261 | `	if( nArg < 1 ){` |
|      - | 8262 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8263 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8264 | `		return PH7_OK;` |
|      - | 8265 | `	}` |
|      - | 8266 | `	/* Extract the target string */` |
|     17 | 8267 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8268 | `	zEnd = &zIn[nLen];` |
|     17 | 8269 | `	if( nLen < 1 ){` |
|      - | 8270 | `		/* Empty string,return FALSE */` |
|      3 | 8271 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8272 | `		return PH7_OK;` |
|      - | 8273 | `	}` |
|      - | 8274 | `	/* Perform the requested operation */` |
|     28 | 8275 | `	for(;;){` |
|     57 | 8276 | `		if( zIn >= zEnd ){` |
|      - | 8277 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 8278 | `			ph7_result_bool(pCtx,1);` |
|      5 | 8279 | `			return PH7_OK;` |
|      - | 8280 | `		}` |
|     53 | 8281 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 8282 | `			break;` |
|      - | 8283 | `		}` |
|      - | 8284 | `		/* Point to the next character */` |
|     43 | 8285 | `		zIn++;` |
|      1 | 8286 | `	}` |
|      - | 8287 | `	/* The test failed,return FALSE */` |
|     11 | 8288 | `	ph7_result_bool(pCtx,0);` |
|     11 | 8289 | `	return PH7_OK;` |
|      9 | 8290 | `}` |
|      - | 8291 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 8292 | `/*` |
|      - | 8293 | ` * Section:` |
|      - | 8294 | ` *    URL handling Functions.` |
|      - | 8295 | ` * Status:` |
|      - | 8296 | ` *    Stable.` |
|      - | 8297 | ` */` |
|      - | 8298 | `/*` |
|      - | 8299 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 8300 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 8301 | ` */` |
|   1026 | 8302 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 8303 | `{` |
|      - | 8304 | `	/* Store in the call context result buffer */` |
|   1028 | 8305 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 8306 | `	return SXRET_OK;` |
|      2 | 8307 | `}` |
|      - | 8308 | `/*` |
|      - | 8309 | ` * string base64_encode(string $data)` |
|      - | 8310 | ` * string convert_uuencode(string $data)` |
|      - | 8311 | ` *  Encodes data with MIME base64` |
|      - | 8312 | ` * Parameter` |
|      - | 8313 | ` *  $data` |
|      - | 8314 | ` *    Data to encode` |
|      - | 8315 | ` * Return` |
|      - | 8316 | ` *  Encoded data or FALSE on failure.` |
|      - | 8317 | ` */` |
|      6 | 8318 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8319 | `{` |
|      - | 8320 | `	const char *zIn;` |
|      - | 8321 | `	int nLen;` |
|      7 | 8322 | `	if( nArg < 1 ){` |
|      - | 8323 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8324 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8325 | `		return PH7_OK;` |
|      - | 8326 | `	}` |
|      - | 8327 | `	/* Extract the input string */` |
|      7 | 8328 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8329 | `	if( nLen < 1 ){` |
|      - | 8330 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8331 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8332 | `		return PH7_OK;` |
|      - | 8333 | `	}` |
|      - | 8334 | `	/* Perform the BASE64 encoding */` |
|      7 | 8335 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 8336 | `	return PH7_OK;` |
|      4 | 8337 | `}` |
|      - | 8338 | `/*` |
|      - | 8339 | ` * string base64_decode(string $data)` |
|      - | 8340 | ` * string convert_uudecode(string $data)` |
|      - | 8341 | ` *  Decodes data encoded with MIME base64` |
|      - | 8342 | ` * Parameter` |
|      - | 8343 | ` *  $data` |
|      - | 8344 | ` *    Encoded data.` |
|      - | 8345 | ` * Return` |
|      - | 8346 | ` *  Returns the original data or FALSE on failure.` |
|      - | 8347 | ` */` |
|     34 | 8348 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 8349 | `{` |
|      - | 8350 | `	const char *zIn;` |
|      - | 8351 | `	int nLen;` |
|     36 | 8352 | `	if( nArg < 1 ){` |
|      - | 8353 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8354 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8355 | `		return PH7_OK;` |
|      - | 8356 | `	}` |
|      - | 8357 | `	/* Extract the input string */` |
|     36 | 8358 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 8359 | `	if( nLen < 1 ){` |
|      - | 8360 | `		/* Nothing to process,return FALSE */` |
|      3 | 8361 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8362 | `		return PH7_OK;` |
|      - | 8363 | `	}` |
|      - | 8364 | `	/* Perform the BASE64 decoding */` |
|     34 | 8365 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 8366 | `	return PH7_OK;` |
|     19 | 8367 | `}` |
|      - | 8368 | `/*` |
|      - | 8369 | ` * string urlencode(string $str)` |
|      - | 8370 | ` *  URL encoding` |
|      - | 8371 | ` * Parameter` |
|      - | 8372 | ` *  $data` |
|      - | 8373 | ` *   Input string.` |
|      - | 8374 | ` * Return` |
|      - | 8375 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 8376 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 8377 | ` *  encoded as plus (+) signs.` |
|      - | 8378 | ` */` |
|      4 | 8379 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8380 | `{` |
|      - | 8381 | `	const char *zIn;` |
|      - | 8382 | `	int nLen;` |
|      5 | 8383 | `	if( nArg < 1 ){` |
|      - | 8384 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8385 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8386 | `		return PH7_OK;` |
|      - | 8387 | `	}` |
|      - | 8388 | `	/* Extract the input string */` |
|      5 | 8389 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 8390 | `	if( nLen < 1 ){` |
|      - | 8391 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8392 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8393 | `		return PH7_OK;` |
|      - | 8394 | `	}` |
|      - | 8395 | `	/* Perform the URL encoding */` |
|      5 | 8396 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 8397 | `	return PH7_OK;` |
|      3 | 8398 | `}` |
|      - | 8399 | `/*` |
|      - | 8400 | ` * string urldecode(string $str)` |
|      - | 8401 | ` *  Decodes any %## encoding in the given string.` |
|      - | 8402 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 8403 | ` * Parameter` |
|      - | 8404 | ` *  $data` |
|      - | 8405 | ` *    Input string.` |
|      - | 8406 | ` * Return` |
|      - | 8407 | ` *  Decoded URL or FALSE on failure.` |
|      - | 8408 | ` */` |
|      6 | 8409 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8410 | `{` |
|      - | 8411 | `	const char *zIn;` |
|      - | 8412 | `	int nLen;` |
|      7 | 8413 | `	if( nArg < 1 ){` |
|      - | 8414 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8415 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8416 | `		return PH7_OK;` |
|      - | 8417 | `	}` |
|      - | 8418 | `	/* Extract the input string */` |
|      7 | 8419 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 8420 | `	if( nLen < 1 ){` |
|      - | 8421 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 8422 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8423 | `		return PH7_OK;` |
|      - | 8424 | `	}` |
|      - | 8425 | `	/* Perform the URL decoding */` |
|      7 | 8426 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 8427 | `	return PH7_OK;` |
|      4 | 8428 | `}` |
|      - | 8429 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8430 | `/* Table of the built-in functions */` |
|      - | 8431 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 8432 | `	   /* Variable handling functions */` |
|      - | 8433 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 8434 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 8435 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 8436 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 8437 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 8438 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 8439 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 8440 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 8441 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 8442 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 8443 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 8444 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 8445 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 8446 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 8447 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 8448 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 8449 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 8450 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 8451 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 8452 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 8453 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8454 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 8455 | `	   /* Math functions */` |
|      - | 8456 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 8457 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 8458 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 8459 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 8460 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 8461 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 8462 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 8463 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 8464 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 8465 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 8466 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 8467 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 8468 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 8469 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 8470 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 8471 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 8472 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 8473 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 8474 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 8475 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 8476 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 8477 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 8478 | `	{ "round",    PH7_builtin_round        },` |
|      - | 8479 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 8480 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 8481 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 8482 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 8483 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 8484 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 8485 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 8486 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 8487 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 8488 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8489 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8490 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 8491 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8492 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8493 | `	   /* String handling functions */` |
|      - | 8494 |  |
|      - | 8495 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 8496 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 8497 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 8498 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 8499 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 8500 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 8501 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 8502 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 8503 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 8504 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 8505 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 8506 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 8507 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 8508 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 8509 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 8510 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 8511 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 8512 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 8513 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 8514 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 8515 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 8516 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 8517 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 8518 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 8519 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 8520 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 8521 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 8522 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 8523 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 8524 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 8525 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 8526 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 8527 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 8528 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 8529 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 8530 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 8531 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 8532 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 8533 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 8534 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 8535 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 8536 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 8537 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 8538 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 8539 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 8540 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 8541 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 8542 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 8543 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 8544 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 8545 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 8546 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 8547 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8548 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8549 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 8550 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 8551 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 8552 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 8553 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8554 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8555 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 8556 |  |
|      - | 8557 |  |
|      - | 8558 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 8559 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 8560 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 8561 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 8562 | `	{ "hash",         PH7_builtin_hash      },` |
|      - | 8563 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|      - | 8564 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|      - | 8565 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|      - | 8566 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 8567 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|      - | 8568 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|      - | 8569 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|      - | 8570 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|      - | 8571 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|      - | 8572 | `	{ "filter_input",          PH7_builtin_filter_input },` |
|      - | 8573 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8574 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8575 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 8576 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 8577 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8578 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8579 |  |
|      - | 8580 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 8581 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 8582 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 8583 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 8584 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 8585 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 8586 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 8587 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 8588 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 8589 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 8590 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 8591 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 8592 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8593 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8594 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 8595 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8596 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8597 |  |
|      - | 8598 | `	         /* Ctype functions */` |
|      - | 8599 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 8600 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 8601 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 8602 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 8603 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 8604 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 8605 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 8606 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 8607 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 8608 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 8609 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 8610 | `	         /* Time functions */` |
|      - | 8611 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 8612 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 8613 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 8614 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 8615 | `	{ "date",        PH7_builtin_date         },` |
|      - | 8616 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 8617 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 8618 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 8619 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 8620 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 8621 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 8622 | `	        /* URL functions */` |
|      - | 8623 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 8624 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 8625 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 8626 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 8627 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 8628 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 8629 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 8630 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 8631 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8632 | `};` |
|      - | 8633 | `/*` |
|      - | 8634 | ` * Register the built-in functions defined above,the array functions` |
|      - | 8635 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 8636 | ` */` |
|   3472 | 8637 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      5 | 8638 | `{` |
|      - | 8639 | `	sxu32 n;` |
| 583301 | 8640 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 579829 | 8641 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 289917 | 8642 | `	}` |
|      - | 8643 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   3477 | 8644 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 8645 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   3477 | 8646 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   3477 | 8647 | `}` |
|      - | 8648 |  |
