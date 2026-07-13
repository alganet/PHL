# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4265/4950 lines (86.16%)

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
|     50 |   30 | `static int PH7_builtin_is_bool(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   31 | `{` |
|     51 |   32 | `	int res = 0; /* Assume false by default */` |
|     51 |   33 | `	if( nArg > 0 ){` |
|     51 |   34 | `		res = ph7_value_is_bool(apArg[0]);` |
|     25 |   35 | `	}` |
|      - |   36 | `	/* Query result */` |
|     51 |   37 | `	ph7_result_bool(pCtx,res);` |
|     51 |   38 | `	return PH7_OK;` |
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
|    254 |   50 | `static int PH7_builtin_is_float(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   51 | `{` |
|    255 |   52 | `	int res = 0; /* Assume false by default */` |
|    255 |   53 | `	if( nArg > 0 ){` |
|    255 |   54 | `		res = ph7_value_is_float(apArg[0]);` |
|    127 |   55 | `	}` |
|      - |   56 | `	/* Query result */` |
|    255 |   57 | `	ph7_result_bool(pCtx,res);` |
|    255 |   58 | `	return PH7_OK;` |
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
|    772 |   70 | `static int PH7_builtin_is_int(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 |   71 | `{` |
|    775 |   72 | `	int res = 0; /* Assume false by default */` |
|    775 |   73 | `	if( nArg > 0 ){` |
|      - |   74 | `		/* Strict PHP identity: a float is never an int, even when it holds an` |
|      - |   75 | `		 * integer value (1.0). An integer-valued real carries both MEMOBJ_INT` |
|      - |   76 | `		 * (cached) and MEMOBJ_REAL, so REAL must be excluded here. */` |
|    775 |   77 | `		res = ph7_value_is_int(apArg[0]) && !ph7_value_is_float(apArg[0]);` |
|    386 |   78 | `	}` |
|      - |   79 | `	/* Query result */` |
|    775 |   80 | `	ph7_result_bool(pCtx,res);` |
|    775 |   81 | `	return PH7_OK;` |
|      3 |   82 | `}` |
|      - |   83 | `/*` |
|      - |   84 | ` * bool is_string($var)` |
|      - |   85 | ` *  Finds out whether a variable is a string.` |
|      - |   86 | ` * Parameters` |
|      - |   87 | ` *   $var: The variable being evaluated.` |
|      - |   88 | ` * Return` |
|      - |   89 | ` *  TRUE if var is string. False otherwise.` |
|      - |   90 | ` */` |
|    494 |   91 | `static int PH7_builtin_is_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |   92 | `{` |
|    495 |   93 | `	int res = 0; /* Assume false by default */` |
|    495 |   94 | `	if( nArg > 0 ){` |
|    495 |   95 | `		res = ph7_value_is_string(apArg[0]);` |
|    247 |   96 | `	}` |
|      - |   97 | `	/* Query result */` |
|    495 |   98 | `	ph7_result_bool(pCtx,res);` |
|    495 |   99 | `	return PH7_OK;` |
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
|     60 |  127 | `static int PH7_builtin_is_numeric(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  128 | `{` |
|     65 |  129 | `	int res = 0; /* Assume false by default */` |
|     65 |  130 | `	if( nArg > 0 ){` |
|     65 |  131 | `		res = ph7_value_is_numeric(apArg[0]);` |
|     30 |  132 | `	}` |
|      - |  133 | `	/* Query result */` |
|     65 |  134 | `	ph7_result_bool(pCtx,res);` |
|     65 |  135 | `	return PH7_OK;` |
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
|    386 |  163 | `static int PH7_builtin_is_array(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  164 | `{` |
|    391 |  165 | `	int res = 0; /* Assume false by default */` |
|    391 |  166 | `	if( nArg > 0 ){` |
|    391 |  167 | `		res = ph7_value_is_array(apArg[0]);` |
|    193 |  168 | `	}` |
|      - |  169 | `	/* Query result */` |
|    391 |  170 | `	ph7_result_bool(pCtx,res);` |
|    391 |  171 | `	return PH7_OK;` |
|      5 |  172 | `}` |
|      - |  173 | `/*` |
|      - |  174 | ` * bool is_object($var)` |
|      - |  175 | ` *  Find out whether a variable is an object.` |
|      - |  176 | ` * Parameters` |
|      - |  177 | ` *  $var: The variable being evaluated.` |
|      - |  178 | ` * Return` |
|      - |  179 | ` *  True if var is an object. False otherwise.` |
|      - |  180 | ` */` |
|    312 |  181 | `static int PH7_builtin_is_object(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  182 | `{` |
|    313 |  183 | `	int res = 0; /* Assume false by default */` |
|    313 |  184 | `	if( nArg > 0 ){` |
|    313 |  185 | `		res = ph7_value_is_object(apArg[0]);` |
|    156 |  186 | `	}` |
|      - |  187 | `	/* Query result */` |
|    313 |  188 | `	ph7_result_bool(pCtx,res);` |
|    313 |  189 | `	return PH7_OK;` |
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
|     46 |  237 | `static int PH7_builtin_intval(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  238 | `{` |
|     47 |  239 | `	if( nArg < 1 ){` |
|      - |  240 | `		/* return 0 */` |
|    ! 0 |  241 | `		ph7_result_int(pCtx,0);` |
|    ! 0 |  242 | `	}else{` |
|      - |  243 | `		sxi64 iVal;` |
|      - |  244 | `		/* Perform the cast */` |
|     47 |  245 | `		iVal = ph7_value_to_int64(apArg[0]);` |
|     47 |  246 | `		ph7_result_int64(pCtx,iVal);` |
|      - |  247 | `	}` |
|     47 |  248 | `	return PH7_OK;` |
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
|  33872 |  303 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  304 | `{` |
|  33877 |  305 | `	int res = 1; /* Assume empty by default */` |
|  33877 |  306 | `	if( nArg > 0 ){` |
|  33875 |  307 | `		res = ph7_value_is_empty(apArg[0]);` |
|  16935 |  308 | `	}` |
|  33877 |  309 | `	ph7_result_bool(pCtx,res);` |
|  33877 |  310 | `	return PH7_OK;` |
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
| 223092 |  353 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  354 | `{` |
|      - |  355 | `	const char *zSource,*zOfft;` |
|      - |  356 | `	int nOfft,nLen,nSrcLen;` |
| 223097 |  357 | `	if( nArg < 2 ){` |
|      - |  358 | `		/* return FALSE */` |
|    ! 0 |  359 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  360 | `		return PH7_OK;` |
|      - |  361 | `	}` |
|      - |  362 | `	/* Extract the target string */` |
| 223097 |  363 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 223097 |  364 | `	if( nSrcLen < 1 ){` |
|      - |  365 | `		/* Empty string,return FALSE */` |
|  12085 |  366 | `		ph7_result_bool(pCtx,0);` |
|  12085 |  367 | `		return PH7_OK;` |
|      - |  368 | `	}` |
| 211017 |  369 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  370 | `	/* Extract the offset */` |
| 211017 |  371 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 211017 |  372 | `	if( nOfft < 0 ){` |
|  32671 |  373 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  32671 |  374 | `		if( zOfft < zSource ){` |
|      - |  375 | `			/* Invalid offset */` |
|      5 |  376 | `			ph7_result_bool(pCtx,0);` |
|      5 |  377 | `			return PH7_OK;` |
|      - |  378 | `		}` |
|  32667 |  379 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  32667 |  380 | `		nOfft = (int)(zOfft-zSource);` |
| 194682 |  381 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  382 | `		/* Invalid offset */` |
|    217 |  383 | `		ph7_result_bool(pCtx,0);` |
|    217 |  384 | `		return PH7_OK;` |
|    ! 0 |  385 | `	}else{` |
| 178139 |  386 | `		zOfft = &zSource[nOfft];` |
| 178139 |  387 | `		nLen = nSrcLen - nOfft;` |
|      - |  388 | `	}` |
| 210801 |  389 | `	if( nArg > 2 ){` |
|      - |  390 | `		/* Extract the length */` |
| 173743 |  391 | `		nLen = ph7_value_to_int(apArg[2]);` |
| 173743 |  392 | `		if( nLen == 0 ){` |
|      - |  393 | `			/* Invalid length,return an empty string */` |
|      5 |  394 | `			ph7_result_string(pCtx,"",0);` |
|      5 |  395 | `			return PH7_OK;` |
| 173739 |  396 | `		}else if( nLen < 0 ){` |
|  32659 |  397 | `			nLen = nSrcLen + nLen - nOfft;` |
|  32659 |  398 | `			if( nLen < 1 ){` |
|      - |  399 | `				/* Invalid  length */` |
|      3 |  400 | `				nLen = nSrcLen - nOfft;` |
|      1 |  401 | `			}` |
|  16327 |  402 | `		}` |
| 173739 |  403 | `		if( nLen + nOfft > nSrcLen ){` |
|      - |  404 | `			/* Invalid length */` |
|   5571 |  405 | `			nLen = nSrcLen - nOfft;` |
|   2783 |  406 | `		}` |
|  86867 |  407 | `	}` |
|      - |  408 | `	/* Return the substring */` |
| 210797 |  409 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 210797 |  410 | `	return PH7_OK;` |
| 111551 |  411 | `}` |
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
|      - |  595 | `/* Forward declarations: defined with the trim/addcslashes and str_contains` |
|      - |  596 | ` * families below. */` |
|      - |  597 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256]);` |
|      - |  598 | `static sxi32 StrPredicateResolveArg(ph7_context *pCtx,ph7_value *pArg,const char *zFunc,` |
|      - |  599 | `	int iArgNum,const char *zParamName,const char *zTypeStr,const char *zNullMsg,` |
|      - |  600 | `	ph7_value *pTmp,const char **pzOut,int *pnOut);` |
|      - |  601 | `/*` |
|      - |  602 | ` * Emit a formatted E_DEPRECATED diagnostic WITHOUT the active-function-name` |
|      - |  603 | ` * prefix that ph7_context_throw_error_format() prepends — php's implicit-` |
|      - |  604 | ` * conversion notices carry no prefix, and the ZPP null notices embed the` |
|      - |  605 | ` * function name mid-message themselves.` |
|      - |  606 | ` */` |
|    ! 0 |  607 | `static void BuiltinThrowDeprecatedFmt(ph7_vm *pVm,const char *zFmt,...)` |
|    ! 0 |  608 | `{` |
|      - |  609 | `	va_list ap;` |
|    ! 0 |  610 | `	va_start(ap,zFmt);` |
|    ! 0 |  611 | `	PH7_VmThrowErrorAp(pVm,0,E_DEPRECATED,zFmt,ap);` |
|    ! 0 |  612 | `	va_end(ap);` |
|    ! 0 |  613 | `}` |
|      - |  614 | `/*` |
|      - |  615 | ` * Validate and resolve an int-typed builtin parameter with php-8 ZPP weak-mode` |
|      - |  616 | ` * semantics: ints and bools pass through; null emits the 8.1 deprecation and` |
|      - |  617 | ` * resolves to 0; floats and float-strings convert, with the implicit-conversion` |
|      - |  618 | ` * E_DEPRECATED when lossy and a TypeError when NAN/INF/out of int range;` |
|      - |  619 | ` * integral numeric strings convert exactly; everything else (arrays, resources,` |
|      - |  620 | ` * objects, non-numeric strings) is a TypeError naming zTypeStr (e.g. "int",` |
|      - |  621 | ` * "array\|int"). Returns PH7_OK with *pOut set, or the throw status.` |
|      - |  622 | ` */` |
|    150 |  623 | `static sxi32 IntArgResolve(` |
|      - |  624 | `	ph7_context *pCtx,` |
|      - |  625 | `	ph7_value *pArg,` |
|      - |  626 | `	const char *zFunc,` |
|      - |  627 | `	int iArgNum,` |
|      - |  628 | `	const char *zParamName,` |
|      - |  629 | `	const char *zTypeStr,` |
|      - |  630 | `	sxi64 *pOut` |
|      1 |  631 | `){` |
|    151 |  632 | `	if( ph7_value_is_null(pArg) ){` |
|    ! 0 |  633 | `		BuiltinThrowDeprecatedFmt(pCtx->pVm,` |
|      - |  634 | `			"%s(): Passing null to parameter #%d (%s) of type %s is deprecated",` |
|    ! 0 |  635 | `			zFunc,iArgNum,zParamName,zTypeStr` |
|      - |  636 | `			);` |
|    ! 0 |  637 | `		*pOut = 0;` |
|    ! 0 |  638 | `		return PH7_OK;` |
|      - |  639 | `	}` |
|    151 |  640 | `	if( ph7_value_is_float(pArg) ){` |
|      5 |  641 | `		double dVal = ph7_value_to_double(pArg);` |
|      - |  642 | `		sxi64 iVal;` |
|      - |  643 | `		/* php: NAN/INF/out-of-int64-range floats fail ZPP outright */` |
|      5 |  644 | `		if( dVal != dVal \|\| dVal >= 9223372036854775808.0 \|\| dVal < -9223372036854775808.0 ){` |
|      7 |  645 | `			return PH7_VmThrowException(pCtx,` |
|      - |  646 | `				"TypeError",` |
|      - |  647 | `				"%s(): Argument #%d (%s) must be of type %s, float given",` |
|      2 |  648 | `				zFunc,iArgNum,zParamName,zTypeStr` |
|      - |  649 | `				);` |
|      - |  650 | `		}` |
|    ! 0 |  651 | `		iVal = (sxi64)dVal;` |
|    ! 0 |  652 | `		if( (double)iVal != dVal ){` |
|    ! 0 |  653 | `			BuiltinThrowDeprecatedFmt(pCtx->pVm,` |
|      - |  654 | `				"Implicit conversion from float %s to int loses precision",` |
|    ! 0 |  655 | `				ph7_value_to_string(pArg,0)` |
|      - |  656 | `				);` |
|    ! 0 |  657 | `		}` |
|    ! 0 |  658 | `		*pOut = iVal;` |
|    ! 0 |  659 | `		return PH7_OK;` |
|      - |  660 | `	}` |
|    147 |  661 | `	if( ph7_value_is_string(pArg) ){` |
|      - |  662 | `		const char *zNum;` |
|      - |  663 | `		int nSlen;` |
|     15 |  664 | `		int i,bFloat = 0;` |
|     15 |  665 | `		if( !PH7_MemObjStringIsNumeric(pArg) ){` |
|     16 |  666 | `			return PH7_VmThrowException(pCtx,` |
|      - |  667 | `				"TypeError",` |
|      - |  668 | `				"%s(): Argument #%d (%s) must be of type %s, string given",` |
|      5 |  669 | `				zFunc,iArgNum,zParamName,zTypeStr` |
|      - |  670 | `				);` |
|      - |  671 | `		}` |
|      5 |  672 | `		zNum = ph7_value_to_string(pArg,&nSlen);` |
|      9 |  673 | `		for( i = 0 ; i < nSlen ; i++ ){` |
|      5 |  674 | `			if( zNum[i] == '.' \|\| zNum[i] == 'e' \|\| zNum[i] == 'E' ){` |
|    ! 0 |  675 | `				bFloat = 1;` |
|    ! 0 |  676 | `				break;` |
|      - |  677 | `			}` |
|      3 |  678 | `		}` |
|      5 |  679 | `		if( bFloat ){` |
|    ! 0 |  680 | `			double dVal = 0;` |
|      - |  681 | `			sxi64 iVal;` |
|    ! 0 |  682 | `			SyStrToReal(zNum,(sxu32)nSlen,(void *)&dVal,0);` |
|    ! 0 |  683 | `			if( dVal != dVal \|\| dVal >= 9223372036854775808.0 \|\| dVal < -9223372036854775808.0 ){` |
|    ! 0 |  684 | `				return PH7_VmThrowException(pCtx,` |
|      - |  685 | `					"TypeError",` |
|      - |  686 | `					"%s(): Argument #%d (%s) must be of type %s, string given",` |
|    ! 0 |  687 | `					zFunc,iArgNum,zParamName,zTypeStr` |
|      - |  688 | `					);` |
|      - |  689 | `			}` |
|    ! 0 |  690 | `			iVal = (sxi64)dVal;` |
|    ! 0 |  691 | `			if( (double)iVal != dVal ){` |
|    ! 0 |  692 | `				BuiltinThrowDeprecatedFmt(pCtx->pVm,` |
|      - |  693 | `					"Implicit conversion from float-string \"%s\" to int loses precision",` |
|    ! 0 |  694 | `					zNum` |
|      - |  695 | `					);` |
|    ! 0 |  696 | `			}` |
|    ! 0 |  697 | `			*pOut = iVal;` |
|    ! 0 |  698 | `			return PH7_OK;` |
|      - |  699 | `		}` |
|      5 |  700 | `		*pOut = ph7_value_to_int64(pArg);` |
|      5 |  701 | `		return PH7_OK;` |
|      - |  702 | `	}` |
|    133 |  703 | `	if( !ph7_value_is_int(pArg) && !ph7_value_is_bool(pArg) ){` |
|      - |  704 | `		/* Arrays, resources and objects: php names the class for objects */` |
|      5 |  705 | `		const char *zType = ph7_type_name(pArg);` |
|      5 |  706 | `		if( ph7_value_is_object(pArg) ){` |
|      3 |  707 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|      3 |  708 | `			if( pInst && pInst->pClass ){` |
|      3 |  709 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      1 |  710 | `			}` |
|      1 |  711 | `		}` |
|      7 |  712 | `		return PH7_VmThrowException(pCtx,` |
|      - |  713 | `			"TypeError",` |
|      - |  714 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|      2 |  715 | `			zFunc,iArgNum,zParamName,zTypeStr,zType` |
|      - |  716 | `			);` |
|      - |  717 | `	}` |
|    129 |  718 | `	*pOut = ph7_value_to_int64(pArg);` |
|    129 |  719 | `	return PH7_OK;` |
|     76 |  720 | `}` |
|      - |  721 | `/*` |
|      - |  722 | ` * Normalize a substr_replace() offset/length pair against a string of nStrLen` |
|      - |  723 | ` * bytes, exactly like PHP: a negative offset counts from the end (clamped to 0),` |
|      - |  724 | ` * an offset past the end clamps to the end; a negative length leaves that many` |
|      - |  725 | ` * bytes off the end of the remaining region (clamped to 0), and the length is` |
|      - |  726 | ` * finally clamped to the remaining region. Written without f+l additions so an` |
|      - |  727 | ` * INT64_MAX length cannot overflow.` |
|      - |  728 | ` */` |
|     60 |  729 | `static void SubstrReplaceWindow(sxi64 *pF,sxi64 *pL,int nStrLen)` |
|      1 |  730 | `{` |
|     61 |  731 | `	sxi64 f = *pF,l = *pL;` |
|     61 |  732 | `	if( f < 0 ){` |
|      9 |  733 | `		f += nStrLen;` |
|      9 |  734 | `		if( f < 0 ){` |
|      5 |  735 | `			f = 0;` |
|      3 |  736 | `		}` |
|     57 |  737 | `	}else if( f > nStrLen ){` |
|      5 |  738 | `		f = nStrLen;` |
|      2 |  739 | `	}` |
|     61 |  740 | `	if( l < 0 ){` |
|      7 |  741 | `		l += nStrLen - f;` |
|      7 |  742 | `		if( l < 0 ){` |
|      5 |  743 | `			l = 0;` |
|      2 |  744 | `		}` |
|      3 |  745 | `	}` |
|     61 |  746 | `	if( l > nStrLen - f ){` |
|     25 |  747 | `		l = nStrLen - f;` |
|     12 |  748 | `	}` |
|     61 |  749 | `	*pF = f;` |
|     61 |  750 | `	*pL = l;` |
|     61 |  751 | `}` |
|      - |  752 | `/* A replacement string collected out of substr_replace()'s $replace array.` |
|      - |  753 | ` * The bytes live in a shared pool blob (walker values are transient), so the` |
|      - |  754 | ` * item stores pool offsets, mirroring the strtr_entry technique. */` |
|      - |  755 | `typedef struct substr_repl_item substr_repl_item;` |
|      - |  756 | `struct substr_repl_item` |
|      - |  757 | `{` |
|      - |  758 | `	sxu32 nOfft; /* Offset of the string inside the pool */` |
|      - |  759 | `	sxu32 nLen;  /* Length of the string */` |
|      - |  760 | `};` |
|      - |  761 | `typedef struct substr_replace_collect substr_replace_collect;` |
|      - |  762 | `struct substr_replace_collect` |
|      - |  763 | `{` |
|      - |  764 | `	SyBlob *pPool;  /* Byte pool for string items (string walker only) */` |
|      - |  765 | `	SySet *pSet;    /* substr_repl_item set (string) or sxi64 set (int) */` |
|      - |  766 | `	sxi32 rc;       /* SXRET_OK or SXERR_MEM on collector failure */` |
|      - |  767 | `};` |
|      - |  768 | `/* ph7_array_walk() callback: append one $replace element to the pool. */` |
|      6 |  769 | `static int SubstrReplaceStrWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  770 | `{` |
|      7 |  771 | `	substr_replace_collect *pCol = (substr_replace_collect *)pUserData;` |
|      - |  772 | `	substr_repl_item sItem;` |
|      - |  773 | `	const char *zStr;` |
|      - |  774 | `	int nLen;` |
|      3 |  775 | `	SXUNUSED(pKey);` |
|      7 |  776 | `	zStr = ph7_value_to_string(pData,&nLen);` |
|      7 |  777 | `	sItem.nOfft = SyBlobLength(pCol->pPool);` |
|      7 |  778 | `	sItem.nLen = (sxu32)nLen;` |
|      7 |  779 | `	if( nLen > 0 && SXRET_OK != SyBlobAppend(pCol->pPool,(const void *)zStr,(sxu32)nLen) ){` |
|    ! 0 |  780 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  781 | `		return SXERR_ABORT;` |
|      - |  782 | `	}` |
|      7 |  783 | `	if( SXRET_OK != SySetPut(pCol->pSet,(const void *)&sItem) ){` |
|    ! 0 |  784 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  785 | `		return SXERR_ABORT;` |
|      - |  786 | `	}` |
|      7 |  787 | `	return PH7_OK;` |
|      4 |  788 | `}` |
|      - |  789 | `/* ph7_array_walk() callback: collect one $offset/$length element as an int. */` |
|     12 |  790 | `static int SubstrReplaceIntWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  791 | `{` |
|     13 |  792 | `	substr_replace_collect *pCol = (substr_replace_collect *)pUserData;` |
|     13 |  793 | `	sxi64 iVal = ph7_value_to_int64(pData);` |
|      6 |  794 | `	SXUNUSED(pKey);` |
|     13 |  795 | `	if( SXRET_OK != SySetPut(pCol->pSet,(const void *)&iVal) ){` |
|    ! 0 |  796 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  797 | `		return SXERR_ABORT;` |
|      - |  798 | `	}` |
|     13 |  799 | `	return PH7_OK;` |
|      7 |  800 | `}` |
|      - |  801 | `/* Per-element state while walking substr_replace()'s array $string. */` |
|      - |  802 | `typedef struct substr_replace_ctx substr_replace_ctx;` |
|      - |  803 | `struct substr_replace_ctx` |
|      - |  804 | `{` |
|      - |  805 | `	ph7_value *pResult;   /* Result array (keys preserved) */` |
|      - |  806 | `	ph7_value *pScratch;  /* Reusable string value for each element */` |
|      - |  807 | `	SyBlob *pReplPool;    /* Pool behind aRepl items */` |
|      - |  808 | `	SySet *pRepl;         /* substr_repl_item set or NULL when $replace is scalar */` |
|      - |  809 | `	SySet *pFrom;         /* sxi64 set or NULL when $offset is scalar */` |
|      - |  810 | `	SySet *pLen;          /* sxi64 set or NULL when $length is scalar/absent */` |
|      - |  811 | `	sxu32 iReplCur;       /* Next-position cursors into the three sets */` |
|      - |  812 | `	sxu32 iFromCur;` |
|      - |  813 | `	sxu32 iLenCur;` |
|      - |  814 | `	const char *zRepl;    /* Scalar $replace */` |
|      - |  815 | `	int nRepl;` |
|      - |  816 | `	sxi64 iFrom;          /* Scalar $offset */` |
|      - |  817 | `	sxi64 iLen;           /* Scalar $length */` |
|      - |  818 | `	int bLenGiven;        /* FALSE: $length absent/null -> element length */` |
|      - |  819 | `	sxi32 rc;             /* SXRET_OK or SXERR_MEM */` |
|      - |  820 | `};` |
|      - |  821 | `/*` |
|      - |  822 | ` * ph7_array_walk() callback over the array $string: replace the window of one` |
|      - |  823 | ` * element and insert the result under the element's original key. Array-form` |
|      - |  824 | ` * $replace/$offset/$length are consumed positionally; when a set runs out PHP` |
|      - |  825 | ` * falls back to ""/0/element-length respectively.` |
|      - |  826 | ` */` |
|     24 |  827 | `static int SubstrReplaceElemWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  828 | `{` |
|     25 |  829 | `	substr_replace_ctx *pRep = (substr_replace_ctx *)pUserData;` |
|      - |  830 | `	const char *zStr,*zRepl;` |
|      - |  831 | `	sxi64 f,l;` |
|      - |  832 | `	int nLen,nRepl;` |
|     25 |  833 | `	zStr = ph7_value_to_string(pData,&nLen);` |
|      - |  834 | `	/* Positional $replace element ("" when exhausted) */` |
|     25 |  835 | `	if( pRep->pRepl ){` |
|     11 |  836 | `		if( pRep->iReplCur < SySetUsed(pRep->pRepl) ){` |
|      7 |  837 | `			substr_repl_item *pItem = (substr_repl_item *)SySetAt(pRep->pRepl,pRep->iReplCur++);` |
|      7 |  838 | `			zRepl = (const char *)SyBlobDataAt(pRep->pReplPool,pItem->nOfft);` |
|      7 |  839 | `			nRepl = (int)pItem->nLen;` |
|      4 |  840 | `		}else{` |
|      5 |  841 | `			zRepl = "";` |
|      5 |  842 | `			nRepl = 0;` |
|      - |  843 | `		}` |
|      6 |  844 | `	}else{` |
|     15 |  845 | `		zRepl = pRep->zRepl;` |
|     15 |  846 | `		nRepl = pRep->nRepl;` |
|      - |  847 | `	}` |
|      - |  848 | `	/* Positional $offset element (0 when exhausted) */` |
|     25 |  849 | `	if( pRep->pFrom ){` |
|     13 |  850 | `		sxi64 *pVal = 0;` |
|     13 |  851 | `		if( pRep->iFromCur < SySetUsed(pRep->pFrom) ){` |
|      9 |  852 | `			pVal = (sxi64 *)SySetAt(pRep->pFrom,pRep->iFromCur++);` |
|      4 |  853 | `		}` |
|     13 |  854 | `		f = pVal ? *pVal : 0;` |
|      7 |  855 | `	}else{` |
|     13 |  856 | `		f = pRep->iFrom;` |
|      - |  857 | `	}` |
|      - |  858 | `	/* Positional $length element (element length when exhausted) */` |
|     25 |  859 | `	if( pRep->pLen ){` |
|      7 |  860 | `		sxi64 *pVal = 0;` |
|      7 |  861 | `		if( pRep->iLenCur < SySetUsed(pRep->pLen) ){` |
|      5 |  862 | `			pVal = (sxi64 *)SySetAt(pRep->pLen,pRep->iLenCur++);` |
|      2 |  863 | `		}` |
|      7 |  864 | `		l = pVal ? *pVal : nLen;` |
|      4 |  865 | `	}else{` |
|     19 |  866 | `		l = pRep->bLenGiven ? pRep->iLen : nLen;` |
|      - |  867 | `	}` |
|     25 |  868 | `	SubstrReplaceWindow(&f,&l,nLen);` |
|      - |  869 | `	/* Assemble prefix + replacement + suffix in the scratch value */` |
|     25 |  870 | `	ph7_value_reset_string_cursor(pRep->pScratch);` |
|     24 |  871 | `	if( (f > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,zStr,(int)f))` |
|     24 |  872 | `	 \|\| (nRepl > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,zRepl,nRepl))` |
|     40 |  873 | `	 \|\| (nLen - (int)(f+l) > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,&zStr[f+l],nLen - (int)(f+l))) ){` |
|     30 |  874 | `		pRep->rc = SXERR_MEM;` |
|     30 |  875 | `		return SXERR_ABORT;` |
|      - |  876 | `	}` |
|     25 |  877 | `	if( SXRET_OK != ph7_array_add_elem(pRep->pResult,pKey,pRep->pScratch) ){` |
|    ! 0 |  878 | `		pRep->rc = SXERR_MEM;` |
|    ! 0 |  879 | `		return SXERR_ABORT;` |
|      - |  880 | `	}` |
|     25 |  881 | `	return PH7_OK;` |
|     43 |  882 | `}` |
|      - |  883 | `/*` |
|      - |  884 | ` * mixed substr_replace(array\|string $string,array\|string $replace,array\|int $offset[,array\|int\|null $length = null])` |
|      - |  885 | ` *  Replace text within a portion of a string.` |
|      - |  886 | ` * Parameters` |
|      - |  887 | ` *  $string` |
|      - |  888 | ` *   The input string or an array of strings (each element is processed with` |
|      - |  889 | ` *   its own positional replace/offset/length when those are arrays too).` |
|      - |  890 | ` *  $replace` |
|      - |  891 | ` *   The replacement string. When $string is scalar and $replace is an array,` |
|      - |  892 | ` *   only its first element is used (PHP quirk).` |
|      - |  893 | ` *  $offset` |
|      - |  894 | ` *   Window start; negative counts from the end of the string.` |
|      - |  895 | ` *  $length` |
|      - |  896 | ` *   Window length; negative leaves that many bytes at the end; null/absent` |
|      - |  897 | ` *   means "to the end of the string".` |
|      - |  898 | ` * Return` |
|      - |  899 | ` *  The processed string, or an array of processed strings (keys preserved).` |
|      - |  900 | ` * Errors` |
|      - |  901 | ` *  ArgumentCountError on fewer than 3 arguments; TypeError when an array` |
|      - |  902 | ` *  $offset/$length is combined with a scalar $string.` |
|      - |  903 | ` */` |
|     68 |  904 | `static int PH7_builtin_substr_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  905 | `{` |
|      - |  906 | `	ph7_value sStrTmp,sReplTmp;` |
|     69 |  907 | `	const char *zStr = 0,*zRepl = 0;` |
|     69 |  908 | `	int nLen = 0,nRepl = 0;` |
|      - |  909 | `	int bLenGiven;` |
|     69 |  910 | `	sxi64 f = 0,l = 0;` |
|      - |  911 | `	sxi32 rc;` |
|     69 |  912 | `	if( nArg < 3 ){` |
|      7 |  913 | `		return PH7_VmThrowException(pCtx,` |
|      - |  914 | `			"ArgumentCountError",` |
|      - |  915 | `			"substr_replace() expects at least 3 arguments, %d given",` |
|      2 |  916 | `			nArg` |
|      - |  917 | `			);` |
|      - |  918 | `	}` |
|      - |  919 | `	/* $length counts as given unless absent or null (php: ?null semantics) */` |
|     65 |  920 | `	bLenGiven = (nArg > 3 && !ph7_value_is_null(apArg[3]));` |
|      - |  921 | `	/* php ZPP validates all four args, in order, before the body runs: the` |
|      - |  922 | `	 * non-array forms resolve here (null deprecation, __toString objects,` |
|      - |  923 | `	 * numeric strings), arrays pass through to the per-mode handling. */` |
|     65 |  924 | `	PH7_MemObjInit(pCtx->pVm,&sStrTmp);` |
|     65 |  925 | `	PH7_MemObjInit(pCtx->pVm,&sReplTmp);` |
|     65 |  926 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     49 |  927 | `		rc = StrPredicateResolveArg(pCtx,apArg[0],"substr_replace",1,"$string","array\|string",` |
|      - |  928 | `			"substr_replace(): Passing null to parameter #1 ($string) "` |
|      - |  929 | `			"of type array\|string is deprecated",` |
|      - |  930 | `			&sStrTmp,&zStr,&nLen);` |
|     49 |  931 | `		if( rc != PH7_OK ) goto out;` |
|     23 |  932 | `	}` |
|     63 |  933 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|     55 |  934 | `		rc = StrPredicateResolveArg(pCtx,apArg[1],"substr_replace",2,"$replace","array\|string",` |
|      - |  935 | `			"substr_replace(): Passing null to parameter #2 ($replace) "` |
|      - |  936 | `			"of type array\|string is deprecated",` |
|      - |  937 | `			&sReplTmp,&zRepl,&nRepl);` |
|     55 |  938 | `		if( rc != PH7_OK ) goto out;` |
|     25 |  939 | `	}` |
|     59 |  940 | `	if( !ph7_value_is_array(apArg[2]) ){` |
|     51 |  941 | `		rc = IntArgResolve(pCtx,apArg[2],"substr_replace",3,"$offset","array\|int",&f);` |
|     51 |  942 | `		if( rc != PH7_OK ) goto out;` |
|     24 |  943 | `	}` |
|     57 |  944 | `	if( bLenGiven && !ph7_value_is_array(apArg[3]) ){` |
|     31 |  945 | `		rc = IntArgResolve(pCtx,apArg[3],"substr_replace",4,"$length","array\|int\|null",&l);` |
|     31 |  946 | `		if( rc != PH7_OK ) goto out;` |
|     14 |  947 | `	}` |
|     55 |  948 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - |  949 | `		/* Array form: process each element, preserving keys */` |
|      - |  950 | `		substr_replace_ctx sRep;` |
|      - |  951 | `		substr_replace_collect sCol;` |
|      - |  952 | `		SyBlob sReplPool;` |
|      - |  953 | `		SySet sRepl,sFrom,sLen;` |
|      - |  954 | `		ph7_value *pResult,*pScratch;` |
|     15 |  955 | `		sxi32 rcWalk = SXRET_OK;` |
|     15 |  956 | `		SyBlobInit(&sReplPool,&pCtx->pVm->sAllocator);` |
|     15 |  957 | `		SySetInit(&sRepl,&pCtx->pVm->sAllocator,sizeof(substr_repl_item));` |
|     15 |  958 | `		SySetInit(&sFrom,&pCtx->pVm->sAllocator,sizeof(sxi64));` |
|     15 |  959 | `		SySetInit(&sLen,&pCtx->pVm->sAllocator,sizeof(sxi64));` |
|     15 |  960 | `		SyZero(&sRep,sizeof(substr_replace_ctx));` |
|     15 |  961 | `		sRep.bLenGiven = bLenGiven;` |
|     15 |  962 | `		sCol.rc = SXRET_OK;` |
|      - |  963 | `		/* Collect array-form $replace/$offset/$length positionally; the` |
|      - |  964 | `		 * scalar forms were already resolved above. */` |
|     15 |  965 | `		if( ph7_value_is_array(apArg[1]) ){` |
|      5 |  966 | `			sCol.pPool = &sReplPool;` |
|      5 |  967 | `			sCol.pSet = &sRepl;` |
|      5 |  968 | `			ph7_array_walk(apArg[1],SubstrReplaceStrWalker,&sCol);` |
|      5 |  969 | `			sRep.pRepl = &sRepl;` |
|      5 |  970 | `			sRep.pReplPool = &sReplPool;` |
|      3 |  971 | `		}else{` |
|     11 |  972 | `			sRep.zRepl = zRepl;` |
|     11 |  973 | `			sRep.nRepl = nRepl;` |
|      - |  974 | `		}` |
|     15 |  975 | `		if( sCol.rc == SXRET_OK && ph7_value_is_array(apArg[2]) ){` |
|      7 |  976 | `			sCol.pSet = &sFrom;` |
|      7 |  977 | `			ph7_array_walk(apArg[2],SubstrReplaceIntWalker,&sCol);` |
|      7 |  978 | `			sRep.pFrom = &sFrom;` |
|      4 |  979 | `		}else{` |
|      9 |  980 | `			sRep.iFrom = f;` |
|      - |  981 | `		}` |
|     15 |  982 | `		if( sCol.rc == SXRET_OK && bLenGiven ){` |
|      9 |  983 | `			if( ph7_value_is_array(apArg[3]) ){` |
|      5 |  984 | `				sCol.pSet = &sLen;` |
|      5 |  985 | `				ph7_array_walk(apArg[3],SubstrReplaceIntWalker,&sCol);` |
|      5 |  986 | `				sRep.pLen = &sLen;` |
|      3 |  987 | `			}else{` |
|      5 |  988 | `				sRep.iLen = l;` |
|      - |  989 | `			}` |
|      4 |  990 | `		}` |
|     15 |  991 | `		pResult = ph7_context_new_array(pCtx);` |
|     15 |  992 | `		pScratch = ph7_context_new_scalar(pCtx);` |
|     15 |  993 | `		if( sCol.rc != SXRET_OK \|\| pResult == 0 \|\| pScratch == 0 ){` |
|    ! 0 |  994 | `			rcWalk = SXERR_MEM;` |
|    ! 0 |  995 | `		}else{` |
|     15 |  996 | `			sRep.pResult = pResult;` |
|     15 |  997 | `			sRep.pScratch = pScratch;` |
|     15 |  998 | `			ph7_value_string(pScratch,"",0); /* Force string representation */` |
|     15 |  999 | `			ph7_array_walk(apArg[0],SubstrReplaceElemWalker,&sRep);` |
|     15 | 1000 | `			rcWalk = sRep.rc;` |
|      - | 1001 | `		}` |
|     15 | 1002 | `		SyBlobRelease(&sReplPool);` |
|     15 | 1003 | `		SySetRelease(&sRepl);` |
|     15 | 1004 | `		SySetRelease(&sFrom);` |
|     15 | 1005 | `		SySetRelease(&sLen);` |
|     15 | 1006 | `		if( rcWalk != SXRET_OK ){` |
|    ! 0 | 1007 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1008 | `			goto out;` |
|      - | 1009 | `		}` |
|     15 | 1010 | `		ph7_result_value(pCtx,pResult);` |
|     15 | 1011 | `		rc = PH7_OK;` |
|     15 | 1012 | `		goto out;` |
|      - | 1013 | `	}` |
|      - | 1014 | `	/* Scalar form: array $offset/$length are a TypeError, array $replace` |
|      - | 1015 | `	 * degrades to its first element (php quirk). */` |
|     41 | 1016 | `	if( ph7_value_is_array(apArg[2]) ){` |
|      3 | 1017 | `		rc = PH7_VmThrowException(pCtx,` |
|      - | 1018 | `			"TypeError",` |
|      - | 1019 | `			"substr_replace(): Argument #3 ($offset) cannot be an array when working on a single string"` |
|      - | 1020 | `			);` |
|      3 | 1021 | `		goto out;` |
|      - | 1022 | `	}` |
|     39 | 1023 | `	if( bLenGiven && ph7_value_is_array(apArg[3]) ){` |
|      3 | 1024 | `		rc = PH7_VmThrowException(pCtx,` |
|      - | 1025 | `			"TypeError",` |
|      - | 1026 | `			"substr_replace(): Argument #4 ($length) cannot be an array when working on a single string"` |
|      - | 1027 | `			);` |
|      3 | 1028 | `		goto out;` |
|      - | 1029 | `	}` |
|     37 | 1030 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 1031 | `		/* First element of the replace array, or "" when empty */` |
|      5 | 1032 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      5 | 1033 | `		zRepl = "";` |
|      5 | 1034 | `		nRepl = 0;` |
|      5 | 1035 | `		if( pMap->pFirst ){` |
|      3 | 1036 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pMap->pFirst->nValIdx);` |
|      3 | 1037 | `			if( pVal ){` |
|      3 | 1038 | `				zRepl = ph7_value_to_string(pVal,&nRepl);` |
|      1 | 1039 | `			}` |
|      1 | 1040 | `		}` |
|      2 | 1041 | `	}` |
|     37 | 1042 | `	if( !bLenGiven ){` |
|     15 | 1043 | `		l = nLen;` |
|      7 | 1044 | `	}` |
|     37 | 1045 | `	SubstrReplaceWindow(&f,&l,nLen);` |
|      - | 1046 | `	/* Assemble prefix + replacement + suffix straight into the call result` |
|      - | 1047 | `	 * (ph7_result_string appends), no scratch buffer needed. */` |
|     37 | 1048 | `	rc = SXRET_OK;` |
|     37 | 1049 | `	if( f > 0 ){` |
|     29 | 1050 | `		rc = ph7_result_string(pCtx,zStr,(int)f);` |
|     14 | 1051 | `	}` |
|     37 | 1052 | `	if( rc == SXRET_OK && nRepl > 0 ){` |
|     33 | 1053 | `		rc = ph7_result_string(pCtx,zRepl,nRepl);` |
|     16 | 1054 | `	}` |
|     37 | 1055 | `	if( rc == SXRET_OK && nLen - (int)(f+l) > 0 ){` |
|     17 | 1056 | `		rc = ph7_result_string(pCtx,&zStr[f+l],nLen - (int)(f+l));` |
|      8 | 1057 | `	}` |
|     37 | 1058 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 1059 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1060 | `		goto out;` |
|      - | 1061 | `	}` |
|      - | 1062 | `	/* Force a string result even when all three segments are empty */` |
|     37 | 1063 | `	rc = ph7_result_string(pCtx,"",0);` |
|     37 | 1064 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 1065 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1066 | `		goto out;` |
|      - | 1067 | `	}` |
|     37 | 1068 | `	rc = PH7_OK;` |
|     32 | 1069 | `out:` |
|     65 | 1070 | `	PH7_MemObjRelease(&sStrTmp);` |
|     65 | 1071 | `	PH7_MemObjRelease(&sReplTmp);` |
|     65 | 1072 | `	return rc;` |
|     35 | 1073 | `}` |
|      - | 1074 | `/*` |
|      - | 1075 | ` * int levenshtein(string $string1,string $string2[,int $insertion_cost = 1[,int $replacement_cost = 1[,int $deletion_cost = 1]]])` |
|      - | 1076 | ` *  Calculate the Levenshtein distance between two strings, byte per byte` |
|      - | 1077 | ` *  (case-sensitive), with optional per-operation costs. Mirrors PHP's` |
|      - | 1078 | ` *  reference_levdist(): two rolling rows over string2.` |
|      - | 1079 | ` * Return` |
|      - | 1080 | ` *  The minimal number of weighted edit operations turning $string1 into` |
|      - | 1081 | ` *  $string2.` |
|      - | 1082 | ` */` |
|     42 | 1083 | `static int PH7_builtin_levenshtein(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1084 | `{` |
|      - | 1085 | `	static const char *azParam[] = { "$insertion_cost","$replacement_cost","$deletion_cost" };` |
|      - | 1086 | `	const char *zStr1,*zStr2;` |
|     43 | 1087 | `	sxi64 iCostIns = 1,iCostRep = 1,iCostDel = 1;` |
|      - | 1088 | `	sxi64 *p1,*p2,*pTmp;` |
|      - | 1089 | `	sxi64 c0,c1,c2;` |
|      - | 1090 | `	ph7_value sTmp1,sTmp2;` |
|      - | 1091 | `	int nLen1,nLen2;` |
|      - | 1092 | `	int i1,i2;` |
|      - | 1093 | `	sxi32 rc;` |
|      - | 1094 | `	int i;` |
|     43 | 1095 | `	if( nArg < 2 ){` |
|      4 | 1096 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1097 | `			"ArgumentCountError",` |
|      - | 1098 | `			"levenshtein() expects at least 2 arguments, %d given",` |
|      1 | 1099 | `			nArg` |
|      - | 1100 | `			);` |
|      - | 1101 | `	}` |
|      - | 1102 | `	/* $string1/$string2: null deprecates to "", __toString objects resolve,` |
|      - | 1103 | `	 * everything non-stringish is a TypeError (php ZPP weak mode). */` |
|     41 | 1104 | `	PH7_MemObjInit(pCtx->pVm,&sTmp1);` |
|     41 | 1105 | `	PH7_MemObjInit(pCtx->pVm,&sTmp2);` |
|     41 | 1106 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"levenshtein",1,"$string1","string",` |
|      - | 1107 | `		"levenshtein(): Passing null to parameter #1 ($string1) "` |
|      - | 1108 | `		"of type string is deprecated",` |
|      - | 1109 | `		&sTmp1,&zStr1,&nLen1);` |
|     41 | 1110 | `	if( rc != PH7_OK ) goto out;` |
|     39 | 1111 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"levenshtein",2,"$string2","string",` |
|      - | 1112 | `		"levenshtein(): Passing null to parameter #2 ($string2) "` |
|      - | 1113 | `		"of type string is deprecated",` |
|      - | 1114 | `		&sTmp2,&zStr2,&nLen2);` |
|     39 | 1115 | `	if( rc != PH7_OK ) goto out;` |
|      - | 1116 | `	/* Optional integer costs */` |
|     63 | 1117 | `	for( i = 2 ; i < nArg && i < 5 ; i++ ){` |
|      - | 1118 | `		sxi64 iVal;` |
|     37 | 1119 | `		rc = IntArgResolve(pCtx,apArg[i],"levenshtein",i+1,azParam[i-2],"int",&iVal);` |
|     37 | 1120 | `		if( rc != PH7_OK ) goto out;` |
|     25 | 1121 | `		if( i == 2 ){` |
|     13 | 1122 | `			iCostIns = iVal;` |
|     19 | 1123 | `		}else if( i == 3 ){` |
|      7 | 1124 | `			iCostRep = iVal;` |
|      4 | 1125 | `		}else{` |
|      7 | 1126 | `			iCostDel = iVal;` |
|      - | 1127 | `		}` |
|     13 | 1128 | `	}` |
|     27 | 1129 | `	if( nLen1 == 0 ){` |
|      3 | 1130 | `		ph7_result_int64(pCtx,(sxi64)nLen2 * iCostIns);` |
|      3 | 1131 | `		rc = PH7_OK;` |
|      3 | 1132 | `		goto out;` |
|      - | 1133 | `	}` |
|     25 | 1134 | `	if( nLen2 == 0 ){` |
|      3 | 1135 | `		ph7_result_int64(pCtx,(sxi64)nLen1 * iCostDel);` |
|      3 | 1136 | `		rc = PH7_OK;` |
|      3 | 1137 | `		goto out;` |
|      - | 1138 | `	}` |
|      - | 1139 | `	/* Two rolling DP rows over string2 (auto-released on return). Reject a` |
|      - | 1140 | `	 * string2 long enough to overflow the 32-bit allocation size. */` |
|     23 | 1141 | `	if( (sxu32)nLen2 >= (SXU32_HIGH / sizeof(sxi64)) - 1 ){` |
|    ! 0 | 1142 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1143 | `		goto out;` |
|      - | 1144 | `	}` |
|     23 | 1145 | `	p1 = (sxi64 *)ph7_context_alloc_chunk(pCtx,(unsigned int)(sizeof(sxi64) * (sxu32)(nLen2 + 1)),FALSE,TRUE);` |
|     23 | 1146 | `	p2 = (sxi64 *)ph7_context_alloc_chunk(pCtx,(unsigned int)(sizeof(sxi64) * (sxu32)(nLen2 + 1)),FALSE,TRUE);` |
|     23 | 1147 | `	if( p1 == 0 \|\| p2 == 0 ){` |
|    ! 0 | 1148 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1149 | `		goto out;` |
|      - | 1150 | `	}` |
|    733 | 1151 | `	for( i2 = 0 ; i2 <= nLen2 ; i2++ ){` |
|    711 | 1152 | `		p1[i2] = (sxi64)i2 * iCostIns;` |
|    356 | 1153 | `	}` |
|    707 | 1154 | `	for( i1 = 0 ; i1 < nLen1 ; i1++ ){` |
|    685 | 1155 | `		p2[0] = p1[0] + iCostDel;` |
| 181111 | 1156 | `		for( i2 = 0 ; i2 < nLen2 ; i2++ ){` |
| 180427 | 1157 | `			c0 = p1[i2] + ((zStr1[i1] == zStr2[i2]) ? 0 : iCostRep);` |
| 180427 | 1158 | `			c1 = p1[i2 + 1] + iCostDel;` |
| 180427 | 1159 | `			if( c1 < c0 ){` |
|  45393 | 1160 | `				c0 = c1;` |
|  22696 | 1161 | `			}` |
| 180427 | 1162 | `			c2 = p2[i2] + iCostIns;` |
| 180427 | 1163 | `			if( c2 < c0 ){` |
|  44809 | 1164 | `				c0 = c2;` |
|  22404 | 1165 | `			}` |
| 180427 | 1166 | `			p2[i2 + 1] = c0;` |
|  90214 | 1167 | `		}` |
|    685 | 1168 | `		pTmp = p1;` |
|    685 | 1169 | `		p1 = p2;` |
|    685 | 1170 | `		p2 = pTmp;` |
|    343 | 1171 | `	}` |
|     23 | 1172 | `	ph7_result_int64(pCtx,p1[nLen2]);` |
|     23 | 1173 | `	rc = PH7_OK;` |
|     20 | 1174 | `out:` |
|     41 | 1175 | `	PH7_MemObjRelease(&sTmp1);` |
|     41 | 1176 | `	PH7_MemObjRelease(&sTmp2);` |
|     41 | 1177 | `	return rc;` |
|     22 | 1178 | `}` |
|      - | 1179 | `/*` |
|      - | 1180 | ` * Longest common substring scan behind similar_text() — a faithful port of` |
|      - | 1181 | ` * PHP's php_similar_str(): O(n*m) scan recording the first longest run.` |
|      - | 1182 | ` */` |
|     26 | 1183 | `static void SimilarStr(const char *zTxt1,int nLen1,const char *zTxt2,int nLen2,` |
|      - | 1184 | `	int *pPos1,int *pPos2,int *pMax,int *pCount)` |
|      1 | 1185 | `{` |
|      - | 1186 | `	const char *p,*q;` |
|     27 | 1187 | `	const char *zEnd1 = &zTxt1[nLen1];` |
|     27 | 1188 | `	const char *zEnd2 = &zTxt2[nLen2];` |
|      - | 1189 | `	int l;` |
|     27 | 1190 | `	*pMax = 0;` |
|     27 | 1191 | `	*pCount = 0;` |
|    143 | 1192 | `	for( p = zTxt1 ; p < zEnd1 ; p++ ){` |
|    843 | 1193 | `		for( q = zTxt2 ; q < zEnd2 ; q++ ){` |
|    999 | 1194 | `			for( l = 0 ; (p+l < zEnd1) && (q+l < zEnd2) && (p[l] == q[l]) ; l++ );` |
|    727 | 1195 | `			if( l > *pMax ){` |
|     25 | 1196 | `				*pMax = l;` |
|     25 | 1197 | `				*pCount += 1;` |
|     25 | 1198 | `				*pPos1 = (int)(p - zTxt1);` |
|     25 | 1199 | `				*pPos2 = (int)(q - zTxt2);` |
|     12 | 1200 | `			}` |
|    364 | 1201 | `		}` |
|     59 | 1202 | `	}` |
|     27 | 1203 | `}` |
|      - | 1204 | `/*` |
|      - | 1205 | ` * Recursive divide-and-conquer behind similar_text() — a faithful port of` |
|      - | 1206 | `` * PHP's php_similar_char(), including its quirky `count > 1` guard on the`` |
|      - | 1207 | ` * left-side recursion.` |
|      - | 1208 | ` */` |
|     26 | 1209 | `static int SimilarChar(const char *zTxt1,int nLen1,const char *zTxt2,int nLen2)` |
|      1 | 1210 | `{` |
|      - | 1211 | `	int nSum;` |
|     27 | 1212 | `	int nPos1 = 0,nPos2 = 0,nMax,nCount;` |
|     27 | 1213 | `	SimilarStr(zTxt1,nLen1,zTxt2,nLen2,&nPos1,&nPos2,&nMax,&nCount);` |
|     27 | 1214 | `	if( (nSum = nMax) != 0 ){` |
|     25 | 1215 | `		if( nPos1 && nPos2 && nCount > 1 ){` |
|    ! 0 | 1216 | `			nSum += SimilarChar(zTxt1,nPos1,zTxt2,nPos2);` |
|    ! 0 | 1217 | `		}` |
|     25 | 1218 | `		if( (nPos1 + nMax < nLen1) && (nPos2 + nMax < nLen2) ){` |
|     13 | 1219 | `			nSum += SimilarChar(&zTxt1[nPos1 + nMax],nLen1 - nPos1 - nMax,` |
|      8 | 1220 | `				&zTxt2[nPos2 + nMax],nLen2 - nPos2 - nMax);` |
|      4 | 1221 | `		}` |
|     12 | 1222 | `	}` |
|     27 | 1223 | `	return nSum;` |
|      1 | 1224 | `}` |
|      - | 1225 | `/*` |
|      - | 1226 | ` * int similar_text(string $string1,string $string2[,float &$percent])` |
|      - | 1227 | ` *  Calculate the similarity between two strings, as the number of matching` |
|      - | 1228 | ` *  characters found by PHP's greedy longest-common-substring recursion.` |
|      - | 1229 | ` *  When $percent is given it receives the similarity in percent:` |
|      - | 1230 | ` *  matching * 200 / (len1 + len2).` |
|      - | 1231 | ` * Return` |
|      - | 1232 | ` *  The number of matching characters in both strings.` |
|      - | 1233 | ` */` |
|     28 | 1234 | `static int PH7_builtin_similar_text(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1235 | `{` |
|      - | 1236 | `	const char *zStr1,*zStr2;` |
|      - | 1237 | `	ph7_value sTmp1,sTmp2;` |
|      - | 1238 | `	int nLen1,nLen2;` |
|      - | 1239 | `	int nSim;` |
|      - | 1240 | `	sxi32 rc;` |
|     29 | 1241 | `	if( nArg < 2 ){` |
|      4 | 1242 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1243 | `			"ArgumentCountError",` |
|      - | 1244 | `			"similar_text() expects at least 2 arguments, %d given",` |
|      1 | 1245 | `			nArg` |
|      - | 1246 | `			);` |
|      - | 1247 | `	}` |
|     27 | 1248 | `	PH7_MemObjInit(pCtx->pVm,&sTmp1);` |
|     27 | 1249 | `	PH7_MemObjInit(pCtx->pVm,&sTmp2);` |
|     27 | 1250 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"similar_text",1,"$string1","string",` |
|      - | 1251 | `		"similar_text(): Passing null to parameter #1 ($string1) "` |
|      - | 1252 | `		"of type string is deprecated",` |
|      - | 1253 | `		&sTmp1,&zStr1,&nLen1);` |
|     27 | 1254 | `	if( rc != PH7_OK ) goto out;` |
|     25 | 1255 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"similar_text",2,"$string2","string",` |
|      - | 1256 | `		"similar_text(): Passing null to parameter #2 ($string2) "` |
|      - | 1257 | `		"of type string is deprecated",` |
|      - | 1258 | `		&sTmp2,&zStr2,&nLen2);` |
|     25 | 1259 | `	if( rc != PH7_OK ) goto out;` |
|     23 | 1260 | `	if( nLen1 + nLen2 == 0 ){` |
|      5 | 1261 | `		nSim = 0;` |
|      3 | 1262 | `	}else{` |
|     19 | 1263 | `		nSim = SimilarChar(zStr1,nLen1,zStr2,nLen2);` |
|      - | 1264 | `	}` |
|     23 | 1265 | `	if( nArg > 2 ){` |
|      - | 1266 | `		/* Write the percentage through the by-ref out-param */` |
|      7 | 1267 | `		ph7_value *pPercent = ph7_context_new_scalar(pCtx);` |
|      7 | 1268 | `		if( pPercent == 0 ){` |
|    ! 0 | 1269 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1270 | `			goto out;` |
|    ! 0 | 1271 | `		}else{` |
|      7 | 1272 | `			double dPct = (nLen1 + nLen2 == 0) ? 0.0 : (double)nSim * 200.0 / (double)(nLen1 + nLen2);` |
|      7 | 1273 | `			ph7_value_double(pPercent,dPct);` |
|      7 | 1274 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],pPercent);` |
|      - | 1275 | `		}` |
|      3 | 1276 | `	}` |
|     23 | 1277 | `	ph7_result_int(pCtx,nSim);` |
|     23 | 1278 | `	rc = PH7_OK;` |
|     13 | 1279 | `out:` |
|     27 | 1280 | `	PH7_MemObjRelease(&sTmp1);` |
|     27 | 1281 | `	PH7_MemObjRelease(&sTmp2);` |
|     27 | 1282 | `	return rc;` |
|     15 | 1283 | `}` |
|      - | 1284 | `/*` |
|      - | 1285 | ` * array\|int str_word_count(string $string[,int $format = 0[,?string $characters = null]])` |
|      - | 1286 | ` *  Count (or return) the words inside a string. A word is a run of alphabetic` |
|      - | 1287 | ` *  characters, which may contain (but not start the string with) "'" and "-";` |
|      - | 1288 | ` *  $characters adds extra bytes to the word set ("a..z" ranges supported, as` |
|      - | 1289 | ` *  in PHP's php_charmask).` |
|      - | 1290 | ` *  $format: 0 -> word count, 1 -> array of words, 2 -> array of words keyed` |
|      - | 1291 | ` *  by their byte position in $string.` |
|      - | 1292 | ` * Errors` |
|      - | 1293 | ` *  ValueError when $format is not 0, 1 or 2.` |
|      - | 1294 | ` */` |
|     52 | 1295 | `static int PH7_builtin_str_word_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1296 | `{` |
|      - | 1297 | `	const char *zIn,*zEnd,*zPtr;` |
|     53 | 1298 | `	ph7_value *pArray = 0,*pValue = 0;` |
|      - | 1299 | `	ph7_value sTmp,sListTmp;` |
|      - | 1300 | `	char aMask[256];` |
|     53 | 1301 | `	int bMask = 0;` |
|     53 | 1302 | `	int iFormat = 0;` |
|     53 | 1303 | `	int nCount = 0;` |
|      - | 1304 | `	int nLen;` |
|      - | 1305 | `	sxi32 rc;` |
|     53 | 1306 | `	if( nArg < 1 ){` |
|      4 | 1307 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1308 | `			"ArgumentCountError",` |
|      - | 1309 | `			"str_word_count() expects at least 1 argument, %d given",` |
|      1 | 1310 | `			nArg` |
|      - | 1311 | `			);` |
|      - | 1312 | `	}` |
|     51 | 1313 | `	PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|     51 | 1314 | `	PH7_MemObjInit(pCtx->pVm,&sListTmp);` |
|     51 | 1315 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_word_count",1,"$string","string",` |
|      - | 1316 | `		"str_word_count(): Passing null to parameter #1 ($string) "` |
|      - | 1317 | `		"of type string is deprecated",` |
|      - | 1318 | `		&sTmp,&zIn,&nLen);` |
|     51 | 1319 | `	if( rc != PH7_OK ) goto out;` |
|     49 | 1320 | `	if( nArg > 1 ){` |
|      - | 1321 | `		sxi64 iVal;` |
|     35 | 1322 | `		rc = IntArgResolve(pCtx,apArg[1],"str_word_count",2,"$format","int",&iVal);` |
|     37 | 1323 | `		if( rc != PH7_OK ) goto out;` |
|     33 | 1324 | `		if( iVal < 0 \|\| iVal > 2 ){` |
|      5 | 1325 | `			rc = PH7_VmThrowException(pCtx,` |
|      - | 1326 | `				"ValueError",` |
|      - | 1327 | `				"str_word_count(): Argument #2 ($format) must be a valid format value"` |
|      - | 1328 | `				);` |
|      5 | 1329 | `			goto out;` |
|      - | 1330 | `		}` |
|     29 | 1331 | `		iFormat = (int)iVal;` |
|     14 | 1332 | `	}` |
|     43 | 1333 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      - | 1334 | `		/* $characters is ?string: null (skipped above) simply keeps the` |
|      - | 1335 | `		 * default word set, no deprecation. */` |
|      - | 1336 | `		const char *zList;` |
|      - | 1337 | `		int nList;` |
|     17 | 1338 | `		rc = StrPredicateResolveArg(pCtx,apArg[2],"str_word_count",3,"$characters","?string",` |
|      - | 1339 | `			"" /* unreachable: null never gets here */,` |
|      - | 1340 | `			&sListTmp,&zList,&nList);` |
|     17 | 1341 | `		if( rc != PH7_OK ) goto out;` |
|     13 | 1342 | `		PH7_BuildCharMask(pCtx,zList,nList,aMask);` |
|     13 | 1343 | `		bMask = 1;` |
|      6 | 1344 | `	}` |
|     39 | 1345 | `	if( iFormat != 0 ){` |
|     25 | 1346 | `		pArray = ph7_context_new_array(pCtx);` |
|     25 | 1347 | `		pValue = ph7_context_new_scalar(pCtx);` |
|     25 | 1348 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1349 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1350 | `			goto out;` |
|      - | 1351 | `		}` |
|     12 | 1352 | `	}` |
|     39 | 1353 | `	zPtr = zIn;` |
|     39 | 1354 | `	zEnd = &zIn[nLen];` |
|     39 | 1355 | `	if( nLen > 0 ){` |
|      - | 1356 | `		/* php: the string's first byte cannot be ' or -, and its last byte` |
|      - | 1357 | `		 * cannot be -, unless the charlist explicitly allows them. */` |
|     33 | 1358 | `		if( (zPtr[0] == '\'' && (!bMask \|\| !aMask[(unsigned char)'\''])) \|\|` |
|     28 | 1359 | `			(zPtr[0] == '-'  && (!bMask \|\| !aMask[(unsigned char)'-'])) ){` |
|      9 | 1360 | `			zPtr++;` |
|      4 | 1361 | `		}` |
|     33 | 1362 | `		if( zEnd[-1] == '-' && (!bMask \|\| !aMask[(unsigned char)'-']) ){` |
|      9 | 1363 | `			zEnd--;` |
|      4 | 1364 | `		}` |
|     16 | 1365 | `	}` |
|    135 | 1366 | `	while( zPtr < zEnd ){` |
|     91 | 1367 | `		const char *zStart = zPtr;` |
|    477 | 1368 | `		while( zPtr < zEnd && ( SyisAlpha((unsigned char)zPtr[0])` |
|    253 | 1369 | `			\|\| (bMask && aMask[(unsigned char)zPtr[0]])` |
|     98 | 1370 | `			\|\| zPtr[0] == '\'' \|\| zPtr[0] == '-' ) ){` |
|    339 | 1371 | `			zPtr++;` |
|      1 | 1372 | `		}` |
|     97 | 1373 | `		if( zPtr > zStart ){` |
|     91 | 1374 | `			if( iFormat == 0 ){` |
|     19 | 1375 | `				nCount++;` |
|     10 | 1376 | `			}else{` |
|     73 | 1377 | `				ph7_value_reset_string_cursor(pValue);` |
|     73 | 1378 | `				if( SXRET_OK != ph7_value_string(pValue,zStart,(int)(zPtr-zStart)) ){` |
|    ! 0 | 1379 | `					rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1380 | `					goto out;` |
|      - | 1381 | `				}` |
|     73 | 1382 | `				if( iFormat == 1 ){` |
|     59 | 1383 | `					if( SXRET_OK != ph7_array_add_elem(pArray,0,pValue) ){` |
|    ! 0 | 1384 | `						rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1385 | `						goto out;` |
|      - | 1386 | `					}` |
|     30 | 1387 | `				}else{` |
|     15 | 1388 | `					if( SXRET_OK != ph7_array_add_intkey_elem(pArray,(int)(zStart-zIn),pValue) ){` |
|    ! 0 | 1389 | `						rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1390 | `						goto out;` |
|      - | 1391 | `					}` |
|      - | 1392 | `				}` |
|      - | 1393 | `			}` |
|     45 | 1394 | `		}` |
|     97 | 1395 | `		zPtr++;` |
|      1 | 1396 | `	}` |
|     37 | 1397 | `	if( iFormat == 0 ){` |
|     13 | 1398 | `		ph7_result_int(pCtx,nCount);` |
|      7 | 1399 | `	}else{` |
|     25 | 1400 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1401 | `	}` |
|     37 | 1402 | `	rc = PH7_OK;` |
|     24 | 1403 | `out:` |
|     49 | 1404 | `	PH7_MemObjRelease(&sTmp);` |
|     49 | 1405 | `	PH7_MemObjRelease(&sListTmp);` |
|     49 | 1406 | `	return rc;` |
|     26 | 1407 | `}` |
|      - | 1408 | `/*` |
|      - | 1409 | ` * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])` |
|      - | 1410 | ` *   Split a string into smaller chunks.` |
|      - | 1411 | ` * Parameters` |
|      - | 1412 | ` *  $body` |
|      - | 1413 | ` *   The string to be chunked.` |
|      - | 1414 | ` * $chunklen` |
|      - | 1415 | ` *   The chunk length.` |
|      - | 1416 | ` * $end` |
|      - | 1417 | ` *   The line ending sequence.` |
|      - | 1418 | ` * Return` |
|      - | 1419 | ` *  The chunked string or NULL on failure.` |
|      - | 1420 | ` */` |
|     14 | 1421 | `static int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1422 | `{` |
|     15 | 1423 | `	const char *zIn,*zEnd,*zSep = "\r\n";` |
|      - | 1424 | `	int nSepLen,nChunkLen,nLen;` |
|     15 | 1425 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1426 | `		/* Nothing to split,return null */` |
|      3 | 1427 | `		ph7_result_null(pCtx);` |
|      3 | 1428 | `		return PH7_OK;` |
|      - | 1429 | `	}` |
|      - | 1430 | `	/* initialize/Extract arguments */` |
|     13 | 1431 | `	nSepLen = (int)sizeof("\r\n") - 1;` |
|     13 | 1432 | `	nChunkLen = 76;` |
|     13 | 1433 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 1434 | `	zEnd = &zIn[nLen];` |
|     13 | 1435 | `	if( nArg > 1 ){` |
|      - | 1436 | `		/* Chunk length */` |
|     13 | 1437 | `		nChunkLen = ph7_value_to_int(apArg[1]);` |
|     13 | 1438 | `		if( nChunkLen < 1 ){` |
|      - | 1439 | `			/* PHP 8 throws a catchable ValueError for a non-positive length. */` |
|      3 | 1440 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1441 | `				"chunk_split(): Argument #2 ($length) must be greater than 0");` |
|      - | 1442 | `		}` |
|     11 | 1443 | `		if( nArg > 2 ){` |
|      - | 1444 | `			/* Separator */` |
|      9 | 1445 | `			zSep = ph7_value_to_string(apArg[2],&nSepLen);` |
|      9 | 1446 | `			if( nSepLen < 1 ){` |
|      - | 1447 | `				/* Switch back to the default separator */` |
|      3 | 1448 | `				zSep = "\r\n";` |
|      3 | 1449 | `				nSepLen = (int)sizeof("\r\n") - 1;` |
|      1 | 1450 | `			}` |
|      4 | 1451 | `		}` |
|      5 | 1452 | `	}` |
|      - | 1453 | `	/* Perform the requested operation */` |
|     11 | 1454 | `	if( nChunkLen > nLen ){` |
|      - | 1455 | `		/* Nothing to split,return the string and the separator */` |
|      7 | 1456 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);` |
|      7 | 1457 | `		return PH7_OK;` |
|      - | 1458 | `	}` |
|     17 | 1459 | `	while( zIn < zEnd ){` |
|     13 | 1460 | `		if( nChunkLen > (int)(zEnd-zIn) ){` |
|      3 | 1461 | `			nChunkLen = (int)(zEnd - zIn);` |
|      1 | 1462 | `		}` |
|      - | 1463 | `		/* Append the chunk and the separator */` |
|     13 | 1464 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);` |
|      - | 1465 | `		/* Point beyond the chunk */` |
|     13 | 1466 | `		zIn += nChunkLen;` |
|      1 | 1467 | `	}` |
|      5 | 1468 | `	return PH7_OK;` |
|      8 | 1469 | `}` |
|      - | 1470 | `/*` |
|      - | 1471 | ` * string addslashes(string $str)` |
|      - | 1472 | ` *  Quote string with slashes.` |
|      - | 1473 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1474 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1475 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1476 | ` * Parameter` |
|      - | 1477 | ` *  str: The string to be escaped.` |
|      - | 1478 | ` * Return` |
|      - | 1479 | ` *  Returns the escaped string` |
|      - | 1480 | ` */` |
|     24 | 1481 | `static int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1482 | `{` |
|      - | 1483 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1484 | `	int nLen;` |
|      - | 1485 | `	/* PHP enforces exactly one argument. */` |
|     28 | 1486 | `	if( nArg != 1 ){` |
|      8 | 1487 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1488 | `			"ArgumentCountError",` |
|      - | 1489 | `			"addslashes() expects exactly 1 argument, %d given",` |
|      2 | 1490 | `			nArg` |
|      - | 1491 | `			);` |
|      - | 1492 | `	}` |
|      - | 1493 | `	/* NULL is deprecated and treated as an empty string; other invalid` |
|      - | 1494 | `	 * types still produce a TypeError. */` |
|     22 | 1495 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 1496 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 1497 | `			E_DEPRECATED,` |
|      - | 1498 | `			"addslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - | 1499 | `			);` |
|      - | 1500 | `		/* fall through so conversion below yields empty string */` |
|      1 | 1501 | `	}` |
|      - | 1502 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     29 | 1503 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     29 | 1504 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     18 | 1505 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 1506 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1507 | `			"TypeError",` |
|      - | 1508 | `			"addslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 1509 | `			ph7_type_name(apArg[0])` |
|      - | 1510 | `			);` |
|      - | 1511 | `	}` |
|      - | 1512 | `	/* Convert to string representation first and obtain length. */` |
|     19 | 1513 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 1514 | `	if( nLen < 1 ){` |
|      - | 1515 | `		/* Return the empty string */` |
|      5 | 1516 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1517 | `		return PH7_OK;` |
|      - | 1518 | `	}` |
|     15 | 1519 | `	zEnd = &zIn[nLen];` |
|     15 | 1520 | `	zCur = 0; /* cc warning */` |
|     20 | 1521 | `	for(;;){` |
|     41 | 1522 | `		if( zIn >= zEnd ){` |
|      - | 1523 | `			/* No more input */` |
|     15 | 1524 | `			break;` |
|      - | 1525 | `		}` |
|     27 | 1526 | `		zCur = zIn;` |
|      - | 1527 | `		/* scan until a character that needs escaping (', ", \\, or NUL) */` |
|     89 | 1528 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' && zIn[0] != '\0' ){` |
|     63 | 1529 | `			zIn++;` |
|      1 | 1530 | `		}` |
|     27 | 1531 | `		if( zIn > zCur ){` |
|      - | 1532 | `			/* Append raw contents */` |
|     23 | 1533 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 1534 | `		}` |
|     27 | 1535 | `		if( zIn < zEnd ){` |
|     17 | 1536 | `			int c = zIn[0];` |
|     17 | 1537 | `			if( c == '\0' ){` |
|      - | 1538 | `				/* PHP escapes NUL as "\\0" (two characters) */` |
|      3 | 1539 | `				ph7_result_string(pCtx,"\\0",2);` |
|      2 | 1540 | `			}else{` |
|     15 | 1541 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1542 | `			}` |
|      8 | 1543 | `		}` |
|     27 | 1544 | `		zIn++;` |
|      1 | 1545 | `	}` |
|     15 | 1546 | `	return PH7_OK;` |
|     16 | 1547 | `}` |
|      - | 1548 | `/*` |
|      - | 1549 | ``  * Build a 256-entry membership mask from a PHP charlist, expanding `a..z` `` |
|      - | 1550 | ` * byte ranges exactly like PHP's php_charmask(). On return aMask[c] != 0 iff` |
|      - | 1551 | ` * the byte c belongs to the set. Emits the PHP-exact warnings for the three` |
|      - | 1552 | ` * malformed-range shapes (ph7_context_throw_error_format prepends the active` |
|      - | 1553 | ` * function name, so the messages omit it); on a bad range the surrounding` |
|      - | 1554 | ` * bytes are still added and the scan never aborts. Reads only within` |
|      - | 1555 | ` * [zList, zList+nLen).` |
|      - | 1556 | ` *` |
|      - | 1557 | ` * Use ONLY for the builtins whose charlist expands ranges the way PHP's` |
|      - | 1558 | ` * php_charmask() does: trim/ltrim/rtrim/addcslashes (and quotemeta, whose set` |
|      - | 1559 | ` * is a fixed literal with no ".."). Do NOT route strspn/strcspn/strtok/strpbrk` |
|      - | 1560 | ` * through this — PHP treats their charlists literally, so expanding "a..z" here` |
|      - | 1561 | ` * would be a behavior regression plus spurious "Invalid '..'-range" warnings.` |
|      - | 1562 | ` */` |
|     90 | 1563 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256])` |
|      3 | 1564 | `{` |
|     93 | 1565 | `	const unsigned char *zIn  = (const unsigned char *)zList;` |
|     93 | 1566 | `	const unsigned char *zEnd = zIn + (nLen > 0 ? nLen : 0);` |
|     93 | 1567 | `	SyZero(aMask,256);` |
|    315 | 1568 | `	for( ; zIn < zEnd ; zIn++ ){` |
|    225 | 1569 | `		int c = zIn[0];` |
|    225 | 1570 | `		if( zIn + 3 < zEnd && zIn[1] == '.' && zIn[2] == '.' && zIn[3] >= c ){` |
|      - | 1571 | `			/* Valid incrementing range c..zIn[3] */` |
|     22 | 1572 | `			int hi = zIn[3],k;` |
|    386 | 1573 | `			for( k = c ; k <= hi ; k++ ){` |
|    366 | 1574 | `				aMask[k] = 1;` |
|    184 | 1575 | `			}` |
|     22 | 1576 | `			zIn += 3; /* the loop's ++ then steps past the range end */` |
|    224 | 1577 | `		}else if( zIn + 1 < zEnd && zIn[0] == '.' && zIn[1] == '.' ){` |
|      - | 1578 | `			/* Malformed range: mirror php_charmask's three diagnostics. */` |
|      - | 1579 | `			const char *zMsg;` |
|     20 | 1580 | `			if( (const unsigned char *)zList >= zIn ){` |
|      6 | 1581 | `				zMsg = "no character to the left of '..'";` |
|     18 | 1582 | `			}else if( zIn + 2 >= zEnd ){` |
|      6 | 1583 | `				zMsg = "no character to the right of '..'";` |
|     14 | 1584 | `			}else if( zIn[-1] > zIn[2] ){` |
|     12 | 1585 | `				zMsg = "'..'-range needs to be incrementing";` |
|      7 | 1586 | `			}else{` |
|    ! 0 | 1587 | `				zMsg = 0; /* catch-all (e.g. a..b..c) */` |
|      - | 1588 | `			}` |
|     20 | 1589 | `			if( zMsg ){` |
|     29 | 1590 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      9 | 1591 | `					"Invalid '..'-range, %s",zMsg);` |
|     11 | 1592 | `			}else{` |
|    ! 0 | 1593 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1594 | `					"Invalid '..'-range");` |
|      - | 1595 | `			}` |
|      - | 1596 | `			/* Do not consume the dots: the loop's ++ steps one byte so the` |
|      - | 1597 | `			 * dots are re-scanned as literals, exactly like php_charmask. */` |
|     11 | 1598 | `		}else{` |
|    187 | 1599 | `			aMask[c] = 1;` |
|      - | 1600 | `		}` |
|    114 | 1601 | `	}` |
|     93 | 1602 | `}` |
|      - | 1603 | `/*` |
|      - | 1604 | ` * string addcslashes(string $str,string $charlist)` |
|      - | 1605 | ` *  Quote string with slashes in a C style.` |
|      - | 1606 | ` * Parameter` |
|      - | 1607 | ` *  $str:` |
|      - | 1608 | ` *    The string to be escaped.` |
|      - | 1609 | ` *  $charlist:` |
|      - | 1610 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|      - | 1611 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|      - | 1612 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|      - | 1613 | ` * Return` |
|      - | 1614 | ` *  Returns the escaped string.` |
|      - | 1615 | ` * Note:` |
|      - | 1616 | ` *  Character ranges [i.e: 'A..Z'] are supported (see PH7_BuildCharMask).` |
|      - | 1617 | ` */` |
|     40 | 1618 | `static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1619 | `{` |
|      - | 1620 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - | 1621 | `	char aMask[256];` |
|      - | 1622 | `	int nLen,nMask;` |
|      - | 1623 | `	/* PHP enforces exactly two arguments. */` |
|     45 | 1624 | `	if( nArg != 2 ){` |
|      8 | 1625 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1626 | `			"ArgumentCountError",` |
|      - | 1627 | `			"addcslashes() expects exactly 2 arguments, %d given",` |
|      2 | 1628 | `			nArg` |
|      - | 1629 | `			);` |
|      - | 1630 | `	}` |
|      - | 1631 | `	/* First argument must be a string-ish value.  NULL is deprecated and` |
|      - | 1632 | `	 * treated as the empty string (PHP 8.1). */` |
|     40 | 1633 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      - | 1634 | `		/* Emit deprecation only once, similar to PHP behaviour. */` |
|      3 | 1635 | `		PH7_VmThrowError(pCtx->pVm,0,/* iErr will be patched to 8192 below */` |
|      - | 1636 | `			E_DEPRECATED,` |
|      - | 1637 | `			"addcslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - | 1638 | `			);` |
|      - | 1639 | `		/* treat as empty string; fall through to conversion logic */` |
|     52 | 1640 | `	} else if( ph7_value_is_array(apArg[0]) \|\|` |
|     52 | 1641 | `	          ph7_value_is_object(apArg[0]) \|\|` |
|     32 | 1642 | `	          ph7_value_is_resource(apArg[0]) ){` |
|      4 | 1643 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1644 | `			"TypeError",` |
|      - | 1645 | `			"addcslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 1646 | `			ph7_type_name(apArg[0])` |
|      - | 1647 | `			);` |
|      - | 1648 | `	}` |
|      - | 1649 | `	/* Second argument must be a string.  NULL is deprecated and treated as` |
|      - | 1650 | `	 * an empty mask per PHP semantics.  Arrays/objects/resources still` |
|      - | 1651 | `	 * trigger a TypeError. */` |
|     37 | 1652 | `	if( ph7_value_is_null(apArg[1]) ){` |
|      3 | 1653 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 1654 | `			E_DEPRECATED,` |
|      - | 1655 | `			"addcslashes(): Passing null to parameter #2 ($characters) of type string is deprecated"` |
|      - | 1656 | `			);` |
|      - | 1657 | `		/* allow through so it becomes empty string below */` |
|     49 | 1658 | `	} else if( ph7_value_is_array(apArg[1]) \|\|` |
|     48 | 1659 | `	          ph7_value_is_object(apArg[1]) \|\|` |
|     30 | 1660 | `	          ph7_value_is_resource(apArg[1]) ){` |
|      4 | 1661 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1662 | `			"TypeError",` |
|      - | 1663 | `			"addcslashes(): Argument #2 ($characters) must be of type string, %s given",` |
|      2 | 1664 | `			ph7_type_name(apArg[1])` |
|      - | 1665 | `			);` |
|      - | 1666 | `	}` |
|      - | 1667 | `	/* Extract the string to process */` |
|     35 | 1668 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1669 | `	/* NULL would never reach here due to the check above. */` |
|     35 | 1670 | `	if( nLen < 1 ){` |
|      - | 1671 | `		/* Empty string returns itself. */` |
|      5 | 1672 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      5 | 1673 | `		return PH7_OK;` |
|      - | 1674 | `	}` |
|      - | 1675 | ``	/* Extract the desired mask and expand any `a..z` ranges into a lookup. */`` |
|     31 | 1676 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|     31 | 1677 | `	PH7_BuildCharMask(pCtx,zMask,nMask,aMask);` |
|     31 | 1678 | `	zEnd = &zIn[nLen];` |
|     31 | 1679 | `	zCur = 0; /* cc warning */` |
|     37 | 1680 | `	for(;;){` |
|     77 | 1681 | `		if( zIn >= zEnd ){` |
|      - | 1682 | `			/* No more input */` |
|     31 | 1683 | `			break;` |
|      - | 1684 | `		}` |
|     49 | 1685 | `		zCur = zIn;` |
|    125 | 1686 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     79 | 1687 | `			zIn++;` |
|      3 | 1688 | `		}` |
|     49 | 1689 | `		if( zIn > zCur ){` |
|      - | 1690 | `			/* Append raw contents */` |
|     43 | 1691 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     20 | 1692 | `		}` |
|     49 | 1693 | `		if( zIn < zEnd ){` |
|      - | 1694 | `			/* Make sure we treat the byte as unsigned to avoid negative values` |
|      - | 1695 | `			 * on platforms where char is signed. */` |
|     29 | 1696 | `			int c = (unsigned char)zIn[0];` |
|      - | 1697 | `			/* Handle special C-like escapes for common control characters first.` |
|      - | 1698 | `			 * PHP outputs "\n" "\r" "\t" "\v" "\f" when those chars are` |
|      - | 1699 | `			 * in the mask. NUL is left to the octal conversion below. */` |
|     29 | 1700 | `			if( c == '\n' ){` |
|      3 | 1701 | `				ph7_result_string(pCtx,"\\n",2);` |
|     28 | 1702 | `			}else if( c == '\r' ){` |
|      3 | 1703 | `				ph7_result_string(pCtx,"\\r",2);` |
|     26 | 1704 | `			}else if( c == '\t' ){` |
|      3 | 1705 | `				ph7_result_string(pCtx,"\\t",2);` |
|     24 | 1706 | `			}else if( c == '\v' ){` |
|      3 | 1707 | `				ph7_result_string(pCtx,"\\v",2);` |
|     22 | 1708 | `			}else if( c == '\f' ){` |
|      3 | 1709 | `				ph7_result_string(pCtx,"\\f",2);` |
|     20 | 1710 | `			}else if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - | 1711 | `				/* Convert to octal.  PHP always emits three-digit zero-padded` |
|      - | 1712 | `				 * octal escapes (\001 not \1). */` |
|      7 | 1713 | `				ph7_result_string_format(pCtx,"\\%03o",c);` |
|      4 | 1714 | `			}else{` |
|     13 | 1715 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1716 | `			}` |
|     13 | 1717 | `		}` |
|     49 | 1718 | `		zIn++;` |
|      3 | 1719 | `	}` |
|     31 | 1720 | `	return PH7_OK;` |
|     25 | 1721 | `}` |
|      - | 1722 | `/*` |
|      - | 1723 | ` * string quotemeta(string $str)` |
|      - | 1724 | ` *  Quote meta characters.` |
|      - | 1725 | ` * Parameter` |
|      - | 1726 | ` *  $str:` |
|      - | 1727 | ` *    The string to be escaped.` |
|      - | 1728 | ` * Return` |
|      - | 1729 | ` *  Returns the escaped string.` |
|      - | 1730 | `*/` |
|     10 | 1731 | `static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1732 | `{` |
|      - | 1733 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1734 | `	char aMask[256];` |
|      - | 1735 | `	int nLen;` |
|     12 | 1736 | `	if( nArg < 1 ){` |
|      - | 1737 | `		/* Nothing to process,retun NULL */` |
|    ! 0 | 1738 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1739 | `		return PH7_OK;` |
|      - | 1740 | `	}` |
|      - | 1741 | `	/* Extract the string to process */` |
|     12 | 1742 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     12 | 1743 | `	if( nLen < 1 ){` |
|      - | 1744 | `		/* Return the empty string */` |
|      3 | 1745 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1746 | `		return PH7_OK;` |
|      - | 1747 | `	}` |
|      - | 1748 | `	/* Fixed meta-character set (no ranges); build the lookup once. */` |
|     10 | 1749 | `	PH7_BuildCharMask(pCtx,".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1,aMask);` |
|     10 | 1750 | `	zEnd = &zIn[nLen];` |
|     10 | 1751 | `	zCur = 0; /* cc warning */` |
|     22 | 1752 | `	for(;;){` |
|     46 | 1753 | `		if( zIn >= zEnd ){` |
|      - | 1754 | `			/* No more input */` |
|     10 | 1755 | `			break;` |
|      - | 1756 | `		}` |
|     38 | 1757 | `		zCur = zIn;` |
|     76 | 1758 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     40 | 1759 | `			zIn++;` |
|      2 | 1760 | `		}` |
|     38 | 1761 | `		if( zIn > zCur ){` |
|      - | 1762 | `			/* Append raw contents */` |
|     20 | 1763 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      9 | 1764 | `		}` |
|     38 | 1765 | `		if( zIn < zEnd ){` |
|     36 | 1766 | `			int c = zIn[0];` |
|     36 | 1767 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     17 | 1768 | `		}` |
|     38 | 1769 | `		zIn++;` |
|      2 | 1770 | `	}` |
|     10 | 1771 | `	return PH7_OK;` |
|      7 | 1772 | `}` |
|      - | 1773 | `/*` |
|      - | 1774 | ` * string stripslashes(string $str)` |
|      - | 1775 | ` *  Un-quotes a quoted string.` |
|      - | 1776 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1777 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1778 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1779 | ` * Parameter` |
|      - | 1780 | ` *  $str` |
|      - | 1781 | ` *   The input string.` |
|      - | 1782 | ` * Return` |
|      - | 1783 | ` *  Returns a string with backslashes stripped off.` |
|      - | 1784 | ` */` |
|      6 | 1785 | `static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1786 | `{` |
|      - | 1787 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1788 | `	int nLen;` |
|      7 | 1789 | `	if( nArg < 1 ){` |
|      - | 1790 | `		/* Nothing to process,retun NULL */` |
|    ! 0 | 1791 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1792 | `		return PH7_OK;` |
|      - | 1793 | `	}` |
|      - | 1794 | `	/* Extract the string to process */` |
|      7 | 1795 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 1796 | `	if( zIn == 0 ){` |
|    ! 0 | 1797 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1798 | `		return PH7_OK;` |
|      - | 1799 | `	}` |
|      7 | 1800 | `	zEnd = &zIn[nLen];` |
|      7 | 1801 | `	zCur = 0; /* cc warning */` |
|      - | 1802 | `	/* Encode the string */` |
|      4 | 1803 | `	for(;;){` |
|      9 | 1804 | `		if( zIn >= zEnd ){` |
|      - | 1805 | `			/* No more input */` |
|      5 | 1806 | `			break;` |
|      - | 1807 | `		}` |
|      5 | 1808 | `		zCur = zIn;` |
|     17 | 1809 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 | 1810 | `			zIn++;` |
|      1 | 1811 | `		}` |
|      5 | 1812 | `		if( zIn > zCur ){` |
|      - | 1813 | `			/* Append raw contents */` |
|      5 | 1814 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 1815 | `		}` |
|      5 | 1816 | `		if( &zIn[1] < zEnd ){` |
|      3 | 1817 | `			int c = zIn[1];` |
|      3 | 1818 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - | 1819 | `				/* Ignore the backslash */` |
|      3 | 1820 | `				zIn++;` |
|      1 | 1821 | `			}` |
|      2 | 1822 | `		}else{` |
|      3 | 1823 | `			break;` |
|      - | 1824 | `		}` |
|      1 | 1825 | `	}` |
|      7 | 1826 | `	return PH7_OK;` |
|      4 | 1827 | `}` |
|      - | 1828 | `/*` |
|      - | 1829 | ` * UTF-8-aware HTML entity machinery, shared by htmlspecialchars/htmlentities/` |
|      - | 1830 | ` * htmlspecialchars_decode/html_entity_decode/get_html_translation_table.` |
|      - | 1831 | ` * The implementations live further down in this file, next to the filter_var` |
|      - | 1832 | ` * FULL_SPECIAL_CHARS machinery they reuse (aHtml401Ent[]/FvHtml401Lookup()/` |
|      - | 1833 | ` * FvUtf8Next()). Semantics are byte-exact vs php 8.5.7; PHL is UTF-8-only` |
|      - | 1834 | ` * (PLAN.md §6) so every charset argument other than a UTF-8 alias gets PHP's` |
|      - | 1835 | ` * unsupported-charset warning and is treated as UTF-8.` |
|      - | 1836 | ` *` |
|      - | 1837 | ` * Flag model (the PHP-exact ENT_* values, see constant.c): bit 1 = encode/` |
|      - | 1838 | ` * decode single quotes, bit 2 = double quotes (ENT_QUOTES=3, ENT_COMPAT=2,` |
|      - | 1839 | ` * ENT_NOQUOTES=0); bits 16\|32 select the doctype (0=HTML401, 16=XML1,` |
|      - | 1840 | ` * 32=XHTML, 48=HTML5); ENT_IGNORE=4 drops invalid UTF-8 bytes (wins over` |
|      - | 1841 | ` * ENT_SUBSTITUTE=8, which replaces each with U+FFFD; with neither set the` |
|      - | 1842 | ` * whole result collapses to ""); ENT_DISALLOWED=128 substitutes valid but` |
|      - | 1843 | ` * doctype-disallowed codepoints. The shared default is` |
|      - | 1844 | ` * ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 = 11.` |
|      - | 1845 | ` */` |
|      - | 1846 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bAll,int bDoubleEncode);` |
|      - | 1847 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bFull);` |
|      - | 1848 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx);` |
|      - | 1849 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags);` |
|      - | 1850 | `/*` |
|      - | 1851 | ` * string htmlspecialchars(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1852 | ` *                         [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1853 | ` *  Convert the special characters & < > " ' to HTML entities.` |
|      - | 1854 | ` * Return` |
|      - | 1855 | ` *  The escaped string or NULL on failure.` |
|      - | 1856 | ` */` |
|     42 | 1857 | `static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1858 | `{` |
|     43 | 1859 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1860 | `	const char *zIn;` |
|     43 | 1861 | `	int nLen,bDouble = 1;` |
|     43 | 1862 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1863 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 1864 | `		ph7_result_null(pCtx);` |
|      3 | 1865 | `		return PH7_OK;` |
|      - | 1866 | `	}` |
|      - | 1867 | `	/* Extract the target string */` |
|     41 | 1868 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 1869 | `	if( nArg > 1 ){` |
|     35 | 1870 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     17 | 1871 | `	}` |
|     41 | 1872 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     41 | 1873 | `	if( nArg > 3 ){` |
|      7 | 1874 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      3 | 1875 | `	}` |
|     41 | 1876 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,0,bDouble);` |
|     41 | 1877 | `	return PH7_OK;` |
|     22 | 1878 | `}` |
|      - | 1879 | `/*` |
|      - | 1880 | ` * string htmlspecialchars_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401])` |
|      - | 1881 | ` *  Convert the special HTML entities (&amp; &lt; &gt; &quot; and the` |
|      - | 1882 | ` *  numeric/doctype forms of the two quotes) back to characters.` |
|      - | 1883 | ` * Return` |
|      - | 1884 | ` *  The unescaped string or NULL on failure.` |
|      - | 1885 | ` */` |
|     22 | 1886 | `static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1887 | `{` |
|     23 | 1888 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1889 | `	const char *zIn;` |
|      - | 1890 | `	int nLen;` |
|     23 | 1891 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1892 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 1893 | `		ph7_result_null(pCtx);` |
|      3 | 1894 | `		return PH7_OK;` |
|      - | 1895 | `	}` |
|      - | 1896 | `	/* Extract the target string */` |
|     21 | 1897 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     21 | 1898 | `	if( nArg > 1 ){` |
|      9 | 1899 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1900 | `	}` |
|     21 | 1901 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,0);` |
|     21 | 1902 | `	return PH7_OK;` |
|     12 | 1903 | `}` |
|      - | 1904 | `/*` |
|      - | 1905 | ` * array get_html_translation_table(int $table = HTML_SPECIALCHARS` |
|      - | 1906 | ` *      [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 [, string $encoding = "UTF-8"]])` |
|      - | 1907 | ` *  Return the translation table used by htmlspecialchars() (HTML_SPECIALCHARS)` |
|      - | 1908 | ` *  or htmlentities() (HTML_ENTITIES) as character => entity pairs.` |
|      - | 1909 | ` * Return` |
|      - | 1910 | ` *  The translation table as an array or NULL on failure.` |
|      - | 1911 | ` */` |
|     12 | 1912 | `static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1913 | `{` |
|     13 | 1914 | `	int iTable = 0; /* HTML_SPECIALCHARS */` |
|     13 | 1915 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|     13 | 1916 | `	if( nArg > 0 ){` |
|     11 | 1917 | `		iTable = ph7_value_to_int(apArg[0]);` |
|      5 | 1918 | `	}` |
|     13 | 1919 | `	if( nArg > 1 ){` |
|      9 | 1920 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1921 | `	}` |
|     13 | 1922 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     13 | 1923 | `	HtmlTranslationTable(pCtx,iTable,iFlags);` |
|     13 | 1924 | `	return PH7_OK;` |
|      1 | 1925 | `}` |
|      - | 1926 | `/*` |
|      - | 1927 | ` * string htmlentities(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1928 | ` *                     [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1929 | ` *  Convert all applicable characters to HTML entities: the specials plus` |
|      - | 1930 | ` *  every codepoint with an HTML 4.01 named entity (aHtml401Ent[]).` |
|      - | 1931 | ` * Return` |
|      - | 1932 | ` *  The encoded string or NULL on failure.` |
|      - | 1933 | ` */` |
|     30 | 1934 | `static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1935 | `{` |
|     31 | 1936 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1937 | `	const char *zIn;` |
|     31 | 1938 | `	int nLen,bDouble = 1;` |
|     31 | 1939 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1940 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1941 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1942 | `		return PH7_OK;` |
|      - | 1943 | `	}` |
|      - | 1944 | `	/* Extract the target string */` |
|     31 | 1945 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 1946 | `	if( nArg > 1 ){` |
|     19 | 1947 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1948 | `	}` |
|     31 | 1949 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     31 | 1950 | `	if( nArg > 3 ){` |
|      3 | 1951 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      1 | 1952 | `	}` |
|     31 | 1953 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,1,bDouble);` |
|     31 | 1954 | `	return PH7_OK;` |
|     16 | 1955 | `}` |
|      - | 1956 | `/*` |
|      - | 1957 | ` * string html_entity_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1958 | ` *                           [, string $encoding = "UTF-8"]])` |
|      - | 1959 | ` *  Convert HTML entities (named — case-sensitive — and numeric, decimal or` |
|      - | 1960 | ` *  hex) back to their UTF-8 characters. The reverse of htmlentities().` |
|      - | 1961 | ` * Return` |
|      - | 1962 | ` *  The decoded string or NULL on failure.` |
|      - | 1963 | ` */` |
|     58 | 1964 | `static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1965 | `{` |
|     59 | 1966 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1967 | `	const char *zIn;` |
|      - | 1968 | `	int nLen;` |
|     59 | 1969 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1970 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1971 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1972 | `		return PH7_OK;` |
|      - | 1973 | `	}` |
|      - | 1974 | `	/* Extract the target string */` |
|     59 | 1975 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     59 | 1976 | `	if( nArg > 1 ){` |
|     27 | 1977 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     13 | 1978 | `	}` |
|     59 | 1979 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     59 | 1980 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,1);` |
|     59 | 1981 | `	return PH7_OK;` |
|     30 | 1982 | `}` |
|      - | 1983 | `/*` |
|      - | 1984 | ` * int strlen($string)` |
|      - | 1985 | ` *  return the length of the given string.` |
|      - | 1986 | ` * Parameter` |
|      - | 1987 | ` *  string: The string being measured for length.` |
|      - | 1988 | ` * Return` |
|      - | 1989 | ` *  length of the given string.` |
|      - | 1990 | ` */` |
|  11568 | 1991 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1992 | `{` |
|  11573 | 1993 | `	int iLen = 0;` |
|  11573 | 1994 | `	if( nArg > 0 ){` |
|  11573 | 1995 | `		ph7_value_to_string(apArg[0],&iLen);` |
|   5784 | 1996 | `	}` |
|      - | 1997 | `	/* String length */` |
|  11573 | 1998 | `	ph7_result_int(pCtx,iLen);` |
|  11573 | 1999 | `	return PH7_OK;` |
|      5 | 2000 | `}` |
|      - | 2001 | `/*` |
|      - | 2002 | ` * int strcmp(string $str1,string $str2)` |
|      - | 2003 | ` *  Perform a binary safe string comparison.` |
|      - | 2004 | ` * Parameter` |
|      - | 2005 | ` *  str1: The first string` |
|      - | 2006 | ` *  str2: The second string` |
|      - | 2007 | ` * Return` |
|      - | 2008 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2009 | ` *  than str2, and 0 if they are equal.` |
|      - | 2010 | ` */` |
|     72 | 2011 | `static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2012 | `{` |
|      - | 2013 | `	const char *z1,*z2;` |
|      - | 2014 | `	int n1,n2;` |
|      - | 2015 | `	int res;` |
|     73 | 2016 | `	if( nArg < 2 ){` |
|    ! 0 | 2017 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 2018 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 2019 | `		return PH7_OK;` |
|      - | 2020 | `	}` |
|      - | 2021 | `	/* Perform the comparison */` |
|     73 | 2022 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     73 | 2023 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     73 | 2024 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2025 | `	/* Comparison result */` |
|     73 | 2026 | `	ph7_result_int(pCtx,res);` |
|     73 | 2027 | `	return PH7_OK;` |
|     37 | 2028 | `}` |
|      - | 2029 | `/*` |
|      - | 2030 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 2031 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 2032 | ` * Parameter` |
|      - | 2033 | ` *  str1: The first string` |
|      - | 2034 | ` *  str2: The second string` |
|      - | 2035 | ` * Return` |
|      - | 2036 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2037 | ` *  than str2, and 0 if they are equal.` |
|      - | 2038 | ` */` |
|     18 | 2039 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2040 | `{` |
|      - | 2041 | `	const char *z1,*z2;` |
|      - | 2042 | `	int res;` |
|      - | 2043 | `	int n;` |
|     19 | 2044 | `	if( nArg < 3 ){` |
|      - | 2045 | `		/* Perform a standard comparison */` |
|    ! 0 | 2046 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 2047 | `	}` |
|      - | 2048 | `	/* Desired comparison length */` |
|     19 | 2049 | `	n  = ph7_value_to_int(apArg[2]);` |
|     19 | 2050 | `	if( n < 0 ){` |
|      - | 2051 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 2052 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2053 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 2054 | `			ph7_function_name(pCtx));` |
|      - | 2055 | `	}` |
|      - | 2056 | `	/* Perform the comparison */` |
|     17 | 2057 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     17 | 2058 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     17 | 2059 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 2060 | `	/* Comparison result */` |
|     17 | 2061 | `	ph7_result_int(pCtx,res);` |
|     17 | 2062 | `	return PH7_OK;` |
|     10 | 2063 | `}` |
|      - | 2064 | `/*` |
|      - | 2065 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 2066 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 2067 | ` * Parameter` |
|      - | 2068 | ` *  str1: The first string` |
|      - | 2069 | ` *  str2: The second string` |
|      - | 2070 | ` * Return` |
|      - | 2071 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2072 | ` *  than str2, and 0 if they are equal.` |
|      - | 2073 | ` */` |
|     14 | 2074 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2075 | `{` |
|      - | 2076 | `	const char *z1,*z2;` |
|      - | 2077 | `	int n1,n2;` |
|      - | 2078 | `	int res;` |
|     15 | 2079 | `	if( nArg < 2 ){` |
|    ! 0 | 2080 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 2081 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 2082 | `		return PH7_OK;` |
|      - | 2083 | `	}` |
|      - | 2084 | `	/* Perform the comparison */` |
|     15 | 2085 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     15 | 2086 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     15 | 2087 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2088 | `	/* Comparison result */` |
|     15 | 2089 | `	ph7_result_int(pCtx,res);` |
|     15 | 2090 | `	return PH7_OK;` |
|      8 | 2091 | `}` |
|      - | 2092 | `/*` |
|      - | 2093 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 2094 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 2095 | ` * Parameter` |
|      - | 2096 | ` *  $str1: The first string` |
|      - | 2097 | ` *  $str2: The second string` |
|      - | 2098 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 2099 | ` * Return` |
|      - | 2100 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2101 | ` *  than str2, and 0 if they are equal.` |
|      - | 2102 | ` */` |
|      8 | 2103 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2104 | `{` |
|      - | 2105 | `	const char *z1,*z2;` |
|      - | 2106 | `	int res;` |
|      - | 2107 | `	int n;` |
|      9 | 2108 | `	if( nArg < 3 ){` |
|      - | 2109 | `		/* Perform a standard comparison */` |
|    ! 0 | 2110 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 2111 | `	}` |
|      - | 2112 | `	/* Desired comparison length */` |
|      9 | 2113 | `	n  = ph7_value_to_int(apArg[2]);` |
|      9 | 2114 | `	if( n < 0 ){` |
|      - | 2115 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 2116 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2117 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 2118 | `			ph7_function_name(pCtx));` |
|      - | 2119 | `	}` |
|      - | 2120 | `	/* Perform the comparison */` |
|      7 | 2121 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|      7 | 2122 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|      7 | 2123 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 2124 | `	/* Comparison result */` |
|      7 | 2125 | `	ph7_result_int(pCtx,res);` |
|      7 | 2126 | `	return PH7_OK;` |
|      5 | 2127 | `}` |
|      - | 2128 | `/*` |
|      - | 2129 | ` * Implode context [i.e: it's private data].` |
|      - | 2130 | ` * A pointer to the following structure is forwarded` |
|      - | 2131 | ` * verbatim to the array walker callback defined below.` |
|      - | 2132 | ` */` |
|      - | 2133 | `struct implode_data {` |
|      - | 2134 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 2135 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 2136 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 2137 | `	int nSeplen;          /* Separator length */` |
|      - | 2138 | `	int bFirst;           /* TRUE if first call */` |
|      - | 2139 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 2140 | `	sxi32 rc;             /* Captured allocation rc; SXERR_MEM => the builtin raises an OOM fatal */` |
|      - | 2141 | `};` |
|      - | 2142 | `/*` |
|      - | 2143 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 2144 | ` * The following routine is invoked for each array entry passed` |
|      - | 2145 | ` * to the implode() function.` |
|      - | 2146 | ` */` |
| 141386 | 2147 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 2148 | `{` |
|  70693 | 2149 | `	SXUNUSED(pKey);` |
| 141391 | 2150 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 2151 | `	const char *zData;` |
|      - | 2152 | `	int nLen;` |
| 141391 | 2153 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 2154 | `		if( pData->nSeplen > 0 ){` |
|      3 | 2155 | `			if( !pData->bFirst ){` |
|      - | 2156 | `				/* append the separator first */` |
|      3 | 2157 | `				if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 2158 | `					pData->rc = SXERR_MEM;` |
|    ! 0 | 2159 | `					return PH7_ABORT;` |
|      - | 2160 | `				}` |
|      2 | 2161 | `			}else{` |
|    ! 0 | 2162 | `				pData->bFirst = 0;` |
|      - | 2163 | `			}` |
|      1 | 2164 | `		}` |
|      - | 2165 | `		/* Recurse */` |
|      3 | 2166 | `		pData->bFirst = 1;` |
|      3 | 2167 | `		pData->nRecCount++;` |
|      3 | 2168 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 2169 | `		pData->nRecCount--;` |
|      - | 2170 | `		/* Propagate an allocation failure surfaced deeper in the recursion. */` |
|      3 | 2171 | `		if( pData->rc != SXRET_OK ){` |
|    ! 0 | 2172 | `			return PH7_ABORT;` |
|      - | 2173 | `		}` |
|      3 | 2174 | `		return PH7_OK;` |
|      - | 2175 | `	}` |
|      - | 2176 | `	/* Extract the string representation of the entry value */` |
| 141389 | 2177 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 2178 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 141389 | 2179 | `	if( pData->bFirst ){` |
|  33079 | 2180 | `		pData->bFirst = 0;` |
| 124852 | 2181 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 2182 | `		/* append the separator first */` |
| 108303 | 2183 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 2184 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 2185 | `			return PH7_ABORT;` |
|      - | 2186 | `		}` |
|  54149 | 2187 | `	}` |
|      - | 2188 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 141389 | 2189 | `	if( nLen > 0 ){` |
| 129309 | 2190 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2191 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 2192 | `			return PH7_ABORT;` |
|      - | 2193 | `		}` |
|  64652 | 2194 | `	}` |
| 141389 | 2195 | `	return PH7_OK;` |
|  70698 | 2196 | `}` |
|      - | 2197 | `/*` |
|      - | 2198 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 2199 | ` * string implode(array $pieces,...)` |
|      - | 2200 | ` *  Join array elements with a string.` |
|      - | 2201 | ` * $glue` |
|      - | 2202 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 2203 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 2204 | ` * $pieces` |
|      - | 2205 | ` *   The array of strings to implode.` |
|      - | 2206 | ` * Return` |
|      - | 2207 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 2208 | ` *  order, with the glue string between each element.` |
|      - | 2209 | ` */` |
|  33100 | 2210 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2211 | `{` |
|      - | 2212 | `	struct implode_data imp_data;` |
|  33105 | 2213 | `	int i = 1;` |
|  33105 | 2214 | `	if( nArg < 1 ){` |
|      - | 2215 | `		/* Missing argument,return NULL */` |
|    ! 0 | 2216 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2217 | `		return PH7_OK;` |
|      - | 2218 | `	}` |
|      - | 2219 | `	/* Prepare the implode context */` |
|  33105 | 2220 | `	imp_data.pCtx = pCtx;` |
|  33105 | 2221 | `	imp_data.bRecursive = 0;` |
|  33105 | 2222 | `	imp_data.bFirst = 1;` |
|  33105 | 2223 | `	imp_data.nRecCount = 0;` |
|  33105 | 2224 | `	imp_data.rc = SXRET_OK;` |
|  33105 | 2225 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  33103 | 2226 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  16554 | 2227 | `	}else{` |
|      3 | 2228 | `		imp_data.zSep = 0;` |
|      3 | 2229 | `		imp_data.nSeplen = 0;` |
|      3 | 2230 | `		i = 0;` |
|      - | 2231 | `	}` |
|  33105 | 2232 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2233 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2234 | `	}` |
|      - | 2235 | `	/* Start the 'join' process */` |
|  66205 | 2236 | `	while( i < nArg ){` |
|  33105 | 2237 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2238 | `			/* Iterate throw array entries */` |
|  33105 | 2239 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2240 | `			/* Surface a callback allocation failure as a fatal */` |
|  33105 | 2241 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2242 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2243 | `			}` |
|  16555 | 2244 | `		}else{` |
|      - | 2245 | `			const char *zData;` |
|      - | 2246 | `			int nLen;` |
|      - | 2247 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 2248 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2249 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 2250 | `			if( imp_data.bFirst ){` |
|    ! 0 | 2251 | `				imp_data.bFirst = 0;` |
|    ! 0 | 2252 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2253 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2254 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2255 | `				}` |
|    ! 0 | 2256 | `			}` |
|      - | 2257 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 2258 | `			if( nLen > 0 ){` |
|    ! 0 | 2259 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2260 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2261 | `				}` |
|    ! 0 | 2262 | `			}` |
|      - | 2263 | `		}` |
|  33105 | 2264 | `		i++;` |
|      5 | 2265 | `	}` |
|  33105 | 2266 | `	return PH7_OK;` |
|  16555 | 2267 | `}` |
|      - | 2268 | `/*` |
|      - | 2269 | ` * Symisc eXtension:` |
|      - | 2270 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 2271 | ` * Purpose` |
|      - | 2272 | ` *  Same as implode() but recurse on arrays.` |
|      - | 2273 | ` * Example:` |
|      - | 2274 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 2275 | ` *   echo implode_recursive("/",$a);` |
|      - | 2276 | ` *   Will output` |
|      - | 2277 | ` *     usr/home/dean.` |
|      - | 2278 | ` *   While the standard implode would produce.` |
|      - | 2279 | ` *    usr/Array.` |
|      - | 2280 | ` * Parameter` |
|      - | 2281 | ` *  Refer to implode().` |
|      - | 2282 | ` * Return` |
|      - | 2283 | ` *  Refer to implode().` |
|      - | 2284 | ` */` |
|     12 | 2285 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2286 | `{` |
|      - | 2287 | `	struct implode_data imp_data;` |
|     13 | 2288 | `	int i = 1;` |
|     13 | 2289 | `	if( nArg < 1 ){` |
|      - | 2290 | `		/* Missing argument,return NULL */` |
|      3 | 2291 | `		ph7_result_null(pCtx);` |
|      3 | 2292 | `		return PH7_OK;` |
|      - | 2293 | `	}` |
|      - | 2294 | `	/* Prepare the implode context */` |
|     11 | 2295 | `	imp_data.pCtx = pCtx;` |
|     11 | 2296 | `	imp_data.bRecursive = 1;` |
|     11 | 2297 | `	imp_data.bFirst = 1;` |
|     11 | 2298 | `	imp_data.nRecCount = 0;` |
|     11 | 2299 | `	imp_data.rc = SXRET_OK;` |
|     11 | 2300 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 2301 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 2302 | `	}else{` |
|    ! 0 | 2303 | `		imp_data.zSep = 0;` |
|    ! 0 | 2304 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 2305 | `		i = 0;` |
|      - | 2306 | `	}` |
|     11 | 2307 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2308 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2309 | `	}` |
|      - | 2310 | `	/* Start the 'join' process */` |
|     21 | 2311 | `	while( i < nArg ){` |
|     11 | 2312 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2313 | `			/* Iterate throw array entries */` |
|      3 | 2314 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2315 | `			/* Surface a callback allocation failure as a fatal */` |
|      3 | 2316 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2317 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2318 | `			}` |
|      2 | 2319 | `		}else{` |
|      - | 2320 | `			const char *zData;` |
|      - | 2321 | `			int nLen;` |
|      - | 2322 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 2323 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2324 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 2325 | `			if( imp_data.bFirst ){` |
|      9 | 2326 | `				imp_data.bFirst = 0;` |
|      4 | 2327 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2328 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2329 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2330 | `				}` |
|    ! 0 | 2331 | `			}` |
|      - | 2332 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 2333 | `			if( nLen > 0 ){` |
|      9 | 2334 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2335 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2336 | `				}` |
|      4 | 2337 | `			}` |
|      - | 2338 | `		}` |
|     11 | 2339 | `		i++;` |
|      1 | 2340 | `	}` |
|     11 | 2341 | `	return PH7_OK;` |
|      7 | 2342 | `}` |
|      - | 2343 | `/*` |
|      - | 2344 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 2345 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 2346 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 2347 | ` * Parameters` |
|      - | 2348 | ` *  $delimiter` |
|      - | 2349 | ` *   The boundary string.` |
|      - | 2350 | ` * $string` |
|      - | 2351 | ` *   The input string.` |
|      - | 2352 | ` * $limit` |
|      - | 2353 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 2354 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 2355 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 2356 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 2357 | ` * Returns` |
|      - | 2358 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 2359 | ` *  on boundaries formed by the delimiter.` |
|      - | 2360 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 2361 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 2362 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 2363 | ` *  will be returned.` |
|      - | 2364 | ` * NOTE:` |
|      - | 2365 | ` *  Negative limit is not supported.` |
|      - | 2366 | ` */` |
|   6442 | 2367 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2368 | `{` |
|      - | 2369 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2370 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2371 | `	ph7_value *pArray;` |
|      - | 2372 | `	ph7_value *pValue;` |
|      - | 2373 | `	sxu32 nOfft;` |
|      - | 2374 | `	sxi32 rc;` |
|   6447 | 2375 | `	if( nArg < 2 ){` |
|      - | 2376 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2377 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2378 | `		return PH7_OK;` |
|      - | 2379 | `	}` |
|      - | 2380 | `	/* Extract the delimiter */` |
|   6447 | 2381 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   6447 | 2382 | `	if( nDelim < 1 ){` |
|      - | 2383 | `		/* Empty delimiter: PHP 8 throws a catchable ValueError. */` |
|      5 | 2384 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2385 | `			"explode(): Argument #1 ($separator) must not be empty");` |
|      - | 2386 | `	}` |
|      - | 2387 | `	/* Extract the string */` |
|   6443 | 2388 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   6443 | 2389 | `	if( nStrlen < 1 ){` |
|      - | 2390 | `		/* Empty string: normally an array with a single empty element (PHP behavior).` |
|      - | 2391 | `		 * A negative limit drops the last -limit components, so the sole empty` |
|      - | 2392 | `		 * component is dropped and the result is an empty array. */` |
|      7 | 2393 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      7 | 2394 | `		if( pArrayTmp == 0 ){` |
|      - | 2395 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 2396 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2397 | `			return PH7_OK;` |
|      - | 2398 | `		}` |
|      7 | 2399 | `		if( !(nArg > 2 && ph7_value_to_int(apArg[2]) < 0) ){` |
|      5 | 2400 | `			ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      5 | 2401 | `			if( pValueTmp == 0 ){` |
|      - | 2402 | `				/* Out of memory,return FALSE */` |
|    ! 0 | 2403 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 2404 | `				return PH7_OK;` |
|      - | 2405 | `			}` |
|      5 | 2406 | `			ph7_value_string(pValueTmp, "", 0);` |
|      5 | 2407 | `			if( ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp) != SXRET_OK ){` |
|    ! 0 | 2408 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2409 | `			}` |
|      2 | 2410 | `		}` |
|      7 | 2411 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      7 | 2412 | `		return PH7_OK;` |
|      - | 2413 | `	}` |
|      - | 2414 | `	/* Point to the end of the string */` |
|   6437 | 2415 | `	zEnd = &zString[nStrlen];` |
|      - | 2416 | `	/* Create the array */` |
|   6437 | 2417 | `	pArray =  ph7_context_new_array(pCtx);` |
|   6437 | 2418 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   6437 | 2419 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2420 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2421 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2422 | `		return PH7_OK;` |
|      - | 2423 | `	}` |
|      - | 2424 | `	/* Set a defualt limit */` |
|   6437 | 2425 | `	iLimit = SXI32_HIGH;` |
|   6437 | 2426 | `	if( nArg > 2 ){` |
|     38 | 2427 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     38 | 2428 | `		if( iLimit < 0 ){` |
|      - | 2429 | `			/* Negative limit: keep all components except the last -iLimit (PHP).` |
|      - | 2430 | `			 * Pre-count the components (delimiters + 1), then emit only the first` |
|      - | 2431 | `			 * nKeep CLEAN components — no trailing-remainder merge (the difference` |
|      - | 2432 | `			 * from the positive path). nKeep <= 0 drops everything -> empty array. */` |
|     17 | 2433 | `			int nTotal = 1,nKeep;` |
|     17 | 2434 | `			const char *zScan = zString;` |
|      - | 2435 | `			sxu32 nScanOfft;` |
|     57 | 2436 | `			while( SyBlobSearch(zScan,(sxu32)(zEnd - zScan),zDelim,nDelim,&nScanOfft) == SXRET_OK ){` |
|     41 | 2437 | `				nTotal++;` |
|     41 | 2438 | `				zScan = &zScan[nScanOfft + nDelim];` |
|      1 | 2439 | `			}` |
|     17 | 2440 | `			nKeep = nTotal + iLimit; /* iLimit < 0, so this is nTotal - (-iLimit) */` |
|     49 | 2441 | `			while( nKeep > (int)ph7_array_count(pArray)` |
|     39 | 2442 | `				&& SyBlobSearch(zString,(sxu32)(zEnd - zString),zDelim,nDelim,&nOfft) == SXRET_OK ){` |
|      - | 2443 | `				/* Emit the next clean component */` |
|     23 | 2444 | `				zCur = &zString[nOfft];` |
|     23 | 2445 | `				ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|     23 | 2446 | `				if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2447 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2448 | `				}` |
|     23 | 2449 | `				zString = &zCur[nDelim];` |
|     23 | 2450 | `				ph7_value_reset_string_cursor(pValue);` |
|      1 | 2451 | `			}` |
|     17 | 2452 | `			ph7_result_value(pCtx,pArray);` |
|     17 | 2453 | `			return PH7_OK;` |
|      - | 2454 | `		}` |
|     22 | 2455 | `		if( iLimit == 0 ){` |
|      5 | 2456 | `			iLimit = 1;` |
|      2 | 2457 | `		}` |
|     22 | 2458 | `		iLimit--;` |
|      9 | 2459 | `	}` |
|      - | 2460 | `	/* Start exploding */` |
|  76517 | 2461 | `	for(;;){` |
| 153039 | 2462 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 153039 | 2463 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2464 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   6421 | 2465 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   6421 | 2466 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2467 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2468 | `			}` |
|   6421 | 2469 | `			break;` |
|      - | 2470 | `		}` |
|      - | 2471 | `		/* Point to the desired offset */` |
| 146623 | 2472 | `		zCur = &zString[nOfft];` |
|      - | 2473 | `		/* Perform the store operation (may be empty) */` |
| 146623 | 2474 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 146623 | 2475 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2476 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2477 | `		}` |
|      - | 2478 | `		/* Point beyond the delimiter */` |
| 146623 | 2479 | `		zString = &zCur[nDelim];` |
|      - | 2480 | `		/* Reset the cursor */` |
| 146623 | 2481 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 2482 | `	}` |
|      - | 2483 | `	/* Return the freshly created array */` |
|   6421 | 2484 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2485 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2486 | `	 * released as soon we return from this foregin function.` |
|      - | 2487 | `	 */` |
|   6421 | 2488 | `	return PH7_OK;` |
|   3226 | 2489 | `}` |
|      - | 2490 | `/*` |
|      - | 2491 | ` * string trim(string $str[,string $charlist ])` |
|      - | 2492 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2493 | ` * Parameters` |
|      - | 2494 | ` *  $str` |
|      - | 2495 | ` *   The string that will be trimmed.` |
|      - | 2496 | ` * $charlist` |
|      - | 2497 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2498 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2499 | ` *   With .. you can specify a range of characters.` |
|      - | 2500 | ` * Returns.` |
|      - | 2501 | ` *  Thr processed string.` |
|      - | 2502 | ` * NOTE:` |
|      - | 2503 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2504 | ` */` |
|  14538 | 2505 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2506 | `{` |
|      - | 2507 | `	const char *zString;` |
|      - | 2508 | `	int nLen;` |
|  14543 | 2509 | `	if( nArg < 1 ){` |
|      - | 2510 | `		/* Missing arguments,return null */` |
|    ! 0 | 2511 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2512 | `		return PH7_OK;` |
|      - | 2513 | `	}` |
|      - | 2514 | `	/* Extract the target string */` |
|  14543 | 2515 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  14543 | 2516 | `	if( nLen < 1 ){` |
|      - | 2517 | `		/* Empty string,return */` |
|   1305 | 2518 | `		ph7_result_string(pCtx,"",0);` |
|   1305 | 2519 | `		return PH7_OK;` |
|      - | 2520 | `	}` |
|      - | 2521 | `	/* Start the trim process */` |
|  13243 | 2522 | `	if( nArg < 2 ){` |
|      - | 2523 | `		SyString sStr;` |
|      - | 2524 | `		/* Remove white spaces and NUL bytes */` |
|  13213 | 2525 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  33147 | 2526 | `		SyStringFullTrimSafe(&sStr);` |
|  13213 | 2527 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   6609 | 2528 | `	}else{` |
|      - | 2529 | `		/* Char list */` |
|      - | 2530 | `		const char *zList;` |
|      - | 2531 | `		int nListlen;` |
|     33 | 2532 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     33 | 2533 | `		if( nListlen < 1 ){` |
|      - | 2534 | `			/* Return the string unchanged */` |
|      6 | 2535 | `			ph7_result_string(pCtx,zString,nLen);` |
|      4 | 2536 | `		}else{` |
|      - | 2537 | `			char aMask[256];` |
|     29 | 2538 | `			const char *zEnd = &zString[nLen];` |
|     29 | 2539 | `			const char *zCur = zString;` |
|     29 | 2540 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2541 | `			/* Left trim */` |
|     79 | 2542 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     53 | 2543 | `				zCur++;` |
|      3 | 2544 | `			}` |
|      - | 2545 | `			/* Right trim */` |
|     79 | 2546 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     53 | 2547 | `				zEnd--;` |
|      3 | 2548 | `			}` |
|     29 | 2549 | `			if( zCur >= zEnd ){` |
|      - | 2550 | `				/* Return the empty string */` |
|    ! 0 | 2551 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2552 | `			}else{` |
|     29 | 2553 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2554 | `			}` |
|      - | 2555 | `		}` |
|      - | 2556 | `	}` |
|  13243 | 2557 | `	return PH7_OK;` |
|   7274 | 2558 | `}` |
|      - | 2559 | `/*` |
|      - | 2560 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 2561 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 2562 | ` * Parameters` |
|      - | 2563 | ` *  $str` |
|      - | 2564 | ` *   The string that will be trimmed.` |
|      - | 2565 | ` * $charlist` |
|      - | 2566 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2567 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2568 | ` *   With .. you can specify a range of characters.` |
|      - | 2569 | ` * Returns.` |
|      - | 2570 | ` *  Thr processed string.` |
|      - | 2571 | ` * NOTE:` |
|      - | 2572 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2573 | ` */` |
|     28 | 2574 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2575 | `{` |
|      - | 2576 | `	const char *zString;` |
|      - | 2577 | `	int nLen;` |
|     31 | 2578 | `	if( nArg < 1 ){` |
|      - | 2579 | `		/* Missing arguments,return null */` |
|    ! 0 | 2580 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2581 | `		return PH7_OK;` |
|      - | 2582 | `	}` |
|      - | 2583 | `	/* Extract the target string */` |
|     31 | 2584 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 2585 | `	if( nLen < 1 ){` |
|      - | 2586 | `		/* Empty string,return */` |
|      5 | 2587 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2588 | `		return PH7_OK;` |
|      - | 2589 | `	}` |
|      - | 2590 | `	/* Start the trim process */` |
|     27 | 2591 | `	if( nArg < 2 ){` |
|      - | 2592 | `		SyString sStr;` |
|      - | 2593 | `		/* Remove white spaces and NUL bytes*/` |
|     17 | 2594 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     41 | 2595 | `		SyStringRightTrimSafe(&sStr);` |
|     17 | 2596 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      9 | 2597 | `	}else{` |
|      - | 2598 | `		/* Char list */` |
|      - | 2599 | `		const char *zList;` |
|      - | 2600 | `		int nListlen;` |
|     11 | 2601 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     11 | 2602 | `		if( nListlen < 1 ){` |
|      - | 2603 | `			/* Return the string unchanged */` |
|    ! 0 | 2604 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 2605 | `		}else{` |
|      - | 2606 | `			char aMask[256];` |
|     11 | 2607 | `			const char *zEnd = &zString[nLen];` |
|     11 | 2608 | `			const char *zCur = zString;` |
|     11 | 2609 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2610 | `			/* Right trim */` |
|     29 | 2611 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     20 | 2612 | `				zEnd--;` |
|      2 | 2613 | `			}` |
|     11 | 2614 | `			if( zEnd <= zCur ){` |
|      - | 2615 | `				/* Return the empty string */` |
|    ! 0 | 2616 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2617 | `			}else{` |
|     11 | 2618 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2619 | `			}` |
|      - | 2620 | `		}` |
|      - | 2621 | `	}` |
|     27 | 2622 | `	return PH7_OK;` |
|     17 | 2623 | `}` |
|      - | 2624 | `/*` |
|      - | 2625 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 2626 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2627 | ` * Parameters` |
|      - | 2628 | ` *  $str` |
|      - | 2629 | ` *   The string that will be trimmed.` |
|      - | 2630 | ` * $charlist` |
|      - | 2631 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2632 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2633 | ` *   With .. you can specify a range of characters.` |
|      - | 2634 | ` * Returns.` |
|      - | 2635 | ` *  Thr processed string.` |
|      - | 2636 | ` * NOTE:` |
|      - | 2637 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2638 | ` */` |
|     12 | 2639 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2640 | `{` |
|      - | 2641 | `	const char *zString;` |
|      - | 2642 | `	int nLen;` |
|     14 | 2643 | `	if( nArg < 1 ){` |
|      - | 2644 | `		/* Missing arguments,return null */` |
|    ! 0 | 2645 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2646 | `		return PH7_OK;` |
|      - | 2647 | `	}` |
|      - | 2648 | `	/* Extract the target string */` |
|     14 | 2649 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     14 | 2650 | `	if( nLen < 1 ){` |
|      - | 2651 | `		/* Empty string,return */` |
|    ! 0 | 2652 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2653 | `		return PH7_OK;` |
|      - | 2654 | `	}` |
|      - | 2655 | `	/* Start the trim process */` |
|     14 | 2656 | `	if( nArg < 2 ){` |
|      - | 2657 | `		SyString sStr;` |
|      - | 2658 | `		/* Remove white spaces and NUL byte */` |
|      3 | 2659 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|      8 | 2660 | `		SyStringLeftTrimSafe(&sStr);` |
|      3 | 2661 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      2 | 2662 | `	}else{` |
|      - | 2663 | `		/* Char list */` |
|      - | 2664 | `		const char *zList;` |
|      - | 2665 | `		int nListlen;` |
|     12 | 2666 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     12 | 2667 | `		if( nListlen < 1 ){` |
|      - | 2668 | `			/* Return the string unchanged */` |
|      3 | 2669 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2670 | `		}else{` |
|      - | 2671 | `			char aMask[256];` |
|     10 | 2672 | `			const char *zEnd = &zString[nLen];` |
|     10 | 2673 | `			const char *zCur = zString;` |
|     10 | 2674 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2675 | `			/* Left trim */` |
|     28 | 2676 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     20 | 2677 | `				zCur++;` |
|      2 | 2678 | `			}` |
|     10 | 2679 | `			if( zCur >= zEnd ){` |
|      - | 2680 | `				/* Return the empty string */` |
|    ! 0 | 2681 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2682 | `			}else{` |
|     10 | 2683 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2684 | `			}` |
|      - | 2685 | `		}` |
|      - | 2686 | `	}` |
|     14 | 2687 | `	return PH7_OK;` |
|      8 | 2688 | `}` |
|      - | 2689 | `/*` |
|      - | 2690 | ` * string strtolower(string $str)` |
|      - | 2691 | ` *  Make a string lowercase.` |
|      - | 2692 | ` * Parameters` |
|      - | 2693 | ` *  $str` |
|      - | 2694 | ` *   The input string.` |
|      - | 2695 | ` * Returns.` |
|      - | 2696 | ` *  The lowercased string.` |
|      - | 2697 | ` */` |
|  33084 | 2698 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2699 | `{` |
|      - | 2700 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2701 | `	int nLen;` |
|  33089 | 2702 | `	if( nArg < 1 ){` |
|      - | 2703 | `		/* Missing arguments,return null */` |
|    ! 0 | 2704 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2705 | `		return PH7_OK;` |
|      - | 2706 | `	}` |
|      - | 2707 | `	/* Extract the target string */` |
|  33089 | 2708 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  33089 | 2709 | `	if( nLen < 1 ){` |
|      - | 2710 | `		/* Empty string,return */` |
|      3 | 2711 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2712 | `		return PH7_OK;` |
|      - | 2713 | `	}` |
|      - | 2714 | `	/* Perform the requested operation */` |
|  33087 | 2715 | `	zEnd = &zString[nLen];` |
| 104444 | 2716 | `	for(;;){` |
| 208893 | 2717 | `		if( zString >= zEnd ){` |
|      - | 2718 | `			/* No more input,break immediately */` |
|  33087 | 2719 | `			break;` |
|      - | 2720 | `		}` |
| 175811 | 2721 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2722 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2723 | `			zCur = zString;` |
|    ! 0 | 2724 | `			zString++;` |
|    ! 0 | 2725 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2726 | `				zString++;` |
|    ! 0 | 2727 | `			}` |
|      - | 2728 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2729 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2730 | `		}else{` |
| 175811 | 2731 | `			int c = zString[0];` |
| 175811 | 2732 | `			if( SyisUpper(c) ){` |
| 173465 | 2733 | `				c = SyToLower(zString[0]);` |
|  86730 | 2734 | `			}` |
|      - | 2735 | `			/* Append character */` |
| 175811 | 2736 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2737 | `			/* Advance the cursor */` |
| 175811 | 2738 | `			zString++;` |
|      - | 2739 | `		}` |
|      5 | 2740 | `	}` |
|  33087 | 2741 | `	return PH7_OK;` |
|  16547 | 2742 | `}` |
|      - | 2743 | `/*` |
|      - | 2744 | ` * string strtolower(string $str)` |
|      - | 2745 | ` *  Make a string uppercase.` |
|      - | 2746 | ` * Parameters` |
|      - | 2747 | ` *  $str` |
|      - | 2748 | ` *   The input string.` |
|      - | 2749 | ` * Returns.` |
|      - | 2750 | ` *  The uppercased string.` |
|      - | 2751 | ` */` |
|     48 | 2752 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2753 | `{` |
|      - | 2754 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2755 | `	int nLen;` |
|     52 | 2756 | `	if( nArg < 1 ){` |
|      - | 2757 | `		/* Missing arguments,return null */` |
|    ! 0 | 2758 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2759 | `		return PH7_OK;` |
|      - | 2760 | `	}` |
|      - | 2761 | `	/* Extract the target string */` |
|     52 | 2762 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     52 | 2763 | `	if( nLen < 1 ){` |
|      - | 2764 | `		/* Empty string,return */` |
|      3 | 2765 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2766 | `		return PH7_OK;` |
|      - | 2767 | `	}` |
|      - | 2768 | `	/* Perform the requested operation */` |
|     50 | 2769 | `	zEnd = &zString[nLen];` |
|    111 | 2770 | `	for(;;){` |
|    226 | 2771 | `		if( zString >= zEnd ){` |
|      - | 2772 | `			/* No more input,break immediately */` |
|     50 | 2773 | `			break;` |
|      - | 2774 | `		}` |
|    180 | 2775 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2776 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2777 | `			zCur = zString;` |
|    ! 0 | 2778 | `			zString++;` |
|    ! 0 | 2779 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2780 | `				zString++;` |
|    ! 0 | 2781 | `			}` |
|      - | 2782 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2783 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2784 | `		}else{` |
|    180 | 2785 | `			int c = zString[0];` |
|    180 | 2786 | `			if( SyisLower(c) ){` |
|    174 | 2787 | `				c = SyToUpper(zString[0]);` |
|     85 | 2788 | `			}` |
|      - | 2789 | `			/* Append character */` |
|    180 | 2790 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2791 | `			/* Advance the cursor */` |
|    180 | 2792 | `			zString++;` |
|      - | 2793 | `		}` |
|      4 | 2794 | `	}` |
|     50 | 2795 | `	return PH7_OK;` |
|     28 | 2796 | `}` |
|      - | 2797 | `/*` |
|      - | 2798 | ` * string ucfirst(string $str)` |
|      - | 2799 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2800 | ` *  character is alphabetic.` |
|      - | 2801 | ` * Parameters` |
|      - | 2802 | ` *  $str` |
|      - | 2803 | ` *   The input string.` |
|      - | 2804 | ` * Returns.` |
|      - | 2805 | ` *  The processed string.` |
|      - | 2806 | ` */` |
|      4 | 2807 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2808 | `{` |
|      - | 2809 | `	const char *zString,*zEnd;` |
|      - | 2810 | `	int nLen,c;` |
|      5 | 2811 | `	if( nArg < 1 ){` |
|      - | 2812 | `		/* Missing arguments,return null */` |
|    ! 0 | 2813 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2814 | `		return PH7_OK;` |
|      - | 2815 | `	}` |
|      - | 2816 | `	/* Extract the target string */` |
|      5 | 2817 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2818 | `	if( nLen < 1 ){` |
|      - | 2819 | `		/* Empty string,return */` |
|      3 | 2820 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2821 | `		return PH7_OK;` |
|      - | 2822 | `	}` |
|      - | 2823 | `	/* Perform the requested operation */` |
|      3 | 2824 | `	zEnd = &zString[nLen];` |
|      3 | 2825 | `	c = zString[0];` |
|      3 | 2826 | `	if( SyisLower(c) ){` |
|      3 | 2827 | `		c = SyToUpper(c);` |
|      1 | 2828 | `	}` |
|      - | 2829 | `	/* Append the first character */` |
|      3 | 2830 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2831 | `	zString++;` |
|      3 | 2832 | `	if( zString < zEnd ){` |
|      - | 2833 | `		/* Append the rest of the input verbatim */` |
|      3 | 2834 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2835 | `	}` |
|      3 | 2836 | `	return PH7_OK;` |
|      3 | 2837 | `}` |
|      - | 2838 | `/*` |
|      - | 2839 | ` * string lcfirst(string $str)` |
|      - | 2840 | ` *  Make a string's first character lowercase.` |
|      - | 2841 | ` * Parameters` |
|      - | 2842 | ` *  $str` |
|      - | 2843 | ` *   The input string.` |
|      - | 2844 | ` * Returns.` |
|      - | 2845 | ` *  The processed string.` |
|      - | 2846 | ` */` |
|      4 | 2847 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2848 | `{` |
|      - | 2849 | `	const char *zString,*zEnd;` |
|      - | 2850 | `	int nLen,c;` |
|      5 | 2851 | `	if( nArg < 1 ){` |
|      - | 2852 | `		/* Missing arguments,return null */` |
|    ! 0 | 2853 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2854 | `		return PH7_OK;` |
|      - | 2855 | `	}` |
|      - | 2856 | `	/* Extract the target string */` |
|      5 | 2857 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2858 | `	if( nLen < 1 ){` |
|      - | 2859 | `		/* Empty string,return */` |
|      3 | 2860 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2861 | `		return PH7_OK;` |
|      - | 2862 | `	}` |
|      - | 2863 | `	/* Perform the requested operation */` |
|      3 | 2864 | `	zEnd = &zString[nLen];` |
|      3 | 2865 | `	c = zString[0];` |
|      3 | 2866 | `	if( SyisUpper(c) ){` |
|      3 | 2867 | `		c = SyToLower(c);` |
|      1 | 2868 | `	}` |
|      - | 2869 | `	/* Append the first character */` |
|      3 | 2870 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2871 | `	zString++;` |
|      3 | 2872 | `	if( zString < zEnd ){` |
|      - | 2873 | `		/* Append the rest of the input verbatim */` |
|      3 | 2874 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2875 | `	}` |
|      3 | 2876 | `	return PH7_OK;` |
|      3 | 2877 | `}` |
|      - | 2878 | `/*` |
|      - | 2879 | ` * int ord(string $string)` |
|      - | 2880 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 2881 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 2882 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 2883 | ` * Parameters` |
|      - | 2884 | ` *  $string` |
|      - | 2885 | ` *   The input string.` |
|      - | 2886 | ` * Returns` |
|      - | 2887 | ` *  The ASCII value as an integer.` |
|      - | 2888 | ` */` |
|     56 | 2889 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2890 | `{` |
|      - | 2891 | `	const char *zString;` |
|      - | 2892 | `	int nLen,c;` |
|      - | 2893 | `	/* PHP requires exactly one argument. */` |
|     59 | 2894 | `	if( nArg != 1 ){` |
|      8 | 2895 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2896 | `			"ArgumentCountError",` |
|      - | 2897 | `			"ord() expects exactly 1 argument, %d given",` |
|      2 | 2898 | `			nArg` |
|      - | 2899 | `			);` |
|      - | 2900 | `	}` |
|      - | 2901 | `	/* Passing null is deprecated (E_DEPRECATED).  PHP emits this before` |
|      - | 2902 | `	 * the empty-string deprecation, so we check null first. */` |
|     53 | 2903 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 2904 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2905 | `			"ord(): Passing null to parameter #1 ($character) "` |
|      - | 2906 | `			"of type string is deprecated"` |
|      - | 2907 | `			);` |
|      1 | 2908 | `	}` |
|      - | 2909 | `	/* Extract the target string */` |
|     53 | 2910 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     53 | 2911 | `	if( nLen < 1 ){` |
|      - | 2912 | `		/* Empty string is deprecated (E_DEPRECATED). */` |
|      5 | 2913 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2914 | `			"ord(): Providing an empty string is deprecated"` |
|      - | 2915 | `			);` |
|      5 | 2916 | `		ph7_result_int(pCtx,0);` |
|      5 | 2917 | `		return PH7_OK;` |
|      - | 2918 | `	}` |
|      - | 2919 | `	/* A string longer than one byte is deprecated (E_DEPRECATED). */` |
|     49 | 2920 | `	if( nLen > 1 ){` |
|      7 | 2921 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2922 | `			"ord(): Providing a string that is not one byte long "` |
|      - | 2923 | `			"is deprecated. Use ord($str[0]) instead"` |
|      - | 2924 | `			);` |
|      3 | 2925 | `	}` |
|      - | 2926 | `	/* Extract the ASCII value of the first character */` |
|     49 | 2927 | `	c = (unsigned char)zString[0];` |
|      - | 2928 | `	/* Return that value */` |
|     49 | 2929 | `	ph7_result_int(pCtx,c);` |
|     49 | 2930 | `	return PH7_OK;` |
|     31 | 2931 | `}` |
|      - | 2932 | `/*` |
|      - | 2933 | ` * string chr(int $codepoint)` |
|      - | 2934 | ` *  Returns a one-character string containing the character specified` |
|      - | 2935 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 2936 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 2937 | ` * Parameters` |
|      - | 2938 | ` *  $codepoint` |
|      - | 2939 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 2940 | ` *   will be constrained to a single byte.` |
|      - | 2941 | ` * Returns` |
|      - | 2942 | ` *  A single-character string.` |
|      - | 2943 | ` */` |
|   6486 | 2944 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2945 | `{` |
|      - | 2946 | `	int c;` |
|      - | 2947 | `	unsigned char ch;` |
|      - | 2948 | `	/* PHP requires exactly one argument. */` |
|   6489 | 2949 | `	if( nArg != 1 ){` |
|      8 | 2950 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2951 | `			"ArgumentCountError",` |
|      - | 2952 | `			"chr() expects exactly 1 argument, %d given",` |
|      2 | 2953 | `			nArg` |
|      - | 2954 | `			);` |
|      - | 2955 | `	}` |
|      - | 2956 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 2957 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 2958 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 2959 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|   6483 | 2960 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      - | 2961 | `		char zBuf[120];` |
|      4 | 2962 | `		SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 2963 | `			"Implicit conversion from float %g to int loses precision",` |
|      1 | 2964 | `			ph7_value_to_double(apArg[0])` |
|      - | 2965 | `			);` |
|      3 | 2966 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);` |
|      1 | 2967 | `	}` |
|      - | 2968 | `	/* Extract the codepoint. */` |
|   6483 | 2969 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 2970 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 2971 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 2972 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 2973 | `	 * name to avoid the API double-prefixing it. */` |
|   6483 | 2974 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 2975 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 2976 | `			E_DEPRECATED,` |
|      - | 2977 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 2978 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 2979 | `			"The value used will be constrained using % 256"` |
|      - | 2980 | `			);` |
|      2 | 2981 | `	}` |
|      - | 2982 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 2983 | `	 * when taking the address of a wider int. */` |
|   6483 | 2984 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 2985 | `	/* Return the specified character */` |
|   6483 | 2986 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|   6483 | 2987 | `	return PH7_OK;` |
|   3246 | 2988 | `}` |
|      - | 2989 | `/*` |
|      - | 2990 | ` * Binary to hex consumer callback.` |
|      - | 2991 | ` * This callback is the default consumer used by the hash functions` |
|      - | 2992 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 2993 | ` */` |
|   3118 | 2994 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 2995 | `{` |
|      - | 2996 | `	/* Append hex chunk verbatim */` |
|   3120 | 2997 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   3120 | 2998 | `	return SXRET_OK;` |
|      2 | 2999 | `}` |
|      - | 3000 |  |
|      - | 3001 | `/*` |
|      - | 3002 | ` * string bin2hex(string $str)` |
|      - | 3003 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 3004 | ` * Parameters` |
|      - | 3005 | ` *  $str` |
|      - | 3006 | ` *   The input string.` |
|      - | 3007 | ` * Returns.` |
|      - | 3008 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 3009 | ` */` |
|    138 | 3010 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3011 | `{` |
|      - | 3012 | `	const char *zString;` |
|      - | 3013 | `	int nLen;` |
|      - | 3014 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|    143 | 3015 | `	if( nArg != 1 ){` |
|      8 | 3016 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3017 | `			"ArgumentCountError",` |
|      - | 3018 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|      2 | 3019 | `			nArg` |
|      - | 3020 | `			);` |
|      - | 3021 | `	}` |
|      - | 3022 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 3023 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 3024 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 3025 | `	 */` |
|    204 | 3026 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|    130 | 3027 | `		( ph7_value_is_object(apArg[0]) &&` |
|      3 | 3028 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|      2 | 3029 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|      1 | 3030 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 3031 | `		)` |
|      - | 3032 | `	){` |
|      9 | 3033 | `		const char *zType = ph7_type_name(apArg[0]);` |
|      9 | 3034 | `		if( ph7_value_is_object(apArg[0]) ){` |
|      3 | 3035 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      3 | 3036 | `			if( pInst && pInst->pClass ){` |
|      3 | 3037 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      1 | 3038 | `			}` |
|      1 | 3039 | `		}` |
|     12 | 3040 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3041 | `			"TypeError",` |
|      - | 3042 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|      3 | 3043 | `			zType` |
|      - | 3044 | `			);` |
|      - | 3045 | `	}` |
|      - | 3046 | `	/* Extract the target string */` |
|    130 | 3047 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    130 | 3048 | `	if( nLen < 1 ){` |
|      - | 3049 | `		/* Empty string,return */` |
|     13 | 3050 | `		ph7_result_string(pCtx,"",0);` |
|     13 | 3051 | `		return PH7_OK;` |
|      - | 3052 | `	}` |
|      - | 3053 | `	/* Perform the requested operation */` |
|    118 | 3054 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|    118 | 3055 | `	return PH7_OK;` |
|     74 | 3056 | `}` |
|      - | 3057 |  |
|      - | 3058 | `/* Search callback signature */` |
|      - | 3059 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 3060 | `/*` |
|      - | 3061 | ` * Case-insensitive pattern match.` |
|      - | 3062 | ` * Brute force is the default search method used here.` |
|      - | 3063 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 3064 | ` * well for short/medium texts on modern hardware.` |
|      - | 3065 | ` */` |
|    276 | 3066 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      1 | 3067 | `{` |
|    277 | 3068 | `	const char *zpIn = (const char *)pPattern;` |
|    277 | 3069 | `	const char *zIn = (const char *)pText;` |
|    277 | 3070 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    277 | 3071 | `	const char *zEnd = &zIn[nLen];` |
|      - | 3072 | `	const char *zPtr,*zPtr2;` |
|      - | 3073 | `	int c,d;` |
|    277 | 3074 | `	if( iPatLen > nLen ){` |
|      - | 3075 | `		/* Don't bother processing */` |
|     67 | 3076 | `		return SXERR_NOTFOUND;` |
|      - | 3077 | `	}` |
|    783 | 3078 | `	for(;;){` |
|   1567 | 3079 | `		if( zIn >= zEnd ){` |
|    171 | 3080 | `			break;` |
|      - | 3081 | `		}` |
|   1397 | 3082 | `		c = SyToLower(zIn[0]);` |
|   1397 | 3083 | `		d = SyToLower(zpIn[0]);` |
|   1397 | 3084 | `		if( c == d ){` |
|    159 | 3085 | `			zPtr   = &zIn[1];` |
|    159 | 3086 | `			zPtr2  = &zpIn[1];` |
|    130 | 3087 | `			for(;;){` |
|    261 | 3088 | `				if( zPtr2 >= zpEnd ){` |
|      - | 3089 | `					/* Pattern found */` |
|     41 | 3090 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 3091 | `					return SXRET_OK;` |
|      - | 3092 | `				}` |
|    221 | 3093 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 3094 | `					break;` |
|      - | 3095 | `				}` |
|    221 | 3096 | `				c = SyToLower(zPtr[0]);` |
|    221 | 3097 | `				d = SyToLower(zPtr2[0]);` |
|    221 | 3098 | `				if( c != d ){` |
|    119 | 3099 | `					break;` |
|      - | 3100 | `				}` |
|    103 | 3101 | `				zPtr++; zPtr2++;` |
|      1 | 3102 | `			}` |
|     59 | 3103 | `		}` |
|   1357 | 3104 | `		zIn++;` |
|      1 | 3105 | `	}` |
|      - | 3106 | `	/* Pattern not found */` |
|    171 | 3107 | `	return SXERR_NOTFOUND;` |
|    139 | 3108 | `}` |
|      - | 3109 | `/*` |
|      - | 3110 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3111 | ` *  Find the first occurrence of a string.` |
|      - | 3112 | ` * Parameters` |
|      - | 3113 | ` *  $haystack` |
|      - | 3114 | ` *   The input string.` |
|      - | 3115 | ` * $needle` |
|      - | 3116 | ` *   Search pattern (must be a string).` |
|      - | 3117 | ` * $before_needle` |
|      - | 3118 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3119 | ` *   of the needle (excluding the needle).` |
|      - | 3120 | ` * Return` |
|      - | 3121 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3122 | ` */` |
|      6 | 3123 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3124 | `{` |
|      7 | 3125 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3126 | `	const char *zBlob,*zPattern;` |
|      - | 3127 | `	int nLen,nPatLen;` |
|      - | 3128 | `	sxu32 nOfft;` |
|      - | 3129 | `	sxi32 rc;` |
|      7 | 3130 | `	if( nArg < 2 ){` |
|      - | 3131 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3132 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3133 | `		return PH7_OK;` |
|      - | 3134 | `	}` |
|      - | 3135 | `	/* Extract the needle and the haystack */` |
|      7 | 3136 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 3137 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 3138 | `	nOfft = 0; /* cc warning */` |
|      9 | 3139 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3140 | `		int before = 0;` |
|      - | 3141 | `		/* Perform the lookup */` |
|      5 | 3142 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3143 | `		if( rc != SXRET_OK ){` |
|      - | 3144 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3145 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3146 | `			return PH7_OK;` |
|      - | 3147 | `		}` |
|      - | 3148 | `		/* Return the portion of the string */` |
|      5 | 3149 | `		if( nArg > 2 ){` |
|      3 | 3150 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3151 | `		}` |
|      5 | 3152 | `		if( before ){` |
|      3 | 3153 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3154 | `		}else{` |
|      3 | 3155 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3156 | `		}` |
|      3 | 3157 | `	}else{` |
|      3 | 3158 | `		ph7_result_bool(pCtx,0);` |
|      - | 3159 | `	}` |
|      7 | 3160 | `	return PH7_OK;` |
|      4 | 3161 | `}` |
|      - | 3162 | `/*` |
|      - | 3163 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3164 | ` *  Case-insensitive strstr().` |
|      - | 3165 | ` * Parameters` |
|      - | 3166 | ` *  $haystack` |
|      - | 3167 | ` *   The input string.` |
|      - | 3168 | ` * $needle` |
|      - | 3169 | ` *   Search pattern (must be a string).` |
|      - | 3170 | ` * $before_needle` |
|      - | 3171 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3172 | ` *   of the needle (excluding the needle).` |
|      - | 3173 | ` * Return` |
|      - | 3174 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3175 | ` */` |
|      4 | 3176 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3177 | `{` |
|      5 | 3178 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3179 | `	const char *zBlob,*zPattern;` |
|      - | 3180 | `	int nLen,nPatLen;` |
|      - | 3181 | `	sxu32 nOfft;` |
|      - | 3182 | `	sxi32 rc;` |
|      5 | 3183 | `	if( nArg < 2 ){` |
|      - | 3184 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3185 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3186 | `		return PH7_OK;` |
|      - | 3187 | `	}` |
|      - | 3188 | `	/* Extract the needle and the haystack */` |
|      5 | 3189 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3190 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 3191 | `	nOfft = 0; /* cc warning */` |
|      7 | 3192 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3193 | `		int before = 0;` |
|      - | 3194 | `		/* Perform the lookup */` |
|      5 | 3195 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3196 | `		if( rc != SXRET_OK ){` |
|      - | 3197 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3198 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3199 | `			return PH7_OK;` |
|      - | 3200 | `		}` |
|      - | 3201 | `		/* Return the portion of the string */` |
|      5 | 3202 | `		if( nArg > 2 ){` |
|      3 | 3203 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3204 | `		}` |
|      5 | 3205 | `		if( before ){` |
|      3 | 3206 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3207 | `		}else{` |
|      3 | 3208 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3209 | `		}` |
|      3 | 3210 | `	}else{` |
|    ! 0 | 3211 | `		ph7_result_bool(pCtx,0);` |
|      - | 3212 | `	}` |
|      5 | 3213 | `	return PH7_OK;` |
|      3 | 3214 | `}` |
|      - | 3215 | `/*` |
|      - | 3216 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3217 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 3218 | ` * Parameters` |
|      - | 3219 | ` *  $haystack` |
|      - | 3220 | ` *   The input string.` |
|      - | 3221 | ` * $needle` |
|      - | 3222 | ` *   Search pattern (must be a string).` |
|      - | 3223 | ` * $offset` |
|      - | 3224 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3225 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3226 | ` *   of haystack.` |
|      - | 3227 | ` * Return` |
|      - | 3228 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3229 | ` */` |
|   1342 | 3230 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3231 | `{` |
|   1347 | 3232 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3233 | `	const char *zBlob,*zPattern;` |
|      - | 3234 | `	int nLen,nPatLen,nStart;` |
|      - | 3235 | `	sxu32 nOfft;` |
|      - | 3236 | `	sxi32 rc;` |
|   1347 | 3237 | `	if( nArg < 2 ){` |
|      - | 3238 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3239 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3240 | `		return PH7_OK;` |
|      - | 3241 | `	}` |
|      - | 3242 | `	/* Extract the needle and the haystack */` |
|   1347 | 3243 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|   1347 | 3244 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|   1347 | 3245 | `	nOfft = 0; /* cc warning */` |
|   1347 | 3246 | `	nStart = 0;` |
|      - | 3247 | `	/* Peek the starting offset if available */` |
|   1347 | 3248 | `	if( nArg > 2 ){` |
|    ! 0 | 3249 | `		nStart = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 3250 | `		if( nStart < 0 ){` |
|    ! 0 | 3251 | `			nStart = -nStart;` |
|    ! 0 | 3252 | `		}` |
|    ! 0 | 3253 | `		if( nStart >= nLen ){` |
|      - | 3254 | `			/* Invalid offset */` |
|    ! 0 | 3255 | `			nStart = 0;` |
|    ! 0 | 3256 | `		}else{` |
|    ! 0 | 3257 | `			zBlob += nStart;` |
|    ! 0 | 3258 | `			nLen -= nStart;` |
|      - | 3259 | `		}` |
|    ! 0 | 3260 | `	}` |
|   1347 | 3261 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3262 | `		/* Perform the lookup */` |
|   1345 | 3263 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|   1345 | 3264 | `		if( rc != SXRET_OK ){` |
|      - | 3265 | `			/* Pattern not found,return FALSE */` |
|    719 | 3266 | `			ph7_result_bool(pCtx,0);` |
|    719 | 3267 | `			return PH7_OK;` |
|      - | 3268 | `		}` |
|      - | 3269 | `		/* Return the pattern position */` |
|    630 | 3270 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|    317 | 3271 | `	}else{` |
|      3 | 3272 | `		ph7_result_bool(pCtx,0);` |
|      - | 3273 | `	}` |
|    632 | 3274 | `	return PH7_OK;` |
|    676 | 3275 | `}` |
|      - | 3276 | `/*` |
|      - | 3277 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|      - | 3278 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|      - | 3279 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|      - | 3280 | ` * TypeError for arrays, resources, and objects without __toString.` |
|      - | 3281 | ` *` |
|      - | 3282 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|      - | 3283 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|      - | 3284 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|      - | 3285 | ` *` |
|      - | 3286 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|      - | 3287 | ` * is valid until pTmp is released or pArg is mutated.` |
|      - | 3288 | ` */` |
|    720 | 3289 | `static sxi32 StrPredicateResolveArg(` |
|      - | 3290 | `	ph7_context *pCtx,` |
|      - | 3291 | `	ph7_value *pArg,` |
|      - | 3292 | `	const char *zFunc,` |
|      - | 3293 | `	int iArgNum,` |
|      - | 3294 | `	const char *zParamName,` |
|      - | 3295 | `	const char *zTypeStr, /* Declared type in the TypeError, e.g. "string" / "?string" */` |
|      - | 3296 | `	const char *zNullMsg,` |
|      - | 3297 | `	ph7_value *pTmp,` |
|      - | 3298 | `	const char **pzOut,` |
|      - | 3299 | `	int *pnOut` |
|      4 | 3300 | `){` |
|    724 | 3301 | `	if( ph7_value_is_null(pArg) ){` |
|     13 | 3302 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zNullMsg);` |
|     13 | 3303 | `		*pzOut = "";` |
|     13 | 3304 | `		*pnOut = 0;` |
|     13 | 3305 | `		return PH7_OK;` |
|      - | 3306 | `	}` |
|   1088 | 3307 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    682 | 3308 | `	    ( ph7_value_is_object(pArg) &&` |
|    105 | 3309 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     70 | 3310 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     35 | 3311 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 3312 | `	    )` |
|      - | 3313 | `	){` |
|     52 | 3314 | `		const char *zType = ph7_type_name(pArg);` |
|     52 | 3315 | `		if( ph7_value_is_object(pArg) ){` |
|     23 | 3316 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     23 | 3317 | `			if( pInst && pInst->pClass ){` |
|     23 | 3318 | `				zType = SyStringData(&pInst->pClass->sName);` |
|     11 | 3319 | `			}` |
|     11 | 3320 | `		}` |
|     76 | 3321 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3322 | `			"TypeError",` |
|      - | 3323 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|     24 | 3324 | `			zFunc, iArgNum, zParamName, zTypeStr, zType` |
|      - | 3325 | `			);` |
|      - | 3326 | `	}` |
|    661 | 3327 | `	if( ph7_value_is_object(pArg) ){` |
|     49 | 3328 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     49 | 3329 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 3330 | `			"__toString",sizeof("__toString")-1);` |
|     49 | 3331 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|     49 | 3332 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|     49 | 3333 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|     49 | 3334 | `		return PH7_OK;` |
|      - | 3335 | `	}` |
|    613 | 3336 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|    613 | 3337 | `	return PH7_OK;` |
|    364 | 3338 | `}` |
|      - | 3339 | `/*` |
|      - | 3340 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 3341 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 3342 | ` * Return` |
|      - | 3343 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 3344 | ` */` |
|     96 | 3345 | `static int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 3346 | `{` |
|      - | 3347 | `	const char *zHaystack,*zNeedle;` |
|      - | 3348 | `	int nHayLen,nNeedleLen;` |
|      - | 3349 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3350 | `	sxi32 rc;` |
|    100 | 3351 | `	if( nArg != 2 ){` |
|     18 | 3352 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3353 | `			"ArgumentCountError",` |
|      - | 3354 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|      5 | 3355 | `			nArg` |
|      - | 3356 | `			);` |
|      - | 3357 | `	}` |
|     88 | 3358 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     88 | 3359 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     88 | 3360 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack","string",` |
|      - | 3361 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|      - | 3362 | `		"of type string is deprecated",` |
|      - | 3363 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     88 | 3364 | `	if( rc != PH7_OK ) goto out;` |
|     81 | 3365 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle","string",` |
|      - | 3366 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|      - | 3367 | `		"of type string is deprecated",` |
|      - | 3368 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     81 | 3369 | `	if( rc != PH7_OK ) goto out;` |
|     77 | 3370 | `	if( nNeedleLen < 1 ){` |
|     13 | 3371 | `		ph7_result_bool(pCtx,1);` |
|     71 | 3372 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3373 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3374 | `	}else{` |
|     85 | 3375 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|     28 | 3376 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|     57 | 3377 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|      - | 3378 | `	}` |
|     77 | 3379 | `	rc = PH7_OK;` |
|     43 | 3380 | `out:` |
|     88 | 3381 | `	PH7_MemObjRelease(&sHayTmp);` |
|     88 | 3382 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     88 | 3383 | `	return rc;` |
|     52 | 3384 | `}` |
|      - | 3385 | `/*` |
|      - | 3386 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 3387 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 3388 | ` * Return` |
|      - | 3389 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 3390 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3391 | ` */` |
|     78 | 3392 | `static int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 3393 | `{` |
|      - | 3394 | `	const char *zHaystack,*zNeedle;` |
|      - | 3395 | `	int nHayLen,nNeedleLen;` |
|      - | 3396 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3397 | `	sxi32 rc;` |
|     82 | 3398 | `	if( nArg != 2 ){` |
|     18 | 3399 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3400 | `			"ArgumentCountError",` |
|      - | 3401 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|      5 | 3402 | `			nArg` |
|      - | 3403 | `			);` |
|      - | 3404 | `	}` |
|     70 | 3405 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 3406 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 3407 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack","string",` |
|      - | 3408 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3409 | `		"of type string is deprecated",` |
|      - | 3410 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 3411 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 3412 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle","string",` |
|      - | 3413 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3414 | `		"of type string is deprecated",` |
|      - | 3415 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 3416 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3417 | `	if( nNeedleLen < 1 ){` |
|     13 | 3418 | `		ph7_result_bool(pCtx,1);` |
|     53 | 3419 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3420 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3421 | `	}else{` |
|     58 | 3422 | `		ph7_result_bool(pCtx,` |
|     38 | 3423 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3424 | `	}` |
|     59 | 3425 | `	rc = PH7_OK;` |
|     34 | 3426 | `out:` |
|     70 | 3427 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 3428 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 3429 | `	return rc;` |
|     43 | 3430 | `}` |
|      - | 3431 | `/*` |
|      - | 3432 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 3433 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 3434 | ` * Return` |
|      - | 3435 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 3436 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3437 | ` */` |
|     78 | 3438 | `static int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 3439 | `{` |
|      - | 3440 | `	const char *zHaystack,*zNeedle;` |
|      - | 3441 | `	int nHayLen,nNeedleLen;` |
|      - | 3442 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3443 | `	sxi32 rc;` |
|     82 | 3444 | `	if( nArg != 2 ){` |
|     18 | 3445 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3446 | `			"ArgumentCountError",` |
|      - | 3447 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|      5 | 3448 | `			nArg` |
|      - | 3449 | `			);` |
|      - | 3450 | `	}` |
|     70 | 3451 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 3452 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 3453 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack","string",` |
|      - | 3454 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3455 | `		"of type string is deprecated",` |
|      - | 3456 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 3457 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 3458 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle","string",` |
|      - | 3459 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3460 | `		"of type string is deprecated",` |
|      - | 3461 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 3462 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3463 | `	if( nNeedleLen < 1 ){` |
|     13 | 3464 | `		ph7_result_bool(pCtx,1);` |
|     53 | 3465 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3466 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3467 | `	}else{` |
|     58 | 3468 | `		ph7_result_bool(pCtx,` |
|     38 | 3469 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3470 | `	}` |
|     59 | 3471 | `	rc = PH7_OK;` |
|     34 | 3472 | `out:` |
|     70 | 3473 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 3474 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 3475 | `	return rc;` |
|     43 | 3476 | `}` |
|      - | 3477 | `/*` |
|      - | 3478 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3479 | ` *  Case-insensitive strpos.` |
|      - | 3480 | ` * Parameters` |
|      - | 3481 | ` *  $haystack` |
|      - | 3482 | ` *   The input string.` |
|      - | 3483 | ` * $needle` |
|      - | 3484 | ` *   Search pattern (must be a string).` |
|      - | 3485 | ` * $offset` |
|      - | 3486 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3487 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3488 | ` *   of haystack.` |
|      - | 3489 | ` * Return` |
|      - | 3490 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3491 | ` */` |
|    174 | 3492 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3493 | `{` |
|    175 | 3494 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3495 | `	const char *zBlob,*zPattern;` |
|      - | 3496 | `	int nLen,nPatLen,nStart;` |
|      - | 3497 | `	sxu32 nOfft;` |
|      - | 3498 | `	sxi32 rc;` |
|    175 | 3499 | `	if( nArg < 2 ){` |
|      - | 3500 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3501 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3502 | `		return PH7_OK;` |
|      - | 3503 | `	}` |
|      - | 3504 | `	/* Extract the needle and the haystack */` |
|    175 | 3505 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    175 | 3506 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    175 | 3507 | `	nOfft = 0; /* cc warning */` |
|    175 | 3508 | `	nStart = 0;` |
|      - | 3509 | `	/* Peek the starting offset if available */` |
|    175 | 3510 | `	if( nArg > 2 ){` |
|      5 | 3511 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 3512 | `		if( nStart < 0 ){` |
|      3 | 3513 | `			nStart = -nStart;` |
|      1 | 3514 | `		}` |
|      5 | 3515 | `		if( nStart >= nLen ){` |
|      - | 3516 | `			/* Invalid offset */` |
|    ! 0 | 3517 | `			nStart = 0;` |
|    ! 0 | 3518 | `		}else{` |
|      5 | 3519 | `			zBlob += nStart;` |
|      5 | 3520 | `			nLen -= nStart;` |
|      - | 3521 | `		}` |
|      2 | 3522 | `	}` |
|    175 | 3523 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3524 | `		/* Perform the lookup */` |
|    175 | 3525 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    175 | 3526 | `		if( rc != SXRET_OK ){` |
|      - | 3527 | `			/* Pattern not found,return FALSE */` |
|    161 | 3528 | `			ph7_result_bool(pCtx,0);` |
|    161 | 3529 | `			return PH7_OK;` |
|      - | 3530 | `		}` |
|      - | 3531 | `		/* Return the pattern position */` |
|     15 | 3532 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 3533 | `	}else{` |
|    ! 0 | 3534 | `		ph7_result_bool(pCtx,0);` |
|      - | 3535 | `	}` |
|     15 | 3536 | `	return PH7_OK;` |
|     88 | 3537 | `}` |
|      - | 3538 | `/*` |
|      - | 3539 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3540 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 3541 | ` * Parameters` |
|      - | 3542 | ` *  $haystack` |
|      - | 3543 | ` *   The input string.` |
|      - | 3544 | ` * $needle` |
|      - | 3545 | ` *   Search pattern (must be a string).` |
|      - | 3546 | ` * $offset` |
|      - | 3547 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3548 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3549 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3550 | ` * Return` |
|      - | 3551 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3552 | ` */` |
|     40 | 3553 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3554 | `{` |
|      - | 3555 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     41 | 3556 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3557 | `	int nLen,nPatLen;` |
|      - | 3558 | `	sxu32 nOfft;` |
|      - | 3559 | `	sxi32 rc;` |
|     41 | 3560 | `	if( nArg < 2 ){` |
|      - | 3561 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3562 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3563 | `		return PH7_OK;` |
|      - | 3564 | `	}` |
|      - | 3565 | `	/* Extract the needle and the haystack */` |
|     41 | 3566 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 3567 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3568 | `	/* Point to the end of the pattern */` |
|     41 | 3569 | `	zPtr = &zBlob[nLen - 1];` |
|     41 | 3570 | `	zEnd = &zBlob[nLen];` |
|      - | 3571 | `	/* Save the starting posistion */` |
|     41 | 3572 | `	zStart = zBlob;` |
|     41 | 3573 | `	nOfft = 0; /* cc warning */` |
|      - | 3574 | `	/* Peek the starting offset if available */` |
|     41 | 3575 | `	if( nArg > 2 ){` |
|      - | 3576 | `		int nStart;` |
|     21 | 3577 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 3578 | `		if( nStart < 0 ){` |
|     11 | 3579 | `			nStart = -nStart;` |
|     11 | 3580 | `			if( nStart >= nLen ){` |
|      - | 3581 | `				/* Invalid offset */` |
|      3 | 3582 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3583 | `				return PH7_OK;` |
|    ! 0 | 3584 | `			}else{` |
|      9 | 3585 | `				nLen -= nStart;` |
|      9 | 3586 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 3587 | `				zEnd = &zBlob[nLen];` |
|      - | 3588 | `			}` |
|      5 | 3589 | `		}else{` |
|     11 | 3590 | `			if( nStart >= nLen ){` |
|      - | 3591 | `				/* Invalid offset */` |
|      5 | 3592 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3593 | `				return PH7_OK;` |
|    ! 0 | 3594 | `			}else{` |
|      7 | 3595 | `				zBlob += nStart;` |
|      7 | 3596 | `				nLen -= nStart;` |
|      - | 3597 | `			}` |
|      - | 3598 | `		}` |
|      7 | 3599 | `	}` |
|     35 | 3600 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3601 | `		/* Perform the lookup */` |
|    121 | 3602 | `		for(;;){` |
|    243 | 3603 | `			if( zBlob >= zPtr ){` |
|     21 | 3604 | `				break;` |
|      - | 3605 | `			}` |
|    223 | 3606 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    223 | 3607 | `			if( rc == SXRET_OK ){` |
|      - | 3608 | `				/* Pattern found,return it's position */` |
|     13 | 3609 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 3610 | `				return PH7_OK;` |
|      - | 3611 | `			}` |
|    211 | 3612 | `			zPtr--;` |
|      1 | 3613 | `		}` |
|      - | 3614 | `		/* Pattern not found,return FALSE */` |
|     21 | 3615 | `		ph7_result_bool(pCtx,0);` |
|     11 | 3616 | `	}else{` |
|      3 | 3617 | `		ph7_result_bool(pCtx,0);` |
|      - | 3618 | `	}` |
|     23 | 3619 | `	return PH7_OK;` |
|     21 | 3620 | `}` |
|      - | 3621 | `/*` |
|      - | 3622 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3623 | ` *  Case-insensitive strrpos.` |
|      - | 3624 | ` * Parameters` |
|      - | 3625 | ` *  $haystack` |
|      - | 3626 | ` *   The input string.` |
|      - | 3627 | ` * $needle` |
|      - | 3628 | ` *   Search pattern (must be a string).` |
|      - | 3629 | ` * $offset` |
|      - | 3630 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3631 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3632 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3633 | ` * Return` |
|      - | 3634 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3635 | ` */` |
|     26 | 3636 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3637 | `{` |
|      - | 3638 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     27 | 3639 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3640 | `	int nLen,nPatLen;` |
|      - | 3641 | `	sxu32 nOfft;` |
|      - | 3642 | `	sxi32 rc;` |
|     27 | 3643 | `	if( nArg < 2 ){` |
|      - | 3644 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3645 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3646 | `		return PH7_OK;` |
|      - | 3647 | `	}` |
|      - | 3648 | `	/* Extract the needle and the haystack */` |
|     27 | 3649 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 3650 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3651 | `	/* Point to the end of the pattern */` |
|     27 | 3652 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3653 | `	zEnd = &zBlob[nLen];` |
|      - | 3654 | `	/* Save the starting posistion */` |
|     27 | 3655 | `	zStart = zBlob;` |
|     27 | 3656 | `	nOfft = 0; /* cc warning */` |
|      - | 3657 | `	/* Peek the starting offset if available */` |
|     27 | 3658 | `	if( nArg > 2 ){` |
|      - | 3659 | `		int nStart;` |
|     15 | 3660 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3661 | `		if( nStart < 0 ){` |
|      7 | 3662 | `			nStart = -nStart;` |
|      7 | 3663 | `			if( nStart >= nLen ){` |
|      - | 3664 | `				/* Invalid offset */` |
|      3 | 3665 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3666 | `				return PH7_OK;` |
|    ! 0 | 3667 | `			}else{` |
|      5 | 3668 | `				nLen -= nStart;` |
|      5 | 3669 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3670 | `				zEnd = &zBlob[nLen];` |
|      - | 3671 | `			}` |
|      3 | 3672 | `		}else{` |
|      9 | 3673 | `			if( nStart >= nLen ){` |
|      - | 3674 | `				/* Invalid offset */` |
|      5 | 3675 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3676 | `				return PH7_OK;` |
|    ! 0 | 3677 | `			}else{` |
|      5 | 3678 | `				zBlob += nStart;` |
|      5 | 3679 | `				nLen -= nStart;` |
|      - | 3680 | `			}` |
|      - | 3681 | `		}` |
|      4 | 3682 | `	}` |
|     21 | 3683 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3684 | `		/* Perform the lookup */` |
|     44 | 3685 | `		for(;;){` |
|     89 | 3686 | `			if( zBlob >= zPtr ){` |
|      9 | 3687 | `				break;` |
|      - | 3688 | `			}` |
|     81 | 3689 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3690 | `			if( rc == SXRET_OK ){` |
|      - | 3691 | `				/* Pattern found,return it's position */` |
|     11 | 3692 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3693 | `				return PH7_OK;` |
|      - | 3694 | `			}` |
|     71 | 3695 | `			zPtr--;` |
|      1 | 3696 | `		}` |
|      - | 3697 | `		/* Pattern not found,return FALSE */` |
|      9 | 3698 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3699 | `	}else{` |
|      3 | 3700 | `		ph7_result_bool(pCtx,0);` |
|      - | 3701 | `	}` |
|     11 | 3702 | `	return PH7_OK;` |
|     14 | 3703 | `}` |
|      - | 3704 | `/*` |
|      - | 3705 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3706 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3707 | ` * Parameters` |
|      - | 3708 | ` *  $haystack` |
|      - | 3709 | ` *   The input string.` |
|      - | 3710 | ` * $needle` |
|      - | 3711 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3712 | ` *  This behavior is different from that of strstr().` |
|      - | 3713 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3714 | ` *  as the ordinal value of a character.` |
|      - | 3715 | ` * Return` |
|      - | 3716 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3717 | ` */` |
|     22 | 3718 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3719 | `{` |
|      - | 3720 | `	const char *zBlob;` |
|      - | 3721 | `	int nLen,c;` |
|     23 | 3722 | `	if( nArg < 2 ){` |
|      - | 3723 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3724 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3725 | `		return PH7_OK;` |
|      - | 3726 | `	}` |
|      - | 3727 | `	/* Extract the haystack */` |
|     23 | 3728 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3729 | `	c = 0; /* cc warning */` |
|     23 | 3730 | `	if( nLen > 0 ){` |
|      - | 3731 | `		sxu32 nOfft;` |
|      - | 3732 | `		sxi32 rc;` |
|     21 | 3733 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3734 | `			const char *zPattern;` |
|     11 | 3735 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3736 | `														 * for NULL pointer.` |
|      - | 3737 | `														 */` |
|     11 | 3738 | `			c = zPattern[0];` |
|      6 | 3739 | `		}else{` |
|      - | 3740 | `			/* Int cast */` |
|     11 | 3741 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3742 | `		}` |
|      - | 3743 | `		/* Perform the lookup */` |
|     21 | 3744 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3745 | `		if( rc != SXRET_OK ){` |
|      - | 3746 | `			/* No such entry,return FALSE */` |
|      7 | 3747 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3748 | `			return PH7_OK;` |
|      - | 3749 | `		}` |
|      - | 3750 | `		/* Return the string portion */` |
|     15 | 3751 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3752 | `	}else{` |
|      3 | 3753 | `		ph7_result_bool(pCtx,0);` |
|      - | 3754 | `	}` |
|     17 | 3755 | `	return PH7_OK;` |
|     12 | 3756 | `}` |
|      - | 3757 | `/*` |
|      - | 3758 | ` * string strrev(string $string)` |
|      - | 3759 | ` *  Reverse a string.` |
|      - | 3760 | ` * Parameters` |
|      - | 3761 | ` *  $string` |
|      - | 3762 | ` *   String to be reversed.` |
|      - | 3763 | ` * Return` |
|      - | 3764 | ` *  The reversed string.` |
|      - | 3765 | ` */` |
|      2 | 3766 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3767 | `{` |
|      - | 3768 | `	const char *zIn,*zEnd;` |
|      - | 3769 | `	int nLen,c;` |
|      3 | 3770 | `	if( nArg < 1 ){` |
|      - | 3771 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3772 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3773 | `		return PH7_OK;` |
|      - | 3774 | `	}` |
|      - | 3775 | `	/* Extract the target string */` |
|      3 | 3776 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3777 | `	if( nLen < 1 ){` |
|      - | 3778 | `		/* Empty string Return null */` |
|    ! 0 | 3779 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3780 | `		return PH7_OK;` |
|      - | 3781 | `	}` |
|      - | 3782 | `	/* Perform the requested operation */` |
|      3 | 3783 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3784 | `	for(;;){` |
|      9 | 3785 | `		if( zEnd < zIn ){` |
|      - | 3786 | `			/* No more input to process */` |
|      3 | 3787 | `			break;` |
|      - | 3788 | `		}` |
|      - | 3789 | `		/* Append current character */` |
|      7 | 3790 | `		c = zEnd[0];` |
|      7 | 3791 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3792 | `		zEnd--;` |
|      1 | 3793 | `	}` |
|      3 | 3794 | `	return PH7_OK;` |
|      2 | 3795 | `}` |
|      - | 3796 | `/*` |
|      - | 3797 | ` * string ucwords(string $string [, string $separators = " \t\r\n\f\v"])` |
|      - | 3798 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3799 | ` *  A word begins at the start of the string and after any character present in` |
|      - | 3800 | ` *  $separators. The default separators are the whitespace characters (space,` |
|      - | 3801 | ` *  horizontal tab, carriage return, newline, form-feed and vertical tab); an` |
|      - | 3802 | ` *  explicit $separators argument REPLACES them (an empty string leaves only the` |
|      - | 3803 | ` *  very first character upper-cased). Like PHP, this is byte-based: only ASCII` |
|      - | 3804 | ` *  bytes are upper-cased and a byte is a separator only if it appears in the set.` |
|      - | 3805 | ` * Parameters` |
|      - | 3806 | ` *  $string` |
|      - | 3807 | ` *   The input string.` |
|      - | 3808 | ` *  $separators` |
|      - | 3809 | ` *   The optional word-boundary characters.` |
|      - | 3810 | ` * Return` |
|      - | 3811 | ` *  The modified string.` |
|      - | 3812 | ` */` |
|     22 | 3813 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3814 | `{` |
|      - | 3815 | `	const char *zIn;` |
|      - | 3816 | `	int nLen,i,iStart;` |
|      - | 3817 | `	char aDelim[256];` |
|     23 | 3818 | `	if( nArg < 1 ){` |
|      - | 3819 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3820 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3821 | `		return PH7_OK;` |
|      - | 3822 | `	}` |
|      - | 3823 | `	/* Build the separator membership table: an explicit $separators argument` |
|      - | 3824 | `	 * replaces the default whitespace set (an empty string clears it). */` |
|     23 | 3825 | `	SyZero(aDelim,(sxu32)sizeof(aDelim));` |
|     23 | 3826 | `	if( nArg > 1 ){` |
|      - | 3827 | `		int nDelim;` |
|      9 | 3828 | `		const char *zDelim = ph7_value_to_string(apArg[1],&nDelim);` |
|     17 | 3829 | `		for( i = 0 ; i < nDelim ; i++ ){` |
|      9 | 3830 | `			aDelim[(unsigned char)zDelim[i]] = 1;` |
|      5 | 3831 | `		}` |
|      5 | 3832 | `	}else{` |
|     15 | 3833 | `		aDelim[(unsigned char)' ']  = 1;` |
|     15 | 3834 | `		aDelim[(unsigned char)'\t'] = 1;` |
|     15 | 3835 | `		aDelim[(unsigned char)'\r'] = 1;` |
|     15 | 3836 | `		aDelim[(unsigned char)'\n'] = 1;` |
|     15 | 3837 | `		aDelim[(unsigned char)'\f'] = 1;` |
|     15 | 3838 | `		aDelim[(unsigned char)'\v'] = 1;` |
|      - | 3839 | `	}` |
|      - | 3840 | `	/* Extract the target string */` |
|     23 | 3841 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3842 | `	if( nLen < 1 ){` |
|      - | 3843 | `		/* Empty string – match PHP semantics */` |
|      3 | 3844 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3845 | `		return PH7_OK;` |
|      - | 3846 | `	}` |
|      - | 3847 | `	/* Upper-case the first byte of each word (the leading byte, or any byte that` |
|      - | 3848 | `	 * follows a separator), appending the untouched runs in between verbatim. */` |
|     21 | 3849 | `	iStart = 0;` |
|    309 | 3850 | `	for( i = 0 ; i < nLen ; i++ ){` |
|    289 | 3851 | `		int c = (unsigned char)zIn[i];` |
|    289 | 3852 | `		if( (i == 0 \|\| aDelim[(unsigned char)zIn[i-1]]) && c < 0x80 && SyisLower(c) ){` |
|     53 | 3853 | `			char up = (char)SyToUpper(c);` |
|     53 | 3854 | `			if( i > iStart ){` |
|     35 | 3855 | `				ph7_result_string(pCtx,&zIn[iStart],i - iStart);` |
|     17 | 3856 | `			}` |
|     53 | 3857 | `			ph7_result_string(pCtx,&up,1);` |
|     53 | 3858 | `			iStart = i + 1;` |
|     26 | 3859 | `		}` |
|    145 | 3860 | `	}` |
|     21 | 3861 | `	if( nLen > iStart ){` |
|     21 | 3862 | `		ph7_result_string(pCtx,&zIn[iStart],nLen - iStart);` |
|     10 | 3863 | `	}` |
|     21 | 3864 | `	return PH7_OK;` |
|     12 | 3865 | `}` |
|      - | 3866 | `/*` |
|      - | 3867 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3868 | ` *  Returns input repeated multiplier times.` |
|      - | 3869 | ` * Parameters` |
|      - | 3870 | ` *  $string` |
|      - | 3871 | ` *   String to be repeated.` |
|      - | 3872 | ` * $multiplier` |
|      - | 3873 | ` *  Number of time the input string should be repeated.` |
|      - | 3874 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3875 | ` *  to 0, the function will return an empty string.` |
|      - | 3876 | ` * Return` |
|      - | 3877 | ` *  The repeated string.` |
|      - | 3878 | ` */` |
|  20434 | 3879 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3880 | `{` |
|      - | 3881 | `	const char *zIn;` |
|      - | 3882 | `	int nLen;` |
|      - | 3883 | `	ph7_int64 nMul;` |
|      - | 3884 | `	int rc;` |
|  20436 | 3885 | `	if( nArg < 2 ){` |
|      - | 3886 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3887 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3888 | `		return PH7_OK;` |
|      - | 3889 | `	}` |
|      - | 3890 | `	/* Extract the target string */` |
|  20436 | 3891 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3892 | `	/* Extract the multiplier as a 64-bit value (a 32-bit read would wrap a large` |
|      - | 3893 | `	 * positive $times into a negative one and trip a spurious ValueError). PHP` |
|      - | 3894 | `	 * validates $times regardless of the string contents: a negative count throws` |
|      - | 3895 | `	 * a catchable ValueError. */` |
|  20436 | 3896 | `	nMul = ph7_value_to_int64(apArg[1]);` |
|  20436 | 3897 | `	if( nMul < 0 ){` |
|      3 | 3898 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 3899 | `			"str_repeat(): Argument #2 ($times) must be greater than or equal to 0");` |
|      - | 3900 | `	}` |
|  20434 | 3901 | `	if( nLen < 1 \|\| nMul < 1 ){` |
|      - | 3902 | `		/* Empty input or a zero multiplier yields the empty string (PHP). */` |
|    ! 0 | 3903 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3904 | `		return PH7_OK;` |
|      - | 3905 | `	}` |
|      - | 3906 | `	/* Perform the requested operation */` |
| 221930 | 3907 | `	for(;;){` |
| 443862 | 3908 | `		if( !nMul ){` |
|  20434 | 3909 | `			break;` |
|      - | 3910 | `		}` |
|      - | 3911 | `		/* Append the copy */` |
| 423430 | 3912 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 423430 | 3913 | `		if( rc != PH7_OK ){` |
|      - | 3914 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 3915 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 3916 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 3917 | `		}` |
| 423430 | 3918 | `		nMul--;` |
|      2 | 3919 | `	}` |
|  20434 | 3920 | `	return PH7_OK;` |
|  10219 | 3921 | `}` |
|      - | 3922 | `/*` |
|      - | 3923 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3924 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3925 | ` * Parameters` |
|      - | 3926 | ` *  $string` |
|      - | 3927 | ` *   The input string.` |
|      - | 3928 | ` * $is_xhtml` |
|      - | 3929 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3930 | ` * Return` |
|      - | 3931 | ` *  The processed string.` |
|      - | 3932 | ` */` |
|      4 | 3933 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3934 | `{` |
|      - | 3935 | `	const char *zIn,*zCur,*zEnd;` |
|      5 | 3936 | `	int is_xhtml = 1; /* Default to XHTML-style '<br />' like PHP */` |
|      - | 3937 | `	int nLen;` |
|      5 | 3938 | `	if( nArg < 1 ){` |
|      - | 3939 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 3940 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3941 | `		return PH7_OK;` |
|      - | 3942 | `	}` |
|      - | 3943 | `	/* Extract the target string */` |
|      5 | 3944 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3945 | `	if( nLen < 1 ){` |
|      - | 3946 | `		/* Empty string,return null */` |
|    ! 0 | 3947 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3948 | `		return PH7_OK;` |
|      - | 3949 | `	}` |
|      5 | 3950 | `	if( nArg > 1 ){` |
|      3 | 3951 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3952 | `	}` |
|      5 | 3953 | `	zEnd = &zIn[nLen];` |
|      - | 3954 | `	/* Perform the requested operation */` |
|      4 | 3955 | `	for(;;){` |
|      9 | 3956 | `		zCur = zIn;` |
|      - | 3957 | `		/* Delimit the string */` |
|     21 | 3958 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3959 | `			zIn++;` |
|      1 | 3960 | `		}` |
|      9 | 3961 | `		if( zCur < zIn ){` |
|      - | 3962 | `			/* Output chunk verbatim */` |
|      9 | 3963 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3964 | `		}` |
|      9 | 3965 | `		if( zIn >= zEnd ){` |
|      - | 3966 | `			/* No more input to process */` |
|      5 | 3967 | `			break;` |
|      - | 3968 | `		}` |
|      - | 3969 | `		/* Output the HTML line break */` |
|      - | 3970 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br />' (space before the slash), otherwise use '<br>' */` |
|      5 | 3971 | `		if( is_xhtml ){` |
|      3 | 3972 | `			ph7_result_string(pCtx,"<br />",(int)sizeof("<br />")-1);` |
|      2 | 3973 | `		}else{` |
|      3 | 3974 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3975 | `		}` |
|      5 | 3976 | `		zCur = zIn;` |
|      - | 3977 | `		/* Append trailing line */` |
|     11 | 3978 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3979 | `			zIn++;` |
|      1 | 3980 | `		}` |
|      5 | 3981 | `		if( zCur < zIn ){` |
|      - | 3982 | `			/* Output chunk verbatim */` |
|      5 | 3983 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3984 | `		}` |
|      1 | 3985 | `	}` |
|      5 | 3986 | `	return PH7_OK;` |
|      3 | 3987 | `}` |
|      - | 3988 | `/*` |
|      - | 3989 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3990 | ` *  According to the PHP reference manual.` |
|      - | 3991 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3992 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3993 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3994 | ` * This applies to both sprintf() and printf().` |
|      - | 3995 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3996 | ` * or more of these elements, in order:` |
|      - | 3997 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3998 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3999 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 4000 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 4001 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 4002 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 4003 | ` *   it with a single quote ('). See the examples below.` |
|      - | 4004 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 4005 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 4006 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 4007 | ` *   should result in.` |
|      - | 4008 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 4009 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 4010 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 4011 | ` *   limit to the string.` |
|      - | 4012 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 4013 | ` *       % - a literal percent character. No argument is required.` |
|      - | 4014 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 4015 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 4016 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 4017 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 4018 | ` * 	     for the number of digits after the decimal point.` |
|      - | 4019 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 4020 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 4021 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 4022 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 4023 | ` *       g - shorter of %e and %f.` |
|      - | 4024 | ` *       G - shorter of %E and %f.` |
|      - | 4025 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 4026 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 4027 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 4028 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 4029 | ` */` |
|      - | 4030 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4031 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4032 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 4033 | `/*` |
|      - | 4034 | `** Conversion types fall into various categories as defined by the` |
|      - | 4035 | `** following enumeration.` |
|      - | 4036 | `*/` |
|      - | 4037 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 4038 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 4039 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 4040 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 4041 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 4042 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 4043 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 4044 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 4045 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 4046 |  |
|      - | 4047 | `/*` |
|      - | 4048 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 4049 | `*/` |
|      - | 4050 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 4051 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 4052 | `/*` |
|      - | 4053 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 4054 | `** by an instance of the following structure` |
|      - | 4055 | `*/` |
|      - | 4056 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 4057 | `struct ph7_fmt_info` |
|      - | 4058 | `{` |
|      - | 4059 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 4060 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 4061 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 4062 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 4063 | `  char *charset; /* The character set for conversion */` |
|      - | 4064 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 4065 | `};` |
|      - | 4066 | `/* PH7_PhpFloatShape (php's float-shape post-processing) lives in memobj.c —` |
|      - | 4067 | ` * the default float->string cast needs it even when this whole formatting` |
|      - | 4068 | ` * region is compiled out by PH7_DISABLE_DISK_IO. */` |
|      - | 4069 | `/*` |
|      - | 4070 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 4071 | ` * used conversion types first.` |
|      - | 4072 | ` */` |
|      - | 4073 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 4074 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 4075 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 4076 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 4077 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 4078 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 4079 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 4080 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 4081 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 4082 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4083 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4084 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 4085 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 4086 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 4087 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 4088 | `  /* php's 'h'/'H' are the locale-independent twins of 'g'/'G'; PHL always` |
|      - | 4089 | `   * formats in the C locale, so they behave identically. */` |
|      - | 4090 | `  {  'h',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 4091 | `  {  'H',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 4092 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 4093 | `};` |
|      - | 4094 | `/*` |
|      - | 4095 | ` * PHP 8 raises a catchable ValueError for an unknown conversion specifier` |
|      - | 4096 | ` * (e.g. "%y", or the C-ism "%#x" — '#' is not a php flag). Because printf()` |
|      - | 4097 | ` * and fprintf() stream their output incrementally while sprintf() buffers it,` |
|      - | 4098 | ` * every format builtin calls PH7_FormatValidate (below) to check the whole` |
|      - | 4099 | ` * format string BEFORE formatting so the throw happens with no partial output` |
|      - | 4100 | ` * escaping (php buffers the entire result and only emits it on success). This` |
|      - | 4101 | ` * scan mirrors the specifier-locating logic of the main format loop below.` |
|      - | 4102 | ` * On the first unknown specifier, stores it in *pBad and returns TRUE; returns` |
|      - | 4103 | ` * FALSE when every specifier is known. (A found-flag rather than a sentinel` |
|      - | 4104 | ` * char, so a NUL specifier byte — "%\0" — is still reported, not mistaken for` |
|      - | 4105 | ` * "all valid".)` |
|      - | 4106 | ` */` |
|    334 | 4107 | `static int FormatUnknownSpec(const char *zIn,int nByte,int *pBad)` |
|      1 | 4108 | `{` |
|    335 | 4109 | `	const char *zEnd = &zIn[nByte];` |
|      - | 4110 | `	int c,idx;` |
|   3165 | 4111 | `	while( zIn < zEnd ){` |
|   2851 | 4112 | `		if( zIn[0] != '%' ){` |
|   2201 | 4113 | `			zIn++;` |
|   2201 | 4114 | `			continue;` |
|      - | 4115 | `		}` |
|    651 | 4116 | `		zIn++; /* jump the percent sign */` |
|      - | 4117 | `		/* php-supported flags: '-', '+', ' ', '0' and the "'<pad>'" custom-pad` |
|      - | 4118 | `		 * form. '#' is intentionally NOT treated as a flag so it surfaces as an` |
|      - | 4119 | `		 * unknown specifier, matching php. */` |
|    693 | 4120 | `		while( zIn < zEnd ){` |
|    691 | 4121 | `			c = zIn[0];` |
|    691 | 4122 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|     43 | 4123 | `				zIn++;` |
|     43 | 4124 | `				continue;` |
|      - | 4125 | `			}` |
|    649 | 4126 | `			if( c=='\'' ){` |
|    ! 0 | 4127 | `				zIn++;` |
|    ! 0 | 4128 | `				if( zIn < zEnd ){` |
|    ! 0 | 4129 | `					zIn++; /* the custom pad character */` |
|    ! 0 | 4130 | `				}` |
|    ! 0 | 4131 | `				continue;` |
|      - | 4132 | `			}` |
|    649 | 4133 | `			break;` |
|    ! 0 | 4134 | `		}` |
|      - | 4135 | `		/* field width */` |
|    725 | 4136 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|     75 | 4137 | `			zIn++;` |
|      1 | 4138 | `		}` |
|      - | 4139 | `		/* positional specifier ($) — php parses flags AFTER it (e.g. "%1$-10s"),` |
|      - | 4140 | `		 * so skip the full flag set and width again, mirroring the main loop. */` |
|    651 | 4141 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
|    ! 0 | 4142 | `			zIn++;` |
|    ! 0 | 4143 | `			while( zIn < zEnd ){` |
|    ! 0 | 4144 | `				c = zIn[0];` |
|    ! 0 | 4145 | `				if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|    ! 0 | 4146 | `					zIn++;` |
|    ! 0 | 4147 | `					continue;` |
|      - | 4148 | `				}` |
|    ! 0 | 4149 | `				if( c=='\'' ){` |
|    ! 0 | 4150 | `					zIn++;` |
|    ! 0 | 4151 | `					if( zIn < zEnd ){` |
|    ! 0 | 4152 | `						zIn++;` |
|    ! 0 | 4153 | `					}` |
|    ! 0 | 4154 | `					continue;` |
|      - | 4155 | `				}` |
|    ! 0 | 4156 | `				break;` |
|    ! 0 | 4157 | `			}` |
|    ! 0 | 4158 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    ! 0 | 4159 | `				zIn++;` |
|    ! 0 | 4160 | `			}` |
|    ! 0 | 4161 | `		}` |
|      - | 4162 | `		/* precision */` |
|    651 | 4163 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|     87 | 4164 | `			zIn++;` |
|    183 | 4165 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|     97 | 4166 | `				zIn++;` |
|      1 | 4167 | `			}` |
|     43 | 4168 | `		}` |
|      - | 4169 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|    651 | 4170 | `		if( zIn < zEnd && zIn[0]=='l' ){` |
|     11 | 4171 | `			zIn++;` |
|      5 | 4172 | `		}` |
|    651 | 4173 | `		if( zIn >= zEnd ){` |
|      - | 4174 | `			/* A dangling '%' with no specifier: PHL's legacy path silently` |
|      - | 4175 | `			 * truncates here (recorded residual); nothing to validate. */` |
|      3 | 4176 | `			break;` |
|      - | 4177 | `		}` |
|    649 | 4178 | `		c = zIn[0];` |
|    649 | 4179 | `		zIn++; /* jump the conversion specifier */` |
|   3191 | 4180 | `		for( idx = 0 ; idx < (int)SX_ARRAYSIZE(aFmt) ; idx++ ){` |
|   3173 | 4181 | `			if( c == aFmt[idx].fmttype ){` |
|    631 | 4182 | `				break;` |
|      - | 4183 | `			}` |
|   1272 | 4184 | `		}` |
|    649 | 4185 | `		if( idx >= (int)SX_ARRAYSIZE(aFmt) ){` |
|     19 | 4186 | `			*pBad = c; /* unknown specifier */` |
|     19 | 4187 | `			return TRUE;` |
|      - | 4188 | `		}` |
|      1 | 4189 | `	}` |
|    317 | 4190 | `	return FALSE;` |
|    168 | 4191 | `}` |
|      - | 4192 | `/*` |
|      - | 4193 | ` * Validate a printf-style format string. PHP 8 raises a catchable ValueError for` |
|      - | 4194 | ` * an unknown conversion specifier, thrown before any output is produced. Every` |
|      - | 4195 | ` * format builtin (sprintf/printf/vprintf/vsprintf/fprintf/vfprintf) calls this` |
|      - | 4196 | ` * up-front, then propagates the returned status verbatim (PH7_EXCEPTION when the` |
|      - | 4197 | ` * throw is caught in place, PH7_ABORT when it goes uncaught).` |
|      - | 4198 | ` * Returns PH7_OK when the format is valid.` |
|      - | 4199 | ` */` |
|    334 | 4200 | `PH7_PRIVATE sxi32 PH7_FormatValidate(ph7_context *pCtx,const char *zFormat,int nByte)` |
|      1 | 4201 | `{` |
|    335 | 4202 | `	int badSpec = 0;` |
|    335 | 4203 | `	if( FormatUnknownSpec(zFormat,nByte,&badSpec) ){` |
|     28 | 4204 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      9 | 4205 | `			"Unknown format specifier \"%c\"",badSpec);` |
|      - | 4206 | `	}` |
|    317 | 4207 | `	return PH7_OK;` |
|    168 | 4208 | `}` |
|      - | 4209 | `/*` |
|      - | 4210 | `` * PHP 8: a printf-family `$format` argument is a `string` parameter — scalars`` |
|      - | 4211 | ` * (int/float/bool) and null coerce to a string, but an array/object/resource` |
|      - | 4212 | ` * raises a catchable TypeError. iArg is the 1-based argument position ($format` |
|      - | 4213 | ` * is #1 for sprintf/printf/vprintf/vsprintf, #2 for fprintf/vfprintf). Returns` |
|      - | 4214 | ` * PH7_OK when the value is string-coercible (the caller then uses` |
|      - | 4215 | ` * ph7_value_to_string, which renders scalars/null verbatim).` |
|      - | 4216 | ` */` |
|    354 | 4217 | `PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg)` |
|      1 | 4218 | `{` |
|    355 | 4219 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg) \|\| ph7_value_is_resource(pArg) ){` |
|      - | 4220 | `		char zBuf[64];` |
|     13 | 4221 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4222 | `			"%s(): Argument #%d ($format) must be of type string, %s given",` |
|      4 | 4223 | `			ph7_function_name(pCtx),iArg,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|      - | 4224 | `	}` |
|    347 | 4225 | `	return PH7_OK;` |
|    178 | 4226 | `}` |
|      - | 4227 | `/*` |
|      - | 4228 | ` * Format a given string.` |
|      - | 4229 | ` * The root program.  All variations call this core.` |
|      - | 4230 | ` * INPUTS:` |
|      - | 4231 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 4232 | ` *            1. A pointer to the call context.` |
|      - | 4233 | ` *            2. A pointer to the list of characters to be output` |
|      - | 4234 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 4235 | ` *            3. An integer number of characters to be output.` |
|      - | 4236 | ` *               (Note: This number might be zero.)` |
|      - | 4237 | ` *            4. Upper layer private data.` |
|      - | 4238 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 4239 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 4240 | ` */` |
|    316 | 4241 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 4242 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 4243 | `	ph7_context *pCtx,  /* call context */` |
|      - | 4244 | `	const char *zIn,    /* Format string */` |
|      - | 4245 | `	int nByte,          /* Format string length */` |
|      - | 4246 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 4247 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 4248 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 4249 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 4250 | `	)` |
|      1 | 4251 | `{` |
|    317 | 4252 | `	char spaces[] = "                                                  ";` |
|      - | 4253 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    317 | 4254 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 4255 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 4256 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 4257 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 4258 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 4259 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 4260 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 4261 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 4262 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 4263 | `	ph7_int64 iVal;` |
|      - | 4264 | `	int precision;           /* Precision of the current field */` |
|      - | 4265 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 4266 | `	int c,rc,n;` |
|      - | 4267 | `	int length;              /* Length of the field */` |
|      - | 4268 | `	int prefix;` |
|      - | 4269 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 4270 | `	int width;               /* Width of the current field */` |
|      - | 4271 | `	int idx;` |
|    317 | 4272 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 4273 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 4274 | `	/* An unknown conversion specifier is rejected up-front by PH7_FormatValidate()` |
|      - | 4275 | `	 * (called by every format builtin before this routine), so the specifier set` |
|      - | 4276 | `	 * seen here is always valid. */` |
|      - | 4277 | `	/* Start the format process */` |
|    473 | 4278 | `	for(;;){` |
|    947 | 4279 | `		zCur = zIn;` |
|   3133 | 4280 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|   2187 | 4281 | `			zIn++;` |
|      1 | 4282 | `		}` |
|    947 | 4283 | `		if( zCur < zIn ){` |
|      - | 4284 | `			/* Consume chunk verbatim */` |
|    661 | 4285 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|    661 | 4286 | `			if( rc != SXRET_OK ){` |
|      - | 4287 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|    ! 0 | 4288 | `				break;` |
|      - | 4289 | `			}` |
|    330 | 4290 | `		}` |
|    947 | 4291 | `		if( zIn >= zEnd ){` |
|      - | 4292 | `			/* No more input to process,break immediately */` |
|    315 | 4293 | `			break;` |
|      - | 4294 | `		}` |
|      - | 4295 | `		/* Find out what flags are present */` |
|    633 | 4296 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    632 | 4297 | `			flag_alternateform = flag_zeropad = 0;` |
|    633 | 4298 | `		zIn++; /* Jump the precent sign */` |
|    316 | 4299 | `		do{` |
|    675 | 4300 | `			c = zIn[0];` |
|    675 | 4301 | `			switch( c ){` |
|     15 | 4302 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      7 | 4303 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 4304 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|     17 | 4305 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 4306 | `			case '\'':` |
|    ! 0 | 4307 | `				zIn++;` |
|    ! 0 | 4308 | `				if( zIn < zEnd ){` |
|      - | 4309 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 4310 | `					c = zIn[0];` |
|    ! 0 | 4311 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4312 | `						spaces[idx] = (char)c;` |
|    ! 0 | 4313 | `					}` |
|    ! 0 | 4314 | `					c = 0;` |
|    ! 0 | 4315 | `				}` |
|    ! 0 | 4316 | `				break;` |
|    632 | 4317 | `			default:                                       break;` |
|      - | 4318 | `			}` |
|    675 | 4319 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 4320 | `		/* Get the field width */` |
|    633 | 4321 | `		width = 0;` |
|   1023 | 4322 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     75 | 4323 | `			width = width*10 + (zIn[0] - '0');` |
|     75 | 4324 | `			zIn++;` |
|      1 | 4325 | `		}` |
|    633 | 4326 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 4327 | `			/* Position specifer */` |
|    ! 0 | 4328 | `			if( width > 0 ){` |
|    ! 0 | 4329 | `				n = width;` |
|    ! 0 | 4330 | `				if( vf && n > 0 ){` |
|    ! 0 | 4331 | `					n--;` |
|    ! 0 | 4332 | `				}` |
|    ! 0 | 4333 | `			}` |
|    ! 0 | 4334 | `			zIn++;` |
|    ! 0 | 4335 | `			width = 0;` |
|      - | 4336 | `			/* php's grammar is %argnum$<flags><width>: the flags come AFTER the` |
|      - | 4337 | `			 * positional, so re-parse the full flag set here (e.g. "%1$-10s"),` |
|      - | 4338 | `			 * not just zero-padding. */` |
|    ! 0 | 4339 | `			do{` |
|    ! 0 | 4340 | `				c = zIn[0];` |
|    ! 0 | 4341 | `				switch( c ){` |
|    ! 0 | 4342 | `				case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|    ! 0 | 4343 | `				case '+':   flag_plussign = 1;        c = 0;   break;` |
|    ! 0 | 4344 | `				case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    ! 0 | 4345 | `				case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 4346 | `				case '\'':` |
|    ! 0 | 4347 | `					zIn++;` |
|    ! 0 | 4348 | `					if( zIn < zEnd ){` |
|    ! 0 | 4349 | `						c = zIn[0];` |
|    ! 0 | 4350 | `						for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4351 | `							spaces[idx] = (char)c;` |
|    ! 0 | 4352 | `						}` |
|    ! 0 | 4353 | `						c = 0;` |
|    ! 0 | 4354 | `					}` |
|    ! 0 | 4355 | `					break;` |
|    ! 0 | 4356 | `				default:                                       break;` |
|      - | 4357 | `				}` |
|    ! 0 | 4358 | `			}while( c==0 && (zIn++ < zEnd) );` |
|    ! 0 | 4359 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 4360 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 4361 | `				zIn++;` |
|    ! 0 | 4362 | `			}` |
|    ! 0 | 4363 | `		}` |
|    633 | 4364 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 4365 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 4366 | `		}` |
|      - | 4367 | `		/* Get the precision */` |
|    633 | 4368 | `		precision = -1;` |
|    633 | 4369 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     87 | 4370 | `			precision = 0;` |
|     87 | 4371 | `			zIn++;` |
|    226 | 4372 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     97 | 4373 | `				precision = precision*10 + (zIn[0] - '0');` |
|     97 | 4374 | `				zIn++;` |
|      1 | 4375 | `			}` |
|     43 | 4376 | `		}` |
|      - | 4377 | `		/* Consume a single 'l' length modifier (a C-ism php accepts and ignores,` |
|      - | 4378 | `		 * e.g. "%ld"); PH7_FormatValidate mirrors this. Exactly one is skipped:` |
|      - | 4379 | `		 * in "%lld" the second 'l' becomes the (unknown) specifier, just like php. */` |
|    633 | 4380 | `		if( zIn < zEnd && zIn[0] == 'l' ){` |
|      9 | 4381 | `			zIn++;` |
|      4 | 4382 | `		}` |
|    633 | 4383 | `		if( zIn >= zEnd ){` |
|      - | 4384 | `			/* No more input */` |
|      3 | 4385 | `			break;` |
|      - | 4386 | `		}` |
|      - | 4387 | `		/* Fetch the info entry for the field */` |
|    631 | 4388 | `		pInfo = 0;` |
|    631 | 4389 | `		xtype = PH7_FMT_ERROR;` |
|    631 | 4390 | `		c = zIn[0];` |
|    631 | 4391 | `		zIn++; /* Jump the format specifer */` |
|   2867 | 4392 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|   2867 | 4393 | `			if( c==aFmt[idx].fmttype ){` |
|    631 | 4394 | `				pInfo = &aFmt[idx];` |
|    631 | 4395 | `				xtype = pInfo->type;` |
|    631 | 4396 | `				break;` |
|      - | 4397 | `			}` |
|   1119 | 4398 | `		}` |
|    631 | 4399 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    631 | 4400 | `		length = 0;` |
|      - | 4401 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 4402 | `		 /*` |
|      - | 4403 | `		  ** At this point, variables are initialized as follows:` |
|      - | 4404 | `		  **` |
|      - | 4405 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 4406 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 4407 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 4408 | `		  **                               field width was negative.` |
|      - | 4409 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 4410 | `		  **                               the conversion character.` |
|      - | 4411 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 4412 | `		  **   width                       The specified field width.  This is` |
|      - | 4413 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 4414 | `		  **   precision                   The specified precision.  The default` |
|      - | 4415 | `		  **                               is -1.` |
|      - | 4416 | `		  */` |
|    631 | 4417 | `		switch(xtype){` |
|      3 | 4418 | `		case PH7_FMT_PERCENT:` |
|      - | 4419 | `			/* A literal percent character */` |
|      7 | 4420 | `			zWorker[0] = '%';` |
|      7 | 4421 | `			length = (int)sizeof(char);` |
|      7 | 4422 | `			break;` |
|      3 | 4423 | `		case PH7_FMT_CHARX:` |
|      - | 4424 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 4425 | `			 * with that ASCII value` |
|      - | 4426 | `			 */` |
|      7 | 4427 | `			pArg = NEXT_ARG;` |
|      7 | 4428 | `			if( pArg == 0 ){` |
|      3 | 4429 | `				c = 0;` |
|      2 | 4430 | `			}else{` |
|      5 | 4431 | `				c = ph7_value_to_int(pArg);` |
|      - | 4432 | `			}` |
|      - | 4433 | `			/* NUL byte is an acceptable value */` |
|      7 | 4434 | `			zWorker[0] = (char)c;` |
|      7 | 4435 | `			length = (int)sizeof(char);` |
|      7 | 4436 | `			break;` |
|    162 | 4437 | `		case PH7_FMT_STRING:` |
|      - | 4438 | `			/* the argument is treated as and presented as a string */` |
|    325 | 4439 | `			pArg = NEXT_ARG;` |
|    325 | 4440 | `			if( pArg == 0 ){` |
|    ! 0 | 4441 | `				length = 0;` |
|    ! 0 | 4442 | `			}else{` |
|    325 | 4443 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 4444 | `			}` |
|    325 | 4445 | `			if( length < 1 ){` |
|    ! 0 | 4446 | `				zBuf = " ";` |
|    ! 0 | 4447 | `				length = (int)sizeof(char);` |
|    ! 0 | 4448 | `			}` |
|    325 | 4449 | `			if( precision>=0 && precision<length ){` |
|      3 | 4450 | `				length = precision;` |
|      1 | 4451 | `			}` |
|    325 | 4452 | `			if( flag_zeropad ){` |
|      - | 4453 | `				/* zero-padding works on strings too */` |
|    ! 0 | 4454 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4455 | `					spaces[idx] = '0';` |
|    ! 0 | 4456 | `				}` |
|    ! 0 | 4457 | `			}` |
|    325 | 4458 | `			break;` |
|     59 | 4459 | `		case PH7_FMT_RADIX:` |
|    119 | 4460 | `			pArg = NEXT_ARG;` |
|    119 | 4461 | `			if( pArg == 0 ){` |
|    ! 0 | 4462 | `				iVal = 0;` |
|    ! 0 | 4463 | `			}else{` |
|    119 | 4464 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 4465 | `			}` |
|      - | 4466 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|    119 | 4467 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 4468 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 4469 | `			}` |
|      - | 4470 | `#if 1` |
|      - | 4471 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 4472 | `        ** I think this is stupid.*/` |
|    119 | 4473 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 4474 | `#else` |
|      - | 4475 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 4476 | `        ** but leave the prefix for hex.*/` |
|      - | 4477 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 4478 | `#endif` |
|    119 | 4479 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|     95 | 4480 | `          if( iVal<0 ){` |
|     25 | 4481 | `            iVal = -iVal;` |
|      - | 4482 | `			/* Ticket 1433-003 */` |
|     25 | 4483 | `			if( iVal < 0 ){` |
|      - | 4484 | `				/* Overflow */` |
|    ! 0 | 4485 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4486 | `			}` |
|     25 | 4487 | `            prefix = '-';` |
|     83 | 4488 | `          }else if( flag_plussign )  prefix = '+';` |
|     69 | 4489 | `          else if( flag_blanksign )  prefix = ' ';` |
|     67 | 4490 | `          else                       prefix = 0;` |
|     48 | 4491 | `        }else{` |
|     25 | 4492 | `			if( iVal<0 ){` |
|    ! 0 | 4493 | `				iVal = -iVal;` |
|      - | 4494 | `				/* Ticket 1433-003 */` |
|    ! 0 | 4495 | `				if( iVal < 0 ){` |
|      - | 4496 | `					/* Overflow */` |
|    ! 0 | 4497 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4498 | `				}` |
|    ! 0 | 4499 | `			}` |
|     25 | 4500 | `			prefix = 0;` |
|      - | 4501 | `		}` |
|    119 | 4502 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      7 | 4503 | `          precision = width-(prefix!=0);` |
|      3 | 4504 | `        }` |
|    119 | 4505 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 4506 | `        {` |
|      - | 4507 | `          register char *cset;      /* Use registers for speed */` |
|      - | 4508 | `          register int base;` |
|    119 | 4509 | `          cset = pInfo->charset;` |
|    119 | 4510 | `          base = pInfo->base;` |
|     59 | 4511 | `          do{                                           /* Convert to ascii */` |
|    185 | 4512 | `            *(--zBuf) = cset[iVal%base];` |
|    185 | 4513 | `            iVal = iVal/base;` |
|    185 | 4514 | `          }while( iVal>0 );` |
|      - | 4515 | `        }` |
|    119 | 4516 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    141 | 4517 | `        for(idx=precision-length; idx>0; idx--){` |
|     23 | 4518 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|     12 | 4519 | `        }` |
|    119 | 4520 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|    119 | 4521 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 4522 | `          char *pre, x;` |
|    ! 0 | 4523 | `          pre = pInfo->prefix;` |
|    ! 0 | 4524 | `          if( *zBuf!=pre[0] ){` |
|    ! 0 | 4525 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|    ! 0 | 4526 | `          }` |
|    ! 0 | 4527 | `        }` |
|    119 | 4528 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    119 | 4529 | `		break;` |
|     88 | 4530 | `		case PH7_FMT_FLOAT:` |
|      - | 4531 | `		case PH7_FMT_EXP:` |
|      - | 4532 | `		case PH7_FMT_GENERIC:{` |
|      - | 4533 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 4534 | `		double realvalue;` |
|      - | 4535 | `		char zFmt[8];` |
|      - | 4536 | `		int nOut, nFmt;` |
|    177 | 4537 | `		pArg = NEXT_ARG;` |
|    177 | 4538 | `		if( pArg == 0 ){` |
|    ! 0 | 4539 | `			realvalue = 0;` |
|    ! 0 | 4540 | `		}else{` |
|    177 | 4541 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 4542 | `		}` |
|      - | 4543 | `		/* php prints the IEEE specials bare — NaN / INF / -INF with no width` |
|      - | 4544 | `		 * padding, precision, or sign flags (php_sprintf_appenddouble). */` |
|    177 | 4545 | `		if( PH7_IS_NAN(realvalue) ){` |
|     21 | 4546 | `			zBuf = "NaN";` |
|     21 | 4547 | `			length = 3;` |
|     21 | 4548 | `			width = 0;` |
|     21 | 4549 | `			break;` |
|      - | 4550 | `		}` |
|    157 | 4551 | `		if( PH7_IS_INF(realvalue) ){` |
|     37 | 4552 | `			if( realvalue < 0.0 ){` |
|     15 | 4553 | `				zBuf = "-INF";` |
|     15 | 4554 | `				length = 4;` |
|      8 | 4555 | `			}else{` |
|     23 | 4556 | `				zBuf = "INF";` |
|     23 | 4557 | `				length = 3;` |
|      - | 4558 | `			}` |
|     37 | 4559 | `			width = 0;` |
|     37 | 4560 | `			break;` |
|      - | 4561 | `		}` |
|    121 | 4562 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|    121 | 4563 | `		if( precision > 53 ){` |
|      - | 4564 | `			/* php's FORMAT_CONV_MAX_PRECISION cap, with the same E_NOTICE` |
|      - | 4565 | `			 * (message prefixed with the active function's name, like` |
|      - | 4566 | `			 * php_error_docref). */` |
|      - | 4567 | `			char zMsg[160];` |
|      4 | 4568 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 4569 | `				"%z(): Requested precision of %d digits was truncated to PHP maximum of %d digits",` |
|      2 | 4570 | `				&pCtx->pFunc->sName,precision,53);` |
|      3 | 4571 | `			PH7_VmThrowError(pCtx->pVm,0,E_NOTICE,zMsg);` |
|      3 | 4572 | `			precision = 53;` |
|      1 | 4573 | `		}` |
|      - | 4574 | ``		/* php's %f/%e extract the sign via `num < 0`, so negative zero prints`` |
|      - | 4575 | `		 * unsigned there — while %g (php_gcvt on the raw value) keeps "-0". */` |
|    121 | 4576 | `		if( xtype!=PH7_FMT_GENERIC && realvalue == 0.0 ){` |
|      9 | 4577 | `			realvalue = 0.0;` |
|      4 | 4578 | `		}` |
|      - | 4579 | `		/* php's float conversions are correctly rounded (zend_dtoa); use libc` |
|      - | 4580 | `		 * snprintf as the digit engine (the byte-exact-floats rule — the old` |
|      - | 4581 | `		 * hand-rolled vxGetdigit loop stopped at 16 significant digits, so` |
|      - | 4582 | `		 * e.g. %f of 1e308 printed zeros where php prints the exact binary64` |
|      - | 4583 | `		 * expansion), then post-process into php's exact shapes below. */` |
|    121 | 4584 | `		nFmt = 0;` |
|    121 | 4585 | `		zFmt[nFmt++] = '%';` |
|    121 | 4586 | `		if( flag_alternateform ) zFmt[nFmt++] = '#';` |
|      - | 4587 | `		/* php's ' ' flag selects space PADDING (its default), not C's` |
|      - | 4588 | `		 * space-for-positive-sign — so flag_blanksign is NOT forwarded. */` |
|    121 | 4589 | `		if( flag_plussign ) zFmt[nFmt++] = '+';` |
|    121 | 4590 | `		zFmt[nFmt++] = '.';` |
|    121 | 4591 | `		zFmt[nFmt++] = '*';` |
|    165 | 4592 | `		zFmt[nFmt++] = (char)(xtype==PH7_FMT_FLOAT ? 'f' :` |
|     30 | 4593 | `			(xtype==PH7_FMT_EXP ? ((pInfo->charset[0]=='E') ? 'E' : 'e')` |
|     28 | 4594 | `			                    : ((pInfo->charset[0]=='E') ? 'G' : 'g')));` |
|    121 | 4595 | `		zFmt[nFmt] = 0;` |
|    121 | 4596 | `		nOut = snprintf(zWorker,sizeof(zWorker),zFmt,precision,realvalue);` |
|    121 | 4597 | `		if( nOut < 0 \|\| nOut >= (int)sizeof(zWorker) ){` |
|      - | 4598 | `			/* Cannot happen with precision capped at 53 (%f of DBL_MAX is` |
|      - | 4599 | `			 * ~365 bytes); keep the truncated output rather than overrun. */` |
|    ! 0 | 4600 | `			nOut = (int)SyStrlen(zWorker);` |
|    ! 0 | 4601 | `		}` |
|    121 | 4602 | `		nOut = (int)PH7_PhpFloatShape(zWorker,(sxi32)nOut,xtype==PH7_FMT_GENERIC);` |
|    121 | 4603 | `		zBuf = zWorker;` |
|    121 | 4604 | `		length = nOut;` |
|      - | 4605 | `		/* Let the zero-pad block below insert zeros between the sign (written` |
|      - | 4606 | `		 * by snprintf) and the first digit, as before. */` |
|    121 | 4607 | `		prefix = (zWorker[0]=='-' \|\| zWorker[0]=='+' \|\| zWorker[0]==' ') ? zWorker[0] : 0;` |
|      - | 4608 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 4609 | `        ** set and we are not left justified */` |
|    121 | 4610 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 4611 | `          int i;` |
|      7 | 4612 | `          int nPad = width - length;` |
|     51 | 4613 | `          for(i=width; i>=nPad; i--){` |
|     45 | 4614 | `            zBuf[i] = zBuf[i-nPad];` |
|     23 | 4615 | `          }` |
|      7 | 4616 | `          i = prefix!=0;` |
|     29 | 4617 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      7 | 4618 | `          length = width;` |
|      3 | 4619 | `        }` |
|      - | 4620 | `#else` |
|      - | 4621 | `         zBuf = " ";` |
|      - | 4622 | `		 length = (int)sizeof(char);` |
|      - | 4623 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|    121 | 4624 | `		 break;` |
|      - | 4625 | `							 }` |
|    ! 0 | 4626 | `		default:` |
|      - | 4627 | `			/* Unreachable: PH7_FormatValidate() rejects unknown specifiers with a` |
|      - | 4628 | `			 * catchable ValueError before formatting begins. Kept as a defensive` |
|      - | 4629 | `			 * no-op that emits nothing. */` |
|    ! 0 | 4630 | `			length = 0;` |
|    ! 0 | 4631 | `			break;` |
|      - | 4632 | `		}` |
|      - | 4633 | `		 /*` |
|      - | 4634 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 4635 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 4636 | `		 ** the output.` |
|      - | 4637 | `		 */` |
|    631 | 4638 | `    if( !flag_leftjustify ){` |
|      - | 4639 | `      register int nspace;` |
|    617 | 4640 | `      nspace = width-length;` |
|    617 | 4641 | `      if( nspace>0 ){` |
|      7 | 4642 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4643 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4644 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4645 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4646 | `			}` |
|    ! 0 | 4647 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4648 | `        }` |
|      7 | 4649 | `        if( nspace>0 ){` |
|      7 | 4650 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      7 | 4651 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4652 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4653 | `			}` |
|      3 | 4654 | `		}` |
|      3 | 4655 | `      }` |
|    308 | 4656 | `    }` |
|    631 | 4657 | `    if( length>0 ){` |
|    631 | 4658 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    631 | 4659 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4660 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4661 | `		}` |
|    315 | 4662 | `    }` |
|    631 | 4663 | `    if( flag_leftjustify ){` |
|      - | 4664 | `      register int nspace;` |
|     15 | 4665 | `      nspace = width-length;` |
|     15 | 4666 | `      if( nspace>0 ){` |
|     11 | 4667 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4668 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4669 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4670 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4671 | `			}` |
|    ! 0 | 4672 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4673 | `        }` |
|     11 | 4674 | `        if( nspace>0 ){` |
|     11 | 4675 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|     11 | 4676 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4677 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4678 | `			}` |
|      5 | 4679 | `		}` |
|      5 | 4680 | `      }` |
|      7 | 4681 | `    }` |
|      1 | 4682 | ` }/* for(;;) */` |
|    317 | 4683 | `	return SXRET_OK;` |
|    159 | 4684 | `}` |
|      - | 4685 | `/*` |
|      - | 4686 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 4687 | ` */` |
|    146 | 4688 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4689 | `{` |
|      - | 4690 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|      - | 4691 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|      - | 4692 | `	 * non-OK rc also stops the format loop. */` |
|    147 | 4693 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|    147 | 4694 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|    147 | 4695 | `	return *pRc;` |
|      1 | 4696 | `}` |
|      - | 4697 | `/*` |
|      - | 4698 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 4699 | ` *  Return a formatted string.` |
|      - | 4700 | ` * Parameters` |
|      - | 4701 | ` *  $format` |
|      - | 4702 | ` *    The format string (see block comment above)` |
|      - | 4703 | ` * Return` |
|      - | 4704 | ` *  A string produced according to the formatting string format.` |
|      - | 4705 | ` */` |
|    110 | 4706 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4707 | `{` |
|      - | 4708 | `	const char *zFormat;` |
|    111 | 4709 | `	sxi32 rc = SXRET_OK;` |
|      - | 4710 | `	int nLen;` |
|    111 | 4711 | `	if( nArg < 1 ){` |
|      - | 4712 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4713 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4714 | `		return PH7_OK;` |
|      - | 4715 | `	}` |
|      - | 4716 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|    111 | 4717 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    111 | 4718 | `	if( rc != PH7_OK ){` |
|      5 | 4719 | `		return rc;` |
|      - | 4720 | `	}` |
|      - | 4721 | `	/* Extract the string format (scalars/null coerce). */` |
|    107 | 4722 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    107 | 4723 | `	if( nLen < 1 ){` |
|      - | 4724 | `		/* Empty string */` |
|    ! 0 | 4725 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4726 | `		return PH7_OK;` |
|      - | 4727 | `	}` |
|      - | 4728 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4729 | `	 * output; propagate the throw status verbatim. */` |
|    107 | 4730 | `	rc = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    107 | 4731 | `	if( rc != PH7_OK ){` |
|     17 | 4732 | `		return rc;` |
|      - | 4733 | `	}` |
|      - | 4734 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|     91 | 4735 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|     91 | 4736 | `	if( rc != SXRET_OK ){` |
|      - | 4737 | `		/* The result append ran out of memory: raise a fatal rather than` |
|      - | 4738 | `		 * returning a silently-truncated string. */` |
|    ! 0 | 4739 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4740 | `	}` |
|     91 | 4741 | `	return PH7_OK;` |
|     56 | 4742 | `}` |
|      - | 4743 | `/*` |
|      - | 4744 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 4745 | ` */` |
|   1130 | 4746 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4747 | `{` |
|   1131 | 4748 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 4749 | `	/* Call the VM output consumer directly */` |
|   1131 | 4750 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 4751 | `	/* Increment counter */` |
|   1131 | 4752 | `	*pCounter += nLen;` |
|   1131 | 4753 | `	return PH7_OK;` |
|      1 | 4754 | `}` |
|      - | 4755 | `/*` |
|      - | 4756 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 4757 | ` *  Output a formatted string.` |
|      - | 4758 | ` * Parameters` |
|      - | 4759 | ` *  $format` |
|      - | 4760 | ` *   See sprintf() for a description of format.` |
|      - | 4761 | ` * Return` |
|      - | 4762 | ` *  The length of the outputted string.` |
|      - | 4763 | ` */` |
|    200 | 4764 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4765 | `{` |
|    201 | 4766 | `	ph7_int64 nCounter = 0;` |
|      - | 4767 | `	const char *zFormat;` |
|      - | 4768 | `	int nLen;` |
|    201 | 4769 | `	if( nArg < 1 ){` |
|      - | 4770 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4771 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4772 | `		return PH7_OK;` |
|      - | 4773 | `	}` |
|      - | 4774 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|      - | 4775 | `	{` |
|    201 | 4776 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    201 | 4777 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 4778 | `			return rcf;` |
|      - | 4779 | `		}` |
|      - | 4780 | `	}` |
|      - | 4781 | `	/* Extract the string format (scalars/null coerce). */` |
|    201 | 4782 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    201 | 4783 | `	if( nLen < 1 ){` |
|      - | 4784 | `		/* Empty string */` |
|    ! 0 | 4785 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4786 | `		return PH7_OK;` |
|      - | 4787 | `	}` |
|      - | 4788 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4789 | `	 * output; propagate the throw status verbatim. */` |
|      - | 4790 | `	{` |
|    201 | 4791 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    201 | 4792 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 4793 | `			return rcv;` |
|      - | 4794 | `		}` |
|      - | 4795 | `	}` |
|      - | 4796 | `	/* Format the string */` |
|    201 | 4797 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 4798 | `	/* Return the length of the outputted string */` |
|    201 | 4799 | `	ph7_result_int64(pCtx,nCounter);` |
|    201 | 4800 | `	return PH7_OK;` |
|    101 | 4801 | `}` |
|      - | 4802 | `/*` |
|      - | 4803 | ` * int vprintf(string $format,array $args)` |
|      - | 4804 | ` *  Output a formatted string.` |
|      - | 4805 | ` * Parameters` |
|      - | 4806 | ` *  $format` |
|      - | 4807 | ` *   See sprintf() for a description of format.` |
|      - | 4808 | ` * Return` |
|      - | 4809 | ` *  The length of the outputted string.` |
|      - | 4810 | ` */` |
|      4 | 4811 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4812 | `{` |
|      5 | 4813 | `	ph7_int64 nCounter = 0;` |
|      - | 4814 | `	const char *zFormat;` |
|      - | 4815 | `	ph7_hashmap *pMap;` |
|      - | 4816 | `	SySet sArg;` |
|      - | 4817 | `	int nLen,n;` |
|      - | 4818 | `	sxi32 rcFmt;` |
|      5 | 4819 | `	if( nArg < 2 ){` |
|      - | 4820 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4821 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4822 | `		return PH7_OK;` |
|      - | 4823 | `	}` |
|      - | 4824 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|      5 | 4825 | `	rcFmt = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|      5 | 4826 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4827 | `		return rcFmt;` |
|      - | 4828 | `	}` |
|      5 | 4829 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 4830 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 4831 | `		char zBuf[64];` |
|      4 | 4832 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4833 | `			"vprintf(): Argument #2 ($values) must be of type array, %s given",` |
|      2 | 4834 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 4835 | `	}` |
|      - | 4836 | `	/* Extract the string format (scalars/null coerce). */` |
|      3 | 4837 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4838 | `	if( nLen < 1 ){` |
|      - | 4839 | `		/* Empty string */` |
|    ! 0 | 4840 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4841 | `		return PH7_OK;` |
|      - | 4842 | `	}` |
|      - | 4843 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4844 | `	 * output; propagate the throw status verbatim. */` |
|      3 | 4845 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      3 | 4846 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4847 | `		return rcFmt;` |
|      - | 4848 | `	}` |
|      - | 4849 | `	/* Point to the hashmap */` |
|      3 | 4850 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4851 | `	/* Extract arguments from the hashmap */` |
|      3 | 4852 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4853 | `	/* Format the string */` |
|      3 | 4854 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 4855 | `	/* Release the container */` |
|      3 | 4856 | `	SySetRelease(&sArg);` |
|      - | 4857 | `	/* Return the length of the outputted string */` |
|      3 | 4858 | `	ph7_result_int64(pCtx,nCounter);` |
|      3 | 4859 | `	return PH7_OK;` |
|      3 | 4860 | `}` |
|      - | 4861 | `/*` |
|      - | 4862 | ` * int vsprintf(string $format,array $args)` |
|      - | 4863 | ` *  Output a formatted string.` |
|      - | 4864 | ` * Parameters` |
|      - | 4865 | ` *  $format` |
|      - | 4866 | ` *   See sprintf() for a description of format.` |
|      - | 4867 | ` * Return` |
|      - | 4868 | ` *  A string produced according to the formatting string format.` |
|      - | 4869 | ` */` |
|     22 | 4870 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4871 | `{` |
|      - | 4872 | `	const char *zFormat;` |
|      - | 4873 | `	ph7_hashmap *pMap;` |
|      - | 4874 | `	SySet sArg;` |
|     23 | 4875 | `	sxi32 rc = SXRET_OK;` |
|      - | 4876 | `	sxi32 rcFmt;` |
|      - | 4877 | `	int nLen,n;` |
|     23 | 4878 | `	if( nArg < 2 ){` |
|      - | 4879 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4880 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4881 | `		return PH7_OK;` |
|      - | 4882 | `	}` |
|      - | 4883 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|     23 | 4884 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|     23 | 4885 | `	if( rc != PH7_OK ){` |
|      5 | 4886 | `		return rc;` |
|      - | 4887 | `	}` |
|     19 | 4888 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 4889 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 4890 | `		char zBuf[64];` |
|     16 | 4891 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4892 | `			"vsprintf(): Argument #2 ($values) must be of type array, %s given",` |
|     10 | 4893 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 4894 | `	}` |
|      - | 4895 | `	/* Extract the string format (scalars/null coerce). */` |
|      9 | 4896 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 4897 | `	if( nLen < 1 ){` |
|      - | 4898 | `		/* Empty string */` |
|    ! 0 | 4899 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4900 | `		return PH7_OK;` |
|      - | 4901 | `	}` |
|      - | 4902 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4903 | `	 * output; propagate the throw status verbatim. */` |
|      9 | 4904 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      9 | 4905 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4906 | `		return rcFmt;` |
|      - | 4907 | `	}` |
|      - | 4908 | `	/* Point to hashmap */` |
|      9 | 4909 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4910 | `	/* Extract arguments from the hashmap */` |
|      9 | 4911 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4912 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|      9 | 4913 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|      - | 4914 | `	/* Release the container */` |
|      9 | 4915 | `	SySetRelease(&sArg);` |
|      9 | 4916 | `	if( rc != SXRET_OK ){` |
|      - | 4917 | `		/* The result append ran out of memory: raise a fatal. */` |
|    ! 0 | 4918 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4919 | `	}` |
|      9 | 4920 | `	return PH7_OK;` |
|     12 | 4921 | `}` |
|      - | 4922 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4923 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4924 | `/*` |
|      - | 4925 | ` * Symisc eXtension.` |
|      - | 4926 | ` * string size_format(int64 $size)` |
|      - | 4927 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 4928 | ` *  Example:` |
|      - | 4929 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 4930 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 4931 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 4932 | ` * Parameter` |
|      - | 4933 | ` *  $size` |
|      - | 4934 | ` *    Entity size in bytes.` |
|      - | 4935 | ` * Return` |
|      - | 4936 | ` *   Formatted string representation of the given size.` |
|      - | 4937 | ` */` |
|     24 | 4938 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4939 | `{` |
|      - | 4940 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 4941 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 4942 | `	sxi32 nRest,i_32;` |
|      - | 4943 | `	ph7_int64 iSize;` |
|     25 | 4944 | `	int c = -1; /* index in zUnit[] */` |
|      - | 4945 |  |
|     25 | 4946 | `	if( nArg < 1 ){` |
|      - | 4947 | `		/* Missing argument,return the empty string */` |
|      3 | 4948 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4949 | `		return PH7_OK;` |
|      - | 4950 | `	}` |
|      - | 4951 | `	/* Extract the given size */` |
|     23 | 4952 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 4953 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 4954 | `		/* Don't bother formatting,return immediately */` |
|      5 | 4955 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 4956 | `		return PH7_OK;` |
|      - | 4957 | `	}` |
|     19 | 4958 | `	for(;;){` |
|     39 | 4959 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 4960 | `		iSize >>= 10;` |
|     39 | 4961 | `		c++;` |
|     39 | 4962 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 4963 | `			break;` |
|      - | 4964 | `		}` |
|      1 | 4965 | `	}` |
|     19 | 4966 | `	nRest /= 100;` |
|     19 | 4967 | `	if( nRest > 9 ){` |
|    ! 0 | 4968 | `		nRest = 9;` |
|    ! 0 | 4969 | `	}` |
|     19 | 4970 | `	if( iSize > 999 ){` |
|    ! 0 | 4971 | `		c++;` |
|    ! 0 | 4972 | `		nRest = 9;` |
|    ! 0 | 4973 | `		iSize = 0;` |
|    ! 0 | 4974 | `	}` |
|     19 | 4975 | `	i_32 = (sxi32)iSize;` |
|      - | 4976 | `	/* Format */` |
|     19 | 4977 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 4978 | `	return PH7_OK;` |
|     13 | 4979 | `}` |
|      - | 4980 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4981 | `/*` |
|      - | 4982 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 4983 | ` *   Calculate the md5 hash of a string.` |
|      - | 4984 | ` * Parameter` |
|      - | 4985 | ` *  $str` |
|      - | 4986 | ` *   Input string` |
|      - | 4987 | ` * $raw_output` |
|      - | 4988 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4989 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4990 | ` * Return` |
|      - | 4991 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 4992 | ` */` |
|     12 | 4993 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4994 | `{` |
|      - | 4995 | `	unsigned char zDigest[16];` |
|     13 | 4996 | `	int raw_output = FALSE;` |
|      - | 4997 | `	const void *pIn;` |
|      - | 4998 | `	int nLen;` |
|     13 | 4999 | `	if( nArg < 1 ){` |
|      - | 5000 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5001 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5002 | `		return PH7_OK;` |
|      - | 5003 | `	}` |
|      - | 5004 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 5005 | `	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */` |
|     13 | 5006 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 5007 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 5008 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 5009 | `	}` |
|      - | 5010 | `	/* Compute the MD5 digest */` |
|     13 | 5011 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|     13 | 5012 | `	if( raw_output ){` |
|      - | 5013 | `		/* Output raw digest */` |
|      5 | 5014 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 5015 | `	}else{` |
|      - | 5016 | `		/* Perform a binary to hex conversion */` |
|      9 | 5017 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 5018 | `	}` |
|     13 | 5019 | `	return PH7_OK;` |
|      7 | 5020 | `}` |
|      - | 5021 | `/*` |
|      - | 5022 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 5023 | ` *   Calculate the sha1 hash of a string.` |
|      - | 5024 | ` * Parameter` |
|      - | 5025 | ` *  $str` |
|      - | 5026 | ` *   Input string` |
|      - | 5027 | ` * $raw_output` |
|      - | 5028 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 5029 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 5030 | ` * Return` |
|      - | 5031 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 5032 | ` */` |
|     10 | 5033 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5034 | `{` |
|      - | 5035 | `	unsigned char zDigest[20];` |
|     11 | 5036 | `	int raw_output = FALSE;` |
|      - | 5037 | `	const void *pIn;` |
|      - | 5038 | `	int nLen;` |
|     11 | 5039 | `	if( nArg < 1 ){` |
|      - | 5040 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5041 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5042 | `		return PH7_OK;` |
|      - | 5043 | `	}` |
|      - | 5044 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 5045 | `	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */` |
|     11 | 5046 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5047 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 5048 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 5049 | `	}` |
|      - | 5050 | `	/* Compute the SHA1 digest */` |
|     11 | 5051 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|     11 | 5052 | `	if( raw_output ){` |
|      - | 5053 | `		/* Output raw digest */` |
|      5 | 5054 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 5055 | `	}else{` |
|      - | 5056 | `		/* Perform a binary to hex conversion */` |
|      7 | 5057 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 5058 | `	}` |
|     11 | 5059 | `	return PH7_OK;` |
|      6 | 5060 | `}` |
|      - | 5061 | `/*` |
|      - | 5062 | ` * int64 crc32(string $str)` |
|      - | 5063 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 5064 | ` * Parameter` |
|      - | 5065 | ` *  $str` |
|      - | 5066 | ` *   Input string` |
|      - | 5067 | ` * Return` |
|      - | 5068 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 5069 | ` */` |
|      2 | 5070 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5071 | `{` |
|      - | 5072 | `	const void *pIn;` |
|      - | 5073 | `	sxu32 nCRC;` |
|      - | 5074 | `	int nLen;` |
|      3 | 5075 | `	if( nArg < 1 ){` |
|      - | 5076 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 5077 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5078 | `		return PH7_OK;` |
|      - | 5079 | `	}` |
|      - | 5080 | `	/* Extract the input string */` |
|      3 | 5081 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5082 | `	if( nLen < 1 ){` |
|      - | 5083 | `		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike` |
|      - | 5084 | `		 * md5()/sha1(), whose empty-string digests are non-zero. */` |
|    ! 0 | 5085 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5086 | `		return PH7_OK;` |
|      - | 5087 | `	}` |
|      - | 5088 | `	/* Calculate the sum */` |
|      3 | 5089 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 5090 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 5091 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 5092 | `	return PH7_OK;` |
|      2 | 5093 | `}` |
|      - | 5094 | `/*` |
|      - | 5095 | ` * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is` |
|      - | 5096 | ` * described by a small record so one dispatch (and one generic HMAC) serves them` |
|      - | 5097 | ` * all. Thin adapters normalize the differing context types and the reversed` |
|      - | 5098 | ` * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.` |
|      - | 5099 | ` */` |
|     11 | 5100 | `static void HashMd5Init(HashCtx *c){ MD5Init(&c->md5); }` |
|     15 | 5101 | `static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }` |
|     11 | 5102 | `static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }` |
|     11 | 5103 | `static void HashSha1Init(HashCtx *c){ SHA1Init(&c->sha1); }` |
|     15 | 5104 | `static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }` |
|     11 | 5105 | `static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }` |
|      9 | 5106 | `static void HashSha224Init(HashCtx *c){ SHA224Init(&c->sha256); }` |
|     33 | 5107 | `static void HashSha256Init(HashCtx *c){ SHA256Init(&c->sha256); }` |
|     57 | 5108 | `static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }` |
|     41 | 5109 | `static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }` |
|      9 | 5110 | `static void HashSha384Init(HashCtx *c){ SHA384Init(&c->sha512); }` |
|     15 | 5111 | `static void HashSha512Init(HashCtx *c){ SHA512Init(&c->sha512); }` |
|     27 | 5112 | `static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }` |
|     23 | 5113 | `static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }` |
|      - | 5114 | `typedef struct HashAlgo HashAlgo;` |
|      - | 5115 | `struct HashAlgo {` |
|      - | 5116 | `	const char *zName;   /* lowercase canonical name */` |
|      - | 5117 | `	int nDigestLen;      /* output bytes: 16/20/28/32/48/64 */` |
|      - | 5118 | `	int nBlockLen;       /* internal block bytes (for HMAC): 64 or 128 */` |
|      - | 5119 | `	void (*xInit)(HashCtx *);` |
|      - | 5120 | `	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);` |
|      - | 5121 | `	void (*xFinal)(HashCtx *,unsigned char *);` |
|      - | 5122 | `};` |
|      - | 5123 | `static const HashAlgo aHashAlgo[] = {` |
|      - | 5124 | `	{ "md5",    16, 64,  HashMd5Init,    HashMd5Update,    HashMd5Final    },` |
|      - | 5125 | `	{ "sha1",   20, 64,  HashSha1Init,   HashSha1Update,   HashSha1Final   },` |
|      - | 5126 | `	{ "sha224", 28, 64,  HashSha224Init, HashSha256Update, HashSha256Final },` |
|      - | 5127 | `	{ "sha256", 32, 64,  HashSha256Init, HashSha256Update, HashSha256Final },` |
|      - | 5128 | `	{ "sha384", 48, 128, HashSha384Init, HashSha512Update, HashSha512Final },` |
|      - | 5129 | `	{ "sha512", 64, 128, HashSha512Init, HashSha512Update, HashSha512Final },` |
|      - | 5130 | `};` |
|      - | 5131 | `/* Case-insensitive algorithm lookup (PHP accepts 'SHA256' etc.). */` |
|     73 | 5132 | `static const HashAlgo * HashFindAlgo(const char *zName,int nLen){` |
|      - | 5133 | `	sxu32 i;` |
|    279 | 5134 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|    272 | 5135 | `		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen` |
|    211 | 5136 | `			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){` |
|     67 | 5137 | `			return &aHashAlgo[i];` |
|      - | 5138 | `		}` |
|    106 | 5139 | `	}` |
|      6 | 5140 | `	return 0;` |
|     38 | 5141 | `}` |
|      - | 5142 | `/*` |
|      - | 5143 | ` * string hash(string $algo,string $data[,bool $binary = false])` |
|      - | 5144 | ` *   Generate a hash value (message digest).` |
|      - | 5145 | ` */` |
|     54 | 5146 | `static int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5147 | `{` |
|      - | 5148 | `	const HashAlgo *pAlgo;` |
|      - | 5149 | `	const char *zAlgo,*zData;` |
|     56 | 5150 | `	int nAlgoLen,nDataLen,raw_output = FALSE;` |
|      - | 5151 | `	HashCtx sCtx;` |
|      - | 5152 | `	unsigned char zDigest[64];` |
|     56 | 5153 | `	if( nArg < 2 ){` |
|    ! 0 | 5154 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5155 | `			"hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5156 | `	}` |
|     56 | 5157 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     56 | 5158 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     56 | 5159 | `	if( pAlgo == 0 ){` |
|      3 | 5160 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5161 | `			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - | 5162 | `	}` |
|     53 | 5163 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     53 | 5164 | `	if( nArg > 2 ){` |
|      9 | 5165 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|      4 | 5166 | `	}` |
|     53 | 5167 | `	pAlgo->xInit(&sCtx);` |
|     53 | 5168 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     53 | 5169 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     53 | 5170 | `	if( raw_output ){` |
|      9 | 5171 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      5 | 5172 | `	}else{` |
|     45 | 5173 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 5174 | `	}` |
|     53 | 5175 | `	return PH7_OK;` |
|     29 | 5176 | `}` |
|      - | 5177 | `/*` |
|      - | 5178 | ` * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])` |
|      - | 5179 | ` *   Generate a keyed hash value using the HMAC method (RFC 2104).` |
|      - | 5180 | ` */` |
|     16 | 5181 | `static int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5182 | `{` |
|      - | 5183 | `	const HashAlgo *pAlgo;` |
|      - | 5184 | `	const char *zAlgo,*zData,*zKey;` |
|     18 | 5185 | `	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;` |
|      - | 5186 | `	HashCtx sCtx;` |
|      - | 5187 | `	unsigned char zKeyBlock[128],zIpad[128],zOpad[128],zInner[64],zDigest[64];` |
|      - | 5188 | `	int i,nBlock,nDigest;` |
|     18 | 5189 | `	if( nArg < 3 ){` |
|    ! 0 | 5190 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5191 | `			"hash_hmac() expects at least 3 arguments, %d given",nArg);` |
|      - | 5192 | `	}` |
|     18 | 5193 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     18 | 5194 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     18 | 5195 | `	if( pAlgo == 0 ){` |
|      3 | 5196 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5197 | `			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 5198 | `	}` |
|     15 | 5199 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     15 | 5200 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|     15 | 5201 | `	if( nArg > 3 ){` |
|      3 | 5202 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|      1 | 5203 | `	}` |
|     15 | 5204 | `	nBlock = pAlgo->nBlockLen;` |
|     15 | 5205 | `	nDigest = pAlgo->nDigestLen;` |
|      - | 5206 | `	/* Reduce the key to a single block: hash it if longer than the block, then` |
|      - | 5207 | `	 * zero-pad (a short or empty key is just zero-padded). */` |
|     15 | 5208 | `	SyZero(zKeyBlock,sizeof(zKeyBlock));` |
|     15 | 5209 | `	if( nKeyLen > nBlock ){` |
|      3 | 5210 | `		pAlgo->xInit(&sCtx);` |
|      3 | 5211 | `		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);` |
|      3 | 5212 | `		pAlgo->xFinal(&sCtx,zKeyBlock);` |
|     14 | 5213 | `	}else if( nKeyLen > 0 ){` |
|     11 | 5214 | `		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);` |
|      5 | 5215 | `	}` |
|   1039 | 5216 | `	for( i = 0; i < nBlock; i++ ){` |
|   1025 | 5217 | `		zIpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);` |
|   1025 | 5218 | `		zOpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);` |
|    513 | 5219 | `	}` |
|      - | 5220 | `	/* inner = H((key ^ ipad) \|\| data) */` |
|     15 | 5221 | `	pAlgo->xInit(&sCtx);` |
|     15 | 5222 | `	pAlgo->xUpdate(&sCtx,zIpad,(unsigned int)nBlock);` |
|     15 | 5223 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     15 | 5224 | `	pAlgo->xFinal(&sCtx,zInner);` |
|      - | 5225 | `	/* out = H((key ^ opad) \|\| inner) */` |
|     15 | 5226 | `	pAlgo->xInit(&sCtx);` |
|     15 | 5227 | `	pAlgo->xUpdate(&sCtx,zOpad,(unsigned int)nBlock);` |
|     15 | 5228 | `	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)nDigest);` |
|     15 | 5229 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     15 | 5230 | `	if( raw_output ){` |
|      3 | 5231 | `		ph7_result_string(pCtx,(const char *)zDigest,nDigest);` |
|      2 | 5232 | `	}else{` |
|     13 | 5233 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);` |
|      - | 5234 | `	}` |
|     15 | 5235 | `	return PH7_OK;` |
|     10 | 5236 | `}` |
|      - | 5237 | `/*` |
|      - | 5238 | ` * bool hash_equals(string $known_string,string $user_string)` |
|      - | 5239 | ` *   Timing-attack-safe string comparison.` |
|      - | 5240 | ` */` |
|     14 | 5241 | `static int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 5242 | `{` |
|      - | 5243 | `	const char *zKnown,*zUser;` |
|      - | 5244 | `	int nKnown,nUser,i;` |
|     17 | 5245 | `	volatile unsigned char vDiff = 0;` |
|     17 | 5246 | `	if( nArg < 2 ){` |
|    ! 0 | 5247 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5248 | `			"hash_equals() expects exactly 2 arguments, %d given",nArg);` |
|      - | 5249 | `	}` |
|     17 | 5250 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      4 | 5251 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5252 | `			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",` |
|      1 | 5253 | `			ph7_type_name(apArg[0]));` |
|      - | 5254 | `	}` |
|     14 | 5255 | `	if( !ph7_value_is_string(apArg[1]) ){` |
|      4 | 5256 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5257 | `			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",` |
|      2 | 5258 | `			ph7_type_name(apArg[1]));` |
|      - | 5259 | `	}` |
|     11 | 5260 | `	zKnown = ph7_value_to_string(apArg[0],&nKnown);` |
|     11 | 5261 | `	zUser = ph7_value_to_string(apArg[1],&nUser);` |
|     11 | 5262 | `	if( nKnown != nUser ){` |
|      5 | 5263 | `		ph7_result_bool(pCtx,0);` |
|      5 | 5264 | `		return PH7_OK;` |
|      - | 5265 | `	}` |
|      - | 5266 | `	/* Constant-time: read every byte, never short-circuit. */` |
|     19 | 5267 | `	for( i = 0; i < nKnown; i++ ){` |
|     13 | 5268 | `		vDiff \|= (unsigned char)(zKnown[i] ^ zUser[i]);` |
|      7 | 5269 | `	}` |
|      7 | 5270 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|      7 | 5271 | `	return PH7_OK;` |
|     10 | 5272 | `}` |
|      - | 5273 | `/*` |
|      - | 5274 | ` * array hash_algos(void)` |
|      - | 5275 | ` *   Return a list of the registered hashing algorithms.` |
|      - | 5276 | ` */` |
|      2 | 5277 | `static int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5278 | `{` |
|      - | 5279 | `	ph7_value *pArray,*pValue;` |
|      - | 5280 | `	sxu32 i;` |
|      1 | 5281 | `	SXUNUSED(nArg);` |
|      1 | 5282 | `	SXUNUSED(apArg);` |
|      3 | 5283 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5284 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 5285 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 5286 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5287 | `		return PH7_OK;` |
|      - | 5288 | `	}` |
|     15 | 5289 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|     13 | 5290 | `		ph7_value_string(pValue,aHashAlgo[i].zName,-1);` |
|     13 | 5291 | `		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);` |
|     13 | 5292 | `		ph7_value_reset_string_cursor(pValue);` |
|      7 | 5293 | `	}` |
|      3 | 5294 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5295 | `	return PH7_OK;` |
|      2 | 5296 | `}` |
|      - | 5297 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 5298 | `/*` |
|      - | 5299 | ` * password_* (bcrypt). These live in ext/standard in real PHP — outside the` |
|      - | 5300 | ` * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.` |
|      - | 5301 | ` */` |
|      - | 5302 | `/*` |
|      - | 5303 | ` * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a` |
|      - | 5304 | ` * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).` |
|      - | 5305 | ` */` |
|     40 | 5306 | `static int BcryptParseHash(const char *zHash,int nHash,int *piCost)` |
|      1 | 5307 | `{` |
|      - | 5308 | `	int iCost;` |
|     40 | 5309 | `	if( nHash != 60 \|\| zHash[0] != '$' \|\| zHash[1] != '2' \|\| zHash[3] != '$'` |
|     29 | 5310 | `		\|\| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){` |
|     13 | 5311 | `		return FALSE;` |
|      - | 5312 | `	}` |
|     29 | 5313 | `	if( zHash[4] < '0' \|\| zHash[4] > '9' \|\| zHash[5] < '0' \|\| zHash[5] > '9' \|\| zHash[6] != '$' ){` |
|    ! 0 | 5314 | `		return FALSE;` |
|      - | 5315 | `	}` |
|     29 | 5316 | `	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');` |
|     29 | 5317 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      3 | 5318 | `		return FALSE;` |
|      - | 5319 | `	}` |
|     27 | 5320 | `	if( piCost ){ *piCost = iCost; }` |
|     27 | 5321 | `	return TRUE;` |
|     21 | 5322 | `}` |
|      - | 5323 | `/*` |
|      - | 5324 | ` * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the` |
|      - | 5325 | ` * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.` |
|      - | 5326 | ` */` |
|     20 | 5327 | `static int BcryptIsBcryptAlgo(ph7_value *pAlgo)` |
|      3 | 5328 | `{` |
|     23 | 5329 | `	if( ph7_value_is_null(pAlgo) ){` |
|    ! 0 | 5330 | `		return TRUE;` |
|      - | 5331 | `	}` |
|     23 | 5332 | `	if( ph7_value_is_string(pAlgo) ){` |
|      - | 5333 | `		int nAlgo;` |
|     23 | 5334 | `		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);` |
|     23 | 5335 | `		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );` |
|      - | 5336 | `	}` |
|    ! 0 | 5337 | `	return FALSE;` |
|     13 | 5338 | `}` |
|      - | 5339 | `/*` |
|      - | 5340 | ` * bool\|string password_hash(string $password,string\|int\|null $algo[,array $options])` |
|      - | 5341 | ` *  Create a bcrypt hash of the password.` |
|      - | 5342 | ` */` |
|     16 | 5343 | `static int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 5344 | `{` |
|      - | 5345 | `	const char *zPwd;` |
|     19 | 5346 | `	int nPwd,iCost = 12;` |
|      - | 5347 | `	unsigned char aSalt[16];` |
|      - | 5348 | `	char zHash[60];` |
|     19 | 5349 | `	if( nArg < 2 ){` |
|    ! 0 | 5350 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5351 | `			"password_hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5352 | `	}` |
|     19 | 5353 | `	if( !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      3 | 5354 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5355 | `			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");` |
|      - | 5356 | `	}` |
|      - | 5357 | `	/* cost from $options['cost'] (default 12). */` |
|     16 | 5358 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|     14 | 5359 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     14 | 5360 | `		if( pCost ){ iCost = ph7_value_to_int(pCost); }` |
|      6 | 5361 | `	}` |
|     16 | 5362 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      4 | 5363 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 5364 | `			"Invalid bcrypt cost parameter specified: %d",iCost);` |
|      - | 5365 | `	}` |
|     13 | 5366 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     13 | 5367 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 5368 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 5369 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 5370 | `	}` |
|     13 | 5371 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){` |
|    ! 0 | 5372 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5373 | `		return PH7_OK;` |
|      - | 5374 | `	}` |
|     13 | 5375 | `	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));` |
|     13 | 5376 | `	return PH7_OK;` |
|     11 | 5377 | `}` |
|      - | 5378 | `/*` |
|      - | 5379 | ` * bool password_verify(string $password,string $hash)` |
|      - | 5380 | ` *  Verify a password against a bcrypt hash. Never throws on a malformed hash.` |
|      - | 5381 | ` */` |
|     28 | 5382 | `static int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5383 | `{` |
|      - | 5384 | `	const char *zPwd,*zHash;` |
|      - | 5385 | `	int nPwd,nHash,iCost,i;` |
|      - | 5386 | `	unsigned char aSalt[16];` |
|      - | 5387 | `	char zComputed[60];` |
|     29 | 5388 | `	volatile unsigned char vDiff = 0;` |
|     29 | 5389 | `	if( nArg < 2 ){` |
|    ! 0 | 5390 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5391 | `			"password_verify() expects exactly 2 arguments, %d given",nArg);` |
|      - | 5392 | `	}` |
|     29 | 5393 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     29 | 5394 | `	zHash = ph7_value_to_string(apArg[1],&nHash);` |
|     29 | 5395 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|     11 | 5396 | `		ph7_result_bool(pCtx,0);` |
|     11 | 5397 | `		return PH7_OK;` |
|      - | 5398 | `	}` |
|      - | 5399 | `	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */` |
|     19 | 5400 | `	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 5401 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5402 | `		return PH7_OK;` |
|      - | 5403 | `	}` |
|     19 | 5404 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){` |
|    ! 0 | 5405 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5406 | `		return PH7_OK;` |
|      - | 5407 | `	}` |
|      - | 5408 | `	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps` |
|      - | 5409 | `	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */` |
|    577 | 5410 | `	for( i = 29; i < 60; i++ ){` |
|    559 | 5411 | `		vDiff \|= (unsigned char)(zComputed[i] ^ zHash[i]);` |
|    280 | 5412 | `	}` |
|     19 | 5413 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|     19 | 5414 | `	return PH7_OK;` |
|     15 | 5415 | `}` |
|      - | 5416 | `/*` |
|      - | 5417 | ` * array password_get_info(string $hash)` |
|      - | 5418 | ` *  Return ["algo"=>id\|null, "algoName"=>name, "options"=>[...]].` |
|      - | 5419 | ` */` |
|      6 | 5420 | `static int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5421 | `{` |
|      7 | 5422 | `	const char *zHash = "";` |
|      7 | 5423 | `	int nHash,iCost = 0,bBcrypt = 0;` |
|      - | 5424 | `	ph7_value *pArray,*pOptions,*pVal;` |
|      7 | 5425 | `	if( nArg > 0 ){` |
|      7 | 5426 | `		zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 5427 | `		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);` |
|      3 | 5428 | `	}` |
|      7 | 5429 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 5430 | `	pOptions = ph7_context_new_array(pCtx);` |
|      7 | 5431 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      7 | 5432 | `	if( pArray == 0 \|\| pOptions == 0 \|\| pVal == 0 ){` |
|    ! 0 | 5433 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5434 | `		return PH7_OK;` |
|      - | 5435 | `	}` |
|      7 | 5436 | `	if( bBcrypt ){` |
|      5 | 5437 | `		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */` |
|      5 | 5438 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 5439 | `		ph7_value_reset_string_cursor(pVal);` |
|      5 | 5440 | `		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);` |
|      5 | 5441 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 5442 | `		ph7_value_int(pVal,iCost);` |
|      5 | 5443 | `		ph7_array_add_strkey_elem(pOptions,"cost",pVal);` |
|      3 | 5444 | `	}else{` |
|      3 | 5445 | `		ph7_value_null(pVal);                          /* algo => null */` |
|      3 | 5446 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      3 | 5447 | `		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);` |
|      3 | 5448 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      - | 5449 | `	}` |
|      7 | 5450 | `	ph7_array_add_strkey_elem(pArray,"options",pOptions);` |
|      7 | 5451 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 5452 | `	return PH7_OK;` |
|      4 | 5453 | `}` |
|      - | 5454 | `/*` |
|      - | 5455 | ` * bool password_needs_rehash(string $hash,string\|int\|null $algo[,array $options])` |
|      - | 5456 | ` *  True if the hash was not made with the given algo/options.` |
|      - | 5457 | ` */` |
|      6 | 5458 | `static int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5459 | `{` |
|      - | 5460 | `	const char *zHash;` |
|      7 | 5461 | `	int nHash,iCost = 0,iWantCost = 12;` |
|      7 | 5462 | `	if( nArg < 2 ){` |
|    ! 0 | 5463 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5464 | `			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5465 | `	}` |
|      7 | 5466 | `	zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 5467 | `	if( !BcryptParseHash(zHash,nHash,&iCost) \|\| !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      - | 5468 | `		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */` |
|      3 | 5469 | `		ph7_result_bool(pCtx,1);` |
|      3 | 5470 | `		return PH7_OK;` |
|      - | 5471 | `	}` |
|      5 | 5472 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      5 | 5473 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|      5 | 5474 | `		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }` |
|      2 | 5475 | `	}` |
|      5 | 5476 | `	ph7_result_bool(pCtx,iCost != iWantCost);` |
|      5 | 5477 | `	return PH7_OK;` |
|      4 | 5478 | `}` |
|      - | 5479 | `/*` |
|      - | 5480 | ` * filter_var() — input validation and sanitization (the ext/filter API).` |
|      - | 5481 | ` *` |
|      - | 5482 | ` * Filter and flag identifiers (values match PHP 8.5; the constants themselves` |
|      - | 5483 | ` * are registered in constant.c). The validate filters are hand-rolled rather` |
|      - | 5484 | ` * than delegating to SyStrToInt64/SyStrToReal: the former silently skips leading` |
|      - | 5485 | ` * zeros and cannot signal overflow, and the latter treats ',' as a decimal point` |
|      - | 5486 | ` * unconditionally — neither matches PHP's filter semantics.` |
|      - | 5487 | ` */` |
|      - | 5488 | `#define FV_VALIDATE_INT     257` |
|      - | 5489 | `#define FV_VALIDATE_BOOLEAN 258` |
|      - | 5490 | `#define FV_VALIDATE_FLOAT   259` |
|      - | 5491 | `#define FV_VALIDATE_REGEXP  272` |
|      - | 5492 | `#define FV_VALIDATE_URL     273` |
|      - | 5493 | `#define FV_VALIDATE_EMAIL   274` |
|      - | 5494 | `#define FV_VALIDATE_IP      275` |
|      - | 5495 | `#define FV_VALIDATE_MAC     276` |
|      - | 5496 | `#define FV_VALIDATE_DOMAIN  277` |
|      - | 5497 | `#define FV_SANITIZE_SPECIAL_CHARS      515` |
|      - | 5498 | `#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */` |
|      - | 5499 | `#define FV_SANITIZE_EMAIL   517` |
|      - | 5500 | `#define FV_SANITIZE_URL     518` |
|      - | 5501 | `#define FV_SANITIZE_NUMBER_INT   519` |
|      - | 5502 | `#define FV_SANITIZE_NUMBER_FLOAT 520` |
|      - | 5503 | `#define FV_SANITIZE_FULL_SPECIAL_CHARS 522` |
|      - | 5504 | `#define FV_FLAG_ALLOW_OCTAL  1` |
|      - | 5505 | `#define FV_FLAG_ALLOW_HEX    2` |
|      - | 5506 | `#define FV_FLAG_STRIP_LOW    4` |
|      - | 5507 | `#define FV_FLAG_STRIP_HIGH   8` |
|      - | 5508 | `#define FV_FLAG_ENCODE_LOW   16` |
|      - | 5509 | `#define FV_FLAG_ENCODE_HIGH  32` |
|      - | 5510 | `#define FV_FLAG_ENCODE_AMP   64` |
|      - | 5511 | `#define FV_FLAG_NO_ENCODE_QUOTES 128` |
|      - | 5512 | `#define FV_FLAG_STRIP_BACKTICK   512` |
|      - | 5513 | `#define FV_FLAG_ALLOW_FRACTION   4096` |
|      - | 5514 | `#define FV_FLAG_ALLOW_THOUSAND   8192` |
|      - | 5515 | `#define FV_FLAG_ALLOW_SCIENTIFIC 16384` |
|      - | 5516 | `#define FV_FLAG_IPV4  1048576` |
|      - | 5517 | `#define FV_FLAG_IPV6  2097152` |
|      - | 5518 | `#define FV_NULL_ON_FAILURE 134217728` |
|      - | 5519 | `/* The subset of flags the UNSAFE_RAW/DEFAULT string filter (FvSanitizeString)` |
|      - | 5520 | ` * acts on: when none are set the filter is a verbatim pass-through, so FV_DEFAULT` |
|      - | 5521 | ` * can shortcut. Keep this in sync with FvSanitizeString's flag handling. */` |
|      - | 5522 | `#define FV_FLAG_STRING_MASK (FV_FLAG_STRIP_LOW\|FV_FLAG_STRIP_HIGH\|FV_FLAG_STRIP_BACKTICK \` |
|      - | 5523 | `                            \|FV_FLAG_ENCODE_LOW\|FV_FLAG_ENCODE_HIGH\|FV_FLAG_ENCODE_AMP)` |
|      - | 5524 |  |
|      - | 5525 | `/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.` |
|      - | 5526 | ` * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */` |
|    153 | 5527 | `static void FvTrim(const char **pz,int *pn){` |
|    153 | 5528 | `	const char *z = *pz;` |
|    153 | 5529 | `	int n = *pn;` |
|    157 | 5530 | `	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }` |
|    161 | 5531 | `	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }` |
|    153 | 5532 | `	*pz = z; *pn = n;` |
|    153 | 5533 | `}` |
|      - | 5534 | `/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     57 | 5535 | `static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){` |
|     57 | 5536 | `	int neg = 0, i;` |
|     57 | 5537 | `	sxu64 u = 0;` |
|     57 | 5538 | `	FvTrim(&z,&n);` |
|     57 | 5539 | `	if( n==0 ){ return 0; }` |
|     51 | 5540 | `	if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }` |
|     51 | 5541 | `	if( n==0 ){ return 0; }` |
|     49 | 5542 | `	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'\|\|z[1]=='X') ){` |
|      3 | 5543 | `		z += 2; n -= 2;` |
|      3 | 5544 | `		if( n==0 ){ return 0; }` |
|      7 | 5545 | `		for( i=0; i<n; i++ ){` |
|      5 | 5546 | `			int h = SyHexToint((unsigned char)z[i]);` |
|      5 | 5547 | `			if( h<0 ){ return 0; }` |
|      5 | 5548 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }` |
|      5 | 5549 | `			u = u*16 + (sxu64)h;` |
|      3 | 5550 | `		}` |
|     48 | 5551 | `	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){` |
|      9 | 5552 | `		for( i=0; i<n; i++ ){` |
|      7 | 5553 | `			if( z[i]<'0' \|\| z[i]>'7' ){ return 0; }` |
|      7 | 5554 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }` |
|      7 | 5555 | `			u = u*8 + (sxu64)(z[i]-'0');` |
|      4 | 5556 | `		}` |
|      2 | 5557 | `	}else{` |
|     45 | 5558 | `		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */` |
|    201 | 5559 | `		for( i=0; i<n; i++ ){` |
|    173 | 5560 | `			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|    161 | 5561 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }` |
|    161 | 5562 | `			u = u*10 + (sxu64)(z[i]-'0');` |
|     81 | 5563 | `		}` |
|      - | 5564 | `	}` |
|     33 | 5565 | `	if( neg ){` |
|      5 | 5566 | `		if( u > 0x8000000000000000ULL ){ return 0; }` |
|      5 | 5567 | `		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */` |
|      3 | 5568 | `	}else{` |
|     29 | 5569 | `		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }` |
|     27 | 5570 | `		*pOut = (ph7_int64)u;` |
|      - | 5571 | `	}` |
|     31 | 5572 | `	return 1;` |
|     29 | 5573 | `}` |
|      - | 5574 | `/* FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     69 | 5575 | `static int FvValidateFloat(const char *z,int n,int flags,double *pOut){` |
|      - | 5576 | `	char zBuf[512];` |
|     69 | 5577 | `	int i, m = 0, seenDigit = 0;` |
|     69 | 5578 | `	const char *zv; int nv; double d = 0;` |
|     69 | 5579 | `	FvTrim(&z,&n);` |
|      - | 5580 | `	/* Bound the input: zBuf[512] holds the thousand-separator-stripped copy, and` |
|      - | 5581 | `	 * the cap also rejects the pathological 500+ digit floats PHP refuses. */` |
|     69 | 5582 | `	if( n==0 \|\| n>500 ){ return 0; }` |
|     69 | 5583 | `	if( flags & FV_FLAG_ALLOW_THOUSAND ){` |
|      - | 5584 | `		/* Commas are optional, but when present they must group the integer part` |
|      - | 5585 | `		 * into a leading run of 1..3 digits followed by groups of exactly 3` |
|      - | 5586 | `		 * ("1,000" ok, "1,5"/"1234,567" rejected). Strip them into zBuf and reject` |
|      - | 5587 | `		 * a comma anywhere in the fractional/exponent tail. */` |
|     25 | 5588 | `		int s = 0, intEnd, segStart, segIdx, hasComma = 0;` |
|     25 | 5589 | `		if( s<n && (z[s]=='+'\|\|z[s]=='-') ){ zBuf[m++] = z[s]; s++; }` |
|     25 | 5590 | `		intEnd = s;` |
|    167 | 5591 | `		while( intEnd<n && z[intEnd]!='.' && z[intEnd]!='e' && z[intEnd]!='E' ){` |
|    143 | 5592 | `			if( z[intEnd]==',' ){ hasComma = 1; }` |
|    143 | 5593 | `			intEnd++;` |
|      1 | 5594 | `		}` |
|     25 | 5595 | `		if( hasComma ){` |
|     25 | 5596 | `			segStart = s; segIdx = 0;` |
|    165 | 5597 | `			for( i=s; i<=intEnd; i++ ){` |
|    151 | 5598 | `				if( i==intEnd \|\| z[i]==',' ){` |
|     49 | 5599 | `					int segLen = i - segStart, k;` |
|     49 | 5600 | `					if( segIdx==0 ){ if( segLen<1 \|\| segLen>3 ){ return 0; } }` |
|     25 | 5601 | `					else if( segLen!=3 ){ return 0; }` |
|    119 | 5602 | `					for( k=segStart; k<i; k++ ){` |
|     81 | 5603 | `						if( !SyisDigit((unsigned char)z[k]) ){ return 0; }` |
|     81 | 5604 | `						zBuf[m++] = z[k];` |
|     41 | 5605 | `					}` |
|     39 | 5606 | `					segStart = i+1; segIdx++;` |
|     19 | 5607 | `				}` |
|     71 | 5608 | `			}` |
|      8 | 5609 | `		}else{` |
|    ! 0 | 5610 | `			for( i=s; i<intEnd; i++ ){ zBuf[m++] = z[i]; }` |
|      - | 5611 | `		}` |
|     27 | 5612 | `		for( i=intEnd; i<n; i++ ){` |
|     13 | 5613 | `			if( z[i]==',' ){ return 0; }` |
|     13 | 5614 | `			zBuf[m++] = z[i];` |
|      7 | 5615 | `		}` |
|     15 | 5616 | `		zv = zBuf; nv = m;` |
|      8 | 5617 | `	}else{` |
|     45 | 5618 | `		zv = z; nv = n;` |
|      - | 5619 | `	}` |
|     59 | 5620 | `	i = 0;` |
|     59 | 5621 | `	if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|    167 | 5622 | `	while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     59 | 5623 | `	if( i<nv && zv[i]=='.' ){` |
|     21 | 5624 | `		i++;` |
|     39 | 5625 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     10 | 5626 | `	}` |
|     59 | 5627 | `	if( !seenDigit ){ return 0; }` |
|     57 | 5628 | `	if( i<nv && (zv[i]=='e'\|\|zv[i]=='E') ){` |
|     29 | 5629 | `		i++;` |
|     29 | 5630 | `		if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|     29 | 5631 | `		if( i>=nv \|\| !SyisDigit((unsigned char)zv[i]) ){ return 0; }` |
|    105 | 5632 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; }` |
|     14 | 5633 | `	}` |
|     57 | 5634 | `	if( i!=nv ){ return 0; } /* trailing junk */` |
|      - | 5635 | `	/* The grammar above guarantees zv[0..nv) is a clean ASCII decimal float (no hex /` |
|      - | 5636 | `	 * inf / nan / trailing junk), so it is safe to hand to libc strtod, which — unlike` |
|      - | 5637 | `	 * SyStrToReal (15 sig-digits + exponent clamped to 308, so it silently saturates` |
|      - | 5638 | `	 * overflowing magnitudes to a finite value) — is overflow/underflow-aware and` |
|      - | 5639 | `	 * correctly rounded. strtod needs a NUL-terminated string: the ALLOW_THOUSAND path` |
|      - | 5640 | `	 * already built the span in zBuf (zv==zBuf); the plain path must copy it there (z is` |
|      - | 5641 | `	 * const + not NUL-terminated). nv <= n <= 500 < sizeof(zBuf) by the cap above.` |
|      - | 5642 | `	 * Matches PHP 8.5 byte-for-byte: reject overflow (-> +/-INF) and total underflow` |
|      - | 5643 | `	 * (-> 0.0), keep subnormals (nonzero, errno==ERANGE) and a genuine "0" (errno==0). */` |
|     53 | 5644 | `	if( zv != zBuf ){ SyMemcpy(zv,zBuf,(sxu32)nv); }` |
|     53 | 5645 | `	zBuf[nv] = 0;` |
|     53 | 5646 | `	errno = 0;` |
|     53 | 5647 | `	d = strtod(zBuf,0);` |
|     53 | 5648 | `	if( errno == ERANGE && (d == HUGE_VAL \|\| d == -HUGE_VAL \|\| d == 0.0) ){` |
|     15 | 5649 | `		return 0;` |
|      - | 5650 | `	}` |
|     39 | 5651 | `	*pOut = d;` |
|     39 | 5652 | `	return 1;` |
|     35 | 5653 | `}` |
|      - | 5654 | `/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),` |
|      - | 5655 | ` * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as` |
|      - | 5656 | ` * false, NOT failures. */` |
|     33 | 5657 | `static int FvValidateBool(const char *z,int n,int *pBool){` |
|     33 | 5658 | `	FvTrim(&z,&n);` |
|     32 | 5659 | `	if( (n==1 && z[0]=='1') \|\| (n==4 && SyStrnicmp(z,"true",4)==0)` |
|     25 | 5660 | `	    \|\| (n==2 && SyStrnicmp(z,"on",2)==0) \|\| (n==3 && SyStrnicmp(z,"yes",3)==0) ){` |
|     11 | 5661 | `		*pBool = 1; return 1;` |
|      - | 5662 | `	}` |
|     22 | 5663 | `	if( n==0 \|\| (n==1 && z[0]=='0') \|\| (n==5 && SyStrnicmp(z,"false",5)==0)` |
|     11 | 5664 | `	    \|\| (n==3 && SyStrnicmp(z,"off",3)==0) \|\| (n==2 && SyStrnicmp(z,"no",2)==0) ){` |
|     11 | 5665 | `		*pBool = 0; return 1;` |
|      - | 5666 | `	}` |
|      9 | 5667 | `	return 0;` |
|     15 | 5668 | `}` |
|      - | 5669 | `/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. */` |
|     33 | 5670 | `static int FvValidateIp4(const char *z,int n){` |
|     33 | 5671 | `	int i = 0, parts = 0;` |
|     77 | 5672 | `	while( i<n ){` |
|     65 | 5673 | `		int val = 0, digits = 0, start = i;` |
|    143 | 5674 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|     85 | 5675 | `			val = val*10 + (z[i]-'0');` |
|     85 | 5676 | `			if( val>255 ){ return 0; }` |
|     79 | 5677 | `			digits++; i++;` |
|      1 | 5678 | `		}` |
|     59 | 5679 | `		if( digits==0 \|\| digits>3 ){ return 0; }` |
|     49 | 5680 | `		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */` |
|     45 | 5681 | `		parts++;` |
|     45 | 5682 | `		if( parts>4 ){ return 0; }` |
|     45 | 5683 | `		if( i<n ){` |
|     33 | 5684 | `			if( z[i]!='.' ){ return 0; }` |
|     33 | 5685 | `			i++;` |
|     33 | 5686 | `			if( i>=n ){ return 0; } /* trailing dot */` |
|     16 | 5687 | `		}` |
|      1 | 5688 | `	}` |
|     13 | 5689 | `	return parts==4;` |
|     17 | 5690 | `}` |
|      - | 5691 | `/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),` |
|      - | 5692 | ` * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1. */` |
|     19 | 5693 | `static int FvIp6Hextets(const char *z,int n){` |
|     19 | 5694 | `	int i = 0, segStart = 0, groups = 0;` |
|     19 | 5695 | `	if( n==0 ){ return 0; }` |
|    145 | 5696 | `	while( i<=n ){` |
|    133 | 5697 | `		if( i==n \|\| z[i]==':' ){` |
|     23 | 5698 | `			int segLen = i - segStart, j, isV4 = 0;` |
|     23 | 5699 | `			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */` |
|     77 | 5700 | `			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }` |
|     23 | 5701 | `			if( isV4 ){` |
|     11 | 5702 | `				if( i!=n ){ return -1; } /* IPv4 only as the final token */` |
|     11 | 5703 | `				if( !FvValidateIp4(z+segStart,segLen) ){ return -1; }` |
|      5 | 5704 | `				groups += 2;` |
|      3 | 5705 | `			}else{` |
|     13 | 5706 | `				if( segLen>4 ){ return -1; }` |
|     47 | 5707 | `				for( j=segStart; j<i; j++ ){ if( SyHexToint((unsigned char)z[j])<0 ){ return -1; } }` |
|     13 | 5708 | `				groups++;` |
|      - | 5709 | `			}` |
|     17 | 5710 | `			segStart = i+1;` |
|      8 | 5711 | `		}` |
|    127 | 5712 | `		i++;` |
|      1 | 5713 | `	}` |
|     13 | 5714 | `	return groups;` |
|     10 | 5715 | `}` |
|      - | 5716 | `/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present. */` |
|     19 | 5717 | `static int FvValidateIp6(const char *z,int n){` |
|     19 | 5718 | `	const char *zDbl = 0;` |
|      - | 5719 | `	int i, ga, gb;` |
|    139 | 5720 | `	for( i=0; i+1<n; i++ ){` |
|    123 | 5721 | `		if( z[i]==':' && z[i+1]==':' ){` |
|     13 | 5722 | `			if( zDbl ){ return 0; } /* a second "::" is invalid */` |
|     11 | 5723 | `			zDbl = z+i;` |
|      5 | 5724 | `		}` |
|     61 | 5725 | `	}` |
|     17 | 5726 | `	if( zDbl==0 ){` |
|      9 | 5727 | `		return FvIp6Hextets(z,n)==8;` |
|    ! 0 | 5728 | `	}else{` |
|      9 | 5729 | `		int lenA = (int)(zDbl - z);` |
|      9 | 5730 | `		int lenB = n - lenA - 2;` |
|      9 | 5731 | `		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA);` |
|      9 | 5732 | `		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB);` |
|      9 | 5733 | `		if( ga<0 \|\| gb<0 ){ return 0; }` |
|      9 | 5734 | `		return (ga+gb)<=7; /* "::" stands for at least one zero group */` |
|      - | 5735 | `	}` |
|     10 | 5736 | `}` |
|     25 | 5737 | `static int FvValidateIp(const char *z,int n,int flags){` |
|     25 | 5738 | `	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);` |
|     25 | 5739 | `	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */` |
|     25 | 5740 | `	if( v4 && FvValidateIp4(z,n) ){ return 1; }` |
|     21 | 5741 | `	if( v6 && FvValidateIp6(z,n) ){ return 1; }` |
|     13 | 5742 | `	return 0;` |
|     13 | 5743 | `}` |
|      - | 5744 | `/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */` |
|     11 | 5745 | `static int FvValidateMac(const char *z,int n){` |
|      - | 5746 | `	char sep;` |
|      - | 5747 | `	int i;` |
|     11 | 5748 | `	if( n!=17 ){ return 0; }` |
|      7 | 5749 | `	sep = z[2];` |
|      7 | 5750 | `	if( sep!=':' && sep!='-' ){ return 0; }` |
|    105 | 5751 | `	for( i=0; i<17; i++ ){` |
|    101 | 5752 | `		if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }` |
|     71 | 5753 | `		else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|     50 | 5754 | `	}` |
|      5 | 5755 | `	return 1;` |
|      6 | 5756 | `}` |
|      - | 5757 | `/* FILTER_VALIDATE_EMAIL (best-effort: covers the common cases, not quoted local` |
|      - | 5758 | ` * parts or IP-literal domains). */` |
|     28 | 5759 | `static int FvValidateEmail(const char *z,int n){` |
|     28 | 5760 | `	int at = -1, i, localLen, domLen, labelStart, dotCount = 0;` |
|      - | 5761 | `	const char *zDom;` |
|     28 | 5762 | `	if( n==0 \|\| n>320 ){ return 0; }` |
|    201 | 5763 | `	for( i=0; i<n; i++ ){` |
|    181 | 5764 | `		if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }` |
|     91 | 5765 | `	}` |
|     21 | 5766 | `	if( at<=0 \|\| at==n-1 ){ return 0; } /* one '@', non-empty local and domain */` |
|     21 | 5767 | `	localLen = at;` |
|     21 | 5768 | `	zDom = z + at + 1;` |
|     21 | 5769 | `	domLen = n - at - 1;` |
|     21 | 5770 | `	if( z[0]=='.' \|\| z[at-1]=='.' ){ return 0; }` |
|     57 | 5771 | `	for( i=0; i<localLen; i++ ){` |
|     43 | 5772 | `		unsigned char c = (unsigned char)z[i];` |
|     43 | 5773 | `		if( c<=' ' ){ return 0; }` |
|     41 | 5774 | `		if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }` |
|     20 | 5775 | `	}` |
|     15 | 5776 | `	if( zDom[0]=='.' \|\| zDom[domLen-1]=='.' ){ return 0; }` |
|     13 | 5777 | `	labelStart = 0;` |
|     85 | 5778 | `	for( i=0; i<=domLen; i++ ){` |
|     75 | 5779 | `		if( i==domLen \|\| zDom[i]=='.' ){` |
|     25 | 5780 | `			int ll = i - labelStart;` |
|     25 | 5781 | `			if( ll==0 ){ return 0; } /* consecutive dots */` |
|     23 | 5782 | `			if( zDom[labelStart]=='-' \|\| zDom[i-1]=='-' ){ return 0; }` |
|     23 | 5783 | `			if( i<domLen ){ dotCount++; }` |
|     23 | 5784 | `			labelStart = i+1;` |
|     12 | 5785 | `		}else{` |
|     51 | 5786 | `			unsigned char c = (unsigned char)zDom[i];` |
|     51 | 5787 | `			if( !((c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9')\|\|c=='-') ){ return 0; }` |
|      - | 5788 | `		}` |
|     37 | 5789 | `	}` |
|     11 | 5790 | `	if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */` |
|      9 | 5791 | `	return 1;` |
|     15 | 5792 | `}` |
|      - | 5793 | `/* FILTER_VALIDATE_DOMAIN (lenient, matching PHP without FILTER_FLAG_HOSTNAME). */` |
|     11 | 5794 | `static int FvValidateDomain(const char *z,int n){` |
|      - | 5795 | `	int i;` |
|     11 | 5796 | `	if( n<1 \|\| n>253 \|\| z[0]=='.' ){ return 0; }` |
|     81 | 5797 | `	for( i=0; i<n; i++ ){` |
|     75 | 5798 | `		unsigned char c = (unsigned char)z[i];` |
|     75 | 5799 | `		if( c<=' ' ){ return 0; }` |
|     75 | 5800 | `		if( c=='.' && i+1<n && z[i+1]=='.' ){ return 0; }` |
|     37 | 5801 | `	}` |
|      7 | 5802 | `	return 1;` |
|      6 | 5803 | `}` |
|      - | 5804 | `/* FILTER_VALIDATE_URL: require a scheme and a host (PHP's filter is itself` |
|      - | 5805 | ` * parse_url-based, so PH7_VmHttpSplitURI tracks it closely). */` |
|     15 | 5806 | `static int FvValidateUrl(const char *z,int n){` |
|      - | 5807 | `	SyhttpUri sUri;` |
|     15 | 5808 | `	if( n==0 ){ return 0; }` |
|     15 | 5809 | `	SyZero(&sUri,(sxu32)sizeof(sUri));` |
|     15 | 5810 | `	if( PH7_VmHttpSplitURI(&sUri,z,(sxu32)n)!=SXRET_OK ){ return 0; }` |
|     15 | 5811 | `	return sUri.sScheme.nByte!=0 && sUri.sHost.nByte!=0;` |
|      8 | 5812 | `}` |
|      - | 5813 | `/* The Fv sanitizers build their result by appending directly to the call` |
|      - | 5814 | ` * context (ph7_result_string accumulates, like htmlspecialchars), emitting each` |
|      - | 5815 | ` * kept run in one call and seeding "" so an all-stripped input yields "". */` |
|      - | 5816 | `/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */` |
|     37 | 5817 | `static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){` |
|     37 | 5818 | `	int i, runStart = 0;` |
|     37 | 5819 | `	ph7_result_string(pCtx,"",0);` |
|     97 | 5820 | `	for( i=0; i<n; i++ ){` |
|     91 | 5821 | `		char c = z[i];` |
|     91 | 5822 | `		int keep = (c>='0'&&c<='9') \|\| c=='+' \|\| c=='-';` |
|     91 | 5823 | `		if( !keep && isFloat ){` |
|     38 | 5824 | `			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))` |
|     23 | 5825 | `			    \|\| (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))` |
|     36 | 5826 | `			    \|\| ((c=='e'\|\|c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));` |
|     12 | 5827 | `		}` |
|     61 | 5828 | `		if( !keep ){` |
|     33 | 5829 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     33 | 5830 | `			runStart = i+1;` |
|     16 | 5831 | `		}` |
|     31 | 5832 | `	}` |
|      7 | 5833 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 5834 | `}` |
|      - | 5835 | `/* Return non-zero when byte c must be stripped under the STRIP_* flags. Shared` |
|      - | 5836 | ` * by the UNSAFE_RAW string filter and SANITIZE_SPECIAL_CHARS. STRIP_LOW drops` |
|      - | 5837 | `` * bytes <32, STRIP_HIGH drops bytes >=127 (incl. DEL), STRIP_BACKTICK drops '`'.`` |
|      - | 5838 | ` * Matches php_filter_strip(); verified byte-exact vs php 8.5.7. */` |
|    287 | 5839 | `static int FvStripByte(unsigned char c,int flags){` |
|    287 | 5840 | `	if( (flags & FV_FLAG_STRIP_LOW)      && c<32 )    { return 1; }` |
|    281 | 5841 | `	if( (flags & FV_FLAG_STRIP_HIGH)     && c>=127 )  { return 1; }` |
|    269 | 5842 | `	if( (flags & FV_FLAG_STRIP_BACKTICK) && c==0x60 ) { return 1; }` |
|    267 | 5843 | `	return 0;` |
|    144 | 5844 | `}` |
|      - | 5845 | `/* FILTER_UNSAFE_RAW / FILTER_DEFAULT with flags: no default transform, but the` |
|      - | 5846 | ` * STRIP/ENCODE flags apply. Precedence (per php_filter_unsafe_raw, verified` |
|      - | 5847 | ` * vs php 8.5.7): a byte is first tested for stripping; a surviving byte is then` |
|      - | 5848 | ` * encoded as a decimal numeric entity if ENCODE_LOW (<32) / ENCODE_HIGH (>=127)` |
|      - | 5849 | ` * is set, and '&' becomes "&#38;" under ENCODE_AMP. So STRIP_LOW\|ENCODE_LOW` |
|      - | 5850 | ` * strips (nothing left to encode). Bytes are treated individually — ENCODE_HIGH` |
|      - | 5851 | ` * numeric-encodes each byte of a multibyte sequence separately, not the codepoint. */` |
|     25 | 5852 | `static void FvSanitizeString(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 5853 | `	int i, runStart = 0;` |
|     25 | 5854 | `	ph7_result_string(pCtx,"",0);` |
|    193 | 5855 | `	for( i=0; i<n; i++ ){` |
|    179 | 5856 | `		unsigned char c = (unsigned char)z[i];` |
|    179 | 5857 | `		if( FvStripByte(c,flags) ){` |
|     13 | 5858 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     13 | 5859 | `			runStart = i+1;` |
|     13 | 5860 | `			continue;` |
|      - | 5861 | `		}` |
|    167 | 5862 | `		if( c=='&' && (flags & FV_FLAG_ENCODE_AMP) ){` |
|      3 | 5863 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      3 | 5864 | `			ph7_result_string(pCtx,"&#38;",-1);` |
|      3 | 5865 | `			runStart = i+1;` |
|    166 | 5866 | `		}else if( (c<32 && (flags & FV_FLAG_ENCODE_LOW))` |
|    164 | 5867 | `		       \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     37 | 5868 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 5869 | `			ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|      9 | 5870 | `			runStart = i+1;` |
|      4 | 5871 | `		}` |
|     79 | 5872 | `	}` |
|     15 | 5873 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     15 | 5874 | `}` |
|      - | 5875 | `/* FILTER_SANITIZE_SPECIAL_CHARS: encode <>&"' and every control byte <32 as a` |
|      - | 5876 | ` * decimal numeric entity (&#60; &#38; &#34; ...). The STRIP_* flags remove bytes` |
|      - | 5877 | ` * before encoding; ENCODE_HIGH numeric-encodes surviving bytes >=127. Bytes >=128` |
|      - | 5878 | ` * are otherwise passed through verbatim (this filter is NOT UTF-8-aware — only the` |
|      - | 5879 | ` * FULL variant is). Byte-exact vs php 8.5.7. */` |
|     13 | 5880 | `static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int flags){` |
|     13 | 5881 | `	int i, runStart = 0;` |
|      - | 5882 | `	const char *zEnt;` |
|     13 | 5883 | `	ph7_result_string(pCtx,"",0);` |
|    131 | 5884 | `	for( i=0; i<n; i++ ){` |
|    119 | 5885 | `		unsigned char c = (unsigned char)z[i];` |
|    119 | 5886 | `		if( FvStripByte(c,flags) ){` |
|      9 | 5887 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 5888 | `			runStart = i+1;` |
|      9 | 5889 | `			continue;` |
|      - | 5890 | `		}` |
|    111 | 5891 | `		switch( c ){` |
|      3 | 5892 | `		case '<':  zEnt = "&#60;"; break;` |
|      3 | 5893 | `		case '>':  zEnt = "&#62;"; break;` |
|     11 | 5894 | `		case '&':  zEnt = "&#38;"; break;` |
|      3 | 5895 | `		case '"':  zEnt = "&#34;"; break;` |
|      3 | 5896 | `		case '\'': zEnt = "&#39;"; break;` |
|     46 | 5897 | `		default:` |
|      - | 5898 | `			/* Control bytes <32 are always numeric-encoded; bytes >=127 only when` |
|      - | 5899 | `			 * ENCODE_HIGH is set. Everything else stays in the current run. */` |
|     93 | 5900 | `			if( c<32 \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     17 | 5901 | `				if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     17 | 5902 | `				ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|     17 | 5903 | `				runStart = i+1;` |
|      8 | 5904 | `			}` |
|     93 | 5905 | `			continue; /* keep in the current run */` |
|      - | 5906 | `		}` |
|     19 | 5907 | `		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     19 | 5908 | `		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */` |
|     19 | 5909 | `		runStart = i+1;` |
|     10 | 5910 | `	}` |
|     13 | 5911 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     13 | 5912 | `}` |
|      - | 5913 | `/* HTML 4.01 named-entity table (codepoint -> "&name;") used by the UTF-8-aware` |
|      - | 5914 | ` * FULL_SPECIAL_CHARS filter, sorted ascending by codepoint for binary search.` |
|      - | 5915 | ` * Generated from php 8.5.7 (the exact set php_escape_html_entities emits for the` |
|      - | 5916 | ` * default document type); the five inline specials <>&"' are handled separately,` |
|      - | 5917 | ` * so every entry here is a codepoint >=0xA0. 248 rows. */` |
|      - | 5918 | `static const struct { sxu32 cp; const char *zEnt; } aHtml401Ent[] = {` |
|      - | 5919 | `	{0x00A0,"&nbsp;"},{0x00A1,"&iexcl;"},{0x00A2,"&cent;"},{0x00A3,"&pound;"},` |
|      - | 5920 | `	{0x00A4,"&curren;"},{0x00A5,"&yen;"},{0x00A6,"&brvbar;"},{0x00A7,"&sect;"},` |
|      - | 5921 | `	{0x00A8,"&uml;"},{0x00A9,"&copy;"},{0x00AA,"&ordf;"},{0x00AB,"&laquo;"},` |
|      - | 5922 | `	{0x00AC,"&not;"},{0x00AD,"&shy;"},{0x00AE,"&reg;"},{0x00AF,"&macr;"},` |
|      - | 5923 | `	{0x00B0,"&deg;"},{0x00B1,"&plusmn;"},{0x00B2,"&sup2;"},{0x00B3,"&sup3;"},` |
|      - | 5924 | `	{0x00B4,"&acute;"},{0x00B5,"&micro;"},{0x00B6,"&para;"},{0x00B7,"&middot;"},` |
|      - | 5925 | `	{0x00B8,"&cedil;"},{0x00B9,"&sup1;"},{0x00BA,"&ordm;"},{0x00BB,"&raquo;"},` |
|      - | 5926 | `	{0x00BC,"&frac14;"},{0x00BD,"&frac12;"},{0x00BE,"&frac34;"},{0x00BF,"&iquest;"},` |
|      - | 5927 | `	{0x00C0,"&Agrave;"},{0x00C1,"&Aacute;"},{0x00C2,"&Acirc;"},{0x00C3,"&Atilde;"},` |
|      - | 5928 | `	{0x00C4,"&Auml;"},{0x00C5,"&Aring;"},{0x00C6,"&AElig;"},{0x00C7,"&Ccedil;"},` |
|      - | 5929 | `	{0x00C8,"&Egrave;"},{0x00C9,"&Eacute;"},{0x00CA,"&Ecirc;"},{0x00CB,"&Euml;"},` |
|      - | 5930 | `	{0x00CC,"&Igrave;"},{0x00CD,"&Iacute;"},{0x00CE,"&Icirc;"},{0x00CF,"&Iuml;"},` |
|      - | 5931 | `	{0x00D0,"&ETH;"},{0x00D1,"&Ntilde;"},{0x00D2,"&Ograve;"},{0x00D3,"&Oacute;"},` |
|      - | 5932 | `	{0x00D4,"&Ocirc;"},{0x00D5,"&Otilde;"},{0x00D6,"&Ouml;"},{0x00D7,"&times;"},` |
|      - | 5933 | `	{0x00D8,"&Oslash;"},{0x00D9,"&Ugrave;"},{0x00DA,"&Uacute;"},{0x00DB,"&Ucirc;"},` |
|      - | 5934 | `	{0x00DC,"&Uuml;"},{0x00DD,"&Yacute;"},{0x00DE,"&THORN;"},{0x00DF,"&szlig;"},` |
|      - | 5935 | `	{0x00E0,"&agrave;"},{0x00E1,"&aacute;"},{0x00E2,"&acirc;"},{0x00E3,"&atilde;"},` |
|      - | 5936 | `	{0x00E4,"&auml;"},{0x00E5,"&aring;"},{0x00E6,"&aelig;"},{0x00E7,"&ccedil;"},` |
|      - | 5937 | `	{0x00E8,"&egrave;"},{0x00E9,"&eacute;"},{0x00EA,"&ecirc;"},{0x00EB,"&euml;"},` |
|      - | 5938 | `	{0x00EC,"&igrave;"},{0x00ED,"&iacute;"},{0x00EE,"&icirc;"},{0x00EF,"&iuml;"},` |
|      - | 5939 | `	{0x00F0,"&eth;"},{0x00F1,"&ntilde;"},{0x00F2,"&ograve;"},{0x00F3,"&oacute;"},` |
|      - | 5940 | `	{0x00F4,"&ocirc;"},{0x00F5,"&otilde;"},{0x00F6,"&ouml;"},{0x00F7,"&divide;"},` |
|      - | 5941 | `	{0x00F8,"&oslash;"},{0x00F9,"&ugrave;"},{0x00FA,"&uacute;"},{0x00FB,"&ucirc;"},` |
|      - | 5942 | `	{0x00FC,"&uuml;"},{0x00FD,"&yacute;"},{0x00FE,"&thorn;"},{0x00FF,"&yuml;"},` |
|      - | 5943 | `	{0x0152,"&OElig;"},{0x0153,"&oelig;"},{0x0160,"&Scaron;"},{0x0161,"&scaron;"},` |
|      - | 5944 | `	{0x0178,"&Yuml;"},{0x0192,"&fnof;"},{0x02C6,"&circ;"},{0x02DC,"&tilde;"},` |
|      - | 5945 | `	{0x0391,"&Alpha;"},{0x0392,"&Beta;"},{0x0393,"&Gamma;"},{0x0394,"&Delta;"},` |
|      - | 5946 | `	{0x0395,"&Epsilon;"},{0x0396,"&Zeta;"},{0x0397,"&Eta;"},{0x0398,"&Theta;"},` |
|      - | 5947 | `	{0x0399,"&Iota;"},{0x039A,"&Kappa;"},{0x039B,"&Lambda;"},{0x039C,"&Mu;"},` |
|      - | 5948 | `	{0x039D,"&Nu;"},{0x039E,"&Xi;"},{0x039F,"&Omicron;"},{0x03A0,"&Pi;"},` |
|      - | 5949 | `	{0x03A1,"&Rho;"},{0x03A3,"&Sigma;"},{0x03A4,"&Tau;"},{0x03A5,"&Upsilon;"},` |
|      - | 5950 | `	{0x03A6,"&Phi;"},{0x03A7,"&Chi;"},{0x03A8,"&Psi;"},{0x03A9,"&Omega;"},` |
|      - | 5951 | `	{0x03B1,"&alpha;"},{0x03B2,"&beta;"},{0x03B3,"&gamma;"},{0x03B4,"&delta;"},` |
|      - | 5952 | `	{0x03B5,"&epsilon;"},{0x03B6,"&zeta;"},{0x03B7,"&eta;"},{0x03B8,"&theta;"},` |
|      - | 5953 | `	{0x03B9,"&iota;"},{0x03BA,"&kappa;"},{0x03BB,"&lambda;"},{0x03BC,"&mu;"},` |
|      - | 5954 | `	{0x03BD,"&nu;"},{0x03BE,"&xi;"},{0x03BF,"&omicron;"},{0x03C0,"&pi;"},` |
|      - | 5955 | `	{0x03C1,"&rho;"},{0x03C2,"&sigmaf;"},{0x03C3,"&sigma;"},{0x03C4,"&tau;"},` |
|      - | 5956 | `	{0x03C5,"&upsilon;"},{0x03C6,"&phi;"},{0x03C7,"&chi;"},{0x03C8,"&psi;"},` |
|      - | 5957 | `	{0x03C9,"&omega;"},{0x03D1,"&thetasym;"},{0x03D2,"&upsih;"},{0x03D6,"&piv;"},` |
|      - | 5958 | `	{0x2002,"&ensp;"},{0x2003,"&emsp;"},{0x2009,"&thinsp;"},{0x200C,"&zwnj;"},` |
|      - | 5959 | `	{0x200D,"&zwj;"},{0x200E,"&lrm;"},{0x200F,"&rlm;"},{0x2013,"&ndash;"},` |
|      - | 5960 | `	{0x2014,"&mdash;"},{0x2018,"&lsquo;"},{0x2019,"&rsquo;"},{0x201A,"&sbquo;"},` |
|      - | 5961 | `	{0x201C,"&ldquo;"},{0x201D,"&rdquo;"},{0x201E,"&bdquo;"},{0x2020,"&dagger;"},` |
|      - | 5962 | `	{0x2021,"&Dagger;"},{0x2022,"&bull;"},{0x2026,"&hellip;"},{0x2030,"&permil;"},` |
|      - | 5963 | `	{0x2032,"&prime;"},{0x2033,"&Prime;"},{0x2039,"&lsaquo;"},{0x203A,"&rsaquo;"},` |
|      - | 5964 | `	{0x203E,"&oline;"},{0x2044,"&frasl;"},{0x20AC,"&euro;"},{0x2111,"&image;"},` |
|      - | 5965 | `	{0x2118,"&weierp;"},{0x211C,"&real;"},{0x2122,"&trade;"},{0x2135,"&alefsym;"},` |
|      - | 5966 | `	{0x2190,"&larr;"},{0x2191,"&uarr;"},{0x2192,"&rarr;"},{0x2193,"&darr;"},` |
|      - | 5967 | `	{0x2194,"&harr;"},{0x21B5,"&crarr;"},{0x21D0,"&lArr;"},{0x21D1,"&uArr;"},` |
|      - | 5968 | `	{0x21D2,"&rArr;"},{0x21D3,"&dArr;"},{0x21D4,"&hArr;"},{0x2200,"&forall;"},` |
|      - | 5969 | `	{0x2202,"&part;"},{0x2203,"&exist;"},{0x2205,"&empty;"},{0x2207,"&nabla;"},` |
|      - | 5970 | `	{0x2208,"&isin;"},{0x2209,"&notin;"},{0x220B,"&ni;"},{0x220F,"&prod;"},` |
|      - | 5971 | `	{0x2211,"&sum;"},{0x2212,"&minus;"},{0x2217,"&lowast;"},{0x221A,"&radic;"},` |
|      - | 5972 | `	{0x221D,"&prop;"},{0x221E,"&infin;"},{0x2220,"&ang;"},{0x2227,"&and;"},` |
|      - | 5973 | `	{0x2228,"&or;"},{0x2229,"&cap;"},{0x222A,"&cup;"},{0x222B,"&int;"},` |
|      - | 5974 | `	{0x2234,"&there4;"},{0x223C,"&sim;"},{0x2245,"&cong;"},{0x2248,"&asymp;"},` |
|      - | 5975 | `	{0x2260,"&ne;"},{0x2261,"&equiv;"},{0x2264,"&le;"},{0x2265,"&ge;"},` |
|      - | 5976 | `	{0x2282,"&sub;"},{0x2283,"&sup;"},{0x2284,"&nsub;"},{0x2286,"&sube;"},` |
|      - | 5977 | `	{0x2287,"&supe;"},{0x2295,"&oplus;"},{0x2297,"&otimes;"},{0x22A5,"&perp;"},` |
|      - | 5978 | `	{0x22C5,"&sdot;"},{0x2308,"&lceil;"},{0x2309,"&rceil;"},{0x230A,"&lfloor;"},` |
|      - | 5979 | `	{0x230B,"&rfloor;"},{0x2329,"&lang;"},{0x232A,"&rang;"},{0x25CA,"&loz;"},` |
|      - | 5980 | `	{0x2660,"&spades;"},{0x2663,"&clubs;"},{0x2665,"&hearts;"},{0x2666,"&diams;"}` |
|      - | 5981 | `};` |
|      - | 5982 | `/* Binary-search aHtml401Ent[] for cp; return its "&name;" entity or 0. */` |
|     41 | 5983 | `static const char *FvHtml401Lookup(sxu32 cp){` |
|     41 | 5984 | `	int lo = 0, hi = (int)SX_ARRAYSIZE(aHtml401Ent) - 1;` |
|    323 | 5985 | `	while( lo <= hi ){` |
|    309 | 5986 | `		int mid = (lo + hi) / 2;` |
|    309 | 5987 | `		sxu32 c = aHtml401Ent[mid].cp;` |
|    309 | 5988 | `		if( c == cp ){ return aHtml401Ent[mid].zEnt; }` |
|    283 | 5989 | `		if( c < cp ){ lo = mid + 1; } else { hi = mid - 1; }` |
|      1 | 5990 | `	}` |
|     15 | 5991 | `	return 0;` |
|     21 | 5992 | `}` |
|      - | 5993 | `/* Decode one strict-UTF-8 sequence at p (< zEnd). On success returns its byte` |
|      - | 5994 | ` * length (1..4) and sets *pCp to the codepoint; on any malformed, overlong,` |
|      - | 5995 | ` * surrogate, truncated or out-of-range (>U+10FFFF) sequence returns 0. Matches` |
|      - | 5996 | ` * PHP's UTF-8 validation used by FULL_SPECIAL_CHARS (verified vs php 8.5.7). */` |
|    101 | 5997 | `static int FvUtf8Next(const unsigned char *p,const unsigned char *zEnd,sxu32 *pCp){` |
|    101 | 5998 | `	unsigned char c = p[0];` |
|    101 | 5999 | `	if( c < 0x80 ){ *pCp = c; return 1; }` |
|    101 | 6000 | `	if( c < 0xC2 ){ return 0; }              /* 0x80-0xBF stray cont / 0xC0-0xC1 overlong */` |
|     99 | 6001 | `	if( c < 0xE0 ){                          /* 2-byte: U+0080..U+07FF */` |
|     47 | 6002 | `		if( zEnd-p < 2 \|\| (p[1]&0xC0)!=0x80 ){ return 0; }` |
|     45 | 6003 | `		*pCp = ((sxu32)(c&0x1F)<<6) \| (p[1]&0x3F);` |
|     45 | 6004 | `		return 2;` |
|      - | 6005 | `	}` |
|     53 | 6006 | `	if( c < 0xF0 ){                          /* 3-byte: U+0800..U+FFFF minus surrogates */` |
|      - | 6007 | `		sxu32 cp;` |
|     47 | 6008 | `		if( zEnd-p < 3 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 ){ return 0; }` |
|     33 | 6009 | `		cp = ((sxu32)(c&0x0F)<<12) \| ((sxu32)(p[1]&0x3F)<<6) \| (p[2]&0x3F);` |
|     33 | 6010 | `		if( cp < 0x800 \|\| (cp>=0xD800 && cp<=0xDFFF) ){ return 0; }` |
|     29 | 6011 | `		*pCp = cp;` |
|     29 | 6012 | `		return 3;` |
|      - | 6013 | `	}` |
|      7 | 6014 | `	if( c < 0xF5 ){                          /* 4-byte: U+10000..U+10FFFF */` |
|      - | 6015 | `		sxu32 cp;` |
|      5 | 6016 | `		if( zEnd-p < 4 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 \|\| (p[3]&0xC0)!=0x80 ){ return 0; }` |
|      5 | 6017 | `		cp = ((sxu32)(c&0x07)<<18) \| ((sxu32)(p[1]&0x3F)<<12) \| ((sxu32)(p[2]&0x3F)<<6) \| (p[3]&0x3F);` |
|      5 | 6018 | `		if( cp < 0x10000 \|\| cp > 0x10FFFF ){ return 0; }` |
|      5 | 6019 | `		*pCp = cp;` |
|      5 | 6020 | `		return 4;` |
|      - | 6021 | `	}` |
|      3 | 6022 | `	return 0;                                /* 0xF5-0xFF */` |
|     51 | 6023 | `}` |
|      - | 6024 | `/* FILTER_SANITIZE_FULL_SPECIAL_CHARS: htmlentities-style, UTF-8-aware. Encodes` |
|      - | 6025 | ` * <>&"' as named entities ("'" -> &#039;; quotes suppressed under NO_ENCODE_QUOTES),` |
|      - | 6026 | ` * and every valid UTF-8 codepoint with an HTML 4.01 named entity as that entity;` |
|      - | 6027 | ` * valid codepoints without a named entity (and low control bytes) pass through` |
|      - | 6028 | ` * verbatim. If the input contains ANY invalid UTF-8 the whole result is "".` |
|      - | 6029 | ` * The STRIP/ENCODE flags do NOT apply to this filter (only NO_ENCODE_QUOTES).` |
|      - | 6030 | ` * php's filter does NOT re-encode valid pre-existing entities ("&amp;" stays,` |
|      - | 6031 | ` * "&bogus;" becomes "&amp;bogus;"), i.e. double_encode=false semantics —` |
|      - | 6032 | ` * exactly htmlentities(ENT_QUOTES\|ENT_HTML401, double_encode: false), so this` |
|      - | 6033 | ` * delegates to the shared encoder. Byte-exact vs php 8.5.7. */` |
|     25 | 6034 | `static void FvSanitizeFull(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 6035 | `	int iEntFlags = (flags & FV_FLAG_NO_ENCODE_QUOTES) ? 0 : PH7_ENT_QUOTES;` |
|     25 | 6036 | `	HtmlEscape(pCtx,z,n,iEntFlags,1/*bAll*/,0/*bDoubleEncode*/);` |
|     25 | 6037 | `}` |
|      - | 6038 | `/* ---------------------------------------------------------------------------` |
|      - | 6039 | ` * UTF-8-aware HTML entity core (htmlspecialchars/htmlentities family).` |
|      - | 6040 | ` * Prototyped next to the five builtins earlier in this file; lives here so it` |
|      - | 6041 | ` * can share aHtml401Ent[]/FvHtml401Lookup()/FvUtf8Next() with the filter_var` |
|      - | 6042 | ` * FULL_SPECIAL_CHARS filter above. Byte-exact vs php 8.5.7 (oracle-swept).` |
|      - | 6043 | ` * ------------------------------------------------------------------------ */` |
|      - | 6044 | `/* Encode cp as UTF-8 into zBuf (>= 4 bytes); return the byte length 1..4.` |
|      - | 6045 | ` * Thin wrapper over the engine-wide SX_WRITE_UTF8 (sxmacros.h). */` |
|    585 | 6046 | `static int HtmlCpUtf8(sxu32 cp,char *zBuf){` |
|    585 | 6047 | `	sxu8 *z = (sxu8 *)zBuf;` |
|    585 | 6048 | `	SX_WRITE_UTF8(z,cp);` |
|    585 | 6049 | `	return (int)(z - (sxu8 *)zBuf);` |
|      1 | 6050 | `}` |
|      - | 6051 | `/* Doctype-allowed codepoint test (php's unicode_cp_is_allowed) — gates what a` |
|      - | 6052 | ` * numeric reference may DECODE to. Oracle-pinned per doctype: HTML401` |
|      - | 6053 | ` * disallows C0 (except TAB/LF/CR) and DEL..U+009F; XML1 and XHTML share the` |
|      - | 6054 | ` * XML rules — DEL..U+009F allowed, U+FFFE/U+FFFF excluded; HTML5 swaps CR` |
|      - | 6055 | ` * for FF (0x0C) and excludes the noncharacters (U+FDD0..U+FDEF and every` |
|      - | 6056 | ` * U+xFFFE/U+xFFFF). Surrogates are disallowed everywhere. */` |
|     91 | 6057 | `static int HtmlCpAllowed(sxu32 cp,int iFlags){` |
|     91 | 6058 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|     91 | 6059 | `	if( cp==0x09 \|\| cp==0x0A ){ return 1; }` |
|     87 | 6060 | `	if( cp==0x0D ){ return iDoc != PH7_ENT_DOC_HTML5; }` |
|     85 | 6061 | `	if( cp==0x0C ){ return iDoc == PH7_ENT_DOC_HTML5; }` |
|     85 | 6062 | `	if( cp < 0x20 \|\| cp > 0x10FFFF ){ return 0; }` |
|     79 | 6063 | `	if( cp>=0xD800 && cp<=0xDFFF ){ return 0; }` |
|     77 | 6064 | `	if( cp>=0x7F && cp<=0x9F ){ return iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML; }` |
|     71 | 6065 | `	if( iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML ){` |
|    ! 0 | 6066 | `		return cp!=0xFFFE && cp!=0xFFFF;` |
|      - | 6067 | `	}` |
|     71 | 6068 | `	if( iDoc == PH7_ENT_DOC_HTML5 ){` |
|      9 | 6069 | `		if( cp>=0xFDD0 && cp<=0xFDEF ){ return 0; }` |
|      9 | 6070 | `		if( (cp & 0xFFFF) >= 0xFFFE ){ return 0; }` |
|      4 | 6071 | `	}` |
|     71 | 6072 | `	return 1;` |
|     46 | 6073 | `}` |
|      - | 6074 | `/* The ENT_DISALLOWED gate for RAW characters on the ENCODE side. Same as the` |
|      - | 6075 | ` * decode gate except CR under HTML5: php's encode-side unicode_cp_is_allowed` |
|      - | 6076 | ` * keeps a literal "\r" verbatim under ENT_HTML5\|ENT_DISALLOWED while the` |
|      - | 6077 | ` * decode side leaves "&#13;" un-decoded (oracle-pinned at flags 176). */` |
|      9 | 6078 | `static int HtmlCpAllowedEncode(sxu32 cp,int iFlags){` |
|      9 | 6079 | `	if( cp==0x0D && (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 ){ return 1; }` |
|      9 | 6080 | `	return HtmlCpAllowed(cp,iFlags);` |
|      5 | 6081 | `}` |
|      - | 6082 | `/* Numeric-reference validity for the double_encode=false "is this already a` |
|      - | 6083 | ` * valid entity" test — a MUCH looser predicate than the decode gate above:` |
|      - | 6084 | ` * any codepoint <= U+10FFFF is valid (controls and surrogates included, every` |
|      - | 6085 | ` * doctype). ENT_DISALLOWED re-tightens non-HTML401 doctypes to the decode` |
|      - | 6086 | ` * gate, except that HTML5 exempts surrogates. All oracle-pinned: &#0; and` |
|      - | 6087 | ` * &#xD800; stay verbatim at flags 11 and 139; flags -1 (HTML5+DISALLOWED)` |
|      - | 6088 | ` * re-encodes &#0; and &#x10FFFF; but still keeps &#xD800;; flags 144` |
|      - | 6089 | ` * (XML1+DISALLOWED) re-encodes &#xD800;. */` |
|      9 | 6090 | `static int HtmlNumericAllowed(sxu32 cp,int iFlags){` |
|      9 | 6091 | `	if( cp > 0x10FFFF ){ return 0; }` |
|      7 | 6092 | `	if( (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML401 ){ return 1; /* never tightened */ }` |
|    ! 0 | 6093 | `	if( (iFlags & PH7_ENT_DISALLOWED)` |
|    ! 0 | 6094 | `	 && !((iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 && cp>=0xD800 && cp<=0xDFFF)` |
|    ! 0 | 6095 | `	 && !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|    ! 0 | 6096 | `	return 1;` |
|      5 | 6097 | `}` |
|      - | 6098 | `/* How many bytes the malformed UTF-8 sequence at p consumes — php's` |
|      - | 6099 | ` * get_next_char failure step (one U+FFFD substitution / one ENT_IGNORE drop` |
|      - | 6100 | ` * per MAXIMAL invalid subpart, not per byte): a prefix-valid sequence eats` |
|      - | 6101 | ` * its continuation bytes ("\xE0\x80\xAF" is ONE unit) while a byte that could` |
|      - | 6102 | ` * start a new sequence is left for the next round. */` |
|      5 | 6103 | `static int HtmlUtf8Trail(unsigned char c){ return c>=0x80 && c<=0xBF; }` |
|     11 | 6104 | `static int HtmlUtf8Lead(unsigned char c){ return c<0x80 \|\| (c>=0xC2 && c<=0xF4); }` |
|     15 | 6105 | `static int HtmlUtf8FailAdvance(const unsigned char *p,const unsigned char *zEnd){` |
|     15 | 6106 | `	unsigned char c = p[0];` |
|     15 | 6107 | `	int nAvail = (int)(zEnd - p);` |
|     15 | 6108 | `	if( c < 0xC2 \|\| c > 0xF4 ){ return 1; } /* stray trail / C0-C1 / F5-FF */` |
|     13 | 6109 | `	if( c < 0xE0 ){` |
|      3 | 6110 | `		if( nAvail < 2 ){ return 1; }` |
|      3 | 6111 | `		return HtmlUtf8Lead(p[1]) ? 1 : 2;` |
|      - | 6112 | `	}` |
|     11 | 6113 | `	if( c < 0xF0 ){` |
|     11 | 6114 | `		if( nAvail >= 3 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) ){` |
|      3 | 6115 | `			return 3; /* complete but overlong/surrogate */` |
|      - | 6116 | `		}` |
|      9 | 6117 | `		if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 6118 | `		if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 6119 | `		return 3;` |
|      - | 6120 | `	}` |
|    ! 0 | 6121 | `	if( nAvail >= 4 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) && HtmlUtf8Trail(p[3]) ){` |
|    ! 0 | 6122 | `		return 4; /* complete but overlong / > U+10FFFF */` |
|      - | 6123 | `	}` |
|    ! 0 | 6124 | `	if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 6125 | `	if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 6126 | `	if( nAvail < 4 \|\| HtmlUtf8Lead(p[3]) ){ return 3; }` |
|    ! 0 | 6127 | `	return 4;` |
|      8 | 6128 | `}` |
|      - | 6129 | `/* The basic special entities, shared by named matching, the hsc_decode` |
|      - | 6130 | ` * numeric whitelist and the translation-table builder so the sets can never` |
|      - | 6131 | ` * drift apart. (&apos; is not an HTML 4.01 entity — doctype-gated below.) */` |
|      - | 6132 | `static const struct { const char *zEnt; int n; sxu32 cp; } aHtmlSpecEnt[] = {` |
|      - | 6133 | `	{"&amp;",5,38},{"&lt;",4,60},{"&gt;",4,62},{"&quot;",6,34},{"&apos;",6,39}` |
|      - | 6134 | `};` |
|      - | 6135 | `/* Does this doctype consult the named-entity table (aHtml401Ent)? XML 1.0 has` |
|      - | 6136 | ` * no named entities beyond the specials; XHTML/HTML5 are approximated by the` |
|      - | 6137 | ` * HTML 4.01 table (documented divergence, PLAN.md §3.9). */` |
|     63 | 6138 | `static int HtmlDocHasNamedTable(int iDoc){` |
|     63 | 6139 | `	return iDoc != PH7_ENT_DOC_XML1;` |
|      1 | 6140 | `}` |
|      - | 6141 | `/* The single-quote entity per doctype. Oracle-pinned asymmetry: for every` |
|      - | 6142 | ` * non-HTML401 doctype htmlspecialchars emits &apos; while htmlentities` |
|      - | 6143 | ` * (bEntities) keeps &#039; under XHTML too. The translation table mirrors` |
|      - | 6144 | ` * whichever function the requested table belongs to. */` |
|     29 | 6145 | `static const char *HtmlAposEntity(int iDoc,int bEntities){` |
|     29 | 6146 | `	if( iDoc == PH7_ENT_DOC_HTML401 \|\| (bEntities && iDoc == PH7_ENT_DOC_XHTML) ){` |
|     21 | 6147 | `		return "&#039;";` |
|      - | 6148 | `	}` |
|      9 | 6149 | `	return "&apos;";` |
|     15 | 6150 | `}` |
|      - | 6151 | `/* Try to parse one HTML entity at z (z[0]=='&', z < zEnd). bFull selects the` |
|      - | 6152 | ` * html_entity_decode set (doctype named table + any allowed numeric ref) vs` |
|      - | 6153 | ` * the htmlspecialchars_decode set (the basic specials + quote numerics only).` |
|      - | 6154 | ` * Named matching is case-SENSITIVE and the ';' is required (both PHP-exact);` |
|      - | 6155 | ` * numeric refs accept dec/hex (x or X) with any number of leading zeros but` |
|      - | 6156 | ` * reject out-of-range, surrogate and doctype-disallowed codepoints (the` |
|      - | 6157 | ` * caller then leaves the source verbatim). Quote-flag gating is NOT applied` |
|      - | 6158 | ` * here — the same routine doubles as the "is this a valid entity" test for` |
|      - | 6159 | ` * double_encode=false, which ignores the quote bits (oracle-pinned).` |
|      - | 6160 | ` * bEncodeCheck selects the looser HtmlNumericAllowed predicate used by that` |
|      - | 6161 | ` * double_encode test; decode callers pass 0 for the HtmlCpAllowed gate.` |
|      - | 6162 | ` * On success sets *pCp / *pnConsumed and returns 1. */` |
|    172 | 6163 | `static int HtmlParseEntity(const unsigned char *z,const unsigned char *zEnd,` |
|      1 | 6164 | `                           int iFlags,int bFull,int bEncodeCheck,sxu32 *pCp,int *pnConsumed){` |
|    173 | 6165 | `	int nAvail = (int)(zEnd - z);` |
|    173 | 6166 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6167 | `	sxu32 n;` |
|    173 | 6168 | `	if( nAvail < 4 ){ return 0; } /* shortest entities: &lt; &#9; */` |
|    169 | 6169 | `	if( z[1] == '#' ){` |
|      - | 6170 | `		/* Numeric reference */` |
|     89 | 6171 | `		sxu32 cp = 0;` |
|     89 | 6172 | `		int i = 2, bHex = 0, nDig = 0;` |
|     89 | 6173 | `		if( z[i]=='x' \|\| z[i]=='X' ){ bHex = 1; i++; }` |
|    317 | 6174 | `		for( ; i < nAvail && z[i] != ';' ; i++ ){` |
|      - | 6175 | `			int v;` |
|    221 | 6176 | `			unsigned char c = z[i];` |
|    221 | 6177 | `			if( c>='0' && c<='9' ){ v = c - '0'; }` |
|     17 | 6178 | `			else if( bHex && c>='a' && c<='f' ){ v = c - 'a' + 10; }` |
|     17 | 6179 | `			else if( bHex && c>='A' && c<='F' ){ v = c - 'A' + 10; }` |
|    ! 0 | 6180 | `			else { return 0; }` |
|      - | 6181 | `			/* Stop accumulating once out of range (keeps validating the shape;` |
|      - | 6182 | `			 * max intermediate is 0x10FFFF*16+15, no sxu32 overflow). */` |
|    221 | 6183 | `			if( cp <= 0x10FFFF ){ cp = cp * (bHex ? 16 : 10) + (sxu32)v; }` |
|    221 | 6184 | `			nDig++;` |
|    111 | 6185 | `		}` |
|     97 | 6186 | `		if( nDig == 0 \|\| i >= nAvail ){ return 0; } /* no digits / no ';' */` |
|     97 | 6187 | `		if( bEncodeCheck ? !HtmlNumericAllowed(cp,iFlags) : !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|     83 | 6188 | `		if( !bFull ){` |
|      - | 6189 | `			/* hsc_decode: numeric refs to the five specials only. */` |
|     99 | 6190 | `			for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) && aHtmlSpecEnt[n].cp != cp ; n++ ){}` |
|     25 | 6191 | `			if( n >= SX_ARRAYSIZE(aHtmlSpecEnt) ){ return 0; }` |
|     11 | 6192 | `		}` |
|     75 | 6193 | `		*pCp = cp;` |
|     75 | 6194 | `		*pnConsumed = i + 1;` |
|     75 | 6195 | `		return 1;` |
|      - | 6196 | `	}` |
|      - | 6197 | `	/* Named reference — every entity name starts with a letter, so anything` |
|      - | 6198 | `	 * else can bail out before touching the tables. */` |
|     81 | 6199 | `	if( !((z[1]>='a' && z[1]<='z') \|\| (z[1]>='A' && z[1]<='Z')) ){ return 0; }` |
|    287 | 6200 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) ; n++ ){` |
|    265 | 6201 | `		if( aHtmlSpecEnt[n].cp == 39 && iDoc == PH7_ENT_DOC_HTML401 ){ continue; }` |
|    243 | 6202 | `		if( nAvail >= aHtmlSpecEnt[n].n && SyMemcmp(z,aHtmlSpecEnt[n].zEnt,(sxu32)aHtmlSpecEnt[n].n) == 0 ){` |
|     53 | 6203 | `			*pCp = aHtmlSpecEnt[n].cp;` |
|     53 | 6204 | `			*pnConsumed = aHtmlSpecEnt[n].n;` |
|     53 | 6205 | `			return 1;` |
|      - | 6206 | `		}` |
|     96 | 6207 | `	}` |
|     23 | 6208 | `	if( bFull && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 6209 | `		/* Linear scan of the 248-row table: runs only at '&'-then-letter` |
|      - | 6210 | `		 * positions and guarantees the decode set can never drift from the` |
|      - | 6211 | `		 * encode table. The first-letter guard skips the SyStrlen/SyMemcmp` |
|      - | 6212 | `		 * for ~96% of rows. */` |
|   3369 | 6213 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|      - | 6214 | `			sxu32 nEnt;` |
|   3357 | 6215 | `			if( z[1] != (unsigned char)aHtml401Ent[n].zEnt[1] ){ continue; }` |
|    121 | 6216 | `			nEnt = SyStrlen(aHtml401Ent[n].zEnt);` |
|    121 | 6217 | `			if( (sxu32)nAvail >= nEnt && SyMemcmp(z,aHtml401Ent[n].zEnt,nEnt) == 0 ){` |
|      7 | 6218 | `				*pCp = aHtml401Ent[n].cp;` |
|      7 | 6219 | `				*pnConsumed = (int)nEnt;` |
|      7 | 6220 | `				return 1;` |
|      - | 6221 | `			}` |
|     58 | 6222 | `		}` |
|      6 | 6223 | `	}` |
|     17 | 6224 | `	return 0;` |
|     88 | 6225 | `}` |
|      - | 6226 | `/* Shared encoder for htmlspecialchars (bAll=0) and htmlentities (bAll=1).` |
|      - | 6227 | ` * Invalid UTF-8 policy: ENT_IGNORE drops the byte (and wins over SUBSTITUTE),` |
|      - | 6228 | ` * ENT_SUBSTITUTE emits one U+FFFD per invalid byte, neither -> the whole` |
|      - | 6229 | ` * result is "" (pre-validated in a first pass: the accumulating result API` |
|      - | 6230 | ` * cannot roll back — same reason FvSanitizeFull is two-pass). */` |
|     94 | 6231 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 6232 | `                       int iFlags,int bAll,int bDoubleEncode){` |
|     95 | 6233 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     95 | 6234 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|      - | 6235 | `	const unsigned char *runStart;` |
|     95 | 6236 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6237 | `	sxu32 cp;` |
|     95 | 6238 | `	if( (iFlags & (PH7_ENT_IGNORE\|PH7_ENT_SUBSTITUTE)) == 0 ){` |
|      - | 6239 | `		/* Pass 1: any malformed sequence rejects the entire input. ASCII` |
|      - | 6240 | `		 * bytes cannot be malformed, so skip them without the decoder. */` |
|    381 | 6241 | `		while( p < zEnd ){` |
|      - | 6242 | `			int len;` |
|    323 | 6243 | `			if( *p < 0x80 ){ p++; continue; }` |
|     37 | 6244 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     37 | 6245 | `			if( len == 0 ){ ph7_result_string(pCtx,"",0); return; }` |
|     27 | 6246 | `			p += len;` |
|      1 | 6247 | `		}` |
|     59 | 6248 | `		p = (const unsigned char *)zIn;` |
|     29 | 6249 | `	}` |
|     85 | 6250 | `	runStart = p;` |
|     85 | 6251 | `	ph7_result_string(pCtx,"",0);` |
|    455 | 6252 | `	while( p < zEnd ){` |
|    371 | 6253 | `		const char *zEnt = 0;` |
|      - | 6254 | `		int len;` |
|    371 | 6255 | `		if( *p < 0x80 ){` |
|    307 | 6256 | `			len = 1;` |
|    307 | 6257 | `			switch( *p ){` |
|     25 | 6258 | `			case '<': zEnt = "&lt;"; break;` |
|     25 | 6259 | `			case '>': zEnt = "&gt;"; break;` |
|     18 | 6260 | `			case '&':` |
|     37 | 6261 | `				zEnt = "&amp;";` |
|     37 | 6262 | `				if( !bDoubleEncode ){` |
|      - | 6263 | `					sxu32 eCp; int nEat;` |
|     25 | 6264 | `					if( HtmlParseEntity(p,zEnd,iFlags,1,1,&eCp,&nEat) ){` |
|      - | 6265 | `						/* A valid existing entity: keep it verbatim. */` |
|     13 | 6266 | `						zEnt = 0;` |
|     13 | 6267 | `						len = nEat;` |
|      6 | 6268 | `					}` |
|     12 | 6269 | `				}` |
|     37 | 6270 | `				break;` |
|     10 | 6271 | `			case '"':` |
|     21 | 6272 | `				if( iFlags & PH7_ENT_QUOTE_DOUBLE ){ zEnt = "&quot;"; }` |
|     21 | 6273 | `				break;` |
|     12 | 6274 | `			case '\'':` |
|     25 | 6275 | `				if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|     23 | 6276 | `					zEnt = HtmlAposEntity(iDoc,bAll);` |
|     11 | 6277 | `				}` |
|     25 | 6278 | `				break;` |
|     89 | 6279 | `			default:` |
|    179 | 6280 | `				if( (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode((sxu32)*p,iFlags) ){` |
|    ! 0 | 6281 | `					zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 6282 | `				}` |
|    178 | 6283 | `				break;` |
|      - | 6284 | `			}` |
|    154 | 6285 | `		}else{` |
|     65 | 6286 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     65 | 6287 | `			if( len == 0 ){` |
|      - | 6288 | `				/* Malformed subpart (IGNORE or SUBSTITUTE is set, else pass 1` |
|      - | 6289 | `				 * would have rejected): drop it or emit ONE U+FFFD for the` |
|      - | 6290 | `				 * whole unit (php substitutes per maximal invalid subpart). */` |
|     15 | 6291 | `				if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|     15 | 6292 | `				if( (iFlags & PH7_ENT_IGNORE) == 0 ){ ph7_result_string(pCtx,"\xEF\xBF\xBD",3); }` |
|     15 | 6293 | `				p += HtmlUtf8FailAdvance(p,zEnd);` |
|     15 | 6294 | `				runStart = p;` |
|     15 | 6295 | `				continue;` |
|      - | 6296 | `			}` |
|     51 | 6297 | `			if( bAll && HtmlDocHasNamedTable(iDoc) ){` |
|     41 | 6298 | `				zEnt = FvHtml401Lookup(cp);` |
|     20 | 6299 | `			}` |
|     51 | 6300 | `			if( zEnt == 0 && (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode(cp,iFlags) ){` |
|    ! 0 | 6301 | `				zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 6302 | `			}` |
|      - | 6303 | `		}` |
|    357 | 6304 | `		if( zEnt ){` |
|    135 | 6305 | `			if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|    135 | 6306 | `			ph7_result_string(pCtx,zEnt,-1);` |
|    135 | 6307 | `			runStart = p + len;` |
|     67 | 6308 | `		}` |
|    357 | 6309 | `		p += len;` |
|      1 | 6310 | `	}` |
|     85 | 6311 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     48 | 6312 | `}` |
|      - | 6313 | `/* Shared decoder for html_entity_decode (bFull=1) and htmlspecialchars_decode` |
|      - | 6314 | ` * (bFull=0). Quote refs (cp 34/39, named or numeric) are gated by the quote` |
|      - | 6315 | ` * bits and left verbatim when suppressed; an invalid entity leaves its '&'` |
|      - | 6316 | ` * verbatim and rescans right after it, which also yields PHP's no-double-` |
|      - | 6317 | ` * decode behavior ("&amp;lt;" -> "&lt;"). */` |
|     82 | 6318 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 6319 | `                         int iFlags,int bFull){` |
|     83 | 6320 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     83 | 6321 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|     83 | 6322 | `	const unsigned char *runStart = p;` |
|     83 | 6323 | `	ph7_result_string(pCtx,"",0);` |
|    557 | 6324 | `	while( p < zEnd ){` |
|      - | 6325 | `		sxu32 cp;` |
|      - | 6326 | `		int nEat;` |
|    510 | 6327 | `		if( *p != '&' ){ p++; continue; }` |
|    155 | 6328 | `		if( !HtmlParseEntity(p,zEnd,iFlags,bFull,0,&cp,&nEat) ){ p++; continue; }` |
|    124 | 6329 | `		if( (cp == 34 && (iFlags & PH7_ENT_QUOTE_DOUBLE) == 0)` |
|    117 | 6330 | `		 \|\| (cp == 39 && (iFlags & PH7_ENT_QUOTE_SINGLE) == 0) ){` |
|      - | 6331 | `			/* Suppressed quote: leave the entity source verbatim. */` |
|     37 | 6332 | `			p += nEat;` |
|     37 | 6333 | `			continue;` |
|      - | 6334 | `		}` |
|     89 | 6335 | `		if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|      - | 6336 | `		{` |
|      - | 6337 | `			char zBuf[4];` |
|     89 | 6338 | `			int n = HtmlCpUtf8(cp,zBuf);` |
|     89 | 6339 | `			ph7_result_string(pCtx,zBuf,n);` |
|      - | 6340 | `		}` |
|     89 | 6341 | `		p += nEat;` |
|     89 | 6342 | `		runStart = p;` |
|      1 | 6343 | `	}` |
|     79 | 6344 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     79 | 6345 | `}` |
|      - | 6346 | `/* Validate the optional charset argument at apArg[idx]: UTF-8 aliases (and` |
|      - | 6347 | ` * ""/NULL meaning the default) are accepted; anything else — including` |
|      - | 6348 | ` * php-supported single-byte charsets like ISO-8859-1, PHL is UTF-8-only per` |
|      - | 6349 | ` * PLAN.md §6 — raises PHP's unsupported-charset warning and is treated as` |
|      - | 6350 | ` * UTF-8 (ph7_context_throw_error_format prepends the function name). */` |
|    141 | 6351 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx){` |
|      - | 6352 | `	const char *zCs;` |
|      - | 6353 | `	int nCs;` |
|    148 | 6354 | `	if( nArg <= idx \|\| ph7_value_is_null(apArg[idx]) ){ return; }` |
|     15 | 6355 | `	zCs = ph7_value_to_string(apArg[idx],&nCs);` |
|     15 | 6356 | `	if( nCs == 0 ){ return; } /* "" selects the default charset (UTF-8) */` |
|     13 | 6357 | `	if( nCs == 5 && SyStrnicmp(zCs,"UTF-8",5) == 0 ){` |
|     13 | 6358 | `		return; /* php accepts only "UTF-8" (any case) silently — "UTF8" warns */` |
|      - | 6359 | `	}` |
|    ! 0 | 6360 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6361 | `		"Charset \"%.*s\" is not supported, assuming UTF-8",nCs,zCs);` |
|     71 | 6362 | `}` |
|      - | 6363 | `/* get_html_translation_table() worker: character (UTF-8 bytes) => entity.` |
|      - | 6364 | ` * The five specials come first in byte order, then — for HTML_ENTITIES with a` |
|      - | 6365 | ` * named-table doctype — the 248 aHtml401Ent rows ascending (oracle-pinned` |
|      - | 6366 | ` * ordering; 253 entries under the defaults). */` |
|    549 | 6367 | `static void HtmlTableAdd(ph7_value *pArray,ph7_value *pValue,const char *zKey,const char *zEnt){` |
|    549 | 6368 | `	ph7_value_string(pValue,zEnt,-1);` |
|    549 | 6369 | `	ph7_array_add_strkey_elem(pArray,zKey,pValue);` |
|    549 | 6370 | `	ph7_value_reset_string_cursor(pValue);` |
|    549 | 6371 | `}` |
|     13 | 6372 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags){` |
|      - | 6373 | `	ph7_value *pArray,*pValue;` |
|     13 | 6374 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6375 | `	sxu32 n;` |
|     13 | 6376 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 6377 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 6378 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|    ! 0 | 6379 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6380 | `		return;` |
|      - | 6381 | `	}` |
|     13 | 6382 | `	if( iFlags & PH7_ENT_QUOTE_DOUBLE ){` |
|     11 | 6383 | `		HtmlTableAdd(pArray,pValue,"\"","&quot;");` |
|      5 | 6384 | `	}` |
|     13 | 6385 | `	HtmlTableAdd(pArray,pValue,"&","&amp;");` |
|     13 | 6386 | `	if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|      - | 6387 | `		/* The apostrophe row mirrors the function each table belongs to:` |
|      - | 6388 | `		 * SPECIALCHARS follows htmlspecialchars, ENTITIES follows` |
|      - | 6389 | `		 * htmlentities (oracle-pinned at flags 35). */` |
|      7 | 6390 | `		HtmlTableAdd(pArray,pValue,"'",HtmlAposEntity(iDoc,iTable != 0));` |
|      3 | 6391 | `	}` |
|     13 | 6392 | `	HtmlTableAdd(pArray,pValue,"<","&lt;");` |
|     13 | 6393 | `	HtmlTableAdd(pArray,pValue,">","&gt;");` |
|     13 | 6394 | `	if( iTable != 0 /*php: any non-HTML_SPECIALCHARS table => entities*/ && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 6395 | `		char zKey[8];` |
|    499 | 6396 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|    497 | 6397 | `			int nK = HtmlCpUtf8(aHtml401Ent[n].cp,zKey);` |
|    497 | 6398 | `			zKey[nK] = 0;` |
|    497 | 6399 | `			HtmlTableAdd(pArray,pValue,zKey,aHtml401Ent[n].zEnt);` |
|    249 | 6400 | `		}` |
|      1 | 6401 | `	}` |
|     13 | 6402 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 6403 | `}` |
|     25 | 6404 | `static int FvEmailAllowed(unsigned char c){` |
|     25 | 6405 | `	if( (c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9') ){ return 1; }` |
|     16 | 6406 | `	return c=='!'\|\|c=='#'\|\|c=='$'\|\|c=='%'\|\|c=='&'\|\|c=='\''\|\|c=='*'\|\|c=='+'` |
|     10 | 6407 | ``	    \|\| c=='-'\|\|c=='='\|\|c=='?'\|\|c=='^'\|\|c=='_'\|\|c=='`'\|\|c=='{'\|\|c=='\|'`` |
|     15 | 6408 | `	    \|\| c=='}'\|\|c=='~'\|\|c=='@'\|\|c=='.'\|\|c=='['\|\|c==']';` |
|     13 | 6409 | `}` |
|     23 | 6410 | `static int FvUrlAllowed(unsigned char c){` |
|     23 | 6411 | `	return c>=33 && c<=126; /* PHP keeps every printable ASCII byte except space */` |
|      1 | 6412 | `}` |
|      - | 6413 | `/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */` |
|      5 | 6414 | `static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){` |
|      5 | 6415 | `	int i, runStart = 0;` |
|      5 | 6416 | `	ph7_result_string(pCtx,"",0);` |
|     51 | 6417 | `	for( i=0; i<n; i++ ){` |
|     47 | 6418 | `		unsigned char c = (unsigned char)z[i];` |
|     47 | 6419 | `		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){` |
|     11 | 6420 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     11 | 6421 | `			runStart = i+1;` |
|      5 | 6422 | `		}` |
|     24 | 6423 | `	}` |
|      5 | 6424 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      5 | 6425 | `}` |
|      - | 6426 | `/*` |
|      - | 6427 | ` * Apply the selected filter to one already-resolved input value and write the` |
|      - | 6428 | ` * result into pCtx. Shared by filter_var() and filter_input(): the caller has` |
|      - | 6429 | ` * already parsed $filter/$flags/$options. On validation failure the 'default'` |
|      - | 6430 | ` * option (if any) is returned, else null when FILTER_NULL_ON_FAILURE is set,` |
|      - | 6431 | ` * else false. A validating filter that passes returns the (string) input` |
|      - | 6432 | ` * unchanged; a sanitizer writes its transformed output directly.` |
|      - | 6433 | ` */` |
|    316 | 6434 | `static int FvApplyFilter(ph7_context *pCtx,ph7_value *pInput,` |
|      - | 6435 | `                         int iFilter,int iFlags,ph7_value *pOpts,` |
|      - | 6436 | `                         ph7_value *pDefault)` |
|      3 | 6437 | `{` |
|    319 | 6438 | `	int bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|      - | 6439 | `	const char *zVal; int nVal;` |
|      - | 6440 | `	/* An array/object input fails every scalar filter. */` |
|    319 | 6441 | `	if( ph7_value_is_array(pInput) ){ goto fail; }` |
|    317 | 6442 | `	zVal = ph7_value_to_string(pInput,&nVal);` |
|    317 | 6443 | `	switch( iFilter ){` |
|     28 | 6444 | `	case FV_VALIDATE_INT: {` |
|      - | 6445 | `		ph7_int64 v;` |
|     58 | 6446 | `		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }` |
|     31 | 6447 | `		if( pOpts ){` |
|      7 | 6448 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|      7 | 6449 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|      7 | 6450 | `			if( pMin && v<ph7_value_to_int64(pMin) ){ goto fail; }` |
|      7 | 6451 | `			if( pMax && v>ph7_value_to_int64(pMax) ){ goto fail; }` |
|      2 | 6452 | `		}` |
|     29 | 6453 | `		ph7_result_int64(pCtx,v);` |
|     29 | 6454 | `		return PH7_OK;` |
|      - | 6455 | `	}` |
|     34 | 6456 | `	case FV_VALIDATE_FLOAT: {` |
|      - | 6457 | `		double d;` |
|     69 | 6458 | `		if( !FvValidateFloat(zVal,nVal,iFlags,&d) ){ goto fail; }` |
|     39 | 6459 | `		ph7_result_double(pCtx,d);` |
|     39 | 6460 | `		return PH7_OK;` |
|      - | 6461 | `	}` |
|     14 | 6462 | `	case FV_VALIDATE_BOOLEAN: {` |
|      - | 6463 | `		int b;` |
|     29 | 6464 | `		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }` |
|     21 | 6465 | `		ph7_result_bool(pCtx,b);` |
|     21 | 6466 | `		return PH7_OK;` |
|      - | 6467 | `	}` |
|     25 | 6468 | `	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|     11 | 6469 | `	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;` |
|     28 | 6470 | `	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(zVal,nVal) ){ goto fail; }     goto pass;` |
|     11 | 6471 | `	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal) ){ goto fail; }    goto pass;` |
|     15 | 6472 | `	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal) ){ goto fail; }       goto pass;` |
|      3 | 6473 | `	case FV_VALIDATE_REGEXP: {` |
|      - | 6474 | `#ifdef PH7_ENABLE_PCRE` |
|      8 | 6475 | `		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;` |
|      8 | 6476 | `		const char *zRe; int nRe, matched = 0;` |
|      8 | 6477 | `		if( pRe==0 ){` |
|      3 | 6478 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6479 | `				"filter_var(): \"regexp\" option is missing");` |
|      - | 6480 | `		}` |
|      5 | 6481 | `		zRe = ph7_value_to_string(pRe,&nRe);` |
|      5 | 6482 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK \|\| !matched ){ goto fail; }` |
|      3 | 6483 | `		goto pass;` |
|      - | 6484 | `#else` |
|      - | 6485 | `		goto fail;` |
|      - | 6486 | `#endif` |
|      - | 6487 | `	}` |
|      3 | 6488 | `	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;` |
|      5 | 6489 | `	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;` |
|     13 | 6490 | `	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,iFlags); return PH7_OK;` |
|     25 | 6491 | `	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeFull(pCtx,zVal,nVal,iFlags);    return PH7_OK;` |
|      3 | 6492 | `	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 6493 | `	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;` |
|     13 | 6494 | `	case FV_DEFAULT:` |
|      - | 6495 | `		/* FILTER_UNSAFE_RAW / FILTER_DEFAULT: pass through unchanged unless a` |
|      - | 6496 | `		 * STRIP/ENCODE flag is set, in which case apply the string filter. */` |
|     28 | 6497 | `		if( iFlags & FV_FLAG_STRING_MASK ){` |
|     15 | 6498 | `			FvSanitizeString(pCtx,zVal,nVal,iFlags);` |
|     15 | 6499 | `			return PH7_OK;` |
|      - | 6500 | `		}` |
|     14 | 6501 | `		goto pass;` |
|    ! 0 | 6502 | `	default:` |
|    ! 0 | 6503 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6504 | `			"Unknown filter with ID %d",iFilter);` |
|    ! 0 | 6505 | `		break; /* unknown filter id -> fail */` |
|    ! 0 | 6506 | `	}` |
|     58 | 6507 | `fail:` |
|    118 | 6508 | `	if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|    114 | 6509 | `	else if( bNull ){ ph7_result_null(pCtx); }` |
|    108 | 6510 | `	else { ph7_result_bool(pCtx,0); }` |
|    118 | 6511 | `	return PH7_OK;` |
|     26 | 6512 | `pass: /* validation passed: return the (string) input unchanged */` |
|     54 | 6513 | `	ph7_result_string(pCtx,zVal,nVal);` |
|     54 | 6514 | `	return PH7_OK;` |
|    161 | 6515 | `}` |
|      - | 6516 | `/*` |
|      - | 6517 | ` * Parse the ($filter, $options) pair shared by filter_var()/filter_input() out` |
|      - | 6518 | ` * of apArg[iBase] ($filter) and apArg[iBase+1] ($options): $options is either a` |
|      - | 6519 | ` * plain flags int, or an array with 'flags' and an 'options' sub-array (whose` |
|      - | 6520 | ` * 'default' entry is the fallback value). Fills the four output pointers;` |
|      - | 6521 | ` * unset outputs keep the caller-provided defaults.` |
|      - | 6522 | ` */` |
|    328 | 6523 | `static void FvParseFilterArgs(int nArg,ph7_value **apArg,int iBase,` |
|      - | 6524 | `                              int *piFilter,int *piFlags,` |
|      - | 6525 | `                              ph7_value **ppOpts,ph7_value **ppDefault)` |
|      3 | 6526 | `{` |
|    331 | 6527 | `	if( nArg>iBase ){ *piFilter = ph7_value_to_int(apArg[iBase]); }` |
|    331 | 6528 | `	if( nArg>iBase+1 ){` |
|     88 | 6529 | `		if( ph7_value_is_array(apArg[iBase+1]) ){` |
|     42 | 6530 | `			ph7_value *pF = ph7_array_fetch(apArg[iBase+1],"flags",(int)sizeof("flags")-1);` |
|     42 | 6531 | `			if( pF ){ *piFlags = ph7_value_to_int(pF); }` |
|     42 | 6532 | `			*ppOpts = ph7_array_fetch(apArg[iBase+1],"options",(int)sizeof("options")-1);` |
|     42 | 6533 | `			if( *ppOpts && !ph7_value_is_array(*ppOpts) ){ *ppOpts = 0; }` |
|     42 | 6534 | `			if( *ppOpts ){ *ppDefault = ph7_array_fetch(*ppOpts,"default",(int)sizeof("default")-1); }` |
|     22 | 6535 | `		}else{` |
|     48 | 6536 | `			*piFlags = ph7_value_to_int(apArg[iBase+1]);` |
|      - | 6537 | `		}` |
|     43 | 6538 | `	}` |
|    331 | 6539 | `}` |
|      - | 6540 | `/*` |
|      - | 6541 | ` * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 6542 | ` *  Validate or sanitize a value; see FvApplyFilter for the failure semantics.` |
|      - | 6543 | ` */` |
|    306 | 6544 | `static int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6545 | `{` |
|    308 | 6546 | `	int iFilter = FV_DEFAULT, iFlags = 0;` |
|    308 | 6547 | `	ph7_value *pOpts = 0, *pDefault = 0;` |
|    308 | 6548 | `	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    308 | 6549 | `	FvParseFilterArgs(nArg,apArg,1,&iFilter,&iFlags,&pOpts,&pDefault);` |
|    308 | 6550 | `	return FvApplyFilter(pCtx,apArg[0],iFilter,iFlags,pOpts,pDefault);` |
|    155 | 6551 | `}` |
|      - | 6552 | `/*` |
|      - | 6553 | ` * filter_input($type, $var_name, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 6554 | ` *  Look up $var_name in the requested INPUT_* superglobal, then apply the` |
|      - | 6555 | ` *  filter. Semantics verified byte-for-byte against php 8.5:` |
|      - | 6556 | ` *   - variable NOT set: 'default' option wins, else false when` |
|      - | 6557 | ` *     FILTER_NULL_ON_FAILURE is set, else null. (Note the null/false roles are` |
|      - | 6558 | ` *     INVERTED relative to a present value that fails validation, which yields` |
|      - | 6559 | ` *     default > null-if-NULL_ON_FAILURE > false via FvApplyFilter.)` |
|      - | 6560 | ` *   - variable present: delegate to FvApplyFilter.` |
|      - | 6561 | ` *  Divergence: php reads a SAPI snapshot of the original request variables` |
|      - | 6562 | ` *  captured at startup; PHL reads the live superglobal. In CLI they match for` |
|      - | 6563 | ` *  the SAPI-registered keys (SCRIPT_NAME/PHP_SELF/DOCUMENT_ROOT); keys added` |
|      - | 6564 | ` *  only to the live $_SERVER (REQUEST_TIME/PWD/…) are visible here but not in` |
|      - | 6565 | ` *  php's snapshot.` |
|      - | 6566 | ` */` |
|     28 | 6567 | `static int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6568 | `{` |
|     30 | 6569 | `	int iType, iFilter = FV_DEFAULT, iFlags = 0;` |
|     30 | 6570 | `	ph7_value *pOpts = 0, *pDefault = 0, *pSuper, *pElem;` |
|      - | 6571 | `	const char *zVar, *zSuper; int nVar; sxu32 nSuper;` |
|     30 | 6572 | `	if( nArg<2 ){` |
|      7 | 6573 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|      2 | 6574 | `			"filter_input() expects at least 2 arguments, %d given",nArg);` |
|      - | 6575 | `	}` |
|     26 | 6576 | `	iType = ph7_value_to_int(apArg[0]);` |
|     26 | 6577 | `	switch( iType ){` |
|      3 | 6578 | `	case 0: zSuper = "_POST";   nSuper = (sxu32)sizeof("_POST")-1;   break; /* INPUT_POST */` |
|      3 | 6579 | `	case 1: zSuper = "_GET";    nSuper = (sxu32)sizeof("_GET")-1;    break; /* INPUT_GET */` |
|    ! 0 | 6580 | `	case 2: zSuper = "_COOKIE"; nSuper = (sxu32)sizeof("_COOKIE")-1; break; /* INPUT_COOKIE */` |
|    ! 0 | 6581 | `	case 4: zSuper = "_ENV";    nSuper = (sxu32)sizeof("_ENV")-1;    break; /* INPUT_ENV */` |
|     19 | 6582 | `	case 5: zSuper = "_SERVER"; nSuper = (sxu32)sizeof("_SERVER")-1; break; /* INPUT_SERVER */` |
|      1 | 6583 | `	default:` |
|      3 | 6584 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6585 | `			"filter_input(): Argument #1 ($type) must be an INPUT_* constant");` |
|      - | 6586 | `	}` |
|     23 | 6587 | `	zVar = ph7_value_to_string(apArg[1],&nVar);` |
|     23 | 6588 | `	FvParseFilterArgs(nArg,apArg,2,&iFilter,&iFlags,&pOpts,&pDefault);` |
|      - | 6589 | `	/* Resolve the variable from the superglobal (missing/non-array -> not set). */` |
|     23 | 6590 | `	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);` |
|     23 | 6591 | `	pElem = (pSuper && ph7_value_is_array(pSuper))` |
|     33 | 6592 | `		? ph7_array_fetch(pSuper,zVar,nVar) : 0;` |
|     23 | 6593 | `	if( pElem==0 ){` |
|      - | 6594 | `		/* Variable not set: default > false(if NULL_ON_FAILURE) > null. Note the` |
|      - | 6595 | `		 * false/null roles are inverted vs FvApplyFilter's present-but-fails path. */` |
|     13 | 6596 | `		if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|      9 | 6597 | `		else if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_bool(pCtx,0); }` |
|      7 | 6598 | `		else { ph7_result_null(pCtx); }` |
|     13 | 6599 | `		return PH7_OK;` |
|      - | 6600 | `	}` |
|     11 | 6601 | `	return FvApplyFilter(pCtx,pElem,iFilter,iFlags,pOpts,pDefault);` |
|     16 | 6602 | `}` |
|      - | 6603 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6604 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6605 | `/*` |
|      - | 6606 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 6607 |  |
|      - | 6608 | ` */` |
|      4 | 6609 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 6610 | `	const char *zInput, /* Raw input */` |
|      - | 6611 | `	int nByte,  /* Input length */` |
|      - | 6612 | `	int delim,  /* Delimiter */` |
|      - | 6613 | `	int encl,   /* Enclosure */` |
|      - | 6614 | `	int escape,  /* Escape character */` |
|      - | 6615 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 6616 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 6617 | `	)` |
|      1 | 6618 | `{` |
|      5 | 6619 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 6620 | `	const char *zIn = zInput;` |
|      - | 6621 | `	const char *zPtr;` |
|      - | 6622 | `	int isEnc;` |
|      - | 6623 | `	/* Start processing */` |
|      8 | 6624 | `	for(;;){` |
|     17 | 6625 | `		if( zIn >= zEnd ){` |
|      - | 6626 | `			/* No more input to process */` |
|      5 | 6627 | `			break;` |
|      - | 6628 | `		}` |
|     13 | 6629 | `		isEnc = 0;` |
|     13 | 6630 | `		zPtr = zIn;` |
|      - | 6631 | `		/* Find the first delimiter */` |
|     27 | 6632 | `		while( zIn < zEnd ){` |
|     23 | 6633 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 6634 | `				/* Delimiter found,break imediately */` |
|      5 | 6635 | `				break;` |
|     15 | 6636 | `			}else if( zIn[0] == encl ){` |
|      - | 6637 | `				/* Inside enclosure? */` |
|    ! 0 | 6638 | `				isEnc = !isEnc;` |
|     15 | 6639 | `			}else if( zIn[0] == escape ){` |
|      - | 6640 | `				/* Escape sequence */` |
|    ! 0 | 6641 | `				zIn++;` |
|    ! 0 | 6642 | `			}` |
|      - | 6643 | `			/* Advance the cursor */` |
|     15 | 6644 | `			zIn++;` |
|      1 | 6645 | `		}` |
|     13 | 6646 | `		if( zIn > zPtr ){` |
|     13 | 6647 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 6648 | `			sxi32 rc;` |
|      - | 6649 | `			/* Invoke the supllied callback */` |
|     13 | 6650 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 6651 | `				zPtr++;` |
|    ! 0 | 6652 | `				nByteChunk-=2;` |
|    ! 0 | 6653 | `			}` |
|     13 | 6654 | `			if( nByteChunk > 0 ){` |
|     13 | 6655 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 6656 | `				if( rc == SXERR_ABORT ){` |
|      - | 6657 | `					/* User callback request an operation abort */` |
|    ! 0 | 6658 | `					break;` |
|      - | 6659 | `				}` |
|      6 | 6660 | `			}` |
|      6 | 6661 | `		}` |
|      - | 6662 | `		/* Ignore trailing delimiter */` |
|     21 | 6663 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 6664 | `			zIn++;` |
|      1 | 6665 | `		}` |
|      1 | 6666 | `	}` |
|      5 | 6667 | `	return SXRET_OK;` |
|      1 | 6668 | `}` |
|      - | 6669 | `/*` |
|      - | 6670 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 6671 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 6672 | ` * argument to this callback.` |
|      - | 6673 | ` */` |
|     12 | 6674 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 6675 | `{` |
|     13 | 6676 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 6677 | `	ph7_value sEntry;` |
|      - | 6678 | `	SyString sToken;` |
|      - | 6679 | `	/* Insert the token in the given array */` |
|     13 | 6680 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 6681 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 6682 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 6683 | `	if( sToken.nByte < 1){` |
|    ! 0 | 6684 | `		return SXRET_OK;` |
|      - | 6685 | `	}` |
|     13 | 6686 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 6687 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 6688 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 6689 | `	return SXRET_OK;` |
|      7 | 6690 | `}` |
|      - | 6691 | `/*` |
|      - | 6692 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 6693 | ` *  Parse a CSV string into an array.` |
|      - | 6694 | ` * Parameters` |
|      - | 6695 | ` *  $input` |
|      - | 6696 | ` *   The string to parse.` |
|      - | 6697 | ` *  $delimiter` |
|      - | 6698 | ` *   Set the field delimiter (one character only).` |
|      - | 6699 | ` *  $enclosure` |
|      - | 6700 | ` *   Set the field enclosure character (one character only).` |
|      - | 6701 | ` *  $escape` |
|      - | 6702 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 6703 | ` * Return` |
|      - | 6704 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 6705 | ` */` |
|      2 | 6706 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6707 | `{` |
|      - | 6708 | `	const char *zInput,*zPtr;` |
|      - | 6709 | `	ph7_value *pArray;` |
|      3 | 6710 | `	int delim  = ',';   /* Delimiter */` |
|      3 | 6711 | `	int encl   = '"' ;  /* Enclosure */` |
|      3 | 6712 | `	int escape = '\\';  /* Escape character */` |
|      - | 6713 | `	int nLen;` |
|      3 | 6714 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6715 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 6716 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6717 | `		return PH7_OK;` |
|      - | 6718 | `	}` |
|      - | 6719 | `	/* Extract the raw input */` |
|      3 | 6720 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 6721 | `	if( nArg > 1 ){` |
|      - | 6722 | `		int i;` |
|      3 | 6723 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 6724 | `			/* Extract the delimiter */` |
|      3 | 6725 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 6726 | `			if( i > 0 ){` |
|      3 | 6727 | `				delim = zPtr[0];` |
|      1 | 6728 | `			}` |
|      1 | 6729 | `		}` |
|      3 | 6730 | `		if( nArg > 2 ){` |
|      3 | 6731 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 6732 | `				/* Extract the enclosure */` |
|      3 | 6733 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 6734 | `				if( i > 0 ){` |
|      3 | 6735 | `					encl = zPtr[0];` |
|      1 | 6736 | `				}` |
|      1 | 6737 | `			}` |
|      3 | 6738 | `			if( nArg > 3 ){` |
|      3 | 6739 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 6740 | `					/* Extract the escape character */` |
|      3 | 6741 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 6742 | `					if( i > 0 ){` |
|      3 | 6743 | `						escape = zPtr[0];` |
|      1 | 6744 | `					}` |
|      1 | 6745 | `				}` |
|      1 | 6746 | `			}` |
|      1 | 6747 | `		}` |
|      1 | 6748 | `	}` |
|      - | 6749 | `	/* Create our array */` |
|      3 | 6750 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 6751 | `	if( pArray == 0 ){` |
|      - | 6752 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|    ! 0 | 6753 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6754 | `	}` |
|      - | 6755 | `	/* Parse the raw input */` |
|      3 | 6756 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 6757 | `	/* Return the freshly created array */` |
|      3 | 6758 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 6759 | `	return PH7_OK;` |
|      2 | 6760 | `}` |
|      - | 6761 | `/*` |
|      - | 6762 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 6763 | ` * container.` |
|      - | 6764 | ` * Refer to [strip_tags()].` |
|      - | 6765 | ` */` |
|     10 | 6766 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 6767 | `{` |
|     11 | 6768 | `	const char *zEnd = &zTag[nByte];` |
|      - | 6769 | `	const char *zPtr;` |
|      - | 6770 | `	SyString sEntry;` |
|      - | 6771 | `	/* Strip tags */` |
|     10 | 6772 | `	for(;;){` |
|     45 | 6773 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 6774 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 6775 | `				zTag++;` |
|      1 | 6776 | `		}` |
|     21 | 6777 | `		if( zTag >= zEnd ){` |
|     11 | 6778 | `			break;` |
|      - | 6779 | `		}` |
|     11 | 6780 | `		zPtr = zTag;` |
|      - | 6781 | `		/* Delimit the tag */` |
|     25 | 6782 | `		while(zTag < zEnd ){` |
|     25 | 6783 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 6784 | `				/* UTF-8 stream */` |
|      3 | 6785 | `				zTag++;` |
|      5 | 6786 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 6787 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 6788 | `				break;` |
|    ! 0 | 6789 | `			}else{` |
|     13 | 6790 | `				zTag++;` |
|      - | 6791 | `			}` |
|      1 | 6792 | `		}` |
|     11 | 6793 | `		if( zTag > zPtr ){` |
|      - | 6794 | `			/* Perform the insertion */` |
|     11 | 6795 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 6796 | `			SyStringFullTrim(&sEntry);` |
|     11 | 6797 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 6798 | `		}` |
|      - | 6799 | `		/* Jump the trailing '>' */` |
|     11 | 6800 | `		zTag++;` |
|      1 | 6801 | `	}` |
|     11 | 6802 | `	return SXRET_OK;` |
|      1 | 6803 | `}` |
|      - | 6804 | `/*` |
|      - | 6805 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 6806 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 6807 | ` * Refer to [strip_tags()].` |
|      - | 6808 | ` */` |
|     36 | 6809 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 6810 | `{` |
|     37 | 6811 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 6812 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 6813 | `		SyString sTag;` |
|     85 | 6814 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 6815 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 6816 | `			zTag++;` |
|      1 | 6817 | `		}` |
|      - | 6818 | `		/* Delimit the tag */` |
|     25 | 6819 | `		zCur = zTag;` |
|     77 | 6820 | `		while(zTag < zEnd ){` |
|     77 | 6821 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 6822 | `				/* UTF-8 stream */` |
|      5 | 6823 | `				zTag++;` |
|      9 | 6824 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 6825 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 6826 | `				break;` |
|    ! 0 | 6827 | `			}else{` |
|     49 | 6828 | `				zTag++;` |
|      - | 6829 | `			}` |
|      1 | 6830 | `		}` |
|     25 | 6831 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 6832 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 6833 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 6834 | `		if( sTag.nByte > 0 ){` |
|      - | 6835 | `			SyString *aEntry,*pEntry;` |
|      - | 6836 | `			sxi32 rc;` |
|      - | 6837 | `			sxu32 n;` |
|      - | 6838 | `			/* Perform the lookup */` |
|     25 | 6839 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 6840 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 6841 | `				pEntry = &aEntry[n];` |
|      - | 6842 | `				/* Do the comparison */` |
|     25 | 6843 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 6844 | `				if( !rc ){` |
|     21 | 6845 | `					return SXRET_OK;` |
|      - | 6846 | `				}` |
|      3 | 6847 | `			}` |
|      2 | 6848 | `		}` |
|      2 | 6849 | `	}` |
|      - | 6850 | `	/* No such tag */` |
|     17 | 6851 | `	return SXERR_NOTFOUND;` |
|     19 | 6852 | `}` |
|      - | 6853 | `/*` |
|      - | 6854 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 6855 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 6856 | ` * Refer to [strip_tags()].` |
|      - | 6857 | ` */` |
|     16 | 6858 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 6859 | `{` |
|     17 | 6860 | `	const char *zEnd = &zIn[nByte];` |
|      - | 6861 | `	const char *zPtr,*zTag;` |
|      - | 6862 | `	SySet sSet;` |
|      - | 6863 | `	/* initialize the set of allowed tags */` |
|     17 | 6864 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 6865 | `	if( nTaglen > 0 ){` |
|      - | 6866 | `		/* Set of allowed tags */` |
|     11 | 6867 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 6868 | `	}` |
|      - | 6869 | `	/* Set the empty string */` |
|     17 | 6870 | `	ph7_result_string(pCtx,"",0);` |
|      - | 6871 | `	/* Start processing */` |
|     26 | 6872 | `	for(;;){` |
|     53 | 6873 | `		if(zIn >= zEnd){` |
|      - | 6874 | `			/* No more input to process */` |
|     15 | 6875 | `			break;` |
|      - | 6876 | `		}` |
|     39 | 6877 | `		zPtr = zIn;` |
|      - | 6878 | `		/* Find a tag */` |
|    133 | 6879 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 6880 | `			zIn++;` |
|      1 | 6881 | `		}` |
|     39 | 6882 | `		if( zIn > zPtr ){` |
|      - | 6883 | `			/* Consume raw input */` |
|     21 | 6884 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 6885 | `		}` |
|      - | 6886 | `		/* Ignore trailing null bytes */` |
|     39 | 6887 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 6888 | `			zIn++;` |
|    ! 0 | 6889 | `		}` |
|     39 | 6890 | `		if(zIn >= zEnd){` |
|      - | 6891 | `			/* No more input to process */` |
|      3 | 6892 | `			break;` |
|      - | 6893 | `		}` |
|     37 | 6894 | `		if( zIn[0] == '<' ){` |
|      - | 6895 | `			sxi32 rc;` |
|     37 | 6896 | `			zTag = zIn++;` |
|      - | 6897 | `			/* Delimit the tag */` |
|    127 | 6898 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 6899 | `				zIn++;` |
|      1 | 6900 | `			}` |
|     37 | 6901 | `			if( zIn < zEnd ){` |
|     37 | 6902 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 6903 | `			}` |
|      - | 6904 | `			/* Query the set */` |
|     37 | 6905 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 6906 | `			if( rc == SXRET_OK ){` |
|      - | 6907 | `				/* Keep the tag */` |
|     21 | 6908 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 6909 | `			}` |
|     18 | 6910 | `		}` |
|      1 | 6911 | `	}` |
|      - | 6912 | `	/* Cleanup */` |
|     17 | 6913 | `	SySetRelease(&sSet);` |
|     17 | 6914 | `	return SXRET_OK;` |
|      1 | 6915 | `}` |
|      - | 6916 | `/*` |
|      - | 6917 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 6918 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 6919 | ` * Parameters` |
|      - | 6920 | ` *  $str` |
|      - | 6921 | ` *  The input string.` |
|      - | 6922 | ` * $allowable_tags` |
|      - | 6923 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 6924 | ` * Return` |
|      - | 6925 | ` *  Returns the stripped string.` |
|      - | 6926 | ` */` |
|     14 | 6927 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6928 | `{` |
|     15 | 6929 | `	const char *zTaglist = 0;` |
|      - | 6930 | `	const char *zString;` |
|     15 | 6931 | `	int nTaglen = 0;` |
|      - | 6932 | `	int nLen;` |
|     15 | 6933 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6934 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 6935 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 6936 | `		return PH7_OK;` |
|      - | 6937 | `	}` |
|      - | 6938 | `	/* Point to the raw string */` |
|     15 | 6939 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 6940 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 6941 | `		/* Allowed tag */` |
|     11 | 6942 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 6943 | `	}` |
|      - | 6944 | `	/* Process input */` |
|     15 | 6945 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 6946 | `	return PH7_OK;` |
|      8 | 6947 | `}` |
|      - | 6948 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6949 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6950 | `/*` |
|      - | 6951 | ` * string str_shuffle(string $str)` |
|      - | 6952 |  |
|      - | 6953 | ` *  Randomly shuffles a string.` |
|      - | 6954 | ` * Parameters` |
|      - | 6955 | ` *  $str` |
|      - | 6956 | ` *   The input string.` |
|      - | 6957 | ` * Return` |
|      - | 6958 | ` *  Returns the shuffled string.` |
|      - | 6959 | ` */` |
|     10 | 6960 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6961 | `{` |
|      - | 6962 | `	const char *zString;` |
|      - | 6963 | `	int nLen,i,c;` |
|      - | 6964 | `	sxu32 iR;` |
|     11 | 6965 | `	if( nArg < 1 ){` |
|      - | 6966 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 6967 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 6968 | `		return PH7_OK;` |
|      - | 6969 | `	}` |
|      - | 6970 | `	/* Extract the target string */` |
|     11 | 6971 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 6972 | `	if( nLen < 1 ){` |
|      - | 6973 | `		/* Nothing to shuffle */` |
|      3 | 6974 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 6975 | `		return PH7_OK;` |
|      - | 6976 | `	}` |
|      - | 6977 | `	/* Shuffle the string */` |
|     43 | 6978 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 6979 | `		/* Generate a random number first */` |
|     35 | 6980 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 6981 | `		/* Extract a random offset */` |
|     35 | 6982 | `		c = zString[iR % nLen];` |
|      - | 6983 | `		/* Append it */` |
|     35 | 6984 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 6985 | `	}` |
|      9 | 6986 | `	return PH7_OK;` |
|      6 | 6987 | `}` |
|      - | 6988 | `/*` |
|      - | 6989 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 6990 | ` *  Convert a string to an array.` |
|      - | 6991 | ` * Parameters` |
|      - | 6992 | ` * $string` |
|      - | 6993 | ` *  The input string.` |
|      - | 6994 | ` * $split_length` |
|      - | 6995 | ` *  Maximum length of the chunk.` |
|      - | 6996 | ` * Return` |
|      - | 6997 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 6998 | ` *  except possibly the last one which may be shorter.` |
|      - | 6999 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 7000 | ` *  as the first (and only) array element.` |
|      - | 7001 | ` *  An empty string returns an empty array.` |
|      - | 7002 | ` * Errors` |
|      - | 7003 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 7004 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 7005 | ` *  ValueError if $split_length is less than 1.` |
|      - | 7006 | ` */` |
|     28 | 7007 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 7008 | `{` |
|      - | 7009 | `	const char *zString,*zEnd;` |
|      - | 7010 | `	ph7_value *pArray,*pValue;` |
|      - | 7011 | `	int split_len;` |
|      - | 7012 | `	int nLen;` |
|     33 | 7013 | `	if( nArg < 1 ){` |
|      4 | 7014 | `		return PH7_VmThrowException(pCtx,` |
|      - | 7015 | `			"ArgumentCountError",` |
|      - | 7016 | `			"str_split() expects at least 1 argument, %d given",` |
|      1 | 7017 | `			nArg` |
|      - | 7018 | `			);` |
|      - | 7019 | `	}` |
|      - | 7020 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     38 | 7021 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     40 | 7022 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 7023 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 7024 | `		return PH7_VmThrowException(pCtx,` |
|      - | 7025 | `			"TypeError",` |
|      - | 7026 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 7027 | `			ph7_type_name(apArg[0])` |
|      - | 7028 | `			);` |
|      - | 7029 | `	}` |
|      - | 7030 | `	/* Point to the target string */` |
|     27 | 7031 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 7032 | `	split_len = (int)sizeof(char);` |
|     27 | 7033 | `	if( nArg > 1 ){` |
|      - | 7034 | `		/* Split length */` |
|     17 | 7035 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 7036 | `		if( split_len < 1 ){` |
|      6 | 7037 | `			return PH7_VmThrowException(pCtx,` |
|      - | 7038 | `				"ValueError",` |
|      - | 7039 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 7040 | `				);` |
|      - | 7041 | `		}` |
|     11 | 7042 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 7043 | `			split_len = nLen;` |
|      1 | 7044 | `		}` |
|      5 | 7045 | `	}` |
|      - | 7046 | `	/* Create the array and the scalar value */` |
|     21 | 7047 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 7048 | `	/*Chunk value */` |
|     21 | 7049 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 7050 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 7051 | `		/* Return FALSE */` |
|    ! 0 | 7052 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7053 | `		return PH7_OK;` |
|      - | 7054 | `	}` |
|      - | 7055 | `	/* Point to the end of the string */` |
|     21 | 7056 | `	zEnd = &zString[nLen];` |
|      - | 7057 | `	/* Perform the requested operation */` |
|     48 | 7058 | `	for(;;){` |
|      - | 7059 | `		int nMax;` |
|     59 | 7060 | `		if( zString >= zEnd ){` |
|      - | 7061 | `			/* No more input to process */` |
|     21 | 7062 | `			break;` |
|      - | 7063 | `		}` |
|     39 | 7064 | `		nMax = (int)(zEnd-zString);` |
|     39 | 7065 | `		if( nMax < split_len ){` |
|      3 | 7066 | `			split_len = nMax;` |
|      1 | 7067 | `		}` |
|      - | 7068 | `		/* Copy the current chunk */` |
|     39 | 7069 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 7070 | `		/* Insert it */` |
|     39 | 7071 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 7072 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 7073 | `		}` |
|      - | 7074 | `		/* reset the string cursor */` |
|     39 | 7075 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 7076 | `		/* Update position */` |
|     39 | 7077 | `		zString += split_len;` |
|      1 | 7078 | `	}` |
|      - | 7079 | `	/*` |
|      - | 7080 | `	 * Return the array.` |
|      - | 7081 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 7082 | `	 * upon we return from this function.` |
|      - | 7083 | `	 */` |
|     21 | 7084 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 7085 | `	return PH7_OK;` |
|     19 | 7086 | `}` |
|      - | 7087 | `/*` |
|      - | 7088 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 7089 | ` * Refer to [strspn()].` |
|      - | 7090 | ` */` |
|     28 | 7091 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 7092 | `{` |
|     29 | 7093 | `	const char *zIn = *pzIn;` |
|      - | 7094 | `	const char *zPtr;` |
|      - | 7095 | `	/* Ignore leading white spaces */` |
|     29 | 7096 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 7097 | `		zIn++;` |
|    ! 0 | 7098 | `	}` |
|     29 | 7099 | `	if( zIn >= zEnd ){` |
|      - | 7100 | `		/* End of input */` |
|    ! 0 | 7101 | `		return SXERR_EOF;` |
|      - | 7102 | `	}` |
|     29 | 7103 | `	zPtr = zIn;` |
|      - | 7104 | `	/* Extract the token */` |
|    201 | 7105 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 7106 | `		zIn++;` |
|      1 | 7107 | `	}` |
|     29 | 7108 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 7109 | `	/* Synchronize pointers */` |
|     29 | 7110 | `	*pzIn = zIn;` |
|      - | 7111 | `	/* Return to the caller */` |
|     29 | 7112 | `	return SXRET_OK;` |
|     15 | 7113 | `}` |
|      - | 7114 | `/*` |
|      - | 7115 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 7116 | ` * return the longest match.` |
|      - | 7117 | ` * Refer to [strspn()].` |
|      - | 7118 | ` */` |
|     18 | 7119 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 7120 | `{` |
|     19 | 7121 | `	const char *zEnd = &zString[nLen];` |
|     19 | 7122 | `	const char *zIn = zString;` |
|      - | 7123 | `	int i,c;` |
|     45 | 7124 | `	for(;;){` |
|     91 | 7125 | `		if( zString >= zEnd ){` |
|      7 | 7126 | `			break;` |
|      - | 7127 | `		}` |
|      - | 7128 | `		/* Extract current character */` |
|     85 | 7129 | `		c = zString[0];` |
|      - | 7130 | `		/* Perform the lookup */` |
|    383 | 7131 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 7132 | `			if( c == zMask[i] ){` |
|      - | 7133 | `				/* Character found */` |
|     73 | 7134 | `				break;` |
|      - | 7135 | `			}` |
|    150 | 7136 | `		}` |
|     85 | 7137 | `		if( i >= nMaskLen ){` |
|      - | 7138 | `			/* Character not in the current mask,break immediately */` |
|     13 | 7139 | `			break;` |
|      - | 7140 | `		}` |
|      - | 7141 | `		/* Advance cursor */` |
|     73 | 7142 | `		zString++;` |
|      1 | 7143 | `	}` |
|      - | 7144 | `	/* Longest match */` |
|     19 | 7145 | `	return (int)(zString-zIn);` |
|      1 | 7146 | `}` |
|      - | 7147 | `/*` |
|      - | 7148 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 7149 | ` * Refer to [strcspn()].` |
|      - | 7150 | ` */` |
|     10 | 7151 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 7152 | `{` |
|     11 | 7153 | `	const char *zEnd = &zString[nLen];` |
|     11 | 7154 | `	const char *zIn = zString;` |
|      - | 7155 | `	int i,c;` |
|     12 | 7156 | `	for(;;){` |
|     25 | 7157 | `		if( zString >= zEnd ){` |
|      3 | 7158 | `			break;` |
|      - | 7159 | `		}` |
|      - | 7160 | `		/* Extract current character */` |
|     23 | 7161 | `		c = zString[0];` |
|      - | 7162 | `		/* Perform the lookup */` |
|     51 | 7163 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 7164 | `			if( c == zMask[i] ){` |
|      9 | 7165 | `				break;` |
|      - | 7166 | `			}` |
|     15 | 7167 | `		}` |
|     23 | 7168 | `		if( i < nMaskLen ){` |
|      - | 7169 | `			/* Character in the current mask,break immediately */` |
|      9 | 7170 | `			break;` |
|      - | 7171 | `		}` |
|      - | 7172 | `		/* Advance cursor */` |
|     15 | 7173 | `		zString++;` |
|      1 | 7174 | `	}` |
|      - | 7175 | `	/* Longest match */` |
|     11 | 7176 | `	return (int)(zString-zIn);` |
|      1 | 7177 | `}` |
|      - | 7178 | `/*` |
|      - | 7179 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 7180 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 7181 | ` *  of characters contained within a given mask.` |
|      - | 7182 | ` * Parameters` |
|      - | 7183 | ` * $str` |
|      - | 7184 | ` *  The input string.` |
|      - | 7185 | ` * $mask` |
|      - | 7186 | ` *  The list of allowable characters.` |
|      - | 7187 | ` * $start` |
|      - | 7188 | ` *  The position in subject to start searching.` |
|      - | 7189 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 7190 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 7191 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 7192 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 7193 | ` *  start'th position from the end of subject.` |
|      - | 7194 | ` * $length` |
|      - | 7195 | ` *  The length of the segment from subject to examine.` |
|      - | 7196 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 7197 | ` *  characters after the starting position.` |
|      - | 7198 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 7199 | ` *  position up to length characters from the end of subject.` |
|      - | 7200 | ` * Return` |
|      - | 7201 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 7202 | ` * in mask.` |
|      - | 7203 | ` */` |
|     24 | 7204 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7205 | `{` |
|      - | 7206 | `	const char *zString,*zMask,*zEnd;` |
|      - | 7207 | `	int iMasklen,iLen;` |
|      - | 7208 | `	SyString sToken;` |
|     25 | 7209 | `	int iCount = 0;` |
|      - | 7210 | `	int rc;` |
|     25 | 7211 | `	if( nArg < 2 ){` |
|      - | 7212 | `		/* Missing agruments,return zero */` |
|    ! 0 | 7213 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7214 | `		return PH7_OK;` |
|      - | 7215 | `	}` |
|      - | 7216 | `	/* Extract the target string */` |
|     25 | 7217 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7218 | `	/* Extract the mask */` |
|     25 | 7219 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 7220 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 7221 | `		/* Nothing to process,return zero */` |
|      7 | 7222 | `		ph7_result_int(pCtx,0);` |
|      7 | 7223 | `		return PH7_OK;` |
|      - | 7224 | `	}` |
|     19 | 7225 | `	if( nArg > 2 ){` |
|      - | 7226 | `		int nOfft;` |
|      - | 7227 | `		/* Extract the offset */` |
|      9 | 7228 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 7229 | `		if( nOfft < 0 ){` |
|    ! 0 | 7230 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 7231 | `			if( zBase > zString ){` |
|    ! 0 | 7232 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 7233 | `				zString = zBase;` |
|    ! 0 | 7234 | `			}else{` |
|      - | 7235 | `				/* Invalid offset */` |
|    ! 0 | 7236 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7237 | `				return PH7_OK;` |
|      - | 7238 | `			}` |
|    ! 0 | 7239 | `		}else{` |
|      9 | 7240 | `			if( nOfft >= iLen ){` |
|      - | 7241 | `				/* Invalid offset */` |
|    ! 0 | 7242 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7243 | `				return PH7_OK;` |
|    ! 0 | 7244 | `			}else{` |
|      - | 7245 | `				/* Update offset */` |
|      9 | 7246 | `				zString += nOfft;` |
|      9 | 7247 | `				iLen -= nOfft;` |
|      - | 7248 | `			}` |
|      - | 7249 | `		}` |
|      9 | 7250 | `		if( nArg > 3 ){` |
|      - | 7251 | `			int iUserlen;` |
|      - | 7252 | `			/* Extract the desired length */` |
|      9 | 7253 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 7254 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 7255 | `				iLen = iUserlen;` |
|      2 | 7256 | `			}` |
|      4 | 7257 | `		}` |
|      4 | 7258 | `	}` |
|      - | 7259 | `	/* Point to the end of the string */` |
|     19 | 7260 | `	zEnd = &zString[iLen];` |
|      - | 7261 | `	/* Extract the first non-space token */` |
|     19 | 7262 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 7263 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 7264 | `		/* Compare against the current mask */` |
|     19 | 7265 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 7266 | `	}` |
|      - | 7267 | `	/* Longest match */` |
|     19 | 7268 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 7269 | `	return PH7_OK;` |
|     13 | 7270 | `}` |
|      - | 7271 | `/*` |
|      - | 7272 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 7273 | ` *  Find length of initial segment not matching mask.` |
|      - | 7274 | ` * Parameters` |
|      - | 7275 | ` * $str` |
|      - | 7276 | ` *  The input string.` |
|      - | 7277 | ` * $mask` |
|      - | 7278 | ` *  The list of not allowed characters.` |
|      - | 7279 | ` * $start` |
|      - | 7280 | ` *  The position in subject to start searching.` |
|      - | 7281 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 7282 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 7283 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 7284 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 7285 | ` *  start'th position from the end of subject.` |
|      - | 7286 | ` * $length` |
|      - | 7287 | ` *  The length of the segment from subject to examine.` |
|      - | 7288 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 7289 | ` *  characters after the starting position.` |
|      - | 7290 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 7291 | ` *  position up to length characters from the end of subject.` |
|      - | 7292 | ` * Return` |
|      - | 7293 | ` *  Returns the length of the segment as an integer.` |
|      - | 7294 | ` */` |
|     14 | 7295 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7296 | `{` |
|      - | 7297 | `	const char *zString,*zMask,*zEnd;` |
|      - | 7298 | `	int iMasklen,iLen;` |
|      - | 7299 | `	SyString sToken;` |
|     15 | 7300 | `	int iCount = 0;` |
|      - | 7301 | `	int rc;` |
|     15 | 7302 | `	if( nArg < 2 ){` |
|      - | 7303 | `		/* Missing agruments,return zero */` |
|    ! 0 | 7304 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7305 | `		return PH7_OK;` |
|      - | 7306 | `	}` |
|      - | 7307 | `	/* Extract the target string */` |
|     15 | 7308 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7309 | `	/* Extract the mask */` |
|     15 | 7310 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 7311 | `	if( iLen < 1 ){` |
|      - | 7312 | `		/* Nothing to process,return zero */` |
|    ! 0 | 7313 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7314 | `		return PH7_OK;` |
|      - | 7315 | `	}` |
|     15 | 7316 | `	if( iMasklen < 1 ){` |
|      - | 7317 | `		/* No given mask,return the string length */` |
|      3 | 7318 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 7319 | `		return PH7_OK;` |
|      - | 7320 | `	}` |
|     13 | 7321 | `	if( nArg > 2 ){` |
|      - | 7322 | `		int nOfft;` |
|      - | 7323 | `		/* Extract the offset */` |
|     11 | 7324 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 7325 | `		if( nOfft < 0 ){` |
|    ! 0 | 7326 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 7327 | `			if( zBase > zString ){` |
|    ! 0 | 7328 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 7329 | `				zString = zBase;` |
|    ! 0 | 7330 | `			}else{` |
|      - | 7331 | `				/* Invalid offset */` |
|    ! 0 | 7332 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7333 | `				return PH7_OK;` |
|      - | 7334 | `			}` |
|    ! 0 | 7335 | `		}else{` |
|     11 | 7336 | `			if( nOfft >= iLen ){` |
|      - | 7337 | `				/* Invalid offset */` |
|      3 | 7338 | `				ph7_result_int(pCtx,0);` |
|      3 | 7339 | `				return PH7_OK;` |
|    ! 0 | 7340 | `			}else{` |
|      - | 7341 | `				/* Update offset */` |
|      9 | 7342 | `				zString += nOfft;` |
|      9 | 7343 | `				iLen -= nOfft;` |
|      - | 7344 | `			}` |
|      - | 7345 | `		}` |
|      9 | 7346 | `		if( nArg > 3 ){` |
|      - | 7347 | `			int iUserlen;` |
|      - | 7348 | `			/* Extract the desired length */` |
|    ! 0 | 7349 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 7350 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 7351 | `				iLen = iUserlen;` |
|    ! 0 | 7352 | `			}` |
|    ! 0 | 7353 | `		}` |
|      4 | 7354 | `	}` |
|      - | 7355 | `	/* Point to the end of the string */` |
|     11 | 7356 | `	zEnd = &zString[iLen];` |
|      - | 7357 | `	/* Extract the first non-space token */` |
|     11 | 7358 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 7359 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 7360 | `		/* Compare against the current mask */` |
|     11 | 7361 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 7362 | `	}` |
|      - | 7363 | `	/* Longest match */` |
|     11 | 7364 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 7365 | `	return PH7_OK;` |
|      8 | 7366 | `}` |
|      - | 7367 | `/*` |
|      - | 7368 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 7369 | ` *  Search a string for any of a set of characters.` |
|      - | 7370 | ` * Parameters` |
|      - | 7371 | ` *  $haystack` |
|      - | 7372 | ` *   The string where char_list is looked for.` |
|      - | 7373 | ` *  $char_list` |
|      - | 7374 | ` *   This parameter is case sensitive.` |
|      - | 7375 | ` * Return` |
|      - | 7376 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 7377 | ` */` |
|      4 | 7378 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7379 | `{` |
|      - | 7380 | `	const char *zString,*zList,*zEnd;` |
|      - | 7381 | `	int iLen,iListLen,i,c;` |
|      - | 7382 | `	sxu32 nOfft,nMax;` |
|      - | 7383 | `	sxi32 rc;` |
|      5 | 7384 | `	if( nArg < 2 ){` |
|      - | 7385 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7386 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7387 | `		return PH7_OK;` |
|      - | 7388 | `	}` |
|      - | 7389 | `	/* Extract the haystack and the char list */` |
|      5 | 7390 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 7391 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 7392 | `	if( iLen < 1 ){` |
|      - | 7393 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7394 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7395 | `		return PH7_OK;` |
|      - | 7396 | `	}` |
|      - | 7397 | `	/* Point to the end of the string */` |
|      5 | 7398 | `	zEnd = &zString[iLen];` |
|      5 | 7399 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 7400 | `	/* perform the requested operation */` |
|     15 | 7401 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 7402 | `		c = zList[i];` |
|     11 | 7403 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 7404 | `		if( rc == SXRET_OK ){` |
|      5 | 7405 | `			if( nMax < nOfft ){` |
|      3 | 7406 | `				nOfft = nMax;` |
|      1 | 7407 | `			}` |
|      2 | 7408 | `		}` |
|      6 | 7409 | `	}` |
|      5 | 7410 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 7411 | `		/* No such substring,return FALSE */` |
|      3 | 7412 | `		ph7_result_bool(pCtx,0);` |
|      2 | 7413 | `	}else{` |
|      - | 7414 | `		/* Return the substring */` |
|      3 | 7415 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 7416 | `	}` |
|      5 | 7417 | `	return PH7_OK;` |
|      3 | 7418 | `}` |
|      - | 7419 | `/* SPDX-SnippetBegin */` |
|      - | 7420 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 7421 | `/* SPDX-License-Identifier: blessing */` |
|      - | 7422 | `/*` |
|      - | 7423 | ` * string soundex(string $str)` |
|      - | 7424 | ` *  Calculate the soundex key of a string.` |
|      - | 7425 | ` * Parameters` |
|      - | 7426 | ` *  $str` |
|      - | 7427 | ` *   The input string.` |
|      - | 7428 | ` * Return` |
|      - | 7429 | ` *  Returns the soundex key as a string.` |
|      - | 7430 | ` * Note:` |
|      - | 7431 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 7432 | ` * source tree.` |
|      - | 7433 | ` */` |
|     22 | 7434 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7435 | `{` |
|      - | 7436 | `	const unsigned char *zIn;` |
|      - | 7437 | `	char zResult[8];` |
|      - | 7438 | `	int i, j;` |
|      - | 7439 | `	static const unsigned char iCode[] = {` |
|      - | 7440 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7441 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7442 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7443 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7444 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 7445 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 7446 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 7447 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 7448 | `	};` |
|     23 | 7449 | `	if( nArg < 1 ){` |
|      - | 7450 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7451 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7452 | `		return PH7_OK;` |
|      - | 7453 | `	}` |
|     23 | 7454 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     35 | 7455 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     23 | 7456 | `	if( zIn[i] ){` |
|     17 | 7457 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 7458 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 7459 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 7460 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 7461 | `			if( code>0 ){` |
|     45 | 7462 | `				if( code!=prevcode ){` |
|     33 | 7463 | `					prevcode = (unsigned char)code;` |
|     33 | 7464 | `					zResult[j++] = (char)code + '0';` |
|     16 | 7465 | `				}` |
|     23 | 7466 | `			}else{` |
|     49 | 7467 | `				prevcode = 0;` |
|      - | 7468 | `			}` |
|     47 | 7469 | `		}` |
|     33 | 7470 | `		while( j<4 ){` |
|     17 | 7471 | `			zResult[j++] = '0';` |
|      1 | 7472 | `		}` |
|     17 | 7473 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 7474 | `	}else{` |
|      - | 7475 | `	  /* No alphabetic character: PHP returns "0000" (not the SQLite "?000"). */` |
|      7 | 7476 | `	  ph7_result_string(pCtx,"0000",4);` |
|      - | 7477 | `	}` |
|     23 | 7478 | `	return PH7_OK;` |
|     12 | 7479 | `}` |
|      - | 7480 | `/* SPDX-SnippetEnd */` |
|      - | 7481 | `/*` |
|      - | 7482 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 7483 | ` *  Wraps a string to a given number of characters.` |
|      - | 7484 | ` * Parameters` |
|      - | 7485 | ` *  $str` |
|      - | 7486 | ` *   The input string.` |
|      - | 7487 | ` * $width` |
|      - | 7488 | ` *  The column width.` |
|      - | 7489 | ` * $break` |
|      - | 7490 | ` *  The line is broken using the optional break parameter.` |
|      - | 7491 | ` * Return` |
|      - | 7492 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 7493 | ` */` |
|     26 | 7494 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7495 | `{` |
|      - | 7496 | `	const char *zIn,*zBreak;` |
|      - | 7497 | `	SyBlob sWorker;` |
|      - | 7498 | `	int iLen,iBreaklen,iWidth,iCut,iStart,iSpace,iCur;` |
|      - | 7499 | `	sxi32 rc;` |
|     27 | 7500 | `	if( nArg < 1 ){` |
|      - | 7501 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7502 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7503 | `		return PH7_OK;` |
|      - | 7504 | `	}` |
|      - | 7505 | `	/* Extract the input string */` |
|     27 | 7506 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7507 | `	/* Width (default 75; PHP allows 0/negative — break at every space). */` |
|     27 | 7508 | `	iWidth = 75;` |
|     27 | 7509 | `	if( nArg > 1 ){` |
|     27 | 7510 | `		iWidth = ph7_value_to_int(apArg[1]);` |
|     13 | 7511 | `	}` |
|      - | 7512 | `	/* Break string (default "\n"). */` |
|     27 | 7513 | `	zBreak = "\n";` |
|     27 | 7514 | `	iBreaklen = (int)sizeof(char);` |
|     27 | 7515 | `	if( nArg > 2 ){` |
|     13 | 7516 | `		zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      6 | 7517 | `	}` |
|      - | 7518 | `	/* Cut long words? (default false). */` |
|     27 | 7519 | `	iCut = 0;` |
|     27 | 7520 | `	if( nArg > 3 ){` |
|      7 | 7521 | `		iCut = ph7_value_to_bool(apArg[3]);` |
|      3 | 7522 | `	}` |
|     27 | 7523 | `	if( iLen < 1 ){` |
|      - | 7524 | `		/* PHP returns the empty string for empty input before validating the other args. */` |
|      5 | 7525 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 7526 | `		return PH7_OK;` |
|      - | 7527 | `	}` |
|      - | 7528 | `	/* PHP 8 domain errors (catchable ValueError). */` |
|     23 | 7529 | `	if( iBreaklen < 1 ){` |
|      3 | 7530 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7531 | `			"wordwrap(): Argument #3 ($break) must not be empty");` |
|      - | 7532 | `	}` |
|     21 | 7533 | `	if( iWidth == 0 && iCut ){` |
|      3 | 7534 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7535 | `			"wordwrap(): Argument #4 ($cut_long_words) cannot be true when argument #2 ($width) is 0");` |
|      - | 7536 | `	}` |
|      - | 7537 | `	/*` |
|      - | 7538 | `	 * PHP's algorithm: a single left-to-right pass tracking the start of the` |
|      - | 7539 | `	 * current line (iStart) and the position of the last space seen on it` |
|      - | 7540 | `	 * (iSpace). A break is emitted when the line reaches the width, at the last` |
|      - | 7541 | `	 * space if there was one, otherwise (only when cut is enabled) hard at the` |
|      - | 7542 | `	 * boundary. An existing break sequence in the input resets the line.` |
|      - | 7543 | `	 */` |
|     19 | 7544 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     19 | 7545 | `	iStart = iSpace = iCur = 0;` |
|     19 | 7546 | `	rc = SXRET_OK;` |
|    551 | 7547 | `	while( iCur < iLen ){` |
|    533 | 7548 | `		if( iBreaklen <= iLen - iCur && SyMemcmp(&zIn[iCur],zBreak,(sxu32)iBreaklen) == 0 ){` |
|      - | 7549 | `			/* Existing break sequence in the input: copy it verbatim and reset the line. */` |
|    ! 0 | 7550 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart + iBreaklen));` |
|    ! 0 | 7551 | `			if( rc != SXRET_OK ){ goto oom; }` |
|    ! 0 | 7552 | `			iCur += iBreaklen;` |
|    ! 0 | 7553 | `			iStart = iSpace = iCur;` |
|    ! 0 | 7554 | `			continue;` |
|    533 | 7555 | `		}else if( zIn[iCur] == ' ' ){` |
|     67 | 7556 | `			if( iCur - iStart >= iWidth ){` |
|      - | 7557 | `				/* The line already fills the width at this space: break here (the space is consumed). */` |
|     13 | 7558 | `				rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     13 | 7559 | `				if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     13 | 7560 | `				if( rc != SXRET_OK ){ goto oom; }` |
|     13 | 7561 | `				iStart = iCur + 1;` |
|      6 | 7562 | `			}` |
|     67 | 7563 | `			iSpace = iCur;` |
|    500 | 7564 | `		}else if( iCut && iCur - iStart >= iWidth && iStart >= iSpace ){` |
|      - | 7565 | `			/* A word longer than the width with no space to break at: hard-cut at the boundary. */` |
|      7 | 7566 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|      7 | 7567 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|      7 | 7568 | `			if( rc != SXRET_OK ){ goto oom; }` |
|      7 | 7569 | `			iStart = iSpace = iCur;` |
|    464 | 7570 | `		}else if( iCur - iStart >= iWidth && iStart < iSpace ){` |
|      - | 7571 | `			/* Past the width mid-word: wrap back to the last space (which is consumed). */` |
|     17 | 7572 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iSpace - iStart));` |
|     17 | 7573 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     17 | 7574 | `			if( rc != SXRET_OK ){ goto oom; }` |
|     17 | 7575 | `			iStart = iSpace = iSpace + 1;` |
|      8 | 7576 | `		}` |
|    533 | 7577 | `		iCur++;` |
|      1 | 7578 | `	}` |
|      - | 7579 | `	/* Emit the trailing chunk. */` |
|     19 | 7580 | `	if( iStart < iCur ){` |
|     19 | 7581 | `		rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     19 | 7582 | `		if( rc != SXRET_OK ){ goto oom; }` |
|      9 | 7583 | `	}` |
|     19 | 7584 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|     19 | 7585 | `	SyBlobRelease(&sWorker);` |
|     19 | 7586 | `	return PH7_OK;` |
|    ! 0 | 7587 | `oom:` |
|    ! 0 | 7588 | `	SyBlobRelease(&sWorker);` |
|    ! 0 | 7589 | `	return PH7_ContextMemoryError(pCtx);` |
|     14 | 7590 | `}` |
|      - | 7591 | `/*` |
|      - | 7592 | ` * Check if the given character is a member of the given mask.` |
|      - | 7593 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 7594 | ` * Refer to [strtok()].` |
|      - | 7595 | ` */` |
|     30 | 7596 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 7597 | `{` |
|      - | 7598 | `	int i;` |
|     57 | 7599 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 7600 | `		if( c == zMask[i] ){` |
|     13 | 7601 | `			if( pOfft ){` |
|      5 | 7602 | `				*pOfft = i;` |
|      2 | 7603 | `			}` |
|     13 | 7604 | `			return TRUE;` |
|      - | 7605 | `		}` |
|     14 | 7606 | `	}` |
|     19 | 7607 | `	return FALSE;` |
|     16 | 7608 | `}` |
|      - | 7609 | `/*` |
|      - | 7610 | ` * Extract a single token from the input stream.` |
|      - | 7611 | ` * Refer to [strtok()].` |
|      - | 7612 | ` */` |
|      6 | 7613 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 7614 | `{` |
|      7 | 7615 | `	const char *zIn = *pzIn;` |
|      - | 7616 | `	const char *zPtr;` |
|      - | 7617 | `	/* Ignore leading delimiter */` |
|     11 | 7618 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 7619 | `		zIn++;` |
|      1 | 7620 | `	}` |
|      7 | 7621 | `	if( zIn >= zEnd ){` |
|      - | 7622 | `		/* End of input */` |
|    ! 0 | 7623 | `		return SXERR_EOF;` |
|      - | 7624 | `	}` |
|      7 | 7625 | `	zPtr = zIn;` |
|      - | 7626 | `	/* Extract the token */` |
|     13 | 7627 | `	while( zIn < zEnd ){` |
|     11 | 7628 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 7629 | `			/* UTF-8 stream */` |
|    ! 0 | 7630 | `			zIn++;` |
|    ! 0 | 7631 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 7632 | `		}else{` |
|     11 | 7633 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 7634 | `				break;` |
|      - | 7635 | `			}` |
|      7 | 7636 | `			zIn++;` |
|      - | 7637 | `		}` |
|      1 | 7638 | `	}` |
|      7 | 7639 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 7640 | `	/* Update the cursor */` |
|      7 | 7641 | `	*pzIn = zIn;` |
|      - | 7642 | `	/* Return to the caller */` |
|      7 | 7643 | `	return SXRET_OK;` |
|      4 | 7644 | `}` |
|      - | 7645 | `/* strtok auxiliary private data */` |
|      - | 7646 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 7647 | `struct strtok_aux_data` |
|      - | 7648 | `{` |
|      - | 7649 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 7650 | `	const char *zIn;   /* Current input stream */` |
|      - | 7651 | `	const char *zEnd;  /* End of input */` |
|      - | 7652 | `};` |
|      - | 7653 | `/*` |
|      - | 7654 | ` * string strtok(string $str,string $token)` |
|      - | 7655 | ` * string strtok(string $token)` |
|      - | 7656 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 7657 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 7658 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 7659 | ` *  words by using the space character as the token.` |
|      - | 7660 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 7661 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 7662 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 7663 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 7664 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 7665 | ` *  the argument are found.` |
|      - | 7666 | ` * Parameters` |
|      - | 7667 | ` *  $str` |
|      - | 7668 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 7669 | ` * $token` |
|      - | 7670 | ` *  The delimiter used when splitting up str.` |
|      - | 7671 | ` * Return` |
|      - | 7672 | ` *   Current token or FALSE on EOF.` |
|      - | 7673 | ` */` |
|      6 | 7674 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7675 | `{` |
|      - | 7676 | `	strtok_aux_data *pAux;` |
|      - | 7677 | `	const char *zMask;` |
|      - | 7678 | `	SyString sToken;` |
|      - | 7679 | `	int nMasklen;` |
|      - | 7680 | `	sxi32 rc;` |
|      7 | 7681 | `	if( nArg < 2 ){` |
|      - | 7682 | `		/* Extract top aux data */` |
|      5 | 7683 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      5 | 7684 | `		if( pAux == 0 ){` |
|      - | 7685 | `			/* No aux data,return FALSE */` |
|    ! 0 | 7686 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7687 | `			return PH7_OK;` |
|      - | 7688 | `		}` |
|      5 | 7689 | `		nMasklen = 0;` |
|      5 | 7690 | `		zMask = ""; /* cc warning */` |
|      5 | 7691 | `		if( nArg > 0 ){` |
|      - | 7692 | `			/* Extract the mask */` |
|      5 | 7693 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 7694 | `		}` |
|      5 | 7695 | `		if( nMasklen < 1 ){` |
|      - | 7696 | `			/* Invalid mask,return FALSE */` |
|    ! 0 | 7697 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 7698 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7699 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 7700 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7701 | `			return PH7_OK;` |
|      - | 7702 | `		}` |
|      - | 7703 | `		/* Extract the token */` |
|      5 | 7704 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 7705 | `		if( rc != SXRET_OK ){` |
|      - | 7706 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 7707 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 7708 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7709 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 7710 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7711 | `		}else{` |
|      - | 7712 | `			/* Return the extracted token */` |
|      5 | 7713 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 7714 | `		}` |
|      3 | 7715 | `	}else{` |
|      - | 7716 | `		const char *zInput,*zCur;` |
|      - | 7717 | `		char *zDup;` |
|      - | 7718 | `		int nLen;` |
|      - | 7719 | `		/* Extract the raw input */` |
|      3 | 7720 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 7721 | `		if( nLen < 1 ){` |
|      - | 7722 | `			/* Empty input,return FALSE */` |
|    ! 0 | 7723 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7724 | `			return PH7_OK;` |
|      - | 7725 | `		}` |
|      - | 7726 | `		/* Extract the mask */` |
|      3 | 7727 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 7728 | `		if( nMasklen < 1 ){` |
|      - | 7729 | `			/* Set a default mask */` |
|      - | 7730 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 7731 | `			zMask = TOK_MASK;` |
|    ! 0 | 7732 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 7733 | `#undef TOK_MASK` |
|    ! 0 | 7734 | `		}` |
|      - | 7735 | `		/* Extract a single token */` |
|      3 | 7736 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 7737 | `		if( rc != SXRET_OK ){` |
|      - | 7738 | `			/* Empty input */` |
|    ! 0 | 7739 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7740 | `			return PH7_OK;` |
|    ! 0 | 7741 | `		}else{` |
|      - | 7742 | `			/* Return the extracted token */` |
|      3 | 7743 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 7744 | `		}` |
|      - | 7745 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 7746 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 7747 | `		if( pAux ){` |
|      3 | 7748 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 7749 | `			if( nLen < 1 ){` |
|    ! 0 | 7750 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7751 | `				return PH7_OK;` |
|      - | 7752 | `			}` |
|      - | 7753 | `			/* Duplicate input */` |
|      3 | 7754 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 7755 | `			if( zDup  ){` |
|      3 | 7756 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 7757 | `				/* Register the aux data */` |
|      3 | 7758 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 7759 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 7760 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 7761 | `			}` |
|      1 | 7762 | `		}` |
|      - | 7763 | `	}` |
|      7 | 7764 | `	return PH7_OK;` |
|      4 | 7765 | `}` |
|      - | 7766 | `/*` |
|      - | 7767 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 7768 | ` *  Pad a string to a certain length with another string` |
|      - | 7769 | ` * Parameters` |
|      - | 7770 | ` *  $input` |
|      - | 7771 | ` *   The input string.` |
|      - | 7772 | ` * $pad_length` |
|      - | 7773 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 7774 | ` *   string, no padding takes place.` |
|      - | 7775 | ` * $pad_string` |
|      - | 7776 | ` *   Note:` |
|      - | 7777 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 7778 | ` *    divided by the pad_string's length.` |
|      - | 7779 | ` * $pad_type` |
|      - | 7780 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 7781 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 7782 | ` * Return` |
|      - | 7783 | ` *  The padded string.` |
|      - | 7784 | ` */` |
|     10 | 7785 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7786 | `{` |
|      - | 7787 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 7788 | `	const char *zIn,*zPad;` |
|     11 | 7789 | `	if( nArg < 2 ){` |
|      - | 7790 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7791 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7792 | `		return PH7_OK;` |
|      - | 7793 | `	}` |
|      - | 7794 | `	/* Extract the target string */` |
|     11 | 7795 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7796 | `	/* Padding length */` |
|     11 | 7797 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|     11 | 7798 | `	if( iPadlen > 0 ){` |
|      9 | 7799 | `		iPadlen -= iLen;` |
|      4 | 7800 | `	}` |
|     11 | 7801 | `	if( iPadlen < 1  ){` |
|      - | 7802 | `		/* Return the string verbatim */` |
|      5 | 7803 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      5 | 7804 | `		return PH7_OK;` |
|      - | 7805 | `	}` |
|      7 | 7806 | `	zPad = " "; /* Whitespace padding */` |
|      7 | 7807 | `	iStrpad = (int)sizeof(char);` |
|      7 | 7808 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      7 | 7809 | `	if( nArg > 2 ){` |
|      - | 7810 | `		/* Padding string */` |
|      7 | 7811 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      7 | 7812 | `		if( iStrpad < 1 ){` |
|      - | 7813 | `			/* An empty pad string throws a catchable ValueError in PHP 8` |
|      - | 7814 | `			 * (only reached once padding is actually required). */` |
|      3 | 7815 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7816 | `				"str_pad(): Argument #3 ($pad_string) must not be empty");` |
|      - | 7817 | `		}` |
|      5 | 7818 | `		if( nArg > 3 ){` |
|      - | 7819 | `			/* Padd type */` |
|      5 | 7820 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 7821 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 7822 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 7823 | `			}` |
|      2 | 7824 | `		}` |
|      2 | 7825 | `	}` |
|      5 | 7826 | `	iDiv = 1;` |
|      5 | 7827 | `	if( iType == 2 ){` |
|    ! 0 | 7828 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 7829 | `	}` |
|      - | 7830 | `	/* Perform the requested operation */` |
|      5 | 7831 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 7832 | `		jPad = iStrpad;` |
|      5 | 7833 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 7834 | `			/* Padding */` |
|      5 | 7835 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 7836 | `				break;` |
|      - | 7837 | `			}` |
|      3 | 7838 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7839 | `		}` |
|      3 | 7840 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 7841 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 7842 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 7843 | `				if( jPad > iStrpad ){` |
|    ! 0 | 7844 | `					jPad = iStrpad;` |
|    ! 0 | 7845 | `				}` |
|      3 | 7846 | `				if( jPad < 1){` |
|    ! 0 | 7847 | `					break;` |
|      - | 7848 | `				}` |
|      3 | 7849 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 7850 | `			}` |
|      1 | 7851 | `		}` |
|      1 | 7852 | `	}` |
|      5 | 7853 | `	if( iLen > 0 ){` |
|      - | 7854 | `		/* Append the input string */` |
|      5 | 7855 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7856 | `	}` |
|      5 | 7857 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 7858 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 7859 | `			/* Padding */` |
|      5 | 7860 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 7861 | `				break;` |
|      - | 7862 | `			}` |
|      3 | 7863 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7864 | `		}` |
|      5 | 7865 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 7866 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 7867 | `			if( jPad > iStrpad ){` |
|    ! 0 | 7868 | `				jPad = iStrpad;` |
|    ! 0 | 7869 | `			}` |
|      3 | 7870 | `			if( jPad < 1){` |
|    ! 0 | 7871 | `				break;` |
|      - | 7872 | `			}` |
|      3 | 7873 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 7874 | `		}` |
|      1 | 7875 | `	}` |
|      5 | 7876 | `	return PH7_OK;` |
|      6 | 7877 | `}` |
|      - | 7878 | `/*` |
|      - | 7879 | ` * String replacement private data.` |
|      - | 7880 | ` */` |
|      - | 7881 | `typedef struct str_replace_data str_replace_data;` |
|      - | 7882 | `struct str_replace_data` |
|      - | 7883 | `{` |
|      - | 7884 | `	/* Used by the str_replace family to collect the search/replace arguments. */` |
|      - | 7885 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 7886 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 7887 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 7888 | `};` |
|      - | 7889 | `/*` |
|      - | 7890 | ` * Remove a substring.` |
|      - | 7891 | ` */` |
|      - | 7892 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 7893 | `	for(;;){\` |
|      - | 7894 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 7895 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 7896 | `		++OFFT;\` |
|      - | 7897 | `	}\` |
|      - | 7898 | `}` |
|      - | 7899 | `/*` |
|      - | 7900 | ` * Shift right and insert algorithm.` |
|      - | 7901 | ` */` |
|      - | 7902 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 7903 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 7904 | `		for(;;){\` |
|      - | 7905 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 7906 | `			if(INLEN < 1 ) { break; }\` |
|      - | 7907 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 7908 | `			--INLEN; \` |
|      - | 7909 | `		}\` |
|      - | 7910 | `		for(;;){\` |
|      - | 7911 | `				if(ELEN < 1) { break; }\` |
|      - | 7912 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 7913 | `				OFFT++;\` |
|      - | 7914 | `				ENTRY++;\` |
|      - | 7915 | `				--ELEN;\` |
|      - | 7916 | `		}\` |
|      - | 7917 | `}` |
|      - | 7918 | `/*` |
|      - | 7919 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 7920 | ` * replacement string [i.e: zReplace].` |
|      - | 7921 | ` */` |
|     46 | 7922 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 7923 | `{` |
|     47 | 7924 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 7925 | `	sxu32 n,m;` |
|     47 | 7926 | `	n = SyBlobLength(pWorker);` |
|     47 | 7927 | `	m = nOfft;` |
|      - | 7928 | `	/* Delete the old entry */` |
|   6573 | 7929 | `	STRDEL(zInput,n,m,nLen);` |
|     47 | 7930 | `	SyBlobLength(pWorker) -= nLen;` |
|     47 | 7931 | `	if( nReplen > 0 ){` |
|     41 | 7932 | `		sxi32 iRep = nReplen;` |
|      - | 7933 | `		sxi32 rc;` |
|      - | 7934 | `		/*` |
|      - | 7935 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 7936 | `		 * string.` |
|      - | 7937 | `		 */` |
|     41 | 7938 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     41 | 7939 | `		if( rc != SXRET_OK ){` |
|      - | 7940 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 7941 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 7942 | `			return rc;` |
|      - | 7943 | `		}` |
|      - | 7944 | `		/* Perform the insertion now */` |
|     41 | 7945 | `		zInput = (char *)SyBlobData(pWorker);` |
|     41 | 7946 | `		n = SyBlobLength(pWorker);` |
|   6357 | 7947 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     41 | 7948 | `		SyBlobLength(pWorker) += nReplen;` |
|     20 | 7949 | `	}` |
|     47 | 7950 | `	return SXRET_OK;` |
|     24 | 7951 | `}` |
|      - | 7952 | `/*` |
|      - | 7953 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 7954 | ` * to collect search/replace string.` |
|      - | 7955 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 7956 | ` */` |
|     26 | 7957 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 7958 | `{` |
|     27 | 7959 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 7960 | `	SyString sWorker;` |
|      - | 7961 | `	const char *zIn;` |
|      - | 7962 | `	int nByte;` |
|      - | 7963 | `	/* Extract a string representation of the given argument */` |
|     27 | 7964 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 7965 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 7966 | `	if( nByte > 0 ){` |
|      - | 7967 | `		char *zDup;` |
|      - | 7968 | `		/* Duplicate the chunk */` |
|     25 | 7969 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 7970 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 7971 | `			);` |
|     25 | 7972 | `		if( zDup == 0 ){` |
|      - | 7973 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 7974 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 7975 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 7976 | `			return SXERR_MEM;` |
|      - | 7977 | `		}` |
|     25 | 7978 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 7979 | `		/* Save the chunk */` |
|     25 | 7980 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 7981 | `	}` |
|      - | 7982 | `	/* Save for later processing */` |
|     27 | 7983 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 7984 | `	/* All done */` |
|     13 | 7985 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 7986 | `	return PH7_OK;` |
|     14 | 7987 | `}` |
|      - | 7988 | `/*` |
|      - | 7989 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 7990 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 7991 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 7992 | ` * Parameters` |
|      - | 7993 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 7994 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 7995 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 7996 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 7997 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 7998 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 7999 | ` * $search` |
|      - | 8000 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 8001 | ` *  to designate multiple needles.` |
|      - | 8002 | ` * $replace` |
|      - | 8003 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 8004 | ` *  to designate multiple replacements.` |
|      - | 8005 | ` * $subject` |
|      - | 8006 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 8007 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 8008 | ` *  of subject, and the return value is an array as well.` |
|      - | 8009 | ` * $count (Not used)` |
|      - | 8010 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 8011 | ` * Return` |
|      - | 8012 | ` * This function returns a string or an array with the replaced values.` |
|      - | 8013 | ` */` |
|  29280 | 8014 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 8015 | `{` |
|      - | 8016 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 8017 | `	ProcStringMatch xMatch;` |
|      - | 8018 | `	const char *zIn,*zFunc;` |
|      - | 8019 | `	str_replace_data sRep;` |
|      - | 8020 | `	SyBlob sWorker;` |
|      - | 8021 | `	SySet sReplace;` |
|      - | 8022 | `	SySet sSearch;` |
|      - | 8023 | `	int rep_str;` |
|      - | 8024 | `	int nByte;` |
|      - | 8025 | `	sxi32 rc;` |
|  29285 | 8026 | `	if( nArg < 3 ){` |
|      - | 8027 | `		/* Missing/Invalid arguments,return null */` |
|    ! 0 | 8028 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8029 | `		return PH7_OK;` |
|      - | 8030 | `	}` |
|      - | 8031 | `	/* Initialize fields */` |
|  29285 | 8032 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  29285 | 8033 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  29285 | 8034 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  29285 | 8035 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  29285 | 8036 | `	sRep.pCtx = pCtx;` |
|  29285 | 8037 | `	sRep.pCollector = &sSearch;` |
|  29285 | 8038 | `	rep_str = 0;` |
|      - | 8039 | `	/* Extract the subject */` |
|  29285 | 8040 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  29285 | 8041 | `	if( nByte < 1 ){` |
|      - | 8042 | `		/* Nothing to replace,return the empty string */` |
|     29 | 8043 | `		ph7_result_string(pCtx,"",0);` |
|     29 | 8044 | `		return PH7_OK;` |
|      - | 8045 | `	}` |
|      - | 8046 | `	/* Copy the subject */` |
|  29257 | 8047 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 8048 | `	/* Search string */` |
|  29257 | 8049 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 8050 | `		/* Collect search string */` |
|      9 | 8051 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 8052 | `	}else{` |
|      - | 8053 | `		/* Single pattern */` |
|  29249 | 8054 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  29249 | 8055 | `		if( nByte < 1 ){` |
|      - | 8056 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 8057 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 8058 | `			return PH7_OK;` |
|      - | 8059 | `		}` |
|  29245 | 8060 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 8061 | `		/* Save for later processing */` |
|  29245 | 8062 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 8063 | `	}` |
|      - | 8064 | `	/* Replace string */` |
|  29253 | 8065 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 8066 | `		/* Collect replace string */` |
|      7 | 8067 | `		sRep.pCollector = &sReplace;` |
|      7 | 8068 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 8069 | `	}else{` |
|      - | 8070 | `		/* Single needle */` |
|  29247 | 8071 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  29247 | 8072 | `		rep_str = 1;` |
|  29247 | 8073 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 8074 | `		/* Save for later processing */` |
|  29247 | 8075 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 8076 | `	}` |
|      - | 8077 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  29253 | 8078 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 8079 | `		SySetRelease(&sSearch);` |
|    ! 0 | 8080 | `		SySetRelease(&sReplace);` |
|    ! 0 | 8081 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 8082 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8083 | `	}` |
|      - | 8084 | `	/* Reset loop cursors */` |
|  29253 | 8085 | `	SySetResetCursor(&sSearch);` |
|  29253 | 8086 | `	SySetResetCursor(&sReplace);` |
|  29253 | 8087 | `	pReplace = pSearch = 0; /* cc warning */` |
|  29253 | 8088 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 8089 | `	/* Extract function name */` |
|  29253 | 8090 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 8091 | `	/* Set the default pattern match routine */` |
|  29253 | 8092 | `	xMatch = SyBlobSearch;` |
|  29253 | 8093 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 8094 | `		/* Case insensitive pattern match */` |
|     11 | 8095 | `		xMatch = iPatternMatch;` |
|      5 | 8096 | `	}` |
|      - | 8097 | `	/* Start the replace process */` |
|  58509 | 8098 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 8099 | `		sxu32 nCount,nOfft;` |
|  29261 | 8100 | `		if( pSearch->nByte <  1 ){` |
|      - | 8101 | `			/* Empty string,ignore */` |
|      3 | 8102 | `			continue;` |
|      - | 8103 | `		}` |
|      - | 8104 | `		/* Extract the replace string */` |
|  29259 | 8105 | `		if( rep_str ){` |
|  29249 | 8106 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  14627 | 8107 | `		}else{` |
|     11 | 8108 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 8109 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 8110 | `				 * An empty string is used for the rest of replacement values` |
|      - | 8111 | `				 */` |
|      3 | 8112 | `				pReplace = 0;` |
|      1 | 8113 | `			}` |
|      - | 8114 | `		}` |
|  29259 | 8115 | `		if( pReplace == 0 ){` |
|      - | 8116 | `			/* Use an empty string instead */` |
|      3 | 8117 | `			pReplace = &sTemp;` |
|      1 | 8118 | `		}` |
|  29259 | 8119 | `		nOfft = nCount = 0;` |
|  14650 | 8120 | `		for(;;){` |
|  29305 | 8121 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 8122 | `				break;` |
|      - | 8123 | `			}` |
|      - | 8124 | `			/* Perform a pattern lookup */` |
|  43937 | 8125 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  29288 | 8126 | `				pSearch->nByte,&nOfft);` |
|  29293 | 8127 | `			if( rc != SXRET_OK ){` |
|      - | 8128 | `				/* Pattern not found */` |
|  29247 | 8129 | `				break;` |
|      - | 8130 | `			}` |
|      - | 8131 | `			/* Perform the replace operation */` |
|     47 | 8132 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     47 | 8133 | `			if( rc != SXRET_OK ){` |
|      - | 8134 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 8135 | `				SySetRelease(&sSearch);` |
|    ! 0 | 8136 | `				SySetRelease(&sReplace);` |
|    ! 0 | 8137 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8138 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8139 | `			}` |
|      - | 8140 | `			/* Increment offset counter */` |
|     47 | 8141 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 8142 | `		}` |
|      5 | 8143 | `	}` |
|      - | 8144 | `	/* All done,clean-up the mess left behind */` |
|  29253 | 8145 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  29253 | 8146 | `	SySetRelease(&sSearch);` |
|  29253 | 8147 | `	SySetRelease(&sReplace);` |
|  29253 | 8148 | `	SyBlobRelease(&sWorker);` |
|  29253 | 8149 | `	if( rc != PH7_OK ){` |
|    ! 0 | 8150 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8151 | `	}` |
|  29253 | 8152 | `	return PH7_OK;` |
|  14645 | 8153 | `}` |
|      - | 8154 | `/*` |
|      - | 8155 | ` * strtr() array form: a single (key,value) pair copied out of the replace_pairs` |
|      - | 8156 | ` * array. The bytes are owned by a persistent pool (see strtr_collect) rather than` |
|      - | 8157 | ` * the transient walker values, which HashmapWalk releases after each callback, so` |
|      - | 8158 | ` * we store byte offsets into that pool instead of raw pointers.` |
|      - | 8159 | ` */` |
|      - | 8160 | `typedef struct strtr_entry strtr_entry;` |
|      - | 8161 | `struct strtr_entry` |
|      - | 8162 | `{` |
|      - | 8163 | `	sxu32 nKeyOfft; /* Offset of the search key inside the pool */` |
|      - | 8164 | `	sxu32 nKeyLen;  /* Length of the search key */` |
|      - | 8165 | `	sxu32 nValOfft; /* Offset of the replacement inside the pool */` |
|      - | 8166 | `	sxu32 nValLen;  /* Length of the replacement */` |
|      - | 8167 | `};` |
|      - | 8168 | `typedef struct strtr_collect strtr_collect;` |
|      - | 8169 | `struct strtr_collect` |
|      - | 8170 | `{` |
|      - | 8171 | `	SyBlob *pPool;  /* Byte pool holding copied key + value bytes */` |
|      - | 8172 | `	SySet  *pTable; /* Set of strtr_entry (parallel offsets into pPool) */` |
|      - | 8173 | `	sxi32   rc;     /* Carries an allocation failure (SXERR_MEM) out of the walker */` |
|      - | 8174 | `};` |
|      - | 8175 | `/*` |
|      - | 8176 | ` * Collect one replace_pairs entry into the persistent pool/offset table.` |
|      - | 8177 | ` * PHP coerces both the key and the value to string (an integer key becomes its` |
|      - | 8178 | ` * decimal form) and ignores an empty-string key.` |
|      - | 8179 | ` */` |
|     20 | 8180 | `static int StrtrCollectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 8181 | `{` |
|     21 | 8182 | `	strtr_collect *pCol = (strtr_collect *)pUserData;` |
|      - | 8183 | `	const char *zKey,*zVal;` |
|      - | 8184 | `	strtr_entry sEnt;` |
|      - | 8185 | `	int nKey,nVal;` |
|     21 | 8186 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     21 | 8187 | `	if( nKey < 1 ){` |
|      - | 8188 | `		/* PHP ignores an empty-string key (it also emits a warning we do not replicate). */` |
|    ! 0 | 8189 | `		return PH7_OK;` |
|      - | 8190 | `	}` |
|     21 | 8191 | `	zVal = ph7_value_to_string(pData,&nVal);` |
|     21 | 8192 | `	sEnt.nKeyOfft = SyBlobLength(pCol->pPool);` |
|     21 | 8193 | `	sEnt.nKeyLen  = (sxu32)nKey;` |
|     21 | 8194 | `	if( SyBlobAppend(pCol->pPool,(const void *)zKey,(sxu32)nKey) != SXRET_OK ){` |
|    ! 0 | 8195 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8196 | `		return SXERR_ABORT;` |
|      - | 8197 | `	}` |
|     21 | 8198 | `	sEnt.nValOfft = SyBlobLength(pCol->pPool);` |
|     21 | 8199 | `	sEnt.nValLen  = (sxu32)nVal;` |
|     21 | 8200 | `	if( nVal > 0 && SyBlobAppend(pCol->pPool,(const void *)zVal,(sxu32)nVal) != SXRET_OK ){` |
|    ! 0 | 8201 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8202 | `		return SXERR_ABORT;` |
|      - | 8203 | `	}` |
|     21 | 8204 | `	if( SySetPut(pCol->pTable,(const void *)&sEnt) != SXRET_OK ){` |
|    ! 0 | 8205 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8206 | `		return SXERR_ABORT;` |
|      - | 8207 | `	}` |
|     21 | 8208 | `	return PH7_OK;` |
|     11 | 8209 | `}` |
|      - | 8210 | `/*` |
|      - | 8211 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 8212 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 8213 | ` *  Translate characters or replace substrings.` |
|      - | 8214 | ` * Parameters` |
|      - | 8215 | ` *  $str` |
|      - | 8216 | ` *  The string being translated.` |
|      - | 8217 | ` * $from` |
|      - | 8218 | ` *  The string being translated to to.` |
|      - | 8219 | ` * $to` |
|      - | 8220 | ` *  The string replacing from.` |
|      - | 8221 | ` * $replace_pairs` |
|      - | 8222 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 8223 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 8224 | ` * Return` |
|      - | 8225 | ` *  The translated string.` |
|      - | 8226 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 8227 | ` */` |
|     12 | 8228 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8229 | `{` |
|      - | 8230 | `	const char *zIn;` |
|      - | 8231 | `	int nLen;` |
|     13 | 8232 | `	if( nArg < 1 ){` |
|      - | 8233 | `		/* Nothing to replace,return FALSE */` |
|    ! 0 | 8234 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8235 | `		return PH7_OK;` |
|      - | 8236 | `	}` |
|     13 | 8237 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 8238 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 8239 | `		/* Invalid arguments */` |
|    ! 0 | 8240 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8241 | `		return PH7_OK;` |
|      - | 8242 | `	}` |
|     18 | 8243 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 8244 | `		strtr_collect sCol;` |
|      - | 8245 | `		SyBlob sPool,sWorker;` |
|      - | 8246 | `		SySet sTable;` |
|      - | 8247 | `		const char *zPool;` |
|      - | 8248 | `		strtr_entry *pEnt;` |
|      - | 8249 | `		sxi32 rc;` |
|      - | 8250 | `		int i,iRun;` |
|      - | 8251 | `		/*` |
|      - | 8252 | `		 * PHP's array-form strtr is a single left-to-right pass over the subject:` |
|      - | 8253 | `		 * at every position it substitutes the LONGEST replace_pairs key that` |
|      - | 8254 | `		 * matches there, then advances past the key (replacements are never` |
|      - | 8255 | `		 * rescanned). It is not a sequential per-key global replace. First copy` |
|      - | 8256 | `		 * the pairs into a persistent pool, then run that scan.` |
|      - | 8257 | `		 */` |
|     11 | 8258 | `		SyBlobInit(&sPool,&pCtx->pVm->sAllocator);` |
|     11 | 8259 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     11 | 8260 | `		SySetInit(&sTable,&pCtx->pVm->sAllocator,sizeof(strtr_entry));` |
|     11 | 8261 | `		sCol.pPool  = &sPool;` |
|     11 | 8262 | `		sCol.pTable = &sTable;` |
|     11 | 8263 | `		sCol.rc     = SXRET_OK;` |
|     11 | 8264 | `		ph7_array_walk(apArg[1],StrtrCollectWalker,&sCol);` |
|     11 | 8265 | `		if( sCol.rc != SXRET_OK ){` |
|      - | 8266 | `			/* Allocation failure while collecting the pairs: surface a fatal */` |
|    ! 0 | 8267 | `			SyBlobRelease(&sPool);` |
|    ! 0 | 8268 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 8269 | `			SySetRelease(&sTable);` |
|    ! 0 | 8270 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 8271 | `		}` |
|      - | 8272 | `		/* The pool is now stable, so offsets can be resolved against its base. */` |
|     11 | 8273 | `		zPool = (const char *)SyBlobData(&sPool);` |
|     11 | 8274 | `		rc = SXRET_OK;` |
|     11 | 8275 | `		iRun = 0; /* Start of the pending run of unmatched bytes copied verbatim. */` |
|     43 | 8276 | `		for( i = 0 ; i < nLen ; ){` |
|     33 | 8277 | `			strtr_entry *pBest = 0;` |
|     33 | 8278 | `			sxu32 nBest = 0;` |
|      - | 8279 | `			/* Pick the longest key that matches at the current position. */` |
|     33 | 8280 | `			SySetResetCursor(&sTable);` |
|     97 | 8281 | `			while( SXRET_OK == SySetGetNextEntry(&sTable,(void **)&pEnt) ){` |
|     64 | 8282 | `				if( pEnt->nKeyLen > nBest` |
|     60 | 8283 | `					&& pEnt->nKeyLen <= (sxu32)(nLen - i)` |
|     56 | 8284 | `					&& SyMemcmp(zPool + pEnt->nKeyOfft,zIn + i,pEnt->nKeyLen) == 0 ){` |
|     29 | 8285 | `					nBest = pEnt->nKeyLen;` |
|     29 | 8286 | `					pBest = pEnt;` |
|     14 | 8287 | `				}` |
|      1 | 8288 | `			}` |
|     33 | 8289 | `			if( pBest == 0 ){` |
|      - | 8290 | `				/* No key here: extend the literal run and copy it in one shot later. */` |
|      9 | 8291 | `				i++;` |
|      9 | 8292 | `				continue;` |
|      - | 8293 | `			}` |
|      - | 8294 | `			/* Flush the pending literal run, then the replacement. */` |
|     25 | 8295 | `			if( i > iRun ){` |
|      5 | 8296 | `				rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(i - iRun));` |
|      2 | 8297 | `			}` |
|     25 | 8298 | `			if( rc == SXRET_OK && pBest->nValLen > 0 ){` |
|     25 | 8299 | `				rc = SyBlobAppend(&sWorker,zPool + pBest->nValOfft,pBest->nValLen);` |
|     12 | 8300 | `			}` |
|     25 | 8301 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8302 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 8303 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8304 | `				SySetRelease(&sTable);` |
|    ! 0 | 8305 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8306 | `			}` |
|     25 | 8307 | `			i += (int)pBest->nKeyLen;` |
|     25 | 8308 | `			iRun = i;` |
|      1 | 8309 | `		}` |
|      - | 8310 | `		/* Flush the trailing literal run. */` |
|     11 | 8311 | `		if( nLen > iRun ){` |
|      3 | 8312 | `			rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(nLen - iRun));` |
|      3 | 8313 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8314 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 8315 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8316 | `				SySetRelease(&sTable);` |
|    ! 0 | 8317 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8318 | `			}` |
|      1 | 8319 | `		}` |
|      - | 8320 | `		/* All done, return the result string */` |
|     16 | 8321 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|     10 | 8322 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 8323 | `		/* Clean-up */` |
|     11 | 8324 | `		SyBlobRelease(&sPool);` |
|     11 | 8325 | `		SyBlobRelease(&sWorker);` |
|     11 | 8326 | `		SySetRelease(&sTable);` |
|     11 | 8327 | `		if( rc != PH7_OK ){` |
|    ! 0 | 8328 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 8329 | `		}` |
|      6 | 8330 | `	}else{` |
|      - | 8331 | `		int i,flen,tlen,c,iOfft;` |
|      - | 8332 | `		const char *zFrom,*zTo;` |
|      3 | 8333 | `		if( nArg < 3 ){` |
|      - | 8334 | `			/* Nothing to replace */` |
|    ! 0 | 8335 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8336 | `			return PH7_OK;` |
|      - | 8337 | `		}` |
|      - | 8338 | `		/* Extract given arguments */` |
|      3 | 8339 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 8340 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 8341 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 8342 | `			/* Nothing to replace */` |
|    ! 0 | 8343 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8344 | `			return PH7_OK;` |
|      - | 8345 | `		}` |
|      - | 8346 | `		/* Start the replace process */` |
|     13 | 8347 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 8348 | `			c = zIn[i];` |
|     11 | 8349 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 8350 | `				if ( iOfft < tlen ){` |
|      5 | 8351 | `					c = zTo[iOfft];` |
|      2 | 8352 | `				}` |
|      2 | 8353 | `			}` |
|     11 | 8354 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 8355 |  |
|      6 | 8356 | `		}` |
|      - | 8357 | `	}` |
|     13 | 8358 | `	return PH7_OK;` |
|      7 | 8359 | `}` |
|      - | 8360 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8361 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8362 | `/*` |
|      - | 8363 | ` * Parse an INI string.` |
|      - | 8364 |  |
|      - | 8365 | ` * According to wikipedia` |
|      - | 8366 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 8367 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 8368 | ` *  Format` |
|      - | 8369 | `*    Properties` |
|      - | 8370 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 8371 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 8372 | `*     Example:` |
|      - | 8373 | `*      name=value` |
|      - | 8374 | `*    Sections` |
|      - | 8375 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 8376 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 8377 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 8378 | `*     or the end of the file. Sections may not be nested.` |
|      - | 8379 | `*     Example:` |
|      - | 8380 | `*      [section]` |
|      - | 8381 | `*   Comments` |
|      - | 8382 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 8383 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 8384 | `*/` |
|     12 | 8385 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 8386 | `{` |
|      - | 8387 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 8388 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 8389 | `	SyHashEntry *pEntry;` |
|      - | 8390 | `	SyString sEntry;` |
|      - | 8391 | `	SyHash sHash;` |
|      - | 8392 | `	int c;` |
|      - | 8393 | `	/* Create an empty array and worker variables */` |
|     13 | 8394 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 8395 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 8396 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 8397 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 8398 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|    ! 0 | 8399 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8400 | `	}` |
|     13 | 8401 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 8402 | `	pCur = pArray;` |
|      - | 8403 | `	/* Start the parse process */` |
|     21 | 8404 | `	for(;;){` |
|      - | 8405 | `		/* Ignore leading white spaces */` |
|     69 | 8406 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 8407 | `			zIn++;` |
|      1 | 8408 | `		}` |
|     43 | 8409 | `		if( zIn >= zEnd ){` |
|      - | 8410 | `			/* No more input to process */` |
|     13 | 8411 | `			break;` |
|      - | 8412 | `		}` |
|     31 | 8413 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 8414 | `			/* Comment til the end of line */` |
|    ! 0 | 8415 | `			zIn++;` |
|    ! 0 | 8416 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 8417 | `				zIn++;` |
|    ! 0 | 8418 | `			}` |
|    ! 0 | 8419 | `			continue;` |
|      - | 8420 | `		}` |
|      - | 8421 | `		/* Reset the string cursor of the working variable */` |
|     31 | 8422 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 8423 | `		if( zIn[0] == '[' ){` |
|      - | 8424 | `			/* Section: Extract the section name */` |
|      9 | 8425 | `			zIn++;` |
|      9 | 8426 | `			zCur = zIn;` |
|     73 | 8427 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 8428 | `				zIn++;` |
|      1 | 8429 | `			}` |
|      9 | 8430 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 8431 | `				/* Save the section name */` |
|      5 | 8432 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 8433 | `				SyStringFullTrim(&sEntry);` |
|      5 | 8434 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 8435 | `				if( sEntry.nByte > 0 ){` |
|      - | 8436 | `					/* Associate an array with the section */` |
|      5 | 8437 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 8438 | `					if( pSection ){` |
|      5 | 8439 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 8440 | `						pCur = pSection;` |
|      2 | 8441 | `					}` |
|      2 | 8442 | `				}` |
|      2 | 8443 | `			}` |
|      9 | 8444 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 8445 | `		}else{` |
|      - | 8446 | `			ph7_value *pOldCur;` |
|      - | 8447 | `			int is_array;` |
|      - | 8448 | `			int iLen;` |
|      - | 8449 | `			/* Properties */` |
|     23 | 8450 | `			is_array = 0;` |
|     23 | 8451 | `			zCur = zIn;` |
|     23 | 8452 | `			iLen = 0; /* cc warning */` |
|     23 | 8453 | `			pOldCur = pCur;` |
|    155 | 8454 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 8455 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 8456 | `					/* Array */` |
|    ! 0 | 8457 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 8458 | `					is_array = 1;` |
|    ! 0 | 8459 | `					if( iLen > 0 ){` |
|    ! 0 | 8460 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 8461 | `						/* Query the hashtable */` |
|    ! 0 | 8462 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 8463 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 8464 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 8465 | `						if( pEntry ){` |
|    ! 0 | 8466 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 8467 | `						}else{` |
|      - | 8468 | `							/* Create an empty array */` |
|    ! 0 | 8469 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 8470 | `							if( pvArr ){` |
|      - | 8471 | `								/* Save the entry */` |
|    ! 0 | 8472 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 8473 | `								/* Insert the entry */` |
|    ! 0 | 8474 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 8475 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 8476 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 8477 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 8478 | `							}` |
|      - | 8479 | `						}` |
|    ! 0 | 8480 | `						if( pvArr ){` |
|    ! 0 | 8481 | `							pCur = pvArr;` |
|    ! 0 | 8482 | `						}` |
|    ! 0 | 8483 | `					}` |
|    ! 0 | 8484 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 8485 | `						zIn++;` |
|    ! 0 | 8486 | `					}` |
|    ! 0 | 8487 | `				}` |
|    133 | 8488 | `				zIn++;` |
|      1 | 8489 | `			}` |
|     23 | 8490 | `			if( !is_array ){` |
|     23 | 8491 | `				iLen = (int)(zIn-zCur);` |
|     11 | 8492 | `			}` |
|      - | 8493 | `			/* Trim the key */` |
|     23 | 8494 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 8495 | `			SyStringFullTrim(&sEntry);` |
|     23 | 8496 | `			if( sEntry.nByte > 0 ){` |
|     23 | 8497 | `				if( !is_array ){` |
|      - | 8498 | `					/* Save the key name */` |
|     23 | 8499 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 8500 | `				}` |
|      - | 8501 | `				/* extract key value */` |
|     23 | 8502 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 8503 | `				zIn++; /* '=' */` |
|     39 | 8504 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 8505 | `					zIn++;` |
|      1 | 8506 | `				}` |
|     23 | 8507 | `				if( zIn < zEnd ){` |
|     21 | 8508 | `					zCur = zIn;` |
|     21 | 8509 | `					c = zIn[0];` |
|     21 | 8510 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 8511 | `						zIn++;` |
|      - | 8512 | `						/* Delimit the value */` |
|    ! 0 | 8513 | `						while( zIn < zEnd ){` |
|    ! 0 | 8514 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 8515 | `								break;` |
|      - | 8516 | `							}` |
|    ! 0 | 8517 | `							zIn++;` |
|    ! 0 | 8518 | `						}` |
|    ! 0 | 8519 | `						if( zIn < zEnd ){` |
|    ! 0 | 8520 | `							zIn++;` |
|    ! 0 | 8521 | `						}` |
|    ! 0 | 8522 | `					}else{` |
|    125 | 8523 | `						while( zIn < zEnd ){` |
|    123 | 8524 | `							if( zIn[0] == '\n' ){` |
|     19 | 8525 | `								if( zIn[-1] != '\\' ){` |
|     19 | 8526 | `									break;` |
|    ! 0 | 8527 | `								}` |
|    105 | 8528 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 8529 | `								/* Inline comments */` |
|    ! 0 | 8530 | `								break;` |
|      - | 8531 | `							}` |
|    105 | 8532 | `							zIn++;` |
|      1 | 8533 | `						}` |
|      - | 8534 | `					}` |
|      - | 8535 | `					/* Trim the value */` |
|     21 | 8536 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 8537 | `					SyStringFullTrim(&sEntry);` |
|     21 | 8538 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 8539 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 8540 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 8541 | `					}` |
|     21 | 8542 | `					if( sEntry.nByte > 0 ){` |
|     21 | 8543 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 8544 | `					}` |
|      - | 8545 | `					/* Insert the key and it's value */` |
|     21 | 8546 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 8547 | `				}` |
|     12 | 8548 | `			}else{` |
|    ! 0 | 8549 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 8550 | `					zIn++;` |
|    ! 0 | 8551 | `				}` |
|      - | 8552 | `			}` |
|     23 | 8553 | `			pCur = pOldCur;` |
|      - | 8554 | `		}` |
|      1 | 8555 | `	}` |
|     13 | 8556 | `	SyHashRelease(&sHash);` |
|      - | 8557 | `	/* Return the parse of the INI string */` |
|     13 | 8558 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 8559 | `	return SXRET_OK;` |
|      7 | 8560 | `}` |
|      - | 8561 | `/*` |
|      - | 8562 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 8563 | ` *  Parse a configuration string.` |
|      - | 8564 | ` * Parameters` |
|      - | 8565 | ` *  $ini` |
|      - | 8566 | ` *   The contents of the ini file being parsed.` |
|      - | 8567 | ` *  $process_sections` |
|      - | 8568 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 8569 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 8570 | ` *  $scanner_mode (Not used)` |
|      - | 8571 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 8572 | ` *   then option values will not be parsed.` |
|      - | 8573 | ` * Return` |
|      - | 8574 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 8575 | ` */` |
|     10 | 8576 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8577 | `{` |
|      - | 8578 | `	const char *zIni;` |
|      - | 8579 | `	int nByte;` |
|     11 | 8580 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8581 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 8582 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8583 | `		return PH7_OK;` |
|      - | 8584 | `	}` |
|      - | 8585 | `	/* Extract the raw INI buffer */` |
|     11 | 8586 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 8587 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|     11 | 8588 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      6 | 8589 | `}` |
|      - | 8590 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8591 |  |
|      - | 8592 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8593 |  |
|      - | 8594 | `/*` |
|      - | 8595 | ` * Ctype Functions.` |
|      - | 8596 | ` * Status:` |
|      - | 8597 | ` *    Stable.` |
|      - | 8598 | ` */` |
|      - | 8599 | `/*` |
|      - | 8600 | ` * bool ctype_alnum(string $text)` |
|      - | 8601 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 8602 | ` * Parameters` |
|      - | 8603 | ` *  $text` |
|      - | 8604 | ` *   The tested string.` |
|      - | 8605 | ` * Return` |
|      - | 8606 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 8607 | ` */` |
|     14 | 8608 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8609 | `{` |
|      - | 8610 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8611 | `	int nLen;` |
|     15 | 8612 | `	if( nArg < 1 ){` |
|      - | 8613 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8614 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8615 | `		return PH7_OK;` |
|      - | 8616 | `	}` |
|      - | 8617 | `	/* Extract the target string */` |
|     15 | 8618 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 8619 | `	zEnd = &zIn[nLen];` |
|     15 | 8620 | `	if( nLen < 1 ){` |
|      - | 8621 | `		/* Empty string,return FALSE */` |
|      3 | 8622 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8623 | `		return PH7_OK;` |
|      - | 8624 | `	}` |
|      - | 8625 | `	/* Perform the requested operation */` |
|     32 | 8626 | `	for(;;){` |
|     65 | 8627 | `		if( zIn >= zEnd ){` |
|      - | 8628 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8629 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8630 | `			return PH7_OK;` |
|      - | 8631 | `		}` |
|     57 | 8632 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 8633 | `			break;` |
|      - | 8634 | `		}` |
|      - | 8635 | `		/* Point to the next character */` |
|     53 | 8636 | `		zIn++;` |
|      1 | 8637 | `	}` |
|      - | 8638 | `	/* The test failed,return FALSE */` |
|      5 | 8639 | `	ph7_result_bool(pCtx,0);` |
|      5 | 8640 | `	return PH7_OK;` |
|      8 | 8641 | `}` |
|      - | 8642 | `/*` |
|      - | 8643 | ` * bool ctype_alpha(string $text)` |
|      - | 8644 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 8645 | ` * Parameters` |
|      - | 8646 | ` *  $text` |
|      - | 8647 | ` *   The tested string.` |
|      - | 8648 | ` * Return` |
|      - | 8649 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 8650 | ` */` |
|     16 | 8651 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8652 | `{` |
|      - | 8653 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8654 | `	int nLen;` |
|     17 | 8655 | `	if( nArg < 1 ){` |
|      - | 8656 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8657 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8658 | `		return PH7_OK;` |
|      - | 8659 | `	}` |
|      - | 8660 | `	/* Extract the target string */` |
|     17 | 8661 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8662 | `	zEnd = &zIn[nLen];` |
|     17 | 8663 | `	if( nLen < 1 ){` |
|      - | 8664 | `		/* Empty string,return FALSE */` |
|      3 | 8665 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8666 | `		return PH7_OK;` |
|      - | 8667 | `	}` |
|      - | 8668 | `	/* Perform the requested operation */` |
|     42 | 8669 | `	for(;;){` |
|     85 | 8670 | `		if( zIn >= zEnd ){` |
|      - | 8671 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8672 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8673 | `			return PH7_OK;` |
|      - | 8674 | `		}` |
|     77 | 8675 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 8676 | `			break;` |
|      - | 8677 | `		}` |
|      - | 8678 | `		/* Point to the next character */` |
|     71 | 8679 | `		zIn++;` |
|      1 | 8680 | `	}` |
|      - | 8681 | `	/* The test failed,return FALSE */` |
|      7 | 8682 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8683 | `	return PH7_OK;` |
|      9 | 8684 | `}` |
|      - | 8685 | `/*` |
|      - | 8686 | ` * bool ctype_cntrl(string $text)` |
|      - | 8687 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 8688 | ` * Parameters` |
|      - | 8689 | ` *  $text` |
|      - | 8690 | ` *   The tested string.` |
|      - | 8691 | ` * Return` |
|      - | 8692 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 8693 | ` */` |
|     16 | 8694 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8695 | `{` |
|      - | 8696 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8697 | `	int nLen;` |
|     17 | 8698 | `	if( nArg < 1 ){` |
|      - | 8699 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8700 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8701 | `		return PH7_OK;` |
|      - | 8702 | `	}` |
|      - | 8703 | `	/* Extract the target string */` |
|     17 | 8704 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8705 | `	zEnd = &zIn[nLen];` |
|     17 | 8706 | `	if( nLen < 1 ){` |
|      - | 8707 | `		/* Empty string,return FALSE */` |
|      3 | 8708 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8709 | `		return PH7_OK;` |
|      - | 8710 | `	}` |
|      - | 8711 | `	/* Perform the requested operation */` |
|     14 | 8712 | `	for(;;){` |
|     29 | 8713 | `		if( zIn >= zEnd ){` |
|      - | 8714 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8715 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8716 | `			return PH7_OK;` |
|      - | 8717 | `		}` |
|     21 | 8718 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8719 | `			/* UTF-8 stream  */` |
|    ! 0 | 8720 | `			break;` |
|      - | 8721 | `		}` |
|     21 | 8722 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 8723 | `			break;` |
|      - | 8724 | `		}` |
|      - | 8725 | `		/* Point to the next character */` |
|     15 | 8726 | `		zIn++;` |
|      1 | 8727 | `	}` |
|      - | 8728 | `	/* The test failed,return FALSE */` |
|      7 | 8729 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8730 | `	return PH7_OK;` |
|      9 | 8731 | `}` |
|      - | 8732 | `/*` |
|      - | 8733 | ` * bool ctype_digit(string $text)` |
|      - | 8734 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 8735 | ` * Parameters` |
|      - | 8736 | ` *  $text` |
|      - | 8737 | ` *   The tested string.` |
|      - | 8738 | ` * Return` |
|      - | 8739 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 8740 | ` */` |
|   1614 | 8741 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 8742 | `{` |
|      - | 8743 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8744 | `	int nLen;` |
|   1619 | 8745 | `	if( nArg < 1 ){` |
|      - | 8746 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8747 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8748 | `		return PH7_OK;` |
|      - | 8749 | `	}` |
|      - | 8750 | `	/* Extract the target string */` |
|   1619 | 8751 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1619 | 8752 | `	zEnd = &zIn[nLen];` |
|   1619 | 8753 | `	if( nLen < 1 ){` |
|      - | 8754 | `		/* Empty string,return FALSE */` |
|      3 | 8755 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8756 | `		return PH7_OK;` |
|      - | 8757 | `	}` |
|      - | 8758 | `	/* Perform the requested operation */` |
|   1515 | 8759 | `	for(;;){` |
|   3035 | 8760 | `		if( zIn >= zEnd ){` |
|      - | 8761 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1373 | 8762 | `			ph7_result_bool(pCtx,1);` |
|   1373 | 8763 | `			return PH7_OK;` |
|      - | 8764 | `		}` |
|   1667 | 8765 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8766 | `			/* UTF-8 stream  */` |
|    ! 0 | 8767 | `			break;` |
|      - | 8768 | `		}` |
|   1667 | 8769 | `		if( !SyisDigit(zIn[0]) ){` |
|    249 | 8770 | `			break;` |
|      - | 8771 | `		}` |
|      - | 8772 | `		/* Point to the next character */` |
|   1423 | 8773 | `		zIn++;` |
|      5 | 8774 | `	}` |
|      - | 8775 | `	/* The test failed,return FALSE */` |
|    249 | 8776 | `	ph7_result_bool(pCtx,0);` |
|    249 | 8777 | `	return PH7_OK;` |
|    812 | 8778 | `}` |
|      - | 8779 | `/*` |
|      - | 8780 | ` * bool ctype_xdigit(string $text)` |
|      - | 8781 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 8782 | ` * Parameters` |
|      - | 8783 | ` *  $text` |
|      - | 8784 | ` *   The tested string.` |
|      - | 8785 | ` * Return` |
|      - | 8786 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 8787 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 8788 | ` */` |
|     18 | 8789 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8790 | `{` |
|      - | 8791 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8792 | `	int nLen;` |
|     19 | 8793 | `	if( nArg < 1 ){` |
|      - | 8794 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8795 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8796 | `		return PH7_OK;` |
|      - | 8797 | `	}` |
|      - | 8798 | `	/* Extract the target string */` |
|     19 | 8799 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 8800 | `	zEnd = &zIn[nLen];` |
|     19 | 8801 | `	if( nLen < 1 ){` |
|      - | 8802 | `		/* Empty string,return FALSE */` |
|      3 | 8803 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8804 | `		return PH7_OK;` |
|      - | 8805 | `	}` |
|      - | 8806 | `	/* Perform the requested operation */` |
|     46 | 8807 | `	for(;;){` |
|     93 | 8808 | `		if( zIn >= zEnd ){` |
|      - | 8809 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 8810 | `			ph7_result_bool(pCtx,1);` |
|     11 | 8811 | `			return PH7_OK;` |
|      - | 8812 | `		}` |
|     83 | 8813 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8814 | `			/* UTF-8 stream  */` |
|    ! 0 | 8815 | `			break;` |
|      - | 8816 | `		}` |
|     83 | 8817 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 8818 | `			break;` |
|      - | 8819 | `		}` |
|      - | 8820 | `		/* Point to the next character */` |
|     77 | 8821 | `		zIn++;` |
|      1 | 8822 | `	}` |
|      - | 8823 | `	/* The test failed,return FALSE */` |
|      7 | 8824 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8825 | `	return PH7_OK;` |
|     10 | 8826 | `}` |
|      - | 8827 | `/*` |
|      - | 8828 | ` * bool ctype_graph(string $text)` |
|      - | 8829 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 8830 | ` * Parameters` |
|      - | 8831 | ` *  $text` |
|      - | 8832 | ` *   The tested string.` |
|      - | 8833 | ` * Return` |
|      - | 8834 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 8835 | ` * (no white space), FALSE otherwise.` |
|      - | 8836 | ` */` |
|     16 | 8837 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8838 | `{` |
|      - | 8839 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8840 | `	int nLen;` |
|     17 | 8841 | `	if( nArg < 1 ){` |
|      - | 8842 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8843 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8844 | `		return PH7_OK;` |
|      - | 8845 | `	}` |
|      - | 8846 | `	/* Extract the target string */` |
|     17 | 8847 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8848 | `	zEnd = &zIn[nLen];` |
|     17 | 8849 | `	if( nLen < 1 ){` |
|      - | 8850 | `		/* Empty string,return FALSE */` |
|      3 | 8851 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8852 | `		return PH7_OK;` |
|      - | 8853 | `	}` |
|      - | 8854 | `	/* Perform the requested operation */` |
|     57 | 8855 | `	for(;;){` |
|    115 | 8856 | `		if( zIn >= zEnd ){` |
|      - | 8857 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8858 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8859 | `			return PH7_OK;` |
|      - | 8860 | `		}` |
|    107 | 8861 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8862 | `			/* UTF-8 stream  */` |
|    ! 0 | 8863 | `			break;` |
|      - | 8864 | `		}` |
|    107 | 8865 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 8866 | `			break;` |
|      - | 8867 | `		}` |
|      - | 8868 | `		/* Point to the next character */` |
|    101 | 8869 | `		zIn++;` |
|      1 | 8870 | `	}` |
|      - | 8871 | `	/* The test failed,return FALSE */` |
|      7 | 8872 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8873 | `	return PH7_OK;` |
|      9 | 8874 | `}` |
|      - | 8875 | `/*` |
|      - | 8876 | ` * bool ctype_print(string $text)` |
|      - | 8877 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 8878 | ` * Parameters` |
|      - | 8879 | ` *  $text` |
|      - | 8880 | ` *   The tested string.` |
|      - | 8881 | ` * Return` |
|      - | 8882 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 8883 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 8884 | ` *  or control function at all.` |
|      - | 8885 | ` */` |
|     16 | 8886 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8887 | `{` |
|      - | 8888 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8889 | `	int nLen;` |
|     17 | 8890 | `	if( nArg < 1 ){` |
|      - | 8891 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8892 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8893 | `		return PH7_OK;` |
|      - | 8894 | `	}` |
|      - | 8895 | `	/* Extract the target string */` |
|     17 | 8896 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8897 | `	zEnd = &zIn[nLen];` |
|     17 | 8898 | `	if( nLen < 1 ){` |
|      - | 8899 | `		/* Empty string,return FALSE */` |
|      3 | 8900 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8901 | `		return PH7_OK;` |
|      - | 8902 | `	}` |
|      - | 8903 | `	/* Perform the requested operation */` |
|     63 | 8904 | `	for(;;){` |
|    127 | 8905 | `		if( zIn >= zEnd ){` |
|      - | 8906 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8907 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8908 | `			return PH7_OK;` |
|      - | 8909 | `		}` |
|    119 | 8910 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8911 | `			/* UTF-8 stream  */` |
|    ! 0 | 8912 | `			break;` |
|      - | 8913 | `		}` |
|    119 | 8914 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 8915 | `			break;` |
|      - | 8916 | `		}` |
|      - | 8917 | `		/* Point to the next character */` |
|    113 | 8918 | `		zIn++;` |
|      1 | 8919 | `	}` |
|      - | 8920 | `	/* The test failed,return FALSE */` |
|      7 | 8921 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8922 | `	return PH7_OK;` |
|      9 | 8923 | `}` |
|      - | 8924 | `/*` |
|      - | 8925 | ` * bool ctype_punct(string $text)` |
|      - | 8926 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 8927 | ` * Parameters` |
|      - | 8928 | ` *  $text` |
|      - | 8929 | ` *   The tested string.` |
|      - | 8930 | ` * Return` |
|      - | 8931 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 8932 | ` *  digit or blank, FALSE otherwise.` |
|      - | 8933 | ` */` |
|     18 | 8934 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8935 | `{` |
|      - | 8936 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8937 | `	int nLen;` |
|     19 | 8938 | `	if( nArg < 1 ){` |
|      - | 8939 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8940 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8941 | `		return PH7_OK;` |
|      - | 8942 | `	}` |
|      - | 8943 | `	/* Extract the target string */` |
|     19 | 8944 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 8945 | `	zEnd = &zIn[nLen];` |
|     19 | 8946 | `	if( nLen < 1 ){` |
|      - | 8947 | `		/* Empty string,return FALSE */` |
|      3 | 8948 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8949 | `		return PH7_OK;` |
|      - | 8950 | `	}` |
|      - | 8951 | `	/* Perform the requested operation */` |
|     38 | 8952 | `	for(;;){` |
|     77 | 8953 | `		if( zIn >= zEnd ){` |
|      - | 8954 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8955 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8956 | `			return PH7_OK;` |
|      - | 8957 | `		}` |
|     69 | 8958 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8959 | `			/* UTF-8 stream  */` |
|    ! 0 | 8960 | `			break;` |
|      - | 8961 | `		}` |
|     69 | 8962 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 8963 | `			break;` |
|      - | 8964 | `		}` |
|      - | 8965 | `		/* Point to the next character */` |
|     61 | 8966 | `		zIn++;` |
|      1 | 8967 | `	}` |
|      - | 8968 | `	/* The test failed,return FALSE */` |
|      9 | 8969 | `	ph7_result_bool(pCtx,0);` |
|      9 | 8970 | `	return PH7_OK;` |
|     10 | 8971 | `}` |
|      - | 8972 | `/*` |
|      - | 8973 | ` * bool ctype_space(string $text)` |
|      - | 8974 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 8975 | ` * Parameters` |
|      - | 8976 | ` *  $text` |
|      - | 8977 | ` *   The tested string.` |
|      - | 8978 | ` * Return` |
|      - | 8979 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 8980 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 8981 | ` *  and form feed characters.` |
|      - | 8982 | ` */` |
|  61965 | 8983 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 8984 | `{` |
|      - | 8985 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8986 | `	int nLen;` |
|  61970 | 8987 | `	if( nArg < 1 ){` |
|      - | 8988 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8989 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8990 | `		return PH7_OK;` |
|      - | 8991 | `	}` |
|      - | 8992 | `	/* Extract the target string */` |
|  61970 | 8993 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  61970 | 8994 | `	zEnd = &zIn[nLen];` |
|  61970 | 8995 | `	if( nLen < 1 ){` |
|      - | 8996 | `		/* Empty string,return FALSE */` |
|      3 | 8997 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8998 | `		return PH7_OK;` |
|      - | 8999 | `	}` |
|      - | 9000 | `	/* Perform the requested operation */` |
|  32087 | 9001 | `	for(;;){` |
|  64094 | 9002 | `		if( zIn >= zEnd ){` |
|      - | 9003 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   2107 | 9004 | `			ph7_result_bool(pCtx,1);` |
|   2107 | 9005 | `			return PH7_OK;` |
|      - | 9006 | `		}` |
|  61992 | 9007 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 9008 | `			/* UTF-8 stream  */` |
|    ! 0 | 9009 | `			break;` |
|      - | 9010 | `		}` |
|  61992 | 9011 | `		if( !SyisSpace(zIn[0]) ){` |
|  59866 | 9012 | `			break;` |
|      - | 9013 | `		}` |
|      - | 9014 | `		/* Point to the next character */` |
|   2131 | 9015 | `		zIn++;` |
|      5 | 9016 | `	}` |
|      - | 9017 | `	/* The test failed,return FALSE */` |
|  59866 | 9018 | `	ph7_result_bool(pCtx,0);` |
|  59866 | 9019 | `	return PH7_OK;` |
|  31030 | 9020 | `}` |
|      - | 9021 | `/*` |
|      - | 9022 | ` * bool ctype_lower(string $text)` |
|      - | 9023 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 9024 | ` * Parameters` |
|      - | 9025 | ` *  $text` |
|      - | 9026 | ` *   The tested string.` |
|      - | 9027 | ` * Return` |
|      - | 9028 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 9029 | ` */` |
|     16 | 9030 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9031 | `{` |
|      - | 9032 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9033 | `	int nLen;` |
|     17 | 9034 | `	if( nArg < 1 ){` |
|      - | 9035 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9036 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9037 | `		return PH7_OK;` |
|      - | 9038 | `	}` |
|      - | 9039 | `	/* Extract the target string */` |
|     17 | 9040 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9041 | `	zEnd = &zIn[nLen];` |
|     17 | 9042 | `	if( nLen < 1 ){` |
|      - | 9043 | `		/* Empty string,return FALSE */` |
|      3 | 9044 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9045 | `		return PH7_OK;` |
|      - | 9046 | `	}` |
|      - | 9047 | `	/* Perform the requested operation */` |
|     27 | 9048 | `	for(;;){` |
|     55 | 9049 | `		if( zIn >= zEnd ){` |
|      - | 9050 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 9051 | `			ph7_result_bool(pCtx,1);` |
|      5 | 9052 | `			return PH7_OK;` |
|      - | 9053 | `		}` |
|     51 | 9054 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 9055 | `			break;` |
|      - | 9056 | `		}` |
|      - | 9057 | `		/* Point to the next character */` |
|     41 | 9058 | `		zIn++;` |
|      1 | 9059 | `	}` |
|      - | 9060 | `	/* The test failed,return FALSE */` |
|     11 | 9061 | `	ph7_result_bool(pCtx,0);` |
|     11 | 9062 | `	return PH7_OK;` |
|      9 | 9063 | `}` |
|      - | 9064 | `/*` |
|      - | 9065 | ` * bool ctype_upper(string $text)` |
|      - | 9066 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 9067 | ` * Parameters` |
|      - | 9068 | ` *  $text` |
|      - | 9069 | ` *   The tested string.` |
|      - | 9070 | ` * Return` |
|      - | 9071 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 9072 | ` */` |
|     16 | 9073 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9074 | `{` |
|      - | 9075 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9076 | `	int nLen;` |
|     17 | 9077 | `	if( nArg < 1 ){` |
|      - | 9078 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9079 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9080 | `		return PH7_OK;` |
|      - | 9081 | `	}` |
|      - | 9082 | `	/* Extract the target string */` |
|     17 | 9083 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9084 | `	zEnd = &zIn[nLen];` |
|     17 | 9085 | `	if( nLen < 1 ){` |
|      - | 9086 | `		/* Empty string,return FALSE */` |
|      3 | 9087 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9088 | `		return PH7_OK;` |
|      - | 9089 | `	}` |
|      - | 9090 | `	/* Perform the requested operation */` |
|     28 | 9091 | `	for(;;){` |
|     57 | 9092 | `		if( zIn >= zEnd ){` |
|      - | 9093 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 9094 | `			ph7_result_bool(pCtx,1);` |
|      5 | 9095 | `			return PH7_OK;` |
|      - | 9096 | `		}` |
|     53 | 9097 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 9098 | `			break;` |
|      - | 9099 | `		}` |
|      - | 9100 | `		/* Point to the next character */` |
|     43 | 9101 | `		zIn++;` |
|      1 | 9102 | `	}` |
|      - | 9103 | `	/* The test failed,return FALSE */` |
|     11 | 9104 | `	ph7_result_bool(pCtx,0);` |
|     11 | 9105 | `	return PH7_OK;` |
|      9 | 9106 | `}` |
|      - | 9107 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 9108 | `/*` |
|      - | 9109 | ` * Section:` |
|      - | 9110 | ` *    URL handling Functions.` |
|      - | 9111 | ` * Status:` |
|      - | 9112 | ` *    Stable.` |
|      - | 9113 | ` */` |
|      - | 9114 | `/*` |
|      - | 9115 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 9116 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 9117 | ` */` |
|   1026 | 9118 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 9119 | `{` |
|      - | 9120 | `	/* Store in the call context result buffer */` |
|   1028 | 9121 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 9122 | `	return SXRET_OK;` |
|      2 | 9123 | `}` |
|      - | 9124 | `/*` |
|      - | 9125 | ` * string base64_encode(string $data)` |
|      - | 9126 | ` * string convert_uuencode(string $data)` |
|      - | 9127 | ` *  Encodes data with MIME base64` |
|      - | 9128 | ` * Parameter` |
|      - | 9129 | ` *  $data` |
|      - | 9130 | ` *    Data to encode` |
|      - | 9131 | ` * Return` |
|      - | 9132 | ` *  Encoded data or FALSE on failure.` |
|      - | 9133 | ` */` |
|      6 | 9134 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9135 | `{` |
|      - | 9136 | `	const char *zIn;` |
|      - | 9137 | `	int nLen;` |
|      7 | 9138 | `	if( nArg < 1 ){` |
|      - | 9139 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9140 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9141 | `		return PH7_OK;` |
|      - | 9142 | `	}` |
|      - | 9143 | `	/* Extract the input string */` |
|      7 | 9144 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 9145 | `	if( nLen < 1 ){` |
|      - | 9146 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9147 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9148 | `		return PH7_OK;` |
|      - | 9149 | `	}` |
|      - | 9150 | `	/* Perform the BASE64 encoding */` |
|      7 | 9151 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 9152 | `	return PH7_OK;` |
|      4 | 9153 | `}` |
|      - | 9154 | `/*` |
|      - | 9155 | ` * string base64_decode(string $data)` |
|      - | 9156 | ` * string convert_uudecode(string $data)` |
|      - | 9157 | ` *  Decodes data encoded with MIME base64` |
|      - | 9158 | ` * Parameter` |
|      - | 9159 | ` *  $data` |
|      - | 9160 | ` *    Encoded data.` |
|      - | 9161 | ` * Return` |
|      - | 9162 | ` *  Returns the original data or FALSE on failure.` |
|      - | 9163 | ` */` |
|     34 | 9164 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 9165 | `{` |
|      - | 9166 | `	const char *zIn;` |
|      - | 9167 | `	int nLen;` |
|     36 | 9168 | `	if( nArg < 1 ){` |
|      - | 9169 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9170 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9171 | `		return PH7_OK;` |
|      - | 9172 | `	}` |
|      - | 9173 | `	/* Extract the input string */` |
|     36 | 9174 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 9175 | `	if( nLen < 1 ){` |
|      - | 9176 | `		/* Nothing to process,return FALSE */` |
|      3 | 9177 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9178 | `		return PH7_OK;` |
|      - | 9179 | `	}` |
|      - | 9180 | `	/* Perform the BASE64 decoding */` |
|     34 | 9181 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 9182 | `	return PH7_OK;` |
|     19 | 9183 | `}` |
|      - | 9184 | `/*` |
|      - | 9185 | ` * string urlencode(string $str)` |
|      - | 9186 | ` *  URL encoding` |
|      - | 9187 | ` * Parameter` |
|      - | 9188 | ` *  $data` |
|      - | 9189 | ` *   Input string.` |
|      - | 9190 | ` * Return` |
|      - | 9191 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 9192 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 9193 | ` *  encoded as plus (+) signs.` |
|      - | 9194 | ` */` |
|      4 | 9195 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9196 | `{` |
|      - | 9197 | `	const char *zIn;` |
|      - | 9198 | `	int nLen;` |
|      5 | 9199 | `	if( nArg < 1 ){` |
|      - | 9200 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9201 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9202 | `		return PH7_OK;` |
|      - | 9203 | `	}` |
|      - | 9204 | `	/* Extract the input string */` |
|      5 | 9205 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 9206 | `	if( nLen < 1 ){` |
|      - | 9207 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9208 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9209 | `		return PH7_OK;` |
|      - | 9210 | `	}` |
|      - | 9211 | `	/* Perform the URL encoding */` |
|      5 | 9212 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 9213 | `	return PH7_OK;` |
|      3 | 9214 | `}` |
|      - | 9215 | `/*` |
|      - | 9216 | ` * string urldecode(string $str)` |
|      - | 9217 | ` *  Decodes any %## encoding in the given string.` |
|      - | 9218 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 9219 | ` * Parameter` |
|      - | 9220 | ` *  $data` |
|      - | 9221 | ` *    Input string.` |
|      - | 9222 | ` * Return` |
|      - | 9223 | ` *  Decoded URL or FALSE on failure.` |
|      - | 9224 | ` */` |
|      6 | 9225 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9226 | `{` |
|      - | 9227 | `	const char *zIn;` |
|      - | 9228 | `	int nLen;` |
|      7 | 9229 | `	if( nArg < 1 ){` |
|      - | 9230 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9231 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9232 | `		return PH7_OK;` |
|      - | 9233 | `	}` |
|      - | 9234 | `	/* Extract the input string */` |
|      7 | 9235 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 9236 | `	if( nLen < 1 ){` |
|      - | 9237 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9238 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9239 | `		return PH7_OK;` |
|      - | 9240 | `	}` |
|      - | 9241 | `	/* Perform the URL decoding */` |
|      7 | 9242 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 9243 | `	return PH7_OK;` |
|      4 | 9244 | `}` |
|      - | 9245 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9246 | `/* Table of the built-in functions */` |
|      - | 9247 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 9248 | `	   /* Variable handling functions */` |
|      - | 9249 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 9250 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 9251 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 9252 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 9253 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 9254 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 9255 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 9256 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 9257 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 9258 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 9259 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 9260 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 9261 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 9262 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 9263 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 9264 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 9265 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 9266 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 9267 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 9268 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 9269 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9270 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 9271 | `	   /* Math functions */` |
|      - | 9272 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 9273 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 9274 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 9275 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 9276 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 9277 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 9278 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 9279 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 9280 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 9281 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 9282 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 9283 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 9284 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 9285 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 9286 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 9287 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 9288 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 9289 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 9290 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 9291 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 9292 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 9293 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 9294 | `	{ "round",    PH7_builtin_round        },` |
|      - | 9295 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 9296 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 9297 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 9298 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 9299 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 9300 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 9301 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 9302 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 9303 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 9304 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9305 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9306 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 9307 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9308 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9309 | `	   /* String handling functions */` |
|      - | 9310 |  |
|      - | 9311 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 9312 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 9313 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 9314 | `	{ "substr_replace",  PH7_builtin_substr_replace },` |
|      - | 9315 | `	{ "levenshtein",     PH7_builtin_levenshtein },` |
|      - | 9316 | `	{ "similar_text",    PH7_builtin_similar_text },` |
|      - | 9317 | `	{ "str_word_count",  PH7_builtin_str_word_count },` |
|      - | 9318 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 9319 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 9320 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 9321 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 9322 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 9323 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 9324 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 9325 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 9326 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 9327 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 9328 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 9329 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 9330 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 9331 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 9332 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 9333 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 9334 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 9335 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 9336 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 9337 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 9338 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 9339 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 9340 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 9341 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 9342 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 9343 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 9344 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 9345 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 9346 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 9347 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 9348 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 9349 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 9350 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 9351 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 9352 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 9353 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 9354 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 9355 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 9356 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 9357 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 9358 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 9359 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 9360 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 9361 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 9362 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 9363 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 9364 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 9365 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 9366 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 9367 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9368 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9369 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 9370 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 9371 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 9372 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 9373 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9374 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9375 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 9376 |  |
|      - | 9377 |  |
|      - | 9378 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 9379 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 9380 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 9381 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 9382 | `	{ "hash",         PH7_builtin_hash      },` |
|      - | 9383 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|      - | 9384 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|      - | 9385 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|      - | 9386 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 9387 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|      - | 9388 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|      - | 9389 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|      - | 9390 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|      - | 9391 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|      - | 9392 | `	{ "filter_input",          PH7_builtin_filter_input },` |
|      - | 9393 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9394 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9395 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 9396 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 9397 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9398 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9399 |  |
|      - | 9400 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 9401 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 9402 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 9403 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 9404 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 9405 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 9406 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 9407 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 9408 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 9409 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 9410 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 9411 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 9412 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9413 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9414 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 9415 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9416 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9417 |  |
|      - | 9418 | `	         /* Ctype functions */` |
|      - | 9419 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 9420 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 9421 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 9422 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 9423 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 9424 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 9425 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 9426 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 9427 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 9428 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 9429 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 9430 | `	         /* Time functions */` |
|      - | 9431 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 9432 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 9433 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 9434 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 9435 | `	{ "date",        PH7_builtin_date         },` |
|      - | 9436 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 9437 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 9438 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 9439 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 9440 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 9441 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 9442 | `	        /* URL functions */` |
|      - | 9443 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 9444 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 9445 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 9446 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 9447 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 9448 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 9449 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 9450 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 9451 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9452 | `};` |
|      - | 9453 | `/*` |
|      - | 9454 | ` * Register the built-in functions defined above,the array functions` |
|      - | 9455 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 9456 | ` */` |
|   3474 | 9457 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      5 | 9458 | `{` |
|      - | 9459 | `	sxu32 n;` |
| 597533 | 9460 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 594059 | 9461 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 297032 | 9462 | `	}` |
|      - | 9463 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   3479 | 9464 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 9465 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   3479 | 9466 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   3479 | 9467 | `}` |
|      - | 9468 |  |
