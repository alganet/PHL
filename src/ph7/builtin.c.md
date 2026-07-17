# src/ph7/builtin.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 4265/4944 lines (86.27%)

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
|  33882 |  303 | `static int PH7_builtin_empty(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  304 | `{` |
|  33887 |  305 | `	int res = 1; /* Assume empty by default */` |
|  33887 |  306 | `	if( nArg > 0 ){` |
|  33885 |  307 | `		res = ph7_value_is_empty(apArg[0]);` |
|  16940 |  308 | `	}` |
|  33887 |  309 | `	ph7_result_bool(pCtx,res);` |
|  33887 |  310 | `	return PH7_OK;` |
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
| 223188 |  353 | `static int PH7_builtin_substr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 |  354 | `{` |
|      - |  355 | `	const char *zSource,*zOfft;` |
|      - |  356 | `	int nOfft,nLen,nSrcLen;` |
| 223193 |  357 | `	if( nArg < 2 ){` |
|      - |  358 | `		/* return FALSE */` |
|    ! 0 |  359 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  360 | `		return PH7_OK;` |
|      - |  361 | `	}` |
|      - |  362 | `	/* Extract the target string */` |
| 223193 |  363 | `	zSource = ph7_value_to_string(apArg[0],&nSrcLen);` |
| 223193 |  364 | `	if( nSrcLen < 1 ){` |
|      - |  365 | `		/* Empty string,return FALSE */` |
|  12087 |  366 | `		ph7_result_bool(pCtx,0);` |
|  12087 |  367 | `		return PH7_OK;` |
|      - |  368 | `	}` |
| 211111 |  369 | `	nLen = nSrcLen; /* cc warning */` |
|      - |  370 | `	/* Extract the offset */` |
| 211111 |  371 | `	nOfft = ph7_value_to_int(apArg[1]);` |
| 211111 |  372 | `	if( nOfft < 0 ){` |
|  32681 |  373 | `		zOfft = &zSource[nSrcLen+nOfft];` |
|  32681 |  374 | `		if( zOfft < zSource ){` |
|      - |  375 | `			/* Invalid offset */` |
|      5 |  376 | `			ph7_result_bool(pCtx,0);` |
|      5 |  377 | `			return PH7_OK;` |
|      - |  378 | `		}` |
|  32677 |  379 | `		nLen = (int)(&zSource[nSrcLen]-zOfft);` |
|  32677 |  380 | `		nOfft = (int)(zOfft-zSource);` |
| 194771 |  381 | `	}else if( nOfft >= nSrcLen ){` |
|      - |  382 | `		/* Invalid offset */` |
|    217 |  383 | `		ph7_result_bool(pCtx,0);` |
|    217 |  384 | `		return PH7_OK;` |
|    ! 0 |  385 | `	}else{` |
| 178223 |  386 | `		zOfft = &zSource[nOfft];` |
| 178223 |  387 | `		nLen = nSrcLen - nOfft;` |
|      - |  388 | `	}` |
| 210895 |  389 | `	if( nArg > 2 ){` |
|      - |  390 | `		/* Extract the length */` |
| 173827 |  391 | `		nLen = ph7_value_to_int(apArg[2]);` |
| 173827 |  392 | `		if( nLen == 0 ){` |
|      - |  393 | `			/* Invalid length,return an empty string */` |
|      5 |  394 | `			ph7_result_string(pCtx,"",0);` |
|      5 |  395 | `			return PH7_OK;` |
| 173823 |  396 | `		}else if( nLen < 0 ){` |
|  32669 |  397 | `			nLen = nSrcLen + nLen - nOfft;` |
|  32669 |  398 | `			if( nLen < 1 ){` |
|      - |  399 | `				/* Invalid  length */` |
|      3 |  400 | `				nLen = nSrcLen - nOfft;` |
|      1 |  401 | `			}` |
|  16332 |  402 | `		}` |
| 173823 |  403 | `		if( nLen + nOfft > nSrcLen ){` |
|      - |  404 | `			/* Invalid length */` |
|   5571 |  405 | `			nLen = nSrcLen - nOfft;` |
|   2783 |  406 | `		}` |
|  86909 |  407 | `	}` |
|      - |  408 | `	/* Return the substring */` |
| 210891 |  409 | `	ph7_result_string(pCtx,zOfft,nLen);` |
| 210891 |  410 | `	return PH7_OK;` |
| 111599 |  411 | `}` |
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
|      - |  602 | ` * Validate and resolve an int-typed builtin parameter with php-8 ZPP weak-mode` |
|      - |  603 | ` * semantics: ints and bools pass through; null emits the 8.1 deprecation and` |
|      - |  604 | ` * resolves to 0; floats and float-strings convert, with the implicit-conversion` |
|      - |  605 | ` * E_DEPRECATED when lossy and a TypeError when NAN/INF/out of int range;` |
|      - |  606 | ` * integral numeric strings convert exactly; everything else (arrays, resources,` |
|      - |  607 | ` * objects, non-numeric strings) is a TypeError naming zTypeStr (e.g. "int",` |
|      - |  608 | ` * "array\|int"). Returns PH7_OK with *pOut set, or the throw status.` |
|      - |  609 | ` */` |
|    150 |  610 | `static sxi32 IntArgResolve(` |
|      - |  611 | `	ph7_context *pCtx,` |
|      - |  612 | `	ph7_value *pArg,` |
|      - |  613 | `	const char *zFunc,` |
|      - |  614 | `	int iArgNum,` |
|      - |  615 | `	const char *zParamName,` |
|      - |  616 | `	const char *zTypeStr,` |
|      - |  617 | `	sxi64 *pOut` |
|      1 |  618 | `){` |
|    151 |  619 | `	if( ph7_value_is_null(pArg) ){` |
|    ! 0 |  620 | `		PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |  621 | `			"%s(): Passing null to parameter #%d (%s) of type %s is deprecated",` |
|    ! 0 |  622 | `			zFunc,iArgNum,zParamName,zTypeStr` |
|      - |  623 | `			);` |
|    ! 0 |  624 | `		*pOut = 0;` |
|    ! 0 |  625 | `		return PH7_OK;` |
|      - |  626 | `	}` |
|    151 |  627 | `	if( ph7_value_is_float(pArg) ){` |
|      5 |  628 | `		double dVal = ph7_value_to_double(pArg);` |
|      - |  629 | `		sxi64 iVal;` |
|      - |  630 | `		/* php: NAN/INF/out-of-int64-range floats fail ZPP outright */` |
|      5 |  631 | `		if( dVal != dVal \|\| dVal >= 9223372036854775808.0 \|\| dVal < -9223372036854775808.0 ){` |
|      7 |  632 | `			return PH7_VmThrowException(pCtx,` |
|      - |  633 | `				"TypeError",` |
|      - |  634 | `				"%s(): Argument #%d (%s) must be of type %s, float given",` |
|      2 |  635 | `				zFunc,iArgNum,zParamName,zTypeStr` |
|      - |  636 | `				);` |
|      - |  637 | `		}` |
|    ! 0 |  638 | `		iVal = (sxi64)dVal;` |
|    ! 0 |  639 | `		if( (double)iVal != dVal ){` |
|    ! 0 |  640 | `			PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |  641 | `				"Implicit conversion from float %s to int loses precision",` |
|    ! 0 |  642 | `				ph7_value_to_string(pArg,0)` |
|      - |  643 | `				);` |
|    ! 0 |  644 | `		}` |
|    ! 0 |  645 | `		*pOut = iVal;` |
|    ! 0 |  646 | `		return PH7_OK;` |
|      - |  647 | `	}` |
|    147 |  648 | `	if( ph7_value_is_string(pArg) ){` |
|      - |  649 | `		const char *zNum;` |
|      - |  650 | `		int nSlen;` |
|     15 |  651 | `		int i,bFloat = 0;` |
|     15 |  652 | `		if( !PH7_MemObjStringIsNumeric(pArg) ){` |
|     16 |  653 | `			return PH7_VmThrowException(pCtx,` |
|      - |  654 | `				"TypeError",` |
|      - |  655 | `				"%s(): Argument #%d (%s) must be of type %s, string given",` |
|      5 |  656 | `				zFunc,iArgNum,zParamName,zTypeStr` |
|      - |  657 | `				);` |
|      - |  658 | `		}` |
|      5 |  659 | `		zNum = ph7_value_to_string(pArg,&nSlen);` |
|      9 |  660 | `		for( i = 0 ; i < nSlen ; i++ ){` |
|      5 |  661 | `			if( zNum[i] == '.' \|\| zNum[i] == 'e' \|\| zNum[i] == 'E' ){` |
|    ! 0 |  662 | `				bFloat = 1;` |
|    ! 0 |  663 | `				break;` |
|      - |  664 | `			}` |
|      3 |  665 | `		}` |
|      5 |  666 | `		if( bFloat ){` |
|    ! 0 |  667 | `			double dVal = 0;` |
|      - |  668 | `			sxi64 iVal;` |
|    ! 0 |  669 | `			SyStrToReal(zNum,(sxu32)nSlen,(void *)&dVal,0);` |
|    ! 0 |  670 | `			if( dVal != dVal \|\| dVal >= 9223372036854775808.0 \|\| dVal < -9223372036854775808.0 ){` |
|    ! 0 |  671 | `				return PH7_VmThrowException(pCtx,` |
|      - |  672 | `					"TypeError",` |
|      - |  673 | `					"%s(): Argument #%d (%s) must be of type %s, string given",` |
|    ! 0 |  674 | `					zFunc,iArgNum,zParamName,zTypeStr` |
|      - |  675 | `					);` |
|      - |  676 | `			}` |
|    ! 0 |  677 | `			iVal = (sxi64)dVal;` |
|    ! 0 |  678 | `			if( (double)iVal != dVal ){` |
|    ! 0 |  679 | `				PH7_VmThrowDeprecatedFmt(pCtx->pVm,` |
|      - |  680 | `					"Implicit conversion from float-string \"%s\" to int loses precision",` |
|    ! 0 |  681 | `					zNum` |
|      - |  682 | `					);` |
|    ! 0 |  683 | `			}` |
|    ! 0 |  684 | `			*pOut = iVal;` |
|    ! 0 |  685 | `			return PH7_OK;` |
|      - |  686 | `		}` |
|      5 |  687 | `		*pOut = ph7_value_to_int64(pArg);` |
|      5 |  688 | `		return PH7_OK;` |
|      - |  689 | `	}` |
|    133 |  690 | `	if( !ph7_value_is_int(pArg) && !ph7_value_is_bool(pArg) ){` |
|      - |  691 | `		/* Arrays, resources and objects: php names the class for objects */` |
|      5 |  692 | `		const char *zType = ph7_type_name(pArg);` |
|      5 |  693 | `		if( ph7_value_is_object(pArg) ){` |
|      3 |  694 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|      3 |  695 | `			if( pInst && pInst->pClass ){` |
|      3 |  696 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      1 |  697 | `			}` |
|      1 |  698 | `		}` |
|      7 |  699 | `		return PH7_VmThrowException(pCtx,` |
|      - |  700 | `			"TypeError",` |
|      - |  701 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|      2 |  702 | `			zFunc,iArgNum,zParamName,zTypeStr,zType` |
|      - |  703 | `			);` |
|      - |  704 | `	}` |
|    129 |  705 | `	*pOut = ph7_value_to_int64(pArg);` |
|    129 |  706 | `	return PH7_OK;` |
|     76 |  707 | `}` |
|      - |  708 | `/*` |
|      - |  709 | ` * Normalize a substr_replace() offset/length pair against a string of nStrLen` |
|      - |  710 | ` * bytes, exactly like PHP: a negative offset counts from the end (clamped to 0),` |
|      - |  711 | ` * an offset past the end clamps to the end; a negative length leaves that many` |
|      - |  712 | ` * bytes off the end of the remaining region (clamped to 0), and the length is` |
|      - |  713 | ` * finally clamped to the remaining region. Written without f+l additions so an` |
|      - |  714 | ` * INT64_MAX length cannot overflow.` |
|      - |  715 | ` */` |
|     60 |  716 | `static void SubstrReplaceWindow(sxi64 *pF,sxi64 *pL,int nStrLen)` |
|      1 |  717 | `{` |
|     61 |  718 | `	sxi64 f = *pF,l = *pL;` |
|     61 |  719 | `	if( f < 0 ){` |
|      9 |  720 | `		f += nStrLen;` |
|      9 |  721 | `		if( f < 0 ){` |
|      5 |  722 | `			f = 0;` |
|      3 |  723 | `		}` |
|     57 |  724 | `	}else if( f > nStrLen ){` |
|      5 |  725 | `		f = nStrLen;` |
|      2 |  726 | `	}` |
|     61 |  727 | `	if( l < 0 ){` |
|      7 |  728 | `		l += nStrLen - f;` |
|      7 |  729 | `		if( l < 0 ){` |
|      5 |  730 | `			l = 0;` |
|      2 |  731 | `		}` |
|      3 |  732 | `	}` |
|     61 |  733 | `	if( l > nStrLen - f ){` |
|     25 |  734 | `		l = nStrLen - f;` |
|     12 |  735 | `	}` |
|     61 |  736 | `	*pF = f;` |
|     61 |  737 | `	*pL = l;` |
|     61 |  738 | `}` |
|      - |  739 | `/* A replacement string collected out of substr_replace()'s $replace array.` |
|      - |  740 | ` * The bytes live in a shared pool blob (walker values are transient), so the` |
|      - |  741 | ` * item stores pool offsets, mirroring the strtr_entry technique. */` |
|      - |  742 | `typedef struct substr_repl_item substr_repl_item;` |
|      - |  743 | `struct substr_repl_item` |
|      - |  744 | `{` |
|      - |  745 | `	sxu32 nOfft; /* Offset of the string inside the pool */` |
|      - |  746 | `	sxu32 nLen;  /* Length of the string */` |
|      - |  747 | `};` |
|      - |  748 | `typedef struct substr_replace_collect substr_replace_collect;` |
|      - |  749 | `struct substr_replace_collect` |
|      - |  750 | `{` |
|      - |  751 | `	SyBlob *pPool;  /* Byte pool for string items (string walker only) */` |
|      - |  752 | `	SySet *pSet;    /* substr_repl_item set (string) or sxi64 set (int) */` |
|      - |  753 | `	sxi32 rc;       /* SXRET_OK or SXERR_MEM on collector failure */` |
|      - |  754 | `};` |
|      - |  755 | `/* ph7_array_walk() callback: append one $replace element to the pool. */` |
|      6 |  756 | `static int SubstrReplaceStrWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  757 | `{` |
|      7 |  758 | `	substr_replace_collect *pCol = (substr_replace_collect *)pUserData;` |
|      - |  759 | `	substr_repl_item sItem;` |
|      - |  760 | `	const char *zStr;` |
|      - |  761 | `	int nLen;` |
|      3 |  762 | `	SXUNUSED(pKey);` |
|      7 |  763 | `	zStr = ph7_value_to_string(pData,&nLen);` |
|      7 |  764 | `	sItem.nOfft = SyBlobLength(pCol->pPool);` |
|      7 |  765 | `	sItem.nLen = (sxu32)nLen;` |
|      7 |  766 | `	if( nLen > 0 && SXRET_OK != SyBlobAppend(pCol->pPool,(const void *)zStr,(sxu32)nLen) ){` |
|    ! 0 |  767 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  768 | `		return SXERR_ABORT;` |
|      - |  769 | `	}` |
|      7 |  770 | `	if( SXRET_OK != SySetPut(pCol->pSet,(const void *)&sItem) ){` |
|    ! 0 |  771 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  772 | `		return SXERR_ABORT;` |
|      - |  773 | `	}` |
|      7 |  774 | `	return PH7_OK;` |
|      4 |  775 | `}` |
|      - |  776 | `/* ph7_array_walk() callback: collect one $offset/$length element as an int. */` |
|     12 |  777 | `static int SubstrReplaceIntWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  778 | `{` |
|     13 |  779 | `	substr_replace_collect *pCol = (substr_replace_collect *)pUserData;` |
|     13 |  780 | `	sxi64 iVal = ph7_value_to_int64(pData);` |
|      6 |  781 | `	SXUNUSED(pKey);` |
|     13 |  782 | `	if( SXRET_OK != SySetPut(pCol->pSet,(const void *)&iVal) ){` |
|    ! 0 |  783 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 |  784 | `		return SXERR_ABORT;` |
|      - |  785 | `	}` |
|     13 |  786 | `	return PH7_OK;` |
|      7 |  787 | `}` |
|      - |  788 | `/* Per-element state while walking substr_replace()'s array $string. */` |
|      - |  789 | `typedef struct substr_replace_ctx substr_replace_ctx;` |
|      - |  790 | `struct substr_replace_ctx` |
|      - |  791 | `{` |
|      - |  792 | `	ph7_value *pResult;   /* Result array (keys preserved) */` |
|      - |  793 | `	ph7_value *pScratch;  /* Reusable string value for each element */` |
|      - |  794 | `	SyBlob *pReplPool;    /* Pool behind aRepl items */` |
|      - |  795 | `	SySet *pRepl;         /* substr_repl_item set or NULL when $replace is scalar */` |
|      - |  796 | `	SySet *pFrom;         /* sxi64 set or NULL when $offset is scalar */` |
|      - |  797 | `	SySet *pLen;          /* sxi64 set or NULL when $length is scalar/absent */` |
|      - |  798 | `	sxu32 iReplCur;       /* Next-position cursors into the three sets */` |
|      - |  799 | `	sxu32 iFromCur;` |
|      - |  800 | `	sxu32 iLenCur;` |
|      - |  801 | `	const char *zRepl;    /* Scalar $replace */` |
|      - |  802 | `	int nRepl;` |
|      - |  803 | `	sxi64 iFrom;          /* Scalar $offset */` |
|      - |  804 | `	sxi64 iLen;           /* Scalar $length */` |
|      - |  805 | `	int bLenGiven;        /* FALSE: $length absent/null -> element length */` |
|      - |  806 | `	sxi32 rc;             /* SXRET_OK or SXERR_MEM */` |
|      - |  807 | `};` |
|      - |  808 | `/*` |
|      - |  809 | ` * ph7_array_walk() callback over the array $string: replace the window of one` |
|      - |  810 | ` * element and insert the result under the element's original key. Array-form` |
|      - |  811 | ` * $replace/$offset/$length are consumed positionally; when a set runs out PHP` |
|      - |  812 | ` * falls back to ""/0/element-length respectively.` |
|      - |  813 | ` */` |
|     24 |  814 | `static int SubstrReplaceElemWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 |  815 | `{` |
|     25 |  816 | `	substr_replace_ctx *pRep = (substr_replace_ctx *)pUserData;` |
|      - |  817 | `	const char *zStr,*zRepl;` |
|      - |  818 | `	sxi64 f,l;` |
|      - |  819 | `	int nLen,nRepl;` |
|     25 |  820 | `	zStr = ph7_value_to_string(pData,&nLen);` |
|      - |  821 | `	/* Positional $replace element ("" when exhausted) */` |
|     25 |  822 | `	if( pRep->pRepl ){` |
|     11 |  823 | `		if( pRep->iReplCur < SySetUsed(pRep->pRepl) ){` |
|      7 |  824 | `			substr_repl_item *pItem = (substr_repl_item *)SySetAt(pRep->pRepl,pRep->iReplCur++);` |
|      7 |  825 | `			zRepl = (const char *)SyBlobDataAt(pRep->pReplPool,pItem->nOfft);` |
|      7 |  826 | `			nRepl = (int)pItem->nLen;` |
|      4 |  827 | `		}else{` |
|      5 |  828 | `			zRepl = "";` |
|      5 |  829 | `			nRepl = 0;` |
|      - |  830 | `		}` |
|      6 |  831 | `	}else{` |
|     15 |  832 | `		zRepl = pRep->zRepl;` |
|     15 |  833 | `		nRepl = pRep->nRepl;` |
|      - |  834 | `	}` |
|      - |  835 | `	/* Positional $offset element (0 when exhausted) */` |
|     25 |  836 | `	if( pRep->pFrom ){` |
|     13 |  837 | `		sxi64 *pVal = 0;` |
|     13 |  838 | `		if( pRep->iFromCur < SySetUsed(pRep->pFrom) ){` |
|      9 |  839 | `			pVal = (sxi64 *)SySetAt(pRep->pFrom,pRep->iFromCur++);` |
|      4 |  840 | `		}` |
|     13 |  841 | `		f = pVal ? *pVal : 0;` |
|      7 |  842 | `	}else{` |
|     13 |  843 | `		f = pRep->iFrom;` |
|      - |  844 | `	}` |
|      - |  845 | `	/* Positional $length element (element length when exhausted) */` |
|     25 |  846 | `	if( pRep->pLen ){` |
|      7 |  847 | `		sxi64 *pVal = 0;` |
|      7 |  848 | `		if( pRep->iLenCur < SySetUsed(pRep->pLen) ){` |
|      5 |  849 | `			pVal = (sxi64 *)SySetAt(pRep->pLen,pRep->iLenCur++);` |
|      2 |  850 | `		}` |
|      7 |  851 | `		l = pVal ? *pVal : nLen;` |
|      4 |  852 | `	}else{` |
|     19 |  853 | `		l = pRep->bLenGiven ? pRep->iLen : nLen;` |
|      - |  854 | `	}` |
|     25 |  855 | `	SubstrReplaceWindow(&f,&l,nLen);` |
|      - |  856 | `	/* Assemble prefix + replacement + suffix in the scratch value */` |
|     25 |  857 | `	ph7_value_reset_string_cursor(pRep->pScratch);` |
|     24 |  858 | `	if( (f > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,zStr,(int)f))` |
|     24 |  859 | `	 \|\| (nRepl > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,zRepl,nRepl))` |
|     40 |  860 | `	 \|\| (nLen - (int)(f+l) > 0 && SXRET_OK != ph7_value_string(pRep->pScratch,&zStr[f+l],nLen - (int)(f+l))) ){` |
|     30 |  861 | `		pRep->rc = SXERR_MEM;` |
|     30 |  862 | `		return SXERR_ABORT;` |
|      - |  863 | `	}` |
|     25 |  864 | `	if( SXRET_OK != ph7_array_add_elem(pRep->pResult,pKey,pRep->pScratch) ){` |
|    ! 0 |  865 | `		pRep->rc = SXERR_MEM;` |
|    ! 0 |  866 | `		return SXERR_ABORT;` |
|      - |  867 | `	}` |
|     25 |  868 | `	return PH7_OK;` |
|     43 |  869 | `}` |
|      - |  870 | `/*` |
|      - |  871 | ` * mixed substr_replace(array\|string $string,array\|string $replace,array\|int $offset[,array\|int\|null $length = null])` |
|      - |  872 | ` *  Replace text within a portion of a string.` |
|      - |  873 | ` * Parameters` |
|      - |  874 | ` *  $string` |
|      - |  875 | ` *   The input string or an array of strings (each element is processed with` |
|      - |  876 | ` *   its own positional replace/offset/length when those are arrays too).` |
|      - |  877 | ` *  $replace` |
|      - |  878 | ` *   The replacement string. When $string is scalar and $replace is an array,` |
|      - |  879 | ` *   only its first element is used (PHP quirk).` |
|      - |  880 | ` *  $offset` |
|      - |  881 | ` *   Window start; negative counts from the end of the string.` |
|      - |  882 | ` *  $length` |
|      - |  883 | ` *   Window length; negative leaves that many bytes at the end; null/absent` |
|      - |  884 | ` *   means "to the end of the string".` |
|      - |  885 | ` * Return` |
|      - |  886 | ` *  The processed string, or an array of processed strings (keys preserved).` |
|      - |  887 | ` * Errors` |
|      - |  888 | ` *  ArgumentCountError on fewer than 3 arguments; TypeError when an array` |
|      - |  889 | ` *  $offset/$length is combined with a scalar $string.` |
|      - |  890 | ` */` |
|     68 |  891 | `static int PH7_builtin_substr_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  892 | `{` |
|      - |  893 | `	ph7_value sStrTmp,sReplTmp;` |
|     69 |  894 | `	const char *zStr = 0,*zRepl = 0;` |
|     69 |  895 | `	int nLen = 0,nRepl = 0;` |
|      - |  896 | `	int bLenGiven;` |
|     69 |  897 | `	sxi64 f = 0,l = 0;` |
|      - |  898 | `	sxi32 rc;` |
|     69 |  899 | `	if( nArg < 3 ){` |
|      7 |  900 | `		return PH7_VmThrowException(pCtx,` |
|      - |  901 | `			"ArgumentCountError",` |
|      - |  902 | `			"substr_replace() expects at least 3 arguments, %d given",` |
|      2 |  903 | `			nArg` |
|      - |  904 | `			);` |
|      - |  905 | `	}` |
|      - |  906 | `	/* $length counts as given unless absent or null (php: ?null semantics) */` |
|     65 |  907 | `	bLenGiven = (nArg > 3 && !ph7_value_is_null(apArg[3]));` |
|      - |  908 | `	/* php ZPP validates all four args, in order, before the body runs: the` |
|      - |  909 | `	 * non-array forms resolve here (null deprecation, __toString objects,` |
|      - |  910 | `	 * numeric strings), arrays pass through to the per-mode handling. */` |
|     65 |  911 | `	PH7_MemObjInit(pCtx->pVm,&sStrTmp);` |
|     65 |  912 | `	PH7_MemObjInit(pCtx->pVm,&sReplTmp);` |
|     65 |  913 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     49 |  914 | `		rc = StrPredicateResolveArg(pCtx,apArg[0],"substr_replace",1,"$string","array\|string",` |
|      - |  915 | `			"substr_replace(): Passing null to parameter #1 ($string) "` |
|      - |  916 | `			"of type array\|string is deprecated",` |
|      - |  917 | `			&sStrTmp,&zStr,&nLen);` |
|     49 |  918 | `		if( rc != PH7_OK ) goto out;` |
|     23 |  919 | `	}` |
|     63 |  920 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|     55 |  921 | `		rc = StrPredicateResolveArg(pCtx,apArg[1],"substr_replace",2,"$replace","array\|string",` |
|      - |  922 | `			"substr_replace(): Passing null to parameter #2 ($replace) "` |
|      - |  923 | `			"of type array\|string is deprecated",` |
|      - |  924 | `			&sReplTmp,&zRepl,&nRepl);` |
|     55 |  925 | `		if( rc != PH7_OK ) goto out;` |
|     25 |  926 | `	}` |
|     59 |  927 | `	if( !ph7_value_is_array(apArg[2]) ){` |
|     51 |  928 | `		rc = IntArgResolve(pCtx,apArg[2],"substr_replace",3,"$offset","array\|int",&f);` |
|     51 |  929 | `		if( rc != PH7_OK ) goto out;` |
|     24 |  930 | `	}` |
|     57 |  931 | `	if( bLenGiven && !ph7_value_is_array(apArg[3]) ){` |
|     31 |  932 | `		rc = IntArgResolve(pCtx,apArg[3],"substr_replace",4,"$length","array\|int\|null",&l);` |
|     31 |  933 | `		if( rc != PH7_OK ) goto out;` |
|     14 |  934 | `	}` |
|     55 |  935 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - |  936 | `		/* Array form: process each element, preserving keys */` |
|      - |  937 | `		substr_replace_ctx sRep;` |
|      - |  938 | `		substr_replace_collect sCol;` |
|      - |  939 | `		SyBlob sReplPool;` |
|      - |  940 | `		SySet sRepl,sFrom,sLen;` |
|      - |  941 | `		ph7_value *pResult,*pScratch;` |
|     15 |  942 | `		sxi32 rcWalk = SXRET_OK;` |
|     15 |  943 | `		SyBlobInit(&sReplPool,&pCtx->pVm->sAllocator);` |
|     15 |  944 | `		SySetInit(&sRepl,&pCtx->pVm->sAllocator,sizeof(substr_repl_item));` |
|     15 |  945 | `		SySetInit(&sFrom,&pCtx->pVm->sAllocator,sizeof(sxi64));` |
|     15 |  946 | `		SySetInit(&sLen,&pCtx->pVm->sAllocator,sizeof(sxi64));` |
|     15 |  947 | `		SyZero(&sRep,sizeof(substr_replace_ctx));` |
|     15 |  948 | `		sRep.bLenGiven = bLenGiven;` |
|     15 |  949 | `		sCol.rc = SXRET_OK;` |
|      - |  950 | `		/* Collect array-form $replace/$offset/$length positionally; the` |
|      - |  951 | `		 * scalar forms were already resolved above. */` |
|     15 |  952 | `		if( ph7_value_is_array(apArg[1]) ){` |
|      5 |  953 | `			sCol.pPool = &sReplPool;` |
|      5 |  954 | `			sCol.pSet = &sRepl;` |
|      5 |  955 | `			ph7_array_walk(apArg[1],SubstrReplaceStrWalker,&sCol);` |
|      5 |  956 | `			sRep.pRepl = &sRepl;` |
|      5 |  957 | `			sRep.pReplPool = &sReplPool;` |
|      3 |  958 | `		}else{` |
|     11 |  959 | `			sRep.zRepl = zRepl;` |
|     11 |  960 | `			sRep.nRepl = nRepl;` |
|      - |  961 | `		}` |
|     15 |  962 | `		if( sCol.rc == SXRET_OK && ph7_value_is_array(apArg[2]) ){` |
|      7 |  963 | `			sCol.pSet = &sFrom;` |
|      7 |  964 | `			ph7_array_walk(apArg[2],SubstrReplaceIntWalker,&sCol);` |
|      7 |  965 | `			sRep.pFrom = &sFrom;` |
|      4 |  966 | `		}else{` |
|      9 |  967 | `			sRep.iFrom = f;` |
|      - |  968 | `		}` |
|     15 |  969 | `		if( sCol.rc == SXRET_OK && bLenGiven ){` |
|      9 |  970 | `			if( ph7_value_is_array(apArg[3]) ){` |
|      5 |  971 | `				sCol.pSet = &sLen;` |
|      5 |  972 | `				ph7_array_walk(apArg[3],SubstrReplaceIntWalker,&sCol);` |
|      5 |  973 | `				sRep.pLen = &sLen;` |
|      3 |  974 | `			}else{` |
|      5 |  975 | `				sRep.iLen = l;` |
|      - |  976 | `			}` |
|      4 |  977 | `		}` |
|     15 |  978 | `		pResult = ph7_context_new_array(pCtx);` |
|     15 |  979 | `		pScratch = ph7_context_new_scalar(pCtx);` |
|     15 |  980 | `		if( sCol.rc != SXRET_OK \|\| pResult == 0 \|\| pScratch == 0 ){` |
|    ! 0 |  981 | `			rcWalk = SXERR_MEM;` |
|    ! 0 |  982 | `		}else{` |
|     15 |  983 | `			sRep.pResult = pResult;` |
|     15 |  984 | `			sRep.pScratch = pScratch;` |
|     15 |  985 | `			ph7_value_string(pScratch,"",0); /* Force string representation */` |
|     15 |  986 | `			ph7_array_walk(apArg[0],SubstrReplaceElemWalker,&sRep);` |
|     15 |  987 | `			rcWalk = sRep.rc;` |
|      - |  988 | `		}` |
|     15 |  989 | `		SyBlobRelease(&sReplPool);` |
|     15 |  990 | `		SySetRelease(&sRepl);` |
|     15 |  991 | `		SySetRelease(&sFrom);` |
|     15 |  992 | `		SySetRelease(&sLen);` |
|     15 |  993 | `		if( rcWalk != SXRET_OK ){` |
|    ! 0 |  994 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 |  995 | `			goto out;` |
|      - |  996 | `		}` |
|     15 |  997 | `		ph7_result_value(pCtx,pResult);` |
|     15 |  998 | `		rc = PH7_OK;` |
|     15 |  999 | `		goto out;` |
|      - | 1000 | `	}` |
|      - | 1001 | `	/* Scalar form: array $offset/$length are a TypeError, array $replace` |
|      - | 1002 | `	 * degrades to its first element (php quirk). */` |
|     41 | 1003 | `	if( ph7_value_is_array(apArg[2]) ){` |
|      3 | 1004 | `		rc = PH7_VmThrowException(pCtx,` |
|      - | 1005 | `			"TypeError",` |
|      - | 1006 | `			"substr_replace(): Argument #3 ($offset) cannot be an array when working on a single string"` |
|      - | 1007 | `			);` |
|      3 | 1008 | `		goto out;` |
|      - | 1009 | `	}` |
|     39 | 1010 | `	if( bLenGiven && ph7_value_is_array(apArg[3]) ){` |
|      3 | 1011 | `		rc = PH7_VmThrowException(pCtx,` |
|      - | 1012 | `			"TypeError",` |
|      - | 1013 | `			"substr_replace(): Argument #4 ($length) cannot be an array when working on a single string"` |
|      - | 1014 | `			);` |
|      3 | 1015 | `		goto out;` |
|      - | 1016 | `	}` |
|     37 | 1017 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 1018 | `		/* First element of the replace array, or "" when empty */` |
|      5 | 1019 | `		ph7_hashmap *pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      5 | 1020 | `		zRepl = "";` |
|      5 | 1021 | `		nRepl = 0;` |
|      5 | 1022 | `		if( pMap->pFirst ){` |
|      3 | 1023 | `			ph7_value *pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,pMap->pFirst->nValIdx);` |
|      3 | 1024 | `			if( pVal ){` |
|      3 | 1025 | `				zRepl = ph7_value_to_string(pVal,&nRepl);` |
|      1 | 1026 | `			}` |
|      1 | 1027 | `		}` |
|      2 | 1028 | `	}` |
|     37 | 1029 | `	if( !bLenGiven ){` |
|     15 | 1030 | `		l = nLen;` |
|      7 | 1031 | `	}` |
|     37 | 1032 | `	SubstrReplaceWindow(&f,&l,nLen);` |
|      - | 1033 | `	/* Assemble prefix + replacement + suffix straight into the call result` |
|      - | 1034 | `	 * (ph7_result_string appends), no scratch buffer needed. */` |
|     37 | 1035 | `	rc = SXRET_OK;` |
|     37 | 1036 | `	if( f > 0 ){` |
|     29 | 1037 | `		rc = ph7_result_string(pCtx,zStr,(int)f);` |
|     14 | 1038 | `	}` |
|     37 | 1039 | `	if( rc == SXRET_OK && nRepl > 0 ){` |
|     33 | 1040 | `		rc = ph7_result_string(pCtx,zRepl,nRepl);` |
|     16 | 1041 | `	}` |
|     37 | 1042 | `	if( rc == SXRET_OK && nLen - (int)(f+l) > 0 ){` |
|     17 | 1043 | `		rc = ph7_result_string(pCtx,&zStr[f+l],nLen - (int)(f+l));` |
|      8 | 1044 | `	}` |
|     37 | 1045 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 1046 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1047 | `		goto out;` |
|      - | 1048 | `	}` |
|      - | 1049 | `	/* Force a string result even when all three segments are empty */` |
|     37 | 1050 | `	rc = ph7_result_string(pCtx,"",0);` |
|     37 | 1051 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 1052 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1053 | `		goto out;` |
|      - | 1054 | `	}` |
|     37 | 1055 | `	rc = PH7_OK;` |
|     32 | 1056 | `out:` |
|     65 | 1057 | `	PH7_MemObjRelease(&sStrTmp);` |
|     65 | 1058 | `	PH7_MemObjRelease(&sReplTmp);` |
|     65 | 1059 | `	return rc;` |
|     35 | 1060 | `}` |
|      - | 1061 | `/*` |
|      - | 1062 | ` * int levenshtein(string $string1,string $string2[,int $insertion_cost = 1[,int $replacement_cost = 1[,int $deletion_cost = 1]]])` |
|      - | 1063 | ` *  Calculate the Levenshtein distance between two strings, byte per byte` |
|      - | 1064 | ` *  (case-sensitive), with optional per-operation costs. Mirrors PHP's` |
|      - | 1065 | ` *  reference_levdist(): two rolling rows over string2.` |
|      - | 1066 | ` * Return` |
|      - | 1067 | ` *  The minimal number of weighted edit operations turning $string1 into` |
|      - | 1068 | ` *  $string2.` |
|      - | 1069 | ` */` |
|     42 | 1070 | `static int PH7_builtin_levenshtein(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1071 | `{` |
|      - | 1072 | `	static const char *azParam[] = { "$insertion_cost","$replacement_cost","$deletion_cost" };` |
|      - | 1073 | `	const char *zStr1,*zStr2;` |
|     43 | 1074 | `	sxi64 iCostIns = 1,iCostRep = 1,iCostDel = 1;` |
|      - | 1075 | `	sxi64 *p1,*p2,*pTmp;` |
|      - | 1076 | `	sxi64 c0,c1,c2;` |
|      - | 1077 | `	ph7_value sTmp1,sTmp2;` |
|      - | 1078 | `	int nLen1,nLen2;` |
|      - | 1079 | `	int i1,i2;` |
|      - | 1080 | `	sxi32 rc;` |
|      - | 1081 | `	int i;` |
|     43 | 1082 | `	if( nArg < 2 ){` |
|      4 | 1083 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1084 | `			"ArgumentCountError",` |
|      - | 1085 | `			"levenshtein() expects at least 2 arguments, %d given",` |
|      1 | 1086 | `			nArg` |
|      - | 1087 | `			);` |
|      - | 1088 | `	}` |
|      - | 1089 | `	/* $string1/$string2: null deprecates to "", __toString objects resolve,` |
|      - | 1090 | `	 * everything non-stringish is a TypeError (php ZPP weak mode). */` |
|     41 | 1091 | `	PH7_MemObjInit(pCtx->pVm,&sTmp1);` |
|     41 | 1092 | `	PH7_MemObjInit(pCtx->pVm,&sTmp2);` |
|     41 | 1093 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"levenshtein",1,"$string1","string",` |
|      - | 1094 | `		"levenshtein(): Passing null to parameter #1 ($string1) "` |
|      - | 1095 | `		"of type string is deprecated",` |
|      - | 1096 | `		&sTmp1,&zStr1,&nLen1);` |
|     41 | 1097 | `	if( rc != PH7_OK ) goto out;` |
|     39 | 1098 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"levenshtein",2,"$string2","string",` |
|      - | 1099 | `		"levenshtein(): Passing null to parameter #2 ($string2) "` |
|      - | 1100 | `		"of type string is deprecated",` |
|      - | 1101 | `		&sTmp2,&zStr2,&nLen2);` |
|     39 | 1102 | `	if( rc != PH7_OK ) goto out;` |
|      - | 1103 | `	/* Optional integer costs */` |
|     63 | 1104 | `	for( i = 2 ; i < nArg && i < 5 ; i++ ){` |
|      - | 1105 | `		sxi64 iVal;` |
|     37 | 1106 | `		rc = IntArgResolve(pCtx,apArg[i],"levenshtein",i+1,azParam[i-2],"int",&iVal);` |
|     37 | 1107 | `		if( rc != PH7_OK ) goto out;` |
|     25 | 1108 | `		if( i == 2 ){` |
|     13 | 1109 | `			iCostIns = iVal;` |
|     19 | 1110 | `		}else if( i == 3 ){` |
|      7 | 1111 | `			iCostRep = iVal;` |
|      4 | 1112 | `		}else{` |
|      7 | 1113 | `			iCostDel = iVal;` |
|      - | 1114 | `		}` |
|     13 | 1115 | `	}` |
|     27 | 1116 | `	if( nLen1 == 0 ){` |
|      3 | 1117 | `		ph7_result_int64(pCtx,(sxi64)nLen2 * iCostIns);` |
|      3 | 1118 | `		rc = PH7_OK;` |
|      3 | 1119 | `		goto out;` |
|      - | 1120 | `	}` |
|     25 | 1121 | `	if( nLen2 == 0 ){` |
|      3 | 1122 | `		ph7_result_int64(pCtx,(sxi64)nLen1 * iCostDel);` |
|      3 | 1123 | `		rc = PH7_OK;` |
|      3 | 1124 | `		goto out;` |
|      - | 1125 | `	}` |
|      - | 1126 | `	/* Two rolling DP rows over string2 (auto-released on return). Reject a` |
|      - | 1127 | `	 * string2 long enough to overflow the 32-bit allocation size. */` |
|     23 | 1128 | `	if( (sxu32)nLen2 >= (SXU32_HIGH / sizeof(sxi64)) - 1 ){` |
|    ! 0 | 1129 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1130 | `		goto out;` |
|      - | 1131 | `	}` |
|     23 | 1132 | `	p1 = (sxi64 *)ph7_context_alloc_chunk(pCtx,(unsigned int)(sizeof(sxi64) * (sxu32)(nLen2 + 1)),FALSE,TRUE);` |
|     23 | 1133 | `	p2 = (sxi64 *)ph7_context_alloc_chunk(pCtx,(unsigned int)(sizeof(sxi64) * (sxu32)(nLen2 + 1)),FALSE,TRUE);` |
|     23 | 1134 | `	if( p1 == 0 \|\| p2 == 0 ){` |
|    ! 0 | 1135 | `		rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1136 | `		goto out;` |
|      - | 1137 | `	}` |
|    733 | 1138 | `	for( i2 = 0 ; i2 <= nLen2 ; i2++ ){` |
|    711 | 1139 | `		p1[i2] = (sxi64)i2 * iCostIns;` |
|    356 | 1140 | `	}` |
|    707 | 1141 | `	for( i1 = 0 ; i1 < nLen1 ; i1++ ){` |
|    685 | 1142 | `		p2[0] = p1[0] + iCostDel;` |
| 181111 | 1143 | `		for( i2 = 0 ; i2 < nLen2 ; i2++ ){` |
| 180427 | 1144 | `			c0 = p1[i2] + ((zStr1[i1] == zStr2[i2]) ? 0 : iCostRep);` |
| 180427 | 1145 | `			c1 = p1[i2 + 1] + iCostDel;` |
| 180427 | 1146 | `			if( c1 < c0 ){` |
|  45393 | 1147 | `				c0 = c1;` |
|  22696 | 1148 | `			}` |
| 180427 | 1149 | `			c2 = p2[i2] + iCostIns;` |
| 180427 | 1150 | `			if( c2 < c0 ){` |
|  44809 | 1151 | `				c0 = c2;` |
|  22404 | 1152 | `			}` |
| 180427 | 1153 | `			p2[i2 + 1] = c0;` |
|  90214 | 1154 | `		}` |
|    685 | 1155 | `		pTmp = p1;` |
|    685 | 1156 | `		p1 = p2;` |
|    685 | 1157 | `		p2 = pTmp;` |
|    343 | 1158 | `	}` |
|     23 | 1159 | `	ph7_result_int64(pCtx,p1[nLen2]);` |
|     23 | 1160 | `	rc = PH7_OK;` |
|     20 | 1161 | `out:` |
|     41 | 1162 | `	PH7_MemObjRelease(&sTmp1);` |
|     41 | 1163 | `	PH7_MemObjRelease(&sTmp2);` |
|     41 | 1164 | `	return rc;` |
|     22 | 1165 | `}` |
|      - | 1166 | `/*` |
|      - | 1167 | ` * Longest common substring scan behind similar_text() — a faithful port of` |
|      - | 1168 | ` * PHP's php_similar_str(): O(n*m) scan recording the first longest run.` |
|      - | 1169 | ` */` |
|     26 | 1170 | `static void SimilarStr(const char *zTxt1,int nLen1,const char *zTxt2,int nLen2,` |
|      - | 1171 | `	int *pPos1,int *pPos2,int *pMax,int *pCount)` |
|      1 | 1172 | `{` |
|      - | 1173 | `	const char *p,*q;` |
|     27 | 1174 | `	const char *zEnd1 = &zTxt1[nLen1];` |
|     27 | 1175 | `	const char *zEnd2 = &zTxt2[nLen2];` |
|      - | 1176 | `	int l;` |
|     27 | 1177 | `	*pMax = 0;` |
|     27 | 1178 | `	*pCount = 0;` |
|    143 | 1179 | `	for( p = zTxt1 ; p < zEnd1 ; p++ ){` |
|    843 | 1180 | `		for( q = zTxt2 ; q < zEnd2 ; q++ ){` |
|    999 | 1181 | `			for( l = 0 ; (p+l < zEnd1) && (q+l < zEnd2) && (p[l] == q[l]) ; l++ );` |
|    727 | 1182 | `			if( l > *pMax ){` |
|     25 | 1183 | `				*pMax = l;` |
|     25 | 1184 | `				*pCount += 1;` |
|     25 | 1185 | `				*pPos1 = (int)(p - zTxt1);` |
|     25 | 1186 | `				*pPos2 = (int)(q - zTxt2);` |
|     12 | 1187 | `			}` |
|    364 | 1188 | `		}` |
|     59 | 1189 | `	}` |
|     27 | 1190 | `}` |
|      - | 1191 | `/*` |
|      - | 1192 | ` * Recursive divide-and-conquer behind similar_text() — a faithful port of` |
|      - | 1193 | `` * PHP's php_similar_char(), including its quirky `count > 1` guard on the`` |
|      - | 1194 | ` * left-side recursion.` |
|      - | 1195 | ` */` |
|     26 | 1196 | `static int SimilarChar(const char *zTxt1,int nLen1,const char *zTxt2,int nLen2)` |
|      1 | 1197 | `{` |
|      - | 1198 | `	int nSum;` |
|     27 | 1199 | `	int nPos1 = 0,nPos2 = 0,nMax,nCount;` |
|     27 | 1200 | `	SimilarStr(zTxt1,nLen1,zTxt2,nLen2,&nPos1,&nPos2,&nMax,&nCount);` |
|     27 | 1201 | `	if( (nSum = nMax) != 0 ){` |
|     25 | 1202 | `		if( nPos1 && nPos2 && nCount > 1 ){` |
|    ! 0 | 1203 | `			nSum += SimilarChar(zTxt1,nPos1,zTxt2,nPos2);` |
|    ! 0 | 1204 | `		}` |
|     25 | 1205 | `		if( (nPos1 + nMax < nLen1) && (nPos2 + nMax < nLen2) ){` |
|     13 | 1206 | `			nSum += SimilarChar(&zTxt1[nPos1 + nMax],nLen1 - nPos1 - nMax,` |
|      8 | 1207 | `				&zTxt2[nPos2 + nMax],nLen2 - nPos2 - nMax);` |
|      4 | 1208 | `		}` |
|     12 | 1209 | `	}` |
|     27 | 1210 | `	return nSum;` |
|      1 | 1211 | `}` |
|      - | 1212 | `/*` |
|      - | 1213 | ` * int similar_text(string $string1,string $string2[,float &$percent])` |
|      - | 1214 | ` *  Calculate the similarity between two strings, as the number of matching` |
|      - | 1215 | ` *  characters found by PHP's greedy longest-common-substring recursion.` |
|      - | 1216 | ` *  When $percent is given it receives the similarity in percent:` |
|      - | 1217 | ` *  matching * 200 / (len1 + len2).` |
|      - | 1218 | ` * Return` |
|      - | 1219 | ` *  The number of matching characters in both strings.` |
|      - | 1220 | ` */` |
|     28 | 1221 | `static int PH7_builtin_similar_text(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1222 | `{` |
|      - | 1223 | `	const char *zStr1,*zStr2;` |
|      - | 1224 | `	ph7_value sTmp1,sTmp2;` |
|      - | 1225 | `	int nLen1,nLen2;` |
|      - | 1226 | `	int nSim;` |
|      - | 1227 | `	sxi32 rc;` |
|     29 | 1228 | `	if( nArg < 2 ){` |
|      4 | 1229 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1230 | `			"ArgumentCountError",` |
|      - | 1231 | `			"similar_text() expects at least 2 arguments, %d given",` |
|      1 | 1232 | `			nArg` |
|      - | 1233 | `			);` |
|      - | 1234 | `	}` |
|     27 | 1235 | `	PH7_MemObjInit(pCtx->pVm,&sTmp1);` |
|     27 | 1236 | `	PH7_MemObjInit(pCtx->pVm,&sTmp2);` |
|     27 | 1237 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"similar_text",1,"$string1","string",` |
|      - | 1238 | `		"similar_text(): Passing null to parameter #1 ($string1) "` |
|      - | 1239 | `		"of type string is deprecated",` |
|      - | 1240 | `		&sTmp1,&zStr1,&nLen1);` |
|     27 | 1241 | `	if( rc != PH7_OK ) goto out;` |
|     25 | 1242 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"similar_text",2,"$string2","string",` |
|      - | 1243 | `		"similar_text(): Passing null to parameter #2 ($string2) "` |
|      - | 1244 | `		"of type string is deprecated",` |
|      - | 1245 | `		&sTmp2,&zStr2,&nLen2);` |
|     25 | 1246 | `	if( rc != PH7_OK ) goto out;` |
|     23 | 1247 | `	if( nLen1 + nLen2 == 0 ){` |
|      5 | 1248 | `		nSim = 0;` |
|      3 | 1249 | `	}else{` |
|     19 | 1250 | `		nSim = SimilarChar(zStr1,nLen1,zStr2,nLen2);` |
|      - | 1251 | `	}` |
|     23 | 1252 | `	if( nArg > 2 ){` |
|      - | 1253 | `		/* Write the percentage through the by-ref out-param */` |
|      7 | 1254 | `		ph7_value *pPercent = ph7_context_new_scalar(pCtx);` |
|      7 | 1255 | `		if( pPercent == 0 ){` |
|    ! 0 | 1256 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1257 | `			goto out;` |
|    ! 0 | 1258 | `		}else{` |
|      7 | 1259 | `			double dPct = (nLen1 + nLen2 == 0) ? 0.0 : (double)nSim * 200.0 / (double)(nLen1 + nLen2);` |
|      7 | 1260 | `			ph7_value_double(pPercent,dPct);` |
|      7 | 1261 | `			PH7_VmStoreArgByRef(pCtx->pVm,apArg[2],pPercent);` |
|      - | 1262 | `		}` |
|      3 | 1263 | `	}` |
|     23 | 1264 | `	ph7_result_int(pCtx,nSim);` |
|     23 | 1265 | `	rc = PH7_OK;` |
|     13 | 1266 | `out:` |
|     27 | 1267 | `	PH7_MemObjRelease(&sTmp1);` |
|     27 | 1268 | `	PH7_MemObjRelease(&sTmp2);` |
|     27 | 1269 | `	return rc;` |
|     15 | 1270 | `}` |
|      - | 1271 | `/*` |
|      - | 1272 | ` * array\|int str_word_count(string $string[,int $format = 0[,?string $characters = null]])` |
|      - | 1273 | ` *  Count (or return) the words inside a string. A word is a run of alphabetic` |
|      - | 1274 | ` *  characters, which may contain (but not start the string with) "'" and "-";` |
|      - | 1275 | ` *  $characters adds extra bytes to the word set ("a..z" ranges supported, as` |
|      - | 1276 | ` *  in PHP's php_charmask).` |
|      - | 1277 | ` *  $format: 0 -> word count, 1 -> array of words, 2 -> array of words keyed` |
|      - | 1278 | ` *  by their byte position in $string.` |
|      - | 1279 | ` * Errors` |
|      - | 1280 | ` *  ValueError when $format is not 0, 1 or 2.` |
|      - | 1281 | ` */` |
|     52 | 1282 | `static int PH7_builtin_str_word_count(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1283 | `{` |
|      - | 1284 | `	const char *zIn,*zEnd,*zPtr;` |
|     53 | 1285 | `	ph7_value *pArray = 0,*pValue = 0;` |
|      - | 1286 | `	ph7_value sTmp,sListTmp;` |
|      - | 1287 | `	char aMask[256];` |
|     53 | 1288 | `	int bMask = 0;` |
|     53 | 1289 | `	int iFormat = 0;` |
|     53 | 1290 | `	int nCount = 0;` |
|      - | 1291 | `	int nLen;` |
|      - | 1292 | `	sxi32 rc;` |
|     53 | 1293 | `	if( nArg < 1 ){` |
|      4 | 1294 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1295 | `			"ArgumentCountError",` |
|      - | 1296 | `			"str_word_count() expects at least 1 argument, %d given",` |
|      1 | 1297 | `			nArg` |
|      - | 1298 | `			);` |
|      - | 1299 | `	}` |
|     51 | 1300 | `	PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|     51 | 1301 | `	PH7_MemObjInit(pCtx->pVm,&sListTmp);` |
|     51 | 1302 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_word_count",1,"$string","string",` |
|      - | 1303 | `		"str_word_count(): Passing null to parameter #1 ($string) "` |
|      - | 1304 | `		"of type string is deprecated",` |
|      - | 1305 | `		&sTmp,&zIn,&nLen);` |
|     51 | 1306 | `	if( rc != PH7_OK ) goto out;` |
|     49 | 1307 | `	if( nArg > 1 ){` |
|      - | 1308 | `		sxi64 iVal;` |
|     35 | 1309 | `		rc = IntArgResolve(pCtx,apArg[1],"str_word_count",2,"$format","int",&iVal);` |
|     37 | 1310 | `		if( rc != PH7_OK ) goto out;` |
|     33 | 1311 | `		if( iVal < 0 \|\| iVal > 2 ){` |
|      5 | 1312 | `			rc = PH7_VmThrowException(pCtx,` |
|      - | 1313 | `				"ValueError",` |
|      - | 1314 | `				"str_word_count(): Argument #2 ($format) must be a valid format value"` |
|      - | 1315 | `				);` |
|      5 | 1316 | `			goto out;` |
|      - | 1317 | `		}` |
|     29 | 1318 | `		iFormat = (int)iVal;` |
|     14 | 1319 | `	}` |
|     43 | 1320 | `	if( nArg > 2 && !ph7_value_is_null(apArg[2]) ){` |
|      - | 1321 | `		/* $characters is ?string: null (skipped above) simply keeps the` |
|      - | 1322 | `		 * default word set, no deprecation. */` |
|      - | 1323 | `		const char *zList;` |
|      - | 1324 | `		int nList;` |
|     17 | 1325 | `		rc = StrPredicateResolveArg(pCtx,apArg[2],"str_word_count",3,"$characters","?string",` |
|      - | 1326 | `			"" /* unreachable: null never gets here */,` |
|      - | 1327 | `			&sListTmp,&zList,&nList);` |
|     17 | 1328 | `		if( rc != PH7_OK ) goto out;` |
|     13 | 1329 | `		PH7_BuildCharMask(pCtx,zList,nList,aMask);` |
|     13 | 1330 | `		bMask = 1;` |
|      6 | 1331 | `	}` |
|     39 | 1332 | `	if( iFormat != 0 ){` |
|     25 | 1333 | `		pArray = ph7_context_new_array(pCtx);` |
|     25 | 1334 | `		pValue = ph7_context_new_scalar(pCtx);` |
|     25 | 1335 | `		if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 1336 | `			rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1337 | `			goto out;` |
|      - | 1338 | `		}` |
|     12 | 1339 | `	}` |
|     39 | 1340 | `	zPtr = zIn;` |
|     39 | 1341 | `	zEnd = &zIn[nLen];` |
|     39 | 1342 | `	if( nLen > 0 ){` |
|      - | 1343 | `		/* php: the string's first byte cannot be ' or -, and its last byte` |
|      - | 1344 | `		 * cannot be -, unless the charlist explicitly allows them. */` |
|     33 | 1345 | `		if( (zPtr[0] == '\'' && (!bMask \|\| !aMask[(unsigned char)'\''])) \|\|` |
|     28 | 1346 | `			(zPtr[0] == '-'  && (!bMask \|\| !aMask[(unsigned char)'-'])) ){` |
|      9 | 1347 | `			zPtr++;` |
|      4 | 1348 | `		}` |
|     33 | 1349 | `		if( zEnd[-1] == '-' && (!bMask \|\| !aMask[(unsigned char)'-']) ){` |
|      9 | 1350 | `			zEnd--;` |
|      4 | 1351 | `		}` |
|     16 | 1352 | `	}` |
|    135 | 1353 | `	while( zPtr < zEnd ){` |
|     91 | 1354 | `		const char *zStart = zPtr;` |
|    477 | 1355 | `		while( zPtr < zEnd && ( SyisAlpha((unsigned char)zPtr[0])` |
|    253 | 1356 | `			\|\| (bMask && aMask[(unsigned char)zPtr[0]])` |
|     98 | 1357 | `			\|\| zPtr[0] == '\'' \|\| zPtr[0] == '-' ) ){` |
|    339 | 1358 | `			zPtr++;` |
|      1 | 1359 | `		}` |
|     97 | 1360 | `		if( zPtr > zStart ){` |
|     91 | 1361 | `			if( iFormat == 0 ){` |
|     19 | 1362 | `				nCount++;` |
|     10 | 1363 | `			}else{` |
|     73 | 1364 | `				ph7_value_reset_string_cursor(pValue);` |
|     73 | 1365 | `				if( SXRET_OK != ph7_value_string(pValue,zStart,(int)(zPtr-zStart)) ){` |
|    ! 0 | 1366 | `					rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1367 | `					goto out;` |
|      - | 1368 | `				}` |
|     73 | 1369 | `				if( iFormat == 1 ){` |
|     59 | 1370 | `					if( SXRET_OK != ph7_array_add_elem(pArray,0,pValue) ){` |
|    ! 0 | 1371 | `						rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1372 | `						goto out;` |
|      - | 1373 | `					}` |
|     30 | 1374 | `				}else{` |
|     15 | 1375 | `					if( SXRET_OK != ph7_array_add_intkey_elem(pArray,(int)(zStart-zIn),pValue) ){` |
|    ! 0 | 1376 | `						rc = PH7_ContextMemoryError(pCtx);` |
|    ! 0 | 1377 | `						goto out;` |
|      - | 1378 | `					}` |
|      - | 1379 | `				}` |
|      - | 1380 | `			}` |
|     45 | 1381 | `		}` |
|     97 | 1382 | `		zPtr++;` |
|      1 | 1383 | `	}` |
|     37 | 1384 | `	if( iFormat == 0 ){` |
|     13 | 1385 | `		ph7_result_int(pCtx,nCount);` |
|      7 | 1386 | `	}else{` |
|     25 | 1387 | `		ph7_result_value(pCtx,pArray);` |
|      - | 1388 | `	}` |
|     37 | 1389 | `	rc = PH7_OK;` |
|     24 | 1390 | `out:` |
|     49 | 1391 | `	PH7_MemObjRelease(&sTmp);` |
|     49 | 1392 | `	PH7_MemObjRelease(&sListTmp);` |
|     49 | 1393 | `	return rc;` |
|     26 | 1394 | `}` |
|      - | 1395 | `/*` |
|      - | 1396 | ` * string chunk_split(string $body[,int $chunklen = 76 [, string $end = "\r\n" ]])` |
|      - | 1397 | ` *   Split a string into smaller chunks.` |
|      - | 1398 | ` * Parameters` |
|      - | 1399 | ` *  $body` |
|      - | 1400 | ` *   The string to be chunked.` |
|      - | 1401 | ` * $chunklen` |
|      - | 1402 | ` *   The chunk length.` |
|      - | 1403 | ` * $end` |
|      - | 1404 | ` *   The line ending sequence.` |
|      - | 1405 | ` * Return` |
|      - | 1406 | ` *  The chunked string or NULL on failure.` |
|      - | 1407 | ` */` |
|     14 | 1408 | `static int PH7_builtin_chunk_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1409 | `{` |
|     15 | 1410 | `	const char *zIn,*zEnd,*zSep = "\r\n";` |
|      - | 1411 | `	int nSepLen,nChunkLen,nLen;` |
|     15 | 1412 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1413 | `		/* Nothing to split,return null */` |
|      3 | 1414 | `		ph7_result_null(pCtx);` |
|      3 | 1415 | `		return PH7_OK;` |
|      - | 1416 | `	}` |
|      - | 1417 | `	/* initialize/Extract arguments */` |
|     13 | 1418 | `	nSepLen = (int)sizeof("\r\n") - 1;` |
|     13 | 1419 | `	nChunkLen = 76;` |
|     13 | 1420 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 1421 | `	zEnd = &zIn[nLen];` |
|     13 | 1422 | `	if( nArg > 1 ){` |
|      - | 1423 | `		/* Chunk length */` |
|     13 | 1424 | `		nChunkLen = ph7_value_to_int(apArg[1]);` |
|     13 | 1425 | `		if( nChunkLen < 1 ){` |
|      - | 1426 | `			/* PHP 8 throws a catchable ValueError for a non-positive length. */` |
|      3 | 1427 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 1428 | `				"chunk_split(): Argument #2 ($length) must be greater than 0");` |
|      - | 1429 | `		}` |
|     11 | 1430 | `		if( nArg > 2 ){` |
|      - | 1431 | `			/* Separator */` |
|      9 | 1432 | `			zSep = ph7_value_to_string(apArg[2],&nSepLen);` |
|      9 | 1433 | `			if( nSepLen < 1 ){` |
|      - | 1434 | `				/* Switch back to the default separator */` |
|      3 | 1435 | `				zSep = "\r\n";` |
|      3 | 1436 | `				nSepLen = (int)sizeof("\r\n") - 1;` |
|      1 | 1437 | `			}` |
|      4 | 1438 | `		}` |
|      5 | 1439 | `	}` |
|      - | 1440 | `	/* Perform the requested operation */` |
|     11 | 1441 | `	if( nChunkLen > nLen ){` |
|      - | 1442 | `		/* Nothing to split,return the string and the separator */` |
|      7 | 1443 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nLen,zIn,nSepLen,zSep);` |
|      7 | 1444 | `		return PH7_OK;` |
|      - | 1445 | `	}` |
|     17 | 1446 | `	while( zIn < zEnd ){` |
|     13 | 1447 | `		if( nChunkLen > (int)(zEnd-zIn) ){` |
|      3 | 1448 | `			nChunkLen = (int)(zEnd - zIn);` |
|      1 | 1449 | `		}` |
|      - | 1450 | `		/* Append the chunk and the separator */` |
|     13 | 1451 | `		ph7_result_string_format(pCtx,"%.*s%.*s",nChunkLen,zIn,nSepLen,zSep);` |
|      - | 1452 | `		/* Point beyond the chunk */` |
|     13 | 1453 | `		zIn += nChunkLen;` |
|      1 | 1454 | `	}` |
|      5 | 1455 | `	return PH7_OK;` |
|      8 | 1456 | `}` |
|      - | 1457 | `/*` |
|      - | 1458 | ` * string addslashes(string $str)` |
|      - | 1459 | ` *  Quote string with slashes.` |
|      - | 1460 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1461 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1462 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1463 | ` * Parameter` |
|      - | 1464 | ` *  str: The string to be escaped.` |
|      - | 1465 | ` * Return` |
|      - | 1466 | ` *  Returns the escaped string` |
|      - | 1467 | ` */` |
|     24 | 1468 | `static int PH7_builtin_addslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 1469 | `{` |
|      - | 1470 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1471 | `	int nLen;` |
|      - | 1472 | `	/* PHP enforces exactly one argument. */` |
|     28 | 1473 | `	if( nArg != 1 ){` |
|      8 | 1474 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1475 | `			"ArgumentCountError",` |
|      - | 1476 | `			"addslashes() expects exactly 1 argument, %d given",` |
|      2 | 1477 | `			nArg` |
|      - | 1478 | `			);` |
|      - | 1479 | `	}` |
|      - | 1480 | `	/* NULL is deprecated and treated as an empty string; other invalid` |
|      - | 1481 | `	 * types still produce a TypeError. */` |
|     22 | 1482 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 1483 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 1484 | `			E_DEPRECATED,` |
|      - | 1485 | `			"addslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - | 1486 | `			);` |
|      - | 1487 | `		/* fall through so conversion below yields empty string */` |
|      1 | 1488 | `	}` |
|      - | 1489 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     29 | 1490 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     29 | 1491 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     18 | 1492 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 1493 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1494 | `			"TypeError",` |
|      - | 1495 | `			"addslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 1496 | `			ph7_type_name(apArg[0])` |
|      - | 1497 | `			);` |
|      - | 1498 | `	}` |
|      - | 1499 | `	/* Convert to string representation first and obtain length. */` |
|     19 | 1500 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 1501 | `	if( nLen < 1 ){` |
|      - | 1502 | `		/* Return the empty string */` |
|      5 | 1503 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 1504 | `		return PH7_OK;` |
|      - | 1505 | `	}` |
|     15 | 1506 | `	zEnd = &zIn[nLen];` |
|     15 | 1507 | `	zCur = 0; /* cc warning */` |
|     20 | 1508 | `	for(;;){` |
|     41 | 1509 | `		if( zIn >= zEnd ){` |
|      - | 1510 | `			/* No more input */` |
|     15 | 1511 | `			break;` |
|      - | 1512 | `		}` |
|     27 | 1513 | `		zCur = zIn;` |
|      - | 1514 | `		/* scan until a character that needs escaping (', ", \\, or NUL) */` |
|     89 | 1515 | `		while( zIn < zEnd && zIn[0] != '\'' && zIn[0] != '"' && zIn[0] != '\\' && zIn[0] != '\0' ){` |
|     63 | 1516 | `			zIn++;` |
|      1 | 1517 | `		}` |
|     27 | 1518 | `		if( zIn > zCur ){` |
|      - | 1519 | `			/* Append raw contents */` |
|     23 | 1520 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     11 | 1521 | `		}` |
|     27 | 1522 | `		if( zIn < zEnd ){` |
|     17 | 1523 | `			int c = zIn[0];` |
|     17 | 1524 | `			if( c == '\0' ){` |
|      - | 1525 | `				/* PHP escapes NUL as "\\0" (two characters) */` |
|      3 | 1526 | `				ph7_result_string(pCtx,"\\0",2);` |
|      2 | 1527 | `			}else{` |
|     15 | 1528 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1529 | `			}` |
|      8 | 1530 | `		}` |
|     27 | 1531 | `		zIn++;` |
|      1 | 1532 | `	}` |
|     15 | 1533 | `	return PH7_OK;` |
|     16 | 1534 | `}` |
|      - | 1535 | `/*` |
|      - | 1536 | ``  * Build a 256-entry membership mask from a PHP charlist, expanding `a..z` `` |
|      - | 1537 | ` * byte ranges exactly like PHP's php_charmask(). On return aMask[c] != 0 iff` |
|      - | 1538 | ` * the byte c belongs to the set. Emits the PHP-exact warnings for the three` |
|      - | 1539 | ` * malformed-range shapes (ph7_context_throw_error_format prepends the active` |
|      - | 1540 | ` * function name, so the messages omit it); on a bad range the surrounding` |
|      - | 1541 | ` * bytes are still added and the scan never aborts. Reads only within` |
|      - | 1542 | ` * [zList, zList+nLen).` |
|      - | 1543 | ` *` |
|      - | 1544 | ` * Use ONLY for the builtins whose charlist expands ranges the way PHP's` |
|      - | 1545 | ` * php_charmask() does: trim/ltrim/rtrim/addcslashes (and quotemeta, whose set` |
|      - | 1546 | ` * is a fixed literal with no ".."). Do NOT route strspn/strcspn/strtok/strpbrk` |
|      - | 1547 | ` * through this — PHP treats their charlists literally, so expanding "a..z" here` |
|      - | 1548 | ` * would be a behavior regression plus spurious "Invalid '..'-range" warnings.` |
|      - | 1549 | ` */` |
|     90 | 1550 | `static void PH7_BuildCharMask(ph7_context *pCtx,const char *zList,int nLen,char aMask[256])` |
|      3 | 1551 | `{` |
|     93 | 1552 | `	const unsigned char *zIn  = (const unsigned char *)zList;` |
|     93 | 1553 | `	const unsigned char *zEnd = zIn + (nLen > 0 ? nLen : 0);` |
|     93 | 1554 | `	SyZero(aMask,256);` |
|    315 | 1555 | `	for( ; zIn < zEnd ; zIn++ ){` |
|    225 | 1556 | `		int c = zIn[0];` |
|    225 | 1557 | `		if( zIn + 3 < zEnd && zIn[1] == '.' && zIn[2] == '.' && zIn[3] >= c ){` |
|      - | 1558 | `			/* Valid incrementing range c..zIn[3] */` |
|     22 | 1559 | `			int hi = zIn[3],k;` |
|    386 | 1560 | `			for( k = c ; k <= hi ; k++ ){` |
|    366 | 1561 | `				aMask[k] = 1;` |
|    184 | 1562 | `			}` |
|     22 | 1563 | `			zIn += 3; /* the loop's ++ then steps past the range end */` |
|    224 | 1564 | `		}else if( zIn + 1 < zEnd && zIn[0] == '.' && zIn[1] == '.' ){` |
|      - | 1565 | `			/* Malformed range: mirror php_charmask's three diagnostics. */` |
|      - | 1566 | `			const char *zMsg;` |
|     20 | 1567 | `			if( (const unsigned char *)zList >= zIn ){` |
|      6 | 1568 | `				zMsg = "no character to the left of '..'";` |
|     18 | 1569 | `			}else if( zIn + 2 >= zEnd ){` |
|      6 | 1570 | `				zMsg = "no character to the right of '..'";` |
|     14 | 1571 | `			}else if( zIn[-1] > zIn[2] ){` |
|     12 | 1572 | `				zMsg = "'..'-range needs to be incrementing";` |
|      7 | 1573 | `			}else{` |
|    ! 0 | 1574 | `				zMsg = 0; /* catch-all (e.g. a..b..c) */` |
|      - | 1575 | `			}` |
|     20 | 1576 | `			if( zMsg ){` |
|     29 | 1577 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      9 | 1578 | `					"Invalid '..'-range, %s",zMsg);` |
|     11 | 1579 | `			}else{` |
|    ! 0 | 1580 | `				ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|      - | 1581 | `					"Invalid '..'-range");` |
|      - | 1582 | `			}` |
|      - | 1583 | `			/* Do not consume the dots: the loop's ++ steps one byte so the` |
|      - | 1584 | `			 * dots are re-scanned as literals, exactly like php_charmask. */` |
|     11 | 1585 | `		}else{` |
|    187 | 1586 | `			aMask[c] = 1;` |
|      - | 1587 | `		}` |
|    114 | 1588 | `	}` |
|     93 | 1589 | `}` |
|      - | 1590 | `/*` |
|      - | 1591 | ` * string addcslashes(string $str,string $charlist)` |
|      - | 1592 | ` *  Quote string with slashes in a C style.` |
|      - | 1593 | ` * Parameter` |
|      - | 1594 | ` *  $str:` |
|      - | 1595 | ` *    The string to be escaped.` |
|      - | 1596 | ` *  $charlist:` |
|      - | 1597 | ` *    A list of characters to be escaped. If charlist contains characters \n, \r etc.` |
|      - | 1598 | ` *    they are converted in C-like style, while other non-alphanumeric characters` |
|      - | 1599 | ` *    with ASCII codes lower than 32 and higher than 126 converted to octal representation.` |
|      - | 1600 | ` * Return` |
|      - | 1601 | ` *  Returns the escaped string.` |
|      - | 1602 | ` * Note:` |
|      - | 1603 | ` *  Character ranges [i.e: 'A..Z'] are supported (see PH7_BuildCharMask).` |
|      - | 1604 | ` */` |
|     40 | 1605 | `static int PH7_builtin_addcslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1606 | `{` |
|      - | 1607 | `	const char *zCur,*zIn,*zEnd,*zMask;` |
|      - | 1608 | `	char aMask[256];` |
|      - | 1609 | `	int nLen,nMask;` |
|      - | 1610 | `	/* PHP enforces exactly two arguments. */` |
|     45 | 1611 | `	if( nArg != 2 ){` |
|      8 | 1612 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1613 | `			"ArgumentCountError",` |
|      - | 1614 | `			"addcslashes() expects exactly 2 arguments, %d given",` |
|      2 | 1615 | `			nArg` |
|      - | 1616 | `			);` |
|      - | 1617 | `	}` |
|      - | 1618 | `	/* First argument must be a string-ish value.  NULL is deprecated and` |
|      - | 1619 | `	 * treated as the empty string (PHP 8.1). */` |
|     40 | 1620 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      - | 1621 | `		/* Emit deprecation only once, similar to PHP behaviour. */` |
|      3 | 1622 | `		PH7_VmThrowError(pCtx->pVm,0,/* iErr will be patched to 8192 below */` |
|      - | 1623 | `			E_DEPRECATED,` |
|      - | 1624 | `			"addcslashes(): Passing null to parameter #1 ($string) of type string is deprecated"` |
|      - | 1625 | `			);` |
|      - | 1626 | `		/* treat as empty string; fall through to conversion logic */` |
|     52 | 1627 | `	} else if( ph7_value_is_array(apArg[0]) \|\|` |
|     52 | 1628 | `	          ph7_value_is_object(apArg[0]) \|\|` |
|     32 | 1629 | `	          ph7_value_is_resource(apArg[0]) ){` |
|      4 | 1630 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1631 | `			"TypeError",` |
|      - | 1632 | `			"addcslashes(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 1633 | `			ph7_type_name(apArg[0])` |
|      - | 1634 | `			);` |
|      - | 1635 | `	}` |
|      - | 1636 | `	/* Second argument must be a string.  NULL is deprecated and treated as` |
|      - | 1637 | `	 * an empty mask per PHP semantics.  Arrays/objects/resources still` |
|      - | 1638 | `	 * trigger a TypeError. */` |
|     37 | 1639 | `	if( ph7_value_is_null(apArg[1]) ){` |
|      3 | 1640 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 1641 | `			E_DEPRECATED,` |
|      - | 1642 | `			"addcslashes(): Passing null to parameter #2 ($characters) of type string is deprecated"` |
|      - | 1643 | `			);` |
|      - | 1644 | `		/* allow through so it becomes empty string below */` |
|     49 | 1645 | `	} else if( ph7_value_is_array(apArg[1]) \|\|` |
|     48 | 1646 | `	          ph7_value_is_object(apArg[1]) \|\|` |
|     30 | 1647 | `	          ph7_value_is_resource(apArg[1]) ){` |
|      4 | 1648 | `		return PH7_VmThrowException(pCtx,` |
|      - | 1649 | `			"TypeError",` |
|      - | 1650 | `			"addcslashes(): Argument #2 ($characters) must be of type string, %s given",` |
|      2 | 1651 | `			ph7_type_name(apArg[1])` |
|      - | 1652 | `			);` |
|      - | 1653 | `	}` |
|      - | 1654 | `	/* Extract the string to process */` |
|     35 | 1655 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 1656 | `	/* NULL would never reach here due to the check above. */` |
|     35 | 1657 | `	if( nLen < 1 ){` |
|      - | 1658 | `		/* Empty string returns itself. */` |
|      5 | 1659 | `		ph7_result_string(pCtx,zIn,nLen);` |
|      5 | 1660 | `		return PH7_OK;` |
|      - | 1661 | `	}` |
|      - | 1662 | ``	/* Extract the desired mask and expand any `a..z` ranges into a lookup. */`` |
|     31 | 1663 | `	zMask = ph7_value_to_string(apArg[1],&nMask);` |
|     31 | 1664 | `	PH7_BuildCharMask(pCtx,zMask,nMask,aMask);` |
|     31 | 1665 | `	zEnd = &zIn[nLen];` |
|     31 | 1666 | `	zCur = 0; /* cc warning */` |
|     37 | 1667 | `	for(;;){` |
|     77 | 1668 | `		if( zIn >= zEnd ){` |
|      - | 1669 | `			/* No more input */` |
|     31 | 1670 | `			break;` |
|      - | 1671 | `		}` |
|     49 | 1672 | `		zCur = zIn;` |
|    125 | 1673 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     79 | 1674 | `			zIn++;` |
|      3 | 1675 | `		}` |
|     49 | 1676 | `		if( zIn > zCur ){` |
|      - | 1677 | `			/* Append raw contents */` |
|     43 | 1678 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|     20 | 1679 | `		}` |
|     49 | 1680 | `		if( zIn < zEnd ){` |
|      - | 1681 | `			/* Make sure we treat the byte as unsigned to avoid negative values` |
|      - | 1682 | `			 * on platforms where char is signed. */` |
|     29 | 1683 | `			int c = (unsigned char)zIn[0];` |
|      - | 1684 | `			/* Handle special C-like escapes for common control characters first.` |
|      - | 1685 | `			 * PHP outputs "\n" "\r" "\t" "\v" "\f" when those chars are` |
|      - | 1686 | `			 * in the mask. NUL is left to the octal conversion below. */` |
|     29 | 1687 | `			if( c == '\n' ){` |
|      3 | 1688 | `				ph7_result_string(pCtx,"\\n",2);` |
|     28 | 1689 | `			}else if( c == '\r' ){` |
|      3 | 1690 | `				ph7_result_string(pCtx,"\\r",2);` |
|     26 | 1691 | `			}else if( c == '\t' ){` |
|      3 | 1692 | `				ph7_result_string(pCtx,"\\t",2);` |
|     24 | 1693 | `			}else if( c == '\v' ){` |
|      3 | 1694 | `				ph7_result_string(pCtx,"\\v",2);` |
|     22 | 1695 | `			}else if( c == '\f' ){` |
|      3 | 1696 | `				ph7_result_string(pCtx,"\\f",2);` |
|     20 | 1697 | `			}else if( c > 126 \|\| (c < 32 && (!SyisAlphaNum(c)/*EBCDIC*/ && !SyisSpace(c))) ){` |
|      - | 1698 | `				/* Convert to octal.  PHP always emits three-digit zero-padded` |
|      - | 1699 | `				 * octal escapes (\001 not \1). */` |
|      7 | 1700 | `				ph7_result_string_format(pCtx,"\\%03o",c);` |
|      4 | 1701 | `			}else{` |
|     13 | 1702 | `				ph7_result_string_format(pCtx,"\\%c",c);` |
|      - | 1703 | `			}` |
|     13 | 1704 | `		}` |
|     49 | 1705 | `		zIn++;` |
|      3 | 1706 | `	}` |
|     31 | 1707 | `	return PH7_OK;` |
|     25 | 1708 | `}` |
|      - | 1709 | `/*` |
|      - | 1710 | ` * string quotemeta(string $str)` |
|      - | 1711 | ` *  Quote meta characters.` |
|      - | 1712 | ` * Parameter` |
|      - | 1713 | ` *  $str:` |
|      - | 1714 | ` *    The string to be escaped.` |
|      - | 1715 | ` * Return` |
|      - | 1716 | ` *  Returns the escaped string.` |
|      - | 1717 | `*/` |
|     10 | 1718 | `static int PH7_builtin_quotemeta(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 1719 | `{` |
|      - | 1720 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1721 | `	char aMask[256];` |
|      - | 1722 | `	int nLen;` |
|     12 | 1723 | `	if( nArg < 1 ){` |
|      - | 1724 | `		/* Nothing to process,retun NULL */` |
|    ! 0 | 1725 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1726 | `		return PH7_OK;` |
|      - | 1727 | `	}` |
|      - | 1728 | `	/* Extract the string to process */` |
|     12 | 1729 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|     12 | 1730 | `	if( nLen < 1 ){` |
|      - | 1731 | `		/* Return the empty string */` |
|      3 | 1732 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 1733 | `		return PH7_OK;` |
|      - | 1734 | `	}` |
|      - | 1735 | `	/* Fixed meta-character set (no ranges); build the lookup once. */` |
|     10 | 1736 | `	PH7_BuildCharMask(pCtx,".\\+*?[^]($)",(int)sizeof(".\\+*?[^]($)")-1,aMask);` |
|     10 | 1737 | `	zEnd = &zIn[nLen];` |
|     10 | 1738 | `	zCur = 0; /* cc warning */` |
|     22 | 1739 | `	for(;;){` |
|     46 | 1740 | `		if( zIn >= zEnd ){` |
|      - | 1741 | `			/* No more input */` |
|     10 | 1742 | `			break;` |
|      - | 1743 | `		}` |
|     38 | 1744 | `		zCur = zIn;` |
|     76 | 1745 | `		while( zIn < zEnd && !aMask[(unsigned char)zIn[0]] ){` |
|     40 | 1746 | `			zIn++;` |
|      2 | 1747 | `		}` |
|     38 | 1748 | `		if( zIn > zCur ){` |
|      - | 1749 | `			/* Append raw contents */` |
|     20 | 1750 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      9 | 1751 | `		}` |
|     38 | 1752 | `		if( zIn < zEnd ){` |
|     36 | 1753 | `			int c = zIn[0];` |
|     36 | 1754 | `			ph7_result_string_format(pCtx,"\\%c",c);` |
|     17 | 1755 | `		}` |
|     38 | 1756 | `		zIn++;` |
|      2 | 1757 | `	}` |
|     10 | 1758 | `	return PH7_OK;` |
|      7 | 1759 | `}` |
|      - | 1760 | `/*` |
|      - | 1761 | ` * string stripslashes(string $str)` |
|      - | 1762 | ` *  Un-quotes a quoted string.` |
|      - | 1763 | ` *  Returns a string with backslashes before characters that need` |
|      - | 1764 | ` *  to be quoted in database queries etc. These characters are single` |
|      - | 1765 | ` *  quote ('), double quote ("), backslash (\) and NUL (the NULL byte).` |
|      - | 1766 | ` * Parameter` |
|      - | 1767 | ` *  $str` |
|      - | 1768 | ` *   The input string.` |
|      - | 1769 | ` * Return` |
|      - | 1770 | ` *  Returns a string with backslashes stripped off.` |
|      - | 1771 | ` */` |
|      6 | 1772 | `static int PH7_builtin_stripslashes(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1773 | `{` |
|      - | 1774 | `	const char *zCur,*zIn,*zEnd;` |
|      - | 1775 | `	int nLen;` |
|      7 | 1776 | `	if( nArg < 1 ){` |
|      - | 1777 | `		/* Nothing to process,retun NULL */` |
|    ! 0 | 1778 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1779 | `		return PH7_OK;` |
|      - | 1780 | `	}` |
|      - | 1781 | `	/* Extract the string to process */` |
|      7 | 1782 | `	zIn  = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 1783 | `	if( zIn == 0 ){` |
|    ! 0 | 1784 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1785 | `		return PH7_OK;` |
|      - | 1786 | `	}` |
|      7 | 1787 | `	zEnd = &zIn[nLen];` |
|      7 | 1788 | `	zCur = 0; /* cc warning */` |
|      - | 1789 | `	/* Encode the string */` |
|      4 | 1790 | `	for(;;){` |
|      9 | 1791 | `		if( zIn >= zEnd ){` |
|      - | 1792 | `			/* No more input */` |
|      5 | 1793 | `			break;` |
|      - | 1794 | `		}` |
|      5 | 1795 | `		zCur = zIn;` |
|     17 | 1796 | `		while( zIn < zEnd && zIn[0] != '\\' ){` |
|     13 | 1797 | `			zIn++;` |
|      1 | 1798 | `		}` |
|      5 | 1799 | `		if( zIn > zCur ){` |
|      - | 1800 | `			/* Append raw contents */` |
|      5 | 1801 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 1802 | `		}` |
|      5 | 1803 | `		if( &zIn[1] < zEnd ){` |
|      3 | 1804 | `			int c = zIn[1];` |
|      3 | 1805 | `			if( c == '\'' \|\| c == '"' \|\| c == '\\' ){` |
|      - | 1806 | `				/* Ignore the backslash */` |
|      3 | 1807 | `				zIn++;` |
|      1 | 1808 | `			}` |
|      2 | 1809 | `		}else{` |
|      3 | 1810 | `			break;` |
|      - | 1811 | `		}` |
|      1 | 1812 | `	}` |
|      7 | 1813 | `	return PH7_OK;` |
|      4 | 1814 | `}` |
|      - | 1815 | `/*` |
|      - | 1816 | ` * UTF-8-aware HTML entity machinery, shared by htmlspecialchars/htmlentities/` |
|      - | 1817 | ` * htmlspecialchars_decode/html_entity_decode/get_html_translation_table.` |
|      - | 1818 | ` * The implementations live further down in this file, next to the filter_var` |
|      - | 1819 | ` * FULL_SPECIAL_CHARS machinery they reuse (aHtml401Ent[]/FvHtml401Lookup()/` |
|      - | 1820 | ` * FvUtf8Next()). Semantics are byte-exact vs php 8.5.7; PHL is UTF-8-only` |
|      - | 1821 | ` * (PLAN.md §6) so every charset argument other than a UTF-8 alias gets PHP's` |
|      - | 1822 | ` * unsupported-charset warning and is treated as UTF-8.` |
|      - | 1823 | ` *` |
|      - | 1824 | ` * Flag model (the PHP-exact ENT_* values, see constant.c): bit 1 = encode/` |
|      - | 1825 | ` * decode single quotes, bit 2 = double quotes (ENT_QUOTES=3, ENT_COMPAT=2,` |
|      - | 1826 | ` * ENT_NOQUOTES=0); bits 16\|32 select the doctype (0=HTML401, 16=XML1,` |
|      - | 1827 | ` * 32=XHTML, 48=HTML5); ENT_IGNORE=4 drops invalid UTF-8 bytes (wins over` |
|      - | 1828 | ` * ENT_SUBSTITUTE=8, which replaces each with U+FFFD; with neither set the` |
|      - | 1829 | ` * whole result collapses to ""); ENT_DISALLOWED=128 substitutes valid but` |
|      - | 1830 | ` * doctype-disallowed codepoints. The shared default is` |
|      - | 1831 | ` * ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 = 11.` |
|      - | 1832 | ` */` |
|      - | 1833 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bAll,int bDoubleEncode);` |
|      - | 1834 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,int iFlags,int bFull);` |
|      - | 1835 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx);` |
|      - | 1836 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags);` |
|      - | 1837 | `/*` |
|      - | 1838 | ` * string htmlspecialchars(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1839 | ` *                         [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1840 | ` *  Convert the special characters & < > " ' to HTML entities.` |
|      - | 1841 | ` * Return` |
|      - | 1842 | ` *  The escaped string or NULL on failure.` |
|      - | 1843 | ` */` |
|     42 | 1844 | `static int PH7_builtin_htmlspecialchars(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1845 | `{` |
|     43 | 1846 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1847 | `	const char *zIn;` |
|     43 | 1848 | `	int nLen,bDouble = 1;` |
|     43 | 1849 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1850 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 1851 | `		ph7_result_null(pCtx);` |
|      3 | 1852 | `		return PH7_OK;` |
|      - | 1853 | `	}` |
|      - | 1854 | `	/* Extract the target string */` |
|     41 | 1855 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 1856 | `	if( nArg > 1 ){` |
|     35 | 1857 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     17 | 1858 | `	}` |
|     41 | 1859 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     41 | 1860 | `	if( nArg > 3 ){` |
|      7 | 1861 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      3 | 1862 | `	}` |
|     41 | 1863 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,0,bDouble);` |
|     41 | 1864 | `	return PH7_OK;` |
|     22 | 1865 | `}` |
|      - | 1866 | `/*` |
|      - | 1867 | ` * string htmlspecialchars_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401])` |
|      - | 1868 | ` *  Convert the special HTML entities (&amp; &lt; &gt; &quot; and the` |
|      - | 1869 | ` *  numeric/doctype forms of the two quotes) back to characters.` |
|      - | 1870 | ` * Return` |
|      - | 1871 | ` *  The unescaped string or NULL on failure.` |
|      - | 1872 | ` */` |
|     22 | 1873 | `static int PH7_builtin_htmlspecialchars_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1874 | `{` |
|     23 | 1875 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1876 | `	const char *zIn;` |
|      - | 1877 | `	int nLen;` |
|     23 | 1878 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1879 | `		/* Missing/Invalid arguments,return NULL */` |
|      3 | 1880 | `		ph7_result_null(pCtx);` |
|      3 | 1881 | `		return PH7_OK;` |
|      - | 1882 | `	}` |
|      - | 1883 | `	/* Extract the target string */` |
|     21 | 1884 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     21 | 1885 | `	if( nArg > 1 ){` |
|      9 | 1886 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1887 | `	}` |
|     21 | 1888 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,0);` |
|     21 | 1889 | `	return PH7_OK;` |
|     12 | 1890 | `}` |
|      - | 1891 | `/*` |
|      - | 1892 | ` * array get_html_translation_table(int $table = HTML_SPECIALCHARS` |
|      - | 1893 | ` *      [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 [, string $encoding = "UTF-8"]])` |
|      - | 1894 | ` *  Return the translation table used by htmlspecialchars() (HTML_SPECIALCHARS)` |
|      - | 1895 | ` *  or htmlentities() (HTML_ENTITIES) as character => entity pairs.` |
|      - | 1896 | ` * Return` |
|      - | 1897 | ` *  The translation table as an array or NULL on failure.` |
|      - | 1898 | ` */` |
|     12 | 1899 | `static int PH7_builtin_get_html_translation_table(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1900 | `{` |
|     13 | 1901 | `	int iTable = 0; /* HTML_SPECIALCHARS */` |
|     13 | 1902 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|     13 | 1903 | `	if( nArg > 0 ){` |
|     11 | 1904 | `		iTable = ph7_value_to_int(apArg[0]);` |
|      5 | 1905 | `	}` |
|     13 | 1906 | `	if( nArg > 1 ){` |
|      9 | 1907 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      4 | 1908 | `	}` |
|     13 | 1909 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     13 | 1910 | `	HtmlTranslationTable(pCtx,iTable,iFlags);` |
|     13 | 1911 | `	return PH7_OK;` |
|      1 | 1912 | `}` |
|      - | 1913 | `/*` |
|      - | 1914 | ` * string htmlentities(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1915 | ` *                     [, ?string $encoding = "UTF-8" [, bool $double_encode = true]]])` |
|      - | 1916 | ` *  Convert all applicable characters to HTML entities: the specials plus` |
|      - | 1917 | ` *  every codepoint with an HTML 4.01 named entity (aHtml401Ent[]).` |
|      - | 1918 | ` * Return` |
|      - | 1919 | ` *  The encoded string or NULL on failure.` |
|      - | 1920 | ` */` |
|     30 | 1921 | `static int PH7_builtin_htmlentities(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1922 | `{` |
|     31 | 1923 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1924 | `	const char *zIn;` |
|     31 | 1925 | `	int nLen,bDouble = 1;` |
|     31 | 1926 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1927 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1928 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1929 | `		return PH7_OK;` |
|      - | 1930 | `	}` |
|      - | 1931 | `	/* Extract the target string */` |
|     31 | 1932 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 1933 | `	if( nArg > 1 ){` |
|     19 | 1934 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|      9 | 1935 | `	}` |
|     31 | 1936 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     31 | 1937 | `	if( nArg > 3 ){` |
|      3 | 1938 | `		bDouble = ph7_value_to_bool(apArg[3]);` |
|      1 | 1939 | `	}` |
|     31 | 1940 | `	HtmlEscape(pCtx,zIn,nLen,iFlags,1,bDouble);` |
|     31 | 1941 | `	return PH7_OK;` |
|     16 | 1942 | `}` |
|      - | 1943 | `/*` |
|      - | 1944 | ` * string html_entity_decode(string $string [, int $flags = ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401` |
|      - | 1945 | ` *                           [, string $encoding = "UTF-8"]])` |
|      - | 1946 | ` *  Convert HTML entities (named — case-sensitive — and numeric, decimal or` |
|      - | 1947 | ` *  hex) back to their UTF-8 characters. The reverse of htmlentities().` |
|      - | 1948 | ` * Return` |
|      - | 1949 | ` *  The decoded string or NULL on failure.` |
|      - | 1950 | ` */` |
|     58 | 1951 | `static int PH7_builtin_html_entity_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1952 | `{` |
|     59 | 1953 | `	int iFlags = PH7_ENT_DEFAULT; /* ENT_QUOTES\|ENT_SUBSTITUTE\|ENT_HTML401 */` |
|      - | 1954 | `	const char *zIn;` |
|      - | 1955 | `	int nLen;` |
|     59 | 1956 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 1957 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 1958 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1959 | `		return PH7_OK;` |
|      - | 1960 | `	}` |
|      - | 1961 | `	/* Extract the target string */` |
|     59 | 1962 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     59 | 1963 | `	if( nArg > 1 ){` |
|     27 | 1964 | `		iFlags = ph7_value_to_int(apArg[1]);` |
|     13 | 1965 | `	}` |
|     59 | 1966 | `	HtmlCheckCharset(pCtx,nArg,apArg,2);` |
|     59 | 1967 | `	HtmlUnescape(pCtx,zIn,nLen,iFlags,1);` |
|     59 | 1968 | `	return PH7_OK;` |
|     30 | 1969 | `}` |
|      - | 1970 | `/*` |
|      - | 1971 | ` * int strlen($string)` |
|      - | 1972 | ` *  return the length of the given string.` |
|      - | 1973 | ` * Parameter` |
|      - | 1974 | ` *  string: The string being measured for length.` |
|      - | 1975 | ` * Return` |
|      - | 1976 | ` *  length of the given string.` |
|      - | 1977 | ` */` |
|  11568 | 1978 | `static int PH7_builtin_strlen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 1979 | `{` |
|  11573 | 1980 | `	int iLen = 0;` |
|  11573 | 1981 | `	if( nArg > 0 ){` |
|  11573 | 1982 | `		ph7_value_to_string(apArg[0],&iLen);` |
|   5784 | 1983 | `	}` |
|      - | 1984 | `	/* String length */` |
|  11573 | 1985 | `	ph7_result_int(pCtx,iLen);` |
|  11573 | 1986 | `	return PH7_OK;` |
|      5 | 1987 | `}` |
|      - | 1988 | `/*` |
|      - | 1989 | ` * int strcmp(string $str1,string $str2)` |
|      - | 1990 | ` *  Perform a binary safe string comparison.` |
|      - | 1991 | ` * Parameter` |
|      - | 1992 | ` *  str1: The first string` |
|      - | 1993 | ` *  str2: The second string` |
|      - | 1994 | ` * Return` |
|      - | 1995 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 1996 | ` *  than str2, and 0 if they are equal.` |
|      - | 1997 | ` */` |
|     72 | 1998 | `static int PH7_builtin_strcmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 1999 | `{` |
|      - | 2000 | `	const char *z1,*z2;` |
|      - | 2001 | `	int n1,n2;` |
|      - | 2002 | `	int res;` |
|     73 | 2003 | `	if( nArg < 2 ){` |
|    ! 0 | 2004 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 2005 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 2006 | `		return PH7_OK;` |
|      - | 2007 | `	}` |
|      - | 2008 | `	/* Perform the comparison */` |
|     73 | 2009 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     73 | 2010 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     73 | 2011 | `	res = SyStrncmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2012 | `	/* Comparison result */` |
|     73 | 2013 | `	ph7_result_int(pCtx,res);` |
|     73 | 2014 | `	return PH7_OK;` |
|     37 | 2015 | `}` |
|      - | 2016 | `/*` |
|      - | 2017 | ` * int strncmp(string $str1,string $str2,int n)` |
|      - | 2018 | ` *  Perform a binary safe string comparison of the first n characters.` |
|      - | 2019 | ` * Parameter` |
|      - | 2020 | ` *  str1: The first string` |
|      - | 2021 | ` *  str2: The second string` |
|      - | 2022 | ` * Return` |
|      - | 2023 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2024 | ` *  than str2, and 0 if they are equal.` |
|      - | 2025 | ` */` |
|     18 | 2026 | `static int PH7_builtin_strncmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2027 | `{` |
|      - | 2028 | `	const char *z1,*z2;` |
|      - | 2029 | `	int res;` |
|      - | 2030 | `	int n;` |
|     19 | 2031 | `	if( nArg < 3 ){` |
|      - | 2032 | `		/* Perform a standard comparison */` |
|    ! 0 | 2033 | `		return PH7_builtin_strcmp(pCtx,nArg,apArg);` |
|      - | 2034 | `	}` |
|      - | 2035 | `	/* Desired comparison length */` |
|     19 | 2036 | `	n  = ph7_value_to_int(apArg[2]);` |
|     19 | 2037 | `	if( n < 0 ){` |
|      - | 2038 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 2039 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2040 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 2041 | `			ph7_function_name(pCtx));` |
|      - | 2042 | `	}` |
|      - | 2043 | `	/* Perform the comparison */` |
|     17 | 2044 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|     17 | 2045 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|     17 | 2046 | `	res = SyStrncmp(z1,z2,(sxu32)n);` |
|      - | 2047 | `	/* Comparison result */` |
|     17 | 2048 | `	ph7_result_int(pCtx,res);` |
|     17 | 2049 | `	return PH7_OK;` |
|     10 | 2050 | `}` |
|      - | 2051 | `/*` |
|      - | 2052 | ` * int strcasecmp(string $str1,string $str2,int n)` |
|      - | 2053 | ` *  Perform a binary safe case-insensitive string comparison.` |
|      - | 2054 | ` * Parameter` |
|      - | 2055 | ` *  str1: The first string` |
|      - | 2056 | ` *  str2: The second string` |
|      - | 2057 | ` * Return` |
|      - | 2058 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2059 | ` *  than str2, and 0 if they are equal.` |
|      - | 2060 | ` */` |
|     14 | 2061 | `static int PH7_builtin_strcasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2062 | `{` |
|      - | 2063 | `	const char *z1,*z2;` |
|      - | 2064 | `	int n1,n2;` |
|      - | 2065 | `	int res;` |
|     15 | 2066 | `	if( nArg < 2 ){` |
|    ! 0 | 2067 | `		res = nArg == 0 ? 0 : 1;` |
|    ! 0 | 2068 | `		ph7_result_int(pCtx,res);` |
|    ! 0 | 2069 | `		return PH7_OK;` |
|      - | 2070 | `	}` |
|      - | 2071 | `	/* Perform the comparison */` |
|     15 | 2072 | `	z1 = ph7_value_to_string(apArg[0],&n1);` |
|     15 | 2073 | `	z2 = ph7_value_to_string(apArg[1],&n2);` |
|     15 | 2074 | `	res = SyStrnicmp(z1,z2,(sxu32)(SXMAX(n1,n2)));` |
|      - | 2075 | `	/* Comparison result */` |
|     15 | 2076 | `	ph7_result_int(pCtx,res);` |
|     15 | 2077 | `	return PH7_OK;` |
|      8 | 2078 | `}` |
|      - | 2079 | `/*` |
|      - | 2080 | ` * int strncasecmp(string $str1,string $str2,int n)` |
|      - | 2081 | ` *  Perform a binary safe case-insensitive string comparison of the first n characters.` |
|      - | 2082 | ` * Parameter` |
|      - | 2083 | ` *  $str1: The first string` |
|      - | 2084 | ` *  $str2: The second string` |
|      - | 2085 | ` *  $len:  The length of strings to be used in the comparison.` |
|      - | 2086 | ` * Return` |
|      - | 2087 | ` *  Returns < 0 if str1 is less than str2; > 0 if str1 is greater` |
|      - | 2088 | ` *  than str2, and 0 if they are equal.` |
|      - | 2089 | ` */` |
|      8 | 2090 | `static int PH7_builtin_strncasecmp(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2091 | `{` |
|      - | 2092 | `	const char *z1,*z2;` |
|      - | 2093 | `	int res;` |
|      - | 2094 | `	int n;` |
|      9 | 2095 | `	if( nArg < 3 ){` |
|      - | 2096 | `		/* Perform a standard comparison */` |
|    ! 0 | 2097 | `		return PH7_builtin_strcasecmp(pCtx,nArg,apArg);` |
|      - | 2098 | `	}` |
|      - | 2099 | `	/* Desired comparison length */` |
|      9 | 2100 | `	n  = ph7_value_to_int(apArg[2]);` |
|      9 | 2101 | `	if( n < 0 ){` |
|      - | 2102 | `		/* PHP 8 throws a catchable ValueError for a negative length. */` |
|      4 | 2103 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2104 | `			"%s(): Argument #3 ($length) must be greater than or equal to 0",` |
|      1 | 2105 | `			ph7_function_name(pCtx));` |
|      - | 2106 | `	}` |
|      - | 2107 | `	/* Perform the comparison */` |
|      7 | 2108 | `	z1 = ph7_value_to_string(apArg[0],0);` |
|      7 | 2109 | `	z2 = ph7_value_to_string(apArg[1],0);` |
|      7 | 2110 | `	res = SyStrnicmp(z1,z2,(sxu32)n);` |
|      - | 2111 | `	/* Comparison result */` |
|      7 | 2112 | `	ph7_result_int(pCtx,res);` |
|      7 | 2113 | `	return PH7_OK;` |
|      5 | 2114 | `}` |
|      - | 2115 | `/*` |
|      - | 2116 | ` * Implode context [i.e: it's private data].` |
|      - | 2117 | ` * A pointer to the following structure is forwarded` |
|      - | 2118 | ` * verbatim to the array walker callback defined below.` |
|      - | 2119 | ` */` |
|      - | 2120 | `struct implode_data {` |
|      - | 2121 | `	ph7_context *pCtx;    /* Call context */` |
|      - | 2122 | `	int bRecursive;       /* TRUE if recursive implode [this is a symisc eXtension] */` |
|      - | 2123 | `	const char *zSep;     /* Arguments separator if any */` |
|      - | 2124 | `	int nSeplen;          /* Separator length */` |
|      - | 2125 | `	int bFirst;           /* TRUE if first call */` |
|      - | 2126 | `	int nRecCount;        /* Recursion count to avoid infinite loop */` |
|      - | 2127 | `	sxi32 rc;             /* Captured allocation rc; SXERR_MEM => the builtin raises an OOM fatal */` |
|      - | 2128 | `};` |
|      - | 2129 | `/*` |
|      - | 2130 | ` * Implode walker callback for the [ph7_array_walk()] interface.` |
|      - | 2131 | ` * The following routine is invoked for each array entry passed` |
|      - | 2132 | ` * to the implode() function.` |
|      - | 2133 | ` */` |
| 141452 | 2134 | `static int implode_callback(ph7_value *pKey,ph7_value *pValue,void *pUserData)` |
|      5 | 2135 | `{` |
|  70726 | 2136 | `	SXUNUSED(pKey);` |
| 141457 | 2137 | `	struct implode_data *pData = (struct implode_data *)pUserData;` |
|      - | 2138 | `	const char *zData;` |
|      - | 2139 | `	int nLen;` |
| 141457 | 2140 | `	if( pData->bRecursive && ph7_value_is_array(pValue) && pData->nRecCount < 32 ){` |
|      3 | 2141 | `		if( pData->nSeplen > 0 ){` |
|      3 | 2142 | `			if( !pData->bFirst ){` |
|      - | 2143 | `				/* append the separator first */` |
|      3 | 2144 | `				if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 2145 | `					pData->rc = SXERR_MEM;` |
|    ! 0 | 2146 | `					return PH7_ABORT;` |
|      - | 2147 | `				}` |
|      2 | 2148 | `			}else{` |
|    ! 0 | 2149 | `				pData->bFirst = 0;` |
|      - | 2150 | `			}` |
|      1 | 2151 | `		}` |
|      - | 2152 | `		/* Recurse */` |
|      3 | 2153 | `		pData->bFirst = 1;` |
|      3 | 2154 | `		pData->nRecCount++;` |
|      3 | 2155 | `		PH7_HashmapWalk((ph7_hashmap *)pValue->x.pOther,implode_callback,pData);` |
|      3 | 2156 | `		pData->nRecCount--;` |
|      - | 2157 | `		/* Propagate an allocation failure surfaced deeper in the recursion. */` |
|      3 | 2158 | `		if( pData->rc != SXRET_OK ){` |
|    ! 0 | 2159 | `			return PH7_ABORT;` |
|      - | 2160 | `		}` |
|      3 | 2161 | `		return PH7_OK;` |
|      - | 2162 | `	}` |
|      - | 2163 | `	/* Extract the string representation of the entry value */` |
| 141455 | 2164 | `	zData = ph7_value_to_string(pValue,&nLen);` |
|      - | 2165 | `	/* Manage separator insertion: always mark first seen; append separator for subsequent items */` |
| 141455 | 2166 | `	if( pData->bFirst ){` |
|  33089 | 2167 | `		pData->bFirst = 0;` |
| 124913 | 2168 | `	}else if( pData->nSeplen > 0 ){` |
|      - | 2169 | `		/* append the separator first */` |
| 108359 | 2170 | `		if( ph7_result_string(pData->pCtx,pData->zSep,pData->nSeplen) != SXRET_OK ){` |
|    ! 0 | 2171 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 2172 | `			return PH7_ABORT;` |
|      - | 2173 | `		}` |
|  54177 | 2174 | `	}` |
|      - | 2175 | `	/* Append the value if non-empty; empty values are represented by the separators */` |
| 141455 | 2176 | `	if( nLen > 0 ){` |
| 129373 | 2177 | `		if( ph7_result_string(pData->pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2178 | `			pData->rc = SXERR_MEM;` |
|    ! 0 | 2179 | `			return PH7_ABORT;` |
|      - | 2180 | `		}` |
|  64684 | 2181 | `	}` |
| 141455 | 2182 | `	return PH7_OK;` |
|  70731 | 2183 | `}` |
|      - | 2184 | `/*` |
|      - | 2185 | ` * string implode(string $glue,array $pieces,...)` |
|      - | 2186 | ` * string implode(array $pieces,...)` |
|      - | 2187 | ` *  Join array elements with a string.` |
|      - | 2188 | ` * $glue` |
|      - | 2189 | ` *   Defaults to an empty string. This is not the preferred usage of implode() as glue` |
|      - | 2190 | ` *   would be the second parameter and thus, the bad prototype would be used.` |
|      - | 2191 | ` * $pieces` |
|      - | 2192 | ` *   The array of strings to implode.` |
|      - | 2193 | ` * Return` |
|      - | 2194 | ` *  Returns a string containing a string representation of all the array elements in the same` |
|      - | 2195 | ` *  order, with the glue string between each element.` |
|      - | 2196 | ` */` |
|  33110 | 2197 | `static int PH7_builtin_implode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2198 | `{` |
|      - | 2199 | `	struct implode_data imp_data;` |
|  33115 | 2200 | `	int i = 1;` |
|  33115 | 2201 | `	if( nArg < 1 ){` |
|      - | 2202 | `		/* Missing argument,return NULL */` |
|    ! 0 | 2203 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2204 | `		return PH7_OK;` |
|      - | 2205 | `	}` |
|      - | 2206 | `	/* Prepare the implode context */` |
|  33115 | 2207 | `	imp_data.pCtx = pCtx;` |
|  33115 | 2208 | `	imp_data.bRecursive = 0;` |
|  33115 | 2209 | `	imp_data.bFirst = 1;` |
|  33115 | 2210 | `	imp_data.nRecCount = 0;` |
|  33115 | 2211 | `	imp_data.rc = SXRET_OK;` |
|  33115 | 2212 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|  33113 | 2213 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|  16559 | 2214 | `	}else{` |
|      3 | 2215 | `		imp_data.zSep = 0;` |
|      3 | 2216 | `		imp_data.nSeplen = 0;` |
|      3 | 2217 | `		i = 0;` |
|      - | 2218 | `	}` |
|  33115 | 2219 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2220 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2221 | `	}` |
|      - | 2222 | `	/* Start the 'join' process */` |
|  66225 | 2223 | `	while( i < nArg ){` |
|  33115 | 2224 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2225 | `			/* Iterate throw array entries */` |
|  33115 | 2226 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2227 | `			/* Surface a callback allocation failure as a fatal */` |
|  33115 | 2228 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2229 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2230 | `			}` |
|  16560 | 2231 | `		}else{` |
|      - | 2232 | `			const char *zData;` |
|      - | 2233 | `			int nLen;` |
|      - | 2234 | `			/* Extract the string representation of the ph7 value */` |
|    ! 0 | 2235 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2236 | `			/* Manage separator insertion regardless of string length */` |
|    ! 0 | 2237 | `			if( imp_data.bFirst ){` |
|    ! 0 | 2238 | `				imp_data.bFirst = 0;` |
|    ! 0 | 2239 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2240 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2241 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2242 | `				}` |
|    ! 0 | 2243 | `			}` |
|      - | 2244 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|    ! 0 | 2245 | `			if( nLen > 0 ){` |
|    ! 0 | 2246 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2247 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2248 | `				}` |
|    ! 0 | 2249 | `			}` |
|      - | 2250 | `		}` |
|  33115 | 2251 | `		i++;` |
|      5 | 2252 | `	}` |
|  33115 | 2253 | `	return PH7_OK;` |
|  16560 | 2254 | `}` |
|      - | 2255 | `/*` |
|      - | 2256 | ` * Symisc eXtension:` |
|      - | 2257 | ` * string implode_recursive(string $glue,array $pieces,...)` |
|      - | 2258 | ` * Purpose` |
|      - | 2259 | ` *  Same as implode() but recurse on arrays.` |
|      - | 2260 | ` * Example:` |
|      - | 2261 | ` *   $a = array('usr',array('home','dean'));` |
|      - | 2262 | ` *   echo implode_recursive("/",$a);` |
|      - | 2263 | ` *   Will output` |
|      - | 2264 | ` *     usr/home/dean.` |
|      - | 2265 | ` *   While the standard implode would produce.` |
|      - | 2266 | ` *    usr/Array.` |
|      - | 2267 | ` * Parameter` |
|      - | 2268 | ` *  Refer to implode().` |
|      - | 2269 | ` * Return` |
|      - | 2270 | ` *  Refer to implode().` |
|      - | 2271 | ` */` |
|     12 | 2272 | `static int PH7_builtin_implode_recursive(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2273 | `{` |
|      - | 2274 | `	struct implode_data imp_data;` |
|     13 | 2275 | `	int i = 1;` |
|     13 | 2276 | `	if( nArg < 1 ){` |
|      - | 2277 | `		/* Missing argument,return NULL */` |
|      3 | 2278 | `		ph7_result_null(pCtx);` |
|      3 | 2279 | `		return PH7_OK;` |
|      - | 2280 | `	}` |
|      - | 2281 | `	/* Prepare the implode context */` |
|     11 | 2282 | `	imp_data.pCtx = pCtx;` |
|     11 | 2283 | `	imp_data.bRecursive = 1;` |
|     11 | 2284 | `	imp_data.bFirst = 1;` |
|     11 | 2285 | `	imp_data.nRecCount = 0;` |
|     11 | 2286 | `	imp_data.rc = SXRET_OK;` |
|     11 | 2287 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     11 | 2288 | `		imp_data.zSep = ph7_value_to_string(apArg[0],&imp_data.nSeplen);` |
|      6 | 2289 | `	}else{` |
|    ! 0 | 2290 | `		imp_data.zSep = 0;` |
|    ! 0 | 2291 | `		imp_data.nSeplen = 0;` |
|    ! 0 | 2292 | `		i = 0;` |
|      - | 2293 | `	}` |
|     11 | 2294 | `	if( ph7_result_string(pCtx,"",0) != SXRET_OK ){ /* Set an empty stirng */` |
|    ! 0 | 2295 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2296 | `	}` |
|      - | 2297 | `	/* Start the 'join' process */` |
|     21 | 2298 | `	while( i < nArg ){` |
|     11 | 2299 | `		if( ph7_value_is_array(apArg[i]) ){` |
|      - | 2300 | `			/* Iterate throw array entries */` |
|      3 | 2301 | `			ph7_array_walk(apArg[i],implode_callback,&imp_data);` |
|      - | 2302 | `			/* Surface a callback allocation failure as a fatal */` |
|      3 | 2303 | `			if( imp_data.rc != SXRET_OK ){` |
|    ! 0 | 2304 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2305 | `			}` |
|      2 | 2306 | `		}else{` |
|      - | 2307 | `			const char *zData;` |
|      - | 2308 | `			int nLen;` |
|      - | 2309 | `			/* Extract the string representation of the ph7 value */` |
|      9 | 2310 | `			zData = ph7_value_to_string(apArg[i],&nLen);` |
|      - | 2311 | `			/* Manage separator insertion regardless of string length */` |
|      9 | 2312 | `			if( imp_data.bFirst ){` |
|      9 | 2313 | `				imp_data.bFirst = 0;` |
|      4 | 2314 | `			}else if( imp_data.nSeplen > 0 ){` |
|    ! 0 | 2315 | `				if( ph7_result_string(pCtx, imp_data.zSep, imp_data.nSeplen) != SXRET_OK ){` |
|    ! 0 | 2316 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2317 | `				}` |
|    ! 0 | 2318 | `			}` |
|      - | 2319 | `			/* Append the value if non-empty; empty values are represented by the separators */` |
|      9 | 2320 | `			if( nLen > 0 ){` |
|      9 | 2321 | `				if( ph7_result_string(pCtx,zData,nLen) != SXRET_OK ){` |
|    ! 0 | 2322 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2323 | `				}` |
|      4 | 2324 | `			}` |
|      - | 2325 | `		}` |
|     11 | 2326 | `		i++;` |
|      1 | 2327 | `	}` |
|     11 | 2328 | `	return PH7_OK;` |
|      7 | 2329 | `}` |
|      - | 2330 | `/*` |
|      - | 2331 | ` * array explode(string $delimiter,string $string[,int $limit ])` |
|      - | 2332 | ` *  Returns an array of strings, each of which is a substring of string` |
|      - | 2333 | ` *  formed by splitting it on boundaries formed by the string delimiter.` |
|      - | 2334 | ` * Parameters` |
|      - | 2335 | ` *  $delimiter` |
|      - | 2336 | ` *   The boundary string.` |
|      - | 2337 | ` * $string` |
|      - | 2338 | ` *   The input string.` |
|      - | 2339 | ` * $limit` |
|      - | 2340 | ` *   If limit is set and positive, the returned array will contain a maximum` |
|      - | 2341 | ` *   of limit elements with the last element containing the rest of string.` |
|      - | 2342 | ` *   If the limit parameter is negative, all fields except the last -limit are returned.` |
|      - | 2343 | ` *   If the limit parameter is zero, then this is treated as 1.` |
|      - | 2344 | ` * Returns` |
|      - | 2345 | ` *  Returns an array of strings created by splitting the string parameter` |
|      - | 2346 | ` *  on boundaries formed by the delimiter.` |
|      - | 2347 | ` *  If delimiter is an empty string (""), explode() will return FALSE.` |
|      - | 2348 | ` *  If delimiter contains a value that is not contained in string and a negative` |
|      - | 2349 | ` *  limit is used, then an empty array will be returned, otherwise an array containing string` |
|      - | 2350 | ` *  will be returned.` |
|      - | 2351 | ` * NOTE:` |
|      - | 2352 | ` *  Negative limit is not supported.` |
|      - | 2353 | ` */` |
|   6444 | 2354 | `static int PH7_builtin_explode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2355 | `{` |
|      - | 2356 | `	const char *zDelim,*zString,*zCur,*zEnd;` |
|      - | 2357 | `	int nDelim,nStrlen,iLimit;` |
|      - | 2358 | `	ph7_value *pArray;` |
|      - | 2359 | `	ph7_value *pValue;` |
|      - | 2360 | `	sxu32 nOfft;` |
|      - | 2361 | `	sxi32 rc;` |
|   6449 | 2362 | `	if( nArg < 2 ){` |
|      - | 2363 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 2364 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2365 | `		return PH7_OK;` |
|      - | 2366 | `	}` |
|      - | 2367 | `	/* Extract the delimiter */` |
|   6449 | 2368 | `	zDelim = ph7_value_to_string(apArg[0],&nDelim);` |
|   6449 | 2369 | `	if( nDelim < 1 ){` |
|      - | 2370 | `		/* Empty delimiter: PHP 8 throws a catchable ValueError. */` |
|      5 | 2371 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 2372 | `			"explode(): Argument #1 ($separator) must not be empty");` |
|      - | 2373 | `	}` |
|      - | 2374 | `	/* Extract the string */` |
|   6445 | 2375 | `	zString = ph7_value_to_string(apArg[1],&nStrlen);` |
|   6445 | 2376 | `	if( nStrlen < 1 ){` |
|      - | 2377 | `		/* Empty string: normally an array with a single empty element (PHP behavior).` |
|      - | 2378 | `		 * A negative limit drops the last -limit components, so the sole empty` |
|      - | 2379 | `		 * component is dropped and the result is an empty array. */` |
|      7 | 2380 | `		ph7_value *pArrayTmp = ph7_context_new_array(pCtx);` |
|      7 | 2381 | `		if( pArrayTmp == 0 ){` |
|      - | 2382 | `			/* Out of memory,return FALSE */` |
|    ! 0 | 2383 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 2384 | `			return PH7_OK;` |
|      - | 2385 | `		}` |
|      7 | 2386 | `		if( !(nArg > 2 && ph7_value_to_int(apArg[2]) < 0) ){` |
|      5 | 2387 | `			ph7_value *pValueTmp = ph7_context_new_scalar(pCtx);` |
|      5 | 2388 | `			if( pValueTmp == 0 ){` |
|      - | 2389 | `				/* Out of memory,return FALSE */` |
|    ! 0 | 2390 | `				ph7_result_bool(pCtx,0);` |
|    ! 0 | 2391 | `				return PH7_OK;` |
|      - | 2392 | `			}` |
|      5 | 2393 | `			ph7_value_string(pValueTmp, "", 0);` |
|      5 | 2394 | `			if( ph7_array_add_elem(pArrayTmp, 0 /* Automatic index assign */, pValueTmp) != SXRET_OK ){` |
|    ! 0 | 2395 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2396 | `			}` |
|      2 | 2397 | `		}` |
|      7 | 2398 | `		ph7_result_value(pCtx, pArrayTmp);` |
|      7 | 2399 | `		return PH7_OK;` |
|      - | 2400 | `	}` |
|      - | 2401 | `	/* Point to the end of the string */` |
|   6439 | 2402 | `	zEnd = &zString[nStrlen];` |
|      - | 2403 | `	/* Create the array */` |
|   6439 | 2404 | `	pArray =  ph7_context_new_array(pCtx);` |
|   6439 | 2405 | `	pValue = ph7_context_new_scalar(pCtx);` |
|   6439 | 2406 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|      - | 2407 | `		/* Out of memory,return FALSE */` |
|    ! 0 | 2408 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 2409 | `		return PH7_OK;` |
|      - | 2410 | `	}` |
|      - | 2411 | `	/* Set a defualt limit */` |
|   6439 | 2412 | `	iLimit = SXI32_HIGH;` |
|   6439 | 2413 | `	if( nArg > 2 ){` |
|     38 | 2414 | `		iLimit = ph7_value_to_int(apArg[2]);` |
|     38 | 2415 | `		if( iLimit < 0 ){` |
|      - | 2416 | `			/* Negative limit: keep all components except the last -iLimit (PHP).` |
|      - | 2417 | `			 * Pre-count the components (delimiters + 1), then emit only the first` |
|      - | 2418 | `			 * nKeep CLEAN components — no trailing-remainder merge (the difference` |
|      - | 2419 | `			 * from the positive path). nKeep <= 0 drops everything -> empty array. */` |
|     17 | 2420 | `			int nTotal = 1,nKeep;` |
|     17 | 2421 | `			const char *zScan = zString;` |
|      - | 2422 | `			sxu32 nScanOfft;` |
|     57 | 2423 | `			while( SyBlobSearch(zScan,(sxu32)(zEnd - zScan),zDelim,nDelim,&nScanOfft) == SXRET_OK ){` |
|     41 | 2424 | `				nTotal++;` |
|     41 | 2425 | `				zScan = &zScan[nScanOfft + nDelim];` |
|      1 | 2426 | `			}` |
|     17 | 2427 | `			nKeep = nTotal + iLimit; /* iLimit < 0, so this is nTotal - (-iLimit) */` |
|     49 | 2428 | `			while( nKeep > (int)ph7_array_count(pArray)` |
|     39 | 2429 | `				&& SyBlobSearch(zString,(sxu32)(zEnd - zString),zDelim,nDelim,&nOfft) == SXRET_OK ){` |
|      - | 2430 | `				/* Emit the next clean component */` |
|     23 | 2431 | `				zCur = &zString[nOfft];` |
|     23 | 2432 | `				ph7_value_string(pValue, zString, (int)(zCur - zString));` |
|     23 | 2433 | `				if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2434 | `					return PH7_ContextMemoryError(pCtx);` |
|      - | 2435 | `				}` |
|     23 | 2436 | `				zString = &zCur[nDelim];` |
|     23 | 2437 | `				ph7_value_reset_string_cursor(pValue);` |
|      1 | 2438 | `			}` |
|     17 | 2439 | `			ph7_result_value(pCtx,pArray);` |
|     17 | 2440 | `			return PH7_OK;` |
|      - | 2441 | `		}` |
|     22 | 2442 | `		if( iLimit == 0 ){` |
|      5 | 2443 | `			iLimit = 1;` |
|      2 | 2444 | `		}` |
|     22 | 2445 | `		iLimit--;` |
|      9 | 2446 | `	}` |
|      - | 2447 | `	/* Start exploding */` |
|  76555 | 2448 | `	for(;;){` |
| 153115 | 2449 | `		rc = SyBlobSearch(zString,(sxu32)(zEnd-zString),zDelim,nDelim,&nOfft);` |
| 153115 | 2450 | `		if( rc != SXRET_OK \|\| iLimit <= (int)ph7_array_count(pArray) ){` |
|      - | 2451 | `			/* Limit reached or no more delimiter; insert the rest (may be empty) and break */` |
|   6423 | 2452 | `			ph7_value_string(pValue, zString, (int)(zEnd - zString));` |
|   6423 | 2453 | `			if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2454 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 2455 | `			}` |
|   6423 | 2456 | `			break;` |
|      - | 2457 | `		}` |
|      - | 2458 | `		/* Point to the desired offset */` |
| 146697 | 2459 | `		zCur = &zString[nOfft];` |
|      - | 2460 | `		/* Perform the store operation (may be empty) */` |
| 146697 | 2461 | `		ph7_value_string(pValue, zString, (int)(zCur - zString));` |
| 146697 | 2462 | `		if( ph7_array_add_elem(pArray, 0/* Automatic index assign */, pValue) != SXRET_OK ){` |
|    ! 0 | 2463 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2464 | `		}` |
|      - | 2465 | `		/* Point beyond the delimiter */` |
| 146697 | 2466 | `		zString = &zCur[nDelim];` |
|      - | 2467 | `		/* Reset the cursor */` |
| 146697 | 2468 | `		ph7_value_reset_string_cursor(pValue);` |
|      5 | 2469 | `	}` |
|      - | 2470 | `	/* Return the freshly created array */` |
|   6423 | 2471 | `	ph7_result_value(pCtx,pArray);` |
|      - | 2472 | `	/* NOTE that every allocated ph7_value will be automatically` |
|      - | 2473 | `	 * released as soon we return from this foregin function.` |
|      - | 2474 | `	 */` |
|   6423 | 2475 | `	return PH7_OK;` |
|   3227 | 2476 | `}` |
|      - | 2477 | `/*` |
|      - | 2478 | ` * string trim(string $str[,string $charlist ])` |
|      - | 2479 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2480 | ` * Parameters` |
|      - | 2481 | ` *  $str` |
|      - | 2482 | ` *   The string that will be trimmed.` |
|      - | 2483 | ` * $charlist` |
|      - | 2484 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2485 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2486 | ` *   With .. you can specify a range of characters.` |
|      - | 2487 | ` * Returns.` |
|      - | 2488 | ` *  Thr processed string.` |
|      - | 2489 | ` * NOTE:` |
|      - | 2490 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2491 | ` */` |
|  14542 | 2492 | `static int PH7_builtin_trim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2493 | `{` |
|      - | 2494 | `	const char *zString;` |
|      - | 2495 | `	int nLen;` |
|  14547 | 2496 | `	if( nArg < 1 ){` |
|      - | 2497 | `		/* Missing arguments,return null */` |
|    ! 0 | 2498 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2499 | `		return PH7_OK;` |
|      - | 2500 | `	}` |
|      - | 2501 | `	/* Extract the target string */` |
|  14547 | 2502 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  14547 | 2503 | `	if( nLen < 1 ){` |
|      - | 2504 | `		/* Empty string,return */` |
|   1305 | 2505 | `		ph7_result_string(pCtx,"",0);` |
|   1305 | 2506 | `		return PH7_OK;` |
|      - | 2507 | `	}` |
|      - | 2508 | `	/* Start the trim process */` |
|  13247 | 2509 | `	if( nArg < 2 ){` |
|      - | 2510 | `		SyString sStr;` |
|      - | 2511 | `		/* Remove white spaces and NUL bytes */` |
|  13217 | 2512 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|  33157 | 2513 | `		SyStringFullTrimSafe(&sStr);` |
|  13217 | 2514 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|   6611 | 2515 | `	}else{` |
|      - | 2516 | `		/* Char list */` |
|      - | 2517 | `		const char *zList;` |
|      - | 2518 | `		int nListlen;` |
|     33 | 2519 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     33 | 2520 | `		if( nListlen < 1 ){` |
|      - | 2521 | `			/* Return the string unchanged */` |
|      6 | 2522 | `			ph7_result_string(pCtx,zString,nLen);` |
|      4 | 2523 | `		}else{` |
|      - | 2524 | `			char aMask[256];` |
|     29 | 2525 | `			const char *zEnd = &zString[nLen];` |
|     29 | 2526 | `			const char *zCur = zString;` |
|     29 | 2527 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2528 | `			/* Left trim */` |
|     79 | 2529 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     53 | 2530 | `				zCur++;` |
|      3 | 2531 | `			}` |
|      - | 2532 | `			/* Right trim */` |
|     79 | 2533 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     53 | 2534 | `				zEnd--;` |
|      3 | 2535 | `			}` |
|     29 | 2536 | `			if( zCur >= zEnd ){` |
|      - | 2537 | `				/* Return the empty string */` |
|    ! 0 | 2538 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2539 | `			}else{` |
|     29 | 2540 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2541 | `			}` |
|      - | 2542 | `		}` |
|      - | 2543 | `	}` |
|  13247 | 2544 | `	return PH7_OK;` |
|   7276 | 2545 | `}` |
|      - | 2546 | `/*` |
|      - | 2547 | ` * string rtrim(string $str[,string $charlist ])` |
|      - | 2548 | ` *  Strip whitespace (or other characters) from the end of a string.` |
|      - | 2549 | ` * Parameters` |
|      - | 2550 | ` *  $str` |
|      - | 2551 | ` *   The string that will be trimmed.` |
|      - | 2552 | ` * $charlist` |
|      - | 2553 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2554 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2555 | ` *   With .. you can specify a range of characters.` |
|      - | 2556 | ` * Returns.` |
|      - | 2557 | ` *  Thr processed string.` |
|      - | 2558 | ` * NOTE:` |
|      - | 2559 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2560 | ` */` |
|     28 | 2561 | `static int PH7_builtin_rtrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2562 | `{` |
|      - | 2563 | `	const char *zString;` |
|      - | 2564 | `	int nLen;` |
|     31 | 2565 | `	if( nArg < 1 ){` |
|      - | 2566 | `		/* Missing arguments,return null */` |
|    ! 0 | 2567 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2568 | `		return PH7_OK;` |
|      - | 2569 | `	}` |
|      - | 2570 | `	/* Extract the target string */` |
|     31 | 2571 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     31 | 2572 | `	if( nLen < 1 ){` |
|      - | 2573 | `		/* Empty string,return */` |
|      5 | 2574 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 2575 | `		return PH7_OK;` |
|      - | 2576 | `	}` |
|      - | 2577 | `	/* Start the trim process */` |
|     27 | 2578 | `	if( nArg < 2 ){` |
|      - | 2579 | `		SyString sStr;` |
|      - | 2580 | `		/* Remove white spaces and NUL bytes*/` |
|     17 | 2581 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|     41 | 2582 | `		SyStringRightTrimSafe(&sStr);` |
|     17 | 2583 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      9 | 2584 | `	}else{` |
|      - | 2585 | `		/* Char list */` |
|      - | 2586 | `		const char *zList;` |
|      - | 2587 | `		int nListlen;` |
|     11 | 2588 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     11 | 2589 | `		if( nListlen < 1 ){` |
|      - | 2590 | `			/* Return the string unchanged */` |
|    ! 0 | 2591 | `			ph7_result_string(pCtx,zString,nLen);` |
|    ! 0 | 2592 | `		}else{` |
|      - | 2593 | `			char aMask[256];` |
|     11 | 2594 | `			const char *zEnd = &zString[nLen];` |
|     11 | 2595 | `			const char *zCur = zString;` |
|     11 | 2596 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2597 | `			/* Right trim */` |
|     29 | 2598 | `			while( zEnd > zCur && aMask[(unsigned char)zEnd[-1]] ){` |
|     20 | 2599 | `				zEnd--;` |
|      2 | 2600 | `			}` |
|     11 | 2601 | `			if( zEnd <= zCur ){` |
|      - | 2602 | `				/* Return the empty string */` |
|    ! 0 | 2603 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2604 | `			}else{` |
|     11 | 2605 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2606 | `			}` |
|      - | 2607 | `		}` |
|      - | 2608 | `	}` |
|     27 | 2609 | `	return PH7_OK;` |
|     17 | 2610 | `}` |
|      - | 2611 | `/*` |
|      - | 2612 | ` * string ltrim(string $str[,string $charlist ])` |
|      - | 2613 | ` *  Strip whitespace (or other characters) from the beginning and end of a string.` |
|      - | 2614 | ` * Parameters` |
|      - | 2615 | ` *  $str` |
|      - | 2616 | ` *   The string that will be trimmed.` |
|      - | 2617 | ` * $charlist` |
|      - | 2618 | ` *   Optionally, the stripped characters can also be specified using the charlist parameter.` |
|      - | 2619 | ` *   Simply list all characters that you want to be stripped.` |
|      - | 2620 | ` *   With .. you can specify a range of characters.` |
|      - | 2621 | ` * Returns.` |
|      - | 2622 | ` *  Thr processed string.` |
|      - | 2623 | ` * NOTE:` |
|      - | 2624 | ` *   Character ranges [i.e: 'a..z'] are supported (see PH7_BuildCharMask).` |
|      - | 2625 | ` */` |
|     12 | 2626 | `static int PH7_builtin_ltrim(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 2627 | `{` |
|      - | 2628 | `	const char *zString;` |
|      - | 2629 | `	int nLen;` |
|     14 | 2630 | `	if( nArg < 1 ){` |
|      - | 2631 | `		/* Missing arguments,return null */` |
|    ! 0 | 2632 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2633 | `		return PH7_OK;` |
|      - | 2634 | `	}` |
|      - | 2635 | `	/* Extract the target string */` |
|     14 | 2636 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     14 | 2637 | `	if( nLen < 1 ){` |
|      - | 2638 | `		/* Empty string,return */` |
|    ! 0 | 2639 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2640 | `		return PH7_OK;` |
|      - | 2641 | `	}` |
|      - | 2642 | `	/* Start the trim process */` |
|     14 | 2643 | `	if( nArg < 2 ){` |
|      - | 2644 | `		SyString sStr;` |
|      - | 2645 | `		/* Remove white spaces and NUL byte */` |
|      3 | 2646 | `		SyStringInitFromBuf(&sStr,zString,nLen);` |
|      8 | 2647 | `		SyStringLeftTrimSafe(&sStr);` |
|      3 | 2648 | `		ph7_result_string(pCtx,sStr.zString,(int)sStr.nByte);` |
|      2 | 2649 | `	}else{` |
|      - | 2650 | `		/* Char list */` |
|      - | 2651 | `		const char *zList;` |
|      - | 2652 | `		int nListlen;` |
|     12 | 2653 | `		zList = ph7_value_to_string(apArg[1],&nListlen);` |
|     12 | 2654 | `		if( nListlen < 1 ){` |
|      - | 2655 | `			/* Return the string unchanged */` |
|      3 | 2656 | `			ph7_result_string(pCtx,zString,nLen);` |
|      2 | 2657 | `		}else{` |
|      - | 2658 | `			char aMask[256];` |
|     10 | 2659 | `			const char *zEnd = &zString[nLen];` |
|     10 | 2660 | `			const char *zCur = zString;` |
|     10 | 2661 | `			PH7_BuildCharMask(pCtx,zList,nListlen,aMask);` |
|      - | 2662 | `			/* Left trim */` |
|     28 | 2663 | `			while( zCur < zEnd && aMask[(unsigned char)zCur[0]] ){` |
|     20 | 2664 | `				zCur++;` |
|      2 | 2665 | `			}` |
|     10 | 2666 | `			if( zCur >= zEnd ){` |
|      - | 2667 | `				/* Return the empty string */` |
|    ! 0 | 2668 | `				ph7_result_string(pCtx,"",0);` |
|    ! 0 | 2669 | `			}else{` |
|     10 | 2670 | `				ph7_result_string(pCtx,zCur,(int)(zEnd-zCur));` |
|      - | 2671 | `			}` |
|      - | 2672 | `		}` |
|      - | 2673 | `	}` |
|     14 | 2674 | `	return PH7_OK;` |
|      8 | 2675 | `}` |
|      - | 2676 | `/*` |
|      - | 2677 | ` * string strtolower(string $str)` |
|      - | 2678 | ` *  Make a string lowercase.` |
|      - | 2679 | ` * Parameters` |
|      - | 2680 | ` *  $str` |
|      - | 2681 | ` *   The input string.` |
|      - | 2682 | ` * Returns.` |
|      - | 2683 | ` *  The lowercased string.` |
|      - | 2684 | ` */` |
|  33094 | 2685 | `static int PH7_builtin_strtolower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2686 | `{` |
|      - | 2687 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2688 | `	int nLen;` |
|  33099 | 2689 | `	if( nArg < 1 ){` |
|      - | 2690 | `		/* Missing arguments,return null */` |
|    ! 0 | 2691 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2692 | `		return PH7_OK;` |
|      - | 2693 | `	}` |
|      - | 2694 | `	/* Extract the target string */` |
|  33099 | 2695 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|  33099 | 2696 | `	if( nLen < 1 ){` |
|      - | 2697 | `		/* Empty string,return */` |
|      3 | 2698 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2699 | `		return PH7_OK;` |
|      - | 2700 | `	}` |
|      - | 2701 | `	/* Perform the requested operation */` |
|  33097 | 2702 | `	zEnd = &zString[nLen];` |
| 104475 | 2703 | `	for(;;){` |
| 208955 | 2704 | `		if( zString >= zEnd ){` |
|      - | 2705 | `			/* No more input,break immediately */` |
|  33097 | 2706 | `			break;` |
|      - | 2707 | `		}` |
| 175863 | 2708 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2709 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2710 | `			zCur = zString;` |
|    ! 0 | 2711 | `			zString++;` |
|    ! 0 | 2712 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2713 | `				zString++;` |
|    ! 0 | 2714 | `			}` |
|      - | 2715 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2716 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2717 | `		}else{` |
| 175863 | 2718 | `			int c = zString[0];` |
| 175863 | 2719 | `			if( SyisUpper(c) ){` |
| 173517 | 2720 | `				c = SyToLower(zString[0]);` |
|  86756 | 2721 | `			}` |
|      - | 2722 | `			/* Append character */` |
| 175863 | 2723 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2724 | `			/* Advance the cursor */` |
| 175863 | 2725 | `			zString++;` |
|      - | 2726 | `		}` |
|      5 | 2727 | `	}` |
|  33097 | 2728 | `	return PH7_OK;` |
|  16552 | 2729 | `}` |
|      - | 2730 | `/*` |
|      - | 2731 | ` * string strtolower(string $str)` |
|      - | 2732 | ` *  Make a string uppercase.` |
|      - | 2733 | ` * Parameters` |
|      - | 2734 | ` *  $str` |
|      - | 2735 | ` *   The input string.` |
|      - | 2736 | ` * Returns.` |
|      - | 2737 | ` *  The uppercased string.` |
|      - | 2738 | ` */` |
|     48 | 2739 | `static int PH7_builtin_strtoupper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 2740 | `{` |
|      - | 2741 | `	const char *zString,*zCur,*zEnd;` |
|      - | 2742 | `	int nLen;` |
|     52 | 2743 | `	if( nArg < 1 ){` |
|      - | 2744 | `		/* Missing arguments,return null */` |
|    ! 0 | 2745 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2746 | `		return PH7_OK;` |
|      - | 2747 | `	}` |
|      - | 2748 | `	/* Extract the target string */` |
|     52 | 2749 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     52 | 2750 | `	if( nLen < 1 ){` |
|      - | 2751 | `		/* Empty string,return */` |
|      3 | 2752 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2753 | `		return PH7_OK;` |
|      - | 2754 | `	}` |
|      - | 2755 | `	/* Perform the requested operation */` |
|     50 | 2756 | `	zEnd = &zString[nLen];` |
|    111 | 2757 | `	for(;;){` |
|    226 | 2758 | `		if( zString >= zEnd ){` |
|      - | 2759 | `			/* No more input,break immediately */` |
|     50 | 2760 | `			break;` |
|      - | 2761 | `		}` |
|    180 | 2762 | `		if( (unsigned char)zString[0] >= 0xc0 ){` |
|      - | 2763 | `			/* UTF-8 stream,output verbatim */` |
|    ! 0 | 2764 | `			zCur = zString;` |
|    ! 0 | 2765 | `			zString++;` |
|    ! 0 | 2766 | `			while( zString < zEnd && ((unsigned char)zString[0] & 0xc0) == 0x80){` |
|    ! 0 | 2767 | `				zString++;` |
|    ! 0 | 2768 | `			}` |
|      - | 2769 | `			/* Append UTF-8 stream */` |
|    ! 0 | 2770 | `			ph7_result_string(pCtx,zCur,(int)(zString-zCur));` |
|    ! 0 | 2771 | `		}else{` |
|    180 | 2772 | `			int c = zString[0];` |
|    180 | 2773 | `			if( SyisLower(c) ){` |
|    174 | 2774 | `				c = SyToUpper(zString[0]);` |
|     85 | 2775 | `			}` |
|      - | 2776 | `			/* Append character */` |
|    180 | 2777 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 2778 | `			/* Advance the cursor */` |
|    180 | 2779 | `			zString++;` |
|      - | 2780 | `		}` |
|      4 | 2781 | `	}` |
|     50 | 2782 | `	return PH7_OK;` |
|     28 | 2783 | `}` |
|      - | 2784 | `/*` |
|      - | 2785 | ` * string ucfirst(string $str)` |
|      - | 2786 | ` *  Returns a string with the first character of str capitalized, if that` |
|      - | 2787 | ` *  character is alphabetic.` |
|      - | 2788 | ` * Parameters` |
|      - | 2789 | ` *  $str` |
|      - | 2790 | ` *   The input string.` |
|      - | 2791 | ` * Returns.` |
|      - | 2792 | ` *  The processed string.` |
|      - | 2793 | ` */` |
|      4 | 2794 | `static int PH7_builtin_ucfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2795 | `{` |
|      - | 2796 | `	const char *zString,*zEnd;` |
|      - | 2797 | `	int nLen,c;` |
|      5 | 2798 | `	if( nArg < 1 ){` |
|      - | 2799 | `		/* Missing arguments,return null */` |
|    ! 0 | 2800 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2801 | `		return PH7_OK;` |
|      - | 2802 | `	}` |
|      - | 2803 | `	/* Extract the target string */` |
|      5 | 2804 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2805 | `	if( nLen < 1 ){` |
|      - | 2806 | `		/* Empty string,return */` |
|      3 | 2807 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2808 | `		return PH7_OK;` |
|      - | 2809 | `	}` |
|      - | 2810 | `	/* Perform the requested operation */` |
|      3 | 2811 | `	zEnd = &zString[nLen];` |
|      3 | 2812 | `	c = zString[0];` |
|      3 | 2813 | `	if( SyisLower(c) ){` |
|      3 | 2814 | `		c = SyToUpper(c);` |
|      1 | 2815 | `	}` |
|      - | 2816 | `	/* Append the first character */` |
|      3 | 2817 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2818 | `	zString++;` |
|      3 | 2819 | `	if( zString < zEnd ){` |
|      - | 2820 | `		/* Append the rest of the input verbatim */` |
|      3 | 2821 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2822 | `	}` |
|      3 | 2823 | `	return PH7_OK;` |
|      3 | 2824 | `}` |
|      - | 2825 | `/*` |
|      - | 2826 | ` * string lcfirst(string $str)` |
|      - | 2827 | ` *  Make a string's first character lowercase.` |
|      - | 2828 | ` * Parameters` |
|      - | 2829 | ` *  $str` |
|      - | 2830 | ` *   The input string.` |
|      - | 2831 | ` * Returns.` |
|      - | 2832 | ` *  The processed string.` |
|      - | 2833 | ` */` |
|      4 | 2834 | `static int PH7_builtin_lcfirst(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 2835 | `{` |
|      - | 2836 | `	const char *zString,*zEnd;` |
|      - | 2837 | `	int nLen,c;` |
|      5 | 2838 | `	if( nArg < 1 ){` |
|      - | 2839 | `		/* Missing arguments,return null */` |
|    ! 0 | 2840 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2841 | `		return PH7_OK;` |
|      - | 2842 | `	}` |
|      - | 2843 | `	/* Extract the target string */` |
|      5 | 2844 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 2845 | `	if( nLen < 1 ){` |
|      - | 2846 | `		/* Empty string,return */` |
|      3 | 2847 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 2848 | `		return PH7_OK;` |
|      - | 2849 | `	}` |
|      - | 2850 | `	/* Perform the requested operation */` |
|      3 | 2851 | `	zEnd = &zString[nLen];` |
|      3 | 2852 | `	c = zString[0];` |
|      3 | 2853 | `	if( SyisUpper(c) ){` |
|      3 | 2854 | `		c = SyToLower(c);` |
|      1 | 2855 | `	}` |
|      - | 2856 | `	/* Append the first character */` |
|      3 | 2857 | `	ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      3 | 2858 | `	zString++;` |
|      3 | 2859 | `	if( zString < zEnd ){` |
|      - | 2860 | `		/* Append the rest of the input verbatim */` |
|      3 | 2861 | `		ph7_result_string(pCtx,zString,(int)(zEnd-zString));` |
|      1 | 2862 | `	}` |
|      3 | 2863 | `	return PH7_OK;` |
|      3 | 2864 | `}` |
|      - | 2865 | `/*` |
|      - | 2866 | ` * int ord(string $string)` |
|      - | 2867 | ` *  Returns the ASCII value of the first character of string.` |
|      - | 2868 | ` *  Passing null, an empty string, or a multi-byte string emits` |
|      - | 2869 | ` *  E_DEPRECATED to match PHP 8.4+ behaviour.` |
|      - | 2870 | ` * Parameters` |
|      - | 2871 | ` *  $string` |
|      - | 2872 | ` *   The input string.` |
|      - | 2873 | ` * Returns` |
|      - | 2874 | ` *  The ASCII value as an integer.` |
|      - | 2875 | ` */` |
|     56 | 2876 | `static int PH7_builtin_ord(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2877 | `{` |
|      - | 2878 | `	const char *zString;` |
|      - | 2879 | `	int nLen,c;` |
|      - | 2880 | `	/* PHP requires exactly one argument. */` |
|     59 | 2881 | `	if( nArg != 1 ){` |
|      8 | 2882 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2883 | `			"ArgumentCountError",` |
|      - | 2884 | `			"ord() expects exactly 1 argument, %d given",` |
|      2 | 2885 | `			nArg` |
|      - | 2886 | `			);` |
|      - | 2887 | `	}` |
|      - | 2888 | `	/* Passing null is deprecated (E_DEPRECATED).  PHP emits this before` |
|      - | 2889 | `	 * the empty-string deprecation, so we check null first. */` |
|     53 | 2890 | `	if( ph7_value_is_null(apArg[0]) ){` |
|      3 | 2891 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2892 | `			"ord(): Passing null to parameter #1 ($character) "` |
|      - | 2893 | `			"of type string is deprecated"` |
|      - | 2894 | `			);` |
|      1 | 2895 | `	}` |
|      - | 2896 | `	/* Extract the target string */` |
|     53 | 2897 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     53 | 2898 | `	if( nLen < 1 ){` |
|      - | 2899 | `		/* Empty string is deprecated (E_DEPRECATED). */` |
|      5 | 2900 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2901 | `			"ord(): Providing an empty string is deprecated"` |
|      - | 2902 | `			);` |
|      5 | 2903 | `		ph7_result_int(pCtx,0);` |
|      5 | 2904 | `		return PH7_OK;` |
|      - | 2905 | `	}` |
|      - | 2906 | `	/* A string longer than one byte is deprecated (E_DEPRECATED). */` |
|     49 | 2907 | `	if( nLen > 1 ){` |
|      7 | 2908 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,` |
|      - | 2909 | `			"ord(): Providing a string that is not one byte long "` |
|      - | 2910 | `			"is deprecated. Use ord($str[0]) instead"` |
|      - | 2911 | `			);` |
|      3 | 2912 | `	}` |
|      - | 2913 | `	/* Extract the ASCII value of the first character */` |
|     49 | 2914 | `	c = (unsigned char)zString[0];` |
|      - | 2915 | `	/* Return that value */` |
|     49 | 2916 | `	ph7_result_int(pCtx,c);` |
|     49 | 2917 | `	return PH7_OK;` |
|     31 | 2918 | `}` |
|      - | 2919 | `/*` |
|      - | 2920 | ` * string chr(int $codepoint)` |
|      - | 2921 | ` *  Returns a one-character string containing the character specified` |
|      - | 2922 | ` *  by the given codepoint.  Any integer is accepted; values outside` |
|      - | 2923 | ` *  the [0, 255] range emit an E_DEPRECATED and are masked with & 0xFF.` |
|      - | 2924 | ` * Parameters` |
|      - | 2925 | ` *  $codepoint` |
|      - | 2926 | ` *   An integer codepoint.  Values outside 0-255 are deprecated and` |
|      - | 2927 | ` *   will be constrained to a single byte.` |
|      - | 2928 | ` * Returns` |
|      - | 2929 | ` *  A single-character string.` |
|      - | 2930 | ` */` |
|   6486 | 2931 | `static int PH7_builtin_chr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 2932 | `{` |
|      - | 2933 | `	int c;` |
|      - | 2934 | `	unsigned char ch;` |
|      - | 2935 | `	/* PHP requires exactly one argument. */` |
|   6489 | 2936 | `	if( nArg != 1 ){` |
|      8 | 2937 | `		return PH7_VmThrowException(pCtx,` |
|      - | 2938 | `			"ArgumentCountError",` |
|      - | 2939 | `			"chr() expects exactly 1 argument, %d given",` |
|      2 | 2940 | `			nArg` |
|      - | 2941 | `			);` |
|      - | 2942 | `	}` |
|      - | 2943 | `	/* Implicit float-to-int conversion loses precision (E_DEPRECATED).` |
|      - | 2944 | `	 * PHP does not prefix this message with "chr():", so we call` |
|      - | 2945 | `	 * PH7_VmThrowError() with a NULL function name to avoid the` |
|      - | 2946 | `	 * automatic prefix that ph7_context_throw_error*() would add. */` |
|   6483 | 2947 | `	if( ph7_value_is_float(apArg[0]) ){` |
|      - | 2948 | `		char zBuf[120];` |
|      4 | 2949 | `		SyBufferFormat(zBuf,sizeof(zBuf),` |
|      - | 2950 | `			"Implicit conversion from float %g to int loses precision",` |
|      1 | 2951 | `			ph7_value_to_double(apArg[0])` |
|      - | 2952 | `			);` |
|      3 | 2953 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zBuf);` |
|      1 | 2954 | `	}` |
|      - | 2955 | `	/* Extract the codepoint. */` |
|   6483 | 2956 | `	c = ph7_value_to_int(apArg[0]);` |
|      - | 2957 | `	/* Out-of-range codepoint (E_DEPRECATED), then mask to a single byte.` |
|      - | 2958 | `	 * PHP includes "chr(): " in the $errstr passed to set_error_handler,` |
|      - | 2959 | `	 * so we embed the prefix in the message and pass NULL as the function` |
|      - | 2960 | `	 * name to avoid the API double-prefixing it. */` |
|   6483 | 2961 | `	if( c < 0 \|\| c > 255 ){` |
|      5 | 2962 | `		PH7_VmThrowError(pCtx->pVm,0,` |
|      - | 2963 | `			E_DEPRECATED,` |
|      - | 2964 | `			"chr(): Providing a value not in-between 0 and 255 is deprecated, "` |
|      - | 2965 | `			"this is because a byte value must be in the [0, 255] interval. "` |
|      - | 2966 | `			"The value used will be constrained using % 256"` |
|      - | 2967 | `			);` |
|      2 | 2968 | `	}` |
|      - | 2969 | `	/* Store in an unsigned char to avoid endian-dependent behaviour` |
|      - | 2970 | `	 * when taking the address of a wider int. */` |
|   6483 | 2971 | `	ch = (unsigned char)(c & 0xFF);` |
|      - | 2972 | `	/* Return the specified character */` |
|   6483 | 2973 | `	ph7_result_string(pCtx,(const char *)&ch,(int)sizeof(char));` |
|   6483 | 2974 | `	return PH7_OK;` |
|   3246 | 2975 | `}` |
|      - | 2976 | `/*` |
|      - | 2977 | ` * Binary to hex consumer callback.` |
|      - | 2978 | ` * This callback is the default consumer used by the hash functions` |
|      - | 2979 | ` * [i.e: bin2hex(),md5(),sha1(),md5_file() ... ] defined below.` |
|      - | 2980 | ` */` |
|   3118 | 2981 | `static int HashConsumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 2982 | `{` |
|      - | 2983 | `	/* Append hex chunk verbatim */` |
|   3120 | 2984 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   3120 | 2985 | `	return SXRET_OK;` |
|      2 | 2986 | `}` |
|      - | 2987 |  |
|      - | 2988 | `/*` |
|      - | 2989 | ` * string bin2hex(string $str)` |
|      - | 2990 | ` *  Convert binary data into hexadecimal representation.` |
|      - | 2991 | ` * Parameters` |
|      - | 2992 | ` *  $str` |
|      - | 2993 | ` *   The input string.` |
|      - | 2994 | ` * Returns.` |
|      - | 2995 | ` *  Returns the hexadecimal representation of the given string.` |
|      - | 2996 | ` */` |
|    138 | 2997 | `static int PH7_builtin_bin2hex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 2998 | `{` |
|      - | 2999 | `	const char *zString;` |
|      - | 3000 | `	int nLen;` |
|      - | 3001 | `	/* PHP 8 requires exactly one argument (ArgumentCountError). */` |
|    143 | 3002 | `	if( nArg != 1 ){` |
|      8 | 3003 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3004 | `			"ArgumentCountError",` |
|      - | 3005 | `			"bin2hex() expects exactly 1 argument, %d given",` |
|      2 | 3006 | `			nArg` |
|      - | 3007 | `			);` |
|      - | 3008 | `	}` |
|      - | 3009 | `	/* In PHP 8, bin2hex() is strict about its parameter type.` |
|      - | 3010 | `	 * Array/Resource values are not allowed and trigger a TypeError.` |
|      - | 3011 | `	 * Objects without __toString() must also raise a TypeError.` |
|      - | 3012 | `	 */` |
|    204 | 3013 | `	if( ph7_value_is_array(apArg[0]) \|\| ph7_value_is_resource(apArg[0]) \|\|` |
|    130 | 3014 | `		( ph7_value_is_object(apArg[0]) &&` |
|      3 | 3015 | `		  ((ph7_class_instance *)apArg[0]->x.pOther) != 0 &&` |
|      2 | 3016 | `		  PH7_ClassExtractMethod(((ph7_class_instance *)apArg[0]->x.pOther)->pClass,` |
|      1 | 3017 | `			"__toString",sizeof("__toString")-1) == 0` |
|      - | 3018 | `		)` |
|      - | 3019 | `	){` |
|      9 | 3020 | `		const char *zType = ph7_type_name(apArg[0]);` |
|      9 | 3021 | `		if( ph7_value_is_object(apArg[0]) ){` |
|      3 | 3022 | `			ph7_class_instance *pInst = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      3 | 3023 | `			if( pInst && pInst->pClass ){` |
|      3 | 3024 | `				zType = SyStringData(&pInst->pClass->sName);` |
|      1 | 3025 | `			}` |
|      1 | 3026 | `		}` |
|     12 | 3027 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3028 | `			"TypeError",` |
|      - | 3029 | `			"bin2hex(): Argument #1 ($string) must be of type string, %s given",` |
|      3 | 3030 | `			zType` |
|      - | 3031 | `			);` |
|      - | 3032 | `	}` |
|      - | 3033 | `	/* Extract the target string */` |
|    130 | 3034 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|    130 | 3035 | `	if( nLen < 1 ){` |
|      - | 3036 | `		/* Empty string,return */` |
|     13 | 3037 | `		ph7_result_string(pCtx,"",0);` |
|     13 | 3038 | `		return PH7_OK;` |
|      - | 3039 | `	}` |
|      - | 3040 | `	/* Perform the requested operation */` |
|    118 | 3041 | `	SyBinToHexConsumer((const void *)zString,(sxu32)nLen,HashConsumer,pCtx);` |
|    118 | 3042 | `	return PH7_OK;` |
|     74 | 3043 | `}` |
|      - | 3044 |  |
|      - | 3045 | `/* Search callback signature */` |
|      - | 3046 | `typedef sxi32 (*ProcStringMatch)(const void *,sxu32,const void *,sxu32,sxu32 *);` |
|      - | 3047 | `/*` |
|      - | 3048 | ` * Case-insensitive pattern match.` |
|      - | 3049 | ` * Brute force is the default search method used here.` |
|      - | 3050 | ` * This is due to the fact that brute-forcing works quite` |
|      - | 3051 | ` * well for short/medium texts on modern hardware.` |
|      - | 3052 | ` */` |
|    276 | 3053 | `static sxi32 iPatternMatch(const void *pText,sxu32 nLen,const void *pPattern,sxu32 iPatLen,sxu32 *pOfft)` |
|      1 | 3054 | `{` |
|    277 | 3055 | `	const char *zpIn = (const char *)pPattern;` |
|    277 | 3056 | `	const char *zIn = (const char *)pText;` |
|    277 | 3057 | `	const char *zpEnd = &zpIn[iPatLen];` |
|    277 | 3058 | `	const char *zEnd = &zIn[nLen];` |
|      - | 3059 | `	const char *zPtr,*zPtr2;` |
|      - | 3060 | `	int c,d;` |
|    277 | 3061 | `	if( iPatLen > nLen ){` |
|      - | 3062 | `		/* Don't bother processing */` |
|     67 | 3063 | `		return SXERR_NOTFOUND;` |
|      - | 3064 | `	}` |
|    783 | 3065 | `	for(;;){` |
|   1567 | 3066 | `		if( zIn >= zEnd ){` |
|    171 | 3067 | `			break;` |
|      - | 3068 | `		}` |
|   1397 | 3069 | `		c = SyToLower(zIn[0]);` |
|   1397 | 3070 | `		d = SyToLower(zpIn[0]);` |
|   1397 | 3071 | `		if( c == d ){` |
|    159 | 3072 | `			zPtr   = &zIn[1];` |
|    159 | 3073 | `			zPtr2  = &zpIn[1];` |
|    130 | 3074 | `			for(;;){` |
|    261 | 3075 | `				if( zPtr2 >= zpEnd ){` |
|      - | 3076 | `					/* Pattern found */` |
|     41 | 3077 | `					if( pOfft ){ *pOfft = (sxu32)(zIn-(const char *)pText); }` |
|     41 | 3078 | `					return SXRET_OK;` |
|      - | 3079 | `				}` |
|    221 | 3080 | `				if( zPtr >= zEnd ){` |
|    ! 0 | 3081 | `					break;` |
|      - | 3082 | `				}` |
|    221 | 3083 | `				c = SyToLower(zPtr[0]);` |
|    221 | 3084 | `				d = SyToLower(zPtr2[0]);` |
|    221 | 3085 | `				if( c != d ){` |
|    119 | 3086 | `					break;` |
|      - | 3087 | `				}` |
|    103 | 3088 | `				zPtr++; zPtr2++;` |
|      1 | 3089 | `			}` |
|     59 | 3090 | `		}` |
|   1357 | 3091 | `		zIn++;` |
|      1 | 3092 | `	}` |
|      - | 3093 | `	/* Pattern not found */` |
|    171 | 3094 | `	return SXERR_NOTFOUND;` |
|    139 | 3095 | `}` |
|      - | 3096 | `/*` |
|      - | 3097 | ` * string strstr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3098 | ` *  Find the first occurrence of a string.` |
|      - | 3099 | ` * Parameters` |
|      - | 3100 | ` *  $haystack` |
|      - | 3101 | ` *   The input string.` |
|      - | 3102 | ` * $needle` |
|      - | 3103 | ` *   Search pattern (must be a string).` |
|      - | 3104 | ` * $before_needle` |
|      - | 3105 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3106 | ` *   of the needle (excluding the needle).` |
|      - | 3107 | ` * Return` |
|      - | 3108 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3109 | ` */` |
|      6 | 3110 | `static int PH7_builtin_strstr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3111 | `{` |
|      7 | 3112 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3113 | `	const char *zBlob,*zPattern;` |
|      - | 3114 | `	int nLen,nPatLen;` |
|      - | 3115 | `	sxu32 nOfft;` |
|      - | 3116 | `	sxi32 rc;` |
|      7 | 3117 | `	if( nArg < 2 ){` |
|      - | 3118 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3119 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3120 | `		return PH7_OK;` |
|      - | 3121 | `	}` |
|      - | 3122 | `	/* Extract the needle and the haystack */` |
|      7 | 3123 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 3124 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      7 | 3125 | `	nOfft = 0; /* cc warning */` |
|      9 | 3126 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3127 | `		int before = 0;` |
|      - | 3128 | `		/* Perform the lookup */` |
|      5 | 3129 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3130 | `		if( rc != SXRET_OK ){` |
|      - | 3131 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3132 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3133 | `			return PH7_OK;` |
|      - | 3134 | `		}` |
|      - | 3135 | `		/* Return the portion of the string */` |
|      5 | 3136 | `		if( nArg > 2 ){` |
|      3 | 3137 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3138 | `		}` |
|      5 | 3139 | `		if( before ){` |
|      3 | 3140 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3141 | `		}else{` |
|      3 | 3142 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3143 | `		}` |
|      3 | 3144 | `	}else{` |
|      3 | 3145 | `		ph7_result_bool(pCtx,0);` |
|      - | 3146 | `	}` |
|      7 | 3147 | `	return PH7_OK;` |
|      4 | 3148 | `}` |
|      - | 3149 | `/*` |
|      - | 3150 | ` * string stristr(string $haystack,string $needle[,bool $before_needle = false ])` |
|      - | 3151 | ` *  Case-insensitive strstr().` |
|      - | 3152 | ` * Parameters` |
|      - | 3153 | ` *  $haystack` |
|      - | 3154 | ` *   The input string.` |
|      - | 3155 | ` * $needle` |
|      - | 3156 | ` *   Search pattern (must be a string).` |
|      - | 3157 | ` * $before_needle` |
|      - | 3158 | ` *   If TRUE, strstr() returns the part of the haystack before the first occurrence` |
|      - | 3159 | ` *   of the needle (excluding the needle).` |
|      - | 3160 | ` * Return` |
|      - | 3161 | ` *  Returns the portion of string, or FALSE if needle is not found.` |
|      - | 3162 | ` */` |
|      4 | 3163 | `static int PH7_builtin_stristr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3164 | `{` |
|      5 | 3165 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3166 | `	const char *zBlob,*zPattern;` |
|      - | 3167 | `	int nLen,nPatLen;` |
|      - | 3168 | `	sxu32 nOfft;` |
|      - | 3169 | `	sxi32 rc;` |
|      5 | 3170 | `	if( nArg < 2 ){` |
|      - | 3171 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3172 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3173 | `		return PH7_OK;` |
|      - | 3174 | `	}` |
|      - | 3175 | `	/* Extract the needle and the haystack */` |
|      5 | 3176 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3177 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      5 | 3178 | `	nOfft = 0; /* cc warning */` |
|      7 | 3179 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      5 | 3180 | `		int before = 0;` |
|      - | 3181 | `		/* Perform the lookup */` |
|      5 | 3182 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|      5 | 3183 | `		if( rc != SXRET_OK ){` |
|      - | 3184 | `			/* Pattern not found,return FALSE */` |
|    ! 0 | 3185 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 3186 | `			return PH7_OK;` |
|      - | 3187 | `		}` |
|      - | 3188 | `		/* Return the portion of the string */` |
|      5 | 3189 | `		if( nArg > 2 ){` |
|      3 | 3190 | `			before = ph7_value_to_int(apArg[2]);` |
|      1 | 3191 | `		}` |
|      5 | 3192 | `		if( before ){` |
|      3 | 3193 | `			ph7_result_string(pCtx,zBlob,(int)(&zBlob[nOfft]-zBlob));` |
|      2 | 3194 | `		}else{` |
|      3 | 3195 | `			ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      - | 3196 | `		}` |
|      3 | 3197 | `	}else{` |
|    ! 0 | 3198 | `		ph7_result_bool(pCtx,0);` |
|      - | 3199 | `	}` |
|      5 | 3200 | `	return PH7_OK;` |
|      3 | 3201 | `}` |
|      - | 3202 | `/*` |
|      - | 3203 | ` * int strpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3204 | ` *  Returns the numeric position of the first occurrence of needle in the haystack string.` |
|      - | 3205 | ` * Parameters` |
|      - | 3206 | ` *  $haystack` |
|      - | 3207 | ` *   The input string.` |
|      - | 3208 | ` * $needle` |
|      - | 3209 | ` *   Search pattern (must be a string).` |
|      - | 3210 | ` * $offset` |
|      - | 3211 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3212 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3213 | ` *   of haystack.` |
|      - | 3214 | ` * Return` |
|      - | 3215 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3216 | ` */` |
|   1342 | 3217 | `static int PH7_builtin_strpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 3218 | `{` |
|   1347 | 3219 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3220 | `	const char *zBlob,*zPattern;` |
|      - | 3221 | `	int nLen,nPatLen,nStart;` |
|      - | 3222 | `	sxu32 nOfft;` |
|      - | 3223 | `	sxi32 rc;` |
|   1347 | 3224 | `	if( nArg < 2 ){` |
|      - | 3225 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3226 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3227 | `		return PH7_OK;` |
|      - | 3228 | `	}` |
|      - | 3229 | `	/* Extract the needle and the haystack */` |
|   1347 | 3230 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|   1347 | 3231 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|   1347 | 3232 | `	nOfft = 0; /* cc warning */` |
|   1347 | 3233 | `	nStart = 0;` |
|      - | 3234 | `	/* Peek the starting offset if available */` |
|   1347 | 3235 | `	if( nArg > 2 ){` |
|    ! 0 | 3236 | `		nStart = ph7_value_to_int(apArg[2]);` |
|    ! 0 | 3237 | `		if( nStart < 0 ){` |
|    ! 0 | 3238 | `			nStart = -nStart;` |
|    ! 0 | 3239 | `		}` |
|    ! 0 | 3240 | `		if( nStart >= nLen ){` |
|      - | 3241 | `			/* Invalid offset */` |
|    ! 0 | 3242 | `			nStart = 0;` |
|    ! 0 | 3243 | `		}else{` |
|    ! 0 | 3244 | `			zBlob += nStart;` |
|    ! 0 | 3245 | `			nLen -= nStart;` |
|      - | 3246 | `		}` |
|    ! 0 | 3247 | `	}` |
|   1347 | 3248 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3249 | `		/* Perform the lookup */` |
|   1345 | 3250 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|   1345 | 3251 | `		if( rc != SXRET_OK ){` |
|      - | 3252 | `			/* Pattern not found,return FALSE */` |
|    719 | 3253 | `			ph7_result_bool(pCtx,0);` |
|    719 | 3254 | `			return PH7_OK;` |
|      - | 3255 | `		}` |
|      - | 3256 | `		/* Return the pattern position */` |
|    630 | 3257 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|    317 | 3258 | `	}else{` |
|      3 | 3259 | `		ph7_result_bool(pCtx,0);` |
|      - | 3260 | `	}` |
|    632 | 3261 | `	return PH7_OK;` |
|    676 | 3262 | `}` |
|      - | 3263 | `/*` |
|      - | 3264 | ` * Validate and resolve a single string-typed parameter for str_contains/` |
|      - | 3265 | ` * str_starts_with/str_ends_with. Emits an E_DEPRECATED notice for null` |
|      - | 3266 | ` * (matching PHP 8.1+; falls through with an empty string), and throws` |
|      - | 3267 | ` * TypeError for arrays, resources, and objects without __toString.` |
|      - | 3268 | ` *` |
|      - | 3269 | ` * For objects with __toString, invokes the method directly into pTmp and` |
|      - | 3270 | ` * uses its raw byte buffer. This preserves empty results, which the` |
|      - | 3271 | ` * engine's MemObjStringValue otherwise replaces with the literal "Object".` |
|      - | 3272 | ` *` |
|      - | 3273 | ` * On success, pzOut/pnOut point at the resolved byte buffer; the buffer` |
|      - | 3274 | ` * is valid until pTmp is released or pArg is mutated.` |
|      - | 3275 | ` */` |
|    720 | 3276 | `static sxi32 StrPredicateResolveArg(` |
|      - | 3277 | `	ph7_context *pCtx,` |
|      - | 3278 | `	ph7_value *pArg,` |
|      - | 3279 | `	const char *zFunc,` |
|      - | 3280 | `	int iArgNum,` |
|      - | 3281 | `	const char *zParamName,` |
|      - | 3282 | `	const char *zTypeStr, /* Declared type in the TypeError, e.g. "string" / "?string" */` |
|      - | 3283 | `	const char *zNullMsg,` |
|      - | 3284 | `	ph7_value *pTmp,` |
|      - | 3285 | `	const char **pzOut,` |
|      - | 3286 | `	int *pnOut` |
|      4 | 3287 | `){` |
|    724 | 3288 | `	if( ph7_value_is_null(pArg) ){` |
|     13 | 3289 | `		PH7_VmThrowError(pCtx->pVm,0,E_DEPRECATED,zNullMsg);` |
|     13 | 3290 | `		*pzOut = "";` |
|     13 | 3291 | `		*pnOut = 0;` |
|     13 | 3292 | `		return PH7_OK;` |
|      - | 3293 | `	}` |
|   1088 | 3294 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_resource(pArg) \|\|` |
|    682 | 3295 | `	    ( ph7_value_is_object(pArg) &&` |
|    105 | 3296 | `	      ((ph7_class_instance *)pArg->x.pOther) != 0 &&` |
|     70 | 3297 | `	      PH7_ClassExtractMethod(((ph7_class_instance *)pArg->x.pOther)->pClass,` |
|     35 | 3298 | `	        "__toString",sizeof("__toString")-1) == 0` |
|      - | 3299 | `	    )` |
|      - | 3300 | `	){` |
|     52 | 3301 | `		const char *zType = ph7_type_name(pArg);` |
|     52 | 3302 | `		if( ph7_value_is_object(pArg) ){` |
|     23 | 3303 | `			ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     23 | 3304 | `			if( pInst && pInst->pClass ){` |
|     23 | 3305 | `				zType = SyStringData(&pInst->pClass->sName);` |
|     11 | 3306 | `			}` |
|     11 | 3307 | `		}` |
|     76 | 3308 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3309 | `			"TypeError",` |
|      - | 3310 | `			"%s(): Argument #%d (%s) must be of type %s, %s given",` |
|     24 | 3311 | `			zFunc, iArgNum, zParamName, zTypeStr, zType` |
|      - | 3312 | `			);` |
|      - | 3313 | `	}` |
|    661 | 3314 | `	if( ph7_value_is_object(pArg) ){` |
|     49 | 3315 | `		ph7_class_instance *pInst = (ph7_class_instance *)pArg->x.pOther;` |
|     49 | 3316 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pInst->pClass,` |
|      - | 3317 | `			"__toString",sizeof("__toString")-1);` |
|     49 | 3318 | `		PH7_VmCallClassMethod(pCtx->pVm,pInst,pMethod,pTmp,0,0);` |
|     49 | 3319 | `		*pzOut = (const char *)SyBlobData(&pTmp->sBlob);` |
|     49 | 3320 | `		*pnOut = (int)SyBlobLength(&pTmp->sBlob);` |
|     49 | 3321 | `		return PH7_OK;` |
|      - | 3322 | `	}` |
|    613 | 3323 | `	*pzOut = ph7_value_to_string(pArg,pnOut);` |
|    613 | 3324 | `	return PH7_OK;` |
|    364 | 3325 | `}` |
|      - | 3326 | `/*` |
|      - | 3327 | ` * bool str_contains(string $haystack, string $needle)` |
|      - | 3328 | ` *  Determine if a string contains a given substring (PHP 8.0).` |
|      - | 3329 | ` * Return` |
|      - | 3330 | ` *  TRUE if needle occurs in haystack. An empty needle always returns TRUE.` |
|      - | 3331 | ` */` |
|     96 | 3332 | `static int PH7_builtin_str_contains(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 3333 | `{` |
|      - | 3334 | `	const char *zHaystack,*zNeedle;` |
|      - | 3335 | `	int nHayLen,nNeedleLen;` |
|      - | 3336 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3337 | `	sxi32 rc;` |
|    100 | 3338 | `	if( nArg != 2 ){` |
|     18 | 3339 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3340 | `			"ArgumentCountError",` |
|      - | 3341 | `			"str_contains() expects exactly 2 arguments, %d given",` |
|      5 | 3342 | `			nArg` |
|      - | 3343 | `			);` |
|      - | 3344 | `	}` |
|     88 | 3345 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     88 | 3346 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     88 | 3347 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_contains",1,"$haystack","string",` |
|      - | 3348 | `		"str_contains(): Passing null to parameter #1 ($haystack) "` |
|      - | 3349 | `		"of type string is deprecated",` |
|      - | 3350 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     88 | 3351 | `	if( rc != PH7_OK ) goto out;` |
|     81 | 3352 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_contains",2,"$needle","string",` |
|      - | 3353 | `		"str_contains(): Passing null to parameter #2 ($needle) "` |
|      - | 3354 | `		"of type string is deprecated",` |
|      - | 3355 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     81 | 3356 | `	if( rc != PH7_OK ) goto out;` |
|     77 | 3357 | `	if( nNeedleLen < 1 ){` |
|     13 | 3358 | `		ph7_result_bool(pCtx,1);` |
|     71 | 3359 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3360 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3361 | `	}else{` |
|     85 | 3362 | `		sxi32 srch = SyBlobSearch((const void *)zHaystack,(sxu32)nHayLen,` |
|     28 | 3363 | `		                          (const void *)zNeedle,(sxu32)nNeedleLen,0);` |
|     57 | 3364 | `		ph7_result_bool(pCtx,srch == SXRET_OK ? 1 : 0);` |
|      - | 3365 | `	}` |
|     77 | 3366 | `	rc = PH7_OK;` |
|     43 | 3367 | `out:` |
|     88 | 3368 | `	PH7_MemObjRelease(&sHayTmp);` |
|     88 | 3369 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     88 | 3370 | `	return rc;` |
|     52 | 3371 | `}` |
|      - | 3372 | `/*` |
|      - | 3373 | ` * bool str_starts_with(string $haystack, string $needle)` |
|      - | 3374 | ` *  Check if a string starts with a given substring (PHP 8.0).` |
|      - | 3375 | ` * Return` |
|      - | 3376 | ` *  TRUE if haystack begins with needle. An empty needle always returns TRUE.` |
|      - | 3377 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3378 | ` */` |
|     78 | 3379 | `static int PH7_builtin_str_starts_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 3380 | `{` |
|      - | 3381 | `	const char *zHaystack,*zNeedle;` |
|      - | 3382 | `	int nHayLen,nNeedleLen;` |
|      - | 3383 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3384 | `	sxi32 rc;` |
|     82 | 3385 | `	if( nArg != 2 ){` |
|     18 | 3386 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3387 | `			"ArgumentCountError",` |
|      - | 3388 | `			"str_starts_with() expects exactly 2 arguments, %d given",` |
|      5 | 3389 | `			nArg` |
|      - | 3390 | `			);` |
|      - | 3391 | `	}` |
|     70 | 3392 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 3393 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 3394 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_starts_with",1,"$haystack","string",` |
|      - | 3395 | `		"str_starts_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3396 | `		"of type string is deprecated",` |
|      - | 3397 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 3398 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 3399 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_starts_with",2,"$needle","string",` |
|      - | 3400 | `		"str_starts_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3401 | `		"of type string is deprecated",` |
|      - | 3402 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 3403 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3404 | `	if( nNeedleLen < 1 ){` |
|     13 | 3405 | `		ph7_result_bool(pCtx,1);` |
|     53 | 3406 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3407 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3408 | `	}else{` |
|     58 | 3409 | `		ph7_result_bool(pCtx,` |
|     38 | 3410 | `			SyMemcmp(zHaystack,zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3411 | `	}` |
|     59 | 3412 | `	rc = PH7_OK;` |
|     34 | 3413 | `out:` |
|     70 | 3414 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 3415 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 3416 | `	return rc;` |
|     43 | 3417 | `}` |
|      - | 3418 | `/*` |
|      - | 3419 | ` * bool str_ends_with(string $haystack, string $needle)` |
|      - | 3420 | ` *  Check if a string ends with a given substring (PHP 8.0).` |
|      - | 3421 | ` * Return` |
|      - | 3422 | ` *  TRUE if haystack ends with needle. An empty needle always returns TRUE.` |
|      - | 3423 | ` *  Comparison is binary-safe (uses SyMemcmp, not SyStrncmp).` |
|      - | 3424 | ` */` |
|     78 | 3425 | `static int PH7_builtin_str_ends_with(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      4 | 3426 | `{` |
|      - | 3427 | `	const char *zHaystack,*zNeedle;` |
|      - | 3428 | `	int nHayLen,nNeedleLen;` |
|      - | 3429 | `	ph7_value sHayTmp,sNeedleTmp;` |
|      - | 3430 | `	sxi32 rc;` |
|     82 | 3431 | `	if( nArg != 2 ){` |
|     18 | 3432 | `		return PH7_VmThrowException(pCtx,` |
|      - | 3433 | `			"ArgumentCountError",` |
|      - | 3434 | `			"str_ends_with() expects exactly 2 arguments, %d given",` |
|      5 | 3435 | `			nArg` |
|      - | 3436 | `			);` |
|      - | 3437 | `	}` |
|     70 | 3438 | `	PH7_MemObjInit(pCtx->pVm,&sHayTmp);` |
|     70 | 3439 | `	PH7_MemObjInit(pCtx->pVm,&sNeedleTmp);` |
|     70 | 3440 | `	rc = StrPredicateResolveArg(pCtx,apArg[0],"str_ends_with",1,"$haystack","string",` |
|      - | 3441 | `		"str_ends_with(): Passing null to parameter #1 ($haystack) "` |
|      - | 3442 | `		"of type string is deprecated",` |
|      - | 3443 | `		&sHayTmp,&zHaystack,&nHayLen);` |
|     70 | 3444 | `	if( rc != PH7_OK ) goto out;` |
|     63 | 3445 | `	rc = StrPredicateResolveArg(pCtx,apArg[1],"str_ends_with",2,"$needle","string",` |
|      - | 3446 | `		"str_ends_with(): Passing null to parameter #2 ($needle) "` |
|      - | 3447 | `		"of type string is deprecated",` |
|      - | 3448 | `		&sNeedleTmp,&zNeedle,&nNeedleLen);` |
|     63 | 3449 | `	if( rc != PH7_OK ) goto out;` |
|     59 | 3450 | `	if( nNeedleLen < 1 ){` |
|     13 | 3451 | `		ph7_result_bool(pCtx,1);` |
|     53 | 3452 | `	}else if( nHayLen < nNeedleLen ){` |
|      9 | 3453 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3454 | `	}else{` |
|     58 | 3455 | `		ph7_result_bool(pCtx,` |
|     38 | 3456 | `			SyMemcmp(zHaystack + (nHayLen - nNeedleLen),zNeedle,(sxu32)nNeedleLen) == 0 ? 1 : 0);` |
|      - | 3457 | `	}` |
|     59 | 3458 | `	rc = PH7_OK;` |
|     34 | 3459 | `out:` |
|     70 | 3460 | `	PH7_MemObjRelease(&sHayTmp);` |
|     70 | 3461 | `	PH7_MemObjRelease(&sNeedleTmp);` |
|     70 | 3462 | `	return rc;` |
|     43 | 3463 | `}` |
|      - | 3464 | `/*` |
|      - | 3465 | ` * int stripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3466 | ` *  Case-insensitive strpos.` |
|      - | 3467 | ` * Parameters` |
|      - | 3468 | ` *  $haystack` |
|      - | 3469 | ` *   The input string.` |
|      - | 3470 | ` * $needle` |
|      - | 3471 | ` *   Search pattern (must be a string).` |
|      - | 3472 | ` * $offset` |
|      - | 3473 | ` *   This optional offset parameter allows you to specify which character in haystack` |
|      - | 3474 | ` *   to start searching. The position returned is still relative to the beginning` |
|      - | 3475 | ` *   of haystack.` |
|      - | 3476 | ` * Return` |
|      - | 3477 | ` *  Returns the position as an integer.If needle is not found, strpos() will return FALSE.` |
|      - | 3478 | ` */` |
|    174 | 3479 | `static int PH7_builtin_stripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3480 | `{` |
|    175 | 3481 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3482 | `	const char *zBlob,*zPattern;` |
|      - | 3483 | `	int nLen,nPatLen,nStart;` |
|      - | 3484 | `	sxu32 nOfft;` |
|      - | 3485 | `	sxi32 rc;` |
|    175 | 3486 | `	if( nArg < 2 ){` |
|      - | 3487 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3488 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3489 | `		return PH7_OK;` |
|      - | 3490 | `	}` |
|      - | 3491 | `	/* Extract the needle and the haystack */` |
|    175 | 3492 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|    175 | 3493 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|    175 | 3494 | `	nOfft = 0; /* cc warning */` |
|    175 | 3495 | `	nStart = 0;` |
|      - | 3496 | `	/* Peek the starting offset if available */` |
|    175 | 3497 | `	if( nArg > 2 ){` |
|      5 | 3498 | `		nStart = ph7_value_to_int(apArg[2]);` |
|      5 | 3499 | `		if( nStart < 0 ){` |
|      3 | 3500 | `			nStart = -nStart;` |
|      1 | 3501 | `		}` |
|      5 | 3502 | `		if( nStart >= nLen ){` |
|      - | 3503 | `			/* Invalid offset */` |
|    ! 0 | 3504 | `			nStart = 0;` |
|    ! 0 | 3505 | `		}else{` |
|      5 | 3506 | `			zBlob += nStart;` |
|      5 | 3507 | `			nLen -= nStart;` |
|      - | 3508 | `		}` |
|      2 | 3509 | `	}` |
|    175 | 3510 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3511 | `		/* Perform the lookup */` |
|    175 | 3512 | `		rc = xPatternMatch(zBlob,(sxu32)nLen,zPattern,(sxu32)nPatLen,&nOfft);` |
|    175 | 3513 | `		if( rc != SXRET_OK ){` |
|      - | 3514 | `			/* Pattern not found,return FALSE */` |
|    161 | 3515 | `			ph7_result_bool(pCtx,0);` |
|    161 | 3516 | `			return PH7_OK;` |
|      - | 3517 | `		}` |
|      - | 3518 | `		/* Return the pattern position */` |
|     15 | 3519 | `		ph7_result_int64(pCtx,(ph7_int64)(nOfft+nStart));` |
|      8 | 3520 | `	}else{` |
|    ! 0 | 3521 | `		ph7_result_bool(pCtx,0);` |
|      - | 3522 | `	}` |
|     15 | 3523 | `	return PH7_OK;` |
|     88 | 3524 | `}` |
|      - | 3525 | `/*` |
|      - | 3526 | ` * int strrpos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3527 | ` *  Find the numeric position of the last occurrence of needle in the haystack string.` |
|      - | 3528 | ` * Parameters` |
|      - | 3529 | ` *  $haystack` |
|      - | 3530 | ` *   The input string.` |
|      - | 3531 | ` * $needle` |
|      - | 3532 | ` *   Search pattern (must be a string).` |
|      - | 3533 | ` * $offset` |
|      - | 3534 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3535 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3536 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3537 | ` * Return` |
|      - | 3538 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3539 | ` */` |
|     40 | 3540 | `static int PH7_builtin_strrpos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3541 | `{` |
|      - | 3542 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     41 | 3543 | `	ProcStringMatch xPatternMatch = SyBlobSearch; /* Case-sensitive pattern match */` |
|      - | 3544 | `	int nLen,nPatLen;` |
|      - | 3545 | `	sxu32 nOfft;` |
|      - | 3546 | `	sxi32 rc;` |
|     41 | 3547 | `	if( nArg < 2 ){` |
|      - | 3548 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3549 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3550 | `		return PH7_OK;` |
|      - | 3551 | `	}` |
|      - | 3552 | `	/* Extract the needle and the haystack */` |
|     41 | 3553 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     41 | 3554 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3555 | `	/* Point to the end of the pattern */` |
|     41 | 3556 | `	zPtr = &zBlob[nLen - 1];` |
|     41 | 3557 | `	zEnd = &zBlob[nLen];` |
|      - | 3558 | `	/* Save the starting posistion */` |
|     41 | 3559 | `	zStart = zBlob;` |
|     41 | 3560 | `	nOfft = 0; /* cc warning */` |
|      - | 3561 | `	/* Peek the starting offset if available */` |
|     41 | 3562 | `	if( nArg > 2 ){` |
|      - | 3563 | `		int nStart;` |
|     21 | 3564 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     21 | 3565 | `		if( nStart < 0 ){` |
|     11 | 3566 | `			nStart = -nStart;` |
|     11 | 3567 | `			if( nStart >= nLen ){` |
|      - | 3568 | `				/* Invalid offset */` |
|      3 | 3569 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3570 | `				return PH7_OK;` |
|    ! 0 | 3571 | `			}else{` |
|      9 | 3572 | `				nLen -= nStart;` |
|      9 | 3573 | `				zPtr = &zBlob[nLen - 1];` |
|      9 | 3574 | `				zEnd = &zBlob[nLen];` |
|      - | 3575 | `			}` |
|      5 | 3576 | `		}else{` |
|     11 | 3577 | `			if( nStart >= nLen ){` |
|      - | 3578 | `				/* Invalid offset */` |
|      5 | 3579 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3580 | `				return PH7_OK;` |
|    ! 0 | 3581 | `			}else{` |
|      7 | 3582 | `				zBlob += nStart;` |
|      7 | 3583 | `				nLen -= nStart;` |
|      - | 3584 | `			}` |
|      - | 3585 | `		}` |
|      7 | 3586 | `	}` |
|     35 | 3587 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3588 | `		/* Perform the lookup */` |
|    121 | 3589 | `		for(;;){` |
|    243 | 3590 | `			if( zBlob >= zPtr ){` |
|     21 | 3591 | `				break;` |
|      - | 3592 | `			}` |
|    223 | 3593 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|    223 | 3594 | `			if( rc == SXRET_OK ){` |
|      - | 3595 | `				/* Pattern found,return it's position */` |
|     13 | 3596 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     13 | 3597 | `				return PH7_OK;` |
|      - | 3598 | `			}` |
|    211 | 3599 | `			zPtr--;` |
|      1 | 3600 | `		}` |
|      - | 3601 | `		/* Pattern not found,return FALSE */` |
|     21 | 3602 | `		ph7_result_bool(pCtx,0);` |
|     11 | 3603 | `	}else{` |
|      3 | 3604 | `		ph7_result_bool(pCtx,0);` |
|      - | 3605 | `	}` |
|     23 | 3606 | `	return PH7_OK;` |
|     21 | 3607 | `}` |
|      - | 3608 | `/*` |
|      - | 3609 | ` * int strripos(string $haystack,string $needle [,int $offset = 0 ] )` |
|      - | 3610 | ` *  Case-insensitive strrpos.` |
|      - | 3611 | ` * Parameters` |
|      - | 3612 | ` *  $haystack` |
|      - | 3613 | ` *   The input string.` |
|      - | 3614 | ` * $needle` |
|      - | 3615 | ` *   Search pattern (must be a string).` |
|      - | 3616 | ` * $offset` |
|      - | 3617 | ` *   If specified, search will start this number of characters counted from the beginning` |
|      - | 3618 | ` *   of the string. If the value is negative, search will instead start from that many` |
|      - | 3619 | ` *   characters from the end of the string, searching backwards.` |
|      - | 3620 | ` * Return` |
|      - | 3621 | ` *  Returns the position as an integer.If needle is not found, strrpos() will return FALSE.` |
|      - | 3622 | ` */` |
|     26 | 3623 | `static int PH7_builtin_strripos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3624 | `{` |
|      - | 3625 | `	const char *zStart,*zBlob,*zPattern,*zPtr,*zEnd;` |
|     27 | 3626 | `	ProcStringMatch xPatternMatch = iPatternMatch; /* Case-insensitive pattern match */` |
|      - | 3627 | `	int nLen,nPatLen;` |
|      - | 3628 | `	sxu32 nOfft;` |
|      - | 3629 | `	sxi32 rc;` |
|     27 | 3630 | `	if( nArg < 2 ){` |
|      - | 3631 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3632 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3633 | `		return PH7_OK;` |
|      - | 3634 | `	}` |
|      - | 3635 | `	/* Extract the needle and the haystack */` |
|     27 | 3636 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 3637 | `	zPattern = ph7_value_to_string(apArg[1],&nPatLen);` |
|      - | 3638 | `	/* Point to the end of the pattern */` |
|     27 | 3639 | `	zPtr = &zBlob[nLen - 1];` |
|     27 | 3640 | `	zEnd = &zBlob[nLen];` |
|      - | 3641 | `	/* Save the starting posistion */` |
|     27 | 3642 | `	zStart = zBlob;` |
|     27 | 3643 | `	nOfft = 0; /* cc warning */` |
|      - | 3644 | `	/* Peek the starting offset if available */` |
|     27 | 3645 | `	if( nArg > 2 ){` |
|      - | 3646 | `		int nStart;` |
|     15 | 3647 | `		nStart = ph7_value_to_int(apArg[2]);` |
|     15 | 3648 | `		if( nStart < 0 ){` |
|      7 | 3649 | `			nStart = -nStart;` |
|      7 | 3650 | `			if( nStart >= nLen ){` |
|      - | 3651 | `				/* Invalid offset */` |
|      3 | 3652 | `				ph7_result_bool(pCtx,0);` |
|      3 | 3653 | `				return PH7_OK;` |
|    ! 0 | 3654 | `			}else{` |
|      5 | 3655 | `				nLen -= nStart;` |
|      5 | 3656 | `				zPtr = &zBlob[nLen - 1];` |
|      5 | 3657 | `				zEnd = &zBlob[nLen];` |
|      - | 3658 | `			}` |
|      3 | 3659 | `		}else{` |
|      9 | 3660 | `			if( nStart >= nLen ){` |
|      - | 3661 | `				/* Invalid offset */` |
|      5 | 3662 | `				ph7_result_bool(pCtx,0);` |
|      5 | 3663 | `				return PH7_OK;` |
|    ! 0 | 3664 | `			}else{` |
|      5 | 3665 | `				zBlob += nStart;` |
|      5 | 3666 | `				nLen -= nStart;` |
|      - | 3667 | `			}` |
|      - | 3668 | `		}` |
|      4 | 3669 | `	}` |
|     21 | 3670 | `	if( nLen > 0 && nPatLen > 0 ){` |
|      - | 3671 | `		/* Perform the lookup */` |
|     44 | 3672 | `		for(;;){` |
|     89 | 3673 | `			if( zBlob >= zPtr ){` |
|      9 | 3674 | `				break;` |
|      - | 3675 | `			}` |
|     81 | 3676 | `			rc = xPatternMatch((const void *)zPtr,(sxu32)(zEnd-zPtr),(const void *)zPattern,(sxu32)nPatLen,&nOfft);` |
|     81 | 3677 | `			if( rc == SXRET_OK ){` |
|      - | 3678 | `				/* Pattern found,return it's position */` |
|     11 | 3679 | `				ph7_result_int64(pCtx,(ph7_int64)(&zPtr[nOfft] - zStart));` |
|     11 | 3680 | `				return PH7_OK;` |
|      - | 3681 | `			}` |
|     71 | 3682 | `			zPtr--;` |
|      1 | 3683 | `		}` |
|      - | 3684 | `		/* Pattern not found,return FALSE */` |
|      9 | 3685 | `		ph7_result_bool(pCtx,0);` |
|      5 | 3686 | `	}else{` |
|      3 | 3687 | `		ph7_result_bool(pCtx,0);` |
|      - | 3688 | `	}` |
|     11 | 3689 | `	return PH7_OK;` |
|     14 | 3690 | `}` |
|      - | 3691 | `/*` |
|      - | 3692 | ` * int strrchr(string $haystack,mixed $needle)` |
|      - | 3693 | ` *  Find the last occurrence of a character in a string.` |
|      - | 3694 | ` * Parameters` |
|      - | 3695 | ` *  $haystack` |
|      - | 3696 | ` *   The input string.` |
|      - | 3697 | ` * $needle` |
|      - | 3698 | ` *  If needle contains more than one character, only the first is used.` |
|      - | 3699 | ` *  This behavior is different from that of strstr().` |
|      - | 3700 | ` *  If needle is not a string, it is converted to an integer and applied` |
|      - | 3701 | ` *  as the ordinal value of a character.` |
|      - | 3702 | ` * Return` |
|      - | 3703 | ` *  This function returns the portion of string, or FALSE if needle is not found.` |
|      - | 3704 | ` */` |
|     22 | 3705 | `static int PH7_builtin_strrchr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3706 | `{` |
|      - | 3707 | `	const char *zBlob;` |
|      - | 3708 | `	int nLen,c;` |
|     23 | 3709 | `	if( nArg < 2 ){` |
|      - | 3710 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 3711 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 3712 | `		return PH7_OK;` |
|      - | 3713 | `	}` |
|      - | 3714 | `	/* Extract the haystack */` |
|     23 | 3715 | `	zBlob = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3716 | `	c = 0; /* cc warning */` |
|     23 | 3717 | `	if( nLen > 0 ){` |
|      - | 3718 | `		sxu32 nOfft;` |
|      - | 3719 | `		sxi32 rc;` |
|     21 | 3720 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 3721 | `			const char *zPattern;` |
|     11 | 3722 | `			zPattern = ph7_value_to_string(apArg[1],0); /* Never fail,so there is no need to check` |
|      - | 3723 | `														 * for NULL pointer.` |
|      - | 3724 | `														 */` |
|     11 | 3725 | `			c = zPattern[0];` |
|      6 | 3726 | `		}else{` |
|      - | 3727 | `			/* Int cast */` |
|     11 | 3728 | `			c = ph7_value_to_int(apArg[1]);` |
|      - | 3729 | `		}` |
|      - | 3730 | `		/* Perform the lookup */` |
|     21 | 3731 | `		rc = SyByteFind2(zBlob,(sxu32)nLen,c,&nOfft);` |
|     21 | 3732 | `		if( rc != SXRET_OK ){` |
|      - | 3733 | `			/* No such entry,return FALSE */` |
|      7 | 3734 | `			ph7_result_bool(pCtx,0);` |
|      7 | 3735 | `			return PH7_OK;` |
|      - | 3736 | `		}` |
|      - | 3737 | `		/* Return the string portion */` |
|     15 | 3738 | `		ph7_result_string(pCtx,&zBlob[nOfft],(int)(&zBlob[nLen]-&zBlob[nOfft]));` |
|      8 | 3739 | `	}else{` |
|      3 | 3740 | `		ph7_result_bool(pCtx,0);` |
|      - | 3741 | `	}` |
|     17 | 3742 | `	return PH7_OK;` |
|     12 | 3743 | `}` |
|      - | 3744 | `/*` |
|      - | 3745 | ` * string strrev(string $string)` |
|      - | 3746 | ` *  Reverse a string.` |
|      - | 3747 | ` * Parameters` |
|      - | 3748 | ` *  $string` |
|      - | 3749 | ` *   String to be reversed.` |
|      - | 3750 | ` * Return` |
|      - | 3751 | ` *  The reversed string.` |
|      - | 3752 | ` */` |
|      2 | 3753 | `static int PH7_builtin_strrev(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3754 | `{` |
|      - | 3755 | `	const char *zIn,*zEnd;` |
|      - | 3756 | `	int nLen,c;` |
|      3 | 3757 | `	if( nArg < 1 ){` |
|      - | 3758 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3759 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3760 | `		return PH7_OK;` |
|      - | 3761 | `	}` |
|      - | 3762 | `	/* Extract the target string */` |
|      3 | 3763 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 3764 | `	if( nLen < 1 ){` |
|      - | 3765 | `		/* Empty string Return null */` |
|    ! 0 | 3766 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3767 | `		return PH7_OK;` |
|      - | 3768 | `	}` |
|      - | 3769 | `	/* Perform the requested operation */` |
|      3 | 3770 | `	zEnd = &zIn[nLen - 1];` |
|      4 | 3771 | `	for(;;){` |
|      9 | 3772 | `		if( zEnd < zIn ){` |
|      - | 3773 | `			/* No more input to process */` |
|      3 | 3774 | `			break;` |
|      - | 3775 | `		}` |
|      - | 3776 | `		/* Append current character */` |
|      7 | 3777 | `		c = zEnd[0];` |
|      7 | 3778 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      7 | 3779 | `		zEnd--;` |
|      1 | 3780 | `	}` |
|      3 | 3781 | `	return PH7_OK;` |
|      2 | 3782 | `}` |
|      - | 3783 | `/*` |
|      - | 3784 | ` * string ucwords(string $string [, string $separators = " \t\r\n\f\v"])` |
|      - | 3785 | ` *  Uppercase the first character of each word in a string.` |
|      - | 3786 | ` *  A word begins at the start of the string and after any character present in` |
|      - | 3787 | ` *  $separators. The default separators are the whitespace characters (space,` |
|      - | 3788 | ` *  horizontal tab, carriage return, newline, form-feed and vertical tab); an` |
|      - | 3789 | ` *  explicit $separators argument REPLACES them (an empty string leaves only the` |
|      - | 3790 | ` *  very first character upper-cased). Like PHP, this is byte-based: only ASCII` |
|      - | 3791 | ` *  bytes are upper-cased and a byte is a separator only if it appears in the set.` |
|      - | 3792 | ` * Parameters` |
|      - | 3793 | ` *  $string` |
|      - | 3794 | ` *   The input string.` |
|      - | 3795 | ` *  $separators` |
|      - | 3796 | ` *   The optional word-boundary characters.` |
|      - | 3797 | ` * Return` |
|      - | 3798 | ` *  The modified string.` |
|      - | 3799 | ` */` |
|     22 | 3800 | `static int PH7_builtin_ucwords(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3801 | `{` |
|      - | 3802 | `	const char *zIn;` |
|      - | 3803 | `	int nLen,i,iStart;` |
|      - | 3804 | `	char aDelim[256];` |
|     23 | 3805 | `	if( nArg < 1 ){` |
|      - | 3806 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3807 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3808 | `		return PH7_OK;` |
|      - | 3809 | `	}` |
|      - | 3810 | `	/* Build the separator membership table: an explicit $separators argument` |
|      - | 3811 | `	 * replaces the default whitespace set (an empty string clears it). */` |
|     23 | 3812 | `	SyZero(aDelim,(sxu32)sizeof(aDelim));` |
|     23 | 3813 | `	if( nArg > 1 ){` |
|      - | 3814 | `		int nDelim;` |
|      9 | 3815 | `		const char *zDelim = ph7_value_to_string(apArg[1],&nDelim);` |
|     17 | 3816 | `		for( i = 0 ; i < nDelim ; i++ ){` |
|      9 | 3817 | `			aDelim[(unsigned char)zDelim[i]] = 1;` |
|      5 | 3818 | `		}` |
|      5 | 3819 | `	}else{` |
|     15 | 3820 | `		aDelim[(unsigned char)' ']  = 1;` |
|     15 | 3821 | `		aDelim[(unsigned char)'\t'] = 1;` |
|     15 | 3822 | `		aDelim[(unsigned char)'\r'] = 1;` |
|     15 | 3823 | `		aDelim[(unsigned char)'\n'] = 1;` |
|     15 | 3824 | `		aDelim[(unsigned char)'\f'] = 1;` |
|     15 | 3825 | `		aDelim[(unsigned char)'\v'] = 1;` |
|      - | 3826 | `	}` |
|      - | 3827 | `	/* Extract the target string */` |
|     23 | 3828 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     23 | 3829 | `	if( nLen < 1 ){` |
|      - | 3830 | `		/* Empty string – match PHP semantics */` |
|      3 | 3831 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 3832 | `		return PH7_OK;` |
|      - | 3833 | `	}` |
|      - | 3834 | `	/* Upper-case the first byte of each word (the leading byte, or any byte that` |
|      - | 3835 | `	 * follows a separator), appending the untouched runs in between verbatim. */` |
|     21 | 3836 | `	iStart = 0;` |
|    309 | 3837 | `	for( i = 0 ; i < nLen ; i++ ){` |
|    289 | 3838 | `		int c = (unsigned char)zIn[i];` |
|    289 | 3839 | `		if( (i == 0 \|\| aDelim[(unsigned char)zIn[i-1]]) && c < 0x80 && SyisLower(c) ){` |
|     53 | 3840 | `			char up = (char)SyToUpper(c);` |
|     53 | 3841 | `			if( i > iStart ){` |
|     35 | 3842 | `				ph7_result_string(pCtx,&zIn[iStart],i - iStart);` |
|     17 | 3843 | `			}` |
|     53 | 3844 | `			ph7_result_string(pCtx,&up,1);` |
|     53 | 3845 | `			iStart = i + 1;` |
|     26 | 3846 | `		}` |
|    145 | 3847 | `	}` |
|     21 | 3848 | `	if( nLen > iStart ){` |
|     21 | 3849 | `		ph7_result_string(pCtx,&zIn[iStart],nLen - iStart);` |
|     10 | 3850 | `	}` |
|     21 | 3851 | `	return PH7_OK;` |
|     12 | 3852 | `}` |
|      - | 3853 | `/*` |
|      - | 3854 | ` * string str_repeat(string $input,int $multiplier)` |
|      - | 3855 | ` *  Returns input repeated multiplier times.` |
|      - | 3856 | ` * Parameters` |
|      - | 3857 | ` *  $string` |
|      - | 3858 | ` *   String to be repeated.` |
|      - | 3859 | ` * $multiplier` |
|      - | 3860 | ` *  Number of time the input string should be repeated.` |
|      - | 3861 | ` *  multiplier has to be greater than or equal to 0. If the multiplier is set` |
|      - | 3862 | ` *  to 0, the function will return an empty string.` |
|      - | 3863 | ` * Return` |
|      - | 3864 | ` *  The repeated string.` |
|      - | 3865 | ` */` |
|  20434 | 3866 | `static int PH7_builtin_str_repeat(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 3867 | `{` |
|      - | 3868 | `	const char *zIn;` |
|      - | 3869 | `	int nLen;` |
|      - | 3870 | `	ph7_int64 nMul;` |
|      - | 3871 | `	int rc;` |
|  20436 | 3872 | `	if( nArg < 2 ){` |
|      - | 3873 | `		/* Missing arguments,return NULL */` |
|    ! 0 | 3874 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3875 | `		return PH7_OK;` |
|      - | 3876 | `	}` |
|      - | 3877 | `	/* Extract the target string */` |
|  20436 | 3878 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      - | 3879 | `	/* Extract the multiplier as a 64-bit value (a 32-bit read would wrap a large` |
|      - | 3880 | `	 * positive $times into a negative one and trip a spurious ValueError). PHP` |
|      - | 3881 | `	 * validates $times regardless of the string contents: a negative count throws` |
|      - | 3882 | `	 * a catchable ValueError. */` |
|  20436 | 3883 | `	nMul = ph7_value_to_int64(apArg[1]);` |
|  20436 | 3884 | `	if( nMul < 0 ){` |
|      3 | 3885 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 3886 | `			"str_repeat(): Argument #2 ($times) must be greater than or equal to 0");` |
|      - | 3887 | `	}` |
|  20434 | 3888 | `	if( nLen < 1 \|\| nMul < 1 ){` |
|      - | 3889 | `		/* Empty input or a zero multiplier yields the empty string (PHP). */` |
|    ! 0 | 3890 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3891 | `		return PH7_OK;` |
|      - | 3892 | `	}` |
|      - | 3893 | `	/* Perform the requested operation */` |
| 221930 | 3894 | `	for(;;){` |
| 443862 | 3895 | `		if( !nMul ){` |
|  20434 | 3896 | `			break;` |
|      - | 3897 | `		}` |
|      - | 3898 | `		/* Append the copy */` |
| 423430 | 3899 | `		rc = ph7_result_string(pCtx,zIn,nLen);` |
| 423430 | 3900 | `		if( rc != PH7_OK ){` |
|      - | 3901 | `			/* Allocation failed: surface a fatal instead of returning a` |
|      - | 3902 | `			 * silently-truncated string with a success status. */` |
|    ! 0 | 3903 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 3904 | `		}` |
| 423430 | 3905 | `		nMul--;` |
|      2 | 3906 | `	}` |
|  20434 | 3907 | `	return PH7_OK;` |
|  10219 | 3908 | `}` |
|      - | 3909 | `/*` |
|      - | 3910 | ` * string nl2br(string $string[,bool $is_xhtml = true ])` |
|      - | 3911 | ` *  Inserts HTML line breaks before all newlines in a string.` |
|      - | 3912 | ` * Parameters` |
|      - | 3913 | ` *  $string` |
|      - | 3914 | ` *   The input string.` |
|      - | 3915 | ` * $is_xhtml` |
|      - | 3916 | ` *   Whenever to use XHTML compatible line breaks or not.` |
|      - | 3917 | ` * Return` |
|      - | 3918 | ` *  The processed string.` |
|      - | 3919 | ` */` |
|      4 | 3920 | `static int PH7_builtin_nl2br(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 3921 | `{` |
|      - | 3922 | `	const char *zIn,*zCur,*zEnd;` |
|      5 | 3923 | `	int is_xhtml = 1; /* Default to XHTML-style '<br />' like PHP */` |
|      - | 3924 | `	int nLen;` |
|      5 | 3925 | `	if( nArg < 1 ){` |
|      - | 3926 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 3927 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 3928 | `		return PH7_OK;` |
|      - | 3929 | `	}` |
|      - | 3930 | `	/* Extract the target string */` |
|      5 | 3931 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 3932 | `	if( nLen < 1 ){` |
|      - | 3933 | `		/* Empty string,return null */` |
|    ! 0 | 3934 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3935 | `		return PH7_OK;` |
|      - | 3936 | `	}` |
|      5 | 3937 | `	if( nArg > 1 ){` |
|      3 | 3938 | `		is_xhtml = ph7_value_to_bool(apArg[1]);` |
|      1 | 3939 | `	}` |
|      5 | 3940 | `	zEnd = &zIn[nLen];` |
|      - | 3941 | `	/* Perform the requested operation */` |
|      4 | 3942 | `	for(;;){` |
|      9 | 3943 | `		zCur = zIn;` |
|      - | 3944 | `		/* Delimit the string */` |
|     21 | 3945 | `		while( zIn < zEnd && (zIn[0] != '\n'&& zIn[0] != '\r') ){` |
|      9 | 3946 | `			zIn++;` |
|      1 | 3947 | `		}` |
|      9 | 3948 | `		if( zCur < zIn ){` |
|      - | 3949 | `			/* Output chunk verbatim */` |
|      9 | 3950 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      4 | 3951 | `		}` |
|      9 | 3952 | `		if( zIn >= zEnd ){` |
|      - | 3953 | `			/* No more input to process */` |
|      5 | 3954 | `			break;` |
|      - | 3955 | `		}` |
|      - | 3956 | `		/* Output the HTML line break */` |
|      - | 3957 | `		/* Follow PHP semantics: if is_xhtml is true, use '<br />' (space before the slash), otherwise use '<br>' */` |
|      5 | 3958 | `		if( is_xhtml ){` |
|      3 | 3959 | `			ph7_result_string(pCtx,"<br />",(int)sizeof("<br />")-1);` |
|      2 | 3960 | `		}else{` |
|      3 | 3961 | `			ph7_result_string(pCtx,"<br>",(int)sizeof("<br>")-1);` |
|      - | 3962 | `		}` |
|      5 | 3963 | `		zCur = zIn;` |
|      - | 3964 | `		/* Append trailing line */` |
|     11 | 3965 | `		while( zIn < zEnd && (zIn[0] == '\n'  \|\| zIn[0] == '\r') ){` |
|      5 | 3966 | `			zIn++;` |
|      1 | 3967 | `		}` |
|      5 | 3968 | `		if( zCur < zIn ){` |
|      - | 3969 | `			/* Output chunk verbatim */` |
|      5 | 3970 | `			ph7_result_string(pCtx,zCur,(int)(zIn-zCur));` |
|      2 | 3971 | `		}` |
|      1 | 3972 | `	}` |
|      5 | 3973 | `	return PH7_OK;` |
|      3 | 3974 | `}` |
|      - | 3975 | `/*` |
|      - | 3976 | ` * Format a given string and invoke the given callback on each processed chunk.` |
|      - | 3977 | ` *  According to the PHP reference manual.` |
|      - | 3978 | ` * The format string is composed of zero or more directives: ordinary characters` |
|      - | 3979 | ` * (excluding %) that are copied directly to the result, and conversion` |
|      - | 3980 | ` * specifications, each of which results in fetching its own parameter.` |
|      - | 3981 | ` * This applies to both sprintf() and printf().` |
|      - | 3982 | ` * Each conversion specification consists of a percent sign (%), followed by one` |
|      - | 3983 | ` * or more of these elements, in order:` |
|      - | 3984 | ` *   An optional sign specifier that forces a sign (- or +) to be used on a number.` |
|      - | 3985 | ` *   By default, only the - sign is used on a number if it's negative. This specifier forces` |
|      - | 3986 | ` *   positive numbers to have the + sign attached as well.` |
|      - | 3987 | ` *   An optional padding specifier that says what character will be used for padding` |
|      - | 3988 | ` *   the results to the right string size. This may be a space character or a 0 (zero character).` |
|      - | 3989 | ` *   The default is to pad with spaces. An alternate padding character can be specified by prefixing` |
|      - | 3990 | ` *   it with a single quote ('). See the examples below.` |
|      - | 3991 | ` *   An optional alignment specifier that says if the result should be left-justified or right-justified.` |
|      - | 3992 | ` *   The default is right-justified; a - character here will make it left-justified.` |
|      - | 3993 | ` *   An optional number, a width specifier that says how many characters (minimum) this conversion` |
|      - | 3994 | ` *   should result in.` |
|      - | 3995 | `` *   An optional precision specifier in the form of a period (`.') followed by an optional decimal`` |
|      - | 3996 | ` *   digit string that says how many decimal digits should be displayed for floating-point numbers.` |
|      - | 3997 | ` *   When using this specifier on a string, it acts as a cutoff point, setting a maximum character` |
|      - | 3998 | ` *   limit to the string.` |
|      - | 3999 | ` *  A type specifier that says what type the argument data should be treated as. Possible types:` |
|      - | 4000 | ` *       % - a literal percent character. No argument is required.` |
|      - | 4001 | ` *       b - the argument is treated as an integer, and presented as a binary number.` |
|      - | 4002 | ` *       c - the argument is treated as an integer, and presented as the character with that ASCII value.` |
|      - | 4003 | ` *       d - the argument is treated as an integer, and presented as a (signed) decimal number.` |
|      - | 4004 | ` *       e - the argument is treated as scientific notation (e.g. 1.2e+2). The precision specifier stands` |
|      - | 4005 | ` * 	     for the number of digits after the decimal point.` |
|      - | 4006 | ` *       E - like %e but uses uppercase letter (e.g. 1.2E+2).` |
|      - | 4007 | ` *       u - the argument is treated as an integer, and presented as an unsigned decimal number.` |
|      - | 4008 | ` *       f - the argument is treated as a float, and presented as a floating-point number (locale aware).` |
|      - | 4009 | ` *       F - the argument is treated as a float, and presented as a floating-point number (non-locale aware).` |
|      - | 4010 | ` *       g - shorter of %e and %f.` |
|      - | 4011 | ` *       G - shorter of %E and %f.` |
|      - | 4012 | ` *       o - the argument is treated as an integer, and presented as an octal number.` |
|      - | 4013 | ` *       s - the argument is treated as and presented as a string.` |
|      - | 4014 | ` *       x - the argument is treated as an integer and presented as a hexadecimal number (with lowercase letters).` |
|      - | 4015 | ` *       X - the argument is treated as an integer and presented as a hexadecimal number (with uppercase letters).` |
|      - | 4016 | ` */` |
|      - | 4017 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 4018 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 4019 | `#define PH7_FMT_BUFSIZ 1024 /* Conversion buffer size */` |
|      - | 4020 | `/*` |
|      - | 4021 | `** Conversion types fall into various categories as defined by the` |
|      - | 4022 | `** following enumeration.` |
|      - | 4023 | `*/` |
|      - | 4024 | `#define PH7_FMT_RADIX       1 /* Integer types.%d, %x, %o, and so forth */` |
|      - | 4025 | `#define PH7_FMT_FLOAT       2 /* Floating point.%f */` |
|      - | 4026 | `#define PH7_FMT_EXP         3 /* Exponentional notation.%e and %E */` |
|      - | 4027 | `#define PH7_FMT_GENERIC     4 /* Floating or exponential, depending on exponent.%g */` |
|      - | 4028 | `#define PH7_FMT_SIZE        5 /* Total number of characters processed so far.%n */` |
|      - | 4029 | `#define PH7_FMT_STRING      6 /* Strings.%s */` |
|      - | 4030 | `#define PH7_FMT_PERCENT     7 /* Percent symbol.%% */` |
|      - | 4031 | `#define PH7_FMT_CHARX       8 /* Characters.%c */` |
|      - | 4032 | `#define PH7_FMT_ERROR       9 /* Used to indicate no such conversion type */` |
|      - | 4033 |  |
|      - | 4034 | `/*` |
|      - | 4035 | `** Allowed values for ph7_fmt_info.flags` |
|      - | 4036 | `*/` |
|      - | 4037 | `#define PH7_FMT_FLAG_SIGNED	  0x01` |
|      - | 4038 | `#define PH7_FMT_FLAG_UNSIGNED 0x02` |
|      - | 4039 | `/*` |
|      - | 4040 | `** Each builtin conversion character (ex: the 'd' in "%d") is described` |
|      - | 4041 | `** by an instance of the following structure` |
|      - | 4042 | `*/` |
|      - | 4043 | `typedef struct ph7_fmt_info ph7_fmt_info;` |
|      - | 4044 | `struct ph7_fmt_info` |
|      - | 4045 | `{` |
|      - | 4046 | `  char fmttype;  /* The format field code letter [i.e: 'd','s','x'] */` |
|      - | 4047 | `  sxu8 base;     /* The base for radix conversion */` |
|      - | 4048 | `  int flags;    /* One or more of PH7_FMT_FLAG_ constants below */` |
|      - | 4049 | `  sxu8 type;     /* Conversion paradigm */` |
|      - | 4050 | `  char *charset; /* The character set for conversion */` |
|      - | 4051 | `  char *prefix;  /* Prefix on non-zero values in alt format */` |
|      - | 4052 | `};` |
|      - | 4053 | `/* PH7_PhpFloatShape (php's float-shape post-processing) lives in memobj.c —` |
|      - | 4054 | ` * the default float->string cast needs it even when this whole formatting` |
|      - | 4055 | ` * region is compiled out by PH7_DISABLE_DISK_IO. */` |
|      - | 4056 | `/*` |
|      - | 4057 | ` * The following table is searched linearly, so it is good to put the most frequently` |
|      - | 4058 | ` * used conversion types first.` |
|      - | 4059 | ` */` |
|      - | 4060 | `static const ph7_fmt_info aFmt[] = {` |
|      - | 4061 | `  {  'd', 10, PH7_FMT_FLAG_SIGNED, PH7_FMT_RADIX, "0123456789",0    },` |
|      - | 4062 | `  {  's',  0, 0, PH7_FMT_STRING,     0,                  0    },` |
|      - | 4063 | `  {  'c',  0, 0, PH7_FMT_CHARX,      0,                  0    },` |
|      - | 4064 | `  {  'x', 16, 0, PH7_FMT_RADIX,      "0123456789abcdef", "x0" },` |
|      - | 4065 | `  {  'X', 16, 0, PH7_FMT_RADIX,      "0123456789ABCDEF", "X0" },` |
|      - | 4066 | `  {  'b',  2, 0, PH7_FMT_RADIX,      "01",                "b0"},` |
|      - | 4067 | `  {  'o',  8, 0, PH7_FMT_RADIX,      "01234567",         "0"  },` |
|      - | 4068 | `  {  'u', 10, 0, PH7_FMT_RADIX,      "0123456789",       0    },` |
|      - | 4069 | `  {  'f',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4070 | `  {  'F',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_FLOAT,        0,    0    },` |
|      - | 4071 | `  {  'e',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "e",    0    },` |
|      - | 4072 | `  {  'E',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_EXP,        "E",    0    },` |
|      - | 4073 | `  {  'g',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 4074 | `  {  'G',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 4075 | `  /* php's 'h'/'H' are the locale-independent twins of 'g'/'G'; PHL always` |
|      - | 4076 | `   * formats in the C locale, so they behave identically. */` |
|      - | 4077 | `  {  'h',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "e",    0    },` |
|      - | 4078 | `  {  'H',  0, PH7_FMT_FLAG_SIGNED, PH7_FMT_GENERIC,    "E",    0    },` |
|      - | 4079 | `  {  '%',  0, 0, PH7_FMT_PERCENT,    0,                  0    }` |
|      - | 4080 | `};` |
|      - | 4081 | `/*` |
|      - | 4082 | ` * PHP 8 raises a catchable ValueError for an unknown conversion specifier` |
|      - | 4083 | ` * (e.g. "%y", or the C-ism "%#x" — '#' is not a php flag). Because printf()` |
|      - | 4084 | ` * and fprintf() stream their output incrementally while sprintf() buffers it,` |
|      - | 4085 | ` * every format builtin calls PH7_FormatValidate (below) to check the whole` |
|      - | 4086 | ` * format string BEFORE formatting so the throw happens with no partial output` |
|      - | 4087 | ` * escaping (php buffers the entire result and only emits it on success). This` |
|      - | 4088 | ` * scan mirrors the specifier-locating logic of the main format loop below.` |
|      - | 4089 | ` * On the first unknown specifier, stores it in *pBad and returns TRUE; returns` |
|      - | 4090 | ` * FALSE when every specifier is known. (A found-flag rather than a sentinel` |
|      - | 4091 | ` * char, so a NUL specifier byte — "%\0" — is still reported, not mistaken for` |
|      - | 4092 | ` * "all valid".)` |
|      - | 4093 | ` */` |
|    334 | 4094 | `static int FormatUnknownSpec(const char *zIn,int nByte,int *pBad)` |
|      1 | 4095 | `{` |
|    335 | 4096 | `	const char *zEnd = &zIn[nByte];` |
|      - | 4097 | `	int c,idx;` |
|   3165 | 4098 | `	while( zIn < zEnd ){` |
|   2851 | 4099 | `		if( zIn[0] != '%' ){` |
|   2201 | 4100 | `			zIn++;` |
|   2201 | 4101 | `			continue;` |
|      - | 4102 | `		}` |
|    651 | 4103 | `		zIn++; /* jump the percent sign */` |
|      - | 4104 | `		/* php-supported flags: '-', '+', ' ', '0' and the "'<pad>'" custom-pad` |
|      - | 4105 | `		 * form. '#' is intentionally NOT treated as a flag so it surfaces as an` |
|      - | 4106 | `		 * unknown specifier, matching php. */` |
|    693 | 4107 | `		while( zIn < zEnd ){` |
|    691 | 4108 | `			c = zIn[0];` |
|    691 | 4109 | `			if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|     43 | 4110 | `				zIn++;` |
|     43 | 4111 | `				continue;` |
|      - | 4112 | `			}` |
|    649 | 4113 | `			if( c=='\'' ){` |
|    ! 0 | 4114 | `				zIn++;` |
|    ! 0 | 4115 | `				if( zIn < zEnd ){` |
|    ! 0 | 4116 | `					zIn++; /* the custom pad character */` |
|    ! 0 | 4117 | `				}` |
|    ! 0 | 4118 | `				continue;` |
|      - | 4119 | `			}` |
|    649 | 4120 | `			break;` |
|    ! 0 | 4121 | `		}` |
|      - | 4122 | `		/* field width */` |
|    725 | 4123 | `		while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|     75 | 4124 | `			zIn++;` |
|      1 | 4125 | `		}` |
|      - | 4126 | `		/* positional specifier ($) — php parses flags AFTER it (e.g. "%1$-10s"),` |
|      - | 4127 | `		 * so skip the full flag set and width again, mirroring the main loop. */` |
|    651 | 4128 | `		if( zIn < zEnd && zIn[0]=='$' ){` |
|    ! 0 | 4129 | `			zIn++;` |
|    ! 0 | 4130 | `			while( zIn < zEnd ){` |
|    ! 0 | 4131 | `				c = zIn[0];` |
|    ! 0 | 4132 | `				if( c=='-' \|\| c=='+' \|\| c==' ' \|\| c=='0' ){` |
|    ! 0 | 4133 | `					zIn++;` |
|    ! 0 | 4134 | `					continue;` |
|      - | 4135 | `				}` |
|    ! 0 | 4136 | `				if( c=='\'' ){` |
|    ! 0 | 4137 | `					zIn++;` |
|    ! 0 | 4138 | `					if( zIn < zEnd ){` |
|    ! 0 | 4139 | `						zIn++;` |
|    ! 0 | 4140 | `					}` |
|    ! 0 | 4141 | `					continue;` |
|      - | 4142 | `				}` |
|    ! 0 | 4143 | `				break;` |
|    ! 0 | 4144 | `			}` |
|    ! 0 | 4145 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|    ! 0 | 4146 | `				zIn++;` |
|    ! 0 | 4147 | `			}` |
|    ! 0 | 4148 | `		}` |
|      - | 4149 | `		/* precision */` |
|    651 | 4150 | `		if( zIn < zEnd && zIn[0]=='.' ){` |
|     87 | 4151 | `			zIn++;` |
|    183 | 4152 | `			while( zIn < zEnd && zIn[0]>='0' && zIn[0]<='9' ){` |
|     97 | 4153 | `				zIn++;` |
|      1 | 4154 | `			}` |
|     43 | 4155 | `		}` |
|      - | 4156 | `		/* a single 'l' length modifier (ignored, php compat) */` |
|    651 | 4157 | `		if( zIn < zEnd && zIn[0]=='l' ){` |
|     11 | 4158 | `			zIn++;` |
|      5 | 4159 | `		}` |
|    651 | 4160 | `		if( zIn >= zEnd ){` |
|      - | 4161 | `			/* A dangling '%' with no specifier: PHL's legacy path silently` |
|      - | 4162 | `			 * truncates here (recorded residual); nothing to validate. */` |
|      3 | 4163 | `			break;` |
|      - | 4164 | `		}` |
|    649 | 4165 | `		c = zIn[0];` |
|    649 | 4166 | `		zIn++; /* jump the conversion specifier */` |
|   3191 | 4167 | `		for( idx = 0 ; idx < (int)SX_ARRAYSIZE(aFmt) ; idx++ ){` |
|   3173 | 4168 | `			if( c == aFmt[idx].fmttype ){` |
|    631 | 4169 | `				break;` |
|      - | 4170 | `			}` |
|   1272 | 4171 | `		}` |
|    649 | 4172 | `		if( idx >= (int)SX_ARRAYSIZE(aFmt) ){` |
|     19 | 4173 | `			*pBad = c; /* unknown specifier */` |
|     19 | 4174 | `			return TRUE;` |
|      - | 4175 | `		}` |
|      1 | 4176 | `	}` |
|    317 | 4177 | `	return FALSE;` |
|    168 | 4178 | `}` |
|      - | 4179 | `/*` |
|      - | 4180 | ` * Validate a printf-style format string. PHP 8 raises a catchable ValueError for` |
|      - | 4181 | ` * an unknown conversion specifier, thrown before any output is produced. Every` |
|      - | 4182 | ` * format builtin (sprintf/printf/vprintf/vsprintf/fprintf/vfprintf) calls this` |
|      - | 4183 | ` * up-front, then propagates the returned status verbatim (PH7_EXCEPTION when the` |
|      - | 4184 | ` * throw is caught in place, PH7_ABORT when it goes uncaught).` |
|      - | 4185 | ` * Returns PH7_OK when the format is valid.` |
|      - | 4186 | ` */` |
|    334 | 4187 | `PH7_PRIVATE sxi32 PH7_FormatValidate(ph7_context *pCtx,const char *zFormat,int nByte)` |
|      1 | 4188 | `{` |
|    335 | 4189 | `	int badSpec = 0;` |
|    335 | 4190 | `	if( FormatUnknownSpec(zFormat,nByte,&badSpec) ){` |
|     28 | 4191 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      9 | 4192 | `			"Unknown format specifier \"%c\"",badSpec);` |
|      - | 4193 | `	}` |
|    317 | 4194 | `	return PH7_OK;` |
|    168 | 4195 | `}` |
|      - | 4196 | `/*` |
|      - | 4197 | `` * PHP 8: a printf-family `$format` argument is a `string` parameter — scalars`` |
|      - | 4198 | ` * (int/float/bool) and null coerce to a string, but an array/object/resource` |
|      - | 4199 | ` * raises a catchable TypeError. iArg is the 1-based argument position ($format` |
|      - | 4200 | ` * is #1 for sprintf/printf/vprintf/vsprintf, #2 for fprintf/vfprintf). Returns` |
|      - | 4201 | ` * PH7_OK when the value is string-coercible (the caller then uses` |
|      - | 4202 | ` * ph7_value_to_string, which renders scalars/null verbatim).` |
|      - | 4203 | ` */` |
|    354 | 4204 | `PH7_PRIVATE sxi32 PH7_FormatCheckFormatArg(ph7_context *pCtx,ph7_value *pArg,int iArg)` |
|      1 | 4205 | `{` |
|    355 | 4206 | `	if( ph7_value_is_array(pArg) \|\| ph7_value_is_object(pArg) \|\| ph7_value_is_resource(pArg) ){` |
|      - | 4207 | `		char zBuf[64];` |
|     13 | 4208 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4209 | `			"%s(): Argument #%d ($format) must be of type string, %s given",` |
|      4 | 4210 | `			ph7_function_name(pCtx),iArg,VmValueGivenName(pArg,zBuf,sizeof(zBuf)));` |
|      - | 4211 | `	}` |
|    347 | 4212 | `	return PH7_OK;` |
|    178 | 4213 | `}` |
|      - | 4214 | `/*` |
|      - | 4215 | ` * Format a given string.` |
|      - | 4216 | ` * The root program.  All variations call this core.` |
|      - | 4217 | ` * INPUTS:` |
|      - | 4218 | ` *   xConsumer   This is a pointer to a function taking four arguments` |
|      - | 4219 | ` *            1. A pointer to the call context.` |
|      - | 4220 | ` *            2. A pointer to the list of characters to be output` |
|      - | 4221 | ` *               (Note, this list is NOT null terminated.)` |
|      - | 4222 | ` *            3. An integer number of characters to be output.` |
|      - | 4223 | ` *               (Note: This number might be zero.)` |
|      - | 4224 | ` *            4. Upper layer private data.` |
|      - | 4225 | ` *   zIn       This is the format string, as in the usual print.` |
|      - | 4226 | ` *   apArg     This is a pointer to a list of arguments.` |
|      - | 4227 | ` */` |
|    316 | 4228 | `PH7_PRIVATE sxi32 PH7_InputFormat(` |
|      - | 4229 | `	int (*xConsumer)(ph7_context *,const char *,int,void *), /* Format consumer */` |
|      - | 4230 | `	ph7_context *pCtx,  /* call context */` |
|      - | 4231 | `	const char *zIn,    /* Format string */` |
|      - | 4232 | `	int nByte,          /* Format string length */` |
|      - | 4233 | `	int nArg,           /* Total argument of the given arguments */` |
|      - | 4234 | `	ph7_value **apArg,  /* User arguments */` |
|      - | 4235 | `	void *pUserData,    /* Last argument to xConsumer() */` |
|      - | 4236 | `	int vf              /* TRUE if called from vfprintf,vsprintf context */` |
|      - | 4237 | `	)` |
|      1 | 4238 | `{` |
|    317 | 4239 | `	char spaces[] = "                                                  ";` |
|      - | 4240 | `#define etSPACESIZE ((int)sizeof(spaces)-1)` |
|    317 | 4241 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 4242 | `	char *zBuf,zWorker[PH7_FMT_BUFSIZ];       /* Working buffer */` |
|      - | 4243 | `	const ph7_fmt_info *pInfo;  /* Pointer to the appropriate info structure */` |
|      - | 4244 | `	int flag_alternateform; /* True if "#" flag is present */` |
|      - | 4245 | `	int flag_leftjustify;   /* True if "-" flag is present */` |
|      - | 4246 | `	int flag_blanksign;     /* True if " " flag is present */` |
|      - | 4247 | `	int flag_plussign;      /* True if "+" flag is present */` |
|      - | 4248 | `	int flag_zeropad;       /* True if field width constant starts with zero */` |
|      - | 4249 | `	ph7_value *pArg;         /* Current processed argument */` |
|      - | 4250 | `	ph7_int64 iVal;` |
|      - | 4251 | `	int precision;           /* Precision of the current field */` |
|      - | 4252 | `	/* zExtra (unused) removed to prevent compiler warning. */` |
|      - | 4253 | `	int c,rc,n;` |
|      - | 4254 | `	int length;              /* Length of the field */` |
|      - | 4255 | `	int prefix;` |
|      - | 4256 | `	sxu8 xtype;              /* Conversion paradigm */` |
|      - | 4257 | `	int width;               /* Width of the current field */` |
|      - | 4258 | `	int idx;` |
|    317 | 4259 | `	n = (vf == TRUE) ? 0 : 1;` |
|      - | 4260 | `#define NEXT_ARG	( n < nArg ? apArg[n++] : 0 )` |
|      - | 4261 | `	/* An unknown conversion specifier is rejected up-front by PH7_FormatValidate()` |
|      - | 4262 | `	 * (called by every format builtin before this routine), so the specifier set` |
|      - | 4263 | `	 * seen here is always valid. */` |
|      - | 4264 | `	/* Start the format process */` |
|    473 | 4265 | `	for(;;){` |
|    947 | 4266 | `		zCur = zIn;` |
|   3133 | 4267 | `		while( zIn < zEnd && zIn[0] != '%' ){` |
|   2187 | 4268 | `			zIn++;` |
|      1 | 4269 | `		}` |
|    947 | 4270 | `		if( zCur < zIn ){` |
|      - | 4271 | `			/* Consume chunk verbatim */` |
|    661 | 4272 | `			rc = xConsumer(pCtx,zCur,(int)(zIn-zCur),pUserData);` |
|    661 | 4273 | `			if( rc != SXRET_OK ){` |
|      - | 4274 | `				/* Callback requested an abort (e.g. an allocation failure) */` |
|    ! 0 | 4275 | `				break;` |
|      - | 4276 | `			}` |
|    330 | 4277 | `		}` |
|    947 | 4278 | `		if( zIn >= zEnd ){` |
|      - | 4279 | `			/* No more input to process,break immediately */` |
|    315 | 4280 | `			break;` |
|      - | 4281 | `		}` |
|      - | 4282 | `		/* Find out what flags are present */` |
|    633 | 4283 | `		flag_leftjustify = flag_plussign = flag_blanksign =` |
|    632 | 4284 | `			flag_alternateform = flag_zeropad = 0;` |
|    633 | 4285 | `		zIn++; /* Jump the precent sign */` |
|    316 | 4286 | `		do{` |
|    675 | 4287 | `			c = zIn[0];` |
|    675 | 4288 | `			switch( c ){` |
|     15 | 4289 | `			case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|      7 | 4290 | `			case '+':   flag_plussign = 1;        c = 0;   break;` |
|      7 | 4291 | `			case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|     17 | 4292 | `			case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 4293 | `			case '\'':` |
|    ! 0 | 4294 | `				zIn++;` |
|    ! 0 | 4295 | `				if( zIn < zEnd ){` |
|      - | 4296 | `					/* An alternate padding character can be specified by prefixing it with a single quote (') */` |
|    ! 0 | 4297 | `					c = zIn[0];` |
|    ! 0 | 4298 | `					for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4299 | `						spaces[idx] = (char)c;` |
|    ! 0 | 4300 | `					}` |
|    ! 0 | 4301 | `					c = 0;` |
|    ! 0 | 4302 | `				}` |
|    ! 0 | 4303 | `				break;` |
|    632 | 4304 | `			default:                                       break;` |
|      - | 4305 | `			}` |
|    675 | 4306 | `		}while( c==0 && (zIn++ < zEnd) );` |
|      - | 4307 | `		/* Get the field width */` |
|    633 | 4308 | `		width = 0;` |
|   1023 | 4309 | `		while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     75 | 4310 | `			width = width*10 + (zIn[0] - '0');` |
|     75 | 4311 | `			zIn++;` |
|      1 | 4312 | `		}` |
|    633 | 4313 | `		if( zIn < zEnd && zIn[0] == '$' ){` |
|      - | 4314 | `			/* Position specifer */` |
|    ! 0 | 4315 | `			if( width > 0 ){` |
|    ! 0 | 4316 | `				n = width;` |
|    ! 0 | 4317 | `				if( vf && n > 0 ){` |
|    ! 0 | 4318 | `					n--;` |
|    ! 0 | 4319 | `				}` |
|    ! 0 | 4320 | `			}` |
|    ! 0 | 4321 | `			zIn++;` |
|    ! 0 | 4322 | `			width = 0;` |
|      - | 4323 | `			/* php's grammar is %argnum$<flags><width>: the flags come AFTER the` |
|      - | 4324 | `			 * positional, so re-parse the full flag set here (e.g. "%1$-10s"),` |
|      - | 4325 | `			 * not just zero-padding. */` |
|    ! 0 | 4326 | `			do{` |
|    ! 0 | 4327 | `				c = zIn[0];` |
|    ! 0 | 4328 | `				switch( c ){` |
|    ! 0 | 4329 | `				case '-':   flag_leftjustify = 1;     c = 0;   break;` |
|    ! 0 | 4330 | `				case '+':   flag_plussign = 1;        c = 0;   break;` |
|    ! 0 | 4331 | `				case ' ':   flag_blanksign = 1;       c = 0;   break;` |
|    ! 0 | 4332 | `				case '0':   flag_zeropad = 1;         c = 0;   break;` |
|    ! 0 | 4333 | `				case '\'':` |
|    ! 0 | 4334 | `					zIn++;` |
|    ! 0 | 4335 | `					if( zIn < zEnd ){` |
|    ! 0 | 4336 | `						c = zIn[0];` |
|    ! 0 | 4337 | `						for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4338 | `							spaces[idx] = (char)c;` |
|    ! 0 | 4339 | `						}` |
|    ! 0 | 4340 | `						c = 0;` |
|    ! 0 | 4341 | `					}` |
|    ! 0 | 4342 | `					break;` |
|    ! 0 | 4343 | `				default:                                       break;` |
|      - | 4344 | `				}` |
|    ! 0 | 4345 | `			}while( c==0 && (zIn++ < zEnd) );` |
|    ! 0 | 4346 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|    ! 0 | 4347 | `				width = width*10 + (zIn[0] - '0');` |
|    ! 0 | 4348 | `				zIn++;` |
|    ! 0 | 4349 | `			}` |
|    ! 0 | 4350 | `		}` |
|    633 | 4351 | `		if( width > PH7_FMT_BUFSIZ-10 ){` |
|    ! 0 | 4352 | `			width = PH7_FMT_BUFSIZ-10;` |
|    ! 0 | 4353 | `		}` |
|      - | 4354 | `		/* Get the precision */` |
|    633 | 4355 | `		precision = -1;` |
|    633 | 4356 | `		if( zIn < zEnd && zIn[0] == '.' ){` |
|     87 | 4357 | `			precision = 0;` |
|     87 | 4358 | `			zIn++;` |
|    226 | 4359 | `			while( zIn < zEnd && ( zIn[0] >='0' && zIn[0] <='9') ){` |
|     97 | 4360 | `				precision = precision*10 + (zIn[0] - '0');` |
|     97 | 4361 | `				zIn++;` |
|      1 | 4362 | `			}` |
|     43 | 4363 | `		}` |
|      - | 4364 | `		/* Consume a single 'l' length modifier (a C-ism php accepts and ignores,` |
|      - | 4365 | `		 * e.g. "%ld"); PH7_FormatValidate mirrors this. Exactly one is skipped:` |
|      - | 4366 | `		 * in "%lld" the second 'l' becomes the (unknown) specifier, just like php. */` |
|    633 | 4367 | `		if( zIn < zEnd && zIn[0] == 'l' ){` |
|      9 | 4368 | `			zIn++;` |
|      4 | 4369 | `		}` |
|    633 | 4370 | `		if( zIn >= zEnd ){` |
|      - | 4371 | `			/* No more input */` |
|      3 | 4372 | `			break;` |
|      - | 4373 | `		}` |
|      - | 4374 | `		/* Fetch the info entry for the field */` |
|    631 | 4375 | `		pInfo = 0;` |
|    631 | 4376 | `		xtype = PH7_FMT_ERROR;` |
|    631 | 4377 | `		c = zIn[0];` |
|    631 | 4378 | `		zIn++; /* Jump the format specifer */` |
|   2867 | 4379 | `		for(idx=0; idx< (int)SX_ARRAYSIZE(aFmt); idx++){` |
|   2867 | 4380 | `			if( c==aFmt[idx].fmttype ){` |
|    631 | 4381 | `				pInfo = &aFmt[idx];` |
|    631 | 4382 | `				xtype = pInfo->type;` |
|    631 | 4383 | `				break;` |
|      - | 4384 | `			}` |
|   1119 | 4385 | `		}` |
|    631 | 4386 | `		zBuf = zWorker; /* Point to the working buffer */` |
|    631 | 4387 | `		length = 0;` |
|      - | 4388 | `		/* zExtra previously assigned here; not used anywhere, removed. */` |
|      - | 4389 | `		 /*` |
|      - | 4390 | `		  ** At this point, variables are initialized as follows:` |
|      - | 4391 | `		  **` |
|      - | 4392 | `		  **   flag_alternateform          TRUE if a '#' is present.` |
|      - | 4393 | `		  **   flag_plussign               TRUE if a '+' is present.` |
|      - | 4394 | `		  **   flag_leftjustify            TRUE if a '-' is present or if the` |
|      - | 4395 | `		  **                               field width was negative.` |
|      - | 4396 | `		  **   flag_zeropad                TRUE if the width began with 0.` |
|      - | 4397 | `		  **                               the conversion character.` |
|      - | 4398 | `		  **   flag_blanksign              TRUE if a ' ' is present.` |
|      - | 4399 | `		  **   width                       The specified field width.  This is` |
|      - | 4400 | `		  **                               always non-negative.  Zero is the default.` |
|      - | 4401 | `		  **   precision                   The specified precision.  The default` |
|      - | 4402 | `		  **                               is -1.` |
|      - | 4403 | `		  */` |
|    631 | 4404 | `		switch(xtype){` |
|      3 | 4405 | `		case PH7_FMT_PERCENT:` |
|      - | 4406 | `			/* A literal percent character */` |
|      7 | 4407 | `			zWorker[0] = '%';` |
|      7 | 4408 | `			length = (int)sizeof(char);` |
|      7 | 4409 | `			break;` |
|      3 | 4410 | `		case PH7_FMT_CHARX:` |
|      - | 4411 | `			/* The argument is treated as an integer, and presented as the character` |
|      - | 4412 | `			 * with that ASCII value` |
|      - | 4413 | `			 */` |
|      7 | 4414 | `			pArg = NEXT_ARG;` |
|      7 | 4415 | `			if( pArg == 0 ){` |
|      3 | 4416 | `				c = 0;` |
|      2 | 4417 | `			}else{` |
|      5 | 4418 | `				c = ph7_value_to_int(pArg);` |
|      - | 4419 | `			}` |
|      - | 4420 | `			/* NUL byte is an acceptable value */` |
|      7 | 4421 | `			zWorker[0] = (char)c;` |
|      7 | 4422 | `			length = (int)sizeof(char);` |
|      7 | 4423 | `			break;` |
|    162 | 4424 | `		case PH7_FMT_STRING:` |
|      - | 4425 | `			/* the argument is treated as and presented as a string */` |
|    325 | 4426 | `			pArg = NEXT_ARG;` |
|    325 | 4427 | `			if( pArg == 0 ){` |
|    ! 0 | 4428 | `				length = 0;` |
|    ! 0 | 4429 | `			}else{` |
|    325 | 4430 | `				zBuf = (char *)ph7_value_to_string(pArg,&length);` |
|      - | 4431 | `			}` |
|    325 | 4432 | `			if( length < 1 ){` |
|    ! 0 | 4433 | `				zBuf = " ";` |
|    ! 0 | 4434 | `				length = (int)sizeof(char);` |
|    ! 0 | 4435 | `			}` |
|    325 | 4436 | `			if( precision>=0 && precision<length ){` |
|      3 | 4437 | `				length = precision;` |
|      1 | 4438 | `			}` |
|    325 | 4439 | `			if( flag_zeropad ){` |
|      - | 4440 | `				/* zero-padding works on strings too */` |
|    ! 0 | 4441 | `				for(idx = 0 ; idx < etSPACESIZE ; ++idx ){` |
|    ! 0 | 4442 | `					spaces[idx] = '0';` |
|    ! 0 | 4443 | `				}` |
|    ! 0 | 4444 | `			}` |
|    325 | 4445 | `			break;` |
|     59 | 4446 | `		case PH7_FMT_RADIX:` |
|    119 | 4447 | `			pArg = NEXT_ARG;` |
|    119 | 4448 | `			if( pArg == 0 ){` |
|    ! 0 | 4449 | `				iVal = 0;` |
|    ! 0 | 4450 | `			}else{` |
|    119 | 4451 | `				iVal = ph7_value_to_int64(pArg);` |
|      - | 4452 | `			}` |
|      - | 4453 | `			/* Limit the precision to prevent overflowing buf[] during conversion */` |
|    119 | 4454 | `			if( precision>PH7_FMT_BUFSIZ-40 ){` |
|    ! 0 | 4455 | `				precision = PH7_FMT_BUFSIZ-40;` |
|    ! 0 | 4456 | `			}` |
|      - | 4457 | `#if 1` |
|      - | 4458 | `        /* For the format %#x, the value zero is printed "0" not "0x0".` |
|      - | 4459 | `        ** I think this is stupid.*/` |
|    119 | 4460 | `        if( iVal==0 ) flag_alternateform = 0;` |
|      - | 4461 | `#else` |
|      - | 4462 | `        /* More sensible: turn off the prefix for octal (to prevent "00"),` |
|      - | 4463 | `        ** but leave the prefix for hex.*/` |
|      - | 4464 | `        if( iVal==0 && pInfo->base==8 ) flag_alternateform = 0;` |
|      - | 4465 | `#endif` |
|    119 | 4466 | `        if( pInfo->flags & PH7_FMT_FLAG_SIGNED ){` |
|     95 | 4467 | `          if( iVal<0 ){` |
|     25 | 4468 | `            iVal = -iVal;` |
|      - | 4469 | `			/* Ticket 1433-003 */` |
|     25 | 4470 | `			if( iVal < 0 ){` |
|      - | 4471 | `				/* Overflow */` |
|    ! 0 | 4472 | `				iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4473 | `			}` |
|     25 | 4474 | `            prefix = '-';` |
|     83 | 4475 | `          }else if( flag_plussign )  prefix = '+';` |
|     69 | 4476 | `          else if( flag_blanksign )  prefix = ' ';` |
|     67 | 4477 | `          else                       prefix = 0;` |
|     48 | 4478 | `        }else{` |
|     25 | 4479 | `			if( iVal<0 ){` |
|    ! 0 | 4480 | `				iVal = -iVal;` |
|      - | 4481 | `				/* Ticket 1433-003 */` |
|    ! 0 | 4482 | `				if( iVal < 0 ){` |
|      - | 4483 | `					/* Overflow */` |
|    ! 0 | 4484 | `					iVal= 0x7FFFFFFFFFFFFFFF;` |
|    ! 0 | 4485 | `				}` |
|    ! 0 | 4486 | `			}` |
|     25 | 4487 | `			prefix = 0;` |
|      - | 4488 | `		}` |
|    119 | 4489 | `        if( flag_zeropad && precision<width-(prefix!=0) ){` |
|      7 | 4490 | `          precision = width-(prefix!=0);` |
|      3 | 4491 | `        }` |
|    119 | 4492 | `        zBuf = &zWorker[PH7_FMT_BUFSIZ-1];` |
|      - | 4493 | `        {` |
|      - | 4494 | `          register char *cset;      /* Use registers for speed */` |
|      - | 4495 | `          register int base;` |
|    119 | 4496 | `          cset = pInfo->charset;` |
|    119 | 4497 | `          base = pInfo->base;` |
|     59 | 4498 | `          do{                                           /* Convert to ascii */` |
|    185 | 4499 | `            *(--zBuf) = cset[iVal%base];` |
|    185 | 4500 | `            iVal = iVal/base;` |
|    185 | 4501 | `          }while( iVal>0 );` |
|      - | 4502 | `        }` |
|    119 | 4503 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    141 | 4504 | `        for(idx=precision-length; idx>0; idx--){` |
|     23 | 4505 | `          *(--zBuf) = '0';                             /* Zero pad */` |
|     12 | 4506 | `        }` |
|    119 | 4507 | `        if( prefix ) *(--zBuf) = (char)prefix;               /* Add sign */` |
|    119 | 4508 | `        if( flag_alternateform && pInfo->prefix ){      /* Add "0" or "0x" */` |
|      - | 4509 | `          char *pre, x;` |
|    ! 0 | 4510 | `          pre = pInfo->prefix;` |
|    ! 0 | 4511 | `          if( *zBuf!=pre[0] ){` |
|    ! 0 | 4512 | `            for(pre=pInfo->prefix; (x=(*pre))!=0; pre++) *(--zBuf) = x;` |
|    ! 0 | 4513 | `          }` |
|    ! 0 | 4514 | `        }` |
|    119 | 4515 | `		length = (int)(&zWorker[PH7_FMT_BUFSIZ-1]-zBuf);` |
|    119 | 4516 | `		break;` |
|     88 | 4517 | `		case PH7_FMT_FLOAT:` |
|      - | 4518 | `		case PH7_FMT_EXP:` |
|      - | 4519 | `		case PH7_FMT_GENERIC:{` |
|      - | 4520 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - | 4521 | `		double realvalue;` |
|      - | 4522 | `		char zFmt[8];` |
|      - | 4523 | `		int nOut, nFmt;` |
|    177 | 4524 | `		pArg = NEXT_ARG;` |
|    177 | 4525 | `		if( pArg == 0 ){` |
|    ! 0 | 4526 | `			realvalue = 0;` |
|    ! 0 | 4527 | `		}else{` |
|    177 | 4528 | `			realvalue = ph7_value_to_double(pArg);` |
|      - | 4529 | `		}` |
|      - | 4530 | `		/* php prints the IEEE specials bare — NaN / INF / -INF with no width` |
|      - | 4531 | `		 * padding, precision, or sign flags (php_sprintf_appenddouble). */` |
|    177 | 4532 | `		if( PH7_IS_NAN(realvalue) ){` |
|     21 | 4533 | `			zBuf = "NaN";` |
|     21 | 4534 | `			length = 3;` |
|     21 | 4535 | `			width = 0;` |
|     21 | 4536 | `			break;` |
|      - | 4537 | `		}` |
|    157 | 4538 | `		if( PH7_IS_INF(realvalue) ){` |
|     37 | 4539 | `			if( realvalue < 0.0 ){` |
|     15 | 4540 | `				zBuf = "-INF";` |
|     15 | 4541 | `				length = 4;` |
|      8 | 4542 | `			}else{` |
|     23 | 4543 | `				zBuf = "INF";` |
|     23 | 4544 | `				length = 3;` |
|      - | 4545 | `			}` |
|     37 | 4546 | `			width = 0;` |
|     37 | 4547 | `			break;` |
|      - | 4548 | `		}` |
|    121 | 4549 | `		if( precision<0 ) precision = 6;         /* Set default precision */` |
|    121 | 4550 | `		if( precision > 53 ){` |
|      - | 4551 | `			/* php's FORMAT_CONV_MAX_PRECISION cap, with the same E_NOTICE` |
|      - | 4552 | `			 * (message prefixed with the active function's name, like` |
|      - | 4553 | `			 * php_error_docref). */` |
|      - | 4554 | `			char zMsg[160];` |
|      4 | 4555 | `			SyBufferFormat(zMsg,sizeof(zMsg),` |
|      - | 4556 | `				"%z(): Requested precision of %d digits was truncated to PHP maximum of %d digits",` |
|      2 | 4557 | `				&pCtx->pFunc->sName,precision,53);` |
|      3 | 4558 | `			PH7_VmThrowError(pCtx->pVm,0,E_NOTICE,zMsg);` |
|      3 | 4559 | `			precision = 53;` |
|      1 | 4560 | `		}` |
|      - | 4561 | ``		/* php's %f/%e extract the sign via `num < 0`, so negative zero prints`` |
|      - | 4562 | `		 * unsigned there — while %g (php_gcvt on the raw value) keeps "-0". */` |
|    121 | 4563 | `		if( xtype!=PH7_FMT_GENERIC && realvalue == 0.0 ){` |
|      9 | 4564 | `			realvalue = 0.0;` |
|      4 | 4565 | `		}` |
|      - | 4566 | `		/* php's float conversions are correctly rounded (zend_dtoa); use libc` |
|      - | 4567 | `		 * snprintf as the digit engine (the byte-exact-floats rule — the old` |
|      - | 4568 | `		 * hand-rolled vxGetdigit loop stopped at 16 significant digits, so` |
|      - | 4569 | `		 * e.g. %f of 1e308 printed zeros where php prints the exact binary64` |
|      - | 4570 | `		 * expansion), then post-process into php's exact shapes below. */` |
|    121 | 4571 | `		nFmt = 0;` |
|    121 | 4572 | `		zFmt[nFmt++] = '%';` |
|    121 | 4573 | `		if( flag_alternateform ) zFmt[nFmt++] = '#';` |
|      - | 4574 | `		/* php's ' ' flag selects space PADDING (its default), not C's` |
|      - | 4575 | `		 * space-for-positive-sign — so flag_blanksign is NOT forwarded. */` |
|    121 | 4576 | `		if( flag_plussign ) zFmt[nFmt++] = '+';` |
|    121 | 4577 | `		zFmt[nFmt++] = '.';` |
|    121 | 4578 | `		zFmt[nFmt++] = '*';` |
|    165 | 4579 | `		zFmt[nFmt++] = (char)(xtype==PH7_FMT_FLOAT ? 'f' :` |
|     30 | 4580 | `			(xtype==PH7_FMT_EXP ? ((pInfo->charset[0]=='E') ? 'E' : 'e')` |
|     28 | 4581 | `			                    : ((pInfo->charset[0]=='E') ? 'G' : 'g')));` |
|    121 | 4582 | `		zFmt[nFmt] = 0;` |
|    121 | 4583 | `		nOut = snprintf(zWorker,sizeof(zWorker),zFmt,precision,realvalue);` |
|    121 | 4584 | `		if( nOut < 0 \|\| nOut >= (int)sizeof(zWorker) ){` |
|      - | 4585 | `			/* Cannot happen with precision capped at 53 (%f of DBL_MAX is` |
|      - | 4586 | `			 * ~365 bytes); keep the truncated output rather than overrun. */` |
|    ! 0 | 4587 | `			nOut = (int)SyStrlen(zWorker);` |
|    ! 0 | 4588 | `		}` |
|    121 | 4589 | `		nOut = (int)PH7_PhpFloatShape(zWorker,(sxi32)nOut,xtype==PH7_FMT_GENERIC);` |
|    121 | 4590 | `		zBuf = zWorker;` |
|    121 | 4591 | `		length = nOut;` |
|      - | 4592 | `		/* Let the zero-pad block below insert zeros between the sign (written` |
|      - | 4593 | `		 * by snprintf) and the first digit, as before. */` |
|    121 | 4594 | `		prefix = (zWorker[0]=='-' \|\| zWorker[0]=='+' \|\| zWorker[0]==' ') ? zWorker[0] : 0;` |
|      - | 4595 | `        /* Special case:  Add leading zeros if the flag_zeropad flag is` |
|      - | 4596 | `        ** set and we are not left justified */` |
|    121 | 4597 | `        if( flag_zeropad && !flag_leftjustify && length < width){` |
|      - | 4598 | `          int i;` |
|      7 | 4599 | `          int nPad = width - length;` |
|     51 | 4600 | `          for(i=width; i>=nPad; i--){` |
|     45 | 4601 | `            zBuf[i] = zBuf[i-nPad];` |
|     23 | 4602 | `          }` |
|      7 | 4603 | `          i = prefix!=0;` |
|     29 | 4604 | `          while( nPad-- ) zBuf[i++] = '0';` |
|      7 | 4605 | `          length = width;` |
|      3 | 4606 | `        }` |
|      - | 4607 | `#else` |
|      - | 4608 | `         zBuf = " ";` |
|      - | 4609 | `		 length = (int)sizeof(char);` |
|      - | 4610 | `#endif /* PH7_OMIT_FLOATING_POINT */` |
|    121 | 4611 | `		 break;` |
|      - | 4612 | `							 }` |
|    ! 0 | 4613 | `		default:` |
|      - | 4614 | `			/* Unreachable: PH7_FormatValidate() rejects unknown specifiers with a` |
|      - | 4615 | `			 * catchable ValueError before formatting begins. Kept as a defensive` |
|      - | 4616 | `			 * no-op that emits nothing. */` |
|    ! 0 | 4617 | `			length = 0;` |
|    ! 0 | 4618 | `			break;` |
|      - | 4619 | `		}` |
|      - | 4620 | `		 /*` |
|      - | 4621 | `		 ** The text of the conversion is pointed to by "zBuf" and is` |
|      - | 4622 | `		 ** "length" characters long.The field width is "width".Do` |
|      - | 4623 | `		 ** the output.` |
|      - | 4624 | `		 */` |
|    631 | 4625 | `    if( !flag_leftjustify ){` |
|      - | 4626 | `      register int nspace;` |
|    617 | 4627 | `      nspace = width-length;` |
|    617 | 4628 | `      if( nspace>0 ){` |
|      7 | 4629 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4630 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4631 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4632 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4633 | `			}` |
|    ! 0 | 4634 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4635 | `        }` |
|      7 | 4636 | `        if( nspace>0 ){` |
|      7 | 4637 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|      7 | 4638 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4639 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4640 | `			}` |
|      3 | 4641 | `		}` |
|      3 | 4642 | `      }` |
|    308 | 4643 | `    }` |
|    631 | 4644 | `    if( length>0 ){` |
|    631 | 4645 | `		rc = xConsumer(pCtx,zBuf,(unsigned int)length,pUserData);` |
|    631 | 4646 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4647 | `		  return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4648 | `		}` |
|    315 | 4649 | `    }` |
|    631 | 4650 | `    if( flag_leftjustify ){` |
|      - | 4651 | `      register int nspace;` |
|     15 | 4652 | `      nspace = width-length;` |
|     15 | 4653 | `      if( nspace>0 ){` |
|     11 | 4654 | `        while( nspace>=etSPACESIZE ){` |
|    ! 0 | 4655 | `			rc = xConsumer(pCtx,spaces,etSPACESIZE,pUserData);` |
|    ! 0 | 4656 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4657 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4658 | `			}` |
|    ! 0 | 4659 | `			nspace -= etSPACESIZE;` |
|    ! 0 | 4660 | `        }` |
|     11 | 4661 | `        if( nspace>0 ){` |
|     11 | 4662 | `			rc = xConsumer(pCtx,spaces,(unsigned int)nspace,pUserData);` |
|     11 | 4663 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 4664 | `				return SXERR_ABORT; /* Consumer routine request an operation abort */` |
|      - | 4665 | `			}` |
|      5 | 4666 | `		}` |
|      5 | 4667 | `      }` |
|      7 | 4668 | `    }` |
|      1 | 4669 | ` }/* for(;;) */` |
|    317 | 4670 | `	return SXRET_OK;` |
|    159 | 4671 | `}` |
|      - | 4672 | `/*` |
|      - | 4673 | ` * Callback [i.e: Formatted input consumer] of the sprintf function.` |
|      - | 4674 | ` */` |
|    146 | 4675 | `static int sprintfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4676 | `{` |
|      - | 4677 | `	/* pUserData points to the caller's allocation-rc slot so an OOM during the` |
|      - | 4678 | `	 * result append is surfaced (the builtin raises a fatal); returning the` |
|      - | 4679 | `	 * non-OK rc also stops the format loop. */` |
|    147 | 4680 | `	sxi32 *pRc = (sxi32 *)pUserData;` |
|    147 | 4681 | `	*pRc = ph7_result_string(pCtx,zInput,nLen);` |
|    147 | 4682 | `	return *pRc;` |
|      1 | 4683 | `}` |
|      - | 4684 | `/*` |
|      - | 4685 | ` * string sprintf(string $format[,mixed $args [, mixed $... ]])` |
|      - | 4686 | ` *  Return a formatted string.` |
|      - | 4687 | ` * Parameters` |
|      - | 4688 | ` *  $format` |
|      - | 4689 | ` *    The format string (see block comment above)` |
|      - | 4690 | ` * Return` |
|      - | 4691 | ` *  A string produced according to the formatting string format.` |
|      - | 4692 | ` */` |
|    110 | 4693 | `static int PH7_builtin_sprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4694 | `{` |
|      - | 4695 | `	const char *zFormat;` |
|    111 | 4696 | `	sxi32 rc = SXRET_OK;` |
|      - | 4697 | `	int nLen;` |
|    111 | 4698 | `	if( nArg < 1 ){` |
|      - | 4699 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4700 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4701 | `		return PH7_OK;` |
|      - | 4702 | `	}` |
|      - | 4703 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|    111 | 4704 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    111 | 4705 | `	if( rc != PH7_OK ){` |
|      5 | 4706 | `		return rc;` |
|      - | 4707 | `	}` |
|      - | 4708 | `	/* Extract the string format (scalars/null coerce). */` |
|    107 | 4709 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    107 | 4710 | `	if( nLen < 1 ){` |
|      - | 4711 | `		/* Empty string */` |
|    ! 0 | 4712 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4713 | `		return PH7_OK;` |
|      - | 4714 | `	}` |
|      - | 4715 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4716 | `	 * output; propagate the throw status verbatim. */` |
|    107 | 4717 | `	rc = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    107 | 4718 | `	if( rc != PH7_OK ){` |
|     17 | 4719 | `		return rc;` |
|      - | 4720 | `	}` |
|      - | 4721 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|     91 | 4722 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&rc,FALSE);` |
|     91 | 4723 | `	if( rc != SXRET_OK ){` |
|      - | 4724 | `		/* The result append ran out of memory: raise a fatal rather than` |
|      - | 4725 | `		 * returning a silently-truncated string. */` |
|    ! 0 | 4726 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4727 | `	}` |
|     91 | 4728 | `	return PH7_OK;` |
|     56 | 4729 | `}` |
|      - | 4730 | `/*` |
|      - | 4731 | ` * Callback [i.e: Formatted input consumer] of the printf function.` |
|      - | 4732 | ` */` |
|   1130 | 4733 | `static int printfConsumer(ph7_context *pCtx,const char *zInput,int nLen,void *pUserData)` |
|      1 | 4734 | `{` |
|   1131 | 4735 | `	ph7_int64 *pCounter = (ph7_int64 *)pUserData;` |
|      - | 4736 | `	/* Call the VM output consumer directly */` |
|   1131 | 4737 | `	ph7_context_output(pCtx,zInput,nLen);` |
|      - | 4738 | `	/* Increment counter */` |
|   1131 | 4739 | `	*pCounter += nLen;` |
|   1131 | 4740 | `	return PH7_OK;` |
|      1 | 4741 | `}` |
|      - | 4742 | `/*` |
|      - | 4743 | ` * int64 printf(string $format[,mixed $args[,mixed $... ]])` |
|      - | 4744 | ` *  Output a formatted string.` |
|      - | 4745 | ` * Parameters` |
|      - | 4746 | ` *  $format` |
|      - | 4747 | ` *   See sprintf() for a description of format.` |
|      - | 4748 | ` * Return` |
|      - | 4749 | ` *  The length of the outputted string.` |
|      - | 4750 | ` */` |
|    200 | 4751 | `static int PH7_builtin_printf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4752 | `{` |
|    201 | 4753 | `	ph7_int64 nCounter = 0;` |
|      - | 4754 | `	const char *zFormat;` |
|      - | 4755 | `	int nLen;` |
|    201 | 4756 | `	if( nArg < 1 ){` |
|      - | 4757 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4758 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4759 | `		return PH7_OK;` |
|      - | 4760 | `	}` |
|      - | 4761 | `	/* PHP 8: a non-string-coercible $format (array/object/resource) is a TypeError. */` |
|      - | 4762 | `	{` |
|    201 | 4763 | `		sxi32 rcf = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|    201 | 4764 | `		if( rcf != PH7_OK ){` |
|    ! 0 | 4765 | `			return rcf;` |
|      - | 4766 | `		}` |
|      - | 4767 | `	}` |
|      - | 4768 | `	/* Extract the string format (scalars/null coerce). */` |
|    201 | 4769 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|    201 | 4770 | `	if( nLen < 1 ){` |
|      - | 4771 | `		/* Empty string */` |
|    ! 0 | 4772 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4773 | `		return PH7_OK;` |
|      - | 4774 | `	}` |
|      - | 4775 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4776 | `	 * output; propagate the throw status verbatim. */` |
|      - | 4777 | `	{` |
|    201 | 4778 | `		sxi32 rcv = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|    201 | 4779 | `		if( rcv != PH7_OK ){` |
|    ! 0 | 4780 | `			return rcv;` |
|      - | 4781 | `		}` |
|      - | 4782 | `	}` |
|      - | 4783 | `	/* Format the string */` |
|    201 | 4784 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,nArg,apArg,(void *)&nCounter,FALSE);` |
|      - | 4785 | `	/* Return the length of the outputted string */` |
|    201 | 4786 | `	ph7_result_int64(pCtx,nCounter);` |
|    201 | 4787 | `	return PH7_OK;` |
|    101 | 4788 | `}` |
|      - | 4789 | `/*` |
|      - | 4790 | ` * int vprintf(string $format,array $args)` |
|      - | 4791 | ` *  Output a formatted string.` |
|      - | 4792 | ` * Parameters` |
|      - | 4793 | ` *  $format` |
|      - | 4794 | ` *   See sprintf() for a description of format.` |
|      - | 4795 | ` * Return` |
|      - | 4796 | ` *  The length of the outputted string.` |
|      - | 4797 | ` */` |
|      4 | 4798 | `static int PH7_builtin_vprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4799 | `{` |
|      5 | 4800 | `	ph7_int64 nCounter = 0;` |
|      - | 4801 | `	const char *zFormat;` |
|      - | 4802 | `	ph7_hashmap *pMap;` |
|      - | 4803 | `	SySet sArg;` |
|      - | 4804 | `	int nLen,n;` |
|      - | 4805 | `	sxi32 rcFmt;` |
|      5 | 4806 | `	if( nArg < 2 ){` |
|      - | 4807 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 4808 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4809 | `		return PH7_OK;` |
|      - | 4810 | `	}` |
|      - | 4811 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|      5 | 4812 | `	rcFmt = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|      5 | 4813 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4814 | `		return rcFmt;` |
|      - | 4815 | `	}` |
|      5 | 4816 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 4817 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 4818 | `		char zBuf[64];` |
|      4 | 4819 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4820 | `			"vprintf(): Argument #2 ($values) must be of type array, %s given",` |
|      2 | 4821 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 4822 | `	}` |
|      - | 4823 | `	/* Extract the string format (scalars/null coerce). */` |
|      3 | 4824 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 4825 | `	if( nLen < 1 ){` |
|      - | 4826 | `		/* Empty string */` |
|    ! 0 | 4827 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 4828 | `		return PH7_OK;` |
|      - | 4829 | `	}` |
|      - | 4830 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4831 | `	 * output; propagate the throw status verbatim. */` |
|      3 | 4832 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      3 | 4833 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4834 | `		return rcFmt;` |
|      - | 4835 | `	}` |
|      - | 4836 | `	/* Point to the hashmap */` |
|      3 | 4837 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4838 | `	/* Extract arguments from the hashmap */` |
|      3 | 4839 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4840 | `	/* Format the string */` |
|      3 | 4841 | `	PH7_InputFormat(printfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&nCounter,TRUE);` |
|      - | 4842 | `	/* Release the container */` |
|      3 | 4843 | `	SySetRelease(&sArg);` |
|      - | 4844 | `	/* Return the length of the outputted string */` |
|      3 | 4845 | `	ph7_result_int64(pCtx,nCounter);` |
|      3 | 4846 | `	return PH7_OK;` |
|      3 | 4847 | `}` |
|      - | 4848 | `/*` |
|      - | 4849 | ` * int vsprintf(string $format,array $args)` |
|      - | 4850 | ` *  Output a formatted string.` |
|      - | 4851 | ` * Parameters` |
|      - | 4852 | ` *  $format` |
|      - | 4853 | ` *   See sprintf() for a description of format.` |
|      - | 4854 | ` * Return` |
|      - | 4855 | ` *  A string produced according to the formatting string format.` |
|      - | 4856 | ` */` |
|     22 | 4857 | `static int PH7_builtin_vsprintf(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4858 | `{` |
|      - | 4859 | `	const char *zFormat;` |
|      - | 4860 | `	ph7_hashmap *pMap;` |
|      - | 4861 | `	SySet sArg;` |
|     23 | 4862 | `	sxi32 rc = SXRET_OK;` |
|      - | 4863 | `	sxi32 rcFmt;` |
|      - | 4864 | `	int nLen,n;` |
|     23 | 4865 | `	if( nArg < 2 ){` |
|      - | 4866 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4867 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4868 | `		return PH7_OK;` |
|      - | 4869 | `	}` |
|      - | 4870 | `	/* PHP 8 checks arguments left-to-right: $format (#1) then $values (#2). */` |
|     23 | 4871 | `	rc = PH7_FormatCheckFormatArg(pCtx,apArg[0],1);` |
|     23 | 4872 | `	if( rc != PH7_OK ){` |
|      5 | 4873 | `		return rc;` |
|      - | 4874 | `	}` |
|     19 | 4875 | `	if( !ph7_value_is_array(apArg[1]) ){` |
|      - | 4876 | `		/* PHP 8: a non-array $values is a catchable TypeError. */` |
|      - | 4877 | `		char zBuf[64];` |
|     16 | 4878 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 4879 | `			"vsprintf(): Argument #2 ($values) must be of type array, %s given",` |
|     10 | 4880 | `			VmValueGivenName(apArg[1],zBuf,sizeof(zBuf)));` |
|      - | 4881 | `	}` |
|      - | 4882 | `	/* Extract the string format (scalars/null coerce). */` |
|      9 | 4883 | `	zFormat = ph7_value_to_string(apArg[0],&nLen);` |
|      9 | 4884 | `	if( nLen < 1 ){` |
|      - | 4885 | `		/* Empty string */` |
|    ! 0 | 4886 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4887 | `		return PH7_OK;` |
|      - | 4888 | `	}` |
|      - | 4889 | `	/* PHP 8: an unknown format specifier throws a catchable ValueError before any` |
|      - | 4890 | `	 * output; propagate the throw status verbatim. */` |
|      9 | 4891 | `	rcFmt = PH7_FormatValidate(pCtx,zFormat,nLen);` |
|      9 | 4892 | `	if( rcFmt != PH7_OK ){` |
|    ! 0 | 4893 | `		return rcFmt;` |
|      - | 4894 | `	}` |
|      - | 4895 | `	/* Point to hashmap */` |
|      9 | 4896 | `	pMap = (ph7_hashmap *)apArg[1]->x.pOther;` |
|      - | 4897 | `	/* Extract arguments from the hashmap */` |
|      9 | 4898 | `	n = PH7_HashmapValuesToSet(pMap,&sArg);` |
|      - | 4899 | `	/* Format the string; sprintfConsumer reports an allocation failure via &rc. */` |
|      9 | 4900 | `	PH7_InputFormat(sprintfConsumer,pCtx,zFormat,nLen,n,(ph7_value **)SySetBasePtr(&sArg),(void *)&rc,TRUE);` |
|      - | 4901 | `	/* Release the container */` |
|      9 | 4902 | `	SySetRelease(&sArg);` |
|      9 | 4903 | `	if( rc != SXRET_OK ){` |
|      - | 4904 | `		/* The result append ran out of memory: raise a fatal. */` |
|    ! 0 | 4905 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4906 | `	}` |
|      9 | 4907 | `	return PH7_OK;` |
|     12 | 4908 | `}` |
|      - | 4909 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 4910 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 4911 | `/*` |
|      - | 4912 | ` * Symisc eXtension.` |
|      - | 4913 | ` * string size_format(int64 $size)` |
|      - | 4914 | ` *  Return a smart string represenation of the given size [i.e: 64-bit integer]` |
|      - | 4915 | ` *  Example:` |
|      - | 4916 | ` *    echo size_format(1*1024*1024*1024);// 1GB` |
|      - | 4917 | ` *    echo size_format(512*1024*1024); // 512 MB` |
|      - | 4918 | ` *    echo size_format(file_size(/path/to/my/file_8192)); //8KB` |
|      - | 4919 | ` * Parameter` |
|      - | 4920 | ` *  $size` |
|      - | 4921 | ` *    Entity size in bytes.` |
|      - | 4922 | ` * Return` |
|      - | 4923 | ` *   Formatted string representation of the given size.` |
|      - | 4924 | ` */` |
|     24 | 4925 | `static int PH7_builtin_size_format(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4926 | `{` |
|      - | 4927 | `	/*Kilo*/ /*Mega*/ /*Giga*/ /*Tera*/ /*Peta*/ /*Exa*/ /*Zeta*/` |
|      - | 4928 | `	static const char zUnit[] = {"KMGTPEZ"};` |
|      - | 4929 | `	sxi32 nRest,i_32;` |
|      - | 4930 | `	ph7_int64 iSize;` |
|     25 | 4931 | `	int c = -1; /* index in zUnit[] */` |
|      - | 4932 |  |
|     25 | 4933 | `	if( nArg < 1 ){` |
|      - | 4934 | `		/* Missing argument,return the empty string */` |
|      3 | 4935 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 4936 | `		return PH7_OK;` |
|      - | 4937 | `	}` |
|      - | 4938 | `	/* Extract the given size */` |
|     23 | 4939 | `	iSize = ph7_value_to_int64(apArg[0]);` |
|     23 | 4940 | `	if( iSize < 100 /* Bytes */ ){` |
|      - | 4941 | `		/* Don't bother formatting,return immediately */` |
|      5 | 4942 | `		ph7_result_string(pCtx,"0.1 KB",(int)sizeof("0.1 KB")-1);` |
|      5 | 4943 | `		return PH7_OK;` |
|      - | 4944 | `	}` |
|     19 | 4945 | `	for(;;){` |
|     39 | 4946 | `		nRest = (sxi32)(iSize & 0x3FF);` |
|     39 | 4947 | `		iSize >>= 10;` |
|     39 | 4948 | `		c++;` |
|     39 | 4949 | `		if( (iSize & (~0 ^ 1023)) == 0 ){` |
|     19 | 4950 | `			break;` |
|      - | 4951 | `		}` |
|      1 | 4952 | `	}` |
|     19 | 4953 | `	nRest /= 100;` |
|     19 | 4954 | `	if( nRest > 9 ){` |
|    ! 0 | 4955 | `		nRest = 9;` |
|    ! 0 | 4956 | `	}` |
|     19 | 4957 | `	if( iSize > 999 ){` |
|    ! 0 | 4958 | `		c++;` |
|    ! 0 | 4959 | `		nRest = 9;` |
|    ! 0 | 4960 | `		iSize = 0;` |
|    ! 0 | 4961 | `	}` |
|     19 | 4962 | `	i_32 = (sxi32)iSize;` |
|      - | 4963 | `	/* Format */` |
|     19 | 4964 | `	ph7_result_string_format(pCtx,"%d.%d %cB",i_32,nRest,zUnit[c]);` |
|     19 | 4965 | `	return PH7_OK;` |
|     13 | 4966 | `}` |
|      - | 4967 | `#if !defined(PH7_DISABLE_HASH_FUNC)` |
|      - | 4968 | `/*` |
|      - | 4969 | ` * string md5(string $str[,bool $raw_output = false])` |
|      - | 4970 | ` *   Calculate the md5 hash of a string.` |
|      - | 4971 | ` * Parameter` |
|      - | 4972 | ` *  $str` |
|      - | 4973 | ` *   Input string` |
|      - | 4974 | ` * $raw_output` |
|      - | 4975 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 4976 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 4977 | ` * Return` |
|      - | 4978 | ` *  MD5 Hash as a 32-character hexadecimal string.` |
|      - | 4979 | ` */` |
|     12 | 4980 | `static int PH7_builtin_md5(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 4981 | `{` |
|      - | 4982 | `	unsigned char zDigest[16];` |
|     13 | 4983 | `	int raw_output = FALSE;` |
|      - | 4984 | `	const void *pIn;` |
|      - | 4985 | `	int nLen;` |
|     13 | 4986 | `	if( nArg < 1 ){` |
|      - | 4987 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 4988 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 4989 | `		return PH7_OK;` |
|      - | 4990 | `	}` |
|      - | 4991 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 4992 | `	 * digest in PHP — d41d8cd9… — so it must NOT short-circuit). */` |
|     13 | 4993 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 4994 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 4995 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 4996 | `	}` |
|      - | 4997 | `	/* Compute the MD5 digest */` |
|     13 | 4998 | `	SyMD5Compute(pIn,(sxu32)nLen,zDigest);` |
|     13 | 4999 | `	if( raw_output ){` |
|      - | 5000 | `		/* Output raw digest */` |
|      5 | 5001 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 5002 | `	}else{` |
|      - | 5003 | `		/* Perform a binary to hex conversion */` |
|      9 | 5004 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 5005 | `	}` |
|     13 | 5006 | `	return PH7_OK;` |
|      7 | 5007 | `}` |
|      - | 5008 | `/*` |
|      - | 5009 | ` * string sha1(string $str[,bool $raw_output = false])` |
|      - | 5010 | ` *   Calculate the sha1 hash of a string.` |
|      - | 5011 | ` * Parameter` |
|      - | 5012 | ` *  $str` |
|      - | 5013 | ` *   Input string` |
|      - | 5014 | ` * $raw_output` |
|      - | 5015 | ` *   If the optional raw_output is set to TRUE, then the md5 digest` |
|      - | 5016 | ` *   is instead returned in raw binary format with a length of 16.` |
|      - | 5017 | ` * Return` |
|      - | 5018 | ` *  SHA1 Hash as a 40-character hexadecimal string.` |
|      - | 5019 | ` */` |
|     10 | 5020 | `static int PH7_builtin_sha1(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5021 | `{` |
|      - | 5022 | `	unsigned char zDigest[20];` |
|     11 | 5023 | `	int raw_output = FALSE;` |
|      - | 5024 | `	const void *pIn;` |
|      - | 5025 | `	int nLen;` |
|     11 | 5026 | `	if( nArg < 1 ){` |
|      - | 5027 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 5028 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 5029 | `		return PH7_OK;` |
|      - | 5030 | `	}` |
|      - | 5031 | `	/* Extract the input string (the empty string hashes to a well-defined` |
|      - | 5032 | `	 * digest in PHP — da39a3ee… — so it must NOT short-circuit). */` |
|     11 | 5033 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 5034 | `	if( nArg > 1 && ph7_value_is_bool(apArg[1])){` |
|      5 | 5035 | `		raw_output = ph7_value_to_bool(apArg[1]);` |
|      2 | 5036 | `	}` |
|      - | 5037 | `	/* Compute the SHA1 digest */` |
|     11 | 5038 | `	SySha1Compute(pIn,(sxu32)nLen,zDigest);` |
|     11 | 5039 | `	if( raw_output ){` |
|      - | 5040 | `		/* Output raw digest */` |
|      5 | 5041 | `		ph7_result_string(pCtx,(const char *)zDigest,(int)sizeof(zDigest));` |
|      3 | 5042 | `	}else{` |
|      - | 5043 | `		/* Perform a binary to hex conversion */` |
|      7 | 5044 | `		SyBinToHexConsumer((const void *)zDigest,sizeof(zDigest),HashConsumer,pCtx);` |
|      - | 5045 | `	}` |
|     11 | 5046 | `	return PH7_OK;` |
|      6 | 5047 | `}` |
|      - | 5048 | `/*` |
|      - | 5049 | ` * int64 crc32(string $str)` |
|      - | 5050 | ` *   Calculates the crc32 polynomial of a strin.` |
|      - | 5051 | ` * Parameter` |
|      - | 5052 | ` *  $str` |
|      - | 5053 | ` *   Input string` |
|      - | 5054 | ` * Return` |
|      - | 5055 | ` *  CRC32 checksum of the given input (64-bit integer).` |
|      - | 5056 | ` */` |
|      2 | 5057 | `static int PH7_builtin_crc32(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5058 | `{` |
|      - | 5059 | `	const void *pIn;` |
|      - | 5060 | `	sxu32 nCRC;` |
|      - | 5061 | `	int nLen;` |
|      3 | 5062 | `	if( nArg < 1 ){` |
|      - | 5063 | `		/* Missing arguments,return 0 */` |
|    ! 0 | 5064 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5065 | `		return PH7_OK;` |
|      - | 5066 | `	}` |
|      - | 5067 | `	/* Extract the input string */` |
|      3 | 5068 | `	pIn = (const void *)ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 5069 | `	if( nLen < 1 ){` |
|      - | 5070 | `		/* crc32("") is 0 in PHP, so this short-circuit is correct here — unlike` |
|      - | 5071 | `		 * md5()/sha1(), whose empty-string digests are non-zero. */` |
|    ! 0 | 5072 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 5073 | `		return PH7_OK;` |
|      - | 5074 | `	}` |
|      - | 5075 | `	/* Calculate the sum */` |
|      3 | 5076 | `	nCRC = SyCrc32(pIn,(sxu32)nLen);` |
|      - | 5077 | `	/* Return the CRC32 as 64-bit integer */` |
|      3 | 5078 | `	ph7_result_int64(pCtx,(ph7_int64)nCRC^ 0xFFFFFFFF);` |
|      3 | 5079 | `	return PH7_OK;` |
|      2 | 5080 | `}` |
|      - | 5081 | `/*` |
|      - | 5082 | ` * The hash() family (hash/hash_hmac/hash_equals/hash_algos). Each algorithm is` |
|      - | 5083 | ` * described by a small record so one dispatch (and one generic HMAC) serves them` |
|      - | 5084 | ` * all. Thin adapters normalize the differing context types and the reversed` |
|      - | 5085 | ` * MD5Final argument order behind a uniform Init/Update/Final over a HashCtx union.` |
|      - | 5086 | ` */` |
|     11 | 5087 | `static void HashMd5Init(HashCtx *c){ MD5Init(&c->md5); }` |
|     15 | 5088 | `static void HashMd5Update(HashCtx *c,const unsigned char *d,unsigned int n){ MD5Update(&c->md5,d,n); }` |
|     11 | 5089 | `static void HashMd5Final(HashCtx *c,unsigned char *o){ MD5Final(o,&c->md5); }` |
|     11 | 5090 | `static void HashSha1Init(HashCtx *c){ SHA1Init(&c->sha1); }` |
|     15 | 5091 | `static void HashSha1Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA1Update(&c->sha1,d,n); }` |
|     11 | 5092 | `static void HashSha1Final(HashCtx *c,unsigned char *o){ SHA1Final(&c->sha1,o); }` |
|      9 | 5093 | `static void HashSha224Init(HashCtx *c){ SHA224Init(&c->sha256); }` |
|     33 | 5094 | `static void HashSha256Init(HashCtx *c){ SHA256Init(&c->sha256); }` |
|     57 | 5095 | `static void HashSha256Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA256Update(&c->sha256,d,n); }` |
|     41 | 5096 | `static void HashSha256Final(HashCtx *c,unsigned char *o){ SHA256Final(&c->sha256,o); }` |
|      9 | 5097 | `static void HashSha384Init(HashCtx *c){ SHA384Init(&c->sha512); }` |
|     15 | 5098 | `static void HashSha512Init(HashCtx *c){ SHA512Init(&c->sha512); }` |
|     27 | 5099 | `static void HashSha512Update(HashCtx *c,const unsigned char *d,unsigned int n){ SHA512Update(&c->sha512,d,n); }` |
|     23 | 5100 | `static void HashSha512Final(HashCtx *c,unsigned char *o){ SHA512Final(&c->sha512,o); }` |
|      - | 5101 | `typedef struct HashAlgo HashAlgo;` |
|      - | 5102 | `struct HashAlgo {` |
|      - | 5103 | `	const char *zName;   /* lowercase canonical name */` |
|      - | 5104 | `	int nDigestLen;      /* output bytes: 16/20/28/32/48/64 */` |
|      - | 5105 | `	int nBlockLen;       /* internal block bytes (for HMAC): 64 or 128 */` |
|      - | 5106 | `	void (*xInit)(HashCtx *);` |
|      - | 5107 | `	void (*xUpdate)(HashCtx *,const unsigned char *,unsigned int);` |
|      - | 5108 | `	void (*xFinal)(HashCtx *,unsigned char *);` |
|      - | 5109 | `};` |
|      - | 5110 | `static const HashAlgo aHashAlgo[] = {` |
|      - | 5111 | `	{ "md5",    16, 64,  HashMd5Init,    HashMd5Update,    HashMd5Final    },` |
|      - | 5112 | `	{ "sha1",   20, 64,  HashSha1Init,   HashSha1Update,   HashSha1Final   },` |
|      - | 5113 | `	{ "sha224", 28, 64,  HashSha224Init, HashSha256Update, HashSha256Final },` |
|      - | 5114 | `	{ "sha256", 32, 64,  HashSha256Init, HashSha256Update, HashSha256Final },` |
|      - | 5115 | `	{ "sha384", 48, 128, HashSha384Init, HashSha512Update, HashSha512Final },` |
|      - | 5116 | `	{ "sha512", 64, 128, HashSha512Init, HashSha512Update, HashSha512Final },` |
|      - | 5117 | `};` |
|      - | 5118 | `/* Case-insensitive algorithm lookup (PHP accepts 'SHA256' etc.). */` |
|     73 | 5119 | `static const HashAlgo * HashFindAlgo(const char *zName,int nLen){` |
|      - | 5120 | `	sxu32 i;` |
|    279 | 5121 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|    272 | 5122 | `		if( (int)SyStrlen(aHashAlgo[i].zName) == nLen` |
|    211 | 5123 | `			&& SyStrnicmp(aHashAlgo[i].zName,zName,(sxu32)nLen) == 0 ){` |
|     67 | 5124 | `			return &aHashAlgo[i];` |
|      - | 5125 | `		}` |
|    106 | 5126 | `	}` |
|      6 | 5127 | `	return 0;` |
|     38 | 5128 | `}` |
|      - | 5129 | `/*` |
|      - | 5130 | ` * string hash(string $algo,string $data[,bool $binary = false])` |
|      - | 5131 | ` *   Generate a hash value (message digest).` |
|      - | 5132 | ` */` |
|     54 | 5133 | `static int PH7_builtin_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5134 | `{` |
|      - | 5135 | `	const HashAlgo *pAlgo;` |
|      - | 5136 | `	const char *zAlgo,*zData;` |
|     56 | 5137 | `	int nAlgoLen,nDataLen,raw_output = FALSE;` |
|      - | 5138 | `	HashCtx sCtx;` |
|      - | 5139 | `	unsigned char zDigest[64];` |
|     56 | 5140 | `	if( nArg < 2 ){` |
|    ! 0 | 5141 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5142 | `			"hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5143 | `	}` |
|     56 | 5144 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     56 | 5145 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     56 | 5146 | `	if( pAlgo == 0 ){` |
|      3 | 5147 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5148 | `			"hash(): Argument #1 ($algo) must be a valid hashing algorithm");` |
|      - | 5149 | `	}` |
|     53 | 5150 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     53 | 5151 | `	if( nArg > 2 ){` |
|      9 | 5152 | `		raw_output = ph7_value_to_bool(apArg[2]);` |
|      4 | 5153 | `	}` |
|     53 | 5154 | `	pAlgo->xInit(&sCtx);` |
|     53 | 5155 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     53 | 5156 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     53 | 5157 | `	if( raw_output ){` |
|      9 | 5158 | `		ph7_result_string(pCtx,(const char *)zDigest,pAlgo->nDigestLen);` |
|      5 | 5159 | `	}else{` |
|     45 | 5160 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)pAlgo->nDigestLen,HashConsumer,pCtx);` |
|      - | 5161 | `	}` |
|     53 | 5162 | `	return PH7_OK;` |
|     29 | 5163 | `}` |
|      - | 5164 | `/*` |
|      - | 5165 | ` * string hash_hmac(string $algo,string $data,string $key[,bool $binary = false])` |
|      - | 5166 | ` *   Generate a keyed hash value using the HMAC method (RFC 2104).` |
|      - | 5167 | ` */` |
|     16 | 5168 | `static int PH7_builtin_hash_hmac(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 5169 | `{` |
|      - | 5170 | `	const HashAlgo *pAlgo;` |
|      - | 5171 | `	const char *zAlgo,*zData,*zKey;` |
|     18 | 5172 | `	int nAlgoLen,nDataLen,nKeyLen,raw_output = FALSE;` |
|      - | 5173 | `	HashCtx sCtx;` |
|      - | 5174 | `	unsigned char zKeyBlock[128],zIpad[128],zOpad[128],zInner[64],zDigest[64];` |
|      - | 5175 | `	int i,nBlock,nDigest;` |
|     18 | 5176 | `	if( nArg < 3 ){` |
|    ! 0 | 5177 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5178 | `			"hash_hmac() expects at least 3 arguments, %d given",nArg);` |
|      - | 5179 | `	}` |
|     18 | 5180 | `	zAlgo = ph7_value_to_string(apArg[0],&nAlgoLen);` |
|     18 | 5181 | `	pAlgo = HashFindAlgo(zAlgo,nAlgoLen);` |
|     18 | 5182 | `	if( pAlgo == 0 ){` |
|      3 | 5183 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5184 | `			"hash_hmac(): Argument #1 ($algo) must be a valid cryptographic hashing algorithm");` |
|      - | 5185 | `	}` |
|     15 | 5186 | `	zData = ph7_value_to_string(apArg[1],&nDataLen);` |
|     15 | 5187 | `	zKey = ph7_value_to_string(apArg[2],&nKeyLen);` |
|     15 | 5188 | `	if( nArg > 3 ){` |
|      3 | 5189 | `		raw_output = ph7_value_to_bool(apArg[3]);` |
|      1 | 5190 | `	}` |
|     15 | 5191 | `	nBlock = pAlgo->nBlockLen;` |
|     15 | 5192 | `	nDigest = pAlgo->nDigestLen;` |
|      - | 5193 | `	/* Reduce the key to a single block: hash it if longer than the block, then` |
|      - | 5194 | `	 * zero-pad (a short or empty key is just zero-padded). */` |
|     15 | 5195 | `	SyZero(zKeyBlock,sizeof(zKeyBlock));` |
|     15 | 5196 | `	if( nKeyLen > nBlock ){` |
|      3 | 5197 | `		pAlgo->xInit(&sCtx);` |
|      3 | 5198 | `		pAlgo->xUpdate(&sCtx,(const unsigned char *)zKey,(unsigned int)nKeyLen);` |
|      3 | 5199 | `		pAlgo->xFinal(&sCtx,zKeyBlock);` |
|     14 | 5200 | `	}else if( nKeyLen > 0 ){` |
|     11 | 5201 | `		SyMemcpy(zKey,zKeyBlock,(sxu32)nKeyLen);` |
|      5 | 5202 | `	}` |
|   1039 | 5203 | `	for( i = 0; i < nBlock; i++ ){` |
|   1025 | 5204 | `		zIpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x36);` |
|   1025 | 5205 | `		zOpad[i] = (unsigned char)(zKeyBlock[i] ^ 0x5c);` |
|    513 | 5206 | `	}` |
|      - | 5207 | `	/* inner = H((key ^ ipad) \|\| data) */` |
|     15 | 5208 | `	pAlgo->xInit(&sCtx);` |
|     15 | 5209 | `	pAlgo->xUpdate(&sCtx,zIpad,(unsigned int)nBlock);` |
|     15 | 5210 | `	pAlgo->xUpdate(&sCtx,(const unsigned char *)zData,(unsigned int)nDataLen);` |
|     15 | 5211 | `	pAlgo->xFinal(&sCtx,zInner);` |
|      - | 5212 | `	/* out = H((key ^ opad) \|\| inner) */` |
|     15 | 5213 | `	pAlgo->xInit(&sCtx);` |
|     15 | 5214 | `	pAlgo->xUpdate(&sCtx,zOpad,(unsigned int)nBlock);` |
|     15 | 5215 | `	pAlgo->xUpdate(&sCtx,zInner,(unsigned int)nDigest);` |
|     15 | 5216 | `	pAlgo->xFinal(&sCtx,zDigest);` |
|     15 | 5217 | `	if( raw_output ){` |
|      3 | 5218 | `		ph7_result_string(pCtx,(const char *)zDigest,nDigest);` |
|      2 | 5219 | `	}else{` |
|     13 | 5220 | `		SyBinToHexConsumer((const void *)zDigest,(sxu32)nDigest,HashConsumer,pCtx);` |
|      - | 5221 | `	}` |
|     15 | 5222 | `	return PH7_OK;` |
|     10 | 5223 | `}` |
|      - | 5224 | `/*` |
|      - | 5225 | ` * bool hash_equals(string $known_string,string $user_string)` |
|      - | 5226 | ` *   Timing-attack-safe string comparison.` |
|      - | 5227 | ` */` |
|     14 | 5228 | `static int PH7_builtin_hash_equals(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 5229 | `{` |
|      - | 5230 | `	const char *zKnown,*zUser;` |
|      - | 5231 | `	int nKnown,nUser,i;` |
|     17 | 5232 | `	volatile unsigned char vDiff = 0;` |
|     17 | 5233 | `	if( nArg < 2 ){` |
|    ! 0 | 5234 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5235 | `			"hash_equals() expects exactly 2 arguments, %d given",nArg);` |
|      - | 5236 | `	}` |
|     17 | 5237 | `	if( !ph7_value_is_string(apArg[0]) ){` |
|      4 | 5238 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5239 | `			"hash_equals(): Argument #1 ($known_string) must be of type string, %s given",` |
|      1 | 5240 | `			ph7_type_name(apArg[0]));` |
|      - | 5241 | `	}` |
|     14 | 5242 | `	if( !ph7_value_is_string(apArg[1]) ){` |
|      4 | 5243 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - | 5244 | `			"hash_equals(): Argument #2 ($user_string) must be of type string, %s given",` |
|      2 | 5245 | `			ph7_type_name(apArg[1]));` |
|      - | 5246 | `	}` |
|     11 | 5247 | `	zKnown = ph7_value_to_string(apArg[0],&nKnown);` |
|     11 | 5248 | `	zUser = ph7_value_to_string(apArg[1],&nUser);` |
|     11 | 5249 | `	if( nKnown != nUser ){` |
|      5 | 5250 | `		ph7_result_bool(pCtx,0);` |
|      5 | 5251 | `		return PH7_OK;` |
|      - | 5252 | `	}` |
|      - | 5253 | `	/* Constant-time: read every byte, never short-circuit. */` |
|     19 | 5254 | `	for( i = 0; i < nKnown; i++ ){` |
|     13 | 5255 | `		vDiff \|= (unsigned char)(zKnown[i] ^ zUser[i]);` |
|      7 | 5256 | `	}` |
|      7 | 5257 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|      7 | 5258 | `	return PH7_OK;` |
|     10 | 5259 | `}` |
|      - | 5260 | `/*` |
|      - | 5261 | ` * array hash_algos(void)` |
|      - | 5262 | ` *   Return a list of the registered hashing algorithms.` |
|      - | 5263 | ` */` |
|      2 | 5264 | `static int PH7_builtin_hash_algos(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5265 | `{` |
|      - | 5266 | `	ph7_value *pArray,*pValue;` |
|      - | 5267 | `	sxu32 i;` |
|      1 | 5268 | `	SXUNUSED(nArg);` |
|      1 | 5269 | `	SXUNUSED(apArg);` |
|      3 | 5270 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 5271 | `	pValue = ph7_context_new_scalar(pCtx);` |
|      3 | 5272 | `	if( pArray == 0 \|\| pValue == 0 ){` |
|    ! 0 | 5273 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5274 | `		return PH7_OK;` |
|      - | 5275 | `	}` |
|     15 | 5276 | `	for( i = 0; i < SX_ARRAYSIZE(aHashAlgo); i++ ){` |
|     13 | 5277 | `		ph7_value_string(pValue,aHashAlgo[i].zName,-1);` |
|     13 | 5278 | `		ph7_array_add_elem(pArray,0 /* Automatic 0-based index */,pValue);` |
|     13 | 5279 | `		ph7_value_reset_string_cursor(pValue);` |
|      7 | 5280 | `	}` |
|      3 | 5281 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 5282 | `	return PH7_OK;` |
|      2 | 5283 | `}` |
|      - | 5284 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 5285 | `/*` |
|      - | 5286 | ` * password_* (bcrypt). These live in ext/standard in real PHP — outside the` |
|      - | 5287 | ` * hash extension — so they are NOT guarded by PH7_DISABLE_HASH_FUNC.` |
|      - | 5288 | ` */` |
|      - | 5289 | `/*` |
|      - | 5290 | ` * Parse a bcrypt crypt string. Returns TRUE and fills *piCost when zHash is a` |
|      - | 5291 | ` * well-formed "$2?$NN$"+53-char bcrypt hash (60 bytes, valid minor, cost 4..31).` |
|      - | 5292 | ` */` |
|     40 | 5293 | `static int BcryptParseHash(const char *zHash,int nHash,int *piCost)` |
|      1 | 5294 | `{` |
|      - | 5295 | `	int iCost;` |
|     40 | 5296 | `	if( nHash != 60 \|\| zHash[0] != '$' \|\| zHash[1] != '2' \|\| zHash[3] != '$'` |
|     29 | 5297 | `		\|\| (zHash[2] != 'a' && zHash[2] != 'b' && zHash[2] != 'x' && zHash[2] != 'y') ){` |
|     13 | 5298 | `		return FALSE;` |
|      - | 5299 | `	}` |
|     29 | 5300 | `	if( zHash[4] < '0' \|\| zHash[4] > '9' \|\| zHash[5] < '0' \|\| zHash[5] > '9' \|\| zHash[6] != '$' ){` |
|    ! 0 | 5301 | `		return FALSE;` |
|      - | 5302 | `	}` |
|     29 | 5303 | `	iCost = (zHash[4]-'0')*10 + (zHash[5]-'0');` |
|     29 | 5304 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      3 | 5305 | `		return FALSE;` |
|      - | 5306 | `	}` |
|     27 | 5307 | `	if( piCost ){ *piCost = iCost; }` |
|     27 | 5308 | `	return TRUE;` |
|     21 | 5309 | `}` |
|      - | 5310 | `/*` |
|      - | 5311 | ` * TRUE if the $algo argument selects bcrypt: null (PASSWORD_DEFAULT) or the` |
|      - | 5312 | ` * "2y" id (PASSWORD_BCRYPT/PASSWORD_DEFAULT). bcrypt is the only supported algo.` |
|      - | 5313 | ` */` |
|     20 | 5314 | `static int BcryptIsBcryptAlgo(ph7_value *pAlgo)` |
|      3 | 5315 | `{` |
|     23 | 5316 | `	if( ph7_value_is_null(pAlgo) ){` |
|    ! 0 | 5317 | `		return TRUE;` |
|      - | 5318 | `	}` |
|     23 | 5319 | `	if( ph7_value_is_string(pAlgo) ){` |
|      - | 5320 | `		int nAlgo;` |
|     23 | 5321 | `		const char *zAlgo = ph7_value_to_string(pAlgo,&nAlgo);` |
|     23 | 5322 | `		return ( nAlgo == 2 && zAlgo[0] == '2' && zAlgo[1] == 'y' );` |
|      - | 5323 | `	}` |
|    ! 0 | 5324 | `	return FALSE;` |
|     13 | 5325 | `}` |
|      - | 5326 | `/*` |
|      - | 5327 | ` * bool\|string password_hash(string $password,string\|int\|null $algo[,array $options])` |
|      - | 5328 | ` *  Create a bcrypt hash of the password.` |
|      - | 5329 | ` */` |
|     16 | 5330 | `static int PH7_builtin_password_hash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      3 | 5331 | `{` |
|      - | 5332 | `	const char *zPwd;` |
|     19 | 5333 | `	int nPwd,iCost = 12;` |
|      - | 5334 | `	unsigned char aSalt[16];` |
|      - | 5335 | `	char zHash[60];` |
|     19 | 5336 | `	if( nArg < 2 ){` |
|    ! 0 | 5337 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5338 | `			"password_hash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5339 | `	}` |
|     19 | 5340 | `	if( !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      3 | 5341 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 5342 | `			"password_hash(): Argument #2 ($algo) must be a valid password hashing algorithm");` |
|      - | 5343 | `	}` |
|      - | 5344 | `	/* cost from $options['cost'] (default 12). */` |
|     16 | 5345 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|     14 | 5346 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|     14 | 5347 | `		if( pCost ){ iCost = ph7_value_to_int(pCost); }` |
|      6 | 5348 | `	}` |
|     16 | 5349 | `	if( iCost < 4 \|\| iCost > 31 ){` |
|      4 | 5350 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      1 | 5351 | `			"Invalid bcrypt cost parameter specified: %d",iCost);` |
|      - | 5352 | `	}` |
|     13 | 5353 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     13 | 5354 | `	if( SyOSCSPRNG(aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 5355 | `		return PH7_VmThrowException(pCtx,"Exception",` |
|      - | 5356 | `			"password_hash(): unable to gather sufficient entropy for the salt");` |
|      - | 5357 | `	}` |
|     13 | 5358 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zHash) != SXRET_OK ){` |
|    ! 0 | 5359 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5360 | `		return PH7_OK;` |
|      - | 5361 | `	}` |
|     13 | 5362 | `	ph7_result_string(pCtx,zHash,(int)sizeof(zHash));` |
|     13 | 5363 | `	return PH7_OK;` |
|     11 | 5364 | `}` |
|      - | 5365 | `/*` |
|      - | 5366 | ` * bool password_verify(string $password,string $hash)` |
|      - | 5367 | ` *  Verify a password against a bcrypt hash. Never throws on a malformed hash.` |
|      - | 5368 | ` */` |
|     28 | 5369 | `static int PH7_builtin_password_verify(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5370 | `{` |
|      - | 5371 | `	const char *zPwd,*zHash;` |
|      - | 5372 | `	int nPwd,nHash,iCost,i;` |
|      - | 5373 | `	unsigned char aSalt[16];` |
|      - | 5374 | `	char zComputed[60];` |
|     29 | 5375 | `	volatile unsigned char vDiff = 0;` |
|     29 | 5376 | `	if( nArg < 2 ){` |
|    ! 0 | 5377 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5378 | `			"password_verify() expects exactly 2 arguments, %d given",nArg);` |
|      - | 5379 | `	}` |
|     29 | 5380 | `	zPwd = ph7_value_to_string(apArg[0],&nPwd);` |
|     29 | 5381 | `	zHash = ph7_value_to_string(apArg[1],&nHash);` |
|     29 | 5382 | `	if( !BcryptParseHash(zHash,nHash,&iCost) ){` |
|     11 | 5383 | `		ph7_result_bool(pCtx,0);` |
|     11 | 5384 | `		return PH7_OK;` |
|      - | 5385 | `	}` |
|      - | 5386 | `	/* Recover the 16 salt bytes from the 22-char salt field [7..28]. */` |
|     19 | 5387 | `	if( SyBcryptB64Decode(&zHash[7],22,aSalt,sizeof(aSalt)) != SXRET_OK ){` |
|    ! 0 | 5388 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5389 | `		return PH7_OK;` |
|      - | 5390 | `	}` |
|     19 | 5391 | `	if( SyBcryptHash((const unsigned char *)zPwd,(sxu32)nPwd,(sxu32)iCost,aSalt,zComputed) != SXRET_OK ){` |
|    ! 0 | 5392 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 5393 | `		return PH7_OK;` |
|      - | 5394 | `	}` |
|      - | 5395 | `	/* Constant-time compare of the 31-char hash field [29..59] only — sidesteps` |
|      - | 5396 | `	 * salt re-canonicalisation and any "$2a"/"$2y" prefix difference. */` |
|    577 | 5397 | `	for( i = 29; i < 60; i++ ){` |
|    559 | 5398 | `		vDiff \|= (unsigned char)(zComputed[i] ^ zHash[i]);` |
|    280 | 5399 | `	}` |
|     19 | 5400 | `	ph7_result_bool(pCtx,vDiff == 0);` |
|     19 | 5401 | `	return PH7_OK;` |
|     15 | 5402 | `}` |
|      - | 5403 | `/*` |
|      - | 5404 | ` * array password_get_info(string $hash)` |
|      - | 5405 | ` *  Return ["algo"=>id\|null, "algoName"=>name, "options"=>[...]].` |
|      - | 5406 | ` */` |
|      6 | 5407 | `static int PH7_builtin_password_get_info(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5408 | `{` |
|      7 | 5409 | `	const char *zHash = "";` |
|      7 | 5410 | `	int nHash,iCost = 0,bBcrypt = 0;` |
|      - | 5411 | `	ph7_value *pArray,*pOptions,*pVal;` |
|      7 | 5412 | `	if( nArg > 0 ){` |
|      7 | 5413 | `		zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 5414 | `		bBcrypt = BcryptParseHash(zHash,nHash,&iCost);` |
|      3 | 5415 | `	}` |
|      7 | 5416 | `	pArray = ph7_context_new_array(pCtx);` |
|      7 | 5417 | `	pOptions = ph7_context_new_array(pCtx);` |
|      7 | 5418 | `	pVal = ph7_context_new_scalar(pCtx);` |
|      7 | 5419 | `	if( pArray == 0 \|\| pOptions == 0 \|\| pVal == 0 ){` |
|    ! 0 | 5420 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5421 | `		return PH7_OK;` |
|      - | 5422 | `	}` |
|      7 | 5423 | `	if( bBcrypt ){` |
|      5 | 5424 | `		ph7_value_string(pVal,&zHash[1],2);            /* algo "2y"/"2a" */` |
|      5 | 5425 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      5 | 5426 | `		ph7_value_reset_string_cursor(pVal);` |
|      5 | 5427 | `		ph7_value_string(pVal,"bcrypt",(int)sizeof("bcrypt")-1);` |
|      5 | 5428 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      5 | 5429 | `		ph7_value_int(pVal,iCost);` |
|      5 | 5430 | `		ph7_array_add_strkey_elem(pOptions,"cost",pVal);` |
|      3 | 5431 | `	}else{` |
|      3 | 5432 | `		ph7_value_null(pVal);                          /* algo => null */` |
|      3 | 5433 | `		ph7_array_add_strkey_elem(pArray,"algo",pVal);` |
|      3 | 5434 | `		ph7_value_string(pVal,"unknown",(int)sizeof("unknown")-1);` |
|      3 | 5435 | `		ph7_array_add_strkey_elem(pArray,"algoName",pVal);` |
|      - | 5436 | `	}` |
|      7 | 5437 | `	ph7_array_add_strkey_elem(pArray,"options",pOptions);` |
|      7 | 5438 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 5439 | `	return PH7_OK;` |
|      4 | 5440 | `}` |
|      - | 5441 | `/*` |
|      - | 5442 | ` * bool password_needs_rehash(string $hash,string\|int\|null $algo[,array $options])` |
|      - | 5443 | ` *  True if the hash was not made with the given algo/options.` |
|      - | 5444 | ` */` |
|      6 | 5445 | `static int PH7_builtin_password_needs_rehash(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 5446 | `{` |
|      - | 5447 | `	const char *zHash;` |
|      7 | 5448 | `	int nHash,iCost = 0,iWantCost = 12;` |
|      7 | 5449 | `	if( nArg < 2 ){` |
|    ! 0 | 5450 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|    ! 0 | 5451 | `			"password_needs_rehash() expects at least 2 arguments, %d given",nArg);` |
|      - | 5452 | `	}` |
|      7 | 5453 | `	zHash = ph7_value_to_string(apArg[0],&nHash);` |
|      7 | 5454 | `	if( !BcryptParseHash(zHash,nHash,&iCost) \|\| !BcryptIsBcryptAlgo(apArg[1]) ){` |
|      - | 5455 | `		/* A non-bcrypt hash, or a request for a different algo → needs rehash. */` |
|      3 | 5456 | `		ph7_result_bool(pCtx,1);` |
|      3 | 5457 | `		return PH7_OK;` |
|      - | 5458 | `	}` |
|      5 | 5459 | `	if( nArg > 2 && ph7_value_is_array(apArg[2]) ){` |
|      5 | 5460 | `		ph7_value *pCost = ph7_array_fetch(apArg[2],"cost",(int)sizeof("cost")-1);` |
|      5 | 5461 | `		if( pCost ){ iWantCost = ph7_value_to_int(pCost); }` |
|      2 | 5462 | `	}` |
|      5 | 5463 | `	ph7_result_bool(pCtx,iCost != iWantCost);` |
|      5 | 5464 | `	return PH7_OK;` |
|      4 | 5465 | `}` |
|      - | 5466 | `/*` |
|      - | 5467 | ` * filter_var() — input validation and sanitization (the ext/filter API).` |
|      - | 5468 | ` *` |
|      - | 5469 | ` * Filter and flag identifiers (values match PHP 8.5; the constants themselves` |
|      - | 5470 | ` * are registered in constant.c). The validate filters are hand-rolled rather` |
|      - | 5471 | ` * than delegating to SyStrToInt64/SyStrToReal: the former silently skips leading` |
|      - | 5472 | ` * zeros and cannot signal overflow, and the latter treats ',' as a decimal point` |
|      - | 5473 | ` * unconditionally — neither matches PHP's filter semantics.` |
|      - | 5474 | ` */` |
|      - | 5475 | `#define FV_VALIDATE_INT     257` |
|      - | 5476 | `#define FV_VALIDATE_BOOLEAN 258` |
|      - | 5477 | `#define FV_VALIDATE_FLOAT   259` |
|      - | 5478 | `#define FV_VALIDATE_REGEXP  272` |
|      - | 5479 | `#define FV_VALIDATE_URL     273` |
|      - | 5480 | `#define FV_VALIDATE_EMAIL   274` |
|      - | 5481 | `#define FV_VALIDATE_IP      275` |
|      - | 5482 | `#define FV_VALIDATE_MAC     276` |
|      - | 5483 | `#define FV_VALIDATE_DOMAIN  277` |
|      - | 5484 | `#define FV_SANITIZE_SPECIAL_CHARS      515` |
|      - | 5485 | `#define FV_DEFAULT          516 /* == FILTER_UNSAFE_RAW: pass the value through */` |
|      - | 5486 | `#define FV_SANITIZE_EMAIL   517` |
|      - | 5487 | `#define FV_SANITIZE_URL     518` |
|      - | 5488 | `#define FV_SANITIZE_NUMBER_INT   519` |
|      - | 5489 | `#define FV_SANITIZE_NUMBER_FLOAT 520` |
|      - | 5490 | `#define FV_SANITIZE_FULL_SPECIAL_CHARS 522` |
|      - | 5491 | `#define FV_FLAG_ALLOW_OCTAL  1` |
|      - | 5492 | `#define FV_FLAG_ALLOW_HEX    2` |
|      - | 5493 | `#define FV_FLAG_STRIP_LOW    4` |
|      - | 5494 | `#define FV_FLAG_STRIP_HIGH   8` |
|      - | 5495 | `#define FV_FLAG_ENCODE_LOW   16` |
|      - | 5496 | `#define FV_FLAG_ENCODE_HIGH  32` |
|      - | 5497 | `#define FV_FLAG_ENCODE_AMP   64` |
|      - | 5498 | `#define FV_FLAG_NO_ENCODE_QUOTES 128` |
|      - | 5499 | `#define FV_FLAG_STRIP_BACKTICK   512` |
|      - | 5500 | `#define FV_FLAG_ALLOW_FRACTION   4096` |
|      - | 5501 | `#define FV_FLAG_ALLOW_THOUSAND   8192` |
|      - | 5502 | `#define FV_FLAG_ALLOW_SCIENTIFIC 16384` |
|      - | 5503 | `#define FV_FLAG_IPV4  1048576` |
|      - | 5504 | `#define FV_FLAG_IPV6  2097152` |
|      - | 5505 | `#define FV_NULL_ON_FAILURE 134217728` |
|      - | 5506 | `/* The subset of flags the UNSAFE_RAW/DEFAULT string filter (FvSanitizeString)` |
|      - | 5507 | ` * acts on: when none are set the filter is a verbatim pass-through, so FV_DEFAULT` |
|      - | 5508 | ` * can shortcut. Keep this in sync with FvSanitizeString's flag handling. */` |
|      - | 5509 | `#define FV_FLAG_STRING_MASK (FV_FLAG_STRIP_LOW\|FV_FLAG_STRIP_HIGH\|FV_FLAG_STRIP_BACKTICK \` |
|      - | 5510 | `                            \|FV_FLAG_ENCODE_LOW\|FV_FLAG_ENCODE_HIGH\|FV_FLAG_ENCODE_AMP)` |
|      - | 5511 |  |
|      - | 5512 | `/* Trim leading/trailing PHP whitespace, adjusting the (*pz,*pn) view in place.` |
|      - | 5513 | ` * SyisSpace (isspace) matches PHP's filter whitespace set " \t\n\r\v\f". */` |
|    153 | 5514 | `static void FvTrim(const char **pz,int *pn){` |
|    153 | 5515 | `	const char *z = *pz;` |
|    153 | 5516 | `	int n = *pn;` |
|    157 | 5517 | `	while( n>0 && SyisSpace((unsigned char)z[0]) ){ z++; n--; }` |
|    161 | 5518 | `	while( n>0 && SyisSpace((unsigned char)z[n-1]) ){ n--; }` |
|    153 | 5519 | `	*pz = z; *pn = n;` |
|    153 | 5520 | `}` |
|      - | 5521 | `/* FILTER_VALIDATE_INT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     57 | 5522 | `static int FvValidateInt(const char *z,int n,int flags,ph7_int64 *pOut){` |
|     57 | 5523 | `	int neg = 0, i;` |
|     57 | 5524 | `	sxu64 u = 0;` |
|     57 | 5525 | `	FvTrim(&z,&n);` |
|     57 | 5526 | `	if( n==0 ){ return 0; }` |
|     51 | 5527 | `	if( z[0]=='+' \|\| z[0]=='-' ){ neg = (z[0]=='-'); z++; n--; }` |
|     51 | 5528 | `	if( n==0 ){ return 0; }` |
|     49 | 5529 | `	if( (flags & FV_FLAG_ALLOW_HEX) && n>=2 && z[0]=='0' && (z[1]=='x'\|\|z[1]=='X') ){` |
|      3 | 5530 | `		z += 2; n -= 2;` |
|      3 | 5531 | `		if( n==0 ){ return 0; }` |
|      7 | 5532 | `		for( i=0; i<n; i++ ){` |
|      5 | 5533 | `			int h = SyHexToint((unsigned char)z[i]);` |
|      5 | 5534 | `			if( h<0 ){ return 0; }` |
|      5 | 5535 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)h)/16 ){ return 0; }` |
|      5 | 5536 | `			u = u*16 + (sxu64)h;` |
|      3 | 5537 | `		}` |
|     48 | 5538 | `	}else if( (flags & FV_FLAG_ALLOW_OCTAL) && z[0]=='0' ){` |
|      9 | 5539 | `		for( i=0; i<n; i++ ){` |
|      7 | 5540 | `			if( z[i]<'0' \|\| z[i]>'7' ){ return 0; }` |
|      7 | 5541 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/8 ){ return 0; }` |
|      7 | 5542 | `			u = u*8 + (sxu64)(z[i]-'0');` |
|      4 | 5543 | `		}` |
|      2 | 5544 | `	}else{` |
|     45 | 5545 | `		if( z[0]=='0' && n>1 ){ return 0; } /* a leading zero is rejected in base 10 */` |
|    201 | 5546 | `		for( i=0; i<n; i++ ){` |
|    173 | 5547 | `			if( !SyisDigit((unsigned char)z[i]) ){ return 0; }` |
|    161 | 5548 | `			if( u > (0xFFFFFFFFFFFFFFFFULL - (sxu64)(z[i]-'0'))/10 ){ return 0; }` |
|    161 | 5549 | `			u = u*10 + (sxu64)(z[i]-'0');` |
|     81 | 5550 | `		}` |
|      - | 5551 | `	}` |
|     33 | 5552 | `	if( neg ){` |
|      5 | 5553 | `		if( u > 0x8000000000000000ULL ){ return 0; }` |
|      5 | 5554 | `		*pOut = (ph7_int64)(0ULL - u); /* two's-complement negate in unsigned space */` |
|      3 | 5555 | `	}else{` |
|     29 | 5556 | `		if( u > 0x7FFFFFFFFFFFFFFFULL ){ return 0; }` |
|     27 | 5557 | `		*pOut = (ph7_int64)u;` |
|      - | 5558 | `	}` |
|     31 | 5559 | `	return 1;` |
|     29 | 5560 | `}` |
|      - | 5561 | `/* FILTER_VALIDATE_FLOAT. Returns 1 and sets *pOut on success, 0 on failure. */` |
|     69 | 5562 | `static int FvValidateFloat(const char *z,int n,int flags,double *pOut){` |
|      - | 5563 | `	char zBuf[512];` |
|     69 | 5564 | `	int i, m = 0, seenDigit = 0;` |
|     69 | 5565 | `	const char *zv; int nv; double d = 0;` |
|     69 | 5566 | `	FvTrim(&z,&n);` |
|      - | 5567 | `	/* Bound the input: zBuf[512] holds the thousand-separator-stripped copy, and` |
|      - | 5568 | `	 * the cap also rejects the pathological 500+ digit floats PHP refuses. */` |
|     69 | 5569 | `	if( n==0 \|\| n>500 ){ return 0; }` |
|     69 | 5570 | `	if( flags & FV_FLAG_ALLOW_THOUSAND ){` |
|      - | 5571 | `		/* Commas are optional, but when present they must group the integer part` |
|      - | 5572 | `		 * into a leading run of 1..3 digits followed by groups of exactly 3` |
|      - | 5573 | `		 * ("1,000" ok, "1,5"/"1234,567" rejected). Strip them into zBuf and reject` |
|      - | 5574 | `		 * a comma anywhere in the fractional/exponent tail. */` |
|     25 | 5575 | `		int s = 0, intEnd, segStart, segIdx, hasComma = 0;` |
|     25 | 5576 | `		if( s<n && (z[s]=='+'\|\|z[s]=='-') ){ zBuf[m++] = z[s]; s++; }` |
|     25 | 5577 | `		intEnd = s;` |
|    167 | 5578 | `		while( intEnd<n && z[intEnd]!='.' && z[intEnd]!='e' && z[intEnd]!='E' ){` |
|    143 | 5579 | `			if( z[intEnd]==',' ){ hasComma = 1; }` |
|    143 | 5580 | `			intEnd++;` |
|      1 | 5581 | `		}` |
|     25 | 5582 | `		if( hasComma ){` |
|     25 | 5583 | `			segStart = s; segIdx = 0;` |
|    165 | 5584 | `			for( i=s; i<=intEnd; i++ ){` |
|    151 | 5585 | `				if( i==intEnd \|\| z[i]==',' ){` |
|     49 | 5586 | `					int segLen = i - segStart, k;` |
|     49 | 5587 | `					if( segIdx==0 ){ if( segLen<1 \|\| segLen>3 ){ return 0; } }` |
|     25 | 5588 | `					else if( segLen!=3 ){ return 0; }` |
|    119 | 5589 | `					for( k=segStart; k<i; k++ ){` |
|     81 | 5590 | `						if( !SyisDigit((unsigned char)z[k]) ){ return 0; }` |
|     81 | 5591 | `						zBuf[m++] = z[k];` |
|     41 | 5592 | `					}` |
|     39 | 5593 | `					segStart = i+1; segIdx++;` |
|     19 | 5594 | `				}` |
|     71 | 5595 | `			}` |
|      8 | 5596 | `		}else{` |
|    ! 0 | 5597 | `			for( i=s; i<intEnd; i++ ){ zBuf[m++] = z[i]; }` |
|      - | 5598 | `		}` |
|     27 | 5599 | `		for( i=intEnd; i<n; i++ ){` |
|     13 | 5600 | `			if( z[i]==',' ){ return 0; }` |
|     13 | 5601 | `			zBuf[m++] = z[i];` |
|      7 | 5602 | `		}` |
|     15 | 5603 | `		zv = zBuf; nv = m;` |
|      8 | 5604 | `	}else{` |
|     45 | 5605 | `		zv = z; nv = n;` |
|      - | 5606 | `	}` |
|     59 | 5607 | `	i = 0;` |
|     59 | 5608 | `	if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|    167 | 5609 | `	while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     59 | 5610 | `	if( i<nv && zv[i]=='.' ){` |
|     21 | 5611 | `		i++;` |
|     39 | 5612 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; seenDigit = 1; }` |
|     10 | 5613 | `	}` |
|     59 | 5614 | `	if( !seenDigit ){ return 0; }` |
|     57 | 5615 | `	if( i<nv && (zv[i]=='e'\|\|zv[i]=='E') ){` |
|     29 | 5616 | `		i++;` |
|     29 | 5617 | `		if( i<nv && (zv[i]=='+'\|\|zv[i]=='-') ){ i++; }` |
|     29 | 5618 | `		if( i>=nv \|\| !SyisDigit((unsigned char)zv[i]) ){ return 0; }` |
|    105 | 5619 | `		while( i<nv && SyisDigit((unsigned char)zv[i]) ){ i++; }` |
|     14 | 5620 | `	}` |
|     57 | 5621 | `	if( i!=nv ){ return 0; } /* trailing junk */` |
|      - | 5622 | `	/* The grammar above guarantees zv[0..nv) is a clean ASCII decimal float (no hex /` |
|      - | 5623 | `	 * inf / nan / trailing junk), so it is safe to hand to libc strtod, which — unlike` |
|      - | 5624 | `	 * SyStrToReal (15 sig-digits + exponent clamped to 308, so it silently saturates` |
|      - | 5625 | `	 * overflowing magnitudes to a finite value) — is overflow/underflow-aware and` |
|      - | 5626 | `	 * correctly rounded. strtod needs a NUL-terminated string: the ALLOW_THOUSAND path` |
|      - | 5627 | `	 * already built the span in zBuf (zv==zBuf); the plain path must copy it there (z is` |
|      - | 5628 | `	 * const + not NUL-terminated). nv <= n <= 500 < sizeof(zBuf) by the cap above.` |
|      - | 5629 | `	 * Matches PHP 8.5 byte-for-byte: reject overflow (-> +/-INF) and total underflow` |
|      - | 5630 | `	 * (-> 0.0), keep subnormals (nonzero, errno==ERANGE) and a genuine "0" (errno==0). */` |
|     53 | 5631 | `	if( zv != zBuf ){ SyMemcpy(zv,zBuf,(sxu32)nv); }` |
|     53 | 5632 | `	zBuf[nv] = 0;` |
|     53 | 5633 | `	errno = 0;` |
|     53 | 5634 | `	d = strtod(zBuf,0);` |
|     53 | 5635 | `	if( errno == ERANGE && (d == HUGE_VAL \|\| d == -HUGE_VAL \|\| d == 0.0) ){` |
|     15 | 5636 | `		return 0;` |
|      - | 5637 | `	}` |
|     39 | 5638 | `	*pOut = d;` |
|     39 | 5639 | `	return 1;` |
|     35 | 5640 | `}` |
|      - | 5641 | `/* FILTER_VALIDATE_BOOLEAN. Returns 1 if the string is recognized (sets *pBool),` |
|      - | 5642 | ` * 0 if it is unrecognized (the failure path). "0"/"false"/"" are recognized as` |
|      - | 5643 | ` * false, NOT failures. */` |
|     33 | 5644 | `static int FvValidateBool(const char *z,int n,int *pBool){` |
|     33 | 5645 | `	FvTrim(&z,&n);` |
|     32 | 5646 | `	if( (n==1 && z[0]=='1') \|\| (n==4 && SyStrnicmp(z,"true",4)==0)` |
|     25 | 5647 | `	    \|\| (n==2 && SyStrnicmp(z,"on",2)==0) \|\| (n==3 && SyStrnicmp(z,"yes",3)==0) ){` |
|     11 | 5648 | `		*pBool = 1; return 1;` |
|      - | 5649 | `	}` |
|     22 | 5650 | `	if( n==0 \|\| (n==1 && z[0]=='0') \|\| (n==5 && SyStrnicmp(z,"false",5)==0)` |
|     11 | 5651 | `	    \|\| (n==3 && SyStrnicmp(z,"off",3)==0) \|\| (n==2 && SyStrnicmp(z,"no",2)==0) ){` |
|     11 | 5652 | `		*pBool = 0; return 1;` |
|      - | 5653 | `	}` |
|      9 | 5654 | `	return 0;` |
|     15 | 5655 | `}` |
|      - | 5656 | `/* IPv4 dotted-quad: exactly 4 octets 0..255, no leading zeros. */` |
|     33 | 5657 | `static int FvValidateIp4(const char *z,int n){` |
|     33 | 5658 | `	int i = 0, parts = 0;` |
|     77 | 5659 | `	while( i<n ){` |
|     65 | 5660 | `		int val = 0, digits = 0, start = i;` |
|    143 | 5661 | `		while( i<n && SyisDigit((unsigned char)z[i]) ){` |
|     85 | 5662 | `			val = val*10 + (z[i]-'0');` |
|     85 | 5663 | `			if( val>255 ){ return 0; }` |
|     79 | 5664 | `			digits++; i++;` |
|      1 | 5665 | `		}` |
|     59 | 5666 | `		if( digits==0 \|\| digits>3 ){ return 0; }` |
|     49 | 5667 | `		if( digits>1 && z[start]=='0' ){ return 0; } /* leading zero */` |
|     45 | 5668 | `		parts++;` |
|     45 | 5669 | `		if( parts>4 ){ return 0; }` |
|     45 | 5670 | `		if( i<n ){` |
|     33 | 5671 | `			if( z[i]!='.' ){ return 0; }` |
|     33 | 5672 | `			i++;` |
|     33 | 5673 | `			if( i>=n ){ return 0; } /* trailing dot */` |
|     16 | 5674 | `		}` |
|      1 | 5675 | `	}` |
|     13 | 5676 | `	return parts==4;` |
|     17 | 5677 | `}` |
|      - | 5678 | `/* A colon-separated run of IPv6 hextets with no "::" (n may be 0 -> 0 groups),` |
|      - | 5679 | ` * allowing a trailing embedded IPv4. Returns the 16-bit group count or -1. */` |
|     19 | 5680 | `static int FvIp6Hextets(const char *z,int n){` |
|     19 | 5681 | `	int i = 0, segStart = 0, groups = 0;` |
|     19 | 5682 | `	if( n==0 ){ return 0; }` |
|    145 | 5683 | `	while( i<=n ){` |
|    133 | 5684 | `		if( i==n \|\| z[i]==':' ){` |
|     23 | 5685 | `			int segLen = i - segStart, j, isV4 = 0;` |
|     23 | 5686 | `			if( segLen==0 ){ return -1; } /* an empty hextet (stray ':') */` |
|     77 | 5687 | `			for( j=segStart; j<i; j++ ){ if( z[j]=='.' ){ isV4 = 1; break; } }` |
|     23 | 5688 | `			if( isV4 ){` |
|     11 | 5689 | `				if( i!=n ){ return -1; } /* IPv4 only as the final token */` |
|     11 | 5690 | `				if( !FvValidateIp4(z+segStart,segLen) ){ return -1; }` |
|      5 | 5691 | `				groups += 2;` |
|      3 | 5692 | `			}else{` |
|     13 | 5693 | `				if( segLen>4 ){ return -1; }` |
|     47 | 5694 | `				for( j=segStart; j<i; j++ ){ if( SyHexToint((unsigned char)z[j])<0 ){ return -1; } }` |
|     13 | 5695 | `				groups++;` |
|      - | 5696 | `			}` |
|     17 | 5697 | `			segStart = i+1;` |
|      8 | 5698 | `		}` |
|    127 | 5699 | `		i++;` |
|      1 | 5700 | `	}` |
|     13 | 5701 | `	return groups;` |
|     10 | 5702 | `}` |
|      - | 5703 | `/* IPv6: at most one "::" zero-run; 8 groups exactly, or fewer when "::" present. */` |
|     19 | 5704 | `static int FvValidateIp6(const char *z,int n){` |
|     19 | 5705 | `	const char *zDbl = 0;` |
|      - | 5706 | `	int i, ga, gb;` |
|    139 | 5707 | `	for( i=0; i+1<n; i++ ){` |
|    123 | 5708 | `		if( z[i]==':' && z[i+1]==':' ){` |
|     13 | 5709 | `			if( zDbl ){ return 0; } /* a second "::" is invalid */` |
|     11 | 5710 | `			zDbl = z+i;` |
|      5 | 5711 | `		}` |
|     61 | 5712 | `	}` |
|     17 | 5713 | `	if( zDbl==0 ){` |
|      9 | 5714 | `		return FvIp6Hextets(z,n)==8;` |
|    ! 0 | 5715 | `	}else{` |
|      9 | 5716 | `		int lenA = (int)(zDbl - z);` |
|      9 | 5717 | `		int lenB = n - lenA - 2;` |
|      9 | 5718 | `		ga = (lenA==0) ? 0 : FvIp6Hextets(z,lenA);` |
|      9 | 5719 | `		gb = (lenB==0) ? 0 : FvIp6Hextets(zDbl+2,lenB);` |
|      9 | 5720 | `		if( ga<0 \|\| gb<0 ){ return 0; }` |
|      9 | 5721 | `		return (ga+gb)<=7; /* "::" stands for at least one zero group */` |
|      - | 5722 | `	}` |
|     10 | 5723 | `}` |
|     25 | 5724 | `static int FvValidateIp(const char *z,int n,int flags){` |
|     25 | 5725 | `	int v4 = (flags & FV_FLAG_IPV4), v6 = (flags & FV_FLAG_IPV6);` |
|     25 | 5726 | `	if( !v4 && !v6 ){ v4 = v6 = 1; } /* default accepts either family */` |
|     25 | 5727 | `	if( v4 && FvValidateIp4(z,n) ){ return 1; }` |
|     21 | 5728 | `	if( v6 && FvValidateIp6(z,n) ){ return 1; }` |
|     13 | 5729 | `	return 0;` |
|     13 | 5730 | `}` |
|      - | 5731 | `/* FILTER_VALIDATE_MAC: 17-char colon- or dash-separated hex (XX:XX:..:XX). */` |
|     11 | 5732 | `static int FvValidateMac(const char *z,int n){` |
|      - | 5733 | `	char sep;` |
|      - | 5734 | `	int i;` |
|     11 | 5735 | `	if( n!=17 ){ return 0; }` |
|      7 | 5736 | `	sep = z[2];` |
|      7 | 5737 | `	if( sep!=':' && sep!='-' ){ return 0; }` |
|    105 | 5738 | `	for( i=0; i<17; i++ ){` |
|    101 | 5739 | `		if( (i%3)==2 ){ if( z[i]!=sep ){ return 0; } }` |
|     71 | 5740 | `		else if( SyHexToint((unsigned char)z[i])<0 ){ return 0; }` |
|     50 | 5741 | `	}` |
|      5 | 5742 | `	return 1;` |
|      6 | 5743 | `}` |
|      - | 5744 | `/* FILTER_VALIDATE_EMAIL (best-effort: covers the common cases, not quoted local` |
|      - | 5745 | ` * parts or IP-literal domains). */` |
|     28 | 5746 | `static int FvValidateEmail(const char *z,int n){` |
|     28 | 5747 | `	int at = -1, i, localLen, domLen, labelStart, dotCount = 0;` |
|      - | 5748 | `	const char *zDom;` |
|     28 | 5749 | `	if( n==0 \|\| n>320 ){ return 0; }` |
|    201 | 5750 | `	for( i=0; i<n; i++ ){` |
|    181 | 5751 | `		if( z[i]=='@' ){ if( at>=0 ){ return 0; } at = i; }` |
|     91 | 5752 | `	}` |
|     21 | 5753 | `	if( at<=0 \|\| at==n-1 ){ return 0; } /* one '@', non-empty local and domain */` |
|     21 | 5754 | `	localLen = at;` |
|     21 | 5755 | `	zDom = z + at + 1;` |
|     21 | 5756 | `	domLen = n - at - 1;` |
|     21 | 5757 | `	if( z[0]=='.' \|\| z[at-1]=='.' ){ return 0; }` |
|     57 | 5758 | `	for( i=0; i<localLen; i++ ){` |
|     43 | 5759 | `		unsigned char c = (unsigned char)z[i];` |
|     43 | 5760 | `		if( c<=' ' ){ return 0; }` |
|     41 | 5761 | `		if( c=='.' && i+1<localLen && z[i+1]=='.' ){ return 0; }` |
|     20 | 5762 | `	}` |
|     15 | 5763 | `	if( zDom[0]=='.' \|\| zDom[domLen-1]=='.' ){ return 0; }` |
|     13 | 5764 | `	labelStart = 0;` |
|     85 | 5765 | `	for( i=0; i<=domLen; i++ ){` |
|     75 | 5766 | `		if( i==domLen \|\| zDom[i]=='.' ){` |
|     25 | 5767 | `			int ll = i - labelStart;` |
|     25 | 5768 | `			if( ll==0 ){ return 0; } /* consecutive dots */` |
|     23 | 5769 | `			if( zDom[labelStart]=='-' \|\| zDom[i-1]=='-' ){ return 0; }` |
|     23 | 5770 | `			if( i<domLen ){ dotCount++; }` |
|     23 | 5771 | `			labelStart = i+1;` |
|     12 | 5772 | `		}else{` |
|     51 | 5773 | `			unsigned char c = (unsigned char)zDom[i];` |
|     51 | 5774 | `			if( !((c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9')\|\|c=='-') ){ return 0; }` |
|      - | 5775 | `		}` |
|     37 | 5776 | `	}` |
|     11 | 5777 | `	if( dotCount<1 ){ return 0; } /* PHP requires a dot in the domain (any TLD length) */` |
|      9 | 5778 | `	return 1;` |
|     15 | 5779 | `}` |
|      - | 5780 | `/* FILTER_VALIDATE_DOMAIN (lenient, matching PHP without FILTER_FLAG_HOSTNAME). */` |
|     11 | 5781 | `static int FvValidateDomain(const char *z,int n){` |
|      - | 5782 | `	int i;` |
|     11 | 5783 | `	if( n<1 \|\| n>253 \|\| z[0]=='.' ){ return 0; }` |
|     81 | 5784 | `	for( i=0; i<n; i++ ){` |
|     75 | 5785 | `		unsigned char c = (unsigned char)z[i];` |
|     75 | 5786 | `		if( c<=' ' ){ return 0; }` |
|     75 | 5787 | `		if( c=='.' && i+1<n && z[i+1]=='.' ){ return 0; }` |
|     37 | 5788 | `	}` |
|      7 | 5789 | `	return 1;` |
|      6 | 5790 | `}` |
|      - | 5791 | `/* FILTER_VALIDATE_URL: require a scheme and a host (PHP's filter is itself` |
|      - | 5792 | ` * parse_url-based, so PH7_VmHttpSplitURI tracks it closely). */` |
|     15 | 5793 | `static int FvValidateUrl(const char *z,int n){` |
|      - | 5794 | `	SyhttpUri sUri;` |
|     15 | 5795 | `	if( n==0 ){ return 0; }` |
|     15 | 5796 | `	SyZero(&sUri,(sxu32)sizeof(sUri));` |
|     15 | 5797 | `	if( PH7_VmHttpSplitURI(&sUri,z,(sxu32)n)!=SXRET_OK ){ return 0; }` |
|     15 | 5798 | `	return sUri.sScheme.nByte!=0 && sUri.sHost.nByte!=0;` |
|      8 | 5799 | `}` |
|      - | 5800 | `/* The Fv sanitizers build their result by appending directly to the call` |
|      - | 5801 | ` * context (ph7_result_string accumulates, like htmlspecialchars), emitting each` |
|      - | 5802 | ` * kept run in one call and seeding "" so an all-stripped input yields "". */` |
|      - | 5803 | `/* SANITIZE_NUMBER_INT (isFloat=0) / SANITIZE_NUMBER_FLOAT (isFloat=1). */` |
|     37 | 5804 | `static void FvSanitizeNumber(ph7_context *pCtx,const char *z,int n,int isFloat,int flags){` |
|     37 | 5805 | `	int i, runStart = 0;` |
|     37 | 5806 | `	ph7_result_string(pCtx,"",0);` |
|     97 | 5807 | `	for( i=0; i<n; i++ ){` |
|     91 | 5808 | `		char c = z[i];` |
|     91 | 5809 | `		int keep = (c>='0'&&c<='9') \|\| c=='+' \|\| c=='-';` |
|     91 | 5810 | `		if( !keep && isFloat ){` |
|     38 | 5811 | `			keep = (c=='.' && (flags & FV_FLAG_ALLOW_FRACTION))` |
|     23 | 5812 | `			    \|\| (c==',' && (flags & FV_FLAG_ALLOW_THOUSAND))` |
|     36 | 5813 | `			    \|\| ((c=='e'\|\|c=='E') && (flags & FV_FLAG_ALLOW_SCIENTIFIC));` |
|     12 | 5814 | `		}` |
|     61 | 5815 | `		if( !keep ){` |
|     33 | 5816 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     33 | 5817 | `			runStart = i+1;` |
|     16 | 5818 | `		}` |
|     31 | 5819 | `	}` |
|      7 | 5820 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      7 | 5821 | `}` |
|      - | 5822 | `/* Return non-zero when byte c must be stripped under the STRIP_* flags. Shared` |
|      - | 5823 | ` * by the UNSAFE_RAW string filter and SANITIZE_SPECIAL_CHARS. STRIP_LOW drops` |
|      - | 5824 | `` * bytes <32, STRIP_HIGH drops bytes >=127 (incl. DEL), STRIP_BACKTICK drops '`'.`` |
|      - | 5825 | ` * Matches php_filter_strip(); verified byte-exact vs php 8.5.7. */` |
|    287 | 5826 | `static int FvStripByte(unsigned char c,int flags){` |
|    287 | 5827 | `	if( (flags & FV_FLAG_STRIP_LOW)      && c<32 )    { return 1; }` |
|    281 | 5828 | `	if( (flags & FV_FLAG_STRIP_HIGH)     && c>=127 )  { return 1; }` |
|    269 | 5829 | `	if( (flags & FV_FLAG_STRIP_BACKTICK) && c==0x60 ) { return 1; }` |
|    267 | 5830 | `	return 0;` |
|    144 | 5831 | `}` |
|      - | 5832 | `/* FILTER_UNSAFE_RAW / FILTER_DEFAULT with flags: no default transform, but the` |
|      - | 5833 | ` * STRIP/ENCODE flags apply. Precedence (per php_filter_unsafe_raw, verified` |
|      - | 5834 | ` * vs php 8.5.7): a byte is first tested for stripping; a surviving byte is then` |
|      - | 5835 | ` * encoded as a decimal numeric entity if ENCODE_LOW (<32) / ENCODE_HIGH (>=127)` |
|      - | 5836 | ` * is set, and '&' becomes "&#38;" under ENCODE_AMP. So STRIP_LOW\|ENCODE_LOW` |
|      - | 5837 | ` * strips (nothing left to encode). Bytes are treated individually — ENCODE_HIGH` |
|      - | 5838 | ` * numeric-encodes each byte of a multibyte sequence separately, not the codepoint. */` |
|     25 | 5839 | `static void FvSanitizeString(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 5840 | `	int i, runStart = 0;` |
|     25 | 5841 | `	ph7_result_string(pCtx,"",0);` |
|    193 | 5842 | `	for( i=0; i<n; i++ ){` |
|    179 | 5843 | `		unsigned char c = (unsigned char)z[i];` |
|    179 | 5844 | `		if( FvStripByte(c,flags) ){` |
|     13 | 5845 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     13 | 5846 | `			runStart = i+1;` |
|     13 | 5847 | `			continue;` |
|      - | 5848 | `		}` |
|    167 | 5849 | `		if( c=='&' && (flags & FV_FLAG_ENCODE_AMP) ){` |
|      3 | 5850 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      3 | 5851 | `			ph7_result_string(pCtx,"&#38;",-1);` |
|      3 | 5852 | `			runStart = i+1;` |
|    166 | 5853 | `		}else if( (c<32 && (flags & FV_FLAG_ENCODE_LOW))` |
|    164 | 5854 | `		       \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     37 | 5855 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 5856 | `			ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|      9 | 5857 | `			runStart = i+1;` |
|      4 | 5858 | `		}` |
|     79 | 5859 | `	}` |
|     15 | 5860 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     15 | 5861 | `}` |
|      - | 5862 | `/* FILTER_SANITIZE_SPECIAL_CHARS: encode <>&"' and every control byte <32 as a` |
|      - | 5863 | ` * decimal numeric entity (&#60; &#38; &#34; ...). The STRIP_* flags remove bytes` |
|      - | 5864 | ` * before encoding; ENCODE_HIGH numeric-encodes surviving bytes >=127. Bytes >=128` |
|      - | 5865 | ` * are otherwise passed through verbatim (this filter is NOT UTF-8-aware — only the` |
|      - | 5866 | ` * FULL variant is). Byte-exact vs php 8.5.7. */` |
|     13 | 5867 | `static void FvSanitizeSpecial(ph7_context *pCtx,const char *z,int n,int flags){` |
|     13 | 5868 | `	int i, runStart = 0;` |
|      - | 5869 | `	const char *zEnt;` |
|     13 | 5870 | `	ph7_result_string(pCtx,"",0);` |
|    131 | 5871 | `	for( i=0; i<n; i++ ){` |
|    119 | 5872 | `		unsigned char c = (unsigned char)z[i];` |
|    119 | 5873 | `		if( FvStripByte(c,flags) ){` |
|      9 | 5874 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|      9 | 5875 | `			runStart = i+1;` |
|      9 | 5876 | `			continue;` |
|      - | 5877 | `		}` |
|    111 | 5878 | `		switch( c ){` |
|      3 | 5879 | `		case '<':  zEnt = "&#60;"; break;` |
|      3 | 5880 | `		case '>':  zEnt = "&#62;"; break;` |
|     11 | 5881 | `		case '&':  zEnt = "&#38;"; break;` |
|      3 | 5882 | `		case '"':  zEnt = "&#34;"; break;` |
|      3 | 5883 | `		case '\'': zEnt = "&#39;"; break;` |
|     46 | 5884 | `		default:` |
|      - | 5885 | `			/* Control bytes <32 are always numeric-encoded; bytes >=127 only when` |
|      - | 5886 | `			 * ENCODE_HIGH is set. Everything else stays in the current run. */` |
|     93 | 5887 | `			if( c<32 \|\| (c>=127 && (flags & FV_FLAG_ENCODE_HIGH)) ){` |
|     17 | 5888 | `				if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     17 | 5889 | `				ph7_result_string_format(pCtx,"&#%d;",(int)c);` |
|     17 | 5890 | `				runStart = i+1;` |
|      8 | 5891 | `			}` |
|     93 | 5892 | `			continue; /* keep in the current run */` |
|      - | 5893 | `		}` |
|     19 | 5894 | `		if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     19 | 5895 | `		ph7_result_string(pCtx,zEnt,-1); /* -1: length from strlen */` |
|     19 | 5896 | `		runStart = i+1;` |
|     10 | 5897 | `	}` |
|     13 | 5898 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|     13 | 5899 | `}` |
|      - | 5900 | `/* HTML 4.01 named-entity table (codepoint -> "&name;") used by the UTF-8-aware` |
|      - | 5901 | ` * FULL_SPECIAL_CHARS filter, sorted ascending by codepoint for binary search.` |
|      - | 5902 | ` * Generated from php 8.5.7 (the exact set php_escape_html_entities emits for the` |
|      - | 5903 | ` * default document type); the five inline specials <>&"' are handled separately,` |
|      - | 5904 | ` * so every entry here is a codepoint >=0xA0. 248 rows. */` |
|      - | 5905 | `static const struct { sxu32 cp; const char *zEnt; } aHtml401Ent[] = {` |
|      - | 5906 | `	{0x00A0,"&nbsp;"},{0x00A1,"&iexcl;"},{0x00A2,"&cent;"},{0x00A3,"&pound;"},` |
|      - | 5907 | `	{0x00A4,"&curren;"},{0x00A5,"&yen;"},{0x00A6,"&brvbar;"},{0x00A7,"&sect;"},` |
|      - | 5908 | `	{0x00A8,"&uml;"},{0x00A9,"&copy;"},{0x00AA,"&ordf;"},{0x00AB,"&laquo;"},` |
|      - | 5909 | `	{0x00AC,"&not;"},{0x00AD,"&shy;"},{0x00AE,"&reg;"},{0x00AF,"&macr;"},` |
|      - | 5910 | `	{0x00B0,"&deg;"},{0x00B1,"&plusmn;"},{0x00B2,"&sup2;"},{0x00B3,"&sup3;"},` |
|      - | 5911 | `	{0x00B4,"&acute;"},{0x00B5,"&micro;"},{0x00B6,"&para;"},{0x00B7,"&middot;"},` |
|      - | 5912 | `	{0x00B8,"&cedil;"},{0x00B9,"&sup1;"},{0x00BA,"&ordm;"},{0x00BB,"&raquo;"},` |
|      - | 5913 | `	{0x00BC,"&frac14;"},{0x00BD,"&frac12;"},{0x00BE,"&frac34;"},{0x00BF,"&iquest;"},` |
|      - | 5914 | `	{0x00C0,"&Agrave;"},{0x00C1,"&Aacute;"},{0x00C2,"&Acirc;"},{0x00C3,"&Atilde;"},` |
|      - | 5915 | `	{0x00C4,"&Auml;"},{0x00C5,"&Aring;"},{0x00C6,"&AElig;"},{0x00C7,"&Ccedil;"},` |
|      - | 5916 | `	{0x00C8,"&Egrave;"},{0x00C9,"&Eacute;"},{0x00CA,"&Ecirc;"},{0x00CB,"&Euml;"},` |
|      - | 5917 | `	{0x00CC,"&Igrave;"},{0x00CD,"&Iacute;"},{0x00CE,"&Icirc;"},{0x00CF,"&Iuml;"},` |
|      - | 5918 | `	{0x00D0,"&ETH;"},{0x00D1,"&Ntilde;"},{0x00D2,"&Ograve;"},{0x00D3,"&Oacute;"},` |
|      - | 5919 | `	{0x00D4,"&Ocirc;"},{0x00D5,"&Otilde;"},{0x00D6,"&Ouml;"},{0x00D7,"&times;"},` |
|      - | 5920 | `	{0x00D8,"&Oslash;"},{0x00D9,"&Ugrave;"},{0x00DA,"&Uacute;"},{0x00DB,"&Ucirc;"},` |
|      - | 5921 | `	{0x00DC,"&Uuml;"},{0x00DD,"&Yacute;"},{0x00DE,"&THORN;"},{0x00DF,"&szlig;"},` |
|      - | 5922 | `	{0x00E0,"&agrave;"},{0x00E1,"&aacute;"},{0x00E2,"&acirc;"},{0x00E3,"&atilde;"},` |
|      - | 5923 | `	{0x00E4,"&auml;"},{0x00E5,"&aring;"},{0x00E6,"&aelig;"},{0x00E7,"&ccedil;"},` |
|      - | 5924 | `	{0x00E8,"&egrave;"},{0x00E9,"&eacute;"},{0x00EA,"&ecirc;"},{0x00EB,"&euml;"},` |
|      - | 5925 | `	{0x00EC,"&igrave;"},{0x00ED,"&iacute;"},{0x00EE,"&icirc;"},{0x00EF,"&iuml;"},` |
|      - | 5926 | `	{0x00F0,"&eth;"},{0x00F1,"&ntilde;"},{0x00F2,"&ograve;"},{0x00F3,"&oacute;"},` |
|      - | 5927 | `	{0x00F4,"&ocirc;"},{0x00F5,"&otilde;"},{0x00F6,"&ouml;"},{0x00F7,"&divide;"},` |
|      - | 5928 | `	{0x00F8,"&oslash;"},{0x00F9,"&ugrave;"},{0x00FA,"&uacute;"},{0x00FB,"&ucirc;"},` |
|      - | 5929 | `	{0x00FC,"&uuml;"},{0x00FD,"&yacute;"},{0x00FE,"&thorn;"},{0x00FF,"&yuml;"},` |
|      - | 5930 | `	{0x0152,"&OElig;"},{0x0153,"&oelig;"},{0x0160,"&Scaron;"},{0x0161,"&scaron;"},` |
|      - | 5931 | `	{0x0178,"&Yuml;"},{0x0192,"&fnof;"},{0x02C6,"&circ;"},{0x02DC,"&tilde;"},` |
|      - | 5932 | `	{0x0391,"&Alpha;"},{0x0392,"&Beta;"},{0x0393,"&Gamma;"},{0x0394,"&Delta;"},` |
|      - | 5933 | `	{0x0395,"&Epsilon;"},{0x0396,"&Zeta;"},{0x0397,"&Eta;"},{0x0398,"&Theta;"},` |
|      - | 5934 | `	{0x0399,"&Iota;"},{0x039A,"&Kappa;"},{0x039B,"&Lambda;"},{0x039C,"&Mu;"},` |
|      - | 5935 | `	{0x039D,"&Nu;"},{0x039E,"&Xi;"},{0x039F,"&Omicron;"},{0x03A0,"&Pi;"},` |
|      - | 5936 | `	{0x03A1,"&Rho;"},{0x03A3,"&Sigma;"},{0x03A4,"&Tau;"},{0x03A5,"&Upsilon;"},` |
|      - | 5937 | `	{0x03A6,"&Phi;"},{0x03A7,"&Chi;"},{0x03A8,"&Psi;"},{0x03A9,"&Omega;"},` |
|      - | 5938 | `	{0x03B1,"&alpha;"},{0x03B2,"&beta;"},{0x03B3,"&gamma;"},{0x03B4,"&delta;"},` |
|      - | 5939 | `	{0x03B5,"&epsilon;"},{0x03B6,"&zeta;"},{0x03B7,"&eta;"},{0x03B8,"&theta;"},` |
|      - | 5940 | `	{0x03B9,"&iota;"},{0x03BA,"&kappa;"},{0x03BB,"&lambda;"},{0x03BC,"&mu;"},` |
|      - | 5941 | `	{0x03BD,"&nu;"},{0x03BE,"&xi;"},{0x03BF,"&omicron;"},{0x03C0,"&pi;"},` |
|      - | 5942 | `	{0x03C1,"&rho;"},{0x03C2,"&sigmaf;"},{0x03C3,"&sigma;"},{0x03C4,"&tau;"},` |
|      - | 5943 | `	{0x03C5,"&upsilon;"},{0x03C6,"&phi;"},{0x03C7,"&chi;"},{0x03C8,"&psi;"},` |
|      - | 5944 | `	{0x03C9,"&omega;"},{0x03D1,"&thetasym;"},{0x03D2,"&upsih;"},{0x03D6,"&piv;"},` |
|      - | 5945 | `	{0x2002,"&ensp;"},{0x2003,"&emsp;"},{0x2009,"&thinsp;"},{0x200C,"&zwnj;"},` |
|      - | 5946 | `	{0x200D,"&zwj;"},{0x200E,"&lrm;"},{0x200F,"&rlm;"},{0x2013,"&ndash;"},` |
|      - | 5947 | `	{0x2014,"&mdash;"},{0x2018,"&lsquo;"},{0x2019,"&rsquo;"},{0x201A,"&sbquo;"},` |
|      - | 5948 | `	{0x201C,"&ldquo;"},{0x201D,"&rdquo;"},{0x201E,"&bdquo;"},{0x2020,"&dagger;"},` |
|      - | 5949 | `	{0x2021,"&Dagger;"},{0x2022,"&bull;"},{0x2026,"&hellip;"},{0x2030,"&permil;"},` |
|      - | 5950 | `	{0x2032,"&prime;"},{0x2033,"&Prime;"},{0x2039,"&lsaquo;"},{0x203A,"&rsaquo;"},` |
|      - | 5951 | `	{0x203E,"&oline;"},{0x2044,"&frasl;"},{0x20AC,"&euro;"},{0x2111,"&image;"},` |
|      - | 5952 | `	{0x2118,"&weierp;"},{0x211C,"&real;"},{0x2122,"&trade;"},{0x2135,"&alefsym;"},` |
|      - | 5953 | `	{0x2190,"&larr;"},{0x2191,"&uarr;"},{0x2192,"&rarr;"},{0x2193,"&darr;"},` |
|      - | 5954 | `	{0x2194,"&harr;"},{0x21B5,"&crarr;"},{0x21D0,"&lArr;"},{0x21D1,"&uArr;"},` |
|      - | 5955 | `	{0x21D2,"&rArr;"},{0x21D3,"&dArr;"},{0x21D4,"&hArr;"},{0x2200,"&forall;"},` |
|      - | 5956 | `	{0x2202,"&part;"},{0x2203,"&exist;"},{0x2205,"&empty;"},{0x2207,"&nabla;"},` |
|      - | 5957 | `	{0x2208,"&isin;"},{0x2209,"&notin;"},{0x220B,"&ni;"},{0x220F,"&prod;"},` |
|      - | 5958 | `	{0x2211,"&sum;"},{0x2212,"&minus;"},{0x2217,"&lowast;"},{0x221A,"&radic;"},` |
|      - | 5959 | `	{0x221D,"&prop;"},{0x221E,"&infin;"},{0x2220,"&ang;"},{0x2227,"&and;"},` |
|      - | 5960 | `	{0x2228,"&or;"},{0x2229,"&cap;"},{0x222A,"&cup;"},{0x222B,"&int;"},` |
|      - | 5961 | `	{0x2234,"&there4;"},{0x223C,"&sim;"},{0x2245,"&cong;"},{0x2248,"&asymp;"},` |
|      - | 5962 | `	{0x2260,"&ne;"},{0x2261,"&equiv;"},{0x2264,"&le;"},{0x2265,"&ge;"},` |
|      - | 5963 | `	{0x2282,"&sub;"},{0x2283,"&sup;"},{0x2284,"&nsub;"},{0x2286,"&sube;"},` |
|      - | 5964 | `	{0x2287,"&supe;"},{0x2295,"&oplus;"},{0x2297,"&otimes;"},{0x22A5,"&perp;"},` |
|      - | 5965 | `	{0x22C5,"&sdot;"},{0x2308,"&lceil;"},{0x2309,"&rceil;"},{0x230A,"&lfloor;"},` |
|      - | 5966 | `	{0x230B,"&rfloor;"},{0x2329,"&lang;"},{0x232A,"&rang;"},{0x25CA,"&loz;"},` |
|      - | 5967 | `	{0x2660,"&spades;"},{0x2663,"&clubs;"},{0x2665,"&hearts;"},{0x2666,"&diams;"}` |
|      - | 5968 | `};` |
|      - | 5969 | `/* Binary-search aHtml401Ent[] for cp; return its "&name;" entity or 0. */` |
|     41 | 5970 | `static const char *FvHtml401Lookup(sxu32 cp){` |
|     41 | 5971 | `	int lo = 0, hi = (int)SX_ARRAYSIZE(aHtml401Ent) - 1;` |
|    323 | 5972 | `	while( lo <= hi ){` |
|    309 | 5973 | `		int mid = (lo + hi) / 2;` |
|    309 | 5974 | `		sxu32 c = aHtml401Ent[mid].cp;` |
|    309 | 5975 | `		if( c == cp ){ return aHtml401Ent[mid].zEnt; }` |
|    283 | 5976 | `		if( c < cp ){ lo = mid + 1; } else { hi = mid - 1; }` |
|      1 | 5977 | `	}` |
|     15 | 5978 | `	return 0;` |
|     21 | 5979 | `}` |
|      - | 5980 | `/* Decode one strict-UTF-8 sequence at p (< zEnd). On success returns its byte` |
|      - | 5981 | ` * length (1..4) and sets *pCp to the codepoint; on any malformed, overlong,` |
|      - | 5982 | ` * surrogate, truncated or out-of-range (>U+10FFFF) sequence returns 0. Matches` |
|      - | 5983 | ` * PHP's UTF-8 validation used by FULL_SPECIAL_CHARS (verified vs php 8.5.7). */` |
|    101 | 5984 | `static int FvUtf8Next(const unsigned char *p,const unsigned char *zEnd,sxu32 *pCp){` |
|    101 | 5985 | `	unsigned char c = p[0];` |
|    101 | 5986 | `	if( c < 0x80 ){ *pCp = c; return 1; }` |
|    101 | 5987 | `	if( c < 0xC2 ){ return 0; }              /* 0x80-0xBF stray cont / 0xC0-0xC1 overlong */` |
|     99 | 5988 | `	if( c < 0xE0 ){                          /* 2-byte: U+0080..U+07FF */` |
|     47 | 5989 | `		if( zEnd-p < 2 \|\| (p[1]&0xC0)!=0x80 ){ return 0; }` |
|     45 | 5990 | `		*pCp = ((sxu32)(c&0x1F)<<6) \| (p[1]&0x3F);` |
|     45 | 5991 | `		return 2;` |
|      - | 5992 | `	}` |
|     53 | 5993 | `	if( c < 0xF0 ){                          /* 3-byte: U+0800..U+FFFF minus surrogates */` |
|      - | 5994 | `		sxu32 cp;` |
|     47 | 5995 | `		if( zEnd-p < 3 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 ){ return 0; }` |
|     33 | 5996 | `		cp = ((sxu32)(c&0x0F)<<12) \| ((sxu32)(p[1]&0x3F)<<6) \| (p[2]&0x3F);` |
|     33 | 5997 | `		if( cp < 0x800 \|\| (cp>=0xD800 && cp<=0xDFFF) ){ return 0; }` |
|     29 | 5998 | `		*pCp = cp;` |
|     29 | 5999 | `		return 3;` |
|      - | 6000 | `	}` |
|      7 | 6001 | `	if( c < 0xF5 ){                          /* 4-byte: U+10000..U+10FFFF */` |
|      - | 6002 | `		sxu32 cp;` |
|      5 | 6003 | `		if( zEnd-p < 4 \|\| (p[1]&0xC0)!=0x80 \|\| (p[2]&0xC0)!=0x80 \|\| (p[3]&0xC0)!=0x80 ){ return 0; }` |
|      5 | 6004 | `		cp = ((sxu32)(c&0x07)<<18) \| ((sxu32)(p[1]&0x3F)<<12) \| ((sxu32)(p[2]&0x3F)<<6) \| (p[3]&0x3F);` |
|      5 | 6005 | `		if( cp < 0x10000 \|\| cp > 0x10FFFF ){ return 0; }` |
|      5 | 6006 | `		*pCp = cp;` |
|      5 | 6007 | `		return 4;` |
|      - | 6008 | `	}` |
|      3 | 6009 | `	return 0;                                /* 0xF5-0xFF */` |
|     51 | 6010 | `}` |
|      - | 6011 | `/* FILTER_SANITIZE_FULL_SPECIAL_CHARS: htmlentities-style, UTF-8-aware. Encodes` |
|      - | 6012 | ` * <>&"' as named entities ("'" -> &#039;; quotes suppressed under NO_ENCODE_QUOTES),` |
|      - | 6013 | ` * and every valid UTF-8 codepoint with an HTML 4.01 named entity as that entity;` |
|      - | 6014 | ` * valid codepoints without a named entity (and low control bytes) pass through` |
|      - | 6015 | ` * verbatim. If the input contains ANY invalid UTF-8 the whole result is "".` |
|      - | 6016 | ` * The STRIP/ENCODE flags do NOT apply to this filter (only NO_ENCODE_QUOTES).` |
|      - | 6017 | ` * php's filter does NOT re-encode valid pre-existing entities ("&amp;" stays,` |
|      - | 6018 | ` * "&bogus;" becomes "&amp;bogus;"), i.e. double_encode=false semantics —` |
|      - | 6019 | ` * exactly htmlentities(ENT_QUOTES\|ENT_HTML401, double_encode: false), so this` |
|      - | 6020 | ` * delegates to the shared encoder. Byte-exact vs php 8.5.7. */` |
|     25 | 6021 | `static void FvSanitizeFull(ph7_context *pCtx,const char *z,int n,int flags){` |
|     25 | 6022 | `	int iEntFlags = (flags & FV_FLAG_NO_ENCODE_QUOTES) ? 0 : PH7_ENT_QUOTES;` |
|     25 | 6023 | `	HtmlEscape(pCtx,z,n,iEntFlags,1/*bAll*/,0/*bDoubleEncode*/);` |
|     25 | 6024 | `}` |
|      - | 6025 | `/* ---------------------------------------------------------------------------` |
|      - | 6026 | ` * UTF-8-aware HTML entity core (htmlspecialchars/htmlentities family).` |
|      - | 6027 | ` * Prototyped next to the five builtins earlier in this file; lives here so it` |
|      - | 6028 | ` * can share aHtml401Ent[]/FvHtml401Lookup()/FvUtf8Next() with the filter_var` |
|      - | 6029 | ` * FULL_SPECIAL_CHARS filter above. Byte-exact vs php 8.5.7 (oracle-swept).` |
|      - | 6030 | ` * ------------------------------------------------------------------------ */` |
|      - | 6031 | `/* Encode cp as UTF-8 into zBuf (>= 4 bytes); return the byte length 1..4.` |
|      - | 6032 | ` * Thin wrapper over the engine-wide SX_WRITE_UTF8 (sxmacros.h). */` |
|    585 | 6033 | `static int HtmlCpUtf8(sxu32 cp,char *zBuf){` |
|    585 | 6034 | `	sxu8 *z = (sxu8 *)zBuf;` |
|    585 | 6035 | `	SX_WRITE_UTF8(z,cp);` |
|    585 | 6036 | `	return (int)(z - (sxu8 *)zBuf);` |
|      1 | 6037 | `}` |
|      - | 6038 | `/* Doctype-allowed codepoint test (php's unicode_cp_is_allowed) — gates what a` |
|      - | 6039 | ` * numeric reference may DECODE to. Oracle-pinned per doctype: HTML401` |
|      - | 6040 | ` * disallows C0 (except TAB/LF/CR) and DEL..U+009F; XML1 and XHTML share the` |
|      - | 6041 | ` * XML rules — DEL..U+009F allowed, U+FFFE/U+FFFF excluded; HTML5 swaps CR` |
|      - | 6042 | ` * for FF (0x0C) and excludes the noncharacters (U+FDD0..U+FDEF and every` |
|      - | 6043 | ` * U+xFFFE/U+xFFFF). Surrogates are disallowed everywhere. */` |
|     91 | 6044 | `static int HtmlCpAllowed(sxu32 cp,int iFlags){` |
|     91 | 6045 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|     91 | 6046 | `	if( cp==0x09 \|\| cp==0x0A ){ return 1; }` |
|     87 | 6047 | `	if( cp==0x0D ){ return iDoc != PH7_ENT_DOC_HTML5; }` |
|     85 | 6048 | `	if( cp==0x0C ){ return iDoc == PH7_ENT_DOC_HTML5; }` |
|     85 | 6049 | `	if( cp < 0x20 \|\| cp > 0x10FFFF ){ return 0; }` |
|     79 | 6050 | `	if( cp>=0xD800 && cp<=0xDFFF ){ return 0; }` |
|     77 | 6051 | `	if( cp>=0x7F && cp<=0x9F ){ return iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML; }` |
|     71 | 6052 | `	if( iDoc == PH7_ENT_DOC_XML1 \|\| iDoc == PH7_ENT_DOC_XHTML ){` |
|    ! 0 | 6053 | `		return cp!=0xFFFE && cp!=0xFFFF;` |
|      - | 6054 | `	}` |
|     71 | 6055 | `	if( iDoc == PH7_ENT_DOC_HTML5 ){` |
|      9 | 6056 | `		if( cp>=0xFDD0 && cp<=0xFDEF ){ return 0; }` |
|      9 | 6057 | `		if( (cp & 0xFFFF) >= 0xFFFE ){ return 0; }` |
|      4 | 6058 | `	}` |
|     71 | 6059 | `	return 1;` |
|     46 | 6060 | `}` |
|      - | 6061 | `/* The ENT_DISALLOWED gate for RAW characters on the ENCODE side. Same as the` |
|      - | 6062 | ` * decode gate except CR under HTML5: php's encode-side unicode_cp_is_allowed` |
|      - | 6063 | ` * keeps a literal "\r" verbatim under ENT_HTML5\|ENT_DISALLOWED while the` |
|      - | 6064 | ` * decode side leaves "&#13;" un-decoded (oracle-pinned at flags 176). */` |
|      9 | 6065 | `static int HtmlCpAllowedEncode(sxu32 cp,int iFlags){` |
|      9 | 6066 | `	if( cp==0x0D && (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 ){ return 1; }` |
|      9 | 6067 | `	return HtmlCpAllowed(cp,iFlags);` |
|      5 | 6068 | `}` |
|      - | 6069 | `/* Numeric-reference validity for the double_encode=false "is this already a` |
|      - | 6070 | ` * valid entity" test — a MUCH looser predicate than the decode gate above:` |
|      - | 6071 | ` * any codepoint <= U+10FFFF is valid (controls and surrogates included, every` |
|      - | 6072 | ` * doctype). ENT_DISALLOWED re-tightens non-HTML401 doctypes to the decode` |
|      - | 6073 | ` * gate, except that HTML5 exempts surrogates. All oracle-pinned: &#0; and` |
|      - | 6074 | ` * &#xD800; stay verbatim at flags 11 and 139; flags -1 (HTML5+DISALLOWED)` |
|      - | 6075 | ` * re-encodes &#0; and &#x10FFFF; but still keeps &#xD800;; flags 144` |
|      - | 6076 | ` * (XML1+DISALLOWED) re-encodes &#xD800;. */` |
|      9 | 6077 | `static int HtmlNumericAllowed(sxu32 cp,int iFlags){` |
|      9 | 6078 | `	if( cp > 0x10FFFF ){ return 0; }` |
|      7 | 6079 | `	if( (iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML401 ){ return 1; /* never tightened */ }` |
|    ! 0 | 6080 | `	if( (iFlags & PH7_ENT_DISALLOWED)` |
|    ! 0 | 6081 | `	 && !((iFlags & PH7_ENT_DOC_MASK)==PH7_ENT_DOC_HTML5 && cp>=0xD800 && cp<=0xDFFF)` |
|    ! 0 | 6082 | `	 && !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|    ! 0 | 6083 | `	return 1;` |
|      5 | 6084 | `}` |
|      - | 6085 | `/* How many bytes the malformed UTF-8 sequence at p consumes — php's` |
|      - | 6086 | ` * get_next_char failure step (one U+FFFD substitution / one ENT_IGNORE drop` |
|      - | 6087 | ` * per MAXIMAL invalid subpart, not per byte): a prefix-valid sequence eats` |
|      - | 6088 | ` * its continuation bytes ("\xE0\x80\xAF" is ONE unit) while a byte that could` |
|      - | 6089 | ` * start a new sequence is left for the next round. */` |
|      5 | 6090 | `static int HtmlUtf8Trail(unsigned char c){ return c>=0x80 && c<=0xBF; }` |
|     11 | 6091 | `static int HtmlUtf8Lead(unsigned char c){ return c<0x80 \|\| (c>=0xC2 && c<=0xF4); }` |
|     15 | 6092 | `static int HtmlUtf8FailAdvance(const unsigned char *p,const unsigned char *zEnd){` |
|     15 | 6093 | `	unsigned char c = p[0];` |
|     15 | 6094 | `	int nAvail = (int)(zEnd - p);` |
|     15 | 6095 | `	if( c < 0xC2 \|\| c > 0xF4 ){ return 1; } /* stray trail / C0-C1 / F5-FF */` |
|     13 | 6096 | `	if( c < 0xE0 ){` |
|      3 | 6097 | `		if( nAvail < 2 ){ return 1; }` |
|      3 | 6098 | `		return HtmlUtf8Lead(p[1]) ? 1 : 2;` |
|      - | 6099 | `	}` |
|     11 | 6100 | `	if( c < 0xF0 ){` |
|     11 | 6101 | `		if( nAvail >= 3 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) ){` |
|      3 | 6102 | `			return 3; /* complete but overlong/surrogate */` |
|      - | 6103 | `		}` |
|      9 | 6104 | `		if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 6105 | `		if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 6106 | `		return 3;` |
|      - | 6107 | `	}` |
|    ! 0 | 6108 | `	if( nAvail >= 4 && HtmlUtf8Trail(p[1]) && HtmlUtf8Trail(p[2]) && HtmlUtf8Trail(p[3]) ){` |
|    ! 0 | 6109 | `		return 4; /* complete but overlong / > U+10FFFF */` |
|      - | 6110 | `	}` |
|    ! 0 | 6111 | `	if( nAvail < 2 \|\| HtmlUtf8Lead(p[1]) ){ return 1; }` |
|    ! 0 | 6112 | `	if( nAvail < 3 \|\| HtmlUtf8Lead(p[2]) ){ return 2; }` |
|    ! 0 | 6113 | `	if( nAvail < 4 \|\| HtmlUtf8Lead(p[3]) ){ return 3; }` |
|    ! 0 | 6114 | `	return 4;` |
|      8 | 6115 | `}` |
|      - | 6116 | `/* The basic special entities, shared by named matching, the hsc_decode` |
|      - | 6117 | ` * numeric whitelist and the translation-table builder so the sets can never` |
|      - | 6118 | ` * drift apart. (&apos; is not an HTML 4.01 entity — doctype-gated below.) */` |
|      - | 6119 | `static const struct { const char *zEnt; int n; sxu32 cp; } aHtmlSpecEnt[] = {` |
|      - | 6120 | `	{"&amp;",5,38},{"&lt;",4,60},{"&gt;",4,62},{"&quot;",6,34},{"&apos;",6,39}` |
|      - | 6121 | `};` |
|      - | 6122 | `/* Does this doctype consult the named-entity table (aHtml401Ent)? XML 1.0 has` |
|      - | 6123 | ` * no named entities beyond the specials; XHTML/HTML5 are approximated by the` |
|      - | 6124 | ` * HTML 4.01 table (documented divergence, PLAN.md §3.9). */` |
|     63 | 6125 | `static int HtmlDocHasNamedTable(int iDoc){` |
|     63 | 6126 | `	return iDoc != PH7_ENT_DOC_XML1;` |
|      1 | 6127 | `}` |
|      - | 6128 | `/* The single-quote entity per doctype. Oracle-pinned asymmetry: for every` |
|      - | 6129 | ` * non-HTML401 doctype htmlspecialchars emits &apos; while htmlentities` |
|      - | 6130 | ` * (bEntities) keeps &#039; under XHTML too. The translation table mirrors` |
|      - | 6131 | ` * whichever function the requested table belongs to. */` |
|     29 | 6132 | `static const char *HtmlAposEntity(int iDoc,int bEntities){` |
|     29 | 6133 | `	if( iDoc == PH7_ENT_DOC_HTML401 \|\| (bEntities && iDoc == PH7_ENT_DOC_XHTML) ){` |
|     21 | 6134 | `		return "&#039;";` |
|      - | 6135 | `	}` |
|      9 | 6136 | `	return "&apos;";` |
|     15 | 6137 | `}` |
|      - | 6138 | `/* Try to parse one HTML entity at z (z[0]=='&', z < zEnd). bFull selects the` |
|      - | 6139 | ` * html_entity_decode set (doctype named table + any allowed numeric ref) vs` |
|      - | 6140 | ` * the htmlspecialchars_decode set (the basic specials + quote numerics only).` |
|      - | 6141 | ` * Named matching is case-SENSITIVE and the ';' is required (both PHP-exact);` |
|      - | 6142 | ` * numeric refs accept dec/hex (x or X) with any number of leading zeros but` |
|      - | 6143 | ` * reject out-of-range, surrogate and doctype-disallowed codepoints (the` |
|      - | 6144 | ` * caller then leaves the source verbatim). Quote-flag gating is NOT applied` |
|      - | 6145 | ` * here — the same routine doubles as the "is this a valid entity" test for` |
|      - | 6146 | ` * double_encode=false, which ignores the quote bits (oracle-pinned).` |
|      - | 6147 | ` * bEncodeCheck selects the looser HtmlNumericAllowed predicate used by that` |
|      - | 6148 | ` * double_encode test; decode callers pass 0 for the HtmlCpAllowed gate.` |
|      - | 6149 | ` * On success sets *pCp / *pnConsumed and returns 1. */` |
|    172 | 6150 | `static int HtmlParseEntity(const unsigned char *z,const unsigned char *zEnd,` |
|      1 | 6151 | `                           int iFlags,int bFull,int bEncodeCheck,sxu32 *pCp,int *pnConsumed){` |
|    173 | 6152 | `	int nAvail = (int)(zEnd - z);` |
|    173 | 6153 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6154 | `	sxu32 n;` |
|    173 | 6155 | `	if( nAvail < 4 ){ return 0; } /* shortest entities: &lt; &#9; */` |
|    169 | 6156 | `	if( z[1] == '#' ){` |
|      - | 6157 | `		/* Numeric reference */` |
|     89 | 6158 | `		sxu32 cp = 0;` |
|     89 | 6159 | `		int i = 2, bHex = 0, nDig = 0;` |
|     89 | 6160 | `		if( z[i]=='x' \|\| z[i]=='X' ){ bHex = 1; i++; }` |
|    317 | 6161 | `		for( ; i < nAvail && z[i] != ';' ; i++ ){` |
|      - | 6162 | `			int v;` |
|    221 | 6163 | `			unsigned char c = z[i];` |
|    221 | 6164 | `			if( c>='0' && c<='9' ){ v = c - '0'; }` |
|     17 | 6165 | `			else if( bHex && c>='a' && c<='f' ){ v = c - 'a' + 10; }` |
|     17 | 6166 | `			else if( bHex && c>='A' && c<='F' ){ v = c - 'A' + 10; }` |
|    ! 0 | 6167 | `			else { return 0; }` |
|      - | 6168 | `			/* Stop accumulating once out of range (keeps validating the shape;` |
|      - | 6169 | `			 * max intermediate is 0x10FFFF*16+15, no sxu32 overflow). */` |
|    221 | 6170 | `			if( cp <= 0x10FFFF ){ cp = cp * (bHex ? 16 : 10) + (sxu32)v; }` |
|    221 | 6171 | `			nDig++;` |
|    111 | 6172 | `		}` |
|     97 | 6173 | `		if( nDig == 0 \|\| i >= nAvail ){ return 0; } /* no digits / no ';' */` |
|     97 | 6174 | `		if( bEncodeCheck ? !HtmlNumericAllowed(cp,iFlags) : !HtmlCpAllowed(cp,iFlags) ){ return 0; }` |
|     83 | 6175 | `		if( !bFull ){` |
|      - | 6176 | `			/* hsc_decode: numeric refs to the five specials only. */` |
|     99 | 6177 | `			for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) && aHtmlSpecEnt[n].cp != cp ; n++ ){}` |
|     25 | 6178 | `			if( n >= SX_ARRAYSIZE(aHtmlSpecEnt) ){ return 0; }` |
|     11 | 6179 | `		}` |
|     75 | 6180 | `		*pCp = cp;` |
|     75 | 6181 | `		*pnConsumed = i + 1;` |
|     75 | 6182 | `		return 1;` |
|      - | 6183 | `	}` |
|      - | 6184 | `	/* Named reference — every entity name starts with a letter, so anything` |
|      - | 6185 | `	 * else can bail out before touching the tables. */` |
|     81 | 6186 | `	if( !((z[1]>='a' && z[1]<='z') \|\| (z[1]>='A' && z[1]<='Z')) ){ return 0; }` |
|    287 | 6187 | `	for( n = 0 ; n < SX_ARRAYSIZE(aHtmlSpecEnt) ; n++ ){` |
|    265 | 6188 | `		if( aHtmlSpecEnt[n].cp == 39 && iDoc == PH7_ENT_DOC_HTML401 ){ continue; }` |
|    243 | 6189 | `		if( nAvail >= aHtmlSpecEnt[n].n && SyMemcmp(z,aHtmlSpecEnt[n].zEnt,(sxu32)aHtmlSpecEnt[n].n) == 0 ){` |
|     53 | 6190 | `			*pCp = aHtmlSpecEnt[n].cp;` |
|     53 | 6191 | `			*pnConsumed = aHtmlSpecEnt[n].n;` |
|     53 | 6192 | `			return 1;` |
|      - | 6193 | `		}` |
|     96 | 6194 | `	}` |
|     23 | 6195 | `	if( bFull && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 6196 | `		/* Linear scan of the 248-row table: runs only at '&'-then-letter` |
|      - | 6197 | `		 * positions and guarantees the decode set can never drift from the` |
|      - | 6198 | `		 * encode table. The first-letter guard skips the SyStrlen/SyMemcmp` |
|      - | 6199 | `		 * for ~96% of rows. */` |
|   3369 | 6200 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|      - | 6201 | `			sxu32 nEnt;` |
|   3357 | 6202 | `			if( z[1] != (unsigned char)aHtml401Ent[n].zEnt[1] ){ continue; }` |
|    121 | 6203 | `			nEnt = SyStrlen(aHtml401Ent[n].zEnt);` |
|    121 | 6204 | `			if( (sxu32)nAvail >= nEnt && SyMemcmp(z,aHtml401Ent[n].zEnt,nEnt) == 0 ){` |
|      7 | 6205 | `				*pCp = aHtml401Ent[n].cp;` |
|      7 | 6206 | `				*pnConsumed = (int)nEnt;` |
|      7 | 6207 | `				return 1;` |
|      - | 6208 | `			}` |
|     58 | 6209 | `		}` |
|      6 | 6210 | `	}` |
|     17 | 6211 | `	return 0;` |
|     88 | 6212 | `}` |
|      - | 6213 | `/* Shared encoder for htmlspecialchars (bAll=0) and htmlentities (bAll=1).` |
|      - | 6214 | ` * Invalid UTF-8 policy: ENT_IGNORE drops the byte (and wins over SUBSTITUTE),` |
|      - | 6215 | ` * ENT_SUBSTITUTE emits one U+FFFD per invalid byte, neither -> the whole` |
|      - | 6216 | ` * result is "" (pre-validated in a first pass: the accumulating result API` |
|      - | 6217 | ` * cannot roll back — same reason FvSanitizeFull is two-pass). */` |
|     94 | 6218 | `static void HtmlEscape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 6219 | `                       int iFlags,int bAll,int bDoubleEncode){` |
|     95 | 6220 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     95 | 6221 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|      - | 6222 | `	const unsigned char *runStart;` |
|     95 | 6223 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6224 | `	sxu32 cp;` |
|     95 | 6225 | `	if( (iFlags & (PH7_ENT_IGNORE\|PH7_ENT_SUBSTITUTE)) == 0 ){` |
|      - | 6226 | `		/* Pass 1: any malformed sequence rejects the entire input. ASCII` |
|      - | 6227 | `		 * bytes cannot be malformed, so skip them without the decoder. */` |
|    381 | 6228 | `		while( p < zEnd ){` |
|      - | 6229 | `			int len;` |
|    323 | 6230 | `			if( *p < 0x80 ){ p++; continue; }` |
|     37 | 6231 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     37 | 6232 | `			if( len == 0 ){ ph7_result_string(pCtx,"",0); return; }` |
|     27 | 6233 | `			p += len;` |
|      1 | 6234 | `		}` |
|     59 | 6235 | `		p = (const unsigned char *)zIn;` |
|     29 | 6236 | `	}` |
|     85 | 6237 | `	runStart = p;` |
|     85 | 6238 | `	ph7_result_string(pCtx,"",0);` |
|    455 | 6239 | `	while( p < zEnd ){` |
|    371 | 6240 | `		const char *zEnt = 0;` |
|      - | 6241 | `		int len;` |
|    371 | 6242 | `		if( *p < 0x80 ){` |
|    307 | 6243 | `			len = 1;` |
|    307 | 6244 | `			switch( *p ){` |
|     25 | 6245 | `			case '<': zEnt = "&lt;"; break;` |
|     25 | 6246 | `			case '>': zEnt = "&gt;"; break;` |
|     18 | 6247 | `			case '&':` |
|     37 | 6248 | `				zEnt = "&amp;";` |
|     37 | 6249 | `				if( !bDoubleEncode ){` |
|      - | 6250 | `					sxu32 eCp; int nEat;` |
|     25 | 6251 | `					if( HtmlParseEntity(p,zEnd,iFlags,1,1,&eCp,&nEat) ){` |
|      - | 6252 | `						/* A valid existing entity: keep it verbatim. */` |
|     13 | 6253 | `						zEnt = 0;` |
|     13 | 6254 | `						len = nEat;` |
|      6 | 6255 | `					}` |
|     12 | 6256 | `				}` |
|     37 | 6257 | `				break;` |
|     10 | 6258 | `			case '"':` |
|     21 | 6259 | `				if( iFlags & PH7_ENT_QUOTE_DOUBLE ){ zEnt = "&quot;"; }` |
|     21 | 6260 | `				break;` |
|     12 | 6261 | `			case '\'':` |
|     25 | 6262 | `				if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|     23 | 6263 | `					zEnt = HtmlAposEntity(iDoc,bAll);` |
|     11 | 6264 | `				}` |
|     25 | 6265 | `				break;` |
|     89 | 6266 | `			default:` |
|    179 | 6267 | `				if( (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode((sxu32)*p,iFlags) ){` |
|    ! 0 | 6268 | `					zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 6269 | `				}` |
|    178 | 6270 | `				break;` |
|      - | 6271 | `			}` |
|    154 | 6272 | `		}else{` |
|     65 | 6273 | `			len = FvUtf8Next(p,zEnd,&cp);` |
|     65 | 6274 | `			if( len == 0 ){` |
|      - | 6275 | `				/* Malformed subpart (IGNORE or SUBSTITUTE is set, else pass 1` |
|      - | 6276 | `				 * would have rejected): drop it or emit ONE U+FFFD for the` |
|      - | 6277 | `				 * whole unit (php substitutes per maximal invalid subpart). */` |
|     15 | 6278 | `				if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|     15 | 6279 | `				if( (iFlags & PH7_ENT_IGNORE) == 0 ){ ph7_result_string(pCtx,"\xEF\xBF\xBD",3); }` |
|     15 | 6280 | `				p += HtmlUtf8FailAdvance(p,zEnd);` |
|     15 | 6281 | `				runStart = p;` |
|     15 | 6282 | `				continue;` |
|      - | 6283 | `			}` |
|     51 | 6284 | `			if( bAll && HtmlDocHasNamedTable(iDoc) ){` |
|     41 | 6285 | `				zEnt = FvHtml401Lookup(cp);` |
|     20 | 6286 | `			}` |
|     51 | 6287 | `			if( zEnt == 0 && (iFlags & PH7_ENT_DISALLOWED) && !HtmlCpAllowedEncode(cp,iFlags) ){` |
|    ! 0 | 6288 | `				zEnt = "\xEF\xBF\xBD";` |
|    ! 0 | 6289 | `			}` |
|      - | 6290 | `		}` |
|    357 | 6291 | `		if( zEnt ){` |
|    135 | 6292 | `			if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|    135 | 6293 | `			ph7_result_string(pCtx,zEnt,-1);` |
|    135 | 6294 | `			runStart = p + len;` |
|     67 | 6295 | `		}` |
|    357 | 6296 | `		p += len;` |
|      1 | 6297 | `	}` |
|     85 | 6298 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     48 | 6299 | `}` |
|      - | 6300 | `/* Shared decoder for html_entity_decode (bFull=1) and htmlspecialchars_decode` |
|      - | 6301 | ` * (bFull=0). Quote refs (cp 34/39, named or numeric) are gated by the quote` |
|      - | 6302 | ` * bits and left verbatim when suppressed; an invalid entity leaves its '&'` |
|      - | 6303 | ` * verbatim and rescans right after it, which also yields PHP's no-double-` |
|      - | 6304 | ` * decode behavior ("&amp;lt;" -> "&lt;"). */` |
|     82 | 6305 | `static void HtmlUnescape(ph7_context *pCtx,const char *zIn,int nIn,` |
|      1 | 6306 | `                         int iFlags,int bFull){` |
|     83 | 6307 | `	const unsigned char *zEnd = (const unsigned char *)(zIn + nIn);` |
|     83 | 6308 | `	const unsigned char *p = (const unsigned char *)zIn;` |
|     83 | 6309 | `	const unsigned char *runStart = p;` |
|     83 | 6310 | `	ph7_result_string(pCtx,"",0);` |
|    557 | 6311 | `	while( p < zEnd ){` |
|      - | 6312 | `		sxu32 cp;` |
|      - | 6313 | `		int nEat;` |
|    510 | 6314 | `		if( *p != '&' ){ p++; continue; }` |
|    155 | 6315 | `		if( !HtmlParseEntity(p,zEnd,iFlags,bFull,0,&cp,&nEat) ){ p++; continue; }` |
|    124 | 6316 | `		if( (cp == 34 && (iFlags & PH7_ENT_QUOTE_DOUBLE) == 0)` |
|    117 | 6317 | `		 \|\| (cp == 39 && (iFlags & PH7_ENT_QUOTE_SINGLE) == 0) ){` |
|      - | 6318 | `			/* Suppressed quote: leave the entity source verbatim. */` |
|     37 | 6319 | `			p += nEat;` |
|     37 | 6320 | `			continue;` |
|      - | 6321 | `		}` |
|     89 | 6322 | `		if( p > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(p-runStart)); }` |
|      - | 6323 | `		{` |
|      - | 6324 | `			char zBuf[4];` |
|     89 | 6325 | `			int n = HtmlCpUtf8(cp,zBuf);` |
|     89 | 6326 | `			ph7_result_string(pCtx,zBuf,n);` |
|      - | 6327 | `		}` |
|     89 | 6328 | `		p += nEat;` |
|     89 | 6329 | `		runStart = p;` |
|      1 | 6330 | `	}` |
|     79 | 6331 | `	if( zEnd > runStart ){ ph7_result_string(pCtx,(const char *)runStart,(int)(zEnd-runStart)); }` |
|     79 | 6332 | `}` |
|      - | 6333 | `/* Validate the optional charset argument at apArg[idx]: UTF-8 aliases (and` |
|      - | 6334 | ` * ""/NULL meaning the default) are accepted; anything else — including` |
|      - | 6335 | ` * php-supported single-byte charsets like ISO-8859-1, PHL is UTF-8-only per` |
|      - | 6336 | ` * PLAN.md §6 — raises PHP's unsupported-charset warning and is treated as` |
|      - | 6337 | ` * UTF-8 (ph7_context_throw_error_format prepends the function name). */` |
|    141 | 6338 | `static void HtmlCheckCharset(ph7_context *pCtx,int nArg,ph7_value **apArg,int idx){` |
|      - | 6339 | `	const char *zCs;` |
|      - | 6340 | `	int nCs;` |
|    148 | 6341 | `	if( nArg <= idx \|\| ph7_value_is_null(apArg[idx]) ){ return; }` |
|     15 | 6342 | `	zCs = ph7_value_to_string(apArg[idx],&nCs);` |
|     15 | 6343 | `	if( nCs == 0 ){ return; } /* "" selects the default charset (UTF-8) */` |
|     13 | 6344 | `	if( nCs == 5 && SyStrnicmp(zCs,"UTF-8",5) == 0 ){` |
|     13 | 6345 | `		return; /* php accepts only "UTF-8" (any case) silently — "UTF8" warns */` |
|      - | 6346 | `	}` |
|    ! 0 | 6347 | `	ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6348 | `		"Charset \"%.*s\" is not supported, assuming UTF-8",nCs,zCs);` |
|     71 | 6349 | `}` |
|      - | 6350 | `/* get_html_translation_table() worker: character (UTF-8 bytes) => entity.` |
|      - | 6351 | ` * The five specials come first in byte order, then — for HTML_ENTITIES with a` |
|      - | 6352 | ` * named-table doctype — the 248 aHtml401Ent rows ascending (oracle-pinned` |
|      - | 6353 | ` * ordering; 253 entries under the defaults). */` |
|    549 | 6354 | `static void HtmlTableAdd(ph7_value *pArray,ph7_value *pValue,const char *zKey,const char *zEnt){` |
|    549 | 6355 | `	ph7_value_string(pValue,zEnt,-1);` |
|    549 | 6356 | `	ph7_array_add_strkey_elem(pArray,zKey,pValue);` |
|    549 | 6357 | `	ph7_value_reset_string_cursor(pValue);` |
|    549 | 6358 | `}` |
|     13 | 6359 | `static void HtmlTranslationTable(ph7_context *pCtx,int iTable,int iFlags){` |
|      - | 6360 | `	ph7_value *pArray,*pValue;` |
|     13 | 6361 | `	int iDoc = iFlags & PH7_ENT_DOC_MASK;` |
|      - | 6362 | `	sxu32 n;` |
|     13 | 6363 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 6364 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 6365 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|    ! 0 | 6366 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6367 | `		return;` |
|      - | 6368 | `	}` |
|     13 | 6369 | `	if( iFlags & PH7_ENT_QUOTE_DOUBLE ){` |
|     11 | 6370 | `		HtmlTableAdd(pArray,pValue,"\"","&quot;");` |
|      5 | 6371 | `	}` |
|     13 | 6372 | `	HtmlTableAdd(pArray,pValue,"&","&amp;");` |
|     13 | 6373 | `	if( iFlags & PH7_ENT_QUOTE_SINGLE ){` |
|      - | 6374 | `		/* The apostrophe row mirrors the function each table belongs to:` |
|      - | 6375 | `		 * SPECIALCHARS follows htmlspecialchars, ENTITIES follows` |
|      - | 6376 | `		 * htmlentities (oracle-pinned at flags 35). */` |
|      7 | 6377 | `		HtmlTableAdd(pArray,pValue,"'",HtmlAposEntity(iDoc,iTable != 0));` |
|      3 | 6378 | `	}` |
|     13 | 6379 | `	HtmlTableAdd(pArray,pValue,"<","&lt;");` |
|     13 | 6380 | `	HtmlTableAdd(pArray,pValue,">","&gt;");` |
|     13 | 6381 | `	if( iTable != 0 /*php: any non-HTML_SPECIALCHARS table => entities*/ && HtmlDocHasNamedTable(iDoc) ){` |
|      - | 6382 | `		char zKey[8];` |
|    499 | 6383 | `		for( n = 0 ; n < SX_ARRAYSIZE(aHtml401Ent) ; n++ ){` |
|    497 | 6384 | `			int nK = HtmlCpUtf8(aHtml401Ent[n].cp,zKey);` |
|    497 | 6385 | `			zKey[nK] = 0;` |
|    497 | 6386 | `			HtmlTableAdd(pArray,pValue,zKey,aHtml401Ent[n].zEnt);` |
|    249 | 6387 | `		}` |
|      1 | 6388 | `	}` |
|     13 | 6389 | `	ph7_result_value(pCtx,pArray);` |
|      7 | 6390 | `}` |
|     25 | 6391 | `static int FvEmailAllowed(unsigned char c){` |
|     25 | 6392 | `	if( (c>='a'&&c<='z')\|\|(c>='A'&&c<='Z')\|\|(c>='0'&&c<='9') ){ return 1; }` |
|     16 | 6393 | `	return c=='!'\|\|c=='#'\|\|c=='$'\|\|c=='%'\|\|c=='&'\|\|c=='\''\|\|c=='*'\|\|c=='+'` |
|     10 | 6394 | ``	    \|\| c=='-'\|\|c=='='\|\|c=='?'\|\|c=='^'\|\|c=='_'\|\|c=='`'\|\|c=='{'\|\|c=='\|'`` |
|     15 | 6395 | `	    \|\| c=='}'\|\|c=='~'\|\|c=='@'\|\|c=='.'\|\|c=='['\|\|c==']';` |
|     13 | 6396 | `}` |
|     23 | 6397 | `static int FvUrlAllowed(unsigned char c){` |
|     23 | 6398 | `	return c>=33 && c<=126; /* PHP keeps every printable ASCII byte except space */` |
|      1 | 6399 | `}` |
|      - | 6400 | `/* SANITIZE_EMAIL (isUrl=0) / SANITIZE_URL (isUrl=1): strip disallowed bytes. */` |
|      5 | 6401 | `static void FvSanitizeChars(ph7_context *pCtx,const char *z,int n,int isUrl){` |
|      5 | 6402 | `	int i, runStart = 0;` |
|      5 | 6403 | `	ph7_result_string(pCtx,"",0);` |
|     51 | 6404 | `	for( i=0; i<n; i++ ){` |
|     47 | 6405 | `		unsigned char c = (unsigned char)z[i];` |
|     47 | 6406 | `		if( !(isUrl ? FvUrlAllowed(c) : FvEmailAllowed(c)) ){` |
|     11 | 6407 | `			if( i>runStart ){ ph7_result_string(pCtx,z+runStart,i-runStart); }` |
|     11 | 6408 | `			runStart = i+1;` |
|      5 | 6409 | `		}` |
|     24 | 6410 | `	}` |
|      5 | 6411 | `	if( n>runStart ){ ph7_result_string(pCtx,z+runStart,n-runStart); }` |
|      5 | 6412 | `}` |
|      - | 6413 | `/*` |
|      - | 6414 | ` * Apply the selected filter to one already-resolved input value and write the` |
|      - | 6415 | ` * result into pCtx. Shared by filter_var() and filter_input(): the caller has` |
|      - | 6416 | ` * already parsed $filter/$flags/$options. On validation failure the 'default'` |
|      - | 6417 | ` * option (if any) is returned, else null when FILTER_NULL_ON_FAILURE is set,` |
|      - | 6418 | ` * else false. A validating filter that passes returns the (string) input` |
|      - | 6419 | ` * unchanged; a sanitizer writes its transformed output directly.` |
|      - | 6420 | ` */` |
|    316 | 6421 | `static int FvApplyFilter(ph7_context *pCtx,ph7_value *pInput,` |
|      - | 6422 | `                         int iFilter,int iFlags,ph7_value *pOpts,` |
|      - | 6423 | `                         ph7_value *pDefault)` |
|      3 | 6424 | `{` |
|    319 | 6425 | `	int bNull = (iFlags & FV_NULL_ON_FAILURE) ? 1 : 0;` |
|      - | 6426 | `	const char *zVal; int nVal;` |
|      - | 6427 | `	/* An array/object input fails every scalar filter. */` |
|    319 | 6428 | `	if( ph7_value_is_array(pInput) ){ goto fail; }` |
|    317 | 6429 | `	zVal = ph7_value_to_string(pInput,&nVal);` |
|    317 | 6430 | `	switch( iFilter ){` |
|     28 | 6431 | `	case FV_VALIDATE_INT: {` |
|      - | 6432 | `		ph7_int64 v;` |
|     58 | 6433 | `		if( !FvValidateInt(zVal,nVal,iFlags,&v) ){ goto fail; }` |
|     31 | 6434 | `		if( pOpts ){` |
|      7 | 6435 | `			ph7_value *pMin = ph7_array_fetch(pOpts,"min_range",(int)sizeof("min_range")-1);` |
|      7 | 6436 | `			ph7_value *pMax = ph7_array_fetch(pOpts,"max_range",(int)sizeof("max_range")-1);` |
|      7 | 6437 | `			if( pMin && v<ph7_value_to_int64(pMin) ){ goto fail; }` |
|      7 | 6438 | `			if( pMax && v>ph7_value_to_int64(pMax) ){ goto fail; }` |
|      2 | 6439 | `		}` |
|     29 | 6440 | `		ph7_result_int64(pCtx,v);` |
|     29 | 6441 | `		return PH7_OK;` |
|      - | 6442 | `	}` |
|     34 | 6443 | `	case FV_VALIDATE_FLOAT: {` |
|      - | 6444 | `		double d;` |
|     69 | 6445 | `		if( !FvValidateFloat(zVal,nVal,iFlags,&d) ){ goto fail; }` |
|     39 | 6446 | `		ph7_result_double(pCtx,d);` |
|     39 | 6447 | `		return PH7_OK;` |
|      - | 6448 | `	}` |
|     14 | 6449 | `	case FV_VALIDATE_BOOLEAN: {` |
|      - | 6450 | `		int b;` |
|     29 | 6451 | `		if( !FvValidateBool(zVal,nVal,&b) ){ goto fail; }` |
|     21 | 6452 | `		ph7_result_bool(pCtx,b);` |
|     21 | 6453 | `		return PH7_OK;` |
|      - | 6454 | `	}` |
|     25 | 6455 | `	case FV_VALIDATE_IP:     if( !FvValidateIp(zVal,nVal,iFlags) ){ goto fail; } goto pass;` |
|     11 | 6456 | `	case FV_VALIDATE_MAC:    if( !FvValidateMac(zVal,nVal) ){ goto fail; }       goto pass;` |
|     28 | 6457 | `	case FV_VALIDATE_EMAIL:  if( !FvValidateEmail(zVal,nVal) ){ goto fail; }     goto pass;` |
|     11 | 6458 | `	case FV_VALIDATE_DOMAIN: if( !FvValidateDomain(zVal,nVal) ){ goto fail; }    goto pass;` |
|     15 | 6459 | `	case FV_VALIDATE_URL:    if( !FvValidateUrl(zVal,nVal) ){ goto fail; }       goto pass;` |
|      3 | 6460 | `	case FV_VALIDATE_REGEXP: {` |
|      - | 6461 | `#ifdef PH7_ENABLE_PCRE` |
|      8 | 6462 | `		ph7_value *pRe = pOpts ? ph7_array_fetch(pOpts,"regexp",(int)sizeof("regexp")-1) : 0;` |
|      8 | 6463 | `		const char *zRe; int nRe, matched = 0;` |
|      8 | 6464 | `		if( pRe==0 ){` |
|      3 | 6465 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6466 | `				"filter_var(): \"regexp\" option is missing");` |
|      - | 6467 | `		}` |
|      5 | 6468 | `		zRe = ph7_value_to_string(pRe,&nRe);` |
|      5 | 6469 | `		if( PH7_PcreMatchQuiet(pCtx,zRe,nRe,zVal,nVal,&matched)!=SXRET_OK \|\| !matched ){ goto fail; }` |
|      3 | 6470 | `		goto pass;` |
|      - | 6471 | `#else` |
|      - | 6472 | `		goto fail;` |
|      - | 6473 | `#endif` |
|      - | 6474 | `	}` |
|      3 | 6475 | `	case FV_SANITIZE_NUMBER_INT:   FvSanitizeNumber(pCtx,zVal,nVal,0,0);      return PH7_OK;` |
|      5 | 6476 | `	case FV_SANITIZE_NUMBER_FLOAT: FvSanitizeNumber(pCtx,zVal,nVal,1,iFlags); return PH7_OK;` |
|     13 | 6477 | `	case FV_SANITIZE_SPECIAL_CHARS:      FvSanitizeSpecial(pCtx,zVal,nVal,iFlags); return PH7_OK;` |
|     25 | 6478 | `	case FV_SANITIZE_FULL_SPECIAL_CHARS: FvSanitizeFull(pCtx,zVal,nVal,iFlags);    return PH7_OK;` |
|      3 | 6479 | `	case FV_SANITIZE_EMAIL: FvSanitizeChars(pCtx,zVal,nVal,0); return PH7_OK;` |
|      3 | 6480 | `	case FV_SANITIZE_URL:   FvSanitizeChars(pCtx,zVal,nVal,1); return PH7_OK;` |
|     13 | 6481 | `	case FV_DEFAULT:` |
|      - | 6482 | `		/* FILTER_UNSAFE_RAW / FILTER_DEFAULT: pass through unchanged unless a` |
|      - | 6483 | `		 * STRIP/ENCODE flag is set, in which case apply the string filter. */` |
|     28 | 6484 | `		if( iFlags & FV_FLAG_STRING_MASK ){` |
|     15 | 6485 | `			FvSanitizeString(pCtx,zVal,nVal,iFlags);` |
|     15 | 6486 | `			return PH7_OK;` |
|      - | 6487 | `		}` |
|     14 | 6488 | `		goto pass;` |
|    ! 0 | 6489 | `	default:` |
|    ! 0 | 6490 | `		ph7_context_throw_error_format(pCtx,PH7_CTX_WARNING,` |
|    ! 0 | 6491 | `			"Unknown filter with ID %d",iFilter);` |
|    ! 0 | 6492 | `		break; /* unknown filter id -> fail */` |
|    ! 0 | 6493 | `	}` |
|     58 | 6494 | `fail:` |
|    118 | 6495 | `	if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|    114 | 6496 | `	else if( bNull ){ ph7_result_null(pCtx); }` |
|    108 | 6497 | `	else { ph7_result_bool(pCtx,0); }` |
|    118 | 6498 | `	return PH7_OK;` |
|     26 | 6499 | `pass: /* validation passed: return the (string) input unchanged */` |
|     54 | 6500 | `	ph7_result_string(pCtx,zVal,nVal);` |
|     54 | 6501 | `	return PH7_OK;` |
|    161 | 6502 | `}` |
|      - | 6503 | `/*` |
|      - | 6504 | ` * Parse the ($filter, $options) pair shared by filter_var()/filter_input() out` |
|      - | 6505 | ` * of apArg[iBase] ($filter) and apArg[iBase+1] ($options): $options is either a` |
|      - | 6506 | ` * plain flags int, or an array with 'flags' and an 'options' sub-array (whose` |
|      - | 6507 | ` * 'default' entry is the fallback value). Fills the four output pointers;` |
|      - | 6508 | ` * unset outputs keep the caller-provided defaults.` |
|      - | 6509 | ` */` |
|    328 | 6510 | `static void FvParseFilterArgs(int nArg,ph7_value **apArg,int iBase,` |
|      - | 6511 | `                              int *piFilter,int *piFlags,` |
|      - | 6512 | `                              ph7_value **ppOpts,ph7_value **ppDefault)` |
|      3 | 6513 | `{` |
|    331 | 6514 | `	if( nArg>iBase ){ *piFilter = ph7_value_to_int(apArg[iBase]); }` |
|    331 | 6515 | `	if( nArg>iBase+1 ){` |
|     88 | 6516 | `		if( ph7_value_is_array(apArg[iBase+1]) ){` |
|     42 | 6517 | `			ph7_value *pF = ph7_array_fetch(apArg[iBase+1],"flags",(int)sizeof("flags")-1);` |
|     42 | 6518 | `			if( pF ){ *piFlags = ph7_value_to_int(pF); }` |
|     42 | 6519 | `			*ppOpts = ph7_array_fetch(apArg[iBase+1],"options",(int)sizeof("options")-1);` |
|     42 | 6520 | `			if( *ppOpts && !ph7_value_is_array(*ppOpts) ){ *ppOpts = 0; }` |
|     42 | 6521 | `			if( *ppOpts ){ *ppDefault = ph7_array_fetch(*ppOpts,"default",(int)sizeof("default")-1); }` |
|     22 | 6522 | `		}else{` |
|     48 | 6523 | `			*piFlags = ph7_value_to_int(apArg[iBase+1]);` |
|      - | 6524 | `		}` |
|     43 | 6525 | `	}` |
|    331 | 6526 | `}` |
|      - | 6527 | `/*` |
|      - | 6528 | ` * filter_var($value, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 6529 | ` *  Validate or sanitize a value; see FvApplyFilter for the failure semantics.` |
|      - | 6530 | ` */` |
|    306 | 6531 | `static int PH7_builtin_filter_var(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6532 | `{` |
|    308 | 6533 | `	int iFilter = FV_DEFAULT, iFlags = 0;` |
|    308 | 6534 | `	ph7_value *pOpts = 0, *pDefault = 0;` |
|    308 | 6535 | `	if( nArg<1 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    308 | 6536 | `	FvParseFilterArgs(nArg,apArg,1,&iFilter,&iFlags,&pOpts,&pDefault);` |
|    308 | 6537 | `	return FvApplyFilter(pCtx,apArg[0],iFilter,iFlags,pOpts,pDefault);` |
|    155 | 6538 | `}` |
|      - | 6539 | `/*` |
|      - | 6540 | ` * filter_input($type, $var_name, $filter = FILTER_DEFAULT, $options = 0)` |
|      - | 6541 | ` *  Look up $var_name in the requested INPUT_* superglobal, then apply the` |
|      - | 6542 | ` *  filter. Semantics verified byte-for-byte against php 8.5:` |
|      - | 6543 | ` *   - variable NOT set: 'default' option wins, else false when` |
|      - | 6544 | ` *     FILTER_NULL_ON_FAILURE is set, else null. (Note the null/false roles are` |
|      - | 6545 | ` *     INVERTED relative to a present value that fails validation, which yields` |
|      - | 6546 | ` *     default > null-if-NULL_ON_FAILURE > false via FvApplyFilter.)` |
|      - | 6547 | ` *   - variable present: delegate to FvApplyFilter.` |
|      - | 6548 | ` *  Divergence: php reads a SAPI snapshot of the original request variables` |
|      - | 6549 | ` *  captured at startup; PHL reads the live superglobal. In CLI they match for` |
|      - | 6550 | ` *  the SAPI-registered keys (SCRIPT_NAME/PHP_SELF/DOCUMENT_ROOT); keys added` |
|      - | 6551 | ` *  only to the live $_SERVER (REQUEST_TIME/PWD/…) are visible here but not in` |
|      - | 6552 | ` *  php's snapshot.` |
|      - | 6553 | ` */` |
|     28 | 6554 | `static int PH7_builtin_filter_input(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 6555 | `{` |
|     30 | 6556 | `	int iType, iFilter = FV_DEFAULT, iFlags = 0;` |
|     30 | 6557 | `	ph7_value *pOpts = 0, *pDefault = 0, *pSuper, *pElem;` |
|      - | 6558 | `	const char *zVar, *zSuper; int nVar; sxu32 nSuper;` |
|     30 | 6559 | `	if( nArg<2 ){` |
|      7 | 6560 | `		return PH7_VmThrowException(pCtx,"ArgumentCountError",` |
|      2 | 6561 | `			"filter_input() expects at least 2 arguments, %d given",nArg);` |
|      - | 6562 | `	}` |
|     26 | 6563 | `	iType = ph7_value_to_int(apArg[0]);` |
|     26 | 6564 | `	switch( iType ){` |
|      3 | 6565 | `	case 0: zSuper = "_POST";   nSuper = (sxu32)sizeof("_POST")-1;   break; /* INPUT_POST */` |
|      3 | 6566 | `	case 1: zSuper = "_GET";    nSuper = (sxu32)sizeof("_GET")-1;    break; /* INPUT_GET */` |
|    ! 0 | 6567 | `	case 2: zSuper = "_COOKIE"; nSuper = (sxu32)sizeof("_COOKIE")-1; break; /* INPUT_COOKIE */` |
|    ! 0 | 6568 | `	case 4: zSuper = "_ENV";    nSuper = (sxu32)sizeof("_ENV")-1;    break; /* INPUT_ENV */` |
|     19 | 6569 | `	case 5: zSuper = "_SERVER"; nSuper = (sxu32)sizeof("_SERVER")-1; break; /* INPUT_SERVER */` |
|      1 | 6570 | `	default:` |
|      3 | 6571 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 6572 | `			"filter_input(): Argument #1 ($type) must be an INPUT_* constant");` |
|      - | 6573 | `	}` |
|     23 | 6574 | `	zVar = ph7_value_to_string(apArg[1],&nVar);` |
|     23 | 6575 | `	FvParseFilterArgs(nArg,apArg,2,&iFilter,&iFlags,&pOpts,&pDefault);` |
|      - | 6576 | `	/* Resolve the variable from the superglobal (missing/non-array -> not set). */` |
|     23 | 6577 | `	pSuper = PH7_VmExtractSuper(pCtx->pVm,zSuper,nSuper);` |
|     23 | 6578 | `	pElem = (pSuper && ph7_value_is_array(pSuper))` |
|     33 | 6579 | `		? ph7_array_fetch(pSuper,zVar,nVar) : 0;` |
|     23 | 6580 | `	if( pElem==0 ){` |
|      - | 6581 | `		/* Variable not set: default > false(if NULL_ON_FAILURE) > null. Note the` |
|      - | 6582 | `		 * false/null roles are inverted vs FvApplyFilter's present-but-fails path. */` |
|     13 | 6583 | `		if( pDefault ){ ph7_result_value(pCtx,pDefault); }` |
|      9 | 6584 | `		else if( iFlags & FV_NULL_ON_FAILURE ){ ph7_result_bool(pCtx,0); }` |
|      7 | 6585 | `		else { ph7_result_null(pCtx); }` |
|     13 | 6586 | `		return PH7_OK;` |
|      - | 6587 | `	}` |
|     11 | 6588 | `	return FvApplyFilter(pCtx,pElem,iFilter,iFlags,pOpts,pDefault);` |
|     16 | 6589 | `}` |
|      - | 6590 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 6591 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 6592 | `/*` |
|      - | 6593 | ` * Parse a CSV string and invoke the supplied callback for each processed xhunk.` |
|      - | 6594 |  |
|      - | 6595 | ` */` |
|      4 | 6596 | `PH7_PRIVATE sxi32 PH7_ProcessCsv(` |
|      - | 6597 | `	const char *zInput, /* Raw input */` |
|      - | 6598 | `	int nByte,  /* Input length */` |
|      - | 6599 | `	int delim,  /* Delimiter */` |
|      - | 6600 | `	int encl,   /* Enclosure */` |
|      - | 6601 | `	int escape,  /* Escape character */` |
|      - | 6602 | `	sxi32 (*xConsumer)(const char *,int,void *), /* User callback */` |
|      - | 6603 | `	void *pUserData /* Last argument to xConsumer() */` |
|      - | 6604 | `	)` |
|      1 | 6605 | `{` |
|      5 | 6606 | `	const char *zEnd = &zInput[nByte];` |
|      5 | 6607 | `	const char *zIn = zInput;` |
|      - | 6608 | `	const char *zPtr;` |
|      - | 6609 | `	int isEnc;` |
|      - | 6610 | `	/* Start processing */` |
|      8 | 6611 | `	for(;;){` |
|     17 | 6612 | `		if( zIn >= zEnd ){` |
|      - | 6613 | `			/* No more input to process */` |
|      5 | 6614 | `			break;` |
|      - | 6615 | `		}` |
|     13 | 6616 | `		isEnc = 0;` |
|     13 | 6617 | `		zPtr = zIn;` |
|      - | 6618 | `		/* Find the first delimiter */` |
|     27 | 6619 | `		while( zIn < zEnd ){` |
|     23 | 6620 | `			if( zIn[0] == delim && !isEnc){` |
|      - | 6621 | `				/* Delimiter found,break imediately */` |
|      5 | 6622 | `				break;` |
|     15 | 6623 | `			}else if( zIn[0] == encl ){` |
|      - | 6624 | `				/* Inside enclosure? */` |
|    ! 0 | 6625 | `				isEnc = !isEnc;` |
|     15 | 6626 | `			}else if( zIn[0] == escape ){` |
|      - | 6627 | `				/* Escape sequence */` |
|    ! 0 | 6628 | `				zIn++;` |
|    ! 0 | 6629 | `			}` |
|      - | 6630 | `			/* Advance the cursor */` |
|     15 | 6631 | `			zIn++;` |
|      1 | 6632 | `		}` |
|     13 | 6633 | `		if( zIn > zPtr ){` |
|     13 | 6634 | `			int nByteChunk = (int)(zIn-zPtr);` |
|      - | 6635 | `			sxi32 rc;` |
|      - | 6636 | `			/* Invoke the supllied callback */` |
|     13 | 6637 | `			if( zPtr[0] == encl ){` |
|    ! 0 | 6638 | `				zPtr++;` |
|    ! 0 | 6639 | `				nByteChunk-=2;` |
|    ! 0 | 6640 | `			}` |
|     13 | 6641 | `			if( nByteChunk > 0 ){` |
|     13 | 6642 | `				rc = xConsumer(zPtr,nByteChunk,pUserData);` |
|     13 | 6643 | `				if( rc == SXERR_ABORT ){` |
|      - | 6644 | `					/* User callback request an operation abort */` |
|    ! 0 | 6645 | `					break;` |
|      - | 6646 | `				}` |
|      6 | 6647 | `			}` |
|      6 | 6648 | `		}` |
|      - | 6649 | `		/* Ignore trailing delimiter */` |
|     21 | 6650 | `		while( zIn < zEnd && zIn[0] == delim ){` |
|      9 | 6651 | `			zIn++;` |
|      1 | 6652 | `		}` |
|      1 | 6653 | `	}` |
|      5 | 6654 | `	return SXRET_OK;` |
|      1 | 6655 | `}` |
|      - | 6656 | `/*` |
|      - | 6657 | ` * Default consumer callback for the CSV parsing routine defined above.` |
|      - | 6658 | ` * All the processed input is insereted into an array passed as the last` |
|      - | 6659 | ` * argument to this callback.` |
|      - | 6660 | ` */` |
|     12 | 6661 | `PH7_PRIVATE sxi32 PH7_CsvConsumer(const char *zToken,int nTokenLen,void *pUserData)` |
|      1 | 6662 | `{` |
|     13 | 6663 | `	ph7_value *pArray = (ph7_value *)pUserData;` |
|      - | 6664 | `	ph7_value sEntry;` |
|      - | 6665 | `	SyString sToken;` |
|      - | 6666 | `	/* Insert the token in the given array */` |
|     13 | 6667 | `	SyStringInitFromBuf(&sToken,zToken,nTokenLen);` |
|      - | 6668 | `	/* Remove trailing and leading white spcaces and null bytes */` |
|     27 | 6669 | `	SyStringFullTrimSafe(&sToken);` |
|     13 | 6670 | `	if( sToken.nByte < 1){` |
|    ! 0 | 6671 | `		return SXRET_OK;` |
|      - | 6672 | `	}` |
|     13 | 6673 | `	PH7_MemObjInitFromString(pArray->pVm,&sEntry,&sToken);` |
|     13 | 6674 | `	ph7_array_add_elem(pArray,0,&sEntry);` |
|     13 | 6675 | `	PH7_MemObjRelease(&sEntry);` |
|     13 | 6676 | `	return SXRET_OK;` |
|      7 | 6677 | `}` |
|      - | 6678 | `/*` |
|      - | 6679 | ` * array str_getcsv(string $input[,string $delimiter = ','[,string $enclosure = '"' [,string $escape='\\']]])` |
|      - | 6680 | ` *  Parse a CSV string into an array.` |
|      - | 6681 | ` * Parameters` |
|      - | 6682 | ` *  $input` |
|      - | 6683 | ` *   The string to parse.` |
|      - | 6684 | ` *  $delimiter` |
|      - | 6685 | ` *   Set the field delimiter (one character only).` |
|      - | 6686 | ` *  $enclosure` |
|      - | 6687 | ` *   Set the field enclosure character (one character only).` |
|      - | 6688 | ` *  $escape` |
|      - | 6689 | ` *   Set the escape character (one character only). Defaults as a backslash (\)` |
|      - | 6690 | ` * Return` |
|      - | 6691 | ` *  An indexed array containing the CSV fields or NULL on failure.` |
|      - | 6692 | ` */` |
|      2 | 6693 | `static int PH7_builtin_str_getcsv(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6694 | `{` |
|      - | 6695 | `	const char *zInput,*zPtr;` |
|      - | 6696 | `	ph7_value *pArray;` |
|      3 | 6697 | `	int delim  = ',';   /* Delimiter */` |
|      3 | 6698 | `	int encl   = '"' ;  /* Enclosure */` |
|      3 | 6699 | `	int escape = '\\';  /* Escape character */` |
|      - | 6700 | `	int nLen;` |
|      3 | 6701 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6702 | `		/* Missing/Invalid arguments,return NULL */` |
|    ! 0 | 6703 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6704 | `		return PH7_OK;` |
|      - | 6705 | `	}` |
|      - | 6706 | `	/* Extract the raw input */` |
|      3 | 6707 | `	zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 6708 | `	if( nArg > 1 ){` |
|      - | 6709 | `		int i;` |
|      3 | 6710 | `		if( ph7_value_is_string(apArg[1]) ){` |
|      - | 6711 | `			/* Extract the delimiter */` |
|      3 | 6712 | `			zPtr = ph7_value_to_string(apArg[1],&i);` |
|      3 | 6713 | `			if( i > 0 ){` |
|      3 | 6714 | `				delim = zPtr[0];` |
|      1 | 6715 | `			}` |
|      1 | 6716 | `		}` |
|      3 | 6717 | `		if( nArg > 2 ){` |
|      3 | 6718 | `			if( ph7_value_is_string(apArg[2]) ){` |
|      - | 6719 | `				/* Extract the enclosure */` |
|      3 | 6720 | `				zPtr = ph7_value_to_string(apArg[2],&i);` |
|      3 | 6721 | `				if( i > 0 ){` |
|      3 | 6722 | `					encl = zPtr[0];` |
|      1 | 6723 | `				}` |
|      1 | 6724 | `			}` |
|      3 | 6725 | `			if( nArg > 3 ){` |
|      3 | 6726 | `				if( ph7_value_is_string(apArg[3]) ){` |
|      - | 6727 | `					/* Extract the escape character */` |
|      3 | 6728 | `					zPtr = ph7_value_to_string(apArg[3],&i);` |
|      3 | 6729 | `					if( i > 0 ){` |
|      3 | 6730 | `						escape = zPtr[0];` |
|      1 | 6731 | `					}` |
|      1 | 6732 | `				}` |
|      1 | 6733 | `			}` |
|      1 | 6734 | `		}` |
|      1 | 6735 | `	}` |
|      - | 6736 | `	/* Create our array */` |
|      3 | 6737 | `	pArray = ph7_context_new_array(pCtx);` |
|      3 | 6738 | `	if( pArray == 0 ){` |
|      - | 6739 | `		/* Surface a fatal instead of silently returning null on OOM */` |
|    ! 0 | 6740 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 6741 | `	}` |
|      - | 6742 | `	/* Parse the raw input */` |
|      3 | 6743 | `	PH7_ProcessCsv(zInput,nLen,delim,encl,escape,PH7_CsvConsumer,pArray);` |
|      - | 6744 | `	/* Return the freshly created array */` |
|      3 | 6745 | `	ph7_result_value(pCtx,pArray);` |
|      3 | 6746 | `	return PH7_OK;` |
|      2 | 6747 | `}` |
|      - | 6748 | `/*` |
|      - | 6749 | ` * Extract a tag name from a raw HTML input and insert it in the given` |
|      - | 6750 | ` * container.` |
|      - | 6751 | ` * Refer to [strip_tags()].` |
|      - | 6752 | ` */` |
|     10 | 6753 | `static sxi32 AddTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 6754 | `{` |
|     11 | 6755 | `	const char *zEnd = &zTag[nByte];` |
|      - | 6756 | `	const char *zPtr;` |
|      - | 6757 | `	SyString sEntry;` |
|      - | 6758 | `	/* Strip tags */` |
|     10 | 6759 | `	for(;;){` |
|     45 | 6760 | `		while( zTag < zEnd && (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?'` |
|     14 | 6761 | `			\|\| zTag[0] == '!' \|\| zTag[0] == '-' \|\| ((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     15 | 6762 | `				zTag++;` |
|      1 | 6763 | `		}` |
|     21 | 6764 | `		if( zTag >= zEnd ){` |
|     11 | 6765 | `			break;` |
|      - | 6766 | `		}` |
|     11 | 6767 | `		zPtr = zTag;` |
|      - | 6768 | `		/* Delimit the tag */` |
|     25 | 6769 | `		while(zTag < zEnd ){` |
|     25 | 6770 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 6771 | `				/* UTF-8 stream */` |
|      3 | 6772 | `				zTag++;` |
|      5 | 6773 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     24 | 6774 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     11 | 6775 | `				break;` |
|    ! 0 | 6776 | `			}else{` |
|     13 | 6777 | `				zTag++;` |
|      - | 6778 | `			}` |
|      1 | 6779 | `		}` |
|     11 | 6780 | `		if( zTag > zPtr ){` |
|      - | 6781 | `			/* Perform the insertion */` |
|     11 | 6782 | `			SyStringInitFromBuf(&sEntry,zPtr,(int)(zTag-zPtr));` |
|     11 | 6783 | `			SyStringFullTrim(&sEntry);` |
|     11 | 6784 | `			SySetPut(pSet,(const void *)&sEntry);` |
|      5 | 6785 | `		}` |
|      - | 6786 | `		/* Jump the trailing '>' */` |
|     11 | 6787 | `		zTag++;` |
|      1 | 6788 | `	}` |
|     11 | 6789 | `	return SXRET_OK;` |
|      1 | 6790 | `}` |
|      - | 6791 | `/*` |
|      - | 6792 | ` * Check if the given HTML tag name is present in the given container.` |
|      - | 6793 | ` * Return SXRET_OK if present.SXERR_NOTFOUND otherwise.` |
|      - | 6794 | ` * Refer to [strip_tags()].` |
|      - | 6795 | ` */` |
|     36 | 6796 | `static sxi32 FindTag(SySet *pSet,const char *zTag,int nByte)` |
|      1 | 6797 | `{` |
|     37 | 6798 | `	if( SySetUsed(pSet) > 0 ){` |
|     25 | 6799 | `		const char *zCur,*zEnd = &zTag[nByte];` |
|      - | 6800 | `		SyString sTag;` |
|     85 | 6801 | `		while( zTag < zEnd &&  (zTag[0] == '<' \|\| zTag[0] == '/' \|\| zTag[0] == '?' \|\|` |
|     24 | 6802 | `			((unsigned char)zTag[0] < 0xc0 && SyisSpace(zTag[0]))) ){` |
|     37 | 6803 | `			zTag++;` |
|      1 | 6804 | `		}` |
|      - | 6805 | `		/* Delimit the tag */` |
|     25 | 6806 | `		zCur = zTag;` |
|     77 | 6807 | `		while(zTag < zEnd ){` |
|     77 | 6808 | `			if( (unsigned char)zTag[0] >= 0xc0 ){` |
|      - | 6809 | `				/* UTF-8 stream */` |
|      5 | 6810 | `				zTag++;` |
|      9 | 6811 | `				SX_JMP_UTF8(zTag,zEnd);` |
|     75 | 6812 | `			}else if( !SyisAlphaNum(zTag[0]) ){` |
|     25 | 6813 | `				break;` |
|    ! 0 | 6814 | `			}else{` |
|     49 | 6815 | `				zTag++;` |
|      - | 6816 | `			}` |
|      1 | 6817 | `		}` |
|     25 | 6818 | `		SyStringInitFromBuf(&sTag,zCur,zTag-zCur);` |
|      - | 6819 | `		/* Trim leading white spaces and null bytes */` |
|     35 | 6820 | `		SyStringLeftTrimSafe(&sTag);` |
|     25 | 6821 | `		if( sTag.nByte > 0 ){` |
|      - | 6822 | `			SyString *aEntry,*pEntry;` |
|      - | 6823 | `			sxi32 rc;` |
|      - | 6824 | `			sxu32 n;` |
|      - | 6825 | `			/* Perform the lookup */` |
|     25 | 6826 | `			aEntry = (SyString *)SySetBasePtr(pSet);` |
|     29 | 6827 | `			for( n = 0 ; n < SySetUsed(pSet) ; ++n ){` |
|     25 | 6828 | `				pEntry = &aEntry[n];` |
|      - | 6829 | `				/* Do the comparison */` |
|     25 | 6830 | `				rc = SyStringCmp(pEntry,&sTag,SyStrnicmp);` |
|     25 | 6831 | `				if( !rc ){` |
|     21 | 6832 | `					return SXRET_OK;` |
|      - | 6833 | `				}` |
|      3 | 6834 | `			}` |
|      2 | 6835 | `		}` |
|      2 | 6836 | `	}` |
|      - | 6837 | `	/* No such tag */` |
|     17 | 6838 | `	return SXERR_NOTFOUND;` |
|     19 | 6839 | `}` |
|      - | 6840 | `/*` |
|      - | 6841 | ` * This function tries to return a string [i.e: in the call context result buffer]` |
|      - | 6842 | ` * with all NUL bytes,HTML and PHP tags stripped from a given string.` |
|      - | 6843 | ` * Refer to [strip_tags()].` |
|      - | 6844 | ` */` |
|     16 | 6845 | `PH7_PRIVATE sxi32 PH7_StripTagsFromString(ph7_context *pCtx,const char *zIn,int nByte,const char *zTaglist,int nTaglen)` |
|      1 | 6846 | `{` |
|     17 | 6847 | `	const char *zEnd = &zIn[nByte];` |
|      - | 6848 | `	const char *zPtr,*zTag;` |
|      - | 6849 | `	SySet sSet;` |
|      - | 6850 | `	/* initialize the set of allowed tags */` |
|     17 | 6851 | `	SySetInit(&sSet,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|     17 | 6852 | `	if( nTaglen > 0 ){` |
|      - | 6853 | `		/* Set of allowed tags */` |
|     11 | 6854 | `		AddTag(&sSet,zTaglist,nTaglen);` |
|      5 | 6855 | `	}` |
|      - | 6856 | `	/* Set the empty string */` |
|     17 | 6857 | `	ph7_result_string(pCtx,"",0);` |
|      - | 6858 | `	/* Start processing */` |
|     26 | 6859 | `	for(;;){` |
|     53 | 6860 | `		if(zIn >= zEnd){` |
|      - | 6861 | `			/* No more input to process */` |
|     15 | 6862 | `			break;` |
|      - | 6863 | `		}` |
|     39 | 6864 | `		zPtr = zIn;` |
|      - | 6865 | `		/* Find a tag */` |
|    133 | 6866 | `		while( zIn < zEnd && zIn[0] != '<' && zIn[0] != 0 /* NUL byte */ ){` |
|     95 | 6867 | `			zIn++;` |
|      1 | 6868 | `		}` |
|     39 | 6869 | `		if( zIn > zPtr ){` |
|      - | 6870 | `			/* Consume raw input */` |
|     21 | 6871 | `			ph7_result_string(pCtx,zPtr,(int)(zIn-zPtr));` |
|     10 | 6872 | `		}` |
|      - | 6873 | `		/* Ignore trailing null bytes */` |
|     39 | 6874 | `		while( zIn < zEnd && zIn[0] == 0 ){` |
|    ! 0 | 6875 | `			zIn++;` |
|    ! 0 | 6876 | `		}` |
|     39 | 6877 | `		if(zIn >= zEnd){` |
|      - | 6878 | `			/* No more input to process */` |
|      3 | 6879 | `			break;` |
|      - | 6880 | `		}` |
|     37 | 6881 | `		if( zIn[0] == '<' ){` |
|      - | 6882 | `			sxi32 rc;` |
|     37 | 6883 | `			zTag = zIn++;` |
|      - | 6884 | `			/* Delimit the tag */` |
|    127 | 6885 | `			while( zIn < zEnd && zIn[0] != '>' ){` |
|     91 | 6886 | `				zIn++;` |
|      1 | 6887 | `			}` |
|     37 | 6888 | `			if( zIn < zEnd ){` |
|     37 | 6889 | `				zIn++; /* Ignore the trailing closing tag */` |
|     18 | 6890 | `			}` |
|      - | 6891 | `			/* Query the set */` |
|     37 | 6892 | `			rc = FindTag(&sSet,zTag,(int)(zIn-zTag));` |
|     37 | 6893 | `			if( rc == SXRET_OK ){` |
|      - | 6894 | `				/* Keep the tag */` |
|     21 | 6895 | `				ph7_result_string(pCtx,zTag,(int)(zIn-zTag));` |
|     10 | 6896 | `			}` |
|     18 | 6897 | `		}` |
|      1 | 6898 | `	}` |
|      - | 6899 | `	/* Cleanup */` |
|     17 | 6900 | `	SySetRelease(&sSet);` |
|     17 | 6901 | `	return SXRET_OK;` |
|      1 | 6902 | `}` |
|      - | 6903 | `/*` |
|      - | 6904 | ` * string strip_tags(string $str[,string $allowable_tags])` |
|      - | 6905 | ` *   Strip HTML and PHP tags from a string.` |
|      - | 6906 | ` * Parameters` |
|      - | 6907 | ` *  $str` |
|      - | 6908 | ` *  The input string.` |
|      - | 6909 | ` * $allowable_tags` |
|      - | 6910 | ` *  You can use the optional second parameter to specify tags which should not be stripped.` |
|      - | 6911 | ` * Return` |
|      - | 6912 | ` *  Returns the stripped string.` |
|      - | 6913 | ` */` |
|     14 | 6914 | `static int PH7_builtin_strip_tags(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6915 | `{` |
|     15 | 6916 | `	const char *zTaglist = 0;` |
|      - | 6917 | `	const char *zString;` |
|     15 | 6918 | `	int nTaglen = 0;` |
|      - | 6919 | `	int nLen;` |
|     15 | 6920 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 6921 | `		/* Missing/Invalid arguments,return the empty string */` |
|    ! 0 | 6922 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 6923 | `		return PH7_OK;` |
|      - | 6924 | `	}` |
|      - | 6925 | `	/* Point to the raw string */` |
|     15 | 6926 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 6927 | `	if( nArg > 1 && ph7_value_is_string(apArg[1]) ){` |
|      - | 6928 | `		/* Allowed tag */` |
|     11 | 6929 | `		zTaglist = ph7_value_to_string(apArg[1],&nTaglen);` |
|      5 | 6930 | `	}` |
|      - | 6931 | `	/* Process input */` |
|     15 | 6932 | `	PH7_StripTagsFromString(pCtx,zString,nLen,zTaglist,nTaglen);` |
|     15 | 6933 | `	return PH7_OK;` |
|      8 | 6934 | `}` |
|      - | 6935 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 6936 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 6937 | `/*` |
|      - | 6938 | ` * string str_shuffle(string $str)` |
|      - | 6939 |  |
|      - | 6940 | ` *  Randomly shuffles a string.` |
|      - | 6941 | ` * Parameters` |
|      - | 6942 | ` *  $str` |
|      - | 6943 | ` *   The input string.` |
|      - | 6944 | ` * Return` |
|      - | 6945 | ` *  Returns the shuffled string.` |
|      - | 6946 | ` */` |
|     10 | 6947 | `static int PH7_builtin_str_shuffle(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 6948 | `{` |
|      - | 6949 | `	const char *zString;` |
|      - | 6950 | `	int nLen,i,c;` |
|      - | 6951 | `	sxu32 iR;` |
|     11 | 6952 | `	if( nArg < 1 ){` |
|      - | 6953 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 6954 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 6955 | `		return PH7_OK;` |
|      - | 6956 | `	}` |
|      - | 6957 | `	/* Extract the target string */` |
|     11 | 6958 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     11 | 6959 | `	if( nLen < 1 ){` |
|      - | 6960 | `		/* Nothing to shuffle */` |
|      3 | 6961 | `		ph7_result_string(pCtx,"",0);` |
|      3 | 6962 | `		return PH7_OK;` |
|      - | 6963 | `	}` |
|      - | 6964 | `	/* Shuffle the string */` |
|     43 | 6965 | `	for( i = 0 ; i < nLen ; ++i ){` |
|      - | 6966 | `		/* Generate a random number first */` |
|     35 | 6967 | `		iR = ph7_context_random_num(pCtx);` |
|      - | 6968 | `		/* Extract a random offset */` |
|     35 | 6969 | `		c = zString[iR % nLen];` |
|      - | 6970 | `		/* Append it */` |
|     35 | 6971 | `		ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|     18 | 6972 | `	}` |
|      9 | 6973 | `	return PH7_OK;` |
|      6 | 6974 | `}` |
|      - | 6975 | `/*` |
|      - | 6976 | ` * array str_split(string $string[,int $split_length = 1 ])` |
|      - | 6977 | ` *  Convert a string to an array.` |
|      - | 6978 | ` * Parameters` |
|      - | 6979 | ` * $string` |
|      - | 6980 | ` *  The input string.` |
|      - | 6981 | ` * $split_length` |
|      - | 6982 | ` *  Maximum length of the chunk.` |
|      - | 6983 | ` * Return` |
|      - | 6984 | ` *  Returns an array of chunks. Each chunk is split_length characters long,` |
|      - | 6985 | ` *  except possibly the last one which may be shorter.` |
|      - | 6986 | ` *  If split_length exceeds the string length, the entire string is returned` |
|      - | 6987 | ` *  as the first (and only) array element.` |
|      - | 6988 | ` *  An empty string returns an empty array.` |
|      - | 6989 | ` * Errors` |
|      - | 6990 | ` *  ArgumentCountError if no arguments are given.` |
|      - | 6991 | ` *  TypeError if $string is an array, object or resource.` |
|      - | 6992 | ` *  ValueError if $split_length is less than 1.` |
|      - | 6993 | ` */` |
|     28 | 6994 | `static int PH7_builtin_str_split(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 6995 | `{` |
|      - | 6996 | `	const char *zString,*zEnd;` |
|      - | 6997 | `	ph7_value *pArray,*pValue;` |
|      - | 6998 | `	int split_len;` |
|      - | 6999 | `	int nLen;` |
|     33 | 7000 | `	if( nArg < 1 ){` |
|      4 | 7001 | `		return PH7_VmThrowException(pCtx,` |
|      - | 7002 | `			"ArgumentCountError",` |
|      - | 7003 | `			"str_split() expects at least 1 argument, %d given",` |
|      1 | 7004 | `			nArg` |
|      - | 7005 | `			);` |
|      - | 7006 | `	}` |
|      - | 7007 | `	/* Arrays, objects and resources should raise a TypeError like PHP */` |
|     38 | 7008 | `	if( ph7_value_is_array(apArg[0]) \|\|` |
|     40 | 7009 | `	    ph7_value_is_object(apArg[0]) \|\|` |
|     24 | 7010 | `	    ph7_value_is_resource(apArg[0]) ){` |
|      4 | 7011 | `		return PH7_VmThrowException(pCtx,` |
|      - | 7012 | `			"TypeError",` |
|      - | 7013 | `			"str_split(): Argument #1 ($string) must be of type string, %s given",` |
|      1 | 7014 | `			ph7_type_name(apArg[0])` |
|      - | 7015 | `			);` |
|      - | 7016 | `	}` |
|      - | 7017 | `	/* Point to the target string */` |
|     27 | 7018 | `	zString = ph7_value_to_string(apArg[0],&nLen);` |
|     27 | 7019 | `	split_len = (int)sizeof(char);` |
|     27 | 7020 | `	if( nArg > 1 ){` |
|      - | 7021 | `		/* Split length */` |
|     17 | 7022 | `		split_len = ph7_value_to_int(apArg[1]);` |
|     17 | 7023 | `		if( split_len < 1 ){` |
|      6 | 7024 | `			return PH7_VmThrowException(pCtx,` |
|      - | 7025 | `				"ValueError",` |
|      - | 7026 | `				"str_split(): Argument #2 ($length) must be greater than 0"` |
|      - | 7027 | `				);` |
|      - | 7028 | `		}` |
|     11 | 7029 | `		if( split_len > nLen && nLen > 0 ){` |
|      3 | 7030 | `			split_len = nLen;` |
|      1 | 7031 | `		}` |
|      5 | 7032 | `	}` |
|      - | 7033 | `	/* Create the array and the scalar value */` |
|     21 | 7034 | `	pArray = ph7_context_new_array(pCtx);` |
|      - | 7035 | `	/*Chunk value */` |
|     21 | 7036 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     21 | 7037 | `	if( pValue == 0 \|\| pArray == 0 ){` |
|      - | 7038 | `		/* Return FALSE */` |
|    ! 0 | 7039 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7040 | `		return PH7_OK;` |
|      - | 7041 | `	}` |
|      - | 7042 | `	/* Point to the end of the string */` |
|     21 | 7043 | `	zEnd = &zString[nLen];` |
|      - | 7044 | `	/* Perform the requested operation */` |
|     48 | 7045 | `	for(;;){` |
|      - | 7046 | `		int nMax;` |
|     59 | 7047 | `		if( zString >= zEnd ){` |
|      - | 7048 | `			/* No more input to process */` |
|     21 | 7049 | `			break;` |
|      - | 7050 | `		}` |
|     39 | 7051 | `		nMax = (int)(zEnd-zString);` |
|     39 | 7052 | `		if( nMax < split_len ){` |
|      3 | 7053 | `			split_len = nMax;` |
|      1 | 7054 | `		}` |
|      - | 7055 | `		/* Copy the current chunk */` |
|     39 | 7056 | `		ph7_value_string(pValue,zString,split_len);` |
|      - | 7057 | `		/* Insert it */` |
|     39 | 7058 | `		if( ph7_array_add_elem(pArray,0,pValue) != SXRET_OK ){ /* Will make it's own copy */` |
|    ! 0 | 7059 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 7060 | `		}` |
|      - | 7061 | `		/* reset the string cursor */` |
|     39 | 7062 | `		ph7_value_reset_string_cursor(pValue);` |
|      - | 7063 | `		/* Update position */` |
|     39 | 7064 | `		zString += split_len;` |
|      1 | 7065 | `	}` |
|      - | 7066 | `	/*` |
|      - | 7067 | `	 * Return the array.` |
|      - | 7068 | `	 * Don't worry about freeing memory, everything will be automatically released` |
|      - | 7069 | `	 * upon we return from this function.` |
|      - | 7070 | `	 */` |
|     21 | 7071 | `	ph7_result_value(pCtx,pArray);` |
|     21 | 7072 | `	return PH7_OK;` |
|     19 | 7073 | `}` |
|      - | 7074 | `/*` |
|      - | 7075 | ` * Tokenize a raw string and extract the first non-space token.` |
|      - | 7076 | ` * Refer to [strspn()].` |
|      - | 7077 | ` */` |
|     28 | 7078 | `static sxi32 ExtractNonSpaceToken(const char **pzIn,const char *zEnd,SyString *pOut)` |
|      1 | 7079 | `{` |
|     29 | 7080 | `	const char *zIn = *pzIn;` |
|      - | 7081 | `	const char *zPtr;` |
|      - | 7082 | `	/* Ignore leading white spaces */` |
|     29 | 7083 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|    ! 0 | 7084 | `		zIn++;` |
|    ! 0 | 7085 | `	}` |
|     29 | 7086 | `	if( zIn >= zEnd ){` |
|      - | 7087 | `		/* End of input */` |
|    ! 0 | 7088 | `		return SXERR_EOF;` |
|      - | 7089 | `	}` |
|     29 | 7090 | `	zPtr = zIn;` |
|      - | 7091 | `	/* Extract the token */` |
|    201 | 7092 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && !SyisSpace(zIn[0]) ){` |
|    173 | 7093 | `		zIn++;` |
|      1 | 7094 | `	}` |
|     29 | 7095 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 7096 | `	/* Synchronize pointers */` |
|     29 | 7097 | `	*pzIn = zIn;` |
|      - | 7098 | `	/* Return to the caller */` |
|     29 | 7099 | `	return SXRET_OK;` |
|     15 | 7100 | `}` |
|      - | 7101 | `/*` |
|      - | 7102 | ` * Check if the given string contains only characters from the given mask.` |
|      - | 7103 | ` * return the longest match.` |
|      - | 7104 | ` * Refer to [strspn()].` |
|      - | 7105 | ` */` |
|     18 | 7106 | `static int LongestStringMask(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 7107 | `{` |
|     19 | 7108 | `	const char *zEnd = &zString[nLen];` |
|     19 | 7109 | `	const char *zIn = zString;` |
|      - | 7110 | `	int i,c;` |
|     45 | 7111 | `	for(;;){` |
|     91 | 7112 | `		if( zString >= zEnd ){` |
|      7 | 7113 | `			break;` |
|      - | 7114 | `		}` |
|      - | 7115 | `		/* Extract current character */` |
|     85 | 7116 | `		c = zString[0];` |
|      - | 7117 | `		/* Perform the lookup */` |
|    383 | 7118 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|    371 | 7119 | `			if( c == zMask[i] ){` |
|      - | 7120 | `				/* Character found */` |
|     73 | 7121 | `				break;` |
|      - | 7122 | `			}` |
|    150 | 7123 | `		}` |
|     85 | 7124 | `		if( i >= nMaskLen ){` |
|      - | 7125 | `			/* Character not in the current mask,break immediately */` |
|     13 | 7126 | `			break;` |
|      - | 7127 | `		}` |
|      - | 7128 | `		/* Advance cursor */` |
|     73 | 7129 | `		zString++;` |
|      1 | 7130 | `	}` |
|      - | 7131 | `	/* Longest match */` |
|     19 | 7132 | `	return (int)(zString-zIn);` |
|      1 | 7133 | `}` |
|      - | 7134 | `/*` |
|      - | 7135 | ` * Do the reverse operation of the previous function [i.e: LongestStringMask()].` |
|      - | 7136 | ` * Refer to [strcspn()].` |
|      - | 7137 | ` */` |
|     10 | 7138 | `static int LongestStringMask2(const char *zString,int nLen,const char *zMask,int nMaskLen)` |
|      1 | 7139 | `{` |
|     11 | 7140 | `	const char *zEnd = &zString[nLen];` |
|     11 | 7141 | `	const char *zIn = zString;` |
|      - | 7142 | `	int i,c;` |
|     12 | 7143 | `	for(;;){` |
|     25 | 7144 | `		if( zString >= zEnd ){` |
|      3 | 7145 | `			break;` |
|      - | 7146 | `		}` |
|      - | 7147 | `		/* Extract current character */` |
|     23 | 7148 | `		c = zString[0];` |
|      - | 7149 | `		/* Perform the lookup */` |
|     51 | 7150 | `		for( i = 0 ; i < nMaskLen ; i++ ){` |
|     37 | 7151 | `			if( c == zMask[i] ){` |
|      9 | 7152 | `				break;` |
|      - | 7153 | `			}` |
|     15 | 7154 | `		}` |
|     23 | 7155 | `		if( i < nMaskLen ){` |
|      - | 7156 | `			/* Character in the current mask,break immediately */` |
|      9 | 7157 | `			break;` |
|      - | 7158 | `		}` |
|      - | 7159 | `		/* Advance cursor */` |
|     15 | 7160 | `		zString++;` |
|      1 | 7161 | `	}` |
|      - | 7162 | `	/* Longest match */` |
|     11 | 7163 | `	return (int)(zString-zIn);` |
|      1 | 7164 | `}` |
|      - | 7165 | `/*` |
|      - | 7166 | ` * int strspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 7167 | ` *  Finds the length of the initial segment of a string consisting entirely` |
|      - | 7168 | ` *  of characters contained within a given mask.` |
|      - | 7169 | ` * Parameters` |
|      - | 7170 | ` * $str` |
|      - | 7171 | ` *  The input string.` |
|      - | 7172 | ` * $mask` |
|      - | 7173 | ` *  The list of allowable characters.` |
|      - | 7174 | ` * $start` |
|      - | 7175 | ` *  The position in subject to start searching.` |
|      - | 7176 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 7177 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 7178 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 7179 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 7180 | ` *  start'th position from the end of subject.` |
|      - | 7181 | ` * $length` |
|      - | 7182 | ` *  The length of the segment from subject to examine.` |
|      - | 7183 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 7184 | ` *  characters after the starting position.` |
|      - | 7185 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 7186 | ` *  position up to length characters from the end of subject.` |
|      - | 7187 | ` * Return` |
|      - | 7188 | ` * Returns the length of the initial segment of subject which consists entirely of characters` |
|      - | 7189 | ` * in mask.` |
|      - | 7190 | ` */` |
|     24 | 7191 | `static int PH7_builtin_strspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7192 | `{` |
|      - | 7193 | `	const char *zString,*zMask,*zEnd;` |
|      - | 7194 | `	int iMasklen,iLen;` |
|      - | 7195 | `	SyString sToken;` |
|     25 | 7196 | `	int iCount = 0;` |
|      - | 7197 | `	int rc;` |
|     25 | 7198 | `	if( nArg < 2 ){` |
|      - | 7199 | `		/* Missing agruments,return zero */` |
|    ! 0 | 7200 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7201 | `		return PH7_OK;` |
|      - | 7202 | `	}` |
|      - | 7203 | `	/* Extract the target string */` |
|     25 | 7204 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7205 | `	/* Extract the mask */` |
|     25 | 7206 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     25 | 7207 | `	if( iLen < 1 \|\| iMasklen < 1 ){` |
|      - | 7208 | `		/* Nothing to process,return zero */` |
|      7 | 7209 | `		ph7_result_int(pCtx,0);` |
|      7 | 7210 | `		return PH7_OK;` |
|      - | 7211 | `	}` |
|     19 | 7212 | `	if( nArg > 2 ){` |
|      - | 7213 | `		int nOfft;` |
|      - | 7214 | `		/* Extract the offset */` |
|      9 | 7215 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|      9 | 7216 | `		if( nOfft < 0 ){` |
|    ! 0 | 7217 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 7218 | `			if( zBase > zString ){` |
|    ! 0 | 7219 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 7220 | `				zString = zBase;` |
|    ! 0 | 7221 | `			}else{` |
|      - | 7222 | `				/* Invalid offset */` |
|    ! 0 | 7223 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7224 | `				return PH7_OK;` |
|      - | 7225 | `			}` |
|    ! 0 | 7226 | `		}else{` |
|      9 | 7227 | `			if( nOfft >= iLen ){` |
|      - | 7228 | `				/* Invalid offset */` |
|    ! 0 | 7229 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7230 | `				return PH7_OK;` |
|    ! 0 | 7231 | `			}else{` |
|      - | 7232 | `				/* Update offset */` |
|      9 | 7233 | `				zString += nOfft;` |
|      9 | 7234 | `				iLen -= nOfft;` |
|      - | 7235 | `			}` |
|      - | 7236 | `		}` |
|      9 | 7237 | `		if( nArg > 3 ){` |
|      - | 7238 | `			int iUserlen;` |
|      - | 7239 | `			/* Extract the desired length */` |
|      9 | 7240 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|      9 | 7241 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|      5 | 7242 | `				iLen = iUserlen;` |
|      2 | 7243 | `			}` |
|      4 | 7244 | `		}` |
|      4 | 7245 | `	}` |
|      - | 7246 | `	/* Point to the end of the string */` |
|     19 | 7247 | `	zEnd = &zString[iLen];` |
|      - | 7248 | `	/* Extract the first non-space token */` |
|     19 | 7249 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     19 | 7250 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 7251 | `		/* Compare against the current mask */` |
|     19 | 7252 | `		iCount = LongestStringMask(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      9 | 7253 | `	}` |
|      - | 7254 | `	/* Longest match */` |
|     19 | 7255 | `	ph7_result_int(pCtx,iCount);` |
|     19 | 7256 | `	return PH7_OK;` |
|     13 | 7257 | `}` |
|      - | 7258 | `/*` |
|      - | 7259 | ` * int strcspn(string $str,string $mask[,int $start[,int $length]])` |
|      - | 7260 | ` *  Find length of initial segment not matching mask.` |
|      - | 7261 | ` * Parameters` |
|      - | 7262 | ` * $str` |
|      - | 7263 | ` *  The input string.` |
|      - | 7264 | ` * $mask` |
|      - | 7265 | ` *  The list of not allowed characters.` |
|      - | 7266 | ` * $start` |
|      - | 7267 | ` *  The position in subject to start searching.` |
|      - | 7268 | ` *  If start is given and is non-negative, then strspn() will begin examining` |
|      - | 7269 | ` *  subject at the start'th position. For instance, in the string 'abcdef', the character` |
|      - | 7270 | ` *  at position 0 is 'a', the character at position 2 is 'c', and so forth.` |
|      - | 7271 | ` *  If start is given and is negative, then strspn() will begin examining subject at the` |
|      - | 7272 | ` *  start'th position from the end of subject.` |
|      - | 7273 | ` * $length` |
|      - | 7274 | ` *  The length of the segment from subject to examine.` |
|      - | 7275 | ` *  If length is given and is non-negative, then subject will be examined for length` |
|      - | 7276 | ` *  characters after the starting position.` |
|      - | 7277 | ` *  If lengthis given and is negative, then subject will be examined from the starting` |
|      - | 7278 | ` *  position up to length characters from the end of subject.` |
|      - | 7279 | ` * Return` |
|      - | 7280 | ` *  Returns the length of the segment as an integer.` |
|      - | 7281 | ` */` |
|     14 | 7282 | `static int PH7_builtin_strcspn(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7283 | `{` |
|      - | 7284 | `	const char *zString,*zMask,*zEnd;` |
|      - | 7285 | `	int iMasklen,iLen;` |
|      - | 7286 | `	SyString sToken;` |
|     15 | 7287 | `	int iCount = 0;` |
|      - | 7288 | `	int rc;` |
|     15 | 7289 | `	if( nArg < 2 ){` |
|      - | 7290 | `		/* Missing agruments,return zero */` |
|    ! 0 | 7291 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7292 | `		return PH7_OK;` |
|      - | 7293 | `	}` |
|      - | 7294 | `	/* Extract the target string */` |
|     15 | 7295 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7296 | `	/* Extract the mask */` |
|     15 | 7297 | `	zMask = ph7_value_to_string(apArg[1],&iMasklen);` |
|     15 | 7298 | `	if( iLen < 1 ){` |
|      - | 7299 | `		/* Nothing to process,return zero */` |
|    ! 0 | 7300 | `		ph7_result_int(pCtx,0);` |
|    ! 0 | 7301 | `		return PH7_OK;` |
|      - | 7302 | `	}` |
|     15 | 7303 | `	if( iMasklen < 1 ){` |
|      - | 7304 | `		/* No given mask,return the string length */` |
|      3 | 7305 | `		ph7_result_int(pCtx,iLen);` |
|      3 | 7306 | `		return PH7_OK;` |
|      - | 7307 | `	}` |
|     13 | 7308 | `	if( nArg > 2 ){` |
|      - | 7309 | `		int nOfft;` |
|      - | 7310 | `		/* Extract the offset */` |
|     11 | 7311 | `		nOfft = ph7_value_to_int(apArg[2]);` |
|     11 | 7312 | `		if( nOfft < 0 ){` |
|    ! 0 | 7313 | `			const char *zBase = &zString[iLen + nOfft];` |
|    ! 0 | 7314 | `			if( zBase > zString ){` |
|    ! 0 | 7315 | `				iLen = (int)(&zString[iLen]-zBase);` |
|    ! 0 | 7316 | `				zString = zBase;` |
|    ! 0 | 7317 | `			}else{` |
|      - | 7318 | `				/* Invalid offset */` |
|    ! 0 | 7319 | `				ph7_result_int(pCtx,0);` |
|    ! 0 | 7320 | `				return PH7_OK;` |
|      - | 7321 | `			}` |
|    ! 0 | 7322 | `		}else{` |
|     11 | 7323 | `			if( nOfft >= iLen ){` |
|      - | 7324 | `				/* Invalid offset */` |
|      3 | 7325 | `				ph7_result_int(pCtx,0);` |
|      3 | 7326 | `				return PH7_OK;` |
|    ! 0 | 7327 | `			}else{` |
|      - | 7328 | `				/* Update offset */` |
|      9 | 7329 | `				zString += nOfft;` |
|      9 | 7330 | `				iLen -= nOfft;` |
|      - | 7331 | `			}` |
|      - | 7332 | `		}` |
|      9 | 7333 | `		if( nArg > 3 ){` |
|      - | 7334 | `			int iUserlen;` |
|      - | 7335 | `			/* Extract the desired length */` |
|    ! 0 | 7336 | `			iUserlen = ph7_value_to_int(apArg[3]);` |
|    ! 0 | 7337 | `			if( iUserlen > 0 && iUserlen < iLen ){` |
|    ! 0 | 7338 | `				iLen = iUserlen;` |
|    ! 0 | 7339 | `			}` |
|    ! 0 | 7340 | `		}` |
|      4 | 7341 | `	}` |
|      - | 7342 | `	/* Point to the end of the string */` |
|     11 | 7343 | `	zEnd = &zString[iLen];` |
|      - | 7344 | `	/* Extract the first non-space token */` |
|     11 | 7345 | `	rc = ExtractNonSpaceToken(&zString,zEnd,&sToken);` |
|     11 | 7346 | `	if( rc == SXRET_OK && sToken.nByte > 0 ){` |
|      - | 7347 | `		/* Compare against the current mask */` |
|     11 | 7348 | `		iCount = LongestStringMask2(sToken.zString,(int)sToken.nByte,zMask,iMasklen);` |
|      5 | 7349 | `	}` |
|      - | 7350 | `	/* Longest match */` |
|     11 | 7351 | `	ph7_result_int(pCtx,iCount);` |
|     11 | 7352 | `	return PH7_OK;` |
|      8 | 7353 | `}` |
|      - | 7354 | `/*` |
|      - | 7355 | ` * string strpbrk(string $haystack,string $char_list)` |
|      - | 7356 | ` *  Search a string for any of a set of characters.` |
|      - | 7357 | ` * Parameters` |
|      - | 7358 | ` *  $haystack` |
|      - | 7359 | ` *   The string where char_list is looked for.` |
|      - | 7360 | ` *  $char_list` |
|      - | 7361 | ` *   This parameter is case sensitive.` |
|      - | 7362 | ` * Return` |
|      - | 7363 | ` *  Returns a string starting from the character found, or FALSE if it is not found.` |
|      - | 7364 | ` */` |
|      4 | 7365 | `static int PH7_builtin_strpbrk(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7366 | `{` |
|      - | 7367 | `	const char *zString,*zList,*zEnd;` |
|      - | 7368 | `	int iLen,iListLen,i,c;` |
|      - | 7369 | `	sxu32 nOfft,nMax;` |
|      - | 7370 | `	sxi32 rc;` |
|      5 | 7371 | `	if( nArg < 2 ){` |
|      - | 7372 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 7373 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7374 | `		return PH7_OK;` |
|      - | 7375 | `	}` |
|      - | 7376 | `	/* Extract the haystack and the char list */` |
|      5 | 7377 | `	zString = ph7_value_to_string(apArg[0],&iLen);` |
|      5 | 7378 | `	zList = ph7_value_to_string(apArg[1],&iListLen);` |
|      5 | 7379 | `	if( iLen < 1 ){` |
|      - | 7380 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 7381 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 7382 | `		return PH7_OK;` |
|      - | 7383 | `	}` |
|      - | 7384 | `	/* Point to the end of the string */` |
|      5 | 7385 | `	zEnd = &zString[iLen];` |
|      5 | 7386 | `	nOfft = nMax = SXU32_HIGH;` |
|      - | 7387 | `	/* perform the requested operation */` |
|     15 | 7388 | `	for( i = 0 ; i < iListLen ; i++ ){` |
|     11 | 7389 | `		c = zList[i];` |
|     11 | 7390 | `		rc = SyByteFind(zString,(sxu32)iLen,c,&nMax);` |
|     11 | 7391 | `		if( rc == SXRET_OK ){` |
|      5 | 7392 | `			if( nMax < nOfft ){` |
|      3 | 7393 | `				nOfft = nMax;` |
|      1 | 7394 | `			}` |
|      2 | 7395 | `		}` |
|      6 | 7396 | `	}` |
|      5 | 7397 | `	if( nOfft == SXU32_HIGH ){` |
|      - | 7398 | `		/* No such substring,return FALSE */` |
|      3 | 7399 | `		ph7_result_bool(pCtx,0);` |
|      2 | 7400 | `	}else{` |
|      - | 7401 | `		/* Return the substring */` |
|      3 | 7402 | `		ph7_result_string(pCtx,&zString[nOfft],(int)(zEnd-&zString[nOfft]));` |
|      - | 7403 | `	}` |
|      5 | 7404 | `	return PH7_OK;` |
|      3 | 7405 | `}` |
|      - | 7406 | `/* SPDX-SnippetBegin */` |
|      - | 7407 | `/* SPDX-SnippetCopyrightText: D. Richard Hipp and the SQLite authors <https://sqlite.org/> */` |
|      - | 7408 | `/* SPDX-License-Identifier: blessing */` |
|      - | 7409 | `/*` |
|      - | 7410 | ` * string soundex(string $str)` |
|      - | 7411 | ` *  Calculate the soundex key of a string.` |
|      - | 7412 | ` * Parameters` |
|      - | 7413 | ` *  $str` |
|      - | 7414 | ` *   The input string.` |
|      - | 7415 | ` * Return` |
|      - | 7416 | ` *  Returns the soundex key as a string.` |
|      - | 7417 | ` * Note:` |
|      - | 7418 | ` *  This implementation is based on the one found in the SQLite3` |
|      - | 7419 | ` * source tree.` |
|      - | 7420 | ` */` |
|     22 | 7421 | `static int PH7_builtin_soundex(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7422 | `{` |
|      - | 7423 | `	const unsigned char *zIn;` |
|      - | 7424 | `	char zResult[8];` |
|      - | 7425 | `	int i, j;` |
|      - | 7426 | `	static const unsigned char iCode[] = {` |
|      - | 7427 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7428 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7429 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7430 | `		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,` |
|      - | 7431 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 7432 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 7433 | `		0, 0, 1, 2, 3, 0, 1, 2, 0, 0, 2, 2, 4, 5, 5, 0,` |
|      - | 7434 | `		1, 2, 6, 2, 3, 0, 1, 0, 2, 0, 2, 0, 0, 0, 0, 0,` |
|      - | 7435 | `	};` |
|     23 | 7436 | `	if( nArg < 1 ){` |
|      - | 7437 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7438 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7439 | `		return PH7_OK;` |
|      - | 7440 | `	}` |
|     23 | 7441 | `	zIn = (unsigned char *)ph7_value_to_string(apArg[0],0);` |
|     35 | 7442 | `	for(i=0; zIn[i] && zIn[i] < 0xc0 && !SyisAlpha(zIn[i]); i++){}` |
|     23 | 7443 | `	if( zIn[i] ){` |
|     17 | 7444 | `		unsigned char prevcode = iCode[zIn[i]&0x7f];` |
|     17 | 7445 | `		zResult[0] = (char)SyToUpper(zIn[i]);` |
|    109 | 7446 | `		for(j=1; j<4 && zIn[i]; i++){` |
|     93 | 7447 | `			int code = iCode[zIn[i]&0x7f];` |
|     93 | 7448 | `			if( code>0 ){` |
|     45 | 7449 | `				if( code!=prevcode ){` |
|     33 | 7450 | `					prevcode = (unsigned char)code;` |
|     33 | 7451 | `					zResult[j++] = (char)code + '0';` |
|     16 | 7452 | `				}` |
|     23 | 7453 | `			}else{` |
|     49 | 7454 | `				prevcode = 0;` |
|      - | 7455 | `			}` |
|     47 | 7456 | `		}` |
|     33 | 7457 | `		while( j<4 ){` |
|     17 | 7458 | `			zResult[j++] = '0';` |
|      1 | 7459 | `		}` |
|     17 | 7460 | `		ph7_result_string(pCtx,zResult,4);` |
|      9 | 7461 | `	}else{` |
|      - | 7462 | `	  /* No alphabetic character: PHP returns "0000" (not the SQLite "?000"). */` |
|      7 | 7463 | `	  ph7_result_string(pCtx,"0000",4);` |
|      - | 7464 | `	}` |
|     23 | 7465 | `	return PH7_OK;` |
|     12 | 7466 | `}` |
|      - | 7467 | `/* SPDX-SnippetEnd */` |
|      - | 7468 | `/*` |
|      - | 7469 | ` * string wordwrap(string $str[,int $width = 75[,string $break = "\n"]])` |
|      - | 7470 | ` *  Wraps a string to a given number of characters.` |
|      - | 7471 | ` * Parameters` |
|      - | 7472 | ` *  $str` |
|      - | 7473 | ` *   The input string.` |
|      - | 7474 | ` * $width` |
|      - | 7475 | ` *  The column width.` |
|      - | 7476 | ` * $break` |
|      - | 7477 | ` *  The line is broken using the optional break parameter.` |
|      - | 7478 | ` * Return` |
|      - | 7479 | ` *  Returns the given string wrapped at the specified column.` |
|      - | 7480 | ` */` |
|     26 | 7481 | `static int PH7_builtin_wordwrap(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7482 | `{` |
|      - | 7483 | `	const char *zIn,*zBreak;` |
|      - | 7484 | `	SyBlob sWorker;` |
|      - | 7485 | `	int iLen,iBreaklen,iWidth,iCut,iStart,iSpace,iCur;` |
|      - | 7486 | `	sxi32 rc;` |
|     27 | 7487 | `	if( nArg < 1 ){` |
|      - | 7488 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7489 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7490 | `		return PH7_OK;` |
|      - | 7491 | `	}` |
|      - | 7492 | `	/* Extract the input string */` |
|     27 | 7493 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7494 | `	/* Width (default 75; PHP allows 0/negative — break at every space). */` |
|     27 | 7495 | `	iWidth = 75;` |
|     27 | 7496 | `	if( nArg > 1 ){` |
|     27 | 7497 | `		iWidth = ph7_value_to_int(apArg[1]);` |
|     13 | 7498 | `	}` |
|      - | 7499 | `	/* Break string (default "\n"). */` |
|     27 | 7500 | `	zBreak = "\n";` |
|     27 | 7501 | `	iBreaklen = (int)sizeof(char);` |
|     27 | 7502 | `	if( nArg > 2 ){` |
|     13 | 7503 | `		zBreak = ph7_value_to_string(apArg[2],&iBreaklen);` |
|      6 | 7504 | `	}` |
|      - | 7505 | `	/* Cut long words? (default false). */` |
|     27 | 7506 | `	iCut = 0;` |
|     27 | 7507 | `	if( nArg > 3 ){` |
|      7 | 7508 | `		iCut = ph7_value_to_bool(apArg[3]);` |
|      3 | 7509 | `	}` |
|     27 | 7510 | `	if( iLen < 1 ){` |
|      - | 7511 | `		/* PHP returns the empty string for empty input before validating the other args. */` |
|      5 | 7512 | `		ph7_result_string(pCtx,"",0);` |
|      5 | 7513 | `		return PH7_OK;` |
|      - | 7514 | `	}` |
|      - | 7515 | `	/* PHP 8 domain errors (catchable ValueError). */` |
|     23 | 7516 | `	if( iBreaklen < 1 ){` |
|      3 | 7517 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7518 | `			"wordwrap(): Argument #3 ($break) must not be empty");` |
|      - | 7519 | `	}` |
|     21 | 7520 | `	if( iWidth == 0 && iCut ){` |
|      3 | 7521 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7522 | `			"wordwrap(): Argument #4 ($cut_long_words) cannot be true when argument #2 ($width) is 0");` |
|      - | 7523 | `	}` |
|      - | 7524 | `	/*` |
|      - | 7525 | `	 * PHP's algorithm: a single left-to-right pass tracking the start of the` |
|      - | 7526 | `	 * current line (iStart) and the position of the last space seen on it` |
|      - | 7527 | `	 * (iSpace). A break is emitted when the line reaches the width, at the last` |
|      - | 7528 | `	 * space if there was one, otherwise (only when cut is enabled) hard at the` |
|      - | 7529 | `	 * boundary. An existing break sequence in the input resets the line.` |
|      - | 7530 | `	 */` |
|     19 | 7531 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     19 | 7532 | `	iStart = iSpace = iCur = 0;` |
|     19 | 7533 | `	rc = SXRET_OK;` |
|    551 | 7534 | `	while( iCur < iLen ){` |
|    533 | 7535 | `		if( iBreaklen <= iLen - iCur && SyMemcmp(&zIn[iCur],zBreak,(sxu32)iBreaklen) == 0 ){` |
|      - | 7536 | `			/* Existing break sequence in the input: copy it verbatim and reset the line. */` |
|    ! 0 | 7537 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart + iBreaklen));` |
|    ! 0 | 7538 | `			if( rc != SXRET_OK ){ goto oom; }` |
|    ! 0 | 7539 | `			iCur += iBreaklen;` |
|    ! 0 | 7540 | `			iStart = iSpace = iCur;` |
|    ! 0 | 7541 | `			continue;` |
|    533 | 7542 | `		}else if( zIn[iCur] == ' ' ){` |
|     67 | 7543 | `			if( iCur - iStart >= iWidth ){` |
|      - | 7544 | `				/* The line already fills the width at this space: break here (the space is consumed). */` |
|     13 | 7545 | `				rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     13 | 7546 | `				if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     13 | 7547 | `				if( rc != SXRET_OK ){ goto oom; }` |
|     13 | 7548 | `				iStart = iCur + 1;` |
|      6 | 7549 | `			}` |
|     67 | 7550 | `			iSpace = iCur;` |
|    500 | 7551 | `		}else if( iCut && iCur - iStart >= iWidth && iStart >= iSpace ){` |
|      - | 7552 | `			/* A word longer than the width with no space to break at: hard-cut at the boundary. */` |
|      7 | 7553 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|      7 | 7554 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|      7 | 7555 | `			if( rc != SXRET_OK ){ goto oom; }` |
|      7 | 7556 | `			iStart = iSpace = iCur;` |
|    464 | 7557 | `		}else if( iCur - iStart >= iWidth && iStart < iSpace ){` |
|      - | 7558 | `			/* Past the width mid-word: wrap back to the last space (which is consumed). */` |
|     17 | 7559 | `			rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iSpace - iStart));` |
|     17 | 7560 | `			if( rc == SXRET_OK ){ rc = SyBlobAppend(&sWorker,zBreak,(sxu32)iBreaklen); }` |
|     17 | 7561 | `			if( rc != SXRET_OK ){ goto oom; }` |
|     17 | 7562 | `			iStart = iSpace = iSpace + 1;` |
|      8 | 7563 | `		}` |
|    533 | 7564 | `		iCur++;` |
|      1 | 7565 | `	}` |
|      - | 7566 | `	/* Emit the trailing chunk. */` |
|     19 | 7567 | `	if( iStart < iCur ){` |
|     19 | 7568 | `		rc = SyBlobAppend(&sWorker,&zIn[iStart],(sxu32)(iCur - iStart));` |
|     19 | 7569 | `		if( rc != SXRET_OK ){ goto oom; }` |
|      9 | 7570 | `	}` |
|     19 | 7571 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|     19 | 7572 | `	SyBlobRelease(&sWorker);` |
|     19 | 7573 | `	return PH7_OK;` |
|    ! 0 | 7574 | `oom:` |
|    ! 0 | 7575 | `	SyBlobRelease(&sWorker);` |
|    ! 0 | 7576 | `	return PH7_ContextMemoryError(pCtx);` |
|     14 | 7577 | `}` |
|      - | 7578 | `/*` |
|      - | 7579 | ` * Check if the given character is a member of the given mask.` |
|      - | 7580 | ` * Return TRUE on success. FALSE otherwise.` |
|      - | 7581 | ` * Refer to [strtok()].` |
|      - | 7582 | ` */` |
|     30 | 7583 | `static int CheckMask(int c,const char *zMask,int nMasklen,int *pOfft)` |
|      1 | 7584 | `{` |
|      - | 7585 | `	int i;` |
|     57 | 7586 | `	for( i = 0 ; i < nMasklen ; ++i ){` |
|     39 | 7587 | `		if( c == zMask[i] ){` |
|     13 | 7588 | `			if( pOfft ){` |
|      5 | 7589 | `				*pOfft = i;` |
|      2 | 7590 | `			}` |
|     13 | 7591 | `			return TRUE;` |
|      - | 7592 | `		}` |
|     14 | 7593 | `	}` |
|     19 | 7594 | `	return FALSE;` |
|     16 | 7595 | `}` |
|      - | 7596 | `/*` |
|      - | 7597 | ` * Extract a single token from the input stream.` |
|      - | 7598 | ` * Refer to [strtok()].` |
|      - | 7599 | ` */` |
|      6 | 7600 | `static sxi32 ExtractToken(const char **pzIn,const char *zEnd,const char *zMask,int nMasklen,SyString *pOut)` |
|      1 | 7601 | `{` |
|      7 | 7602 | `	const char *zIn = *pzIn;` |
|      - | 7603 | `	const char *zPtr;` |
|      - | 7604 | `	/* Ignore leading delimiter */` |
|     11 | 7605 | `	while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 7606 | `		zIn++;` |
|      1 | 7607 | `	}` |
|      7 | 7608 | `	if( zIn >= zEnd ){` |
|      - | 7609 | `		/* End of input */` |
|    ! 0 | 7610 | `		return SXERR_EOF;` |
|      - | 7611 | `	}` |
|      7 | 7612 | `	zPtr = zIn;` |
|      - | 7613 | `	/* Extract the token */` |
|     13 | 7614 | `	while( zIn < zEnd ){` |
|     11 | 7615 | `		if( (unsigned char)zIn[0] >= 0xc0 ){` |
|      - | 7616 | `			/* UTF-8 stream */` |
|    ! 0 | 7617 | `			zIn++;` |
|    ! 0 | 7618 | `			SX_JMP_UTF8(zIn,zEnd);` |
|    ! 0 | 7619 | `		}else{` |
|     11 | 7620 | `			if( CheckMask(zIn[0],zMask,nMasklen,0) ){` |
|      5 | 7621 | `				break;` |
|      - | 7622 | `			}` |
|      7 | 7623 | `			zIn++;` |
|      - | 7624 | `		}` |
|      1 | 7625 | `	}` |
|      7 | 7626 | `	SyStringInitFromBuf(pOut,zPtr,zIn-zPtr);` |
|      - | 7627 | `	/* Update the cursor */` |
|      7 | 7628 | `	*pzIn = zIn;` |
|      - | 7629 | `	/* Return to the caller */` |
|      7 | 7630 | `	return SXRET_OK;` |
|      4 | 7631 | `}` |
|      - | 7632 | `/* strtok auxiliary private data */` |
|      - | 7633 | `typedef struct strtok_aux_data strtok_aux_data;` |
|      - | 7634 | `struct strtok_aux_data` |
|      - | 7635 | `{` |
|      - | 7636 | `	const char *zDup;  /* Complete duplicate of the input */` |
|      - | 7637 | `	const char *zIn;   /* Current input stream */` |
|      - | 7638 | `	const char *zEnd;  /* End of input */` |
|      - | 7639 | `};` |
|      - | 7640 | `/*` |
|      - | 7641 | ` * string strtok(string $str,string $token)` |
|      - | 7642 | ` * string strtok(string $token)` |
|      - | 7643 | ` *  strtok() splits a string (str) into smaller strings (tokens), with each token` |
|      - | 7644 | ` *  being delimited by any character from token. That is, if you have a string like` |
|      - | 7645 | ` *  "This is an example string" you could tokenize this string into its individual` |
|      - | 7646 | ` *  words by using the space character as the token.` |
|      - | 7647 | ` *  Note that only the first call to strtok uses the string argument. Every subsequent` |
|      - | 7648 | ` *  call to strtok only needs the token to use, as it keeps track of where it is in` |
|      - | 7649 | ` *  the current string. To start over, or to tokenize a new string you simply call strtok` |
|      - | 7650 | ` *  with the string argument again to initialize it. Note that you may put multiple tokens` |
|      - | 7651 | ` *  in the token parameter. The string will be tokenized when any one of the characters in` |
|      - | 7652 | ` *  the argument are found.` |
|      - | 7653 | ` * Parameters` |
|      - | 7654 | ` *  $str` |
|      - | 7655 | ` *  The string being split up into smaller strings (tokens).` |
|      - | 7656 | ` * $token` |
|      - | 7657 | ` *  The delimiter used when splitting up str.` |
|      - | 7658 | ` * Return` |
|      - | 7659 | ` *   Current token or FALSE on EOF.` |
|      - | 7660 | ` */` |
|      6 | 7661 | `static int PH7_builtin_strtok(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7662 | `{` |
|      - | 7663 | `	strtok_aux_data *pAux;` |
|      - | 7664 | `	const char *zMask;` |
|      - | 7665 | `	SyString sToken;` |
|      - | 7666 | `	int nMasklen;` |
|      - | 7667 | `	sxi32 rc;` |
|      7 | 7668 | `	if( nArg < 2 ){` |
|      - | 7669 | `		/* Extract top aux data */` |
|      5 | 7670 | `		pAux = (strtok_aux_data *)ph7_context_peek_aux_data(pCtx);` |
|      5 | 7671 | `		if( pAux == 0 ){` |
|      - | 7672 | `			/* No aux data,return FALSE */` |
|    ! 0 | 7673 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7674 | `			return PH7_OK;` |
|      - | 7675 | `		}` |
|      5 | 7676 | `		nMasklen = 0;` |
|      5 | 7677 | `		zMask = ""; /* cc warning */` |
|      5 | 7678 | `		if( nArg > 0 ){` |
|      - | 7679 | `			/* Extract the mask */` |
|      5 | 7680 | `			zMask = ph7_value_to_string(apArg[0],&nMasklen);` |
|      2 | 7681 | `		}` |
|      5 | 7682 | `		if( nMasklen < 1 ){` |
|      - | 7683 | `			/* Invalid mask,return FALSE */` |
|    ! 0 | 7684 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 7685 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7686 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 7687 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7688 | `			return PH7_OK;` |
|      - | 7689 | `		}` |
|      - | 7690 | `		/* Extract the token */` |
|      5 | 7691 | `		rc = ExtractToken(&pAux->zIn,pAux->zEnd,zMask,nMasklen,&sToken);` |
|      5 | 7692 | `		if( rc != SXRET_OK ){` |
|      - | 7693 | `			/* EOF ,discard the aux data */` |
|    ! 0 | 7694 | `			ph7_context_free_chunk(pCtx,(void *)pAux->zDup);` |
|    ! 0 | 7695 | `			ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7696 | `			(void)ph7_context_pop_aux_data(pCtx);` |
|    ! 0 | 7697 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7698 | `		}else{` |
|      - | 7699 | `			/* Return the extracted token */` |
|      5 | 7700 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 7701 | `		}` |
|      3 | 7702 | `	}else{` |
|      - | 7703 | `		const char *zInput,*zCur;` |
|      - | 7704 | `		char *zDup;` |
|      - | 7705 | `		int nLen;` |
|      - | 7706 | `		/* Extract the raw input */` |
|      3 | 7707 | `		zCur = zInput = ph7_value_to_string(apArg[0],&nLen);` |
|      3 | 7708 | `		if( nLen < 1 ){` |
|      - | 7709 | `			/* Empty input,return FALSE */` |
|    ! 0 | 7710 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7711 | `			return PH7_OK;` |
|      - | 7712 | `		}` |
|      - | 7713 | `		/* Extract the mask */` |
|      3 | 7714 | `		zMask = ph7_value_to_string(apArg[1],&nMasklen);` |
|      3 | 7715 | `		if( nMasklen < 1 ){` |
|      - | 7716 | `			/* Set a default mask */` |
|      - | 7717 | `#define TOK_MASK " \n\t\r\f"` |
|    ! 0 | 7718 | `			zMask = TOK_MASK;` |
|    ! 0 | 7719 | `			nMasklen = (int)sizeof(TOK_MASK) - 1;` |
|      - | 7720 | `#undef TOK_MASK` |
|    ! 0 | 7721 | `		}` |
|      - | 7722 | `		/* Extract a single token */` |
|      3 | 7723 | `		rc = ExtractToken(&zInput,&zInput[nLen],zMask,nMasklen,&sToken);` |
|      3 | 7724 | `		if( rc != SXRET_OK ){` |
|      - | 7725 | `			/* Empty input */` |
|    ! 0 | 7726 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 | 7727 | `			return PH7_OK;` |
|    ! 0 | 7728 | `		}else{` |
|      - | 7729 | `			/* Return the extracted token */` |
|      3 | 7730 | `			ph7_result_string(pCtx,sToken.zString,(int)sToken.nByte);` |
|      - | 7731 | `		}` |
|      - | 7732 | `		/* Create our auxilliary data and copy the input */` |
|      3 | 7733 | `		pAux = (strtok_aux_data *)ph7_context_alloc_chunk(pCtx,sizeof(strtok_aux_data),TRUE,FALSE);` |
|      3 | 7734 | `		if( pAux ){` |
|      3 | 7735 | `			nLen -= (int)(zInput-zCur);` |
|      3 | 7736 | `			if( nLen < 1 ){` |
|    ! 0 | 7737 | `				ph7_context_free_chunk(pCtx,pAux);` |
|    ! 0 | 7738 | `				return PH7_OK;` |
|      - | 7739 | `			}` |
|      - | 7740 | `			/* Duplicate input */` |
|      3 | 7741 | `			zDup = (char *)ph7_context_alloc_chunk(pCtx,(unsigned int)(nLen+1),TRUE,FALSE);` |
|      3 | 7742 | `			if( zDup  ){` |
|      3 | 7743 | `				SyMemcpy(zInput,zDup,(sxu32)nLen);` |
|      - | 7744 | `				/* Register the aux data */` |
|      3 | 7745 | `				pAux->zDup = pAux->zIn = zDup;` |
|      3 | 7746 | `				pAux->zEnd = &zDup[nLen];` |
|      3 | 7747 | `				ph7_context_push_aux_data(pCtx,pAux);` |
|      1 | 7748 | `			}` |
|      1 | 7749 | `		}` |
|      - | 7750 | `	}` |
|      7 | 7751 | `	return PH7_OK;` |
|      4 | 7752 | `}` |
|      - | 7753 | `/*` |
|      - | 7754 | ` * string str_pad(string $input,int $pad_length[,string $pad_string = " " [,int $pad_type = STR_PAD_RIGHT]])` |
|      - | 7755 | ` *  Pad a string to a certain length with another string` |
|      - | 7756 | ` * Parameters` |
|      - | 7757 | ` *  $input` |
|      - | 7758 | ` *   The input string.` |
|      - | 7759 | ` * $pad_length` |
|      - | 7760 | ` *   If the value of pad_length is negative, less than, or equal to the length of the input` |
|      - | 7761 | ` *   string, no padding takes place.` |
|      - | 7762 | ` * $pad_string` |
|      - | 7763 | ` *   Note:` |
|      - | 7764 | ` *    The pad_string WIIL NOT BE truncated if the required number of padding characters can't be evenly` |
|      - | 7765 | ` *    divided by the pad_string's length.` |
|      - | 7766 | ` * $pad_type` |
|      - | 7767 | ` *    Optional argument pad_type can be STR_PAD_RIGHT, STR_PAD_LEFT, or STR_PAD_BOTH. If pad_type` |
|      - | 7768 | ` *    is not specified it is assumed to be STR_PAD_RIGHT.` |
|      - | 7769 | ` * Return` |
|      - | 7770 | ` *  The padded string.` |
|      - | 7771 | ` */` |
|     10 | 7772 | `static int PH7_builtin_str_pad(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 7773 | `{` |
|      - | 7774 | `	int iLen,iPadlen,iType,i,iDiv,iStrpad,iRealPad,jPad;` |
|      - | 7775 | `	const char *zIn,*zPad;` |
|     11 | 7776 | `	if( nArg < 2 ){` |
|      - | 7777 | `		/* Missing arguments,return the empty string */` |
|    ! 0 | 7778 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 | 7779 | `		return PH7_OK;` |
|      - | 7780 | `	}` |
|      - | 7781 | `	/* Extract the target string */` |
|     11 | 7782 | `	zIn = ph7_value_to_string(apArg[0],&iLen);` |
|      - | 7783 | `	/* Padding length */` |
|     11 | 7784 | `	iRealPad = iPadlen = ph7_value_to_int(apArg[1]);` |
|     11 | 7785 | `	if( iPadlen > 0 ){` |
|      9 | 7786 | `		iPadlen -= iLen;` |
|      4 | 7787 | `	}` |
|     11 | 7788 | `	if( iPadlen < 1  ){` |
|      - | 7789 | `		/* Return the string verbatim */` |
|      5 | 7790 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      5 | 7791 | `		return PH7_OK;` |
|      - | 7792 | `	}` |
|      7 | 7793 | `	zPad = " "; /* Whitespace padding */` |
|      7 | 7794 | `	iStrpad = (int)sizeof(char);` |
|      7 | 7795 | `	iType = 1 ; /* STR_PAD_RIGHT */` |
|      7 | 7796 | `	if( nArg > 2 ){` |
|      - | 7797 | `		/* Padding string */` |
|      7 | 7798 | `		zPad = ph7_value_to_string(apArg[2],&iStrpad);` |
|      7 | 7799 | `		if( iStrpad < 1 ){` |
|      - | 7800 | `			/* An empty pad string throws a catchable ValueError in PHP 8` |
|      - | 7801 | `			 * (only reached once padding is actually required). */` |
|      3 | 7802 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - | 7803 | `				"str_pad(): Argument #3 ($pad_string) must not be empty");` |
|      - | 7804 | `		}` |
|      5 | 7805 | `		if( nArg > 3 ){` |
|      - | 7806 | `			/* Padd type */` |
|      5 | 7807 | `			iType = ph7_value_to_int(apArg[3]);` |
|      5 | 7808 | `			if( iType != 0 /* STR_PAD_LEFT */ && iType != 2 /* STR_PAD_BOTH */ ){` |
|      3 | 7809 | `				iType = 1 ; /* STR_PAD_RIGHT */` |
|      1 | 7810 | `			}` |
|      2 | 7811 | `		}` |
|      2 | 7812 | `	}` |
|      5 | 7813 | `	iDiv = 1;` |
|      5 | 7814 | `	if( iType == 2 ){` |
|    ! 0 | 7815 | `		iDiv = 2; /* STR_PAD_BOTH */` |
|    ! 0 | 7816 | `	}` |
|      - | 7817 | `	/* Perform the requested operation */` |
|      5 | 7818 | `	if( iType == 0 /* STR_PAD_LEFT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      3 | 7819 | `		jPad = iStrpad;` |
|      5 | 7820 | `		for( i = 0 ; i < iPadlen/iDiv ; i += jPad ){` |
|      - | 7821 | `			/* Padding */` |
|      5 | 7822 | `			if( (int)ph7_context_result_buf_length(pCtx) + iLen + jPad >= iRealPad ){` |
|      3 | 7823 | `				break;` |
|      - | 7824 | `			}` |
|      3 | 7825 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7826 | `		}` |
|      3 | 7827 | `		if( iType == 0 /* STR_PAD_LEFT */ ){` |
|      5 | 7828 | `			while( (int)ph7_context_result_buf_length(pCtx) + iLen < iRealPad ){` |
|      3 | 7829 | `				jPad = iRealPad - (iLen + (int)ph7_context_result_buf_length(pCtx) );` |
|      3 | 7830 | `				if( jPad > iStrpad ){` |
|    ! 0 | 7831 | `					jPad = iStrpad;` |
|    ! 0 | 7832 | `				}` |
|      3 | 7833 | `				if( jPad < 1){` |
|    ! 0 | 7834 | `					break;` |
|      - | 7835 | `				}` |
|      3 | 7836 | `				if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 7837 | `			}` |
|      1 | 7838 | `		}` |
|      1 | 7839 | `	}` |
|      5 | 7840 | `	if( iLen > 0 ){` |
|      - | 7841 | `		/* Append the input string */` |
|      5 | 7842 | `		if( ph7_result_string(pCtx,zIn,iLen) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7843 | `	}` |
|      5 | 7844 | `	if( iType == 1 /* STR_PAD_RIGHT */ \|\| iType == 2 /* STR_PAD_BOTH */ ){` |
|      5 | 7845 | `		for( i = 0 ; i < iPadlen/iDiv ; i += iStrpad ){` |
|      - | 7846 | `			/* Padding */` |
|      5 | 7847 | `			if( (int)ph7_context_result_buf_length(pCtx) + iStrpad >= iRealPad ){` |
|      3 | 7848 | `				break;` |
|      - | 7849 | `			}` |
|      3 | 7850 | `			if( ph7_result_string(pCtx,zPad,iStrpad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      2 | 7851 | `		}` |
|      5 | 7852 | `		while( (int)ph7_context_result_buf_length(pCtx) < iRealPad ){` |
|      3 | 7853 | `			jPad = iRealPad - (int)ph7_context_result_buf_length(pCtx);` |
|      3 | 7854 | `			if( jPad > iStrpad ){` |
|    ! 0 | 7855 | `				jPad = iStrpad;` |
|    ! 0 | 7856 | `			}` |
|      3 | 7857 | `			if( jPad < 1){` |
|    ! 0 | 7858 | `				break;` |
|      - | 7859 | `			}` |
|      3 | 7860 | `			if( ph7_result_string(pCtx,zPad,jPad) != SXRET_OK ){ return PH7_ContextMemoryError(pCtx); }` |
|      1 | 7861 | `		}` |
|      1 | 7862 | `	}` |
|      5 | 7863 | `	return PH7_OK;` |
|      6 | 7864 | `}` |
|      - | 7865 | `/*` |
|      - | 7866 | ` * String replacement private data.` |
|      - | 7867 | ` */` |
|      - | 7868 | `typedef struct str_replace_data str_replace_data;` |
|      - | 7869 | `struct str_replace_data` |
|      - | 7870 | `{` |
|      - | 7871 | `	/* Used by the str_replace family to collect the search/replace arguments. */` |
|      - | 7872 | `	SySet *pCollector;  /* Argument collector*/` |
|      - | 7873 | `	ph7_context *pCtx;  /* Call context */` |
|      - | 7874 | `	sxi32 rc;           /* Carries an allocation failure (SXERR_MEM) out of a walker */` |
|      - | 7875 | `};` |
|      - | 7876 | `/*` |
|      - | 7877 | ` * Remove a substring.` |
|      - | 7878 | ` */` |
|      - | 7879 | `#define STRDEL(SRC,SLEN,OFFT,ILEN){\` |
|      - | 7880 | `	for(;;){\` |
|      - | 7881 | `		if( OFFT + ILEN >= SLEN ) { break; }\` |
|      - | 7882 | `		SRC[OFFT] = SRC[OFFT+ILEN];\` |
|      - | 7883 | `		++OFFT;\` |
|      - | 7884 | `	}\` |
|      - | 7885 | `}` |
|      - | 7886 | `/*` |
|      - | 7887 | ` * Shift right and insert algorithm.` |
|      - | 7888 | ` */` |
|      - | 7889 | `#define SHIFTRANDINSERT(SRC,LEN,OFFT,ENTRY,ELEN){\` |
|      - | 7890 | `		sxu32 INLEN = LEN - OFFT;\` |
|      - | 7891 | `		for(;;){\` |
|      - | 7892 | `			if( LEN > 0 ){ LEN--; }\` |
|      - | 7893 | `			if(INLEN < 1 ) { break; }\` |
|      - | 7894 | `			SRC[LEN + ELEN] = SRC[LEN];\` |
|      - | 7895 | `			--INLEN; \` |
|      - | 7896 | `		}\` |
|      - | 7897 | `		for(;;){\` |
|      - | 7898 | `				if(ELEN < 1) { break; }\` |
|      - | 7899 | `				SRC[OFFT] = ENTRY[0];\` |
|      - | 7900 | `				OFFT++;\` |
|      - | 7901 | `				ENTRY++;\` |
|      - | 7902 | `				--ELEN;\` |
|      - | 7903 | `		}\` |
|      - | 7904 | `}` |
|      - | 7905 | `/*` |
|      - | 7906 | ` * Replace all occurrences of the search string at offset (nOfft) with the given` |
|      - | 7907 | ` * replacement string [i.e: zReplace].` |
|      - | 7908 | ` */` |
|     46 | 7909 | `static int StringReplace(SyBlob *pWorker,sxu32 nOfft,int nLen,const char *zReplace,int nReplen)` |
|      1 | 7910 | `{` |
|     47 | 7911 | `	char *zInput = (char *)SyBlobData(pWorker);` |
|      - | 7912 | `	sxu32 n,m;` |
|     47 | 7913 | `	n = SyBlobLength(pWorker);` |
|     47 | 7914 | `	m = nOfft;` |
|      - | 7915 | `	/* Delete the old entry */` |
|   6573 | 7916 | `	STRDEL(zInput,n,m,nLen);` |
|     47 | 7917 | `	SyBlobLength(pWorker) -= nLen;` |
|     47 | 7918 | `	if( nReplen > 0 ){` |
|     41 | 7919 | `		sxi32 iRep = nReplen;` |
|      - | 7920 | `		sxi32 rc;` |
|      - | 7921 | `		/*` |
|      - | 7922 | `		 * Make sure the working buffer is big enough to hold the replacement` |
|      - | 7923 | `		 * string.` |
|      - | 7924 | `		 */` |
|     41 | 7925 | `		rc = SyBlobAppend(pWorker,0/* Grow without an append operation*/,(sxu32)nReplen);` |
|     41 | 7926 | `		if( rc != SXRET_OK ){` |
|      - | 7927 | `			/* Propagate the allocation failure so the caller can raise a fatal` |
|      - | 7928 | `			 * instead of returning a partially-replaced string as success. */` |
|    ! 0 | 7929 | `			return rc;` |
|      - | 7930 | `		}` |
|      - | 7931 | `		/* Perform the insertion now */` |
|     41 | 7932 | `		zInput = (char *)SyBlobData(pWorker);` |
|     41 | 7933 | `		n = SyBlobLength(pWorker);` |
|   6357 | 7934 | `		SHIFTRANDINSERT(zInput,n,nOfft,zReplace,iRep);` |
|     41 | 7935 | `		SyBlobLength(pWorker) += nReplen;` |
|     20 | 7936 | `	}` |
|     47 | 7937 | `	return SXRET_OK;` |
|     24 | 7938 | `}` |
|      - | 7939 | `/*` |
|      - | 7940 | ` * The following walker callback is invoked by the str_rplace() function inorder` |
|      - | 7941 | ` * to collect search/replace string.` |
|      - | 7942 | ` * This callback is invoked only if the given argument is of type array.` |
|      - | 7943 | ` */` |
|     26 | 7944 | `static int StrReplaceWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 7945 | `{` |
|     27 | 7946 | `	str_replace_data *pRep = (str_replace_data *)pUserData;` |
|      - | 7947 | `	SyString sWorker;` |
|      - | 7948 | `	const char *zIn;` |
|      - | 7949 | `	int nByte;` |
|      - | 7950 | `	/* Extract a string representation of the given argument */` |
|     27 | 7951 | `	zIn = ph7_value_to_string(pData,&nByte);` |
|     27 | 7952 | `	SyStringInitFromBuf(&sWorker,0,0);` |
|     27 | 7953 | `	if( nByte > 0 ){` |
|      - | 7954 | `		char *zDup;` |
|      - | 7955 | `		/* Duplicate the chunk */` |
|     25 | 7956 | `		zDup = (char *)ph7_context_alloc_chunk(pRep->pCtx,(unsigned int)nByte,FALSE,` |
|      - | 7957 | `			TRUE /* Release the chunk automatically,upon this context is destroyd */` |
|      - | 7958 | `			);` |
|     25 | 7959 | `		if( zDup == 0 ){` |
|      - | 7960 | `			/* Allocation failure: carry it out and stop the walk so the caller` |
|      - | 7961 | `			 * raises a fatal instead of silently dropping a search/replace term. */` |
|    ! 0 | 7962 | `			pRep->rc = SXERR_MEM;` |
|    ! 0 | 7963 | `			return SXERR_MEM;` |
|      - | 7964 | `		}` |
|     25 | 7965 | `		SyMemcpy(zIn,zDup,(sxu32)nByte);` |
|      - | 7966 | `		/* Save the chunk */` |
|     25 | 7967 | `		SyStringInitFromBuf(&sWorker,zDup,nByte);` |
|     12 | 7968 | `	}` |
|      - | 7969 | `	/* Save for later processing */` |
|     27 | 7970 | `	SySetPut(pRep->pCollector,(const void *)&sWorker);` |
|      - | 7971 | `	/* All done */` |
|     13 | 7972 | `	SXUNUSED(pKey); /* cc warning */` |
|     27 | 7973 | `	return PH7_OK;` |
|     14 | 7974 | `}` |
|      - | 7975 | `/*` |
|      - | 7976 | ` * mixed str_replace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 7977 | ` * mixed str_ireplace(mixed $search,mixed $replace,mixed $subject[,int &$count ])` |
|      - | 7978 | ` *  Replace all occurrences of the search string with the replacement string.` |
|      - | 7979 | ` * Parameters` |
|      - | 7980 | ` *  If search and replace are arrays, then str_replace() takes a value from each` |
|      - | 7981 | ` *  array and uses them to search and replace on subject. If replace has fewer values` |
|      - | 7982 | ` *  than search, then an empty string is used for the rest of replacement values.` |
|      - | 7983 | ` *  If search is an array and replace is a string, then this replacement string is used` |
|      - | 7984 | ` *  for every value of search. The converse would not make sense, though.` |
|      - | 7985 | ` *  If search or replace are arrays, their elements are processed first to last.` |
|      - | 7986 | ` * $search` |
|      - | 7987 | ` *  The value being searched for, otherwise known as the needle. An array may be used` |
|      - | 7988 | ` *  to designate multiple needles.` |
|      - | 7989 | ` * $replace` |
|      - | 7990 | ` *  The replacement value that replaces found search values. An array may be used` |
|      - | 7991 | ` *  to designate multiple replacements.` |
|      - | 7992 | ` * $subject` |
|      - | 7993 | ` *  The string or array being searched and replaced on, otherwise known as the haystack.` |
|      - | 7994 | ` *  If subject is an array, then the search and replace is performed with every entry` |
|      - | 7995 | ` *  of subject, and the return value is an array as well.` |
|      - | 7996 | ` * $count (Not used)` |
|      - | 7997 | ` *  If passed, this will be set to the number of replacements performed.` |
|      - | 7998 | ` * Return` |
|      - | 7999 | ` * This function returns a string or an array with the replaced values.` |
|      - | 8000 | ` */` |
|  29288 | 8001 | `static int PH7_builtin_str_replace(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 8002 | `{` |
|      - | 8003 | `	SyString sTemp,*pSearch,*pReplace;` |
|      - | 8004 | `	ProcStringMatch xMatch;` |
|      - | 8005 | `	const char *zIn,*zFunc;` |
|      - | 8006 | `	str_replace_data sRep;` |
|      - | 8007 | `	SyBlob sWorker;` |
|      - | 8008 | `	SySet sReplace;` |
|      - | 8009 | `	SySet sSearch;` |
|      - | 8010 | `	int rep_str;` |
|      - | 8011 | `	int nByte;` |
|      - | 8012 | `	sxi32 rc;` |
|  29293 | 8013 | `	if( nArg < 3 ){` |
|      - | 8014 | `		/* Missing/Invalid arguments,return null */` |
|    ! 0 | 8015 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8016 | `		return PH7_OK;` |
|      - | 8017 | `	}` |
|      - | 8018 | `	/* Initialize fields */` |
|  29293 | 8019 | `	SySetInit(&sSearch,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  29293 | 8020 | `	SySetInit(&sReplace,&pCtx->pVm->sAllocator,sizeof(SyString));` |
|  29293 | 8021 | `	SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|  29293 | 8022 | `	SyZero(&sRep,sizeof(str_replace_data));` |
|  29293 | 8023 | `	sRep.pCtx = pCtx;` |
|  29293 | 8024 | `	sRep.pCollector = &sSearch;` |
|  29293 | 8025 | `	rep_str = 0;` |
|      - | 8026 | `	/* Extract the subject */` |
|  29293 | 8027 | `	zIn = ph7_value_to_string(apArg[2],&nByte);` |
|  29293 | 8028 | `	if( nByte < 1 ){` |
|      - | 8029 | `		/* Nothing to replace,return the empty string */` |
|     29 | 8030 | `		ph7_result_string(pCtx,"",0);` |
|     29 | 8031 | `		return PH7_OK;` |
|      - | 8032 | `	}` |
|      - | 8033 | `	/* Copy the subject */` |
|  29265 | 8034 | `	SyBlobAppend(&sWorker,(const void *)zIn,(sxu32)nByte);` |
|      - | 8035 | `	/* Search string */` |
|  29265 | 8036 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - | 8037 | `		/* Collect search string */` |
|      9 | 8038 | `		ph7_array_walk(apArg[0],StrReplaceWalker,&sRep);` |
|      5 | 8039 | `	}else{` |
|      - | 8040 | `		/* Single pattern */` |
|  29257 | 8041 | `		zIn = ph7_value_to_string(apArg[0],&nByte);` |
|  29257 | 8042 | `		if( nByte < 1 ){` |
|      - | 8043 | `			/* Return the subject untouched since no search string is available */` |
|      5 | 8044 | `			ph7_result_value(pCtx,apArg[2]/* Subject as thrird argument*/);` |
|      5 | 8045 | `			return PH7_OK;` |
|      - | 8046 | `		}` |
|  29253 | 8047 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 8048 | `		/* Save for later processing */` |
|  29253 | 8049 | `		SySetPut(&sSearch,(const void *)&sTemp);` |
|      - | 8050 | `	}` |
|      - | 8051 | `	/* Replace string */` |
|  29261 | 8052 | `	if( ph7_value_is_array(apArg[1]) ){` |
|      - | 8053 | `		/* Collect replace string */` |
|      7 | 8054 | `		sRep.pCollector = &sReplace;` |
|      7 | 8055 | `		ph7_array_walk(apArg[1],StrReplaceWalker,&sRep);` |
|      4 | 8056 | `	}else{` |
|      - | 8057 | `		/* Single needle */` |
|  29255 | 8058 | `		zIn = ph7_value_to_string(apArg[1],&nByte);` |
|  29255 | 8059 | `		rep_str = 1;` |
|  29255 | 8060 | `		SyStringInitFromBuf(&sTemp,zIn,nByte);` |
|      - | 8061 | `		/* Save for later processing */` |
|  29255 | 8062 | `		SySetPut(&sReplace,(const void *)&sTemp);` |
|      - | 8063 | `	}` |
|      - | 8064 | `	/* Surface a collector allocation failure (StrReplaceWalker) as a fatal */` |
|  29261 | 8065 | `	if( sRep.rc != SXRET_OK ){` |
|    ! 0 | 8066 | `		SySetRelease(&sSearch);` |
|    ! 0 | 8067 | `		SySetRelease(&sReplace);` |
|    ! 0 | 8068 | `		SyBlobRelease(&sWorker);` |
|    ! 0 | 8069 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8070 | `	}` |
|      - | 8071 | `	/* Reset loop cursors */` |
|  29261 | 8072 | `	SySetResetCursor(&sSearch);` |
|  29261 | 8073 | `	SySetResetCursor(&sReplace);` |
|  29261 | 8074 | `	pReplace = pSearch = 0; /* cc warning */` |
|  29261 | 8075 | `	SyStringInitFromBuf(&sTemp,"",0);` |
|      - | 8076 | `	/* Extract function name */` |
|  29261 | 8077 | `	zFunc = ph7_function_name(pCtx);` |
|      - | 8078 | `	/* Set the default pattern match routine */` |
|  29261 | 8079 | `	xMatch = SyBlobSearch;` |
|  29261 | 8080 | `	if( SyStrncmp(zFunc,"str_ireplace",sizeof("str_ireplace") - 1) ==  0 ){` |
|      - | 8081 | `		/* Case insensitive pattern match */` |
|     11 | 8082 | `		xMatch = iPatternMatch;` |
|      5 | 8083 | `	}` |
|      - | 8084 | `	/* Start the replace process */` |
|  58525 | 8085 | `	while( SXRET_OK == SySetGetNextEntry(&sSearch,(void **)&pSearch) ){` |
|      - | 8086 | `		sxu32 nCount,nOfft;` |
|  29269 | 8087 | `		if( pSearch->nByte <  1 ){` |
|      - | 8088 | `			/* Empty string,ignore */` |
|      3 | 8089 | `			continue;` |
|      - | 8090 | `		}` |
|      - | 8091 | `		/* Extract the replace string */` |
|  29267 | 8092 | `		if( rep_str ){` |
|  29257 | 8093 | `			pReplace = (SyString *)SySetPeek(&sReplace);` |
|  14631 | 8094 | `		}else{` |
|     11 | 8095 | `			if( SXRET_OK != SySetGetNextEntry(&sReplace,(void **)&pReplace) ){` |
|      - | 8096 | `				/* Sepecial case when 'replace set' has fewer values than the search set.` |
|      - | 8097 | `				 * An empty string is used for the rest of replacement values` |
|      - | 8098 | `				 */` |
|      3 | 8099 | `				pReplace = 0;` |
|      1 | 8100 | `			}` |
|      - | 8101 | `		}` |
|  29267 | 8102 | `		if( pReplace == 0 ){` |
|      - | 8103 | `			/* Use an empty string instead */` |
|      3 | 8104 | `			pReplace = &sTemp;` |
|      1 | 8105 | `		}` |
|  29267 | 8106 | `		nOfft = nCount = 0;` |
|  14654 | 8107 | `		for(;;){` |
|  29313 | 8108 | `			if( nCount >= SyBlobLength(&sWorker) ){` |
|     13 | 8109 | `				break;` |
|      - | 8110 | `			}` |
|      - | 8111 | `			/* Perform a pattern lookup */` |
|  43949 | 8112 | `			rc = xMatch(SyBlobDataAt(&sWorker,nCount),SyBlobLength(&sWorker) - nCount,(const void *)pSearch->zString,` |
|  29296 | 8113 | `				pSearch->nByte,&nOfft);` |
|  29301 | 8114 | `			if( rc != SXRET_OK ){` |
|      - | 8115 | `				/* Pattern not found */` |
|  29255 | 8116 | `				break;` |
|      - | 8117 | `			}` |
|      - | 8118 | `			/* Perform the replace operation */` |
|     47 | 8119 | `			rc = StringReplace(&sWorker,nCount+nOfft,(int)pSearch->nByte,pReplace->zString,(int)pReplace->nByte);` |
|     47 | 8120 | `			if( rc != SXRET_OK ){` |
|      - | 8121 | `				/* Allocation failure: surface a fatal instead of a partial result */` |
|    ! 0 | 8122 | `				SySetRelease(&sSearch);` |
|    ! 0 | 8123 | `				SySetRelease(&sReplace);` |
|    ! 0 | 8124 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8125 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8126 | `			}` |
|      - | 8127 | `			/* Increment offset counter */` |
|     47 | 8128 | `			nCount += nOfft + pReplace->nByte;` |
|      1 | 8129 | `		}` |
|      5 | 8130 | `	}` |
|      - | 8131 | `	/* All done,clean-up the mess left behind */` |
|  29261 | 8132 | `	rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),(int)SyBlobLength(&sWorker));` |
|  29261 | 8133 | `	SySetRelease(&sSearch);` |
|  29261 | 8134 | `	SySetRelease(&sReplace);` |
|  29261 | 8135 | `	SyBlobRelease(&sWorker);` |
|  29261 | 8136 | `	if( rc != PH7_OK ){` |
|    ! 0 | 8137 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8138 | `	}` |
|  29261 | 8139 | `	return PH7_OK;` |
|  14649 | 8140 | `}` |
|      - | 8141 | `/*` |
|      - | 8142 | ` * strtr() array form: a single (key,value) pair copied out of the replace_pairs` |
|      - | 8143 | ` * array. The bytes are owned by a persistent pool (see strtr_collect) rather than` |
|      - | 8144 | ` * the transient walker values, which HashmapWalk releases after each callback, so` |
|      - | 8145 | ` * we store byte offsets into that pool instead of raw pointers.` |
|      - | 8146 | ` */` |
|      - | 8147 | `typedef struct strtr_entry strtr_entry;` |
|      - | 8148 | `struct strtr_entry` |
|      - | 8149 | `{` |
|      - | 8150 | `	sxu32 nKeyOfft; /* Offset of the search key inside the pool */` |
|      - | 8151 | `	sxu32 nKeyLen;  /* Length of the search key */` |
|      - | 8152 | `	sxu32 nValOfft; /* Offset of the replacement inside the pool */` |
|      - | 8153 | `	sxu32 nValLen;  /* Length of the replacement */` |
|      - | 8154 | `};` |
|      - | 8155 | `typedef struct strtr_collect strtr_collect;` |
|      - | 8156 | `struct strtr_collect` |
|      - | 8157 | `{` |
|      - | 8158 | `	SyBlob *pPool;  /* Byte pool holding copied key + value bytes */` |
|      - | 8159 | `	SySet  *pTable; /* Set of strtr_entry (parallel offsets into pPool) */` |
|      - | 8160 | `	sxi32   rc;     /* Carries an allocation failure (SXERR_MEM) out of the walker */` |
|      - | 8161 | `};` |
|      - | 8162 | `/*` |
|      - | 8163 | ` * Collect one replace_pairs entry into the persistent pool/offset table.` |
|      - | 8164 | ` * PHP coerces both the key and the value to string (an integer key becomes its` |
|      - | 8165 | ` * decimal form) and ignores an empty-string key.` |
|      - | 8166 | ` */` |
|     20 | 8167 | `static int StrtrCollectWalker(ph7_value *pKey,ph7_value *pData,void *pUserData)` |
|      1 | 8168 | `{` |
|     21 | 8169 | `	strtr_collect *pCol = (strtr_collect *)pUserData;` |
|      - | 8170 | `	const char *zKey,*zVal;` |
|      - | 8171 | `	strtr_entry sEnt;` |
|      - | 8172 | `	int nKey,nVal;` |
|     21 | 8173 | `	zKey = ph7_value_to_string(pKey,&nKey);` |
|     21 | 8174 | `	if( nKey < 1 ){` |
|      - | 8175 | `		/* PHP ignores an empty-string key (it also emits a warning we do not replicate). */` |
|    ! 0 | 8176 | `		return PH7_OK;` |
|      - | 8177 | `	}` |
|     21 | 8178 | `	zVal = ph7_value_to_string(pData,&nVal);` |
|     21 | 8179 | `	sEnt.nKeyOfft = SyBlobLength(pCol->pPool);` |
|     21 | 8180 | `	sEnt.nKeyLen  = (sxu32)nKey;` |
|     21 | 8181 | `	if( SyBlobAppend(pCol->pPool,(const void *)zKey,(sxu32)nKey) != SXRET_OK ){` |
|    ! 0 | 8182 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8183 | `		return SXERR_ABORT;` |
|      - | 8184 | `	}` |
|     21 | 8185 | `	sEnt.nValOfft = SyBlobLength(pCol->pPool);` |
|     21 | 8186 | `	sEnt.nValLen  = (sxu32)nVal;` |
|     21 | 8187 | `	if( nVal > 0 && SyBlobAppend(pCol->pPool,(const void *)zVal,(sxu32)nVal) != SXRET_OK ){` |
|    ! 0 | 8188 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8189 | `		return SXERR_ABORT;` |
|      - | 8190 | `	}` |
|     21 | 8191 | `	if( SySetPut(pCol->pTable,(const void *)&sEnt) != SXRET_OK ){` |
|    ! 0 | 8192 | `		pCol->rc = SXERR_MEM;` |
|    ! 0 | 8193 | `		return SXERR_ABORT;` |
|      - | 8194 | `	}` |
|     21 | 8195 | `	return PH7_OK;` |
|     11 | 8196 | `}` |
|      - | 8197 | `/*` |
|      - | 8198 | ` * string strtr(string $str,string $from,string $to)` |
|      - | 8199 | ` * string strtr(string $str,array $replace_pairs)` |
|      - | 8200 | ` *  Translate characters or replace substrings.` |
|      - | 8201 | ` * Parameters` |
|      - | 8202 | ` *  $str` |
|      - | 8203 | ` *  The string being translated.` |
|      - | 8204 | ` * $from` |
|      - | 8205 | ` *  The string being translated to to.` |
|      - | 8206 | ` * $to` |
|      - | 8207 | ` *  The string replacing from.` |
|      - | 8208 | ` * $replace_pairs` |
|      - | 8209 | ` *  The replace_pairs parameter may be used instead of to and` |
|      - | 8210 | ` *  from, in which case it's an array in the form array('from' => 'to', ...).` |
|      - | 8211 | ` * Return` |
|      - | 8212 | ` *  The translated string.` |
|      - | 8213 | ` *  If replace_pairs contains a key which is an empty string (""), FALSE will be returned.` |
|      - | 8214 | ` */` |
|     12 | 8215 | `static int PH7_builtin_strtr(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8216 | `{` |
|      - | 8217 | `	const char *zIn;` |
|      - | 8218 | `	int nLen;` |
|     13 | 8219 | `	if( nArg < 1 ){` |
|      - | 8220 | `		/* Nothing to replace,return FALSE */` |
|    ! 0 | 8221 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8222 | `		return PH7_OK;` |
|      - | 8223 | `	}` |
|     13 | 8224 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     13 | 8225 | `	if( nLen < 1 \|\| nArg < 2 ){` |
|      - | 8226 | `		/* Invalid arguments */` |
|    ! 0 | 8227 | `		ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8228 | `		return PH7_OK;` |
|      - | 8229 | `	}` |
|     18 | 8230 | `	if( nArg == 2 && ph7_value_is_array(apArg[1]) ){` |
|      - | 8231 | `		strtr_collect sCol;` |
|      - | 8232 | `		SyBlob sPool,sWorker;` |
|      - | 8233 | `		SySet sTable;` |
|      - | 8234 | `		const char *zPool;` |
|      - | 8235 | `		strtr_entry *pEnt;` |
|      - | 8236 | `		sxi32 rc;` |
|      - | 8237 | `		int i,iRun;` |
|      - | 8238 | `		/*` |
|      - | 8239 | `		 * PHP's array-form strtr is a single left-to-right pass over the subject:` |
|      - | 8240 | `		 * at every position it substitutes the LONGEST replace_pairs key that` |
|      - | 8241 | `		 * matches there, then advances past the key (replacements are never` |
|      - | 8242 | `		 * rescanned). It is not a sequential per-key global replace. First copy` |
|      - | 8243 | `		 * the pairs into a persistent pool, then run that scan.` |
|      - | 8244 | `		 */` |
|     11 | 8245 | `		SyBlobInit(&sPool,&pCtx->pVm->sAllocator);` |
|     11 | 8246 | `		SyBlobInit(&sWorker,&pCtx->pVm->sAllocator);` |
|     11 | 8247 | `		SySetInit(&sTable,&pCtx->pVm->sAllocator,sizeof(strtr_entry));` |
|     11 | 8248 | `		sCol.pPool  = &sPool;` |
|     11 | 8249 | `		sCol.pTable = &sTable;` |
|     11 | 8250 | `		sCol.rc     = SXRET_OK;` |
|     11 | 8251 | `		ph7_array_walk(apArg[1],StrtrCollectWalker,&sCol);` |
|     11 | 8252 | `		if( sCol.rc != SXRET_OK ){` |
|      - | 8253 | `			/* Allocation failure while collecting the pairs: surface a fatal */` |
|    ! 0 | 8254 | `			SyBlobRelease(&sPool);` |
|    ! 0 | 8255 | `			SyBlobRelease(&sWorker);` |
|    ! 0 | 8256 | `			SySetRelease(&sTable);` |
|    ! 0 | 8257 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 8258 | `		}` |
|      - | 8259 | `		/* The pool is now stable, so offsets can be resolved against its base. */` |
|     11 | 8260 | `		zPool = (const char *)SyBlobData(&sPool);` |
|     11 | 8261 | `		rc = SXRET_OK;` |
|     11 | 8262 | `		iRun = 0; /* Start of the pending run of unmatched bytes copied verbatim. */` |
|     43 | 8263 | `		for( i = 0 ; i < nLen ; ){` |
|     33 | 8264 | `			strtr_entry *pBest = 0;` |
|     33 | 8265 | `			sxu32 nBest = 0;` |
|      - | 8266 | `			/* Pick the longest key that matches at the current position. */` |
|     33 | 8267 | `			SySetResetCursor(&sTable);` |
|     97 | 8268 | `			while( SXRET_OK == SySetGetNextEntry(&sTable,(void **)&pEnt) ){` |
|     64 | 8269 | `				if( pEnt->nKeyLen > nBest` |
|     60 | 8270 | `					&& pEnt->nKeyLen <= (sxu32)(nLen - i)` |
|     56 | 8271 | `					&& SyMemcmp(zPool + pEnt->nKeyOfft,zIn + i,pEnt->nKeyLen) == 0 ){` |
|     29 | 8272 | `					nBest = pEnt->nKeyLen;` |
|     29 | 8273 | `					pBest = pEnt;` |
|     14 | 8274 | `				}` |
|      1 | 8275 | `			}` |
|     33 | 8276 | `			if( pBest == 0 ){` |
|      - | 8277 | `				/* No key here: extend the literal run and copy it in one shot later. */` |
|      9 | 8278 | `				i++;` |
|      9 | 8279 | `				continue;` |
|      - | 8280 | `			}` |
|      - | 8281 | `			/* Flush the pending literal run, then the replacement. */` |
|     25 | 8282 | `			if( i > iRun ){` |
|      5 | 8283 | `				rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(i - iRun));` |
|      2 | 8284 | `			}` |
|     25 | 8285 | `			if( rc == SXRET_OK && pBest->nValLen > 0 ){` |
|     25 | 8286 | `				rc = SyBlobAppend(&sWorker,zPool + pBest->nValOfft,pBest->nValLen);` |
|     12 | 8287 | `			}` |
|     25 | 8288 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8289 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 8290 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8291 | `				SySetRelease(&sTable);` |
|    ! 0 | 8292 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8293 | `			}` |
|     25 | 8294 | `			i += (int)pBest->nKeyLen;` |
|     25 | 8295 | `			iRun = i;` |
|      1 | 8296 | `		}` |
|      - | 8297 | `		/* Flush the trailing literal run. */` |
|     11 | 8298 | `		if( nLen > iRun ){` |
|      3 | 8299 | `			rc = SyBlobAppend(&sWorker,&zIn[iRun],(sxu32)(nLen - iRun));` |
|      3 | 8300 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8301 | `				SyBlobRelease(&sPool);` |
|    ! 0 | 8302 | `				SyBlobRelease(&sWorker);` |
|    ! 0 | 8303 | `				SySetRelease(&sTable);` |
|    ! 0 | 8304 | `				return PH7_ContextMemoryError(pCtx);` |
|      - | 8305 | `			}` |
|      1 | 8306 | `		}` |
|      - | 8307 | `		/* All done, return the result string */` |
|     16 | 8308 | `		rc = ph7_result_string(pCtx,(const char *)SyBlobData(&sWorker),` |
|     10 | 8309 | `			(int)SyBlobLength(&sWorker)); /* Will make it's own copy */` |
|      - | 8310 | `		/* Clean-up */` |
|     11 | 8311 | `		SyBlobRelease(&sPool);` |
|     11 | 8312 | `		SyBlobRelease(&sWorker);` |
|     11 | 8313 | `		SySetRelease(&sTable);` |
|     11 | 8314 | `		if( rc != PH7_OK ){` |
|    ! 0 | 8315 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 8316 | `		}` |
|      6 | 8317 | `	}else{` |
|      - | 8318 | `		int i,flen,tlen,c,iOfft;` |
|      - | 8319 | `		const char *zFrom,*zTo;` |
|      3 | 8320 | `		if( nArg < 3 ){` |
|      - | 8321 | `			/* Nothing to replace */` |
|    ! 0 | 8322 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8323 | `			return PH7_OK;` |
|      - | 8324 | `		}` |
|      - | 8325 | `		/* Extract given arguments */` |
|      3 | 8326 | `		zFrom = ph7_value_to_string(apArg[1],&flen);` |
|      3 | 8327 | `		zTo = ph7_value_to_string(apArg[2],&tlen);` |
|      3 | 8328 | `		if( flen < 1 \|\| tlen < 1 ){` |
|      - | 8329 | `			/* Nothing to replace */` |
|    ! 0 | 8330 | `			ph7_result_string(pCtx,zIn,nLen);` |
|    ! 0 | 8331 | `			return PH7_OK;` |
|      - | 8332 | `		}` |
|      - | 8333 | `		/* Start the replace process */` |
|     13 | 8334 | `		for( i = 0 ; i < nLen ; ++i ){` |
|     11 | 8335 | `			c = zIn[i];` |
|     11 | 8336 | `			if( CheckMask(c,zFrom,flen,&iOfft) ){` |
|      5 | 8337 | `				if ( iOfft < tlen ){` |
|      5 | 8338 | `					c = zTo[iOfft];` |
|      2 | 8339 | `				}` |
|      2 | 8340 | `			}` |
|     11 | 8341 | `			ph7_result_string(pCtx,(const char *)&c,(int)sizeof(char));` |
|      - | 8342 |  |
|      6 | 8343 | `		}` |
|      - | 8344 | `	}` |
|     13 | 8345 | `	return PH7_OK;` |
|      7 | 8346 | `}` |
|      - | 8347 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 8348 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 8349 | `/*` |
|      - | 8350 | ` * Parse an INI string.` |
|      - | 8351 |  |
|      - | 8352 | ` * According to wikipedia` |
|      - | 8353 | ` *  The INI file format is an informal standard for configuration files for some platforms or software.` |
|      - | 8354 | ` *  INI files are simple text files with a basic structure composed of "sections" and "properties".` |
|      - | 8355 | ` *  Format` |
|      - | 8356 | `*    Properties` |
|      - | 8357 | `*     The basic element contained in an INI file is the property. Every property has a name and a value` |
|      - | 8358 | `*     delimited by an equals sign (=). The name appears to the left of the equals sign.` |
|      - | 8359 | `*     Example:` |
|      - | 8360 | `*      name=value` |
|      - | 8361 | `*    Sections` |
|      - | 8362 | `*     Properties may be grouped into arbitrarily named sections. The section name appears on a line by itself` |
|      - | 8363 | `*     in square brackets ([ and ]). All properties after the section declaration are associated with that section.` |
|      - | 8364 | `*     There is no explicit "end of section" delimiter; sections end at the next section declaration` |
|      - | 8365 | `*     or the end of the file. Sections may not be nested.` |
|      - | 8366 | `*     Example:` |
|      - | 8367 | `*      [section]` |
|      - | 8368 | `*   Comments` |
|      - | 8369 | `*    Semicolons (;) at the beginning of the line indicate a comment. Comment lines are ignored.` |
|      - | 8370 | `* This function return an array holding parsed values on success.FALSE otherwise.` |
|      - | 8371 | `*/` |
|     12 | 8372 | `PH7_PRIVATE sxi32 PH7_ParseIniString(ph7_context *pCtx,const char *zIn,sxu32 nByte,int bProcessSection)` |
|      1 | 8373 | `{` |
|      - | 8374 | `	ph7_value *pCur,*pArray,*pSection,*pWorker,*pValue;` |
|     13 | 8375 | `	const char *zCur,*zEnd = &zIn[nByte];` |
|      - | 8376 | `	SyHashEntry *pEntry;` |
|      - | 8377 | `	SyString sEntry;` |
|      - | 8378 | `	SyHash sHash;` |
|      - | 8379 | `	int c;` |
|      - | 8380 | `	/* Create an empty array and worker variables */` |
|     13 | 8381 | `	pArray = ph7_context_new_array(pCtx);` |
|     13 | 8382 | `	pWorker = ph7_context_new_scalar(pCtx);` |
|     13 | 8383 | `	pValue = ph7_context_new_scalar(pCtx);` |
|     13 | 8384 | `	if( pArray == 0 \|\| pWorker == 0 \|\| pValue == 0){` |
|      - | 8385 | `		/* Out of memory: surface a fatal instead of returning FALSE */` |
|    ! 0 | 8386 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8387 | `	}` |
|     13 | 8388 | `	SyHashInit(&sHash,&pCtx->pVm->sAllocator,0,0);` |
|     13 | 8389 | `	pCur = pArray;` |
|      - | 8390 | `	/* Start the parse process */` |
|     21 | 8391 | `	for(;;){` |
|      - | 8392 | `		/* Ignore leading white spaces */` |
|     69 | 8393 | `		while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0])){` |
|     27 | 8394 | `			zIn++;` |
|      1 | 8395 | `		}` |
|     43 | 8396 | `		if( zIn >= zEnd ){` |
|      - | 8397 | `			/* No more input to process */` |
|     13 | 8398 | `			break;` |
|      - | 8399 | `		}` |
|     31 | 8400 | `		if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 8401 | `			/* Comment til the end of line */` |
|    ! 0 | 8402 | `			zIn++;` |
|    ! 0 | 8403 | `			while(zIn < zEnd && zIn[0] != '\n' ){` |
|    ! 0 | 8404 | `				zIn++;` |
|    ! 0 | 8405 | `			}` |
|    ! 0 | 8406 | `			continue;` |
|      - | 8407 | `		}` |
|      - | 8408 | `		/* Reset the string cursor of the working variable */` |
|     31 | 8409 | `		ph7_value_reset_string_cursor(pWorker);` |
|     31 | 8410 | `		if( zIn[0] == '[' ){` |
|      - | 8411 | `			/* Section: Extract the section name */` |
|      9 | 8412 | `			zIn++;` |
|      9 | 8413 | `			zCur = zIn;` |
|     73 | 8414 | `			while( zIn < zEnd && zIn[0] != ']' ){` |
|     65 | 8415 | `				zIn++;` |
|      1 | 8416 | `			}` |
|      9 | 8417 | `			if( zIn > zCur && bProcessSection ){` |
|      - | 8418 | `				/* Save the section name */` |
|      5 | 8419 | `				SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|      5 | 8420 | `				SyStringFullTrim(&sEntry);` |
|      5 | 8421 | `				ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|      5 | 8422 | `				if( sEntry.nByte > 0 ){` |
|      - | 8423 | `					/* Associate an array with the section */` |
|      5 | 8424 | `					pSection = ph7_context_new_array(pCtx);` |
|      5 | 8425 | `					if( pSection ){` |
|      5 | 8426 | `						ph7_array_add_elem(pArray,pWorker/*Section name*/,pSection);` |
|      5 | 8427 | `						pCur = pSection;` |
|      2 | 8428 | `					}` |
|      2 | 8429 | `				}` |
|      2 | 8430 | `			}` |
|      9 | 8431 | `			zIn++; /* Trailing square brackets ']' */` |
|      5 | 8432 | `		}else{` |
|      - | 8433 | `			ph7_value *pOldCur;` |
|      - | 8434 | `			int is_array;` |
|      - | 8435 | `			int iLen;` |
|      - | 8436 | `			/* Properties */` |
|     23 | 8437 | `			is_array = 0;` |
|     23 | 8438 | `			zCur = zIn;` |
|     23 | 8439 | `			iLen = 0; /* cc warning */` |
|     23 | 8440 | `			pOldCur = pCur;` |
|    155 | 8441 | `			while( zIn < zEnd && zIn[0] != '=' ){` |
|    133 | 8442 | `				if( zIn[0] == '[' && !is_array ){` |
|      - | 8443 | `					/* Array */` |
|    ! 0 | 8444 | `					iLen = (int)(zIn-zCur);` |
|    ! 0 | 8445 | `					is_array = 1;` |
|    ! 0 | 8446 | `					if( iLen > 0 ){` |
|    ! 0 | 8447 | `						ph7_value *pvArr = 0; /* cc warning */` |
|      - | 8448 | `						/* Query the hashtable */` |
|    ! 0 | 8449 | `						SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|    ! 0 | 8450 | `						SyStringFullTrim(&sEntry);` |
|    ! 0 | 8451 | `						pEntry = SyHashGet(&sHash,(const void *)sEntry.zString,sEntry.nByte);` |
|    ! 0 | 8452 | `						if( pEntry ){` |
|    ! 0 | 8453 | `							pvArr = (ph7_value *)SyHashEntryGetUserData(pEntry);` |
|    ! 0 | 8454 | `						}else{` |
|      - | 8455 | `							/* Create an empty array */` |
|    ! 0 | 8456 | `							pvArr = ph7_context_new_array(pCtx);` |
|    ! 0 | 8457 | `							if( pvArr ){` |
|      - | 8458 | `								/* Save the entry */` |
|    ! 0 | 8459 | `								SyHashInsert(&sHash,(const void *)sEntry.zString,sEntry.nByte,pvArr);` |
|      - | 8460 | `								/* Insert the entry */` |
|    ! 0 | 8461 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 8462 | `								ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|    ! 0 | 8463 | `								ph7_array_add_elem(pCur,pWorker,pvArr);` |
|    ! 0 | 8464 | `								ph7_value_reset_string_cursor(pWorker);` |
|    ! 0 | 8465 | `							}` |
|      - | 8466 | `						}` |
|    ! 0 | 8467 | `						if( pvArr ){` |
|    ! 0 | 8468 | `							pCur = pvArr;` |
|    ! 0 | 8469 | `						}` |
|    ! 0 | 8470 | `					}` |
|    ! 0 | 8471 | `					while ( zIn < zEnd && zIn[0] != ']' ){` |
|    ! 0 | 8472 | `						zIn++;` |
|    ! 0 | 8473 | `					}` |
|    ! 0 | 8474 | `				}` |
|    133 | 8475 | `				zIn++;` |
|      1 | 8476 | `			}` |
|     23 | 8477 | `			if( !is_array ){` |
|     23 | 8478 | `				iLen = (int)(zIn-zCur);` |
|     11 | 8479 | `			}` |
|      - | 8480 | `			/* Trim the key */` |
|     23 | 8481 | `			SyStringInitFromBuf(&sEntry,zCur,iLen);` |
|     39 | 8482 | `			SyStringFullTrim(&sEntry);` |
|     23 | 8483 | `			if( sEntry.nByte > 0 ){` |
|     23 | 8484 | `				if( !is_array ){` |
|      - | 8485 | `					/* Save the key name */` |
|     23 | 8486 | `					ph7_value_string(pWorker,sEntry.zString,(int)sEntry.nByte);` |
|     11 | 8487 | `				}` |
|      - | 8488 | `				/* extract key value */` |
|     23 | 8489 | `				ph7_value_reset_string_cursor(pValue);` |
|     23 | 8490 | `				zIn++; /* '=' */` |
|     39 | 8491 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && SyisSpace(zIn[0]) ){` |
|     17 | 8492 | `					zIn++;` |
|      1 | 8493 | `				}` |
|     23 | 8494 | `				if( zIn < zEnd ){` |
|     21 | 8495 | `					zCur = zIn;` |
|     21 | 8496 | `					c = zIn[0];` |
|     21 | 8497 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 8498 | `						zIn++;` |
|      - | 8499 | `						/* Delimit the value */` |
|    ! 0 | 8500 | `						while( zIn < zEnd ){` |
|    ! 0 | 8501 | `							if ( zIn[0] == c && zIn[-1] != '\\' ){` |
|    ! 0 | 8502 | `								break;` |
|      - | 8503 | `							}` |
|    ! 0 | 8504 | `							zIn++;` |
|    ! 0 | 8505 | `						}` |
|    ! 0 | 8506 | `						if( zIn < zEnd ){` |
|    ! 0 | 8507 | `							zIn++;` |
|    ! 0 | 8508 | `						}` |
|    ! 0 | 8509 | `					}else{` |
|    125 | 8510 | `						while( zIn < zEnd ){` |
|    123 | 8511 | `							if( zIn[0] == '\n' ){` |
|     19 | 8512 | `								if( zIn[-1] != '\\' ){` |
|     19 | 8513 | `									break;` |
|    ! 0 | 8514 | `								}` |
|    105 | 8515 | `							}else if( zIn[0] == ';' \|\| zIn[0] == '#' ){` |
|      - | 8516 | `								/* Inline comments */` |
|    ! 0 | 8517 | `								break;` |
|      - | 8518 | `							}` |
|    105 | 8519 | `							zIn++;` |
|      1 | 8520 | `						}` |
|      - | 8521 | `					}` |
|      - | 8522 | `					/* Trim the value */` |
|     21 | 8523 | `					SyStringInitFromBuf(&sEntry,zCur,(int)(zIn-zCur));` |
|     21 | 8524 | `					SyStringFullTrim(&sEntry);` |
|     21 | 8525 | `					if( c == '"' \|\| c == '\'' ){` |
|    ! 0 | 8526 | `						SyStringTrimLeadingChar(&sEntry,c);` |
|    ! 0 | 8527 | `						SyStringTrimTrailingChar(&sEntry,c);` |
|    ! 0 | 8528 | `					}` |
|     21 | 8529 | `					if( sEntry.nByte > 0 ){` |
|     21 | 8530 | `						ph7_value_string(pValue,sEntry.zString,(int)sEntry.nByte);` |
|     10 | 8531 | `					}` |
|      - | 8532 | `					/* Insert the key and it's value */` |
|     21 | 8533 | `					ph7_array_add_elem(pCur,is_array ? 0 /*Automatic index assign */: pWorker,pValue);` |
|     10 | 8534 | `				}` |
|     12 | 8535 | `			}else{` |
|    ! 0 | 8536 | `				while( zIn < zEnd && (unsigned char)zIn[0] < 0xc0 && ( SyisSpace(zIn[0]) \|\| zIn[0] == '=' ) ){` |
|    ! 0 | 8537 | `					zIn++;` |
|    ! 0 | 8538 | `				}` |
|      - | 8539 | `			}` |
|     23 | 8540 | `			pCur = pOldCur;` |
|      - | 8541 | `		}` |
|      1 | 8542 | `	}` |
|     13 | 8543 | `	SyHashRelease(&sHash);` |
|      - | 8544 | `	/* Return the parse of the INI string */` |
|     13 | 8545 | `	ph7_result_value(pCtx,pArray);` |
|     13 | 8546 | `	return SXRET_OK;` |
|      7 | 8547 | `}` |
|      - | 8548 | `/*` |
|      - | 8549 | ` * array parse_ini_string(string $ini[,bool $process_sections = false[,int $scanner_mode = INI_SCANNER_NORMAL ]])` |
|      - | 8550 | ` *  Parse a configuration string.` |
|      - | 8551 | ` * Parameters` |
|      - | 8552 | ` *  $ini` |
|      - | 8553 | ` *   The contents of the ini file being parsed.` |
|      - | 8554 | ` *  $process_sections` |
|      - | 8555 | ` *   By setting the process_sections parameter to TRUE, you get a multidimensional array, with the section names` |
|      - | 8556 | ` *   and settings included. The default for process_sections is FALSE.` |
|      - | 8557 | ` *  $scanner_mode (Not used)` |
|      - | 8558 | ` *   Can either be INI_SCANNER_NORMAL (default) or INI_SCANNER_RAW. If INI_SCANNER_RAW is supplied` |
|      - | 8559 | ` *   then option values will not be parsed.` |
|      - | 8560 | ` * Return` |
|      - | 8561 | ` *  The settings are returned as an associative array on success, and FALSE on failure.` |
|      - | 8562 | ` */` |
|     10 | 8563 | `static int PH7_builtin_parse_ini_string(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8564 | `{` |
|      - | 8565 | `	const char *zIni;` |
|      - | 8566 | `	int nByte;` |
|     11 | 8567 | `	if( nArg < 1 \|\| !ph7_value_is_string(apArg[0]) ){` |
|      - | 8568 | `		/* Missing/Invalid arguments,return FALSE*/` |
|    ! 0 | 8569 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8570 | `		return PH7_OK;` |
|      - | 8571 | `	}` |
|      - | 8572 | `	/* Extract the raw INI buffer */` |
|     11 | 8573 | `	zIni = ph7_value_to_string(apArg[0],&nByte);` |
|      - | 8574 | `	/* Process the INI buffer; propagate an OOM abort so the fatal actually halts */` |
|     11 | 8575 | `	return PH7_ParseIniString(pCtx,zIni,(sxu32)nByte,(nArg > 1) ? ph7_value_to_bool(apArg[1]) : 0);` |
|      6 | 8576 | `}` |
|      - | 8577 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 8578 |  |
|      - | 8579 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 8580 |  |
|      - | 8581 | `/*` |
|      - | 8582 | ` * Ctype Functions.` |
|      - | 8583 | ` * Status:` |
|      - | 8584 | ` *    Stable.` |
|      - | 8585 | ` */` |
|      - | 8586 | `/*` |
|      - | 8587 | ` * bool ctype_alnum(string $text)` |
|      - | 8588 | ` *  Checks if all of the characters in the provided string, text, are alphanumeric.` |
|      - | 8589 | ` * Parameters` |
|      - | 8590 | ` *  $text` |
|      - | 8591 | ` *   The tested string.` |
|      - | 8592 | ` * Return` |
|      - | 8593 | ` *   TRUE if every character in text is either a letter or a digit, FALSE otherwise.` |
|      - | 8594 | ` */` |
|     14 | 8595 | `static int PH7_builtin_ctype_alnum(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8596 | `{` |
|      - | 8597 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8598 | `	int nLen;` |
|     15 | 8599 | `	if( nArg < 1 ){` |
|      - | 8600 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8601 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8602 | `		return PH7_OK;` |
|      - | 8603 | `	}` |
|      - | 8604 | `	/* Extract the target string */` |
|     15 | 8605 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     15 | 8606 | `	zEnd = &zIn[nLen];` |
|     15 | 8607 | `	if( nLen < 1 ){` |
|      - | 8608 | `		/* Empty string,return FALSE */` |
|      3 | 8609 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8610 | `		return PH7_OK;` |
|      - | 8611 | `	}` |
|      - | 8612 | `	/* Perform the requested operation */` |
|     32 | 8613 | `	for(;;){` |
|     65 | 8614 | `		if( zIn >= zEnd ){` |
|      - | 8615 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8616 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8617 | `			return PH7_OK;` |
|      - | 8618 | `		}` |
|     57 | 8619 | `		if( !SyisAlphaNum(zIn[0]) ){` |
|      5 | 8620 | `			break;` |
|      - | 8621 | `		}` |
|      - | 8622 | `		/* Point to the next character */` |
|     53 | 8623 | `		zIn++;` |
|      1 | 8624 | `	}` |
|      - | 8625 | `	/* The test failed,return FALSE */` |
|      5 | 8626 | `	ph7_result_bool(pCtx,0);` |
|      5 | 8627 | `	return PH7_OK;` |
|      8 | 8628 | `}` |
|      - | 8629 | `/*` |
|      - | 8630 | ` * bool ctype_alpha(string $text)` |
|      - | 8631 | ` *  Checks if all of the characters in the provided string, text, are alphabetic.` |
|      - | 8632 | ` * Parameters` |
|      - | 8633 | ` *  $text` |
|      - | 8634 | ` *   The tested string.` |
|      - | 8635 | ` * Return` |
|      - | 8636 | ` *  TRUE if every character in text is a letter from the current locale, FALSE otherwise.` |
|      - | 8637 | ` */` |
|     16 | 8638 | `static int PH7_builtin_ctype_alpha(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8639 | `{` |
|      - | 8640 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8641 | `	int nLen;` |
|     17 | 8642 | `	if( nArg < 1 ){` |
|      - | 8643 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8644 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8645 | `		return PH7_OK;` |
|      - | 8646 | `	}` |
|      - | 8647 | `	/* Extract the target string */` |
|     17 | 8648 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8649 | `	zEnd = &zIn[nLen];` |
|     17 | 8650 | `	if( nLen < 1 ){` |
|      - | 8651 | `		/* Empty string,return FALSE */` |
|      3 | 8652 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8653 | `		return PH7_OK;` |
|      - | 8654 | `	}` |
|      - | 8655 | `	/* Perform the requested operation */` |
|     42 | 8656 | `	for(;;){` |
|     85 | 8657 | `		if( zIn >= zEnd ){` |
|      - | 8658 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8659 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8660 | `			return PH7_OK;` |
|      - | 8661 | `		}` |
|     77 | 8662 | `		if( !SyisAlpha(zIn[0]) ){` |
|      7 | 8663 | `			break;` |
|      - | 8664 | `		}` |
|      - | 8665 | `		/* Point to the next character */` |
|     71 | 8666 | `		zIn++;` |
|      1 | 8667 | `	}` |
|      - | 8668 | `	/* The test failed,return FALSE */` |
|      7 | 8669 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8670 | `	return PH7_OK;` |
|      9 | 8671 | `}` |
|      - | 8672 | `/*` |
|      - | 8673 | ` * bool ctype_cntrl(string $text)` |
|      - | 8674 | ` *  Checks if all of the characters in the provided string, text, are control characters.` |
|      - | 8675 | ` * Parameters` |
|      - | 8676 | ` *  $text` |
|      - | 8677 | ` *   The tested string.` |
|      - | 8678 | ` * Return` |
|      - | 8679 | ` *  TRUE if every character in text is a control characters,FALSE otherwise.` |
|      - | 8680 | ` */` |
|     16 | 8681 | `static int PH7_builtin_ctype_cntrl(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8682 | `{` |
|      - | 8683 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8684 | `	int nLen;` |
|     17 | 8685 | `	if( nArg < 1 ){` |
|      - | 8686 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8687 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8688 | `		return PH7_OK;` |
|      - | 8689 | `	}` |
|      - | 8690 | `	/* Extract the target string */` |
|     17 | 8691 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8692 | `	zEnd = &zIn[nLen];` |
|     17 | 8693 | `	if( nLen < 1 ){` |
|      - | 8694 | `		/* Empty string,return FALSE */` |
|      3 | 8695 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8696 | `		return PH7_OK;` |
|      - | 8697 | `	}` |
|      - | 8698 | `	/* Perform the requested operation */` |
|     14 | 8699 | `	for(;;){` |
|     29 | 8700 | `		if( zIn >= zEnd ){` |
|      - | 8701 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8702 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8703 | `			return PH7_OK;` |
|      - | 8704 | `		}` |
|     21 | 8705 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8706 | `			/* UTF-8 stream  */` |
|    ! 0 | 8707 | `			break;` |
|      - | 8708 | `		}` |
|     21 | 8709 | `		if( !SyisCtrl(zIn[0]) ){` |
|      7 | 8710 | `			break;` |
|      - | 8711 | `		}` |
|      - | 8712 | `		/* Point to the next character */` |
|     15 | 8713 | `		zIn++;` |
|      1 | 8714 | `	}` |
|      - | 8715 | `	/* The test failed,return FALSE */` |
|      7 | 8716 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8717 | `	return PH7_OK;` |
|      9 | 8718 | `}` |
|      - | 8719 | `/*` |
|      - | 8720 | ` * bool ctype_digit(string $text)` |
|      - | 8721 | ` *  Checks if all of the characters in the provided string, text, are numerical.` |
|      - | 8722 | ` * Parameters` |
|      - | 8723 | ` *  $text` |
|      - | 8724 | ` *   The tested string.` |
|      - | 8725 | ` * Return` |
|      - | 8726 | ` *  TRUE if every character in the string text is a decimal digit, FALSE otherwise.` |
|      - | 8727 | ` */` |
|   1614 | 8728 | `static int PH7_builtin_ctype_digit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 8729 | `{` |
|      - | 8730 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8731 | `	int nLen;` |
|   1619 | 8732 | `	if( nArg < 1 ){` |
|      - | 8733 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8734 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8735 | `		return PH7_OK;` |
|      - | 8736 | `	}` |
|      - | 8737 | `	/* Extract the target string */` |
|   1619 | 8738 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|   1619 | 8739 | `	zEnd = &zIn[nLen];` |
|   1619 | 8740 | `	if( nLen < 1 ){` |
|      - | 8741 | `		/* Empty string,return FALSE */` |
|      3 | 8742 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8743 | `		return PH7_OK;` |
|      - | 8744 | `	}` |
|      - | 8745 | `	/* Perform the requested operation */` |
|   1515 | 8746 | `	for(;;){` |
|   3035 | 8747 | `		if( zIn >= zEnd ){` |
|      - | 8748 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   1373 | 8749 | `			ph7_result_bool(pCtx,1);` |
|   1373 | 8750 | `			return PH7_OK;` |
|      - | 8751 | `		}` |
|   1667 | 8752 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8753 | `			/* UTF-8 stream  */` |
|    ! 0 | 8754 | `			break;` |
|      - | 8755 | `		}` |
|   1667 | 8756 | `		if( !SyisDigit(zIn[0]) ){` |
|    249 | 8757 | `			break;` |
|      - | 8758 | `		}` |
|      - | 8759 | `		/* Point to the next character */` |
|   1423 | 8760 | `		zIn++;` |
|      5 | 8761 | `	}` |
|      - | 8762 | `	/* The test failed,return FALSE */` |
|    249 | 8763 | `	ph7_result_bool(pCtx,0);` |
|    249 | 8764 | `	return PH7_OK;` |
|    812 | 8765 | `}` |
|      - | 8766 | `/*` |
|      - | 8767 | ` * bool ctype_xdigit(string $text)` |
|      - | 8768 | ` *  Check for character(s) representing a hexadecimal digit.` |
|      - | 8769 | ` * Parameters` |
|      - | 8770 | ` *  $text` |
|      - | 8771 | ` *   The tested string.` |
|      - | 8772 | ` * Return` |
|      - | 8773 | ` *  Returns TRUE if every character in text is a hexadecimal 'digit', that is` |
|      - | 8774 | ` * a decimal digit or a character from [A-Fa-f] , FALSE otherwise.` |
|      - | 8775 | ` */` |
|     18 | 8776 | `static int PH7_builtin_ctype_xdigit(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8777 | `{` |
|      - | 8778 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8779 | `	int nLen;` |
|     19 | 8780 | `	if( nArg < 1 ){` |
|      - | 8781 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8782 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8783 | `		return PH7_OK;` |
|      - | 8784 | `	}` |
|      - | 8785 | `	/* Extract the target string */` |
|     19 | 8786 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 8787 | `	zEnd = &zIn[nLen];` |
|     19 | 8788 | `	if( nLen < 1 ){` |
|      - | 8789 | `		/* Empty string,return FALSE */` |
|      3 | 8790 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8791 | `		return PH7_OK;` |
|      - | 8792 | `	}` |
|      - | 8793 | `	/* Perform the requested operation */` |
|     46 | 8794 | `	for(;;){` |
|     93 | 8795 | `		if( zIn >= zEnd ){` |
|      - | 8796 | `			/* If we reach the end of the string,then the test succeeded. */` |
|     11 | 8797 | `			ph7_result_bool(pCtx,1);` |
|     11 | 8798 | `			return PH7_OK;` |
|      - | 8799 | `		}` |
|     83 | 8800 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8801 | `			/* UTF-8 stream  */` |
|    ! 0 | 8802 | `			break;` |
|      - | 8803 | `		}` |
|     83 | 8804 | `		if( !SyisHex(zIn[0]) ){` |
|      7 | 8805 | `			break;` |
|      - | 8806 | `		}` |
|      - | 8807 | `		/* Point to the next character */` |
|     77 | 8808 | `		zIn++;` |
|      1 | 8809 | `	}` |
|      - | 8810 | `	/* The test failed,return FALSE */` |
|      7 | 8811 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8812 | `	return PH7_OK;` |
|     10 | 8813 | `}` |
|      - | 8814 | `/*` |
|      - | 8815 | ` * bool ctype_graph(string $text)` |
|      - | 8816 | ` *  Checks if all of the characters in the provided string, text, creates visible output.` |
|      - | 8817 | ` * Parameters` |
|      - | 8818 | ` *  $text` |
|      - | 8819 | ` *   The tested string.` |
|      - | 8820 | ` * Return` |
|      - | 8821 | ` *  Returns TRUE if every character in text is printable and actually creates visible output` |
|      - | 8822 | ` * (no white space), FALSE otherwise.` |
|      - | 8823 | ` */` |
|     16 | 8824 | `static int PH7_builtin_ctype_graph(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8825 | `{` |
|      - | 8826 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8827 | `	int nLen;` |
|     17 | 8828 | `	if( nArg < 1 ){` |
|      - | 8829 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8830 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8831 | `		return PH7_OK;` |
|      - | 8832 | `	}` |
|      - | 8833 | `	/* Extract the target string */` |
|     17 | 8834 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8835 | `	zEnd = &zIn[nLen];` |
|     17 | 8836 | `	if( nLen < 1 ){` |
|      - | 8837 | `		/* Empty string,return FALSE */` |
|      3 | 8838 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8839 | `		return PH7_OK;` |
|      - | 8840 | `	}` |
|      - | 8841 | `	/* Perform the requested operation */` |
|     57 | 8842 | `	for(;;){` |
|    115 | 8843 | `		if( zIn >= zEnd ){` |
|      - | 8844 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8845 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8846 | `			return PH7_OK;` |
|      - | 8847 | `		}` |
|    107 | 8848 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8849 | `			/* UTF-8 stream  */` |
|    ! 0 | 8850 | `			break;` |
|      - | 8851 | `		}` |
|    107 | 8852 | `		if( !SyisGraph(zIn[0]) ){` |
|      7 | 8853 | `			break;` |
|      - | 8854 | `		}` |
|      - | 8855 | `		/* Point to the next character */` |
|    101 | 8856 | `		zIn++;` |
|      1 | 8857 | `	}` |
|      - | 8858 | `	/* The test failed,return FALSE */` |
|      7 | 8859 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8860 | `	return PH7_OK;` |
|      9 | 8861 | `}` |
|      - | 8862 | `/*` |
|      - | 8863 | ` * bool ctype_print(string $text)` |
|      - | 8864 | ` *  Checks if all of the characters in the provided string, text, are printable.` |
|      - | 8865 | ` * Parameters` |
|      - | 8866 | ` *  $text` |
|      - | 8867 | ` *   The tested string.` |
|      - | 8868 | ` * Return` |
|      - | 8869 | ` *  Returns TRUE if every character in text will actually create output (including blanks).` |
|      - | 8870 | ` *  Returns FALSE if text contains control characters or characters that do not have any output` |
|      - | 8871 | ` *  or control function at all.` |
|      - | 8872 | ` */` |
|     16 | 8873 | `static int PH7_builtin_ctype_print(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8874 | `{` |
|      - | 8875 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8876 | `	int nLen;` |
|     17 | 8877 | `	if( nArg < 1 ){` |
|      - | 8878 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8879 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8880 | `		return PH7_OK;` |
|      - | 8881 | `	}` |
|      - | 8882 | `	/* Extract the target string */` |
|     17 | 8883 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 8884 | `	zEnd = &zIn[nLen];` |
|     17 | 8885 | `	if( nLen < 1 ){` |
|      - | 8886 | `		/* Empty string,return FALSE */` |
|      3 | 8887 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8888 | `		return PH7_OK;` |
|      - | 8889 | `	}` |
|      - | 8890 | `	/* Perform the requested operation */` |
|     63 | 8891 | `	for(;;){` |
|    127 | 8892 | `		if( zIn >= zEnd ){` |
|      - | 8893 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8894 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8895 | `			return PH7_OK;` |
|      - | 8896 | `		}` |
|    119 | 8897 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8898 | `			/* UTF-8 stream  */` |
|    ! 0 | 8899 | `			break;` |
|      - | 8900 | `		}` |
|    119 | 8901 | `		if( !SyisPrint(zIn[0]) ){` |
|      7 | 8902 | `			break;` |
|      - | 8903 | `		}` |
|      - | 8904 | `		/* Point to the next character */` |
|    113 | 8905 | `		zIn++;` |
|      1 | 8906 | `	}` |
|      - | 8907 | `	/* The test failed,return FALSE */` |
|      7 | 8908 | `	ph7_result_bool(pCtx,0);` |
|      7 | 8909 | `	return PH7_OK;` |
|      9 | 8910 | `}` |
|      - | 8911 | `/*` |
|      - | 8912 | ` * bool ctype_punct(string $text)` |
|      - | 8913 | ` *  Checks if all of the characters in the provided string, text, are punctuation character.` |
|      - | 8914 | ` * Parameters` |
|      - | 8915 | ` *  $text` |
|      - | 8916 | ` *   The tested string.` |
|      - | 8917 | ` * Return` |
|      - | 8918 | ` *  Returns TRUE if every character in text is printable, but neither letter` |
|      - | 8919 | ` *  digit or blank, FALSE otherwise.` |
|      - | 8920 | ` */` |
|     18 | 8921 | `static int PH7_builtin_ctype_punct(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 8922 | `{` |
|      - | 8923 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8924 | `	int nLen;` |
|     19 | 8925 | `	if( nArg < 1 ){` |
|      - | 8926 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8927 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8928 | `		return PH7_OK;` |
|      - | 8929 | `	}` |
|      - | 8930 | `	/* Extract the target string */` |
|     19 | 8931 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     19 | 8932 | `	zEnd = &zIn[nLen];` |
|     19 | 8933 | `	if( nLen < 1 ){` |
|      - | 8934 | `		/* Empty string,return FALSE */` |
|      3 | 8935 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8936 | `		return PH7_OK;` |
|      - | 8937 | `	}` |
|      - | 8938 | `	/* Perform the requested operation */` |
|     38 | 8939 | `	for(;;){` |
|     77 | 8940 | `		if( zIn >= zEnd ){` |
|      - | 8941 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      9 | 8942 | `			ph7_result_bool(pCtx,1);` |
|      9 | 8943 | `			return PH7_OK;` |
|      - | 8944 | `		}` |
|     69 | 8945 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8946 | `			/* UTF-8 stream  */` |
|    ! 0 | 8947 | `			break;` |
|      - | 8948 | `		}` |
|     69 | 8949 | `		if( !SyisPunct(zIn[0]) ){` |
|      9 | 8950 | `			break;` |
|      - | 8951 | `		}` |
|      - | 8952 | `		/* Point to the next character */` |
|     61 | 8953 | `		zIn++;` |
|      1 | 8954 | `	}` |
|      - | 8955 | `	/* The test failed,return FALSE */` |
|      9 | 8956 | `	ph7_result_bool(pCtx,0);` |
|      9 | 8957 | `	return PH7_OK;` |
|     10 | 8958 | `}` |
|      - | 8959 | `/*` |
|      - | 8960 | ` * bool ctype_space(string $text)` |
|      - | 8961 | ` *  Checks if all of the characters in the provided string, text, creates whitespace.` |
|      - | 8962 | ` * Parameters` |
|      - | 8963 | ` *  $text` |
|      - | 8964 | ` *   The tested string.` |
|      - | 8965 | ` * Return` |
|      - | 8966 | ` *  Returns TRUE if every character in text creates some sort of white space, FALSE otherwise.` |
|      - | 8967 | ` *  Besides the blank character this also includes tab, vertical tab, line feed, carriage return` |
|      - | 8968 | ` *  and form feed characters.` |
|      - | 8969 | ` */` |
|  61965 | 8970 | `static int PH7_builtin_ctype_space(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      5 | 8971 | `{` |
|      - | 8972 | `	const unsigned char *zIn,*zEnd;` |
|      - | 8973 | `	int nLen;` |
|  61970 | 8974 | `	if( nArg < 1 ){` |
|      - | 8975 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 8976 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 8977 | `		return PH7_OK;` |
|      - | 8978 | `	}` |
|      - | 8979 | `	/* Extract the target string */` |
|  61970 | 8980 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|  61970 | 8981 | `	zEnd = &zIn[nLen];` |
|  61970 | 8982 | `	if( nLen < 1 ){` |
|      - | 8983 | `		/* Empty string,return FALSE */` |
|      3 | 8984 | `		ph7_result_bool(pCtx,0);` |
|      3 | 8985 | `		return PH7_OK;` |
|      - | 8986 | `	}` |
|      - | 8987 | `	/* Perform the requested operation */` |
|  32087 | 8988 | `	for(;;){` |
|  64094 | 8989 | `		if( zIn >= zEnd ){` |
|      - | 8990 | `			/* If we reach the end of the string,then the test succeeded. */` |
|   2107 | 8991 | `			ph7_result_bool(pCtx,1);` |
|   2107 | 8992 | `			return PH7_OK;` |
|      - | 8993 | `		}` |
|  61992 | 8994 | `		if( zIn[0] >= 0xc0 ){` |
|      - | 8995 | `			/* UTF-8 stream  */` |
|    ! 0 | 8996 | `			break;` |
|      - | 8997 | `		}` |
|  61992 | 8998 | `		if( !SyisSpace(zIn[0]) ){` |
|  59866 | 8999 | `			break;` |
|      - | 9000 | `		}` |
|      - | 9001 | `		/* Point to the next character */` |
|   2131 | 9002 | `		zIn++;` |
|      5 | 9003 | `	}` |
|      - | 9004 | `	/* The test failed,return FALSE */` |
|  59866 | 9005 | `	ph7_result_bool(pCtx,0);` |
|  59866 | 9006 | `	return PH7_OK;` |
|  31030 | 9007 | `}` |
|      - | 9008 | `/*` |
|      - | 9009 | ` * bool ctype_lower(string $text)` |
|      - | 9010 | ` *  Checks if all of the characters in the provided string, text, are lowercase letters.` |
|      - | 9011 | ` * Parameters` |
|      - | 9012 | ` *  $text` |
|      - | 9013 | ` *   The tested string.` |
|      - | 9014 | ` * Return` |
|      - | 9015 | ` *  Returns TRUE if every character in text is a lowercase letter in the current locale.` |
|      - | 9016 | ` */` |
|     16 | 9017 | `static int PH7_builtin_ctype_lower(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9018 | `{` |
|      - | 9019 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9020 | `	int nLen;` |
|     17 | 9021 | `	if( nArg < 1 ){` |
|      - | 9022 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9023 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9024 | `		return PH7_OK;` |
|      - | 9025 | `	}` |
|      - | 9026 | `	/* Extract the target string */` |
|     17 | 9027 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9028 | `	zEnd = &zIn[nLen];` |
|     17 | 9029 | `	if( nLen < 1 ){` |
|      - | 9030 | `		/* Empty string,return FALSE */` |
|      3 | 9031 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9032 | `		return PH7_OK;` |
|      - | 9033 | `	}` |
|      - | 9034 | `	/* Perform the requested operation */` |
|     27 | 9035 | `	for(;;){` |
|     55 | 9036 | `		if( zIn >= zEnd ){` |
|      - | 9037 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 9038 | `			ph7_result_bool(pCtx,1);` |
|      5 | 9039 | `			return PH7_OK;` |
|      - | 9040 | `		}` |
|     51 | 9041 | `		if( !SyisLower(zIn[0]) ){` |
|     11 | 9042 | `			break;` |
|      - | 9043 | `		}` |
|      - | 9044 | `		/* Point to the next character */` |
|     41 | 9045 | `		zIn++;` |
|      1 | 9046 | `	}` |
|      - | 9047 | `	/* The test failed,return FALSE */` |
|     11 | 9048 | `	ph7_result_bool(pCtx,0);` |
|     11 | 9049 | `	return PH7_OK;` |
|      9 | 9050 | `}` |
|      - | 9051 | `/*` |
|      - | 9052 | ` * bool ctype_upper(string $text)` |
|      - | 9053 | ` *  Checks if all of the characters in the provided string, text, are uppercase letters.` |
|      - | 9054 | ` * Parameters` |
|      - | 9055 | ` *  $text` |
|      - | 9056 | ` *   The tested string.` |
|      - | 9057 | ` * Return` |
|      - | 9058 | ` *  Returns TRUE if every character in text is a uppercase letter in the current locale.` |
|      - | 9059 | ` */` |
|     16 | 9060 | `static int PH7_builtin_ctype_upper(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9061 | `{` |
|      - | 9062 | `	const unsigned char *zIn,*zEnd;` |
|      - | 9063 | `	int nLen;` |
|     17 | 9064 | `	if( nArg < 1 ){` |
|      - | 9065 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9066 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9067 | `		return PH7_OK;` |
|      - | 9068 | `	}` |
|      - | 9069 | `	/* Extract the target string */` |
|     17 | 9070 | `	zIn  = (const unsigned char *)ph7_value_to_string(apArg[0],&nLen);` |
|     17 | 9071 | `	zEnd = &zIn[nLen];` |
|     17 | 9072 | `	if( nLen < 1 ){` |
|      - | 9073 | `		/* Empty string,return FALSE */` |
|      3 | 9074 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9075 | `		return PH7_OK;` |
|      - | 9076 | `	}` |
|      - | 9077 | `	/* Perform the requested operation */` |
|     28 | 9078 | `	for(;;){` |
|     57 | 9079 | `		if( zIn >= zEnd ){` |
|      - | 9080 | `			/* If we reach the end of the string,then the test succeeded. */` |
|      5 | 9081 | `			ph7_result_bool(pCtx,1);` |
|      5 | 9082 | `			return PH7_OK;` |
|      - | 9083 | `		}` |
|     53 | 9084 | `		if( !SyisUpper(zIn[0]) ){` |
|     11 | 9085 | `			break;` |
|      - | 9086 | `		}` |
|      - | 9087 | `		/* Point to the next character */` |
|     43 | 9088 | `		zIn++;` |
|      1 | 9089 | `	}` |
|      - | 9090 | `	/* The test failed,return FALSE */` |
|     11 | 9091 | `	ph7_result_bool(pCtx,0);` |
|     11 | 9092 | `	return PH7_OK;` |
|      9 | 9093 | `}` |
|      - | 9094 | `/* Date/Time functions moved to builtin_date.c */` |
|      - | 9095 | `/*` |
|      - | 9096 | ` * Section:` |
|      - | 9097 | ` *    URL handling Functions.` |
|      - | 9098 | ` * Status:` |
|      - | 9099 | ` *    Stable.` |
|      - | 9100 | ` */` |
|      - | 9101 | `/*` |
|      - | 9102 | ` * Output consumer callback for the standard Symisc routines.` |
|      - | 9103 | ` * [i.e: SyBase64Encode(),SyBase64Decode(),SyUriEncode(),...].` |
|      - | 9104 | ` */` |
|   1026 | 9105 | `static int Consumer(const void *pData,unsigned int nLen,void *pUserData)` |
|      2 | 9106 | `{` |
|      - | 9107 | `	/* Store in the call context result buffer */` |
|   1028 | 9108 | `	ph7_result_string((ph7_context *)pUserData,(const char *)pData,(int)nLen);` |
|   1028 | 9109 | `	return SXRET_OK;` |
|      2 | 9110 | `}` |
|      - | 9111 | `/*` |
|      - | 9112 | ` * string base64_encode(string $data)` |
|      - | 9113 | ` * string convert_uuencode(string $data)` |
|      - | 9114 | ` *  Encodes data with MIME base64` |
|      - | 9115 | ` * Parameter` |
|      - | 9116 | ` *  $data` |
|      - | 9117 | ` *    Data to encode` |
|      - | 9118 | ` * Return` |
|      - | 9119 | ` *  Encoded data or FALSE on failure.` |
|      - | 9120 | ` */` |
|      6 | 9121 | `static int PH7_builtin_base64_encode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9122 | `{` |
|      - | 9123 | `	const char *zIn;` |
|      - | 9124 | `	int nLen;` |
|      7 | 9125 | `	if( nArg < 1 ){` |
|      - | 9126 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9127 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9128 | `		return PH7_OK;` |
|      - | 9129 | `	}` |
|      - | 9130 | `	/* Extract the input string */` |
|      7 | 9131 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 9132 | `	if( nLen < 1 ){` |
|      - | 9133 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9134 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9135 | `		return PH7_OK;` |
|      - | 9136 | `	}` |
|      - | 9137 | `	/* Perform the BASE64 encoding */` |
|      7 | 9138 | `	SyBase64Encode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      7 | 9139 | `	return PH7_OK;` |
|      4 | 9140 | `}` |
|      - | 9141 | `/*` |
|      - | 9142 | ` * string base64_decode(string $data)` |
|      - | 9143 | ` * string convert_uudecode(string $data)` |
|      - | 9144 | ` *  Decodes data encoded with MIME base64` |
|      - | 9145 | ` * Parameter` |
|      - | 9146 | ` *  $data` |
|      - | 9147 | ` *    Encoded data.` |
|      - | 9148 | ` * Return` |
|      - | 9149 | ` *  Returns the original data or FALSE on failure.` |
|      - | 9150 | ` */` |
|     34 | 9151 | `static int PH7_builtin_base64_decode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      2 | 9152 | `{` |
|      - | 9153 | `	const char *zIn;` |
|      - | 9154 | `	int nLen;` |
|     36 | 9155 | `	if( nArg < 1 ){` |
|      - | 9156 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9157 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9158 | `		return PH7_OK;` |
|      - | 9159 | `	}` |
|      - | 9160 | `	/* Extract the input string */` |
|     36 | 9161 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|     36 | 9162 | `	if( nLen < 1 ){` |
|      - | 9163 | `		/* Nothing to process,return FALSE */` |
|      3 | 9164 | `		ph7_result_bool(pCtx,0);` |
|      3 | 9165 | `		return PH7_OK;` |
|      - | 9166 | `	}` |
|      - | 9167 | `	/* Perform the BASE64 decoding */` |
|     34 | 9168 | `	SyBase64Decode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|     34 | 9169 | `	return PH7_OK;` |
|     19 | 9170 | `}` |
|      - | 9171 | `/*` |
|      - | 9172 | ` * string urlencode(string $str)` |
|      - | 9173 | ` *  URL encoding` |
|      - | 9174 | ` * Parameter` |
|      - | 9175 | ` *  $data` |
|      - | 9176 | ` *   Input string.` |
|      - | 9177 | ` * Return` |
|      - | 9178 | ` *  Returns a string in which all non-alphanumeric characters except -_. have` |
|      - | 9179 | ` *  been replaced with a percent (%) sign followed by two hex digits and spaces` |
|      - | 9180 | ` *  encoded as plus (+) signs.` |
|      - | 9181 | ` */` |
|      4 | 9182 | `static int PH7_builtin_urlencode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9183 | `{` |
|      - | 9184 | `	const char *zIn;` |
|      - | 9185 | `	int nLen;` |
|      5 | 9186 | `	if( nArg < 1 ){` |
|      - | 9187 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9188 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9189 | `		return PH7_OK;` |
|      - | 9190 | `	}` |
|      - | 9191 | `	/* Extract the input string */` |
|      5 | 9192 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      5 | 9193 | `	if( nLen < 1 ){` |
|      - | 9194 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9195 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9196 | `		return PH7_OK;` |
|      - | 9197 | `	}` |
|      - | 9198 | `	/* Perform the URL encoding */` |
|      5 | 9199 | `	SyUriEncode(zIn,(sxu32)nLen,Consumer,pCtx);` |
|      5 | 9200 | `	return PH7_OK;` |
|      3 | 9201 | `}` |
|      - | 9202 | `/*` |
|      - | 9203 | ` * string urldecode(string $str)` |
|      - | 9204 | ` *  Decodes any %## encoding in the given string.` |
|      - | 9205 | ` *  Plus symbols ('+') are decoded to a space character.` |
|      - | 9206 | ` * Parameter` |
|      - | 9207 | ` *  $data` |
|      - | 9208 | ` *    Input string.` |
|      - | 9209 | ` * Return` |
|      - | 9210 | ` *  Decoded URL or FALSE on failure.` |
|      - | 9211 | ` */` |
|      6 | 9212 | `static int PH7_builtin_urldecode(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 | 9213 | `{` |
|      - | 9214 | `	const char *zIn;` |
|      - | 9215 | `	int nLen;` |
|      7 | 9216 | `	if( nArg < 1 ){` |
|      - | 9217 | `		/* Missing arguments,return FALSE */` |
|    ! 0 | 9218 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9219 | `		return PH7_OK;` |
|      - | 9220 | `	}` |
|      - | 9221 | `	/* Extract the input string */` |
|      7 | 9222 | `	zIn = ph7_value_to_string(apArg[0],&nLen);` |
|      7 | 9223 | `	if( nLen < 1 ){` |
|      - | 9224 | `		/* Nothing to process,return FALSE */` |
|    ! 0 | 9225 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 9226 | `		return PH7_OK;` |
|      - | 9227 | `	}` |
|      - | 9228 | `	/* Perform the URL decoding */` |
|      7 | 9229 | `	SyUriDecode(zIn,(sxu32)nLen,Consumer,pCtx,TRUE);` |
|      7 | 9230 | `	return PH7_OK;` |
|      4 | 9231 | `}` |
|      - | 9232 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9233 | `/* Table of the built-in functions */` |
|      - | 9234 | `static const ph7_builtin_func aBuiltInFunc[] = {` |
|      - | 9235 | `	   /* Variable handling functions */` |
|      - | 9236 | `	{ "is_bool"    , PH7_builtin_is_bool     },` |
|      - | 9237 | `	{ "is_float"   , PH7_builtin_is_float    },` |
|      - | 9238 | `	{ "is_real"    , PH7_builtin_is_float    },` |
|      - | 9239 | `	{ "is_double"  , PH7_builtin_is_float    },` |
|      - | 9240 | `	{ "is_int"     , PH7_builtin_is_int      },` |
|      - | 9241 | `	{ "is_integer" , PH7_builtin_is_int      },` |
|      - | 9242 | `	{ "is_long"    , PH7_builtin_is_int      },` |
|      - | 9243 | `	{ "is_string"  , PH7_builtin_is_string   },` |
|      - | 9244 | `	{ "is_null"    , PH7_builtin_is_null     },` |
|      - | 9245 | `	{ "is_numeric" , PH7_builtin_is_numeric  },` |
|      - | 9246 | `	{ "is_scalar"  , PH7_builtin_is_scalar   },` |
|      - | 9247 | `	{ "is_array"   , PH7_builtin_is_array    },` |
|      - | 9248 | `	{ "is_object"  , PH7_builtin_is_object   },` |
|      - | 9249 | `	{ "is_resource", PH7_builtin_is_resource },` |
|      - | 9250 | `	{ "douleval"   , PH7_builtin_floatval    },` |
|      - | 9251 | `	{ "floatval"   , PH7_builtin_floatval    },` |
|      - | 9252 | `	{ "intval"     , PH7_builtin_intval      },` |
|      - | 9253 | `	{ "strval"     , PH7_builtin_strval      },` |
|      - | 9254 | `	{ "boolval"    , PH7_builtin_boolval     },` |
|      - | 9255 | `	{ "empty"      , PH7_builtin_empty       },` |
|      - | 9256 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9257 | `#ifdef PH7_ENABLE_MATH_FUNC` |
|      - | 9258 | `	   /* Math functions */` |
|      - | 9259 | `	{ "abs"  ,    PH7_builtin_abs          },` |
|      - | 9260 | `	{ "sqrt" ,    PH7_builtin_sqrt         },` |
|      - | 9261 | `	{ "exp"  ,    PH7_builtin_exp          },` |
|      - | 9262 | `	{ "floor",    PH7_builtin_floor        },` |
|      - | 9263 | `	{ "cos"  ,    PH7_builtin_cos          },` |
|      - | 9264 | `	{ "sin"  ,    PH7_builtin_sin          },` |
|      - | 9265 | `	{ "acos" ,    PH7_builtin_acos         },` |
|      - | 9266 | `	{ "asin" ,    PH7_builtin_asin         },` |
|      - | 9267 | `	{ "cosh" ,    PH7_builtin_cosh         },` |
|      - | 9268 | `	{ "sinh" ,    PH7_builtin_sinh         },` |
|      - | 9269 | `	{ "ceil" ,    PH7_builtin_ceil         },` |
|      - | 9270 | `	{ "tan"  ,    PH7_builtin_tan          },` |
|      - | 9271 | `	{ "tanh" ,    PH7_builtin_tanh         },` |
|      - | 9272 | `	{ "atan" ,    PH7_builtin_atan         },` |
|      - | 9273 | `	{ "atan2",    PH7_builtin_atan2        },` |
|      - | 9274 | `	{ "log"  ,    PH7_builtin_log          },` |
|      - | 9275 | `	{ "log10" ,   PH7_builtin_log10        },` |
|      - | 9276 | `	{ "pow"  ,    PH7_builtin_pow          },` |
|      - | 9277 | `	{ "pi",       PH7_builtin_pi           },` |
|      - | 9278 | `	{ "fmod",     PH7_builtin_fmod         },` |
|      - | 9279 | `	{ "hypot",    PH7_builtin_hypot        },` |
|      - | 9280 | `#endif /* PH7_ENABLE_MATH_FUNC */` |
|      - | 9281 | `	{ "round",    PH7_builtin_round        },` |
|      - | 9282 | `	{ "intdiv",   PH7_builtin_intdiv       },` |
|      - | 9283 | `	{ "dechex", PH7_builtin_dechex         },` |
|      - | 9284 | `	{ "decoct", PH7_builtin_decoct         },` |
|      - | 9285 | `	{ "decbin", PH7_builtin_decbin         },` |
|      - | 9286 | `	{ "hexdec", PH7_builtin_hexdec         },` |
|      - | 9287 | `	{ "bindec", PH7_builtin_bindec         },` |
|      - | 9288 | `	{ "octdec", PH7_builtin_octdec         },` |
|      - | 9289 | `	{ "srand",  PH7_builtin_srand          },` |
|      - | 9290 | `	{ "mt_srand",PH7_builtin_srand         },` |
|      - | 9291 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9292 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9293 | `	{ "base_convert", PH7_builtin_base_convert },` |
|      - | 9294 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9295 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9296 | `	   /* String handling functions */` |
|      - | 9297 |  |
|      - | 9298 | `	{ "substr",          PH7_builtin_substr     },` |
|      - | 9299 | `	{ "substr_compare",  PH7_builtin_substr_compare },` |
|      - | 9300 | `	{ "substr_count",    PH7_builtin_substr_count },` |
|      - | 9301 | `	{ "substr_replace",  PH7_builtin_substr_replace },` |
|      - | 9302 | `	{ "levenshtein",     PH7_builtin_levenshtein },` |
|      - | 9303 | `	{ "similar_text",    PH7_builtin_similar_text },` |
|      - | 9304 | `	{ "str_word_count",  PH7_builtin_str_word_count },` |
|      - | 9305 | `	{ "chunk_split",     PH7_builtin_chunk_split},` |
|      - | 9306 | `	{ "addslashes" ,     PH7_builtin_addslashes },` |
|      - | 9307 | `	{ "addcslashes",     PH7_builtin_addcslashes},` |
|      - | 9308 | `	{ "quotemeta",       PH7_builtin_quotemeta  },` |
|      - | 9309 | `	{ "stripslashes",    PH7_builtin_stripslashes },` |
|      - | 9310 | `	{ "htmlspecialchars",PH7_builtin_htmlspecialchars },` |
|      - | 9311 | `	{ "htmlspecialchars_decode", PH7_builtin_htmlspecialchars_decode },` |
|      - | 9312 | `	{ "get_html_translation_table",PH7_builtin_get_html_translation_table },` |
|      - | 9313 | `	{ "htmlentities",PH7_builtin_htmlentities},` |
|      - | 9314 | `	{ "html_entity_decode", PH7_builtin_html_entity_decode},` |
|      - | 9315 | `	{ "strlen"     , PH7_builtin_strlen     },` |
|      - | 9316 | `	{ "strcmp"     , PH7_builtin_strcmp     },` |
|      - | 9317 | `	{ "strcoll"    , PH7_builtin_strcmp     },` |
|      - | 9318 | `	{ "strncmp"    , PH7_builtin_strncmp    },` |
|      - | 9319 | `	{ "strcasecmp" , PH7_builtin_strcasecmp },` |
|      - | 9320 | `	{ "strncasecmp", PH7_builtin_strncasecmp},` |
|      - | 9321 | `	{ "implode"    , PH7_builtin_implode    },` |
|      - | 9322 | `	{ "join"       , PH7_builtin_implode    },` |
|      - | 9323 | `	{ "implode_recursive" , PH7_builtin_implode_recursive },` |
|      - | 9324 | `	{ "join_recursive"    , PH7_builtin_implode_recursive },` |
|      - | 9325 | `	{ "explode"     , PH7_builtin_explode    },` |
|      - | 9326 | `	{ "trim"        , PH7_builtin_trim       },` |
|      - | 9327 | `	{ "rtrim"       , PH7_builtin_rtrim      },` |
|      - | 9328 | `	{ "chop"        , PH7_builtin_rtrim      },` |
|      - | 9329 | `	{ "ltrim"       , PH7_builtin_ltrim      },` |
|      - | 9330 | `	{ "strtolower",   PH7_builtin_strtolower },` |
|      - | 9331 | `	{ "mb_strtolower",PH7_builtin_strtolower }, /* Only UTF-8 encoding is supported */` |
|      - | 9332 | `	{ "strtoupper",   PH7_builtin_strtoupper },` |
|      - | 9333 | `	{ "mb_strtoupper",PH7_builtin_strtoupper }, /* Only UTF-8 encoding is supported */` |
|      - | 9334 | `	{ "ucfirst",      PH7_builtin_ucfirst    },` |
|      - | 9335 | `	{ "lcfirst",      PH7_builtin_lcfirst    },` |
|      - | 9336 | `	{ "ord",          PH7_builtin_ord        },` |
|      - | 9337 | `	{ "chr",          PH7_builtin_chr        },` |
|      - | 9338 | `	{ "bin2hex",      PH7_builtin_bin2hex    },` |
|      - | 9339 | `	{ "strstr",       PH7_builtin_strstr     },` |
|      - | 9340 | `	{ "stristr",      PH7_builtin_stristr    },` |
|      - | 9341 | `	{ "strchr",       PH7_builtin_strstr     },` |
|      - | 9342 | `	{ "strpos",       PH7_builtin_strpos     },` |
|      - | 9343 | `	{ "stripos",      PH7_builtin_stripos    },` |
|      - | 9344 | `	{ "strrpos",      PH7_builtin_strrpos    },` |
|      - | 9345 | `	{ "strripos",     PH7_builtin_strripos   },` |
|      - | 9346 | `	{ "strrchr",      PH7_builtin_strrchr    },` |
|      - | 9347 | `	{ "strrev",       PH7_builtin_strrev     },` |
|      - | 9348 | `	{ "ucwords",      PH7_builtin_ucwords    },` |
|      - | 9349 | `	{ "str_repeat",   PH7_builtin_str_repeat },` |
|      - | 9350 | `	{ "str_contains", PH7_builtin_str_contains },` |
|      - | 9351 | `	{ "str_starts_with", PH7_builtin_str_starts_with },` |
|      - | 9352 | `	{ "str_ends_with", PH7_builtin_str_ends_with },` |
|      - | 9353 | `	{ "nl2br",        PH7_builtin_nl2br      },` |
|      - | 9354 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9355 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9356 | `	{ "sprintf",      PH7_builtin_sprintf    },` |
|      - | 9357 | `	{ "printf",       PH7_builtin_printf     },` |
|      - | 9358 | `	{ "vprintf",      PH7_builtin_vprintf    },` |
|      - | 9359 | `	{ "vsprintf",     PH7_builtin_vsprintf   },` |
|      - | 9360 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9361 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9362 | `	{ "size_format",  PH7_builtin_size_format},` |
|      - | 9363 |  |
|      - | 9364 |  |
|      - | 9365 | `#ifndef PH7_DISABLE_HASH_FUNC` |
|      - | 9366 | `	{ "md5",          PH7_builtin_md5       },` |
|      - | 9367 | `	{ "sha1",         PH7_builtin_sha1      },` |
|      - | 9368 | `	{ "crc32",        PH7_builtin_crc32     },` |
|      - | 9369 | `	{ "hash",         PH7_builtin_hash      },` |
|      - | 9370 | `	{ "hash_hmac",    PH7_builtin_hash_hmac },` |
|      - | 9371 | `	{ "hash_equals",  PH7_builtin_hash_equals },` |
|      - | 9372 | `	{ "hash_algos",   PH7_builtin_hash_algos },` |
|      - | 9373 | `#endif /* PH7_DISABLE_HASH_FUNC */` |
|      - | 9374 | `	{ "password_hash",         PH7_builtin_password_hash },` |
|      - | 9375 | `	{ "password_verify",       PH7_builtin_password_verify },` |
|      - | 9376 | `	{ "password_get_info",     PH7_builtin_password_get_info },` |
|      - | 9377 | `	{ "password_needs_rehash", PH7_builtin_password_needs_rehash },` |
|      - | 9378 | `	{ "filter_var",            PH7_builtin_filter_var },` |
|      - | 9379 | `	{ "filter_input",          PH7_builtin_filter_input },` |
|      - | 9380 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9381 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9382 | `	{ "str_getcsv",   PH7_builtin_str_getcsv },` |
|      - | 9383 | `	{ "strip_tags",   PH7_builtin_strip_tags },` |
|      - | 9384 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9385 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9386 |  |
|      - | 9387 | `	{ "str_shuffle",  PH7_builtin_str_shuffle},` |
|      - | 9388 | `	{ "str_split",    PH7_builtin_str_split  },` |
|      - | 9389 | `	{ "strspn",       PH7_builtin_strspn     },` |
|      - | 9390 | `	{ "strcspn",      PH7_builtin_strcspn    },` |
|      - | 9391 | `	{ "strpbrk",      PH7_builtin_strpbrk    },` |
|      - | 9392 | `	{ "soundex",      PH7_builtin_soundex    },` |
|      - | 9393 | `	{ "wordwrap",     PH7_builtin_wordwrap   },` |
|      - | 9394 | `	{ "strtok",       PH7_builtin_strtok     },` |
|      - | 9395 | `	{ "str_pad",      PH7_builtin_str_pad    },` |
|      - | 9396 | `	{ "str_replace",  PH7_builtin_str_replace},` |
|      - | 9397 | `	{ "str_ireplace", PH7_builtin_str_replace},` |
|      - | 9398 | `	{ "strtr",        PH7_builtin_strtr      },` |
|      - | 9399 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9400 | `#ifdef PH7_NEED_FMT_AND_INI` |
|      - | 9401 | `	{ "parse_ini_string", PH7_builtin_parse_ini_string},` |
|      - | 9402 | `#endif /* PH7_NEED_FMT_AND_INI */` |
|      - | 9403 | `#ifdef PH7_NEED_BUILTIN_REG` |
|      - | 9404 |  |
|      - | 9405 | `	         /* Ctype functions */` |
|      - | 9406 | `	{ "ctype_alnum", PH7_builtin_ctype_alnum },` |
|      - | 9407 | `	{ "ctype_alpha", PH7_builtin_ctype_alpha },` |
|      - | 9408 | `	{ "ctype_cntrl", PH7_builtin_ctype_cntrl },` |
|      - | 9409 | `	{ "ctype_digit", PH7_builtin_ctype_digit },` |
|      - | 9410 | `	{ "ctype_xdigit",PH7_builtin_ctype_xdigit},` |
|      - | 9411 | `	{ "ctype_graph", PH7_builtin_ctype_graph },` |
|      - | 9412 | `	{ "ctype_print", PH7_builtin_ctype_print },` |
|      - | 9413 | `	{ "ctype_punct", PH7_builtin_ctype_punct },` |
|      - | 9414 | `	{ "ctype_space", PH7_builtin_ctype_space },` |
|      - | 9415 | `	{ "ctype_lower", PH7_builtin_ctype_lower },` |
|      - | 9416 | `	{ "ctype_upper", PH7_builtin_ctype_upper },` |
|      - | 9417 | `	         /* Time functions */` |
|      - | 9418 | `	{ "time"    ,    PH7_builtin_time         },` |
|      - | 9419 | `	{ "microtime",   PH7_builtin_microtime    },` |
|      - | 9420 | `	{ "getdate" ,    PH7_builtin_getdate      },` |
|      - | 9421 | `	{ "gettimeofday",PH7_builtin_gettimeofday },` |
|      - | 9422 | `	{ "date",        PH7_builtin_date         },` |
|      - | 9423 | `	{ "strftime",    PH7_builtin_strftime     },` |
|      - | 9424 | `	{ "idate",       PH7_builtin_idate        },` |
|      - | 9425 | `	{ "gmdate",      PH7_builtin_gmdate       },` |
|      - | 9426 | `	{ "localtime",   PH7_builtin_localtime    },` |
|      - | 9427 | `	{ "mktime",      PH7_builtin_mktime       },` |
|      - | 9428 | `	{ "gmmktime",    PH7_builtin_mktime       },` |
|      - | 9429 | `	        /* URL functions */` |
|      - | 9430 | `	{ "base64_encode",PH7_builtin_base64_encode },` |
|      - | 9431 | `	{ "base64_decode",PH7_builtin_base64_decode },` |
|      - | 9432 | `	{ "convert_uuencode",PH7_builtin_base64_encode },` |
|      - | 9433 | `	{ "convert_uudecode",PH7_builtin_base64_decode },` |
|      - | 9434 | `	{ "urlencode",    PH7_builtin_urlencode },` |
|      - | 9435 | `	{ "urldecode",    PH7_builtin_urldecode },` |
|      - | 9436 | `	{ "rawurlencode", PH7_builtin_urlencode },` |
|      - | 9437 | `	{ "rawurldecode", PH7_builtin_urldecode },` |
|      - | 9438 | `#endif /* PH7_NEED_BUILTIN_REG */` |
|      - | 9439 | `};` |
|      - | 9440 | `/*` |
|      - | 9441 | ` * Register the built-in functions defined above,the array functions` |
|      - | 9442 | ` * defined in hashmap.c and the IO functions defined in vfs.c.` |
|      - | 9443 | ` */` |
|   3474 | 9444 | `PH7_PRIVATE void PH7_RegisterBuiltInFunction(ph7_vm *pVm)` |
|      5 | 9445 | `{` |
|      - | 9446 | `	sxu32 n;` |
| 597533 | 9447 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBuiltInFunc) ; ++n ){` |
| 594059 | 9448 | `		ph7_create_function(&(*pVm),aBuiltInFunc[n].zName,aBuiltInFunc[n].xFunc,0);` |
| 297032 | 9449 | `	}` |
|      - | 9450 | `	/* Register hashmap functions [i.e: array_merge(),sort(),count(),array_diff(),...] */` |
|   3479 | 9451 | `	PH7_RegisterHashmapFunctions(&(*pVm));` |
|      - | 9452 | `	/* Register IO functions [i.e: fread(),fwrite(),chdir(),mkdir(),file(),...] */` |
|   3479 | 9453 | `	PH7_RegisterIORoutine(&(*pVm));` |
|   3479 | 9454 | `}` |
|      - | 9455 |  |
